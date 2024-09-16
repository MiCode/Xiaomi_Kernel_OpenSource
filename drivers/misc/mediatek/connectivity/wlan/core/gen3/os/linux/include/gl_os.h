/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/include/gl_os.h#4
 */

/*
 * ! \file   gl_os.h
 *  \brief  List the external reference to OS for GLUE Layer.
 *
 *  In this file we define the data structure - GLUE_INFO_T to store those objects
 *  we acquired from OS - e.g. TIMER, SPINLOCK, NET DEVICE ... . And all the
 *  external reference (header file, extern func() ..) to OS for GLUE Layer should
 *  also list down here.
 */

#ifndef _GL_OS_H
#define _GL_OS_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
/*------------------------------------------------------------------------------
 * Flags for LINUX(OS) dependent
 *------------------------------------------------------------------------------
 */
#define CFG_MAX_WLAN_DEVICES                1	/* number of wlan card will coexist */

#define CFG_MAX_TXQ_NUM                     4	/* number of tx queue for support multi-queue h/w  */

#define CFG_USE_SPIN_LOCK_BOTTOM_HALF       0	/*
						 * 1: Enable use of SPIN LOCK Bottom Half for LINUX
						 * 0: Disable - use SPIN LOCK IRQ SAVE instead
						 */

#define CFG_TX_PADDING_SMALL_ETH_PACKET     0	/*
						 * 1: Enable - Drop ethernet packet if it < 14 bytes.
						 * And pad ethernet packet with dummy 0 if it < 60 bytes.
						 * 0: Disable
						 */

#define CFG_TX_STOP_NETIF_QUEUE_THRESHOLD   256	/* packets */

#define CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD   512	/* packets */
#define CFG_TX_START_NETIF_PER_QUEUE_THRESHOLD  128	/* packets */

#if defined(MT6630)
#define CHIP_NAME    "MT6630"
#endif

#define DRV_NAME "["CHIP_NAME"]: "

/* Define if target platform is Android.
 * It should already be defined in Android kernel source
 */
#ifndef CONFIG_ANDROID
#define CONFIG_ANDROID      0
#endif

/* for CFG80211 IE buffering mechanism */
#define CFG_CFG80211_IE_BUF_LEN     (512)
#define GLUE_INFO_WSCIE_LENGTH		500

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#ifdef CFG_CFG80211_VERSION
#define CFG80211_VERSION_CODE CFG_CFG80211_VERSION
#else
#define CFG80211_VERSION_CODE LINUX_VERSION_CODE
#endif

#include <linux/version.h>	/* constant of kernel version */

#include <linux/kernel.h>	/* bitops.h */

#include <linux/timer.h>	/* struct timer_list */
#include <linux/jiffies.h>	/* jiffies */
#include <linux/delay.h>	/* udelay and mdelay macro */

#if CONFIG_ANDROID
#ifdef CONFIG_WAKELOCK
#include <linux/wakelock.h>
#else
#include <linux/device.h>
#endif /*CONFIG_WAKELOCK*/
#endif

#include <linux/irq.h>		/* IRQT_FALLING */

#include <linux/netdevice.h>	/* struct net_device, struct net_device_stats */
#include <linux/etherdevice.h>	/* for eth_type_trans() function */
#include <linux/wireless.h>	/* struct iw_statistics */
#include <linux/if_arp.h>
#include <linux/inetdevice.h>	/* struct in_device */

#include <linux/ip.h>		/* struct iphdr */

#include <linux/string.h>	/* for memcpy()/memset() function */
#include <linux/stddef.h>	/* for offsetof() macro */

#include <linux/proc_fs.h>	/* The proc filesystem constants/structures */

#include <linux/rtnetlink.h>	/* for rtnl_lock() and rtnl_unlock() */
#include <linux/kthread.h>	/* kthread_should_stop(), kthread_run() */
#include <linux/uaccess.h>	/* for copy_from_user() */
#include <linux/fs.h>		/* for firmware download */
#include <linux/vmalloc.h>

#include <linux/kfifo.h>	/* for kfifo interface */
#include <linux/cdev.h>		/* for cdev interface */

#include <linux/firmware.h>	/* for firmware download */

#if defined(_HIF_SDIO)
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#endif

#include <linux/random.h>

#include <linux/io.h>		/* readw and writew */

#if WIRELESS_EXT > 12
#include <net/iw_handler.h>
#endif

#include "version.h"
#include "config.h"

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
#include <linux/wireless.h>
#include <net/cfg80211.h>
#endif

#include <linux/module.h>
#include <linux/can/netlink.h>
#include <net/netlink.h>
#include <linux/fb.h>

#ifdef CONFIG_IPV6
#include <linux/ipv6.h>
#include <linux/in6.h>
#include <net/if_inet6.h>
#endif

#if CFG_SUPPORT_PASSPOINT
#include <net/addrconf.h>
#endif /* CFG_SUPPORT_PASSPOINT */

#include "gl_typedef.h"
#include "typedef.h"
#include "queue.h"
#include "gl_kal.h"

#include "hif.h"

#if CFG_SUPPORT_TDLS
#include "tdls.h"
#endif

#include "debug.h"

#include "wlan_lib.h"
#include "wlan_oid.h"

#if CFG_ENABLE_AEE_MSG
#include <mt-plat/aee.h>
#endif

extern BOOLEAN fgIsBusAccessFailed;
extern UINT_8 g_aucNvram[];

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
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
#define GLUE_FLAG_RX            BIT(10)
#define GLUE_FLAG_TX_CMD_DONE   BIT(11)
#define GLUE_FLAG_HIF_TX        BIT(12)
#define GLUE_FLAG_HIF_TX_CMD    BIT(13)
#define GLUE_FLAG_RX_TO_OS      BIT(14)
#define GLUE_FLAG_HIF_FW_OWN    BIT(15)
#define GLUE_FLAG_HAL_MCR_RD	BIT(16)
#define GLUE_FLAG_HAL_MCR_WR	BIT(17)

#define GLUE_FLAG_RX_BIT            (10)
#define GLUE_FLAG_TX_CMD_DONE_BIT   (11)
#define GLUE_FLAG_HIF_TX_BIT        (12)
#define GLUE_FLAG_HIF_TX_CMD_BIT    (13)
#define GLUE_FLAG_RX_TO_OS_BIT      (14)
#define GLUE_FLAG_HIF_FW_OWN_BIT    (15)
#define GLUE_FLAG_HAL_MCR_RD_BIT    (16)
#define GLUE_FLAG_HAL_MCR_WR_BIT    (17)
#endif
#define GLUE_FLAG_RESET_CONN_BIT  (18)

#if CFG_SUPPORT_REPORT_MISC
#define GLUE_FLAG_REPORT_MISC_BIT  (19)
#endif

#if CFG_SUPPORT_MULTITHREAD
#define GLUE_FLAG_HIF_PRT_HIF_DBG_INFO_BIT	(20)
#endif

#define GLUE_BOW_KFIFO_DEPTH        (1024)
/* #define GLUE_BOW_DEVICE_NAME        "MT6620 802.11 AMP" */
#define GLUE_BOW_DEVICE_NAME        "ampc0"

#define WAKE_LOCK_RX_TIMEOUT                            300	/* ms */
#define WAKE_LOCK_THREAD_WAKEUP_TIMEOUT                 50	/* ms */
/*Full2Partial*/
#define UPDATE_FULL_TO_PARTIAL_SCAN_TIMEOUT             60	/* s */

#define FULL_SCAN_MAX_CHANNEL_NUM			40
#define FULL2PARTIALTHRESHOLD				3

#if CFG_SUPPORT_ABORT_SCAN
#define ABORT_SCAN_INTERVAl				UPDATE_FULL_TO_PARTIAL_SCAN_TIMEOUT	/* s */
#define ABORT_SCAN_COUNT				2
#endif
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef struct _GL_WPA_INFO_T {
	UINT_32 u4WpaVersion;
	UINT_32 u4KeyMgmt;
	UINT_32 u4CipherGroup;
	UINT_32 u4CipherPairwise;
	UINT_32 u4AuthAlg;
	BOOLEAN fgPrivacyInvoke;
#if CFG_SUPPORT_802_11W
	UINT_32 u4Mfp;
	UINT_8 ucRSNMfpCap;
#endif
} GL_WPA_INFO_T, *P_GL_WPA_INFO_T;

#if CFG_SUPPORT_REPLAY_DETECTION
struct GL_REPLEY_PN_INFO {
	UINT_8 auPN[16];
	BOOLEAN fgRekey;
};
struct GL_DETECT_REPLAY_INFO {
	UINT_8 ucCurKeyId;
	UINT_8 ucKeyType;
	struct GL_REPLEY_PN_INFO arReplayPNInfo[4];
};
#endif
typedef enum _ENUM_NET_DEV_IDX_T {
	NET_DEV_WLAN_IDX = 0,
	NET_DEV_P2P_IDX,
	NET_DEV_BOW_IDX,
	NET_DEV_NUM
} ENUM_NET_DEV_IDX_T;

typedef enum _ENUM_RSSI_TRIGGER_TYPE {
	ENUM_RSSI_TRIGGER_NONE,
	ENUM_RSSI_TRIGGER_GREATER,
	ENUM_RSSI_TRIGGER_LESS,
	ENUM_RSSI_TRIGGER_TRIGGERED,
	ENUM_RSSI_TRIGGER_NUM
} ENUM_RSSI_TRIGGER_TYPE;

#if CFG_ENABLE_WIFI_DIRECT
typedef enum _ENUM_NET_REG_STATE_T {
	ENUM_NET_REG_STATE_UNREGISTERED,
	ENUM_NET_REG_STATE_REGISTERING,
	ENUM_NET_REG_STATE_REGISTERED,
	ENUM_NET_REG_STATE_UNREGISTERING,
	ENUM_NET_REG_STATE_NUM
} ENUM_NET_REG_STATE_T;
#endif

typedef enum _ENUM_PKT_FLAG_T {
	ENUM_PKT_802_11,	/* 802.11 or non-802.11 */
	ENUM_PKT_802_3,		/* 802.3 or ethernetII */
	ENUM_PKT_1X,		/* 1x frame or not */
	ENUM_PKT_PROTECTED_1X,	/* prtected 1x frame */
	ENUM_PKT_VLAN_EXIST,	/* VLAN tag exist */
	ENUM_PKT_DHCP,		/* DHCP frame */
	ENUM_PKT_ARP,		/* ARP */
	ENUM_PKT_ICMP,		/* ICMP */
	ENUM_PKT_TDLS,		/* TDLS */
	ENUM_PKT_DNS,           /* DNS */

	ENUM_PKT_FLAG_NUM
} ENUM_PKT_FLAG_T;

enum ENUM_WLAN_DRV_BUF_TYPE_T {
	ENUM_BUF_TYPE_NVRAM,
	ENUM_BUF_TYPE_DRV_CFG,
	ENUM_BUF_TYPE_FW_CFG,
	ENUM_BUF_TYPE_NUM
};


typedef struct _GL_IO_REQ_T {
	QUE_ENTRY_T rQueEntry;
	/* wait_queue_head_t       cmdwait_q; */
	BOOL fgRead;
	BOOL fgWaitResp;
	P_ADAPTER_T prAdapter;
	PFN_OID_HANDLER_FUNC pfnOidHandler;
	PVOID pvInfoBuf;
	UINT_32 u4InfoBufLen;
	PUINT_32 pu4QryInfoLen;
	WLAN_STATUS rStatus;
	UINT_32 u4Flag;
} GL_IO_REQ_T, *P_GL_IO_REQ_T;

#if CFG_ENABLE_BT_OVER_WIFI
typedef struct _GL_BOW_INFO {
	BOOLEAN fgIsRegistered;
	dev_t u4DeviceNumber;	/* dynamic device number */
	/* struct kfifo            *prKfifo;       *//* for buffering indicated events */
	struct kfifo rKfifo;	/* for buffering indicated events */
	spinlock_t rSpinLock;	/* spin lock for kfifo */
	struct cdev cdev;
	UINT_32 u4FreqInKHz;	/* frequency */

	UINT_8 aucRole[CFG_BOW_PHYSICAL_LINK_NUM];	/* 0: Responder, 1: Initiator */
	ENUM_BOW_DEVICE_STATE aeState[CFG_BOW_PHYSICAL_LINK_NUM];
	PARAM_MAC_ADDRESS arPeerAddr[CFG_BOW_PHYSICAL_LINK_NUM];

	wait_queue_head_t outq;

#if CFG_BOW_SEPARATE_DATA_PATH
	/* Device handle */
	struct net_device *prDevHandler;
	BOOLEAN fgIsNetRegistered;
#endif

} GL_BOW_INFO, *P_GL_BOW_INFO;
#endif

struct FT_IES {
	UINT_16 u2MDID;
	struct IE_MOBILITY_DOMAIN_T *prMDIE;
	struct IE_FAST_TRANSITION_T *prFTIE;
	IE_TIMEOUT_INTERVAL_T *prTIE;
	P_RSN_INFO_ELEM_T prRsnIE;
	PUINT_8 pucIEBuf;
	UINT_32 u4IeLength;
};

/*
* type definition of pointer to p2p structure
*/
typedef struct _GL_P2P_INFO_T GL_P2P_INFO_T, *P_GL_P2P_INFO_T;

struct GL_Channel_scan_info {
	INT_8 cScanThreshold;
	UINT_8 ucScanFlag;
};

struct _GLUE_INFO_T {
	/* Device handle */
	struct net_device *prDevHandler;

	/* Device Index(index of arWlanDevInfo[]) */
	INT_32 i4DevIdx;

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

	UINT_64 u8Cookie;

	ULONG ulFlag;		/* GLUE_FLAG_XXX */
	UINT_32 u4PendFlag;
	/* UINT_32 u4TimeoutFlag; */
	UINT_32 u4OidCompleteFlag;
	UINT_32 u4ReadyFlag;	/* check if card is ready */

	UINT_32 u4OsMgmtFrameFilter;

	/* Number of pending frames, also used for debuging if any frame is
	 * missing during the process of unloading Driver.
	 *
	 * NOTE(Kevin): In Linux, we also use this variable as the threshold
	 * for manipulating the netif_stop(wake)_queue() func.
	 */
	INT_32 ai4TxPendingFrameNumPerQueue[HW_BSSID_NUM][CFG_MAX_TXQ_NUM];
	INT_32 i4TxPendingFrameNum;
	INT_32 i4TxPendingSecurityFrameNum;

	/* Tx: for NetDev to BSS index mapping */
	NET_INTERFACE_INFO_T arNetInterfaceInfo[HW_BSSID_NUM];

	/* Rx: for BSS index to NetDev mapping */
	/* P_NET_INTERFACE_INFO_T  aprBssIdxToNetInterfaceInfo[HW_BSSID_NUM]; */

	/* current IO request for kalIoctl */
	GL_IO_REQ_T OidEntry;

	/* registry info */
	REG_INFO_T rRegInfo;

	/* firmware */
	struct firmware *prFw;

	/* Host interface related information */
	/* defined in related hif header file */
	GL_HIF_INFO_T rHifInfo;

	/*! \brief wext wpa related information */
	GL_WPA_INFO_T rWpaInfo;
#if CFG_SUPPORT_REPLAY_DETECTION
	struct GL_DETECT_REPLAY_INFO prDetRplyInfo;
#endif

	/* Pointer to ADAPTER_T - main data structure of internal protocol stack */
	P_ADAPTER_T prAdapter;

#ifdef WLAN_INCLUDE_PROC
	struct proc_dir_entry *pProcRoot;
#endif				/* WLAN_INCLUDE_PROC */

	/* Indicated media state */
	ENUM_PARAM_MEDIA_STATE_T eParamMediaStateIndicated;

	/* Device power state D0~D3 */
	ENUM_ACPI_STATE_T ePowerState;

	struct completion rScanComp;	/* indicate scan complete */
	struct completion rHaltComp;	/* indicate main thread halt complete */
	struct completion rPendComp;	/* indicate main thread halt complete */
#if CFG_SUPPORT_MULTITHREAD
	struct completion rHifHaltComp;	/* indicate hif_thread halt complete */
	struct completion rRxHaltComp;	/* indicate hif_thread halt complete */
	struct completion rHalRDMCRComp;
	struct completion rHalWRMCRComp;

	UINT_32 u4TxThreadPid;
	UINT_32 u4RxThreadPid;
	UINT_32 u4HifThreadPid;
#endif
	WLAN_STATUS rPendStatus;

	QUE_T rTxQueue;

	/* OID related */
	QUE_T rCmdQueue;
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

	struct timer_list tickfn;

#if CFG_SUPPORT_EXT_CONFIG
	UINT_16 au2ExtCfg[256];	/* NVRAM data buffer */
	UINT_32 u4ExtCfgLength;	/* 0 means data is NOT valid */
#endif

#if CFG_ENABLE_BT_OVER_WIFI
	GL_BOW_INFO rBowInfo;
#endif

#if CFG_ENABLE_WIFI_DIRECT
	P_GL_P2P_INFO_T prP2PInfo;
#if CFG_SUPPORT_P2P_RSSI_QUERY
	/* Wireless statistics struct net_device */
	struct iw_statistics rP2pIwStats;
#endif
#endif
	BOOLEAN fgWpsActive;
	UINT_8 aucWSCIE[GLUE_INFO_WSCIE_LENGTH];	/*for probe req */
	UINT_16 u2WSCIELen;

	/* NVRAM availability */
	BOOLEAN fgNvramAvailable;

	BOOLEAN fgMcrAccessAllowed;

	/* MAC Address Overridden by IOCTL */
	BOOLEAN fgIsMacAddrOverride;
	PARAM_MAC_ADDRESS rMacAddrOverride;

	SET_TXPWR_CTRL_T rTxPwr;

	/* for cfg80211 scan done indication */
	struct cfg80211_scan_request *prScanRequest;

	/* for cfg80211 scheduled scan */
	struct cfg80211_sched_scan_request *prSchedScanRequest;

	/* to indicate registered or not */
	BOOLEAN fgIsRegistered;

	/* for cfg80211 connected indication */
	UINT_32 u4RspIeLength;
	UINT_8 aucRspIe[CFG_CFG80211_IE_BUF_LEN];

	UINT_32 u4ReqIeLength;
	UINT_8 aucReqIe[CFG_CFG80211_IE_BUF_LEN];

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
	BOOLEAN fgEnSdioTestPattern;
	BOOLEAN fgSdioReadWriteMode;
	BOOLEAN fgIsSdioTestInitialized;
	UINT_8 aucSdioTestBuffer[256];
#endif

	BOOLEAN fgIsInSuspendMode;

#if CFG_SUPPORT_PASSPOINT
	UINT_8 ucHotspotConfig;
	BOOLEAN fgConnectHS20AP;

	BOOLEAN fgIsDad;
	UINT_8 aucDADipv4[4];
	BOOLEAN fgIs6Dad;
	UINT_8 aucDADipv6[16];
#endif				/* CFG_SUPPORT_PASSPOINT */

	KAL_WAKE_LOCK_T rIntrWakeLock;
	KAL_WAKE_LOCK_T rTimeoutWakeLock;

#if CFG_MET_PACKET_TRACE_SUPPORT
	BOOLEAN fgMetProfilingEn;
	UINT_16 u2MetUdpPort;
#endif

#if CFG_SUPPORT_SNIFFER
	BOOLEAN fgIsEnableMon;
	struct net_device *prMonDevHandler;
	struct work_struct monWork;
#endif

	INT_32 i4RssiCache;
	UINT_32 u4LinkSpeedCache;

	UINT_32 IsrCnt;
	UINT_32 IsrPassCnt;
	UINT_32 TaskIsrCnt;

	UINT_32 IsrAbnormalCnt;
	UINT_32 IsrSoftWareCnt;
	UINT_32 IsrTxCnt;
	UINT_32 IsrRxCnt;

	UINT_64 u8SkbToDriver;
	UINT_64 u8SkbFreed;

	UINT_32 u4Register;
	UINT_32 u4RegValue;
	PUINT_32 prRegValue;

	UINT_64 u8HifIntTime;

	/* save partial scan channel information */
	PARTIAL_SCAN_INFO rScanChannelInfo;
	PUINT_8 pucScanChannel;

	/* Full2Partial */
	OS_SYSTIME u4LastFullScanTime;
#if CFG_SUPPORT_ABORT_SCAN
	OS_SYSTIME u4LastNormalScanTime;
	UINT_8	ucAbortScanCnt;
#endif
	/* full scan or partial scan */
	struct GL_Channel_scan_info arChannelScanInfo[FULL_SCAN_MAX_CHANNEL_NUM];
	PARTIAL_SCAN_INFO rFullScanApChannel;
	PUINT_8 pucFullScan2PartialChannel;
	struct FT_IES rFtIeForTx;
	struct cfg80211_ft_event_params rFtEventParam;

	/* FW Roaming */
	/* store the FW roaming enable state which FWK determines */
	/* if it's = 0, ignore the black/whitelists settings from FWK */
	UINT_32 u4FWRoamingEnable;
};

typedef irqreturn_t(*PFN_WLANISR) (int irq, void *dev_id, struct pt_regs *regs);

typedef void (*PFN_LINUX_TIMER_FUNC) (unsigned long);

/* generic sub module init/exit handler
*   now, we only have one sub module, p2p
*/
#if CFG_ENABLE_WIFI_DIRECT
typedef BOOLEAN(*SUB_MODULE_INIT) (P_GLUE_INFO_T prGlueInfo);
typedef BOOLEAN(*SUB_MODULE_EXIT) (P_GLUE_INFO_T prGlueInfo);

typedef struct _SUB_MODULE_HANDLER {
	SUB_MODULE_INIT subModInit;
	SUB_MODULE_EXIT subModExit;
	BOOLEAN fgIsInited;
} SUB_MODULE_HANDLER, *P_SUB_MODULE_HANDLER;

#endif

#if CONFIG_NL80211_TESTMODE

enum TestModeCmdType {
	TESTMODE_CMD_ID_SW_CMD = 1,
	TESTMODE_CMD_ID_WAPI = 2,
	TESTMODE_CMD_ID_HS20 = 3,
	TESTMODE_CMD_ID_STR_CMD = 102,
	NUM_OF_TESTMODE_CMD_ID
};

#if CFG_SUPPORT_PASSPOINT
enum Hs20CmdType {
	HS20_CMD_ID_SET_BSSID_POOL = 0,
	NUM_OF_HS20_CMD_ID
};
#endif /* CFG_SUPPORT_PASSPOINT */

typedef struct _NL80211_DRIVER_TEST_MODE_PARAMS {
	UINT_32 index;
	UINT_32 buflen;
} NL80211_DRIVER_TEST_MODE_PARAMS, *P_NL80211_DRIVER_TEST_MODE_PARAMS;

/*SW CMD */
typedef struct _NL80211_DRIVER_SW_CMD_PARAMS {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	UINT_8 set;
	UINT_32 adr;
	UINT_32 data;
} NL80211_DRIVER_SW_CMD_PARAMS, *P_NL80211_DRIVER_SW_CMD_PARAMS;

struct iw_encode_exts {
	__u32 ext_flags;	/*!< IW_ENCODE_EXT_* */
	__u8 tx_seq[IW_ENCODE_SEQ_MAX_SIZE];	/*!< LSB first */
	__u8 rx_seq[IW_ENCODE_SEQ_MAX_SIZE];	/*!< LSB first */
	__u8 addr[MAC_ADDR_LEN];	/*
					 * !< ff:ff:ff:ff:ff:ff for broadcast/multicast
					 *   (group) keys or unicast address for
					 *   individual keys
					 */
	__u16 alg;		/* !< IW_ENCODE_ALG_* */
	__u16 key_len;
	__u8 key[32];
};

/*SET KEY EXT */
typedef struct _NL80211_DRIVER_SET_KEY_EXTS {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	UINT_8 key_index;
	UINT_8 key_len;
	struct iw_encode_exts ext;
} NL80211_DRIVER_SET_KEY_EXTS, *P_NL80211_DRIVER_SET_KEY_EXTS;

#if CFG_SUPPORT_PASSPOINT

struct param_hs20_set_bssid_pool {
	UINT_8 fgBssidPoolIsEnable;
	UINT_8 ucNumBssidPool;
	UINT_8 arBssidPool[8][ETH_ALEN];
};

struct wpa_driver_hs20_data_s {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	enum Hs20CmdType CmdType;
	struct param_hs20_set_bssid_pool hs20_set_bssid_pool;
};

#endif /* CFG_SUPPORT_PASSPOINT */

#endif

typedef struct _NETDEV_PRIVATE_GLUE_INFO {
	P_GLUE_INFO_T prGlueInfo;
	UINT_8 ucBssIdx;
} NETDEV_PRIVATE_GLUE_INFO, *P_NETDEV_PRIVATE_GLUE_INFO;

typedef struct _PACKET_PRIVATE_DATA {
	/* tx/rx both use cb */
	QUE_ENTRY_T rQueEntry;  /* 16byte total:16 */

	UINT_8 ucBssIdx;	/* 1byte */
	/* only rx use cb */
	BOOLEAN fgIsIndependentPkt; /* 1byte */
	/* only tx use cb */
	UINT_8 ucTid;		/* 1byte */
	UINT_8 ucHeaderLen;	/* 1byte */
	UINT_8 ucProfilingFlag;	/* 1byte */
	UINT_8 ucSeqNo;		/* 1byte */
	UINT_16 u2Flag;		/* 2byte total:24 */

	UINT_16 u2IpId;		/* 2byte */
	UINT_16 u2FrameLen;	/* 2byte */
	OS_SYSTIME rArrivalTime;/* 4byte total:32 */

	UINT_64 u8ArriveTime;	/* 8byte total:40 */
} PACKET_PRIVATE_DATA, *P_PACKET_PRIVATE_DATA;

typedef struct _PACKET_PRIVATE_RX_DATA {
	UINT_64 u8IntTime;	/* 8byte */
	UINT_64 u8RxTime;	/* 8byte */
} PACKET_PRIVATE_RX_DATA, *P_PACKET_PRIVATE_RX_DATA;

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
/*----------------------------------------------------------------------------*/
/* Macros of SPIN LOCK operations for using in Glue Layer                     */
/*----------------------------------------------------------------------------*/
#if CFG_USE_SPIN_LOCK_BOTTOM_HALF
#define GLUE_SPIN_LOCK_DECLARATION()
#define GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	    { \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_lock_bh(&(prGlueInfo->rSpinLock[rLockCategory])); \
	    }
#define GLUE_RELEASE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	    { \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_unlock_bh(&(prGlueInfo->rSpinLock[rLockCategory])); \
	    }
#else /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */
#define GLUE_SPIN_LOCK_DECLARATION()                        ULONG __ulFlags = 0
#define GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	    { \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_lock_irqsave(&(prGlueInfo)->rSpinLock[rLockCategory], __ulFlags); \
	    }
#define GLUE_RELEASE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	    { \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_unlock_irqrestore(&(prGlueInfo->rSpinLock[rLockCategory]), __ulFlags); \
	    }
#endif /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */

/*----------------------------------------------------------------------------*/
/* Macros for accessing Reserved Fields of native packet                      */
/*----------------------------------------------------------------------------*/

#define GLUE_GET_PKT_PRIVATE_DATA(_p) \
	((P_PACKET_PRIVATE_DATA)(&(((struct sk_buff *)(_p))->cb[0])))

#define GLUE_GET_PKT_QUEUE_ENTRY(_p)    \
	    (&(GLUE_GET_PKT_PRIVATE_DATA(_p)->rQueEntry))

#define GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry)  \
	    ((P_NATIVE_PACKET) (((ULONG)_prQueueEntry) - offsetof(struct sk_buff, cb[0])))

#define GLUE_SET_PKT_TID(_p, _tid) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucTid = (UINT_8)(_tid))

#define GLUE_GET_PKT_TID(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucTid)

#define GLUE_GET_PKT_FLAG(_p) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag)

#define GLUE_SET_PKT_FLAG(_p, _flag) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag |= BIT(_flag))

#define GLUE_TEST_PKT_FLAG(_p, _flag) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag & BIT(_flag))

#define GLUE_IS_PKT_FLAG_SET(_p) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2Flag)

#define GLUE_SET_PKT_BSS_IDX(_p, _ucBssIndex) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucBssIdx = (UINT_8)(_ucBssIndex))

#define GLUE_GET_PKT_BSS_IDX(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucBssIdx)

#define GLUE_SET_PKT_HEADER_LEN(_p, _ucMacHeaderLen) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucHeaderLen = (UINT_8)(_ucMacHeaderLen))

#define GLUE_GET_PKT_HEADER_LEN(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucHeaderLen)

#define GLUE_SET_PKT_FRAME_LEN(_p, _u2PayloadLen) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2FrameLen = (UINT_16)(_u2PayloadLen))

#define GLUE_GET_PKT_FRAME_LEN(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->u2FrameLen)

#define GLUE_SET_PKT_ARRIVAL_TIME(_p, _rSysTime) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->rArrivalTime = (OS_SYSTIME)(_rSysTime))

#define GLUE_GET_PKT_ARRIVAL_TIME(_p)    \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->rArrivalTime)

#define GLUE_SET_PKT_IP_ID(_p, _u2IpId) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u2IpId = (UINT_16)(_u2IpId))

#define GLUE_GET_PKT_IP_ID(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->u2IpId)

#define GLUE_SET_PKT_SEQ_NO(_p, _ucSeqNo) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->ucSeqNo = (UINT_8)(_ucSeqNo))

#define GLUE_GET_PKT_SEQ_NO(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucSeqNo)

#define GLUE_SET_PKT_FLAG_PROF_MET(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucProfilingFlag |= BIT(0))

#define GLUE_GET_PKT_IS_PROF_MET(_p) \
	    (GLUE_GET_PKT_PRIVATE_DATA(_p)->ucProfilingFlag & BIT(0))

#define GLUE_GET_PKT_ETHER_DEST_ADDR(_p)    \
	    ((PUINT_8)&(((struct sk_buff *)(_p))->data))

/* Check validity of prDev, private data, and pointers */
#define GLUE_CHK_DEV(prDev) \
	((prDev && *((P_GLUE_INFO_T *) netdev_priv(prDev))) ? TRUE : FALSE)

#define GLUE_CHK_PR2(prDev, pr2) \
	((GLUE_CHK_DEV(prDev) && pr2) ? TRUE : FALSE)

#define GLUE_CHK_PR3(prDev, pr2, pr3) \
	((GLUE_CHK_PR2(prDev, pr2) && pr3) ? TRUE : FALSE)

#define GLUE_CHK_PR4(prDev, pr2, pr3, pr4) \
	((GLUE_CHK_PR3(prDev, pr2, pr3) && pr4) ? TRUE : FALSE)

#define GLUE_SET_EVENT(pr) \
	kalSetEvent(pr)

#define GLUE_GET_INDEPENDENT_PKT(_p)    \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->fgIsIndependentPkt)
#define GLUE_SET_INDEPENDENT_PKT(_p, _fgIsIndePkt) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->fgIsIndependentPkt = _fgIsIndePkt)

#define GLUE_SET_PKT_XTIME(_p, _rSysTime) \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u8ArriveTime = (UINT_64)(_rSysTime))

#define GLUE_GET_PKT_XTIME(_p)    \
	(GLUE_GET_PKT_PRIVATE_DATA(_p)->u8ArriveTime)

#define GLUE_GET_PKT_PRIVATE_RX_DATA(_p) \
	((P_PACKET_PRIVATE_RX_DATA)(&(((struct sk_buff *)(_p))->cb[24])))

#define GLUE_RX_SET_PKT_INT_TIME(_p, _rTime) \
	(GLUE_GET_PKT_PRIVATE_RX_DATA(_p)->u8IntTime = (UINT_64)(_rTime))

#define GLUE_RX_GET_PKT_INT_TIME(_p) \
	(GLUE_GET_PKT_PRIVATE_RX_DATA(_p)->u8IntTime)

#define GLUE_RX_SET_PKT_RX_TIME(_p, _rTime) \
	(GLUE_GET_PKT_PRIVATE_RX_DATA(_p)->u8RxTime = (UINT_64)(_rTime))

#define GLUE_RX_GET_PKT_RX_TIME(_p) \
	(GLUE_GET_PKT_PRIVATE_RX_DATA(_p)->u8RxTime)

#define GLUE_INC_REF_CNT(_refCount)     atomic_inc((atomic_t *)&(_refCount))
#define GLUE_DEC_REF_CNT(_refCount)     atomic_dec((atomic_t *)&(_refCount))
#define GLUE_GET_REF_CNT(_refCount)     atomic_read((atomic_t *)&(_refCount))

#define DbgPrint(...)

/*----------------------------------------------------------------------------*/
/* Macros of Data Type Check                                                  */
/*----------------------------------------------------------------------------*/
/* Kevin: we don't have to call following function to inspect the data structure.
 * It will check automatically while at compile time.
 */
static __KAL_INLINE__ VOID glPacketDataTypeCheck(VOID)
{

	DATA_STRUCT_INSPECTING_ASSERT(sizeof(PACKET_PRIVATE_DATA) <= sizeof(((struct sk_buff *) 0)->cb));

}

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
#ifdef WLAN_INCLUDE_PROC
INT_32 procRemoveProcfs(VOID);

INT_32 procCreateFsEntry(P_GLUE_INFO_T prGlueInfo);
INT_32 procInitFs(VOID);
INT_32 procUninitProcFs(VOID);
#endif /* WLAN_INCLUDE_PROC */

#if CFG_ENABLE_BT_OVER_WIFI
BOOLEAN glRegisterAmpc(P_GLUE_INFO_T prGlueInfo);

BOOLEAN glUnregisterAmpc(P_GLUE_INFO_T prGlueInfo);
#endif

P_GLUE_INFO_T wlanGetGlueInfo(VOID);

UINT_16 wlanSelectQueue(struct net_device *dev, struct sk_buff *skb,
			void *accel_priv, select_queue_fallback_t fallback);

VOID wlanDebugInit(VOID);

WLAN_STATUS wlanSetDebugLevel(IN UINT_32 u4DbgIdx, IN UINT_32 u4DbgMask);

WLAN_STATUS wlanGetDebugLevel(IN UINT_32 u4DbgIdx, OUT PUINT_32 pu4DbgMask);

VOID wlanSetSuspendMode(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgEnable);

BOOLEAN wlanIsFwOwn(VOID);

/*******************************************************************************
*			 E X T E R N A L   F U N C T I O N S / V A R I A B L E
********************************************************************************
*/

extern void wlanRegisterNotifier(void);
extern void wlanUnregisterNotifier(void);


typedef int (*set_p2p_mode) (struct net_device *netdev, PARAM_CUSTOM_P2P_SET_STRUCT_T p2pmode);
extern void register_set_p2p_mode_handler(set_p2p_mode handler);

#if CFG_ENABLE_EARLY_SUSPEND
extern int glRegisterEarlySuspend(struct early_suspend *prDesc,
				  early_suspend_callback wlanSuspend, late_resume_callback wlanResume);

extern int glUnregisterEarlySuspend(struct early_suspend *prDesc);
#endif

#if CFG_MET_PACKET_TRACE_SUPPORT
VOID kalMetTagPacket(IN P_GLUE_INFO_T prGlueInfo, IN P_NATIVE_PACKET prPacket, IN ENUM_TX_PROFILING_TAG_T eTag);

VOID kalMetInit(IN P_GLUE_INFO_T prGlueInfo);
#endif

VOID wlanUpdateChannelTable(P_GLUE_INFO_T prGlueInfo);

#ifdef FW_CFG_SUPPORT
INT_32 cfgCreateProcEntry(P_GLUE_INFO_T prGlueInfo);
INT_32 cfgRemoveProcEntry(void);
#endif

typedef UINT_8 (*file_buf_handler) (PVOID ctx, const CHAR __user *buf, UINT_16 length);
extern VOID register_file_buf_handler(file_buf_handler handler, PVOID ctx, UINT_8 ucType);
extern void update_driver_loaded_status(uint8_t loaded);
#endif /* _GL_OS_H */
