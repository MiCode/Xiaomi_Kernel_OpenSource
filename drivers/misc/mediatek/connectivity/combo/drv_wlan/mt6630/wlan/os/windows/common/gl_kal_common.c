/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/common/gl_kal_common.c#2 $
*/

/*! \file   gl_kal_common.c
    \brief  KAL routines of Windows driver

*/



/*
** $Log: gl_kal_common.c $
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
** 08 09 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Add new input parameter, Tx done status, for wlanReleaseCommand()
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
** 03 29 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. remove unused HIF definitions
** 2. enable NDIS 5.1 build success
**
** 03 13 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** .
**
** 03 12 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx utility function for management frame
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
** 09 04 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** sync for NVRAM warning scan result generation for CFG80211.
 *
 * 03 02 2012 terry.wu
 * NULL
 * Snc CFG80211 modification for ICS migration from branch 2.2.
 *
 * 08 15 2011 cp.wu
 * [WCXRP00000913] [MT6620 Wi-Fi] create repository of source code dedicated for MT6620 E6 ASIC
 * support to load different firmware image for E3/E4/E5 and E6 ASIC on win32 platforms.
 *
 * 06 07 2011 yuche.tsai
 * [WCXRP00000696] [Volunteer Patch][MT6620][Driver] Infinite loop issue when RX invitation response.[WCXRP00000763] [Volunteer Patch][MT6620][Driver] RX Service Discovery Frame under AP mode Issue
 * Add invitation support.
 *
 * 04 22 2011 cp.wu
 * [WCXRP00000598] [MT6620 Wi-Fi][Driver] Implementation of interface for communicating with user space process for RESET_START and RESET_END events
 * skip power-off handshaking when RESET indication is received.
 *
 * 04 18 2011 cp.wu
 * [WCXRP00000636] [WHQL][MT5931 Driver] 2c_PMHibernate (hang on 2h)
 * 1) add API for glue layer to query ACPI state
 * 2) Windows glue should not access to hardware after switched into D3 state
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
 * 02 23 2011 cp.wu
 * [WCXRP00000490] [MT6620 Wi-Fi][Driver][Win32] modify kalMsleep() implementation because NdisMSleep() won't sleep long enough for specified interval such as 500ms
 * add design to avoid system ticks wraps around
 *
 * 02 23 2011 cp.wu
 * [WCXRP00000490] [MT6620 Wi-Fi][Driver][Win32] modify kalMsleep() implementation because NdisMSleep() won't sleep long enough for specified interval such as 500ms
 * kalMsleep is now implemented with awareness of system ticks to ensure the sleep function has waited for long enough.
 *
 * 01 24 2011 cp.wu
 * [WCXRP00000382] [MT6620 Wi-Fi][Driver] Track forwarding packet number with notifying tx thread for serving
 * 1. add an extra counter for tracking pending forward frames.
 * 2. notify TX service thread as well when there is pending forward frame
 * 3. correct build errors leaded by introduction of Wi-Fi direct separation module
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
 * 11 03 2010 wh.su
 * [WCXRP00000124] [MT6620 Wi-Fi] [Driver] Support the dissolve P2P Group
 * Refine the HT rate disallow TKIP pairwise cipher .
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
 * 10 08 2010 wh.su
 * [WCXRP00000085] [MT6620 Wif-Fi] [Driver] update the modified p2p state machine
 * fixed the compiling error.
 *
 * 10 06 2010 cp.wu
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * code reorganization to improve isolation between GLUE and CORE layers.
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
 * 09 13 2010 cp.wu
 * NULL
 * acquire & release power control in oid handing wrapper.
 *
 * 09 10 2010 wh.su
 * NULL
 * fixed the compiling error at WinXP.
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
 * 08 30 2010 cp.wu
 * NULL
 * API added: nicTxPendingPackets(), for simplifying porting layer
 *
 * 08 23 2010 cp.wu
 * NULL
 * revise constant definitions to be matched with implementation (original cmd-event definition is deprecated)
 *
 * 08 20 2010 yuche.tsai
 * NULL
 * Modify kalP2PSetRole, the max value of ucRole should be 2.
 *
 * 08 16 2010 cp.wu
 * NULL
 * P2P packets are now marked when being queued into driver, and identified later without checking MAC address
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
 * 07 30 2010 cp.wu
 * NULL
 * NDIS-Driver Framework: MMPDU is no longer transmitted via TX0
 *
 * 07 29 2010 cp.wu
 * NULL
 * always decrease oid pending count before indicating completion to NDIS.
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
 * 07 13 2010 cp.wu
 *
 * 1) MMPDUs are now sent to MT6620 by CMD queue for keeping strict order of 1X/MMPDU/CMD packets
 * 2) integrate with qmGetFrameAction() for deciding which MMPDU/1X could pass checking for sending
 * 2) enhance CMD_INFO_T descriptor number from 10 to 32 to avoid descriptor underflow under concurrent network operation
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 07 01 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * implementation of DRV-SCN and related mailbox message handling.
 *
 * 06 24 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 802.1x and bluetooth-over-Wi-Fi security frames are now delievered to firmware via command path instead of data path.
 *
 * 06 23 2010 yarco.yang
 * [WPD00003837][MT6620]Data Path Refine
 * Merge g_arStaRec[] into adapter->arStaRec[]
 *
 * 06 21 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * change MAC address updating logic.
 *
 * 06 21 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * simplify timer usage.
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
 * 06 10 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * change to enqueue TX frame infinitely.
 *
 * 06 08 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * cnm_timer has been migrated.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 06 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * move timer callback to glue layer.
 *
 * 05 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * acquire LP-OWN only when it is necessary.
 *
 * 05 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * acquire LP-own with finer-grain awareness.
 *
 * 05 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * sleepy notify is only used for sleepy state,
 * while wake-up state is automatically set when host needs to access device
 *
 * 05 17 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * add basic handling framework for wireless extension ioctls.
 *
 * 05 17 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * 1) add timeout handler mechanism for pending command packets
 * 2) add p2p add/removal key
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
 * identify BT Over Wi-Fi Security frame and mark it as 802.1X frame
 *
 * 04 27 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add multiple physical link support
 *
 * 04 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * when sending address overriding command, attach with proper TCP/IP offloading setting as well
 *
 * 04 14 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * information buffer for query oid/ioctl is now buffered in prCmdInfo
 *  *  *  *  *  *  *  *  * instead of glue-layer variable to improve multiple oid/ioctl capability
 *
 * 04 13 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add framework for BT-over-Wi-Fi support.
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 1) prPendingCmdInfo is replaced by queue for multiple handler capability
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 2) command sequence number is now increased atomically
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 3) private data could be hold and taken use for other purpose
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. free cmdinfo after command is emiited.
 *  * 2. for BoW frames, user priority is extracted from sk_buff directly.
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * finish non-glue layer access to glue variables
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * accessing to firmware load/start address, and access to OID handling information
 *  *  *  *  * are now handled in glue layer
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * rWlanInfo should be placed at adapter rather than glue due to most operations
 *  *  *  *  *  *  *  *  *  *  *  * are done in adapter layer.
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
 *  *  *  *  *  * 2) add 2 kal API for later integration
 *
 * 03 25 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add Bluetooth-over-Wifi frame header check
 *
 * 03 24 2010 yuche.tsai
 * [WPD00003825]Multiple OID issue.
 * Fix multiple OID issue. Completing one OID may trigger another OID set/query call back to driver. This issue can not be resolved by only raising IRQL to dispatch level. Decrease the pending OID counter before completing the OID.
 *
 * 03 19 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add ACPI D0/D3 state switching support
 *  *  *  *  *  *  *  *  *  * 2) use more formal way to handle interrupt when the status is retrieved from enhanced RX response
 *
 * 03 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add another spin-lock to protect MsduInfoList due to it might be accessed by different thread.
 *  *  *  *  *  * 2) change own-back acquiring procedure to wait for up to 16.67 seconds
 *
 * 03 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * when starting adapter, read local adminsitrated address from registry and send to firmware via CMD_BASIC_CONFIG.
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) the use of prPendingOid revised, all accessing are now protected by spin lock
 *  *  *  *  *  * 2) ensure wlanReleasePendingOid will clear all command queues
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add mutex to avoid multiple access to qmTxQueue simultaneously.
 *
 * 02 12 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) reset timeout flag when cancelling timer
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) implement timeout mechanism when OID is pending for longer than 1 second
 *  *  *  *  * 2) allow OID_802_11_CONFIGURATION to be executed when RF test mode is turned on
 *
 * 01 27 2010 wh.su
 * [WPD00003816][MT6620 Wi-Fi] Adding the security support
 * .
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 *  *  *  *  *  *  *  *  *  *  * 2. block TX/ordinary OID when RF test mode is engaged
 *  *  *  *  *  *  *  *  *  *  * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 *  *  *  *  *  *  *  *  *  *  * 4. correct some HAL implementation
 *
 * 01 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * Under WinXP with SDIO, use prGlueInfo->rHifInfo.pvInformationBuffer instead of prGlueInfo->pvInformationBuffer
**  \main\maintrunk.MT6620WiFiDriver_Prj\34 2009-12-10 16:48:30 GMT mtk02752
**  code clean
**  \main\maintrunk.MT6620WiFiDriver_Prj\33 2009-12-03 16:15:41 GMT mtk01461
**  Add debug message
**  \main\maintrunk.MT6620WiFiDriver_Prj\32 2009-12-03 14:19:59 GMT mtk02752
**  refine: only invoke qmDequeueTxPackets() when i4TxPendingFrameNum >= 0
**  \main\maintrunk.MT6620WiFiDriver_Prj\31 2009-12-03 09:59:14 GMT mtk02752
**  Reset flag will flush TX packet from now on
**  \main\maintrunk.MT6620WiFiDriver_Prj\30 2009-12-02 22:06:31 GMT mtk02752
**  kalOidComplete() will decrease i4OidPendingCount
**  \main\maintrunk.MT6620WiFiDriver_Prj\29 2009-12-01 23:01:54 GMT mtk02752
**  after wlanReleasePendingOid() is called, decrease i4OidPendingCount
**  \main\maintrunk.MT6620WiFiDriver_Prj\28 2009-11-27 12:46:08 GMT mtk02752
**  TX/RX queues should be flushed while finishing reset
**  \main\maintrunk.MT6620WiFiDriver_Prj\27 2009-11-27 11:08:16 GMT mtk02752
**  add flush for reset
**  \main\maintrunk.MT6620WiFiDriver_Prj\26 2009-11-26 22:51:13 GMT mtk02752
**  for oids handled directly inside kalTxServiceThread, after indication i4OidPendingCount should be decreased too
**  \main\maintrunk.MT6620WiFiDriver_Prj\25 2009-11-26 20:55:00 GMT mtk02752
**  separate different checking on different compilation options
**  \main\maintrunk.MT6620WiFiDriver_Prj\24 2009-11-26 20:18:19 GMT mtk02752
**  check return value of qmDequeueTxPackets()
**  \main\maintrunk.MT6620WiFiDriver_Prj\23 2009-11-24 20:52:27 GMT mtk02752
**  integration with SD1's data path API
**  \main\maintrunk.MT6620WiFiDriver_Prj\22 2009-11-24 19:53:12 GMT mtk02752
**  mpSendPacket() is not used in new data path
**  \main\maintrunk.MT6620WiFiDriver_Prj\21 2009-11-17 22:41:07 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\20 2009-11-17 17:36:56 GMT mtk02752
**  complete data path when SD1_SD3_DATAPATH_INTEGRATION is turned on
**  \main\maintrunk.MT6620WiFiDriver_Prj\19 2009-11-16 21:41:39 GMT mtk02752
**  + kalQoSFrameClassifierAndPacketInfo() from wlanQoSFrameClassifierAndPacketInfo
**  + SD1_SD3 integration DATA path
**  \main\maintrunk.MT6620WiFiDriver_Prj\18 2009-11-13 18:12:31 GMT mtk02752
**  Only invoke NdisMSet/QueryInformationComplete when handling is finished processing at OID handling stage
**  \main\maintrunk.MT6620WiFiDriver_Prj\17 2009-11-11 14:46:03 GMT mtk02752
**  re-enable RX0_DONE/RX1_DONE interrupt when enough buffer has been freed
**  \main\maintrunk.MT6620WiFiDriver_Prj\16 2009-11-11 10:36:24 GMT mtk01084
**  invoke wlanIST in thread
**  \main\maintrunk.MT6620WiFiDriver_Prj\15 2009-10-29 19:57:47 GMT mtk01084
**  remoe excessive debug messages
**  \main\maintrunk.MT6620WiFiDriver_Prj\14 2009-10-13 21:59:27 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\13 2009-10-02 14:03:00 GMT mtk01725
**  \main\maintrunk.MT6620WiFiDriver_Prj\12 2009-09-09 17:26:22 GMT mtk01084
**  modify for DDK related functions
**  \main\maintrunk.MT6620WiFiDriver_Prj\11 2009-05-19 10:51:45 GMT mtk01461
**  Fix the issue of RESET flag and ResetComplete(), and all wlanReleasePendingOid
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-05-12 09:46:47 GMT mtk01461
**  Add handle pending reset after OID was completed in kalServiceThread
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-04-21 09:48:48 GMT mtk01461
**  Add ACQUIRE/RECLAIM_POWER_CONTRO in kalTxServiceThread()
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-04-10 21:54:22 GMT mtk01461
**  Add comment to kalTxServiceThread()
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-04-01 11:00:45 GMT mtk01461
**  Add SDIO_TX_ENHANCE function and kalSetEvent()
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-03-23 21:56:52 GMT mtk01461
**  Update kalTxServiceThread() for calling mpSendPacket()
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-03-17 10:43:46 GMT mtk01426
**  Move TxServiceThread to Kal layer
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-17 09:50:21 GMT mtk01426
**  Move TxServiceThread to here
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-16 14:08:12 GMT mtk01461
**  Remove kalAcquireSpinLock() & kalReleaseSpinLock(), replace by MACRO
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:36:53 GMT mtk01426
**  Init for develop
**
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "config.h"
#include "gl_os.h"
#include "mac.h"
#include "link.h"
#include "wlan_def.h"
#include "cmd_buf.h"
#include "mt6630_reg.h"

#include <ntddk.h>
#include "Ntstrsafe.h"
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
ULONG RtlRandomEx(__inout PULONG Seed);

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


/*----------------------------------------------------------------------------*/
/*!
* @brief Cache-able memory free
*
* @param pvAddr Starting address of the memory
* @param eMemType Memory allocation type
* @param u4Size Size of memory to allocate, in bytes
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID kalMemFree(IN PVOID pvAddr, IN ENUM_KAL_MEM_ALLOCATION_TYPE eMemType, IN UINT_32 u4Size)
{
	NdisFreeMemory(pvAddr, u4Size, 0);
}				/* kalMemFree */


/*----------------------------------------------------------------------------*/
/*!
* @brief Micro-second level delay function
*
* @param u4MicroSec Number of microsecond to delay
*
* @retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalUdelay(IN UINT_32 u4MicroSec)
{
	/* for WinCE 4.2 kernal, NdisStallExecution() will cause a known
	   issue that it will delay over the input value */
	while (u4MicroSec > 50) {
		NdisStallExecution(50);
		u4MicroSec -= 50;
	}

	if (u4MicroSec > 0) {
		NdisStallExecution(u4MicroSec);
	}
}				/* kalUdelay */


/*----------------------------------------------------------------------------*/
/*!
* @brief Milli-second level delay function
*
* @param u4MilliSec Number of millisecond to delay
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalMdelay(IN UINT_32 u4MilliSec)
{
	kalUdelay(u4MilliSec * 1000);
}				/* kalMdelay */


/*----------------------------------------------------------------------------*/
/*!
* @brief Milli-second level sleep function
*
* @param u4MilliSec Number of millisecond to sleep
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalMsleep(IN UINT_32 u4MilliSec)
{
	UINT_32 u4Timestamp[2], u4RemainTime;

	u4RemainTime = u4MilliSec;

	/* NdisMSleep is not accurate and might be sleeping for less than expected */
	while (u4RemainTime > 0) {
		u4Timestamp[0] = SYSTIME_TO_MSEC(kalGetTimeTick());

		NdisMSleep(u4MilliSec);

		u4Timestamp[1] = SYSTIME_TO_MSEC(kalGetTimeTick());

		/* to avoid system tick wraps around */
		if (u4Timestamp[1] >= u4Timestamp[0]) {
			if ((u4Timestamp[1] - u4Timestamp[0]) >= u4RemainTime) {
				break;
			} else {
				u4RemainTime -= (u4Timestamp[1] - u4Timestamp[0]);
			}
		} else {
			if ((u4Timestamp[1] + (UINT_MAX - u4Timestamp[0])) >= u4RemainTime) {
				break;
			} else {
				u4RemainTime -= (u4Timestamp[1] + (UINT_MAX - u4Timestamp[0]));
			}
		}
	}
}				/* kalMsleep */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function sets a given event to the Signaled state if it was not already signaled.
*
* @param prGlueInfo     Pointer of GLUE Data Structure
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID kalSetEvent(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* Set EVENT */
	NdisSetEvent(&prGlueInfo->rTxReqEvent);

}				/* end of kalSetEvent() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Notify Host the Interrupt serive is done, used at Indigo SC32442 SP, the ISR call
*         only one time issue
*
* @param prGlueInfo     Pointer of GLUE Data Structure
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID kalInterruptDone(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

#if SC32442_SPI && 0
	InterruptDone(prGlueInfo->rHifInfo.u4sysIntr);
#endif
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Thread to process TX request and CMD request
*
* @param pvGlueContext     Pointer of GLUE Data Structure
*
* @retval WLAN_STATUS_SUCCESS.
* @retval WLAN_STATUS_FAILURE.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS kalTxServiceThread(IN PVOID pvGlueContext)
{
	NDIS_STATUS ndisStatus = NDIS_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo;
	P_QUE_ENTRY_T prQueueEntry;
	P_QUE_T prTxQue;
	P_QUE_T prCmdQue;
	P_QUE_T prReturnQueue;
	PNDIS_PACKET prNdisPacket;
	P_GL_HIF_INFO_T prHifInfo = NULL;
	BOOLEAN fgNeedHwAccess;
	GLUE_SPIN_LOCK_DECLARATION();


	ASSERT(pvGlueContext);

	prGlueInfo = (P_GLUE_INFO_T) pvGlueContext;
	prTxQue = &prGlueInfo->rTxQueue;
	prCmdQue = &prGlueInfo->rCmdQueue;
	prReturnQueue = &prGlueInfo->rReturnQueue;
	prHifInfo = &prGlueInfo->rHifInfo;

	/* CeSetThreadPriority(GetCurrentThread(),100); */

	DEBUGFUNC("kalTxServiceThread");
/* DBGLOG(INIT, TRACE, ("\n")); */

	KeSetEvent(&prHifInfo->rOidReqEvent, EVENT_INCREMENT, FALSE);

	while (TRUE) {
		if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_HALT)) {
			DBGLOG(INIT, TRACE, ("kalTxServiceThread - GLUE_FLAG_HALT!!\n"));
			break;
		}

		GLUE_WAIT_EVENT(prGlueInfo);
		GLUE_RESET_EVENT(prGlueInfo);

		/* Remove pending OID and TX for RESET */
		if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_RESET)) {
			if (prGlueInfo->i4OidPendingCount) {
				/* Call following function will remove one pending OID */
				wlanReleasePendingOid(prGlueInfo->prAdapter, 0);
			}

			/* Remove pending TX */
			if (prGlueInfo->i4TxPendingFrameNum) {
				kalFlushPendingTxPackets(prGlueInfo);

				wlanFlushTxPendingPackets(prGlueInfo->prAdapter);
			}
			/* Remove pending security frames */
			if (prGlueInfo->i4TxPendingSecurityFrameNum) {
				kalClearSecurityFrames(prGlueInfo);
			}

			if ((!prGlueInfo->i4OidPendingCount) &&
			    (!prGlueInfo->i4TxPendingFrameNum) &&
			    (!prGlueInfo->i4TxPendingSecurityFrameNum)) {

				GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_RESET);

				DBGLOG(INIT, WARN, ("NdisMResetComplete()\n"));
				NdisMResetComplete(prGlueInfo->rMiniportAdapterHandle,
						   NDIS_STATUS_SUCCESS, FALSE);
			}
		}

		fgNeedHwAccess = FALSE;

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
		if (prGlueInfo->fgEnSdioTestPattern == TRUE) {
			if (fgNeedHwAccess == FALSE) {
				fgNeedHwAccess = TRUE;

				wlanAcquirePowerControl(prGlueInfo->prAdapter);
			}

			if (prGlueInfo->fgIsSdioTestInitialized == FALSE) {
				/* enable PRBS mode */
				kalDevRegWrite(prGlueInfo, MCR_WTMCR, 0x00080002);
				prGlueInfo->fgIsSdioTestInitialized = TRUE;
			}

			if (prGlueInfo->fgSdioReadWriteMode == TRUE) {
				/* read test */
				kalDevPortRead(prGlueInfo,
					       MCR_WTMDR,
					       256,
					       prGlueInfo->aucSdioTestBuffer,
					       sizeof(prGlueInfo->aucSdioTestBuffer));
			} else {
				/* write test */
				kalDevPortWrite(prGlueInfo,
						MCR_WTMDR,
						172,
						prGlueInfo->aucSdioTestBuffer,
						sizeof(prGlueInfo->aucSdioTestBuffer));
			}
		}
#endif

		/* Process Mailbox Messages */
		wlanProcessMboxMessage(prGlueInfo->prAdapter);

		/* Process CMD request */
		if (wlanGetAcpiState(prGlueInfo->prAdapter) == ACPI_STATE_D0
		    && prCmdQue->u4NumElem > 0) {
			/* Acquire LP-OWN */
			if (fgNeedHwAccess == FALSE) {
				fgNeedHwAccess = TRUE;

				wlanAcquirePowerControl(prGlueInfo->prAdapter);
			}

			wlanProcessCommandQueue(prGlueInfo->prAdapter, prCmdQue);
		}

		/* Process TX request */
		{
			/* <0> packets from OS handling */
			while (TRUE) {
				GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
				QUEUE_REMOVE_HEAD(prTxQue, prQueueEntry, P_QUE_ENTRY_T);
				GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);

				if (prQueueEntry == NULL) {
					break;
				}

				prNdisPacket = GLUE_GET_PKT_DESCRIPTOR(prQueueEntry);

				if (wlanEnqueueTxPacket(prGlueInfo->prAdapter,
							(P_NATIVE_PACKET) prNdisPacket) ==
				    WLAN_STATUS_RESOURCES) {
					GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
					QUEUE_INSERT_HEAD(prTxQue, prQueueEntry);
					GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);

					break;
				}
			}

			if (wlanGetTxPendingFrameCount(prGlueInfo->prAdapter) > 0) {
				wlanTxPendingPackets(prGlueInfo->prAdapter, &fgNeedHwAccess);
			}
		}

		/* Process RX request */
		{
			UINT_32 u4RegValue;
			UINT_32 u4CurrAvailFreeRfbCnt;

			while (TRUE) {

				GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_RX_RETURN_QUE);
				QUEUE_REMOVE_HEAD(prReturnQueue, prQueueEntry, P_QUE_ENTRY_T);
				GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_RX_RETURN_QUE);

				if (prQueueEntry == NULL) {
					break;
				}

				prNdisPacket = MP_GET_MR_PKT(prQueueEntry);

				wlanReturnPacket(prGlueInfo->prAdapter, (PVOID) prNdisPacket);
			}
		}

		/* Process OID request */
		{
			P_WLAN_REQ_ENTRY prEntry;
			UINT_32 u4QueryInfoLen;
			UINT_32 u4SetInfoLen;
			KIRQL rOldIrql;
			WLAN_STATUS rStatus;

			if (_InterlockedAnd(&prGlueInfo->rHifInfo.u4ReqFlag, ~REQ_FLAG_OID) &
			    REQ_FLAG_OID) {

				prEntry = (P_WLAN_REQ_ENTRY) prGlueInfo->pvOidEntry;

				if (prGlueInfo->fgSetOid) {
					if (prGlueInfo->fgIsGlueExtension) {
						rStatus = prEntry->pfOidSetHandler(prGlueInfo,
										   prGlueInfo->
										   pvInformationBuffer,
										   prGlueInfo->
										   u4InformationBufferLength,
										   &u4SetInfoLen);
					} else {
						rStatus = wlanSetInformation(prGlueInfo->prAdapter,
									     prEntry->
									     pfOidSetHandler,
									     prGlueInfo->
									     pvInformationBuffer,
									     prGlueInfo->
									     u4InformationBufferLength,
									     &u4SetInfoLen);
					}

					/* 20091012 George */
					/* it seems original code does not properly handle the case
					 * which there's command/ response to be handled within OID handler.
					 * it should wait until command/ response done and then indicate completed to NDIS.
					 *
					 */
					if (prGlueInfo->pu4BytesReadOrWritten)
						*prGlueInfo->pu4BytesReadOrWritten = u4SetInfoLen;

					if (prGlueInfo->pu4BytesNeeded)
						*prGlueInfo->pu4BytesNeeded = u4SetInfoLen;

					if (rStatus != WLAN_STATUS_PENDING) {
						KeRaiseIrql(DISPATCH_LEVEL, &rOldIrql);


						GLUE_DEC_REF_CNT(prGlueInfo->i4OidPendingCount);
						ASSERT(prGlueInfo->i4OidPendingCount == 0);

						NdisMSetInformationComplete(prGlueInfo->
									    rMiniportAdapterHandle,
									    rStatus);
						KeLowerIrql(rOldIrql);
					} else {
						wlanoidTimeoutCheck(prGlueInfo->prAdapter,
								    prEntry->pfOidSetHandler);
					}
				} else {
					if (prGlueInfo->fgIsGlueExtension) {
						rStatus = prEntry->pfOidQueryHandler(prGlueInfo,
										     prGlueInfo->
										     pvInformationBuffer,
										     prGlueInfo->
										     u4InformationBufferLength,
										     &u4QueryInfoLen);
					} else {
						rStatus =
						    wlanQueryInformation(prGlueInfo->prAdapter,
									 prEntry->pfOidQueryHandler,
									 prGlueInfo->
									 pvInformationBuffer,
									 prGlueInfo->
									 u4InformationBufferLength,
									 &u4QueryInfoLen);
					}

					if (prGlueInfo->pu4BytesReadOrWritten) {
						/* This case is added to solve the problem of both 32-bit
						 * and 64-bit counters supported for the general
						 * statistics OIDs. */
						if (u4QueryInfoLen >
						    prGlueInfo->u4InformationBufferLength) {
							*prGlueInfo->pu4BytesReadOrWritten =
							    prGlueInfo->u4InformationBufferLength;
						} else {
							*prGlueInfo->pu4BytesReadOrWritten =
							    u4QueryInfoLen;
						}
					}

					if (prGlueInfo->pu4BytesNeeded)
						*prGlueInfo->pu4BytesNeeded = u4QueryInfoLen;

					if (rStatus != WLAN_STATUS_PENDING) {
						KeRaiseIrql(DISPATCH_LEVEL, &rOldIrql);


						GLUE_DEC_REF_CNT(prGlueInfo->i4OidPendingCount);
						ASSERT(prGlueInfo->i4OidPendingCount == 0);

						NdisMQueryInformationComplete(prGlueInfo->
									      rMiniportAdapterHandle,
									      rStatus);
						KeLowerIrql(rOldIrql);
					} else {
						wlanoidTimeoutCheck(prGlueInfo->prAdapter,
								    prEntry->pfOidSetHandler);
					}
				}
			}
		}

		/* TODO(Kevin): Should change to test & reset */
		if (_InterlockedAnd(&prGlueInfo->rHifInfo.u4ReqFlag, ~REQ_FLAG_INT) & REQ_FLAG_INT) {
			if (wlanGetAcpiState(prGlueInfo->prAdapter) == ACPI_STATE_D0) {
				/* Acquire LP-OWN */
				if (fgNeedHwAccess == FALSE) {
					fgNeedHwAccess = TRUE;

					wlanAcquirePowerControl(prGlueInfo->prAdapter);
				}

				wlanIST(prGlueInfo->prAdapter);
			}
		}

		if (wlanGetAcpiState(prGlueInfo->prAdapter) == ACPI_STATE_D0
		    && fgNeedHwAccess == TRUE) {
			wlanReleasePowerControl(prGlueInfo->prAdapter);
		}

		/* Do Reset if no pending OID and TX */
		if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_RESET) &&
		    (!prGlueInfo->i4OidPendingCount) &&
		    (!prGlueInfo->i4TxPendingFrameNum) &&
		    (!prGlueInfo->i4TxPendingSecurityFrameNum)) {

			GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_RESET);

			DBGLOG(INIT, WARN, ("NdisMResetComplete()\n"));
			NdisMResetComplete(prGlueInfo->rMiniportAdapterHandle,
					   NDIS_STATUS_SUCCESS, FALSE);
		}

		if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_TIMEOUT)) {
			GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_TIMEOUT);

			wlanTimerTimeoutCheck(prGlueInfo->prAdapter);
		}
#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
		if (prGlueInfo->fgEnSdioTestPattern == TRUE) {
			GLUE_SET_EVENT(prGlueInfo);
		}
#endif
	}

	while (TRUE) {
		GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_RX_RETURN_QUE);
		QUEUE_REMOVE_HEAD(prReturnQueue, prQueueEntry, P_QUE_ENTRY_T);
		GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_RX_RETURN_QUE);

		if (prQueueEntry == NULL) {

			if (prGlueInfo->i4RxPendingFrameNum) {
				/* DbgPrint("Wait for RX return, left = %d\n", prGlueInfo->u4RxPendingFrameNum); */
				NdisMSleep(100000);	/* Sleep 100ms for remaining RX Packets */
			} else {
				/* No pending RX Packet */
				break;
			}
		} else {
			prNdisPacket = MP_GET_MR_PKT(prQueueEntry);

			wlanReturnPacket(prGlueInfo->prAdapter, (PVOID) prNdisPacket);
		}
	}

	return ndisStatus;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Download the patch file
*
* @param
*
* @retval WLAN_STATUS_SUCCESS: SUCCESS
* @retval WLAN_STATUS_FAILURE: FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS kalDownloadPatch(IN PNDIS_STRING FileName)
{
	NDIS_STATUS rStatus;
	NDIS_HANDLE FileHandle;
	UINT_32 u4FileLength;
	NDIS_PHYSICAL_ADDRESS HighestAcceptableAddress = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
	PUINT_8 pucMappedBuffer;

	/* Open the patch file. */
	NdisOpenFile(&rStatus, &FileHandle, &u4FileLength, FileName, HighestAcceptableAddress);

	if (rStatus == NDIS_STATUS_SUCCESS) {
		/* Map the firmware file to memory. */
		NdisMapFile(&rStatus, &pucMappedBuffer, FileHandle);

		if (rStatus == NDIS_STATUS_SUCCESS) {
			/* Issue the download CMD */
		}

		NdisCloseFile(FileHandle);
	}

	return (rStatus);
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Thread to process TX request and CMD request
*
* @param pvGlueContext     Pointer of GLUE Data Structure
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
kalOidComplete(IN P_GLUE_INFO_T prGlueInfo,
	       IN BOOLEAN fgSetQuery, IN UINT_32 u4SetQueryInfoLen, IN WLAN_STATUS rOidStatus)
{
	/* KIRQL rOldIrql; */

	ASSERT(prGlueInfo);
	ASSERT((((fgSetQuery) && (prGlueInfo->fgSetOid)) ||
		((!fgSetQuery) && (!prGlueInfo->fgSetOid))));

	/* Remove Timeout check timer */
	wlanoidClearTimeoutCheck(prGlueInfo->prAdapter);

	/* NOTE(Kevin): We should update reference count before do NdisMSet/QueryComplete()
	 * Because OS may issue new OID request when we call NdisMSet/QueryComplete().
	 */
	GLUE_DEC_REF_CNT(prGlueInfo->i4OidPendingCount);
	ASSERT(prGlueInfo->i4OidPendingCount == 0);

	/* KeRaiseIrql(DISPATCH_LEVEL, &rOldIrql); */
	if (prGlueInfo->fgSetOid) {
		if (prGlueInfo->pu4BytesReadOrWritten) {
			*prGlueInfo->pu4BytesReadOrWritten = u4SetQueryInfoLen;
		}

		if (prGlueInfo->pu4BytesNeeded) {
			*prGlueInfo->pu4BytesNeeded = u4SetQueryInfoLen;
		}

		NdisMSetInformationComplete(prGlueInfo->rMiniportAdapterHandle, rOidStatus);
	} else {
		/* This case is added to solve the problem of both 32-bit
		   and 64-bit counters supported for the general
		   statistics OIDs. */
		if (u4SetQueryInfoLen > prGlueInfo->u4InformationBufferLength) {
			u4SetQueryInfoLen = prGlueInfo->u4InformationBufferLength;
		}

		if (prGlueInfo->pu4BytesReadOrWritten) {
			*prGlueInfo->pu4BytesReadOrWritten = u4SetQueryInfoLen;
		}

		if (prGlueInfo->pu4BytesNeeded) {
			*prGlueInfo->pu4BytesNeeded = u4SetQueryInfoLen;
		}

		NdisMQueryInformationComplete(prGlueInfo->rMiniportAdapterHandle, rOidStatus);
	}
	/* KeLowerIrql(rOldIrql); */

	return;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This inline function is to extract some packet information, including
*        user priority, packet length, destination address, 802.1x and BT over Wi-Fi
*        or not.
*
* @param prGlueInfo         Pointer to the glue structure
* @param prNdisPacket       Packet descriptor
* @param pucPriorityParam   User priority
* @param pu4PacketLen       Packet length
* @param pucEthDestAddr     Destination address
* @param pfgIs1X            802.1x packet or not
* @param pfgIsPAL           BT over Wi-Fi packet or not
* @param pfgIs802_3         802.3 format packet or not
* @param pfgIsVlanExists    VLAN tagged packet or not
*
* @retval TRUE      Success to extract information
* @retval FALSE     Fail to extract correct information
*/
/*----------------------------------------------------------------------------*/
BOOL
kalQoSFrameClassifierAndPacketInfo(IN P_GLUE_INFO_T prGlueInfo,
				   IN P_NATIVE_PACKET prPacket,
				   OUT PUINT_8 pucPriorityParam,
				   OUT PUINT_32 pu4PacketLen,
				   OUT PUINT_8 pucEthDestAddr,
				   OUT PBOOLEAN pfgIs1X,
				   OUT PBOOLEAN pfgIsPAL,
				   OUT PBOOLEAN pfgIs802_3, OUT PBOOLEAN pfgIsVlanExists)
{
	PNDIS_BUFFER prNdisBuffer;
	UINT_32 u4PacketLen;

	UINT_8 aucLookAheadBuf[LOOK_AHEAD_LEN];
	UINT_32 u4ByteCount = 0;
	UINT_8 ucUserPriority = 0;	/* Default, normally glue layer cannot see into non-WLAN header files QM_DEFAULT_USER_PRIORITY; */
	UINT_8 ucLookAheadLen;
	UINT_16 u2EtherTypeLen;
	PNDIS_PACKET prNdisPacket = (PNDIS_PACKET) prPacket;

	DEBUGFUNC("kalQoSFrameClassifierAndPacketInfo");

	/* 4 <1> Find out all buffer information */
	NdisQueryPacket(prNdisPacket, NULL, NULL, &prNdisBuffer, &u4PacketLen);

	if (u4PacketLen < ETHER_HEADER_LEN) {
		DBGLOG(INIT, WARN, ("Invalid Ether packet length: %d\n", u4PacketLen));
		return FALSE;
	}

	if (u4PacketLen < LOOK_AHEAD_LEN) {
		ucLookAheadLen = (UINT_8) u4PacketLen;
	} else {
		ucLookAheadLen = LOOK_AHEAD_LEN;
	}

	/* 4 <2> Copy partial frame to local LOOK AHEAD Buffer */
	while (prNdisBuffer && u4ByteCount < ucLookAheadLen) {
		PVOID pvAddr;
		UINT_32 u4Len;
		PNDIS_BUFFER prNextNdisBuffer;

#ifdef NDIS51_MINIPORT
		NdisQueryBufferSafe(prNdisBuffer, &pvAddr, &u4Len, HighPagePriority);
#else
		NdisQueryBuffer(prNdisBuffer, &pvAddr, &u4Len);
#endif

		if (pvAddr == (PVOID) NULL) {
			ASSERT(0);
			return FALSE;
		}

		if ((u4ByteCount + u4Len) >= ucLookAheadLen) {
			kalMemCopy(&aucLookAheadBuf[u4ByteCount],
				   pvAddr, (ucLookAheadLen - u4ByteCount));
			break;
		} else {
			kalMemCopy(&aucLookAheadBuf[u4ByteCount], pvAddr, u4Len);
		}
		u4ByteCount += u4Len;

		NdisGetNextBuffer(prNdisBuffer, &prNextNdisBuffer);

		prNdisBuffer = prNextNdisBuffer;
	}

	*pfgIs1X = FALSE;
	*pfgIsPAL = FALSE;
	*pfgIsVlanExists = FALSE;
	*pfgIs802_3 = FALSE;

	/* 4 TODO: Check VLAN Tagging */

	/* 4 <3> Obtain the User Priority for WMM */
	u2EtherTypeLen = NTOHS(*(PUINT_16) & aucLookAheadBuf[ETHER_TYPE_LEN_OFFSET]);

	if ((u2EtherTypeLen == ETH_P_IPV4) && (u4PacketLen >= LOOK_AHEAD_LEN)) {
		PUINT_8 pucIpHdr = &aucLookAheadBuf[ETHER_HEADER_LEN];
		UINT_8 ucIpVersion;


		ucIpVersion = (pucIpHdr[0] & IP_VERSION_MASK) >> IP_VERSION_OFFSET;

		if (ucIpVersion == IP_VERSION_4) {
			UINT_8 ucIpTos;


			/* Get the DSCP value from the header of IP packet. */
			ucIpTos = pucIpHdr[1];
			ucUserPriority =
			    ((ucIpTos & IPV4_HDR_TOS_PREC_MASK) >> IPV4_HDR_TOS_PREC_OFFSET);
		}

		/* TODO(Kevin): Add TSPEC classifier here */
	} else if (u2EtherTypeLen == ETH_P_1X) {	/* For Port Control */
		/* DBGLOG(REQ, TRACE, ("Tx 1x\n")); */
		*pfgIs1X = TRUE;
	} else if (u2EtherTypeLen == ETH_P_PRE_1X) {	/* For Port Control */
		/* DBGLOG(REQ, TRACE, ("Tx 1x\n")); */
		*pfgIs1X = TRUE;
	}
#if CFG_SUPPORT_WAPI
	else if (u2EtherTypeLen == ETH_WPI_1X) {
		*pfgIs1X = TRUE;
	}
#endif
	else if (u2EtherTypeLen <= 1500) {	/* 802.3 Frame */
		UINT_8 ucDSAP, ucSSAP, ucControl;
		UINT_8 aucOUI[3];

		*pfgIs802_3 = TRUE;

		ucDSAP = *(PUINT_8) &aucLookAheadBuf[ETH_LLC_OFFSET];
		ucSSAP = *(PUINT_8) &aucLookAheadBuf[ETH_LLC_OFFSET + 1];
		ucControl = *(PUINT_8) &aucLookAheadBuf[ETH_LLC_OFFSET + 2];

		aucOUI[0] = *(PUINT_8) &aucLookAheadBuf[ETH_SNAP_OFFSET];
		aucOUI[1] = *(PUINT_8) &aucLookAheadBuf[ETH_SNAP_OFFSET + 1];
		aucOUI[2] = *(PUINT_8) &aucLookAheadBuf[ETH_SNAP_OFFSET + 2];

		if (ucDSAP == ETH_LLC_DSAP_SNAP &&
		    ucSSAP == ETH_LLC_SSAP_SNAP &&
		    ucControl == ETH_LLC_CONTROL_UNNUMBERED_INFORMATION &&
		    aucOUI[0] == ETH_SNAP_BT_SIG_OUI_0 &&
		    aucOUI[1] == ETH_SNAP_BT_SIG_OUI_1 && aucOUI[2] == ETH_SNAP_BT_SIG_OUI_2) {
			*pfgIsPAL = TRUE;

			/* check if Security Frame */
			if ((*(PUINT_8) &aucLookAheadBuf[ETH_SNAP_OFFSET + 3]) ==
			    ((BOW_PROTOCOL_ID_SECURITY_FRAME & 0xFF00) >> 8)
			    && (*(PUINT_8) &aucLookAheadBuf[ETH_SNAP_OFFSET + 4]) ==
			    (BOW_PROTOCOL_ID_SECURITY_FRAME & 0xFF)) {
				*pfgIs1X = TRUE;
			}
		}
	}
	/* 4 <4> Return the value of Priority Parameter. */
	*pucPriorityParam = ucUserPriority;

	/* 4 <5> Retrieve Packet Information - DA */
	/* Packet Length/ Destination Address */
	*pu4PacketLen = u4PacketLen;
	kalMemCopy(pucEthDestAddr, aucLookAheadBuf, PARAM_MAC_ADDR_LEN);

	return TRUE;

}				/* end of kalQoSFrameClassifier() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is the callback function of master timer
*
* \param[in] systemSpecific1
* \param[in] miniportAdapterContext Pointer to GLUE Data Structure
* \param[in] systemSpecific2
* \param[in] systemSpecific3
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID
kalTimerEvent(IN PVOID systemSpecific1,
	      IN PVOID miniportAdapterContext, IN PVOID systemSpecific2, IN PVOID systemSpecific3)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;
	PFN_TIMER_CALLBACK pfTimerFunc = (PFN_TIMER_CALLBACK) prGlueInfo->pvTimerFunc;

	GLUE_SPIN_LOCK_DECLARATION();

	pfTimerFunc(prGlueInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Timer Initialization Procedure
*
* \param[in] prGlueInfo     Pointer to GLUE Data Structure
* \param[in] prTimerHandler Pointer to timer handling function, whose only
*                           argument is "prAdapter"
*
* \retval none
*
*/
/*----------------------------------------------------------------------------*/
VOID kalOsTimerInitialize(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prTimerHandler)
{
	prGlueInfo->pvTimerFunc = prTimerHandler;

	/* Setup master timer. This master timer is the root timer for following
	 * management timers.
	 * Note: NdisMInitializeTimer() only could be called after
	 * NdisMSetAttributesEx() */
	NdisMInitializeTimer(&prGlueInfo->rMasterTimer,
			     prGlueInfo->rMiniportAdapterHandle,
			     (PNDIS_TIMER_FUNCTION) kalTimerEvent, (PVOID) prGlueInfo);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the time to do the time out check.
*
* \param[in] prGlueInfo Pointer to GLUE Data Structure
* \param[in] rInterval  Time out interval from current time.
*
* \retval TRUE Success.
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalSetTimer(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Interval)
{
	ASSERT(prGlueInfo);

	NdisMSetTimer(&prGlueInfo->rMasterTimer, u4Interval);
	return TRUE;		/* success */
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to cancel
*
* \param[in] prGlueInfo Pointer to GLUE Data Structure
*
* \retval TRUE  :   Timer has been canceled
*         FALAE :   Timer doens't exist
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalCancelTimer(IN P_GLUE_INFO_T prGlueInfo)
{
	BOOLEAN fgTimerCancelled;

	ASSERT(prGlueInfo);

	GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_TIMEOUT);

	NdisMCancelTimer(&prGlueInfo->rMasterTimer, &fgTimerCancelled);

	return fgTimerCancelled;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief command timeout call-back function
 *
 * \param[in] prGlueInfo Pointer to the GLUE data structure.
 *
 * \retval (none)
 */
/*----------------------------------------------------------------------------*/
VOID kalTimeoutHandler(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_TIMEOUT);

	/* Notify TxServiceThread for timeout event */
	GLUE_SET_EVENT(prGlueInfo);

	return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear pending OID queued in pvOidEntry
*        (after windowsSetInformation() and before gl_kal_common.c processing)
*
* \param pvGlueInfo Pointer of GLUE Data Structure

* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalOidClearance(IN P_GLUE_INFO_T prGlueInfo)
{
	P_WLAN_REQ_ENTRY pvEntry;

	ASSERT(prGlueInfo);

	if (_InterlockedAnd(&prGlueInfo->rHifInfo.u4ReqFlag, ~REQ_FLAG_OID) & REQ_FLAG_OID) {
		pvEntry = (P_WLAN_REQ_ENTRY) prGlueInfo->pvOidEntry;

		ASSERT(pvEntry);

		GLUE_DEC_REF_CNT(prGlueInfo->i4OidPendingCount);
		ASSERT(prGlueInfo->i4OidPendingCount == 0);

		if (prGlueInfo->fgSetOid) {
			NdisMSetInformationComplete(prGlueInfo->rMiniportAdapterHandle,
						    NDIS_STATUS_FAILURE);
		} else {
			NdisMQueryInformationComplete(prGlueInfo->rMiniportAdapterHandle,
						      NDIS_STATUS_FAILURE);
		}
	}
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to retrieve overriden netweork address
*
* \param pvGlueInfo Pointer of GLUE Data Structure

* \retval TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalRetrieveNetworkAddress(IN P_GLUE_INFO_T prGlueInfo, IN OUT PARAM_MAC_ADDRESS * prMacAddr)
{
	ASSERT(prGlueInfo);

	if (prGlueInfo->rRegInfo.aucMacAddr[0] & BIT(0)) {	/* invalid MAC address */
		return FALSE;
	} else {
		COPY_MAC_ADDR(prMacAddr, prGlueInfo->rRegInfo.aucMacAddr);

		return TRUE;
	}
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to update netweork address to glue layer
*
* \param pvGlueInfo Pointer of GLUE Data Structure
* \param pucMacAddr Pointer to 6-bytes MAC address
*
* \retval TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
VOID kalUpdateMACAddress(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucMacAddr)
{
	ASSERT(prGlueInfo);
	ASSERT(pucMacAddr);

	/* windows doesn't need to support this routine */
	return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to inform glue layer for address update
*
* \param prGlueInfo Pointer of GLUE Data Structure
* \param rMacAddr   Reference to MAC address
*
* \retval TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
VOID kalUpdateNetworkAddress(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rMacAddr)
{
	ASSERT(prGlueInfo);

	/* windows don't need to handle this */
	return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to load firmware image
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
* \param ppvMapFileBuf  Pointer of pointer to memory-mapped firmware image
* \param pu4FileLength  File length and memory mapped length as well

* \retval Map File Handle, used for unammping
*/
/*----------------------------------------------------------------------------*/
PVOID
kalFirmwareImageMapping(IN P_GLUE_INFO_T prGlueInfo,
			OUT PPVOID ppvMapFileBuf, OUT PUINT_32 pu4FileLength)
{
	NDIS_HANDLE *prFileHandleFwImg;
	NDIS_STRING rFileWifiRam;

	DEBUGFUNC("kalFirmwareImageMapping");

	ASSERT(prGlueInfo);
	ASSERT(ppvMapFileBuf);
	ASSERT(pu4FileLength);

	if ((prFileHandleFwImg = kalMemAlloc(sizeof(NDIS_HANDLE), VIR_MEM_TYPE)) == NULL) {
		DBGLOG(INIT, ERROR, ("Fail to allocate memory for NDIS_HANDLE!\n"));
		return NULL;
	}

	/* Mapping FW image from file */
#if defined(MT6620) && CFG_MULTI_ECOVER_SUPPORT
	if (wlanGetEcoVersion(prGlueInfo->prAdapter) >= 6) {
		NdisInitUnicodeString(&rFileWifiRam, prGlueInfo->rRegInfo.aucFwImgFilenameE6);
	} else {
		NdisInitUnicodeString(&rFileWifiRam, prGlueInfo->rRegInfo.aucFwImgFilename);
	}
#else
	NdisInitUnicodeString(&rFileWifiRam, prGlueInfo->rRegInfo.aucFwImgFilename);
#endif
	if (!imageFileMapping(rFileWifiRam, prFileHandleFwImg, ppvMapFileBuf, pu4FileLength)) {
		DBGLOG(INIT, ERROR, ("Fail to load FW image from file!\n"));
		kalMemFree(prFileHandleFwImg, VIR_MEM_TYPE, sizeof(NDIS_HANDLE));
		return NULL;
	}

	return (PVOID) prFileHandleFwImg;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to unload firmware image mapped memory
*
* \param pvGlueInfo     Pointer of GLUE Data Structure
* \param pvFwHandle     Pointer to mapping handle
* \param pvMapFileBuf   Pointer to memory-mapped firmware image
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID
kalFirmwareImageUnmapping(IN P_GLUE_INFO_T prGlueInfo, IN PVOID prFwHandle, IN PVOID pvMapFileBuf)
{
	NDIS_HANDLE rFileHandleFwImg;

	DEBUGFUNC("kalFirmwareImageUnmapping");

	ASSERT(prGlueInfo);
	ASSERT(prFwHandle);
	ASSERT(pvMapFileBuf);

	rFileHandleFwImg = *(NDIS_HANDLE *) prFwHandle;

	/* unmap */
	imageFileUnMapping(rFileHandleFwImg, pvMapFileBuf);

	/* free handle */
	kalMemFree(prFwHandle, VIR_MEM_TYPE, sizeof(NDIS_HANDLE));

	return;

}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to check if card is removed
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval TRUE:     card is removed
*         FALSE:    card is still attached
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsCardRemoved(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->fgIsCardRemoved;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to flush pending TX packets in glue layer
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval
*/
/*----------------------------------------------------------------------------*/
VOID kalFlushPendingTxPackets(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prTxQue;
	P_QUE_ENTRY_T prQueueEntry;
	PVOID prPacket;

	ASSERT(prGlueInfo);

	prTxQue = &(prGlueInfo->rTxQueue);

	if (prGlueInfo->i4TxPendingFrameNum) {
		while (TRUE) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
			QUEUE_REMOVE_HEAD(prTxQue, prQueueEntry, P_QUE_ENTRY_T);
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);

			if (prQueueEntry == NULL) {
				break;
			}

			prPacket = GLUE_GET_PKT_DESCRIPTOR(prQueueEntry);

			kalSendComplete(prGlueInfo, prPacket, WLAN_STATUS_SUCCESS);
		}
	}
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to retrieve the number of pending TX packets
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetTxPendingFrameCount(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return (UINT_32) (prGlueInfo->i4TxPendingFrameNum);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to retrieve the number of pending commands
*        (including MMPDU, 802.1X and command packets)
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetTxPendingCmdCount(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prCmdQue;

	ASSERT(prGlueInfo);
	prCmdQue = &prGlueInfo->rCmdQueue;

	return prCmdQue->u4NumElem;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is get indicated media state
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
ENUM_PARAM_MEDIA_STATE_T kalGetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->eParamMediaStateIndicated;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to set indicated media state
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID
kalSetMediaStateIndicated(IN P_GLUE_INFO_T prGlueInfo,
			  IN ENUM_PARAM_MEDIA_STATE_T eParamMediaStateIndicate)
{
	ASSERT(prGlueInfo);

	prGlueInfo->eParamMediaStateIndicated = eParamMediaStateIndicate;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to get firmware load address from registry
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetFwLoadAddress(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->rRegInfo.u4LoadAddress;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to get firmware start address from registry
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetFwStartAddress(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return prGlueInfo->rRegInfo.u4StartAddress;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear pending OID staying in command queue
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalOidCmdClearance(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	ASSERT(prGlueInfo);

	/* Clear pending OID in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {

		if (((P_CMD_INFO_T) prQueueEntry)->fgIsOid) {
			prCmdInfo = (P_CMD_INFO_T) prQueueEntry;
			break;
		} else {
			QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	QUEUE_CONCATENATE_QUEUES(prCmdQue, prTempCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	if (prCmdInfo) {
		if (prCmdInfo->pfCmdTimeoutHandler) {
			prCmdInfo->pfCmdTimeoutHandler(prGlueInfo->prAdapter, prCmdInfo);
		} else {
			kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
		}

		cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
	}
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to insert command into prCmdQueue
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*        prQueueEntry   Pointer of queue entry to be inserted
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalEnqueueCommand(IN P_GLUE_INFO_T prGlueInfo, IN P_QUE_ENTRY_T prQueueEntry)
{
	P_QUE_T prCmdQue;

	ASSERT(prGlueInfo);
	ASSERT(prQueueEntry);

	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all pending security frames
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalClearSecurityFrames(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue, rReturnCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue, prReturnCmdQue = &rReturnCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;

	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending security frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME) {
			if (prCmdInfo->pfCmdTimeoutHandler) {
				prCmdInfo->pfCmdTimeoutHandler(prGlueInfo->prAdapter, prCmdInfo);
			} else {
				wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo,
						   TX_RESULT_QUEUE_CLEARANCE);
			}
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	/* insert return queue back to cmd queue */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prReturnCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear pending security frames
*        belongs to dedicated network type
*
* \param prGlueInfo         Pointer of GLUE Data Structure
* \param eNetworkTypeIdx    Network Type Index
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalClearSecurityFramesByBssIdx(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue, rReturnCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue, prReturnCmdQue = &rReturnCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;

	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending security frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME &&
		    prCmdInfo->ucBssIndex == ucBssIndex) {
			if (prCmdInfo->pfCmdTimeoutHandler) {
				prCmdInfo->pfCmdTimeoutHandler(prGlueInfo->prAdapter, prCmdInfo);
			} else {
				wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo,
						   TX_RESULT_QUEUE_CLEARANCE);
			}
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	/* insert return queue back to cmd queue */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prReturnCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all pending management frames
*
* \param prGlueInfo     Pointer of GLUE Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalClearMgmtFrames(IN P_GLUE_INFO_T prGlueInfo)
{
	P_QUE_T prCmdQue;
	QUE_T rTempCmdQue, rReturnCmdQue;
	P_QUE_T prTempCmdQue = &rTempCmdQue, prReturnCmdQue = &rReturnCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;

	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

	ASSERT(prGlueInfo);

	QUEUE_INITIALIZE(prReturnCmdQue);
	/* Clear pending management frames in prGlueInfo->rCmdQueue */
	prCmdQue = &prGlueInfo->rCmdQueue;

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME) {
			wlanReleaseCommand(prGlueInfo->prAdapter, prCmdInfo,
					   TX_RESULT_QUEUE_CLEARANCE);
			cmdBufFreeCmdInfo(prGlueInfo->prAdapter, prCmdInfo);
		} else {
			QUEUE_INSERT_TAIL(prReturnCmdQue, prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	}

	/* insert return queue back to cmd queue */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES_HEAD(prCmdQue, prReturnCmdQue);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_CMD_QUE);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to generate a random number
*
* \param none
*
* \retval UINT_32
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalRandomNumber(VOID)
{
	UINT_32 u4Seed;

	u4Seed = kalGetTimeTick();

	return RtlRandomEx(&u4Seed);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is not used by NDIS
*
* \param prGlueInfo     Pointer of GLUE_INFO_T
*        eNetTypeIdx    Index of Network Type
*        rStatus        Status
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID
kalScanDone(IN P_GLUE_INFO_T prGlueInfo,
	    IN ENUM_KAL_NETWORK_TYPE_INDEX_T eNetTypeIdx, IN WLAN_STATUS rStatus)
{
	ASSERT(prGlueInfo);

	/* check for system configuration for generating error message on scan list */
	wlanCheckSystemConfiguration(prGlueInfo->prAdapter);

	return;
}


#if CFG_ENABLE_BT_OVER_WIFI
/*----------------------------------------------------------------------------*/
/*!
* \brief to indicate event for Bluetooth over Wi-Fi
*
* \param[in]
*           prGlueInfo
*           prEvent
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID kalIndicateBOWEvent(IN P_GLUE_INFO_T prGlueInfo, IN P_AMPC_EVENT prEvent)
{
	ASSERT(prGlueInfo);

	/* BT 3.0 + HS not implemented */

	return;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi state from glue layer
*
* \param[in]
*           prGlueInfo
* \return
*           ENUM_BOW_DEVICE_STATE
*/
/*----------------------------------------------------------------------------*/
ENUM_BOW_DEVICE_STATE kalGetBowState(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr)
{
	ASSERT(prGlueInfo);

	/* BT 3.0 + HS not implemented */

	return BOW_DEVICE_STATE_DISCONNECTED;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to set Bluetooth-over-Wi-Fi state in glue layer
*
* \param[in]
*           prGlueInfo
*           eBowState
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID
kalSetBowState(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_BOW_DEVICE_STATE eBowState, IN PARAM_MAC_ADDRESS rPeerAddr)
{
	ASSERT(prGlueInfo);

	/* BT 3.0 + HS not implemented */

	return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi global state
*
* \param[in]
*           prGlueInfo
*
* \return
*           BOW_DEVICE_STATE_DISCONNECTED
*               in case there is no BoW connection or
*               BoW connection under initialization
*
*           BOW_DEVICE_STATE_STARTING
*               in case there is no BoW connection but
*               some BoW connection under initialization
*
*           BOW_DEVICE_STATE_CONNECTED
*               in case there is any BoW connection available
*/
/*----------------------------------------------------------------------------*/
ENUM_BOW_DEVICE_STATE kalGetBowGlobalState(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* BT 3.0 + HS not implemented */

	return BOW_DEVICE_STATE_DISCONNECTED;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi operating frequency
*
* \param[in]
*           prGlueInfo
*
* \return
*           in unit of KHz
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetBowFreqInKHz(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* BT 3.0 + HS not implemented */

	return 0;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi role
*
* \param[in]
*           prGlueInfo
*
* \return
*           0: Responder
*           1: Initiator
*/
/*----------------------------------------------------------------------------*/
UINT_8 kalGetBowRole(IN P_GLUE_INFO_T prGlueInfo, IN PARAM_MAC_ADDRESS rPeerAddr)
{
	ASSERT(prGlueInfo);

	/* BT 3.0 + HS not implemented */

	return 0;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to set Bluetooth-over-Wi-Fi role
*
* \param[in]
*           prGlueInfo
*           ucRole
*                   0: Responder
*                   1: Initiator
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID kalSetBowRole(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucRole, IN PARAM_MAC_ADDRESS rPeerAddr)
{
	ASSERT(prGlueInfo);

	/* BT 3.0 + HS not implemented */

	return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to get available Bluetooth-over-Wi-Fi physical link number
*
* \param[in]
*           prGlueInfo
* \return
*           UINT_32
*               how many physical links are aviailable
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalGetBowAvailablePhysicalLinkCount(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* BT 3.0 + HS not implemented */

	return 0;
}
#endif

#if CFG_ENABLE_WIFI_DIRECT
/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Wi-Fi Direct state from glue layer
*
* \param[in]
*           prGlueInfo
*           rPeerAddr
* \return
*           ENUM_BOW_DEVICE_STATE
*/
/*----------------------------------------------------------------------------*/
ENUM_PARAM_MEDIA_STATE_T kalP2PGetState(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */

	return PARAM_MEDIA_STATE_DISCONNECTED;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to set Wi-Fi Direct state in glue layer
*
* \param[in]
*           prGlueInfo
*           eBowState
*           rPeerAddr
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID
kalP2PSetState(IN P_GLUE_INFO_T prGlueInfo,
	       IN ENUM_PARAM_MEDIA_STATE_T eState, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucRole)
{
	ASSERT(prGlueInfo);

	switch (eState) {
	case PARAM_MEDIA_STATE_CONNECTED:
		/* TODO: indicate IWEVP2PSTACNT */
		break;

	case PARAM_MEDIA_STATE_DISCONNECTED:
		/* TODO: indicate IWEVP2PSTADISCNT */
		break;

	default:
		ASSERT(0);
		break;
	}
	/* Wi-Fi Direct not implemented yet */

	return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Wi-Fi Direct operating frequency
*
* \param[in]
*           prGlueInfo
*
* \return
*           in unit of KHz
*/
/*----------------------------------------------------------------------------*/
UINT_32 kalP2PGetFreqInKHz(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */

	return 0;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Bluetooth-over-Wi-Fi role
*
* \param[in]
*           prGlueInfo
*
* \return
*           0: P2P Device
*           1: Group Client
*           2: Group Owner
*/
/*----------------------------------------------------------------------------*/
UINT_8 kalP2PGetRole(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */

	return 0;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to set Wi-Fi Direct role
*
* \param[in]
*           prGlueInfo
*           ucResult
*                   0: successful
*                   1: error
*           ucRole
*                   0: P2P Device
*                   1: Group Client
*                   2: Group Owner
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID
kalP2PSetRole(IN P_GLUE_INFO_T prGlueInfo,
	      IN UINT_8 ucResult, IN PUINT_8 pucSSID, IN UINT_8 ucSSIDLen, IN UINT_8 ucRole)
{
	ASSERT(prGlueInfo);
	ASSERT(ucRole <= 2);

	/* Wi-Fi Direct not implemented yet */

	return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief indicate an event to supplicant for device found
*
* \param[in] prGlueInfo Pointer of GLUE_INFO_T
*
* \retval TRUE  Success.
* \retval FALSE Failure
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PIndicateFound(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */
	/* TODO: indicate IWEVP2PDVCFND event */

	return FALSE;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief indicate an event to supplicant for device connection request
*
* \param[in] prGlueInfo Pointer of GLUE_INFO_T
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PIndicateConnReq(IN P_GLUE_INFO_T prGlueInfo, IN PUINT_8 pucDevName, IN INT_32 u4NameLength, IN PARAM_MAC_ADDRESS rPeerAddr, IN UINT_8 ucDevType,	/* 0: P2P Device / 1: GC / 2: GO */
			   IN INT_32 i4ConfigMethod, IN INT_32 i4ActiveConfigMethod)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */
	/* TODO: indicate IWEVP2PDVCRQ event */

	return;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief indicate an event to supplicant for device invitation request
*
* \param[in] prGlueInfo Pointer of GLUE_INFO_T
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID
kalP2PInvitationIndication(IN P_GLUE_INFO_T prGlueInfo,
			   IN P_P2P_DEVICE_DESC_T prP2pDevDesc,
			   IN PUINT_8 pucSsid,
			   IN UINT_8 ucSsidLen,
			   IN UINT_8 ucOperatingChnl, IN UINT_8 ucInvitationType)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */
	/* TODO: indicate IWEVP2PDVCRQ event */

	return;
}



/*----------------------------------------------------------------------------*/
/*!
* \brief to set the cipher for p2p
*
* \param[in]
*           prGlueInfo
*           u4Cipher
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PSetCipher(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Cipher)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */
	return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to get the cipher, return for cipher is ccmp
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE: cipher is ccmp
*           FALSE: cipher is none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PGetCipher(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */
	return FALSE;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to get the cipher, return for cipher is ccmp
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE: cipher is ccmp
*           FALSE: cipher is none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PGetTkipCipher(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */
	return FALSE;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to get the cipher, return for cipher is ccmp
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE: cipher is ccmp
*           FALSE: cipher is none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalP2PGetCcmpCipher(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */
	return FALSE;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to get the wsc ie length
*
* \param[in]
*           prGlueInfo
*
* \return
*           The WPS IE length
*/
/*----------------------------------------------------------------------------*/
UINT_16 kalP2PCalWSC_IELen(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */

	return 0;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to copy the wsc ie setting from p2p supplicant
*
* \param[in]
*           prGlueInfo
*
* \return
*           The WPS IE length
*/
/*----------------------------------------------------------------------------*/
VOID kalP2PGenWSC_IE(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucType, IN PUINT_8 pucBuffer)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to update the assoc req to p2p
*
* \param[in]
*           prGlueInfo
*           eBowState
*           rPeerAddr
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID
kalP2PUpdateAssocInfo(IN P_GLUE_INFO_T prGlueInfo,
		      IN PUINT_8 pucFrameBody,
		      IN UINT_32 u4FrameBodyLen, IN BOOLEAN fgReassocRequest)
{
	ASSERT(prGlueInfo);

	/* Wi-Fi Direct not implemented yet */
}
#endif


/*----------------------------------------------------------------------------*/
/*!
* \brief to check if configuration file (NVRAM/Registry) exists
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsConfigurationExist(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	/* for windows platform, always return TRUE */
	return TRUE;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve Registry information
*
* \param[in]
*           prGlueInfo
*
* \return
*           Pointer of REG_INFO_T
*/
/*----------------------------------------------------------------------------*/
P_REG_INFO_T kalGetConfiguration(IN P_GLUE_INFO_T prGlueInfo)
{
	ASSERT(prGlueInfo);

	return &(prGlueInfo->rRegInfo);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief to retrieve version information of corresponding configuration file
*
* \param[in]
*           prGlueInfo
*
* \param[out]
*           pu2Part1CfgOwnVersion
*           pu2Part1CfgPeerVersion
*           pu2Part2CfgOwnVersion
*           pu2Part2CfgPeerVersion
*
* \return
*           NONE
*/
/*----------------------------------------------------------------------------*/
VOID
kalGetConfigurationVersion(IN P_GLUE_INFO_T prGlueInfo,
			   OUT PUINT_16 pu2Part1CfgOwnVersion,
			   OUT PUINT_16 pu2Part1CfgPeerVersion,
			   OUT PUINT_16 pu2Part2CfgOwnVersion, OUT PUINT_16 pu2Part2CfgPeerVersion)
{
	ASSERT(prGlueInfo);

	ASSERT(pu2Part1CfgOwnVersion);
	ASSERT(pu2Part1CfgPeerVersion);
	ASSERT(pu2Part2CfgOwnVersion);
	ASSERT(pu2Part2CfgPeerVersion);

	/* Windows uses registry instead, */
	/* and we'll always have a default value to use if */
	/* the registry entry doesn't exist, so we use UINT16_MAX / 0 pair */
	/* as version information to keep maximum compatibility */

	*pu2Part1CfgOwnVersion = UINT16_MAX;
	*pu2Part1CfgPeerVersion = 0;

	*pu2Part2CfgOwnVersion = UINT16_MAX;
	*pu2Part2CfgPeerVersion = 0;

	return;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief API for reading data on NVRAM
*
* \param[in]
*           prGlueInfo
*           u4Offset
* \param[out]
*           pu2Data
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalCfgDataRead16(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset, OUT PUINT_16 pu2Data)
{
	ASSERT(prGlueInfo);

	/* windows family doesn't support NVRAM access */
	return FALSE;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief API for writing data on NVRAM
*
* \param[in]
*           prGlueInfo
*           u4Offset
*           u2Data
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalCfgDataWrite16(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Offset, IN UINT_16 u2Data)
{
	ASSERT(prGlueInfo);

	/* windows family doesn't support NVRAM access */
	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief to check if the WPS is active or not
*
* \param[in]
*           prGlueInfo
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalWSCGetActiveState(IN P_GLUE_INFO_T prGlueInfo)
{
	/* Windows family does not support WSC. */
	return (FALSE);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief update RSSI and LinkQuality to GLUE layer
*
* \param[in]
*           prGlueInfo
*           eNetTypeIdx
*           cRssi
*           cLinkQuality
*
* \return
*           None
*/
/*----------------------------------------------------------------------------*/
VOID
kalUpdateRSSI(IN P_GLUE_INFO_T prGlueInfo,
	      IN ENUM_KAL_NETWORK_TYPE_INDEX_T eNetTypeIdx, IN INT_8 cRssi, IN INT_8 cLinkQuality)
{
	ASSERT(prGlueInfo);

	switch (eNetTypeIdx) {
	case KAL_NETWORK_TYPE_AIS_INDEX:
		break;

	default:
		break;

	}

	return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief Dispatch pre-allocated I/O buffer
*
* \param[in]
*           u4AllocSize
*
* \return
*           PVOID for pointer of pre-allocated I/O buffer
*/
/*----------------------------------------------------------------------------*/
PVOID kalAllocateIOBuffer(IN UINT_32 u4AllocSize)
{
	return kalMemAlloc(u4AllocSize, PHY_MEM_TYPE);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief Release all dispatched I/O buffer
*
* \param[in]
*           pvAddr
*           u4Size
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID kalReleaseIOBuffer(IN PVOID pvAddr, IN UINT_32 u4Size)
{
	kalMemFree(pvAddr, PHY_MEM_TYPE, u4Size);
}


#if CFG_CHIP_RESET_SUPPORT
/*----------------------------------------------------------------------------*/
/*!
* \brief Whole-chip Reset Trigger
*
* \param[in]
*           none
*
* \return
*           none
*/
/*----------------------------------------------------------------------------*/
VOID glSendResetRequest(VOID)
{
	/* no implementation for Win32 platform */
	return;
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called for checking if MT6620 is resetting
 *
 * @param   None
 *
 * @retval  TRUE
 *          FALSE
 */
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsResetting(VOID)
{
	return FALSE;
}
#endif


/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called for writing data to file
 *
 * @param [in]
 *              pucPath
 *              fgDoAppend
 *              pucData
 *              u4Size
 *
 * @retval  length
 *
 */
/*----------------------------------------------------------------------------*/
UINT_32 kalWriteToFile(const PUINT_8 pucPath, BOOLEAN fgDoAppend, PUINT_8 pucData, UINT_32 u4Size)
{
	/* no implementation for Win32 platform */
	HANDLE hFile;
	IO_STATUS_BLOCK ioStatusBlock;
	HANDLE handle;
	NTSTATUS ntstatus;
	LARGE_INTEGER byteOffset;
	UNICODE_STRING uniName;
	OBJECT_ATTRIBUTES objAttr;
	/* 1.Path information */
	RtlInitUnicodeString(&uniName, pucPath);

	InitializeObjectAttributes(&objAttr, &uniName,
				   OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
	/*  */

	if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		return STATUS_INVALID_DEVICE_STATE;
	if (!fgDoAppend) {
		ntstatus = ZwCreateFile(&handle,
					GENERIC_WRITE,
					&objAttr, &ioStatusBlock, NULL,
					FILE_ATTRIBUTE_NORMAL,
					0,
					FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

	} else {
		ntstatus = ZwOpenFile(&handle,
				      FILE_APPEND_DATA,
				      &objAttr, &ioStatusBlock,
				      FILE_SHARE_READ | FILE_SHARE_WRITE,
				      FILE_SYNCHRONOUS_IO_NONALERT);


	}

	ntstatus = ZwWriteFile(handle, NULL, NULL, NULL, &ioStatusBlock,
			       pucData, u4Size, NULL, NULL);
	ZwClose(handle);
	return 0;
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is co
 *
 * @param [in]
 *
 * @retval  length
 *
 */
/*----------------------------------------------------------------------------*/
UINT_32
kal_sprintf_ddk(const PUINT_8 pucPath,
		UINT_32 u4size, const char *fmt1, const char *fmt2, const char *fmt3)
{
#if 0
	RtlStringCbPrintfW(pucPath, u4size,
			   L"\\SystemRoot\\dump_%ld_0x%08lX_%ld.hex", fmt1, fmt2, fmt3);
#else
	RtlStringCbPrintfW(pucPath, u4size, L"\\SystemRoot\\dump.hex");

#endif
	return 0;
}

UINT_32 kal_sprintf_done_ddk(const PUINT_8 pucPath, UINT_32 u4size)
{
	RtlStringCbPrintfW(pucPath, u4size, L"\\SystemRoot\\dump_done.txt");

	return 0;
}

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
/*----------------------------------------------------------------------------*/
/*!
* \brief    To configure SDIO test pattern mode
*
* \param[in]
*           prGlueInfo
*           fgEn
*           fgRead
*
* \return
*           TRUE
*           FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN kalSetSdioTestPattern(IN P_GLUE_INFO_T prGlueInfo, IN BOOLEAN fgEn, IN BOOLEAN fgRead)
{
	const UINT_8 aucPattern[] = {
		0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55,
		0xaa, 0x55, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80,
		0x80, 0x7f, 0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f,
		0x40, 0x40, 0x40, 0xbf, 0x40, 0x40, 0x40, 0xbf,
		0xbf, 0xbf, 0x40, 0xbf, 0xbf, 0xbf, 0x20, 0x20,
		0x20, 0xdf, 0x20, 0x20, 0x20, 0xdf, 0xdf, 0xdf,
		0x20, 0xdf, 0xdf, 0xdf, 0x10, 0x10, 0x10, 0xef,
		0x10, 0x10, 0x10, 0xef, 0xef, 0xef, 0x10, 0xef,
		0xef, 0xef, 0x08, 0x08, 0x08, 0xf7, 0x08, 0x08,
		0x08, 0xf7, 0xf7, 0xf7, 0x08, 0xf7, 0xf7, 0xf7,
		0x04, 0x04, 0x04, 0xfb, 0x04, 0x04, 0x04, 0xfb,
		0xfb, 0xfb, 0x04, 0xfb, 0xfb, 0xfb, 0x02, 0x02,
		0x02, 0xfd, 0x02, 0x02, 0x02, 0xfd, 0xfd, 0xfd,
		0x02, 0xfd, 0xfd, 0xfd, 0x01, 0x01, 0x01, 0xfe,
		0x01, 0x01, 0x01, 0xfe, 0xfe, 0xfe, 0x01, 0xfe,
		0xfe, 0xfe, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
		0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff,
		0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
		0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
		0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
		0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff,
		0x00, 0x00, 0x00, 0xff
	};
	UINT_32 i;

	ASSERT(prGlueInfo);

	/* access to MCR_WTMCR to engage PRBS mode */
	prGlueInfo->fgEnSdioTestPattern = fgEn;
	prGlueInfo->fgSdioReadWriteMode = fgRead;

	if (fgRead == FALSE) {
		/* fill buffer for data to be written */
		for (i = 0; i < sizeof(aucPattern); i++) {
			prGlueInfo->aucSdioTestBuffer[i] = aucPattern[i];
		}
	}

	return TRUE;
}
#endif


VOID
kalReadyOnChannel(IN P_GLUE_INFO_T prGlueInfo,
		  IN UINT_64 u8Cookie,
		  IN ENUM_BAND_T eBand,
		  IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum, IN UINT_32 u4DurationMs)
{
	/* no implementation yet */
	return;
}


VOID
kalRemainOnChannelExpired(IN P_GLUE_INFO_T prGlueInfo,
			  IN UINT_64 u8Cookie,
			  IN ENUM_BAND_T eBand, IN ENUM_CHNL_EXT_T eSco, IN UINT_8 ucChannelNum)
{
	/* no implementation yet */
	return;
}


VOID
kalIndicateMgmtTxStatus(IN P_GLUE_INFO_T prGlueInfo,
			IN UINT_64 u8Cookie,
			IN BOOLEAN fgIsAck, IN PUINT_8 pucFrameBuf, IN UINT_32 u4FrameLen)
{
	/* no implementation yet */
	return;
}


VOID kalIndicateRxMgmtFrame(IN P_GLUE_INFO_T prGlueInfo, IN P_SW_RFB_T prSwRfb)
{
	/* no implementation yet */
	return;
}


VOID kalSchedScanResults(IN P_GLUE_INFO_T prGlueInfo)
{
	/* no implementation yet */
	return;
}


VOID kalSchedScanStopped(IN P_GLUE_INFO_T prGlueInfo)
{
	/* no implementation yet */
	return;
}


BOOLEAN
kalGetEthDestAddr(IN P_GLUE_INFO_T prGlueInfo,
		  IN P_NATIVE_PACKET prPacket, OUT PUINT_8 pucEthDestAddr)
{
	PNDIS_BUFFER prNdisBuffer;
	UINT_32 u4PacketLen;

	UINT_32 u4ByteCount = 0;
	PNDIS_PACKET prNdisPacket = (PNDIS_PACKET) prPacket;

	DEBUGFUNC("kalGetEthDestAddr");

	ASSERT(prGlueInfo);
	ASSERT(pucEthDestAddr);

	/* 4 <1> Find out all buffer information */
	NdisQueryPacket(prNdisPacket, NULL, NULL, &prNdisBuffer, &u4PacketLen);

	if (u4PacketLen < 6) {
		DBGLOG(INIT, WARN, ("Invalid Ether packet length: %d\n", u4PacketLen));
		return FALSE;
	}
	/* 4 <2> Copy partial frame to local LOOK AHEAD Buffer */
	while (prNdisBuffer && u4ByteCount < 6) {
		PVOID pvAddr;
		UINT_32 u4Len;
		PNDIS_BUFFER prNextNdisBuffer;

#ifdef NDIS51_MINIPORT
		NdisQueryBufferSafe(prNdisBuffer, &pvAddr, &u4Len, HighPagePriority);
#else
		NdisQueryBuffer(prNdisBuffer, &pvAddr, &u4Len);
#endif

		if (pvAddr == (PVOID) NULL) {
			ASSERT(0);
			return FALSE;
		}

		if ((u4ByteCount + u4Len) >= 6) {
			kalMemCopy((PUINT_8) ((UINT_32) pucEthDestAddr + u4ByteCount),
				   pvAddr, (6 - u4ByteCount));
			break;
		} else {
			kalMemCopy((PUINT_8) ((UINT_32) pucEthDestAddr + u4ByteCount),
				   pvAddr, u4Len);
		}
		u4ByteCount += u4Len;

		NdisGetNextBuffer(prNdisBuffer, &prNextNdisBuffer);

		prNdisBuffer = prNextNdisBuffer;
	}

	return TRUE;
}
