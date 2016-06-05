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




/*
 * This file wniApi.h contains message definitions exported by
 * Sirius software modules.
 * NOTE: See projects/sirius/include/sirApi.h for structure
 * definitions of the host/FW messages.
 *
 * Author:        Chandra Modumudi
 * Date:          04/11/2002
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef __WNI_API_H
#define __WNI_API_H

// DPH return error codes
#define ANI_DPH_SUCCESS            0

#define ANI_DPH_RX_STA_INVALID     1

#define ANI_DPH_DO_TKIP 2

#define ANI_DPH_PORT_BLOCKED 3
#define ANI_DPH_TX_PUSH_ERROR      10
#define ANI_DPH_TX_MAC_HDR_ERROR   11
#define ANI_DPH_TX_INVALID_PAYLOAD 12
#define ANI_DPH_TX_STA_INVALID     13
#define ANI_DPH_TX_HASH_MISS       14
#define ANI_DPH_TX_UNINITIALIZED   15
#define ANI_DPH_TX_RADIO_IN_DIAG_MODE 16
#define ANI_DPH_WMM_DROP 17
#define ANI_DPH_APSD_DROP 18
#define ANI_DPH_UNKNOWN_STA 19

/// HDD type for special handling of BDs in the TX pkts
/// Used in the structure ani_mod_info_t->bd_spl_proc_type
#define ANI_HDD_NO_SPL_PROC             0
#define ANI_HDD_DUMMY_PKT_SPL_PROC      1
#define ANI_HDD_PRE_DUMMY_PKT_SPL_PROC  2
#define ANI_HDD_WNS_L2_UPDATE_SPL_PROC  3
#define ANI_HDD_DUMMY_DATA              4
#ifdef WMM_APSD
#define ANI_HDD_EOSP_PKT                5
#endif

/// Message offset for the cmd to enqueue a dummy pkt to HDD TD ring
#define ANI_DUMMY_PKT_MSG_TYPE_OFFSET    0
#define ANI_DUMMY_PKT_MSG_LEN_OFFSET     2
#define ANI_DUMMY_PKT_MAC_ADDR_OFFSET    4
#define ANI_DUMMY_PKT_STA_ID_OFFSET      10
#define ANI_DUMMY_PKT_RT_FL_OFFSET       12
#define ANI_DUMMY_PKT_MSG_LEN            16
#define ANI_DUMMY_DATA_PAYLOAD_OFFSET    10

/**
 * Product IDs stored in the EEPROM for the different types of AP radio cards
 * supported by Polaris
 */
#define AGN1323AR_00      4
#define AGN1323AR_01      5
#define AGN1223AR_00      6
#define AGN1223AR_01      7
#define AGN1223AR_02      8
#define AGN_EEP_PRODUCT_ID_MAX   8

#define SIR_SME_MODULE_ID 0x16

/// Start of Sirius/Host message types
#define WNI_HOST_MSG_START             0x1500

enum eWniMsgTypes
{
    /// CFG message types
    eWNI_CFG_MSG_TYPES_BEGIN=WNI_HOST_MSG_START,
    eWNI_CFG_MSG_TYPES_END=eWNI_CFG_MSG_TYPES_BEGIN+0xFF,

    /// SME message types
    eWNI_SME_MSG_TYPES_BEGIN=eWNI_CFG_MSG_TYPES_END,
    eWNI_SME_START_REQ,
    eWNI_SME_START_RSP,
    eWNI_SME_SYS_READY_IND,
    eWNI_SME_SCAN_REQ,
    eWNI_SME_SCAN_ABORT_IND,
    eWNI_SME_SCAN_RSP,
#ifdef FEATURE_OEM_DATA_SUPPORT
    eWNI_SME_OEM_DATA_REQ,
    eWNI_SME_OEM_DATA_RSP,
#endif
    eWNI_SME_JOIN_REQ,
    eWNI_SME_JOIN_RSP,
    eWNI_SME_SETCONTEXT_REQ,
    eWNI_SME_SETCONTEXT_RSP,
    eWNI_SME_REASSOC_REQ,
    eWNI_SME_REASSOC_RSP,
    eWNI_SME_AUTH_REQ,
    eWNI_SME_AUTH_RSP,
    eWNI_SME_DISASSOC_REQ,
    eWNI_SME_DISASSOC_RSP,
    eWNI_SME_DISASSOC_IND,
    eWNI_SME_DISASSOC_CNF,
    eWNI_SME_DEAUTH_REQ,
    eWNI_SME_DEAUTH_RSP,
    eWNI_SME_DEAUTH_IND,
    eWNI_SME_WM_STATUS_CHANGE_NTF,
    eWNI_SME_IBSS_NEW_PEER_IND,
    eWNI_SME_IBSS_PEER_DEPARTED_IND,
    eWNI_SME_START_BSS_REQ,
    eWNI_SME_START_BSS_RSP,
    eWNI_SME_AUTH_IND,
    eWNI_SME_ASSOC_IND,
    eWNI_SME_ASSOC_CNF,
    eWNI_SME_REASSOC_IND,
    eWNI_SME_REASSOC_CNF,
    eWNI_SME_SWITCH_CHL_REQ,
    eWNI_SME_SWITCH_CHL_RSP,
    eWNI_SME_STOP_BSS_REQ,
    eWNI_SME_STOP_BSS_RSP,
    eWNI_SME_DEL_BA_PEER_IND,
    eWNI_SME_DEFINE_QOS_REQ,
    eWNI_SME_DEFINE_QOS_RSP,
    eWNI_SME_DELETE_QOS_REQ,
    eWNI_SME_DELETE_QOS_RSP,
    eWNI_SME_PROMISCUOUS_MODE_REQ,
    eWNI_SME_PROMISCUOUS_MODE_RSP,
    eWNI_SME_LINK_TEST_START_REQ,
    eWNI_SME_LINK_TEST_START_RSP,
    eWNI_SME_LINK_TEST_STOP_REQ,
    eWNI_SME_LINK_TEST_STOP_RSP,
    eWNI_SME_LINK_TEST_REPORT_IND,
    eWNI_SME_NEIGHBOR_BSS_IND,
    eWNI_SME_MEASUREMENT_REQ,
    eWNI_SME_MEASUREMENT_RSP,
    eWNI_SME_MEASUREMENT_IND,
    eWNI_SME_SET_WDS_INFO_REQ,
    eWNI_SME_SET_WDS_INFO_RSP,
    eWNI_SME_WDS_INFO_IND,
    eWNI_SME_SET_POWER_REQ,
    eWNI_SME_SET_POWER_RSP,
    eWNI_SME_CLIENT_SIDE_LOAD_BALANCE_REQ,
    eWNI_SME_CLIENT_SIDE_LOAD_BALANCE_RSP,
    eWNI_SME_SELECT_CHANNEL_REQ,
    eWNI_SME_SELECT_CHANNEL_RSP,
    eWNI_SME_SET_PROPRIETARY_IE_REQ,
    eWNI_SME_SET_PROPRIETARY_IE_RSP, // #endif
    eWNI_SME_DISCARD_SKB_NTF,  // Used to cleanup SKBs by HDD
    eWNI_SME_DEAUTH_CNF,
    eWNI_SME_MIC_FAILURE_IND,
    eWNI_SME_ADDTS_REQ,
    eWNI_SME_ADDTS_RSP,
    eWNI_SME_ADDTS_CNF,
    eWNI_SME_ADDTS_IND,
    eWNI_SME_DELTS_REQ,
    eWNI_SME_DELTS_RSP,
    eWNI_SME_DELTS_IND,
    eWNI_SME_SET_BACKGROUND_SCAN_MODE_REQ,
    eWNI_SME_SWITCH_CHL_CB_PRIMARY_REQ,
    eWNI_SME_SWITCH_CHL_CB_PRIMARY_RSP,
    eWNI_SME_SWITCH_CHL_CB_SECONDARY_REQ,
    eWNI_SME_SWITCH_CHL_CB_SECONDARY_RSP,
    eWNI_SME_PROBE_REQ,
    eWNI_SME_STA_STAT_REQ,
    eWNI_SME_STA_STAT_RSP,
    eWNI_SME_AGGR_STAT_REQ,
    eWNI_SME_AGGR_STAT_RSP,
    eWNI_SME_GLOBAL_STAT_REQ,
    eWNI_SME_GLOBAL_STAT_RSP,
    eWNI_SME_STAT_SUMM_REQ,
    eWNI_SME_STAT_SUMM_RSP,
    eWNI_SME_REMOVEKEY_REQ,
    eWNI_SME_REMOVEKEY_RSP,
    eWNI_SME_GET_SCANNED_CHANNEL_REQ,
    eWNI_SME_GET_SCANNED_CHANNEL_RSP,
    eWNI_SME_SET_TX_POWER_REQ,
    eWNI_SME_SET_TX_POWER_RSP,
    eWNI_SME_GET_TX_POWER_REQ,
    eWNI_SME_GET_TX_POWER_RSP,
    eWNI_SME_GET_NOISE_REQ,
    eWNI_SME_GET_NOISE_RSP,
    eWNI_SME_LOW_RSSI_IND,
    eWNI_SME_GET_STATISTICS_REQ,
    eWNI_SME_GET_STATISTICS_RSP,
    eWNI_SME_GET_RSSI_REQ,
    eWNI_SME_GET_ROAM_RSSI_REQ,
    eWNI_SME_GET_ROAM_RSSI_RSP,
    eWNI_SME_GET_ASSOC_STAS_REQ,
    eWNI_SME_TKIP_CNTR_MEAS_REQ,
    eWNI_SME_UPDATE_APWPSIE_REQ,
    eWNI_SME_GET_WPSPBC_SESSION_REQ,
    eWNI_SME_WPS_PBC_PROBE_REQ_IND,
    eWNI_SME_SET_APWPARSNIEs_REQ,
    eWNI_SME_UPPER_LAYER_ASSOC_CNF,
    eWNI_SME_HIDE_SSID_REQ,
    eWNI_SME_CHNG_MCC_BEACON_INTERVAL,
    eWNI_SME_REMAIN_ON_CHANNEL_REQ,
    eWNI_SME_REMAIN_ON_CHN_IND,
    eWNI_SME_REMAIN_ON_CHN_RSP,
    eWNI_SME_REMAIN_ON_CHN_RDY_IND,
    eWNI_SME_SEND_ACTION_FRAME_IND,
    eWNI_SME_ACTION_FRAME_SEND_CNF,
    eWNI_SME_ABORT_REMAIN_ON_CHAN_IND,
    eWNI_SME_UPDATE_NOA,
    eWNI_SME_CLEAR_DFS_CHANNEL_LIST,
    eWNI_SME_PRE_CHANNEL_SWITCH_FULL_POWER,
    eWNI_SME_GET_SNR_REQ,
    eWNI_SME_LOST_LINK_PARAMS_IND,
    //General Power Save Messages
    eWNI_PMC_MSG_TYPES_BEGIN,
    eWNI_PMC_PWR_SAVE_CFG,

    //BMPS Messages
    eWNI_PMC_ENTER_BMPS_REQ,
    eWNI_PMC_ENTER_BMPS_RSP,
    eWNI_PMC_EXIT_BMPS_REQ,
    eWNI_PMC_EXIT_BMPS_RSP,
    eWNI_PMC_EXIT_BMPS_IND,

    //IMPS Messages.
    eWNI_PMC_ENTER_IMPS_REQ,
    eWNI_PMC_ENTER_IMPS_RSP,
    eWNI_PMC_EXIT_IMPS_REQ,
    eWNI_PMC_EXIT_IMPS_RSP,

    //UAPSD Messages
    eWNI_PMC_ENTER_UAPSD_REQ,
    eWNI_PMC_ENTER_UAPSD_RSP,
    eWNI_PMC_EXIT_UAPSD_REQ,
    eWNI_PMC_EXIT_UAPSD_RSP,

    //WOWL Messages
    eWNI_PMC_SMPS_STATE_IND,

    //WoWLAN Messages
    eWNI_PMC_WOWL_ADD_BCAST_PTRN,
    eWNI_PMC_WOWL_DEL_BCAST_PTRN,
    eWNI_PMC_ENTER_WOWL_REQ,
    eWNI_PMC_ENTER_WOWL_RSP,
    eWNI_PMC_EXIT_WOWL_REQ,
    eWNI_PMC_EXIT_WOWL_RSP,

#ifdef WLAN_FEATURE_PACKET_FILTERING
    eWNI_PMC_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP,
#endif // WLAN_FEATURE_PACKET_FILTERING

#if defined WLAN_FEATURE_VOWIFI
    eWNI_SME_RRM_MSG_TYPE_BEGIN,

    eWNI_SME_NEIGHBOR_REPORT_REQ_IND,
    eWNI_SME_NEIGHBOR_REPORT_IND,
    eWNI_SME_BEACON_REPORT_REQ_IND,
    eWNI_SME_BEACON_REPORT_RESP_XMIT_IND,

#endif
    eWNI_SME_ADD_STA_SELF_REQ,
    eWNI_SME_ADD_STA_SELF_RSP,
    eWNI_SME_DEL_STA_SELF_REQ,
    eWNI_SME_DEL_STA_SELF_RSP,

#if defined WLAN_FEATURE_VOWIFI_11R
    eWNI_SME_FT_PRE_AUTH_REQ,
    eWNI_SME_FT_PRE_AUTH_RSP,
    eWNI_SME_FT_UPDATE_KEY,
    eWNI_SME_FT_AGGR_QOS_REQ,
    eWNI_SME_FT_AGGR_QOS_RSP,
#endif

#if defined FEATURE_WLAN_ESE
    eWNI_SME_ESE_ADJACENT_AP_REPORT,
#endif

    eWNI_SME_REGISTER_MGMT_FRAME_REQ,

    eWNI_SME_COEX_IND,

#ifdef FEATURE_WLAN_SCAN_PNO
    eWNI_SME_PREF_NETWORK_FOUND_IND,
#endif // FEATURE_WLAN_SCAN_PNO

    eWNI_SME_TX_PER_HIT_IND,

    eWNI_SME_CHANGE_COUNTRY_CODE,
    eWNI_SME_GENERIC_CHANGE_COUNTRY_CODE,
    eWNI_SME_PRE_SWITCH_CHL_IND,
    eWNI_SME_POST_SWITCH_CHL_IND,

    eWNI_SME_MAX_ASSOC_EXCEEDED,

    eWNI_SME_BTAMP_LOG_LINK_IND,//to serialize the create/accpet LL req from HCI


#ifdef WLAN_WAKEUP_EVENTS
    eWNI_SME_WAKE_REASON_IND,
#endif // WLAN_WAKEUP_EVENTS
    eWNI_SME_EXCLUDE_UNENCRYPTED,
    eWNI_SME_RSSI_IND, //RSSI indication from TL to be serialized on MC thread
#ifdef FEATURE_WLAN_TDLS
    eWNI_SME_TDLS_SEND_MGMT_REQ,    
    eWNI_SME_TDLS_SEND_MGMT_RSP,    
    eWNI_SME_TDLS_ADD_STA_REQ,    
    eWNI_SME_TDLS_ADD_STA_RSP,    
    eWNI_SME_TDLS_DEL_STA_REQ,    
    eWNI_SME_TDLS_DEL_STA_RSP,
    eWNI_SME_TDLS_DEL_STA_IND,
    eWNI_SME_TDLS_DEL_ALL_PEER_IND,
    eWNI_SME_MGMT_FRM_TX_COMPLETION_IND,
    eWNI_SME_TDLS_LINK_ESTABLISH_REQ,
    eWNI_SME_TDLS_LINK_ESTABLISH_RSP,
// tdlsoffchan
    eWNI_SME_TDLS_CHANNEL_SWITCH_REQ,
    eWNI_SME_TDLS_CHANNEL_SWITCH_RSP,
#endif
    //NOTE: If you are planning to add more mesages, please make sure that 
    //SIR_LIM_ITC_MSG_TYPES_BEGIN is moved appropriately. It is set as
    //SIR_LIM_MSG_TYPES_BEGIN+0xB0 = 12B0 (which means max of 176 messages and
    //eWNI_SME_TDLS_DEL_STA_RSP = 175.
    //Should fix above issue to enable TDLS_INTERNAL
    eWNI_SME_SET_BCN_FILTER_REQ,
    eWNI_SME_RESET_AP_CAPS_CHANGED,
#ifdef WLAN_FEATURE_11W
    eWNI_SME_UNPROT_MGMT_FRM_IND,
#endif
#ifdef WLAN_FEATURE_GTK_OFFLOAD
    eWNI_PMC_GTK_OFFLOAD_GETINFO_RSP,
#endif // WLAN_FEATURE_GTK_OFFLOAD
    eWNI_SME_CANDIDATE_FOUND_IND, /*ROAM candidate indication from FW*/
    eWNI_SME_HANDOFF_REQ,/*upper layer requested handoff to driver in STA mode*/
    eWNI_SME_ROAM_SCAN_OFFLOAD_RSP,/*Fwd the LFR scan offload rsp from FW to SME*/
#ifdef FEATURE_WLAN_LPHB
    eWNI_SME_LPHB_IND,
#endif /* FEATURE_WLAN_LPHB */

    eWNI_SME_GET_TSM_STATS_REQ,
    eWNI_SME_GET_TSM_STATS_RSP,
    eWNI_SME_TSM_IE_IND,

#ifdef FEATURE_WLAN_CH_AVOID
   eWNI_SME_CH_AVOID_IND,
#endif /* FEATURE_WLAN_CH_AVOID */
    eWNI_SME_HT40_OBSS_SCAN_IND, /* START and UPDATE OBSS SCAN Indication*/
    eWNI_SME_HT40_STOP_OBSS_SCAN_IND, /* STOP OBSS SCAN indication */
#ifdef WLAN_FEATURE_AP_HT40_24G
    eWNI_SME_SET_HT_2040_MODE, /* HT 20/40 indication in SAP case for 2.4GHz*/
    eWNI_SME_2040_COEX_IND, /* HT20/40 Coex indication in SAP case for 2.4GHz*/
#endif
    eWNI_SME_MAC_SPOOF_ADDR_IND,
    eWNI_SME_ENCRYPT_MSG_RSP,
    eWNI_SME_UPDATE_MAX_RATE_IND,
    eWNI_SME_NAN_EVENT,
    eWNI_SME_SET_TDLS_2040_BSSCOEX_REQ,
    eWNI_SME_DEL_ALL_TDLS_PEERS,
    eWNI_SME_REGISTER_MGMT_FRAME_CB,
    eWNI_SME_MSG_TYPES_END
};

#define WNI_CFG_MSG_TYPES_BEGIN        0x1200

/*---------------------------------------------------------------------*/
/* CFG Module Definitions                                              */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/* CFG message definitions                                             */
/*---------------------------------------------------------------------*/
#define WNI_CFG_MSG_HDR_MASK    0xffff0000
#define WNI_CFG_MSG_LEN_MASK    0x0000ffff
#define WNI_CFG_MB_HDR_LEN      4
#define WNI_CFG_MAX_PARAM_NUM   32


/*---------------------------------------------------------------------*/
/* CFG to HDD message types                                            */
/*---------------------------------------------------------------------*/
#define WNI_CFG_PARAM_UPDATE_IND       (WNI_CFG_MSG_TYPES_BEGIN | 0x00)
#define WNI_CFG_DNLD_REQ               (WNI_CFG_MSG_TYPES_BEGIN | 0x01)
#define WNI_CFG_DNLD_CNF               (WNI_CFG_MSG_TYPES_BEGIN | 0x02)
#define WNI_CFG_GET_RSP                (WNI_CFG_MSG_TYPES_BEGIN | 0x03)
#define WNI_CFG_SET_CNF                (WNI_CFG_MSG_TYPES_BEGIN | 0x04)
#define WNI_CFG_GET_ATTRIB_RSP         (WNI_CFG_MSG_TYPES_BEGIN | 0x05)
#define WNI_CFG_ADD_GRP_ADDR_CNF       (WNI_CFG_MSG_TYPES_BEGIN | 0x06)
#define WNI_CFG_DEL_GRP_ADDR_CNF       (WNI_CFG_MSG_TYPES_BEGIN | 0x07)

#define ANI_CFG_GET_RADIO_STAT_RSP     (WNI_CFG_MSG_TYPES_BEGIN | 0x08)
#define ANI_CFG_GET_PER_STA_STAT_RSP   (WNI_CFG_MSG_TYPES_BEGIN | 0x09)
#define ANI_CFG_GET_AGG_STA_STAT_RSP   (WNI_CFG_MSG_TYPES_BEGIN | 0x0a)
#define ANI_CFG_CLEAR_STAT_RSP         (WNI_CFG_MSG_TYPES_BEGIN | 0x0b)


/*---------------------------------------------------------------------*/
/* CFG to HDD message paramter indices                                 */

/*   The followings are word indices starting from the message body    */

/*   WNI_CFG_xxxx_xxxx_xxxx:         index of parameter                */
/*                                                                     */
/*   WNI_CFG_xxxx_xxxx_NUM:          number of parameters in message   */
/*                                                                     */
/*   WNI_CFG_xxxx_xxxx_LEN:          byte length of message including  */
/*                                   MB header                         */
/*                                                                     */
/*   WNI_CFG_xxxx_xxxx_PARTIAL_LEN:  byte length of message including  */
/*                                   parameters and MB header but      */
/*                                   excluding variable data length    */
/*---------------------------------------------------------------------*/

// Parameter update indication
#define WNI_CFG_PARAM_UPDATE_IND_PID   0

#define WNI_CFG_PARAM_UPDATE_IND_NUM   1
#define WNI_CFG_PARAM_UPDATE_IND_LEN   (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_PARAM_UPDATE_IND_NUM << 2))

// Configuration download request
#define WNI_CFG_DNLD_REQ_NUM           0
#define WNI_CFG_DNLD_REQ_LEN           WNI_CFG_MB_HDR_LEN

// Configuration download confirm
#define WNI_CFG_DNLD_CNF_RES           0

#define WNI_CFG_DNLD_CNF_NUM           1
#define WNI_CFG_DNLD_CNF_LEN           (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_DNLD_CNF_NUM << 2))
// Get response
#define WNI_CFG_GET_RSP_RES            0
#define WNI_CFG_GET_RSP_PID            1
#define WNI_CFG_GET_RSP_PLEN           2

#define WNI_CFG_GET_RSP_NUM            3
#define WNI_CFG_GET_RSP_PARTIAL_LEN    (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_GET_RSP_NUM << 2))
// Set confirm
#define WNI_CFG_SET_CNF_RES            0
#define WNI_CFG_SET_CNF_PID            1

#define WNI_CFG_SET_CNF_NUM            2
#define WNI_CFG_SET_CNF_LEN            (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_SET_CNF_NUM << 2))
// Get attribute response
#define WNI_CFG_GET_ATTRIB_RSP_RES     0
#define WNI_CFG_GET_ATTRIB_RSP_PID     1
#define WNI_CFG_GET_ATTRIB_RSP_TYPE    2
#define WNI_CFG_GET_ATTRIB_RSP_PLEN    3
#define WNI_CFG_GET_ATTRIB_RSP_RW      4

#define WNI_CFG_GET_ATTRIB_RSP_NUM     5
#define WNI_CFG_GET_ATTRIB_RSP_LEN     (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_GET_ATTRIB_RSP_NUM << 2))

// Add group address confirm
#define WNI_CFG_ADD_GRP_ADDR_CNF_RES   0

#define WNI_CFG_ADD_GRP_ADDR_CNF_NUM   1
#define WNI_CFG_ADD_GRP_ADDR_CNF_LEN   (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_ADD_GRP_ADDR_CNF_NUM << 2))

// Delete group address confirm
#define WNI_CFG_DEL_GRP_ADDR_CNF_RES   0

#define WNI_CFG_DEL_GRP_ADDR_CNF_NUM   1
#define WNI_CFG_DEL_GRP_ADDR_CNF_LEN   (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_DEL_GRP_ADDR_CNF_NUM <<2))


#define IS_CFG_MSG(msg) ((msg & 0xff00) == WNI_CFG_MSG_TYPES_BEGIN)

// Clear stats types.
#define ANI_CLEAR_ALL_STATS          0
#define ANI_CLEAR_RX_STATS           1
#define ANI_CLEAR_TX_STATS           2
#define ANI_CLEAR_PER_STA_STATS      3
#define ANI_CLEAR_AGGR_PER_STA_STATS 4
#define ANI_CLEAR_STAT_TYPES_END     5

/*---------------------------------------------------------------------*/
/* HDD to CFG message types                                            */
/*---------------------------------------------------------------------*/
#define WNI_CFG_DNLD_RSP               (WNI_CFG_MSG_TYPES_BEGIN | 0x80)
#define WNI_CFG_GET_REQ                (WNI_CFG_MSG_TYPES_BEGIN | 0x81)
#define WNI_CFG_SET_REQ                (WNI_CFG_MSG_TYPES_BEGIN | 0x82)
#define WNI_CFG_SET_REQ_NO_RSP         (WNI_CFG_MSG_TYPES_BEGIN | 0x83) //No RSP for this set

// Shall be removed after stats integration


/*---------------------------------------------------------------------*/
/* HDD to CFG message paramter indices                                 */
/*                                                                     */
/*   The followings are word indices starting from the message body    */
/*                                                                     */
/*   WNI_CFG_xxxx_xxxx_xxxx:         index of parameter                */
/*                                                                     */
/*   WNI_CFG_xxxx_xxxx_NUM:          number of parameters in message   */
/*                                                                     */
/*   WNI_CFG_xxxx_xxxx_LEN:          byte length of message including  */
/*                                   MB header                         */
/*                                                                     */
/*   WNI_CFG_xxxx_xxxx_PARTIAL_LEN:  byte length of message including  */
/*                                   parameters and MB header but      */
/*                                   excluding variable data length    */
/*---------------------------------------------------------------------*/

// Download response
#define WNI_CFG_DNLD_RSP_BIN_LEN       0

#define WNI_CFG_DNLD_RSP_NUM           1
#define WNI_CFG_DNLD_RSP_PARTIAL_LEN   (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_DNLD_RSP_NUM << 2))

// Set parameter request
#define WNI_CFG_SET_REQ_PID            0
#define WNI_CFG_SET_REQ_PLEN           1

/*
// Get attribute request
//#define WNI_CFG_GET_ATTRIB_REQ_PID   0

//#define WNI_CFG_GET_ATTRIB_REQ_NUM     1
//#define WNI_CFG_GET_ATTRIB_REQ_LEN     (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_GET_ATTRIB_REQ_NUM << 2))
// Add group address request
#define WNI_CFG_ADD_GRP_ADDR_REQ_MAC_ADDR    0

#define WNI_CFG_ADD_GRP_ADDR_REQ_NUM   1
#define WNI_CFG_ADD_GRP_ADDR_REQ_LEN   (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_ADD_GRP_ADDR_REQ_NUM << 2))
// Delete group address request
#define WNI_CFG_DEL_GRP_ADDR_REQ_MAC_ADDR    0

#define WNI_CFG_DEL_GRP_ADDR_REQ_NUM   1
#define WNI_CFG_DEL_GRP_ADDR_REQ_LEN   (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_DEL_GRP_ADDR_REQ_NUM << 2))
*/


/*---------------------------------------------------------------------*/
/* CFG return values                                                   */
/*---------------------------------------------------------------------*/
#define WNI_CFG_SUCCESS             1
#define WNI_CFG_NOT_READY           2
#define WNI_CFG_INVALID_PID         3
#define WNI_CFG_INVALID_LEN         4
#define WNI_CFG_RO_PARAM            5
#define WNI_CFG_WO_PARAM            6
#define WNI_CFG_INVALID_STAID       7
#define WNI_CFG_OTHER_ERROR         8
#define WNI_CFG_NEED_RESTART        9
#define WNI_CFG_NEED_RELOAD        10


/*---------------------------------------------------------------------*/
/* CFG definitions                                                     */
/*---------------------------------------------------------------------*/
#define WNI_CFG_TYPE_STR            0x0000000
#define WNI_CFG_TYPE_INT            0x0000001
#define WNI_CFG_HOST_RE             0x0000002
#define WNI_CFG_HOST_WE             0x0000004


// Shall be removed after integration of stats.
// Get statistic response
#define WNI_CFG_GET_STAT_RSP_RES       0
#define WNI_CFG_GET_STAT_RSP_PARAMID   1
#define WNI_CFG_GET_STAT_RSP_VALUE     2

#define WNI_CFG_GET_STAT_RSP_NUM       3
#define WNI_CFG_GET_STAT_RSP_LEN       (WNI_CFG_MB_HDR_LEN + \
                                        (WNI_CFG_GET_STAT_RSP_NUM <<2))
// Get per station statistic response
#define WNI_CFG_GET_PER_STA_STAT_RSP_RES                        0
#define WNI_CFG_GET_PER_STA_STAT_RSP_STAID                      1
#define WNI_CFG_GET_PER_STA_STAT_RSP_FIRST_PARAM                2

// Per STA statistic structure
typedef struct sAniCfgPerStaStatStruct
{
       unsigned long     sentAesBlksUcastHi;
       unsigned long     sentAesBlksUcastLo;

       unsigned long     recvAesBlksUcastHi;
       unsigned long     recvAesBlksUcastLo;

       unsigned long     aesFormatErrorUcastCnts;

       unsigned long     aesReplaysUcast;

       unsigned long     aesDecryptErrUcast;

       unsigned long     singleRetryPkts;

       unsigned long     failedTxPkts;

       unsigned long     ackTimeouts;

       unsigned long     multiRetryPkts;

       unsigned long     fragTxCntsHi;
       unsigned long     fragTxCntsLo;

       unsigned long     transmittedPktsHi;
       unsigned long     transmittedPktsLo;

       unsigned long     phyStatHi;
       unsigned long     phyStatLo;
} tCfgPerStaStatStruct, *tpAniCfgPerStaStatStruct;

#define WNI_CFG_GET_PER_STA_STAT_RSP_NUM                       23
#define WNI_CFG_GET_PER_STA_STAT_RSP_LEN    (WNI_CFG_MB_HDR_LEN + \
                                   (WNI_CFG_GET_PER_STA_STAT_RSP_NUM << 2))


// Shall be removed after integrating stats.
#define WNI_CFG_GET_STAT_RSP           (WNI_CFG_MSG_TYPES_BEGIN | 0x08)
#define WNI_CFG_GET_PER_STA_STAT_RSP   (WNI_CFG_MSG_TYPES_BEGIN | 0x09)
#define WNI_CFG_GET_AGG_STA_STAT_RSP   (WNI_CFG_MSG_TYPES_BEGIN | 0x0a)
#define WNI_CFG_GET_TX_RATE_CTR_RSP    (WNI_CFG_MSG_TYPES_BEGIN | 0x0b)

#define WNI_CFG_GET_AGG_STA_STAT_RSP_NUM    21
#define WNI_CFG_GET_AGG_STA_STAT_RSP_LEN    (WNI_CFG_MB_HDR_LEN + \
                                   (WNI_CFG_GET_AGG_STA_STAT_RSP_NUM << 2))
#define WNI_CFG_GET_AGG_STA_STAT_RSP_RES 0

  // Get TX rate based stats
#define WNI_CFG_GET_TX_RATE_CTR_RSP_RES                        0

typedef struct sAniCfgTxRateCtrs
{
// add the rate counters here
    unsigned long TxFrames_1Mbps;
    unsigned long TxFrames_2Mbps;
    unsigned long TxFrames_5_5Mbps;
    unsigned long TxFrames_6Mbps;
    unsigned long TxFrames_9Mbps;
    unsigned long TxFrames_11Mbps;
    unsigned long TxFrames_12Mbps;
    unsigned long TxFrames_18Mbps;
    unsigned long TxFrames_24Mbps;
    unsigned long TxFrames_36Mbps;
    unsigned long TxFrames_48Mbps;
    unsigned long TxFrames_54Mbps;
    unsigned long TxFrames_72Mbps;
    unsigned long TxFrames_96Mbps;
    unsigned long TxFrames_108Mbps;

} tAniCfgTxRateCtrs, *tpAniCfgTxRateCtrs;


#define WNI_CFG_GET_STAT_REQ           (WNI_CFG_MSG_TYPES_BEGIN | 0x86)
#define WNI_CFG_GET_PER_STA_STAT_REQ   (WNI_CFG_MSG_TYPES_BEGIN | 0x87)
#define WNI_CFG_GET_AGG_STA_STAT_REQ   (WNI_CFG_MSG_TYPES_BEGIN | 0x88)
#define WNI_CFG_GET_TX_RATE_CTR_REQ    (WNI_CFG_MSG_TYPES_BEGIN | 0x89)

// Get statistic request
#define WNI_CFG_GET_STAT_REQ_PARAMID   0

#define WNI_CFG_GET_STAT_REQ_NUM       1
#define WNI_CFG_GET_STAT_REQ_LEN       (WNI_CFG_MB_HDR_LEN + \
                                       (WNI_CFG_GET_STAT_REQ_NUM << 2))

  // Get per station statistic request
#define WNI_CFG_GET_PER_STA_STAT_REQ_STAID 0

#define WNI_CFG_GET_PER_STA_STAT_REQ_NUM   1
#define WNI_CFG_GET_PER_STA_STAT_REQ_LEN   (WNI_CFG_MB_HDR_LEN + \
                                   (WNI_CFG_GET_PER_STA_STAT_REQ_NUM << 2))




#define DYNAMIC_CFG_TYPE_SELECTED_REGISTRAR   (0)
#define DYNAMIC_CFG_TYPE_WPS_STATE            (1)

#endif /* __WNI_API_H */

