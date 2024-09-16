
/*******************************************************************************
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
 ******************************************************************************/
/*! \file   gl_os.h
 *    \brief  List the external reference to OS for GLUE Layer.
 *
 *    In this file we define the data structure - GLUE_INFO_T to store those
 *    objects
 *    we acquired from OS - e.g. TIMER, SPINLOCK, NET DEVICE ... . And all the
 *    external reference (header file, extern func() ..) to OS for GLUE Layer
 *    should also list down here.
 *    For porting to new OS, the API listed in this file needs to be
 *    implemented
 */


#ifndef _GL_OS_H
#define _GL_OS_H

/* Standard C, for implementation on differenct OS
 * the following include files needs to be refined
 */
/* types */
#include <stdint.h>
#include "sys/types.h"

/* bool type */
#include <stdbool.h>

/* memory operation */
#include <string.h>

/* networking */
#include <netinet/in.h>

/* offsetof series */
#include <stddef.h>

/* sprintf series */
#include <stdio.h>

/* error code */
#include <errno.h>
#include <sys/stat.h>

/* va_* series */
#include<stdarg.h>

/* for INT_MAX for some reason include fail not checked */
#if 0
/* when there is platform specific toolchain
 * there should be this file
 * eg. gcc-arm-none-eabi/arm-none-eabi/include/limits.h
 */
#include <limits.h>
#else
/* Minimum and maximum values a `signed int' can hold.  */
#  define INT_MIN	(-INT_MAX - 1)
#  define INT_MAX	2147483647
#endif
/* cannot find the necessary include header */
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

/* driver internal */
#include "version.h"
#include "config.h"
#include "gl_dependent.h"

#include "gl_typedef.h"
#include "typedef.h"
#include "queue.h"
#include "gl_kal.h"
#include "gl_rst.h"
#include "hif.h"

#if CFG_SUPPORT_TDLS
#include "tdls.h"
#endif

#include "debug.h"

#include "wlan_oid.h"

/*
 * comment: declared in wlan_lib.c
 * and almost all of the glue layer includes #include "precomp.h"
 * should we just but it in wlan_lib.h
 */
extern u_int8_t fgIsBusAccessFailed;

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#ifndef __weak
#define __weak __attribute__((weak))
#endif
/*------------------------------------------------------------------------------
 * Flags for OS dependent
 *------------------------------------------------------------------------------
 */
#define CFG_MAX_WLAN_DEVICES 1 /* number of wlan card will coexist */

#define CFG_MAX_TXQ_NUM 4 /* number of tx queue for support multi-queue h/w  */

/* 1: Enable use of SPIN LOCK Bottom Half for LINUX */
/* 0: Disable - use SPIN LOCK IRQ SAVE instead */
#define CFG_USE_SPIN_LOCK_BOTTOM_HALF       0

/* 1: Enable - Drop ethernet packet if it < 14 bytes.
 * And pad ethernet packet with dummy 0 if it < 60 bytes.
 * 0: Disable
 */
#define CFG_TX_PADDING_SMALL_ETH_PACKET     0

#define CFG_TX_STOP_NETIF_QUEUE_THRESHOLD   256	/* packets */

#define CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD   256	/* packets */
#define CFG_TX_START_NETIF_PER_QUEUE_THRESHOLD  128	/* packets */

#define CHIP_NAME    "MT6632"

#define DRV_NAME "["CHIP_NAME"]: "

/* Define if target platform is Android.
 * It should already be defined in Android kernel source
 */
#ifndef CONFIG_ANDROID
/* #define CONFIG_ANDROID      0 */
#endif

/* for CFG80211 IE buffering mechanism */
#define	CFG_CFG80211_IE_BUF_LEN		(512)
#define	GLUE_INFO_WSCIE_LENGTH		(500)
/* for non-wfa vendor specific IE buffer */
#define NON_WFA_VENDOR_IE_MAX_LEN	(128)


/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* driver events for threading */
#define GLUE_FLAG_HALT                  BIT(0)
#define GLUE_FLAG_INT                   BIT(1)
#define GLUE_FLAG_OID                   BIT(2)
#define GLUE_FLAG_TIMEOUT               BIT(3)
#define GLUE_FLAG_TXREQ                 BIT(4)
#define GLUE_FLAG_SUB_MOD_MULTICAST     BIT(7)
#define GLUE_FLAG_FRAME_FILTER          BIT(8)
#define GLUE_FLAG_FRAME_FILTER_AIS      BIT(9)

#define GLUE_FLAG_HALT_BIT              (0)
#define GLUE_FLAG_INT_BIT               (1)
#define GLUE_FLAG_OID_BIT               (2)
#define GLUE_FLAG_TIMEOUT_BIT           (3)
#define GLUE_FLAG_TXREQ_BIT             (4)
#define GLUE_FLAG_SUB_MOD_MULTICAST_BIT (7)
#define GLUE_FLAG_FRAME_FILTER_BIT      (8)
#define GLUE_FLAG_FRAME_FILTER_AIS_BIT  (9)

#if CFG_SUPPORT_MULTITHREAD
#define GLUE_FLAG_RX				BIT(10)
#define GLUE_FLAG_TX_CMD_DONE			BIT(11)
#define GLUE_FLAG_HIF_TX			BIT(12)
#define GLUE_FLAG_HIF_TX_CMD			BIT(13)
#define GLUE_FLAG_RX_TO_OS			BIT(14)
#define GLUE_FLAG_HIF_FW_OWN			BIT(15)
#define GLUE_FLAG_HIF_PRT_HIF_DBG_INFO		BIT(16)
#define GLUE_FLAG_UPDATE_WMM_QUOTA		BIT(17)

#define GLUE_FLAG_RX_BIT				(10)
#define GLUE_FLAG_TX_CMD_DONE_BIT			(11)
#define GLUE_FLAG_HIF_TX_BIT				(12)
#define GLUE_FLAG_HIF_TX_CMD_BIT			(13)
#define GLUE_FLAG_RX_TO_OS_BIT				(14)
#define GLUE_FLAG_HIF_FW_OWN_BIT			(15)
#define GLUE_FLAG_HIF_PRT_HIF_DBG_INFO_BIT		(16)
#define GLUE_FLAG_UPDATE_WMM_QUOTA_BIT			(17)
#endif

#if CFG_ENABLE_BT_OVER_WIFI
#define GLUE_BOW_KFIFO_DEPTH        (1024)
/* #define GLUE_BOW_DEVICE_NAME        "MT6620 802.11 AMP" */
#define GLUE_BOW_DEVICE_NAME        "ampc0"
#endif

/* wake lock for suspend/resume */
#define WAKE_LOCK_RX_TIMEOUT                            300	/* ms */
#define WAKE_LOCK_THREAD_WAKEUP_TIMEOUT                 50	/* ms */

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
struct GLUE_INFO;

struct GL_WPA_INFO {
	uint32_t u4WpaVersion;
	uint32_t u4KeyMgmt;
	uint32_t u4CipherGroup;
	uint32_t u4CipherPairwise;
	uint32_t u4AuthAlg;
	u_int8_t fgPrivacyInvoke;
#if CFG_SUPPORT_802_11W
	uint32_t u4Mfp;
	uint8_t ucRSNMfpCap;
#endif
};

#if CFG_SUPPORT_REPLAY_DETECTION
struct GL_REPLEY_PN_INFO {
	uint8_t auPN[16];
	u_int8_t fgRekey;
	u_int8_t fgFirstPkt;
};
struct GL_DETECT_REPLAY_INFO {
	uint8_t ucCurKeyId;
	uint8_t ucKeyType;
	struct GL_REPLEY_PN_INFO arReplayPNInfo[4];
};
#endif

enum ENUM_NET_DEV_IDX {
	NET_DEV_WLAN_IDX = 0,
	NET_DEV_P2P_IDX,
	NET_DEV_BOW_IDX,
	NET_DEV_NUM
};

enum ENUM_RSSI_TRIGGER_TYPE {
	ENUM_RSSI_TRIGGER_NONE,
	ENUM_RSSI_TRIGGER_GREATER,
	ENUM_RSSI_TRIGGER_LESS,
	ENUM_RSSI_TRIGGER_TRIGGERED,
	ENUM_RSSI_TRIGGER_NUM
};

#if CFG_ENABLE_WIFI_DIRECT
enum ENUM_NET_REG_STATE {
	ENUM_NET_REG_STATE_UNREGISTERED,
	ENUM_NET_REG_STATE_REGISTERING,
	ENUM_NET_REG_STATE_REGISTERED,
	ENUM_NET_REG_STATE_UNREGISTERING,
	ENUM_NET_REG_STATE_NUM
};
#endif

enum ENUM_PKT_FLAG {
	ENUM_PKT_802_11,	/* 802.11 or non-802.11 */
	ENUM_PKT_802_3,		/* 802.3 or ethernetII */
	ENUM_PKT_1X,		/* 1x frame or not */
	ENUM_PKT_PROTECTED_1X,	/* protected 1x frame */
	ENUM_PKT_NON_PROTECTED_1X,	/* Non protected 1x frame */
	ENUM_PKT_VLAN_EXIST,	/* VLAN tag exist */
	ENUM_PKT_DHCP,		/* DHCP frame */
	ENUM_PKT_ARP,		/* ARP */
	ENUM_PKT_ICMP,		/* ICMP */
	ENUM_PKT_TDLS,		/* TDLS */
	ENUM_PKT_DNS,		/* DNS */

	ENUM_PKT_FLAG_NUM
};

struct GL_IO_REQ {
	struct QUE_ENTRY rQueEntry;
	/* wait_queue_head_t       cmdwait_q; */
	u_int8_t fgRead;
	u_int8_t fgWaitResp;
	struct ADAPTER *prAdapter;
	PFN_OID_HANDLER_FUNC pfnOidHandler;
	void *pvInfoBuf;
	uint32_t u4InfoBufLen;
	uint32_t *pu4QryInfoLen;
	uint32_t rStatus;
	uint32_t u4Flag;
};

#if CFG_ENABLE_BT_OVER_WIFI
struct GL_BOW_INFO {
/* TODO: os-related */
#if 0
	u_int8_t fgIsRegistered;
	dev_t u4DeviceNumber;	/* dynamic device number */
	struct kfifo rKfifo;	/* for buffering indicated events */
	spinlock_t rSpinLock;	/* spin lock for kfifo */
	struct cdev cdev;
	uint32_t u4FreqInKHz;	/* frequency */

	uint8_t aucRole[CFG_BOW_PHYSICAL_LINK_NUM];	/* 0: Responder,
							 * 1: Initiator
							 */
	enum ENUM_BOW_DEVICE_STATE
	aeState[CFG_BOW_PHYSICAL_LINK_NUM];
	uint8_t arPeerAddr[CFG_BOW_PHYSICAL_LINK_NUM][PARAM_MAC_ADDR_LEN];

	wait_queue_head_t outq;

#if CFG_BOW_SEPARATE_DATA_PATH
	/* Device handle */
	struct net_device *prDevHandler;
	u_int8_t fgIsNetRegistered;
#endif
#endif
};
#endif

#if CFG_SUPPORT_SCAN_CACHE_RESULT
struct GL_SCAN_CACHE_INFO {
	struct GLUE_INFO *prGlueInfo;
/* TODO: os-related? */
	/* total number of channels to scan */
	uint32_t n_channels;

	/* Scan period time */
	OS_SYSTIME u4LastScanTime;
/* TODO: os-related */
#if 0
	/* for cfg80211 scan done indication */
	struct cfg80211_scan_request *prRequest;
#endif
};
#endif /* CFG_SUPPORT_SCAN_CACHE_RESULT */

struct FT_IES {
	uint16_t u2MDID;
	struct IE_MOBILITY_DOMAIN *prMDIE;
	struct IE_FAST_TRANSITION *prFTIE;
	struct IE_TIMEOUT_INTERVAL *prTIE;
	struct RSN_INFO_ELEM *prRsnIE;
	uint8_t *pucIEBuf;
	uint32_t u4IeLength;
};

/*
 * type definition of pointer to p2p structure
 */
struct GL_P2P_INFO;	/* declare GL_P2P_INFO_T */
struct GL_P2P_DEV_INFO;	/* declare GL_P2P_DEV_INFO_T */

struct GLUE_INFO {
	/* Pointer to ADAPTER_T - main data structure of internal protocol
	 * stack
	 */
	struct ADAPTER *prAdapter;

	/* OID related */
	struct QUE rCmdQueue;

	spinlock_t rSpinLock[SPIN_LOCK_NUM];

	/* Number of pending frames, also used for debuging if any frame is
	 * missing during the process of unloading Driver.
	 *
	 * NOTE(Kevin): In Linux, we also use this variable as the threshold
	 * for manipulating the netif_stop(wake)_queue() func.
	 */
	int32_t ai4TxPendingFrameNumPerQueue[MAX_BSSID_NUM][CFG_MAX_TXQ_NUM];
	int32_t i4TxPendingFrameNum;
	int32_t i4TxPendingSecurityFrameNum;
	int32_t i4TxPendingCmdNum;

	/*! \brief wext wpa related information */
	struct GL_WPA_INFO rWpaInfo[KAL_AIS_NUM];
#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO prDetRplyInfo[KAL_AIS_NUM];
#endif

	uint32_t u4RoamFailCnt;
	uint64_t u8RoamFailTime;

	/* 11R */
	struct FT_IES rFtIeForTx;
	struct cfg80211_ft_event_params rFtEventParam;

	uint32_t IsrAbnormalCnt;
	uint32_t IsrSoftWareCnt;

	/* Indicated media state */
	enum ENUM_PARAM_MEDIA_STATE eParamMediaStateIndicated;

	/* NVRAM availability */
	u_int8_t fgNvramAvailable;

	struct SET_TXPWR_CTRL rTxPwr;

	uint32_t IsrCnt;
	uint32_t IsrPassCnt;
	uint32_t TaskIsrCnt;

	uint32_t IsrTxCnt;
	uint32_t IsrRxCnt;

	/* Tx: for NetDev to BSS index mapping */
	struct NET_INTERFACE_INFO arNetInterfaceInfo[MAX_BSSID_NUM];

	u_int8_t fgTxDoneDelayIsARP;

	uint32_t u4ArriveDrvTick;
	uint32_t u4EnQueTick;
	uint32_t u4DeQueTick;
	uint32_t u4LeaveDrvTick;
	uint32_t u4CurrTick;
	uint64_t u8CurrTime;

	/*
	 * Buffer to hold non-wfa vendor specific IEs set
	 * from wpa_supplicant. This is used in sending
	 * Association Request in AIS mode.
	 */
	uint16_t non_wfa_vendor_ie_len;
	uint8_t non_wfa_vendor_ie_buf[NON_WFA_VENDOR_IE_MAX_LEN];

#if CFG_ENABLE_WIFI_DIRECT
	struct GL_P2P_DEV_INFO *prP2PDevInfo;
	struct GL_P2P_INFO *prP2PInfo[KAL_P2P_NUM];
#endif

#if CFG_SUPPORT_SNIFFER
	u_int8_t fgIsEnableMon;
#endif

	u_int8_t fgIsInSuspendMode;

	/* registry info */
	struct REG_INFO rRegInfo;

	/*
	 * needed by
	 * common/cmm_asic_connac.c
	 */
	uint32_t u4InfType;

	/*
	 * needed by
	 * mgmt/tdls.c
	 */
	/* Device handle */
	struct net_device *prDevHandler;

	/* Device */
	struct device *prDev;
	/* not necessary for built */
	/* TODO: os-related */
	uint32_t u4ReadyFlag;	/* check if card is ready */
#if 0

	/* Device */
	struct device *prDev;

	/* Device Index(index of arWlanDevInfo[]) */
	int32_t i4DevIdx;

	/* Device statistics */
	/* struct net_device_stats rNetDevStats; */

	/* Wireless statistics struct net_device */
	struct iw_statistics rIwStats;

	/* spinlock to sync power save mechanism */
	spinlock_t rSpinLock[SPIN_LOCK_NUM];

	/* Mutex to protect interruptible section */
	struct mutex arMutex[MUTEX_NUM];

	/* semaphore for ioctl */
	struct semaphore ioctl_sem;

	uint64_t u8Cookie;

	unsigned long ulFlag;		/* GLUE_FLAG_XXX */
	uint32_t u4PendFlag;
	/* UINT_32 u4TimeoutFlag; */
	uint32_t u4OidCompleteFlag;
	uint32_t u4ReadyFlag;	/* check if card is ready */

	uint32_t u4OsMgmtFrameFilter;

	/* Number of pending frames, also used for debuging if any frame is
	 * missing during the process of unloading Driver.
	 *
	 * NOTE(Kevin): In Linux, we also use this variable as the threshold
	 * for manipulating the netif_stop(wake)_queue() func.
	 */
	int32_t ai4TxPendingFrameNumPerQueue[MAX_BSSID_NUM][CFG_MAX_TXQ_NUM];
	int32_t i4TxPendingFrameNum;
	int32_t i4TxPendingSecurityFrameNum;
	int32_t i4TxPendingCmdNum;

	/* Tx: for NetDev to BSS index mapping */
	struct NET_INTERFACE_INFO arNetInterfaceInfo[MAX_BSSID_NUM];

	/* Rx: for BSS index to NetDev mapping */
	/* P_NET_INTERFACE_INFO_T  aprBssIdxToNetInterfaceInfo[HW_BSSID_NUM]; */

	/* current IO request for kalIoctl */
	struct GL_IO_REQ OidEntry;

	/* registry info */
	struct REG_INFO rRegInfo;

	/* firmware */
	struct firmware *prFw;

	/* Host interface related information */
	/* defined in related hif header file */
	struct GL_HIF_INFO rHifInfo;

	/*! \brief wext wpa related information */
	struct GL_WPA_INFO rWpaInfo;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO prDetRplyInfo;
#endif

	/* Pointer to ADAPTER_T - main data structure of internal protocol
	 * stack
	 */
	struct ADAPTER *prAdapter;

#if WLAN_INCLUDE_PROC
	struct proc_dir_entry *pProcRoot;
#endif				/* WLAN_INCLUDE_PROC */

	/* Indicated media state */
	enum ENUM_PARAM_MEDIA_STATE eParamMediaStateIndicated;

	/* Device power state D0~D3 */
	enum PARAM_DEVICE_POWER_STATE ePowerState;

	struct completion rScanComp;	/* indicate scan complete */
	struct completion
		rHaltComp;	/* indicate main thread halt complete */
	struct completion
		rPendComp;	/* indicate main thread halt complete */
#if CFG_SUPPORT_MULTITHREAD
	struct completion
		rHifHaltComp;	/* indicate hif_thread halt complete */
	struct completion
		rRxHaltComp;	/* indicate hif_thread halt complete */

	uint32_t u4TxThreadPid;
	uint32_t u4RxThreadPid;
	uint32_t u4HifThreadPid;
#endif

#if CFG_SUPPORT_NCHO
	struct completion
		rAisChGrntComp;	/* indicate Ais channel grant complete */
#endif

	uint32_t rPendStatus;

	struct QUE rTxQueue;

	/* OID related */
	struct QUE rCmdQueue;
	/* PVOID                   pvInformationBuffer; */
	/* UINT_32                 u4InformationBufferLength; */
	/* PVOID                   pvOidEntry; */
	/* PUINT_8                 pucIOReqBuff; */
	/* QUE_T                   rIOReqQueue; */
	/* QUE_T                   rFreeIOReqQueue; */

	wait_queue_head_t waitq;
	struct task_struct *main_thread;

#if CFG_SUPPORT_MULTITHREAD
	wait_queue_head_t waitq_hif;
	struct task_struct *hif_thread;

	wait_queue_head_t waitq_rx;
	struct task_struct *rx_thread;

#endif
	struct tasklet_struct rRxTask;
	struct tasklet_struct rTxCompleteTask;

	struct work_struct rTxMsduFreeWork;
	struct delayed_work rRxPktDeAggWork;

	struct timer_list tickfn;

#if CFG_SUPPORT_EXT_CONFIG
	uint16_t au2ExtCfg[256];	/* NVRAM data buffer */
	uint32_t u4ExtCfgLength;	/* 0 means data is NOT valid */
#endif

#if 1				/* CFG_SUPPORT_WAPI */
	/* Should be large than the PARAM_WAPI_ASSOC_INFO_T */
	uint8_t aucWapiAssocInfoIEs[42];
	uint16_t u2WapiAssocInfoIESz;
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	struct GL_BOW_INFO rBowInfo;
#endif

#if CFG_ENABLE_WIFI_DIRECT
	struct GL_P2P_DEV_INFO *prP2PDevInfo;
	struct GL_P2P_INFO *prP2PInfo[KAL_P2P_NUM];
#if CFG_SUPPORT_P2P_RSSI_QUERY
	/* Wireless statistics struct net_device */
	struct iw_statistics rP2pIwStats;
#endif
#endif
	u_int8_t fgWpsActive;
	uint8_t aucWSCIE[GLUE_INFO_WSCIE_LENGTH];	/*for probe req */
	uint16_t u2WSCIELen;
	uint8_t aucWSCAssocInfoIE[200];	/*for Assoc req */
	uint16_t u2WSCAssocInfoIELen;

	/* NVRAM availability */
	u_int8_t fgNvramAvailable;

	u_int8_t fgMcrAccessAllowed;

	/* MAC Address Overridden by IOCTL */
	u_int8_t fgIsMacAddrOverride;
	uint8_t rMacAddrOverride[PARAM_MAC_ADDR_LEN];

	struct SET_TXPWR_CTRL rTxPwr;

	/* for cfg80211 scan done indication */
	struct cfg80211_scan_request *prScanRequest;

	/* for cfg80211 scheduled scan */
	struct cfg80211_sched_scan_request *prSchedScanRequest;

	/* to indicate registered or not */
	u_int8_t fgIsRegistered;

	/* for cfg80211 connected indication */
	uint32_t u4RspIeLength;
	uint8_t aucRspIe[CFG_CFG80211_IE_BUF_LEN];

	uint32_t u4ReqIeLength;
	uint8_t aucReqIe[CFG_CFG80211_IE_BUF_LEN];

	/*
	 * Buffer to hold non-wfa vendor specific IEs set
	 * from wpa_supplicant. This is used in sending
	 * Association Request in AIS mode.
	 */
	uint16_t non_wfa_vendor_ie_len;
	uint8_t non_wfa_vendor_ie_buf[NON_WFA_VENDOR_IE_MAX_LEN];

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
	u_int8_t fgEnSdioTestPattern;
	u_int8_t fgSdioReadWriteMode;
	u_int8_t fgIsSdioTestInitialized;
	uint8_t aucSdioTestBuffer[256];
#endif

	u_int8_t fgIsInSuspendMode;

#if CFG_SUPPORT_PASSPOINT
	uint8_t aucHS20AssocInfoIE[200];	/*for Assoc req */
	uint16_t u2HS20AssocInfoIELen;
	uint8_t ucHotspotConfig;
	u_int8_t fgConnectHS20AP;

	u_int8_t fgIsDad;
	uint8_t aucDADipv4[4];
	u_int8_t fgIs6Dad;
	uint8_t aucDADipv6[16];
#endif				/* CFG_SUPPORT_PASSPOINT */

	KAL_WAKE_LOCK_T rIntrWakeLock;
	KAL_WAKE_LOCK_T rTimeoutWakeLock;

#if CFG_MET_PACKET_TRACE_SUPPORT
	u_int8_t fgMetProfilingEn;
	uint16_t u2MetUdpPort;
#endif

#if CFG_SUPPORT_SNIFFER
	u_int8_t fgIsEnableMon;
	struct net_device *prMonDevHandler;
	struct work_struct monWork;
#endif

	int32_t i4RssiCache;
	uint32_t u4LinkSpeedCache;


	uint32_t u4InfType;

	uint32_t IsrCnt;
	uint32_t IsrPassCnt;
	uint32_t TaskIsrCnt;

	uint32_t IsrAbnormalCnt;
	uint32_t IsrSoftWareCnt;
	uint32_t IsrTxCnt;
	uint32_t IsrRxCnt;
	uint64_t u8HifIntTime;

	/* save partial scan channel information */
	/* PARTIAL_SCAN_INFO rScanChannelInfo; */
	uint8_t *pucScanChannel;

#if CFG_SUPPORT_SCAN_CACHE_RESULT
	struct GL_SCAN_CACHE_INFO scanCache;
#endif /* CFG_SUPPORT_SCAN_CACHE_RESULT */

	/* Full2Partial */
	OS_SYSTIME u4LastFullScanTime;
	/* full scan or partial scan */
	uint8_t ucTrScanType;
	/* UINT_8 aucChannelNum[FULL_SCAN_MAX_CHANNEL_NUM]; */
	/* PARTIAL_SCAN_INFO rFullScanApChannel; */
	uint8_t *pucFullScan2PartialChannel;

	uint32_t u4RoamFailCnt;
	uint64_t u8RoamFailTime;
	u_int8_t fgTxDoneDelayIsARP;
	uint32_t u4ArriveDrvTick;
	uint32_t u4EnQueTick;
	uint32_t u4DeQueTick;
	uint32_t u4LeaveDrvTick;
	uint32_t u4CurrTick;
	uint64_t u8CurrTime;

	/* FW Roaming */
	/* store the FW roaming enable state which FWK determines */
	/* if it's = 0, ignore the black/whitelists settings from FWK */
	uint32_t u4FWRoamingEnable;

	/* 11R */
	struct FT_IES rFtIeForTx;
	struct cfg80211_ft_event_params rFtEventParam;
#endif
};

#if 0  /* irq & time in Linux */
typedef irqreturn_t(*PFN_WLANISR) (int irq, void *dev_id,
				   struct pt_regs *regs);

typedef void (*PFN_LINUX_TIMER_FUNC) (unsigned long);
#endif

/* generic sub module init/exit handler
 *   now, we only have one sub module, p2p
 */
#if CFG_ENABLE_WIFI_DIRECT
typedef u_int8_t(*SUB_MODULE_INIT) (struct GLUE_INFO
				    *prGlueInfo);
typedef u_int8_t(*SUB_MODULE_EXIT) (struct GLUE_INFO
				    *prGlueInfo);

struct SUB_MODULE_HANDLER {
	SUB_MODULE_INIT subModInit;
	SUB_MODULE_EXIT subModExit;
	u_int8_t fgIsInited;
};

#endif

#ifdef CONFIG_NL80211_TESTMODE

enum TestModeCmdType {
	TESTMODE_CMD_ID_SW_CMD = 1,
	TESTMODE_CMD_ID_WAPI = 2,
	TESTMODE_CMD_ID_HS20 = 3,

	/* Hotspot managerment testmode command */
	TESTMODE_CMD_ID_HS_CONFIG = 51,

	TESTMODE_CMD_ID_STR_CMD = 102,
	NUM_OF_TESTMODE_CMD_ID
};

#if CFG_SUPPORT_PASSPOINT
enum Hs20CmdType {
	HS20_CMD_ID_SET_BSSID_POOL = 0,
	NUM_OF_HS20_CMD_ID
};
#endif /* CFG_SUPPORT_PASSPOINT */

struct NL80211_DRIVER_TEST_MODE_PARAMS {
	uint32_t index;
	uint32_t buflen;
};

struct NL80211_DRIVER_STRING_CMD_PARAMS {
	struct NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	uint32_t reply_buf_size;
	uint32_t reply_len;
	uint8_t *reply_buf;
};

/*SW CMD */
struct NL80211_DRIVER_SW_CMD_PARAMS {
	struct NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	uint8_t set;
	uint32_t adr;
	uint32_t data;
};

struct iw_encode_exts {
	__u32 ext_flags;	/*!< IW_ENCODE_EXT_* */
	__u8 tx_seq[IW_ENCODE_SEQ_MAX_SIZE];	/*!< LSB first */
	__u8 rx_seq[IW_ENCODE_SEQ_MAX_SIZE];	/*!< LSB first */
	/*!< ff:ff:ff:ff:ff:ff for broadcast/multicast
	 *   (group) keys or unicast address for
	 *   individual keys
	 */
	__u8 addr[MAC_ADDR_LEN];
	__u16 alg;		/*!< IW_ENCODE_ALG_* */
	__u16 key_len;
	__u8 key[32];
};

/*SET KEY EXT */
struct NL80211_DRIVER_SET_KEY_EXTS {
	struct NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	uint8_t key_index;
	uint8_t key_len;
	struct iw_encode_exts ext;
};

#if CFG_SUPPORT_PASSPOINT

struct param_hs20_set_bssid_pool {
	uint8_t fgBssidPoolIsEnable;
	uint8_t ucNumBssidPool;
	uint8_t arBssidPool[8][ETH_ALEN];
};

struct wpa_driver_hs20_data_s {
	struct NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	enum Hs20CmdType CmdType;
	struct param_hs20_set_bssid_pool hs20_set_bssid_pool;
};

#endif /* CFG_SUPPORT_PASSPOINT */

#endif

struct NETDEV_PRIVATE_GLUE_INFO {
	struct GLUE_INFO *prGlueInfo;
	uint8_t ucBssIdx;
#if CFG_ENABLE_UNIFY_WIPHY
	u_int8_t ucIsP2p;
#endif
};

struct PACKET_PRIVATE_DATA {
	/* tx/rx both use cb */
	struct QUE_ENTRY rQueEntry;  /* 16byte total:16 */

	uint8_t ucBssIdx;	/* 1byte */
	/* only rx use cb */
	u_int8_t fgIsIndependentPkt; /* 1byte */
	/* only tx use cb */
	uint8_t ucTid;		/* 1byte */
	uint8_t ucHeaderLen;	/* 1byte */
	uint8_t ucProfilingFlag;	/* 1byte */
	uint8_t ucSeqNo;		/* 1byte */
	uint16_t u2Flag;		/* 2byte total:24 */

	uint16_t u2IpId;		/* 2byte */
	uint16_t u2FrameLen;	/* 2byte */
	OS_SYSTIME rArrivalTime;/* 4byte total:32 */

	uint64_t u8ArriveTime;	/* 8byte total:40 */
};

struct PACKET_PRIVATE_RX_DATA {
	uint64_t u8IntTime;	/* 8byte */
	uint64_t u8RxTime;	/* 8byte */
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/* Macros of SPIN LOCK operations for using in Glue Layer                     */
/*----------------------------------------------------------------------------*/
/* TODO: we have to lock source another is in kalAcquireSpinLock */
#define GLUE_SPIN_LOCK_DECLARATION()
#define GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, rLockCategory)
#define GLUE_RELEASE_SPIN_LOCK(prGlueInfo, rLockCategory)
/*----------------------------------------------------------------------------*/
/* Macros for accessing Reserved Fields of native packet                      */
/*----------------------------------------------------------------------------*/
/* TODO: os-related, implementation may refer to os/linux */
#define GLUE_GET_PKT_PRIVATE_DATA(_p) ((struct PACKET_PRIVATE_DATA *)0)

#define GLUE_GET_PKT_QUEUE_ENTRY(_p)    \
	    (&(GLUE_GET_PKT_PRIVATE_DATA(_p)->rQueEntry))

/* TODO: os-related implementation */
#define GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry) ((struct PACKET_PRIVATE_DATA *)0)

#define GLUE_SET_PKT_TID(_p, _tid) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucTid = (uint8_t)(_tid))

#define GLUE_GET_PKT_TID(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucTid)

#define GLUE_SET_PKT_FLAG(_p, _flag) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag |= BIT(_flag))

#define GLUE_TEST_PKT_FLAG(_p, _flag) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag & BIT(_flag))

#define GLUE_IS_PKT_FLAG_SET(_p) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag)

#define GLUE_SET_PKT_BSS_IDX(_p, _ucBssIndex) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucBssIdx = (uint8_t)(_ucBssIndex))

#define GLUE_GET_PKT_BSS_IDX(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucBssIdx)

#define GLUE_SET_PKT_HEADER_LEN(_p, _ucMacHeaderLen) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucHeaderLen = \
	    (uint8_t)(_ucMacHeaderLen))

#define GLUE_GET_PKT_HEADER_LEN(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucHeaderLen)

#define GLUE_SET_PKT_FRAME_LEN(_p, _u2PayloadLen) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2FrameLen = (uint16_t)(_u2PayloadLen))

#define GLUE_GET_PKT_FRAME_LEN(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->u2FrameLen)

#define GLUE_SET_PKT_ARRIVAL_TIME(_p, _rSysTime) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->rArrivalTime = (OS_SYSTIME)(_rSysTime))

#define GLUE_GET_PKT_ARRIVAL_TIME(_p)    \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->rArrivalTime)

#define GLUE_SET_PKT_IP_ID(_p, _u2IpId) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2IpId = (uint16_t)(_u2IpId))

#define GLUE_GET_PKT_IP_ID(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->u2IpId)

#define GLUE_SET_PKT_SEQ_NO(_p, _ucSeqNo) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->ucSeqNo = (uint8_t)(_ucSeqNo))

#define GLUE_GET_PKT_SEQ_NO(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucSeqNo)

#define GLUE_SET_PKT_FLAG_PROF_MET(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucProfilingFlag |= BIT(0))

#define GLUE_GET_PKT_IS_PROF_MET(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucProfilingFlag & BIT(0))

#define GLUE_SET_PKT_XTIME(_p, _rSysTime) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u8ArriveTime = (uint64_t)(_rSysTime))

#define GLUE_GET_PKT_XTIME(_p)    \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u8ArriveTime)

/* TODO: os-related implementation */
#define GLUE_GET_PKT_PRIVATE_RX_DATA(_p) \
	((struct PACKET_PRIVATE_RX_DATA *)(&(((struct sk_buff *)(_p))->cb[24])))

#define GLUE_RX_SET_PKT_INT_TIME(_p, _rTime) \
	(GLUE_GET_PKT_PRIVATE_RX_DATA(_p)->u8IntTime = (uint64_t)(_rTime))

#define GLUE_RX_GET_PKT_INT_TIME(_p) \
	(GLUE_GET_PKT_PRIVATE_RX_DATA(_p)->u8IntTime)

#define GLUE_RX_SET_PKT_RX_TIME(_p, _rTime) \
	(GLUE_GET_PKT_PRIVATE_RX_DATA(_p)->u8RxTime = (uint64_t)(_rTime))

#define GLUE_RX_GET_PKT_RX_TIME(_p) \
	(GLUE_GET_PKT_PRIVATE_RX_DATA(_p)->u8RxTime)

/* TODO: os-related implementation */
#define GLUE_GET_PKT_ETHER_DEST_ADDR(_p)

/* Check validity of prDev, private data, and pointers */
/* TODO: os-related implementation */
#define GLUE_CHK_DEV(prDev)

#define GLUE_CHK_PR2(prDev, pr2) \
	((GLUE_CHK_DEV(prDev) && pr2) ? TRUE : FALSE)

#define GLUE_CHK_PR3(prDev, pr2, pr3) \
	((GLUE_CHK_PR2(prDev, pr2) && pr3) ? TRUE : FALSE)

#define GLUE_CHK_PR4(prDev, pr2, pr3, pr4) \
	((GLUE_CHK_PR3(prDev, pr2, pr3) && pr4) ? TRUE : FALSE)

#define GLUE_SET_EVENT(pr) \
	kalSetEvent(pr)
/* TODO: os-related implementation */
#define GLUE_INC_REF_CNT(_refCount)     (_refCount++)
#define GLUE_DEC_REF_CNT(_refCount)     (_refCount--)
#define GLUE_ADD_REF_CNT(_value, _refCount) \
	(_refCount += _value)
#define GLUE_SUB_REF_CNT(_value, _refCount) \
	(_refCount -= _value)
#define GLUE_GET_REF_CNT(_refCount)     (_refCount)

#define DbgPrint(...)

#define GLUE_LOOKUP_FUN(fun_name) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)


#if CFG_MET_TAG_SUPPORT
#define GL_MET_TAG_START(_id, _name)	met_tag_start(_id, _name)
#define GL_MET_TAG_END(_id, _name)	met_tag_end(_id, _name)
#define GL_MET_TAG_ONESHOT(_id, _name, _value) \
	met_tag_oneshot(_id, _name, _value)
#define GL_MET_TAG_DISABLE(_id)		met_tag_disable(_id)
#define GL_MET_TAG_ENABLE(_id)		met_tag_enable(_id)
#define GL_MET_TAG_REC_ON()		met_tag_record_on()
#define GL_MET_TAG_REC_OFF()		met_tag_record_off()
#define GL_MET_TAG_INIT()		met_tag_init()
#define GL_MET_TAG_UNINIT()		met_tag_uninit()
#else
#define GL_MET_TAG_START(_id, _name)
#define GL_MET_TAG_END(_id, _name)
#define GL_MET_TAG_ONESHOT(_id, _name, _value)
#define GL_MET_TAG_DISABLE(_id)
#define GL_MET_TAG_ENABLE(_id)
#define GL_MET_TAG_REC_ON()
#define GL_MET_TAG_REC_OFF()
#define GL_MET_TAG_INIT()
#define GL_MET_TAG_UNINIT()
#endif

#define MET_TAG_ID	0

/*----------------------------------------------------------------------------*/
/* Macros of Data Type Check                                                  */
/*----------------------------------------------------------------------------*/
/* Kevin: we don't have to call following function to inspect the data
 * structure.
 * It will check automatically while at compile time.
 */
/* TODO: os-related, API implementation, may refer to Linux */
#define glPacketDataTypeCheck()

#define mtk_wlan_ndev_select_queue(_prNetdev, _prSkb)

#define netdev_for_each_mc_addr(mclist, dev)

#define GET_ADDR(ha)

#define LIST_FOR_EACH_IPV6_ADDR(_prIfa, _ip6_ptr)

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
#if WLAN_INCLUDE_PROC
int32_t procCreateFsEntry(struct GLUE_INFO *prGlueInfo);
int32_t procRemoveProcfs(void);


int32_t procInitFs(void);
int32_t procUninitProcFs(void);



/* TODO: os-related, API implementation, may refer to Linux */
#define procInitProcfs(_prDev, _pucDevName)
#endif /* WLAN_INCLUDE_PROC */

#if CFG_ENABLE_BT_OVER_WIFI
u_int8_t glRegisterAmpc(struct GLUE_INFO *prGlueInfo);

u_int8_t glUnregisterAmpc(struct GLUE_INFO *prGlueInfo);
#endif

#if CFG_ENABLE_WIFI_DIRECT
void p2pSetMulticastListWorkQueueWrapper(struct GLUE_INFO
		*prGlueInfo);
#endif

struct GLUE_INFO *wlanGetGlueInfo(void);

#ifdef CFG_REMIND_IMPLEMENT
#define wlanSelectQueue(_dev, _skb) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#define wlanDebugInit() \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
u16 wlanSelectQueue(struct net_device *dev,
		    struct sk_buff *skb);

void wlanDebugInit(void);
#endif

uint32_t wlanSetDriverDbgLevel(IN uint32_t u4DbgIdx,
			       IN uint32_t u4DbgMask);

uint32_t wlanGetDriverDbgLevel(IN uint32_t u4DbgIdx,
			       OUT uint32_t *pu4DbgMask);

void wlanSetSuspendMode(struct GLUE_INFO *prGlueInfo,
			u_int8_t fgEnable);

#ifdef CFG_REMIND_IMPLEMENT
#define wlanGetConfig(_prAdapter) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void wlanGetConfig(struct ADAPTER *prAdapter);
#endif

uint32_t wlanDownloadBufferBin(struct ADAPTER *prAdapter);

uint32_t wlanConnacDownloadBufferBin(struct ADAPTER
				     *prAdapter);

/*******************************************************************************
 *			 E X T E R N A L   F U N C T I O N S / V A R I A B L E
 *******************************************************************************
 */
extern struct net_device *gPrP2pDev[KAL_P2P_NUM];
extern struct net_device *gPrDev;
extern struct wireless_dev *gprWdev[KAL_AIS_NUM];
extern uint32_t g_u4DevIdx[KAL_P2P_NUM];
extern enum ENUM_NVRAM_STATE g_NvramFsm;

#ifdef CFG_DRIVER_INF_NAME_CHANGE
extern char *gprifnameap;
extern char *gprifnamep2p;
extern char *gprifnamesta;
#endif /* CFG_DRIVER_INF_NAME_CHANGE */

extern void wlanRegisterNotifier(void);
extern void wlanUnregisterNotifier(void);
#if CFG_MTK_ANDROID_WMT
typedef int (*set_p2p_mode) (struct net_device *netdev,
			     struct PARAM_CUSTOM_P2P_SET_STRUCT p2pmode);
extern void register_set_p2p_mode_handler(
	set_p2p_mode handler);
#endif

#if CFG_ENABLE_EARLY_SUSPEND
extern int glRegisterEarlySuspend(struct early_suspend
				  *prDesc,
				  early_suspend_callback wlanSuspend,
				  late_resume_callback wlanResume);

extern int glUnregisterEarlySuspend(struct early_suspend
				    *prDesc);
#endif

#if CFG_MET_PACKET_TRACE_SUPPORT
#ifdef CFG_REMIND_IMPLEMENT
#define kalMetTagPacket(_prGlueInfo, _prPacket, _eTag) \
KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void kalMetTagPacket(IN struct GLUE_INFO *prGlueInfo,
		     IN void *prPacket, IN enum ENUM_TX_PROFILING_TAG eTag);
#endif

void kalMetInit(IN struct GLUE_INFO *prGlueInfo);
#endif

#ifdef CFG_REMIND_IMPLEMENT
#define wlanUpdateChannelTable(_prGlueInfo) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void wlanUpdateChannelTable(struct GLUE_INFO *prGlueInfo);
#endif

#if CFG_SUPPORT_SAP_DFS_CHANNEL
#ifdef CFG_REMIND_IMPLEMENT
#define wlanUpdateDfsChannelTable(_prGlueInfo, \
		_ucRoleIdx, _ucChannel, _ucBandWidth, \
		_eBssSCO, _u4CenterFreq) \
	KAL_NEED_IMPLEMENT(__FILE__, __func__, __LINE__)
#else
void wlanUpdateDfsChannelTable(struct GLUE_INFO *prGlueInfo,
		uint8_t ucRoleIdx, uint8_t ucChannel, uint8_t ucBandWidth,
		enum ENUM_CHNL_EXT eBssSCO, uint32_t u4CenterFreq);
#endif
#endif

#if (CFG_MTK_ANDROID_WMT || WLAN_INCLUDE_PROC)
int set_p2p_mode_handler(struct net_device *netdev,
			 struct PARAM_CUSTOM_P2P_SET_STRUCT p2pmode);
#endif

#if CFG_ENABLE_UNIFY_WIPHY
const struct net_device_ops *wlanGetNdevOps(void);
#endif

#if CFG_MTK_ANDROID_WMT
extern void connectivity_flush_dcache_area(void *addr, size_t len);
extern void connectivity_arch_setup_dma_ops(
	struct device *dev, u64 dma_base,
	u64 size, struct iommu_ops *iommu,
	bool coherent);
#endif

#define wlanHardStartXmit(_prSkb, _prDev)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) \
	(sizeof(arr) / sizeof(((typeof(arr)){})[0]))
#endif
#endif /* _GL_OS_H */
