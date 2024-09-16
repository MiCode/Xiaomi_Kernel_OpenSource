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
#define CFG_MAX_WLAN_DEVICES			1	/* number of wlan card will coexist */

#define CFG_MAX_TXQ_NUM				4	/* number of tx queue for support multi-queue h/w  */


#define CFG_USE_SPIN_LOCK_BOTTOM_HALF		0	/* 1: Enable use of SPIN LOCK Bottom Half for LINUX
							*   0: Disable - use SPIN LOCK IRQ SAVE instead
							*/

#define CFG_TX_PADDING_SMALL_ETH_PACKET		0	/* 1: Enable - Drop ethernet packet if it < 14 bytes.
							*   And pad ethernet packet with dummy 0 if it < 60 bytes.
							*   0: Disable
							*/

#define CFG_TX_STOP_NETIF_QUEUE_THRESHOLD	256	/* packets */

#define CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD	(CFG_TX_MAX_PKT_NUM / 2)
#define CFG_TX_START_NETIF_PER_QUEUE_THRESHOLD	(CFG_TX_MAX_PKT_NUM / 8)

#define ETH_P_1X				0x888E
#define IPTOS_PREC_OFFSET			5
#define USER_PRIORITY_DEFAULT			0

#define ETH_WPI_1X				0x88B4

#define ETH_HLEN				14
#define ETH_TYPE_LEN_OFFSET			12
#define ETH_P_IP				0x0800
#define ETH_P_1X				0x888E
#define ETH_P_PRE_1X				0x88C7
#define ETH_P_ARP				0x0806

#define ARP_PRO_REQ				1
#define ARP_PRO_RSP				2

#define IPVERSION				4
#define IP_HEADER_LEN				20
#define IP_PROTO_HLEN				9

#define IP_PRO_ICMP				0x01
#define IP_PRO_UDP				0x11
#define IP_PRO_TCP				0x06

#define UDP_PORT_DHCPS				0x43
#define UDP_PORT_DHCPC				0x44
#define UDP_PORT_DNS				0x35

#define IPVH_VERSION_OFFSET			4	/* For Little-Endian */
#define IPVH_VERSION_MASK			0xF0
#define IPTOS_PREC_OFFSET			5
#define IPTOS_PREC_MASK				0xE0

#define SOURCE_PORT_LEN				2
/* NOTE(Kevin): Without IP Option Length */
#define LOOK_AHEAD_LEN				(ETH_HLEN + IP_HEADER_LEN + SOURCE_PORT_LEN)

/* 802.2 LLC/SNAP */
#define ETH_LLC_OFFSET				(ETH_HLEN)
#define ETH_LLC_LEN				3
#define ETH_LLC_DSAP_SNAP			0xAA
#define ETH_LLC_SSAP_SNAP			0xAA
#define ETH_LLC_CONTROL_UNNUMBERED_INFORMATION	0x03

/* Bluetooth SNAP */
#define ETH_SNAP_OFFSET				(ETH_HLEN + ETH_LLC_LEN)
#define ETH_SNAP_LEN				5
#define ETH_SNAP_BT_SIG_OUI_0			0x00
#define ETH_SNAP_BT_SIG_OUI_1			0x19
#define ETH_SNAP_BT_SIG_OUI_2			0x58

#define BOW_PROTOCOL_ID_SECURITY_FRAME		0x0003

#if defined(MT6620)
#define CHIP_NAME    "MT6620"
#elif defined(MT6628)
#define CHIP_NAME    "MT6582"
#endif

#define DRV_NAME "["CHIP_NAME"]: "

/* Define if target platform is Android.
 * It should already be defined in Android kernel source
 */
#ifndef CONFIG_ANDROID
#define CONFIG_ANDROID      0
#endif

/* for CFG80211 IE buffering mechanism */
#define CFG_CFG80211_IE_BUF_LEN		(512)
#define GLUE_INFO_WSCIE_LENGTH		500


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/version.h>	/* constant of kernel version */

#include <linux/kernel.h>	/* bitops.h */

#include <linux/timer.h>	/* struct timer_list */
#include <linux/jiffies.h>	/* jiffies */
#include <linux/delay.h>	/* udelay and mdelay macro */

#if CONFIG_ANDROID
#if (KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE)
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
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
#include <linux/fb.h>

#if defined(_HIF_SDIO)
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#endif

#include <linux/random.h>

#include <linux/lockdep.h>
#include <linux/time.h>

#include <linux/io.h>		/* readw and writew */

#if WIRELESS_EXT > 12
#include <net/iw_handler.h>
#endif

#ifdef CFG_CFG80211_VERSION
#define CFG80211_VERSION_CODE CFG_CFG80211_VERSION
#else
#define CFG80211_VERSION_CODE LINUX_VERSION_CODE
#endif

#include "version.h"
#include "config.h"

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
#include <linux/wireless.h>
#include <net/cfg80211.h>
#endif

#include <linux/module.h>

#if CFG_SUPPORT_HOTSPOT_2_0
#include <net/addrconf.h>
#endif

#include "gl_typedef.h"
#include "typedef.h"
#include "queue.h"
#include "gl_kal.h"
#include "hif.h"
#if CFG_CHIP_RESET_SUPPORT
#include "gl_rst.h"
#endif

#if (CFG_SUPPORT_TDLS == 1)
#include "tdls_extr.h"
#include "tdls.h"
#endif
#include "debug.h"

#include "wlan_lib.h"
#include "wlan_oid.h"

#if CFG_ENABLE_AEE_MSG
#include <mt-plat/aee.h>
#endif

extern BOOLEAN fgIsBusAccessFailed;
extern const struct ieee80211_iface_combination *p_mtk_sta_iface_combos;
extern const INT_32 mtk_sta_iface_combos_num;
extern UINT_8 g_aucNvram[];

#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
typedef void (*wifi_fwlog_event_func_cb)(int, int);
/* adaptor ko */
extern int  wifi_fwlog_onoff_status(void);
extern void wifi_fwlog_event_func_register(wifi_fwlog_event_func_cb pfFwlog);
#endif

extern struct net_device *gPrDev;
extern void update_driver_loaded_status(uint8_t loaded);

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define GLUE_FLAG_HALT          BIT(0)
#define GLUE_FLAG_INT           BIT(1)
#define GLUE_FLAG_OID           BIT(2)
#define GLUE_FLAG_TIMEOUT       BIT(3)
#define GLUE_FLAG_TXREQ         BIT(4)
#define GLUE_FLAG_SUB_MOD_INIT  BIT(5)
#define GLUE_FLAG_SUB_MOD_EXIT  BIT(6)
#define GLUE_FLAG_SUB_MOD_MULTICAST BIT(7)
#define GLUE_FLAG_FRAME_FILTER      BIT(8)
#define GLUE_FLAG_FRAME_FILTER_AIS  BIT(9)
#define GLUE_FLAG_HIF_LOOPBK_AUTO   BIT(10)
#define GLUE_FLAG_HALT_BIT          (0)
#define GLUE_FLAG_INT_BIT           (1)
#define GLUE_FLAG_OID_BIT           (2)
#define GLUE_FLAG_TIMEOUT_BIT       (3)
#define GLUE_FLAG_TXREQ_BIT         (4)
#define GLUE_FLAG_SUB_MOD_INIT_BIT  (5)
#define GLUE_FLAG_SUB_MOD_EXIT_BIT  (6)
#define GLUE_FLAG_SUB_MOD_MULTICAST_BIT (7)
#define GLUE_FLAG_FRAME_FILTER_BIT  (8)
#define GLUE_FLAG_FRAME_FILTER_AIS_BIT  (9)
#define GLUE_FLAG_HIF_LOOPBK_AUTO_BIT   (10)
#define GLUE_FLAG_RX_BIT            (11)
#define GLUE_FLAG_RX             BIT(GLUE_FLAG_RX_BIT)
#define GLUE_FLAG_RX_PROCESS (GLUE_FLAG_HALT | GLUE_FLAG_RX)

#define GLUE_BOW_KFIFO_DEPTH        (1024)
/* #define GLUE_BOW_DEVICE_NAME        "MT6620 802.11 AMP" */
#define GLUE_BOW_DEVICE_NAME        "ampc0"

/*Full2Partial*/
#define UPDATE_FULL_TO_PARTIAL_SCAN_TIMEOUT     60 /* s */

#define FULL_SCAN_MAX_CHANNEL_NUM               40

#define WAKE_LOCK_RX_TIMEOUT         300 /* ms */

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

typedef enum _ENUM_RSSI_TRIGGER_TYPE {
	ENUM_RSSI_TRIGGER_NONE,
	ENUM_RSSI_TRIGGER_GREATER,
	ENUM_RSSI_TRIGGER_LESS,
	ENUM_RSSI_TRIGGER_TRIGGERED,
	ENUM_RSSI_TRIGGER_NUM
} ENUM_RSSI_TRIGGER_TYPE;

#if CFG_ENABLE_WIFI_DIRECT
typedef enum _ENUM_SUB_MODULE_IDX_T {
	P2P_MODULE = 0,
	SUB_MODULE_NUM
} ENUM_SUB_MODULE_IDX_T;

typedef enum _ENUM_NET_REG_STATE_T {
	ENUM_NET_REG_STATE_UNREGISTERED,
	ENUM_NET_REG_STATE_REGISTERING,
	ENUM_NET_REG_STATE_REGISTERED,
	ENUM_NET_REG_STATE_UNREGISTERING,
	ENUM_NET_REG_STATE_NUM
} ENUM_NET_REG_STATE_T;

enum ENUM_WLAN_DRV_BUF_TYPE_T {
	ENUM_BUF_TYPE_NVRAM,
	ENUM_BUF_TYPE_DRV_CFG,
	ENUM_BUF_TYPE_FW_CFG,
	ENUM_BUF_TYPE_NUM
};

#endif

typedef struct _GL_IO_REQ_T {
	QUE_ENTRY_T rQueEntry;
	/* wait_queue_head_t       cmdwait_q; */
	BOOLEAN fgRead;
	BOOLEAN fgWaitResp;
#if CFG_ENABLE_WIFI_DIRECT
	BOOLEAN fgIsP2pOid;
#endif
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
/* struct kfifo            *prKfifo; */    /* for buffering indicated events */
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

#if (CFG_SUPPORT_TDLS == 1)
typedef struct _TDLS_INFO_LINK_T {
	/* start time when link is built, end time when link is broken */
	unsigned long jiffies_start, jiffies_end;

	/* the peer MAC */
	UINT8 aucPeerMac[6];

	/* broken reason */
	UINT8 ucReasonCode;

	/* TRUE: torn down is triggerred by us */
	UINT8 fgIsFromUs;

	/* duplicate count; same reason */
	UINT8 ucDupCount;

	/* HT capability */
#define TDLS_INFO_LINK_HT_CAP_SUP			0x01
	UINT8 ucHtCap;
#define TDLS_INFO_LINK_HT_BA_SETUP			0x01
#define TDLS_INFO_LINK_HT_BA_SETUP_OK		0x02
#define TDLS_INFO_LINK_HT_BA_SETUP_DECLINE	0x04
#define TDLS_INFO_LINK_HT_BA_PEER			0x10
#define TDLS_INFO_LINK_HT_BA_RSP_OK			0x20
#define TDLS_INFO_LINK_HT_BA_RSP_DECLINE	0x40
	UINT8 ucHtBa[8];	/* TID0 ~ TID7 */
} TDLS_INFO_LINK_T;

typedef struct _TDLS_INFO_T {
	/* link history */
#define TDLS_LINK_HISTORY_MAX				30
	TDLS_INFO_LINK_T rLinkHistory[TDLS_LINK_HISTORY_MAX];
	UINT32 u4LinkIdx;

	/* TRUE: support 20/40 bandwidth in TDLS link */
	BOOLEAN fgIs2040Sup;

	/* total TDLS link count */
	INT8 cLinkCnt;
} TDLS_INFO_T;
#endif /* CFG_SUPPORT_TDLS */

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

struct _GLUE_INFO_T {
	/* Device handle */
	struct net_device *prDevHandler;

	/* Device Index(index of arWlanDevInfo[]) */
	INT_32 i4DevIdx;

	/* Device statistics */
	struct net_device_stats rNetDevStats;

	/* Wireless statistics struct net_device */
	struct iw_statistics rIwStats;

	/* spinlock to sync power save mechanism */
	spinlock_t rSpinLock[SPIN_LOCK_NUM];

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
	INT_32 ai4TxPendingFrameNumPerQueue[4][CFG_MAX_TXQ_NUM];
	INT_32 i4TxPendingFrameNum;
	INT_32 i4TxPendingSecurityFrameNum;

	/* current IO request for kalIoctl */
	GL_IO_REQ_T OidEntry;

	/* registry info */
	REG_INFO_T rRegInfo;

	/* firmware */
	const struct firmware *prFw;

	/* Host interface related information */
	/* defined in related hif header file */
	GL_HIF_INFO_T rHifInfo;

	/*! \brief wext wpa related information */
	GL_WPA_INFO_T rWpaInfo;

	/* Pointer to ADAPTER_T - main data structure of internal protocol stack */
	P_ADAPTER_T prAdapter;

#ifdef WLAN_INCLUDE_PROC
	struct proc_dir_entry *pProcRoot;
#endif				/* WLAN_INCLUDE_PROC */

	/* Indicated media state */
	ENUM_PARAM_MEDIA_STATE_T eParamMediaStateIndicated;

	struct completion rScanComp;	/* indicate scan complete */
	struct completion rHaltComp;	/* indicate main thread halt complete */
	struct completion rRxHaltComp;	/* indicate hif_thread halt complete */
	struct completion rPendComp;	/* indicate main thread halt complete */
	struct completion rP2pReq;	/* indicate p2p request(request channel/frame tx)
					 * complete
					 */
#if CFG_SUPPORT_NCHO
	struct completion rAisChGrntComp;	/* indicate Ais channel grant complete */
#endif
#if CFG_ENABLE_WIFI_DIRECT
	struct completion rSubModComp;	/*indicate sub module init or exit complete */
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
	wait_queue_head_t waitq_rx;
	struct task_struct *rx_thread;
	KAL_WAKE_LOCK_T *rTimeoutWakeLock;

	struct timer_list tickfn;

#if CFG_SUPPORT_EXT_CONFIG
	UINT_16 au2ExtCfg[256];	/* NVRAM data buffer */
	UINT_32 u4ExtCfgLength;	/* 0 means data is NOT valid */
#endif

#if 1				/* CFG_SUPPORT_WAPI */
	/* Should be large than the PARAM_WAPI_ASSOC_INFO_T */
	UINT_8 aucWapiAssocInfoIEs[42];
	UINT_16 u2WapiAssocInfoIESz;
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
	UINT_8 aucWSCAssocInfoIE[200];	/*for Assoc req */
	UINT_16 u2WSCAssocInfoIELen;

#if CFG_SUPPORT_HOTSPOT_2_0
	UINT_8 aucHS20AssocInfoIE[200];	/*for Assoc req */
	UINT_16 u2HS20AssocInfoIELen;
	UINT_8 ucHotspotConfig;
	BOOLEAN fgConnectHS20AP;
#endif

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

	KAL_WAKE_LOCK_T *rAhbIsrWakeLock;

#if CFG_SUPPORT_HOTSPOT_2_0
	BOOLEAN fgIsDad;
	UINT_8 aucDADipv4[4];
	BOOLEAN fgIs6Dad;
	UINT_8 aucDADipv6[16];
#endif
#if (CFG_SUPPORT_MET_PROFILING == 1)
	UINT_8 u8MetProfEnable;
	INT_16 u16MetUdpPort;
#endif
	BOOLEAN fgPoorlinkValid;
	UINT_64 u8Statistic[2];
	UINT_64 u8TotalFailCnt;
	UINT_32 u4LinkspeedThreshold;
	INT_32 i4RssiThreshold;
	INT_32 i4RssiCache;
	UINT_32 u4LinkSpeedCache;

#if (CFG_SUPPORT_TDLS == 1)
	/* record TX rate used to be a reference for TDLS setup */
	ULONG ulLastUpdate;
	/* The last one of STA_HASH_SIZE is used as target sta */
	struct ksta_info *prStaHash[STA_HASH_SIZE + 1];
	INT_32 i4TdlsLastRx;
	INT_32 i4TdlsLastTx;
	enum MTK_TDLS_STATUS eTdlsStatus;
	ENUM_NETWORK_TYPE_INDEX_T eTdlsNetworkType;

	TDLS_INFO_T rTdlsLink;

	UINT8 aucTdlsHtPeerMac[6];
	IE_HT_CAP_T rTdlsHtCap;	/* temp to queue HT capability element */

	/*
	 * [0~7]: jiffies
	 * [8~13]: Peer MAC
	 * [14]: Reason Code
	 * [15]: From us or peer
	 * [16]: Duplicate Count
	 */
	/* UINT8 aucTdlsDisconHistory[TDLS_DISCON_HISTORY_MAX][20]; */
	/* UINT32 u4TdlsDisconIdx; */
#endif				/* CFG_SUPPORT_TDLS */
	UINT_32 IsrCnt;
	UINT_32 IsrPassCnt;
	UINT_32 TaskIsrCnt;

	UINT_32 IsrPreCnt;
	UINT_32 IsrPrePassCnt;
	UINT_32 TaskPreIsrCnt;

	UINT_32 IsrAbnormalCnt;
	UINT_32 IsrSoftWareCnt;
	UINT_32 IsrTxCnt;
	UINT_32 IsrRxCnt;
	/* save partial scan channel information */
	PUINT_8	puScanChannel;
	UINT_64 u8SkbToDriver;
	UINT_64 u8SkbFreed;

	/*Full2Partial*/
	/*save update full scan to partial scan information*/
	/*last full scan time during AIS_STATE_NORMAL_TR state*/
	OS_SYSTIME u4LastFullScanTime;
	/*full scan or partial scan*/
	UINT_8     ucTrScanType;
	/*channel number, last full scan find AP*/
	/*UINT_8   ucChannelListNum;*/
	/*channel list, last full scan find AP*/
	UINT_8     ucChannelNum[FULL_SCAN_MAX_CHANNEL_NUM];
	/**/
	PUINT_8    puFullScan2PartialChannel;

	struct FT_IES rFtIeForTx;
	struct cfg80211_ft_event_params rFtEventParam;
	UINT_32 i4Priority;

	enum ENUM_BUILD_VARIANT_E rBuildVarint;

	/* FW Roaming */
	/* store the FW roaming enable state which FWK determines */
	/* if it's = 0, ignore the black/whitelists settings from FWK */
	UINT_32 u4FWRoamingEnable;

	BOOLEAN fgIsFwDlDone;
};

typedef irqreturn_t(*PFN_WLANISR) (int irq, void *dev_id, struct pt_regs *regs);

typedef void (*PFN_LINUX_TIMER_FUNC) (unsigned long);

/*
 * generic sub module init/exit handler
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
	/* old test mode command id, compatible with exist testmode command */
	TESTMODE_CMD_ID_SW_CMD = 1,
	TESTMODE_CMD_ID_WAPI = 2,
	TESTMODE_CMD_ID_HS20 = 3,
	TESTMODE_CMD_ID_POORLINK = 4,
	TESTMODE_CMD_ID_STATISTICS = 0x10,
	TESTMODE_CMD_ID_LINK_DETECT = 0x20,
	/* old test mode command id, compatible with exist testmode command */

	/* Hotspot managerment testmode command */
	TESTMODE_CMD_ID_HS_CONFIG = 51,

	/* all new added test mode command should great than TESTMODE_CMD_ID_NEW_BEGIN */
	TESTMODE_CMD_ID_NEW_BEGIN = 100,
	TESTMODE_CMD_ID_SUSPEND = 101,
	TESTMODE_CMD_ID_STR_CMD = 102,
	TESTMODE_CMD_ID_RXFILTER = 103,
};
#if CFG_SUPPORT_HOTSPOT_2_0
enum Hs20CmdType {
	HS20_CMD_ID_SET_BSSID_POOL = 0,
	NUM_OF_HS20_CMD_ID
};
#endif

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

typedef struct _NL80211_DRIVER_SUSPEND_PARAMS {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	UINT_8 suspend;
} NL80211_DRIVER_SUSPEND_PARAMS, *P_NL80211_DRIVER_SUSPEND_PARAMS;

typedef struct _NL80211_DRIVER_RXFILTER_PARAMS {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	UINT_32		Ipv4FilterHigh;
	UINT_32		Ipv4FilterLow;
	UINT_32		Ipv6FilterHigh;
	UINT_32		Ipv6FilterLow;
	UINT_32		SnapFilterHigh;
	UINT_32		SnapFilterLow;
} NL80211_DRIVER_RXFILTER_PARAMS, *P_NL80211_DRIVER_RXFILTER_PARAMS;

struct iw_encode_exts {
	__u32 ext_flags;	/*!< IW_ENCODE_EXT_* */
	__u8 tx_seq[IW_ENCODE_SEQ_MAX_SIZE];	/*!< LSB first */
	__u8 rx_seq[IW_ENCODE_SEQ_MAX_SIZE];	/*!< LSB first */
	__u8 addr[MAC_ADDR_LEN];	/*
					 * !< ff:ff:ff:ff:ff:ff for broadcast/multicast
					 *   (group) keys or unicast address for
					 *   individual keys
					 */
	__u16 alg;		/*!< IW_ENCODE_ALG_* */
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

#if CFG_SUPPORT_HOTSPOT_2_0

struct param_hs20_set_bssid_pool {
	u8 fgBssidPoolIsEnable;
	u8 ucNumBssidPool;
	u8 arBssidPool[8][ETH_ALEN];
};

struct wpa_driver_hs20_data_s {
	NL80211_DRIVER_TEST_MODE_PARAMS hdr;
	enum Hs20CmdType CmdType;
	struct param_hs20_set_bssid_pool hs20_set_bssid_pool;
};

#endif /* CFG_SUPPORT_HOTSPOT_2_0 */

#endif

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
#define GLUE_ACQUIRE_THE_SPIN_LOCK(prLock)                  \
	spin_lock_bh(prLock)
#define GLUE_RELEASE_THE_SPIN_LOCK(prLock)                  \
	spin_unlock_bh(prLock)

#else /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */
#define GLUE_SPIN_LOCK_DECLARATION()                        unsigned long __u4Flags = 0
#define GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	{ \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_lock_irqsave(&(prGlueInfo)->rSpinLock[rLockCategory], __u4Flags); \
	}
#define GLUE_RELEASE_SPIN_LOCK(prGlueInfo, rLockCategory)   \
	{ \
		if (rLockCategory < SPIN_LOCK_NUM) \
			spin_unlock_irqrestore(&(prGlueInfo->rSpinLock[rLockCategory]), __u4Flags); \
	}
#define GLUE_ACQUIRE_THE_SPIN_LOCK(prLock)                  \
	    spin_lock_irqsave(prLock, __u4Flags)
#define GLUE_RELEASE_THE_SPIN_LOCK(prLock)                  \
	    spin_unlock_irqrestore(prLock, __u4Flags)
#endif /* !CFG_USE_SPIN_LOCK_BOTTOM_HALF */

/*----------------------------------------------------------------------------*/
/* Macros for accessing Reserved Fields of native packet                      */
/*----------------------------------------------------------------------------*/
#define GLUE_CB_OFFSET					4	/*
								 * For 64-bit platform, avoiding that the cb
								 * isoverwrited by "(prQueueEntry)->prNext =
								 * (P_QUE_ENTRY_T)NULL;" in QUEUE_INSERT_TAIL
								 */
#define GLUE_GET_PKT_QUEUE_ENTRY(_p)    \
	(&(((struct sk_buff *)(_p))->cb[0]))

#define GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry)  \
	((P_NATIVE_PACKET) ((ULONG)_prQueueEntry - offsetof(struct sk_buff, cb[0])))

#define  GLUE_SET_PKT_FLAG_802_11(_p)  \
	(*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4])) |= BIT(7))

#define GLUE_SET_PKT_FLAG_1X(_p)  \
	(*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4])) |= BIT(6))

#define GLUE_SET_PKT_FLAG_PAL(_p)  \
	(*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4])) |= BIT(5))

#define GLUE_SET_PKT_FLAG_P2P(_p)  \
	(*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4])) |= BIT(4))

#define GLUE_SET_PKT_TID(_p, _tid)  \
	(*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4])) |= (((UINT_8)((_tid) & (BITS(0, 3))))))

#define GLUE_SET_PKT_FRAME_LEN(_p, _u2PayloadLen) \
	(*((PUINT_16)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+6])) = (UINT_16)(_u2PayloadLen))

#define GLUE_GET_PKT_FRAME_LEN(_p)    \
	(*((PUINT_16)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+6])))

#define  GLUE_GET_PKT_IS_802_11(_p)        \
	((*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4]))) & (BIT(7)))

#define  GLUE_GET_PKT_IS_1X(_p)        \
	((*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4]))) & (BIT(6)))

#define GLUE_GET_PKT_TID(_p)        \
	((*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4]))) & (BITS(0, 3)))

#define GLUE_GET_PKT_IS_PAL(_p)        \
	((*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4]))) & (BIT(5)))

#define GLUE_GET_PKT_IS_P2P(_p)        \
	((*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+4]))) & (BIT(4)))

#define GLUE_SET_PKT_HEADER_LEN(_p, _ucMacHeaderLen)    \
	(*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+5])) = (UINT_8)(_ucMacHeaderLen))

#define GLUE_GET_PKT_HEADER_LEN(_p) \
	(*((PUINT_8)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+5])))

#define GLUE_SET_PKT_ARRIVAL_TIME(_p, _rSysTime) \
	(*((POS_SYSTIME)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+8])) = (OS_SYSTIME)(_rSysTime))

#define GLUE_GET_PKT_ARRIVAL_TIME(_p)    \
	(*((POS_SYSTIME)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+8])))

#define GLUE_SET_PKT_XTIME(_p, _rSysTime) \
	(*((UINT_64 *)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+16])) = (UINT_64)(_rSysTime))

#define GLUE_GET_PKT_XTIME(_p)    \
	(*((UINT_64 *)&(((struct sk_buff *)(_p))->cb[GLUE_CB_OFFSET+16])))

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

#define GLUE_INC_REF_CNT(_refCount)     atomic_inc((atomic_t *)&(_refCount))
#define GLUE_DEC_REF_CNT(_refCount)     atomic_dec((atomic_t *)&(_refCount))

#define DbgPrint(...)
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

#if WLAN_INCLUDE_SYS
int32_t sysCreateFsEntry(P_GLUE_INFO_T prGlueInfo);
int32_t sysRemoveSysfs(void);
int32_t sysInitFs(void);
int32_t sysUninitSysFs(void);
int32_t sysMacAddrOverride(uint8_t *prMacAddr);
#endif /* WLAN_INCLUDE_SYS */

#if CFG_ENABLE_BT_OVER_WIFI
BOOLEAN glRegisterAmpc(P_GLUE_INFO_T prGlueInfo);

BOOLEAN glUnregisterAmpc(P_GLUE_INFO_T prGlueInfo);
#endif

#if CFG_ENABLE_WIFI_DIRECT

VOID wlanSubModRunInit(P_GLUE_INFO_T prGlueInfo);

VOID wlanSubModRunExit(P_GLUE_INFO_T prGlueInfo);

BOOLEAN wlanSubModInit(P_GLUE_INFO_T prGlueInfo);

BOOLEAN wlanSubModExit(P_GLUE_INFO_T prGlueInfo);

VOID
wlanSubModRegisterInitExit(SUB_MODULE_INIT rSubModInit, SUB_MODULE_EXIT rSubModExit, ENUM_SUB_MODULE_IDX_T eSubModIdx);

BOOLEAN wlanExportGlueInfo(P_GLUE_INFO_T *prGlueInfoExpAddr);

BOOLEAN wlanIsLaunched(VOID);

VOID wlanUpdateChannelTable(P_GLUE_INFO_T prGlueInfo);

#endif

#ifdef FW_CFG_SUPPORT
INT_32 cfgCreateProcEntry(P_GLUE_INFO_T prGlueInfo);
INT_32 cfgRemoveProcEntry(void);
#endif

typedef UINT_8 (*file_buf_handler) (PVOID ctx, const CHAR __user *buf, UINT_16 length);
extern VOID register_file_buf_handler(file_buf_handler handler, PVOID ctx, UINT_8 ucType);
extern const uint8_t *kalFindIeMatchMask(uint8_t eid,
				const uint8_t *ies, int len,
				const uint8_t *match,
				int match_len, int match_offset,
				const uint8_t *match_mask);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _GL_OS_H */
