/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

#ifdef MT6620
#undef MT6620
#endif

#ifndef MT6628
#define MT6628
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* 2 Flags for OS capability */

#define MTK_WCN_SINGLE_MODULE		0	/* 1: without WMT */

#ifdef LINUX
#ifdef CONFIG_X86
#define MTK_WCN_HIF_SDIO        0
#else
#define MTK_WCN_HIF_SDIO        0	/* samp */
#endif
#else
#define MTK_WCN_HIF_SDIO            0
#endif

#if (CFG_SUPPORT_AEE == 1)
#define CFG_ENABLE_AEE_MSG          1
#else
#define CFG_ENABLE_AEE_MSG          0
#endif

#if CFG_ENABLE_AEE_MSG
#include <mt-plat/aee.h>
#endif

/* 2 Flags for Driver Features */
#define CFG_TX_FRAGMENT                 1	/*
						 * !< 1: Enable TX fragmentation
						 * 0: Disable
						 */
#define CFG_SUPPORT_PERFORMANCE_TEST    0	/*Only for performance Test */

#define CFG_COUNTRY_CODE                NULL	/* "US" */

#ifndef LINUX
#define CFG_FW_FILENAME             L"WIFI_RAM_CODE"
#define CFG_FW_FILENAME_E6          L"WIFI_RAM_CODE_E6"
#else
#define CFG_FW_FILENAME             "WIFI_RAM_CODE"
#endif
#ifndef LINUX
#define CFG_SUPPORT_CFG_FILE     0
#else
#define CFG_SUPPORT_CFG_FILE     1
#endif

#define CFG_SUPPORT_FCC_DYNAMIC_TX_PWR_ADJUST 0  /* Support FCC/CE Dynamic Tx Power Adjust */

#define CFG_SUPPORT_CE_FCC_TXPWR_LIMIT 0 /* Support CE FCC Tx Power limit */

#define CFG_SUPPORT_802_11D             1	/*
						 * !< 1(default): Enable 802.11d
						 * 0: Disable
						 */

#define CFG_SUPPORT_RRM             0	/* Radio Reasource Measurement (802.11k) */
#define CFG_SUPPORT_DFS             1	/* DFS (802.11h) */

#if (CFG_SUPPORT_DFS == 1)	/* Add by Enlai */
#define CFG_SUPPORT_QUIET           1	/* Quiet (802.11h) */
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

#define CFG_SUPPORT_ROAMING_ENC		0	/* enahnced roaming */
#define CFG_SUPPORT_ROAMING_RETRY	1	/* enahnced roaming */

#define CFG_SUPPORT_TDLS			1	/* IEEE802.11z TDLS */
#define CFG_SUPPORT_TDLS_DBG		0	/* TDLS debug */
#define CFG_SUPPORT_STATISTICS		1
#define CFG_SUPPORT_DBG_POWERMODE	1	/* for debugging power always active mode */

#define CFG_SUPPORT_TXR_ENC			0	/* enhanced tx rate switch */

#define CFG_SUPPORT_PERSIST_NETDEV		0	/* create NETDEV when system bootup */

#define CFG_FORCE_USE_20BW			1

#define CFG_SUPPORT_RN				1

#define CFG_SUPPORT_SET_CAM_BY_PROC	1

#define CFG_SUPPORT_RSN_SCORE		0

#define CFG_SUPPORT_GAMING_MODE			1

#define CFG_SUPPORT_OSHARE			1

#define CFG_SUPPORT_WAPI			1

/*------------------------------------------------------------------------------
 * Flags of WPA3 support
 *------------------------------------------------------------------------------
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
#define CFG_SUPPORT_WPA3	0
#else
#define CFG_SUPPORT_WPA3	1
#endif

#if (CFG_SUPPORT_WPA3 == 1)
#define CFG_REFACTORY_PMKSA 1
#else
#define CFG_REFACTORY_PMKSA 0
#endif

/*------------------------------------------------------------------------------
 * Flags of Random MAC support
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_SCAN_RANDOM_MAC        1

/*------------------------------------------------------------------------------
 * SLT Option
 *------------------------------------------------------------------------------
 */
#define CFG_SLT_SUPPORT                         0

#ifdef NDIS60_MINIPORT

#define CFG_NATIVE_802_11                       1
#define CFG_TX_MAX_PKT_SIZE                     2304
#define CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60       0	/*
							 * !< 1: Enable TCP/IP header checksum offload
							 * 0: Disable
							 */
#define CFG_TCP_IP_CHKSUM_OFFLOAD               0
#define CFG_WHQL_DOT11_STATISTICS               1
#define CFG_WHQL_ADD_REMOVE_KEY                 1
#define CFG_WHQL_CUSTOM_IE                      1
#define CFG_WHQL_SAFE_MODE_ENABLED              1

#else

#define CFG_NATIVE_802_11                       0
#define CFG_TX_MAX_PKT_SIZE                     1600
#define CFG_TCP_IP_CHKSUM_OFFLOAD               1	/*
							 * !< 1: Enable TCP/IP header checksum offload
							 * 0: Disable
							 */
#define CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60       0

#endif

/* 2 Flags for Driver Parameters */
/*------------------------------------------------------------------------------
 * Flags for EHPI Interface in Colibri Platform
 *------------------------------------------------------------------------------
 */
#define CFG_EHPI_FASTER_BUS_TIMING                  0	/*
							 * !< 1: Do workaround for faster bus timing
							 * 0(default): Disable
							 */

/*------------------------------------------------------------------------------
 * Flags for HIFSYS Interface
 *------------------------------------------------------------------------------
 */
#ifdef _lint
#define _HIF_SDIO   0		/* samp */
#endif

#define CFG_SDIO_INTR_ENHANCE                        1	/*
							 * !< 1(default): Enable SDIO ISR & TX/RX status enhance mode
							 * 0: Disable
							 */
#define CFG_SDIO_RX_ENHANCE                          0	/*
							 * !< 1(default): Enable SDIO ISR & TX/RX status enhance mode
							 * 0: Disable
							 */
#define CFG_SDIO_TX_AGG                              1	/*
							 * !< 1: Enable SDIO TX enhance
							 * mode(Multiple frames in single BLOCK CMD)
							 * 0(default): Disable
							 */

#define CFG_SDIO_RX_AGG                              1	/*
							 * !< 1: Enable SDIO RX enhance
							 * mode(Multiple frames in single BLOCK CMD)
							 * 0(default): Disable
							 */

#if (CFG_SDIO_RX_AGG == 1) && (CFG_SDIO_INTR_ENHANCE == 0)
#error "CFG_SDIO_INTR_ENHANCE should be 1 once CFG_SDIO_RX_AGG equals to 1"
#elif (CFG_SDIO_INTR_ENHANCE == 1 || CFG_SDIO_RX_ENHANCE == 1) && (CFG_SDIO_RX_AGG == 0)
#error "CFG_SDIO_RX_AGG should be 1 once CFG_SDIO_INTR_ENHANCE and/or CFG_SDIO_RX_ENHANCE equals to 1"
#endif

#define CFG_SDIO_MAX_RX_AGG_NUM                     0	/*
							 * !< 1: Setting the maximum RX aggregation number
							 * 0(default): no limited
							 */

#ifdef WINDOWS_CE
#define CFG_SDIO_PATHRU_MODE                    1	/*
							 * !< 1: Support pass through (PATHRU) mode
							 * 0: Disable
							 */
#else
#define CFG_SDIO_PATHRU_MODE                    0	/*!< 0: Always disable if WINDOWS_CE is not defined */
#endif

#define CFG_MAX_RX_ENHANCE_LOOP_COUNT               3

/*------------------------------------------------------------------------------
 * Flags and Parameters for Integration
 *------------------------------------------------------------------------------
 */
#if defined(MT6620)
#define MT6620_FPGA_BWCS    0
#define MT6620_FPGA_V5      0

#if (MT6620_FPGA_BWCS == 1) && (MT6620_FPGA_V5 == 1)
#error
#endif

#if (MTK_WCN_HIF_SDIO == 1)
#define CFG_MULTI_ECOVER_SUPPORT    1
#elif !defined(LINUX)
#define CFG_MULTI_ECOVER_SUPPORT    1
#else
#define CFG_MULTI_ECOVER_SUPPORT    0
#endif

#define CFG_ENABLE_CAL_LOG      0
#define CFG_REPORT_RFBB_VERSION       0

#elif defined(MT6628)

#define CFG_MULTI_ECOVER_SUPPORT    0

#define CFG_ENABLE_CAL_LOG      1
#define CFG_REPORT_RFBB_VERSION       1

#endif

#define CFG_CHIP_RESET_SUPPORT                      1

#if defined(MT6628)
#define CFG_EMBED_FIRMWARE_BUILD_DATE_CODE  1
#endif

/*------------------------------------------------------------------------------
 * Flags for workaround
 *------------------------------------------------------------------------------
 */
#if defined(MT6620) && (MT6620_FPGA_BWCS == 0) && (MT6620_FPGA_V5 == 0)
#define MT6620_E1_ASIC_HIFSYS_WORKAROUND            0
#else
#define MT6620_E1_ASIC_HIFSYS_WORKAROUND            0
#endif

/* SPM issue: suspend current is higher than deep idle */
#define CFG_SPM_WORKAROUND_FOR_HOTSPOT                  1

/*------------------------------------------------------------------------------
 * Flags for driver version
 *------------------------------------------------------------------------------
 */
#define CFG_DRV_OWN_VERSION \
	((UINT_16)((NIC_DRIVER_MAJOR_VERSION << 8) | (NIC_DRIVER_MINOR_VERSION)))
#define CFG_DRV_PEER_VERSION                    ((UINT_16)0x0000)

/*------------------------------------------------------------------------------
 * Flags for TX path which are OS dependent
 *------------------------------------------------------------------------------
 */
/*! NOTE(Kevin): If the Network buffer is non-scatter-gather like structure(without
 * NETIF_F_FRAGLIST in LINUX), then we can set CFG_TX_BUFFER_IS_SCATTER_LIST to "0"
 * for zero copy TX packets.
 * For scatter-gather like structure, we set "1", driver will do copy frame to
 * internal coalescing buffer before write it to FIFO.
 */
#if defined(LINUX)
#define CFG_TX_BUFFER_IS_SCATTER_LIST       1	/*
						 * !< 1: Do frame copy before write to TX FIFO.
						 * Used when Network buffer is scatter-gather.
						 * 0(default): Do not copy frame
						 */
#else /* WINDOWS/WINCE */
#define CFG_TX_BUFFER_IS_SCATTER_LIST       1
#endif /* LINUX */

#if CFG_SDIO_TX_AGG || CFG_TX_BUFFER_IS_SCATTER_LIST
#define CFG_COALESCING_BUFFER_SIZE          (CFG_TX_MAX_PKT_SIZE * NIC_TX_BUFF_SUM)
#else
#define CFG_COALESCING_BUFFER_SIZE          (CFG_TX_MAX_PKT_SIZE)
#endif /* CFG_SDIO_TX_AGG || CFG_TX_BUFFER_IS_SCATTER_LIST */

/*------------------------------------------------------------------------------
 * Flags and Parameters for TX path
 *------------------------------------------------------------------------------
 */

/*! Maximum number of SW TX packet queue */
#define CFG_TX_MAX_PKT_NUM                      4096	/*
							 * 256 must >= CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD * 2;
							 * or wmm will fail when queue is full
							 */

/*! Maximum number of SW TX CMD packet buffer */
#define CFG_TX_MAX_CMD_PKT_NUM                  32

/*! Maximum number of associated STAs */
#define CFG_NUM_OF_STA_RECORD                   20

/*------------------------------------------------------------------------------
 * Flags and Parameters for RX path
 *------------------------------------------------------------------------------
 */

/*! Max. descriptor number - sync. with firmware */
#if CFG_SLT_SUPPORT
#define CFG_NUM_OF_RX0_HIF_DESC                 42
#else
#define CFG_NUM_OF_RX0_HIF_DESC                 16
#endif
#define CFG_NUM_OF_RX1_HIF_DESC                 2

/*! Max. buffer hold by QM */
#define CFG_NUM_OF_QM_RX_PKT_NUM                4096

/*! Maximum number of SW RX packet buffer */
#define CFG_RX_MAX_PKT_NUM                      ((CFG_NUM_OF_RX0_HIF_DESC + CFG_NUM_OF_RX1_HIF_DESC) * 3 \
						+ CFG_NUM_OF_QM_RX_PKT_NUM)

#define CFG_RX_REORDER_Q_THRESHOLD              8

#ifndef LINUX
#define CFG_RX_RETAINED_PKT_THRESHOLD \
	(CFG_NUM_OF_RX0_HIF_DESC + CFG_NUM_OF_RX1_HIF_DESC + CFG_NUM_OF_QM_RX_PKT_NUM)
#else
#define CFG_RX_RETAINED_PKT_THRESHOLD           0
#endif

/*! Maximum RX packet size, if exceed this value, drop incoming packet */
/* 7.2.3 Maganement frames */
#define CFG_RX_MAX_PKT_SIZE   (28 + 2312 + 12 /*HIF_RX_HEADER_T*/)	/*
									 * TODO: it should be
									 * 4096 under emulation mode
									 */

/*! Minimum RX packet size, if lower than this value, drop incoming packet */
#define CFG_RX_MIN_PKT_SIZE                     10	/*!< 802.11 Control Frame is 10 bytes */

#if CFG_SDIO_RX_AGG
    /* extra size for CS_STATUS and enhanced response */
#define CFG_RX_COALESCING_BUFFER_SIZE       ((CFG_NUM_OF_RX0_HIF_DESC  + 1) \
						* CFG_RX_MAX_PKT_SIZE)
#else
#define CFG_RX_COALESCING_BUFFER_SIZE       (CFG_RX_MAX_PKT_SIZE)
#endif

/*! RX BA capability */
#define CFG_NUM_OF_RX_BA_AGREEMENTS             8
#define CFG_RX_BA_MAX_WINSIZE                   16
#define CFG_RX_BA_INC_SIZE                      4
#define CFG_RX_MAX_BA_TID_NUM                   8
#define CFG_RX_REORDERING_ENABLED               1
#define CFG_RX_BA_REORDERING_ENHANCEMENT		1

/*------------------------------------------------------------------------------
 * Flags and Parameters for CMD/RESPONSE
 *------------------------------------------------------------------------------
 */
#define CFG_RESPONSE_POLLING_TIMEOUT            512

/*------------------------------------------------------------------------------
 * Flags and Parameters for Protocol Stack
 *------------------------------------------------------------------------------
 */
/*! Maximum number of BSS in the SCAN list */
#define CFG_MAX_NUM_BSS_LIST                    128

#define CFG_MAX_NUM_ROAM_BSS_LIST		64

#define CFG_MAX_COMMON_IE_BUF_LEN         ((1500 * CFG_MAX_NUM_BSS_LIST) / 3)

/*! Maximum size of Header buffer of each SCAN record */
#define CFG_RAW_BUFFER_SIZE                      1024

/*! Maximum size of IE buffer of each SCAN record */
#define CFG_IE_BUFFER_SIZE                      512

/*! Maximum number of STA records */
#define CFG_MAX_NUM_STA_RECORD                  32

#define CFG_GOOG_RCPI_THRESHOLD			90
#define CFG_POOR_RCPI_THRESHOLD			67
#define CFG_GOOG_RCPI_SCAN_SKIP_TIMES		3
#define CFG_POOR_RCPI_SCAN_SKIP_TIMES		2
/*------------------------------------------------------------------------------
 * Flags and Parameters for Power management
 *------------------------------------------------------------------------------
 */
#define CFG_ENABLE_FULL_PM                      1
#define CFG_ENABLE_WAKEUP_ON_LAN                0
#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M) || \
	defined(CONFIG_ARCH_MT6753) || defined(CONFIG_ARCH_MT6580)
#define CFG_SUPPORT_WAKEUP_REASON_DEBUG			1	/* debug which packet wake up host */
#else
#define CFG_SUPPORT_WAKEUP_REASON_DEBUG			0	/* debug which packet wake up host */
#endif
#define CFG_INIT_POWER_SAVE_PROF                    ENUM_PSP_FAST_SWITCH

#define CFG_INIT_ENABLE_PATTERN_FILTER_ARP                    0

#define CFG_INIT_UAPSD_AC_BMP                    0	/* (BIT(3) | BIT(2) | BIT(1) | BIT(0)) */

/* #define CFG_SUPPORT_WAPI                        0 */
#define CFG_SUPPORT_WPS                          1
#define CFG_SUPPORT_WPS2                         1

#if (CFG_REFACTORY_PMKSA == 0)
/*------------------------------------------------------------------------------
 * 802.11i RSN Pre-authentication PMKID cahce maximun number
 *------------------------------------------------------------------------------
 */
#define CFG_MAX_PMKID_CACHE                     16	/*
							 * !< max number of PMKID cache
							 * 16(default) : The Max PMKID cache
							 */
#endif
/*------------------------------------------------------------------------------
 * Auto Channel Selection maximun channel number
 *------------------------------------------------------------------------------
 */
#define MAX_CHN_NUM                             39 /* CH1~CH14, CH36~CH48, CH52~CH64, CH100~CH144, CH149~CH165 */
#define MAX_2G_BAND_CHN_NUM                     14

/*------------------------------------------------------------------------------
 * FAST SCAN
 *------------------------------------------------------------------------------
 */
#define CFG_ENABLE_FAST_SCAN                    0
#define CFG_CN_SUPPORT_CLASS121                 0 /* Add Class 121, 5470-5725MHz, support for China domain */
#if CFG_ENABLE_FAST_SCAN
	#define CFG_FAST_SCAN_DWELL_TIME                  40
	#define CFG_FAST_SCAN_REG_DOMAIN_DEF_IDX          10
#endif
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
#define CFG_MULTI_SSID_SCAN			1
#if CFG_TC1_FEATURE
#define CFG_NLO_MSP 1 /* NLO/PNO Multiple Scan Plan */
#else
#define CFG_NLO_MSP 0
#endif
#define CFG_SUPPORT_SCHED_SCN_SSID_SETS		1 /*Sched-scan support hidden SSID*/
#define CFG_SCAN_SSID_MAX_NUM                   (10)


#if CFG_SUPPORT_SCHED_SCN_SSID_SETS
#define CFG_SCAN_HIDDEN_SSID_MAX_NUM       (7)
#endif
#define CFG_SCAN_SSID_MATCH_MAX_NUM             (16)

#define CFG_SUPPORT_DETECT_ATHEROS_AP		0

#define CFG_SCAN_ABORT_HANDLE		1
/*------------------------------------------------------------------------------
 * Flags and Parameters for Support EMI DEBUG
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_EMI_DEBUG                   1


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
#if defined(_HIF_SDIO) && defined(WINDOWS_CE)
#define CFG_IST_LOOP_COUNT                  8
#else
#define CFG_IST_LOOP_COUNT                  8
#endif /* _HIF_SDIO */

#define CFG_INT_WRITE_CLEAR                     0

#if defined(LINUX)
#define CFG_DBG_GPIO_PINS                       0	/* if 1, use MT6516 GPIO pin to log TX behavior */
#endif

/* 2 Flags for Driver Debug Options */
/*------------------------------------------------------------------------------
 * Flags of TX Debug Option. NOTE(Kevin): Confirm with SA before modifying following flags.
 *------------------------------------------------------------------------------
 */
#define CFG_DBG_MGT_BUF                         1	/*
							 * !< 1: Debug statistics usage of MGMT Buffer
							 * 0: Disable
							 */

#define CFG_HIF_STATISTICS                      0

#define CFG_HIF_RX_STARVATION_WARNING           0

#define CFG_STARTUP_DEBUG                       0

#define CFG_RX_PKTS_DUMP                        1

/*------------------------------------------------------------------------------
 * Flags of Firmware Download Option.
 *------------------------------------------------------------------------------
 */
#define CFG_ENABLE_FW_DOWNLOAD                  1

#define CFG_ENABLE_FW_DOWNLOAD_ACK              1
#define CFG_ENABLE_FW_ENCRYPTION                1

#if defined(MT6628)
#define CFG_ENABLE_FW_DOWNLOAD_AGGREGATION  0
#define CFG_ENABLE_FW_DIVIDED_DOWNLOAD      1
#endif

#if defined(MT6620)
#if MT6620_FPGA_BWCS
#define CFG_FW_LOAD_ADDRESS                     0x10014000
#define CFG_OVERRIDE_FW_START_ADDRESS           0
#define CFG_FW_START_ADDRESS                    0x10014001
#elif MT6620_FPGA_V5
#define CFG_FW_LOAD_ADDRESS                     0x10008000
#define CFG_OVERRIDE_FW_START_ADDRESS           0
#define CFG_FW_START_ADDRESS                    0x10008001
#else
#define CFG_FW_LOAD_ADDRESS                     0x10008000
#define CFG_OVERRIDE_FW_START_ADDRESS           0
#define CFG_FW_START_ADDRESS                    0x10008001
#endif
#elif defined(MT6628)
#define CFG_FW_LOAD_ADDRESS                     0x00060000
#define CFG_OVERRIDE_FW_START_ADDRESS           1
#define CFG_FW_START_ADDRESS                    0x00060000
#define CFG_START_ADDRESS_IS_1ST_SECTION_ADDR   1
#endif

/*------------------------------------------------------------------------------
 * Flags of Bluetooth-over-WiFi (BT 3.0 + HS) support
 *------------------------------------------------------------------------------
 */

#ifdef LINUX
#ifdef CONFIG_X86
#define CFG_ENABLE_BT_OVER_WIFI         0
#else
#define CFG_ENABLE_BT_OVER_WIFI         1
#endif
#else
#define CFG_ENABLE_BT_OVER_WIFI             0
#endif

#define CFG_BOW_SEPARATE_DATA_PATH              1

#define CFG_BOW_PHYSICAL_LINK_NUM               4

#define CFG_BOW_TEST                            0

#define CFG_BOW_LIMIT_AIS_CHNL                  1

#define CFG_BOW_SUPPORT_11N                     0

#define CFG_BOW_RATE_LIMITATION                 1

/*------------------------------------------------------------------------------
 * Flags of Wi-Fi Direct support
 *------------------------------------------------------------------------------
 */
#ifdef LINUX
#ifdef CONFIG_X86
#define CFG_ENABLE_WIFI_DIRECT          0
#define CFG_SUPPORT_802_11W             1
#else
#define CFG_ENABLE_WIFI_DIRECT          1
#define CFG_SUPPORT_802_11W             1
#endif
#else
#define CFG_ENABLE_WIFI_DIRECT              0
#define CFG_SUPPORT_802_11W                 0	/* Not support at WinXP */
#endif

#define CFG_SUPPORT_PERSISTENT_GROUP     0

#define CFG_TEST_WIFI_DIRECT_GO                 0

#define CFG_TEST_ANDROID_DIRECT_GO              0

#define CFG_UNITEST_P2P                         0

/*
 * Enable cfg80211 option after Android 2.2(Froyo) is suggested,
 * cfg80211 on linux 2.6.29 is not mature yet
 */
#define CFG_ENABLE_WIFI_DIRECT_CFG_80211        1

#define CFG_SUPPORT_HOTSPOT_OPTIMIZATION        1
#define CFG_HOTSPOT_OPTIMIZATION_BEACON_INTERVAL 300
#define CFG_HOTSPOT_OPTIMIZATION_DTIM           1

#define CFG_AUTO_CHANNEL_SEL_SUPPORT            1

#define CFG_SUPPORT_SOFTAP_WPA3	1

#define CFG_SET_BCN_CAPINFO_BY_DRIVER           0


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
#define CFG_LINK_QUALITY_VALID_PERIOD           1000

/*------------------------------------------------------------------------------
 * Migration Option
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_ADHOC                       0
#define CFG_SUPPORT_AAA                         1

#define CFG_SUPPORT_BCM                         0
#define CFG_SUPPORT_BCM_BWCS                    0
#define CFG_SUPPORT_BCM_BWCS_DEBUG              0

#define CFG_SUPPORT_RDD_TEST_MODE       0

#define CFG_SUPPORT_PWR_MGT                     1

#define CFG_RSN_MIGRATION                       1

#define CFG_PRIVACY_MIGRATION                   1

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
#define CFG_SUPPORT_MULTITHREAD     0

#define CFG_SUPPORT_QOS             1	/* Enable/disable QoS TX, AMPDU */
#define CFG_SUPPORT_AMPDU_TX        1
#define CFG_SUPPORT_AMPDU_RX        1
#define CFG_SUPPORT_TSPEC           0	/* Enable/disable TS-related Action frames handling */
#define CFG_SUPPORT_UAPSD           1
#define CFG_SUPPORT_UL_PSMP         0

#define CFG_SUPPORT_ROAMING         1	/* Roaming System */
#define CFG_SUPPORT_DYNAMIC_ROAM    0
#define CFG_SUPPORT_SWCR            1

#define CFG_SUPPORT_ANTI_PIRACY     1

#define CFG_SUPPORT_OSC_SETTING     1

#define CFG_SUPPORT_P2P_RSSI_QUERY        0

#define CFG_SUPPORT_CHNL_CONFLICT_REVISE	0

#define CFG_SHOW_MACADDR_SOURCE     1
#define CFG_BSS_DISAPPEAR_THRESOLD				20	/*unit: sec */
#define CFG_NEIGHBOR_AP_CHANNEL_NUM				50
#define CFG_MAX_NUM_OF_CHNL_INFO				50
#define CFG_SELECT_BSS_BASE_ON_MULTI_PARAM		1
#define CFG_SUPPORT_VO_ENTERPRISE               1
#define CFG_NEIGHBOR_AP_CHANNEL_NUM             50
#define CFG_SUPPORT_WMM_AC                      1

#if CFG_SUPPORT_VO_ENTERPRISE
#define CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT  1
#define CFG_SUPPORT_802_11R                     1
#define CFG_SUPPORT_802_11V                     1
#define CFG_SUPPORT_802_11K                     1
#else
#define CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT  0
#define CFG_SUPPORT_802_11R                     0
#define CFG_SUPPORT_802_11V                     0
#define CFG_SUPPORT_802_11K                     0
#endif

#define CFG_SUPPORT_802_11V_TIMING_MEASUREMENT  0
#define CFG_SUPPORT_OKC                         1

#if (CFG_SUPPORT_802_11V_TIMING_MEASUREMENT == 1) || CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT == 1 \
	&& (CFG_SUPPORT_802_11V == 0)
#error "CFG_SUPPORT_802_11V should be 1 once CFG_SUPPORT_802_11V_TIMING_MEASUREMENT equals to 1"
#endif

#define WNM_UNIT_TEST 0

#define CFG_SUPPORT_PPR2	1
#define CFG_DRIVER_COMPOSE_ASSOC_REQ   1

#define CFG_STRICT_CHECK_CAPINFO_PRIVACY    0

#define CFG_SUPPORT_WFD                     1

#define CFG_SUPPORT_WFD_COMPOSE_IE          1

#define CFG_SUPPORT_CPU_BOOST			0


#define CFG_SUPPORT_TX_POWER_BACK_OFF       1

#define CFG_SUPPORT_FCC_POWER_BACK_OFF             0


#define CFG_SUPPORT_P2P_ECSA                       0

#define CFG_SUPPORT_P2P_GO_OFFLOAD_PROBE_RSP       0

#define CFG_SUPPORT_RLM_ACT_NETWORK                1

#define CFG_SUPPORT_P2P_EAP_FAIL_WORKAROUND        1

/*------------------------------------------------------------------------------
 * Flags of Packet Lifetime Profiling Mechanism
 *------------------------------------------------------------------------------
 */
#define CFG_ENABLE_PKT_LIFETIME_PROFILE     1

#define CFG_ENABLE_PER_STA_STATISTICS       1
#define CFG_ENABLE_PER_STA_STATISTICS_LOG   1

#define CFG_PRINT_RTP_PROFILE               0	/* If want to enable WFD Debug, please change it to 1. */
#define CFG_PRINT_RTP_SN_SKIP               0

#define CFG_SUPPORT_PWR_LIMIT_COUNTRY       1
#define CFG_SUPPORT_MTK_SYNERGY             1
/*------------------------------------------------------------------------------
 * Flags of bus error tolerance
 *------------------------------------------------------------------------------
 */
#define CFG_FORCE_RESET_UNDER_BUS_ERROR     0

/*------------------------------------------------------------------------------
 * Build Date Code Integration
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_BUILD_DATE_CODE 1

/*------------------------------------------------------------------------------
 * Flags for prepare the FW compile flag
 *------------------------------------------------------------------------------
 */
#define COMPILE_FLAG0_GET_STA_LINK_STATUS     (1 << 0)
#define COMPILE_FLAG0_WFD_ENHANCEMENT_PROTECT (1 << 1)

/*------------------------------------------------------------------------------
 * Flags of Batch Scan SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_BATCH_SCAN              0
#define CFG_BATCH_MAX_MSCAN                 2

/*------------------------------------------------------------------------------
 * Flags of G-Scan SUPPORT and P-SCN SUPPORT, GSCN is one type of PSCN
 *------------------------------------------------------------------------------
 */

#define CFG_SUPPORT_SCN_PSCN	1
#if CFG_SUPPORT_SCN_PSCN
#define CFG_SUPPORT_GSCN	0	/* GSCN can be disabled here */
#else
#define CFG_SUPPORT_GSCN	0
#endif

/*------------------------------------------------------------------------------
 * Flags of Channel Environment SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_GET_CH_ENV              1

/*------------------------------------------------------------------------------
 * Flags of  THERMO_THROTTLING SUPPORT
 *------------------------------------------------------------------------------
 */

#define CFG_SUPPORT_THERMO_THROTTLING       1
#define WLAN_INCLUDE_PROC                   1

#if CFG_TC10_FEATURE
#define WLAN_INCLUDE_SYS                   1
#else
#define WLAN_INCLUDE_SYS                   0
#endif

#define CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE 1
#define CFG_IGNORE_INVALID_AUTH_TSN		0
/*------------------------------------------------------------------------------
 * Flags of drop multicast packet when device suspend
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_DROP_MC_PACKET		0

/*------------------------------------------------------------------------------
 * Flags of NCHO SUPPORT
 *------------------------------------------------------------------------------
 */
#define CFG_SUPPORT_NCHO		0
#define CFG_SUPPORT_NCHO_AUTO_ENABLE		0

#define CFG_SUPPORT_ADD_CONN_AP		1


/*------------------------------------------------------------------------------
 * Flags of Key Word Exception Mechanism
 *------------------------------------------------------------------------------
 */
#define CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM  1

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


/*Driver naming rule: Mdoule_AndroidVersion_Branch_Date_SerialNum*/
/*Module: Gen2(0x01)/Gen3(0x02) |  kernel-3.10(x00)/3.18(0x10),kernel-4.4(0x20)*/
/*AndroidVersion:7.0->70*/
/*Branch: 00 for Trunk, 01->mp1,02->mp2*/
/*Date: relase date*/
/*Serial Number :start form 1*/
#define WIFI_MODULE "11"
#define ANDROID_VER "70"
#define RELEASE_DATE "20170324"
#define SERIAL_NUMBER "1"
#define SP_BRANCH "TC10"
#define WIFI_DRIVER_VERSION		WIFI_MODULE "_" ANDROID_VER "_" RELEASE_DATE "_" SERIAL_NUMBER "_" SP_BRANCH

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
