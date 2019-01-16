/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/include/gl_kal.h#2 $
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
** 	//ALPS_SW/DEV/ALPS.JB2.MT6630.DEV/alps/mediatek/kernel/drivers/combo/drv_wlan/mt6630/wlan/...
**  	
** 	to //ALPS_SW/TRUNK/KK/alps/mediatek/kernel/drivers/combo/drv_wlan/mt6630/wlan/...
**
** 12 27 2013 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch][MT6630][Driver]MT6630 Wi-Fi Patch
** update code for ICAP & nvram
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
** 07 26 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Set NoACK to BMC packet
** 2. Add kalGetEthAddr function for Tx frame
** 3. Update RxIndicatePackets
**
** 07 23 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. build success for win32 port
** 2. add SDIO test read/write pattern for HQA tests (default off)
**
** 07 05 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Avoid large packet Tx issue
**
** 03 12 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx utility function for management frame
**
** 01 23 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Refine net dev implementation
**
** 01 21 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update TX path based on new ucBssIndex modifications.
**
** 01 17 2013 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Use ucBssIndex to replace eNetworkTypeIndex
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
 * 06 13 2012 yuche.tsai
 * NULL
 * Update maintrunk driver.
 * Add support for driver compose assoc request frame.
 *
 * 04 12 2012 terry.wu
 * NULL
 * Add AEE message support
 * 1) Show AEE warning(red screen) if SDIO access error occurs

 *
 * 03 02 2012 terry.wu
 * NULL
 * Snc CFG80211 modification for ICS migration from branch 2.2.
 *
 * 02 06 2012 wh.su
 * [WCXRP00001177] [MT6620 Wi-Fi][Driver][2.2] Adding the query channel filter for AP mode
 * adding the channel query filter for AP mode.
 *
 * 01 02 2012 wh.su
 * [WCXRP00001153] [MT6620 Wi-Fi][Driver] Adding the get_ch_list and set_tx_power proto type function
 * Adding the proto type function for set_int set_tx_power and get int get_ch_list.
 *
 * 12 13 2011 cm.chang
 * [WCXRP00001136] [All Wi-Fi][Driver] Add wake lock for pending timer
 * Add wake lock if timer timeout value is smaller than 5 seconds
 *
 * 11 24 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * adjust the code for Non-DBG and no XLOG.
 *
 * 11 22 2011 cp.wu
 * [WCXRP00001120] [MT6620 Wi-Fi][Driver] Modify roaming to AIS state transition from synchronous to asynchronous approach to avoid incomplete state termination
 * 1. change RDD related compile option brace position.
 * 2. when roaming is triggered, ask AIS to transit immediately only when AIS is in Normal TR state without join timeout timer ticking
 * 3. otherwise, insert AIS_REQUEST into pending request queue
 *
 * 11 11 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * modify the xlog related code.
 *
 * 11 10 2011 cp.wu
 * [WCXRP00001098] [MT6620 Wi-Fi][Driver] Replace printk by DBG LOG macros in linux porting layer
 * 1. eliminaite direct calls to printk in porting layer.
 * 2. replaced by DBGLOG, which would be XLOG on ALPS platforms.
 *
 * 11 10 2011 eddie.chen
 * [WCXRP00001096] [MT6620 Wi-Fi][Driver/FW] Enhance the log function (xlog)
 * Modify the QM xlog level and remove LOG_FUNC.
 *
 * 11 10 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * Using the new XLOG define for dum Memory.
 *
 * 11 08 2011 eddie.chen
 * [WCXRP00001096] [MT6620 Wi-Fi][Driver/FW] Enhance the log function (xlog)
 * Add xlog function.
 *
 * 11 08 2011 tsaiyuan.hsu
 * [WCXRP00001083] [MT6620 Wi-Fi][DRV]] dump debug counter or frames when debugging is triggered
 * add debug counters, eCurPsProf, for PS.
 *
 * 11 08 2011 cm.chang
 * NULL
 * Add RLM and CNM debug message for XLOG
 *
 * 11 07 2011 tsaiyuan.hsu
 * [WCXRP00001083] [MT6620 Wi-Fi][DRV]] dump debug counter or frames when debugging is triggered
 * add debug counters and periodically dump counters for debugging.
 *
 * 11 03 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * Add dumpMemory8 at XLOG support.
 *
 * 11 02 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * adding the code for XLOG.
 *
 * 10 12 2011 wh.su
 * [WCXRP00001036] [MT6620 Wi-Fi][Driver][FW] Adding the 802.11w code for MFP
 * adding the 802.11w related function and define .
 *
 * 04 18 2011 terry.wu
 * [WCXRP00000660] [MT6620 Wi-Fi][Driver] Remove flag CFG_WIFI_DIRECT_MOVED
 * Remove flag CFG_WIFI_DIRECT_MOVED.
 *
 * 04 12 2011 cp.wu
 * [WCXRP00000635] [MT6620 Wi-Fi][Driver] Clear pending security frames when QM clear pending data frames for dedicated network type
 * include link.h for linux's port.
 *
 * 04 12 2011 cp.wu
 * [WCXRP00000635] [MT6620 Wi-Fi][Driver] Clear pending security frames when QM clear pending data frames for dedicated network type
 * clear pending security frames for dedicated network type when BSS is being deactivated/disconnected
 *
 * 04 01 2011 cp.wu
 * [WCXRP00000540] [MT5931][Driver] Add eHPI8/eHPI16 support to Linux Glue Layer
 * 1. simplify config.h due to aggregation options could be also applied for eHPI/SPI interface
 * 2. use spin-lock instead of semaphore for protecting eHPI access because of possible access from ISR
 * 3. request_irq() API has some changes between linux kernel 2.6.12 and 2.6.26
 *
 * 03 16 2011 cp.wu
 * [WCXRP00000562] [MT6620 Wi-Fi][Driver] I/O buffer pre-allocation to avoid physically continuous memory shortage after system running for a long period
 * 1. pre-allocate physical continuous buffer while module is being loaded
 * 2. use pre-allocated physical continuous buffer for TX/RX DMA transfer
 *
 * The windows part remained the same as before, but added similiar APIs to hide the difference.
 *
 * 03 10 2011 chinghwa.yu
 * [WCXRP00000065] Update BoW design and settings
 * Add BOW table.
 *
 * 03 07 2011 terry.wu
 * [WCXRP00000521] [MT6620 Wi-Fi][Driver] Remove non-standard debug message
 * Toggle non-standard debug messages to comments.
 *
 * 03 06 2011 chinghwa.yu
 * [WCXRP00000065] Update BoW design and settings
 * Sync BOW Driver to latest person development branch version..
 *
 * 03 02 2011 cp.wu
 * [WCXRP00000503] [MT6620 Wi-Fi][Driver] Take RCPI brought by association response as initial RSSI right after connection is built.
 * use RCPI brought by ASSOC-RESP after connection is built as initial RCPI to avoid using a uninitialized MAC-RX RCPI.
 *
 * 02 24 2011 cp.wu
 * [WCXRP00000490] [MT6620 Wi-Fi][Driver][Win32] modify kalMsleep() implementation because NdisMSleep() won't sleep long enough for specified interval such as 500ms
 * modify cnm_timer and hem_mbox APIs to be thread safe to ease invoking restrictions
 *
 * 01 12 2011 cp.wu
 * [WCXRP00000357] [MT6620 Wi-Fi][Driver][Bluetooth over Wi-Fi] add another net device interface for BT AMP
 * implementation of separate BT_OVER_WIFI data path.
 *
 * 01 04 2011 cp.wu
 * [WCXRP00000338] [MT6620 Wi-Fi][Driver] Separate kalMemAlloc into kmalloc and vmalloc implementations to ease physically continous memory demands
 * separate kalMemAlloc() into virtually-continous and physically-continous type to ease slab system pressure
 *
 * 12 31 2010 cp.wu
 * [WCXRP00000335] [MT6620 Wi-Fi][Driver] change to use milliseconds sleep instead of delay to avoid blocking to system scheduling
 * change to use msleep() and shorten waiting interval to reduce blocking to other task while Wi-Fi driver is being loaded
 *
 * 12 31 2010 jeffrey.chang
 * [WCXRP00000332] [MT6620 Wi-Fi][Driver] add kal sleep function for delay which use blocking call
 * modify the implementation of kalDelay to msleep
 *
 * 12 22 2010 cp.wu
 * [WCXRP00000283] [MT6620 Wi-Fi][Driver][Wi-Fi Direct] Implementation of interface for supporting Wi-Fi Direct Service Discovery
 * 1. header file restructure for more clear module isolation
 * 2. add function interface definition for implementing Service Discovery callbacks
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
 * 09 28 2010 wh.su
 * NULL
 * [WCXRP00000069][MT6620 Wi-Fi][Driver] Fix some code for phase 1 P2P Demo.
 *
 * 09 21 2010 cp.wu
 * [WCXRP00000053] [MT6620 Wi-Fi][Driver] Reset incomplete and might leads to BSOD when entering RF test with AIS associated
 * Do a complete reset with STA-REC null checking for RF test re-entry
 *
 * 09 21 2010 kevin.huang
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * Eliminate Linux Compile Warning
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
 * 08 02 2010 jeffrey.chang
 * NULL
 * modify kalSetEvent declaration
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
 * 07 23 2010 jeffrey.chang
 *
 * fix kal header file
 *
 * 07 22 2010 jeffrey.chang
 *
 * use different spin lock for security frame
 *
 * 07 22 2010 jeffrey.chang
 *
 * add new spinlock
 *
 * 07 19 2010 jeffrey.chang
 *
 * add kal api for scanning done
 *
 * 07 19 2010 jeffrey.chang
 *
 * modify cmd/data path for new design
 *
 * 07 19 2010 jeffrey.chang
 *
 * add new kal api
 *
 * 07 19 2010 jeffrey.chang
 *
 * Linux port modification
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
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
 * 1) migrate assoc.c.
 * 2) add ucTxSeqNum for tracking frames which needs TX-DONE awareness
 * 3) add configuration options for CNM_MEM and RSN modules
 * 4) add data path for management frames
 * 5) eliminate rPacketInfo of MSDU_INFO_T
 *
 * 06 07 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * gl_kal merged
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 05 17 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * add basic handling framework for wireless extension ioctls.
 *
 * 05 11 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * add ioctl for controlling p2p scan phase parameters
 *
 * 05 10 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * fill network type field while doing frame identification.
 *
 * 05 10 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * implement basic wi-fi direct framework
 *
 * 05 07 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * add basic framework for implementating P2P driver hook.
 *
 * 05 07 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * modify kalMemAlloc method
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
 * 04 27 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * follow Linux's firmware framework, and remove unused kal API
 *
 * 04 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * when acquiring driver-own, wait for up to 8 seconds.
 *
 * 04 22 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 *
 * 1) modify rx path code for supporting Wi-Fi direct
 * 2) modify config.h since Linux dont need to consider retaining packet
 *
 * 04 21 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * add for private ioctl support
 *
 * 04 20 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * don't need SPIN_LOCK_PWR_CTRL anymore, it will raise IRQL
 *  * and cause SdBusSubmitRequest running at DISPATCH_LEVEL as well.
 *
 * 04 14 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * information buffer for query oid/ioctl is now buffered in prCmdInfo
 *  *  *  *  *  *  *  * instead of glue-layer variable to improve multiple oid/ioctl capability
 *
 * 04 13 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add framework for BT-over-Wi-Fi support.
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 1) prPendingCmdInfo is replaced by queue for multiple handler capability
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 2) command sequence number is now increased atomically
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 3) private data could be hold and taken use for other purpose
 *
 * 04 09 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * 1) add spinlock
 *  *  * 2) add KAPI for handling association info
 *
 * 04 09 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * adding firmware download KAPI
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * finish non-glue layer access to glue variables
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * accessing to firmware load/start address, and access to OID handling information
 *  *  *  * are now handled in glue layer
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * rWlanInfo should be placed at adapter rather than glue due to most operations
 *  *  *  *  *  *  *  *  * are done in adapter layer.
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
 * 04 06 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * improve none-glue code portability
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code refine: fgTestMode should be at adapter rather than glue due to the device/fw is also involved
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) for some OID, never do timeout expiration
 *  *  *  * 2) add 2 kal API for later integration
 *
 * 03 30 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * emulate NDIS Pending OID facility
 *
 * 03 26 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * [WPD00003826] Initial import for Linux port
 * adding firmware download KAPI
 *
 * 03 24 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * initial import for Linux port
**  \main\maintrunk.MT5921\41 2009-09-28 20:19:23 GMT mtk01090
**  Add private ioctl to carry OID structures. Restructure public/private ioctl interfaces to Linux kernel.
**  \main\maintrunk.MT5921\40 2009-08-18 22:57:09 GMT mtk01090
**  Add Linux SDIO (with mmc core) support.
**  Add Linux 2.6.21, 2.6.25, 2.6.26.
**  Fix compile warning in Linux.
**  \main\maintrunk.MT5921\39 2009-06-23 23:19:15 GMT mtk01090
**  Add build option BUILD_USE_EEPROM and compile option CFG_SUPPORT_EXT_CONFIG for NVRAM support
**  \main\maintrunk.MT5921\38 2009-02-09 14:03:17 GMT mtk01090
**  Add KAL function kalDevSetPowerState(). It is not implemented yet. Only add an empty macro.
**
**  \main\maintrunk.MT5921\37 2009-01-22 13:05:59 GMT mtk01088
**  new defeine to got 1x value at packet reserved field
**  \main\maintrunk.MT5921\36 2008-12-08 16:15:02 GMT mtk01461
**  Add kalQueryValidBufferLength() macro
**  \main\maintrunk.MT5921\35 2008-11-13 20:33:15 GMT mtk01104
**  Remove lint warning
**  \main\maintrunk.MT5921\34 2008-10-22 11:05:52 GMT mtk01461
**  Remove unused macro
**  \main\maintrunk.MT5921\33 2008-10-16 15:48:17 GMT mtk01461
**  Update driver to fix lint warning
**  \main\maintrunk.MT5921\32 2008-09-02 11:50:51 GMT mtk01461
**  SPIN_LOCK_SDIO_DDK_TX_QUE
**  \main\maintrunk.MT5921\31 2008-08-29 15:58:30 GMT mtk01088
**  remove non-used function for code refine
**  \main\maintrunk.MT5921\30 2008-08-21 00:33:29 GMT mtk01461
**  Update for Driver Review
**  \main\maintrunk.MT5921\29 2008-06-19 13:29:14 GMT mtk01425
**  1. Add declaration of SPIN_LOCK_SDIO_DDK_TX_QUE and SPIN_LOCK_SDIO_DDK_RX_QUE
**  \main\maintrunk.MT5921\28 2008-05-30 20:27:34 GMT mtk01461
**  Rename KAL function
**  \main\maintrunk.MT5921\27 2008-05-30 14:42:05 GMT mtk01461
**  Remove WMM Assoc Flag in KAL
**  \main\maintrunk.MT5921\26 2008-05-29 14:15:18 GMT mtk01084
**  remove un-used function
**  \main\maintrunk.MT5921\25 2008-04-23 14:02:20 GMT mtk01084
**  modify KAL port access function prototype
**  \main\maintrunk.MT5921\24 2008-04-17 23:06:41 GMT mtk01461
**  Add iwpriv support for AdHocMode setting
**  \main\maintrunk.MT5921\23 2008-04-08 15:38:50 GMT mtk01084
**  add KAL function to setting pattern search function enable/ disable
**  \main\maintrunk.MT5921\22 2008-03-26 15:34:48 GMT mtk01461
**  Add update MAC address func
**  \main\maintrunk.MT5921\21 2008-03-18 15:56:15 GMT mtk01084
**  update ENUM_NIC_INITIAL_PARAM_E
**  \main\maintrunk.MT5921\20 2008-03-18 11:49:28 GMT mtk01084
**  update function for initial value access
**  \main\maintrunk.MT5921\19 2008-03-18 10:21:31 GMT mtk01088
**  use kal update associate request at linux
**  \main\maintrunk.MT5921\18 2008-03-14 18:03:41 GMT mtk01084
**  refine register and port access function
**  \main\maintrunk.MT5921\17 2008-03-11 14:51:02 GMT mtk01461
**  Add copy_to(from)_user macro
**  \main\maintrunk.MT5921\16 2008-03-06 23:42:21 GMT mtk01385
**  1. add Query Registry Mac address function.
**  \main\maintrunk.MT5921\15 2008-02-26 09:48:04 GMT mtk01084
**  modify KAL set network address/ checksum offload part
**  \main\maintrunk.MT5921\14 2008-01-09 17:54:58 GMT mtk01084
**  Modify the argument of kalQueryPacketInfo
**  \main\maintrunk.MT5921\13 2007-11-29 02:05:20 GMT mtk01461
**  Fix Windows RX multiple packet retain problem
**  \main\maintrunk.MT5921\12 2007-11-26 19:43:45 GMT mtk01461
**  Add OS_TIMESTAMP macro
**
**  \main\maintrunk.MT5921\11 2007-11-09 16:36:15 GMT mtk01425
**  1. Modify for CSUM offloading with Tx Fragment
**  \main\maintrunk.MT5921\10 2007-11-07 18:38:37 GMT mtk01461
**  Add Tx Fragmentation Support
**  \main\maintrunk.MT5921\9 2007-11-06 19:36:50 GMT mtk01088
**  add the WPS related code
**  \main\maintrunk.MT5921\8 2007-11-02 01:03:57 GMT mtk01461
**  Unify TX Path for Normal and IBSS Power Save + IBSS neighbor learning
** Revision 1.4  2007/07/05 07:25:33  MTK01461
** Add Linux initial code, modify doc, add 11BB, RF init code
**
** Revision 1.3  2007/06/27 02:18:50  MTK01461
** Update SCAN_FSM, Initial(Can Load Module), Proc(Can do Reg R/W), TX API
**
** Revision 1.2  2007/06/25 06:16:23  MTK01461
** Update illustrations, gl_init.c, gl_kal.c, gl_kal.h, gl_os.h and RX API
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
#include "gl_wext_priv.h"
#include "link.h"
#include "nic/mac.h"
#include "nic/wlan_def.h"
#include "wlan_lib.h"
#include "wlan_oid.h"

#if CFG_ENABLE_BT_OVER_WIFI
#include "nic/bow.h"
#endif

#if CFG_SUPPORT_XLOG
#include "linux/xlog.h"
#endif


#include "linux/kallsyms.h"
#include <linux/ftrace_event.h>



#if DBG
extern int allocatedMemSize;
#endif

extern struct semaphore g_halt_sem;
extern int g_u4HaltFlag;

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* Define how many concurrent operation networks. */
#define KAL_BSS_NUM             3
#define KAL_P2P_NUM             1

#if CFG_SUPPORT_MULTITHREAD
#define GLUE_FLAG_TX_PROCESS (GLUE_FLAG_HALT |                \
                              GLUE_FLAG_SUB_MOD_MULTICAST |   \
                              GLUE_FLAG_TX_CMD_DONE |         \
                              GLUE_FLAG_TXREQ |               \
                              GLUE_FLAG_TIMEOUT |             \
                              GLUE_FLAG_FRAME_FILTER |        \
                              GLUE_FLAG_OID |                 \
			      GLUE_FLAG_RX)

#define GLUE_FLAG_HIF_PROCESS (GLUE_FLAG_HALT | GLUE_FLAG_INT | GLUE_FLAG_HIF_TX | GLUE_FLAG_HIF_TX_CMD | GLUE_FLAG_HIF_FW_OWN)
#define GLUE_FLAG_RX_PROCESS (GLUE_FLAG_HALT | GLUE_FLAG_RX_TO_OS)
#else
/* All flags for single thread driver */
#define GLUE_FLAG_TX_PROCESS  0xFFFFFFFF
#endif

#if CFG_SUPPORT_SNIFFER
#define RADIOTAP_FIELD_TSFT			BIT(0)
#define RADIOTAP_FIELD_FLAGS		BIT(1)
#define RADIOTAP_FIELD_RATE			BIT(2)
#define RADIOTAP_FIELD_CHANNEL		BIT(3)
#define RADIOTAP_FIELD_ANT_SIGNAL	BIT(5)
#define RADIOTAP_FIELD_ANT_NOISE	BIT(6)
#define RADIOTAP_FIELD_ANT			BIT(11)
#define RADIOTAP_FIELD_MCS			BIT(19)
#define RADIOTAP_FIELD_AMPDU		BIT(20)
#define RADIOTAP_FIELD_VHT			BIT(21)
#define RADIOTAP_FIELD_VENDOR       BIT(30)

#define RADIOTAP_LEN_VHT			48
#define RADIOTAP_FIELDS_VHT (RADIOTAP_FIELD_TSFT | \
                                    RADIOTAP_FIELD_FLAGS | \
                                    RADIOTAP_FIELD_RATE | \
                                    RADIOTAP_FIELD_CHANNEL | \
                                    RADIOTAP_FIELD_ANT_SIGNAL | \
                                    RADIOTAP_FIELD_ANT_NOISE | \
                                    RADIOTAP_FIELD_ANT | \
                                    RADIOTAP_FIELD_AMPDU | \
                                    RADIOTAP_FIELD_VHT | \
                                    RADIOTAP_FIELD_VENDOR)
                                    
#define RADIOTAP_LEN_HT				36
#define RADIOTAP_FIELDS_HT (RADIOTAP_FIELD_TSFT | \
                                    RADIOTAP_FIELD_FLAGS | \
                                    RADIOTAP_FIELD_RATE | \
                                    RADIOTAP_FIELD_CHANNEL | \
                                    RADIOTAP_FIELD_ANT_SIGNAL | \
                                    RADIOTAP_FIELD_ANT_NOISE | \
                                    RADIOTAP_FIELD_ANT | \
                                    RADIOTAP_FIELD_MCS | \
                                    RADIOTAP_FIELD_AMPDU | \
                                    RADIOTAP_FIELD_VENDOR)

#define RADIOTAP_LEN_LEGACY			26
#define RADIOTAP_FIELDS_LEGACY (RADIOTAP_FIELD_TSFT | \
                                    RADIOTAP_FIELD_FLAGS | \
                                    RADIOTAP_FIELD_RATE | \
                                    RADIOTAP_FIELD_CHANNEL | \
                                    RADIOTAP_FIELD_ANT_SIGNAL | \
                                    RADIOTAP_FIELD_ANT_NOISE | \
                                    RADIOTAP_FIELD_ANT | \
                                    RADIOTAP_FIELD_VENDOR)
#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef enum _ENUM_SPIN_LOCK_CATEGORY_E {
    SPIN_LOCK_FSM = 0,

#if CFG_SUPPORT_MULTITHREAD
    SPIN_LOCK_TX_PORT_QUE,
    SPIN_LOCK_TX_CMD_QUE,
    SPIN_LOCK_TX_CMD_DONE_QUE,
    SPIN_LOCK_TC_RESOURCE,
    SPIN_LOCK_RX_TO_OS_QUE,
#endif

  /* FIX ME */
    SPIN_LOCK_RX_QUE,
    SPIN_LOCK_RX_FREE_QUE,
    SPIN_LOCK_TX_QUE,
    SPIN_LOCK_CMD_QUE,
    SPIN_LOCK_TX_RESOURCE,
    SPIN_LOCK_CMD_RESOURCE,
    SPIN_LOCK_QM_TX_QUEUE,
    SPIN_LOCK_CMD_PENDING,
    SPIN_LOCK_CMD_SEQ_NUM,
    SPIN_LOCK_TX_MSDU_INFO_LIST,
    SPIN_LOCK_TXING_MGMT_LIST,
    SPIN_LOCK_TX_SEQ_NUM,
    SPIN_LOCK_TX_COUNT,
    SPIN_LOCK_TXS_COUNT,
  /* end    */
    SPIN_LOCK_TX,
    SPIN_LOCK_IO_REQ,
    SPIN_LOCK_INT,

    SPIN_LOCK_MGT_BUF,
    SPIN_LOCK_MSG_BUF,
    SPIN_LOCK_STA_REC,

    SPIN_LOCK_MAILBOX,
    SPIN_LOCK_TIMER,

    SPIN_LOCK_BOW_TABLE,

    SPIN_LOCK_EHPI_BUS, /* only for EHPI */
    SPIN_LOCK_NET_DEV,
    SPIN_LOCK_NUM
} ENUM_SPIN_LOCK_CATEGORY_E;

typedef enum _ENUM_MUTEX_CATEGORY_E {
    MUTEX_TX_CMD_CLEAR,    
    MUTEX_NUM
} ENUM_MUTEX_CATEGORY_E;

/* event for assoc infomation update */
typedef struct _EVENT_ASSOC_INFO {
    UINT_8      ucAssocReq; /* 1 for assoc req, 0 for assoc rsp */
    UINT_8      ucReassoc;  /* 0 for assoc, 1 for reassoc */
    UINT_16     u2Length;
    PUINT_8     pucIe;
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
    PHY_MEM_TYPE,   /* physically continuous */
    VIR_MEM_TYPE,   /* virtually continous */
    MEM_TYPE_NUM
} ENUM_KAL_MEM_ALLOCATION_TYPE;

#if CONFIG_ANDROID /* Defined in Android kernel source */
typedef struct wake_lock    KAL_WAKE_LOCK_T, *P_KAL_WAKE_LOCK_T;
#else
typedef UINT_32             KAL_WAKE_LOCK_T, *P_KAL_WAKE_LOCK_T;
#endif

#if CFG_SUPPORT_AGPS_ASSIST
typedef enum _ENUM_MTK_AGPS_ATTR {
	MTK_ATTR_AGPS_INVALID,
	MTK_ATTR_AGPS_CMD,
	MTK_ATTR_AGPS_DATA,
	MTK_ATTR_AGPS_IFINDEX,
	MTK_ATTR_AGPS_IFNAME,
	MTK_ATTR_AGPS_MAX
} ENUM_MTK_CCX_ATTR;

typedef enum _ENUM_AGPS_EVENT {
	AGPS_EVENT_WLAN_ON,
	AGPS_EVENT_WLAN_OFF,
	AGPS_EVENT_WLAN_AP_LIST,
} ENUM_CCX_EVENT;
BOOLEAN kalIndicateAgpsNotify(P_ADAPTER_T prAdapter, UINT_8 cmd, PUINT_8 data
, UINT_16 dataLen);
#endif /* CFG_SUPPORT_AGPS_ASSIST */

#if CFG_SUPPORT_SNIFFER
/* Vendor Namespace
 * Bit Number 30
 * Required Alignment 2 bytes
 */
typedef struct _RADIOTAP_FIELD_VENDOR_T {
    UINT_8      aucOUI[3]; 
    UINT_8      ucSubNamespace;
    UINT_16     u2DataLen;
    UINT_8      ucData;
} __attribute__ ((packed)) RADIOTAP_FIELD_VENDOR_T, *P_RADIOTAP_FIELD_VENDOR_T;

typedef struct _MONITOR_RADIOTAP_T {
	/* radiotap header */
	UINT_8		ucItVersion;		/* set to 0 */
    UINT_8		ucItPad;
    UINT_16		u2ItLen;			/* entire length */
    UINT_32		u4ItPresent;		/* fields present */

	/* TSFT
	 * Bit Number 0
	 * Required Alignment 8 bytes
	 * Unit microseconds
	 */
	UINT_64		u8MacTime;
	
	/* Flags
	 * Bit Number 1
	 */
	UINT_8		ucFlags;
	
	/* Rate
	 * Bit Number 2
	 * Unit 500 Kbps
	 */
	UINT_8		ucRate;
	
	/* Channel
	 * Bit Number 3
	 * Required Alignment 2 bytes
	 */
	UINT_16		u2ChFrequency;
	UINT_16		u2ChFlags;	
	
	/* Antenna signal
	 * Bit Number 5
	 * Unit dBm
	 */
	UINT_8		ucAntennaSignal;	
	
	/* Antenna noise
	 * Bit Number 6
	 * Unit dBm
	 */
	UINT_8		ucAntennaNoise;	
	
	/* Antenna
	 * Bit Number 11
	 * Unit antenna index
	 */
	UINT_8		ucAntenna;	

	/* MCS
	 * Bit Number 19
	 * Required Alignment 1 byte
	 */
	UINT_8		ucMcsKnown;
	UINT_8		ucMcsFlags;
	UINT_8		ucMcsMcs;	
	
	/* A-MPDU status
	 * Bit Number 20
	 * Required Alignment 4 bytes
	 */
	UINT_32		u4AmpduRefNum; 
	UINT_16		u2AmpduFlags; 
	UINT_8		ucAmpduDelimiterCRC; 
	UINT_8		ucAmpduReserved;
	
	/* VHT
	 * Bit Number 21
	 * Required Alignment 2 bytes
	 */
	UINT_16		u2VhtKnown; 
	UINT_8		ucVhtFlags; 
	UINT_8		ucVhtBandwidth; 
	UINT_8		aucVhtMcsNss[4]; 
	UINT_8		ucVhtCoding; 
	UINT_8		ucVhtGroupId; 
	UINT_16		u2VhtPartialAid;

    /* extension space */
    UINT_8      aucReserve[12];
} __attribute__ ((packed)) MONITOR_RADIOTAP_T, *P_MONITOR_RADIOTAP_T;
#endif 

typedef enum _ENUM_GET_SDIO_ERR_INFO_T {
	SDIO_READL_CRC_ERR = 8,
	SDIO_READL_TIMEOUT,
	SDIO_WRITEL_CRC_ERR,
	SDIO_WRITEL_TIMEOUT,
	SDIO_READ_SB_CRC_ERR,
	SDIO_READ_SB_TIMEOUT,
	SDIO_WRITE_SB_CRC_ERR,
	SDIO_WRITE_SB_TIMEOUT,
	SDIO_ERR_NUM
} ENUM_GET_SDIO_ERR_INFO_T, *P_ENUM_GET_SDIO_ERR_INFO_T;

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
/* Macros of getting current thread id                                        */
/*----------------------------------------------------------------------------*/
#define KAL_GET_CURRENT_THREAD_ID() (current->pid)

/*----------------------------------------------------------------------------*/
/* Macros of SPIN LOCK operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_SPIN_LOCK_DECLARATION()             ULONG __ulFlags

#define KAL_ACQUIRE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	    kalAcquireSpinLock(((P_ADAPTER_T)_prAdapter)->prGlueInfo, _rLockCategory, &__ulFlags)

#define KAL_RELEASE_SPIN_LOCK(_prAdapter, _rLockCategory)   \
	    kalReleaseSpinLock(((P_ADAPTER_T)_prAdapter)->prGlueInfo, _rLockCategory, __ulFlags)

/*----------------------------------------------------------------------------*/
/* Macros of MUTEX operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#define KAL_ACQUIRE_MUTEX(_prAdapter, _rLockCategory)   \
            kalAcquireMutex(((P_ADAPTER_T)_prAdapter)->prGlueInfo, _rLockCategory)

#define KAL_RELEASE_MUTEX(_prAdapter, _rLockCategory)   \
            kalReleaseMutex(((P_ADAPTER_T)_prAdapter)->prGlueInfo, _rLockCategory)

/*----------------------------------------------------------------------------*/
/* Macros for accessing Reserved Fields of native packet                      */
/*----------------------------------------------------------------------------*/
#define KAL_GET_PKT_QUEUE_ENTRY(_p)             GLUE_GET_PKT_QUEUE_ENTRY(_p)
#define KAL_GET_PKT_DESCRIPTOR(_prQueueEntry)   GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry)
#define KAL_GET_PKT_TID(_p)                     GLUE_GET_PKT_TID(_p)
#define KAL_GET_PKT_IS1X(_p)                    GLUE_GET_PKT_IS1X(_p)
#define KAL_GET_PKT_HEADER_LEN(_p)              GLUE_GET_PKT_HEADER_LEN(_p)
#define KAL_GET_PKT_PAYLOAD_LEN(_p)             GLUE_GET_PKT_PAYLOAD_LEN(_p)
#define KAL_GET_PKT_ARRIVAL_TIME(_p)            GLUE_GET_PKT_ARRIVAL_TIME(_p)

/*----------------------------------------------------------------------------*/
/* Macros of wake_lock operations for using in Driver Layer                   */
/*----------------------------------------------------------------------------*/
#if CONFIG_ANDROID /* Defined in Android kernel source */
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName) \
        wake_lock_init(_prWakeLock, WAKE_LOCK_SUSPEND, _pcName)

#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock) \
        wake_lock_destroy(_prWakeLock)

#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock) \
        wake_lock(_prWakeLock)

#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout) \
        wake_lock_timeout(_prWakeLock, _u4Timeout)

#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock) \
        wake_unlock(_prWakeLock)

#define KAL_WAKE_LOCK_ACTIVE(_prAdapter, _prWakeLock) \
        wake_lock_active(_prWakeLock)

#else
#define KAL_WAKE_LOCK_INIT(_prAdapter, _prWakeLock, _pcName)
#define KAL_WAKE_LOCK_DESTROY(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK_TIMEOUT(_prAdapter, _prWakeLock, _u4Timeout)
#define KAL_WAKE_UNLOCK(_prAdapter, _prWakeLock)
#define KAL_WAKE_LOCK_ACTIVE(_prAdapter, _prWakeLock)
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Cache memory allocation
*
* \param[in] u4Size Required memory size.
* \param[in] eMemType  Memory allocation type
*
* \return Pointer to allocated memory
*         or NULL
*/
/*----------------------------------------------------------------------------*/
#if DBG
#define kalMemAlloc(u4Size, eMemType) ({    \
    void *pvAddr; \
    if (eMemType == PHY_MEM_TYPE) { \
        pvAddr = kmalloc(u4Size, GFP_KERNEL);   \
    } \
    else { \
        pvAddr = vmalloc(u4Size);   \
    } \
    if (pvAddr) {   \
        allocatedMemSize += u4Size;   \
        printk(KERN_INFO DRV_NAME "0x%p(%ld) allocated (%s:%s)\n", \
	    pvAddr, (UINT_32)u4Size, __FILE__, __func__);  \
    }   \
    pvAddr; \
    })
#else
#define kalMemAlloc(u4Size, eMemType) ({    \
    void *pvAddr; \
    if (eMemType == PHY_MEM_TYPE) { \
        pvAddr = kmalloc(u4Size, GFP_KERNEL);   \
    } \
    else { \
        pvAddr = vmalloc(u4Size);   \
    } \
    pvAddr; \
    })
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Free allocated cache memory
*
* \param[in] pvAddr Required memory size.
* \param[in] eMemType  Memory allocation type
* \param[in] u4Size Allocated memory size.
*
* \return -
*/
/*----------------------------------------------------------------------------*/
#if DBG
#define kalMemFree(pvAddr, eMemType, u4Size)  \
    {   \
        if (pvAddr) {   \
            allocatedMemSize -= u4Size; \
            printk(KERN_INFO DRV_NAME "0x%p(%ld) freed (%s:%s)\n", \
		pvAddr, (UINT_32)u4Size, __FILE__, __func__);  \
        }   \
	if (eMemType == PHY_MEM_TYPE) { \
            kfree(pvAddr); \
        } \
        else { \
            vfree(pvAddr); \
        } \
    }
#else
#define kalMemFree(pvAddr, eMemType, u4Size)  \
    {   \
	if (eMemType == PHY_MEM_TYPE) { \
            kfree(pvAddr); \
        } \
        else { \
            vfree(pvAddr); \
        } \
    }
#endif

#define kalUdelay(u4USec)                           udelay(u4USec)

#define kalMdelay(u4MSec)                           mdelay(u4MSec)
#define kalMsleep(u4MSec)                           msleep(u4MSec)

/* Copy memory from user space to kernel space */
#define kalMemCopyFromUser(_pvTo, _pvFrom, _u4N)    copy_from_user(_pvTo, _pvFrom, _u4N)

/* Copy memory from kernel space to user space */
#define kalMemCopyToUser(_pvTo, _pvFrom, _u4N)      copy_to_user(_pvTo, _pvFrom, _u4N)

/* Copy memory block with specific size */
#define kalMemCopy(pvDst, pvSrc, u4Size)            memcpy(pvDst, pvSrc, u4Size)

/* Set memory block with specific pattern */
#define kalMemSet(pvAddr, ucPattern, u4Size)        memset(pvAddr, ucPattern, u4Size)

/* Compare two memory block with specific length.
 * Return zero if they are the same.
 */
#define kalMemCmp(pvAddr1, pvAddr2, u4Size)         memcmp(pvAddr1, pvAddr2, u4Size)

/* Zero specific memory block */
#define kalMemZero(pvAddr, u4Size)                  memset(pvAddr, 0, u4Size)

/* string operation */
#define kalStrCpy(dest, src)                         strcpy(dest, src)
#define kalStrnCpy(dest, src, n)                      strncpy(dest, src, n)
#define kalStrCmp(ct, cs)                            strcmp(ct, cs)
#define kalStrnCmp(ct, cs, n)                         strncmp(ct, cs, n)
#define kalStrChr(s, c)                              strchr(s, c)
#define kalStrrChr(s, c)                             strrchr(s, c)
#define kalStrnChr(s, n, c)                           strnchr(s, n, c)
#define kalStrLen(s)                                strlen(s)
#define kalStrnLen(s, b)                             strnlen(s, b)
#define kalStrniCmp(s, n)                            strnicmp(s, n)
#define kalStrtoul(cp, endp, base)                    simple_strtoul(cp, endp, base)
#define kalStrtol(cp, endp, base)                    simple_strtol(cp, endp, base)
#define kalSnprintf(buf, size, fmt, ...)              snprintf(buf, size, fmt, __VA_ARGS__)
#define kalSprintf(buf, fmt, ...)                     sprintf(buf, fmt, __VA_ARGS__)
/* remove for AOSP */
/* #define kalSScanf(buf, fmt, ...)                      sscanf(buf, fmt, __VA_ARGS__) */
#define kalStrStr(ct, cs)                            strstr(ct, cs)
#define kalStrSep(s, ct)                            strsep(s, ct)
#define kalStrCat(dest, src)                         strcat(dest, src)


/* defined for wince sdio driver only */
#if defined(_HIF_SDIO)
#define kalDevSetPowerState(prGlueInfo, ePowerMode) glSetPowerState(prGlueInfo, ePowerMode)
#else
#define kalDevSetPowerState(prGlueInfo, ePowerMode)
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Notify OS with SendComplete event of the specific packet. Linux should
*        free packets here.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pvPacket       Pointer of Packet Handle
* \param[in] status         Status Code for OS upper layer
*
* \return -
*/
/*----------------------------------------------------------------------------*/
#define kalSendComplete(prGlueInfo, pvPacket, status)   \
            kalSendCompleteAndAwakeQueue(prGlueInfo, pvPacket)


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to locate the starting address of incoming ethernet
*        frame for skb.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pvPacket       Pointer of Packet Handle
*
* \return starting address of ethernet frame buffer.
*/
/*----------------------------------------------------------------------------*/
#define kalQueryBufferPointer(prGlueInfo, pvPacket)     \
            ((PUINT_8)((struct sk_buff *)pvPacket)->data)


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to query the length of valid buffer which is accessible during
*         port read/write.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pvPacket       Pointer of Packet Handle
*
* \return starting address of ethernet frame buffer.
*/
/*----------------------------------------------------------------------------*/
#define kalQueryValidBufferLength(prGlueInfo, pvPacket)     \
            ((UINT_32)((struct sk_buff *)pvPacket)->end -  \
             (UINT_32)((struct sk_buff *)pvPacket)->data)

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to copy the entire frame from skb to the destination
*        address in the input parameter.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] pvPacket       Pointer of Packet Handle
* \param[in] pucDestBuffer  Destination Address
*
* \return -
*/
/*----------------------------------------------------------------------------*/
#define kalCopyFrame(prGlueInfo, pvPacket, pucDestBuffer)   \
            {struct sk_buff *skb = (struct sk_buff *)pvPacket; \
	    memcpy(pucDestBuffer, skb->data, skb->len); }

#define kalGetTimeTick()                            jiffies_to_msecs(jiffies)

#define kalPrint                                    printk

#if CFG_SUPPORT_XLOG
#define XLOG_TAG   "wlan"

#define XLogErrorFunc(_Fmt...)                     xlog_printk(ANDROID_LOG_ERROR, XLOG_TAG, _Fmt)
#define XLogWarnFunc(_Fmt...)                      xlog_printk(ANDROID_LOG_WARN, XLOG_TAG, _Fmt)
#define XLogInfoFunc(_Fmt...)                      xlog_printk(ANDROID_LOG_DEBUG, XLOG_TAG, _Fmt)
#define XLogDebugFunc(_Fmt...)                     xlog_printk(ANDROID_LOG_DEBUG, XLOG_TAG, _Fmt)
#define XLogVerboseFunc(_Fmt...)                   xlog_printk(ANDROID_LOG_VERBOSE, XLOG_TAG, _Fmt)

#define kalDbgLog(_DbgClass, _Fmt) \
{ \
    if (_DbgClass & DBG_CLASS_ERROR) { \
        XLogErrorFunc _Fmt; \
    } \
    else if (_DbgClass & DBG_CLASS_WARN) { \
        XLogWarnFunc _Fmt; \
    } \
    else if (_DbgClass & (DBG_CLASS_STATE | DBG_CLASS_EVENT | DBG_CLASS_INFO)) { \
        XLogInfoFunc _Fmt; \
    } \
    else if (_DbgClass & (DBG_CLASS_TRACE | DBG_CLASS_LOUD)) { \
        XLogDebugFunc _Fmt; \
    } \
    else if (_DbgClass & DBG_CLASS_TEMP) { \
        XLogVerboseFunc _Fmt; \
    } \
}    
#else
#define WLAN_TAG "[wlan] "
#define kalPrintTag(_Fmt...)        kalPrint(WLAN_TAG _Fmt)
#define kalDbgLog(_DbgClass, _Fmt)  kalPrintTag _Fmt

#endif

#define kalBreakPoint() \
    do { \
        BUG(); \
        panic("Oops"); \
    } while (0)

#if CFG_ENABLE_AEE_MSG
#define kalSendAeeException                         aee_kernel_exception
#define kalSendAeeWarning                           aee_kernel_warning
#define kalSendAeeReminding                         aee_kernel_reminding
#else
#define kalSendAeeException(_module, _desc, ...)
#define kalSendAeeWarning(_module, _desc, ...)
#define kalSendAeeReminding(_module, _desc, ...)
#endif

#define PRINTF_ARG(...)                             __VA_ARGS__
#define SPRINTF(buf, arg)                           {buf += sprintf((char *)(buf), PRINTF_ARG arg); }

#define USEC_TO_SYSTIME(_usec)      ((_usec) / USEC_PER_MSEC)
#define MSEC_TO_SYSTIME(_msec)      (_msec)

#define MSEC_TO_JIFFIES(_msec)      msecs_to_jiffies(_msec)

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/* Routines in gl_kal.c                                                       */
/*----------------------------------------------------------------------------*/
VOID
kalAcquireSpinLock(IN P_GLUE_INFO_T prGlueInfo,
		   IN ENUM_SPIN_LOCK_CATEGORY_E rLockCategory, OUT PULONG plFlags);

VOID
kalReleaseSpinLock(IN P_GLUE_INFO_T prGlueInfo,
		   IN ENUM_SPIN_LOCK_CATEGORY_E rLockCategory, IN ULONG ulFlags);

VOID kalUpdateMACAddress(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucMacAddr);

VOID kalAcquireMutex (IN P_GLUE_INFO_T prGlueInfo, IN ENUM_MUTEX_CATEGORY_E rMutexCategory);

VOID kalReleaseMutex (IN P_GLUE_INFO_T prGlueInfo, IN ENUM_MUTEX_CATEGORY_E rMutexCategory);


VOID kalPacketFree(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket);

PVOID kalPacketAlloc(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Size, OUT PUINT_8 *ppucData);

VOID kalOsTimerInitialize(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prTimerHandler);

BOOL kalSetTimer(IN P_GLUE_INFO_T prGlueInfo, IN OS_SYSTIME rInterval);

WLAN_STATUS
kalProcessRxPacket(IN P_GLUE_INFO_T prGlueInfo,
		   IN PVOID pvPacket, IN PUINT_8 pucPacketStart, IN UINT_32 u4PacketLen,
		   /* IN PBOOLEAN           pfgIsRetain, */
		   IN BOOLEAN fgIsRetain, IN ENUM_CSUM_RESULT_T aeCSUM[]
    );

WLAN_STATUS kalRxIndicatePkts(IN P_GLUE_INFO_T prGlueInfo, IN PVOID apvPkts[], IN UINT_8 ucPktNum);

WLAN_STATUS kalRxIndicateOnePkt(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPkt);

VOID
kalIndicateStatusAndComplete(IN P_GLUE_INFO_T prGlueInfo,
			     IN WLAN_STATUS eStatus, IN PVOID pvBuf, IN UINT_32 u4BufLen);

VOID
kalUpdateReAssocReqInfo(IN P_GLUE_INFO_T prGlueInfo,
    IN PUINT_8       pucFrameBody,
			IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest);

VOID
kalUpdateReAssocRspInfo(IN P_GLUE_INFO_T prGlueInfo,
			IN PUINT_8 pucFrameBody, IN UINT_32 u4FrameBodyLen);

#if CFG_TX_FRAGMENT
BOOLEAN
kalQueryTxPacketHeader(IN P_GLUE_INFO_T prGlueInfo,
		       IN PVOID pvPacket, OUT PUINT_16 pu2EtherTypeLen, OUT PUINT_8 pucEthDestAddr);
#endif /* CFG_TX_FRAGMENT */

VOID kalSendCompleteAndAwakeQueue(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvPacket);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
VOID kalQueryTxChksumOffloadParam(IN PVOID pvPacket, OUT PUINT_8 pucFlag);

VOID kalUpdateRxCSUMOffloadParam(IN PVOID pvPacket, IN ENUM_CSUM_RESULT_T eCSUM[]
    );
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */


BOOLEAN
kalRetrieveNetworkAddress(IN P_GLUE_INFO_T prGlueInfo, IN OUT PARAM_MAC_ADDRESS * prMacAddr);

VOID
kalReadyOnChannel(IN P_GLUE_INFO_T prGlueInfo,
    IN UINT_64          u8Cookie,
    IN ENUM_BAND_T      eBand,
		  IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum, IN UINT_32 u4DurationMs);

VOID
kalRemainOnChannelExpired(IN P_GLUE_INFO_T prGlueInfo,
    IN UINT_64          u8Cookie,
			  IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum);

VOID
kalIndicateMgmtTxStatus(IN P_GLUE_INFO_T prGlueInfo,
    IN UINT_64 u8Cookie,
			IN BOOLEAN fgIsAck, IN PUINT_8 pucFrameBuf, IN UINT_32 u4FrameLen);

VOID kalIndicateRxMgmtFrame(IN P_GLUE_INFO_T prGlueInfo, IN P_SW_RFB_T prSwRfb);

/*----------------------------------------------------------------------------*/
/* Routines in interface - ehpi/sdio.c                                                       */
/*----------------------------------------------------------------------------*/
BOOL kalDevRegRead(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, OUT PUINT_32 pu4Value);

BOOL kalDevRegWrite(P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, IN UINT_32 u4Value);

BOOL
kalDevPortRead(IN P_GLUE_INFO_T prGlueInfo,
    IN  UINT_16         u2Port,
	       IN UINT_32 u2Len, OUT PUINT_8 pucBuf, IN UINT_32 u2ValidOutBufSize);

BOOL
kalDevPortWrite(P_GLUE_INFO_T prGlueInfo,
    IN UINT_16     u2Port,
		IN UINT_32 u2Len, IN PUINT_8 pucBuf, IN UINT_32 u2ValidInBufSize);

UINT_32 kalDevGetSdioErrInfo(IN ENUM_GET_SDIO_ERR_INFO_T eSdioErrInfo);

BOOL kalDevWriteWithSdioCmd52(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Addr, IN UINT_8 ucData);


#if CFG_SUPPORT_EXT_CONFIG
UINT_32 kalReadExtCfg(IN P_GLUE_INFO_T prGlueInfo);
#endif

BOOLEAN
kalQoSFrameClassifierAndPacketInfo(IN P_GLUE_INFO_T prGlueInfo,
    IN P_NATIVE_PACKET prPacket, OUT P_TX_PACKET_INFO prTxPktInfo);

BOOLEAN
kalGetEthDestAddr(IN P_GLUE_INFO_T prGlueInfo,
		  IN P_NATIVE_PACKET prPacket, OUT PUINT_8 pucEthDestAddr);

VOID
kalOidComplete(IN P_GLUE_INFO_T prGlueInfo,
	       IN BOOLEAN fgSetQuery, IN UINT_32 u4SetQueryInfoLen, IN WLAN_STATUS rOidStatus);


WLAN_STATUS
kalIoctl(IN P_GLUE_INFO_T prGlueInfo,
    IN PFN_OID_HANDLER_FUNC     pfnOidHandler,
    IN PVOID                    pvInfoBuf,
    IN UINT_32                  u4InfoBufLen,
	 IN BOOL fgRead, IN BOOL fgWaitResp, IN BOOL fgCmd, OUT PUINT_32 pu4QryInfoLen);

VOID kalHandleAssocInfo(IN P_GLUE_INFO_T prGlueInfo, IN P_EVENT_ASSOC_INFO prAssocInfo);

#if CFG_ENABLE_FW_DOWNLOAD

PVOID
kalFirmwareImageMapping(IN P_GLUE_INFO_T prGlueInfo,
			OUT PPVOID ppvMapFileBuf, OUT PUINT_32 pu4FileLength);

VOID
kalFirmwareImageUnmapping(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prFwHandle, IN PVOID pvMapFileBuf);
#endif


/*----------------------------------------------------------------------------*/
/* Card Removal Check                                                         */
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsCardRemoved(IN P_GLUE_INFO_T prGlueInfo);


/*----------------------------------------------------------------------------*/
/* TX                                                                         */
/*----------------------------------------------------------------------------*/
VOID kalFlushPendingTxPackets(IN P_GLUE_INFO_T prGlueInfo);


/*----------------------------------------------------------------------------*/
/* Media State Indication                                                     */
/*----------------------------------------------------------------------------*/
ENUM_PARAM_MEDIA_STATE_T kalGetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo);


VOID
kalSetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo,
			  IN ENUM_PARAM_MEDIA_STATE_T eParamMediaStateIndicate);


/*----------------------------------------------------------------------------*/
/* OID handling                                                               */
/*----------------------------------------------------------------------------*/
VOID kalOidCmdClearance(IN P_GLUE_INFO_T prGlueInfo);

VOID kalOidClearance(IN P_GLUE_INFO_T prGlueInfo);

VOID kalEnqueueCommand(IN P_GLUE_INFO_T prGlueInfo, IN P_QUE_ENTRY_T prQueueEntry);

#if CFG_ENABLE_BT_OVER_WIFI
/*----------------------------------------------------------------------------*/
/* Bluetooth over Wi-Fi handling                                              */
/*----------------------------------------------------------------------------*/
VOID kalIndicateBOWEvent(IN P_GLUE_INFO_T prGlueInfo, IN P_AMPC_EVENT prEvent);

ENUM_BOW_DEVICE_STATE kalGetBowState(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr);

BOOLEAN
kalSetBowState(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_BOW_DEVICE_STATE eBowState, PARAM_MAC_ADDRESS rPeerAddr);

ENUM_BOW_DEVICE_STATE kalGetBowGlobalState(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetBowFreqInKHz(IN P_GLUE_INFO_T prGlueInfo);

UINT_8 kalGetBowRole(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr);

VOID kalSetBowRole(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRole, IN PARAM_MAC_ADDRESS rPeerAddr);

UINT_8 kalGetBowAvailablePhysicalLinkCount(IN P_GLUE_INFO_T prGlueInfo);

#if CFG_BOW_SEPARATE_DATA_PATH
/*----------------------------------------------------------------------------*/
/* Bluetooth over Wi-Fi Net Device Init/Uninit                                */
/*----------------------------------------------------------------------------*/
BOOLEAN kalInitBowDevice(IN P_GLUE_INFO_T prGlueInfo, IN const char *prDevName);

BOOLEAN kalUninitBowDevice(IN P_GLUE_INFO_T prGlueInfo);
#endif				/* CFG_BOW_SEPARATE_DATA_PATH */
#endif				/* CFG_ENABLE_BT_OVER_WIFI */


/*----------------------------------------------------------------------------*/
/* Firmware Download Handling                                                 */
/*----------------------------------------------------------------------------*/
UINT_32 kalGetFwStartAddress(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetFwLoadAddress(IN P_GLUE_INFO_T prGlueInfo);

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

VOID kalClearMgmtFramesByBssIdx(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex);

UINT_32 kalGetTxPendingFrameCount(IN P_GLUE_INFO_T prGlueInfo);

UINT_32 kalGetTxPendingCmdCount(IN P_GLUE_INFO_T prGlueInfo);

VOID kalClearCommandQueue(IN P_GLUE_INFO_T prGlueInfo);

BOOLEAN kalSetTimer(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Interval);

BOOLEAN kalCancelTimer(IN P_GLUE_INFO_T prGlueInfo);

VOID
kalScanDone(IN P_GLUE_INFO_T prGlueInfo,
	    IN ENUM_KAL_NETWORK_TYPE_INDEX_T eNetTypeIdx, IN WLAN_STATUS status);

UINT_32 kalRandomNumber(VOID);

VOID kalTimeoutHandler(unsigned long arg);

VOID kalSetEvent(P_GLUE_INFO_T pr);

#if CFG_SUPPORT_MULTITHREAD
VOID kalSetTxEvent2Hif(P_GLUE_INFO_T pr);

VOID kalSetTxEvent2Rx(P_GLUE_INFO_T pr);

VOID kalSetTxCmdEvent2Hif(P_GLUE_INFO_T pr);
#endif
/*----------------------------------------------------------------------------*/
/* NVRAM/Registry Service                                                     */
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsConfigurationExist(IN P_GLUE_INFO_T prGlueInfo);

P_REG_INFO_T kalGetConfiguration(IN P_GLUE_INFO_T prGlueInfo);

VOID
kalGetConfigurationVersion(IN P_GLUE_INFO_T prGlueInfo,
    OUT PUINT_16        pu2Part1CfgOwnVersion,
    OUT PUINT_16        pu2Part1CfgPeerVersion,
			   OUT PUINT_16 pu2Part2CfgOwnVersion, OUT PUINT_16 pu2Part2CfgPeerVersion);

BOOLEAN kalCfgDataRead16(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset, OUT PUINT_16 pu2Data);

BOOLEAN kalCfgDataWrite16(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset, IN UINT_16 u2Data);

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
BOOLEAN kalInitIOBuffer(VOID);

VOID kalUninitIOBuffer(VOID);

PVOID kalAllocateIOBuffer(IN UINT_32 u4AllocSize);

VOID kalReleaseIOBuffer(IN PVOID pvAddr, IN UINT_32 u4Size);

VOID
kalGetChannelList(IN P_GLUE_INFO_T prGlueInfo,
    IN ENUM_BAND_T             eSpecificBand,
    IN UINT_8                  ucMaxChannelNum,
		  IN PUINT_8 pucNumOfChannel, IN P_RF_CHANNEL_INFO_T paucChannelList);

BOOL kalIsAPmode(IN P_GLUE_INFO_T prGlueInfo);

#if CFG_SUPPORT_802_11W
/*----------------------------------------------------------------------------*/
/* 802.11W                                                                    */
/*----------------------------------------------------------------------------*/
UINT_32 kalGetMfpSetting(IN P_GLUE_INFO_T prGlueInfo);
#endif

UINT_32 kalWriteToFile(const PUINT_8 pucPath, BOOLEAN fgDoAppend, PUINT_8 pucData, UINT_32 u4Size);

UINT_32 kalCheckPath(const PUINT_8 pucPath);

INT_32 kalReadToFile(const PUINT_8 pucPath, PUINT_8 pucData, UINT_32 u4Size, PUINT_32 pu4ReadSize);

/*----------------------------------------------------------------------------*/
/* NL80211                                                                    */
/*----------------------------------------------------------------------------*/
VOID
kalIndicateBssInfo(IN P_GLUE_INFO_T prGlueInfo,
    IN PUINT_8  pucFrameBuf,
		   IN UINT_32 u4BufLen, IN UINT_8 ucChannelNum, IN INT_32 i4SignalStrength);

/*----------------------------------------------------------------------------*/
/* Net device                                                                 */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
kalHardStartXmit(struct sk_buff *prSkb,
		 IN struct net_device *prDev, P_GLUE_INFO_T prGlueInfo, UINT_8 ucBssIndex);

BOOLEAN
kalGetIPv4Address(IN struct net_device *prDev,
    IN UINT_32 u4MaxNumOfAddr,
		  OUT PUINT_8 pucIpv4Addrs, OUT PUINT_32 pu4NumOfIpv4Addr);

BOOLEAN
kalGetIPv6Address(IN struct net_device *prDev,
    IN UINT_32 u4MaxNumOfAddr,
		  OUT PUINT_8 pucIpv6Addrs, OUT PUINT_32 pu4NumOfIpv6Addr);

VOID
kalSetNetAddressFromInterface(IN P_GLUE_INFO_T prGlueInfo,
			      IN struct net_device *prDev, IN BOOLEAN fgSet);

WLAN_STATUS kalResetStats(IN struct net_device *prDev);

PVOID kalGetStats(IN struct net_device *prDev);

VOID kalResetPacket(IN P_GLUE_INFO_T prGlueInfo,
    IN P_NATIVE_PACKET prPacket);

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
/*----------------------------------------------------------------------------*/
/* SDIO Read/Write Pattern Support                                            */
/*----------------------------------------------------------------------------*/
BOOLEAN kalSetSdioTestPattern(IN P_GLUE_INFO_T prGlueInfo, IN BOOLEAN fgEn, IN BOOLEAN fgRead);
#endif

/*----------------------------------------------------------------------------*/
/* PNO Support                                                                */
/*----------------------------------------------------------------------------*/
VOID kalSchedScanResults(IN P_GLUE_INFO_T prGlueInfo);

VOID kalSchedScanStopped(IN P_GLUE_INFO_T prGlueInfo);

#if CFG_MULTI_ECOVER_SUPPORT
typedef enum _ENUM_WMTHWVER_TYPE_T {
    WMTHWVER_E1 = 0x0,
    WMTHWVER_E2 = 0x1,
    WMTHWVER_E3 = 0x2,
    WMTHWVER_E4 = 0x3,
    WMTHWVER_E5 = 0x4,
    WMTHWVER_E6 = 0x5,
    WMTHWVER_MAX,
    WMTHWVER_INVALID = 0xff
} ENUM_WMTHWVER_TYPE_T, *P_ENUM_WMTHWVER_TYPE_T;

extern ENUM_WMTHWVER_TYPE_T mtk_wcn_wmt_hwver_get(VOID);

#endif

VOID kalSetFwOwnEvent2Hif(P_GLUE_INFO_T pr);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

int tx_thread(void *data);

#if CFG_SUPPORT_MULTITHREAD
int hif_thread(void *data);
int rx_thread(void *data);
#endif
UINT_64 kalGetBootTime(VOID);

int kalMetInitProcfs(
    IN P_GLUE_INFO_T prGlueInfo
    );
int kalMetRemoveProcfs(void);        
BOOLEAN kalIsWakeupByWlan(P_ADAPTER_T  prAdapter);
#endif /* _GL_KAL_H */
