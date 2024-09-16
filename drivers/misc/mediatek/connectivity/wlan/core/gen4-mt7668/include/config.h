/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/config.h#3
*/

/*! \file   "config.h"
*   \brief  This file includes the various configurable parameters for customers
*
*    This file ncludes the configurable parameters except the parameters indicate the turning-on/off of some features
*/

#ifndef _CONFIG_H
#define _CONFIG_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* 2 Flags for OS capability */

#if defined(_HIF_SDIO)
#ifdef LINUX
#ifdef CONFIG_X86
#define MTK_WCN_HIF_SDIO        0
#else
#define MTK_WCN_HIF_SDIO        0
#endif
#else
#define MTK_WCN_HIF_SDIO            0
#endif
#else
#define MTK_WCN_HIF_SDIO	0
#endif

/* Android build-in driver switch, Mike 2016/11/11*/
#ifndef CFG_BUILT_IN_DRIVER
#define CFG_BUILT_IN_DRIVER         0
#endif

/* Mike 2016/09/01 ALPS update K3.18 80211_disconnect to K4.4 version*/
/* work around for any alps K3.18 platform*/
#ifndef CFG_WPS_DISCONNECT
#define CFG_WPS_DISCONNECT         0
#endif


#if (CFG_SUPPORT_AEE == 1)
#define CFG_ENABLE_AEE_MSG          1
#else
#define CFG_ENABLE_AEE_MSG          0
#endif

#define CFG_SUPPORT_MTK_ANDROID_KK      1

#define CFG_ENABLE_EARLY_SUSPEND        0
#define CFG_ENABLE_NET_DEV_NOTIFY		1

/* 2 Flags for Driver Features */
#define CFG_TX_FRAGMENT                 1	/*!< 1: Enable TX fragmentation */
						/* 0: Disable */
#define CFG_SUPPORT_PERFORMANCE_TEST    0	/*Only for performance Test */

#define CFG_COUNTRY_CODE                NULL	/* "US" */
#define CFG_SUPPORT_DEBUG_FS			0
#ifndef LINUX
#define CFG_FW_FILENAME                 L"WIFI_RAM_CODE"
#define CFG_CR4_FW_FILENAME             L"WIFI_RAM_CODE2"
#else
#define CFG_FW_FILENAME                 "WIFI_RAM_CODE"
#define CFG_CR4_FW_FILENAME             "WIFI_RAM_CODE2"
#endif

#ifndef CFG_MET_PACKET_TRACE_SUPPORT
#define CFG_MET_PACKET_TRACE_SUPPORT    0	/*move to wlan/MAKEFILE */
#endif

#ifndef CFG_MET_TAG_SUPPORT
#define CFG_MET_TAG_SUPPORT             0
#endif

/*------------------------------------------------------------------------------
 * Driver config
 *------------------------------------------------------------------------------
 */

#ifndef LINUX
#define CFG_SUPPORT_CFG_FILE     0
#else
#define CFG_SUPPORT_CFG_FILE     1
#endif

#define CFG_SUPPORT_802_11D             1	/*!< 1(default): Enable 802.11d */
						/* 0: Disable */

#define CFG_SUPPORT_RRM             0	/* Radio Reasource Measurement (802.11k) */
#ifndef CFG_SUPPORT_DFS
#define CFG_SUPPORT_DFS             1	/* DFS (802.11h) */
#endif
#ifndef CFG_SUPPORT_DFS_MASTER
#define CFG_SUPPORT_DFS_MASTER      1
#endif

#if (CFG_SUPPORT_DFS == 1)	/* Add by Enlai */
#define CFG_SUPPORT_QUIET           0	/* Quiet (802.11h) */
#define CFG_SUPPORT_SPEC_MGMT       1	/* Spectrum Management (802.11h): TPC and DFS */
#else
#define CFG_SUPPORT_QUIET           0	/* Quiet (802.11h) */
#define CFG_SUPPORT_SPEC_MGMT       0	/* Spectrum Management (802.11h): TPC and DFS */
#endif

#define CFG_SUPPORT_RX_RDG          0	/* 11n feature. RX RDG capability */
#define CFG_SUPPORT_MFB             0	/* 802.11n MCS Feedback responder */
#define CFG_SUPPORT_RX_STBC         1	/* 802.11n RX STBC (1SS) */
#define CFG_SUPPORT_RX_SGI          1	/* 802.11n RX short GI for both 20M and 40M BW */
#define CFG_SUPPORT_RX_HT_GF        1	/* 802.11n RX HT green-field capability */
#define CFG_SUPPORT_BFER            1
#define CFG_SUPPORT_BFEE            1
#define CFG_SUPPORT_WAPI            1

/* Enable QA Tool Support */
#define CFG_SUPPORT_QA_TOOL			1

/* Enable TX BF Support */
#define CFG_SUPPORT_TX_BF			1

/* Enable MU MIMO Support */
#define CFG_SUPPORT_MU_MIMO			1

/* Enable WOW Support */
#define CFG_WOW_SUPPORT			1

/* Enable A-MSDU RX Reordering Support */
#define CFG_SUPPORT_RX_AMSDU		1

/* Enable Android wake_lock operations */
#ifdef CONFIG_HAS_WAKELOCK
#ifndef CFG_ENABLE_WAKE_LOCK
#define CFG_ENABLE_WAKE_LOCK		1
#endif

#else
#undef CFG_ENABLE_WAKE_LOCK
#define CFG_ENABLE_WAKE_LOCK		0
#endif

/*------------------------------------------------------------------------------
 * Flags of Buffer mode SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_BUFFER_MODE                 1

/*------------------------------------------------------------------------------
 * SLT Option
 *------------------------------------------------------------------------------
 */
#define CFG_SLT_SUPPORT                             0

#ifdef NDIS60_MINIPORT
#define CFG_NATIVE_802_11                       1

#define CFG_TX_MAX_PKT_SIZE                     2304
#define CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60       0	/* !< 1: Enable TCP/IP header checksum offload */
							/* 0: Disable */
#define CFG_TCP_IP_CHKSUM_OFFLOAD               0
#define CFG_WHQL_DOT11_STATISTICS               1
#define CFG_WHQL_ADD_REMOVE_KEY                 1
#define CFG_WHQL_CUSTOM_IE                      1
#define CFG_WHQL_SAFE_MODE_ENABLED              1

#else
#define CFG_TCP_IP_CHKSUM_OFFLOAD               1
#define CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60       0
#define CFG_TX_MAX_PKT_SIZE                     1600
#define CFG_NATIVE_802_11                       0
#endif

/* 2 Flags for Driver Parameters */
/*------------------------------------------------------------------------------
 * Flags for EHPI Interface in Colibri Platform
 *------------------------------------------------------------------------------
 */
/*!< 1: Do workaround for faster bus timing */
/* 0(default): Disable */
#define CFG_EHPI_FASTER_BUS_TIMING                  0

/*------------------------------------------------------------------------------
 * Flags for UMAC
 *------------------------------------------------------------------------------
 */
#define CFG_UMAC_GENERATION                         0x20

/*------------------------------------------------------------------------------
 * Flags for HIFSYS Interface
 *------------------------------------------------------------------------------
 */
#ifdef _lint
/* #define _HIF_SDIO   1 */
#endif

/* 1(default): Enable SDIO ISR & TX/RX status enhance mode
 * 0: Disable
 */
#define CFG_SDIO_INTR_ENHANCE                        1
/* 1(default): Enable SDIO ISR & TX/RX status enhance mode
 * 0: Disable
 */
#define CFG_SDIO_RX_ENHANCE                          1
/* 1: Enable SDIO TX enhance mode(Multiple frames in single BLOCK CMD)
 * 0(default): Disable
 */
#define CFG_SDIO_TX_AGG                              1

/* 1: Enable SDIO RX enhance mode(Multiple frames in single BLOCK CMD)
 * 0(default): Disable
 */
#define CFG_SDIO_RX_AGG                              1

/* 1: Enable SDIO RX Tasklet De-Aggregation
 * 0(default): Disable
 */
#ifndef CFG_SDIO_RX_AGG_TASKLET
#define CFG_SDIO_RX_AGG_TASKLET			0
#endif

#if (CFG_SDIO_RX_AGG == 1) && (CFG_SDIO_INTR_ENHANCE == 0)
#error "CFG_SDIO_INTR_ENHANCE should be 1 once CFG_SDIO_RX_AGG equals to 1"
#elif (CFG_SDIO_INTR_ENHANCE == 1 || CFG_SDIO_RX_ENHANCE == 1) && (CFG_SDIO_RX_AGG == 0)
#error "CFG_SDIO_RX_AGG should be 1 once CFG_SDIO_INTR_ENHANCE and/or CFG_SDIO_RX_ENHANCE equals to 1"
#endif

#ifdef WINDOWS_CE
#define CFG_SDIO_PATHRU_MODE                    1	/*!< 1: Support pass through (PATHRU) mode */
							/* 0: Disable */
#else
#define CFG_SDIO_PATHRU_MODE                    0	/*!< 0: Always disable if WINDOWS_CE is not defined */
#endif

#define CFG_SDIO_ACCESS_N9_REGISTER_BY_MAILBOX      0
#define CFG_MAX_RX_ENHANCE_LOOP_COUNT               3

#define CFG_USB_TX_AGG                              1
#define CFG_USB_CONSISTENT_DMA                      0
#define CFG_USB_TX_HANDLE_IN_HIF_THREAD             0
#define CFG_USB_RX_HANDLE_IN_HIF_THREAD             0

#ifndef CFG_TX_DIRECT_USB
#define CFG_TX_DIRECT_USB                           1
#endif
#ifndef CFG_RX_DIRECT_USB
#define CFG_RX_DIRECT_USB                           1
#endif

#define CFG_HW_WMM_BY_BSS                           1
/*------------------------------------------------------------------------------
 * Flags and Parameters for Integration
 *------------------------------------------------------------------------------
 */

#define CFG_MULTI_ECOVER_SUPPORT    1

#define CFG_ENABLE_CAL_LOG          1
#define CFG_REPORT_RFBB_VERSION     1

#define HW_BSSID_NUM                4	/* HW BSSID number by chip */
#define HW_WMM_NUM                  4	/* HW WMM number by chip */

#ifndef CFG_CHIP_RESET_SUPPORT
#define CFG_CHIP_RESET_SUPPORT          0
#endif

/*------------------------------------------------------------------------------
 * Flags for workaround
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Flags for driver version
 *------------------------------------------------------------------------------
 */
#define CFG_DRV_OWN_VERSION                     ((UINT_16)((NIC_DRIVER_MAJOR_VERSION << 8) | \
	(NIC_DRIVER_MINOR_VERSION)))
#define CFG_DRV_PEER_VERSION                    ((UINT_16)0x0000)

/*------------------------------------------------------------------------------
 * Flags and Parameters for TX path
 *------------------------------------------------------------------------------
 */

/*! Maximum number of SW TX packet queue */
#define CFG_TX_MAX_PKT_NUM                      1024

/*! Maximum number of SW TX CMD packet buffer */
#define CFG_TX_MAX_CMD_PKT_NUM                  32

/*------------------------------------------------------------------------------
 * Flags and Parameters for RX path
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
* CONFIG_TITLE : Move BA from FW to Driver
* OWNER            : Puff Wen
* Description  : Move BA from FW to Driver
*------------------------------------------------------------------------------
*/
#define CFG_M0VE_BA_TO_DRIVER                   0

/*! Max. descriptor number - sync. with firmware */
#if CFG_SLT_SUPPORT
#define CFG_NUM_OF_RX0_HIF_DESC                 42
#else
#define CFG_NUM_OF_RX0_HIF_DESC                 16
#endif
#define CFG_NUM_OF_RX1_HIF_DESC                 2

/*! Max. buffer hold by QM */
#define CFG_NUM_OF_QM_RX_PKT_NUM                HIF_NUM_OF_QM_RX_PKT_NUM

/*! Maximum number of SW RX packet buffer */
#define CFG_RX_MAX_PKT_NUM                      ((CFG_NUM_OF_RX0_HIF_DESC + CFG_NUM_OF_RX1_HIF_DESC) * 3 \
						+ CFG_NUM_OF_QM_RX_PKT_NUM)

#define CFG_RX_REORDER_Q_THRESHOLD              8

#ifndef LINUX
#define CFG_RX_RETAINED_PKT_THRESHOLD           (CFG_NUM_OF_RX0_HIF_DESC + CFG_NUM_OF_RX1_HIF_DESC \
						+ CFG_NUM_OF_QM_RX_PKT_NUM)
#else
#define CFG_RX_RETAINED_PKT_THRESHOLD           0
#endif

/*! Maximum RX packet size, if exceed this value, drop incoming packet */
/* 7.2.3 Maganement frames */
/* TODO: it should be 4096 under emulation mode */
#define CFG_RX_MAX_PKT_SIZE                     (28 + 2312 + 12 /*HIF_RX_HEADER_T*/)

/*! Minimum RX packet size, if lower than this value, drop incoming packet */
#define CFG_RX_MIN_PKT_SIZE                     10	/*!< 802.11 Control Frame is 10 bytes */

/*! RX BA capability */
#define CFG_NUM_OF_RX_BA_AGREEMENTS             8
#if CFG_M0VE_BA_TO_DRIVER
#define CFG_RX_BA_MAX_WINSIZE                   64
#endif
#define CFG_RX_BA_INC_SIZE                      64
#define CFG_RX_MAX_BA_TID_NUM                   8
#define CFG_RX_REORDERING_ENABLED               1

#define CFG_PF_ARP_NS_MAX_NUM                   3

#define CFG_COMPRESSION_DEBUG					0
#define CFG_DECOMPRESSION_TMP_ADDRESS			0
#define CFG_SUPPORT_COMPRESSION_FW_OPTION		0

/*------------------------------------------------------------------------------
 * Flags and Parameters for CMD/RESPONSE
 *------------------------------------------------------------------------------
 */
#define CFG_RESPONSE_POLLING_TIMEOUT            1000
#define CFG_RESPONSE_POLLING_DELAY              5

/*------------------------------------------------------------------------------
 * Flags and Parameters for Protocol Stack
 *------------------------------------------------------------------------------
 */
/*! Maximum number of BSS in the SCAN list */
#define CFG_MAX_NUM_BSS_LIST                    192

#define CFG_MAX_COMMON_IE_BUF_LEN         ((1500 * CFG_MAX_NUM_BSS_LIST) / 3)

/*! Maximum size of Header buffer of each SCAN record */
#define CFG_RAW_BUFFER_SIZE                      1024

/*! Maximum size of IE buffer of each SCAN record */
#define CFG_IE_BUFFER_SIZE                      800

/*------------------------------------------------------------------------------
 * Flags and Parameters for Power management
 *------------------------------------------------------------------------------
 */
#define CFG_ENABLE_FULL_PM                      1
#define CFG_ENABLE_WAKEUP_ON_LAN                0

#define CFG_INIT_POWER_SAVE_PROF                    ENUM_PSP_FAST_SWITCH

#define CFG_INIT_ENABLE_PATTERN_FILTER_ARP                    0

#define CFG_INIT_UAPSD_AC_BMP                    0	/* (BIT(3) | BIT(2) | BIT(1) | BIT(0)) */

/* #define CFG_SUPPORT_WAPI                        0 */
#define CFG_SUPPORT_WPS                          1
#define CFG_SUPPORT_WPS2                         1

/*------------------------------------------------------------------------------
 * 802.11i RSN Pre-authentication PMKID cahce maximun number
 *------------------------------------------------------------------------------
 */
/*!< max number of PMKID cache 16(default) : The Max PMKID cache */
#define CFG_MAX_PMKID_CACHE                     16

/*------------------------------------------------------------------------------
  * Auto Channel Selection maximun channel number
  *------------------------------------------------------------------------------
  */
#define MAX_CHN_NUM                             39 /* ARRAY_SIZE(mtk_5ghz_channels) + ARRAY_SIZE(mtk_2ghz_channels) */
#define MAX_2G_BAND_CHN_NUM                     14
#define MAX_5G_BAND_CHN_NUM                     (MAX_CHN_NUM - MAX_2G_BAND_CHN_NUM)

/*------------------------------------------------------------------------------
 * Flags and Parameters for Ad-Hoc
 *------------------------------------------------------------------------------
 */
#define CFG_INIT_ADHOC_FREQ                     (2462000)
#define CFG_INIT_ADHOC_MODE                     AD_HOC_MODE_MIXED_11BG
#define CFG_INIT_ADHOC_BEACON_INTERVAL          (100)
#define CFG_INIT_ADHOC_ATIM_WINDOW              (0)

/*------------------------------------------------------------------------------
 * Flags and Parameters for Maximum Scan SSID number
 *------------------------------------------------------------------------------
 */
#define CFG_SCAN_SSID_MAX_NUM                   (4)
#define CFG_SCAN_SSID_MATCH_MAX_NUM             (16)

/*------------------------------------------------------------------------------
 * Flags and Parameters for Load Setup Default
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Flags for enable 802.11A Band setting
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Flags and Parameters for Interrupt Process
 *------------------------------------------------------------------------------
 */
#define CFG_IST_LOOP_COUNT                      HIF_IST_LOOP_COUNT

#define CFG_INT_WRITE_CLEAR                     0

/* 2 Flags for Driver Debug Options */
/*------------------------------------------------------------------------------
 * Flags of TX Debug Option. NOTE(Kevin): Confirm with SA before modifying following flags.
 *------------------------------------------------------------------------------
 */
/*!< 1: Debug statistics usage of MGMT Buffer */
/* 0: Disable */
#define CFG_DBG_MGT_BUF                         1

#define CFG_HIF_STATISTICS                      0

#define CFG_HIF_RX_STARVATION_WARNING           0

#define CFG_RX_PKTS_DUMP                        0

#define CFG_ASSERT_DUMP                         1
/*------------------------------------------------------------------------------
 * Flags of Firmware Download Option.
 *------------------------------------------------------------------------------
 */
#define CFG_ENABLE_FW_DOWNLOAD                  1

#define CFG_ENABLE_FW_DOWNLOAD_ACK              1

/*------------------------------------------------------------------------------
 * Flags of Bluetooth-over-WiFi (BT 3.0 + HS) support
 *------------------------------------------------------------------------------
 */

#define CFG_ENABLE_BT_OVER_WIFI             0

#define CFG_BOW_SEPARATE_DATA_PATH              1

#define CFG_BOW_PHYSICAL_LINK_NUM               4

#define CFG_BOW_LIMIT_AIS_CHNL                  1

#define CFG_BOW_SUPPORT_11N                     1

#define CFG_BOW_RATE_LIMITATION                 1

/*------------------------------------------------------------------------------
 * Flags of Wi-Fi Direct support
 *------------------------------------------------------------------------------
 */
/*------------------------------------------------------------------------------
 * Support reporting all BSS networks to cfg80211 kernel when scan
 * request is from P2P interface
 * Originally only P2P networks will be reported when scan request is from p2p0
 *------------------------------------------------------------------------------
 */
#ifndef CFG_P2P_SCAN_REPORT_ALL_BSS
#define CFG_P2P_SCAN_REPORT_ALL_BSS            0
#endif

/*------------------------------------------------------------------------------
 * Flags for GTK rekey offload
 *------------------------------------------------------------------------------
 */

#ifdef LINUX
#ifdef CONFIG_X86
#define CFG_ENABLE_WIFI_DIRECT          1
#define CFG_SUPPORT_802_11W             1
#define CONFIG_SUPPORT_GTK_REKEY        1
#else
#define CFG_ENABLE_WIFI_DIRECT          1
#define CFG_SUPPORT_802_11W             1	/*!< 0(default): Disable 802.11W */
#define CONFIG_SUPPORT_GTK_REKEY        1
#endif
#else /* !LINUX */
#define CFG_ENABLE_WIFI_DIRECT           0
#define CFG_SUPPORT_802_11W              0	/* Not support at WinXP */
#endif /* LINUX */

#define CFG_SUPPORT_PERSISTENT_GROUP            0

#define CFG_TEST_WIFI_DIRECT_GO                 0

#define CFG_TEST_ANDROID_DIRECT_GO              0

#define CFG_UNITEST_P2P                         0

/*
 * Enable cfg80211 option after Android 2.2(Froyo) is suggested,
 * cfg80211 on linux 2.6.29 is not mature yet
 */
#define CFG_ENABLE_WIFI_DIRECT_CFG_80211        1

#define CFG_SUPPORT_HOTSPOT_OPTIMIZATION        0
#define CFG_HOTSPOT_OPTIMIZATION_BEACON_INTERVAL 300
#define CFG_HOTSPOT_OPTIMIZATION_DTIM           1
#define CFG_AUTO_CHANNEL_SEL_SUPPORT            1

/*------------------------------------------------------------------------------
 * Configuration Flags (Linux Only)
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_EXT_CONFIG                  0

/*------------------------------------------------------------------------------
 * Statistics Buffering Mechanism
 *------------------------------------------------------------------------------
 */
#if CFG_SUPPORT_PERFORMANCE_TEST
#define CFG_ENABLE_STATISTICS_BUFFERING         1
#else
#define CFG_ENABLE_STATISTICS_BUFFERING         0
#endif
#define CFG_STATISTICS_VALID_CYCLE              2000
#define CFG_LINK_QUALITY_VALID_PERIOD           500

/*------------------------------------------------------------------------------
 * Migration Option
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_ADHOC                       1
#define CFG_SUPPORT_AAA                         1

#define CFG_SUPPORT_BCM                         0
#define CFG_SUPPORT_BCM_BWCS                    0
#define CFG_SUPPORT_BCM_BWCS_DEBUG              0

#define CFG_SUPPORT_RDD_TEST_MODE               0

#define CFG_SUPPORT_PWR_MGT                     1

#define CFG_ENABLE_HOTSPOT_PRIVACY_CHECK        1

#define CFG_MGMT_FRAME_HANDLING                 1

#define CFG_MGMT_HW_ACCESS_REPLACEMENT          0

#if CFG_SUPPORT_PERFORMANCE_TEST

#else

#endif

#define CFG_SUPPORT_AIS_5GHZ                    1
#define CFG_SUPPORT_BEACON_CHANGE_DETECTION     0

/*------------------------------------------------------------------------------
 * Option for NVRAM and Version Checking
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_NVRAM                       1
#define CFG_NVRAM_EXISTENCE_CHECK               1
#define CFG_SW_NVRAM_VERSION_CHECK              1
#define CFG_SUPPORT_NIC_CAPABILITY              1

/*------------------------------------------------------------------------------
 * CONFIG_TITLE : Stress Test Option
 * OWNER        : Puff Wen
 * Description  : For stress test only. DO NOT enable it while normal operation
 *------------------------------------------------------------------------------
 */
#define CFG_STRESS_TEST_SUPPORT                 0

/*------------------------------------------------------------------------------
 * Flags for LINT
 *------------------------------------------------------------------------------
 */
#define LINT_SAVE_AND_DISABLE	/*lint -save -e* */

#define LINT_RESTORE		/*lint -restore */

#define LINT_EXT_HEADER_BEGIN                   LINT_SAVE_AND_DISABLE

#define LINT_EXT_HEADER_END                     LINT_RESTORE

/*------------------------------------------------------------------------------
 * Flags of Features
 *------------------------------------------------------------------------------
 */

#define CFG_SUPPORT_PNO             1
#define CFG_SUPPORT_TDLS		1

#define CFG_SUPPORT_QOS             1	/* Enable/disable QoS TX, AMPDU */
#define CFG_SUPPORT_AMPDU_TX        1
#define CFG_SUPPORT_AMPDU_RX        1
#define CFG_SUPPORT_TSPEC           0	/* Enable/disable TS-related Action frames handling */
#define CFG_SUPPORT_UAPSD           1
#define CFG_SUPPORT_UL_PSMP         0

#ifndef CFG_SUPPORT_ROAMING
#define CFG_SUPPORT_ROAMING         1	/* Roaming System */
#endif
#if (CFG_SUPPORT_ROAMING == 1)

/* Roaming feature: skip roaming when only one ESSID AP
*  Need Android background scan
*  if no roaming event occurred
*  to trigger roaming scan
*  after skip roaming in one ESSID AP case
*/
#define CFG_SUPPORT_ROAMING_SKIP_ONE_AP		1
#if CFG_SUPPORT_ROAMING_SKIP_ONE_AP
#define CFG_MAX_NUM_ROAM_BSS_LIST		64
#endif
#else
#define CFG_SUPPORT_ROAMING_SKIP_ONE_AP		0

#endif /* CFG_SUPPORT_ROAMING */

#define CFG_SUPPORT_SWCR            1

#define CFG_SUPPORT_ANTI_PIRACY     1

#define CFG_SUPPORT_OSC_SETTING     1

#define CFG_SUPPORT_P2P_RSSI_QUERY        0

#define CFG_SHOW_MACADDR_SOURCE     1

#define CFG_SUPPORT_802_11V                    0	/* Support 802.11v Wireless Network Management */
#define CFG_SUPPORT_802_11V_TIMING_MEASUREMENT 0
#if (CFG_SUPPORT_802_11V_TIMING_MEASUREMENT == 1) && (CFG_SUPPORT_802_11V == 0)
#error "CFG_SUPPORT_802_11V should be 1 once CFG_SUPPORT_802_11V_TIMING_MEASUREMENT equals to 1"
#endif
#if (CFG_SUPPORT_802_11V == 0)
#define WNM_UNIT_TEST 0
#endif

#define CFG_DRIVER_COMPOSE_ASSOC_REQ        1
#define CFG_SUPPORT_802_11AC                1
#define CFG_STRICT_CHECK_CAPINFO_PRIVACY    0

#define CFG_SUPPORT_WFD                     1
#define CFG_SUPPORT_WFD_COMPOSE_IE          1

#define CFG_SUPPORT_HOTSPOT_WPS_MANAGER     1
#define CFG_SUPPORT_NFC_BEAM_PLUS           1

/* Refer to CONFIG_MTK_STAGE_SCAN */
#define CFG_MTK_STAGE_SCAN					1

#define CFG_SUPPORT_MULTITHREAD             1	/* Enable driver support multicore */

#define CFG_SUPPORT_MTK_SYNERGY             1

#define CFG_SUPPORT_PWR_LIMIT_COUNTRY       1

#define CFG_FIX_2_TX_PORT					0

#ifndef CFG_DISCONN_DEBUG_FEATURE
#define CFG_DISCONN_DEBUG_FEATURE      1
#define MAX_DISCONNECT_RECORD          5
#endif

/*------------------------------------------------------------------------------
 * Flags of bus error tolerance
 *------------------------------------------------------------------------------
 */
#define CFG_FORCE_RESET_UNDER_BUS_ERROR     0

/*------------------------------------------------------------------------------
 * Build Date Code Integration
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_BUILD_DATE_CODE         0

/*------------------------------------------------------------------------------
 * Flags of SDIO test pattern support
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_SDIO_READ_WRITE_PATTERN 1

/*------------------------------------------------------------------------------
 * Flags of AIS passive scan support
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_AIS_PASSIVE_SCAN        0

/*------------------------------------------------------------------------------
 * Flags of Workaround
 *------------------------------------------------------------------------------
 */
#define CFG_ENABLE_READ_EXTRA_4_BYTES       1

/* Temp workaround for 11ac certification */
#define CFG_WORKAROUND_OPMODE_CONFLICT_OPINFO	1


/*------------------------------------------------------------------------------
 * Flags of 5G NVRAM SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_NVRAM_5G                1

/*------------------------------------------------------------------------------
 * Flags of Packet Lifetime Profiling Mechanism
 *------------------------------------------------------------------------------
 */
#define CFG_ENABLE_PKT_LIFETIME_PROFILE     1
#define CFG_PRINT_PKT_LIFETIME_PROFILE      0

#define CFG_ENABLE_PER_STA_STATISTICS       1

/*------------------------------------------------------------------------------
 * Flags for prepare the FW compile flag
 *------------------------------------------------------------------------------
 */
#define COMPILE_FLAG0_GET_STA_LINK_STATUS     (1<<0)
#define COMPILE_FLAG0_WFD_ENHANCEMENT_PROTECT (1<<1)

/*------------------------------------------------------------------------------
 * Flags for prepare the FW feature flag
 *------------------------------------------------------------------------------
 */
#define FEATURE_FLAG0_NIC_CAPABILITY_V2 (1<<0)

/*------------------------------------------------------------------------------
 * Flags of Batch Scan SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_BATCH_SCAN              1
#define CFG_BATCH_MAX_MSCAN                 2

/*------------------------------------------------------------------------------
 * Flags of Sniffer SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_SNIFFER                 1

#define WLAN_INCLUDE_PROC                   1


/*------------------------------------------------------------------------------
 * Flags of Sniffer SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_DUAL_P2PLIKE_INTERFACE         1

#define RUNNING_P2P_MODE 0
#define RUNNING_AP_MODE 1
#define RUNNING_DUAL_AP_MODE 2
#define RUNNING_P2P_AP_MODE 3
#define RUNNING_P2P_MODE_NUM 4

/*------------------------------------------------------------------------------
 * Flags of MSP SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_MSP				1


/*------------------------------------------------------------------------------
 * Flags of Drop Packet Replay SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_REPLAY_DETECTION		1

/*------------------------------------------------------------------------------
 * Flags of Last Second MCS Tx/Rx Info
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_LAST_SEC_MCS_INFO	1
#if CFG_SUPPORT_LAST_SEC_MCS_INFO
#define MCS_INFO_SAMPLE_CNT			10
#endif


/*------------------------------------------------------------------------------
 * Flags of driver fw customization
 *------------------------------------------------------------------------------
 */

#define CFG_SUPPORT_EASY_DEBUG               1
#define CFG_SUPPORT_FW_DBG_LEVEL_CTRL        1


/*------------------------------------------------------------------------------
 * Flags of driver delay calibration atfer efuse buffer mode CMD
 *------------------------------------------------------------------------------
 */

#define CFG_EFUSE_BUFFER_MODE_DELAY_CAL         1

/*------------------------------------------------------------------------------
 * Flags for E1 IC workaround (SPI clock divided by 2)
 * Disabled for MT7668 since external PMIC is NOT used.
 * TBD: Let chip info structs decide.
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_PMIC_SPI_CLOCK_SWITCH       0

/*------------------------------------------------------------------------------
 * Flags of driver EEPROM pages for QA tool
 *------------------------------------------------------------------------------
 */

#define CFG_EEPROM_PAGE_ACCESS         1

/*------------------------------------------------------------------------------
 * Flags for HOST_OFFLOAD
 *------------------------------------------------------------------------------
 */

#define CFG_SUPPORT_WIFI_HOST_OFFLOAD	1

/*------------------------------------------------------------------------------
 * Flags for DBDC Feature
 *------------------------------------------------------------------------------
 */

#define CFG_SUPPORT_DBDC	1

/*------------------------------------------------------------------------------
 * Flags for Using TC4 Resource in ROM code stage
 *------------------------------------------------------------------------------
 */

#define CFG_USE_TC4_RESOURCE_FOR_INIT_CMD	0

/*------------------------------------------------------------------------------
 * Flags for Efuse Start and End address report from FW
 *------------------------------------------------------------------------------
 */

#define CFG_FW_Report_Efuse_Address	1

/*------------------------------------------------------------------------------
 * FW name max length
 *------------------------------------------------------------------------------
 */
#define CFG_FW_NAME_MAX_LEN	(64)

/*------------------------------------------------------------------------------
 * Chip low power debug function
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_LOW_POWER_DEBUG		1

/*------------------------------------------------------------------------------
 * Support WMT WIFI Path Config
 *------------------------------------------------------------------------------
 */
#define CFG_WMT_WIFI_PATH_SUPPORT	0

/*------------------------------------------------------------------------------
 * Support CFG_SISO_SW_DEVELOP
 *------------------------------------------------------------------------------
 */
#define CFG_SISO_SW_DEVELOP			1

/*------------------------------------------------------------------------------
 * Support antenna selection
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_ANT_SELECT		1

/*------------------------------------------------------------------------------
 * Flags for a Goal for MT6632 : Cal Result Backup in Host or NVRam when Android Boot
 *------------------------------------------------------------------------------
 */
#if 0 /*(MTK_WCN_HIF_SDIO) : 20161003 Default Off, later will enable by MTK_WCN_HIF_SDIO */
#define CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST				1
#define CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG		0
#else
#define CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST				0
#define CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST_DBGLOG		0
#endif

/*------------------------------------------------------------------------------
 * Enable SDIO 1-bit Data Mode. (Usually debug only)
 *------------------------------------------------------------------------------
 */
#define CFG_SDIO_1BIT_DATA_MODE			0

/*------------------------------------------------------------------------------
 * Single Sku
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_SINGLE_SKU	1
#ifndef CFG_SUPPORT_SINGLE_SKU_LOCAL_DB
#define CFG_SUPPORT_SINGLE_SKU_LOCAL_DB 1
#endif

/*------------------------------------------------------------------------------
 * Auto enable SDIO asynchronous interrupt mode
 *------------------------------------------------------------------------------
 */
#define CFG_SDIO_ASYNC_IRQ_AUTO_ENABLE	1


/*------------------------------------------------------------------------------
 * Direct Control for RF/PHY/BB/MAC for Manual Configuration via command/api
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_ADVANCE_CONTROL 1

/*------------------------------------------------------------------------------
 * Support GO and STA SIGNLE CHANNEL CONCURRENT
 *------------------------------------------------------------------------------
 */
#define GO_STA_SCC	0


/*------------------------------------------------------------------------------
 * Driver pre-allocate total size of memory in one time
 *------------------------------------------------------------------------------
 */
#ifndef CFG_PRE_ALLOCATION_IO_BUFFER
#define CFG_PRE_ALLOCATION_IO_BUFFER 0
#endif


/*------------------------------------------------------------------------------
 * Support scan with channels specified
 *------------------------------------------------------------------------------
 */
#ifndef CFG_SCAN_CHANNEL_SPECIFIED
#define CFG_SCAN_CHANNEL_SPECIFIED 1
#endif


/*------------------------------------------------------------------------------
 * Support EFUSE / EEPROM Auto Detect
 *------------------------------------------------------------------------------
 */
#ifndef CFG_EFUSE_AUTO_MODE_SUPPORT
#define CFG_EFUSE_AUTO_MODE_SUPPORT 1
#endif

#ifndef CFG_SUPPORT_CSI
#define CFG_SUPPORT_CSI 1
#endif



/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _CONFIG_H */
