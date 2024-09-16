
/*! \file   wsys_cmd_handler.h
*/

/*****************************************************************************
* Copyright (c) 2017 MediaTek Inc.
*
* All rights reserved. Copying, compilation, modification, distribution
* or any other use whatsoever of this material is strictly prohibited
* except in accordance with a Software License Agreement with
* MediaTek Inc.
******************************************************************************
*/

/*****************************************************************************
* LEGAL DISCLAIMER
*
* BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND
* AGREES THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK
* SOFTWARE") RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE
* PROVIDED TO BUYER ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY
* DISCLAIMS ANY AND ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT
* LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE OR NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE
* ANY WARRANTY WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTY
* WHICH MAY BE USED BY, INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK
* SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY
* WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE
* FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S SPECIFICATION OR TO
* CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
* BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
* LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL
* BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT
* ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY
* BUYER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
* THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
* WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT
* OF LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING
* THEREOF AND RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN
* FRANCISCO, CA, UNDER THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE
* (ICC).
******************************************************************************
*/

#ifndef _WSYS_CMD_HANDLER_FW_H
#define _WSYS_CMD_HANDLER_FW_H

/*****************************************************************************
*                         C O M P I L E R   F L A G S
******************************************************************************
*/

/*****************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
******************************************************************************
*/
#include "mac.h"

/*****************************************************************************
*                              C O N S T A N T S
******************************************************************************
*/
#define EVENT_PACKET_TYPE                       (0xE000)

#define PARAM_MAX_LEN_RATES_EX                  16


#define BKSCAN_CHANNEL_NUM_MAX                  64

#define DISCONNECT_REASON_CODE_RESERVED         0
#define DISCONNECT_REASON_CODE_RADIO_LOST       1
#define DISCONNECT_REASON_CODE_DEAUTHENTICATED  2
#define DISCONNECT_REASON_CODE_DISASSOCIATED    3
#define DISCONNECT_REASON_CODE_NEW_CONNECTION   4


/* Define bandwidth, band, channel and SCO configurations from host command,
 * whose CMD ID is 0x13.
 */
#define CONFIG_BW_20_40M            0
#define CONFIG_BW_20M               1   /* 20MHz only */
#define CONFIG_BW_20_40_80M         2
#define CONFIG_BW_20_40_80_160M     3
#define CONFIG_BW_20_40_80_8080M    4

/* Band definition in CMD EVENT */
#define CMD_BAND_NULL               0
#define CMD_BAND_2G4                1
#define CMD_BAND_5G                 2


/* Action field in structure CMD_CH_PRIVILEGE_T */
#define CMD_CH_ACTION_REQ           0
#define CMD_CH_ACTION_ABORT         1

/* Status field in structure EVENT_CH_PRIVILEGE_T */
#define EVENT_CH_STATUS_GRANT       0
#define EVENT_CH_STATUS_REJECT      1
#define EVENT_CH_STATUS_RECOVER     2
#define EVENT_CH_STATUS_REMOVE_REQ  3

#define WIFI_RESTART_DOWNLOAD_STATUS_NO_ERROR            0
#define WIFI_RESTART_DOWNLOAD_STATUS_ERROR                  1

#define PM_WOWLAN_REQ_START         0x1
#define PM_WOWLAN_REQ_STOP          0x2


#define PACKETF_CAP_TYPE_ARP            BIT(1)
#define PACKETF_CAP_TYPE_MAGIC          BIT(2)
#define PACKETF_CAP_TYPE_BITMAP         BIT(3)
#define PACKETF_CAP_TYPE_EAPOL          BIT(4)
#define PACKETF_CAP_TYPE_TDLS           BIT(5)
#define PACKETF_CAP_TYPE_CF             BIT(6)
#define PACKETF_CAP_TYPE_HEARTBEAT      BIT(7)
#define PACKETF_CAP_TYPE_TCP_SYN        BIT(8)
#define PACKETF_CAP_TYPE_UDP_SYN        BIT(9)
#define PACKETF_CAP_TYPE_BCAST_SYN      BIT(10)
#define PACKETF_CAP_TYPE_MCAST_SYN      BIT(11)
#define PACKETF_CAP_TYPE_V6             BIT(12)
#define PACKETF_CAP_TYPE_TDIM           BIT(13)

/*---------------------------------------------------------------------------
 * Config for scan global controls
 *---------------------------------------------------------------------------
 */

#define PSCAN_MAX_CHANNELS                      8
#define PSCAN_MAX_BUCKETS                       8

#define PSCAN_MAX_SCAN_CACHE_SIZE               8
/*GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE*/
#define PSCAN_MAX_AP_CACHE_PER_SCAN             64
#define PSCAN_MAX_SCAN_REPORTING_THRETHOLD      8
#define PSCAN_MAX_BUFFER_THRETHOLD              80

#define PSCAN_SCAN_HISTORY_MAX_NUM              PSCAN_MAX_AP_CACHE_PER_SCAN
/*Broadcom is 64*/

#define PSCAN_SWC_MAX_NUM                       8 /*PFN_SWC_MAX_NUM_APS*/
#define PSCAN_SWC_RSSI_WIN_MAX                  8 /*PFN_SWC_RSSI_WINDOW_MAX*/
#define PSCAN_SWC_LOSTAP_WIN_MAX                8

/**
 * If there is no  bucket base freq setting from pscan->gscan
 * we will set this value as default pscan bucket base freq
 */
#define PSCAN_PNO_BUCKET_BASE_FREQ              30000

#define PSCAN_HOTLIST_MAX_NUM                   8 /*PFN_HOTLIST_MAX_NUM_APS*/
#define PSCAN_HOTLIST_LOST_AP_WINDOW_DEFAULT    8
/*GSCAN_LOST_AP_WINDOW_DEFAULT*/
#define PSCAN_HOTLIST_REPORT_MAX_NUM            8 /**/

#define SCAN_DONE_EVENT_MAX_CHANNEL_NUM         64
#define SCAN_INFO_CHANNEL_NUM_MAX               64

/* PF TCP/UDP max port number */
#define MAX_TCP_UDP_PORT                        20

#define HE_OP_BYTE_NUM                          3
#define HE_MAC_CAP_BYTE_NUM                     6
#define HE_PHY_CAP_BYTE_NUM                     11

/*****************************************************************************
*                             D A T A   T Y P E S
******************************************************************************
*/

/* Define CMD ID from Host to firmware (v0.07) */
enum ENUM_CMD_ID {
	CMD_ID_DUMMY_RSV            = 0x00, /* 0x00 (Set) */
	CMD_ID_TEST_CTRL            = 0x01, /* 0x01 (Set) */
	CMD_ID_BASIC_CONFIG,                /* 0x02 (Set) */
	CMD_ID_SCAN_REQ_V2,                 /* 0x03 (Set) */
	CMD_ID_NIC_POWER_CTRL,              /* 0x04 (Set) */
	CMD_ID_POWER_SAVE_MODE,             /* 0x05 (Set) */
	CMD_ID_LINK_ATTRIB,                 /* 0x06 (Set) */
	CMD_ID_ADD_REMOVE_KEY,              /* 0x07 (Set) */
	CMD_ID_DEFAULT_KEY_ID,              /* 0x08 (Set) */
	CMD_ID_INFRASTRUCTURE,              /* 0x09 (Set) */
	CMD_ID_SET_RX_FILTER,               /* 0x0a (Set) */
	CMD_ID_DOWNLOAD_BUF,                /* 0x0b (Set) */
	CMD_ID_WIFI_START,                  /* 0x0c (Set) */
	CMD_ID_CMD_BT_OVER_WIFI,            /* 0x0d (Set) */
	CMD_ID_SET_MEDIA_CHANGE_DELAY_TIME, /* 0x0e (Set) */
	CMD_ID_SET_DOMAIN_INFO,             /* 0x0f (Set) */
	CMD_ID_SET_IP_ADDRESS,              /* 0x10 (Set) */
	CMD_ID_BSS_ACTIVATE_CTRL,           /* 0x11 (Set) */
	CMD_ID_SET_BSS_INFO,                /* 0x12 (Set) */
	CMD_ID_UPDATE_STA_RECORD,           /* 0x13 (Set) */
	CMD_ID_REMOVE_STA_RECORD,           /* 0x14 (Set) */
	CMD_ID_INDICATE_PM_BSS_CREATED,     /* 0x15 (Set) */
	CMD_ID_INDICATE_PM_BSS_CONNECTED,   /* 0x16 (Set) */
	CMD_ID_INDICATE_PM_BSS_ABORT,       /* 0x17 (Set) */
	CMD_ID_UPDATE_BEACON_CONTENT,       /* 0x18 (Set) */
	CMD_ID_SET_BSS_RLM_PARAM,           /* 0x19 (Set) */
	CMD_ID_SCAN_REQ,                    /* 0x1a (Set) */
	CMD_ID_SCAN_CANCEL,                 /* 0x1b (Set) */
	CMD_ID_CH_PRIVILEGE,                /* 0x1c (Set) */
	CMD_ID_UPDATE_WMM_PARMS,            /* 0x1d (Set) */
	CMD_ID_SET_WMM_PS_TEST_PARMS,       /* 0x1e (Set) */
	CMD_ID_TX_AMPDU,                    /* 0x1f (Set) */
	CMD_ID_ADDBA_REJECT,                /* 0x20 (Set) */
	CMD_ID_SET_PS_PROFILE_ADV,          /* 0x21 (Set) */
	CMD_ID_SET_RAW_PATTERN,             /* 0x22 (Set) */
	CMD_ID_CONFIG_PATTERN_FUNC,         /* 0x23 (Set) */
	CMD_ID_SET_TX_PWR,                  /* 0x24 (Set) */
	CMD_ID_SET_PWR_PARAM,               /* 0x25 (Set) */
	CMD_ID_P2P_ABORT,                   /* 0x26 (Set) */
	CMD_ID_SET_TX_EXTEND_PWR,           /* 0x27 (Set) */
	CMD_ID_SET_DBDC_PARMS,              /* 0x28 (Set) */
	CMD_ID_SET_FILTER_COEFFICIENT,      /* 0x29 (Set) */

	/* SLT commands */
	CMD_ID_RANDOM_RX_RESET_EN   = 0x2C, /* 0x2C (Set ) */
	CMD_ID_RANDOM_RX_RESET_DE   = 0x2D, /* 0x2D (Set ) */
	CMD_ID_SAPP_EN              = 0x2E, /* 0x2E (Set ) */
	CMD_ID_SAPP_DE              = 0x2F, /* 0x2F (Set ) */

	CMD_ID_ROAMING_TRANSIT      = 0x30, /* 0x30 (Set) */
	CMD_ID_SET_PHY_PARAM,               /* 0x31 (Set) */
	CMD_ID_SET_NOA_PARAM,               /* 0x32 (Set) */
	CMD_ID_SET_OPPPS_PARAM,             /* 0x33 (Set) */
	CMD_ID_SET_UAPSD_PARAM,             /* 0x34 (Set) */
	CMD_ID_SET_SIGMA_STA_SLEEP,         /* 0x35 (Set) */
	CMD_ID_SET_EDGE_TXPWR_LIMIT,        /* 0x36 (Set) */
	CMD_ID_SET_DEVICE_MODE,             /* 0x37 (Set) */
	CMD_ID_SET_TXPWR_CTRL,              /* 0x38 (Set) */
	CMD_ID_SET_AUTOPWR_CTRL,            /* 0x39 (Set) */
	CMD_ID_SET_WFD_CTRL,                /* 0x3a (Set) */
	CMD_ID_SET_NLO_REQ,                 /* 0x3b (Set), NO USE */
	CMD_ID_SET_NLO_CANCEL,              /* 0x3c (Set), NO USE */
	CMD_ID_SET_GTK_REKEY_DATA,          /* 0x3d (Set) */
	CMD_ID_ROAMING_CONTROL,             /* 0x3e (Set) */
	CMD_ID_RESET_BA_SCOREBOARD  = 0x3f, /* 0x3f (Set) */
	CMD_ID_SET_EDGE_TXPWR_LIMIT_5G = 0x40, /* 0x40 (Set) */
	CMD_ID_SET_CHANNEL_PWR_OFFSET,          /* 0x41 (Set) */
	CMD_ID_SET_80211AC_TX_PWR,          /* 0x42 (Set) */
	CMD_ID_SET_PATH_COMPASATION,        /* 0x43 (Set) */
	CMD_ID_SET_RTT_REQ = 0x44,          /* 0x44 (Set) */
	CMD_ID_SET_RTT_CALIBR,              /* 0x45 (Set) */
	CMD_ID_GET_RTT_RANGE_UPDATE,        /* 0x46 (Set) */
	CMD_ID_SET_BATCH_REQ,               /* 0x47 (Set), NO USE */
	CMD_ID_SET_NVRAM_SETTINGS,          /* 0x48 (Set) */
	CMD_ID_SET_COUNTRY_POWER_LIMIT,     /* 0x49 (Set) */
	CMD_ID_SET_WOWLAN = 0x4A,           /* 0x4A (Set) */
	CMD_ID_SET_IPV6_ADDRESS,            /* 0x4B (Set) */

	CMD_ID_SET_SLTINFO = 0x50,          /* 0x50 (Set) */
	CMD_ID_UART_ACK,                    /* 0x51 (Set) */
	CMD_ID_UART_NAK,                    /* 0x52 (Set) */
	CMD_ID_GET_CHIPID,

	CMD_ID_SET_MDDP_FILTER_RULE = 0x54, /* 0x54 (Set) */
	CMD_ID_SET_AM_FILTER = 0x55,        /* 0x55 (Set) */
	CMD_ID_SET_AM_HEARTBEAT,            /* 0x56 (Set) */
	CMD_ID_SET_AM_TCP,                  /* 0x57 (Set) */
	CMD_ID_SET_SUSPEND_MODE = 0x58,     /* 0x58 (Set) */
	CMD_ID_SET_PF_CAPABILITY = 0x59,     /* 0x59 (Set) */
	CMD_ID_SET_RRM_CAPABILITY = 0x5A,   /* 0x5A (Set) */
	CMD_ID_SET_AP_CONSTRAINT_PWR_LIMIT = 0x5B,  /* 0x5B (Set) */
	CMD_ID_SET_COUNTRY_POWER_LIMIT_PER_RATE = 0x5D, /* 0x5D (Set) */
	CMD_ID_SET_TSM_STATISTICS_REQUEST = 0x5E,
	CMD_ID_GET_TSM_STATISTICS = 0x5F,
	/* CFG_SUPPORT_GSCN  */
	CMD_ID_GET_PSCAN_CAPABILITY = 0x60, /* 0x60 (Set), NO USE */
	CMD_ID_SET_SCAN_SCHED_ENABLE,       /* 0x61 (Set) */
	CMD_ID_SET_SCAN_SCHED_REQ,          /* 0x62 (Set) */
	CMD_ID_SET_GSCAN_ADD_HOTLIST_BSSID, /* 0x63 (Set), NO USE */
	CMD_ID_SET_GSCAN_ADD_SWC_BSSID,     /* 0x64 (Set), NO USE */
	CMD_ID_SET_GSCAN_MAC_ADDR,          /* 0x65 (Set), NO USE */
	CMD_ID_GET_GSCAN_RESULT,            /* 0x66 (Get), NO USE */
	CMD_ID_SET_PSCAN_MAC_ADDR,          /* 0x67 (Get), NO USE */
	CMD_ID_FAST_SCAN_DUMMY2,            /* 0x68 (Get), NO USE */
	CMD_ID_FAST_SCAN_DUMMY3,            /* 0x69 (Get), NO USE */

	CMD_ID_UPDATE_AC_PARMS = 0x6A,     /* 0x6A (Set) */
	CMD_ID_SET_ROAMING_SKIP = 0x6D,
	/* 0x6D (Set) used to setting roaming skip*/
	CMD_ID_SET_DROP_PACKET_CFG = 0x6E,
	/* 0x6E (Set/Query) used to setting drop packet */

	/*CFG_SUPPORT_EASY_DEBUG*/
	CMD_ID_GET_SET_CUSTOMER_CFG = 0x70, /* 0x70 (Set/Query) */
	CMD_ID_SET_ALWAYS_SCAN_PARAM = 0x73,/* 0x73 (Set) */
	CMD_ID_TDLS_PS = 0x75,              /* 0x75 (Set) */

	CMD_ID_GET_CNM = 0x79,

	CMD_ID_FRM_IND_FROM_HOST = 0x7D,    /* 0x7D (Set) */
	CMD_ID_PERF_IND = 0x7E,     /* 0x7E(Set) */
#if CFG_SUPPORT_SMART_GEAR
	CMD_ID_SG_PARAM = 0x7F, /* 0x7F(Set) */
#endif
	CMD_ID_GET_NIC_CAPABILITY   = 0x80, /* 0x80 (Query) */
	CMD_ID_GET_LINK_QUALITY,            /* 0x81 (Query) */
	CMD_ID_GET_STATISTICS,              /* 0x82 (Query) */
	CMD_ID_GET_CONNECTION_STATUS,       /* 0x83 (Query) */
	CMD_ID_GET_STA_STATISTICS = 0x85,   /* 0x85 (Query) */
	CMD_ID_GET_LTE_CHN = 0x87,          /* 0x87 (Query) */
	CMD_ID_GET_BUG_REPORT = 0x89,       /* 0x89 (Query) */
	CMD_ID_GET_NIC_CAPABILITY_V2 = 0x8A,/* 0x8A (Query) */
#if CFG_SUPPORT_LOWLATENCY_MODE
	CMD_ID_SET_LOW_LATENCY_MODE = 0x8B, /* 0x8B (Set) */
#endif
	CMD_ID_LOG_UI_INFO  = 0x8D,         /* 0x8D (Set / Query) */
	/* Oshare mode*/
	CMD_ID_SET_OSHARE_MODE = 0x8E,
	CMD_ID_RDD_ON_OFF_CTRL = 0x8F,      /* 0x8F(Set) */
	CMD_ID_SET_FORCE_RTS = 0x90,
	CMD_ID_WFC_KEEP_ALIVE = 0xA0,       /* 0xA0 (Set) */
	CMD_ID_RSSI_MONITOR = 0xA1,         /* 0xA1 (Set) */
	CMD_ID_CAL_BACKUP_IN_HOST_V2 = 0xAE,    /* 0xAE (Set / Query) */

	CMD_ID_MQM_UPDATE_MU_EDCA_PARMS = 0xB0,   /* 0xB0 (Set) */
	CMD_ID_RLM_UPDATE_SR_PARAMS = 0xB1,       /* 0xB1 (Set) */

	CMD_ID_ACCESS_REG           = 0xc0, /* 0xc0 (Set / Query) */
	CMD_ID_MAC_MCAST_ADDR,              /* 0xc1 (Set / Query) */
	CMD_ID_802_11_PMKID,                /* 0xc2 (Set / Query) */
	CMD_ID_ACCESS_EEPROM,               /* 0xc3 (Set / Query) */
	CMD_ID_SW_DBG_CTRL,                 /* 0xc4 (Set / Query) */
	CMD_ID_FW_LOG_2_HOST,               /* 0xc5 (Set) */
	CMD_ID_DUMP_MEM,                    /* 0xc6 (Query) */
	CMD_ID_RESOURCE_CONFIG,             /* 0xc7 (Set / Query) */
	CMD_ID_ACCESS_RX_STAT,              /* 0xc8 (Query) */

	CMD_ID_CHIP_CONFIG          = 0xCA, /* 0xca (Set / Query) */
	CMD_ID_STATS_LOG            = 0xCB,

	CMD_ID_WTBL_INFO        = 0xCD, /* 0xcd (Query) */
	CMD_ID_MIB_INFO     = 0xCE, /* 0xce (Query) */

	CMD_ID_SET_TXBF_BACKOFF = 0xD1,

	CMD_ID_SET_RDD_CH           = 0xE1,

	CMD_ID_LAYER_0_EXT_MAGIC_NUM    = 0xED,
	/* magic number for Extending MT6630 original CMD header  */
	/*CMD_ID_LAYER_1_MAGIC_NUM      = 0xEE, */
	/* magic number for backward compatible with MT76xx CMD  */
	CMD_ID_INIT_CMD_WIFI_RESTART    = 0xEF,
	/* 0xef (reload firmware use) wayne_note 2013.09.26 */

	CMD_ID_SET_BWCS             = 0xF1,
	CMD_ID_SET_OSC              = 0xF2,
	CMD_ID_HIF_CTRL             = 0xF6,
	/* 0xF6 (Set) USB suspend/resume */
	CMD_ID_GET_BUILD_DATE_CODE = 0xF8,   /* 0xf8 (Query) */
	CMD_ID_GET_BSS_INFO = 0xF9,          /* 0xF9 (Query) */
	CMD_ID_SET_HOTSPOT_OPTIMIZATION = 0xFA,    /* 0xFA (Set) */

	CMD_ID_SET_TDLS_CH_SW = 0xFB,

	CMD_ID_SET_MONITOR = 0xFC,          /* 0xfc (Set) */
	CMD_ID_SET_CCK_1M_PWR = 0xFD,	/* 0xFC (Set) */
	CMD_ID_END
};

/* Define EVENT ID from firmware to Host (v0.09) */
enum ENUM_EVENT_ID {
	EVENT_ID_NIC_CAPABILITY     = 0x01, /* 0x01 (Query) */
	EVENT_ID_LINK_QUALITY,              /* 0x02 (Query / Unsolicited) */
	EVENT_ID_STATISTICS,                /* 0x03 (Query) */
	EVENT_ID_MIC_ERR_INFO,              /* 0x04 (Unsolicited) */
	EVENT_ID_ACCESS_REG,
	/* 0x05 (Query - CMD_ID_ACCESS_REG) */
	EVENT_ID_ACCESS_EEPROM,
	/* 0x06 (Query - CMD_ID_ACCESS_EEPROM) */
	EVENT_ID_SLEEPY_INFO,               /* 0x07 (Unsolicited) */
	EVENT_ID_BT_OVER_WIFI,              /* 0x08 (Unsolicited) */
	EVENT_ID_TEST_STATUS,
	/* 0x09 (Query - CMD_ID_TEST_CTRL) */
	EVENT_ID_RX_ADDBA,                  /* 0x0a (Unsolicited) */
	EVENT_ID_RX_DELBA,                  /* 0x0b (Unsolicited) */
	EVENT_ID_ACTIVATE_STA_REC,          /* 0x0c (Response) */
	EVENT_ID_SCAN_DONE,                 /* 0x0d (Unsoiicited) */
	EVENT_ID_RX_FLUSH,                  /* 0x0e (Unsolicited) */
	EVENT_ID_TX_DONE,                   /* 0x0f (Unsolicited) */
	EVENT_ID_CH_PRIVILEGE,              /* 0x10 (Unsolicited) */
	EVENT_ID_BSS_ABSENCE_PRESENCE,      /* 0x11 (Unsolicited) */
	EVENT_ID_STA_CHANGE_PS_MODE,        /* 0x12 (Unsolicited) */
	EVENT_ID_BSS_BEACON_TIMEOUT,        /* 0x13 (Unsolicited) */
	EVENT_ID_UPDATE_NOA_PARAMS,         /* 0x14 (Unsolicited) */
	EVENT_ID_AP_OBSS_STATUS,            /* 0x15 (Unsolicited) */
	EVENT_ID_STA_UPDATE_FREE_QUOTA,     /* 0x16 (Unsolicited) */
	EVENT_ID_SW_DBG_CTRL,
	/* 0x17 (Query - CMD_ID_SW_DBG_CTRL) */
	EVENT_ID_ROAMING_STATUS,            /* 0x18 (Unsolicited) */
	EVENT_ID_STA_AGING_TIMEOUT,         /* 0x19 (Unsolicited) */
	EVENT_ID_SEC_CHECK_RSP,
	/* 0x1a (Query - CMD_ID_SEC_CHECK) */
	EVENT_ID_SEND_DEAUTH,               /* 0x1b (Unsolicited) */
	EVENT_ID_UPDATE_RDD_STATUS,         /* 0x1c (Unsolicited) */
	EVENT_ID_UPDATE_BWCS_STATUS,        /* 0x1d (Unsolicited) */
	EVENT_ID_UPDATE_BCM_DEBUG,          /* 0x1e (Unsolicited) */
	EVENT_ID_RX_ERR,                    /* 0x1f (Unsolicited) */
	EVENT_ID_DUMP_MEM = 0x20,
	/* 0x20 (Query - CMD_ID_DUMP_MEM) */
	EVENT_ID_STA_STATISTICS,            /* 0x21 (Query ) */
	EVENT_ID_STA_STATISTICS_UPDATE,     /* 0x22 (Unsolicited) */
	EVENT_ID_SCHED_SCAN_DONE,                  /* 0x23 (Unsoiicited) */
	EVENT_ID_ADD_PKEY_DONE,             /* 0x24 (Unsoiicited) */
	EVENT_ID_ICAP_DONE,                 /* 0x25 (Unsoiicited) */
	EVENT_ID_RESOURCE_CONFIG = 0x26,
	/* 0x26 (Query - CMD_ID_RESOURCE_CONFIG) */
	EVENT_ID_DEBUG_MSG = 0x27,          /* 0x27 (Unsoiicited) */
	EVENT_ID_RTT_DISCOVER_PEER = 0x28,    /* 0x28 (Unsoiicited) */
	EVENT_ID_RTT_UPDATE_RANGE = 0x29,          /* 0x29 (Unsoiicited) */
	EVENT_ID_CHECK_REORDER_BUBBLE = 0x2a,      /* 0x2a (Unsoiicited) */
	EVENT_ID_BATCH_RESULT = 0x2b,              /* 0x2b (Query) */
	EVENT_ID_STA_ABSENCE_TX = 0x2c,     /* 0x2c (Unsoiicited) */
	EVENT_ID_RTT_UPDATE_LOCATION = 0x2d,       /* 0x2c (Unsoiicited) */
	EVENT_ID_TX_ADDBA = 0x2e,
	EVENT_ID_LTE_SAFE_CHN = 0x2f,       /* 0x2f (Query ) */



	/* CFG_SUPPORT_GSCN  */
	EVENT_ID_GSCAN_CAPABILITY = 0x30,
	EVENT_ID_GSCAN_SCAN_COMPLETE = 0x31,
	EVENT_ID_GSCAN_FULL_RESULT = 0x32,
	EVENT_ID_GSCAN_SIGNIFICANT_CHANGE = 0x33,
	EVENT_ID_GSCAN_GEOFENCE_FOUND = 0x34,
	EVENT_ID_GSCAN_SCAN_AVAILABLE = 0x35,
	EVENT_ID_GSCAN_RESULT = 0x36,
	EVENT_ID_GSCAN_HOTLIST_RESULTS_FOUND = 0x37,
	EVENT_ID_GSCAN_HOTLIST_RESULTS_LOST = 0x38,
	EVENT_ID_FAST_SCAN_DUMMY1 = 0x39,
	EVENT_ID_FAST_SCAN_DUMMY2 = 0x3a,
	EVENT_ID_FAST_SCAN_DUMMY3 = 0x3b,

	EVENT_ID_UART_ACK = 0x40,           /* 0x40 (Unsolicited) */
	EVENT_ID_UART_NAK,                  /* 0x41 (Unsolicited) */
	EVENT_ID_GET_CHIPID,
	/* 0x42 (Query - CMD_ID_GET_CHIPID) */
	EVENT_ID_SLT_STATUS,
	/* 0x43 (Query - CMD_ID_SET_SLTINFO) */
	EVENT_ID_CHIP_CONFIG,
	/* 0x44 (Query - CMD_ID_CHIP_CONFIG) */
	EVENT_ID_ACCESS_RX_STAT = 0x45,
	/* 0x45 (Query - CMD_ID_ACCESS_RX_STAT) */

	EVENT_ID_RDD_SEND_PULSE = 0x50,
	EVENT_ID_PFMU_TAG_READ = 0x51,
	EVENT_ID_PFMU_DATA_READ = 0x52,
	EVENT_ID_MU_GET_QD = 0x53,
	EVENT_ID_MU_GET_LQ = 0x54,
	EVENT_ID_AM_FILTER = 0x55,
	/* 0x55 (Query - CMD_ID_SET_AM_FILTER) */
	EVENT_ID_AM_HEARTBEAT = 0x56,
	/* 0x56 (Query - CMD_ID_SET_AM_HEARTBEAT) */
	EVENT_ID_AM_TCP = 0x57,
	/* 0x57 (Query - CMD_ID_SET_AM_TCP) */
	EVENT_ID_HEARTBEAT_INFO = 0x58,         /* 0x58 (Unsolicited) */
	EVENT_ID_MDDP_FILTER_RULE = 0x59,
	/* 0x59 (Query - CMD_ID_SET_MDDP_FILTER_RULE) */
	EVENT_ID_BA_FW_DROP_SN = 0x5A,          /* 0x5A (Unsolicited) */
	EVENT_ID_LOW_LATENCY_INFO = 0x5B,	/* 0x5B (Unsolicited) */
	EVENT_ID_RDD_REPORT = 0x60,
	EVENT_ID_CSA_DONE = 0x61,

	EVENT_ID_OPMODE_CHANGE = 0x63,
#if CFG_SUPPORT_IDC_CH_SWITCH
	EVENT_ID_LTE_IDC_REPORT = 0x64,
#endif

#if CFG_SUPPORT_SMART_GEAR
	EVENT_ID_SG_STATUS = 0x65,
#endif

#if CFG_SUPPORT_HE_ER
	EVENT_ID_BSS_ER_TX_MODE = 0x66,  /* 0x66 BSS Extend Rage (ER) mode */
#endif

	EVENT_ID_GET_CMD_INFO = 0x70,
	/* 0x70 (Query - EVENT_ID_GET_CMD_INFO) */
	/*query info from cmd.*/
	EVENT_ID_DBDC_SWITCH_DONE = 0x78,
	EVENT_ID_GET_CNM = 0x79,

	EVENT_ID_FRM_IND_FROM_HOST = 0x7D,

	EVENT_ID_TDLS = 0x80,

	EVENT_ID_LOG_UI_INFO  = 0x8D,           /* 0x8D (Set / Query) */
	EVENT_ID_UPDATE_COEX_PHYRATE = 0x90,    /* 0x90 (Unsolicited) */

	EVENT_ID_RSSI_MONITOR = 0xA1,       /* Event ID for Rssi monitoring */
	EVENT_ID_CAL_BACKUP_IN_HOST_V2 = 0xAE,
	/* 0xAE (Query - CMD_ID_CAL_BACKUP) */
	EVENT_ID_CAL_ALL_DONE = 0xAF,   /* 0xAF (FW Cal All Done Event) */

	EVENT_ID_WTBL_INFO = 0xCD,              /* 0xCD (Query) */
	EVENT_ID_MIB_INFO = 0xCE,               /* 0xCE (Query) */

	EVENT_ID_NIC_CAPABILITY_V2 = 0xEC,
	/* 0xEC (Query - CMD_ID_GET_NIC_CAPABILITY_V2) */
	EVENT_ID_LAYER_0_EXT_MAGIC_NUM  = 0xED,
	/* magic number for Extending MT6630 original EVENT header  */
	EVENT_ID_ASSERT_DUMP = 0xF0,
	EVENT_ID_HIF_CTRL = 0xF6,
	EVENT_ID_BUILD_DATE_CODE = 0xF8,
	EVENT_ID_GET_AIS_BSS_INFO = 0xF9,
	EVENT_ID_DEBUG_CODE = 0xFB,
	EVENT_ID_RFTEST_READY = 0xFC,   /* 0xFC */

	EVENT_ID_INIT_EVENT_CMD_RESULT = 0xFD,
	/* 0xFD (Generic event for cmd not found, added by CONNAC) */

	EVENT_ID_END
};

/* commands */

/* already define at wifi_task.h!!! */
/*chiahsuan:  ReStart Download Firmware
 *Request/Response Start. (duplicated one.
 * it is copy from ROM)
 */
struct INIT_WIFI_CMD {
	uint8_t      ucCID;
	uint8_t      ucPktTypeID;    /* Must be 0xA0 (CMD Packet) */
	uint8_t      ucReserved;
	uint8_t      ucSeqNum;
	uint32_t     u4Reserved;
	/* add one DW to compatible with normal TXD format. */
	uint32_t     au4D3toD7Rev[5];
	/* add 5 DW to compatible with normal TXD format. */
	uint8_t      aucBuffer[0];
};

struct INIT_WIFI_EVENT {
	uint16_t     u2RxByteCount;
	uint16_t     u2PacketType;
	/* Must be filled with 0xE000 (EVENT Packet) */
	uint8_t      ucEID;
	uint8_t      ucSeqNum;
	uint8_t      aucReserved[2];

	uint8_t      aucBuffer[0];
};

struct INIT_HIF_TX_HEADER {
	uint16_t     u2TxByteCount;
	uint16_t     u2PQ_ID;        /* Must be 0x8000 (Port1, Queue 0) */
	struct INIT_WIFI_CMD rInitWifiCmd;
};

struct INIT_HIF_TX_HEADER_PENDING_FOR_HW_32BYTES {
	uint16_t     u2TxByteCount;
	uint16_t     u2PQ_ID;        /* Must be 0x8000 (Port1, Queue 0) */

	uint8_t      ucWlanIdx;
	uint8_t      ucHeaderFormat;
	uint8_t      ucHeaderPadding;
	uint8_t      ucPktFt: 2;
	uint8_t      ucOwnMAC: 6;
	uint32_t     au4D2toD7Rev[6];
};

struct INIT_EVENT_CMD_RESULT {
	uint8_t      ucStatus;
	/* 0: success, 0xFE command not support, others: failure */
	uint8_t      ucCID;
	uint8_t      aucReserved[2];
};

/*---------------------------------------------------------------------------*/
/* Parameters of User Configuration which match to NDIS5.1                */
/*---------------------------------------------------------------------------*/
typedef int32_t          PARAM_RSSI;

/* NDIS_802_11_RATES_EX */
typedef uint8_t  PARAM_RATES_EX[PARAM_MAX_LEN_RATES_EX];

/* NDIS_802_11_NETWORK_TYPE */
enum ENUM_PARAM_PHY_NET_TYPE {
	PHY_NET_TYPE_FH,
	PHY_NET_TYPE_DS,
	PHY_NET_TYPE_OFDM5,
	PHY_NET_TYPE_OFDM24,
	PHY_NET_TYPE_AUTOMODE,
	PHY_NET_TYPE_NUM                    /*!< Upper bound, not real case */
};

/* NDIS_802_11_AUTHENTICATION_MODE */
enum ENUM_PARAM_AUTH_MODE {
	AUTH_MODE_OPEN = 0,                 /*!< Open system */
	AUTH_MODE_SHARED,                   /*!< Shared key */
	AUTH_MODE_AUTO_SWITCH,
	/*!< Either open system or shared key */
	AUTH_MODE_WPA,
	AUTH_MODE_WPA_PSK,
	AUTH_MODE_WPA_NONE,                 /*!< For Ad hoc */
	AUTH_MODE_WPA2,
	AUTH_MODE_WPA2_PSK,
	AUTH_MODE_WPA2_FT,                  /* 802.11r */
	AUTH_MODE_WPA2_FT_PSK,              /* 802.11r */
	AUTH_MODE_WPA_OSEN,
	AUTH_MODE_WPA3_SAE,
	AUTH_MODE_WPA3_OWE,
	AUTH_MODE_NUM                       /*!< Upper bound, not real case */
};

/* NDIS_802_11_ENCRYPTION_STATUS */
enum ENUM_WEP_STATUS {
	ENUM_WEP_ENABLED,
	ENUM_ENCRYPTION1_ENABLED = ENUM_WEP_ENABLED,  /* WEP */
	ENUM_WEP_DISABLED,
	ENUM_ENCRYPTION_DISABLED = ENUM_WEP_DISABLED, /* OPEN */
	ENUM_WEP_KEY_ABSENT,
	ENUM_ENCRYPTION1_KEY_ABSENT = ENUM_WEP_KEY_ABSENT,
	ENUM_WEP_NOT_SUPPORTED,
	ENUM_ENCRYPTION_NOT_SUPPORTED = ENUM_WEP_NOT_SUPPORTED,
	ENUM_ENCRYPTION2_ENABLED, /* TKIP(WPA/WPA2) */
	ENUM_ENCRYPTION2_KEY_ABSENT,
	ENUM_ENCRYPTION3_ENABLED, /* CCMP(WPA/WPA2) */
	ENUM_ENCRYPTION3_KEY_ABSENT,
	ENUM_ENCRYPTION4_ENABLED, /* GCMP256(WPA2/WPA3) */
	ENUM_ENCRYPTION4_KEY_ABSENT,
	ENUM_ENCRYPTION_NUM
};

/*---------------------------------------------------------------------------*/
/* CMD Packets                                                            */
/*---------------------------------------------------------------------------*/
struct CMD_BUILD_CONNECTION {
	uint8_t      ucInfraMode;
	uint8_t      ucAuthMode;
	uint8_t      ucEncryptStatus;
	uint8_t      ucSsidLen;
	uint8_t      aucSsid[32];
	uint8_t      aucBssid[6];

	/* The following parameters are for Ad-hoc network */
	uint16_t     u2BeaconPeriod;
	uint16_t     u2ATIMWindow;
	uint8_t      ucJoinOnly;
	uint8_t      ucReserved;
	uint32_t     u4FreqInKHz;

};

struct CMD_802_11_KEY {
	uint8_t      ucAddRemove;
	uint8_t      ucTxKey;   /* 1 : Tx key */
	uint8_t      ucKeyType; /* 0 : group Key, 1 : Pairwise key */
	uint8_t      ucIsAuthenticator; /* 1 : authenticator */
	uint8_t      aucPeerAddr[6];
	uint8_t      ucBssIdx; /* the BSS index */
	uint8_t      ucAlgorithmId;
	uint8_t      ucKeyId;
	uint8_t      ucKeyLen;
	uint8_t      ucWlanIndex;
	uint8_t      ucMgmtProtection;
	uint8_t      aucKeyMaterial[32];
	uint8_t      aucKeyRsc[16];
};

struct CMD_DEFAULT_KEY_ID {
	uint8_t      ucBssIdx;
	uint8_t      ucKeyId;
	uint8_t      ucUnicast;
	uint8_t      ucMulticast;
};


/* CMD for PMKID and related structure */
typedef uint8_t   PARAM_PMKID_VALUE[16];

struct PARAM_BSSID_INFO {
	uint8_t              aucBssid[MAC_ADDR_LEN];    /* MAC address */
	PARAM_PMKID_VALUE   arPMKID;
};

/* This struct only uses uint16_t,
 * compiler will use 2-byte alignment.
 * sizeof(struct CMD_SET_BSS_RLM_PARAM) is 22
 */
struct CMD_SET_BSS_RLM_PARAM {
	uint8_t      ucBssIndex;
	uint8_t      ucRfBand;
	uint8_t      ucPrimaryChannel;
	uint8_t      ucRfSco;
	uint8_t      ucErpProtectMode;
	uint8_t      ucHtProtectMode;
	uint8_t      ucGfOperationMode;
	uint8_t      ucTxRifsMode;
	uint16_t     u2HtOpInfo3;
	uint16_t     u2HtOpInfo2;
	uint8_t      ucHtOpInfo1;
	uint8_t      ucUseShortPreamble;
	uint8_t      ucUseShortSlotTime;
	uint8_t      ucVhtChannelWidth;
	uint8_t      ucVhtChannelFrequencyS1;
	uint8_t      ucVhtChannelFrequencyS2;
	uint16_t     u2VhtBasicMcsSet;
	uint8_t      ucTxNss;
	uint8_t      ucRxNss;
};

/*CMD_ID_FRM_IND_FROM_HOST 0x7D*/

/*ver 1 structure*/
struct CMD_FRM_IND_FROM_HOST {
	/* DWORD_0 - Common Part*/
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen;       /* cmd size including common part and body.*/

	/* DWORD_1 ~ x - Command Body*/
	uint8_t  ucStaIdx;
	uint8_t  ucBssIdx;
	/** TX/RX/TXS(future used) ENUM_CMD_FRM_IND_FROM_HOST_TRANSMIT_TYPE */
	uint8_t  ucTransmitType;
	/** ENUM_CMD_FRM_IND_FROM_HOST_PROTOCOL_TYPE */
	uint8_t  ucProtocolType;
	/** ENUM_CMD_FRM_IND_FROM_HOST_EAP_MSG_TYPE,
	 * ENUM_CMD_M_IND_FROM_HOST_DHCP_MSG_TYPE
	 */
	/** depend on ucProtocolType */
	uint8_t  ucProtocolSubType;
	uint8_t  ucRateValid;
	uint8_t  aucPadding1[2];
	/** TX Rate, Rx Rate, TxS Rate */
	uint32_t u4Rate;
	uint32_t u4Len;
	/** Only TXS type  value in txS*/
	uint8_t  TxS;
	uint8_t  aucPadding3[3];
	uint8_t  aucPadding4[64];
};

enum ENUM_FRM_IND_FROM_HOST_VER {
	CMD_FRM_IND_FROM_HOST_VER_INIT      = 0x0,
	CMD_FRM_IND_FROM_HOST_VER_1ST       = 0x1,
	CMD_FRM_IND_FROM_HOST_VER_MAX       = 0x2,
};

enum ENUM_CMD_FRM_IND_FROM_HOST_TRANSMIT_TYPE {
	CMD_FRM_IND_FROM_HOST_TRANSMIT_TYPE_TX            = 0x00,
	CMD_FRM_IND_FROM_HOST_TRANSMIT_TYPE_RX            = 0x01,
	CMD_FRM_IND_FROM_HOST_TRANSMIT_TYPE_TXS           = 0x02,
	CMD_FRM_IND_FROM_HOST_TRANSMIT_TYPE_MAX           = 0x03,
};

enum ENUM_CMD_FRM_IND_FROM_HOST_PROTOCOL_TYPE {
	CMD_FRM_IND_FROM_HOST_PROTOCOL_TYPE_NON_SPECIFIC  = 0x00,
	CMD_FRM_IND_FROM_HOST_PROTOCOL_TYPE_EAP           = 0x01,
	CMD_FRM_IND_FROM_HOST_PROTOCOL_TYPE_DHCP          = 0x02,
	CMD_FRM_IND_FROM_HOST_PROTOCOL_TYPE_MAX           = 0x03,
};

enum ENUM_CMD_FRM_IND_FROM_HOST_EAP_MSG_TYPE {
	CMD_FRM_IND_FROM_HOST_EAP_MSG_NON_SPECIFIC    = 0x00,
	CMD_FRM_IND_FROM_HOST_EAP_MSG_4WAY_1          = 0x01,
	CMD_FRM_IND_FROM_HOST_EAP_MSG_4WAY_2          = 0x02,
	CMD_FRM_IND_FROM_HOST_EAP_MSG_4WAY_3          = 0x03,
	CMD_FRM_IND_FROM_HOST_EAP_MSG_4WAY_4          = 0x04,
	CMD_FRM_IND_FROM_HOST_EAP_MSG_GROUP_1         = 0x05,
	CMD_FRM_IND_FROM_HOST_EAP_MSG_GROUP_2         = 0x06,
	CMD_FRM_IND_FROM_HOST_EAP_MSG_MAX             = 0x07,
};

enum ENUM_CMD_FRM_IND_FROM_HOST_DHCP_MSG_TYPE {
	CMD_FRM_IND_FROM_HOST_DHCP_MSG_NON_SPECIFIC   = 0x00,
	CMD_FRM_IND_FROM_HOST_DHCP_MSG_DISCOVER       = 0x01,
	CMD_FRM_IND_FROM_HOST_DHCP_MSG_OFFER          = 0x02,
	CMD_FRM_IND_FROM_HOST_DHCP_MSG_REQUEST        = 0x03,
	CMD_FRM_IND_FROM_HOST_DHCP_MSG_ACK            = 0x04,
	CMD_FRM_IND_FROM_HOST_DHCP_MSG_MAX            = 0x05,
};

/* This struct uses uint32_t, compiler will use 4-byte alignment.
 * sizeof(struct CMD_SET_BSS_INFO) is 116
 */
struct CMD_SET_BSS_INFO {
	uint8_t  ucBssIndex;
	uint8_t  ucConnectionState;
	uint8_t  ucCurrentOPMode;
	uint8_t  ucSSIDLen;
	uint8_t  aucSSID[32];
	uint8_t  aucBSSID[6];
	uint8_t  ucIsQBSS;
	uint8_t  ucVersion;
	uint16_t u2OperationalRateSet;
	uint16_t u2BSSBasicRateSet;
	uint8_t  ucStaRecIdxOfAP;
	uint8_t  aucPadding0[1];
	uint16_t u2HwDefaultFixedRateCode;
	uint8_t  ucNonHTBasicPhyType; /* For Slot Time and CWmin */
	uint8_t  ucAuthMode;
	uint8_t  ucEncStatus;
	uint8_t  ucPhyTypeSet;
	uint8_t  ucWapiMode;
	uint8_t  ucIsApMode;
	uint8_t  ucBMCWlanIndex;
	uint8_t  ucHiddenSsidMode;
	uint8_t  ucDisconnectDetectThreshold;
	uint8_t  ucIotApAct;
	uint8_t  aucPadding1[2];
	uint32_t u4PrivateData;
	struct CMD_SET_BSS_RLM_PARAM rBssRlmParam; /*68*/
	uint8_t  ucDBDCBand;  /*90, ENUM_CMD_REQ_DBDC_BAND_T*/
	uint8_t  ucWmmSet;
	uint8_t  ucDBDCAction;
	uint8_t  ucNss;
	uint8_t  aucPadding2[2];
	uint8_t  ucHeOpParams[HE_OP_BYTE_NUM];
	uint8_t  ucBssColorInfo;
	uint16_t u2HeBasicMcsSet;
	uint8_t  ucMaxBSSIDIndicator;
	uint8_t  ucMBSSIDIndex;
	uint8_t  aucPadding[12];
};

struct CMD_HTVHT_BA_SIZE {
	uint8_t ucTxBaSize;
	uint8_t ucRxBaSize;
	uint8_t aucReserved[2];
};

struct CMD_HE_BA_SIZE {
	uint16_t u2TxBaSize;
	uint16_t u2RxBaSize;
};

struct CMD_UPDATE_STA_RECORD {
	uint8_t  ucStaIndex;
	uint8_t  ucStaType;
	uint8_t  aucMacAddr[MAC_ADDR_LEN];
	/* This field should assign at create and keep consistency for update
	 * usage
	 */

	uint16_t u2AssocId;
	uint16_t u2ListenInterval;
	uint8_t  ucBssIndex;
	/* This field should assign at create and keep consistency for update
	 * usage
	 */
	uint8_t  ucDesiredPhyTypeSet;
	uint16_t u2DesiredNonHTRateSet;

	uint16_t u2BSSBasicRateSet;
	uint8_t  ucIsQoS;
	uint8_t  ucIsUapsdSupported;
	uint8_t  ucStaState;
	uint8_t  ucMcsSet;
	uint8_t  ucSupMcs32;
	uint8_t  ucVersion;
	/* Original padding is used for version now */

	uint8_t  aucRxMcsBitmask[10];
	uint16_t u2RxHighestSupportedRate;
	uint32_t u4TxRateInfo;

	uint16_t u2HtCapInfo;
	uint16_t u2HtExtendedCap;
	uint32_t u4TxBeamformingCap;

	uint8_t  ucAmpduParam;
	uint8_t  ucAselCap;
	uint8_t  ucRCPI;
	uint8_t  ucNeedResp;
	uint8_t  ucUapsdAc;
	/* b0~3: Trigger enabled, b4~7: Delivery enabled */
	uint8_t  ucUapsdSp;
	/* 0: all, 1: max 2, 2: max 4, 3: max 6 */
	uint8_t  ucWlanIndex;
	/* This field should assign at create and keep consistency for update
	 * usage
	 */
	uint8_t  ucBMCWlanIndex;
	/* This field should assign at create and keep consistency for update
	 * usage
	 */

	uint32_t u4VhtCapInfo;
	uint16_t u2VhtRxMcsMap;
	uint16_t u2VhtRxHighestSupportedDataRate;
	uint16_t u2VhtTxMcsMap;
	uint16_t u2VhtTxHighestSupportedDataRate;
	uint8_t  ucRtsPolicy;
	/* 0: auto 1: Static BW 2: Dynamic BW 3: Legacy 7: WoRts */
	uint8_t  ucVhtOpMode;
	/* VHT operting mode, bit 7: Rx NSS Type, bit 4-6: Rx NSS, bit 0-1:
	 * Channel Width
	 */

	uint8_t  ucTrafficDataType;
	uint8_t  ucTxGfMode;               /* ENUM_FEATURE_OPTION */
	uint8_t  ucTxSgiMode;              /* ENUM_FEATURE_OPTION */
	uint8_t  ucTxStbcMode;             /* ENUM_FEATURE_OPTION unused */
	uint16_t u2HwDefaultFixedRateCode;
	uint8_t  ucTxAmpdu;
	uint8_t  ucRxAmpdu;
	uint32_t u4FixedPhyRate;
	/* 0: desable BIT(31)==1,BITS(0,15) rate code */
	uint16_t u2MaxLinkSpeed;
	/* 0: unlimit ohter: unit is 0.5 Mbps */
	uint16_t u2MinLinkSpeed;

	uint32_t  u4Flags;

	union BA_SIZE {
		struct CMD_HTVHT_BA_SIZE rHtVhtBaSize;
		struct CMD_HE_BA_SIZE rHeBaSize;
	} rBaSize;

	uint16_t   u2PfmuId;   /* 0xFFFF means no access right for PFMU*/
	uint8_t   fgSU_MU;    /* 0 : SU, 1 : MU*/
	uint8_t   fgETxBfCap; /* 0 : ITxBf, 1 : ETxBf*/
	uint8_t    ucSoundingPhy;
	uint8_t    ucNdpaRate;
	uint8_t    ucNdpRate;
	uint8_t    ucReptPollRate;
	uint8_t    ucTxMode;
	uint8_t    ucNc;
	uint8_t    ucNr;
	uint8_t    ucCBW;      /* 0 : 20M, 1 : 40M, 2 : 80M, 3 : 80 + 80M*/
	uint8_t    ucTotMemRequire;
	uint8_t    ucMemRequire20M;
	uint8_t    ucMemRow0;
	uint8_t    ucMemCol0;
	uint8_t    ucMemRow1;
	uint8_t    ucMemCol1;
	uint8_t    ucMemRow2;
	uint8_t    ucMemCol2;
	uint8_t    ucMemRow3;
	uint8_t    ucMemCol3;
	uint16_t   u2SmartAnt;
	uint8_t    ucSEIdx;
	uint8_t    uciBfTimeOut;
	uint8_t    uciBfDBW;
	uint8_t    uciBfNcol;
	uint8_t    uciBfNrow;
	uint8_t    aucPadding1[3];

	uint8_t ucTxAmsduInAmpdu;
	uint8_t ucRxAmsduInAmpdu;
	uint8_t aucPadding2[2];
	uint32_t u4TxMaxAmsduInAmpduLen;

#if CFG_SUPPORT_802_11AX
	/* These fields can only be accessed if ucVersion = 1*/
	uint8_t  ucHeMacCapInfo[HE_MAC_CAP_BYTE_NUM];
	uint8_t  ucHePhyCapInfo[HE_PHY_CAP_BYTE_NUM];
	uint8_t  aucPadding3[2];
	uint16_t u2HeRxMcsMapBW80;
	uint16_t u2HeTxMcsMapBW80;
	uint16_t u2HeRxMcsMapBW160;
	uint16_t u2HeTxMcsMapBW160;
	uint16_t u2HeRxMcsMapBW80P80;
	uint16_t u2HeTxMcsMapBW80P80;
#endif
	uint8_t  aucPadding4[32];
};

struct CMD_REMOVE_STA_RECORD {
	uint8_t  ucActionType;
	uint8_t  ucStaIndex;
	uint8_t  ucBssIndex;
	uint8_t  ucReserved;
};

struct CMD_INDICATE_PM_BSS_CREATED {
	uint8_t  ucBssIndex;
	uint8_t  ucDtimPeriod;
	uint16_t u2BeaconInterval;
	uint16_t u2AtimWindow;
	uint8_t  aucReserved[2];
};

struct CMD_INDICATE_PM_BSS_CONNECTED {
	uint8_t  ucBssIndex;
	uint8_t  ucDtimPeriod;
	uint16_t u2AssocId;
	uint16_t u2BeaconInterval;
	uint16_t u2AtimWindow;
	uint8_t  fgIsUapsdConnection;
	uint8_t  ucBmpDeliveryAC;
	uint8_t  ucBmpTriggerAC;
	uint8_t  aucReserved[1];
};

struct CMD_INDICATE_PM_BSS_ABORT {
	uint8_t  ucBssIndex;
	uint8_t  aucReserved[3];
};

struct CMD_SET_WMM_PS_TEST_STRUCT {
	uint8_t  ucBssIndex;
	uint8_t  bmfgApsdEnAc;
	/* b0~3: trigger-en AC0~3. b4~7: delivery-en AC0~3 */
	uint8_t  ucIsEnterPsAtOnce;
	/* enter PS immediately without 5 second guard after connected */
	uint8_t  ucIsDisableUcTrigger;
	/* not to trigger UC on beacon TIM is matched (under U-APSD) */
};

struct CMD_CUSTOM_NOA_PARAM_STRUCT {
	uint32_t  u4NoaDurationMs;   /* unit: msec*/
	uint32_t  u4NoaIntervalMs;   /* unit: msec*/
	uint32_t  u4NoaCount;
	uint8_t   ucBssIdx;
	uint8_t   aucReserved[3];
};

struct CMD_CUSTOM_OPPPS_PARAM_STRUCT {
	uint32_t  u4CTwindowMs;
	/* bit0~6 : CTWindow(unit: TU), bit7:OppPS bit (1:enable, 0:disable)*/
	uint8_t   ucBssIdx;
	uint8_t   aucReserved[3];
};

struct CMD_CUSTOM_UAPSD_PARAM_STRUCT {
	uint8_t  fgEnAPSD;
	uint8_t  fgEnAPSD_AcBe;
	uint8_t  fgEnAPSD_AcBk;
	uint8_t  fgEnAPSD_AcVo;
	uint8_t  fgEnAPSD_AcVi;
	uint8_t  ucMaxSpLen;
	uint8_t  aucResv[2];
};

/* Definition for CHANNEL_INFO.ucBand:
 * 0:       Reserved
 * 1:       BAND_2G4
 * 2:       BAND_5G
 * Others:  Reserved
 */
struct CHANNEL_INFO {
	uint8_t  ucBand;
	uint8_t  ucChannelNum;
};

struct CMD_SCAN_REQ {
	uint8_t          ucSeqNum;
	uint8_t          ucBssIndex;
	uint8_t          ucScanType;
	uint8_t          ucSSIDType;
	uint8_t          ucSSIDLength;
	uint8_t          ucNumProbeReq;
	uint16_t         u2ChannelMinDwellTime;
	uint16_t         u2ChannelDwellTime;
	uint16_t         u2TimeoutValue;
	uint8_t          aucSSID[32];
	uint8_t          ucChannelType;
	uint8_t          ucChannelListNum;
	uint8_t          aucReserved[2];
	struct CHANNEL_INFO  arChannelList[32];
	uint16_t         u2IELen;
	uint8_t          aucIE[0];  /*depends on u2IELen*/
};

struct PARAM_SSID {
	uint32_t  u4SsidLen;
	/*!< SSID length in bytes. Zero length is broadcast(any) SSID */
	uint8_t   aucSsid[ELEM_MAX_LEN_SSID];
};

#define CMD_SCAN_REQ_V2_FUNC_RANDOM_MAC_MASK        BIT(0)
#define CMD_SCAN_REQ_V2_FUNC_DIS_DBDC_SCAN_MASK     BIT(1)
#define CMD_SCAN_REQ_V2_FUNC_DBDC_SCAN_TYPE_3_MASK  BIT(2)
/* use 6*4 = 24 bytes as bssid of being scanned ap */
#define CMD_SCAN_REQ_V2_FUNC_USE_PADDING_AS_BSSID	BIT(3)
#define CMD_SCAN_REQ_V2_FUNC_RANDOM_PROBE_REQ_SN_MASK	BIT(4)

struct CMD_SCAN_REQ_V2 {
	uint8_t          ucSeqNum;
	uint8_t          ucBssIndex;
	uint8_t          ucScanType;
	uint8_t          ucSSIDType;
	uint8_t          ucSSIDNum;
	uint8_t          ucNumProbeReq;
	uint8_t          ucScnFuncMask;
	uint8_t          auVersion[1];
	struct PARAM_SSID    arSSID[4];
	uint16_t         u2ProbeDelayTime;
	uint16_t         u2ChannelDwellTime;
	uint16_t         u2TimeoutValue;
	uint8_t          ucChannelType;
	uint8_t          ucChannelListNum;
	struct CHANNEL_INFO  arChannelList[32];
	uint16_t         u2IELen;
	uint8_t          aucIE[600];  /*depends on u2IELen*/
	uint8_t          ucChannelListExtNum;
	uint8_t          ucSSIDExtNum;
	uint16_t         u2ChannelMinDwellTime;
	struct CHANNEL_INFO  arChannelListExtend[32];
	struct PARAM_SSID    arSSIDExtend[6];
	uint8_t          aucBSSID[MAC_ADDR_LEN];
	uint8_t          aucRandomMac[MAC_ADDR_LEN];
	uint8_t          aucPadding_3[64];
};

/* TLV for CMD_ID_SCAN_REQ_V2*/
enum ENUM_CMD_ID_SCAN_REQ_V2_TAG_ID {
	CMD_ID_SCAN_REQ_V2_TAG_00_TBD = 0,
	CMD_ID_SCAN_REQ_V2_TAG_01_BSSID_AND_RANDOM_MAC = 1,
	CMD_ID_SCAN_REQ_V2_TAG_ID_NUM
};

struct CMD_ID_SCAN_REQ_V2_TAG_01_BSSID_AND_RANDOM_MAC {
	uint8_t aucRandomMacAddr[6];
	uint8_t aucBSSID[6];
};


struct CMD_SCAN_CANCEL {
	uint8_t          ucSeqNum;
	uint8_t          ucIsExtChannel;     /* For P2P channel extension. */
	uint8_t          aucReserved[2];
};

struct CMD_P2P_SEC_CHECK {
	uint32_t u4KeyId;
	uint8_t  aucBuffer[32];
};


struct CMD_RESET_BA_SCOREBOARD {
	uint8_t  ucflag;
	uint8_t  ucTID;
	uint8_t  aucMacAddr[MAC_ADDR_LEN];
};


struct CMD_IPV4_NETWORK_ADDRESS_V0 {
	uint8_t      aucIpAddr[IPV4_ADDR_LEN];
};

struct CMD_IPV4_NETWORK_ADDRESS {
	uint8_t      aucIpAddr[IPV4_ADDR_LEN];
	uint8_t      aucIpMask[IPV4_ADDR_LEN];
};

struct CMD_SET_NETWORK_ADDRESS_LIST {
	uint8_t      ucBssIndex;
	uint8_t      ucAddressCount;
	uint8_t      ucVersion;
	uint8_t      ucReserved[1];
	struct CMD_IPV4_NETWORK_ADDRESS arNetAddress[1];
};

struct CMD_IPV6_NETWORK_ADDRESS {
	uint8_t aucIpAddr[IPV6_ADDR_LEN];
};

struct CMD_IPV6_NETWORK_ADDRESS_LIST {
	uint8_t  ucBssIndex;
	uint8_t  ucAddressCount;
	uint8_t  ucReserved[2];
	struct CMD_IPV6_NETWORK_ADDRESS arNetAddress[1];
};

struct CMD_SET_RRM_CAPABILITY {
	/* DWORD_0 - Common Part*/
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen;
	/* Cmd size including common part and body */

	/* DWORD_1 afterwards - Command Body*/
	uint8_t  ucBssIndex;
	uint8_t  ucRrmEnable;                /* 802.11k rrm flag */
	uint8_t  ucCapabilities[5];
	/* Table 7-43e, RRM Enabled Capabilities Field */
	uint8_t  aucPadding1[1];

	uint8_t  aucPadding2[32];            /* for new param in the future */
};

struct CMD_SET_AP_CONSTRAINT_PWR_LIMIT {
	/* DWORD_0 - Common Part*/
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen;
	/* Cmd size including common part and body */

	/* DWORD_1 afterwards - Command Body*/
	uint8_t  ucBssIndex;
	uint8_t  ucPwrSetEnable;
	int8_t   cMaxTxPwr;              /* In unit of 0.5 dBm (signed) */
	int8_t   cMinTxPwr;              /* In unit of 0.5 dBm (signed) */

	uint8_t  aucPadding1[32];        /* for new param in the future */
};

/* RTT */
struct CMD_RTT_REQ {
	uint8_t          ucSeqNum;
	uint8_t          ucBssIndex;
	uint8_t          ucRttType;
	uint8_t          ucRttRole;
	bool         fgRttTrigger;
	uint8_t          ucNumRttReq;
	uint16_t         u2UpdatePeriodIn10MS;
};

struct CMD_RTT_CALIBR {
	uint8_t  aucMacAddr[MAC_ADDR_LEN];
	uint8_t  ucNumRttMeas;
	uint8_t  ucReserved;
};

struct CMD_SET_SCHED_SCAN_ENABLE {
	uint8_t      ucSchedScanAct;  /*ENUM_SCHED_SCAN_ACT*/
	uint8_t      aucReserved[3];
};

struct SCAN_SCHED_SSID_MATCH_SETS {
	uint32_t  cRssiThresold;
	uint8_t    aucSsid[32];
	uint8_t    u4SsidLen;
	uint8_t    aucPadding_1[3];
};

struct CMD_SCAN_SCHED_REQ {
	uint8_t    ucVersion;
	uint8_t    ucSeqNum;
	/*    Fw SCHED SCAN DONE after stop    */
	uint8_t    fgStopAfterIndication;
	uint8_t    ucSsidNum;
	uint8_t    ucMatchSsidNum;
	uint8_t    aucPadding_0;
	uint16_t  u2IELen;
	/*    Send prob request SSID set    */
	struct PARAM_SSID    auSsid[10];
	/*    Match SSID set    */
	struct SCAN_SCHED_SSID_MATCH_SETS    auMatchSsid[16];
	uint8_t ucChannelType;
	uint8_t ucChnlNum;
	uint8_t ucMspEntryNum;
	uint8_t ScnFuncMask;
	struct CHANNEL_INFO aucChannel[64];
	/*    SCHED SCN Interval    */
	uint16_t au2MspList[10];
	uint8_t aucPadding_3[64];
	uint8_t aucIE[0];
};

enum WIFI_SCAN_EVENT {
	WIFI_SCAN_BUFFER_FULL = 0,
	WIFI_SCAN_COMPLETE,
};

struct CMD_HIF_CTRL {
	uint8_t          ucHifType;
	uint8_t          ucHifDirection;
	uint8_t          ucHifStop;
	uint8_t          ucHifSuspend;
	uint8_t          aucReserved2[32];
};

struct CMD_MU_EDCA_PARAMS {
	uint8_t ucECWmin;
	uint8_t ucECWmax;
	uint8_t ucAifsn;
	uint8_t ucIsACMSet;
	uint8_t ucMUEdcaTimer;
	uint8_t aucPadding[3];
};

struct CMD_MQM_UPDATE_MU_EDCA_PARMS {
	/* DWORD_0 - Common Part*/
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen;       /* Cmd size including common part and body */

	/* DWORD_1 afterwards - Command Body*/
	uint8_t ucBssIndex;
	uint8_t fgIsQBSS;
	uint8_t ucWmmSet;
	uint8_t aucPadding1[1];

	struct CMD_MU_EDCA_PARAMS arMUEdcaParams[4];  /* number of ACs */
	uint8_t aucPadding2[32];
};

struct CMD_RLM_UPDATE_SR_PARMS {
	/* DWORD_0 - Common Part*/
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen;       /* Cmd size including common part and body */

	/* DWORD_1 afterwards - Command Body*/
	uint8_t  ucBssIndex;
	uint8_t  ucSRControl;
	uint8_t  ucNonSRGObssPdMaxOffset;
	uint8_t  ucSRGObssPdMinOffset;
	uint8_t  ucSRGObssPdMaxOffset;
	uint8_t  aucPadding1[3];
	uint32_t u4SRGBSSColorBitmapLow;
	uint32_t u4SRGBSSColorBitmapHigh;
	uint32_t u4SRGPartialBSSIDBitmapLow;
	uint32_t u4SRGPartialBSSIDBitmapHigh;

	uint8_t aucPadding2[32];
};

struct CMD_MDDP_FILTER_RULE {
	/* DWORD_0 - Common Part*/
	uint8_t  ucCmdVer;
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen;       /* Cmd size including common part and body */

	/* DWORD_1 afterwards - Command Body*/
	uint8_t  ucPfType;
	uint8_t  ucPfNum;
	uint8_t  aucPadding1[2];
	uint8_t  aucWhPfClsFilterMddp[0];
};

/*---------------------------------------------------------------------------*/
/* EVENT Packets                                                          */
/*---------------------------------------------------------------------------*/

/* event of tkip mic error */
struct EVENT_MIC_ERR_INFO {
	uint32_t     u4Flags;
	uint8_t      aucStaAddr[6];
};

/* event of add key done for port control */
struct EVENT_ADD_KEY_DONE_INFO {
	uint8_t      ucBSSIndex;
	uint8_t      ucReserved;
	uint8_t      aucStaAddr[6];
};

struct LINK_QUALITY {
	int8_t       cRssi; /* AIS Network. */
	int8_t       cLinkQuality;
	uint16_t     u2LinkSpeed;            /* TX rate1 */
	uint8_t      ucMediumBusyPercentage; /* Read clear */
	uint8_t      ucIsLQ0Rdy;
	/* Link Quality BSS0 Ready. */
	uint8_t      aucReserve[2];
};

struct EVENT_LINK_QUALITY {
	struct LINK_QUALITY rLq[4];
	/*Refer to BSS_INFO_NUM, but event should not use HW realted
	 * definition
	 */
};

struct EVENT_ACTIVATE_STA_REC {
	uint8_t      aucMacAddr[6];
	uint8_t      ucStaRecIdx;
	uint8_t      ucBssIndex;
};

struct EVENT_DEACTIVATE_STA_REC {
	uint8_t      ucStaRecIdx;
	uint8_t      aucReserved[3];
};

struct EVENT_BUG_REPORT {

	/* BugReportVersion */
	uint32_t     u4BugReportVersion;

	/* FW Module State */
	uint32_t     u4FWState; /*LP, roaming*/
	uint32_t     u4FWScanState;
	uint32_t     u4FWCnmState;

	/* Scan Counter */
	uint32_t     u4ReceivedBeaconCount;
	uint32_t     u4ReceivedProbeResponseCount;
	uint32_t     u4SentProbeRequestCount;
	uint32_t     u4SentProbeRequestFailCount;

	/* Roaming Counter */
	uint32_t     u4RoamingDebugFlag;
	uint32_t     u4RoamingThreshold;
	uint32_t     u4RoamingCurrentRcpi;

	/* RF Counter */
	uint32_t     u4RFPriChannel;
	uint32_t     u4RFChannelS1;
	uint32_t     u4RFChannelS2;
	uint32_t     u4RFChannelWidth;
	uint32_t     u4RFSco;

	/* Coex Counter */
	uint32_t     u4BTProfile;
	uint32_t     u4BTOn;
	uint32_t     u4LTEOn;

	/* Low Power Counter */
	uint32_t     u4LPTxUcPktNum;
	uint32_t     u4LPRxUcPktNum;
	uint32_t     u4LPPSProfile;

	/* Base Band Counter */
	uint32_t     u4OfdmPdCnt;
	uint32_t     u4CckPdCnt;
	uint32_t     u4CckSigErrorCnt;
	uint32_t     u4CckSfdErrorCnt;
	uint32_t     u4OfdmSigErrorCnt;
	uint32_t     u4OfdmTaqErrorCnt;
	uint32_t     u4OfdmFcsErrorCnt;
	uint32_t     u4CckFcsErrorCnt;
	uint32_t     u4OfdmMdrdyCnt;
	uint32_t     u4CckMdrdyCnt;
	uint32_t     u4PhyCcaStatus;
	uint32_t     u4WifiFastSpiStatus;

	/* Mac RX Counter */
	uint32_t     u4RxMdrdyCount;
	uint32_t     u4RxFcsErrorCount;
	uint32_t     u4RxbFifoFullCount;
	uint32_t     u4RxMpduCount;
	uint32_t     u4RxLengthMismatchCount;
	uint32_t     u4RxCcaPrimCount;
	uint32_t     u4RxEdCount;

	uint32_t     u4LmacFreeRunTimer;
	uint32_t     u4WtblReadPointer;
	uint32_t     u4RmacWritePointer;
	uint32_t     u4SecWritePointer;
	uint32_t     u4SecReadPointer;
	uint32_t     u4DmaReadPointer;

	/* Mac TX Counter */
	uint32_t     u4TxChannelIdleCount;
	uint32_t     u4TxCcaNavTxTime;

};

struct CMD_GET_STATISTICS {
/* DWORD_0 - Common Part*/
	uint8_t  ucCmdVer;
	/* if the structure size is changed, the ucCmdVer shall be increased.*/
	uint8_t  aucPadding0[1];
	uint16_t u2CmdLen;       /* cmd size including common part and body.*/
	/** general use */
	uint8_t  ucGeneralFlags;
	/** TRUE: ucBssIndex is valid */
	uint8_t  ucBssIndexValid;
	uint8_t  ucBssIndex;
	uint8_t  aucPadding1[1];
	/** Request the value for specific BSS */
	uint8_t  aucPadding2[64];
};

struct EVENT_STATISTICS {
	/* Link quality for customer */
	union LARGE_INTEGER rTransmittedFragmentCount;
	union LARGE_INTEGER rMulticastTransmittedFrameCount;
	union LARGE_INTEGER rFailedCount;
	union LARGE_INTEGER rRetryCount;
	union LARGE_INTEGER rMultipleRetryCount;
	union LARGE_INTEGER rRTSSuccessCount;
	union LARGE_INTEGER rRTSFailureCount;
	union LARGE_INTEGER rACKFailureCount;
	union LARGE_INTEGER rFrameDuplicateCount;
	union LARGE_INTEGER rReceivedFragmentCount;
	union LARGE_INTEGER rMulticastReceivedFrameCount;
	union LARGE_INTEGER rFCSErrorCount;
	union LARGE_INTEGER rMdrdyCnt;
	union LARGE_INTEGER rChnlIdleCnt;
	uint32_t u4HwMacAwakeDuration;
	uint32_t au4Padding3[15];

	/* wifi_radio_stat */
	int32_t      i4RadioIdx;
	uint32_t u4OnTime;
	uint32_t u4TxTime;
	uint32_t u4NumTxLevels; /* for au4TxTimePerLevels */
	uint32_t u4RxTime;
	uint32_t u4OnTimeScan;
	uint32_t u4OnTimeNbd;
	uint32_t u4OnTimeGscan;
	uint32_t u4OnTimeRoamScan;
	uint32_t u4OnTimePnoScan;
	uint32_t u4OnTimeHs20;
	uint32_t u4NumChannels;
	uint32_t au4TxTimePerLevels[78];

	/* wifi_iface_stat */
	int32_t      i4IfaceIdx;
	uint32_t u4NumWifiInterfaceLinkLayerInfo;
	uint32_t u4BeaconRx;
	uint32_t u4AverageTsfOffsetHigh;
	uint32_t u4AverageTsfOffsetLow;
	uint32_t u4LeakyApDetected;
	uint32_t u4LeakyApAvgNumFramesLeaked;
	uint32_t u4Leaky_ap_guard_time;
	uint32_t u4MgmtRx;
	uint32_t u4MgmtActionRx;
	uint32_t u4MgmtActionTx;
	int32_t      i4RssiMgmt;
	int32_t      i4RssiData;
	int32_t      i4RssiAck;
	uint32_t u4NumAc;
	uint32_t u4NumPeers;

	/* wifi_interface_link_layer_info */
	int16_t      i2Mode;
	uint8_t      aucMacAddr[6];
	int16_t      i2State;
	int16_t      i2Roaming;
	uint32_t u4Capabilities;
	uint8_t      aucSsid[33];
	uint8_t      aucPadding4[1];
	uint8_t      aucBssid[6];
	uint8_t      aucPadding5[2];
	uint8_t      aucApCountryStr[3];
	uint8_t      aucPadding6[1];
	uint8_t      aucCountryStr[3];
	uint8_t      aucPadding7[1];

	/* wifi_wmm_ac_stat: VO */
	uint32_t u4VoTxMpdu;
	uint32_t u4VoRxMpdu;
	uint32_t u4VoTxMcast;
	uint32_t u4VoRxMcast;
	uint32_t u4VoRxAmpdu;
	uint32_t u4VoTxAmpdu;
	uint32_t u4VoMpduLost;
	uint32_t u4VoRetries;
	uint32_t u4VoRetriesShort;
	uint32_t u4VoRetriesLong;
	uint32_t u4VoContentionTimeMin;
	uint32_t u4VoContentionTimeMax;
	uint32_t u4VoContentionTimeAvg;
	uint32_t u4VoContentionNumSamples;

	/* wifi_wmm_ac_stat: VI */
	uint32_t u4ViTxMpdu;
	uint32_t u4ViRxMpdu;
	uint32_t u4ViTxMcast;
	uint32_t u4ViRxMcast;
	uint32_t u4ViRxAmpdu;
	uint32_t u4ViTxAmpdu;
	uint32_t u4ViMpduLost;
	uint32_t u4ViRetries;
	uint32_t u4ViRetriesShort;
	uint32_t u4ViRetriesLong;
	uint32_t u4ViContentionTimeMin;
	uint32_t u4ViContentionTimeMax;
	uint32_t u4ViContentionTimeAvg;
	uint32_t u4ViContentionNumSamples;

	/* wifi_wmm_ac_stat: BE */
	uint32_t u4BeTxMpdu;
	uint32_t u4BeRxMpdu;
	uint32_t u4BeTxMcast;
	uint32_t u4BeRxMcast;
	uint32_t u4BeRxAmpdu;
	uint32_t u4BeTxAmpdu;
	uint32_t u4BeMpduLost;
	uint32_t u4BeRetries;
	uint32_t u4BeRetriesShort;
	uint32_t u4BeRetriesLong;
	uint32_t u4BeContentionTimeMin;
	uint32_t u4BeContentionTimeMax;
	uint32_t u4BeContentionTimeAvg;
	uint32_t u4BeContentionNumSamples;

	/* wifi_wmm_ac_stat: BK */
	uint32_t u4BkTxMpdu;
	uint32_t u4BkRxMpdu;
	uint32_t u4BkTxMcast;
	uint32_t u4BkRxMcast;
	uint32_t u4BkRxAmpdu;
	uint32_t u4BkTxAmpdu;
	uint32_t u4BkMpduLost;
	uint32_t u4BkRetries;
	uint32_t u4BkRetriesShort;
	uint32_t u4BkRetriesLong;
	uint32_t u4BkContentionTimeMin;
	uint32_t u4BkContentionTimeMax;
	uint32_t u4BkContentionTimeAvg;
	uint32_t u4BkContentionNumSamples;

	uint32_t au4PaddingTail[128];
};

struct EVENT_SCAN_DONE {
	uint8_t          ucSeqNum;
	uint8_t          ucSparseChannelValid;
	struct CHANNEL_INFO  rSparseChannel;
	uint8_t          ucCompleteChanCount;
	uint8_t          ucCurrentState;
	uint8_t          ucScanDoneVersion;
	uint8_t          ucReserved;
	uint32_t         u4ScanDurBcnCnt;
	uint8_t          fgIsPNOenabled;
	uint8_t          aucReserving[3];
	/*channel idle count # Mike */
	uint8_t          ucSparseChannelArrayValidNum;
	uint8_t          aucReserved[3];
	uint8_t          aucChannelNum[SCAN_DONE_EVENT_MAX_CHANNEL_NUM];
	/* Idle format for au2ChannelIdleTime */
	/* 0: first bytes: idle time(ms) 2nd byte: dwell time(ms) */
	/* 1: first bytes: idle time(8ms) 2nd byte: dwell time(8ms) */
	/* 2: dwell time (16us) */
	uint16_t         au2ChannelIdleTime[SCAN_DONE_EVENT_MAX_CHANNEL_NUM];
	/* Beacon and Probe Response Count in each Channel  */
	uint8_t          aucChannelBAndPCnt[SCAN_DONE_EVENT_MAX_CHANNEL_NUM];
	/* Mdrdy Count in each Channel  */
	uint8_t          aucChannelMDRDYCnt[SCAN_DONE_EVENT_MAX_CHANNEL_NUM];
	uint32_t         u4ScanDurBcnCnt2G4;
	uint32_t         u4ScanDurBcnCnt5G;
	uint16_t         au2ChannelScanTime[SCAN_DONE_EVENT_MAX_CHANNEL_NUM];
};

struct EVENT_CH_PRIVILEGE {
	uint8_t          ucBssIndex;
	uint8_t          ucTokenID;
	uint8_t          ucStatus;
	uint8_t          ucPrimaryChannel;
	uint8_t          ucRfSco;
	uint8_t          ucRfBand;
	uint8_t          ucRfChannelWidth;
	/* To support 80/160MHz bandwidth */
	uint8_t          ucRfCenterFreqSeg1;
	/* To support 80/160MHz bandwidth */
	uint8_t          ucRfCenterFreqSeg2;
	/* To support 80/160MHz bandwidth */
	uint8_t          ucReqType;
	uint8_t          ucDBDCBand; /* ENUM_CMD_REQ_DBDC_BAND_T*/
	uint8_t          aucReserved;
	uint32_t         u4GrantInterval;    /* In unit of ms */
	uint8_t          aucReserved2[8];
};

enum ENUM_BEACON_TIMEOUT_REASON {
	BEACON_TIMEOUT_REASON_HW_BEACON_LOST_NONADHOC,
	BEACON_TIMEOUT_REASON_HW_BEACON_LOST_ADHOC,
	BEACON_TIMEOUT_REASON_HW_TSF_DRIFT,
	BEACON_TIMEOUT_REASON_NULL_FRAME_THRESHOLD,
	BEACON_TIMEOUT_REASON_AGING_THRESHOLD,
	BEACON_TIMEOUT_REASON_BSSID_BEACON_PEIROD_NOT_ILLIGAL,
	BEACON_TIMEOUT_REASON_CONNECTION_FAIL,
	BEACON_TIMEOUT_REASON_ALLOCAT_NULL_PKT_FAIL_THRESHOLD,
	BEACON_TIMEOUT_REASON_NO_TX_DONE_EVENT,
	BEACON_TIMEOUT_REASON_UNSPECIF_REASON,
	BEACON_TIMEOUT_REASON_SET_CHIP,
	BEACON_TIMEOUT_REASON_KEEP_SCAN_AP_MISS_CHECK_FAIL,
	BEACON_TIMEOUT_REASON_KEEP_UNCHANGED_LOW_RSSI_CHECK_FAIL,
	BEACON_TIMEOUT_REASON_NULL_FRAME_LIFE_TIMEOUT,
	BEACON_TIMEOUT_REASON_HIGH_PER,
	BEACON_TIMEOUT_REASON_NUM
};

struct EVENT_BSS_BEACON_TIMEOUT {
	uint8_t      ucBssIndex;
	uint8_t      ucReasonCode;
	uint8_t      aucReserved[2];
};

struct EVENT_STA_AGING_TIMEOUT {
	uint8_t      ucStaRecIdx;
	uint8_t      aucReserved[3];
};

struct EVENT_NOA_TIMING {
	uint8_t      ucIsInUse;
	/* Indicate if this entry is in use or not */
	uint8_t      ucCount;                /* Count */
	uint8_t      aucReserved[2];

	uint32_t     u4Duration;             /* Duration */
	uint32_t     u4Interval;             /* Interval */
	uint32_t     u4StartTime;            /* Start Time */
};

struct EVENT_UPDATE_NOA_PARAMS {
	uint8_t      ucBssIndex;
	uint8_t      aucReserved[2];
	uint8_t      ucEnableOppPS;
	uint16_t     u2CTWindow;

	uint8_t              ucNoAIndex;
	uint8_t              ucNoATimingCount; /* Number of NoA Timing */
	struct EVENT_NOA_TIMING  arEventNoaTiming[8/*P2P_MAXIMUM_NOA_COUNT*/];
};

struct EVENT_AP_OBSS_STATUS {
	uint8_t      ucBssIndex;
	uint8_t      ucObssErpProtectMode;
	uint8_t      ucObssHtProtectMode;
	uint8_t      ucObssGfOperationMode;
	uint8_t      ucObssRifsOperationMode;
	uint8_t      ucObssBeaconForcedTo20M;
	uint8_t      aucReserved[2];
};

struct EVENT_RDD_STATUS {
	uint8_t      ucRddStatus;
	uint8_t      aucReserved[3];
};

struct EVENT_NLO_DONE {
	uint8_t      ucSeqNum;
	uint8_t      ucStatus;   /* 0: Found / other: reserved */
	uint8_t      aucReserved[2];
};

/* RTT */
struct RTT_PEER {
	uint8_t      aucBssid[MAC_ADDR_LEN];
	uint16_t     u2CalibrInNS;
	uint32_t     u4RangeInNS;
	uint32_t     u4AccuracyInNS;

	uint32_t     u4RangeInCM;
	uint32_t     u4AccuracyInCM;
	uint32_t *pu4APLon;
	uint32_t *pu4APLat;
	PARAM_RSSI              rRssi;
};

#define PARAM_MAX_RTT_PEERS    8
struct EVENT_RTT_UPDATE_RANGE {
	uint8_t      ucNumPeer;
	uint8_t      aucReserved[3];
	struct RTT_PEER    arPeers[PARAM_MAX_RTT_PEERS];
	uint16_t     au2Event[25];
	uint16_t     u2Timer;
};
struct EVENT_RTT_UPDATE_LOCATION {
	uint8_t      ucNumPeer;
	uint8_t      aucReserved[3];
	struct RTT_PEER    arPeers[PARAM_MAX_RTT_PEERS];
	uint16_t     au2Event[25];
	uint16_t     u2Timer;
};

struct EVENT_RTT_DISCOVER_PEER {
	uint16_t     au2Event[25];
	uint16_t     u2Timer;
};

struct EVENT_BUILD_DATE_CODE {
	uint8_t      aucDateCode[16];
};

struct EVENT_BATCH_RESULT_ENTRY {
	uint8_t      aucBssid[MAC_ADDR_LEN];
	uint8_t      aucSSID[ELEM_MAX_LEN_SSID];
	uint8_t      ucSSIDLen;
	int8_t       cRssi;
	uint32_t     ucFreq;
	uint32_t     u4Age;
	uint32_t     u4Dist;
	uint32_t     u4Distsd;
};

struct EVENT_BATCH_RESULT {
	uint8_t      ucScanCount;
	uint8_t      aucReserved[3];
	struct EVENT_BATCH_RESULT_ENTRY arBatchResult[12];
	/* Must be the same with SCN_BATCH_STORE_MAX_NUM*/
};

struct CMD_SET_FILTER_COEFFICIENT {
	uint32_t          u4BssIndex;
	int32_t           i4FilterCoefficient;
	uint8_t           aucReserved[8];
};

#define BA_FW_DROP_SN_BMAP_SZ 8
struct EVENT_BA_FW_DROP_RECORD {
	uint8_t  ucStaRecIdx;
	uint8_t  ucTid;
	uint16_t u2DropSnStart;
	uint32_t u4DropReason;
	uint8_t  aucSnBmap[BA_FW_DROP_SN_BMAP_SZ];
	uint8_t  aucDropLastAmsduSnBmap[BA_FW_DROP_SN_BMAP_SZ];
	uint8_t  aucReserved[4];
};

struct EVENT_BA_FW_DROP_SN {
	uint8_t  ucEvtVer;
	uint8_t  aucPadding0[1];
	uint16_t ucEvtLen;
	uint8_t  ucRecordNum;
	uint8_t  aucPadding1[3];
	struct EVENT_BA_FW_DROP_RECORD arBaFwDropRecord[8];
	/*CFG_NUM_OF_RX_BA_AGREEMENTS*/
};

/* NDIS_MEDIA_STATE */
enum ENUM_PARAM_MEDIA_STATE {
	MEDIA_STATE_CONNECTED = 0,
	MEDIA_STATE_DISCONNECTED,
	MEDIA_STATE_ROAMING_DISC_PREV,
	/* transient disconnected state for normal/fast roamming purpose */
	MEDIA_STATE_TO_BE_INDICATED,
	MEDIA_STATE_NUM
};

enum ENUM_LOG_UI_LVL {
	ENUM_LOG_UI_LVL_DEFAULT = 0,
	ENUM_LOG_UI_LVL_MORE = 1,
	ENUM_LOG_UI_LVL_EXTREME = 2,
};

struct CMD_EVENT_LOG_UI_INFO {
	uint32_t ucVersion; /* default is 1*/
	uint32_t ucLogLevel; /* 0: Default, 1: More, 2: Extreme*/
	uint8_t aucReserved[4]; /*reserved*/
};

struct CMD_WAKE_HIF {
	/* use in-band signal to wakeup system, ENUM_HIF_TYPE */
	uint8_t  ucWakeupHif;
	uint8_t  ucGpioPin;      /* GPIO Pin */
	uint8_t  ucTriggerLvl;   /* GPIO Pin */
	uint8_t  aucResv1[1];
	/* 0: low to high, 1: high to low */
	uint32_t u4GpioInterval;
	uint8_t  aucResv2[4];
};

struct WOW_PORT {
	uint8_t  ucIPv4UdpPortCnt;
	uint8_t  ucIPv4TcpPortCnt;
	uint8_t  ucIPv6UdpPortCnt;
	uint8_t  ucIPv6TcpPortCnt;
	uint16_t ausIPv4UdpPort[MAX_TCP_UDP_PORT];
	uint16_t ausIPv4TcpPort[MAX_TCP_UDP_PORT];
	uint16_t ausIPv6UdpPort[MAX_TCP_UDP_PORT];
	uint16_t ausIPv6TcpPort[MAX_TCP_UDP_PORT];
};

struct CMD_WOWLAN_PARAM {
	uint8_t  ucCmd;
	uint8_t  ucDetectType;
	uint16_t u2FilterFlag; /* ARP/MC/DropExceptMagic/SendMagicToHost */
	uint8_t  ucScenarioID; /* WOW/WOBLE/Proximity */
	uint8_t  ucBlockCount;
	uint8_t  aucReserved1[2];
	struct CMD_WAKE_HIF   astWakeHif[2];
	struct WOW_PORT   stWowPort;
	uint8_t  aucReserved2[32];
};

/*Oshare mode*/
#define CMD_OSHARE_LENGTH_MAX      64
struct CMD_OSHARE {
	uint8_t      ucCmdVersion;       /* CMD version = OSHARE_CMD_V1*/
	uint8_t      ucCmdType;
	uint8_t      ucMagicCode;
	/* It's like CRC, OSHARE_MAGIC_CODE*/
	uint8_t      ucCmdBufferLen;    /*buffer length <= 64*/
	uint8_t      aucBuffer[CMD_OSHARE_LENGTH_MAX];
};

struct EVENT_MDDP_FILTER_RULE {
	/* DWORD_0 - Common Part*/
	uint8_t  ucEvtVer;
	uint8_t  aucPadding0[1];
	uint16_t u2EvtLen;

	/* DWORD_1 - afterwards*/
	uint8_t  ucPfType;
	uint8_t  ucPfNum;
	uint8_t  aucPadding1[2];
	uint32_t au4PfStatusBitmap[64];
};

struct CMD_DOMAIN_CHANNEL {
	uint16_t u2ChNum;
	uint8_t  aucPadding[2];
	uint32_t eFlags; /*enum ieee80211_channel_flags*/
};

struct CMD_DOMAIN_ACTIVE_CHANNEL_LIST {
	uint8_t u1ActiveChNum2g;
	uint8_t u1ActiveChNum5g;
	uint8_t aucPadding[2];
	struct CMD_DOMAIN_CHANNEL arChannels[0];
};

struct CMD_SET_DOMAIN_INFO_V2 {
	/* DWORD_0 - Country code*/
	uint32_t u4CountryCode;

	/* DWORD_1 - 2.4G & 5G BW info*/
	uint8_t  uc2G4Bandwidth; /* CONFIG_BW_20_40M or CONFIG_BW_20M */
	uint8_t  uc5GBandwidth;  /* CONFIG_BW_20_40M or CONFIG_BW_20M */
	uint8_t  aucPadding[2];

	/* DWORD_2 ~ - 2.4G & 5G active channel info*/
	struct CMD_DOMAIN_ACTIVE_CHANNEL_LIST arActiveChannels;
};

#define SINGLE_SKU_PARAM_NUM 69
struct CMD_SKU_TABLE_TYPE {
	int8_t i1PwrLimit[SINGLE_SKU_PARAM_NUM];
};

struct CMD_TXPOWER_CHANNEL_POWER_LIMIT_PER_RATE {
	uint8_t u1CentralCh;
	struct CMD_SKU_TABLE_TYPE aucTxPwrLimit;
};

struct CMD_SET_TXPOWER_COUNTRY_TX_POWER_LIMIT_PER_RATE {
	/* DWORD_0 - Common info*/
	uint8_t ucCmdVer;
	uint8_t aucPadding0[1];
	uint16_t u2CmdLen;

	/* DWORD_1 - CMD hint*/
	uint8_t ucNum; /* channel #*/
	uint8_t eBand; /* 2.4g or 5g*/
	uint8_t bCmdFinished;
	/* hint for whether 2.4g/5g tx power limit value all be sent*/
	uint8_t aucPadding1[1];

	/* DWORD_2 - Country code*/
	uint32_t u4CountryCode;

	/* WORD_3 ~ 10 - Padding*/
	uint8_t aucPadding2[32];

	/* DWORD_11 ~ - Tx power limit values*/
	struct CMD_TXPOWER_CHANNEL_POWER_LIMIT_PER_RATE rChannelPowerLimit[0];
};

#define POWER_LIMIT_TXBF_BACKOFF_PARAM_NUM 6
struct CMD_TXPWR_TXBF_CHANNEL_BACKOFF {
	uint8_t ucCentralCh;
	uint8_t aucPadding0[1];
	int8_t acTxBfBackoff[POWER_LIMIT_TXBF_BACKOFF_PARAM_NUM];
};

#define CMD_POWER_LIMIT_TABLE_SUPPORT_CHANNEL_NUM 64
struct CMD_TXPWR_TXBF_SET_BACKOFF {
	uint8_t ucCmdVer;
	uint8_t aucPadding0[1];
	uint16_t u2CmdLen;
	uint8_t ucNum;
	uint8_t ucBssIdx;
	uint8_t aucPadding1[2];
	uint8_t aucPadding2[32];
	struct CMD_TXPWR_TXBF_CHANNEL_BACKOFF
		rChannelTxBfBackoff[CMD_POWER_LIMIT_TABLE_SUPPORT_CHANNEL_NUM];
};

/*  The DBDC band request from driver.
 *  So far, the actual used DBDC band is decdied in firmware by channel.
 *  Driver should use ENUM_CMD_REQ_BAND_AUTO only.
 */
enum ENUM_CMD_REQ_DBDC_BAND {
	ENUM_CMD_REQ_BAND_0    = 0,
	ENUM_CMD_REQ_BAND_1    = 1,
	ENUM_CMD_REQ_BAND_ALL  = 3,
	ENUM_CMD_REQ_BAND_AUTO = 4,
	ENUM_CMD_REQ_BAND_NUM  /*Just for checking.*/
};

enum ENUM_EVENT_OPMODE_CHANGE_REASON {
	EVENT_OPMODE_CHANGE_REASON_DBDC         = 0,
	EVENT_OPMODE_CHANGE_REASON_COANT        = 1,
	EVENT_OPMODE_CHANGE_REASON_DBDC_SCAN    = 2,
	EVENT_OPMODE_CHANGE_REASON_SMARTGEAR    = 3,
	EVENT_OPMODE_CHANGE_REASON_COEX         = 4,
	EVENT_OPMODE_CHANGE_REASON_SMARTGEAR_1T2R    = 5,
};

struct EVENT_OPMODE_CHANGE {
	/* DWORD_0 - Common Part*/
	uint8_t  ucEvtVer;
	uint8_t  aucPadding0[1];
	uint16_t u2EvtLen;

	uint8_t  ucBssBitmap;    /*Bit[3:0]*/
	uint8_t  ucEnable;       /*Enable OpTxRx limitation/change*/
	uint8_t  ucOpTxNss;      /*0: don't care*/
	uint8_t  ucOpRxNss;      /*0: don't care*/

	uint8_t  ucReason;       /* ENUM_EVENT_OPMODE_CHANGE_REASON_T*/
	uint8_t  aucPadding1[63];
};

#define EVENT_GET_CNM_BAND_NUM          2  /* ENUM_BAND_NUM*/
#define EVENT_GET_CNM_MAX_OP_CHNL_NUM   3  /* MAX_OP_CHNL_NUM*/
#define EVENT_GET_CNM_MAX_BSS_NUM       4  /* BSS_INFO_NUM*/
#define EVENT_GET_CNM_AMX_BSS_FULL_NUM  5  /* BSS_INFO_FULL_NUM*/
struct EVENT_GET_CNM {
	uint8_t  fgIsDbdcEnable;

	uint8_t  ucOpChNum[EVENT_GET_CNM_BAND_NUM];
	uint8_t  ucChList[EVENT_GET_CNM_BAND_NUM]
		[EVENT_GET_CNM_MAX_OP_CHNL_NUM];
	uint8_t  ucChBw[EVENT_GET_CNM_BAND_NUM]
		[EVENT_GET_CNM_MAX_OP_CHNL_NUM];
	uint8_t  ucChSco[EVENT_GET_CNM_BAND_NUM]
		[EVENT_GET_CNM_MAX_OP_CHNL_NUM];
	uint8_t  ucChNetNum[EVENT_GET_CNM_BAND_NUM]
		[EVENT_GET_CNM_MAX_OP_CHNL_NUM];
	uint8_t  ucChBssList[EVENT_GET_CNM_BAND_NUM]
		[EVENT_GET_CNM_MAX_OP_CHNL_NUM][EVENT_GET_CNM_MAX_BSS_NUM];
	/* BAND/CH/BSS */

	uint8_t  ucBssInuse[EVENT_GET_CNM_AMX_BSS_FULL_NUM];
	uint8_t  ucBssActive[EVENT_GET_CNM_AMX_BSS_FULL_NUM];
	uint8_t  ucBssConnectState[EVENT_GET_CNM_AMX_BSS_FULL_NUM];

	uint8_t  ucBssCh[EVENT_GET_CNM_AMX_BSS_FULL_NUM];
	uint8_t  ucBssDBDCBand[EVENT_GET_CNM_AMX_BSS_FULL_NUM];
	uint8_t  ucBssWmmSet[EVENT_GET_CNM_AMX_BSS_FULL_NUM];
	uint8_t  ucBssWmmDBDCBand[EVENT_GET_CNM_AMX_BSS_FULL_NUM];
	uint8_t  ucBssOMACSet[EVENT_GET_CNM_AMX_BSS_FULL_NUM];
	uint8_t  ucBssOMACDBDCBand[EVENT_GET_CNM_AMX_BSS_FULL_NUM];

	/* Reserved fields */
	uint8_t  au4Reserved[68]; /*Total 164 byte*/
};

struct CMD_SET_FORCE_RTS {
	uint8_t ucForceRtsEn;
	uint8_t ucRtsPktNum;
	uint8_t aucReserved[2];
};

#endif /* _WSYS_CMD_HANDLER_FW_H */


