
/* Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/*==========================================================================
 *
 *  @file:     wlan_hal_msg.h
 *
 *  @brief:    Exports and types for messages sent to HAL from WDI
 *
 *  @author:   Kumar Anand
 *
 *
 *=========================================================================*/

#ifndef _WLAN_HAL_MSG_H_
#define _WLAN_HAL_MSG_H_

#include "halLegacyPalTypes.h"
#include "halCompiler.h"
#include "wlan_qct_dev_defs.h"
#include "wlan_nv.h"

/*---------------------------------------------------------------------------
  API VERSIONING INFORMATION

  The RIVA API is versioned as MAJOR.MINOR.VERSION.REVISION
  The MAJOR is incremented for major product/architecture changes
      (and then MINOR/VERSION/REVISION are zeroed)
  The MINOR is incremented for minor product/architecture changes
      (and then VERSION/REVISION are zeroed)
  The VERSION is incremented if a significant API change occurs
      (and then REVISION is zeroed)
  The REVISION is incremented if an insignificant API change occurs
      or if a new API is added
  All values are in the range 0..255 (ie they are 8-bit values)
 ---------------------------------------------------------------------------*/
#define WLAN_HAL_VER_MAJOR 1
#define WLAN_HAL_VER_MINOR 5
#define WLAN_HAL_VER_VERSION 1
#define WLAN_HAL_VER_REVISION 2

/*---------------------------------------------------------------------------
  Commom Type definitons
 ---------------------------------------------------------------------------*/

#define DISA_MAX_PAYLOAD_SIZE   1600

//This is to force compiler to use the maximum of an int ( 4 bytes )
#define WLAN_HAL_MAX_ENUM_SIZE    0x7FFFFFFF
#define WLAN_HAL_MSG_TYPE_MAX_ENUM_SIZE    0x7FFF

//Max no. of transmit categories
#define STACFG_MAX_TC    8

//The maximum value of access category
#define WLAN_HAL_MAX_AC  4

typedef tANI_U8 tSirMacAddr[6];
typedef tANI_U8 tHalIpv4Addr[4];

#define HAL_MAC_ADDR_LEN        6
#define HAL_IPV4_ADDR_LEN       4

#define WLAN_HAL_STA_INVALID_IDX 0xFF
#define WLAN_HAL_BSS_INVALID_IDX 0xFF

//Default Beacon template size
#define BEACON_TEMPLATE_SIZE 0x180


//Max Tx Data Rate samples
#define MAX_TX_RATE_SAMPLES     10
//Max Beacon Rssi  samples
#define MAX_BCN_RSSI_SAMPLES    10

//Param Change Bitmap sent to HAL
#define PARAM_BCN_INTERVAL_CHANGED                      (1 << 0)
#define PARAM_SHORT_PREAMBLE_CHANGED                 (1 << 1)
#define PARAM_SHORT_SLOT_TIME_CHANGED                 (1 << 2)
#define PARAM_llACOEXIST_CHANGED                            (1 << 3)
#define PARAM_llBCOEXIST_CHANGED                            (1 << 4)
#define PARAM_llGCOEXIST_CHANGED                            (1 << 5)
#define PARAM_HT20MHZCOEXIST_CHANGED                  (1<<6)
#define PARAM_NON_GF_DEVICES_PRESENT_CHANGED (1<<7)
#define PARAM_RIFS_MODE_CHANGED                            (1<<8)
#define PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED   (1<<9)
#define PARAM_OBSS_MODE_CHANGED                               (1<<10)
#define PARAM_BEACON_UPDATE_MASK                (PARAM_BCN_INTERVAL_CHANGED|PARAM_SHORT_PREAMBLE_CHANGED|PARAM_SHORT_SLOT_TIME_CHANGED|PARAM_llACOEXIST_CHANGED |PARAM_llBCOEXIST_CHANGED|\
    PARAM_llGCOEXIST_CHANGED|PARAM_HT20MHZCOEXIST_CHANGED|PARAM_NON_GF_DEVICES_PRESENT_CHANGED|PARAM_RIFS_MODE_CHANGED|PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED| PARAM_OBSS_MODE_CHANGED)

/*Dump command response Buffer size*/
#define DUMPCMD_RSP_BUFFER 500

/*Version string max length (including NUL) */
#define WLAN_HAL_VERSION_LENGTH  64

#define WLAN_HAL_ROAM_SCAN_MAX_PROBE_SIZE     450
/* 80 is actually NUM_RF_CHANNELS_V2, but beyond V2, this number will be ignored by FW */
#define WLAN_HAL_ROAM_SCAN_MAX_CHANNELS       80
#define WLAN_HAL_ROAM_SACN_PMK_SIZE           32
#define WLAN_HAL_ROAM_SCAN_RESERVED_BYTES     20

#define WLAN_HAL_EXT_SCAN_MAX_CHANNELS               16
#define WLAN_HAL_EXT_SCAN_MAX_BUCKETS                16
#define WLAN_HAL_EXT_SCAN_MAX_HOTLIST_APS            128
#define WLAN_HAL_EXT_SCAN_MAX_SIG_CHANGE_APS         64
#define WLAN_HAL_EXT_SCAN_MAX_RSSI_SAMPLE_SIZE       8

/* For Logging enhancement feature currently max 2 address will be passed */
/* In future we may pass 3 address and length in suspend mode corner case */
#define MAX_NUM_OF_BUFFER 3

/* Log types */
typedef enum
{
    MGMT_FRAME_LOGS    = 0,
    QXDM_LOGGING       = 1,
    FW_MEMORY_DUMP     = 2
}tHalFrameLoggingType;

/* Log size */
typedef enum
{
    LOG_SIZE_4K   = 0,
    LOG_SIZE_8K   = 1,
    LOG_SIZE_12K  = 2,
    LOG_SIZE_16K  = 3,
    LOG_SIZE_32K  = 4,
    LOG_SIZE_64K  = 5,
    LOG_SIZE_96K  = 6
}tHalLogBuffSize;

/* Message types for messages exchanged between WDI and HAL */
typedef enum
{
   //Init/De-Init
   WLAN_HAL_START_REQ = 0,
   WLAN_HAL_START_RSP = 1,
   WLAN_HAL_STOP_REQ  = 2,
   WLAN_HAL_STOP_RSP  = 3,

   //Scan
   WLAN_HAL_INIT_SCAN_REQ    = 4,
   WLAN_HAL_INIT_SCAN_RSP    = 5,
   WLAN_HAL_START_SCAN_REQ   = 6,
   WLAN_HAL_START_SCAN_RSP   = 7 ,
   WLAN_HAL_END_SCAN_REQ     = 8,
   WLAN_HAL_END_SCAN_RSP     = 9,
   WLAN_HAL_FINISH_SCAN_REQ  = 10,
   WLAN_HAL_FINISH_SCAN_RSP  = 11,

   // HW STA configuration/deconfiguration
   WLAN_HAL_CONFIG_STA_REQ   = 12,
   WLAN_HAL_CONFIG_STA_RSP   = 13,
   WLAN_HAL_DELETE_STA_REQ   = 14,
   WLAN_HAL_DELETE_STA_RSP   = 15,
   WLAN_HAL_CONFIG_BSS_REQ   = 16,
   WLAN_HAL_CONFIG_BSS_RSP   = 17,
   WLAN_HAL_DELETE_BSS_REQ   = 18,
   WLAN_HAL_DELETE_BSS_RSP   = 19,

   //Infra STA asscoiation
   WLAN_HAL_JOIN_REQ         = 20,
   WLAN_HAL_JOIN_RSP         = 21,
   WLAN_HAL_POST_ASSOC_REQ   = 22,
   WLAN_HAL_POST_ASSOC_RSP   = 23,

   //Security
   WLAN_HAL_SET_BSSKEY_REQ   = 24,
   WLAN_HAL_SET_BSSKEY_RSP   = 25,
   WLAN_HAL_SET_STAKEY_REQ   = 26,
   WLAN_HAL_SET_STAKEY_RSP   = 27,
   WLAN_HAL_RMV_BSSKEY_REQ   = 28,
   WLAN_HAL_RMV_BSSKEY_RSP   = 29,
   WLAN_HAL_RMV_STAKEY_REQ   = 30,
   WLAN_HAL_RMV_STAKEY_RSP   = 31,

   //Qos Related
   WLAN_HAL_ADD_TS_REQ          = 32,
   WLAN_HAL_ADD_TS_RSP          = 33,
   WLAN_HAL_DEL_TS_REQ          = 34,
   WLAN_HAL_DEL_TS_RSP          = 35,
   WLAN_HAL_UPD_EDCA_PARAMS_REQ = 36,
   WLAN_HAL_UPD_EDCA_PARAMS_RSP = 37,
   WLAN_HAL_ADD_BA_REQ          = 38,
   WLAN_HAL_ADD_BA_RSP          = 39,
   WLAN_HAL_DEL_BA_REQ          = 40,
   WLAN_HAL_DEL_BA_RSP          = 41,

   WLAN_HAL_CH_SWITCH_REQ       = 42,
   WLAN_HAL_CH_SWITCH_RSP       = 43,
   WLAN_HAL_SET_LINK_ST_REQ     = 44,
   WLAN_HAL_SET_LINK_ST_RSP     = 45,
   WLAN_HAL_GET_STATS_REQ       = 46,
   WLAN_HAL_GET_STATS_RSP       = 47,
   WLAN_HAL_UPDATE_CFG_REQ      = 48,
   WLAN_HAL_UPDATE_CFG_RSP      = 49,

   WLAN_HAL_MISSED_BEACON_IND           = 50,
   WLAN_HAL_UNKNOWN_ADDR2_FRAME_RX_IND  = 51,
   WLAN_HAL_MIC_FAILURE_IND             = 52,
   WLAN_HAL_FATAL_ERROR_IND             = 53,
   WLAN_HAL_SET_KEYDONE_MSG             = 54,

   //NV Interface
   WLAN_HAL_DOWNLOAD_NV_REQ             = 55,
   WLAN_HAL_DOWNLOAD_NV_RSP             = 56,

   WLAN_HAL_ADD_BA_SESSION_REQ          = 57,
   WLAN_HAL_ADD_BA_SESSION_RSP          = 58,
   WLAN_HAL_TRIGGER_BA_REQ              = 59,
   WLAN_HAL_TRIGGER_BA_RSP              = 60,
   WLAN_HAL_UPDATE_BEACON_REQ           = 61,
   WLAN_HAL_UPDATE_BEACON_RSP           = 62,
   WLAN_HAL_SEND_BEACON_REQ             = 63,
   WLAN_HAL_SEND_BEACON_RSP             = 64,

   WLAN_HAL_SET_BCASTKEY_REQ               = 65,
   WLAN_HAL_SET_BCASTKEY_RSP               = 66,
   WLAN_HAL_DELETE_STA_CONTEXT_IND         = 67,
   WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_REQ  = 68,
   WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_RSP  = 69,

  // PTT interface support
   WLAN_HAL_PROCESS_PTT_REQ   = 70,
   WLAN_HAL_PROCESS_PTT_RSP   = 71,

   // BTAMP related events
   WLAN_HAL_SIGNAL_BTAMP_EVENT_REQ  = 72,
   WLAN_HAL_SIGNAL_BTAMP_EVENT_RSP  = 73,
   WLAN_HAL_TL_HAL_FLUSH_AC_REQ     = 74,
   WLAN_HAL_TL_HAL_FLUSH_AC_RSP     = 75,

   WLAN_HAL_ENTER_IMPS_REQ           = 76,
   WLAN_HAL_EXIT_IMPS_REQ            = 77,
   WLAN_HAL_ENTER_BMPS_REQ           = 78,
   WLAN_HAL_EXIT_BMPS_REQ            = 79,
   WLAN_HAL_ENTER_UAPSD_REQ          = 80,
   WLAN_HAL_EXIT_UAPSD_REQ           = 81,
   WLAN_HAL_UPDATE_UAPSD_PARAM_REQ   = 82,
   WLAN_HAL_CONFIGURE_RXP_FILTER_REQ = 83,
   WLAN_HAL_ADD_BCN_FILTER_REQ       = 84,
   WLAN_HAL_REM_BCN_FILTER_REQ       = 85,
   WLAN_HAL_ADD_WOWL_BCAST_PTRN      = 86,
   WLAN_HAL_DEL_WOWL_BCAST_PTRN      = 87,
   WLAN_HAL_ENTER_WOWL_REQ           = 88,
   WLAN_HAL_EXIT_WOWL_REQ            = 89,
   WLAN_HAL_HOST_OFFLOAD_REQ         = 90,
   WLAN_HAL_SET_RSSI_THRESH_REQ      = 91,
   WLAN_HAL_GET_RSSI_REQ             = 92,
   WLAN_HAL_SET_UAPSD_AC_PARAMS_REQ  = 93,
   WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ = 94,

   WLAN_HAL_ENTER_IMPS_RSP           = 95,
   WLAN_HAL_EXIT_IMPS_RSP            = 96,
   WLAN_HAL_ENTER_BMPS_RSP           = 97,
   WLAN_HAL_EXIT_BMPS_RSP            = 98,
   WLAN_HAL_ENTER_UAPSD_RSP          = 99,
   WLAN_HAL_EXIT_UAPSD_RSP           = 100,
   WLAN_HAL_SET_UAPSD_AC_PARAMS_RSP  = 101,
   WLAN_HAL_UPDATE_UAPSD_PARAM_RSP   = 102,
   WLAN_HAL_CONFIGURE_RXP_FILTER_RSP = 103,
   WLAN_HAL_ADD_BCN_FILTER_RSP       = 104,
   WLAN_HAL_REM_BCN_FILTER_RSP       = 105,
   WLAN_HAL_SET_RSSI_THRESH_RSP      = 106,
   WLAN_HAL_HOST_OFFLOAD_RSP         = 107,
   WLAN_HAL_ADD_WOWL_BCAST_PTRN_RSP  = 108,
   WLAN_HAL_DEL_WOWL_BCAST_PTRN_RSP  = 109,
   WLAN_HAL_ENTER_WOWL_RSP           = 110,
   WLAN_HAL_EXIT_WOWL_RSP            = 111,
   WLAN_HAL_RSSI_NOTIFICATION_IND    = 112,
   WLAN_HAL_GET_RSSI_RSP             = 113,
   WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_RSP = 114,

   //11k related events
   WLAN_HAL_SET_MAX_TX_POWER_REQ   = 115,
   WLAN_HAL_SET_MAX_TX_POWER_RSP   = 116,

   //11R related msgs
   WLAN_HAL_AGGR_ADD_TS_REQ        = 117,
   WLAN_HAL_AGGR_ADD_TS_RSP        = 118,

   //P2P  WLAN_FEATURE_P2P
   WLAN_HAL_SET_P2P_GONOA_REQ      = 119,
   WLAN_HAL_SET_P2P_GONOA_RSP      = 120,

   //WLAN Dump commands
   WLAN_HAL_DUMP_COMMAND_REQ       = 121,
   WLAN_HAL_DUMP_COMMAND_RSP       = 122,

   //OEM_DATA FEATURE SUPPORT
   WLAN_HAL_START_OEM_DATA_REQ   = 123,
   WLAN_HAL_START_OEM_DATA_RSP   = 124,

   //ADD SELF STA REQ and RSP
   WLAN_HAL_ADD_STA_SELF_REQ       = 125,
   WLAN_HAL_ADD_STA_SELF_RSP       = 126,

   //DEL SELF STA SUPPORT
   WLAN_HAL_DEL_STA_SELF_REQ       = 127,
   WLAN_HAL_DEL_STA_SELF_RSP       = 128,

   // Coex Indication
   WLAN_HAL_COEX_IND               = 129,

   // Tx Complete Indication
   WLAN_HAL_OTA_TX_COMPL_IND       = 130,

   //Host Suspend/resume messages
   WLAN_HAL_HOST_SUSPEND_IND       = 131,
   WLAN_HAL_HOST_RESUME_REQ        = 132,
   WLAN_HAL_HOST_RESUME_RSP        = 133,

   WLAN_HAL_SET_TX_POWER_REQ       = 134,
   WLAN_HAL_SET_TX_POWER_RSP       = 135,
   WLAN_HAL_GET_TX_POWER_REQ       = 136,
   WLAN_HAL_GET_TX_POWER_RSP       = 137,

   WLAN_HAL_P2P_NOA_ATTR_IND       = 138,

   WLAN_HAL_ENABLE_RADAR_DETECT_REQ  = 139,
   WLAN_HAL_ENABLE_RADAR_DETECT_RSP  = 140,
   WLAN_HAL_GET_TPC_REPORT_REQ       = 141,
   WLAN_HAL_GET_TPC_REPORT_RSP       = 142,
   WLAN_HAL_RADAR_DETECT_IND         = 143,
   WLAN_HAL_RADAR_DETECT_INTR_IND    = 144,
   WLAN_HAL_KEEP_ALIVE_REQ           = 145,
   WLAN_HAL_KEEP_ALIVE_RSP           = 146,

   /*PNO messages*/
   WLAN_HAL_SET_PREF_NETWORK_REQ     = 147,
   WLAN_HAL_SET_PREF_NETWORK_RSP     = 148,
   WLAN_HAL_SET_RSSI_FILTER_REQ      = 149,
   WLAN_HAL_SET_RSSI_FILTER_RSP      = 150,
   WLAN_HAL_UPDATE_SCAN_PARAM_REQ    = 151,
   WLAN_HAL_UPDATE_SCAN_PARAM_RSP    = 152,
   WLAN_HAL_PREF_NETW_FOUND_IND      = 153,

   WLAN_HAL_SET_TX_PER_TRACKING_REQ  = 154,
   WLAN_HAL_SET_TX_PER_TRACKING_RSP  = 155,
   WLAN_HAL_TX_PER_HIT_IND           = 156,

   WLAN_HAL_8023_MULTICAST_LIST_REQ   = 157,
   WLAN_HAL_8023_MULTICAST_LIST_RSP   = 158,

   WLAN_HAL_SET_PACKET_FILTER_REQ     = 159,
   WLAN_HAL_SET_PACKET_FILTER_RSP     = 160,
   WLAN_HAL_PACKET_FILTER_MATCH_COUNT_REQ   = 161,
   WLAN_HAL_PACKET_FILTER_MATCH_COUNT_RSP   = 162,
   WLAN_HAL_CLEAR_PACKET_FILTER_REQ         = 163,
   WLAN_HAL_CLEAR_PACKET_FILTER_RSP         = 164,
   /*This is temp fix. Should be removed once
    * Host and Riva code is in sync*/
   WLAN_HAL_INIT_SCAN_CON_REQ               = 165,

   WLAN_HAL_SET_POWER_PARAMS_REQ            = 166,
   WLAN_HAL_SET_POWER_PARAMS_RSP            = 167,

   WLAN_HAL_TSM_STATS_REQ                   = 168,
   WLAN_HAL_TSM_STATS_RSP                   = 169,

   // wake reason indication (WOW)
   WLAN_HAL_WAKE_REASON_IND                 = 170,
   // GTK offload support
   WLAN_HAL_GTK_OFFLOAD_REQ                 = 171,
   WLAN_HAL_GTK_OFFLOAD_RSP                 = 172,
   WLAN_HAL_GTK_OFFLOAD_GETINFO_REQ         = 173,
   WLAN_HAL_GTK_OFFLOAD_GETINFO_RSP         = 174,

   WLAN_HAL_FEATURE_CAPS_EXCHANGE_REQ       = 175,
   WLAN_HAL_FEATURE_CAPS_EXCHANGE_RSP       = 176,
   WLAN_HAL_EXCLUDE_UNENCRYPTED_IND         = 177,

   WLAN_HAL_SET_THERMAL_MITIGATION_REQ      = 178,
   WLAN_HAL_SET_THERMAL_MITIGATION_RSP      = 179,

   WLAN_HAL_UPDATE_VHT_OP_MODE_REQ          = 182,
   WLAN_HAL_UPDATE_VHT_OP_MODE_RSP          = 183,

   WLAN_HAL_P2P_NOA_START_IND               = 184,

   WLAN_HAL_GET_ROAM_RSSI_REQ               = 185,
   WLAN_HAL_GET_ROAM_RSSI_RSP               = 186,

   WLAN_HAL_CLASS_B_STATS_IND               = 187,
   WLAN_HAL_DEL_BA_IND                      = 188,
   WLAN_HAL_DHCP_START_IND                  = 189,
   WLAN_HAL_DHCP_STOP_IND                   = 190,
   WLAN_ROAM_SCAN_OFFLOAD_REQ               = 191,
   WLAN_ROAM_SCAN_OFFLOAD_RSP               = 192,
   WLAN_HAL_WIFI_PROXIMITY_REQ              = 193,
   WLAN_HAL_WIFI_PROXIMITY_RSP              = 194,

   WLAN_HAL_START_SPECULATIVE_PS_POLLS_REQ  = 195,
   WLAN_HAL_START_SPECULATIVE_PS_POLLS_RSP  = 196,
   WLAN_HAL_STOP_SPECULATIVE_PS_POLLS_IND   = 197,

   WLAN_HAL_TDLS_LINK_ESTABLISHED_REQ       = 198,
   WLAN_HAL_TDLS_LINK_ESTABLISHED_RSP       = 199,
   WLAN_HAL_TDLS_LINK_TEARDOWN_REQ          = 200,
   WLAN_HAL_TDLS_LINK_TEARDOWN_RSP          = 201,
   WLAN_HAL_TDLS_IND                        = 202,
   WLAN_HAL_IBSS_PEER_INACTIVITY_IND        = 203,

   /* Scan Offload APIs */
   WLAN_HAL_START_SCAN_OFFLOAD_REQ          = 204,
   WLAN_HAL_START_SCAN_OFFLOAD_RSP          = 205,
   WLAN_HAL_STOP_SCAN_OFFLOAD_REQ           = 206,
   WLAN_HAL_STOP_SCAN_OFFLOAD_RSP           = 207,
   WLAN_HAL_UPDATE_CHANNEL_LIST_REQ         = 208,
   WLAN_HAL_UPDATE_CHANNEL_LIST_RSP         = 209,
   WLAN_HAL_OFFLOAD_SCAN_EVENT_IND          = 210,

   /* APIs to offload TCP/UDP Heartbeat handshakes */
   WLAN_HAL_LPHB_CFG_REQ                    = 211,
   WLAN_HAL_LPHB_CFG_RSP                    = 212,
   WLAN_HAL_LPHB_IND                        = 213,

   WLAN_HAL_ADD_PERIODIC_TX_PTRN_IND        = 214,
   WLAN_HAL_DEL_PERIODIC_TX_PTRN_IND        = 215,
   WLAN_HAL_PERIODIC_TX_PTRN_FW_IND         = 216,

   // Events to set Per-Band Tx Power Limit
   WLAN_HAL_SET_MAX_TX_POWER_PER_BAND_REQ   = 217,
   WLAN_HAL_SET_MAX_TX_POWER_PER_BAND_RSP   = 218,

   /* Reliable Multicast using Leader Based Protocol */
   WLAN_HAL_LBP_LEADER_REQ                  = 219,
   WLAN_HAL_LBP_LEADER_RSP                  = 220,
   WLAN_HAL_LBP_UPDATE_IND                  = 221,

   /* Batchscan */
   WLAN_HAL_BATCHSCAN_SET_REQ               = 222,
   WLAN_HAL_BATCHSCAN_SET_RSP               = 223,
   WLAN_HAL_BATCHSCAN_TRIGGER_RESULT_IND    = 224,
   WLAN_HAL_BATCHSCAN_RESULT_IND            = 225,
   WLAN_HAL_BATCHSCAN_STOP_IND              = 226,

   WLAN_HAL_GET_IBSS_PEER_INFO_REQ          = 227,
   WLAN_HAL_GET_IBSS_PEER_INFO_RSP          = 228,

   WLAN_HAL_RATE_UPDATE_IND                 = 229,

   /* Tx Fail for weak link notification */
   WLAN_HAL_TX_FAIL_MONITOR_IND             = 230,
   WLAN_HAL_TX_FAIL_IND                     = 231,

   /* Multi-hop IP routing offload */
   WLAN_HAL_IP_FORWARD_TABLE_UPDATE_IND     = 232,

   /* Channel avoidance for LTE Coex */
   WLAN_HAL_AVOID_FREQ_RANGE_IND            = 233,

   /* Fast Roam Offload Synchup request protocol */
   /* TODO_LFR3 : change this value accordingly before final check-in */
   WLAN_HAL_ROAM_OFFLOAD_SYNCH_IND          = 234,
   WLAN_HAL_ROAM_OFFLOAD_SYNCH_CNF          = 235,

   WLAN_HAL_MOTION_START_EVENT_REQ          = 250,
   WLAN_HAL_MOTION_STOP_EVENT_REQ           = 251,

   /* Channel Switch Request version 1 */
   WLAN_HAL_CH_SWITCH_V1_REQ                = 252,
   WLAN_HAL_CH_SWITCH_V1_RSP                = 253,

   /* 2G4 HT40 OBSS scan */
   WLAN_HAL_START_HT40_OBSS_SCAN_IND        = 254,
   WLAN_HAL_STOP_HT40_OBSS_SCAN_IND         = 255,/* next free entry in tHalHostMsgType. */

   /* WLAN NAN Messages */
   WLAN_HAL_NAN_FIRST                       = 256,
   WLAN_HAL_NAN_REQ                         = WLAN_HAL_NAN_FIRST,
   WLAN_HAL_NAN_RSP                         = 257,
   WLAN_HAL_NAN_EVT                         = 258,
   WLAN_HAL_NAN_LAST                        = WLAN_HAL_NAN_EVT,
   WLAN_HAL_PRINT_REG_INFO_IND              = 259,

   WLAN_HAL_GET_BCN_MISS_RATE_REQ           = 260,
   WLAN_HAL_GET_BCN_MISS_RATE_RSP           = 261,

   /* WLAN LINK LAYER STATS Messages */
   WLAN_HAL_LL_SET_STATS_REQ                = 262,
   WLAN_HAL_LL_SET_STATS_RSP                = 263,
   WLAN_HAL_LL_GET_STATS_REQ                = 264,
   WLAN_HAL_LL_GET_STATS_RSP                = 265,
   WLAN_HAL_LL_CLEAR_STATS_REQ              = 266,
   WLAN_HAL_LL_CLEAR_STATS_RSP              = 267,
   WLAN_HAL_LL_NOTIFY_STATS                 = 268,
   WLAN_HAL_LL_LAST                         = WLAN_HAL_LL_NOTIFY_STATS,

   /* WLAN EXT_SCAN Messages */
   WLAN_HAL_EXT_SCAN_START_REQ              = 269,
   WLAN_HAL_EXT_SCAN_START_RSP              = 270,
   WLAN_HAL_EXT_SCAN_GET_CAP_REQ            = 271,
   WLAN_HAL_EXT_SCAN_GET_CAP_RSP            = 272,
   WLAN_HAL_EXT_SCAN_STOP_REQ               = 273,
   WLAN_HAL_EXT_SCAN_STOP_RSP               = 274,
   WLAN_HAL_EXT_SCAN_GET_SCAN_REQ           = 275,
   WLAN_HAL_EXT_SCAN_GET_SCAN_RSP           = 276,

   WLAN_HAL_BSSID_HOTLIST_SET_REQ           = 277,
   WLAN_HAL_BSSID_HOTLIST_SET_RSP           = 278,
   WLAN_HAL_BSSID_HOTLIST_RESET_REQ         = 279,
   WLAN_HAL_BSSID_HOTLIST_RESET_RSP         = 280,

   WLAN_HAL_SIG_RSSI_SET_REQ                = 281,
   WLAN_HAL_SIG_RSSI_SET_RSP                = 282,
   WLAN_HAL_SIG_RSSI_RESET_REQ              = 283,
   WLAN_HAL_SIG_RSSI_RESET_RSP              = 284,

   WLAN_HAL_EXT_SCAN_RESULT_IND             = 285,
   WLAN_HAL_BSSID_HOTLIST_RESULT_IND        = 286,
   WLAN_HAL_SIG_RSSI_RESULT_IND             = 287,
   WLAN_HAL_EXT_SCAN_PROGRESS_IND           = 288,
   WLAN_HAL_EXT_SCAN_RESULT_AVAILABLE_IND   = 289,
   WLAN_HAL_TDLS_CHAN_SWITCH_REQ            = 290,
   WLAN_HAL_TDLS_CHAN_SWITCH_RSP            = 291,
   WLAN_HAL_MAC_SPOOFED_SCAN_REQ            = 292,
   WLAN_HAL_MAC_SPOOFED_SCAN_RSP            = 293,
   /* LGE DISA encrypt-decrypt Messages */
   WLAN_HAL_ENCRYPT_DATA_REQ                = 294,
   WLAN_HAL_ENCRYPT_DATA_RSP                = 295,

   WLAN_HAL_FW_STATS_REQ                    = 296,
   WLAN_HAL_FW_STATS_RSP                    = 297,
   WLAN_HAL_FW_LOGGING_INIT_REQ             = 298,
   WLAN_HAL_FW_LOGGING_INIT_RSP             = 299,
   WLAN_HAL_GET_FRAME_LOG_REQ               = 300,
   WLAN_HAL_GET_FRAME_LOG_RSP               = 301,

   /* Monitor Mode */
   WLAN_HAL_ENABLE_MONITOR_MODE_REQ         = 302,
   WLAN_HAL_ENABLE_MONITOR_MODE_RSP         = 303,

   WLAN_HAL_DISABLE_MONITOR_MODE_REQ        = 304,
   WLAN_HAL_DISABLE_MONITOR_MODE_RSP        = 305,

   WLAN_HAL_SET_RTS_CTS_HTVHT_IND           = 306,
   // FW Logging
   WLAN_HAL_FATAL_EVENT_LOGGING_REQ         = 307,
   WLAN_HAL_FATAL_EVENT_LOGGING_RSP         = 308,
   WLAN_HAL_FW_MEMORY_DUMP_REQ              = 309,
   WLAN_HAL_FW_MEMORY_DUMP_RSP              = 310,
   WLAN_HAL_FW_LOGGING_DXE_DONE_IND         = 311,
   WLAN_HAL_LOST_LINK_PARAMETERS_IND        = 312,
   WLAN_HAL_SEND_FREQ_RANGE_CONTROL_IND     = 313,
   WLAN_HAL_MSG_MAX = WLAN_HAL_MSG_TYPE_MAX_ENUM_SIZE
}tHalHostMsgType;

/* Enumeration for Version */
typedef enum
{
   WLAN_HAL_MSG_VERSION0 = 0,
   WLAN_HAL_MSG_VERSION1 = 1,
   WLAN_HAL_MSG_WCNSS_CTRL_VERSION = 0x7FFF, /*define as 2 bytes data*/
   WLAN_HAL_MSG_VERSION_MAX_FIELD  = WLAN_HAL_MSG_WCNSS_CTRL_VERSION
}tHalHostMsgVersion;

/* Enumeration for Boolean - False/True, On/Off */
typedef enum tagAniBoolean
{
    eANI_BOOLEAN_FALSE = 0,
    eANI_BOOLEAN_TRUE,
    eANI_BOOLEAN_OFF = 0,
    eANI_BOOLEAN_ON = 1,
    eANI_BOOLEAN_MAX_FIELD = 0x7FFFFFFF  /* define as 4 bytes data */
} eAniBoolean;

typedef enum
{
   eDRIVER_TYPE_PRODUCTION  = 0,
   eDRIVER_TYPE_MFG         = 1,
   eDRIVER_TYPE_DVT         = 2,
   eDRIVER_TYPE_MAX         = WLAN_HAL_MAX_ENUM_SIZE
} tDriverType;

typedef enum
{
   HAL_STOP_TYPE_SYS_RESET,
   HAL_STOP_TYPE_SYS_DEEP_SLEEP,
   HAL_STOP_TYPE_RF_KILL,
   HAL_STOP_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE
}tHalStopType;

typedef enum
{
   eHAL_SYS_MODE_NORMAL,
   eHAL_SYS_MODE_LEARN,
   eHAL_SYS_MODE_SCAN,
   eHAL_SYS_MODE_PROMISC,
   eHAL_SYS_MODE_SUSPEND_LINK,
   eHAL_SYS_MODE_ROAM_SCAN,
   eHAL_SYS_MODE_ROAM_SUSPEND_LINK,
   eHAL_SYS_MODE_OEM_DATA,
   eHAL_SYS_MODE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} eHalSysMode;

typedef enum
{
   eHAL_CHANNEL_SWITCH_SOURCE_SCAN,
   eHAL_CHANNEL_SWITCH_SOURCE_LISTEN,
   eHAL_CHANNEL_SWITCH_SOURCE_MCC,
   eHAL_CHANNEL_SWITCH_SOURCE_CSA,
   eHAL_CHANNEL_SWITCH_SOURCE_CONFIG_BSS,
   eHAL_CHANNEL_SWITCH_SOURCE_CONFIG_STA,
   eHAL_CHANNEL_SWITCH_SOURCE_JOIN_REQ,
   eHAL_CHANNEL_SWITCH_SOURCE_INNAV,
   eHAL_CHANNEL_SWITCH_SOURCE_WCA,
   eHAL_CHANNEL_SWITCH_SOURCE_MLME,
   eHAL_CHANNEL_SWITCH_SOURCE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} eHalChanSwitchSource;

typedef enum
{
    PHY_SINGLE_CHANNEL_CENTERED = 0,     // 20MHz IF bandwidth centered on IF carrier
    PHY_DOUBLE_CHANNEL_LOW_PRIMARY = 1,  // 40MHz IF bandwidth with lower 20MHz supporting the primary channel
    PHY_DOUBLE_CHANNEL_CENTERED = 2,     // 40MHz IF bandwidth centered on IF carrier
    PHY_DOUBLE_CHANNEL_HIGH_PRIMARY = 3, // 40MHz IF bandwidth with higher 20MHz supporting the primary channel
#ifdef WLAN_FEATURE_11AC
    PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED = 4, //20/40MHZ offset LOW 40/80MHZ offset CENTERED
    PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED = 5, //20/40MHZ offset CENTERED 40/80MHZ offset CENTERED
    PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED = 6, //20/40MHZ offset HIGH 40/80MHZ offset CENTERED
    PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW = 7,//20/40MHZ offset LOW 40/80MHZ offset LOW
    PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW = 8, //20/40MHZ offset HIGH 40/80MHZ offset LOW
    PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH = 9, //20/40MHZ offset LOW 40/80MHZ offset HIGH
    PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH = 10,//20/40MHZ offset-HIGH 40/80MHZ offset HIGH
#endif
    PHY_CHANNEL_BONDING_STATE_MAX = WLAN_HAL_MAX_ENUM_SIZE
}ePhyChanBondState;

// Spatial Multiplexing(SM) Power Save mode
typedef enum eSirMacHTMIMOPowerSaveState
{
  eSIR_HT_MIMO_PS_STATIC = 0,    // Static SM Power Save mode
  eSIR_HT_MIMO_PS_DYNAMIC = 1,   // Dynamic SM Power Save mode
  eSIR_HT_MIMO_PS_NA = 2,        // reserved
  eSIR_HT_MIMO_PS_NO_LIMIT = 3,  // SM Power Save disabled
  eSIR_HT_MIMO_PS_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tSirMacHTMIMOPowerSaveState;

/* each station added has a rate mode which specifies the sta attributes */
typedef enum eStaRateMode {
    eSTA_TAURUS = 0,
    eSTA_TITAN,
    eSTA_POLARIS,
    eSTA_11b,
    eSTA_11bg,
    eSTA_11a,
    eSTA_11n,
#ifdef WLAN_FEATURE_11AC
    eSTA_11ac,
#endif
    eSTA_INVALID_RATE_MODE = WLAN_HAL_MAX_ENUM_SIZE
} tStaRateMode, *tpStaRateMode;

#define SIR_NUM_11B_RATES           4  //1,2,5.5,11
#define SIR_NUM_11A_RATES           8  //6,9,12,18,24,36,48,54
#define SIR_NUM_POLARIS_RATES       3  //72,96,108

#define SIR_MAC_MAX_SUPPORTED_MCS_SET    16


typedef enum eSirBssType
{
    eSIR_INFRASTRUCTURE_MODE,
    eSIR_INFRA_AP_MODE,                    //Added for softAP support
    eSIR_IBSS_MODE,
    eSIR_BTAMP_STA_MODE,                   //Added for BT-AMP support
    eSIR_BTAMP_AP_MODE,                    //Added for BT-AMP support
    eSIR_AUTO_MODE,
    eSIR_DONOT_USE_BSS_TYPE = WLAN_HAL_MAX_ENUM_SIZE
} tSirBssType;

typedef enum eSirNwType
{
    eSIR_11A_NW_TYPE,
    eSIR_11B_NW_TYPE,
    eSIR_11G_NW_TYPE,
    eSIR_11N_NW_TYPE,
    eSIR_DONOT_USE_NW_TYPE = WLAN_HAL_MAX_ENUM_SIZE
} tSirNwType;

typedef tANI_U16 tSirMacBeaconInterval;

#define SIR_MAC_RATESET_EID_MAX            12

typedef enum eSirMacHTOperatingMode
{
  eSIR_HT_OP_MODE_PURE,                // No Protection
  eSIR_HT_OP_MODE_OVERLAP_LEGACY,      // Overlap Legacy device present, protection is optional
  eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT,  // No legacy device, but 20 MHz HT present
  eSIR_HT_OP_MODE_MIXED,               // Protection is required
  eSIR_HT_OP_MODE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tSirMacHTOperatingMode;

/// Encryption type enum used with peer
typedef enum eAniEdType
{
    eSIR_ED_NONE,
    eSIR_ED_WEP40,
    eSIR_ED_WEP104,
    eSIR_ED_TKIP,
    eSIR_ED_CCMP,
    eSIR_ED_WPI,
    eSIR_ED_AES_128_CMAC,
    eSIR_ED_NOT_IMPLEMENTED = WLAN_HAL_MAX_ENUM_SIZE
} tAniEdType;

#define WLAN_MAX_KEY_RSC_LEN                16
#define WLAN_WAPI_KEY_RSC_LEN               16

/// MAX key length when ULA is used
#define SIR_MAC_MAX_KEY_LENGTH              32
#define SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS     4

/// Enum to specify whether key is used
/// for TX only, RX only or both
typedef enum eAniKeyDirection
{
    eSIR_TX_ONLY,
    eSIR_RX_ONLY,
    eSIR_TX_RX,
    eSIR_TX_DEFAULT,
    eSIR_DONOT_USE_KEY_DIRECTION = WLAN_HAL_MAX_ENUM_SIZE
} tAniKeyDirection;

typedef enum eAniWepType
{
    eSIR_WEP_STATIC,
    eSIR_WEP_DYNAMIC,
    eSIR_WEP_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tAniWepType;

typedef enum eSriLinkState {

    eSIR_LINK_IDLE_STATE        = 0,
    eSIR_LINK_PREASSOC_STATE    = 1,
    eSIR_LINK_POSTASSOC_STATE   = 2,
    eSIR_LINK_AP_STATE          = 3,
    eSIR_LINK_IBSS_STATE        = 4,

    /* BT-AMP Case */
    eSIR_LINK_BTAMP_PREASSOC_STATE  = 5,
    eSIR_LINK_BTAMP_POSTASSOC_STATE  = 6,
    eSIR_LINK_BTAMP_AP_STATE  = 7,
    eSIR_LINK_BTAMP_STA_STATE  = 8,

    /* Reserved for HAL Internal Use */
    eSIR_LINK_LEARN_STATE       = 9,
    eSIR_LINK_SCAN_STATE        = 10,
    eSIR_LINK_FINISH_SCAN_STATE = 11,
    eSIR_LINK_INIT_CAL_STATE    = 12,
    eSIR_LINK_FINISH_CAL_STATE  = 13,
#ifdef WLAN_FEATURE_P2P
    eSIR_LINK_LISTEN_STATE      = 14,
    eSIR_LINK_SEND_ACTION_STATE = 15,
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    eSIR_LINK_FT_PREASSOC_STATE = 16,
#endif
    eSIR_LINK_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tSirLinkState;

typedef enum
{
    HAL_SUMMARY_STATS_INFO           = 0x00000001,
    HAL_GLOBAL_CLASS_A_STATS_INFO    = 0x00000002,
    HAL_GLOBAL_CLASS_B_STATS_INFO    = 0x00000004,
    HAL_GLOBAL_CLASS_C_STATS_INFO    = 0x00000008,
    HAL_GLOBAL_CLASS_D_STATS_INFO    = 0x00000010,
    HAL_PER_STA_STATS_INFO           = 0x00000020
}eHalStatsMask;

/* BT-AMP events type */
typedef enum
{
    BTAMP_EVENT_CONNECTION_START,
    BTAMP_EVENT_CONNECTION_STOP,
    BTAMP_EVENT_CONNECTION_TERMINATED,
    BTAMP_EVENT_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE, //This and beyond are invalid values
} tBtAmpEventType;

//***************************************************************


/*******************PE Statistics*************************/
typedef enum
{
    PE_SUMMARY_STATS_INFO           = 0x00000001,
    PE_GLOBAL_CLASS_A_STATS_INFO    = 0x00000002,
    PE_GLOBAL_CLASS_B_STATS_INFO    = 0x00000004,
    PE_GLOBAL_CLASS_C_STATS_INFO    = 0x00000008,
    PE_GLOBAL_CLASS_D_STATS_INFO    = 0x00000010,
    PE_PER_STA_STATS_INFO           = 0x00000020,
    PE_STATS_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE //This and beyond are invalid values
}ePEStatsMask;


/******************************LINK LAYER Statitics**********************/

typedef int wifi_radio;
typedef int wifi_channel;
typedef int wifi_tx_rate;

/* channel operating width */
typedef enum {
    WIFI_CHAN_WIDTH_20    = 0,
    WIFI_CHAN_WIDTH_40    = 1,
    WIFI_CHAN_WIDTH_80    = 2,
    WIFI_CHAN_WIDTH_160   = 3,
    WIFI_CHAN_WIDTH_80P80 = 4,
    WIFI_CHAN_WIDTH_5     = 5,
    WIFI_CHAN_WIDTH_10    = 6,
} wifi_channel_width;

typedef enum {
    WIFI_DISCONNECTED = 0,
    WIFI_AUTHENTICATING = 1,
    WIFI_ASSOCIATING = 2,
    WIFI_ASSOCIATED = 3,
    WIFI_EAPOL_STARTED = 4,   // if done by firmware/driver
    WIFI_EAPOL_COMPLETED = 5, // if done by firmware/driver
} wifi_connection_state;

typedef enum {
    WIFI_ROAMING_IDLE = 0,
    WIFI_ROAMING_ACTIVE = 1,
} wifi_roam_state;

typedef enum {
    WIFI_INTERFACE_STA = 0,
    WIFI_INTERFACE_SOFTAP = 1,
    WIFI_INTERFACE_IBSS = 2,
    WIFI_INTERFACE_P2P_CLIENT = 3,
    WIFI_INTERFACE_P2P_GO = 4,
    WIFI_INTERFACE_NAN = 5,
    WIFI_INTERFACE_MESH = 6,
 } wifi_interface_mode;

#define WIFI_CAPABILITY_QOS          0x00000001     // set for QOS association
#define WIFI_CAPABILITY_PROTECTED    0x00000002     // set for protected association (802.11 beacon frame control protected bit set)
#define WIFI_CAPABILITY_INTERWORKING 0x00000004     // set if 802.11 Extended Capabilities element interworking bit is set
#define WIFI_CAPABILITY_HS20         0x00000008     // set for HS20 association
#define WIFI_CAPABILITY_SSID_UTF8    0x00000010     // set is 802.11 Extended Capabilities element UTF-8 SSID bit is set
#define WIFI_CAPABILITY_COUNTRY      0x00000020     // set is 802.11 Country Element is present

typedef PACKED_PRE struct PACKED_POST
{
    wifi_interface_mode      mode;                      // interface mode
    tANI_U8                  mac_addr[6];               // interface mac address (self)
    wifi_connection_state    state;                     // connection state (valid for STA, CLI only)
    wifi_roam_state          roaming;                   // roaming state
    tANI_U32                 capabilities;              // WIFI_CAPABILITY_XXX (self)
    tANI_U8                  ssid[33];                  // null terminated SSID
    tANI_U8                  bssid[6];                  // bssid
    tANI_U8                  ap_country_str[3];         // country string advertised by AP
    tANI_U8                  country_str[3];            // country string for this association
} wifi_interface_info;

/* channel information */
typedef PACKED_PRE struct PACKED_POST
{
    wifi_channel_width width;         // channel width (20, 40, 80, 80+80, 160)
    wifi_channel       center_freq;   // primary 20 MHz channel
    wifi_channel       center_freq0;  // center frequency (MHz) first segment
    wifi_channel       center_freq1;  // center frequency (MHz) second segment
} wifi_channel_info;

/* wifi rate info */
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 preamble   :3;   // 0: OFDM, 1:CCK, 2:HT 3:VHT 4..7 reserved
    tANI_U32 nss        :2;   // 0:1x1, 1:2x2, 3:3x3, 4:4x4
    tANI_U32 bw         :3;   // 0:20MHz, 1:40Mhz, 2:80Mhz, 3:160Mhz
    tANI_U32 rateMcsIdx :8;   // OFDM/CCK rate code would be as per ieee std in the units of 0.5mbps
                              // HT/VHT it would be mcs index
    tANI_U32 reserved  :16;   // reserved
    tANI_U32 bitrate;         // units of 100 Kbps
} wifi_rate;

/* channel statistics */
typedef PACKED_PRE struct PACKED_POST
{
    wifi_channel_info channel;                // channel
    tANI_U32          on_time;                // msecs the radio is awake (32 bits number accruing over time)
    tANI_U32          cca_busy_time;          // msecs the CCA register is busy (32 bits number accruing over time)
} wifi_channel_stats;

/* radio statistics */
typedef PACKED_PRE struct PACKED_POST
{
    wifi_radio radio;                      // wifi radio (if multiple radio supported)
    tANI_U32   on_time;                    // msecs the radio is awake (32 bits number accruing over time)
    tANI_U32   tx_time;                    // msecs the radio is transmitting (32 bits number accruing over time)
    tANI_U32   rx_time;                    // msecs the radio is in active receive (32 bits number accruing over time)
    tANI_U32   on_time_scan;               // msecs the radio is awake due to all scan (32 bits number accruing over time)
    tANI_U32   on_time_nbd;                // msecs the radio is awake due to NAN (32 bits number accruing over time)
    tANI_U32   on_time_gscan;              // msecs the radio is awake due to G?scan (32 bits number accruing over time)
    tANI_U32   on_time_roam_scan;          // msecs the radio is awake due to roam?scan (32 bits number accruing over time)
    tANI_U32   on_time_pno_scan;           // msecs the radio is awake due to PNO scan (32 bits number accruing over time)
    tANI_U32   on_time_hs20;               // msecs the radio is awake due to HS2.0 scans and GAS exchange (32 bits number accruing over time)
    tANI_U32   num_channels;               // number of channels
    wifi_channel_stats channels[1];        // channel statistics
} wifi_radio_stat;

/* per rate statistics */
typedef PACKED_PRE struct PACKED_POST
{
    wifi_rate rate;          // rate information  *
    tANI_U32 tx_mpdu;        // number of successfully transmitted data pkts (ACK rcvd) *
    tANI_U32 rx_mpdu;        // number of received data pkts
    tANI_U32 mpdu_lost;      // number of data packet losses (no ACK)
    tANI_U32 retries;        // total number of data pkt retries *
    tANI_U32 retries_short;  // number of short data pkt retries
    tANI_U32 retries_long;   // number of long data pkt retries
} wifi_rate_stat;

/* access categories */
typedef enum {
    WIFI_AC_VO  = 0,
    WIFI_AC_VI  = 1,
    WIFI_AC_BE  = 2,
    WIFI_AC_BK  = 3,
    WIFI_AC_MAX = 4,
} wifi_traffic_ac;

/* wifi peer type */
typedef enum
{
    WIFI_PEER_STA,
    WIFI_PEER_AP,
    WIFI_PEER_P2P_GO,
    WIFI_PEER_P2P_CLIENT,
    WIFI_PEER_NAN,
    WIFI_PEER_TDLS,
    WIFI_PEER_INVALID,
} wifi_peer_type;

/* per peer statistics */
typedef PACKED_PRE struct PACKED_POST
{
    wifi_peer_type type;                       // peer type (AP, TDLS, GO etc.)
    tANI_U8        peer_mac_address[6];        // mac address
    tANI_U32       capabilities;               // peer WIFI_CAPABILITY_XXX
    tANI_U32       num_rate;                   // number of rates
    wifi_rate_stat rate_stats[1];              // per rate statistics, number of entries  = num_rate
} wifi_peer_info;

/* per access category statistics */
typedef PACKED_PRE struct PACKED_POST
{
    wifi_traffic_ac ac;                  // access category (VI, VO, BE, BK)
    tANI_U32 tx_mpdu;                    // number of successfully transmitted unicast data pkts (ACK rcvd)
    tANI_U32 rx_mpdu;                    // number of received unicast mpdus
    tANI_U32 tx_mcast;                   // number of succesfully transmitted multicast data packets
                                         // STA case: implies ACK received from AP for the unicast packet in which mcast pkt was sent
    tANI_U32 rx_mcast;                   // number of received multicast data packets
    tANI_U32 rx_ampdu;                   // number of received unicast a-mpdus
    tANI_U32 tx_ampdu;                   // number of transmitted unicast a-mpdus
    tANI_U32 mpdu_lost;                  // number of data pkt losses (no ACK)
    tANI_U32 retries;                    // total number of data pkt retries
    tANI_U32 retries_short;              // number of short data pkt retries
    tANI_U32 retries_long;               // number of long data pkt retries
    tANI_U32 contention_time_min;        // data pkt min contention time (usecs)
    tANI_U32 contention_time_max;        // data pkt max contention time (usecs)
    tANI_U32 contention_time_avg;        // data pkt avg contention time (usecs)
    tANI_U32 contention_num_samples;     // num of data pkts used for contention statistics
} wifi_wmm_ac_stat;

/* Interface statistics - corresponding to 2nd most LSB in wifi statistics bitmap  for getting statistics */
typedef PACKED_PRE struct PACKED_POST
{
    wifi_interface_info info;                                       // current state of the interface
    tANI_U32            beacon_rx;                                  // access point beacon received count from connected AP
    tANI_U32            mgmt_rx;                                    // access point mgmt frames received count from connected AP (including Beacon)
    tANI_U32            mgmt_action_rx;                             // action frames received count
    tANI_U32            mgmt_action_tx;                             // action frames transmit count
    tANI_U32            rssi_mgmt;                                  // access Point Beacon and Management frames RSSI (averaged)
    tANI_U32            rssi_data;                                  // access Point Data Frames RSSI (averaged) from connected AP
    tANI_U32            rssi_ack;                                   // access Point ACK RSSI (averaged) from connected AP
    wifi_wmm_ac_stat    AccessclassStats[WIFI_AC_MAX];              // per ac data packet statistics
} wifi_iface_stat;

/* Peer statistics - corresponding to 3rd most LSB in wifi statistics bitmap  for getting statistics */
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32       num_peers;                            // number of peers
    wifi_peer_info peer_info[1];                         // per peer statistics
} wifi_peer_stat;

/* wifi statistics bitmap  for getting statistics */
#define WMI_LINK_STATS_RADIO          0x00000001
#define WMI_LINK_STATS_IFACE          0x00000002
#define WMI_LINK_STATS_ALL_PEER       0x00000004
#define WMI_LINK_STATS_PER_PEER       0x00000008

/* wifi statistics bitmap  for clearing statistics */
#define WIFI_STATS_RADIO              0x00000001      // all radio statistics
#define WIFI_STATS_RADIO_CCA          0x00000002      // cca_busy_time (within radio statistics)
#define WIFI_STATS_RADIO_CHANNELS     0x00000004      // all channel statistics (within radio statistics)
#define WIFI_STATS_RADIO_SCAN         0x00000008      // all scan statistics (within radio statistics)
#define WIFI_STATS_IFACE              0x00000010      // all interface statistics
#define WIFI_STATS_IFACE_TXRATE       0x00000020      // all tx rate statistics (within interface statistics)
#define WIFI_STATS_IFACE_AC           0x00000040      // all ac statistics (within interface statistics)
#define WIFI_STATS_IFACE_CONTENTION   0x00000080      // all contention (min, max, avg) statistics (within ac statisctics)


/*---------------------------------------------------------------------------
  Message definitons - All the messages below need to be packed
 ---------------------------------------------------------------------------*/

#if defined(__ANI_COMPILER_PRAGMA_PACK_STACK)
#pragma pack(push, 1)
#elif defined(__ANI_COMPILER_PRAGMA_PACK)
#pragma pack(1)
#else
#endif

/// Definition for HAL API Version.
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8                  revision;
    tANI_U8                  version;
    tANI_U8                  minor;
    tANI_U8                  major;
} tWcnssWlanVersion, *tpWcnssWlanVersion;

/// Definition for Encryption Keys
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8                  keyId;
    tANI_U8                  unicast;  // 0 for multicast
    tAniKeyDirection         keyDirection;
    tANI_U8                  keyRsc[WLAN_MAX_KEY_RSC_LEN];  // Usage is unknown
    tANI_U8                  paeRole;  // =1 for authenticator,=0 for supplicant
    tANI_U16                 keyLength;
    tANI_U8                  key[SIR_MAC_MAX_KEY_LENGTH];
} tSirKeys, *tpSirKeys;


//SetStaKeyParams Moving here since it is shared by configbss/setstakey msgs
typedef PACKED_PRE struct PACKED_POST
{
    /*STA Index*/
    tANI_U16        staIdx;

    /*Encryption Type used with peer*/
    tAniEdType      encType;

    /*STATIC/DYNAMIC - valid only for WEP*/
    tAniWepType     wepType;

    /*Default WEP key, valid only for static WEP, must between 0 and 3.*/
    tANI_U8         defWEPIdx;

    /* valid only for non-static WEP encyrptions */
    tSirKeys        key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];

    /*Control for Replay Count, 1= Single TID based replay count on Tx
      0 = Per TID based replay count on TX */
    tANI_U8         singleTidRc;

} tSetStaKeyParams, *tpSetStaKeyParams;



/* 4-byte control message header used by HAL*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalHostMsgType  msgType:16;
   tHalHostMsgVersion msgVersion:16;
   tANI_U32         msgLen;
} tHalMsgHeader, *tpHalMsgHeader;

/* Config format required by HAL for each CFG item*/
typedef PACKED_PRE struct PACKED_POST
{
   /* Cfg Id. The Id required by HAL is exported by HAL
    * in shared header file between UMAC and HAL.*/
   tANI_U16   uCfgId;

   /* Length of the Cfg. This parameter is used to go to next cfg
    * in the TLV format.*/
   tANI_U16   uCfgLen;

   /* Padding bytes for unaligned address's */
   tANI_U16   uCfgPadBytes;

   /* Reserve bytes for making cfgVal to align address */
   tANI_U16   uCfgReserve;

   /* Following the uCfgLen field there should be a 'uCfgLen' bytes
    * containing the uCfgValue ; tANI_U8 uCfgValue[uCfgLen] */
} tHalCfg, *tpHalCfg;

/*---------------------------------------------------------------------------
  WLAN_HAL_START_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST sHalMacStartParameters
{
    /* Drive Type - Production or FTM etc */
    tDriverType  driverType;

    /*Length of the config buffer*/
    tANI_U32  uConfigBufferLen;

    /* Following this there is a TLV formatted buffer of length
     * "uConfigBufferLen" bytes containing all config values.
     * The TLV is expected to be formatted like this:
     * 0           15            31           31+CFG_LEN-1        length-1
     * |   CFG_ID   |   CFG_LEN   |   CFG_BODY    |  CFG_ID  |......|
     */
} tHalMacStartParameters, *tpHalMacStartParameters;

typedef PACKED_PRE struct PACKED_POST
{
   /* Note: The length specified in tHalMacStartReqMsg messages should be
    * header.msgLen = sizeof(tHalMacStartReqMsg) + uConfigBufferLen */
   tHalMsgHeader header;
   tHalMacStartParameters startReqParams;
}  tHalMacStartReqMsg, *tpHalMacStartReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_START_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST sHalMacStartRspParameters
{
   /*success or failure */
   tANI_U16  status;

   /*Max number of STA supported by the device*/
   tANI_U8     ucMaxStations;

   /*Max number of BSS supported by the device*/
   tANI_U8     ucMaxBssids;

   /*API Version */
   tWcnssWlanVersion wcnssWlanVersion;

   /*CRM build information */
   tANI_U8     wcnssCrmVersionString[WLAN_HAL_VERSION_LENGTH];

   /*hardware/chipset/misc version information */
   tANI_U8     wcnssWlanVersionString[WLAN_HAL_VERSION_LENGTH];

} tHalMacStartRspParams, *tpHalMacStartRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacStartRspParams startRspParams;
}  tHalMacStartRspMsg, *tpHalMacStartRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_STOP_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*The reason for which the device is being stopped*/
  tHalStopType   reason;

}tHalMacStopReqParams, *tpHalMacStopReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacStopReqParams stopReqParams;
}  tHalMacStopReqMsg, *tpHalMacStopReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_STOP_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

}tHalMacStopRspParams, *tpHalMacStopRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacStopRspParams stopRspParams;
}  tHalMacStopRspMsg, *tpHalMacStopRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_UPDATE_CFG_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Length of the config buffer. Allows UMAC to update multiple CFGs */
    tANI_U32  uConfigBufferLen;

    /* Following this there is a TLV formatted buffer of length
     * "uConfigBufferLen" bytes containing all config values.
     * The TLV is expected to be formatted like this:
     * 0           15            31           31+CFG_LEN-1        length-1
     * |   CFG_ID   |   CFG_LEN   |   CFG_BODY    |  CFG_ID  |......|
     */
} tHalUpdateCfgReqParams, *tpHalUpdateCfgReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   /* Note: The length specified in tHalUpdateCfgReqMsg messages should be
    * header.msgLen = sizeof(tHalUpdateCfgReqMsg) + uConfigBufferLen */
   tHalMsgHeader header;
   tHalUpdateCfgReqParams updateCfgReqParams;
}  tHalUpdateCfgReqMsg, *tpHalUpdateCfgReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_UPDATE_CFG_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /* success or failure */
  tANI_U32   status;

}tHalUpdateCfgRspParams, *tpHalUpdateCfgRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalUpdateCfgRspParams updateCfgRspParams;
}  tHalUpdateCfgRspMsg, *tpHalUpdateCfgRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_INIT_SCAN_REQ
---------------------------------------------------------------------------*/

/// Frame control field format (2 bytes)
typedef  __ani_attr_pre_packed struct sSirMacFrameCtl
{

#ifndef ANI_LITTLE_BIT_ENDIAN

    tANI_U8 subType :4;
    tANI_U8 type :2;
    tANI_U8 protVer :2;

    tANI_U8 order :1;
    tANI_U8 wep :1;
    tANI_U8 moreData :1;
    tANI_U8 powerMgmt :1;
    tANI_U8 retry :1;
    tANI_U8 moreFrag :1;
    tANI_U8 fromDS :1;
    tANI_U8 toDS :1;

#else

    tANI_U8 protVer :2;
    tANI_U8 type :2;
    tANI_U8 subType :4;

    tANI_U8 toDS :1;
    tANI_U8 fromDS :1;
    tANI_U8 moreFrag :1;
    tANI_U8 retry :1;
    tANI_U8 powerMgmt :1;
    tANI_U8 moreData :1;
    tANI_U8 wep :1;
    tANI_U8 order :1;

#endif

} __ani_attr_packed  tSirMacFrameCtl, *tpSirMacFrameCtl;

/// Sequence control field
typedef __ani_attr_pre_packed struct sSirMacSeqCtl
{
    tANI_U8 fragNum : 4;
    tANI_U8 seqNumLo : 4;
    tANI_U8 seqNumHi : 8;
} __ani_attr_packed tSirMacSeqCtl, *tpSirMacSeqCtl;

/// Management header format
typedef __ani_attr_pre_packed struct sSirMacMgmtHdr
{
    tSirMacFrameCtl fc;
    tANI_U8           durationLo;
    tANI_U8           durationHi;
    tANI_U8              da[6];
    tANI_U8              sa[6];
    tANI_U8              bssId[6];
    tSirMacSeqCtl   seqControl;
} __ani_attr_packed tSirMacMgmtHdr, *tpSirMacMgmtHdr;

/// Scan Entry to hold active BSS idx's
typedef __ani_attr_pre_packed struct sSirScanEntry
{
    tANI_U8 bssIdx[HAL_NUM_BSSID];
    tANI_U8 activeBSScnt;
}__ani_attr_packed tSirScanEntry, *ptSirScanEntry;

typedef PACKED_PRE struct PACKED_POST {

    /*LEARN - AP Role
      SCAN - STA Role*/
    eHalSysMode scanMode;

    /*BSSID of the BSS*/
    tSirMacAddr bssid;

    /*Whether BSS needs to be notified*/
    tANI_U8 notifyBss;

    /*Kind of frame to be used for notifying the BSS (Data Null, QoS Null, or
      CTS to Self). Must always be a valid frame type.*/
    tANI_U8 frameType;

    /*UMAC has the option of passing the MAC frame to be used for notifying
      the BSS. If non-zero, HAL will use the MAC frame buffer pointed to by
      macMgmtHdr. If zero, HAL will generate the appropriate MAC frame based on
      frameType.*/
    tANI_U8 frameLength;

    /* Following the framelength there is a MAC frame buffer if frameLength
       is non-zero. */
    tSirMacMgmtHdr macMgmtHdr;

    /*Entry to hold number of active BSS idx's*/
    tSirScanEntry scanEntry;

} tInitScanParams, * tpInitScanParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tInitScanParams initScanParams;
}  tHalInitScanReqMsg, *tpHalInitScanReqMsg;

typedef PACKED_PRE struct PACKED_POST {

    /*LEARN - AP Role
      SCAN - STA Role*/
    eHalSysMode scanMode;

    /*BSSID of the BSS*/
    tSirMacAddr bssid;

    /*Whether BSS needs to be notified*/
    tANI_U8 notifyBss;

    /*Kind of frame to be used for notifying the BSS (Data Null, QoS Null, or
      CTS to Self). Must always be a valid frame type.*/
    tANI_U8 frameType;

    /*UMAC has the option of passing the MAC frame to be used for notifying
      the BSS. If non-zero, HAL will use the MAC frame buffer pointed to by
      macMgmtHdr. If zero, HAL will generate the appropriate MAC frame based on
      frameType.*/
    tANI_U8 frameLength;

    /* Following the framelength there is a MAC frame buffer if frameLength
       is non-zero. */
    tSirMacMgmtHdr macMgmtHdr;

    /*Entry to hold number of active BSS idx's*/
    tSirScanEntry scanEntry;

    /* Single NoA usage in Scanning */
    tANI_U8 useNoA;

    /* Indicates the scan duration (in ms) */
    tANI_U16 scanDuration;

} tInitScanConParams, * tpInitScanConParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tInitScanConParams initScanParams;
}  tHalInitScanConReqMsg, *tpHalInitScanConReqMsg;


/*---------------------------------------------------------------------------
  WLAN_HAL_INIT_SCAN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

}tHalInitScanRspParams, *tpHalInitScanRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalInitScanRspParams initScanRspParams;
}  tHalInitScanRspMsg, *tpHalInitScanRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_START_SCAN_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /*Indicates the channel to scan*/
   tANI_U8 scanChannel;

 } tStartScanParams, * tpStartScanParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tStartScanParams startScanParams;
}  tHalStartScanReqMsg, *tpHalStartScanReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalMotionEventReqMsg, *tpHalMotionEventReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_START_SCAN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

  tANI_U32 startTSF[2];
  tPowerdBm txMgmtPower;

}tHalStartScanRspParams, *tpHalStartScanRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalStartScanRspParams startScanRspParams;
}  tHalStartScanRspMsg, *tpHalStartScanRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_END_SCAN_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /*Indicates the channel to stop scanning.  Not used really. But retained
    for symmetry with "start Scan" message. It can also help in error
    check if needed.*/
    tANI_U8 scanChannel;

} tEndScanParams, *tpEndScanParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tEndScanParams endScanParams;
}  tHalEndScanReqMsg, *tpHalEndScanReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_END_SCAN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

}tHalEndScanRspParams, *tpHalEndScanRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEndScanRspParams endScanRspParams;
}  tHalEndScanRspMsg, *tpHalEndScanRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_FINISH_SCAN_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Identifies the operational state of the AP/STA
     * LEARN - AP Role SCAN - STA Role */
    eHalSysMode scanMode;

    /*Operating channel to tune to.*/
    tANI_U8 currentOperChannel;

    /*Channel Bonding state If 20/40 MHz is operational, this will indicate the
      40 MHz extension channel in combination with the control channel*/
    ePhyChanBondState cbState;

    /*BSSID of the BSS*/
    tSirMacAddr bssid;

    /*Whether BSS needs to be notified*/
    tANI_U8 notifyBss;

    /*Kind of frame to be used for notifying the BSS (Data Null, QoS Null, or
     CTS to Self). Must always be a valid frame type.*/
    tANI_U8 frameType;

    /*UMAC has the option of passing the MAC frame to be used for notifying
      the BSS. If non-zero, HAL will use the MAC frame buffer pointed to by
      macMgmtHdr. If zero, HAL will generate the appropriate MAC frame based on
      frameType.*/
    tANI_U8 frameLength;

    /*Following the framelength there is a MAC frame buffer if frameLength
      is non-zero.*/
    tSirMacMgmtHdr macMgmtHdr;

    /*Entry to hold number of active BSS idx's*/
    tSirScanEntry scanEntry;

} tFinishScanParams, *tpFinishScanParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tFinishScanParams finishScanParams;
}  tHalFinishScanReqMsg, *tpHalFinishScanReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_FINISH_SCAN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

}tHalFinishScanRspParams, *tpHalFinishScanRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalFinishScanRspParams finishScanRspParams;
}  tHalFinishScanRspMsg, *tpHalFinishScanRspMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tSetStaKeyParams keyParams;
   uint8 pn[6];
} tHalEncConfigParams;

typedef PACKED_PRE struct PACKED_POST
{
   uint16 length;
   uint8  data[DISA_MAX_PAYLOAD_SIZE];
} tHalDisaPayload;

typedef PACKED_PRE struct PACKED_POST
{
#ifdef BYTE_ORDER_BIG_ENDIAN
    uint8   reserved1          : 1;
    uint8   ackpolicy          : 2;
    uint8   eosp               : 1;
    uint8   tid                : 4;

    uint8   appsbufferstate    : 8;
#else
    uint8   appsbufferstate    : 8;

    uint8   tid                : 4;
    uint8   eosp               : 1;
    uint8   ackpolicy          : 2;
    uint8   reserved1          : 1;
#endif
} tHalQosCtrlFieldType;

typedef PACKED_PRE struct PACKED_POST
 {
#ifdef  BYTE_ORDER_BIG_ENDIAN
    uint16 subtype   : 4;
    uint16 type      : 2;
    uint16 protocol  : 2;

    uint16 order     : 1;
    uint16 wep       : 1;
    uint16 moredata  : 1;
    uint16 pm        : 1;
    uint16 retry     : 1;
    uint16 morefrag  : 1;
    uint16 fromds    : 1;
    uint16 tods      : 1;
#else

    uint16 tods      : 1;
    uint16 fromds    : 1;
    uint16 morefrag  : 1;
    uint16 retry     : 1;
    uint16 pm        : 1;
    uint16 moredata  : 1;
    uint16 wep       : 1;
    uint16 order     : 1;

    uint16 protocol  : 2;
    uint16 type      : 2;
    uint16 subtype   : 4;
#endif
} tHalFrmCtrlType;

typedef PACKED_PRE struct PACKED_POST
{
   /* Frame control field */
   tHalFrmCtrlType fc;
   /* Duration ID */
   uint16 usDurationId;
   /* Address 1 field */
   uint8 vA1[HAL_MAC_ADDR_LEN];
   /* Address 2 field */
   uint8 vA2[HAL_MAC_ADDR_LEN];
   /* Address 3 field */
   uint8 vA3[HAL_MAC_ADDR_LEN];
   /* Sequence control field */
   uint16 seqNum;
   /* Optional A4 address */
   uint8 optvA4[HAL_MAC_ADDR_LEN];
   /* Optional QOS control field */
   tHalQosCtrlFieldType usQosCtrl;
} tHal80211Header;

typedef PACKED_PRE struct PACKED_POST
{
   tHal80211Header macHeader;
   tHalEncConfigParams encParams;
   tHalDisaPayload data;
} tSetEncryptedDataParams, *tpSetEncryptedDataParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetEncryptedDataParams encryptedDataParams;
}  tSetEncryptedDataReqMsg, *tpSetEncryptedDataReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 status;
   tHalDisaPayload encryptedPayload;
} tSetEncryptedDataRspParams, *tpSetEncryptedDataRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetEncryptedDataRspParams encryptedDataRspParams;
}  tSetEncryptedDataRspMsg, *tpSetEncryptedDataRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_CONFIG_STA_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST {
    /*
    * For Self STA Entry: this represents Self Mode.
    * For Peer Stations, this represents the mode of the peer.
    * On Station:
    * --this mode is updated when PE adds the Self Entry.
    * -- OR when PE sends 'ADD_BSS' message and station context in BSS is used to indicate the mode of the AP.
    * ON AP:
    * -- this mode is updated when PE sends 'ADD_BSS' and Sta entry for that BSS is used
    *     to indicate the self mode of the AP.
    * -- OR when a station is associated, PE sends 'ADD_STA' message with this mode updated.
    */

    tStaRateMode        opRateMode;
    // 11b, 11a and aniLegacyRates are IE rates which gives rate in unit of 500Kbps
    tANI_U16             llbRates[SIR_NUM_11B_RATES];
    tANI_U16             llaRates[SIR_NUM_11A_RATES];
    tANI_U16             aniLegacyRates[SIR_NUM_POLARIS_RATES];
    tANI_U16             reserved;

    //Taurus only supports 26 Titan Rates(no ESF/concat Rates will be supported)
    //First 26 bits are reserved for those Titan rates and
    //the last 4 bits(bit28-31) for Taurus, 2(bit26-27) bits are reserved.
    tANI_U32             aniEnhancedRateBitmap; //Titan and Taurus Rates

    /*
    * 0-76 bits used, remaining reserved
    * bits 0-15 and 32 should be set.
    */
    tANI_U8 supportedMCSSet[SIR_MAC_MAX_SUPPORTED_MCS_SET];

    /*
     * RX Highest Supported Data Rate defines the highest data
     * rate that the STA is able to receive, in unites of 1Mbps.
     * This value is derived from "Supported MCS Set field" inside
     * the HT capability element.
     */
    tANI_U16 rxHighestDataRate;

} tSirSupportedRates, *tpSirSupportedRates;

typedef PACKED_PRE struct PACKED_POST
{
    /*BSSID of STA*/
    tSirMacAddr bssId;

    /*ASSOC ID, as assigned by UMAC*/
    tANI_U16 assocId;

    /* STA entry Type: 0 - Self, 1 - Other/Peer, 2 - BSSID, 3 - BCAST */
    tANI_U8 staType;

    /*Short Preamble Supported.*/
    tANI_U8 shortPreambleSupported;

    /*MAC Address of STA*/
    tSirMacAddr staMac;

    /*Listen interval of the STA*/
    tANI_U16 listenInterval;

    /*Support for 11e/WMM*/
    tANI_U8 wmmEnabled;

    /*11n HT capable STA*/
    tANI_U8 htCapable;

    /*TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz*/
    tANI_U8 txChannelWidthSet;

    /*RIFS mode 0 - NA, 1 - Allowed */
    tANI_U8 rifsMode;

    /*L-SIG TXOP Protection mechanism
      0 - No Support, 1 - Supported
      SG - there is global field */
    tANI_U8 lsigTxopProtection;

    /*Max Ampdu Size supported by STA. TPE programming.
      0 : 8k , 1 : 16k, 2 : 32k, 3 : 64k */
    tANI_U8 maxAmpduSize;

    /*Max Ampdu density. Used by RA.  3 : 0~7 : 2^(11nAMPDUdensity -4)*/
    tANI_U8 maxAmpduDensity;

    /*Max AMSDU size 1 : 3839 bytes, 0 : 7935 bytes*/
    tANI_U8 maxAmsduSize;

    /*Short GI support for 40Mhz packets*/
    tANI_U8 fShortGI40Mhz;

    /*Short GI support for 20Mhz packets*/
    tANI_U8 fShortGI20Mhz;

    /*Robust Management Frame (RMF) enabled/disabled*/
    tANI_U8 rmfEnabled;

    /* The unicast encryption type in the association */
    tANI_U32 encryptType;

    /*HAL should update the existing STA entry, if this flag is set. UMAC
      will set this flag in case of RE-ASSOC, where we want to reuse the old
      STA ID. 0 = Add, 1 = Update*/
    tANI_U8 action;

    /*U-APSD Flags: 1b per AC.  Encoded as follows:
       b7 b6 b5 b4 b3 b2 b1 b0 =
       X  X  X  X  BE BK VI VO */
    tANI_U8 uAPSD;

    /*Max SP Length*/
    tANI_U8 maxSPLen;

    /*11n Green Field preamble support
      0 - Not supported, 1 - Supported */
    tANI_U8 greenFieldCapable;

    /*MIMO Power Save mode*/
    tSirMacHTMIMOPowerSaveState mimoPS;

    /*Delayed BA Support*/
    tANI_U8 delayedBASupport;

    /*Max AMPDU duration in 32us*/
    tANI_U8 us32MaxAmpduDuration;

    /*HT STA should set it to 1 if it is enabled in BSS. HT STA should set
      it to 0 if AP does not support it. This indication is sent to HAL and
      HAL uses this flag to pickup up appropriate 40Mhz rates.*/
    tANI_U8 fDsssCckMode40Mhz;

    /* Valid STA Idx when action=Update. Set to 0xFF when invalid!
       Retained for backward compalibity with existing HAL code*/
    tANI_U8 staIdx;

    /* BSSID of BSS to which station is associated. Set to 0xFF when invalid.
       Retained for backward compalibity with existing HAL code*/
    tANI_U8 bssIdx;

    tANI_U8  p2pCapableSta;

    /*Reserved to align next field on a dword boundary*/
    tANI_U8  reserved;

    /*These rates are the intersection of peer and self capabilities.*/
    tSirSupportedRates supportedRates;

} tConfigStaParams, *tpConfigStaParams;

/*------------------------------------------------------------------------
 * WLAN_HAL_CONFIG_STA_REQ
 * ----------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST {
    /*
    * For Self STA Entry: this represents Self Mode.
    * For Peer Stations, this represents the mode of the peer.
    * On Station:
    * --this mode is updated when PE adds the Self Entry.
    * -- OR when PE sends 'ADD_BSS' message and station context in BSS is used to indicate the mode of the AP.
    * ON AP:
    * -- this mode is updated when PE sends 'ADD_BSS' and Sta entry for that BSS is used
    *     to indicate the self mode of the AP.
    * -- OR when a station is associated, PE sends 'ADD_STA' message with this mode updated.
    */

    tStaRateMode        opRateMode;
    // 11b, 11a and aniLegacyRates are IE rates which gives rate in unit of 500Kbps
    tANI_U16             llbRates[SIR_NUM_11B_RATES];
    tANI_U16             llaRates[SIR_NUM_11A_RATES];
    tANI_U16             aniLegacyRates[SIR_NUM_POLARIS_RATES];
    tANI_U16             reserved;

    //Taurus only supports 26 Titan Rates(no ESF/concat Rates will be supported)
    //First 26 bits are reserved for those Titan rates and
    //the last 4 bits(bit28-31) for Taurus, 2(bit26-27) bits are reserved.
    tANI_U32             aniEnhancedRateBitmap; //Titan and Taurus Rates

    /*
    * 0-76 bits used, remaining reserved
    * bits 0-15 and 32 should be set.
    */
    tANI_U8 supportedMCSSet[SIR_MAC_MAX_SUPPORTED_MCS_SET];

    /*
     * RX Highest Supported Data Rate defines the highest data
     * rate that the STA is able to receive, in unites of 1Mbps.
     * This value is derived from "Supported MCS Set field" inside
     * the HT capability element.
     */
    tANI_U16 rxHighestDataRate;

    /* Indicates the Maximum MCS that can be received for each number
        * of spacial streams */
    tANI_U16 vhtRxMCSMap;

    /*Indicate the highest VHT data rate that the STA is able to receive*/
    tANI_U16 vhtRxHighestDataRate;

    /* Indicates the Maximum MCS that can be transmitted  for each number
         * of spacial streams */
    tANI_U16 vhtTxMCSMap;

    /*Indicate the highest VHT data rate that the STA is able to transmit*/
    tANI_U16 vhtTxHighestDataRate;

} tSirSupportedRates_V1, *tpSirSupportedRates_V1;

typedef PACKED_PRE struct PACKED_POST
{
    /*BSSID of STA*/
    tSirMacAddr bssId;

    /*ASSOC ID, as assigned by UMAC*/
    tANI_U16 assocId;

    /* STA entry Type: 0 - Self, 1 - Other/Peer, 2 - BSSID, 3 - BCAST */
    tANI_U8 staType;

    /*Short Preamble Supported.*/
    tANI_U8 shortPreambleSupported;

    /*MAC Address of STA*/
    tSirMacAddr staMac;

    /*Listen interval of the STA*/
    tANI_U16 listenInterval;

    /*Support for 11e/WMM*/
    tANI_U8 wmmEnabled;

    /*11n HT capable STA*/
    tANI_U8 htCapable;

    /*TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz*/
    tANI_U8 txChannelWidthSet;

    /*RIFS mode 0 - NA, 1 - Allowed */
    tANI_U8 rifsMode;

    /*L-SIG TXOP Protection mechanism
      0 - No Support, 1 - Supported
      SG - there is global field */
    tANI_U8 lsigTxopProtection;

    /*Max Ampdu Size supported by STA. TPE programming.
      0 : 8k , 1 : 16k, 2 : 32k, 3 : 64k */
    tANI_U8 maxAmpduSize;

    /*Max Ampdu density. Used by RA.  3 : 0~7 : 2^(11nAMPDUdensity -4)*/
    tANI_U8 maxAmpduDensity;

    /*Max AMSDU size 1 : 3839 bytes, 0 : 7935 bytes*/
    tANI_U8 maxAmsduSize;

    /*Short GI support for 40Mhz packets*/
    tANI_U8 fShortGI40Mhz;

    /*Short GI support for 20Mhz packets*/
    tANI_U8 fShortGI20Mhz;

    /*Robust Management Frame (RMF) enabled/disabled*/
    tANI_U8 rmfEnabled;

    /* The unicast encryption type in the association */
    tANI_U32 encryptType;

    /*HAL should update the existing STA entry, if this flag is set. UMAC
      will set this flag in case of RE-ASSOC, where we want to reuse the old
      STA ID. 0 = Add, 1 = Update*/
    tANI_U8 action;

    /*U-APSD Flags: 1b per AC.  Encoded as follows:
       b7 b6 b5 b4 b3 b2 b1 b0 =
       X  X  X  X  BE BK VI VO */
    tANI_U8 uAPSD;

    /*Max SP Length*/
    tANI_U8 maxSPLen;

    /*11n Green Field preamble support
      0 - Not supported, 1 - Supported */
    tANI_U8 greenFieldCapable;

    /*MIMO Power Save mode*/
    tSirMacHTMIMOPowerSaveState mimoPS;

    /*Delayed BA Support*/
    tANI_U8 delayedBASupport;

    /*Max AMPDU duration in 32us*/
    tANI_U8 us32MaxAmpduDuration;

    /*HT STA should set it to 1 if it is enabled in BSS. HT STA should set
      it to 0 if AP does not support it. This indication is sent to HAL and
      HAL uses this flag to pickup up appropriate 40Mhz rates.*/
    tANI_U8 fDsssCckMode40Mhz;

    /* Valid STA Idx when action=Update. Set to 0xFF when invalid!
       Retained for backward compalibity with existing HAL code*/
    tANI_U8 staIdx;

    /* BSSID of BSS to which station is associated. Set to 0xFF when invalid.
       Retained for backward compalibity with existing HAL code*/
    tANI_U8 bssIdx;

    tANI_U8  p2pCapableSta;

    /*Reserved to align next field on a dword boundary*/
    tANI_U8 htLdpcEnabled:1;
    tANI_U8 vhtLdpcEnabled:1;
    tANI_U8 vhtTxBFEnabled:1;
    tANI_U8 vhtTxMUBformeeCapable:1;
    tANI_U8 reserved:4;

        /*These rates are the intersection of peer and self capabilities.*/
    tSirSupportedRates_V1 supportedRates;

    tANI_U8  vhtCapable;
    tANI_U8  vhtTxChannelWidthSet;

} tConfigStaParams_V1, *tpConfigStaParams_V1;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   PACKED_PRE union PACKED_POST {
   tConfigStaParams configStaParams;
    tConfigStaParams_V1 configStaParams_V1;
   } uStaParams;
}  tConfigStaReqMsg, *tpConfigStaReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_CONFIG_STA_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

  /* Station index; valid only when 'status' field value SUCCESS */
  tANI_U8 staIdx;

  /* BSSID Index of BSS to which the station is associated */
  tANI_U8 bssIdx;

  /* DPU Index for PTK */
  tANI_U8 dpuIndex;

  /* DPU Index for GTK */
  tANI_U8 bcastDpuIndex;

  /*DPU Index for IGTK  */
  tANI_U8 bcastMgmtDpuIdx;

  /*PTK DPU signature*/
  tANI_U8 ucUcastSig;

  /*GTK DPU isignature*/
  tANI_U8 ucBcastSig;

  /* IGTK DPU signature*/
  tANI_U8 ucMgmtSig;

  tANI_U8  p2pCapableSta;

}tConfigStaRspParams, *tpConfigStaRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tConfigStaRspParams configStaRspParams;
}tConfigStaRspMsg, *tpConfigStaRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_DELETE_STA_REQ
---------------------------------------------------------------------------*/

/* Delete STA Request params */
typedef PACKED_PRE struct PACKED_POST
{
   /* Index of STA to delete */
   tANI_U8    staIdx;
} tDeleteStaParams, *tpDeleteStaParams;

/* Delete STA Request message*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDeleteStaParams delStaParams;
}  tDeleteStaReqMsg, *tpDeleteStaReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_DELETE_STA_RSP
---------------------------------------------------------------------------*/

/* Delete STA Response Params */
typedef PACKED_PRE struct PACKED_POST
{
   /*success or failure */
   tANI_U32   status;

   /* Index of STA deleted */
   tANI_U8    staId;
} tDeleteStaRspParams, *tpDeleteStaRspParams;

/* Delete STA Response message*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDeleteStaRspParams delStaRspParams;
}  tDeleteStaRspMsg, *tpDeleteStaRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_CONFIG_BSS_REQ
---------------------------------------------------------------------------*/

//12 Bytes long because this structure can be used to represent rate
//and extended rate set IEs. The parser assume this to be at least 12
typedef __ani_attr_pre_packed struct sSirMacRateSet
{
    tANI_U8  numRates;
    tANI_U8  rate[SIR_MAC_RATESET_EID_MAX];
} __ani_attr_packed tSirMacRateSet;

// access category record
typedef __ani_attr_pre_packed struct sSirMacAciAifsn
{
#ifndef ANI_LITTLE_BIT_ENDIAN
    tANI_U8  rsvd  : 1;
    tANI_U8  aci   : 2;
    tANI_U8  acm   : 1;
    tANI_U8  aifsn : 4;
#else
    tANI_U8  aifsn : 4;
    tANI_U8  acm   : 1;
    tANI_U8  aci   : 2;
    tANI_U8  rsvd  : 1;
#endif
} __ani_attr_packed tSirMacAciAifsn;

// contention window size
typedef __ani_attr_pre_packed struct sSirMacCW
{
#ifndef ANI_LITTLE_BIT_ENDIAN
    tANI_U8  max : 4;
    tANI_U8  min : 4;
#else
    tANI_U8  min : 4;
    tANI_U8  max : 4;
#endif
} __ani_attr_packed tSirMacCW;

typedef __ani_attr_pre_packed struct sSirMacEdcaParamRecord
{
    tSirMacAciAifsn  aci;
    tSirMacCW        cw;
    tANI_U16         txoplimit;
} __ani_attr_packed tSirMacEdcaParamRecord;

typedef __ani_attr_pre_packed struct sSirMacSSid
{
    tANI_U8        length;
    tANI_U8        ssId[32];
} __ani_attr_packed tSirMacSSid;

// Concurrency role.  These are generic IDs that identify the various roles
// in the software system.
typedef enum {
    HAL_STA_MODE=0,
    HAL_STA_SAP_MODE=1, // to support softAp mode . This is misleading. It means AP MODE only.
    HAL_P2P_CLIENT_MODE,
    HAL_P2P_GO_MODE,
    HAL_MONITOR_MODE,
} tHalConMode;

//This is a bit pattern to be set for each mode
//bit 0 - sta mode
//bit 1 - ap mode
//bit 2 - p2p client mode
//bit 3 - p2p go mode
typedef enum
{
    HAL_STA=1,
    HAL_SAP=2,
    HAL_STA_SAP=3, //to support sta, softAp  mode . This means STA+AP mode
    HAL_P2P_CLIENT=4,
    HAL_P2P_GO=8,
    HAL_MAX_CONCURRENCY_PERSONA=4
} tHalConcurrencyMode;

// IFACE PERSONA for different Operating modes
typedef enum
{
    HAL_IFACE_UNKNOWN=0,
    HAL_IFACE_STA_MODE=1,
    HAL_IFACE_P2P_MODE=2,
    HAL_IFACE_MAX=0x7FFFFFFF,
} tHalIfacePersona;

typedef PACKED_PRE struct PACKED_POST
{
    /* BSSID */
    tSirMacAddr bssId;

    /* Self Mac Address */
    tSirMacAddr  selfMacAddr;

    /* BSS type */
    tSirBssType bssType;

    /*Operational Mode: AP =0, STA = 1*/
    tANI_U8 operMode;

    /*Network Type*/
    tSirNwType nwType;

    /*Used to classify PURE_11G/11G_MIXED to program MTU*/
    tANI_U8 shortSlotTimeSupported;

    /*Co-exist with 11a STA*/
    tANI_U8 llaCoexist;

    /*Co-exist with 11b STA*/
    tANI_U8 llbCoexist;

    /*Co-exist with 11g STA*/
    tANI_U8 llgCoexist;

    /*Coexistence with 11n STA*/
    tANI_U8 ht20Coexist;

    /*Non GF coexist flag*/
    tANI_U8 llnNonGFCoexist;

    /*TXOP protection support*/
    tANI_U8 fLsigTXOPProtectionFullSupport;

    /*RIFS mode*/
    tANI_U8 fRIFSMode;

    /*Beacon Interval in TU*/
    tSirMacBeaconInterval beaconInterval;

    /*DTIM period*/
    tANI_U8 dtimPeriod;

    /*TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz*/
    tANI_U8 txChannelWidthSet;

    /*Operating channel*/
    tANI_U8 currentOperChannel;

    /*Extension channel for channel bonding*/
    tANI_U8 currentExtChannel;

    /*Reserved to align next field on a dword boundary*/
    tANI_U8 reserved;

    /*SSID of the BSS*/
    tSirMacSSid ssId;

    /*HAL should update the existing BSS entry, if this flag is set.
      UMAC will set this flag in case of reassoc, where we want to resue the
      the old BSSID and still return success 0 = Add, 1 = Update*/
    tANI_U8 action;

    /* MAC Rate Set */
    tSirMacRateSet rateSet;

    /*Enable/Disable HT capabilities of the BSS*/
    tANI_U8 htCapable;

    // Enable/Disable OBSS protection
    tANI_U8 obssProtEnabled;

    /*RMF enabled/disabled*/
    tANI_U8 rmfEnabled;

    /*HT Operating Mode operating mode of the 802.11n STA*/
    tSirMacHTOperatingMode htOperMode;

    /*Dual CTS Protection: 0 - Unused, 1 - Used*/
    tANI_U8 dualCTSProtection;

    /* Probe Response Max retries */
    tANI_U8   ucMaxProbeRespRetryLimit;

    /* To Enable Hidden ssid */
    tANI_U8   bHiddenSSIDEn;

    /* To Enable Disable FW Proxy Probe Resp */
    tANI_U8   bProxyProbeRespEn;

    /* Boolean to indicate if EDCA params are valid. UMAC might not have valid
       EDCA params or might not desire to apply EDCA params during config BSS.
       0 implies Not Valid ; Non-Zero implies valid*/
    tANI_U8   edcaParamsValid;

    /*EDCA Parameters for Best Effort Access Category*/
    tSirMacEdcaParamRecord acbe;

    /*EDCA Parameters forBackground Access Category*/
    tSirMacEdcaParamRecord acbk;

    /*EDCA Parameters for Video Access Category*/
    tSirMacEdcaParamRecord acvi;

    /*EDCA Parameters for Voice Access Category*/
    tSirMacEdcaParamRecord acvo;

#ifdef WLAN_FEATURE_VOWIFI_11R
    tANI_U8 extSetStaKeyParamValid; //Ext Bss Config Msg if set
    tSetStaKeyParams extSetStaKeyParam;  //SetStaKeyParams for ext bss msg
#endif

    /* Persona for the BSS can be STA,AP,GO,CLIENT value same as tHalConMode */
    tANI_U8   halPersona;

    tANI_U8 bSpectrumMgtEnable;

    /*HAL fills in the tx power used for mgmt frames in txMgmtPower*/
    tANI_S8     txMgmtPower;
    /*maxTxPower has max power to be used after applying the power constraint if any */
    tANI_S8     maxTxPower;
    /*Context of the station being added in HW
      Add a STA entry for "itself" -
      On AP  - Add the AP itself in an "STA context"
      On STA - Add the AP to which this STA is joining in an "STA context" */
    tConfigStaParams staContext;
} tConfigBssParams, * tpConfigBssParams;


/*--------------------------------------------------------------------------
 * WLAN_HAL_CONFIG_BSS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* BSSID */
    tSirMacAddr bssId;

    /* Self Mac Address */
    tSirMacAddr  selfMacAddr;

    /* BSS type */
    tSirBssType bssType;

    /*Operational Mode: AP =0, STA = 1*/
    tANI_U8 operMode;

    /*Network Type*/
    tSirNwType nwType;

    /*Used to classify PURE_11G/11G_MIXED to program MTU*/
    tANI_U8 shortSlotTimeSupported;

    /*Co-exist with 11a STA*/
    tANI_U8 llaCoexist;

    /*Co-exist with 11b STA*/
    tANI_U8 llbCoexist;

    /*Co-exist with 11g STA*/
    tANI_U8 llgCoexist;

    /*Coexistence with 11n STA*/
    tANI_U8 ht20Coexist;

    /*Non GF coexist flag*/
    tANI_U8 llnNonGFCoexist;

    /*TXOP protection support*/
    tANI_U8 fLsigTXOPProtectionFullSupport;
    /*RIFS mode*/
    tANI_U8 fRIFSMode;

    /*Beacon Interval in TU*/
    tSirMacBeaconInterval beaconInterval;

    /*DTIM period*/
    tANI_U8 dtimPeriod;

    /*TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz*/
    tANI_U8 txChannelWidthSet;

    /*Operating channel*/
    tANI_U8 currentOperChannel;

    /*Extension channel for channel bonding*/
    tANI_U8 currentExtChannel;

    /*Reserved to align next field on a dword boundary*/
    tANI_U8 reserved;

    /*SSID of the BSS*/
    tSirMacSSid ssId;

    /*HAL should update the existing BSS entry, if this flag is set.
      UMAC will set this flag in case of reassoc, where we want to resue the
      the old BSSID and still return success 0 = Add, 1 = Update*/
    tANI_U8 action;

    /* MAC Rate Set */
    tSirMacRateSet rateSet;

    /*Enable/Disable HT capabilities of the BSS*/
    tANI_U8 htCapable;

    // Enable/Disable OBSS protection
    tANI_U8 obssProtEnabled;

    /*RMF enabled/disabled*/
    tANI_U8 rmfEnabled;

    /*HT Operating Mode operating mode of the 802.11n STA*/
    tSirMacHTOperatingMode htOperMode;

    /*Dual CTS Protection: 0 - Unused, 1 - Used*/
    tANI_U8 dualCTSProtection;

    /* Probe Response Max retries */
    tANI_U8   ucMaxProbeRespRetryLimit;

    /* To Enable Hidden ssid */
    tANI_U8   bHiddenSSIDEn;

    /* To Enable Disable FW Proxy Probe Resp */
    tANI_U8   bProxyProbeRespEn;

    /* Boolean to indicate if EDCA params are valid. UMAC might not have valid
       EDCA params or might not desire to apply EDCA params during config BSS.
       0 implies Not Valid ; Non-Zero implies valid*/
    tANI_U8   edcaParamsValid;

    /*EDCA Parameters for Best Effort Access Category*/
    tSirMacEdcaParamRecord acbe;

    /*EDCA Parameters forBackground Access Category*/
    tSirMacEdcaParamRecord acbk;

    /*EDCA Parameters for Video Access Category*/
    tSirMacEdcaParamRecord acvi;

    /*EDCA Parameters for Voice Access Category*/
    tSirMacEdcaParamRecord acvo;

#ifdef WLAN_FEATURE_VOWIFI_11R
    tANI_U8 extSetStaKeyParamValid; //Ext Bss Config Msg if set
    tSetStaKeyParams extSetStaKeyParam;  //SetStaKeyParams for ext bss msg
#endif

    /* Persona for the BSS can be STA,AP,GO,CLIENT value same as tHalConMode */
    tANI_U8   halPersona;

    tANI_U8 bSpectrumMgtEnable;

    /*HAL fills in the tx power used for mgmt frames in txMgmtPower*/
    tANI_S8     txMgmtPower;
    /*maxTxPower has max power to be used after applying the power constraint if any */
    tANI_S8     maxTxPower;
    /*Context of the station being added in HW
      Add a STA entry for "itself" -
      On AP  - Add the AP itself in an "STA context"
      On STA - Add the AP to which this STA is joining in an "STA context" */
    tConfigStaParams_V1 staContext;

    tANI_U8   vhtCapable;
    tANI_U8   vhtTxChannelWidthSet;
} tConfigBssParams_V1, * tpConfigBssParams_V1;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   PACKED_PRE union PACKED_POST {
   tConfigBssParams configBssParams;
    tConfigBssParams_V1 configBssParams_V1;
   }uBssParams;
}  tConfigBssReqMsg, *tpConfigBssReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_CONFIG_BSS_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Success or Failure */
    tANI_U32   status;

    /* BSS index allocated by HAL */
    tANI_U8    bssIdx;

    /* DPU descriptor index for PTK */
    tANI_U8    dpuDescIndx;

    /* PTK DPU signature */
    tANI_U8    ucastDpuSignature;

    /* DPU descriptor index for GTK*/
    tANI_U8    bcastDpuDescIndx;

    /* GTK DPU signature */
    tANI_U8    bcastDpuSignature;

    /*DPU descriptor for IGTK*/
    tANI_U8    mgmtDpuDescIndx;

    /* IGTK DPU signature */
    tANI_U8    mgmtDpuSignature;

    /* Station Index for BSS entry*/
    tANI_U8     bssStaIdx;

    /* Self station index for this BSS */
    tANI_U8     bssSelfStaIdx;

    /* Bcast station for buffering bcast frames in AP role */
    tANI_U8     bssBcastStaIdx;

    /*MAC Address of STA(PEER/SELF) in staContext of configBSSReq*/
    tSirMacAddr   staMac;

    /*HAL fills in the tx power used for mgmt frames in this field. */
    tANI_S8     txMgmtPower;

} tConfigBssRspParams, * tpConfigBssRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tConfigBssRspParams configBssRspParams;
}  tConfigBssRspMsg, *tpConfigBssRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_DELETE_BSS_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* BSS index to be deleted */
    tANI_U8 bssIdx;

} tDeleteBssParams, *tpDeleteBssParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDeleteBssParams deleteBssParams;
}  tDeleteBssReqMsg, *tpDeleteBssReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_DELETE_BSS_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Success or Failure */
    tANI_U32   status;

    /* BSS index that has been deleted */
    tANI_U8 bssIdx;

} tDeleteBssRspParams, *tpDeleteBssRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDeleteBssRspParams deleteBssRspParams;
}  tDeleteBssRspMsg, *tpDeleteBssRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_JOIN_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*Indicates the BSSID to which STA is going to associate*/
  tSirMacAddr     bssId;

  /*Indicates the channel to switch to.*/
  tANI_U8         ucChannel;

  /* Self STA MAC */
  tSirMacAddr selfStaMacAddr;

  /*Local power constraint*/
  tANI_U8         ucLocalPowerConstraint;

  /*Secondary channel offset */
  ePhyChanBondState  secondaryChannelOffset;

  /*link State*/
  tSirLinkState   linkState;

  /* Max TX power */
  tANI_S8 maxTxPower;

} tHalJoinReqParams, *tpHalJoinReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalJoinReqParams joinReqParams;
}  tHalJoinReqMsg, *tpHalJoinReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_JOIN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

  /* HAL fills in the tx power used for mgmt frames in this field */
  tPowerdBm txMgmtPower;

}tHalJoinRspParams, *tpHalJoinRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalJoinRspParams joinRspParams;
}tHalJoinRspMsg, *tpHalJoinRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_POST_ASSOC_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tConfigStaParams configStaParams;
   tConfigBssParams configBssParams;
} tPostAssocReqParams, *tpPostAssocReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tPostAssocReqParams postAssocReqParams;
}  tPostAssocReqMsg, *tpPostAssocReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_POST_ASSOC_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tConfigStaRspParams configStaRspParams;
   tConfigBssRspParams configBssRspParams;
} tPostAssocRspParams, *tpPostAssocRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tPostAssocRspParams postAssocRspParams;
}  tPostAssocRspMsg, *tpPostAssocRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_SET_BSSKEY_REQ
---------------------------------------------------------------------------*/

/*
 * This is used by PE to create a set of WEP keys for a given BSS.
 */
typedef PACKED_PRE struct PACKED_POST
{
    /*BSS Index of the BSS*/
    tANI_U8         bssIdx;

    /*Encryption Type used with peer*/
    tAniEdType      encType;

    /*Number of keys*/
    tANI_U8         numKeys;

    /*Array of keys.*/
    tSirKeys        key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];

    /*Control for Replay Count, 1= Single TID based replay count on Tx
    0 = Per TID based replay count on TX */
    tANI_U8         singleTidRc;
} tSetBssKeyParams, *tpSetBssKeyParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetBssKeyParams setBssKeyParams;
} tSetBssKeyReqMsg, *tpSetBssKeyReqMsg;

/* tagged version of set bss key */
typedef PACKED_PRE struct PACKED_POST
{
   tSetBssKeyReqMsg  Msg;
   uint32            Tag;
} tSetBssKeyReqMsgTagged;

/*---------------------------------------------------------------------------
  WLAN_HAL_SET_BSSKEY_RSP
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

} tSetBssKeyRspParams, *tpSetBssKeyRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetBssKeyRspParams setBssKeyRspParams;
}  tSetBssKeyRspMsg, *tpSetBssKeyRspMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_SET_STAKEY_REQ,
---------------------------------------------------------------------------*/

/*
 * This is used by PE to configure the key information on a given station.
 * When the secType is WEP40 or WEP104, the defWEPIdx is used to locate
 * a preconfigured key from a BSS the station assoicated with; otherwise
 * a new key descriptor is created based on the key field.
 */

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetStaKeyParams setStaKeyParams;
} tSetStaKeyReqMsg, *tpSetStaKeyReqMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_SET_STAKEY_RSP,
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

} tSetStaKeyRspParams, *tpSetStaKeyRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetStaKeyRspParams setStaKeyRspParams;
} tSetStaKeyRspMsg, *tpSetStaKeyRspMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_RMV_BSSKEY_REQ,
---------------------------------------------------------------------------*/
/*
 * This is used by PE to remove keys for a given BSS.
 */
typedef PACKED_PRE struct PACKED_POST

{
    /*BSS Index of the BSS*/
    tANI_U8         bssIdx;

    /*Encryption Type used with peer*/
    tAniEdType      encType;

    /*Key Id*/
    tANI_U8         keyId;

    /*STATIC/DYNAMIC. Used in Nullifying in Key Descriptors for Static/Dynamic keys*/
    tAniWepType    wepType;

} tRemoveBssKeyParams, *tpRemoveBssKeyParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemoveBssKeyParams removeBssKeyParams;
}  tRemoveBssKeyReqMsg, *tpRemoveBssKeyReqMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_RMV_BSSKEY_RSP,
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

} tRemoveBssKeyRspParams, *tpRemoveBssKeyRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemoveBssKeyRspParams removeBssKeyRspParams;
}  tRemoveBssKeyRspMsg, *tpRemoveBssKeyRspMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_RMV_STAKEY_REQ,
---------------------------------------------------------------------------*/
/*
 * This is used by PE to Remove the key information on a given station.
 */
typedef PACKED_PRE struct PACKED_POST
{
    /*STA Index*/
    tANI_U16         staIdx;

    /*Encryption Type used with peer*/
    tAniEdType      encType;

    /*Key Id*/
    tANI_U8           keyId;

    /*Whether to invalidate the Broadcast key or Unicast key. In case of WEP,
      the same key is used for both broadcast and unicast.*/
    tANI_BOOLEAN    unicast;

} tRemoveStaKeyParams, *tpRemoveStaKeyParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemoveStaKeyParams removeStaKeyParams;
}  tRemoveStaKeyReqMsg, *tpRemoveStaKeyReqMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_RMV_STAKEY_RSP,
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;
} tRemoveStaKeyRspParams, *tpRemoveStaKeyRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemoveStaKeyRspParams removeStaKeyRspParams;
}  tRemoveStaKeyRspMsg, *tpRemoveStaKeyRspMsg;

#ifdef FEATURE_OEM_DATA_SUPPORT

#ifndef OEM_DATA_REQ_SIZE
#define OEM_DATA_REQ_SIZE 134
#endif

#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 1968
#endif

/*-------------------------------------------------------------------------
WLAN_HAL_START_OEM_DATA_REQ
--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32                 status;
    tSirMacAddr              selfMacAddr;
    tANI_U8                 oemDataReq[OEM_DATA_REQ_SIZE];
} tStartOemDataReqParams, *tpStartOemDataReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader                header;
    tStartOemDataReqParams  startOemDataReqParams;
} tStartOemDataReqMsg, *tpStartOemDataReqMsg;

/*-------------------------------------------------------------------------
WLAN_HAL_START_OEM_DATA_RSP
--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8                   oemDataRsp[OEM_DATA_RSP_SIZE];
} tStartOemDataRspParams, *tpStartOemDataRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader             header;
   tStartOemDataRspParams    startOemDataRspParams;
} tStartOemDataRspMsg, *tpStartOemDataRspMsg;

#endif

/*---------------------------------------------------------------------------
WLAN_HAL_CH_SWITCH_V1_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Channel number */
    tANI_U8 channelNumber;

    /* Local power constraint */
    tANI_U8 localPowerConstraint;

    /*Secondary channel offset */
    ePhyChanBondState secondaryChannelOffset;

    //HAL fills in the tx power used for mgmt frames in this field.
    tPowerdBm txMgmtPower;

    /* Max TX power */
    tPowerdBm maxTxPower;

    /* Self STA MAC */
    tSirMacAddr selfStaMacAddr;

    /*VO WIFI comment: BSSID needed to identify session. As the request has
     * power constraints, this should be applied only to that session
     * Since MTU timing and EDCA are sessionized, this struct needs to be
     * sessionized and bssid needs to be out of the VOWifi feature flag
     * V IMP: Keep bssId field at the end of this msg. It is used to
     * mantain backward compatbility
     * by way of ignoring if using new host/old FW or old host/new FW since
     * it is at the end of this struct
     */
    tSirMacAddr bssId;

    /* Source of Channel Switch */
    eHalChanSwitchSource channelSwitchSrc;

} tSwitchChannelParams_V1, *tpSwitchChannelParams_V1;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSwitchChannelParams_V1 switchChannelParams_V1;
} tSwitchChannelReqMsg_V1, *tpSwitchChannelReqMsg_V1;

/*---------------------------------------------------------------------------
WLAN_HAL_CH_SWITCH_V1_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Status */
    tANI_U32 status;

    /* Channel number - same as in request*/
    tANI_U8 channelNumber;

    /* HAL fills in the tx power used for mgmt frames in this field */
    tPowerdBm txMgmtPower;

    /* BSSID needed to identify session - same as in request*/
    tSirMacAddr bssId;

    /* Source of Channel Switch */
    eHalChanSwitchSource channelSwitchSrc;

} tSwitchChannelRspParams_V1, *tpSwitchChannelRspParams_V1;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSwitchChannelRspParams_V1 channelSwitchRspParams_V1;
} tSwitchChannelRspMsg_V1, *tpSwitchChannelRspMsg_V1;

/*---------------------------------------------------------------------------
WLAN_HAL_CH_SWITCH_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Channel number */
    tANI_U8 channelNumber;

    /* Local power constraint */
    tANI_U8 localPowerConstraint;

    /*Secondary channel offset */
    ePhyChanBondState secondaryChannelOffset;

    //HAL fills in the tx power used for mgmt frames in this field.
    tPowerdBm txMgmtPower;

    /* Max TX power */
    tPowerdBm maxTxPower;

    /* Self STA MAC */
    tSirMacAddr selfStaMacAddr;

    /*VO WIFI comment: BSSID needed to identify session. As the request has power constraints,
       this should be applied only to that session*/
    /* Since MTU timing and EDCA are sessionized, this struct needs to be sessionized and
     * bssid needs to be out of the VOWifi feature flag */
    /* V IMP: Keep bssId field at the end of this msg. It is used to mantain backward compatbility
     * by way of ignoring if using new host/old FW or old host/new FW since it is at the end of this struct
     */
    tSirMacAddr bssId;

}tSwitchChannelParams, *tpSwitchChannelParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSwitchChannelParams switchChannelParams;
}  tSwitchChannelReqMsg, *tpSwitchChannelReqMsg;

/*---------------------------------------------------------------------------
WLAN_HAL_CH_SWITCH_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Status */
    tANI_U32 status;

    /* Channel number - same as in request*/
    tANI_U8 channelNumber;

    /* HAL fills in the tx power used for mgmt frames in this field */
    tPowerdBm txMgmtPower;

    /* BSSID needed to identify session - same as in request*/
    tSirMacAddr bssId;

}tSwitchChannelRspParams, *tpSwitchChannelRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSwitchChannelRspParams switchChannelRspParams;
}  tSwitchChannelRspMsg, *tpSwitchChannelRspMsg;

/*---------------------------------------------------------------------------
WLAN_HAL_UPD_EDCA_PARAMS_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /*BSS Index*/
   tANI_U16 bssIdx;

   /* Best Effort */
   tSirMacEdcaParamRecord acbe;

   /* Background */
   tSirMacEdcaParamRecord acbk;

   /* Video */
   tSirMacEdcaParamRecord acvi;

   /* Voice */
   tSirMacEdcaParamRecord acvo;

} tEdcaParams, *tpEdcaParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tEdcaParams edcaParams;
}  tUpdateEdcaParamsReqMsg, *tpUpdateEdcaParamsReqMsg;

/*---------------------------------------------------------------------------
WLAN_HAL_UPD_EDCA_PARAMS_RSP
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;
} tEdcaRspParams, *tpEdcaRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tEdcaRspParams edcaRspParams;
}  tUpdateEdcaParamsRspMsg, *tpUpdateEdcaParamsRspMsg;



/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_STATS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST

{
    /* Index of STA to which the statistics */
    tANI_U16 staIdx;

    /* Encryption mode */
    tANI_U8 encMode;

    /* status */
    tANI_U32  status;

    /* Statistics */
    tANI_U32  sendBlocks;
    tANI_U32  recvBlocks;
    tANI_U32  replays;
    tANI_U8   micErrorCnt;
    tANI_U32  protExclCnt;
    tANI_U16  formatErrCnt;
    tANI_U16  unDecryptableCnt;
    tANI_U32  decryptErrCnt;
    tANI_U32  decryptOkCnt;
} tDpuStatsParams, * tpDpuStatsParams;

typedef PACKED_PRE struct PACKED_POST
{
   /* Valid STA Idx for per STA stats request */
   tANI_U32    staId;

   /* Categories of stats requested as specified in eHalStatsMask*/
   tANI_U32    statsMask;
}tHalStatsReqParams, *tpHalStatsReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader        header;
   tHalStatsReqParams   statsReqParams;
} tHalStatsReqMsg, *tpHalStatsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_STATS_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 retry_cnt[4];          //Total number of packets(per AC) that were successfully transmitted with retries
    tANI_U32 multiple_retry_cnt[4]; //The number of MSDU packets and MMPDU frames per AC that the 802.11
                                    // station successfully transmitted after more than one retransmission attempt

    tANI_U32 tx_frm_cnt[4];         //Total number of packets(per AC) that were successfully transmitted
                                    //(with and without retries, including multi-cast, broadcast)
    tANI_U32 rx_frm_cnt;            //Total number of packets that were successfully received
                                    //(after appropriate filter rules including multi-cast, broadcast)
    tANI_U32 frm_dup_cnt;           //Total number of duplicate frames received successfully
    tANI_U32 fail_cnt[4];           //Total number packets(per AC) failed to transmit
    tANI_U32 rts_fail_cnt;          //Total number of RTS/CTS sequence failures for transmission of a packet
    tANI_U32 ack_fail_cnt;          //Total number packets failed transmit because of no ACK from the remote entity
    tANI_U32 rts_succ_cnt;          //Total number of RTS/CTS sequence success for transmission of a packet
    tANI_U32 rx_discard_cnt;        //The sum of the receive error count and dropped-receive-buffer error count.
                                    //HAL will provide this as a sum of (FCS error) + (Fail get BD/PDU in HW)
    tANI_U32 rx_error_cnt;          //The receive error count. HAL will provide the RxP FCS error global counter.
    tANI_U32 tx_byte_cnt;           //The sum of the transmit-directed byte count, transmit-multicast byte count
                                    //and transmit-broadcast byte count. HAL will sum TPE UC/MC/BCAST global counters
                                    //to provide this.
}tAniSummaryStatsInfo, *tpAniSummaryStatsInfo;


// defines tx_rate_flags
typedef enum eTxRateInfo
{
   eHAL_TX_RATE_LEGACY = 0x1,    /* Legacy rates */
   eHAL_TX_RATE_HT20   = 0x2,    /* HT20 rates */
   eHAL_TX_RATE_HT40   = 0x4,    /* HT40 rates */
   eHAL_TX_RATE_SGI    = 0x8,    /* Rate with Short guard interval */
   eHAL_TX_RATE_LGI    = 0x10,   /* Rate with Long guard interval */
   eHAL_TX_RATE_VHT20  = 0x20,   /* VHT 20 rates */
   eHAL_TX_RATE_VHT40  = 0x40,   /* VHT 20 rates */
   eHAL_TX_RATE_VHT80  = 0x80,   /* VHT 20 rates */
   eHAL_TX_RATE_VIRT   = 0x100,  /* Virtual Rate */
   eHAL_TX_RATE_MAX    = WLAN_HAL_MAX_ENUM_SIZE
} tTxrateinfoflags, tTxRateInfoFlags;


typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 rx_frag_cnt;              //The number of MPDU frames received by the 802.11 station for MSDU packets
                                       //or MMPDU frames
    tANI_U32 promiscuous_rx_frag_cnt;  //The number of MPDU frames received by the 802.11 station for MSDU packets
                                       //or MMPDU frames when a promiscuous packet filter was enabled
    tANI_U32 rx_input_sensitivity;     //The receiver input sensitivity referenced to a FER of 8% at an MPDU length
                                       //of 1024 bytes at the antenna connector. Each element of the array shall correspond
                                       //to a supported rate and the order shall be the same as the supporteRates parameter.
    tANI_U32 max_pwr;                  //The maximum transmit power in dBm upto one decimal.
                                       //for eg: if it is 10.5dBm, the value would be 105
    tANI_U32 sync_fail_cnt;            //Number of times the receiver failed to synchronize with the incoming signal
                                       //after detecting the sync in the preamble of the transmitted PLCP protocol data unit.

    tANI_U32 tx_rate;                  //Legacy transmit rate, in units of 500 kbit/sec, for the most
                                       //recently transmitted frame
    tANI_U32  mcs_index;               //mcs index for HT20 and HT40 rates
    tANI_U32  tx_rate_flags;           //to differentiate between HT20 and
                                       //HT40 rates;  short and long guard interval
}tAniGlobalClassAStatsInfo, *tpAniGlobalClassAStatsInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 rx_wep_unencrypted_frm_cnt;  //The number of unencrypted received MPDU frames that the MAC layer discarded when
                                          //the IEEE 802.11 dot11ExcludeUnencrypted management information base (MIB) object
                                          //is enabled
    tANI_U32 rx_mic_fail_cnt;             //The number of received MSDU packets that that the 802.11 station discarded
                                          //because of MIC failures
    tANI_U32 tkip_icv_err;                //The number of encrypted MPDU frames that the 802.11 station failed to decrypt
                                          //because of a TKIP ICV error
    tANI_U32 aes_ccmp_format_err;         //The number of received MPDU frames that the 802.11 discarded because of an
                                          //invalid AES-CCMP format
    tANI_U32 aes_ccmp_replay_cnt;         //The number of received MPDU frames that the 802.11 station discarded because of
                                          //the AES-CCMP replay protection procedure
    tANI_U32 aes_ccmp_decrpt_err;         //The number of received MPDU frames that the 802.11 station discarded because of
                                          //errors detected by the AES-CCMP decryption algorithm
    tANI_U32 wep_undecryptable_cnt;       //The number of encrypted MPDU frames received for which a WEP decryption key was
                                          //not available on the 802.11 station
    tANI_U32 wep_icv_err;                 //The number of encrypted MPDU frames that the 802.11 station failed to decrypt
                                          //because of a WEP ICV error
    tANI_U32 rx_decrypt_succ_cnt;         //The number of received encrypted packets that the 802.11 station successfully
                                          //decrypted
    tANI_U32 rx_decrypt_fail_cnt;         //The number of encrypted packets that the 802.11 station failed to decrypt

}tAniGlobalSecurityStats, *tpAniGlobalSecurityStats;

typedef PACKED_PRE struct PACKED_POST
{
    tAniGlobalSecurityStats ucStats;
    tAniGlobalSecurityStats mcbcStats;
}tAniGlobalClassBStatsInfo, *tpAniGlobalClassBStatsInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 rx_amsdu_cnt;             //This counter shall be incremented for a received A-MSDU frame with the stations
                                       //MAC address in the address 1 field or an A-MSDU frame with a group address in the
                                       //address 1 field
    tANI_U32 rx_ampdu_cnt;             //This counter shall be incremented when the MAC receives an AMPDU from the PHY
    tANI_U32 tx_20_frm_cnt;            //This counter shall be incremented when a Frame is transmitted only on the
                                       //primary channel
    tANI_U32 rx_20_frm_cnt;            //This counter shall be incremented when a Frame is received only on the primary channel
    tANI_U32 rx_mpdu_in_ampdu_cnt;     //This counter shall be incremented by the number of MPDUs received in the A-MPDU
                                       //when an A-MPDU is received
    tANI_U32 ampdu_delimiter_crc_err;  //This counter shall be incremented when an MPDU delimiter has a CRC error when this
                                       //is the first CRC error in the received AMPDU or when the previous delimiter has been
                                       //decoded correctly
}tAniGlobalClassCStatsInfo, *tpAniGlobalClassCStatsInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 tx_frag_cnt[4];        //The number of MPDU frames that the 802.11 station transmitted and acknowledged
                                    //through a received 802.11 ACK frame
    tANI_U32 tx_ampdu_cnt;          //This counter shall be incremented when an A-MPDU is transmitted
    tANI_U32 tx_mpdu_in_ampdu_cnt;  //This counter shall increment by the number of MPDUs in the AMPDU when an A-MPDU
                                    //is transmitted
}tAniPerStaStatsInfo, *tpAniPerStaStatsInfo;

typedef PACKED_PRE struct PACKED_POST
{
   /* Success or Failure */
   tANI_U32 status;

   /* STA Idx */
   tANI_U32 staId;

   /* Categories of STATS being returned as per eHalStatsMask*/
   tANI_U32 statsMask;

   /* message type is same as the request type */
   tANI_U16 msgType;

   /* length of the entire request, includes the pStatsBuf length too */
   tANI_U16 msgLen;

} tHalStatsRspParams, *tpHalStatsRspParams;



typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader  header;
   tHalStatsRspParams statsRspParams;
} tHalStatsRspMsg, *tpHalStatsRspMsg;

 /*---------------------------------------------------------------------------
 * WLAN_HAL_SET_RTS_CTS_HTVHT_IND
 *---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 rtsCtsValue;
}tHalRtsCtsHtvhtIndParams, *tpHalRtsCtsHtvhtIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader        header;
   tHalRtsCtsHtvhtIndParams   rtsCtsHtvhtIndParams;
} tHalRtsCtsHtvhtIndMsg, *tpHalRtsCtsHtvhtIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_FW_STATS_REQ
 *---------------------------------------------------------------------------*/
 typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 type;
}tHalfwStatsReqParams, *tpHalfwStatsReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader        header;
   tHalfwStatsReqParams   fwstatsReqParams;
} tHalfwStatsReqMsg, *tpHalfwStatsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_FW_STATS_RSP
 *---------------------------------------------------------------------------*/
 typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 type;
   tANI_U32 length;
   tANI_U8  data[1];

}tHalfwStatsRspParams, *tpHalfwStatsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader        header;
   tHalfwStatsRspParams   fwstatsRspParams;
} tHalfwStatsRspMsg, *tpHalfwStatsRspMsg;

typedef enum
{
   FW_UBSP_STATS = 1,
} fwstatstype;


/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_LINK_ST_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr bssid;
    tSirLinkState state;
    tSirMacAddr selfMacAddr;
} tLinkStateParams, *tpLinkStateParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tLinkStateParams linkStateParams;
}  tSetLinkStateReqMsg, *tpSetLinkStateReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_LINK_ST_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;
} tLinkStateRspParams, *tpLinkStateRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tLinkStateRspParams linkStateRspParams;
}  tSetLinkStateRspMsg, *tpSetLinkStateRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_TS_REQ
 *--------------------------------------------------------------------------*/

/* TSPEC Params */
typedef __ani_attr_pre_packed struct sSirMacTSInfoTfc
{
    tANI_U16       trafficType : 1;
    tANI_U16       tsid : 4;
    tANI_U16       direction : 2;
    tANI_U16       accessPolicy : 2;
    tANI_U16       aggregation : 1;
    tANI_U16       psb : 1;
    tANI_U16       userPrio : 3;
    tANI_U16       ackPolicy : 2;
} __ani_attr_packed tSirMacTSInfoTfc;

/* Flag to schedule the traffic type */
typedef __ani_attr_pre_packed struct sSirMacTSInfoSch
{
    tANI_U8        schedule : 1;
    tANI_U8        rsvd : 7;
} __ani_attr_packed tSirMacTSInfoSch;

/* Traffic and scheduling info */
typedef __ani_attr_pre_packed struct sSirMacTSInfo
{
    tSirMacTSInfoTfc traffic;
    tSirMacTSInfoSch schedule;
} __ani_attr_packed tSirMacTSInfo;

/* Information elements */
typedef __ani_attr_pre_packed struct sSirMacTspecIE
{
    tANI_U8             type;
    tANI_U8             length;
    tSirMacTSInfo       tsinfo;
    tANI_U16            nomMsduSz;
    tANI_U16            maxMsduSz;
    tANI_U32            minSvcInterval;
    tANI_U32            maxSvcInterval;
    tANI_U32            inactInterval;
    tANI_U32            suspendInterval;
    tANI_U32            svcStartTime;
    tANI_U32            minDataRate;
    tANI_U32            meanDataRate;
    tANI_U32            peakDataRate;
    tANI_U32            maxBurstSz;
    tANI_U32            delayBound;
    tANI_U32            minPhyRate;
    tANI_U16            surplusBw;
    tANI_U16            mediumTime;
}__ani_attr_packed tSirMacTspecIE;

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* TSPEC handler uniquely identifying a TSPEC for a STA in a BSS */
    tANI_U16 tspecIdx;

    /* To program TPE with required parameters */
    tSirMacTspecIE   tspec;

    /* U-APSD Flags: 1b per AC.  Encoded as follows:
     b7 b6 b5 b4 b3 b2 b1 b0 =
     X  X  X  X  BE BK VI VO */
    tANI_U8 uAPSD;

    /* These parameters are for all the access categories */
    tANI_U32 srvInterval[WLAN_HAL_MAX_AC];   // Service Interval
    tANI_U32 susInterval[WLAN_HAL_MAX_AC];   // Suspend Interval
    tANI_U32 delayInterval[WLAN_HAL_MAX_AC]; // Delay Interval

} tAddTsParams, *tpAddTsParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddTsParams  addTsParams;
}  tAddTsReqMsg, *tpAddTsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_TS_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /*success or failure */
    tANI_U32   status;
} tAddTsRspParams, *tpAddTsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddTsRspParams addTsRspParams;
}  tAddTsRspMsg, *tpAddTsRspMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_TS_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* TSPEC identifier uniquely identifying a TSPEC for a STA in a BSS */
    tANI_U16 tspecIdx;

    /* To lookup station id using the mac address */
    tSirMacAddr bssId;

} tDelTsParams, *tpDelTsParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tDelTsParams  delTsParams;
}  tDelTsReqMsg, *tpDelTsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_TS_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /*success or failure */
    tANI_U32   status;
} tDelTsRspParams, *tpDelTsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tDelTsRspParams delTsRspParams;
}  tDelTsRspMsg, *tpDelTsRspMsg;

/* End of TSpec Parameters */

/* Start of BLOCK ACK related Parameters */

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BA_SESSION_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* Peer MAC Address */
    tSirMacAddr peerMacAddr;

    /* ADDBA Action Frame dialog token
       HAL will not interpret this object */
    tANI_U8 baDialogToken;

    /* TID for which the BA is being setup
       This identifies the TC or TS of interest */
    tANI_U8 baTID;

    /* 0 - Delayed BA (Not supported)
       1 - Immediate BA */
    tANI_U8 baPolicy;

    /* Indicates the number of buffers for this TID (baTID)
       NOTE - This is the requested buffer size. When this
       is processed by HAL and subsequently by HDD, it is
       possible that HDD may change this buffer size. Any
       change in the buffer size should be noted by PE and
       advertized appropriately in the ADDBA response */
    tANI_U16 baBufferSize;

    /* BA timeout in TU's 0 means no timeout will occur */
    tANI_U16 baTimeout;

    /* b0..b3 - Fragment Number - Always set to 0
       b4..b15 - Starting Sequence Number of first MSDU
       for which this BA is setup */
    tANI_U16 baSSN;

    /* ADDBA direction
       1 - Originator
       0 - Recipient */
    tANI_U8 baDirection;
} tAddBASessionParams, *tpAddBASessionParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddBASessionParams  addBASessionParams;
}tAddBASessionReqMsg, *tpAddBASessionReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BA_SESSION_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /*success or failure */
    tANI_U32   status;

    /* Dialog token */
    tANI_U8 baDialogToken;

    /* TID for which the BA session has been setup */
    tANI_U8 baTID;

    /* BA Buffer Size allocated for the current BA session */
    tANI_U8 baBufferSize;

    tANI_U8 baSessionID;

    /* Reordering Window buffer */
    tANI_U8 winSize;

    /*Station Index to id the sta */
    tANI_U8 STAID;

    /* Starting Sequence Number */
    tANI_U16 SSN;
} tAddBASessionRspParams, *tpAddBASessionRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddBASessionRspParams addBASessionRspParams;
}  tAddBASessionRspMsg, *tpAddBASessionRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BA_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Session Id */
    tANI_U8 baSessionID;

    /* Reorder Window Size */
    tANI_U8 winSize;

#ifdef FEATURE_ON_CHIP_REORDERING
    tANI_BOOLEAN isReorderingDoneOnChip;
#endif
} tAddBAParams, *tpAddBAParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddBAParams  addBAParams;
}  tAddBAReqMsg, *tpAddBAReqMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BA_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /*success or failure */
    tANI_U32   status;

    /* Dialog token */
    tANI_U8 baDialogToken;

} tAddBARspParams, *tpAddBARspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddBARspParams addBARspParams;
}  tAddBARspMsg, *tpAddBARspMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_TRIGGER_BA_REQ
 *--------------------------------------------------------------------------*/


typedef struct sAddBaInfo
{
    tANI_U16 fBaEnable : 1;
    tANI_U16 startingSeqNum: 12;
    tANI_U16 reserved : 3;
}tAddBaInfo, *tpAddBaInfo;

typedef struct sTriggerBaRspCandidate
{
    tSirMacAddr staAddr;
    tAddBaInfo baInfo[STACFG_MAX_TC];
}tTriggerBaRspCandidate, *tpTriggerBaRspCandidate;

typedef struct sTriggerBaCandidate
{
    tANI_U8  staIdx;
    tANI_U8 tidBitmap;
}tTriggerBaReqCandidate, *tptTriggerBaReqCandidate;

typedef PACKED_PRE struct PACKED_POST
{
    /* Session Id */
    tANI_U8 baSessionID;

    /* baCandidateCnt is followed by trigger BA
     * Candidate List(tTriggerBaCandidate)
     */
    tANI_U16 baCandidateCnt;

} tTriggerBAParams, *tpTriggerBAParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tTriggerBAParams  triggerBAParams;
}  tTriggerBAReqMsg, *tpTriggerBAReqMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_TRIGGER_BA_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{

    /* TO SUPPORT BT-AMP */
    tSirMacAddr  bssId;

    /* success or failure */
    tANI_U32   status;

    /* baCandidateCnt is followed by trigger BA
     * Rsp Candidate List(tTriggerRspBaCandidate)
     */
    tANI_U16 baCandidateCnt;


} tTriggerBARspParams, *tpTriggerBARspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTriggerBARspParams triggerBARspParams;
}  tTriggerBARspMsg, *tpTriggerBARspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_BA_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* TID for which the BA session is being deleted */
    tANI_U8 baTID;

    /* DELBA direction
       1 - Originator
       0 - Recipient */
    tANI_U8 baDirection;
} tDelBAParams, *tpDelBAParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDelBAParams  delBAParams;
}  tDelBAReqMsg, *tpDelBAReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_BA_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tDelBARspParams, *tpDelBARspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tDelBARspParams delBARspParams;
}  tDelBARspMsg, *tpDelBARspMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_TSM_STATS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Traffic Id */
    tANI_U8 tsmTID;

    tSirMacAddr bssId;
} tTsmStatsParams, *tpTsmStatsParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTsmStatsParams  tsmStatsParams;
}  tTsmStatsReqMsg, *tpTsmStatsReqMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_TSM_STATS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /*success or failure */
    tANI_U32   status;

    /* Uplink Packet Queue delay */
    tANI_U16      UplinkPktQueueDly;

    /* Uplink Packet Queue delay histogram */
    tANI_U16      UplinkPktQueueDlyHist[4];

    /* Uplink Packet Transmit delay */
    tANI_U32      UplinkPktTxDly;

    /* Uplink Packet loss */
    tANI_U16      UplinkPktLoss;

    /* Uplink Packet count */
    tANI_U16      UplinkPktCount;

    /* Roaming count */
    tANI_U8       RoamingCount;

    /* Roaming Delay */
    tANI_U16      RoamingDly;
} tTsmStatsRspParams, *tpTsmStatsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTsmStatsRspParams tsmStatsRspParams;
}  tTsmStatsRspMsg, *tpTsmStatsRspMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_KEYDONE_MSG
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*bssid of the keys */
  tANI_U8   bssidx;
  tANI_U8   encType;
} tSetKeyDoneParams, *tpSetKeyDoneParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetKeyDoneParams setKeyDoneParams;
}  tSetKeyDoneMsg, *tpSetKeyDoneMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DOWNLOAD_NV_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Fragment sequence number of the NV Image. Note that NV Image might not
     * fit into one message due to size limitation of the SMD channel FIFO. UMAC
     * can hence choose to chop the NV blob into multiple fragments starting with
     * seqeunce number 0, 1, 2 etc. The last fragment MUST be indicated by
     * marking the isLastFragment field to 1. Note that all the NV blobs would be
     * concatenated together by HAL without any padding bytes in between.*/
    tANI_U16 fragNumber;

    /* Is this the last fragment? When set to 1 it indicates that no more fragments
     * will be sent by UMAC and HAL can concatenate all the NV blobs rcvd & proceed
     * with the parsing. HAL would generate a WLAN_HAL_DOWNLOAD_NV_RSP to the
     * WLAN_HAL_DOWNLOAD_NV_REQ after it receives each fragment */
    tANI_U16 isLastFragment;

    /* NV Image size (number of bytes) */
    tANI_U32 nvImgBufferSize;

    /* Following the 'nvImageBufferSize', there should be nvImageBufferSize
     * bytes of NV Image i.e. uint8[nvImageBufferSize] */
} tHalNvImgDownloadReqParams, *tpHalNvImgDownloadReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    /* Note: The length specified in tHalNvImgDownloadReqMsg messages should be
     * header.msgLen = sizeof(tHalNvImgDownloadReqMsg) + nvImgBufferSize */
    tHalMsgHeader header;
    tHalNvImgDownloadReqParams nvImageReqParams;
} tHalNvImgDownloadReqMsg, *tpHalNvImgDownloadReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DOWNLOAD_NV_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Success or Failure. HAL would generate a WLAN_HAL_DOWNLOAD_NV_RSP
     * after each fragment */
    tANI_U32   status;
} tHalNvImgDownloadRspParams, *tpHalNvImgDownloadRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalNvImgDownloadRspParams nvImageRspParams;
}  tHalNvImgDownloadRspMsg, *tpHalNvImgDownloadRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_STORE_NV_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* NV Item */
    eNvTable tableID;

    /* Size of NV Blob */
    tANI_U32 nvBlobSize;

    /* Following the 'nvBlobSize', there should be nvBlobSize bytes of
     * NV blob i.e. uint8[nvBlobSize] */
} tHalNvStoreParams, *tpHalNvStoreParams;

typedef PACKED_PRE struct PACKED_POST
{
    /* Note: The length specified in tHalNvStoreInd messages should be
     * header.msgLen = sizeof(tHalNvStoreInd) + nvBlobSize */
    tHalMsgHeader header;
    tHalNvStoreParams nvStoreParams;
}  tHalNvStoreInd, *tpHalNvStoreInd;

/* End of Block Ack Related Parameters */

/*---------------------------------------------------------------------------
 * WLAN_HAL_MIC_FAILURE_IND
 *--------------------------------------------------------------------------*/

#define SIR_CIPHER_SEQ_CTR_SIZE 6

typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr  srcMacAddr;     //address used to compute MIC
    tSirMacAddr  taMacAddr;      //transmitter address
    tSirMacAddr  dstMacAddr;
    tANI_U8      multicast;
    tANI_U8      IV1;            // first byte of IV
    tANI_U8      keyId;          // second byte of IV
    tANI_U8      TSC[SIR_CIPHER_SEQ_CTR_SIZE]; // sequence number
    tSirMacAddr  rxMacAddr;      // receive address
} tSirMicFailureInfo, *tpSirMicFailureInfo;

/* Definition for MIC failure indication
   MAC reports this each time a MIC failure occures on Rx TKIP packet
 */
typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr         bssId;   // BSSID
    tSirMicFailureInfo  info;
} tSirMicFailureInd, *tpSirMicFailureInd;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSirMicFailureInd micFailureInd;
}  tMicFailureIndMsg, *tpMicFailureIndMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U16  opMode;
   tANI_U16  staId;
}tUpdateVHTOpMode, *tpUpdateVHTOpMode;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tUpdateVHTOpMode updateVhtOpMode;
}  tUpdateVhtOpModeReqMsg, *tpUpdateVhtOpModeReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;
} tUpdateVhtOpModeParamsRsp, *tpUpdateVhtOpModeParamsRsp;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tUpdateVhtOpModeParamsRsp updateVhtOpModeRspParam;
}  tUpdateVhtOpModeParamsRspMsg,  *tpUpdateVhtOpModeParamsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_UPDATE_BEACON_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{

    tANI_U8  bssIdx;

    //shortPreamble mode. HAL should update all the STA rates when it
    //receives this message
    tANI_U8 fShortPreamble;
    //short Slot time.
    tANI_U8 fShortSlotTime;
    //Beacon Interval
    tANI_U16 beaconInterval;
    //Protection related
    tANI_U8 llaCoexist;
    tANI_U8 llbCoexist;
    tANI_U8 llgCoexist;
    tANI_U8 ht20MhzCoexist;
    tANI_U8 llnNonGFCoexist;
    tANI_U8 fLsigTXOPProtectionFullSupport;
    tANI_U8 fRIFSMode;

    tANI_U16 paramChangeBitmap;
}tUpdateBeaconParams, *tpUpdateBeaconParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tUpdateBeaconParams updateBeaconParam;
}  tUpdateBeaconReqMsg, *tpUpdateBeaconReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_UPDATE_BEACON_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;
} tUpdateBeaconRspParams, *tpUpdateBeaconRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tUpdateBeaconRspParams updateBeaconRspParam;
}  tUpdateBeaconRspMsg, *tpUpdateBeaconRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SEND_BEACON_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 beaconLength; //length of the template.
    tANI_U8 beacon[BEACON_TEMPLATE_SIZE];     // Beacon data.
    tSirMacAddr bssId;
    tANI_U32 timIeOffset; //TIM IE offset from the beginning of the template.
    tANI_U16 p2pIeOffset; //P2P IE offset from the begining of the template
}tSendBeaconParams, *tpSendBeaconParams;


typedef PACKED_PRE struct PACKED_POST
{
  tHalMsgHeader header;
  tSendBeaconParams sendBeaconParam;
}tSendBeaconReqMsg, *tpSendBeaconReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SEND_BEACON_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;
} tSendBeaconRspParams, *tpSendBeaconRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSendBeaconRspParams sendBeaconRspParam;
}  tSendBeaconRspMsg, *tpSendBeaconRspMsg;

#ifdef FEATURE_5GHZ_BAND

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENABLE_RADAR_DETECT_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr BSSID;
    tANI_U8   channel;
}tSirEnableRadarInfoType, *tptSirEnableRadarInfoType;


typedef PACKED_PRE struct PACKED_POST
{
    /* Link Parameters */
    tSirEnableRadarInfoType EnableRadarInfo;
}tEnableRadarReqParams, *tpEnableRadarReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tEnableRadarReqParams  enableRadarReqParams;
}tEnableRadarReqMsg, *tpEnableRadarReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENABLE_RADAR_DETECT_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Link Parameters */
    tSirMacAddr BSSID;
    /* success or failure */
    tANI_U32   status;
}tEnableRadarRspParams, *tpEnableRadarRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tEnableRadarRspParams  enableRadarRspParams;
}tEnableRadarRspMsg, *tpEnableRadarRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_RADAR_DETECT_INTR_IND
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8 radarDetChannel;
}tRadarDetectIntrIndParams, *tpRadarDetectIntrIndParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tRadarDetectIntrIndParams  radarDetectIntrIndParams;
}tRadarDetectIntrIndMsg, *tptRadarDetectIntrIndMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_RADAR_DETECT_IND
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /*channel number in which the RADAR detected*/
    tANI_U8          channelNumber;

    /*RADAR pulse width*/
    tANI_U16         radarPulseWidth; // in usecond

    /*Number of RADAR pulses */
    tANI_U16         numRadarPulse;
}tRadarDetectIndParams,*tpRadarDetectIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRadarDetectIndParams  radarDetectIndParams;
}tRadarDetectIndMsg, *tptRadarDetectIndMsg;


/*---------------------------------------------------------------------------
 *WLAN_HAL_GET_TPC_REPORT_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tSirMacAddr sta;
   tANI_U8     dialogToken;
   tANI_U8     txpower;
}tSirGetTpcReportReqParams, *tpSirGetTpcReportReqParams;


typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSirGetTpcReportReqParams  getTpcReportReqParams;
}tSirGetTpcReportReqMsg, *tpSirGetTpcReportReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_TPC_REPORT_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSirGetTpcReportRspParams, *tpSirGetTpcReportRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSirGetTpcReportRspParams  getTpcReportRspParams;
}tSirGetTpcReportRspMsg, *tpSirGetTpcReportRspMsg;

#endif

/*---------------------------------------------------------------------------
 *WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8      pProbeRespTemplate[BEACON_TEMPLATE_SIZE];
    tANI_U32     probeRespTemplateLen;
    tANI_U32     ucProxyProbeReqValidIEBmap[8];
    tSirMacAddr  bssId;

}tSendProbeRespReqParams, *tpSendProbeRespReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSendProbeRespReqParams sendProbeRespReqParams ;
}tSendProbeRespReqMsg, *tpSendProbeRespReqMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSendProbeRespRspParams, *tpSendProbeRespRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSendProbeRespRspParams sendProbeRespRspParams;
}tSendProbeRespRspMsg, *tpSendProbeRespRspMsg;


/*---------------------------------------------------------------------------
 *WLAN_HAL_UNKNOWN_ADDR2_FRAME_RX_IND
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSendUnkownFrameRxIndParams, *tpSendUnkownFrameRxIndParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSendUnkownFrameRxIndParams sendUnkownFrameRxIndParams;
}tSendUnkownFrameRxIndMsg, *tpSendUnkownFrameRxIndMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_DELETE_STA_CONTEXT_IND
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U16    assocId;
    tANI_U16    staId;
    tSirMacAddr bssId; // TO SUPPORT BT-AMP
                       // HAL copies bssid from the sta table.
    tSirMacAddr addr2;        //
    tANI_U16    reasonCode;   // To unify the keepalive / unknown A2 / tim-based disa

}tDeleteStaContextParams, *tpDeleteStaContextParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tDeleteStaContextParams deleteStaContextParams;
}tDeleteStaContextIndMsg, *tpDeleteStaContextIndMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tANI_U8  assocId;
   tANI_U8  staIdx;
   tANI_U8  bssIdx;
   tANI_U8  uReasonCode;
   tANI_U32  uStatus;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
   tANI_U8  staAddr[6];
   tANI_U8  bssId[6];
#endif
} tIndicateDelSta, *tpIndicateDelSta;

/*---------------------------------------------------------------------------
 *WLAN_HAL_SIGNAL_BTAMP_EVENT_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tBtAmpEventType btAmpEventType;

}tBtAmpEventParams, *tpBtAmpEventParams;



typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tBtAmpEventParams btAmpEventParams;
}tBtAmpEventMsg, *tpBtAmpEventMsg;

/*---------------------------------------------------------------------------
*WLAN_HAL_SIGNAL_BTAMP_EVENT_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tBtAmpEventRspParams, *tpBtAmpEventRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tBtAmpEventRspParams btAmpEventRspParams;
}tBtAmpEventRsp, *tpBtAmpEventRsp;


/*---------------------------------------------------------------------------
 *WLAN_HAL_TL_HAL_FLUSH_AC_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   // Station Index. originates from HAL
    tANI_U8  ucSTAId;

    // TID for which the transmit queue is being flushed
    tANI_U8   ucTid;

}tTlHalFlushAcParams, *tpTlHalFlushAcParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTlHalFlushAcParams tlHalFlushAcParam;
}tTlHalFlushAcReq, *tpTlHalFlushAcReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_TL_HAL_FLUSH_AC_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    // Station Index. originates from HAL
    tANI_U8  ucSTAId;

    // TID for which the transmit queue is being flushed
    tANI_U8   ucTid;

    /* success or failure */
    tANI_U32   status;
}tTlHalFlushAcRspParams, *tpTlHalFlushAcRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTlHalFlushAcRspParams tlHalFlushAcRspParam;
}tTlHalFlushAcRspMsg, *tpTlHalFlushAcRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_IMPS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalEnterImpsReqMsg, *tpHalEnterImpsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_IMPS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalExitImpsReqMsg, *tpHalExitImpsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_BMPS_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8         bssIdx;
   //TBTT value derived from the last beacon
#ifndef BUILD_QWPTTSTATIC
   tANI_U64 tbtt;
#endif
   tANI_U8 dtimCount;
   //DTIM period given to HAL during association may not be valid,
   //if association is based on ProbeRsp instead of beacon.
   tANI_U8 dtimPeriod;

   // For ESE and 11R Roaming
   tANI_U32 rssiFilterPeriod;
   tANI_U32 numBeaconPerRssiAverage;
   tANI_U8  bRssiFilterEnable;

} tHalEnterBmpsReqParams, *tpHalEnterBmpsReqParams;


typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnterBmpsReqParams enterBmpsReq;
}  tHalEnterBmpsReqMsg, *tpHalEnterBmpsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_PRINT_REG_INFO_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   uint32 regAddr;
   uint32 regValue;
} tHalRegDebugInfo, *tpRegDebugInfo;

typedef PACKED_PRE struct PACKED_POST
{
   uint32 regCount;
   uint32 scenario;
   uint32 reasonCode;
} tHalRegDebugInfoParams, *tpRegDebugInfoParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalRegDebugInfoParams regParams;
} tHalRegDebugInfoMsg, *tpRegDebugInfoMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_BMPS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8     sendDataNull;
   tANI_U8     bssIdx;
} tHalExitBmpsReqParams, *tpHalExitBmpsReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitBmpsReqParams exitBmpsReqParams;
}  tHalExitBmpsReqMsg, *tpHalExitBmpsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_MISSED_BEACON_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8     bssIdx;
} tHalMissedBeaconIndParams, *tpHalMissedBeaconIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMissedBeaconIndParams missedBeaconIndParams;
}  tHalMissedBeaconIndMsg, *tpHalMissedBeaconIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BCN_FILTER_REQ
 *--------------------------------------------------------------------------*/
/* Beacon Filtering data structures */
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8     offset;
    tANI_U8     value;
    tANI_U8     bitMask;
    tANI_U8     ref;
} tEidByteInfo, *tpEidByteInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U16    capabilityInfo;
    tANI_U16    capabilityMask;
    tANI_U16    beaconInterval;
    tANI_U16    ieNum;
    tANI_U8     bssIdx;
    tANI_U8     reserved;
} tBeaconFilterMsg, *tpBeaconFilterMsg;

/* The above structure would be followed by multiple of below mentioned structure */
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8         elementId;
    tANI_U8         checkIePresence;
    tEidByteInfo    byte;
} tBeaconFilterIe, *tpBeaconFilterIe;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tBeaconFilterMsg addBcnFilterParams;
}  tHalAddBcnFilterReqMsg, *tpHalAddBcnFilterReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_REM_BCN_FILTER_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8  ucIeCount;
    tANI_U8  ucRemIeId[1];
} tRemBeaconFilterMsg, *tpRemBeaconFilterMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemBeaconFilterMsg remBcnFilterParams;
}  tHalRemBcnFilterReqMsg, *tpHalRemBcnFilterReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_OFFLOAD_REQ
 *--------------------------------------------------------------------------*/
#define HAL_IPV4_ARP_REPLY_OFFLOAD                  0
#define HAL_IPV6_NEIGHBOR_DISCOVERY_OFFLOAD         1
#define HAL_IPV6_NS_OFFLOAD                         2
#define HAL_IPV6_ADDR_LEN                           16
#define HAL_MAC_ADDR_LEN                            6
#define HAL_OFFLOAD_DISABLE                         0
#define HAL_OFFLOAD_ENABLE                          1
#define HAL_OFFLOAD_BCAST_FILTER_ENABLE             0x2
#define HAL_OFFLOAD_MCAST_FILTER_ENABLE             0x4
#define HAL_OFFLOAD_ARP_AND_BCAST_FILTER_ENABLE     (HAL_OFFLOAD_ENABLE|HAL_OFFLOAD_BCAST_FILTER_ENABLE)
#define HAL_OFFLOAD_IPV6NS_AND_MCAST_FILTER_ENABLE  (HAL_OFFLOAD_ENABLE|HAL_OFFLOAD_MCAST_FILTER_ENABLE)

typedef PACKED_PRE struct PACKED_POST _tHalNSOffloadParams
{
   tANI_U8 srcIPv6Addr[HAL_IPV6_ADDR_LEN];
   tANI_U8 selfIPv6Addr[HAL_IPV6_ADDR_LEN];
   //Only support 2 possible Network Advertisement IPv6 address
   tANI_U8 targetIPv6Addr1[HAL_IPV6_ADDR_LEN];
   tANI_U8 targetIPv6Addr2[HAL_IPV6_ADDR_LEN];
   tANI_U8 selfMacAddr[HAL_MAC_ADDR_LEN];
   tANI_U8 srcIPv6AddrValid : 1;
   tANI_U8 targetIPv6Addr1Valid : 1;
   tANI_U8 targetIPv6Addr2Valid : 1;
   tANI_U8 reserved1 : 5;
   tANI_U8 reserved2;   //make it DWORD aligned
   tANI_U8 bssIdx;
   tANI_U32 slotIndex; // slot index for this offload
} tHalNSOffloadParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8 offloadType;
    tANI_U8 enableOrDisable;
    PACKED_PRE union PACKED_POST
    {
        tANI_U8 hostIpv4Addr [4];
        tANI_U8 hostIpv6Addr [HAL_IPV6_ADDR_LEN];
    } params;
} tHalHostOffloadReq, *tpHalHostOffloadReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalHostOffloadReq hostOffloadParams;
   tHalNSOffloadParams nsOffloadParams;
}  tHalHostOffloadReqMsg, *tpHalHostOffloadReqMsg;


#ifdef FEATURE_WLAN_LPHB
typedef enum
{
   WIFI_HB_SET_ENABLE         = 0x0001,
   WIFI_HB_SET_TCP_PARAMS     = 0x0002,
   WIFI_HB_SET_TCP_PKT_FILTER = 0x0003,
   WIFI_HB_SET_UDP_PARAMS     = 0x0004,
   WIFI_HB_SET_UDP_PKT_FILTER = 0x0005,
   WIFI_HB_SET_NETWORK_INFO   = 0x0006,
}tLowPowerHeartBeatCmdType ;

#define MAX_FLITER_SIZE 64
/*---------------------------------------------------------------------------
 *FEATURE_WLAN_LPHB REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   uint32 hostIpv4Addr;
   uint32 destIpv4Addr;
   uint16 hostPort;
   uint16 destPort;
   uint16 timeOutSec;  // in seconds
   tSirMacAddr gatewayMacAddr;
   uint16 timePeriodSec; // in seconds
   uint32 tcpSn;
} tlowPowerHeartBeatParamsTcpStruct;

typedef PACKED_PRE struct PACKED_POST
{
   uint32 hostIpv4Addr;
   uint32 destIpv4Addr;
   uint16 hostPort;
   uint16 destPort;
   uint16 timePeriodSec;// in seconds
   uint16 timeOutSec;   // in seconds
   tSirMacAddr gatewayMacAddr;
} tlowPowerHeartBeatParamsUdpStruct;

typedef PACKED_PRE struct PACKED_POST
{
   uint32 offset;
   uint32 filterLength;
   uint8  filter[MAX_FLITER_SIZE];
} tlowPowerHeartBeatFilterStruct;

typedef PACKED_PRE struct PACKED_POST
{
   uint8 heartBeatEnable;
   uint8 heartBeatType; //TCP or UDP
} tlowPowerHeartBeatEnableStruct;

typedef PACKED_PRE struct PACKED_POST
{
  uint8 dummy;
} tlowPowerHeartBeatNetworkInfoStruct;


typedef PACKED_PRE struct PACKED_POST
{
   uint8 sessionIdx;
   uint16 lowPowerHeartBeatCmdType;
   PACKED_PRE union PACKED_PRO
   {
      tlowPowerHeartBeatEnableStruct control;
      tlowPowerHeartBeatFilterStruct tcpUdpFilter;
      tlowPowerHeartBeatParamsTcpStruct tcpParams;
      tlowPowerHeartBeatParamsUdpStruct udpParams;
      tlowPowerHeartBeatNetworkInfoStruct info;
    }options;
} tHalLowPowerHeartBeatReq, *tpHalLowPowerHeartBeatReq;


typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalLowPowerHeartBeatReq lowPowerHeartBeatParams;
}  tHalLowPowerHeartBeatReqMsg, *tpHalLowPowerHeartBeatReqMsg;

/*---------------------------------------------------------------------------
 * FEATURE_WLAN_LPHB RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /* success or failure */
   uint8  sessionIdx;
   uint32 status;
   uint16 lowPowerHeartBeatCmdType;
}tHalLowPowerHeartBeatRspParams, *tpHalLowPowerHeartBeatRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalLowPowerHeartBeatRspParams lowPowerHeartBeatRspParams;
}tHalLowPowerHeartBeatRspMsg, *tpHalLowPowerHeartBeatRspMsg;


/*---------------------------------------------------------------------------
 * FEATURE_WLAN_LPHB IND
 *--------------------------------------------------------------------------*/
#define WIFI_HB_EVENT_TCP_RX_TIMEOUT 0x0001
#define WIFI_HB_EVENT_UDP_RX_TIMEOUT 0x0002

#define WIFI_LPHB_EVENT_REASON_TIMEOUT 0x01
#define WIFI_LPHB_EVENT_REASON_FW_ON_MONITOR 0x02
#define WIFI_LPHB_EVENT_REASON_FW_OFF_MONITOR 0x03


#define WIFI_LPHB_PROTO_UDP 0x01
#define WIFI_LPHB_PROTO_TCP 0x02

typedef PACKED_PRE struct PACKED_POST
{
   uint8 bssIdx;
   uint8 sessionIdx;
   uint8 protocolType; /*TCP or UDP*/
   uint8 eventReason;

}tHalLowPowerHeartBeatIndParam,*tpHalLowPowerHeartBeatIndParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalLowPowerHeartBeatIndParam lowPowerHeartBeatIndParams;
}tHalLowPowerHeartBeatIndMsg, *tpHalLowPowerHeartBeatIndMsg;

#endif

#ifdef FEATURE_WLAN_BATCH_SCAN

/*---------------------------------------------------------------------------
 * WLAN_HAL_BATCHSCAN_SET_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Scan Frerquency - default to 30Sec*/
    tANI_U32   scanInterval;
    tANI_U32   numScan2Batch;
    tANI_U32   bestNetworks;
    tANI_U8    rfBand;
    tANI_U8    rtt;
} tHalBatchScanSetParams, *tpHalBatchScanSetParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalBatchScanSetParams batchScanParams;
}  tHalBatchScanSetReqMsg, *tpHalBatchScanSetReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_BATCHSCAN_SET_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 supportedMscan;
}  tHalBatchScanSetRspParam, *tpHalBatchScanSetRspParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalBatchScanSetRspParam setBatchScanRspParam;
}  tHalBatchScanSetRspMsg, *tpHalBatchScanSetRspMsg;

/*---------------------------------------------------------------------------
* WLAN_HAL_BATCHSCAN_STOP_IND
*--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 param;
} tHalBatchScanStopIndParam, *tpHalBatchScanStopIndParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader  header;
   tHalBatchScanStopIndParam param;
} tHalBatchScanStopIndMsg, *tpHalBatchScanStopIndMsg;

/*---------------------------------------------------------------------------
* WLAN_HAL_BATCHSCAN_TRIGGER_RESULT_IND
*--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 param;
} tHalBatchScanTriggerResultParam, *tpHalBatchScanTriggerResultParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader  header;
   tHalBatchScanTriggerResultParam param;
} tHalBatchScanTriggerResultIndMsg, *tpHalBatchScanTriggerResultIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_BATCHSCAN_GET_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8   bssid[6];     /* BSSID */
    tANI_U8   ssid[33];     /* SSID */
    tANI_U8   ch;           /* Channel */
    tANI_S8   rssi;         /* RSSI or Level */
    /* Timestamp when Network was found. Used to calculate age based on timestamp in GET_RSP msg header */
    tANI_U32  timestamp;
} tHalBatchScanNetworkInfo, *tpHalBatchScanNetworkInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   scanId;                         /* Scan List ID. */
    /* No of AP in a Scan Result. Should be same as bestNetwork in SET_REQ msg */
    tANI_U32   numNetworksInScanList;
    /* Variable data ptr: Number of AP in Scan List */
    /* following numNetworkInScanList is data of type tHalBatchScanNetworkInfo
     * of sizeof(tHalBatchScanNetworkInfo) * numNetworkInScanList */
    tANI_U8    scanList[1];
} tHalBatchScanList, *tpHalBatchScanList;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32      timestamp;
    tANI_U32      numScanLists;
    boolean       isLastResult;
    /* Variable Data ptr: Number of Scan Lists*/
    /* following isLastResult is data of type tHalBatchScanList
     * of sizeof(tHalBatchScanList) * numScanLists*/
    tANI_U8       scanResults[1];
}  tHalBatchScanResultIndParam, *tpHalBatchScanResultIndParam;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalBatchScanResultIndParam resultIndMsgParam;
}  tHalBatchScanResultIndMsg, *tpHalBatchScanResultIndMsg;

#endif

/*---------------------------------------------------------------------------
 * WLAN_HAL_KEEP_ALIVE_REQ
 *--------------------------------------------------------------------------*/
/* Packet Types. */
#define HAL_KEEP_ALIVE_NULL_PKT              1
#define HAL_KEEP_ALIVE_UNSOLICIT_ARP_RSP     2

/* Enable or disable keep alive */
#define HAL_KEEP_ALIVE_DISABLE   0
#define HAL_KEEP_ALIVE_ENABLE    1

/* Keep Alive request. */
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8          packetType;
    tANI_U32         timePeriod;
    tHalIpv4Addr     hostIpv4Addr;
    tHalIpv4Addr     destIpv4Addr;
    tSirMacAddr      destMacAddr;
    tANI_U8          bssIdx;
} tHalKeepAliveReq, *tpHalKeepAliveReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalKeepAliveReq KeepAliveParams;
}  tHalKeepAliveReqMsg, *tpHalKeepAliveReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_RSSI_THRESH_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_S8   ucRssiThreshold1     : 8;
    tANI_S8   ucRssiThreshold2     : 8;
    tANI_S8   ucRssiThreshold3     : 8;
    tANI_U8   bRssiThres1PosNotify : 1;
    tANI_U8   bRssiThres1NegNotify : 1;
    tANI_U8   bRssiThres2PosNotify : 1;
    tANI_U8   bRssiThres2NegNotify : 1;
    tANI_U8   bRssiThres3PosNotify : 1;
    tANI_U8   bRssiThres3NegNotify : 1;
    tANI_U8   bReserved10          : 2;
} tHalRSSIThresholds, *tpHalRSSIThresholds;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalRSSIThresholds rssiThreshParams;
}  tHalRSSIThresholdReqMsg, *tpHalRSSIThresholdReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_UAPSD_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8     bkDeliveryEnabled:1;
    tANI_U8     beDeliveryEnabled:1;
    tANI_U8     viDeliveryEnabled:1;
    tANI_U8     voDeliveryEnabled:1;
    tANI_U8     bkTriggerEnabled:1;
    tANI_U8     beTriggerEnabled:1;
    tANI_U8     viTriggerEnabled:1;
    tANI_U8     voTriggerEnabled:1;
    tANI_U8     bssIdx;
} tUapsdReqParams, *tpUapsdReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tUapsdReqParams enterUapsdParams;
}  tHalEnterUapsdReqMsg, *tpHalEnterUapsdReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_UAPSD_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tANI_U8       bssIdx;
}  tHalExitUapsdReqMsg, *tpHalExitUapsdReqMsg;

#define HAL_PERIODIC_TX_PTRN_MAX_SIZE 1536
#define HAL_MAXNUM_PERIODIC_TX_PTRNS 6
/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_PERIODIC_TX_PTRN_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 selfStaIdx:8;
    tANI_U32 ucPtrnId:8;         // Pattern ID
    tANI_U32 usPtrnSize:16;      // Non-Zero Pattern size
    tANI_U32 uPtrnIntervalMs;    // In msec
    tANI_U8  ucPattern[HAL_PERIODIC_TX_PTRN_MAX_SIZE]; // Pattern buffer
} tHalAddPeriodicTxPtrn, *tpHalAddPeriodicTxPtrn;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalAddPeriodicTxPtrn ptrnParams;
}  tHalAddPeriodicTxPtrnIndMsg, *tpHalAddPeriodicTxPtrnIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_PERIODIC_TX_PTRN_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 selfStaIdx:8;
    tANI_U32 rsvd:24;
    /* Bitmap of pattern IDs that needs to be deleted */
    tANI_U32 uPatternIdBitmap;
} tHalDelPeriodicTxPtrn, *tpHalDelPeriodicTxPtrn;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalDelPeriodicTxPtrn ptrnParams;
}   tHalDelPeriodicTxPtrnIndMsg, *tpHalDelPeriodicTxPtrnIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_PERIODIC_TX_PTRN_FW_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Type of Failure indication */
    tANI_U32 bssIdx:8;
    tANI_U32 selfStaIdx:8;
    tANI_U32 rsvd:16;
    tANI_U32 status;
    tANI_U32 patternIdBitmap;
} tHalPeriodicTxPtrnFwInd, *tpHalPeriodicTxPtrnFwInd;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalPeriodicTxPtrnFwInd fwIndParams;
}   tHalPeriodicTxPtrnFwIndMsg, *tpHalPeriodicTxPtrnFwIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_WOWL_BCAST_PTRN
 *--------------------------------------------------------------------------*/
#define HAL_WOWL_BCAST_PATTERN_MAX_SIZE 128
#define HAL_WOWL_BCAST_MAX_NUM_PATTERNS 16

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8  ucPatternId;           // Pattern ID
    // Pattern byte offset from beginning of the 802.11 packet to start of the
    // wake-up pattern
    tANI_U8  ucPatternByteOffset;
    tANI_U8  ucPatternSize;         // Non-Zero Pattern size
    tANI_U8  ucPattern[HAL_WOWL_BCAST_PATTERN_MAX_SIZE]; // Pattern
    tANI_U8  ucPatternMaskSize;     // Non-zero pattern mask size
    tANI_U8  ucPatternMask[HAL_WOWL_BCAST_PATTERN_MAX_SIZE]; // Pattern mask
    tANI_U8  ucPatternExt[HAL_WOWL_BCAST_PATTERN_MAX_SIZE]; // Extra pattern
    tANI_U8  ucPatternMaskExt[HAL_WOWL_BCAST_PATTERN_MAX_SIZE]; // Extra pattern mask
    tANI_U8  bssIdx;
} tHalWowlAddBcastPtrn, *tpHalWowlAddBcastPtrn;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWowlAddBcastPtrn ptrnParams;
}  tHalWowlAddBcastPtrnReqMsg, *tpHalWowlAddBcastPtrnReqMsg;



/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_WOWL_BCAST_PTRN
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Pattern ID of the wakeup pattern to be deleted */
    tANI_U8  ucPatternId;
    tANI_U8  bssIdx;
} tHalWowlDelBcastPtrn, *tpHalWowlDelBcastPtrn;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWowlDelBcastPtrn ptrnParams;
}  tHalWowlDelBcastPtrnReqMsg, *tpHalWowlDelBcastPtrnReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_WOWL_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Enables/disables magic packet filtering */
    tANI_U8   ucMagicPktEnable;

    /* Magic pattern */
    tSirMacAddr magicPtrn;

    /* Enables/disables packet pattern filtering in firmware.
       Enabling this flag enables broadcast pattern matching
       in Firmware. If unicast pattern matching is also desired,
       ucUcastPatternFilteringEnable flag must be set tot true
       as well
    */
    tANI_U8   ucPatternFilteringEnable;

    /* Enables/disables unicast packet pattern filtering.
       This flag specifies whether we want to do pattern match
       on unicast packets as well and not just broadcast packets.
       This flag has no effect if the ucPatternFilteringEnable
       (main controlling flag) is set to false
    */
    tANI_U8   ucUcastPatternFilteringEnable;

    /* This configuration is valid only when magicPktEnable=1.
     * It requests hardware to wake up when it receives the
     * Channel Switch Action Frame.
     */
    tANI_U8   ucWowChnlSwitchRcv;

    /* This configuration is valid only when magicPktEnable=1.
     * It requests hardware to wake up when it receives the
     * Deauthentication Frame.
     */
    tANI_U8   ucWowDeauthRcv;

    /* This configuration is valid only when magicPktEnable=1.
     * It requests hardware to wake up when it receives the
     * Disassociation Frame.
     */
    tANI_U8   ucWowDisassocRcv;

    /* This configuration is valid only when magicPktEnable=1.
     * It requests hardware to wake up when it has missed
     * consecutive beacons. This is a hardware register
     * configuration (NOT a firmware configuration).
     */
    tANI_U8   ucWowMaxMissedBeacons;

    /* This configuration is valid only when magicPktEnable=1.
     * This is a timeout value in units of microsec. It requests
     * hardware to unconditionally wake up after it has stayed
     * in WoWLAN mode for some time. Set 0 to disable this feature.
     */
    tANI_U8   ucWowMaxSleepUsec;

    /* This configuration directs the WoW packet filtering to look for EAP-ID
     * requests embedded in EAPOL frames and use this as a wake source.
     */
    tANI_U8   ucWoWEAPIDRequestEnable;

    /* This configuration directs the WoW packet filtering to look for EAPOL-4WAY
     * requests and use this as a wake source.
     */
    tANI_U8   ucWoWEAPOL4WayEnable;

    /* This configuration allows a host wakeup on an network scan offload match.
     */
    tANI_U8   ucWowNetScanOffloadMatch;

    /* This configuration allows a host wakeup on any GTK rekeying error.
     */
    tANI_U8   ucWowGTKRekeyError;

    /* This configuration allows a host wakeup on BSS connection loss.
     */
    tANI_U8   ucWoWBSSConnLoss;

    tANI_U8   bssIdx;

} tHalWowlEnterParams, *tpHalWowlEnterParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWowlEnterParams enterWowlParams;
}  tHalWowlEnterReqMsg, *tpHalWowlEnterReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_WOWL_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8   bssIdx;

} tHalWowlExitParams, *tpHalWowlExitParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader     header;
   tHalWowlExitParams exitWowlParams;
}  tHalWowlExitReqMsg, *tpHalWowlExitReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_RSSI_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalGetRssiReqMsg, *tpHalGetRssiReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
   /* Valid STA Idx for per STA stats request */
   tANI_U32    staId;

}tHalRoamRssiReqParams, *tpHalRoamRssiReqParams;


/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_ROAM_RSSI_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalRoamRssiReqParams roamRssiReqParams;
}  tHalGetRoamRssiReqMsg, *tpHalGetRoamRssiReqMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_UAPSD_AC_PARAMS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST {
    tANI_U8  staidx;        // STA index
    tANI_U8  ac;            // Access Category
    tANI_U8  up;            // User Priority
    tANI_U32 srvInterval;   // Service Interval
    tANI_U32 susInterval;   // Suspend Interval
    tANI_U32 delayInterval; // Delay Interval
} tUapsdInfo, tpUapsdInfo;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tUapsdInfo    enableUapsdAcParams;
}  tHalSetUapsdAcParamsReqMsg, *tpHalSetUapsdAcParamsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_CONFIGURE_RXP_FILTER_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST {
    tANI_U8 setMcstBcstFilterSetting;
    tANI_U8 setMcstBcstFilter;
} tHalConfigureRxpFilterReqParams, tpHalConfigureRxpFilterReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalConfigureRxpFilterReqParams    configureRxpFilterReqParams;
}  tHalConfigureRxpFilterReqMsg, *tpHalConfigureRxpFilterReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_IMPS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalEnterImpsRspParams, *tpHalEnterImpsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnterImpsRspParams enterImpsRspParams;
}  tHalEnterImpsRspMsg, *tpHalEnterImpsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_IMPS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalExitImpsRspParams, *tpHalExitImpsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitImpsRspParams exitImpsRspParams;
}  tHalExitImpsRspMsg, *tpHalExitImpsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_BMPS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
    tANI_U8    bssIdx;
} tHalEnterBmpsRspParams, *tpHalEnterBmpsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnterBmpsRspParams enterBmpsRspParams;
}  tHalEnterBmpsRspMsg, *tpHalEnterBmpsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_BMPS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
    tANI_U8    bssIdx;
} tHalExitBmpsRspParams, *tpHalExitBmpsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitBmpsRspParams exitBmpsRspParams;
}  tHalExitBmpsRspMsg, *tpHalExitBmpsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_UAPSD_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32    status;
    tANI_U8     bssIdx;
}tUapsdRspParams, *tpUapsdRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tUapsdRspParams enterUapsdRspParams;
}  tHalEnterUapsdRspMsg, *tpHalEnterUapsdRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_UAPSD_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
    tANI_U8    bssIdx;
} tHalExitUapsdRspParams, *tpHalExitUapsdRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitUapsdRspParams exitUapsdRspParams;
}  tHalExitUapsdRspMsg, *tpHalExitUapsdRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_RSSI_NOTIFICATION_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32             bRssiThres1PosCross : 1;
    tANI_U32             bRssiThres1NegCross : 1;
    tANI_U32             bRssiThres2PosCross : 1;
    tANI_U32             bRssiThres2NegCross : 1;
    tANI_U32             bRssiThres3PosCross : 1;
    tANI_U32             bRssiThres3NegCross : 1;
    tANI_U32             avgRssi             : 8;
    tANI_U32             uBssIdx             : 8;
    tANI_U32             isBTCoexCompromise  : 1;
    tANI_U32             bReserved           : 9;
    tANI_S8              refRssiThreshold1;
    tANI_S8              refRssiThreshold2;
    tANI_S8              refRssiThreshold3;
} tHalRSSINotification, *tpHalRSSINotification;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalRSSINotification rssiNotificationParams;
}  tHalRSSINotificationIndMsg, *tpHalRSSINotificationIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_RSSI_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
    tANI_S8    rssi;
} tHalGetRssiParams, *tpHalGetRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalGetRssiParams rssiRspParams;
}  tHalGetRssiRspMsg, *tpHalGetRssiRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_ROAM_RSSI_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;

    tANI_U8    staId;
    tANI_S8    rssi;
} tHalGetRoamRssiParams, *tpHalGetRoamRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalGetRoamRssiParams roamRssiRspParams;
}  tHalGetRoamRssiRspMsg, *tpHalGetRoamRssiRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_WOWL_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
   tANI_U32   status;
   tANI_U8    bssIdx;
} tHalEnterWowlRspParams, *tpHalEnterWowlRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnterWowlRspParams enterWowlRspParams;
}  tHalWowlEnterRspMsg, *tpHalWowlEnterRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_WOWL_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
   tANI_U32   status;
   tANI_U8    bssIdx;
} tHalExitWowlRspParams, *tpHalExitWowlRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitWowlRspParams exitWowlRspParams;
}  tHalWowlExitRspMsg, *tpHalWowlExitRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BCN_FILTER_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalAddBcnFilterRspParams, *tpHalAddBcnFilterRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalAddBcnFilterRspParams addBcnFilterRspParams;
}  tHalAddBcnFilterRspMsg, *tpHalAddBcnFilterRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_REM_BCN_FILTER_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalRemBcnFilterRspParams, *tpHalRemBcnFilterRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalRemBcnFilterRspParams remBcnFilterRspParams;
}  tHalRemBcnFilterRspMsg, *tpHalRemBcnFilterRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_WOWL_BCAST_PTRN_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
   tANI_U32   status;
   tANI_U8    bssIdx;
} tHalAddWowlBcastPtrnRspParams, *tpHalAddWowlBcastPtrnRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalAddWowlBcastPtrnRspParams addWowlBcastPtrnRspParams;
}  tHalAddWowlBcastPtrnRspMsg, *tpHalAddWowlBcastPtrnRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_WOWL_BCAST_PTRN_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
   tANI_U32   status;
   tANI_U8    bssIdx;
} tHalDelWowlBcastPtrnRspParams, *tpHalDelWowlBcastPtrnRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalDelWowlBcastPtrnRspParams delWowlBcastRspParams;
}  tHalDelWowlBcastPtrnRspMsg, *tpHalDelWowlBcastPtrnRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_OFFLOAD_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalHostOffloadRspParams, *tpHalHostOffloadRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalHostOffloadRspParams hostOffloadRspParams;
}  tHalHostOffloadRspMsg, *tpHalHostOffloadRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_KEEP_ALIVE_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalKeepAliveRspParams, *tpHalKeepAliveRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalKeepAliveRspParams keepAliveRspParams;
}  tHalKeepAliveRspMsg, *tpHalKeepAliveRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_RSSI_THRESH_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalSetRssiThreshRspParams, *tpHalSetRssiThreshRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalSetRssiThreshRspParams setRssiThreshRspParams;
}  tHalSetRssiThreshRspMsg, *tpHalSetRssiThreshRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_UAPSD_AC_PARAMS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalSetUapsdAcParamsRspParams, *tpHalSetUapsdAcParamsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalSetUapsdAcParamsRspParams setUapsdAcParamsRspParams;
}  tHalSetUapsdAcParamsRspMsg, *tpHalSetUapsdAcParamsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_CONFIGURE_RXP_FILTER_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalConfigureRxpFilterRspParams, *tpHalConfigureRxpFilterRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalConfigureRxpFilterRspParams configureRxpFilterRspParams;
}  tHalConfigureRxpFilterRspMsg, *tpHalConfigureRxpFilterRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_SET_MAX_TX_POWER_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr bssId;  // BSSID is needed to identify which session issued this request. As
                        //the request has power constraints, this should be applied only to that session
    tSirMacAddr selfStaMacAddr;
    //In request,
    //power == MaxTx power to be used.
    tPowerdBm  power;

}tSetMaxTxPwrParams, *tpSetMaxTxPwrParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetMaxTxPwrParams setMaxTxPwrParams;
}tSetMaxTxPwrReq, *tpSetMaxTxPwrReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_SET_MAX_TX_POWER_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    //power == tx power used for management frames.
    tPowerdBm  power;

    /* success or failure */
    tANI_U32   status;
}tSetMaxTxPwrRspParams, *tpSetMaxTxPwrRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetMaxTxPwrRspParams setMaxTxPwrRspParams;
}tSetMaxTxPwrRspMsg, *tpSetMaxTxPwrRspMsg;


/*---------------------------------------------------------------------------
 *WLAN_HAL_SET_MAX_TX_POWER_PER_BAND_REQ
 *--------------------------------------------------------------------------*/

/* Band types for WLAN_HAL_SET_MAX_TX_POWER_PER_BAND_REQ between WDI and HAL */
typedef enum
{
   WLAN_HAL_SET_MAX_TX_POWER_BAND_ALL = 0,
   // For 2.4GHz or 5GHz bands
   WLAN_HAL_SET_MAX_TX_POWER_BAND_2_4_GHZ,
   WLAN_HAL_SET_MAX_TX_POWER_BAND_5_0_GHZ,
   // End of valid enums
   WLAN_HAL_SET_MAX_TX_POWER_BAND_MAX = WLAN_HAL_MAX_ENUM_SIZE,
}tHalSetMaxTxPwrBandInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tHalSetMaxTxPwrBandInfo bandInfo;  // 2_4_GHZ or 5_0_GHZ
    tPowerdBm   power;  // In request, power == MaxTx power to be used.
}tSetMaxTxPwrPerBandParams, *tpSetMaxTxPwrPerBandParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetMaxTxPwrPerBandParams setMaxTxPwrPerBandParams;
}tSetMaxTxPwrPerBandReq, *tpSetMaxTxPwrPerBandReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_SET_MAX_TX_POWER_PER_BAND_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    //power == tx power used for management frames.
    tPowerdBm  power;

    /* success or failure */
    tANI_U32   status;
}tSetMaxTxPwrPerBandRspParams, *tpSetMaxTxPwrPerBandRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetMaxTxPwrPerBandRspParams setMaxTxPwrPerBandRspParams;
}tSetMaxTxPwrPerBandRspMsg, *tpSetMaxTxPwrPerBandRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_SET_TX_POWER_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* TX Power in milli watts */
    tANI_U32  txPower;
    tANI_U8   bssIdx;
}tSetTxPwrReqParams, *tpSetTxPwrReqParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetTxPwrReqParams setTxPwrReqParams;
}tSetTxPwrReqMsg, *tpSetTxPwrReqMsg;

/*---------------------------------------------------------------------------
*WLAN_HAL_SET_TX_POWER_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSetTxPwrRspParams, *tpSetTxPwrRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetTxPwrRspParams setTxPwrRspParams;
}tSetTxPwrRspMsg, *tpSetTxPwrRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_GET_TX_POWER_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8  staId;
}tGetTxPwrReqParams, *tpGetTxPwrReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tGetTxPwrReqParams getTxPwrReqParams;
}tGetTxPwrReqMsg, *tpGetTxPwrReqMsg;

/*---------------------------------------------------------------------------
*WLAN_HAL_GET_TX_POWER_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;

    /* TX Power in milli watts */
    tANI_U32   txPower;
}tGetTxPwrRspParams, *tpGetTxPwrRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tGetTxPwrRspParams getTxPwrRspParams;
}tGetTxPwrRspMsg, *tpGetTxPwrRspMsg;

#ifdef WLAN_FEATURE_P2P
/*---------------------------------------------------------------------------
 *WLAN_HAL_SET_P2P_GONOA_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  tANI_U8   opp_ps;
  tANI_U32  ctWindow;
  tANI_U8   count;
  tANI_U32  duration;
  tANI_U32  interval;
  tANI_U32  single_noa_duration;
  tANI_U8   psSelection;
}tSetP2PGONOAParams, *tpSetP2PGONOAParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetP2PGONOAParams setP2PGONOAParams;
}tSetP2PGONOAReq, *tpSetP2PGONOAReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_SET_P2P_GONOA_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSetP2PGONOARspParams, *tpSetP2PGONOARspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetP2PGONOARspParams setP2PGONOARspParams;
}tSetP2PGONOARspMsg, *tpSetP2PGONOARspMsg;
#endif

/*---------------------------------------------------------------------------
 *WLAN_HAL_ADD_SELF_STA_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  tSirMacAddr selfMacAddr;
  tANI_U32    status;
}tAddStaSelfParams, *tpAddStaSelfParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddStaSelfParams addStaSelfParams;
}tAddStaSelfReq, *tpAddStaSelfReq;

/* This V1 structure carries additionally the IFACE PERSONA
   of the interface as compared to the legacy control
   message */
typedef PACKED_PRE struct PACKED_POST
{
  tSirMacAddr selfMacAddr;
  tANI_U32    status;
  tHalIfacePersona iface_persona;
}tAddStaSelfParams_V1, *tpAddStaSelfParams_V1;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddStaSelfParams_V1 addStaSelfParams_V1;
}tAddStaSelfReq_V1, *tpAddStaSelfReq_V1;

/*---------------------------------------------------------------------------
*WLAN_HAL_ADD_SELF_STA_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;

   /*Self STA Index */
   tANI_U8    selfStaIdx;

   /* DPU Index (IGTK, PTK, GTK all same) */
   tANI_U8 dpuIdx;

   /* DPU Signature */
   tANI_U8 dpuSignature;

}tAddStaSelfRspParams, *tpAddStaSelfRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddStaSelfRspParams addStaSelfRspParams;
}tAddStaSelfRspMsg, *tpAddStaSelfRspMsg;


/*---------------------------------------------------------------------------
  WLAN_HAL_DEL_STA_SELF_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tSirMacAddr selfMacAddr;

}tDelStaSelfParams, *tpDelStaSelfParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDelStaSelfParams delStaSelfParams;
}  tDelStaSelfReqMsg, *tpDelStaSelfReqMsg;


/*---------------------------------------------------------------------------
  WLAN_HAL_DEL_STA_SELF_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

  tSirMacAddr selfMacAddr;
}tDelStaSelfRspParams, *tpDelStaSelfRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDelStaSelfRspParams delStaSelfRspParams;
}  tDelStaSelfRspMsg, *tpDelStaSelfRspMsg;


#ifdef WLAN_FEATURE_VOWIFI_11R

/*---------------------------------------------------------------------------
 *WLAN_HAL_AGGR_ADD_TS_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* TSPEC handler uniquely identifying a TSPEC for a STA in a BSS */
    /* This will carry the bitmap with the bit positions representing different AC.s*/
    tANI_U16 tspecIdx;

    /*  Tspec info per AC To program TPE with required parameters */
    tSirMacTspecIE   tspec[WLAN_HAL_MAX_AC];

    /* U-APSD Flags: 1b per AC.  Encoded as follows:
     b7 b6 b5 b4 b3 b2 b1 b0 =
     X  X  X  X  BE BK VI VO */
    tANI_U8 uAPSD;

    /* These parameters are for all the access categories */
    tANI_U32 srvInterval[WLAN_HAL_MAX_AC];   // Service Interval
    tANI_U32 susInterval[WLAN_HAL_MAX_AC];   // Suspend Interval
    tANI_U32 delayInterval[WLAN_HAL_MAX_AC]; // Delay Interval

}tAggrAddTsParams, *tpAggrAddTsParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAggrAddTsParams aggrAddTsParam;
}tAggrAddTsReq, *tpAggrAddTsReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_AGGR_ADD_TS_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status0;
    /* FIXME PRIMA for future use for 11R */
    tANI_U32   status1;
}tAggrAddTsRspParams, *tpAggrAddTsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAggrAddTsRspParams aggrAddTsRspParam;
}tAggrAddTsRspMsg, *tpAggrAddTsRspMsg;

#endif

/*---------------------------------------------------------------------------
 * WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8   isAppsCpuAwake;
} tHalConfigureAppsCpuWakeupStateReqParams, *tpHalConfigureAppsCpuWakeupStatReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalConfigureAppsCpuWakeupStateReqParams appsStateReqParams;
}  tHalConfigureAppsCpuWakeupStateReqMsg, *tpHalConfigureAppsCpuWakeupStateReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalConfigureAppsCpuWakeupStateRspParams, *tpHalConfigureAppsCpuWakeupStateRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalConfigureAppsCpuWakeupStateRspParams appsStateRspParams;
}  tHalConfigureAppsCpuWakeupStateRspMsg, *tpHalConfigureAppsCpuWakeupStateRspMsg;
/*---------------------------------------------------------------------------
 * WLAN_HAL_DUMP_COMMAND_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32    argument1;
   tANI_U32    argument2;
   tANI_U32    argument3;
   tANI_U32    argument4;
   tANI_U32    argument5;

}tHalDumpCmdReqParams,*tpHalDumpCmdReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader          header;
   tHalDumpCmdReqParams   dumpCmdReqParams;
} tHalDumpCmdReqMsg, *tpHalDumpCmdReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DUMP_COMMAND_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
    /*Length of the responce message*/
    tANI_U32   rspLength;
    /*FiXME: Currently considering the  the responce will be less than 100bytes */
    tANI_U8    rspBuffer[DUMPCMD_RSP_BUFFER];

} tHalDumpCmdRspParams, *tpHalDumpCmdRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalDumpCmdRspParams dumpCmdRspParams;
}  tHalDumpCmdRspMsg, *tpHalDumpCmdRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_COEX_IND
 *-------------------------------------------------------------------------*/
#define WLAN_COEX_IND_DATA_SIZE (4)
#define WLAN_COEX_IND_TYPE_DISABLE_HB_MONITOR (0)
#define WLAN_COEX_IND_TYPE_ENABLE_HB_MONITOR (1)
#define WLAN_COEX_IND_TYPE_SCANS_ARE_COMPROMISED_BY_COEX (2)
#define WLAN_COEX_IND_TYPE_SCANS_ARE_NOT_COMPROMISED_BY_COEX (3)
#define WLAN_COEX_IND_TYPE_DISABLE_AGGREGATION_IN_2p4 (4)
#define WLAN_COEX_IND_TYPE_ENABLE_AGGREGATION_IN_2p4 (5)
#define WLAN_COEX_IND_TYPE_ENABLE_UAPSD (6)
#define WLAN_COEX_IND_TYPE_DISABLE_UAPSD (7)
#define WLAN_COEX_IND_TYPE_CXM_FEATURES_NOTIFICATION (8)

typedef PACKED_PRE struct PACKED_POST
{
    /*Coex Indication Type*/
    tANI_U32   coexIndType;

    /*Coex Indication Data*/
    tANI_U32   coexIndData[WLAN_COEX_IND_DATA_SIZE];
}tCoexIndParams,*tpCoexIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader   header;
   tCoexIndParams  coexIndParams;
}tCoexIndMsg, *tpCoexIndMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_OTA_TX_COMPL_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /*Tx Complete Indication Success or Failure*/
   tANI_U32   status;
   /* Dialog token */
   tANI_U32   dialogToken;
}tTxComplParams,*tpTxComplParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader   header;
   tTxComplParams  txComplParams;
}tTxComplIndMsg, *tpTxComplIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_SUSPEND_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 configuredMcstBcstFilterSetting;
    tANI_U32 activeSessionCount;
}tHalWlanHostSuspendIndParam,*tpHalWlanHostSuspendIndParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWlanHostSuspendIndParam suspendIndParams;
}tHalWlanHostSuspendIndMsg, *tpHalWlanHostSuspendIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXCLUDE_UNENCRYTED_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_BOOLEAN bDot11ExcludeUnencrypted;
    tSirMacAddr bssId;
}tHalWlanExcludeUnEncryptedIndParam,*tpHalWlanExcludeUnEncryptedIndParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWlanExcludeUnEncryptedIndParam excludeUnEncryptedIndParams;
}tHalWlanExcludeUnEncrptedIndMsg, *tpHalWlanExcludeUnEncrptedIndMsg;

#ifdef WLAN_FEATURE_P2P
/*---------------------------------------------------------------------------
 *WLAN_HAL_NOA_ATTR_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8      index ;
   tANI_U8      oppPsFlag ;
   tANI_U16     ctWin  ;

   tANI_U16      uNoa1IntervalCnt;
   tANI_U16      bssIdx;
   tANI_U32      uNoa1Duration;
   tANI_U32      uNoa1Interval;
   tANI_U32      uNoa1StartTime;

   tANI_U16      uNoa2IntervalCnt;
   tANI_U16      rsvd2;
   tANI_U32      uNoa2Duration;
   tANI_U32      uNoa2Interval;
   tANI_U32      uNoa2StartTime;

   tANI_U32   status;
}tNoaAttrIndParams, *tpNoaAttrIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader      header;
   tNoaAttrIndParams  noaAttrIndParams;
}tNoaAttrIndMsg, *tpNoaAttrIndMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_NOA_START_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32             status;
    tANI_U32             bssIdx;
}tNoaStartIndParams, *tpNoaStartIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader      header;
   tNoaStartIndParams noaStartIndParams;
}tNoaStartIndMsg, tpNoaStartIndMsg;
#endif

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_RESUME_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8 configuredMcstBcstFilterSetting;
}tHalWlanHostResumeReqParam,*tpHalWlanHostResumeReqParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWlanHostResumeReqParam resumeReqParams;
}tHalWlanHostResumeReqMsg, *tpHalWlanHostResumeReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_RESUME_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalHostResumeRspParams, *tpHalHostResumeRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalHostResumeRspParams hostResumeRspParams;
}  tHalHostResumeRspMsg, *tpHalHostResumeRspMsg;

typedef PACKED_PRE struct PACKED_POST
{
	tANI_U16 staIdx;
	// Peer MAC Address, whose BA session has timed out
	tSirMacAddr peerMacAddr;
	// TID for which a BA session timeout is being triggered
	tANI_U8 baTID;
	// DELBA direction
	// 1 - Originator
	// 0 - Recipient
	tANI_U8 baDirection;
	tANI_U32 reasonCode;
	tSirMacAddr  bssId; // TO SUPPORT BT-AMP
} tHalWlanDelBaIndMsg, *tpHalWlanDelBaIndMsg;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader   header;
	tHalWlanDelBaIndMsg hostdelBaParam;
} tHalDelBAIndMsg, *tpHalDelBAIndMsg;

/*---------------------------------------------------------------------------
 *PNO Messages
 *-------------------------------------------------------------------------*/
/* Max number of channels that a network can be found on*/
/* WLAN_HAL_PNO_MAX_NETW_CHANNELS and WLAN_HAL_PNO_MAX_NETW_CHANNELS_EX should
 * be changed at same time
 */
#define WLAN_HAL_PNO_MAX_NETW_CHANNELS  60

/*Max number of channels that a network can be found on*/
#define WLAN_HAL_PNO_MAX_NETW_CHANNELS_EX  60

/*Maximum numbers of networks supported by PNO*/
#define WLAN_HAL_PNO_MAX_SUPP_NETWORKS  16

/*The number of scan time intervals that can be programmed into PNO*/
#define WLAN_HAL_PNO_MAX_SCAN_TIMERS    10

/*Maximum size of the probe template*/
#define WLAN_HAL_PNO_MAX_PROBE_SIZE     450

/*Type of PNO enabling
  Immediate - scanning will start immediately and PNO procedure will
  be repeated based on timer
  Suspend - scanning will start at suspend
  Resume - scanning will start on system resume
  Delay - start the scan timer to trigger PNO scan
  */
typedef enum
{
   ePNO_MODE_IMMEDIATE,
   ePNO_MODE_ON_SUSPEND,
   ePNO_MODE_ON_RESUME,
   ePNO_MODE_DELAY,
   ePNO_MODE_PROXIMITY,  // FEATURE_WIFI_PROXIMITY
   ePNO_MODE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} ePNOMode;

/*Authentication type*/
typedef enum
{
    eAUTH_TYPE_ANY                   = 0,
    eAUTH_TYPE_OPEN_SYSTEM           = 1,

    // Upper layer authentication types
    eAUTH_TYPE_WPA                   = 2,
    eAUTH_TYPE_WPA_PSK               = 3,

    eAUTH_TYPE_RSN                   = 4,
    eAUTH_TYPE_RSN_PSK               = 5,
    eAUTH_TYPE_FT_RSN                = 6,
    eAUTH_TYPE_FT_RSN_PSK            = 7,
    eAUTH_TYPE_WAPI_WAI_CERTIFICATE  = 8,
    eAUTH_TYPE_WAPI_WAI_PSK          = 9,
    eAUTH_TYPE_CCKM_WPA              = 10,
    eAUTH_TYPE_CCKM_RSN              = 11,
    eAUTH_TYPE_RSN_PSK_SHA256        = 12,
    eAUTH_TYPE_RSN_8021X_SHA256      = 13,

    eAUTH_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE

}tAuthType;

/* Encryption type */
typedef enum eEdType
{
    eED_ANY           = 0,
    eED_NONE          = 1,
    eED_WEP           = 2,
    eED_TKIP          = 3,
    eED_CCMP          = 4,
    eED_WPI           = 5,

    eED_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tEdType;

/* SSID broadcast  type */
typedef enum eSSIDBcastType
{
  eBCAST_UNKNOWN      = 0,
  eBCAST_NORMAL       = 1,
  eBCAST_HIDDEN       = 2,

  eBCAST_TYPE_MAX     = WLAN_HAL_MAX_ENUM_SIZE
} tSSIDBcastType;

/*
  The network description for which PNO will have to look for
*/
typedef PACKED_PRE struct PACKED_POST
{
  /*SSID of the BSS*/
  tSirMacSSid ssId;

  /*Authentication type for the network*/
  tAuthType   authentication;

  /*Encryption type for the network*/
  tEdType     encryption;

  /*Indicate the channel on which the Network can be found
    0 - if all channels */
  tANI_U8     ucChannelCount;
  tANI_U8     aChannels[WLAN_HAL_PNO_MAX_NETW_CHANNELS];

  /*Indicates the RSSI threshold for the network to be considered*/
  tANI_U8     rssiThreshold;
}tNetworkType;

typedef PACKED_PRE struct PACKED_POST
{
  /*How much it should wait */
  tANI_U32    uTimerValue;

  /*How many times it should repeat that wait value
    0 - keep using this timer until PNO is disabled*/
  tANI_U32    uTimerRepeat;

  /*e.g:   2 3
           4 0
    - it will wait 2s between consecutive scans for 3 times
    - after that it will wait 4s between consecutive scans until disabled*/
}tScanTimer;

/*
  The network parameters to be sent to the PNO algorithm
*/
typedef PACKED_PRE struct PACKED_POST
{
  /*set to 0 if you wish for PNO to use its default telescopic timer*/
  tANI_U8     ucScanTimersCount;

  /*A set value represents the amount of time that PNO will wait between
    two consecutive scan procedures
    If the desired is for a uniform timer that fires always at the exact same
    interval - one single value is to be set
    If there is a desire for a more complex - telescopic like timer multiple
    values can be set - once PNO reaches the end of the array it will
    continue scanning at intervals presented by the last value*/
  tScanTimer  aTimerValues[WLAN_HAL_PNO_MAX_SCAN_TIMERS];

}tScanTimersType;

typedef PACKED_PRE struct PACKED_POST {

    /*Enable PNO*/
    tANI_U32          enable;

    /*Immediate,  On Suspend,   On Resume*/
    ePNOMode         modePNO;

    /*Number of networks sent for PNO*/
    tANI_U32          ucNetworksCount;

    /*The networks that PNO needs to look for*/
    tNetworkType     aNetworks[WLAN_HAL_PNO_MAX_SUPP_NETWORKS];

    /*The scan timers required for PNO*/
    tScanTimersType  scanTimers;

    /*Probe template for 2.4GHz band*/
    tANI_U16         us24GProbeSize;
    tANI_U8          a24GProbeTemplate[WLAN_HAL_PNO_MAX_PROBE_SIZE];

    /*Probe template for 5GHz band*/
    tANI_U16         us5GProbeSize;
    tANI_U8          a5GProbeTemplate[WLAN_HAL_PNO_MAX_PROBE_SIZE];

} tPrefNetwListParams, * tpPrefNetwListParams;

/*
  Preferred network list request
*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tPrefNetwListParams   prefNetwListParams;
}  tSetPrefNetwListReq, *tpSetPrefNetwListReq;


/*
  The network description for which PNO will have to look for
*/
typedef PACKED_PRE struct PACKED_POST
{
  /*SSID of the BSS*/
  tSirMacSSid ssId;

  /*Authentication type for the network*/
  tAuthType   authentication;

  /*Encryption type for the network*/
  tEdType     encryption;

  /*SSID broadcast type, normal, hidden or unknown*/
  tSSIDBcastType bcastNetworkType;

  /*Indicate the channel on which the Network can be found
    0 - if all channels */
  tANI_U8     ucChannelCount;
  tANI_U8     aChannels[WLAN_HAL_PNO_MAX_NETW_CHANNELS];

  /*Indicates the RSSI threshold for the network to be considered*/
  tANI_U8     rssiThreshold;
}tNetworkTypeNew;

typedef PACKED_PRE struct PACKED_POST {

    /*Enable PNO*/
    tANI_U32          enable;

    /*Immediate,  On Suspend,   On Resume*/
    ePNOMode         modePNO;

    /*Number of networks sent for PNO*/
    tANI_U32         ucNetworksCount;

    /*The networks that PNO needs to look for*/
    tNetworkTypeNew  aNetworks[WLAN_HAL_PNO_MAX_SUPP_NETWORKS];

    /*The scan timers required for PNO*/
    tScanTimersType  scanTimers;

    /*Probe template for 2.4GHz band*/
    tANI_U16         us24GProbeSize;
    tANI_U8          a24GProbeTemplate[WLAN_HAL_PNO_MAX_PROBE_SIZE];

    /*Probe template for 5GHz band*/
    tANI_U16         us5GProbeSize;
    tANI_U8          a5GProbeTemplate[WLAN_HAL_PNO_MAX_PROBE_SIZE];

} tPrefNetwListParamsNew, * tpPrefNetwListParamsNew;

/*
  Preferred network list request new
*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tPrefNetwListParamsNew   prefNetwListParams;
}  tSetPrefNetwListReqNew, *tpSetPrefNetwListReqNew;

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
typedef PACKED_PRE struct PACKED_POST
{
   tSirMacSSid ssId;
   tANI_U8     currAPbssid[HAL_MAC_ADDR_LEN];
   tANI_U32    authentication;
   tEdType     encryption;
   tEdType     mcencryption;
   tANI_U8     ChannelCount;
   tANI_U8     ChannelCache[WLAN_HAL_ROAM_SCAN_MAX_CHANNELS];
}tRoamNetworkType;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8 mdiePresent;
   tANI_U16 mobilityDomain;
}tMobilityDomainInfo;

typedef PACKED_PRE struct PACKED_POST {
   eAniBoolean       RoamScanOffloadEnabled;
   tANI_S8           LookupThreshold;
   tANI_U8           RoamRssiDiff;
   tANI_U8           ChannelCacheType;
   tANI_U8           Command;
   tANI_U8           StartScanReason;
   tANI_U16          NeighborScanTimerPeriod;
   tANI_U16          NeighborRoamScanRefreshPeriod;
   tANI_U16          NeighborScanChannelMinTime;
   tANI_U16          NeighborScanChannelMaxTime;
   tANI_U16          EmptyRefreshScanPeriod;
   tANI_U8           ValidChannelCount;
   tANI_U8           ValidChannelList[WLAN_HAL_ROAM_SCAN_MAX_CHANNELS];
   eAniBoolean       IsESEEnabled;

   tANI_U16          us24GProbeSize;
   tANI_U8           a24GProbeTemplate[WLAN_HAL_ROAM_SCAN_MAX_PROBE_SIZE];
   tANI_U16          us5GProbeSize;
   tANI_U8           a5GProbeTemplate[WLAN_HAL_ROAM_SCAN_MAX_PROBE_SIZE];
   /* Add Reserved bytes */
   tANI_U8           nProbes;
   tANI_U16          HomeAwayTime;
   eAniBoolean       MAWCEnabled;
   tANI_S8           RxSensitivityThreshold;
   tANI_U8           RoamOffloadEnabled;
   tANI_U8           PMK[WLAN_HAL_ROAM_SACN_PMK_SIZE];
   tANI_U8           Prefer5GHz;
   tANI_U8           RoamRssiCatGap;
   tANI_U8           Select5GHzMargin;
   tANI_U8           ReservedBytes[WLAN_HAL_ROAM_SCAN_RESERVED_BYTES];
   tRoamNetworkType  ConnectedNetwork;
   tMobilityDomainInfo MDID;
} tRoamCandidateListParams, * tpRoamCandidateListParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRoamCandidateListParams RoamScanOffloadNetwListParams;
}  tSetRoamScanOffloadReq, *tpRoamScanOffloadReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;

  /* status of the request - just to indicate that PNO has acknowledged
   * the request and will start scanning */
   tANI_U32   status;
}  tSetRoamOffloadScanResp, *tpSetRoamOffloadScanResp;
#endif

/*
  Preferred network list response
*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;

   /* status of the request - just to indicate that PNO has acknowledged
    * the request and will start scanning*/
   tANI_U32   status;
}  tSetPrefNetwListResp, *tpSetPrefNetwListResp;

/*
  Preferred network indication parameters
*/
typedef PACKED_PRE struct PACKED_POST {

  /*Network that was found with the highest RSSI*/
  tSirMacSSid ssId;

  /*Indicates the RSSI */
  tANI_U8     rssi;

  //The MPDU frame length of a beacon or probe rsp. data is the start of the frame
  tANI_U16    frameLength;

} tPrefNetwFoundParams, * tpPrefNetwFoundParams;

/*
  Preferred network found indication
*/
typedef PACKED_PRE struct PACKED_POST {

   tHalMsgHeader header;
   tPrefNetwFoundParams   prefNetwFoundParams;
}  tPrefNetwFoundInd, *tpPrefNetwFoundInd;


typedef PACKED_PRE struct PACKED_POST {

  /*RSSI Threshold*/
  tANI_U8          ucRssiThreshold;

} tRssiFilterParams, * tpRssiFilterParams;

/*
  RSSI Filter request
*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRssiFilterParams   prefRSSIFilterParams;
}  tSetRssiFilterReq, *tpSetRssiFilterReq;

/*
 Set RSSI filter resp
*/
typedef PACKED_PRE struct PACKED_POST{
   tHalMsgHeader header;
   /*status of the request */
   tANI_U32   status;
}  tSetRssiFilterResp, *tpSetRssiFilterResp;
/*
  Update scan params
*/
typedef PACKED_PRE struct PACKED_POST
{

  /*Host setting for 11d*/
  tANI_U8   b11dEnabled;

  /*Lets PNO know that host has determined the regulatory domain*/
  tANI_U8   b11dResolved;

  /*Channels on which PNO is allowed to scan*/
  tANI_U8   ucChannelCount;
  tANI_U8   aChannels[WLAN_HAL_PNO_MAX_NETW_CHANNELS];

  /*Minimum channel time*/
  tANI_U16  usActiveMinChTime;

  /*Maximum channel time*/
  tANI_U16  usActiveMaxChTime;

  /*Minimum channel time*/
  tANI_U16  usPassiveMinChTime;

  /*Maximum channel time*/
  tANI_U16  usPassiveMaxChTime;

  /*Cb State*/
  ePhyChanBondState cbState;

} tUpdateScanParams, * tpUpdateScanParams;

/*
  Update scan params
*/
typedef PACKED_PRE struct PACKED_POST
{

  /*Host setting for 11d*/
  tANI_U8   b11dEnabled;

  /*Lets PNO know that host has determined the regulatory domain*/
  tANI_U8   b11dResolved;

  /*Channels on which PNO is allowed to scan*/
  tANI_U8   ucChannelCount;
  tANI_U8   aChannels[WLAN_HAL_PNO_MAX_NETW_CHANNELS_EX];

  /*Minimum channel time*/
  tANI_U16  usActiveMinChTime;

  /*Maximum channel time*/
  tANI_U16  usActiveMaxChTime;

  /*Minimum channel time*/
  tANI_U16  usPassiveMinChTime;

  /*Maximum channel time*/
  tANI_U16  usPassiveMaxChTime;

  /*Cb State*/
  ePhyChanBondState cbState;

} tUpdateScanParamsEx, * tpUpdateScanParamsEx;

/*
  Update scan params - sent from host to PNO
  to be used during PNO scanning
*/
typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;
   tUpdateScanParams   scanParams;
}  tUpdateScanParamsReq, *tpUpdateScanParamsReq;

/*
  Update scan params - sent from host to PNO
  to be used during PNO scanning
*/
typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;
   tUpdateScanParamsEx   scanParams;
}  tUpdateScanParamsReqEx, *tpUpdateScanParamsReqEx;

/*
  Update scan params - sent from host to PNO
  to be used during PNO scanning
*/
typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;

   /*status of the request */
   tANI_U32   status;

}  tUpdateScanParamsResp, *tpUpdateScanParamsResp;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_TX_PER_TRACKING_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8  ucTxPerTrackingEnable;           /* 0: disable, 1:enable */
    tANI_U8  ucTxPerTrackingPeriod;           /* Check period, unit is sec. */
    tANI_U8  ucTxPerTrackingRatio;            /* (Fail TX packet)/(Total TX packet) ratio, the unit is 10%. */
    tANI_U32 uTxPerTrackingWatermark;         /* A watermark of check number, once the tx packet exceed this number, we do the check, default is 5 */
} tHalTxPerTrackingReqParam, *tpHalTxPerTrackingReqParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalTxPerTrackingReqParam txPerTrackingParams;
}  tHalSetTxPerTrackingReqMsg, *tpHalSetTxPerTrackingReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_TX_PER_TRACKING_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalTxPerTrackingRspParams, *tpHalTxPerTrackingRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalTxPerTrackingRspParams txPerTrackingRspParams;
}  tHalSetTxPerTrackingRspMsg, *tpHalSetTxPerTrackingRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_TX_PER_HIT_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader   header;
}tTxPerHitIndMsg, *tpTxPerHitIndMsg;

/*---------------------------------------------------------------------------
 *******************Packet Filtering Definitions Begin*******************
 *--------------------------------------------------------------------------*/
#define    HAL_PROTOCOL_DATA_LEN                  8
#define    HAL_MAX_NUM_MULTICAST_ADDRESS        240
#define    HAL_MAX_NUM_FILTERS                   20
#define    HAL_MAX_CMP_PER_FILTER                10

typedef enum
{
  HAL_RCV_FILTER_TYPE_INVALID,
  HAL_RCV_FILTER_TYPE_FILTER_PKT,
  HAL_RCV_FILTER_TYPE_BUFFER_PKT,
  HAL_RCV_FILTER_TYPE_MAX_ENUM_SIZE
}tHalReceivePacketFilterType;

typedef enum
{
  HAL_FILTER_PROTO_TYPE_INVALID,
  HAL_FILTER_PROTO_TYPE_MAC,
  HAL_FILTER_PROTO_TYPE_ARP,
  HAL_FILTER_PROTO_TYPE_IPV4,
  HAL_FILTER_PROTO_TYPE_IPV6,
  HAL_FILTER_PROTO_TYPE_UDP,
  HAL_FILTER_PROTO_TYPE_MAX
}tHalRcvPktFltProtocolType;

typedef enum
{
  HAL_FILTER_CMP_TYPE_INVALID,
  HAL_FILTER_CMP_TYPE_EQUAL,
  HAL_FILTER_CMP_TYPE_MASK_EQUAL,
  HAL_FILTER_CMP_TYPE_NOT_EQUAL,
  HAL_FILTER_CMP_TYPE_MAX
}tHalRcvPktFltCmpFlagType;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8                          protocolLayer;
    tANI_U8                          cmpFlag;
    tANI_U16                         dataLength; /* Length of the data to compare */
    tANI_U8                          dataOffset; /* from start of the respective frame header */
    tANI_U8                          reserved; /* Reserved field */
    tANI_U8                          compareData[HAL_PROTOCOL_DATA_LEN];  /* Data to compare */
    tANI_U8                          dataMask[HAL_PROTOCOL_DATA_LEN];   /* Mask to be applied on the received packet data before compare */
}tHalRcvPktFilterParams, *tpHalRcvPktFilterParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8                         filterId;
    tANI_U8                         filterType;
    tANI_U8                         numParams;
    tANI_U32                        coalesceTime;
    tHalRcvPktFilterParams          paramsData[1];
}tHalRcvPktFilterCfgType, *tpHalRcvPktFilterCfgType;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8                         filterId;
    tANI_U8                         filterType;
    tANI_U8                         numParams;
    tANI_U32                        coleasceTime;
    tANI_U8                         bssIdx;
    tHalRcvPktFilterParams          paramsData[1];
}tHalSessionizedRcvPktFilterCfgType, *tpHalSessionizedRcvPktFilterCfgType;

typedef PACKED_PRE struct PACKED_POST
{
  tHalMsgHeader                 header;
  tHalRcvPktFilterCfgType       pktFilterCfg;
} tHalSetRcvPktFilterReqMsg, *tpHalSetRcvPktFilterReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8         dataOffset; /* from start of the respective frame header */
    tANI_U32       cMulticastAddr;
    tSirMacAddr    multicastAddr[HAL_MAX_NUM_MULTICAST_ADDRESS];
    tANI_U8        bssIdx;
} tHalRcvFltMcAddrListType, *tpHalRcvFltMcAddrListType;

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
    tANI_U8    bssIdx;
} tHalSetPktFilterRspParams, *tpHalSetPktFilterRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader               header;
   tHalSetPktFilterRspParams   pktFilterRspParams;
}  tHalSetPktFilterRspMsg, *tpHalSetPktFilterRspMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8    bssIdx;
} tHalRcvFltPktMatchCntReqParams, *tpHalRcvFltPktMatchCntReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader        header;
   tHalRcvFltPktMatchCntReqParams   pktMatchCntReqParams;
} tHalRcvFltPktMatchCntReqMsg, *tpHalRcvFltPktMatchCntReqMsg;


typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8    filterId;
   tANI_U32   matchCnt;
} tHalRcvFltPktMatchCnt;
typedef PACKED_PRE struct PACKED_POST
{
   /* Success or Failure */
   tANI_U32                 status;
   tANI_U32                 matchCnt;
   tHalRcvFltPktMatchCnt    filterMatchCnt[HAL_MAX_NUM_FILTERS];
   tANI_U8                  bssIdx;
} tHalRcvFltPktMatchRspParams, *tptHalRcvFltPktMatchRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader  header;
   tHalRcvFltPktMatchRspParams fltPktMatchRspParams;
} tHalRcvFltPktMatchCntRspMsg, *tpHalRcvFltPktMatchCntRspMsg;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;  /* only valid for response message */
    tANI_U8    filterId;
    tANI_U8    bssIdx;
}tHalRcvFltPktClearParam, *tpHalRcvFltPktClearParam;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tHalRcvFltPktClearParam     filterClearParam;
} tHalRcvFltPktClearReqMsg, *tpHalRcvFltPktClearReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader              header;
   tHalRcvFltPktClearParam    filterClearParam;
} tHalRcvFltPktClearRspMsg, *tpHalRcvFltPktClearRspMsg;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;
    tANI_U8    bssIdx;
}tHalRcvFltPktSetMcListRspType, *tpHalRcvFltPktSetMcListRspType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader                    header;
    tHalRcvFltMcAddrListType         mcAddrList;
} tHalRcvFltPktSetMcListReqMsg, *tpHalRcvFltPktSetMcListReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader                    header;
   tHalRcvFltPktSetMcListRspType    rspParam;
} tHalRcvFltPktSetMcListRspMsg, *tpHalRcvFltPktSetMcListRspMsg;


/*---------------------------------------------------------------------------
 *******************Packet Filtering Definitions End*******************
 *--------------------------------------------------------------------------*/

/*
 * There are two versions of this message
 * Version 1         : Base version
 * Current version   : Base version + Max LI modulated DTIM
 */
typedef PACKED_PRE struct PACKED_POST
{
   /*  Ignore DTIM */
  tANI_U32 uIgnoreDTIM;

  /*DTIM Period*/
  tANI_U32 uDTIMPeriod;

  /* Listen Interval */
  tANI_U32 uListenInterval;

  /* Broadcast Multicast Filter  */
  tANI_U32 uBcastMcastFilter;

  /* Beacon Early Termination */
  tANI_U32 uEnableBET;

  /* Beacon Early Termination Interval */
  tANI_U32 uBETInterval;
}tSetPowerParamsVer1Type, *tpSetPowerParamsVer1Type;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tSetPowerParamsVer1Type     powerParams;
} tSetPowerParamsVer1ReqMsg, *tpSetPowerParamsVer1ReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
   /*  Ignore DTIM */
  tANI_U32 uIgnoreDTIM;

  /*DTIM Period*/
  tANI_U32 uDTIMPeriod;

  /* Listen Interval */
  tANI_U32 uListenInterval;

  /* Broadcast Multicast Filter  */
  tANI_U32 uBcastMcastFilter;

  /* Beacon Early Termination */
  tANI_U32 uEnableBET;

  /* Beacon Early Termination Interval */
  tANI_U32 uBETInterval;

  /* MAX LI for modulated DTIM */
  tANI_U32 uMaxLIModulatedDTIM;
}tSetPowerParamsType, *tpSetPowerParamsType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tSetPowerParamsType         powerParams;
} tSetPowerParamsReqMsg, *tpSetPowerParamsReqMsg;

typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;

   /*status of the request */
   tANI_U32   status;

}  tSetPowerParamsResp, *tpSetPowerParamsResp;

/*---------------------------------------------------------------------------
 ****************Capability bitmap exchange definitions and macros starts*************
 *--------------------------------------------------------------------------*/

typedef enum {
    MCC                    = 0,
    P2P                    = 1,
    DOT11AC                = 2,
    SLM_SESSIONIZATION     = 3,
    DOT11AC_OPMODE         = 4,
    SAP32STA               = 5,
    TDLS                   = 6,
    P2P_GO_NOA_DECOUPLE_INIT_SCAN = 7,
    WLANACTIVE_OFFLOAD     = 8,
    BEACON_OFFLOAD         = 9,
    SCAN_OFFLOAD           = 10,
    ROAM_OFFLOAD           = 11,
    BCN_MISS_OFFLOAD       = 12,
    STA_POWERSAVE          = 13,
    STA_ADVANCED_PWRSAVE   = 14,
    AP_UAPSD               = 15,
    AP_DFS                 = 16,
    BLOCKACK               = 17,
    PHY_ERR                = 18,
    BCN_FILTER             = 19,
    RTT                    = 20,
    RATECTRL               = 21,
    WOW                    = 22,
    WLAN_ROAM_SCAN_OFFLOAD = 23,
    SPECULATIVE_PS_POLL    = 24,
    SCAN_SCH               = 25,
    IBSS_HEARTBEAT_OFFLOAD = 26,
    WLAN_SCAN_OFFLOAD      = 27,
    WLAN_PERIODIC_TX_PTRN  = 28,
    ADVANCE_TDLS           = 29,
    BATCH_SCAN             = 30,
    FW_IN_TX_PATH          = 31,
    EXTENDED_NSOFFLOAD_SLOT = 32,
    CH_SWITCH_V1           = 33,
    HT40_OBSS_SCAN         = 34,
    UPDATE_CHANNEL_LIST    = 35,
    WLAN_MCADDR_FLT        = 36,
    WLAN_CH144             = 37,
    NAN                    = 38,
    TDLS_SCAN_COEXISTENCE  = 39,
    LINK_LAYER_STATS_MEAS  = 40,
    MU_MIMO                = 41,
    EXTENDED_SCAN          = 42,
    DYNAMIC_WMM_PS         = 43,
    MAC_SPOOFED_SCAN       = 44,
    BMU_ERROR_GENERIC_RECOVERY = 45,
    DISA                   = 46,
    FW_STATS               = 47,
    WPS_PRBRSP_TMPL        = 48,
    BCN_IE_FLT_DELTA       = 49,
    TDLS_OFF_CHANNEL       = 51,
    MGMT_FRAME_LOGGING     = 53,
    ENHANCED_TXBD_COMPLETION = 54,
    LOGGING_ENHANCEMENT    = 55,
    MAX_FEATURE_SUPPORTED  = 128,
} placeHolderInCapBitmap;

typedef PACKED_PRE struct PACKED_POST{

   tANI_U32 featCaps[4];
}  tWlanFeatCaps, *tpWlanFeatCaps;

typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;
   tWlanFeatCaps wlanFeatCaps;

}  tWlanFeatCapsMsg, *tpWlanFeatCapsMsg;

#define IS_MCC_SUPPORTED_BY_HOST (!!(halMsg_GetHostWlanFeatCaps(MCC)))
#define IS_SLM_SESSIONIZATION_SUPPORTED_BY_HOST (!!(halMsg_GetHostWlanFeatCaps(SLM_SESSIONIZATION)))
#define IS_FEATURE_SUPPORTED_BY_HOST(featEnumValue) (!!halMsg_GetHostWlanFeatCaps(featEnumValue))
#define IS_WLANACTIVE_OFFLOAD_SUPPORTED_BY_HOST (!!(halMsg_GetHostWlanFeatCaps(WLANACTIVE_OFFLOAD)))
#define IS_WLAN_ROAM_SCAN_OFFLOAD_SUPPORTED_BY_HOST (!!(halMsg_GetHostWlanFeatCaps(WLAN_ROAM_SCAN_OFFLOAD)))
#define IS_IBSS_HEARTBEAT_OFFLOAD_SUPPORTED_BY_HOST (!!(halMsg_GetHostWlanFeatCaps(IBSS_HEARTBEAT_OFFLOAD)))
#define IS_SCAN_OFFLOAD_SUPPORTED_BY_HOST (!!(halMsg_GetHostWlanFeatCaps(WLAN_SCAN_OFFLOAD)))
#define IS_CH_SWITCH_V1_SUPPORTED_BY_HOST ((!!(halMsg_GetHostWlanFeatCaps(CH_SWITCH_V1))))
#define IS_TDLS_SCAN_COEXISTENCE_SUPPORTED_BY_HOST ((!!(halMsg_GetHostWlanFeatCaps(TDLS_SCAN_COEXISTENCE))))
#define IS_DYNAMIC_WMM_PS_SUPPORTED_BY_HOST ((!!(halMsg_GetHostWlanFeatCaps(DYNAMIC_WMM_PS))))
#define IS_MAC_SPOOF_SCAN_SUPPORTED_BY_HOST ((!!(halMsg_GetHostWlanFeatCaps(MAC_SPOOFED_SCAN))))
#define IS_NEW_BMU_ERROR_RECOVERY_SUPPORTED_BY_HOST ((!!(halMsg_GetHostWlanFeatCaps(BMU_ERROR_GENERIC_RECOVERY))))
#define IS_ENHANCED_TXBD_COMPLETION_SUPPORTED_BY_HOST ((!!(halMsg_GetHostWlanFeatCaps(ENHANCED_TXBD_COMPLETION))))

tANI_U8 halMsg_GetHostWlanFeatCaps(tANI_U8 feat_enum_value);

#define setFeatCaps(a,b)   {  tANI_U32 arr_index, bit_index; \
                              if ((b)<=127) { \
                                arr_index = (b)/32; \
                                bit_index = (b)%32; \
                                if(arr_index < 4) \
                                (a)->featCaps[arr_index] |= (1<<bit_index); \
                              } \
                           }
#define getFeatCaps(a,b,c) {  tANI_U32 arr_index, bit_index; \
                              if ((b)<=127) { \
                                arr_index = (b)/32; \
                                bit_index = (b)%32; \
                                (c) = ((a)->featCaps[arr_index] & (1<<bit_index))?1:0; \
                              } \
                           }
#define clearFeatCaps(a,b) {  tANI_U32 arr_index, bit_index; \
                              if ((b)<=127) { \
                                arr_index = (b)/32; \
                                bit_index = (b)%32; \
                                (a)->featCaps[arr_index] &= ~(1<<bit_index); \
                              } \
                           }

/*---------------------------------------------------------------------------
 * WLAN_HAL_WAKE_REASON_IND
 *--------------------------------------------------------------------------*/

/* status codes to help debug rekey failures */
typedef enum
{
    WLAN_HAL_GTK_REKEY_STATUS_SUCCESS            = 0,
    WLAN_HAL_GTK_REKEY_STATUS_NOT_HANDLED        = 1, /* rekey detected, but not handled */
    WLAN_HAL_GTK_REKEY_STATUS_MIC_ERROR          = 2, /* MIC check error on M1 */
    WLAN_HAL_GTK_REKEY_STATUS_DECRYPT_ERROR      = 3, /* decryption error on M1  */
    WLAN_HAL_GTK_REKEY_STATUS_REPLAY_ERROR       = 4, /* M1 replay detected */
    WLAN_HAL_GTK_REKEY_STATUS_MISSING_KDE        = 5, /* missing GTK key descriptor in M1 */
    WLAN_HAL_GTK_REKEY_STATUS_MISSING_IGTK_KDE   = 6, /* missing iGTK key descriptor in M1 */
    WLAN_HAL_GTK_REKEY_STATUS_INSTALL_ERROR      = 7, /* key installation error */
    WLAN_HAL_GTK_REKEY_STATUS_IGTK_INSTALL_ERROR = 8, /* iGTK key installation error */
    WLAN_HAL_GTK_REKEY_STATUS_RESP_TX_ERROR      = 9, /* GTK rekey M2 response TX error */

    WLAN_HAL_GTK_REKEY_STATUS_GEN_ERROR          = 255 /* non-specific general error */
} tGTKRekeyStatus;

/* wake reason types */
typedef enum
{
    WLAN_HAL_WAKE_REASON_NONE             = 0,
    WLAN_HAL_WAKE_REASON_MAGIC_PACKET     = 1,  /* magic packet match */
    WLAN_HAL_WAKE_REASON_PATTERN_MATCH    = 2,  /* host defined pattern match */
    WLAN_HAL_WAKE_REASON_EAPID_PACKET     = 3,  /* EAP-ID frame detected */
    WLAN_HAL_WAKE_REASON_EAPOL4WAY_PACKET = 4,  /* start of EAPOL 4-way handshake detected */
    WLAN_HAL_WAKE_REASON_NETSCAN_OFFL_MATCH = 5, /* network scan offload match */
    WLAN_HAL_WAKE_REASON_GTK_REKEY_STATUS = 6,  /* GTK rekey status wakeup (see status) */
    WLAN_HAL_WAKE_REASON_BSS_CONN_LOST    = 7,  /* BSS connection lost */
} tWakeReasonType;

/*
  Wake Packet which is saved at tWakeReasonParams.DataStart
  This data is sent for any wake reasons that involve a packet-based wakeup :

  WLAN_HAL_WAKE_REASON_TYPE_MAGIC_PACKET
  WLAN_HAL_WAKE_REASON_TYPE_PATTERN_MATCH
  WLAN_HAL_WAKE_REASON_TYPE_EAPID_PACKET
  WLAN_HAL_WAKE_REASON_TYPE_EAPOL4WAY_PACKET
  WLAN_HAL_WAKE_REASON_TYPE_GTK_REKEY_STATUS

  The information is provided to the host for auditing and debug purposes

*/

/*
  Wake reason indication parameters
*/
typedef PACKED_PRE struct PACKED_POST
{
    uint32  ulReason;        /* see tWakeReasonType */
    uint32  ulReasonArg;     /* argument specific to the reason type */
    uint32  ulStoredDataLen; /* length of optional data stored in this message, in case
                              HAL truncates the data (i.e. data packets) this length
                              will be less than the actual length */
    uint32  ulActualDataLen; /* actual length of data */
    uint8   aDataStart[1];   /* variable length start of data (length == storedDataLen)
                             see specific wake type */
} tWakeReasonParams, *tpWakeReasonParams;

/*
  Wake reason indication
*/
typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader       header;
    tWakeReasonParams   wakeReasonParams;
    tANI_U32            uBssIdx : 8;
    tANI_U32            bReserved : 24;
} tHalWakeReasonInd, *tpHalWakeReasonInd;

/*---------------------------------------------------------------------------
* WLAN_HAL_GTK_OFFLOAD_REQ
*--------------------------------------------------------------------------*/

#define HAL_GTK_KEK_BYTES 16
#define HAL_GTK_KCK_BYTES 16

#define WLAN_HAL_GTK_OFFLOAD_FLAGS_DISABLE (1 << 0)

#define GTK_SET_BSS_KEY_TAG  0x1234AA55

typedef PACKED_PRE struct PACKED_POST
{
  tANI_U32     ulFlags;             /* optional flags */
  tANI_U8      aKCK[HAL_GTK_KCK_BYTES];  /* Key confirmation key */
  tANI_U8      aKEK[HAL_GTK_KEK_BYTES];  /* key encryption key */
  tANI_U64     ullKeyReplayCounter; /* replay counter */
  tANI_U8      bssIdx;
} tHalGtkOffloadReqParams, *tpHalGtkOffloadReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalGtkOffloadReqParams gtkOffloadReqParams;
}  tHalGtkOffloadReqMsg, *tpHalGtkOffloadReqMsg;

/*---------------------------------------------------------------------------
* WLAN_HAL_GTK_OFFLOAD_RSP
*--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   ulStatus;   /* success or failure */
    tANI_U8    bssIdx;
} tHalGtkOffloadRspParams, *tpHalGtkOffloadRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalGtkOffloadRspParams gtkOffloadRspParams;
}  tHalGtkOffloadRspMsg, *tpHalGtkOffloadRspMsg;


/*---------------------------------------------------------------------------
* WLAN_HAL_GTK_OFFLOAD_GETINFO_REQ
*--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8    bssIdx;

} tHalGtkOffloadGetInfoReqParams, *tptHalGtkOffloadGetInfoReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalGtkOffloadGetInfoReqParams gtkOffloadGetInfoReqParams;
}  tHalGtkOffloadGetInfoReqMsg, *tpHalGtkOffloadGetInfoReqMsg;

/*---------------------------------------------------------------------------
* WLAN_HAL_GTK_OFFLOAD_GETINFO_RSP
*--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32   ulStatus;             /* success or failure */
   tANI_U32   ulLastRekeyStatus;    /* last rekey status when the rekey was offloaded */
   tANI_U64   ullKeyReplayCounter;  /* current replay counter value */
   tANI_U32   ulTotalRekeyCount;    /* total rekey attempts */
   tANI_U32   ulGTKRekeyCount;      /* successful GTK rekeys */
   tANI_U32   ulIGTKRekeyCount;     /* successful iGTK rekeys */
   tANI_U8    bssIdx;
} tHalGtkOffloadGetInfoRspParams, *tptHalGtkOffloadGetInfoRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalGtkOffloadGetInfoRspParams gtkOffloadGetInfoRspParams;
}  tHalGtkOffloadGetInfoRspMsg, *tpHalGtkOffloadGetInfoRspMsg;

/*---------------------------------------------------------------------------
* WLAN_HAL_DHCP_IND
*--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   /*Indicates the device mode which indicates about the DHCP activity */
    tANI_U8 device_mode;
    tSirMacAddr macAddr;
} tDHCPInfo, *tpDHCPInfo;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader  header;
   tANI_U32       status;  /* success or failure */
} tDHCPIndStatus, *tpDHCPIndstatus;

/*
   Thermal Mitigation mode of operation.
   HAL_THERMAL_MITIGATION_MODE_0 - Based on AMPDU disabling aggregation
   HAL_THERMAL_MITIGATION_MODE_1 - Based on AMPDU disabling aggregation and
   reducing transmit power
   HAL_THERMAL_MITIGATION_MODE_2 - Not supported
*/
typedef enum
{
  HAL_THERMAL_MITIGATION_MODE_INVALID = -1,
  HAL_THERMAL_MITIGATION_MODE_0,
  HAL_THERMAL_MITIGATION_MODE_1,
  HAL_THERMAL_MITIGATION_MODE_2,
  HAL_THERMAL_MITIGATION_MODE_MAX = WLAN_HAL_MAX_ENUM_SIZE,
}tHalThermalMitigationModeType;
//typedef tANI_S16 tHalThermalMitigationModeType;

/*
   Thermal Mitigation level.
   Note the levels are incremental i.e HAL_THERMAL_MITIGATION_LEVEL_2 =
   HAL_THERMAL_MITIGATION_LEVEL_0 + HAL_THERMAL_MITIGATION_LEVEL_1

   HAL_THERMAL_MITIGATION_LEVEL_0 - lowest level of thermal mitigation. This
   level indicates normal mode of operation
   HAL_THERMAL_MITIGATION_LEVEL_1 - 1st level of thermal mitigation
   HAL_THERMAL_MITIGATION_LEVEL_2 - 2nd level of thermal mitigation
   HAL_THERMAL_MITIGATION_LEVEL_3 - 3rd level of thermal mitigation
   HAL_THERMAL_MITIGATION_LEVEL_4 - 4th level of thermal mitigation
*/
typedef enum
{
  HAL_THERMAL_MITIGATION_LEVEL_INVALID = -1,
  HAL_THERMAL_MITIGATION_LEVEL_0,
  HAL_THERMAL_MITIGATION_LEVEL_1,
  HAL_THERMAL_MITIGATION_LEVEL_2,
  HAL_THERMAL_MITIGATION_LEVEL_3,
  HAL_THERMAL_MITIGATION_LEVEL_4,
  HAL_THERMAL_MITIGATION_LEVEL_MAX = WLAN_HAL_MAX_ENUM_SIZE,
}tHalThermalMitigationLevelType;
//typedef tANI_S16 tHalThermalMitigationLevelType;

typedef PACKED_PRE struct PACKED_POST
{
   /* Thermal Mitigation Operation Mode */
   tHalThermalMitigationModeType thermalMitMode;

   /* Thermal Mitigation Level */
   tHalThermalMitigationLevelType thermalMitLevel;

}tSetThermalMitgationType, *tpSetThermalMitgationType;

/* WLAN_HAL_SET_THERMAL_MITIGATION_REQ */
typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tSetThermalMitgationType    thermalMitParams;
} tSetThermalMitigationReqMsg, *tpSetThermalMitigationReqMsg;

typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;

   /*status of the request */
   tANI_U32   status;

}  tSetThermalMitigationResp, *tpSetThermalMitigationResp;

/* Per STA Class B Statistics. Class B statistics are STA TX/RX stats
provided to FW from Host via periodic messages */
typedef PACKED_PRE struct PACKED_POST {
   /* TX stats */
   uint32 txBytesPushed;
   uint32 txPacketsPushed;

   /* RX stats */
   uint32 rxBytesRcvd;
   uint32 rxPacketsRcvd;
   uint32 rxTimeTotal;
} tStaStatsClassB, *tpStaStatsClassB;

typedef PACKED_PRE struct PACKED_POST {

   /* Duration over which this stats was collected */
   tANI_U32 duration;

   /* Per STA Stats */
   tStaStatsClassB staStatsClassB[HAL_NUM_STA];
} tStatsClassBIndParams, *tpStatsClassBIndParams;

typedef PACKED_PRE struct PACKED_POST {

   tHalMsgHeader header;

   /* Class B Stats */
   tStatsClassBIndParams statsClassBIndParams;
} tStatsClassBInd, *tpStatsClassBInd;

/*Wifi Proximity paramters in AP mode*/
#ifdef FEATURE_WIFI_PROXIMITY

typedef PACKED_PRE struct PACKED_POST{

   tANI_U8  wifiProximityChannel;
   tANI_U32 wifiProximityDuration;
   tANI_U32 wifiProximityInterval;
   tANI_U32 wifiProximityMode;
   tANI_U32 wifiProximityStatus;
   tSirMacAddr bssId;
   tSirMacSSid ssId;

} tSetWifiProximityReqParam, *tpSetWifiProximityReqParam;

typedef PACKED_PRE struct PACKED_POST
{
  tHalMsgHeader header;

  tSetWifiProximityReqParam wifiProximityReqParams;

}tSetWifiProximityReqMsg, *tpSetWifiProximityReqMsg;

/*WLAN_HAL_WIFI_PROXIMITY_RSP*/
typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;

   /*status of the request */
   tANI_U32   status;

}  tSetWifiProximityRspMsg, *tpSetWifiProxmityRspMsg;

#endif

#ifdef FEATURE_SPECULATIVE_PS_POLL
/*---------------------------------------------------------------------------
 * WLAN_HAL_START_SPECULATIVE_PS_POLLS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8         bssIdx;
   tANI_U16 serviceInterval;
   tANI_U16 suspendInterval;
   tANI_U8 acMask;
} tHalStartSpecPsPollReqParams, *tpHalStartSpecPsPollReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalStartSpecPsPollReqParams specPsPollReq;
}  tHalStartSpecPsPollReqMsg, *tpHalStartSpecPsPollReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_START_SPECULATIVE_PS_POLLS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
    tANI_U8    bssIdx;
} tHalStartSpecPsPollRspParams, *tpHalStartSpecPsPollRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalStartSpecPsPollRspParams startSpecPsPollRspParams;
}  tHalStartSpecPsPollRspMsg, *tpHalStartSpecPsPollRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_STOP_SPECULATIVE_PS_POLLS_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tANI_U8     bssIdx;
}  tHalStopSpecPsPollsIndMsg, *tpHalStopSpecPsPollsIndMsg;
#endif

#ifdef FEATURE_WLAN_TDLS
#define HAL_MAX_SUPP_CHANNELS 128
#define HAL_MAX_SUPP_OPER_CLASSES 32
/*---------------------------------------------------------------------------
 * WLAN_HAL_TDLS_LINK_ESTABLISHED_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /*STA Index*/
    tANI_U16        staIdx;

    /* if this is 1, self is initiator and peer is reponder */
    tANI_U8                bIsResponder;

    /* QoS Info */
    tANI_U8        acVOUAPSDFlag:1;
    tANI_U8        acVIUAPSDFlag:1;
    tANI_U8        acBKUAPSDFlag:1;
    tANI_U8        acBEUAPSDFlag:1;
    tANI_U8        aAck:1;
    tANI_U8        maxServicePeriodLength:2;
    tANI_U8        moreDataAck:1;

    /*TDLS Peer U-APSD Buffer STA Support*/
    tANI_U8        TPUBufferStaSupport;

    /*TDLS off channel related params */
    tANI_U8        tdlsOffChannelSupport;
    tANI_U8        peerCurrOperClass;
    tANI_U8        selfCurrOperClass;
    tANI_U8        validChannelsLen;
    tANI_U8        validChannels[HAL_MAX_SUPP_CHANNELS];
    tANI_U8        validOperClassesLen;
    tANI_U8        validOperClasses[HAL_MAX_SUPP_OPER_CLASSES];
}tTDLSLinkEstablishedType, *tpTDLSLinkEstablishedType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tTDLSLinkEstablishedType    tdlsLinkEstablishedParams;
} tTDLSLinkEstablishedReqMsg, *tpTDLSLinkEstablishedReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_TDLS_LINK_ESTABLISHED_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;

    /*STA Index*/
    tANI_U16        staIdx;
} tTDLSLinkEstablishedResp, *tpTDLSLinkEstablishedResp;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTDLSLinkEstablishedResp TDLSLinkEstablishedRespParams;
}  tTDLSLinkEstablishedRespMsg,  *tpTDLSLinkEstablishedRespMsg;
/*---------------------------------------------------------------------------
   + * WLAN_HAL_TDLS_CHAN_SWITCH_REQ
   + *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /*STA Index*/
    tANI_U16       staIdx;
    /* if this is 1, self is initiator otherwise responder only*/
    tANI_U8        isOffchannelInitiator;
    /*TDLS off channel related params */
    tANI_U8        targetOperClass;
    tANI_U8        targetChannel;
    tANI_U8        secondaryChannelOffset;
    tANI_U8        reserved[32];
}tTDLSChanSwitchReqType, *tpTDLSChanSwitchReqType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader             header;
    tTDLSChanSwitchReqType    tdlsChanSwitchParams;
} tTDLSChanSwitchReqMsg, *tpTDLSChanSwitchReqMsg;
/*---------------------------------------------------------------------------
 * WLAN_HAL_TDLS_CHAN_SWITCH_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 status;
        /*STA Index*/
    tANI_U16 staIdx;
} tTDLSChanSwitchResp, *tpTDLSChanSwitchResp;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTDLSChanSwitchResp tdlsChanSwitchRespParams;
} tTDLSChanSwitchRespMsg, *tpTDLSChanSwitchRespMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_TDLS_LINK_TEARDOWN_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /*STA Index*/
    tANI_U16        staIdx;
}tTDLSLinkTeardownType, *tpTDLSLinkTeardownType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tTDLSLinkTeardownType    tdlsLinkTeardownParams;
} tTDLSLinkTeardownReqMsg, *tpTDLSLinkTeardownReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_TDLS_LINK_TEARDOWN_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;

    /*STA Index*/
    tANI_U16        staIdx;
} tTDLSLinkTeardownResp, *tpTDLSLinkTeardownResp;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTDLSLinkTeardownResp TDLSLinkTeardownRespParams;
}  tTDLSLinkTeardownRespMsg,  *tpTDLSLinkTeardownRespMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_TDLS_IND
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U16    assocId;
    tANI_U16    staIdx;
    tANI_U16    status;
    tANI_U16    reasonCode;
}tTdlsIndParams, *tpTdlsIndParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTdlsIndParams tdlsIndParams;
}tTdlsIndMsg, *tpTdlsIndMsg;

#endif

/*---------------------------------------------------------------------------
 *WLAN_HAL_IBSS_PEER_INACTIVITY_IND
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8     bssIdx;
    tANI_U8     staIdx;
    tSirMacAddr staAddr;
}tIbssPeerInactivityIndParams, *tpIbssPeerInactivityIndParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tIbssPeerInactivityIndParams ibssPeerInactivityIndParams;
}tIbssPeerInactivityIndMsg, *tpIbssPeerInactivityIndMsg;


/*********** Scan Offload Related Structures *************/
#define HAL_NUM_SCAN_SSID           10
#define HAL_NUM_SCAN_BSSID          4

/*
 * Enumetation to indicate scan type (active/passive)
 */
typedef enum
{
    eSIR_PASSIVE_SCAN,
    eSIR_ACTIVE_SCAN = WLAN_HAL_MAX_ENUM_SIZE,
} tSirScanType;

typedef PACKED_PRE struct PACKED_POST
{
  tANI_U8      numBssid;
  tSirMacAddr  bssid[HAL_NUM_SCAN_BSSID];
  tANI_U8      numSsid;
  tSirMacSSid  ssid[HAL_NUM_SCAN_SSID];
  tANI_BOOLEAN hiddenSsid;
  tSirMacAddr  selfMacAddr;
  tSirBssType  bssType;
  tSirScanType scanType;
  tANI_U32     minChannelTime;
  tANI_U32     maxChannelTime;
  tANI_BOOLEAN p2pSearch;
  tANI_U8      channelCount;
  tANI_U8      channels[WLAN_HAL_ROAM_SCAN_MAX_CHANNELS];
  tANI_U16     ieFieldLen;
  tANI_U8      ieField[1];
}tScanOffloadReqType, *tpScanOffloadReqType;

/*---------------------------------------------------------------------------
 * WLAN_HAL_START_SCAN_OFFLOAD_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tScanOffloadReqType scanOffloadParams;
}  tHalStartScanOffloadReqMsg, *tpHalStartScanOffloadReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_START_SCAN_OFFLOAD_RSP
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;

  /*status of the request - just to indicate SO has acknowledged
   *                *      the request and will start scanning*/
   tANI_U32   status;
}  tHalStartScanOffloadRspMsg, *tpHalStartScanOffloadRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_STOP_SCAN_OFFLOAD_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalStopScanOffloadReqMsg, *tpHalStopScanOffloadReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_STOP_SCAN_OFFLOAD_RSP
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;

  /*status of the request - just to indicate SO has acknowledged
    the request and will start scanning*/
   tANI_U32   status;
}  tHalStopScanOffloadRspMsg, *tpHalStopScanOffloadRspMsg;

/*
 * Enumetation of scan events indicated by firmware to the host
 */
typedef enum
{
    WLAN_HAL_SCAN_EVENT_STARTED = 0x1,        /* Scan command accepted by FW */
    WLAN_HAL_SCAN_EVENT_COMPLETED = 0x2,      /* Scan has been completed by FW */
    WLAN_HAL_SCAN_EVENT_BSS_CHANNEL = 0x4,    /* FW is going to move to HOME channel */
    WLAN_HAL_SCAN_EVENT_FOREIGN_CHANNEL = 0x8,/* FW is going to move to FORIEGN channel */
    WLAN_HAL_SCAN_EVENT_DEQUEUED = 0x10,      /* scan request got dequeued */
    WLAN_HAL_SCAN_EVENT_PREEMPTED = 0x20,     /* preempted by other high priority scan */
    WLAN_HAL_SCAN_EVENT_START_FAILED = 0x40,  /* scan start failed */
    WLAN_HAL_SCAN_EVENT_RESTARTED = 0x80,     /*scan restarted*/
    WLAN_HAL_SCAN_EVENT_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tScanEventType;

typedef PACKED_PRE struct PACKED_POST
{
    tScanEventType event;
    tANI_U32 channel;
    tANI_U32 scanId;
} tScanOffloadEventInfo;

/*---------------------------------------------------------------------------
 * WLAN_HAL_OFFLOAD_SCAN_EVENT_IND
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tScanOffloadEventInfo scanOffloadInd;
}  tHalScanOffloadIndMsg, *tpHalScanOffloadIndMsg;

typedef PACKED_PRE struct PACKED_POST {
    /** primary 20 MHz channel frequency in mhz */
    tANI_U32 mhz;
    /** Center frequency 1 in MHz*/
    tANI_U32 band_center_freq1;
    /** Center frequency 2 in MHz - valid only for 11acvht 80plus80 mode*/
    tANI_U32 band_center_freq2;
    /* The first 26 bits are a bit mask to indicate any channel flags,
       (see WLAN_HAL_CHAN_FLAG*)
       The last 6 bits indicate the mode (see tChannelPhyModeType)*/
    tANI_U32 channel_info;
    /** contains min power, max power, reg power and reg class id. */
    tANI_U32 reg_info_1;
    /** contains antennamax */
    tANI_U32 reg_info_2;
} tUpdateChannelParam;


typedef enum {
    WLAN_HAL_MODE_11A            = 0,   /* 11a Mode */
    WLAN_HAL_MODE_11G            = 1,   /* 11b/g Mode */
    WLAN_HAL_MODE_11B            = 2,   /* 11b Mode */
    WLAN_HAL_MODE_11GONLY        = 3,   /* 11g only Mode */
    WLAN_HAL_MODE_11NA_HT20      = 4,  /* 11a HT20 mode */
    WLAN_HAL_MODE_11NG_HT20      = 5,  /* 11g HT20 mode */
    WLAN_HAL_MODE_11NA_HT40      = 6,  /* 11a HT40 mode */
    WLAN_HAL_MODE_11NG_HT40      = 7,  /* 11g HT40 mode */
    WLAN_HAL_MODE_11AC_VHT20     = 8,
    WLAN_HAL_MODE_11AC_VHT40     = 9,
    WLAN_HAL_MODE_11AC_VHT80     = 10,
    WLAN_HAL_MODE_11AC_VHT20_2G  = 11,
    WLAN_HAL_MODE_11AC_VHT40_2G  = 12,
    WLAN_HAL_MODE_11AC_VHT80_2G  = 13,
    WLAN_HAL_MODE_UNKNOWN        = 14,

} tChannelPhyModeType;

#define WLAN_HAL_CHAN_FLAG_HT40_PLUS   6
#define WLAN_HAL_CHAN_FLAG_PASSIVE     7
#define WLAN_HAL_CHAN_ADHOC_ALLOWED    8
#define WLAN_HAL_CHAN_AP_DISABLED      9
#define WLAN_HAL_CHAN_FLAG_DFS         10
#define WLAN_HAL_CHAN_FLAG_ALLOW_HT    11  /* HT is allowed on this channel */
#define WLAN_HAL_CHAN_FLAG_ALLOW_VHT   12  /* VHT is allowed on this channel */
#define WLAN_HAL_CHAN_CHANGE_CAUSE_CSA 13  /* Indicate reason for channel switch */

#define WLAN_HAL_SET_CHANNEL_FLAG(pwlan_hal_update_channel,flag) do { \
        (pwlan_hal_update_channel)->info |=  (1 << flag);      \
     } while(0)

#define WLAN_HAL_GET_CHANNEL_FLAG(pwlan_hal_update_channel,flag)   \
        (((pwlan_hal_update_channel)->info & (1 << flag)) >> flag)

#define WLAN_HAL_SET_CHANNEL_MIN_POWER(pwlan_hal_update_channel,val) do { \
     (pwlan_hal_update_channel)->reg_info_1 &= 0xffffff00;           \
     (pwlan_hal_update_channel)->reg_info_1 |= (val&0xff);           \
     } while(0)
#define WLAN_HAL_GET_CHANNEL_MIN_POWER(pwlan_hal_update_channel) ((pwlan_hal_update_channel)->reg_info_1 & 0xff )

#define WLAN_HAL_SET_CHANNEL_MAX_POWER(pwlan_hal_update_channel,val) do { \
     (pwlan_hal_update_channel)->reg_info_1 &= 0xffff00ff;           \
     (pwlan_hal_update_channel)->reg_info_1 |= ((val&0xff) << 8);    \
     } while(0)
#define WLAN_HAL_GET_CHANNEL_MAX_POWER(pwlan_hal_update_channel) ( (((pwlan_hal_update_channel)->reg_info_1) >> 8) & 0xff )

#define WLAN_HAL_SET_CHANNEL_REG_POWER(pwlan_hal_update_channel,val) do { \
     (pwlan_hal_update_channel)->reg_info_1 &= 0xff00ffff;           \
     (pwlan_hal_update_channel)->reg_info_1 |= ((val&0xff) << 16);   \
     } while(0)
#define WLAN_HAL_GET_CHANNEL_REG_POWER(pwlan_hal_update_channel) ( (((pwlan_hal_update_channel)->reg_info_1) >> 16) & 0xff )
#define WLAN_HAL_SET_CHANNEL_REG_CLASSID(pwlan_hal_update_channel,val) do { \
     (pwlan_hal_update_channel)->reg_info_1 &= 0x00ffffff;             \
     (pwlan_hal_update_channel)->reg_info_1 |= ((val&0xff) << 24);     \
     } while(0)
#define WLAN_HAL_GET_CHANNEL_REG_CLASSID(pwlan_hal_update_channel) ( (((pwlan_hal_update_channel)->reg_info_1) >> 24) & 0xff )

#define WLAN_HAL_SET_CHANNEL_ANTENNA_MAX(pwlan_hal_update_channel,val) do { \
     (pwlan_hal_update_channel)->reg_info_2 &= 0xffffff00;             \
     (pwlan_hal_update_channel)->reg_info_2 |= (val&0xff);             \
     } while(0)
#define WLAN_HAL_GET_CHANNEL_ANTENNA_MAX(pwlan_hal_update_channel) ((pwlan_hal_update_channel)->reg_info_2 & 0xff )

#define WLAN_HAL_SET_CHANNEL_MAX_TX_POWER(pwlan_hal_update_channel,val) do { \
     (pwlan_hal_update_channel)->reg_info_2 &= 0xffff00ff;              \
     (pwlan_hal_update_channel)->reg_info_2 |= ((val&0xff)<<8);         \
     } while(0)
#define WLAN_HAL_GET_CHANNEL_MAX_TX_POWER(pwlan_hal_update_channel) ( (((pwlan_hal_update_channel)->reg_info_2)>>8) & 0xff )

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8 numChan;
    tUpdateChannelParam chanParam[WLAN_HAL_ROAM_SCAN_MAX_CHANNELS];
} tUpdateChannelReqType;

/*---------------------------------------------------------------------------
 * WLAN_HAL_UPDATE_CHANNEL_LIST_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tUpdateChannelReqType updateChannelParams;
}  tHalUpdateChannelReqMsg, *tpHalUpdateChannelReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_UPDATE_CHANNEL_LIST_RSP
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;

  /*status of the request - just to indicate SO has acknowledged
   *                *      the request and will start scanning*/
   tANI_U32   status;
}  tHalUpdateChannelRspMsg, *tpHalUpdateChannelRspMsg;


/*---------------------------------------------------------------------------
* WLAN_HAL_TX_FAIL_IND
*--------------------------------------------------------------------------*/
// Northbound indication from FW to host on weak link detection
typedef PACKED_PRE struct PACKED_POST
{
   // Sequence number increases by 1 whenever the device driver
   // sends a notification event. This is cleared as 0 when the
   // JOIN IBSS commamd is issued
    tANI_U16 seqNo;
    tANI_U16 staId;
    tANI_U8 macAddr[HAL_MAC_ADDR_LEN];
} tHalTXFailIndParams, *tpHalTXFailIndParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalTXFailIndParams txFailIndParams;
}  tHalTXFailIndMsg, *tpHalTXFailIndMsg;

/*---------------------------------------------------------------------------
* WLAN_HAL_TX_FAIL_MONITOR_IND
*--------------------------------------------------------------------------*/
// Southbound message from Host to monitor the Tx failures
typedef PACKED_PRE struct PACKED_POST
{
    // tx_fail_count = 0 should disable the TX Fail monitor, non-zero value should enable it.
    tANI_U8 tx_fail_count;
} tTXFailMonitorInfo, *tpTXFailMonitorInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader  header;
    tTXFailMonitorInfo txFailMonitor;
} tTXFailMonitorInd, *tpTXFailMonitorInd;

/*---------------------------------------------------------------------------
* WLAN_HAL_IP_FORWARD_TABLE_UPDATE_IND
*--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8 destIpv4Addr[HAL_IPV4_ADDR_LEN];
   tANI_U8 nextHopMacAddr[HAL_MAC_ADDR_LEN];
} tDestIpNextHopMacPair;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8 numEntries;
   tDestIpNextHopMacPair destIpMacPair[1];
} tWlanIpForwardTableUpdateIndParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tWlanIpForwardTableUpdateIndParam ipForwardTableParams;
} tWlanIpForwardTableUpdateInd;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ROAM_OFFLOAD_SYNCH_IND
 *-------------------------------------------------------------------------*/
typedef enum
{
    /* reassociation is done, but couldn't finish security handshake */
    WLAN_HAL_ROAM_AUTH_STATUS_CONNECTED = 1,

    /* roam has successfully completed by firmware */
    WLAN_HAL_ROAM_AUTH_STATUS_AUTHENTICATED = 2,

    /* UNKONW error */
    WLAN_HAL_ROAM_AUTH_STATUS_UNKONWN = WLAN_HAL_MAX_ENUM_SIZE
}tHalRoamOffloadRoamAuthStatus;

typedef enum
{
    WLAN_HAL_ROAM_TYPE_WPA_PSK,
    WLAN_HAL_ROAM_TYPE_WPA2_PSK,
    WLAN_HAL_ROAM_TYPE_OKC,
    WLAN_HAL_ROAM_TYPE_CCKM,
    WLAN_HAL_ROAM_TYPE_FT,
    WLAN_HAL_ROAM_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tHalRoamOffloadType;

typedef PACKED_PRE struct PACKED_POST
{
    /* Offset of beacon / probe resp in this structure. Offset from the starting of the message */
    tANI_U16 beaconProbeRespOffset;

    /* Length of beaon / probe resp. */
    tANI_U16 beaconProbeRespLength;

    /* Offset of reassoc resp in this structure. Offset from the starting of the message */
    tANI_U16 reassocRespOffset;

    /* Length of reassoc resp. */
    tANI_U16 reassocRespLength;

    /* 0 for probe response frame, 1 for beacon frame,  */
    tANI_U8     isBeacon;

    /* staIdx of old AP */
    tANI_U8     oldStaIdx;

    /* note : from bssIdx field to txMgmtPower are exactly mapped to
       tConfigBssRspParams */
    /* bssIdx of new roamed AP */
    tANI_U8     bssIdx;

    /* DPU descriptor index for PTK */
    tANI_U8    dpuDescIndx;

    /* PTK DPU signature */
    tANI_U8    ucastDpuSignature;

    /* DPU descriptor index for GTK*/
    tANI_U8    bcastDpuDescIndx;

    /* GTK DPU signature */
    tANI_U8    bcastDpuSignature;

    /*DPU descriptor for IGTK*/
    tANI_U8    mgmtDpuDescIndx;

    /* IGTK DPU signature */
    tANI_U8    mgmtDpuSignature;

    /* Station Index for BSS entry*/
    tANI_U8    staIdx;

    /* Self station index for this BSS */
    tANI_U8    selfStaIdx;

    /* Bcast station for buffering bcast frames in AP role */
    tANI_U8    bcastStaIdx;

    /* MAC address of roamed AP */
    tSirMacAddr bssid;

    /*HAL fills in the tx power used for mgmt frames in this field. */
    tANI_S8     txMgmtPower;

    /* success or failure */
    tHalRoamOffloadRoamAuthStatus authStatus;

    /* TODO : add more info as needed */

    /* beaconProbeRespOffset points to starting of beacon/probe resp frame */
    /* Beacon or probe resp from new AP.  This is in 802.11
       frame format starting with MAC header. */
    /* Up to beaconProbeRespLength */

    /* reassocRespOffset points to starting of reassoc resp frame */
    /* Reassoc resp from new AP.  This is in 802.11
       frame format starting with MAC header. */
    /* Up to reassocRespLength */

} tHalRoamOffloadSynchIndParams, *tpHalRoamOffloadSynchIndParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalRoamOffloadSynchIndParams params;
} tHalRoamOffloadSynchInd, *tpHalRoamOffloadSynchInd;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ROAM_OFFLOAD_SYNCH_CNF
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* MAC address of new AP indicated by FW in RoamOffloadSynchInd */
    tSirMacAddr bssid;
} tHalRoamOffloadSynchCnfParams, *tpHalRoamOffloadSynchCnfParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalRoamOffloadSynchCnfParams params;
} tHalRoamOffloadSynchCnfMsg, *tpHalRoamOffloadSynchCnfMsg;


/*---------------------------------------------------------------------------
 WLAN_HAL_RATE_UPDATE_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* 0 implies UCAST RA, positive value implies fixed rate, -1 implies ignore this param */
    tANI_S32 ucastDataRate; //unit Mbpsx10

    /* TX flag to differentiate between HT20, HT40 etc */
    tTxRateInfoFlags ucastDataRateTxFlag;

    /* BSSID - Optional. 00-00-00-00-00-00 implies apply to all BCAST STAs */
    tSirMacAddr bssid;

    /* 0 implies MCAST RA, positive value implies fixed rate, -1 implies ignore */
    tANI_S32 reliableMcastDataRate; //unit Mbpsx10

    /* TX flag to differentiate between HT20, HT40 etc */
    tTxRateInfoFlags reliableMcastDataRateTxFlag;

    /* Default (non-reliable) MCAST(or BCAST)  fixed rate in 2.4 GHz, 0 implies ignore */
    tANI_U32 mcastDataRate24GHz; //unit Mbpsx10

    /* TX flag to differentiate between HT20, HT40 etc */
    tTxRateInfoFlags mcastDataRate24GHzTxFlag;

    /*  Default (non-reliable) MCAST(or BCAST) fixed rate in 5 GHz, 0 implies ignore */
    tANI_U32 mcastDataRate5GHz; //unit Mbpsx10

    /* TX flag to differentiate between HT20, HT40 etc */
    tTxRateInfoFlags mcastDataRate5GHzTxFlag;

} tHalRateUpdateParams, *tpHalRateUpdateParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalRateUpdateParams halRateUpdateParams;
}  tHalRateUpdateInd, * tpHalRateUpdateInd;

/*---------------------------------------------------------------------------
 * WLAN_HAL_AVOID_FREQ_RANGE_IND
 *-------------------------------------------------------------------------*/

#define WLAN_HAL_MAX_AVOID_FREQ_RANGE           15

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32     startFreq;
   tANI_U32     endFreq;
}  tHalFreqRange, *tpHalFreqRange;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32         avoidCnt;
   tHalFreqRange    avoidRange[WLAN_HAL_MAX_AVOID_FREQ_RANGE];
}  tHalAvoidFreqRangeIndParams, *tpHalAvoidFreqRangeIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalAvoidFreqRangeIndParams freqRangeIndParams;
}  tHalAvoidFreqRangeInd, *tpHalAvoidFreqRangeInd;

/*---------------------------------------------------------------------------
 * WLAN_HAL_START_HT40_OBSS_SCAN_IND
 *-------------------------------------------------------------------------*/

typedef enum
{
   WLAN_HAL_HT40_OBSS_SCAN_PARAM_START,
   WLAN_HAL_HT40_OBSS_SCAN_PARAM_UPDATE,
   WLAN_HAL_HT40_OBSS_SCAN_CMD_MAX = WLAN_HAL_MAX_ENUM_SIZE
}tHT40OBssScanCmdType;

typedef PACKED_PRE struct PACKED_POST
{
   tHT40OBssScanCmdType cmdType;

   tSirScanType scanType;
   tANI_U16 OBSSScanPassiveDwellTime; // In TUs
   tANI_U16 OBSSScanActiveDwellTime;  // In TUs
   tANI_U16 BSSChannelWidthTriggerScanInterval; // In seconds
   tANI_U16 OBSSScanPassiveTotalPerChannel; // In TUs
   tANI_U16 OBSSScanActiveTotalPerChannel;  // In TUs
   tANI_U16 BSSWidthChannelTransitionDelayFactor;
   tANI_U16 OBSSScanActivityThreshold;

   tANI_U8      selfStaIdx;
   tANI_U8      bssIdx;
   tANI_U8      fortyMHZIntolerent;
   tANI_U8      channelCount;
   tANI_U8      channels[WLAN_HAL_ROAM_SCAN_MAX_CHANNELS];
   tANI_U8      currentOperatingClass;

   tANI_U16     ieFieldLen;
   tANI_U8      ieField[WLAN_HAL_PNO_MAX_PROBE_SIZE];
}tHT40ObssScanIndType, *tpHT40ObssScanIndType;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHT40ObssScanIndType scanHT40ObssScanParams;
}  tHalStartHT40ObssScanIndMsg, *tpHalStartHT40ObssScanIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_STOP_HT40_OBSS_SCAN_IND
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tANI_U8       bssIdx;
}  tHalStopHT40OBSSScanIndMsg, *tpHalStopHT40OBSSScanIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_BCN_MISS_RATE_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /* Valid BSS Idx for beacon miss rate */
   tANI_U8    bssIdx;

}tHalBcnMissRateReqParams, *tpHalBcnMissRateReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader        header;
   tHalBcnMissRateReqParams   bcnMissRateReqParams;
} tHalBcnMissRateReqMsg, *tpHalBcnMissRateReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_BCN_MISS_RATE_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32           status;
   tANI_U32           bcnMissCnt;
}tHalBcnMissRateRspParams, *tpHalBcnMissRateRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalBcnMissRateRspParams bcnMissRateRspParams;
}tHalBcnMissRateRspMsg, *tpHalBcnMissRateRspMsg;

/*--------------------------------------------------------------------------
* WLAN_HAL_LL_SET_STATS_REQ
*---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32  req_id;
   tANI_U8   sta_id;
   tANI_U32  mpdu_size_threshold;             // threshold to classify the pkts as short or long
   tANI_U32  aggressive_statistics_gathering; // set for field debug mode. Driver should collect all statistics regardless of performance impact.
} tHalMacLlSetStatsReqParams, *tpHalMacLlSetStatsReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacLlSetStatsReqParams LlSetStatsReqParams;
}  tHalMacLlSetStatsReq, *tpHalMacLlSetStatsReq;

/*---------------------------------------------------------------------------
  WLAN_HAL_LL_SET_STATS_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  tANI_U32  status;
  tANI_U32  resp_id;
  tANI_U8   iface_id;
} tHalMacLlSetStatsRspParams, *tpHalMacLlSetStatsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacLlSetStatsRspParams LlSetStatsRspParams;
}  tHalMacLlSetStatsRsp, *tpHalMacLlSetStatsRsp;

/*---------------------------------------------------------------------------
  WLAN_HAL_LL_GET_STATS_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32  req_id;
   tANI_U8   sta_id;
   tANI_U32  param_id_mask;
}  tHalMacLlGetStatsReqParams, *tpHalMacLlGetStatsReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacLlGetStatsReqParams LlGetStatsReqParams;
}  tHalMacLlGetStatsReq, *tpHalMacLlGetStatsReq;

/*---------------------------------------------------------------------------
  WLAN_HAL_LL_GET_STATS_RSP
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32  status;
   tANI_U32  resp_id;
   tANI_U8   iface_id;
}  tHalMacLlGetStatsRspParams, *tpHalMacLlGetStatsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacLlGetStatsRspParams LlGetStatsRspParams;
}  tHalMacLlGetStatsRsp, *tpHalMacLlGetStatsRsp;

/*---------------------------------------------------------------------------
  WLAN_HAL_LL_CLEAR_STATS_REQ
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32  req_id;
   tANI_U8   sta_id;
   tANI_U32  stats_clear_req_mask;
   tANI_U8   stop_req;
} tHalMacLlClearStatsReqParams, *tpHalMacLlClearStatsReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacLlClearStatsReqParams LlClearStatsReqParams;
}  tHalMacLlClearStatsReq, *tpHalMacLlClearStatsReq;

/*---------------------------------------------------------------------------
  WLAN_HAL_LL_CLEAR_STATS_RSP
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 status;
   tANI_U8  sta_id;
   tANI_U32 resp_id;
   tANI_U32 stats_clear_rsp_mask;
   tANI_U8  stop_req_status;
}  tHalMacLlClearStatsRspParams, *tpHalMacLlClearStatsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacLlClearStatsRspParams LlClearStatsRspParams;
}  tHalMacLlClearStatsRsp, *tpHalMacLlClearStatsRsp;

/*---------------------------------------------------------------------------
  WLAN_HAL_LL_NOTIFY_STATS
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tANI_U32 param_id;
   tANI_U8  iface_id;
   tANI_U32 resp_id;
   tANI_U32 more_result_to_follow;
   tANI_U8  result[1];
}  tHalMacLlNotifyStats, *tpHalMacLlNotifyStats;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_START_REQ
 *-------------------------------------------------------------------------*/

typedef enum
{
   EXT_SCAN_CHANNEL_BAND_UNSPECIFIED  = 0x0000,
   EXT_SCAN_CHANNEL_BAND_BG           = 0x0001,    // 2.4 GHz
   EXT_SCAN_CHANNEL_BAND_A            = 0x0002,    // 5 GHz without DFS
   EXT_SCAN_CHANNEL_BAND_A_DFS        = 0x0004,    // 5 GHz DFS only
   EXT_SCAN_CHANNEL_BAND_A_WITH_DFS   = 0x0006,    // 5 GHz with DFS
   EXT_SCAN_CHANNEL_BAND_ABG          = 0x0003,    // 2.4 GHz + 5 GHz; no DFS
   EXT_SCAN_CHANNEL_BAND_ABG_WITH_DFS = 0x0007,    // 2.4 GHz + 5 GHz with DFS
   EXT_SCAN_CHANNEL_BAND_MAX          = WLAN_HAL_MAX_ENUM_SIZE
} tExtScanChannelBandMask;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 channel;       // frequency
    tANI_U32 dwellTimeMs;   // dwell time hint
    tANI_U8 passive;        // 0 => active,
                            // 1 => passive scan; ignored for DFS
}tExtScanChannelSpec, *tpExtScanChannelSpec;

typedef PACKED_PRE struct PACKED_POST
 {
   /* bucket index, 0 based */
   tANI_U8 bucketId;
   /* when equal to EXT_SCAN_CHANNEL_BAND_UNSPECIFIED, use channel list */
   tExtScanChannelBandMask channelBand;
   /* period (milliseconds) for each bucket defines the periodicity of bucket */
   tANI_U32 period;
   /* 0 => normal reporting (reporting rssi history only,
           when rssi history buffer is % full)
    * 1 => same as 0 + report a scan completion event after scanning this bucket
    * 2 => same as 1 + forward scan results (beacons/probe responses + IEs) in
           real time to HAL (Required for L = P0)
    * 3 => same as 2 + forward scan results (beacons/probe responses + IEs) in
           real time to host (Not required for L =  P3) */
   tANI_U8 reportEvents;
   /* number of channels */
   tANI_U8 numChannels;
   /* if channels to scan. In the TLV channelList[] */
   tExtScanChannelSpec channelList[WLAN_HAL_EXT_SCAN_MAX_CHANNELS];
}tExtScanBucketData, *tpExtScanBucketData;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
   tANI_U8  sessionId;
   /* Base period (milliseconds) used by scan buckets to define periodicity
      of the scans */
   tANI_U32 basePeriod;
   /* number of APs to store in each scan in the BSSID/RSSI history buffer
      (keep the most significant, i.e. stronger RSSI) */
   tANI_U32 maxApPerScan;
   /* in %, when buffer is this much full, wake up host */
   tANI_U32 reportThreshold;
   /* This will be off channel minimum time */
   tANI_U16 neighborScanChannelMinTime;
   /* This will be out off channel max time */
   tANI_U16 neighborScanChannelMaxTime;
   /* This will be the home (BSS) channel time */
   tANI_U16 homeAwayTime;
   /* number of buckets (maximum 8) */
   tANI_U8 numBuckets;
   /* Buckets data */
   tExtScanBucketData bucketData[WLAN_HAL_EXT_SCAN_MAX_BUCKETS];
} tHalExtScanStartReq, *tpHalExtScanStartReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanStartReq extScanStartReq;
}tHalExtScanStartReqMsg, *tpHalExtScanStartReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_START_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 requestId;
    tANI_U32 status;
}tHalExtScanStartRsp, *tpHalExtScanStartRsp;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanStartRsp extScanStartRsp;
}tHalExtScanStartRspMsg, *tpHalExtScanStartRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_GET_CAP_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32      requestId;
   tANI_U8       sessionId;
}tHalExtScanGetCapReq, *tpHalExtScanGetCapReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanGetCapReq extScanGetCapReq;
}tHalExtScanGetCapReqMsg, *tpHalExtScanGetCapReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_GET_CAP_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32      requestId;
   tANI_U32      status;

   tANI_U32      scanCacheSize;
   tANI_U32      scanBuckets;
   tANI_U32      maxApPerScan;
   tANI_U32      maxRssiSampleSize;
   tANI_U32      maxScanReportingThreshold;

   tANI_U32      maxHotlistAPs;
   tANI_U32      maxSignificantWifiChangeAPs;

   tANI_U32      maxBssidHistoryEntries;
}tHalExtScanGetCapRsp, *tpHalExtScanGetCapRsp;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanGetCapRsp extScanGetCapRsp;
}tHalExtScanGetCapRspMsg, *tpHalExtScanGetCapRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_GET_SCAN_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 requestId;
    tANI_U8 sessionId;
    /*
    * 1 return cached results and flush it
    * 0 return cached results and do not flush
    */
   tANI_BOOLEAN  flush;
}tHalExtScanGetScanReq, *tpHalExtScanGetScanReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanGetScanReq getScanReq;
}tHalExtScanGetScanReqMsg, *tpHalExtScanGetScanReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_GET_SCAN_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 requestId;
    tANI_U32 status;
}tHalExtScanGetScanRsp, *tpHalExtScanGetScanRsp;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanGetScanRsp getScanRsp;
}tHalExtScanGetScanRspMsg, *tpHalExtScanGetScanRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_RESULT_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U64 ts;                  // time of discovery
   tANI_U8 ssid[32+1];           // null terminated SSID
   tSirMacAddr bssid;            // BSSID
   tANI_U32 channel;             // channel frequency in MHz
   tANI_S32 rssi;                // RSSI in dBm
   tANI_U32 rtt;                 // RTT in nanoseconds - not expected
   tANI_U32 rttSd;               // standard deviation in rtt - not expected
   tANI_U16 beaconPeriod;        // period advertised in the beacon
   tANI_U16 capability;          // capabilities advertised in the beacon
} tHalExtScanResultParams, *tpHalExtScanResultParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tANI_U32 requestId;
   tANI_U32 scanResultSize;
   tANI_BOOLEAN moreData;
   tANI_U8 extScanResult[1];
}tHalExtScanResultIndMsg, *tpHalExtScanResultIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_STOP_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 requestId;
    tANI_U8 sessionId;
}tHalExtScanStopReq, *tpHalExtScanStopReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanStopReq extScanStopReq;
}tHalExtScanStopReqMsg, *tpHalExtScanStopReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_STOP_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 requestId;
    tANI_U32 status;
}tHalExtScanStopRsp, *tpHalExtScanStopRsp;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanStopRsp extScanStopRsp;
}tHalExtScanStopRspMsg, *tpHalExtScanStopRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_PROGRESS_IND
 *-------------------------------------------------------------------------*/

typedef enum
{
   WLAN_HAL_EXT_SCAN_BUFFER_FULL,
   WLAN_HAL_EXT_SCAN_COMPLETE,
   WLAN_HAL_EXT_SCAN_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tHalExtScanProgressEventType;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
   tANI_U32 status;
   tHalExtScanProgressEventType extScanEventType;
}tHalExtScanProgressInd, *tpHalExtScanProgressInd;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanProgressInd extScanProgressInd;
}tHalExtScanProgressIndMsg, *tpHalExtScanProgressIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXT_SCAN_RESULT_AVAILABLE_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
   tANI_U32 numOfScanResAvailable;
}tHalExtScanResAvailableInd, tpHalExtScanResAvailableInd;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExtScanResAvailableInd extScanResAvailableInd;
}tHalExtScanResAvailableIndMsg, *tpHalExtScanResAvailableIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SIG_RSSI_SET_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /* AP BSSID */
   tSirMacAddr  bssid;
   /* low threshold - used in L for significant_change - not used in L for
      hotlist*/
   tANI_S32 lowRssiThreshold;
   /* high threshold - used in L for significant rssi - used in L for hotlist */
   tANI_S32 highRssiThreshold;
   /* channel hint */
   tANI_U32 channel;
} tApThresholdParams, *tpApThresholdParams;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
   tANI_U8 sessionId;
   /* number of samples for averaging RSSI */
   tANI_U32 rssiSampleSize;
   /* number of missed samples to confirm AP loss */
   tANI_U32 lostApSampleSize;
   /* number of APs breaching threshold required for firmware to generate event */
   tANI_U32 minBreaching;
   /* number of significant APs */
   tANI_U32 numAp;
   /* significant APs */
   tApThresholdParams ap[WLAN_HAL_EXT_SCAN_MAX_SIG_CHANGE_APS];
} tHalSigRssiSetReq, *tpHalSigRssiSetReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalSigRssiSetReq extScanSigRssiReq;
}tHalSigRssiSetReqMsg, *tpHalSigRssiSetReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SIG_RSSI_SET_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
   tANI_U32 status;
}tHalSigRssiSetRsp, *tpHalSigRssiSetRsp;


typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalSigRssiSetRsp sigRssiSetRsp;
}tHalSigRssiSetRspMsg, *tpHalSigRssiSetRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SIG_RSSI_RESET_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
}tHalSigRssiResetReq, *tpHalSigRssiResetReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalSigRssiResetReq sigRssiResetReq;
}tHalSigRssiResetReqMsg, *tpHalSigRssiResetReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SIG_RSSI_RESET_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
   tANI_U32 status;
}tHalSigRssiResetRsp, *tpHalSigRssiResetRsp;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalSigRssiResetRsp sigRssiResetRsp;
}tHalSigRssiResetRspMsg, *tpHalSigRssiResetRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SIG_RSSI_RESULT_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   // BSSID
   tSirMacAddr bssid;
   // channel frequency in MHz
   tANI_U32 channel;
   // number of rssi samples
   tANI_U8 numRssi;
   // RSSI history in db
   tANI_S32 rssi[WLAN_HAL_EXT_SCAN_MAX_RSSI_SAMPLE_SIZE];
} tHalSigRssiResultParams, *tpHalSigRssiResultParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tANI_U32 requestId;
   tANI_U32 numSigRssiBss;
   tANI_BOOLEAN moreData;
   tANI_U8 sigRssiResult[1];
}tHalSigRssiResultIndMsg, *tpHalSigRssiResultIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_BSSID_HOTLIST_SET_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
   tANI_U8 sessionId;
   // number of hotlist APs
   tANI_U32 numAp;
   // hotlist APs
   tApThresholdParams ap[WLAN_HAL_EXT_SCAN_MAX_HOTLIST_APS];
} tHalBssidHotlistSetReq, *tpHalBssidHotlistSetReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalBssidHotlistSetReq bssidHotlistSetReq;
}tHalHotlistSetReqMsg, *tpHalHotlistSetReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_BSSID_HOTLIST_SET_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
   tANI_U32 status;
}tHalHotlistSetRsp, *tpHalHotlistSetRsp;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalHotlistSetRsp hotlistSetRsp;
}tHalHotlistSetRspMsg, *tpHalHotlistSetRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_BSSID_HOTLIST_RESET_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
}tHalHotlistResetReq, *tpHalHotlistResetReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalHotlistResetReq hotlistResetReq;
}tHalHotlistResetReqMsg, *tpHalHotlistResetReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_BSSID_HOTLIST_RESET_RSP
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 requestId;
   tANI_U32 status;
}tHalHotlistResetRsp, *tpHalHotlistResetRsp;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalHotlistResetRsp hotlistResetRsp;
}tHalHotlistResetRspMsg, *tpHalHotlistResetRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_BSSID_HOTLIST_RESULT_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tANI_U32 requestId;
   tANI_U32 numHotlistBss;
   tANI_BOOLEAN moreData;
   tANI_U8 bssHotlist[1];
}tHalHotlistResultIndMsg, *tpHalHotlistResultIndMsg;


/*---------------------------------------------------------------------------
  *WLAN_HAL_MAC_SPOOFED_SCAN_REQ
  *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8        macAddr[6];
    tANI_U32       reserved1;
    tANI_U32       reserved2;
}tMacSpoofedScanReqType, * tpMacSpoofedScanReqType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tMacSpoofedScanReqType tMacSpoofedScanReqParams;
} tMacSpoofedScanReqMsg, * tpMacSpoofedScanReqMsg;

/*---------------------------------------------------------------------------
* WLAN_HAL_MAC_SPOOFED_SCAN_RSP
*-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;
    tANI_U32   reserved1;
} tMacSpoofedScanResp, * tpMacSpoofedScanResp;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader         header;
    tMacSpoofedScanResp tMacSpoofedScanRespParams;
}  tMacSpoofedScanRespMsg,  * tpMacSpoofedScanRespMsg;
/*---------------------------------------------------------------------------
 *WLAN_HAL_GET_FRAME_LOG_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8 flags;
}tGetFrameLogReqType, * tpGetFrameLogReqType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tGetFrameLogReqType         tGetFrameLogReqParams;
} tGetFrameLogReqMsg, * tpGetFrameLogReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_FRAME_LOG_RSP
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;
} tGetFrameLogResp, * tpGetFrameLogResp;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader         header;
    tGetFrameLogResp      tGetFrameLogRespParams;
}  tGetFrameLogRespMsg,  * tpGetFrameLogRespMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_FATAL_EVENT_LOGGING_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 reasonCode;
}tHalFatalEventLoggingReqParams, *tpHalFatalEventLoggingReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalFatalEventLoggingReqParams tFatalEventLoggingReqParams;
}tHalFatalEventLoggingReqMsg, *tpHalFatalEventLoggingReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_FATAL_EVENT_LOGGING_RSP
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{   tANI_U32 status;
}tHalFatalEventLoggingRspParams, *tpHalFatalEventLoggingRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalFatalEventLoggingRspParams tFatalEventLoggingRspParams;
}tHalFatalEventLoggingRspMsg, *tpHalFatalEventLoggingRspMsg;

/*---------------------------------------------------------------------------
  * WLAN_HAL_LOST_LINK_PARAMETERS_IND
  *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8  bssIdx;
    tANI_U8  rssi;
    tSirMacAddr selfMacAddr;
    tANI_U32 linkFlCnt;
    tANI_U32 linkFlTx;
    tANI_U32 lastDataRate;
    tANI_U32 rsvd1;
    tANI_U32 rsvd2;
}tHalLostLinkParametersIndParams, *tpHalLostLinkParametersIndParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalLostLinkParametersIndParams lostLinkParameters;
}tHalLostLinkParametersIndMsg, *tpHalLostLinkParametersIndMsg;


/*---------------------------------------------------------------------------
 *WLAN_HAL_FW_LOGGING_INIT_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* BIT0 - enable frame logging
     * BIT1 - enableBMUHWtracing
     * BIT2 - enableQXDMlogging
     * BIT3 - enableUElogDpuTxp
     */
    tANI_U8 enableFlag;
    tANI_U8 frameType;
    tANI_U8 frameSize;
    tANI_U8 bufferMode;
    /* Host mem address to be used as logmailbox */
    tANI_U64 logMailBoxAddr;
    /* firmware will wakeup the host to send logs always */
    tANI_U8 continuousFrameLogging;
    /* Logging mail box version */
    tANI_U8 logMailBoxVer;
    /* Max ring size in firmware to log msgs when host is suspended state */
    tANI_U8 maxLogBuffSize;
    /* when firmware log reaches this threshold and
     * if host is awake it will push the logs.
     */
    tANI_U8 minLogBuffSize;
    /* Reserved for future purpose */
    tANI_U32 reserved0;
    tANI_U32 reserved1;
    tANI_U32 reserved2;
}tFWLoggingInitReqType, * tpFWLoggingInitReqType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader             header;
    tFWLoggingInitReqType     tFWLoggingInitReqParams;
} tHalFWLoggingInitReqMsg, * tpHalFWLoggingInitReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_FW_LOGGING_INIT_RSP
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 status;
    /* FW mail box address */
    tANI_U64 logMailBoxAddr;
    /* Logging mail box version */
    tANI_U8 logMailBoxVer;
    /* Qshrink is enabled */
    tANI_BOOLEAN logCompressEnabled;
    /* Reserved for future purpose */
    tANI_U32 reserved0;
    tANI_U32 reserved1;
    tANI_U32 reserved2;
} tFWLoggingInitResp, * tpFWLoggingInitResp;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader         header;
    tFWLoggingInitResp    tFWLoggingInitRespParams;
}  tFWLoggingInitRespMsg,  * tpFWLoggingInitRespMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_FW_LOGGING_DXE_DONE_IND
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 status;
    tANI_U32 logBuffLength[MAX_NUM_OF_BUFFER];
    tANI_U64 logBuffAddress[MAX_NUM_OF_BUFFER];
} tFWLoggingDxeDoneInd, * tpFWLoggingDxeDoneInd;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader         header;
    tFWLoggingDxeDoneInd    tFWLoggingDxeDoneIndParams;
}  tFWLoggingDxeDoneIndMsg,  * tpFWLoggingDxeDoneIndMsg;

/*---------------------------------------------------------------------------
 * Logging mail box structure
 *-------------------------------------------------------------------------*/

#define MAILBOX_VERSION_V1 0x1

typedef PACKED_PRE struct PACKED_POST
{
    /* Logging mail box version */
    tANI_U8               logMbVersion;
    /* Current logging buffer address */
    tANI_U64              logBuffAddress[MAX_NUM_OF_BUFFER];
    /* Current logging buffer length */
    tANI_U32              logBuffLength[MAX_NUM_OF_BUFFER];
    /* Flush reason code  0: Host requested Non zero FW FATAL event*/
    tANI_U16              reasonCode;
    /* Log type i.e. Mgmt frame = 0, QXDM = 1, FW Mem dump = 2 */
    tANI_U8               logType;
    /* Indicate if Last segment of log is received*/
    tANI_BOOLEAN          done;
}tLoggingMailBox, *tpLoggingMailBox;

/*---------------------------------------------------------------------------
* WLAN_HAL_ENABLE_MONITOR_MODE_REQ
*-------------------------------------------------------------------------*/

/* only 1 filter is supported as of now */
#define NUM_FILTERS_SUPPORTED 1

typedef PACKED_PRE struct PACKED_POST
{
   tSirMacAddr macAddr;
   tANI_U8 isA1filteringNeeded;
   tANI_U8 isA2filteringNeeded;
   tANI_U8 isA3filteringNeeded;
}tHalMacFilter, *tpHalMacFilter;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8 channelNumber;
   ePhyChanBondState cbState;

   tANI_U32 maxAmpduLen;
   tANI_U32 maxMpduInAmpduLen;

   tANI_U8 crcCheckEnabled;

   /* value is "1" for this FR. "0" means no filter, RECEIVE ALL PACKETS */
   tANI_U8 numMacFilters;
   tHalMacFilter macFilters[NUM_FILTERS_SUPPORTED];

   /* Each bit position maps to IEEE convention of typeSubtype */
   tANI_U64 typeSubtypeBitmap;

   tANI_U64 reserved;

}tHalEnableMonitorModeReqParams, *tpHalEnableMonitorModeReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnableMonitorModeReqParams enableMonitorModeReqParams;
}tHalEnableMonitorModeReqMsg, *tpHalEnableMonitorModeReqMsg;


/*---------------------------------------------------------------------------
* WLAN_HAL_ENABLE_MONITOR_MODE_RSP
*-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 status;
}tHalEnableMonitorModeRspParams, *tpHalEnableMonitorModeRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnableMonitorModeRspParams enableMonitorModeRspParams;
}tHalEnableMonitorModeRspMsg, *tpHalEnableMonitorModeRspMsg;

/*---------------------------------------------------------------------------
* WLAN_HAL_DISABLE_MONITOR_MODE_REQ
*-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tANI_U8 resetConfiguration;
}tHalDisableMonitorModeReqMsg, *tpHalDisableMonitorModeReqMsg;

/*---------------------------------------------------------------------------
* WLAN_HAL_DISABLE_MONITOR_MODE_RSP
*-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32 status;
}tHalDisableMonitorModeRspParams, *tpHalDisableMonitorModeRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalDisableMonitorModeRspParams disableMonitorModeRspParams;
}tHalDisableMonitorModeRspMsg, *tpHalDisableMonitorModeRspMsg;

typedef PACKED_PRE struct PACKED_POST
{
  tANI_U8   status;
}tHalAvoidFreqRangeCtrlParam, *tpHalAvoidFreqRangeCtrlParam;

#if defined(__ANI_COMPILER_PRAGMA_PACK_STACK)
#pragma pack(pop)
#elif defined(__ANI_COMPILER_PRAGMA_PACK)
#else
#endif

#endif /* _WLAN_HAL_MSG_H_ */
