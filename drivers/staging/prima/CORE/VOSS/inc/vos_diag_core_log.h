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

#if !defined( __VOS_DIAG_CORE_LOG_H )
#define __VOS_DIAG_CORE_LOG_H

/**=========================================================================
  
  \file  vos_event.h
  
  \brief virtual Operating System Services (vOSS) DIAG logs
               
   Definitions for vOSS Events
  
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "vos_types.h"
#include "i_vos_diag_core_log.h"

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#define VOS_LOG_MAX_NUM_SSID                                21
#define VOS_LOG_MAX_NUM_BSSID                               21
#define VOS_LOG_MAX_SSID_SIZE                               32
#define VOS_LOG_MAX_BSSID_SIZE                              6
#define VOS_LOG_MAX_NUM_CHANNEL                             64
#define VOS_LOG_MAX_NUM_HO_CANDIDATE_APS                    20
#define VOS_LOG_MAX_WOW_PTRN_SIZE                           128
#define VOS_LOG_MAX_WOW_PTRN_MASK_SIZE                      16

/*---------------------------------------------------------------------------
   This packet contains the scan results of the recent scan operation 
   LOG_WLAN_SCAN_C                                 0x1496 
---------------------------------------------------------------------------*/   
typedef struct
{
   log_hdr_type  hdr;
   v_U8_t  eventId;
   v_U8_t  numSsid;
   v_U8_t  ssid[VOS_LOG_MAX_NUM_SSID][VOS_LOG_MAX_SSID_SIZE];
   v_U8_t  bssid[VOS_LOG_MAX_NUM_BSSID][VOS_LOG_MAX_BSSID_SIZE];
   v_U8_t  totalSsid;
   v_U8_t  minChnTime;
   v_U8_t  maxChnTime;
   v_U16_t timeBetweenBgScan;
   v_U8_t  BSSMode;
   v_U8_t  numChannel;
   v_U8_t  channels[VOS_LOG_MAX_NUM_CHANNEL];
   v_U16_t status;
} vos_log_scan_pkt_type;

/*---------------------------------------------------------------------------
   This packet contains the information related to IBSS connection setup  
   LOG_WLAN_IBSS_C                                 0x1497 
---------------------------------------------------------------------------*/   
typedef struct 
{
   log_hdr_type  hdr;
   v_U8_t eventId;
   v_U8_t channelSetting;
   v_U8_t bssid[VOS_LOG_MAX_BSSID_SIZE];
   v_U8_t peerMacAddr[VOS_LOG_MAX_BSSID_SIZE];
   v_U8_t ssid[VOS_LOG_MAX_SSID_SIZE];
   v_U8_t operatingChannel;
   v_U8_t beaconInterval;
   v_U8_t status;
} vos_log_ibss_pkt_type;

/*---------------------------------------------------------------------------
   This packet contains the information related to 802.11D   
   LOG_WLAN_80211D_C                               0x1498 
---------------------------------------------------------------------------*/   
typedef struct
{
   log_hdr_type  hdr;
   v_U8_t eventId;
   v_U8_t numChannel;
   v_U8_t Channels[VOS_LOG_MAX_NUM_CHANNEL];
   v_U8_t TxPwr[VOS_LOG_MAX_NUM_CHANNEL];
   v_U8_t countryCode[3];
   v_U8_t supportMultipleDomain;
} vos_log_802_11d_pkt_type;

/*---------------------------------------------------------------------------
This is a log packet which contains below handoff information:
- Current AP + RSSI (if already associated)
- Candidate AP + RSSI (before association and when the list is updated)
- For each BSSID in candidate list, provide RSSI, QoS and security compatibility
LOG_WLAN_HANDOFF_C                              0x1499
---------------------------------------------------------------------------*/   
typedef struct
{
  v_U8_t        ssid[9];
  v_U8_t        bssid[VOS_LOG_MAX_BSSID_SIZE];
  v_U8_t        channel_id;
  v_U32_t       qos_score;
  v_U32_t       sec_score;
  v_U32_t       rssi_score;
  v_U32_t       overall_score;
  v_U32_t       tx_per;     /* represented as a %      */
  v_U32_t       rx_per;     /* represented as a %      */

} vos_log_ho_ap_info;

typedef struct
{
  log_hdr_type       hdr;
  v_U32_t            num_aps;
  vos_log_ho_ap_info current_ap_info;
  vos_log_ho_ap_info candidate_ap_info[VOS_LOG_MAX_NUM_HO_CANDIDATE_APS];
} vos_log_ho_pkt_type;

/*---------------------------------------------------------------------------
   This packet contains the information related to the EDCA parameters 
   advertised by the AP
   LOG_WLAN_QOS_EDCA_C                             0x149A
---------------------------------------------------------------------------*/
typedef struct
{
  log_hdr_type       hdr;
  v_U8_t             aci_be;
  v_U8_t             cw_be;
  v_U16_t            txoplimit_be;
  v_U8_t             aci_bk;
  v_U8_t             cw_bk;
  v_U16_t            txoplimit_bk;
  v_U8_t             aci_vi;
  v_U8_t             cw_vi;
  v_U16_t            txoplimit_vi;
  v_U8_t             aci_vo;
  v_U8_t             cw_vo;
  v_U16_t            txoplimit_vo;
} vos_log_qos_edca_pkt_type;

/*---------------------------------------------------------------------------
   This packet contains the total number of beacon received value    
   LOG_WLAN_BEACON_UPDATE_C                        0x149B
---------------------------------------------------------------------------*/
typedef struct
{
  log_hdr_type       hdr;
  v_U32_t            bcn_rx_cnt;
} vos_log_beacon_update_pkt_type;

/*---------------------------------------------------------------------------
   This packet contains the information related to a WoW patern value when set
   LOG_WLAN_POWERSAVE_WOW_ADD_PTRN_C               0x149C 
---------------------------------------------------------------------------*/
typedef struct
{
  log_hdr_type       hdr;
  v_U8_t             pattern_id;
  v_U8_t             pattern_byte_offset;
  v_U8_t             pattern_size;
  v_U8_t             pattern[VOS_LOG_MAX_WOW_PTRN_SIZE];
  v_U8_t             pattern_mask_size;
  v_U8_t             pattern_mask[VOS_LOG_MAX_WOW_PTRN_MASK_SIZE];
} vos_log_powersave_wow_add_ptrn_pkt_type;

/*---------------------------------------------------------------------------
   This packet contains the PHY & MAC layer statistics sent by lower layer
   _WLAN_LINKLAYER_STAT_C                       0x14A1
---------------------------------------------------------------------------*/
typedef struct
{
   v_U32_t retry_cnt[4];
   v_U32_t multiple_retry_cnt[4];
   v_U32_t tx_frm_cnt[4];
   v_U32_t rx_frm_cnt;
   v_U32_t frm_dup_cnt;
   v_U32_t fail_cnt[4];
   v_U32_t rts_fail_cnt;
   v_U32_t ack_fail_cnt;
   v_U32_t rts_succ_cnt;
   v_U32_t rx_discard_cnt;
   v_U32_t rx_error_cnt;
   v_U32_t tx_byte_cnt;

} summaryStatsInfo;

typedef struct
{
   v_U32_t rx_frag_cnt;
   v_U32_t promiscuous_rx_frag_cnt;
   v_U32_t rx_input_sensitivity;
   v_U32_t max_pwr;
   v_U32_t sync_fail_cnt;
   v_U32_t tx_rate;

} globalClassAStatsInfo;

typedef struct
{
   v_U32_t uc_rx_wep_unencrypted_frm_cnt;
   v_U32_t uc_rx_mic_fail_cnt;
   v_U32_t uc_tkip_icv_err;
   v_U32_t uc_aes_ccmp_format_err;
   v_U32_t uc_aes_ccmp_replay_cnt;
   v_U32_t uc_aes_ccmp_decrpt_err;
   v_U32_t uc_wep_undecryptable_cnt;
   v_U32_t uc_wep_icv_err;
   v_U32_t uc_rx_decrypt_succ_cnt;
   v_U32_t uc_rx_decrypt_fail_cnt;
   v_U32_t mcbc_rx_wep_unencrypted_frm_cnt;
   v_U32_t mcbc_rx_mic_fail_cnt;
   v_U32_t mcbc_tkip_icv_err;
   v_U32_t mcbc_aes_ccmp_format_err;
   v_U32_t mcbc_aes_ccmp_replay_cnt;
   v_U32_t mcbc_aes_ccmp_decrpt_err;
   v_U32_t mcbc_wep_undecryptable_cnt;
   v_U32_t mcbc_wep_icv_err;
   v_U32_t mcbc_rx_decrypt_succ_cnt;
   v_U32_t mcbc_rx_decrypt_fail_cnt;

} globalClassBStatsInfo;

typedef struct
{
   v_U32_t rx_amsdu_cnt;
   v_U32_t rx_ampdu_cnt;
   v_U32_t tx_20_frm_cnt;
   v_U32_t rx_20_frm_cnt;
   v_U32_t rx_mpdu_in_ampdu_cnt;
   v_U32_t ampdu_delimiter_crc_err;

} globalClassCStatsInfo;

typedef struct
{
   v_U32_t tx_uc_frm_cnt;
   v_U32_t tx_mc_frm_cnt;
   v_U32_t tx_bc_frm_cnt;
   v_U32_t rx_uc_frm_cnt;
   v_U32_t rx_mc_frm_cnt;
   v_U32_t rx_bc_frm_cnt;
   v_U32_t tx_uc_byte_cnt[4];
   v_U32_t tx_mc_byte_cnt;
   v_U32_t tx_bc_byte_cnt;
   v_U32_t rx_uc_byte_cnt[4];
   v_U32_t rx_mc_byte_cnt;
   v_U32_t rx_bc_byte_cnt;
   v_U32_t rx_byte_cnt;
   v_U32_t num_rx_bytes_crc_ok;
   v_U32_t rx_rate;

} globalClassDStatsInfo;

typedef struct
{
   v_U32_t tx_frag_cnt[4];
   v_U32_t tx_ampdu_cnt;
   v_U32_t tx_mpdu_in_ampdu_cnt;
} perStaStatsInfo;

typedef struct
{
  log_hdr_type             hdr;
  v_U8_t                   version;
  v_U8_t                   reserved[3];
  v_U32_t                  stat_mask;
  summaryStatsInfo         summaryStats;
  globalClassAStatsInfo    globalClassAStats;
  globalClassBStatsInfo    globalClassBStats;
  globalClassCStatsInfo    globalClassCStats;
  globalClassDStatsInfo    globalClassDStats;
  perStaStatsInfo          perStaStats;
} vos_log_statistics_pkt_type;

/*---------------------------------------------------------------------------
   This packet contains the Tspec info negotiated with the AP for the 
   specific AC 
   LOG_WLAN_QOS_TSPEC_C                            0x14A2 
---------------------------------------------------------------------------*/
typedef struct
{
  log_hdr_type    hdr;
  v_U8_t          tsinfo[3];
  v_U16_t         nominal_msdu_size;
  v_U16_t         maximum_msdu_size;
  v_U32_t         min_service_interval;
  v_U32_t         max_service_interval;
  v_U32_t         inactivity_interval;
  v_U32_t         suspension_interval;
  v_U32_t         svc_start_time;
  v_U32_t         min_data_rate;
  v_U32_t         mean_data_rate;
  v_U32_t         peak_data_rate;
  v_U32_t         max_burst_size;
  v_U32_t         delay_bound;
  v_U32_t         min_phy_rate;
  v_U16_t         surplus_bw_allowance;
  v_U16_t         medium_time;
} vos_log_qos_tspec_pkt_type;

/*---------------------------------------------------------------------------
   This packet contains data information when stall detected
   LOG_TRSP_DATA_STALL_C                           0x1801
---------------------------------------------------------------------------*/

typedef struct
{
   char         channelName[4];
   v_U32_t      numDesc;
   v_U32_t      numFreeDesc;
   v_U32_t      numRsvdDesc;
   v_U32_t      headDescOrder;
   v_U32_t      tailDescOrder;
   v_U32_t      ctrlRegVal;
   v_U32_t      statRegVal;
   v_U32_t      numValDesc;
   v_U32_t      numInvalDesc;
} vos_log_data_stall_channel_type;

typedef struct
{
   log_hdr_type                     hdr;
   v_U32_t                          PowerState;
   v_U32_t                          numFreeBd;
   vos_log_data_stall_channel_type  dxeChannelInfo[8];
} vos_log_data_stall_type;

/*---------------------------------------------------------------------------
   This packet contains the rssi value from BSS descriptor
   LOG_WLAN_RSSI_UPDATE_C                          0x1354
---------------------------------------------------------------------------*/
typedef struct
{
  log_hdr_type       hdr;
  v_S7_t            rssi;
} vos_log_rssi_pkt_type;

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __VOS_DIAG_CORE_LOG_H
