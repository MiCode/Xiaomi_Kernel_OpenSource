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




/*
 * This file sirParams.h contains the common parameter definitions, which
 * are not dependent on threadX API. These can be used by all Firmware
 * modules.
 *
 * Author:      Sandesh Goel
 * Date:        04/13/2002
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef __SIRPARAMS_H
#define __SIRPARAMS_H

# include "sirTypes.h"

// Firmware wide constants

#define SIR_MAX_PACKET_SIZE     2048
#define SIR_MAX_NUM_CHANNELS    64
#define SIR_MAX_NUM_STA_IN_IBSS 16
#define SIR_MAX_NUM_STA_IN_BSS  256
#define SIR_ESE_MAX_MEAS_IE_REQS   8

typedef enum
{
    PHY_SINGLE_CHANNEL_CENTERED     = 0,        // 20MHz IF bandwidth centered on IF carrier
    PHY_DOUBLE_CHANNEL_LOW_PRIMARY  = 1,        // 40MHz IF bandwidth with lower 20MHz supporting the primary channel
    PHY_DOUBLE_CHANNEL_HIGH_PRIMARY = 3,        // 40MHz IF bandwidth with higher 20MHz supporting the primary channel
#ifdef WLAN_FEATURE_11AC
    PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED = 4, //20/40MHZ offset LOW 40/80MHZ offset CENTERED
    PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED = 5, //20/40MHZ offset CENTERED 40/80MHZ offset CENTERED
    PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED = 6, //20/40MHZ offset HIGH 40/80MHZ offset CENTERED
    PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW = 7,//20/40MHZ offset LOW 40/80MHZ offset LOW
    PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW = 8, //20/40MHZ offset HIGH 40/80MHZ offset LOW
    PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH = 9, //20/40MHZ offset LOW 40/80MHZ offset HIGH
    PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH = 10,//20/40MHZ offset-HIGH 40/80MHZ offset HIGH
#endif
    PHY_CHANNEL_BONDING_STATE_MAX   = 11
}ePhyChanBondState;

#define SIR_MIN(a,b)   (((a) < (b)) ? (a) : (b))
#define SIR_MAX(a,b)   (((a) > (b)) ? (a) : (b))

typedef enum {
   MCC     = 0,
   P2P     = 1,
   DOT11AC = 2,
   SLM_SESSIONIZATION = 3,
   DOT11AC_OPMODE = 4,
   SAP32STA = 5,
   TDLS = 6,
   P2P_GO_NOA_DECOUPLE_INIT_SCAN = 7,
   WLANACTIVE_OFFLOAD = 8,
   RTT = 20,
   WOW = 22,
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
   WLAN_ROAM_SCAN_OFFLOAD = 23,
#endif
   IBSS_HEARTBEAT_OFFLOAD = 26,
   WLAN_PERIODIC_TX_PTRN = 28,
#ifdef FEATURE_WLAN_TDLS
   ADVANCE_TDLS = 29,
#endif

#ifdef FEATURE_WLAN_BATCH_SCAN
   BATCH_SCAN = 30,
#endif
   FW_IN_TX_PATH = 31,
   EXTENDED_NSOFFLOAD_SLOT = 32,
   CH_SWITCH_V1           = 33,
   HT40_OBSS_SCAN         = 34,
   UPDATE_CHANNEL_LIST    = 35,
   WLAN_MCADDR_FLT        = 36,
   WLAN_CH144             = 37,
#ifdef WLAN_FEATURE_NAN
   NAN = 38,
#endif
#ifdef FEATURE_WLAN_TDLS
   TDLS_SCAN_COEXISTENCE  = 39,
#endif
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
   LINK_LAYER_STATS_MEAS  = 40,
#endif

   MU_MIMO                = 41,
#ifdef WLAN_FEATURE_EXTSCAN
   EXTENDED_SCAN          = 42,
#endif

   DYNAMIC_WMM_PS        = 43,
   MAC_SPOOFED_SCAN      = 44,
   BMU_ERROR_GENERIC_RECOVERY = 45,
   DISA                  = 46,
   FW_STATS              = 47,
   WPS_PRBRSP_TMPL       = 48,
   BCN_IE_FLT_DELTA      = 49,
   //MAX_FEATURE_SUPPORTED = 128
} placeHolderInCapBitmap;

typedef enum eSriLinkState {
    eSIR_LINK_IDLE_STATE        = 0,
    eSIR_LINK_PREASSOC_STATE    = 1,
    eSIR_LINK_POSTASSOC_STATE   = 2,
    eSIR_LINK_AP_STATE          = 3,
    eSIR_LINK_IBSS_STATE        = 4,
    // BT-AMP Case
    eSIR_LINK_BTAMP_PREASSOC_STATE  = 5,
    eSIR_LINK_BTAMP_POSTASSOC_STATE  = 6,
    eSIR_LINK_BTAMP_AP_STATE  = 7,
    eSIR_LINK_BTAMP_STA_STATE  = 8,

    // Reserved for HAL internal use
    eSIR_LINK_LEARN_STATE       = 9,
    eSIR_LINK_SCAN_STATE        = 10,
    eSIR_LINK_FINISH_SCAN_STATE = 11,
    eSIR_LINK_INIT_CAL_STATE    = 12,
    eSIR_LINK_FINISH_CAL_STATE  = 13,
    eSIR_LINK_LISTEN_STATE      = 14,
    eSIR_LINK_SEND_ACTION_STATE = 15,
} tSirLinkState;


/// Message queue structure used across Sirius project.
/// NOTE: this structure should be multiples of a word size (4bytes)
/// as this is used in tx_queue where it expects to be multiples of 4 bytes.
typedef struct sSirMsgQ
{
    tANI_U16 type;
    /*
     * This field can be used as sequence number/dialog token for matching
     * requests and responses.
     */
    tANI_U16 reserved;
    /**
     * Based on the type either a bodyptr pointer into
     * memory or bodyval as a 32 bit data is used.
     * bodyptr: is always a freeable pointer, one should always
     * make sure that bodyptr is always freeable.
     *
     * Messages should use either bodyptr or bodyval; not both !!!.
     */
    void *bodyptr;
    tANI_U32 bodyval;

    /*
     * Some messages provide a callback function.  The function signature
     * must be agreed upon between the two entities exchanging the message
     */
    void *callback;

} tSirMsgQ, *tpSirMsgQ;

/// Mailbox Message Structure Define
typedef struct sSirMbMsg
{
    tANI_U16 type;

    /**
     * This length includes 4 bytes of header, that is,
     * 2 bytes type + 2 bytes msgLen + n*4 bytes of data.
     * This field is byte length.
     */
    tANI_U16 msgLen;

    /**
     * This is the first data word in the mailbox message.
     * It is followed by n words of data.
     * NOTE: data[1] is not a place holder to store data
     * instead to dereference the message body.
     */
    tANI_U32 data[1];
} tSirMbMsg, *tpSirMbMsg;

/// Mailbox Message Structure for P2P
typedef struct sSirMbMsgP2p
{
    tANI_U16 type;

    /**
     * This length includes 4 bytes of header, that is,
     * 2 bytes type + 2 bytes msgLen + n*4 bytes of data.
     * This field is byte length.
     */
    tANI_U16 msgLen;

    tANI_U8 sessionId;
    tANI_U8 noack;
    tANI_U16 wait;

    /**
     * This is the first data word in the mailbox message.
     * It is followed by n words of data.
     * NOTE: data[1] is not a place holder to store data
     * instead to dereference the message body.
     */
    tANI_U32 data[1];
} tSirMbMsgP2p, *tpSirMbMsgP2p;

/// Message queue definitions
//  msgtype(2bytes) reserved(2bytes) bodyptr(4bytes) bodyval(4bytes)
//  NOTE tSirMsgQ should be always multiples of WORD(4Bytes)
//  All Queue Message Size are multiples of word Size (4 bytes)
#define SYS_MSG_SIZE            (sizeof(tSirMsgQ)/4)

/// gHalMsgQ

#define SYS_HAL_MSG_SIZE        SYS_MSG_SIZE

/// gMMHhiPriorityMsgQ

#define SYS_MMH_HI_PRI_MSG_SIZE SYS_MSG_SIZE

/// gMMHprotocolMsgQ

#define SYS_MMH_PROT_MSG_SIZE   SYS_MSG_SIZE

/// gMMHdebugMsgQ

#define SYS_MMH_DEBUG_MSG_SIZE  SYS_MSG_SIZE

/// gMAINTmsgQ

#define SYS_MNT_MSG_SIZE        SYS_MSG_SIZE

/// LIM Message Queue

#define SYS_LIM_MSG_SIZE        SYS_MSG_SIZE

/// ARQ Message Queue

#define SYS_ARQ_MSG_SIZE        SYS_MSG_SIZE

/// Scheduler Message Queue

#define SYS_SCH_MSG_SIZE        SYS_MSG_SIZE

/// PMM Message Queue

#define SYS_PMM_MSG_SIZE        SYS_MSG_SIZE

/// TX Message Queue

#define SYS_TX_MSG_SIZE         (sizeof(void *)/4)  // Message pointer size

/// RX Message Queue

#define SYS_RX_MSG_SIZE         (sizeof(void *)/4)  // Message pointer size

/// PTT  Message Queue
#define SYS_NIM_PTT_MSG_SIZE    SYS_MSG_SIZE  // Message pointer size



/* *************************************** *
 *                                         *
 *        Block pool configuration         *
 *                                         *
 * *************************************** */

// The following values specify the number of blocks to be created
// for each block pool size.

#define SIR_BUF_BLK_32_NUM           64
#define SIR_BUF_BLK_64_NUM           128
#define SIR_BUF_BLK_96_NUM           16
#define SIR_BUF_BLK_128_NUM          128
#define SIR_BUF_BLK_160_NUM          8
#define SIR_BUF_BLK_192_NUM          0
#define SIR_BUF_BLK_224_NUM          0
#define SIR_BUF_BLK_256_NUM          128
#define SIR_BUF_BLK_512_NUM          0
#define SIR_BUF_BLK_768_NUM          0
#define SIR_BUF_BLK_1024_NUM         2
#define SIR_BUF_BLK_1280_NUM         0
#define SIR_BUF_BLK_1536_NUM         2
#define SIR_BUF_BLK_1792_NUM         0
#define SIR_BUF_BLK_2048_NUM         2
#define SIR_BUF_BLK_2304_NUM         0

/* ******************************************* *
 *                                             *
 *         SIRIUS MESSAGE TYPES                *
 *                                             *
 * ******************************************* */


/*
 * The following message types have bounds defined for each module for
 * inter thread/module communications.
 * Each module will get 256 message types in total.
 * Note that message type definitions for mailbox messages for
 * communication with Host are in wniApi.h file.
 *
 * Any addition/deletion to this message list should also be
 * reflected in the halUtil_getMsgString() routine.
 */

// HAL message types
#define SIR_HAL_MSG_TYPES_BEGIN            (SIR_HAL_MODULE_ID << 8)
#define SIR_HAL_ITC_MSG_TYPES_BEGIN        (SIR_HAL_MSG_TYPES_BEGIN+0x20)
#define SIR_HAL_RADAR_DETECTED_IND         SIR_HAL_ITC_MSG_TYPES_BEGIN
#define SIR_HAL_WDT_KAM_RSP                (SIR_HAL_ITC_MSG_TYPES_BEGIN + 1)
#define SIR_HAL_TIMER_TEMP_MEAS_REQ        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 2)
#define SIR_HAL_TIMER_PERIODIC_STATS_COLLECT_REQ   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 3)
#define SIR_HAL_CAL_REQ_NTF                (SIR_HAL_ITC_MSG_TYPES_BEGIN + 4)
#define SIR_HAL_MNT_OPEN_TPC_TEMP_MEAS_REQ (SIR_HAL_ITC_MSG_TYPES_BEGIN + 5)
#define SIR_HAL_CCA_MONITOR_INTERVAL_TO    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 6)
#define SIR_HAL_CCA_MONITOR_DURATION_TO    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 7)
#define SIR_HAL_CCA_MONITOR_START          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 8)
#define SIR_HAL_CCA_MONITOR_STOP           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 9)
#define SIR_HAL_CCA_CHANGE_MODE            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 10)
#define SIR_HAL_TIMER_WRAP_AROUND_STATS_COLLECT_REQ   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 11)

/*
 * New Taurus related messages
 */
#define SIR_HAL_ADD_STA_REQ                (SIR_HAL_ITC_MSG_TYPES_BEGIN + 13)
#define SIR_HAL_ADD_STA_RSP                (SIR_HAL_ITC_MSG_TYPES_BEGIN + 14)
#define SIR_HAL_DELETE_STA_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 15)
#define SIR_HAL_DELETE_STA_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 16)
#define SIR_HAL_ADD_BSS_REQ                (SIR_HAL_ITC_MSG_TYPES_BEGIN + 17)
#define SIR_HAL_ADD_BSS_RSP                (SIR_HAL_ITC_MSG_TYPES_BEGIN + 18)
#define SIR_HAL_DELETE_BSS_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 19)
#define SIR_HAL_DELETE_BSS_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 20)
#define SIR_HAL_INIT_SCAN_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 21)
#define SIR_HAL_INIT_SCAN_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 22)
#define SIR_HAL_START_SCAN_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 23)
#define SIR_HAL_START_SCAN_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 24)
#define SIR_HAL_END_SCAN_REQ               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 25)
#define SIR_HAL_END_SCAN_RSP               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 26)
#define SIR_HAL_FINISH_SCAN_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 27)
#define SIR_HAL_FINISH_SCAN_RSP            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 28)
#define SIR_HAL_SEND_BEACON_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 29)
#define SIR_HAL_SEND_BEACON_RSP            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 30)

#define SIR_HAL_INIT_CFG_REQ               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 31)
#define SIR_HAL_INIT_CFG_RSP               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 32)

#define SIR_HAL_INIT_WM_CFG_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 33)
#define SIR_HAL_INIT_WM_CFG_RSP            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 34)

#define SIR_HAL_SET_BSSKEY_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 35)
#define SIR_HAL_SET_BSSKEY_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 36)
#define SIR_HAL_SET_STAKEY_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 37)
#define SIR_HAL_SET_STAKEY_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 38)
#define SIR_HAL_DPU_STATS_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 39)
#define SIR_HAL_DPU_STATS_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 40)
#define SIR_HAL_GET_DPUINFO_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 41)
#define SIR_HAL_GET_DPUINFO_RSP            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 42)

#define SIR_HAL_UPDATE_EDCA_PROFILE_IND    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 43)

#define SIR_HAL_UPDATE_STARATEINFO_REQ     (SIR_HAL_ITC_MSG_TYPES_BEGIN + 45)
#define SIR_HAL_UPDATE_STARATEINFO_RSP     (SIR_HAL_ITC_MSG_TYPES_BEGIN + 46)

#define SIR_HAL_UPDATE_BEACON_IND          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 47)
#define SIR_HAL_UPDATE_CF_IND              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 48)
#define SIR_HAL_CHNL_SWITCH_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 49)
#define SIR_HAL_ADD_TS_REQ                 (SIR_HAL_ITC_MSG_TYPES_BEGIN + 50)
#define SIR_HAL_DEL_TS_REQ                 (SIR_HAL_ITC_MSG_TYPES_BEGIN + 51)
#define SIR_HAL_SOFTMAC_TXSTAT_REPORT      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 52)

#define SIR_HAL_MBOX_SENDMSG_COMPLETE_IND  (SIR_HAL_ITC_MSG_TYPES_BEGIN + 61)
#define SIR_HAL_EXIT_BMPS_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 53)
#define SIR_HAL_EXIT_BMPS_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 54)
#define SIR_HAL_EXIT_BMPS_IND              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 55)
#define SIR_HAL_ENTER_BMPS_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 56)
#define SIR_HAL_ENTER_BMPS_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 57)
#define SIR_HAL_BMPS_STATUS_IND            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 58)
#define SIR_HAL_MISSED_BEACON_IND          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 59)

#define SIR_HAL_SWITCH_CHANNEL_RSP         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 60)
#define SIR_HAL_PWR_SAVE_CFG               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 62)

#define SIR_HAL_REGISTER_PE_CALLBACK       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 63)
#define SIR_HAL_SOFTMAC_MEM_READREQUEST    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 64)
#define SIR_HAL_SOFTMAC_MEM_WRITEREQUEST   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 65)

#define SIR_HAL_SOFTMAC_MEM_READRESPONSE   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 66)
#define SIR_HAL_SOFTMAC_BULKREGWRITE_CONFIRM      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 67)
#define SIR_HAL_SOFTMAC_BULKREGREAD_RESPONSE      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 68)
#define SIR_HAL_SOFTMAC_HOSTMESG_MSGPROCESSRESULT (SIR_HAL_ITC_MSG_TYPES_BEGIN + 69)

#define SIR_HAL_ADDBA_REQ                  (SIR_HAL_ITC_MSG_TYPES_BEGIN + 70)
#define SIR_HAL_ADDBA_RSP                  (SIR_HAL_ITC_MSG_TYPES_BEGIN + 71)
#define SIR_HAL_DELBA_IND                  (SIR_HAL_ITC_MSG_TYPES_BEGIN + 72)
#define SIR_HAL_DEL_BA_IND                 (SIR_HAL_ITC_MSG_TYPES_BEGIN + 73)

//message from sme to initiate delete block ack session.
#define SIR_HAL_DELBA_REQ                  (SIR_HAL_ITC_MSG_TYPES_BEGIN + 74)
#define SIR_HAL_IBSS_STA_ADD               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 75)
#define SIR_HAL_TIMER_ADJUST_ADAPTIVE_THRESHOLD_IND   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 76)
#define SIR_HAL_SET_LINK_STATE             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 77)
#define SIR_HAL_ENTER_IMPS_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 78)
#define SIR_HAL_ENTER_IMPS_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 79)
#define SIR_HAL_EXIT_IMPS_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 80)
#define SIR_HAL_EXIT_IMPS_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 81)
#define SIR_HAL_SOFTMAC_HOSTMESG_PS_STATUS_IND  (SIR_HAL_ITC_MSG_TYPES_BEGIN + 82)
#define SIR_HAL_POSTPONE_ENTER_IMPS_RSP    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 83)
#define SIR_HAL_STA_STAT_REQ               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 84)
#define SIR_HAL_GLOBAL_STAT_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 85)
#define SIR_HAL_AGGR_STAT_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 86)
#define SIR_HAL_STA_STAT_RSP               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 87)
#define SIR_HAL_GLOBAL_STAT_RSP            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 88)
#define SIR_HAL_AGGR_STAT_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 89)
#define SIR_HAL_STAT_SUMM_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 90)
#define SIR_HAL_STAT_SUMM_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 92)
#define SIR_HAL_REMOVE_BSSKEY_REQ          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 93)
#define SIR_HAL_REMOVE_BSSKEY_RSP          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 94)
#define SIR_HAL_REMOVE_STAKEY_REQ          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 95)
#define SIR_HAL_REMOVE_STAKEY_RSP          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 96)
#define SIR_HAL_SET_STA_BCASTKEY_REQ       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 97)
#define SIR_HAL_SET_STA_BCASTKEY_RSP       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 98)
#define SIR_HAL_REMOVE_STA_BCASTKEY_REQ    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 99)
#define SIR_HAL_REMOVE_STA_BCASTKEY_RSP    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 100)
#define SIR_HAL_ADD_TS_RSP                 (SIR_HAL_ITC_MSG_TYPES_BEGIN + 101)
#define SIR_HAL_DPU_MIC_ERROR              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 102)
#define SIR_HAL_TIMER_BA_ACTIVITY_REQ      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 103)
#define SIR_HAL_TIMER_CHIP_MONITOR_TIMEOUT (SIR_HAL_ITC_MSG_TYPES_BEGIN + 104)
#define SIR_HAL_TIMER_TRAFFIC_ACTIVITY_REQ (SIR_HAL_ITC_MSG_TYPES_BEGIN + 105)
#define SIR_HAL_TIMER_ADC_RSSI_STATS       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 106)
#define SIR_HAL_MIC_FAILURE_IND            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 107)
#define SIR_HAL_UPDATE_UAPSD_IND           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 108)
#define SIR_HAL_SET_MIMOPS_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 109)
#define SIR_HAL_SET_MIMOPS_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 110)
#define SIR_HAL_SYS_READY_IND              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 111)
#define SIR_HAL_SET_TX_POWER_REQ           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 112)
#define SIR_HAL_SET_TX_POWER_RSP           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 113)
#define SIR_HAL_GET_TX_POWER_REQ           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 114)
#define SIR_HAL_GET_TX_POWER_RSP           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 115)
#define SIR_HAL_GET_NOISE_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 116)
#define SIR_HAL_GET_NOISE_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 117)

/* Messages to support transmit_halt and transmit_resume */
#define SIR_HAL_TRANSMISSION_CONTROL_IND   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 118)
/* Indication from LIM to HAL to Initialize radar interrupt */
#define SIR_HAL_INIT_RADAR_IND             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 119)

#define SIR_HAL_BEACON_PRE_IND             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 120)
#define SIR_HAL_ENTER_UAPSD_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 121)
#define SIR_HAL_ENTER_UAPSD_RSP            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 122)
#define SIR_HAL_EXIT_UAPSD_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 123)
#define SIR_HAL_EXIT_UAPSD_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 124)
#define SIR_HAL_LOW_RSSI_IND               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 125)
#define SIR_HAL_BEACON_FILTER_IND          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 126)
/// PE <-> HAL WOWL messages
#define SIR_HAL_WOWL_ADD_BCAST_PTRN        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 127)
#define SIR_HAL_WOWL_DEL_BCAST_PTRN        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 128)
#define SIR_HAL_WOWL_ENTER_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 129)
#define SIR_HAL_WOWL_ENTER_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 130)
#define SIR_HAL_WOWL_EXIT_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 131)
#define SIR_HAL_WOWL_EXIT_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 132)
#define SIR_HAL_TX_COMPLETE_IND            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 133)
#define SIR_HAL_TIMER_RA_COLLECT_AND_ADAPT (SIR_HAL_ITC_MSG_TYPES_BEGIN + 134)
/// PE <-> HAL statistics messages
#define SIR_HAL_GET_STATISTICS_REQ         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 135)
#define SIR_HAL_GET_STATISTICS_RSP         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 136)
#define SIR_HAL_SET_KEY_DONE               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 137)

/// PE <-> HAL BTC messages
#define SIR_HAL_BTC_SET_CFG                (SIR_HAL_ITC_MSG_TYPES_BEGIN + 138)
#define SIR_HAL_SIGNAL_BT_EVENT            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 139)
#define SIR_HAL_HANDLE_FW_MBOX_RSP            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 140)
#define SIR_HAL_UPDATE_PROBE_RSP_TEMPLATE_IND     (SIR_HAL_ITC_MSG_TYPES_BEGIN + 141)

/* PE <-> HAL addr2 mismatch message */
#define SIR_LIM_ADDR2_MISS_IND             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 142)
#ifdef FEATURE_OEM_DATA_SUPPORT
/* PE <-> HAL OEM_DATA RELATED MESSAGES */
#define SIR_HAL_START_OEM_DATA_REQ         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 143)
#define SIR_HAL_START_OEM_DATA_RSP       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 144)
#define SIR_HAL_FINISH_OEM_DATA_REQ      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 145)
#endif

#define SIR_HAL_SET_MAX_TX_POWER_REQ       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 146)
#define SIR_HAL_SET_MAX_TX_POWER_RSP       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 147)

#define SIR_HAL_SEND_MSG_COMPLETE          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 148)

/// PE <-> HAL Host Offload message
#define SIR_HAL_SET_HOST_OFFLOAD           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 149)

#define SIR_HAL_ADD_STA_SELF_REQ           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 150)
#define SIR_HAL_ADD_STA_SELF_RSP           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 151)
#define SIR_HAL_DEL_STA_SELF_REQ           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 152)
#define SIR_HAL_DEL_STA_SELF_RSP           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 153)
#define SIR_HAL_SIGNAL_BTAMP_EVENT         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 154)

#define SIR_HAL_CFG_RXP_FILTER_REQ         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 155)
#define SIR_HAL_CFG_RXP_FILTER_RSP         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 156)

#ifdef WLAN_FEATURE_VOWIFI_11R
#define SIR_HAL_AGGR_ADD_TS_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 157)
#define SIR_HAL_AGGR_ADD_TS_RSP            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 158)
#define SIR_HAL_AGGR_QOS_REQ               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 159)
#define SIR_HAL_AGGR_QOS_RSP               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 160)
#endif /* WLAN_FEATURE_VOWIFI_11R */

/* P2P <-> HAL P2P msg */
#define SIR_HAL_SET_P2P_GO_NOA_REQ         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 161)
#define SIR_HAL_P2P_NOA_ATTR_IND           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 162)
#define SIR_HAL_P2P_NOA_START_IND          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 163)

#define SIR_HAL_SET_LINK_STATE_RSP             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 165)


#define SIR_HAL_WLAN_SUSPEND_IND               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 166)
#define SIR_HAL_WLAN_RESUME_REQ                (SIR_HAL_ITC_MSG_TYPES_BEGIN + 167)

/// PE <-> HAL Keep Alive message
#define SIR_HAL_SET_KEEP_ALIVE             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 168)

#ifdef WLAN_NS_OFFLOAD
#define SIR_HAL_SET_NS_OFFLOAD             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 169)
#endif //WLAN_NS_OFFLOAD

#ifdef FEATURE_WLAN_SCAN_PNO
#define SIR_HAL_SET_PNO_REQ                (SIR_HAL_ITC_MSG_TYPES_BEGIN + 170)
#define SIR_HAL_SET_PNO_CHANGED_IND        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 171)
#define SIR_HAL_UPDATE_SCAN_PARAMS         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 172)
#define SIR_HAL_SET_RSSI_FILTER_REQ        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 173)
#endif // FEATURE_WLAN_SCAN_PNO


#define SIR_HAL_SET_TX_PER_TRACKING_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 174)

#ifdef WLAN_FEATURE_PACKET_FILTERING
#define SIR_HAL_8023_MULTICAST_LIST_REQ                     (SIR_HAL_ITC_MSG_TYPES_BEGIN + 175)
#define SIR_HAL_RECEIVE_FILTER_SET_FILTER_REQ                 (SIR_HAL_ITC_MSG_TYPES_BEGIN + 176)
#define SIR_HAL_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 177)
#define SIR_HAL_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 178)
#define SIR_HAL_RECEIVE_FILTER_CLEAR_FILTER_REQ             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 179)
#endif // WLAN_FEATURE_PACKET_FILTERING

#define SIR_HAL_SET_POWER_PARAMS_REQ (SIR_HAL_ITC_MSG_TYPES_BEGIN + 180)

#ifdef WLAN_FEATURE_GTK_OFFLOAD
#define SIR_HAL_GTK_OFFLOAD_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 181)
#define SIR_HAL_GTK_OFFLOAD_GETINFO_REQ    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 182)
#define SIR_HAL_GTK_OFFLOAD_GETINFO_RSP    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 183)
#endif //WLAN_FEATURE_GTK_OFFLOAD

#ifdef FEATURE_WLAN_ESE
#define SIR_HAL_TSM_STATS_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 184)
#define SIR_HAL_TSM_STATS_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 185)
#endif


#ifdef WLAN_WAKEUP_EVENTS
#define SIR_HAL_WAKE_REASON_IND            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 186)
#endif //WLAN_WAKEUP_EVENTS

#define SIR_HAL_SET_TM_LEVEL_REQ           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 187)

#ifdef WLAN_FEATURE_11AC
#define SIR_HAL_UPDATE_OP_MODE             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 188)
#endif

#ifdef FEATURE_WLAN_TDLS
/// PE <-> HAL TDLS messages
#define SIR_HAL_TDLS_LINK_ESTABLISH        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 189)
#define SIR_HAL_TDLS_LINK_TEARDOWN         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 190)
#endif
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
#define SIR_HAL_ROAM_SCAN_OFFLOAD_REQ (SIR_HAL_ITC_MSG_TYPES_BEGIN + 191)
#define SIR_HAL_ROAM_SCAN_OFFLOAD_RSP      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 192)
#endif
#define SIR_HAL_GET_ROAM_RSSI_REQ          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 193)
#define SIR_HAL_GET_ROAM_RSSI_RSP          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 194)

#define SIR_HAL_TRAFFIC_STATS_IND          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 195)

#ifdef WLAN_FEATURE_11W
#define SIR_HAL_EXCLUDE_UNENCRYPTED_IND    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 196)
#endif
#ifdef FEATURE_WLAN_TDLS
/// PE <-> HAL TDLS messages
#define SIR_HAL_TDLS_LINK_ESTABLISH_REQ (SIR_HAL_ITC_MSG_TYPES_BEGIN + 197)
#define SIR_HAL_TDLS_LINK_ESTABLISH_REQ_RSP (SIR_HAL_ITC_MSG_TYPES_BEGIN + 198)
#define SIR_HAL_TDLS_IND (SIR_HAL_ITC_MSG_TYPES_BEGIN + 199)
#endif

#define SIR_HAL_UPDATE_CHAN_LIST_RSP       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 200)
#define SIR_HAL_STOP_SCAN_OFFLOAD_REQ      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 201)
#define SIR_HAL_STOP_SCAN_OFFLOAD_RSP      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 202)
#define SIR_HAL_RX_SCAN_EVENT              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 203)
#define SIR_HAL_DHCP_START_IND             (SIR_HAL_ITC_MSG_TYPES_BEGIN + 204)
#define SIR_HAL_DHCP_STOP_IND              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 205)
#define SIR_HAL_IBSS_PEER_INACTIVITY_IND   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 206)

#define SIR_HAL_LPHB_CONF_IND              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 206)
#define SIR_HAL_LPHB_WAIT_EXPIRE_IND       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 207)

#define SIR_HAL_ADD_PERIODIC_TX_PTRN_IND   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 208)
#define SIR_HAL_DEL_PERIODIC_TX_PTRN_IND   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 209)

#ifdef FEATURE_WLAN_BATCH_SCAN
#define SIR_HAL_SET_BATCH_SCAN_REQ         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 210)
#define SIR_HAL_SET_BATCH_SCAN_RSP         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 211)
#define SIR_HAL_STOP_BATCH_SCAN_IND        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 212)
#define SIR_HAL_TRIGGER_BATCH_SCAN_RESULT_IND (SIR_HAL_ITC_MSG_TYPES_BEGIN + 213)
#endif

#define SIR_HAL_RATE_UPDATE_IND            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 217)
#define SIR_HAL_START_SCAN_OFFLOAD_REQ     (SIR_HAL_ITC_MSG_TYPES_BEGIN + 218)
#define SIR_HAL_START_SCAN_OFFLOAD_RSP     (SIR_HAL_ITC_MSG_TYPES_BEGIN + 219)
#define SIR_HAL_UPDATE_CHAN_LIST_REQ       (SIR_HAL_ITC_MSG_TYPES_BEGIN + 220)

#define SIR_HAL_SET_MAX_TX_POWER_PER_BAND_REQ \
        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 221)
#define SIR_HAL_SET_MAX_TX_POWER_PER_BAND_RSP \
        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 222)


/* OBSS Scan start Indication to FW*/
#define SIR_HAL_HT40_OBSS_SCAN_IND      (SIR_HAL_ITC_MSG_TYPES_BEGIN +227)
/* OBSS Scan stop Indication to FW*/
#define SIR_HAL_HT40_OBSS_STOP_SCAN_IND (SIR_HAL_ITC_MSG_TYPES_BEGIN +228)

#define SIR_HAL_BCN_MISS_RATE_REQ         (SIR_HAL_ITC_MSG_TYPES_BEGIN + 229)

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
#define SIR_HAL_LL_STATS_CLEAR_REQ      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 232)
#define SIR_HAL_LL_STATS_SET_REQ        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 233)
#define SIR_HAL_LL_STATS_GET_REQ        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 234)
#define SIR_HAL_LL_STATS_RESULTS_RSP    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 235)
#endif

#ifdef WLAN_FEATURE_EXTSCAN
#define SIR_HAL_EXTSCAN_GET_CAPABILITIES_REQ   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 236)
#define SIR_HAL_EXTSCAN_GET_CAPABILITIES_RSP   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 237)
#define SIR_HAL_EXTSCAN_START_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 238)
#define SIR_HAL_EXTSCAN_START_RSP              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 239)
#define SIR_HAL_EXTSCAN_STOP_REQ               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 240)
#define SIR_HAL_EXTSCAN_STOP_RSP               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 241)
#define SIR_HAL_EXTSCAN_SET_BSS_HOTLIST_REQ    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 242)
#define SIR_HAL_EXTSCAN_SET_BSS_HOTLIST_RSP    (SIR_HAL_ITC_MSG_TYPES_BEGIN + 243)
#define SIR_HAL_EXTSCAN_RESET_BSS_HOTLIST_REQ  (SIR_HAL_ITC_MSG_TYPES_BEGIN + 244)
#define SIR_HAL_EXTSCAN_RESET_BSS_HOTLIST_RSP  (SIR_HAL_ITC_MSG_TYPES_BEGIN + 245)
#define SIR_HAL_EXTSCAN_SET_SIGNF_RSSI_CHANGE_REQ   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 246)
#define SIR_HAL_EXTSCAN_SET_SIGNF_RSSI_CHANGE_RSP   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 247)
#define SIR_HAL_EXTSCAN_RESET_SIGNF_RSSI_CHANGE_REQ (SIR_HAL_ITC_MSG_TYPES_BEGIN + 248)
#define SIR_HAL_EXTSCAN_RESET_SIGNF_RSSI_CHANGE_RSP (SIR_HAL_ITC_MSG_TYPES_BEGIN + 249)
#define SIR_HAL_EXTSCAN_GET_CACHED_RESULTS_REQ (SIR_HAL_ITC_MSG_TYPES_BEGIN + 250)
#define SIR_HAL_EXTSCAN_GET_CACHED_RESULTS_RSP (SIR_HAL_ITC_MSG_TYPES_BEGIN + 251)

#define SIR_HAL_EXTSCAN_PROGRESS_IND           (SIR_HAL_ITC_MSG_TYPES_BEGIN + 252)
#define SIR_HAL_EXTSCAN_SCAN_AVAILABLE_IND     (SIR_HAL_ITC_MSG_TYPES_BEGIN + 253)
#define SIR_HAL_EXTSCAN_SCAN_RESULT_IND        (SIR_HAL_ITC_MSG_TYPES_BEGIN + 254)
#define SIR_HAL_EXTSCAN_HOTLIST_MATCH_IND      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 255)
#define SIR_HAL_EXTSCAN_SIGNF_WIFI_CHANGE_IND  (SIR_HAL_ITC_MSG_TYPES_BEGIN + 256)
#define SIR_HAL_EXTSCAN_FULL_SCAN_RESULT_IND   (SIR_HAL_ITC_MSG_TYPES_BEGIN + 257)

#endif /* WLAN_FEATURE_EXTSCAN */

#ifdef FEATURE_WLAN_TDLS
/// PE <-> HAL TDLS messages
// tdlsoffchan
#define SIR_HAL_TDLS_CHAN_SWITCH_REQ          (SIR_HAL_ITC_MSG_TYPES_BEGIN + 258)
#define SIR_HAL_TDLS_CHAN_SWITCH_REQ_RSP      (SIR_HAL_ITC_MSG_TYPES_BEGIN + 259)
#endif
#define SIR_HAL_SPOOF_MAC_ADDR_REQ            (SIR_HAL_ITC_MSG_TYPES_BEGIN + 260)

#define SIR_HAL_FW_STATS_GET_REQ              (SIR_HAL_ITC_MSG_TYPES_BEGIN + 262)

#define SIR_HAL_ENCRYPT_MSG_REQ               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 263)
#define SIR_HAL_ENCRYPT_MSG_RSP               (SIR_HAL_ITC_MSG_TYPES_BEGIN + 264)

#define SIR_HAL_MSG_TYPES_END              (SIR_HAL_MSG_TYPES_BEGIN + 0x1FF)
// CFG message types
#define SIR_CFG_MSG_TYPES_BEGIN        (SIR_CFG_MODULE_ID << 8)
#define SIR_CFG_ITC_MSG_TYPES_BEGIN    (SIR_CFG_MSG_TYPES_BEGIN+0xB0)
#define SIR_CFG_PARAM_UPDATE_IND       (SIR_CFG_ITC_MSG_TYPES_BEGIN)
#define SIR_CFG_DOWNLOAD_COMPLETE_IND  (SIR_CFG_ITC_MSG_TYPES_BEGIN + 1)
#define SIR_CFG_MSG_TYPES_END          (SIR_CFG_MSG_TYPES_BEGIN+0xFF)

// LIM message types
#define SIR_LIM_MSG_TYPES_BEGIN        (SIR_LIM_MODULE_ID << 8)
#define SIR_LIM_ITC_MSG_TYPES_BEGIN    (SIR_LIM_MSG_TYPES_BEGIN+0xB0)

// Messages to/from HAL
// Removed as part of moving HAL down to FW

// Message from ISR upon TFP retry interrupt
#define SIR_LIM_RETRY_INTERRUPT_MSG        (SIR_LIM_ITC_MSG_TYPES_BEGIN + 3)
// Message from BB Transport
#define SIR_BB_XPORT_MGMT_MSG              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 4)
// UNUSED                                  SIR_LIM_ITC_MSG_TYPES_BEGIN + 6
// Message from ISR upon SP's Invalid session key interrupt
#define SIR_LIM_INV_KEY_INTERRUPT_MSG      (SIR_LIM_ITC_MSG_TYPES_BEGIN + 7)
// Message from ISR upon SP's Invalid key ID interrupt
#define SIR_LIM_KEY_ID_INTERRUPT_MSG       (SIR_LIM_ITC_MSG_TYPES_BEGIN + 8)
// Message from ISR upon SP's Replay threshold reached interrupt
#define SIR_LIM_REPLAY_THRES_INTERRUPT_MSG (SIR_LIM_ITC_MSG_TYPES_BEGIN + 9)
// Message from HDD after the TD dummy packet is cleaned up
#define SIR_LIM_TD_DUMMY_CALLBACK_MSG      (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xA)
// Message from SCH when the STA is ready to be deleted
#define SIR_LIM_SCH_CLEAN_MSG              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xB)
// Message from ISR upon Radar Detection
#define SIR_LIM_RADAR_DETECT_IND           (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xC)

/////////////////////////////////////
// message id Available
////////////////////////////////////


// Message from Hal to send out a DEL-TS indication
#define SIR_LIM_DEL_TS_IND                  (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xE)
//Message from HAL to send BA global timer timeout
#define SIR_LIM_ADD_BA_IND                  (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xF)
//Indication from HAL to delete all the BA sessions when the BA activity check timer is disabled
#define SIR_LIM_DEL_BA_ALL_IND                  (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0x10)
//Indication from HAL to delete Station context
#define SIR_LIM_DELETE_STA_CONTEXT_IND          (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0x11)
//Indication from HAL to delete BA
#define SIR_LIM_DEL_BA_IND                      (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0x12)
#define SIR_LIM_UPDATE_BEACON                   (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0x13)


// LIM Timeout messages
#define SIR_LIM_TIMEOUT_MSG_START      ((SIR_LIM_MODULE_ID  << 8) + 0xD0)
#define SIR_LIM_MIN_CHANNEL_TIMEOUT    SIR_LIM_TIMEOUT_MSG_START
#define SIR_LIM_MAX_CHANNEL_TIMEOUT    (SIR_LIM_TIMEOUT_MSG_START + 1)
#define SIR_LIM_JOIN_FAIL_TIMEOUT      (SIR_LIM_TIMEOUT_MSG_START + 2)
#define SIR_LIM_AUTH_FAIL_TIMEOUT      (SIR_LIM_TIMEOUT_MSG_START + 3)
#define SIR_LIM_AUTH_RSP_TIMEOUT       (SIR_LIM_TIMEOUT_MSG_START + 4)
#define SIR_LIM_ASSOC_FAIL_TIMEOUT     (SIR_LIM_TIMEOUT_MSG_START + 5)
#define SIR_LIM_REASSOC_FAIL_TIMEOUT   (SIR_LIM_TIMEOUT_MSG_START + 6)
#define SIR_LIM_HEART_BEAT_TIMEOUT     (SIR_LIM_TIMEOUT_MSG_START + 7)
// currently unused                    SIR_LIM_TIMEOUT_MSG_START + 0x8
// Link Monitoring Messages
#define SIR_LIM_CHANNEL_SCAN_TIMEOUT     (SIR_LIM_TIMEOUT_MSG_START + 0xA)
#define SIR_LIM_PROBE_HB_FAILURE_TIMEOUT (SIR_LIM_TIMEOUT_MSG_START + 0xB)
#define SIR_LIM_ADDTS_RSP_TIMEOUT        (SIR_LIM_TIMEOUT_MSG_START + 0xC)
#define SIR_LIM_LINK_TEST_DURATION_TIMEOUT (SIR_LIM_TIMEOUT_MSG_START + 0x13)
#define SIR_LIM_CNF_WAIT_TIMEOUT         (SIR_LIM_TIMEOUT_MSG_START + 0x17)
#define SIR_LIM_KEEPALIVE_TIMEOUT        (SIR_LIM_TIMEOUT_MSG_START + 0x18)
#define SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT (SIR_LIM_TIMEOUT_MSG_START + 0x19)
#define SIR_LIM_CHANNEL_SWITCH_TIMEOUT   (SIR_LIM_TIMEOUT_MSG_START + 0x1A)
#define SIR_LIM_QUIET_TIMEOUT            (SIR_LIM_TIMEOUT_MSG_START + 0x1B)
#define SIR_LIM_QUIET_BSS_TIMEOUT        (SIR_LIM_TIMEOUT_MSG_START + 0x1C)

#define SIR_LIM_WPS_OVERLAP_TIMEOUT      (SIR_LIM_TIMEOUT_MSG_START + 0x1D)
#ifdef WLAN_FEATURE_VOWIFI_11R
#define SIR_LIM_FT_PREAUTH_RSP_TIMEOUT   (SIR_LIM_TIMEOUT_MSG_START + 0x1E)
#endif
#define SIR_LIM_REMAIN_CHN_TIMEOUT       (SIR_LIM_TIMEOUT_MSG_START + 0x1F)
#define SIR_LIM_INSERT_SINGLESHOT_NOA_TIMEOUT   (SIR_LIM_TIMEOUT_MSG_START + 0x20)

#ifdef WMM_APSD
#define SIR_LIM_WMM_APSD_SP_START_MSG_TYPE (SIR_LIM_TIMEOUT_MSG_START + 0x21)
#define SIR_LIM_WMM_APSD_SP_END_MSG_TYPE (SIR_LIM_TIMEOUT_MSG_START + 0x22)
#endif
#define SIR_LIM_BEACON_GEN_IND          (SIR_LIM_TIMEOUT_MSG_START + 0x23)
#define SIR_LIM_PERIODIC_PROBE_REQ_TIMEOUT    (SIR_LIM_TIMEOUT_MSG_START + 0x24)

#define SIR_LIM_ESE_TSM_TIMEOUT        (SIR_LIM_TIMEOUT_MSG_START + 0x25)

#define SIR_LIM_DISASSOC_ACK_TIMEOUT       (SIR_LIM_TIMEOUT_MSG_START + 0x26)
#define SIR_LIM_DEAUTH_ACK_TIMEOUT       (SIR_LIM_TIMEOUT_MSG_START + 0x27)
#define SIR_LIM_PERIODIC_JOIN_PROBE_REQ_TIMEOUT (SIR_LIM_TIMEOUT_MSG_START + 0x28)

#ifdef FEATURE_WLAN_TDLS_INTERNAL
#define SIR_LIM_TDLS_DISCOVERY_RSP_WAIT     (SIR_LIM_TIMEOUT_MSG_START + 0x29)
#define SIR_LIM_TDLS_LINK_SETUP_RSP_TIMEOUT (SIR_LIM_TIMEOUT_MSG_START + 0x2A)
#define SIR_LIM_TDLS_LINK_SETUP_CNF_TIMEOUT (SIR_LIM_TIMEOUT_MSG_START + 0x2B)
#endif
#define SIR_LIM_CONVERT_ACTIVE_CHANNEL_TO_PASSIVE (SIR_LIM_TIMEOUT_MSG_START + 0x2C)
#define SIR_LIM_MSG_TYPES_END            (SIR_LIM_MSG_TYPES_BEGIN+0xFF)

// SCH message types
#define SIR_SCH_MSG_TYPES_BEGIN        (SIR_SCH_MODULE_ID << 8)
#define SIR_SCH_CHANNEL_SWITCH_REQUEST (SIR_SCH_MSG_TYPES_BEGIN)
#define SIR_SCH_START_SCAN_REQ         (SIR_SCH_MSG_TYPES_BEGIN + 1)
#define SIR_SCH_START_SCAN_RSP         (SIR_SCH_MSG_TYPES_BEGIN + 2)
#define SIR_SCH_END_SCAN_NTF           (SIR_SCH_MSG_TYPES_BEGIN + 3)
#define SIR_SCH_MSG_TYPES_END          (SIR_SCH_MSG_TYPES_BEGIN+0xFF)

// PMM message types
#define SIR_PMM_MSG_TYPES_BEGIN        (SIR_PMM_MODULE_ID << 8)
#define SIR_PMM_CHANGE_PM_MODE         (SIR_PMM_MSG_TYPES_BEGIN)
#define SIR_PMM_CHANGE_IMPS_MODE       (SIR_PMM_MSG_TYPES_BEGIN + 1)        //for Idle mode power save
#define SIR_PMM_MSG_TYPES_END          (SIR_PMM_MSG_TYPES_BEGIN+0xFF)

// MNT message types
#define SIR_MNT_MSG_TYPES_BEGIN        (SIR_MNT_MODULE_ID << 8)
#define SIR_MNT_RELEASE_BD             (SIR_MNT_MSG_TYPES_BEGIN + 0)
#define SIR_MNT_MSG_TYPES_END          (SIR_MNT_MSG_TYPES_BEGIN + 0xFF)

// DVT message types
#define SIR_DVT_MSG_TYPES_BEGIN        (SIR_DVT_MODULE_ID << 8)
#define SIR_DVT_ITC_MSG_TYPES_BEGIN    (SIR_DVT_MSG_TYPES_BEGIN+0x0F)
#define SIR_DVT_MSG_TYPES_END          (SIR_DVT_ITC_MSG_TYPES_BEGIN+0xFFF)


//PTT message types
#define SIR_PTT_MSG_TYPES_BEGIN            0x3000
#define SIR_PTT_MSG_TYPES_END              0x3300


/* ****************************************** *
 *                                            *
 *         EVENT TYPE Defintions              *
 *                                            *
 * ****************************************** */

// MMH Events that are used in other modules to post events to MMH
# define SIR_HAL_MMH_TXMB_READY_EVT     0x00000002
# define SIR_HAL_MMH_RXMB_DONE_EVT      0x00000004
# define SIR_HAL_MMH_MSGQ_NE_EVT        0x00000008

# define SIR_HSTEMUL_TXMB_DONE_EVT         0x00000100
# define SIR_HSTEMUL_RXMB_READY_EVT        0x00000200
# define SIR_HSTEMUL_MSGQ_NE_EVT           0x00000400

# define SIR_TST_XMIT_MSG_QS_EMPTY_EVT     0x00000080

//added for OBSS

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



#endif
