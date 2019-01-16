/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/include/gl_kal.h#2 $
*/

/*! \file   gl_kal.h
    \brief  Declaration of KAL functions - kal*() which is provided by GLUE Layer.

    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/



/*
** $Log: gl_kal.h $
**
** 01 15 2014 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch][MT6630][Driver]MT6630 Wi-Fi Patch
** Merging
**
**	//ALPS_SW/DEV/ALPS.JB2.MT6630.DEV/alps/mediatek/kernel/drivers/combo/drv_wlan/mt6630/wlan/...
**
**	to //ALPS_SW/TRUNK/KK/alps/mediatek/kernel/drivers/combo/drv_wlan/mt6630/wlan/...
**
** 12 27 2013 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch][MT6630][Driver]MT6630 Wi-Fi Patch
** update code for ICAP & nvram
**
** 08 20 2013 eason.tsai
** [BORA00002255] [MT6630 Wi-Fi][Driver] develop
** ICAP part for win32
**
** 08 09 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** 1. integrate scheduled scan functionality
** 2. condition compilation for linux-3.4 & linux-3.8 compatibility
** 3. correct CMD queue access to reduce lock scope
**
** 07 29 2013 cp.wu
** [BORA00002725] [MT6630][Wi-Fi] Add MGMT TX/RX support for Linux port
** Preparation for porting remain_on_channel support
**
** 07 23 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. build success for win32 port
** 2. add SDIO test read/write pattern for HQA tests (default off)
**
** 03 12 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx utility function for management frame
**
** 01 22 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** modification for ucBssIndex migration
**
** 01 21 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update TX path based on new ucBssIndex modifications.
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
**
** 08 24 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** .
 *
 * 12 13 2011 cm.chang
 * [WCXRP00001136] [All Wi-Fi][Driver] Add wake lock for pending timer
 * Add dummy kal function for wake lock
 *
 * 06 07 2011 yuche.tsai
 * [WCXRP00000696] [Volunteer Patch][MT6620][Driver] Infinite loop issue when RX invitation response.[WCXRP00000763] [Volunteer Patch][MT6620][Driver] RX Service Discovery Frame under AP mode Issue
 * Add invitation support.
 *
 * 04 22 2011 cp.wu
 * [WCXRP00000598] [MT6620 Wi-Fi][Driver] Implementation of interface for communicating with user space process for RESET_START and RESET_END events
 * skip power-off handshaking when RESET indication is received.
 *
 * 04 18 2011 terry.wu
 * [WCXRP00000660] [MT6620 Wi-Fi][Driver] Remove flag CFG_WIFI_DIRECT_MOVED
 * Remove flag CFG_WIFI_DIRECT_MOVED.
 *
 * 04 14 2011 cp.wu
 * [WCXRP00000598] [MT6620 Wi-Fi][Driver] Implementation of interface for communicating with user space process for RESET_START and RESET_END events
 * 1. add code to put whole-chip resetting trigger when abnormal firmware assertion is detected
 * 2. add dummy function for both Win32 and Linux part.
 *
 * 04 12 2011 cp.wu
 * [WCXRP00000635] [MT6620 Wi-Fi][Driver] Clear pending security frames when QM clear pending data frames for dedicated network type
 * clear pending security frames for dedicated network type when BSS is being deactivated/disconnected
 *
 * 03 16 2011 cp.wu
 * [WCXRP00000562] [MT6620 Wi-Fi][Driver] I/O buffer pre-allocation to avoid physically continuous memory shortage after system running for a long period
 * 1. pre-allocate physical continuous buffer while module is being loaded
 * 2. use pre-allocated physical continuous buffer for TX/RX DMA transfer
 *
 * The windows part remained the same as before, but added similiar APIs to hide the difference.
 *
 * 03 02 2011 cp.wu
 * [WCXRP00000503] [MT6620 Wi-Fi][Driver] Take RCPI brought by association response as initial RSSI right after connection is built.
 * use RCPI brought by ASSOC-RESP after connection is built as initial RCPI to avoid using a uninitialized MAC-RX RCPI.
 *
 * 02 24 2011 cp.wu
 * [WCXRP00000490] [MT6620 Wi-Fi][Driver][Win32] modify kalMsleep() implementation because NdisMSleep() won't sleep long enough for specified interval such as 500ms
 * modify cnm_timer and hem_mbox APIs to be thread safe to ease invoking restrictions
 *
 * 02 23 2011 cp.wu
 * [WCXRP00000490] [MT6620 Wi-Fi][Driver][Win32] modify kalMsleep() implementation because NdisMSleep() won't sleep long enough for specified interval such as 500ms
 * add design to avoid system ticks wraps around
 *
 * 01 25 2011 eddie.chen
 * [WCXRP00000385] [MT6620 Wi-Fi][DRV] Add destination decision for forwarding packets
 * Fix the compile error in windows.
 *
 * 01 04 2011 cp.wu
 * [WCXRP00000338] [MT6620 Wi-Fi][Driver] Separate kalMemAlloc into kmalloc and vmalloc implementations to ease physically continous memory demands
 * separate kalMemAlloc() into virtually-continous and physically-continous type to ease slab system pressure
 *
 * 12 31 2010 cp.wu
 * [WCXRP00000335] [MT6620 Wi-Fi][Driver] change to use milliseconds sleep instead of delay to avoid blocking to system scheduling
 * change to use msleep() and shorten waiting interval to reduce blocking to other task while Wi-Fi driver is being loaded
 *
 * 11 30 2010 yuche.tsai
 * NULL
 * Invitation & Provision Discovery Indication.
 *
 * 11 26 2010 cp.wu
 * [WCXRP00000209] [MT6620 Wi-Fi][Driver] Modify NVRAM checking mechanism to warning only with necessary data field checking
 * 1. NVRAM error is now treated as warning only, thus normal operation is still available but extra scan result used to indicate user is attached
 * 2. DPD and TX-PWR are needed fields from now on, if these 2 fields are not availble then warning message is shown
 *
 * 11 08 2010 cp.wu
 * [WCXRP00000166] [MT6620 Wi-Fi][Driver] use SDIO CMD52 for enabling/disabling interrupt to reduce transaction period
 * change to use CMD52 for enabling/disabling interrupt to reduce SDIO transaction time
 *
 * 11 01 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000150] [MT6620 Wi-Fi][Driver] Add implementation for querying current TX rate from firmware auto rate module
 * 1) Query link speed (TX rate) from firmware directly with buffering mechanism to reduce overhead
 * 2) Remove CNM CH-RECOVER event handling
 * 3) cfg read/write API renamed with kal prefix for unified naming rules.
 *
 * 10 18 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000086] [MT6620 Wi-Fi][Driver] The mac address is all zero at android
 * complete implementation of Android NVRAM access
 *
 * 10 05 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check
 * 1) add NVRAM access API
 * 2) fake scanning result when NVRAM doesn't exist and/or version mismatch. (off by compiler option)
 * 3) add OID implementation for NVRAM read/write service
 *
 * 10 04 2010 wh.su
 * [WCXRP00000081] [MT6620][Driver] Fix the compiling error at WinXP while enable P2P
 * add a kal function for set cipher.
 *
 * 10 04 2010 wh.su
 * [WCXRP00000081] [MT6620][Driver] Fix the compiling error at WinXP while enable P2P
 * fixed compiling error while enable p2p.
 *
 * 10 04 2010 cp.wu
 * [WCXRP00000077] [MT6620 Wi-Fi][Driver][FW] Eliminate use of ENUM_NETWORK_TYPE_T and replaced by ENUM_NETWORK_TYPE_INDEX_T only
 * remove ENUM_NETWORK_TYPE_T definitions
 *
 * 09 28 2010 wh.su
 * NULL
 * [WCXRP00000069][MT6620 Wi-Fi][Driver] Fix some code for phase 1 P2P Demo.
 *
 * 09 21 2010 cp.wu
 * [WCXRP00000053] [MT6620 Wi-Fi][Driver] Reset incomplete and might leads to BSOD when entering RF test with AIS associated
 * Do a complete reset with STA-REC null checking for RF test re-entry
 *
 * 09 10 2010 wh.su
 * NULL
 * fixed the compiling error at win XP.
 *
 * 09 07 2010 wh.su
 * NULL
 * adding the code for beacon/probe req/ probe rsp wsc ie at p2p.
 *
 * 09 06 2010 wh.su
 * NULL
 * let the p2p can set the privacy bit at beacon and rsn ie at assoc req at key handshake state.
 *
 * 09 03 2010 kevin.huang
 * NULL
 * Refine #include sequence and solve recursive/nested #include issue
 *
 * 08 06 2010 cp.wu
 * NULL
 * driver hook modifications corresponding to ioctl interface change.
 *
 * 08 03 2010 cp.wu
 * NULL
 * [Wi-Fi Direct Driver Hook] change event indication API to be consistent with supplicant
 *
 * 08 03 2010 cp.wu
 * NULL
 * [Wi-Fi Direct] add framework for driver hooks
 *
 * 07 29 2010 cp.wu
 * NULL
 * simplify post-handling after TX_DONE interrupt is handled.
 *
 * 07 23 2010 cp.wu
 *
 * 1) re-enable AIS-FSM beacon timeout handling.
 * 2) scan done API revised
 *
 * 07 23 2010 cp.wu
 *
 * indicate scan done for linux wireless extension
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 24 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 802.1x and bluetooth-over-Wi-Fi security frames are now delievered to firmware via command path instead of data path.
 *
 * 06 21 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * change MAC address updating logic.
 *
 * 06 18 2010 cm.chang
 * [WPD00003841][LITE Driver] Migrate RLM/CNM to host driver
 * Provide cnmMgtPktAlloc() and alloc/free function of msg/buf
 *
 * 06 11 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * auth.c is migrated.
 *
 * 06 11 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) migrate assoc.c.
 * 2) add ucTxSeqNum for tracking frames which needs TX-DONE awareness
 * 3) add configuration options for CNM_MEM and RSN modules
 * 4) add data path for management frames
 * 5) eliminate rPacketInfo of MSDU_INFO_T
 *
 * 06 08 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * cnm_timer has been migrated.
 *
 * 06 07 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * gl_kal merged
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 06 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * move timer callback to glue layer.
 *
 * 05 17 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * add basic handling framework for wireless extension ioctls.
 *
 * 05 10 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * fill network type field while doing frame identification.
 *
 * 05 10 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * implement basic wi-fi direct framework
 *
 * 04 28 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * change prefix for data structure used to communicate with 802.11 PAL
 * to avoid ambiguous naming with firmware interface
 *
 * 04 27 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add multiple physical link support
 *
 * 04 20 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * don't need SPIN_LOCK_PWR_CTRL anymore, it will raise IRQL
 *  *  * and cause SdBusSubmitRequest running at DISPATCH_LEVEL as well.
 *
 * 04 14 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * information buffer for query oid/ioctl is now buffered in prCmdInfo
 *  *  *  *  *  *  *  *  *  * instead of glue-layer variable to improve multiple oid/ioctl capability
 *
 * 04 13 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add framework for BT-over-Wi-Fi support.
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 1) prPendingCmdInfo is replaced by queue for multiple handler capability
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 2) command sequence number is now increased atomically
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 3) private data could be hold and taken use for other purpose
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * finish non-glue layer access to glue variables
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * accessing to firmware load/start address, and access to OID handling information
 *  *  *  *  *  * are now handled in glue layer
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * rWlanInfo should be placed at adapter rather than glue due to most operations
 *  *  *  *  *  *  *  *  *  *  *  *  *  * are done in adapter layer.
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * eliminate direct access to prGlueInfo->eParamMediaStateIndicated from non-glue layer
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add KAL API: kalFlushPendingTxPackets(), and take use of the API
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * eliminate direct access to prGlueInfo->rWlanInfo.eLinkAttr.ucMediaStreamMode from non-glue layer.
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code refine: fgTestMode should be at adapter rather than glue due to the device/fw is also involved
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) for some OID, never do timeout expiration
 *  *  *  *  *  *  * 2) add 2 kal API for later integration
 *
 * 03 19 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add ACPI D0/D3 state switching support
 *  *  *  *  *  *  *  *  *  *  *  * 2) use more formal way to handle interrupt when the status is retrieved from enhanced RX response
 *
 * 03 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add another spin-lock to protect MsduInfoList due to it might be accessed by different thread.
 *  *  *  *  *  *  * 2) change own-back acquiring procedure to wait for up to 16.67 seconds
 *
 * 03 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add high resolution kalGetNanoTick (100ns) for profiling purpose
 *
 * 03 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * when starting adapter, read local adminsitrated address from registry and send to firmware via CMD_BASIC_CONFIG.
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) the use of prPendingOid revised, all accessing are now protected by spin lock
 *  *  *  *  *  *  *  * 2) ensure wlanReleasePendingOid will clear all command queues
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add mutex to avoid multiple access to qmTxQueue simultaneously.
 *
 * 02 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * move EVENT_ID_ASSOC_INFO from nic_rx.c to gl_kal_ndis_51.c
 *  *  *  * 'cause it involves OS dependent data structure handling
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) implement timeout mechanism when OID is pending for longer than 1 second
 *  *  *  *  *  *  * 2) allow OID_802_11_CONFIGURATION to be executed when RF test mode is turned on
**  \main\maintrunk.MT6620WiFiDriver_Prj\19 2009-12-10 16:44:41 GMT mtk02752
**  remove unused API
**  \main\maintrunk.MT6620WiFiDriver_Prj\18 2009-11-16 21:41:03 GMT mtk02752
**  wlanQoSFrameClassifierAndPacketInfo
**
**  rename wlanQoSFrameClassifierAndPacketInfo() to kalQoSFrameClassifierAndPacketInfo() and make it non-inline external function, due to AP mode RX-to-TX path will take use of it
**
**  \main\maintrunk.MT6620WiFiDriver_Prj\17 2009-10-05 11:29:00 GMT mtk01084
**  change CFG_SDIO_TX_ENHANCE to CFG_SDIO_TX_AGG
**  \main\maintrunk.MT6620WiFiDriver_Prj\16 2009-10-02 14:04:45 GMT mtk01725
**  \main\maintrunk.MT6620WiFiDriver_Prj\15 2009-09-09 17:26:28 GMT mtk01084
**  add DDK related macro
**  \main\maintrunk.MT6620WiFiDriver_Prj\14 2009-04-29 15:42:57 GMT mtk01461
**  Update prototype of kalQueryTxOOBData()
**  \main\maintrunk.MT6620WiFiDriver_Prj\13 2009-04-27 12:17:45 GMT mtk01104
**  Add spin-lock protection for SPI bus access
**  \main\maintrunk.MT6620WiFiDriver_Prj\12 2009-04-21 09:44:28 GMT mtk01461
**  Add KAL_SET_EVENT macro and kalOidComplete()
**  \main\maintrunk.MT6620WiFiDriver_Prj\11 2009-04-14 15:52:06 GMT mtk01426
**  Update kalSetRxOOBData() proto type
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-04-01 11:00:04 GMT mtk01461
**  Add kalSetEvent()
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-03-23 22:02:02 GMT mtk01461
**  Modify KAL macro for access NDIS_PACKET's reserved field and add declaration of kalQueryTxOOBData()
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-03-23 00:34:28 GMT mtk01461
**  Add SPIN_LOCK_PWR_CTRL
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-03-18 21:12:30 GMT mtk01426
**  Add kalSetRxOOBData() proto type
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-03-18 20:26:40 GMT mtk01461
**  Fix LINT warning and rearrange the order of function prototype declaration
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-03-17 09:52:14 GMT mtk01426
**  Add kalTxServiceThread function proto type
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-16 16:42:55 GMT mtk01461
**  Modify the spin_lock macro and remove kalAcquireSpinLock(), kalReleaseSpinLock()
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-16 09:12:50 GMT mtk01461
**  Update TX PATH API
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:40:43 GMT mtk01426
**  Init for develop
**
*/

#ifndef _GL_KAL_H
#define _GL_KAL_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "config.h"
#include "gl_typedef.h"
#include "gl_os.h"
#include "nic/mac.h"
#include "nic/wlan_def.h"
#include "wlan_lib.h"
#include "wlan_oid.h"
#include "queue.h"


#if CFG_ENABLE_BT_OVER_WIFI
#include "nic/bow.h"
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define USEC_PER_MSEC   (1000)

#define KAL_BSS_NUM     2
#define KAL_P2P_NUM     1

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_SPIN_LOCK_CATEGORY_E {
	SPIN_LOCK_RX_QUE = 0,
	SPIN_LOCK_RX_RETURN_QUE,
	SPIN_LOCK_TX_QUE,
	SPIN_LOCK_TX_RESOURCE,
	SPIN_LOCK_TX_MSDU_INFO_LIST,
	SPIN_LOCK_TXING_MGMT_LIST,
	SPIN_LOCK_TX_SEQ_NUM,
	SPIN_LOCK_QM_TX_QUEUE,
	SPIN_LOCK_CMD_QUE,
	SPIN_LOCK_CMD_RESOURCE,
	SPIN_LOCK_CMD_PENDING,
	SPIN_LOCK_CMD_SEQ_NUM,
#if defined(_HIF_SPI)
	SPIN_LOCK_SPI_ACCESS,
#endif
	SPIN_LOCK_MGT_BUF,
	SPIN_LOCK_MSG_BUF,
	SPIN_LOCK_STA_REC,

	SPIN_LOCK_MAILBOX,
	SPIN_LOCK_TIMER,

	SPIN_LOCK_NUM
} ENUM_SPIN_LOCK_CATEGORY_E;

/* event for assoc infomation update */
typedef struct _EVENT_ASSOC_INFO {
	UINT_8 ucAssocReq;	/* 1 for assoc req, 0 for assoc rsp */
	UINT_8 ucReassoc;	/* 0 for assoc, 1 for reassoc */
	UINT_16 u2Length;
	PUINT_8 pucIe;
} EVENT_ASSOC_INFO, *P_EVENT_ASSOC_INFO;

typedef enum _ENUM_KAL_NETWORK_TYPE_INDEX_T {
	KAL_NETWORK_TYPE_AIS_INDEX = 0,
#if CFG_ENABLE_WIFI_DIRECT
	KAL_NETWORK_TYPE_P2P_INDEX,
#endif
#if CFG_ENABLE_BT_OVER_WIFI
	KAL_NETWORK_TYPE_BOW_INDEX,
#endif
	KAL_NETWORK_TYPE_INDEX_NUM
} ENUM_KAL_NETWORK_TYPE_INDEX_T;

typedef enum _ENUM_KAL_MEM_ALLOCATION_TYPE_E {
	PHY_MEM_TYPE,		/* physically continuous */
	VIR_MEM_TYPE,		/* virtually continous */
	MEM_TYPE_NUM
} ENUM_KAL_MEM_ALLOCATION_TYPE;

/* Not use wake lock in Windows system */
typedef UINT_32 KAL_WAKE_LOCK_T, *P_KAL_WAKE_LOCK_T;

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
/* Macros of Waiting/Send EVENT                                               */
/*----------------------------------------------------------------------------*/
#if defined(WINDOWS_DDK)
#define KAL_CREATE_THREAD(pvThreadHandle, pfnThreadProc, pvArg, ppvKThread) \
    DBGLOG(INIT, TRACE, ("KAL_CREATE_THREAD\n")); \
    PsCreateSystemThread(&pvThreadHandle, \
			 (ACCESS_MASK)0, \
			 NULL, \
			 (HANDLE) 0, \
			 NULL, \
			 pfnThreadProc, \
			 pvArg); \
    ObReferenceObjectByHandle(pvThreadHandle, THREAD_ALL_ACCESS, NULL, KernelMode, \
			    (PVOID *) ppvKThread, NULL);

#define KAL_KILL_THREAD(pvEvent, pKThread) \
	NdisSetEvent(pvEvent); \
        DBGLOG(INIT, TRACE, ("Notify TxServiceThread to terminate it\n")); \
	if (pKThread) { \
	    KeWaitForSingleObject(pKThread, \
				Executive, \
				KernelMode, \
				FALSE, \
				NULL); \
	    ObDereferenceObject(pKThread); \
	    /*prGlueInfo->hTxService = NULL;*/\
	}



#define KAL_WAIT_FOR_SINGLE_OBJECT(pvThreadHandle) \
    KeWaitForSingleObject(pvThreadHandle, \
			Executive, \
			KernelMode, \
			FALSE, \
			NULL);

#define KAL_CLOSE_HANDLE(pvThreadHandle) \
    ObDereferenceObject(pvThreadHandle);

#endif

#if defined(WINDOWS_CE)
#define KAL_CREATE_THREAD(pvThreadHandle, pfnThreadProc, pvArg) \
				/* CreateThread (NULL, 0, kalTxServiceThread, prGlueInfo, 0, NULL) */
CreateThread(NULL, 0, pfnThreadProc, pvArg, 0, NULL)
#define KAL_WAIT_FOR_SINGLE_OBJECT(pvThreadHandle) \
    WaitForSingleObject(pvThreadHandle, INFINITE);
#define KAL_CLOSE_HANDLE(pvThreadHandle) \
    CloseHandle(pvThreadHandle);
#endif
/*----------------------------------------------------------------------------*/
/* Macros of SPIN LOCK operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_SPIN_LOCK_DECLARATION() \
	    GLUE_SPIN_LOCK_DECLARATION()
#define KAL_ACQUIRE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	    GLUE_ACQUIRE_SPIN_LOCK((_prAdapter)->prGlueInfo, _rLockCategory)
#define KAL_RELEASE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	    GLUE_RELEASE_SPIN_LOCK((_prAdapter)->prGlueInfo, _rLockCategory)
/*----------------------------------------------------------------------------*/
/* Macros of Waiting/Send EVENT                                               */
/*----------------------------------------------------------------------------*/
#define KAL_WAIT_EVENT(_prAdapter)          GLUE_WAIT_EVENT((_prAdapter)->prGlueInfo)
#define KAL_RESET_EVENT(_prAdapter)         GLUE_RESET_EVENT((_prAdapter)->prGlueInfo)
#define KAL_SET_EVENT(_prAdapter)           GLUE_SET_EVENT((_prAdapter)->prGlueInfo)
/*----------------------------------------------------------------------------*/
/* Macros for accessing Reserved Fields of native packet                      */
/*----------------------------------------------------------------------------*/
#define KAL_GET_PKT_QUEUE_ENTRY(_p)             GLUE_GET_PKT_QUEUE_ENTRY(_p)
#define KAL_GET_PKT_DESCRIPTOR(_prQueueEntry)   GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry)
#define KAL_GET_PKT_TID(_p)                     GLUE_GET_PKT_TID(_p)
#define KAL_GET_PKT_IS_802_11(_p)               GLUE_GET_PKT_IS_802_11(_p)
#define KAL_GET_PKT_IS_1X(_p)                   GLUE_GET_PKT_IS_1X(_p)
#define KAL_GET_PKT_IS_PAL(_p)                  GLUE_GET_PKT_IS_PAL(_p)
#define KAL_GET_PKT_HEADER_LEN(_p)              GLUE_GET_PKT_HEADER_LEN(_p)
#define KAL_GET_PKT_FRAME_LEN(_p)               GLUE_GET_PKT_FRAME_LEN(_p)
#define KAL_GET_PKT_ARRIVAL_TIME(_p)            GLUE_GET_PKT_ARRIVAL_TIME(_p)
/*----------------------------------------------------------------------------*/
/* Macros of wake_lock operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName)
#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout)
#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock)
/* Remove Warning 516: Symbol 'NdisMoveMemory()' has arg. type conflict */
#ifdef _lint
VOID NdisZeroMemory(IN PVOID Destination, IN ULONG Length);

VOID NdisMoveMemory(OUT PVOID Destination, IN PVOID Source, IN ULONG Length);

VOID NdisFillMemory(IN PVOID Destination, IN ULONG Length, IN UCHAR Fill);

ULONG NdisEqualMemory(PVOID Source1, PVOID Source2, ULONG Length);
#endif				/* _lint */


/*! Copy memory block with specific size */
#define kalMemCopy(pvDst, pvSrc, u4Size)            NdisMoveMemory(pvDst, pvSrc, u4Size)

/*! Set memory block with specific pattern */
#define kalMemSet(pvAddr, ucPattern, u4Size)        NdisFillMemory(pvAddr, u4Size, ucPattern)

/*! Compare two memory block with specific length.
 * Return zero if they are the same.
 */
#define kalMemCmp(pvAddr1, pvAddr2, u4Size)         !NdisEqualMemory(pvAddr1, pvAddr2, u4Size)

/*! Zero specific memory block */
#define kalMemZero(pvAddr, u4Size)                  NdisZeroMemory(pvAddr, u4Size)


#if CFG_SDIO_TX_AGG
#define kalGetSDIOWriteBlkSize(prGlueInfo)          prGlueInfo->rHifInfo.sdHostBlockCap.WriteBlockSize
#define kalGetSDIOWriteBlkBitSize(prGlueInfo)       prGlueInfo->rHifInfo.WBlkBitSize
#else
#define kalGetSDIOWriteBlkSize(prGlueInfo)
#define kalGetSDIOWriteBlkBitSize(prGlueInfo)
#endif

#if defined(_HIF_SDIO) && defined(WINDOWS_CE)
/*! defined for wince sdio driver only */
#define kalDevSetPowerState(prGlueInfo, ePowerMode) sdioSetPowerState(prGlueInfo, ePowerMode)
#else
#define kalDevSetPowerState(prGlueInfo, ePowerMode)
#endif



/*! Definitions for all of the Debug macros.  If we're in a debug (DBG) mode,
 * these macros will print information to the debug terminal.  If the
 * driver is compiled in a free (non-debug) environment the macros become
 *  NOPs.
 */
#define OS_DEBUG_MSG                0
#define FILE_DEBUG_MSG              1
#define MSG_DEBUG_MSG               2	/* Not verifed yet. */

/*! Set log method to one of above method. */
#define LOG_METHOD                  OS_DEBUG_MSG

#ifdef WINDOWS_CE
#define UNICODE_MESSAGE         1
#else
#define UNICODE_MESSAGE         0
#endif

#if !DBG
static inline void CREATE_LOG_FILE(void)
{
};

static inline void kalPrint(PUINT_8 dbgstr, ...)
{
};

#elif (LOG_METHOD == FILE_DEBUG_MSG) || (LOG_METHOD == MSG_DEBUG_MSG)
#define CREATE_LOG_FILE()       dbgFileCreate()
#define kalPrint                dbgLogWr
#else				/* FILE_DEBUG_MSG */
#if UNICODE_MESSAGE
#define CREATE_LOG_FILE()
#define kalPrint                dbgLogWr2
#else				/* UNICODE_MESSAGE */
#define CREATE_LOG_FILE()
#define kalPrint                DbgPrint
#endif				/* !UNICODE_MESSAGE */
#endif				/* LOG_METHOD */

#define kalBreakPoint()             DbgBreakPoint()

/*! In WinCE/Windoes system, this macro just prints debug message. */
#define SPRINTF(buf, arg)           kalPrint arg

#define USEC_TO_SYSTIME(_usec)      ((_usec) / USEC_PER_MSEC)
#define MSEC_TO_SYSTIME(_msec)      (_msec)

#define SYSTIME_TO_MSEC(_systime)   (_systime)

#define kalIndicateBssInfo(prGlueInfo, pucFrameBuf, u4BufLen, ucChannelNum, i4SignalStrength)


/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/* Routines in gl_kal_ndis51.c                                                */
/*----------------------------------------------------------------------------*/
/*! Cache memory allocation function. */
PVOID kalMemAlloc(IN UINT_32 u4Size, IN ENUM_KAL_MEM_ALLOCATION_TYPE eMemType);

/*! Get 32-bit system time tick to record event time */
OS_SYSTIME kalGetTimeTick(VOID);

/*! Get 64-bit system tick in unit of 100ns */
UINT_64 kalGetNanoTick(VOID);

VOID kalCopyFrame(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket, IN PUINT_8 pucDestBuffer);

PVOID kalPacketAlloc(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Size, OUT PUINT_8 *ppucData);

VOID kalPacketFree(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket);

WLAN_STATUS
kalProcessRxPacket(IN P_GLUE_INFO_T prGlueInfo,
		   IN PVOID pvPacket,
		   IN PUINT_8 pucPacketStart,
		   IN UINT_32 u4PacketLen, IN BOOL fgIsRetain, IN ENUM_CSUM_RESULT_T aeCSUM[]
    );

WLAN_STATUS kalRxIndicatePkts(IN P_GLUE_INFO_T prGlueInfo, IN PVOID apvPkts[], IN UINT_32 ucPktNum);

VOID
kalIndicateStatusAndComplete(IN P_GLUE_INFO_T prGlueInfo,
			     IN WLAN_STATUS eStatus, IN PVOID pvBuf, IN UINT_32 u4BufLen);

VOID
kalUpdateReAssocReqInfo(IN P_GLUE_INFO_T prGlueInfo,
			IN PUINT_8 pucFrameBody,
			IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest);

VOID
kalUpdateReAssocRspInfo(IN P_GLUE_INFO_T prGlueInfo,
			IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
VOID kalQueryTxChksumOffloadParam(IN PVOID pvPacket, OUT PUINT_8 pucFlag);


VOID kalUpdateRxCSUMOffloadParam(IN PVOID pvPacket, IN ENUM_CSUM_RESULT_T aeCSUM[]
    );
#endif				/* CFG_TCP_IP_CHKSUM_OFFLOAD */


VOID kalSendComplete(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket, IN WLAN_STATUS rStatus);

VOID kalHandleAssocInfo(IN P_GLUE_INFO_T prGlueInfo, IN P_EVENT_ASSOC_INFO prAssocInfo);


/*----------------------------------------------------------------------------*/
/* Routines in gl_kal_common.c                                                */
/*----------------------------------------------------------------------------*/
/*! Cache memory free. */
VOID kalMemFree(IN PVOID pvAddr, IN ENUM_KAL_MEM_ALLOCATION_TYPE eMemType, IN UINT_32 u4Size);

/*! Process delay time in unit of micro-second */
VOID kalUdelay(IN UINT_32 u4MicroSec);

/*! Process delay time in unit of milli-second */
VOID kalMdelay(IN UINT_32 u4MilliSec);

VOID kalMsleep(IN UINT_32 u4MilliSec);

VOID kalSetEvent(IN P_GLUE_INFO_T prGlueInfo);

VOID kalInterruptDone(IN P_GLUE_INFO_T prGlueInfo);

WLAN_STATUS kalTxServiceThread(IN PVOID pvGlueContext);

#if 0
WLAN_STATUS kalDownloadPatch(IN PNDIS_STRING FileName);
#endif

VOID
kalOidComplete(IN P_GLUE_INFO_T prGlueInfo,
	       IN BOOLEAN fgSetQuery, IN UINT_32 u4SetQueryInfoLen, IN WLAN_STATUS rOidStatus);

BOOLEAN
kalGetEthDestAddr(IN P_GLUE_INFO_T prGlueInfo,
		  IN P_NATIVE_PACKET prPacket, OUT PUINT_8 pucEthDestAddr);

BOOL
kalQoSFrameClassifierAndPacketInfo(IN P_GLUE_INFO_T prGlueInfo,
				   IN P_NATIVE_PACKET prPacket,
				   OUT PUINT_8 pucPriorityParam,
				   OUT PUINT_32 pu4PacketLen,
				   OUT PUINT_8 pucEthDestAddr,
				   OUT PBOOLEAN pfgIs1X,
				   OUT PBOOLEAN pfgIsPAL,
				   OUT PBOOLEAN pfgIs802_3, OUT PBOOLEAN pfgIsVlanExists);


VOID kalUpdateMACAddress(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucMacAddr);


UINT_32 kalWriteToFile(const PUINT_8 pucPath, BOOLEAN fgDoAppend, PUINT_8 pucData, UINT_32 u4Size);

UINT_32
kal_sprintf_ddk(const PUINT_8 pucPath,
		UINT_32 u4size, const char *fmt1, const char *fmt2, const char *fmt3);

UINT_32 kal_sprintf_done_ddk(const PUINT_8 pucPath, UINT_32 u4size);


#ifdef WINDOWS_CE
/*----------------------------------------------------------------------------*/
/* Routines in dbgce.c                                                        */
/*----------------------------------------------------------------------------*/
#if DBG
#if (LOG_METHOD == FILE_DEBUG_MSG) || (LOG_METHOD == MSG_DEBUG_MSG)

NDIS_STATUS dbgFileCreate(VOID);

NDIS_STATUS dbgLogWr(IN PINT_8 debugStr, IN ...
    );

#elif UNICODE_MESSAGE

NDIS_STATUS dbgLogWr2(IN PINT_8 debugStr, IN ...
    );

#endif
#endif				/* DBG */
#endif

/*----------------------------------------------------------------------------*/
/* Routines in HIF/interface.c (e.g. sdio.c)                                  */
/*----------------------------------------------------------------------------*/
BOOL kalDevRegRead(P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, OUT PUINT_32 pu4Value);

BOOL kalDevRegWrite(P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, IN UINT_32 u4Value);

BOOL
kalDevPortRead(IN P_GLUE_INFO_T prGlueInfo,
	       IN UINT_32 u4Port,
	       IN UINT_32 u4Len, OUT PUINT_8 pucBuf, IN UINT_32 u4ValidOutBufSize);

BOOL
kalDevPortWrite(IN P_GLUE_INFO_T prGlueInfo,
		IN UINT_32 u4Port,
		IN UINT_32 u4Len, IN PUINT_8 pucBuf, IN UINT_32 u4ValidInBufSize);

BOOL kalDevWriteWithSdioCmd52(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Addr, IN UINT_8 ucData);

BOOL
kalDevReadAfterWriteWithSdioCmd52(IN P_GLUE_INFO_T prGlueInfo,
				  IN UINT_32 u4Addr,
				  IN OUT PUINT_8 pucRwBuffer, IN UINT_32 u4RwBufLen);

/*----------------------------------------------------------------------------*/
/* Timer                                                                      */
/*----------------------------------------------------------------------------*/
VOID
kalTimerEvent(IN PVOID systemSpecific1,
	      IN PVOID miniportAdapterContext, IN PVOID systemSpecific2, IN PVOID systemSpecific3);

VOID kalOsTimerInitialize(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prTimerHandler);

BOOLEAN kalSetTimer(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Interval);

BOOLEAN kalCancelTimer(IN P_GLUE_INFO_T prGlueInfo);

VOID kalTimeoutHandler(IN P_GLUE_INFO_T prGlueInfo);


/*----------------------------------------------------------------------------*/
/* Firmware Image Loading & Mapping                                           */
/*----------------------------------------------------------------------------*/
PVOID
kalFirmwareImageMapping(IN P_GLUE_INFO_T prGlueInfo,
			OUT PPVOID ppvMapFileBuf, OUT PUINT_32 pu4FileLength);

VOID
kalFirmwareImageUnmapping(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prFwHandle, IN PVOID pvMapFileBuf);


/*----------------------------------------------------------------------------*/
/* Card Removal Check                                                         */
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsCardRemoved(IN P_GLUE_INFO_T prGlueInfo);


/*----------------------------------------------------------------------------*/
/* TX                                                                         */
/*----------------------------------------------------------------------------*/
VOID kalFlushPendingTxPackets(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetTxPendingFrameCount(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetTxPendingCmdCount(IN P_GLUE_INFO_T prGlueInfo);


/*----------------------------------------------------------------------------*/
/* Media State Indication                                                     */
/*----------------------------------------------------------------------------*/
ENUM_PARAM_MEDIA_STATE_T kalGetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo);


VOID
kalSetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo,
			  IN ENUM_PARAM_MEDIA_STATE_T eParamMediaStateIndicate);


/*----------------------------------------------------------------------------*/
/* Firmware Load/Start Address                                                */
/*----------------------------------------------------------------------------*/
UINT_32 kalGetFwLoadAddress(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetFwStartAddress(IN P_GLUE_INFO_T prGlueInfo);


/*----------------------------------------------------------------------------*/
/* OID handling                                                               */
/*----------------------------------------------------------------------------*/
VOID kalOidCmdClearance(IN P_GLUE_INFO_T prGlueInfo);

VOID kalOidClearance(IN P_GLUE_INFO_T prGlueInfo);

VOID kalEnqueueCommand(IN P_GLUE_INFO_T prGlueInfo, IN P_QUE_ENTRY_T prQueueEntry);

/*----------------------------------------------------------------------------*/
/* Security Frame Clearance                                                   */
/*----------------------------------------------------------------------------*/
VOID kalClearSecurityFrames(IN P_GLUE_INFO_T prGlueInfo);

VOID kalClearSecurityFramesByBssIdx(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex);

VOID
kalSecurityFrameSendComplete(IN P_GLUE_INFO_T prGlueInfo,
			     IN PVOID pvPacket, IN WLAN_STATUS rStatus);


/*----------------------------------------------------------------------------*/
/* Management Frame Clearance                                                 */
/*----------------------------------------------------------------------------*/
VOID kalClearMgmtFrames(IN P_GLUE_INFO_T prGlueInfo);


/*----------------------------------------------------------------------------*/
/* Random number Service                                                      */
/*----------------------------------------------------------------------------*/
UINT_32 kalRandomNumber(VOID);


/*----------------------------------------------------------------------------*/
/* Scan Done Indication                                                       */
/*----------------------------------------------------------------------------*/
VOID
kalScanDone(IN P_GLUE_INFO_T prGlueInfo,
	    IN ENUM_KAL_NETWORK_TYPE_INDEX_T eNetTypeIdx, IN WLAN_STATUS rStatus);

#if CFG_ENABLE_BT_OVER_WIFI
/*----------------------------------------------------------------------------*/
/* Bluetooth over Wi-Fi handling                                              */
/*----------------------------------------------------------------------------*/
VOID kalIndicateBOWEvent(IN P_GLUE_INFO_T prGlueInfo, IN P_AMPC_EVENT prEvent);

ENUM_BOW_DEVICE_STATE kalGetBowState(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr);

VOID
kalSetBowState(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_BOW_DEVICE_STATE eBowState, IN PARAM_MAC_ADDRESS rPeerAddr);

ENUM_BOW_DEVICE_STATE kalGetBowGlobalState(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetBowFreqInKHz(IN P_GLUE_INFO_T prGlueInfo);

UINT_8 kalGetBowRole(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr);

VOID kalSetBowRole(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRole, IN PARAM_MAC_ADDRESS rPeerAddr);

UINT_32 kalGetBowAvailablePhysicalLinkCount(IN P_GLUE_INFO_T prGlueInfo);
#endif

#if CFG_ENABLE_WIFI_DIRECT
/*----------------------------------------------------------------------------*/
/* Wi-Fi Direct handling                                                      */
/*----------------------------------------------------------------------------*/
ENUM_PARAM_MEDIA_STATE_T kalP2PGetState(IN P_GLUE_INFO_T prGlueInfo);

VOID
kalP2PSetState(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_PARAM_MEDIA_STATE_T eState,
	       IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucRole);

VOID
kalP2PUpdateAssocInfo(IN P_GLUE_INFO_T prGlueInfo,
		      IN PUINT_8 pucFrameBody,
		      IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest);

UINT_32 kalP2PGetFreqInKHz(IN P_GLUE_INFO_T prGlueInfo);

UINT_8 kalP2PGetRole(IN P_GLUE_INFO_T prGlueInfo);

VOID
kalP2PSetRole(IN P_GLUE_INFO_T prGlueInfo,
	      IN UINT_8 ucResult, IN PUINT_8 pucSSID, IN UINT_8 ucSSIDLen, IN UINT_8 ucRole);

BOOLEAN kalP2PIndicateFound(IN P_GLUE_INFO_T prGlueInfo);

VOID kalP2PIndicateConnReq(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucDevName, IN INT_32 u4NameLength, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucDevType,	/* 0: P2P Device / 1: GC / 2: GO */
			   IN INT_32 i4ConfigMethod, IN INT_32 i4ActiveConfigMethod);

VOID
kalP2PInvitationIndication(IN P_GLUE_INFO_T prGlueInfo,
			   IN P_P2P_DEVICE_DESC_T prP2pDevDesc,
			   IN PUINT_8 pucSsid,
			   IN UINT_8 ucSsidLen,
			   IN UINT_8 ucOperatingChnl, IN UINT_8 ucInvitationType);

VOID kalP2PSetCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Cipher);

BOOLEAN kalP2PGetCipher(IN P_GLUE_INFO_T prGlueInfo);

UINT_16 kalP2PCalWSC_IELen(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType);

VOID kalP2PGenWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer);
#endif

/*----------------------------------------------------------------------------*/
/* NVRAM/Registry Service                                                     */
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsConfigurationExist(IN P_GLUE_INFO_T prGlueInfo);

P_REG_INFO_T kalGetConfiguration(IN P_GLUE_INFO_T prGlueInfo);

VOID
kalGetConfigurationVersion(IN P_GLUE_INFO_T prGlueInfo,
			   OUT PUINT_16 pu2Part1CfgOwnVersion,
			   OUT PUINT_16 pu2Part1CfgPeerVersion,
			   OUT PUINT_16 pu2Part2CfgOwnVersion, OUT PUINT_16 pu2Part2CfgPeerVersion);

BOOLEAN kalCfgDataRead16(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset, OUT PUINT_16 pu2Data);

BOOLEAN kalCfgDataWrite16(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset, IN UINT_16 u2Data);

BOOLEAN
kalRetrieveNetworkAddress(IN P_GLUE_INFO_T prGlueInfo, IN OUT PARAM_MAC_ADDRESS * prMacAddr);

/*----------------------------------------------------------------------------*/
/* WSC Connection                                                     */
/*----------------------------------------------------------------------------*/
BOOLEAN kalWSCGetActiveState(IN P_GLUE_INFO_T prGlueInfo);

/*----------------------------------------------------------------------------*/
/* RSSI Updating                                                              */
/*----------------------------------------------------------------------------*/
VOID
kalUpdateRSSI(IN P_GLUE_INFO_T prGlueInfo,
	      IN ENUM_KAL_NETWORK_TYPE_INDEX_T eNetTypeIdx, IN INT_8 cRssi, IN INT_8 cLinkQuality);

/*----------------------------------------------------------------------------*/
/* I/O Buffer Pre-allocation                                                  */
/*----------------------------------------------------------------------------*/
PVOID kalAllocateIOBuffer(IN UINT_32 u4AllocSize);

VOID kalReleaseIOBuffer(IN PVOID pvAddr, IN UINT_32 u4Size);


/*----------------------------------------------------------------------------*/
/* Whole-chip Reset Trigger                                                   */
/*----------------------------------------------------------------------------*/
#if CFG_CHIP_RESET_SUPPORT
VOID glSendResetRequest(VOID);

BOOLEAN kalIsResetting(VOID);

#endif

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
/*----------------------------------------------------------------------------*/
/* SDIO Read/Write Pattern Support                                            */
/*----------------------------------------------------------------------------*/
BOOLEAN kalSetSdioTestPattern(IN P_GLUE_INFO_T prGlueInfo, IN BOOLEAN fgEn, IN BOOLEAN fgRead);
#endif


/*----------------------------------------------------------------------------*/
/* AIS Remain-On-Channel, MGMT TX/RX Support                                  */
/*----------------------------------------------------------------------------*/
VOID
kalReadyOnChannel(IN P_GLUE_INFO_T prGlueInfo,
		  IN UINT_64 u8Cookie,
		  IN ENUM_BAND_T eBand,
		  IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum, IN UINT_32 u4DurationMs);

VOID
kalRemainOnChannelExpired(IN P_GLUE_INFO_T prGlueInfo,
			  IN UINT_64 u8Cookie,
			  IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum);

VOID
kalIndicateMgmtTxStatus(IN P_GLUE_INFO_T prGlueInfo,
			IN UINT_64 u8Cookie,
			IN BOOLEAN fgIsAck, IN PUINT_8 pucFrameBuf, IN UINT_32 u4FrameLen);

VOID kalIndicateRxMgmtFrame(IN P_GLUE_INFO_T prGlueInfo, IN P_SW_RFB_T prSwRfb);

/*----------------------------------------------------------------------------*/
/* PNO Support                                                                */
/*----------------------------------------------------------------------------*/
VOID kalSchedScanResults(IN P_GLUE_INFO_T prGlueInfo);

VOID kalSchedScanStopped(IN P_GLUE_INFO_T prGlueInfo);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif				/* _GL_KAL_H */
