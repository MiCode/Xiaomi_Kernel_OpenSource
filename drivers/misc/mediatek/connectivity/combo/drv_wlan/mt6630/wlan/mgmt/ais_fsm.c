/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/mgmt/ais_fsm.c#4
*/

/*! \file   "aa_fsm.c"
    \brief  This file defines the FSM for SAA and AAA MODULE.

    This file defines the FSM for SAA and AAA MODULE.
*/



/*
** Log: ais_fsm.c
**
** 08 20 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** add option to support passive scan for AIS network, default off
**
** 08 16 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** for passive scan, specify SSID num as zero
**
** 08 12 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** use separate SSID container for roaming scan requests
**
** 08 12 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** support passive scan via cfg80211_ops.scan() callback
**
** 08 09 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** 1. integrate scheduled scan functionality
** 2. condition compilation for linux-3.4 & linux-3.8 compatibility
** 3. correct CMD queue access to reduce lock scope
**
** 08 05 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Add SW rate definition
** 2. Add HW default rate selection logic from FW
**
** 07 31 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Fix NetDev binding issue
**
** 07 31 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Change private data of net device.
**
** 07 29 2013 cp.wu
** [BORA00002725] [MT6630][Wi-Fi] Add MGMT TX/RX support for Linux port
** Preparation for porting remain_on_channel support
**
** 07 26 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Reduce extra Tx frame header parsing
** 2. Add TX port control
** 3. Add net interface to BSS binding
**
** 07 23 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Sync the latest jb2.mp 11w code as draft version
** Not the CM bit for avoid wapi 1x drop at re-key
**
** 07 12 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Add 11ac to AIS desired PHY config
** 2. Extend PHY type set to 11ac/11anac/11abgnac
**
** 07 12 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Update VHT IE composing function
** 2. disable bow
** 3. Exchange bss/sta rec update sequence for temp solution
**
** 07 02 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Refine security BMC wlan index assign
** Fix some compiling warning
**
** 06 25 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update for 1st connection
**
** 06 19 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** update MAC address handling logic
**
** 04 30 2013 eason.tsai
** [BORA00002255] [MT6630 Wi-Fi][Driver] develop
** update 11ac channel setting
**
** 03 29 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Do more sta record free mechanism check
** remove non-used code
**
** 03 14 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** .modify some code define and flow
**
** 03 12 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** .
**
** 03 08 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Remove non-used compiling flag and code
**
** 02 18 2013 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** New feature to remove all sta records by BssIndex
**
** 02 18 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** modify bssClearClientList() to bssInitializeClientList()
**
** 02 07 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** add join timeout check for retrying
**
** 02 06 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** change BSS-INFO/STA-REC update sequence:
** always update STA-REC then BSS-INFO
**
** 01 23 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** modify AIS behavior: stop join trial if failed
**
** 01 22 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** modification for ucBssIndex migration
**
** 01 21 2013 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** 1. Create rP2pDevInfo structure
** 2. Support 80/160 MHz channel bandwidth for channel privilege
**
** 01 17 2013 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Use ucBssIndex to replace eNetworkTypeIndex
**
** 01 03 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** carry timeout value and channel dwell time value to scan module
**
** 10 31 2012 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** code sync..
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
**
** 08 24 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** .
**
** 08 24 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** cfg80211 support merge back from ALPS.JB to DaVinci - MT6620 Driver v2.3 branch.
**
** 08 07 2012 cp.wu
** [WCXRP00001086] [MT6620 Wi-Fi][Driver] On Android,
** indicate an extra DISCONNECT for REASSOCIATED cases as an explicit trigger for Android framework
** remove unnecessary driver workaround for WEP key change detection - it should be done by framework instead
 *
 * 04 20 2012 cp.wu
 * [WCXRP00000913] [MT6620 Wi-Fi] create repository of source code dedicated for MT6620 E6 ASIC
 * correct macro
 *
 * 01 16 2012 cp.wu
 * [MT6620 Wi-Fi][Driver] API and behavior modification for preferred band configuration
 * with corresponding network configuration
 * add wlanSetPreferBandByNetwork() for glue layer to invoke for setting preferred band configuration
 * corresponding to network type.
 *
 * 11 24 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * Adjust code for DBG and CONFIG_XLOG.
 *
 * 11 22 2011 cp.wu
 * [WCXRP00001120] [MT6620 Wi-Fi][Driver] Modify roaming to AIS state transition from synchronous to asynchronous
 * approach to avoid incomplete state termination
 * 1. change RDD related compile option brace position.
 * 2. when roaming is triggered, ask AIS to transit immediately only when AIS is in Normal TR state without
 *     join timeout timer ticking
 * 3. otherwise, insert AIS_REQUEST into pending request queue
 *
 * 11 11 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * modify the xlog related code.
 *
 * 11 04 2011 cp.wu
 * [WCXRP00001086] [MT6620 Wi-Fi][Driver] On Android, indicate an extra DISCONNECT for
 * REASSOCIATED cases as an explicit trigger for Android framework
 * correct reference to BSSID field in Association-Response frame.
 *
 * 11 04 2011 cp.wu
 * [WCXRP00001086] [MT6620 Wi-Fi][Driver]
 * 1. for DEAUTH/DISASSOC cases, indicate for DISCONNECTION immediately.
 * 2. (Android only)
 *
 * 11 02 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * adding the code for XLOG.
 *
 * 10 26 2011 tsaiyuan.hsu
 * [WCXRP00001064] [MT6620 Wi-Fi][DRV]] add code with roaming awareness when disconnecting AIS network
 * be aware roaming when disconnecting AIS network.
 *
 * 10 25 2011 cm.chang
 * [WCXRP00001058] [All Wi-Fi][Driver] Fix sta_rec's phyTypeSet and OBSS scan in AP mode
 * STA_REC shall be NULL for Beacon's MSDU
 *
 * 10 13 2011 cp.wu
 * [MT6620 Wi-Fi][Driver] Reduce join failure count limit to 2 for faster re-join for other BSS
 * 1. short join failure count limit to 2
 * 2. treat join timeout as kind of join failure as well
 *
 * 10 12 2011 wh.su
 * [WCXRP00001036] [MT6620 Wi-Fi][Driver][FW] Adding the 802.11w code for MFP
 * adding the 802.11w related function and define .
 *
 * 09 30 2011 cm.chang
 * [WCXRP00001020] [MT6620 Wi-Fi][Driver] Handle secondary channel offset of AP in 5GHz band
 * .
 *
 * 09 20 2011 tsaiyuan.hsu
 * [WCXRP00000931] [MT5931 Wi-Fi][DRV/FW] add swcr to disable roaming from driver
 * change window registry of driver for roaming.
 *
 * 09 20 2011 cm.chang
 * [WCXRP00000997] [MT6620 Wi-Fi][Driver][FW] Handle change of BSS preamble type and slot time
 * Handle client mode about preamble type and slot time
 *
 * 09 08 2011 tsaiyuan.hsu
 * [WCXRP00000972] [MT6620 Wi-Fi][DRV]] check if roaming occurs after join failure to avoid state incosistence.
 * check if roaming occurs after join failure to avoid deactivation of network.
 *
 * 08 24 2011 chinghwa.yu
 * [WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Update RDD test mode cases.
 *
 * 08 16 2011 tsaiyuan.hsu
 * [WCXRP00000931] [MT5931 Wi-Fi][DRV/FW] add swcr to disable roaming from driver
 * EnableRoaming in registry is deprecated.
 *
 * 08 16 2011 tsaiyuan.hsu
 * [WCXRP00000931] [MT5931 Wi-Fi][DRV/FW] add swcr to disable roaming from driver
 * use registry to enable or disable roaming.
 *
 * 07 07 2011 cp.wu
 * [WCXRP00000840] [MT6620 Wi-Fi][Driver][AIS]
 * stop timer when joining operation is failed due to try count exceeds limitation
 *
 * 06 28 2011 cp.wu
 * [WCXRP00000815] [MT6620 Wi-Fi][Driver]
 * do not handle SCAN request immediately after connected to increase the probability of receiving 1st beacon frame.
 *
 * 06 23 2011 cp.wu
 * [WCXRP00000798] [MT6620 Wi-Fi][Firmware] Follow-ups for WAPI frequency offset workaround in firmware SCN module
 * change parameter name from PeerAddr to BSSID
 *
 * 06 20 2011 cp.wu
 * [WCXRP00000798] [MT6620 Wi-Fi][Firmware] Follow-ups for WAPI frequency offset workaround in firmware SCN module
 * 1. specify target's BSSID when requesting channel privilege.
 * 2. pass BSSID information to firmware domain
 *
 * 06 16 2011 cp.wu
 * [WCXRP00000782] [MT6620 Wi-Fi][AIS] Treat connection at higher priority over scanning to avoid WZC connection timeout
 * ensure DEAUTH is always sent before establish a new connection
 *
 * 06 16 2011 cp.wu
 * [WCXRP00000782] [MT6620 Wi-Fi][AIS] Treat connection at higher priority over scanning to avoid WZC connection timeout
 * typo fix: a right brace is missed.
 *
 * 06 16 2011 cp.wu
 * [WCXRP00000782] [MT6620 Wi-Fi][AIS] Treat connection at higher priority over scanning to avoid WZC connection timeout
 * When RECONNECT request is identified as disconnected, it is necessary to check for pending scan request.
 *
 * 06 16 2011 cp.wu
 * [WCXRP00000757] [MT6620 Wi-Fi][Driver][SCN] take use of RLM API to filter out BSS in disallowed channels
 * mark fgIsTransition as TRUE for state rolling.
 *
 * 06 16 2011 cp.wu
 * [WCXRP00000782] [MT6620 Wi-Fi][AIS] Treat connection at higher priority over scanning to avoid WZC connection timeout
 * always check for pending scan after switched into NORMAL_TR state.
 *
 * 06 14 2011 cp.wu
 * [WCXRP00000782] [MT6620 Wi-Fi][AIS] Treat connection at higher priority over scanning to avoid WZC connection timeout
 * always treat connection request at higher priority over scanning request
 *
 * 06 09 2011 tsaiyuan.hsu
 * [WCXRP00000760] [MT5931 Wi-Fi][FW] Refine rxmHandleMacRxDone to reduce code size
 * move send_auth at rxmHandleMacRxDone in firmware to driver to reduce code size.
 *
 * 06 02 2011 cp.wu
 * [WCXRP00000681] [MT5931][Firmware] HIF code size reduction
 * eliminate unused parameters for SAA-FSM
 *
 * 05 18 2011 cp.wu
 * [WCXRP00000732] [MT6620 Wi-Fi][AIS]
 * change SCAN handling behavior when followed by a CONNECT/DISCONNECT requests by pending instead of dropping.
 *
 * 05 17 2011 cp.wu
 * [WCXRP00000732] [MT6620 Wi-Fi][AIS]
 * when TX DONE status is TX_RESULT_DROPPED_IN_DRIVER, no need to switch back to IDLE state.
 *
 * 04 14 2011 cm.chang
 * [WCXRP00000634] [MT6620 Wi-Fi][Driver][FW] 2nd BSS will not support 40MHz bandwidth for concurrency
 * .
 *
 * 04 13 2011 george.huang
 * [WCXRP00000628] [MT6620 Wi-Fi][FW][Driver] Modify U-APSD setting to default OFF
 * remove assert
 *
 * 03 18 2011 cp.wu
 * [WCXRP00000575] [MT6620 Wi-Fi][Driver][AIS] reduce memory usage when generating mailbox message for scan request
 * when there is no IE needed for probe request, then request a smaller memory for mailbox message
 *
 * 03 17 2011 chinglan.wang
 * [WCXRP00000570] [MT6620 Wi-Fi][Driver] Add Wi-Fi Protected Setup v2.0 feature
 * .
 *
 * 03 17 2011 chinglan.wang
 * [WCXRP00000570] [MT6620 Wi-Fi][Driver] Add Wi-Fi Protected Setup v2.0 feature
 * .
 *
 * 03 16 2011 tsaiyuan.hsu
 * [WCXRP00000517] [MT6620 Wi-Fi][Driver][FW] Fine Tune Performance of Roaming
 * remove obsolete definition and unused variables.
 *
 * 03 11 2011 cp.wu
 * [WCXRP00000535] [MT6620 Wi-Fi][Driver] Fixed channel operation when AIS and Tethering are operating concurrently
 * When fixed channel operation is necessary, AIS-FSM would scan and only connect for BSS on the specific channel
 *
 * 03 09 2011 tsaiyuan.hsu
 * [WCXRP00000517] [MT6620 Wi-Fi][Driver][FW] Fine Tune Performance of Roaming
 * avoid clearing fgIsScanReqIssued so as to add scan results.
 *
 * 03 07 2011 terry.wu
 * [WCXRP00000521] [MT6620 Wi-Fi][Driver] Remove non-standard debug message
 * Toggle non-standard debug messages to comments.
 *
 * 03 04 2011 tsaiyuan.hsu
 * [WCXRP00000517] [MT6620 Wi-Fi][Driver][FW] Fine Tune Performance of Roaming
 * reset retry conter of attemp to connect to ap after completion of join.
 *
 * 03 04 2011 cp.wu
 * [WCXRP00000515] [MT6620 Wi-Fi][Driver] Surpress compiler warning which is identified by GNU compiler collection
 * surpress compile warning occured when compiled by GNU compiler collection.
 *
 * 03 02 2011 cp.wu
 * [WCXRP00000503] [MT6620 Wi-Fi][Driver]
 * use RCPI brought by ASSOC-RESP after connection is built as initial RCPI to avoid using a uninitialized MAC-RX RCPI.
 *
 * 02 26 2011 tsaiyuan.hsu
 * [WCXRP00000391] [MT6620 Wi-Fi][FW] Add Roaming Support
 * not send disassoc or deauth to leaving AP so as to improve performace of roaming.
 *
 * 02 23 2011 cp.wu
 * [WCXRP00000487] [MT6620 Wi-Fi][Driver][AIS]
 * when handling reconnect request, set fgTryScan as TRUE
 *
 * 02 22 2011 cp.wu
 * [WCXRP00000487] [MT6620 Wi-Fi][Driver][AIS]
 * handle SCAN and RECONNECT with a FIFO approach.
 *
 * 02 09 2011 tsaiyuan.hsu
 * [WCXRP00000392] [MT6620 Wi-Fi][Driver] Add Roaming Support
 * Check if prRegInfo is null or not before initializing roaming parameters.
 *
 * 02 01 2011 cp.wu
 * [WCXRP00000416] [MT6620 Wi-Fi][Driver]
 * treat "unable to find BSS" as connection trial to prevent infinite reconnection trials.
 *
 * 01 27 2011 tsaiyuan.hsu
 * [WCXRP00000392] [MT6620 Wi-Fi][Driver] Add Roaming Support
 * add roaming fsm
 * 1. not support 11r, only use strength of signal to determine roaming.
 * 2. not enable CFG_SUPPORT_ROAMING until completion of full test.
 * 3. in 6620, adopt work-around to avoid sign extension problem of cck of hw
 * 4. assume that change of link quality in smooth way.
 *
 * 01 26 2011 yuche.tsai
 * [WCXRP00000388] [Volunteer Patch][MT6620][Driver/Fw] change Station Type in station record.
 * .
 *
 * 01 25 2011 yuche.tsai
 * [WCXRP00000388] [Volunteer Patch][MT6620][Driver/Fw] change Station Type in station record.
 * Fix Compile Error when DBG is disabled.
 *
 * 01 25 2011 yuche.tsai
 * [WCXRP00000388] [Volunteer Patch][MT6620][Driver/Fw] change Station Type in station record.
 * Change Station Type in Station Record, Modify MACRO definition for getting station type & network type index & Role.
 *
 * 01 14 2011 cp.wu
 * [WCXRP00000359] [MT6620 Wi-Fi][Driver] add an extra state to ensure DEAUTH frame is always sent
 * Add an extra state to guarantee DEAUTH frame is sent then connect to new BSS.
 * This change is due to WAPI AP needs DEAUTH frame as a necessary step in handshaking protocol.
 *
 * 01 11 2011 cp.wu
 * [WCXRP00000307] [MT6620 Wi-Fi][SQA]WHQL test .2c_wlan_adhoc case fail.
 * [IBSS] when merged in, the bss state should be updated to firmware to pass WHQL adhoc failed item
 *
 * 01 10 2011 cp.wu
 * [WCXRP00000351] [MT6620 Wi-Fi][Driver]
 * remove from scanning result when the BSS is disconnected due to beacon timeout.
 *
 * 01 03 2011 cp.wu
 * [WCXRP00000337] [MT6620 Wi-FI][Driver]
 * do not invoke cnmStaRecResetStatus() directly, nicUpdateBss will do the things after bss is disconnected
 *
 * 12 30 2010 cp.wu
 * [WCXRP00000270] [MT6620 Wi-Fi][Driver] Clear issues after concurrent networking support has been merged
 *
 *
 * 12 27 2010 cp.wu
 * [WCXRP00000269] [MT6620 Wi-Fi][Driver][Firmware] Prepare for v1.1 branch release
 * add DEBUGFUNC() macro invoking for more detailed debugging information
 *
 * 12 23 2010 george.huang
 * [WCXRP00000152] [MT6620 Wi-Fi] AP mode power saving function
 * 1. update WMM IE parsing, with ASSOC REQ handling
 * 2. extend U-APSD parameter passing from driver to FW
 *
 * 12 17 2010 cp.wu
 * [WCXRP00000270] [MT6620 Wi-Fi][Driver] Clear issues after concurrent networking support has been merged
 * before BSS disconnection is indicated to firmware, all correlated peer should be cleared and freed
 *
 * 12 07 2010 cm.chang
 * [WCXRP00000239] MT6620 Wi-Fi][Driver][FW] Merge concurrent branch back to maintrunk
 * 1. BSSINFO include RLM parameter
 * 2. free all sta records when network is disconnected
 *
 * 11 25 2010 yuche.tsai
 * NULL
 * Update SLT Function for QoS Support and not be affected by fixed rate function.
 *
 * 11 25 2010 cp.wu
 * [WCXRP00000208] [MT6620 Wi-Fi][Driver] Add scanning with specified SSID to AIS FSM
 * add scanning with specified SSID facility to AIS-FSM
 *
 * 11 01 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver]
 * 1) Query link speed (TX rate) from firmware directly with buffering mechanism to reduce overhead
 * 2) Remove CNM CH-RECOVER event handling
 * 3) cfg read/write API renamed with kal prefix for unified naming rules.
 *
 * 10 26 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver]
 * 1) update NVRAM content template to ver 1.02
 * 2) add compile option for querying NIC capability (default: off)
 * 3) modify AIS 5GHz support to run-time option, which could be turned on by registry or NVRAM setting
 * 4) correct auto-rate compiler error under linux (treat warning as error)
 * 5) simplify usage of NVRAM and REG_INFO_T
 * 6) add version checking between driver and firmware
 *
 * 10 14 2010 wh.su
 * [WCXRP00000097] [MT6620 Wi-Fi] [Driver] Fixed the P2P not setting the fgIsChannelExt value make scan not abort
 * initial the fgIsChannelExt value.
 *
 * 10 08 2010 cp.wu
 * [WCXRP00000087] [MT6620 Wi-Fi][Driver] Cannot connect to 5GHz AP, driver will cause FW assert.
 * correct erroneous logic: specifying eBand with incompatible eSco
 *
 * 10 04 2010 cp.wu
 * [WCXRP00000077] [MT6620 Wi-Fi][Driver][FW]
 * remove ENUM_NETWORK_TYPE_T definitions
 *
 * 09 27 2010 chinghwa.yu
 * [WCXRP00000063] Update BCM CoEx design and settings[WCXRP00000065] Update BoW design and settings
 * Update BCM/BoW design and settings.
 *
 * 09 23 2010 cp.wu
 * [WCXRP00000049] [MT6620 Wi-Fi][Driver] Adhoc cannot be created successfully.
 * keep IBSS-ALONE state retrying until further instruction is received
 *
 * 09 21 2010 cp.wu
 * [WCXRP00000053] [MT6620 Wi-Fi][Driver]
 * Do a complete reset with STA-REC null checking for RF test re-entry
 *
 * 09 09 2010 yuche.tsai
 * NULL
 * Fix NULL IE Beacon issue. Sync Beacon Content to FW before enable beacon.
 * Both in IBSS Create & IBSS Merge
 *
 * 09 09 2010 cp.wu
 * NULL
 * frequency is in unit of KHz thus no need to divide 1000 once more.
 *
 * 09 06 2010 cp.wu
 * NULL
 * 1) initialize for correct parameter even for disassociation.
 * 2) AIS-FSM should have a limit on trials to build connection
 *
 * 09 03 2010 kevin.huang
 * NULL
 * Refine #include sequence and solve recursive/nested #include issue
 *
 * 08 30 2010 cp.wu
 * NULL
 * eliminate klockwork errors
 *
 * 08 29 2010 yuche.tsai
 * NULL
 * Finish SLT TX/RX & Rate Changing Support.
 *
 * 08 25 2010 cp.wu
 * NULL
 * add option for enabling AIS 5GHz scan
 *
 * 08 25 2010 cp.wu
 * NULL
 * [AIS-FSM]
 *
 * 08 25 2010 george.huang
 * NULL
 * update OID/ registry control path for PM related settings
 *
 * 08 24 2010 cm.chang
 * NULL
 * Support RLM initail channel of Ad-hoc, P2P and BOW
 *
 * 08 20 2010 cm.chang
 * NULL
 * Migrate RLM code to host from FW
 *
 * 08 12 2010 cp.wu
 * NULL
 * check-in missed files.
 *
 * 08 12 2010 kevin.huang
 * NULL
 * Refine bssProcessProbeRequest() and bssSendBeaconProbeResponse()
 *
 * 08 09 2010 cp.wu
 * NULL
 * reset fgIsScanReqIssued when abort request is received right after join completion.
 *
 * 08 03 2010 cp.wu
 * NULL
 * surpress compilation warning.
 *
 * 08 02 2010 cp.wu
 * NULL
 * comment out deprecated members in BSS_INFO, which are only used by firmware rather than driver.
 *
 * 07 30 2010 cp.wu
 * NULL
 * 1) BoW wrapper: use definitions instead of hard-coded constant for error code
 * 2) AIS-FSM: eliminate use of desired RF parameters, use prTargetBssDesc instead
 * 3) add handling for RX_PKT_DESTINATION_HOST_WITH_FORWARD for GO-broadcast frames
 *
 * 07 29 2010 cp.wu
 * NULL
 * eliminate u4FreqInKHz usage, combined into rConnections.ucAdHoc*
 *
 * 07 29 2010 cp.wu
 * NULL
 * allocate on MGMT packet for IBSS beaconing.
 *
 * 07 29 2010 cp.wu
 * NULL
 * [AIS-FSM] fix: when join failed, release channel privilege as well
 *
 * 07 28 2010 cp.wu
 * NULL
 * reuse join-abort sub-procedure to reduce code size.
 *
 * 07 28 2010 cp.wu
 * NULL
 * 1) eliminate redundant variable eOPMode in prAdapter->rWlanInfo
 * 2) change nicMediaStateChange() API prototype
 *
 * 07 26 2010 cp.wu
 *
 * AIS-FSM: when scan request is coming in the 1st 5 seconds of channel privilege period,
 * just pend it til 5-sec. period finishes
 *
 * 07 26 2010 cp.wu
 *
 * AIS-FSM FIX: return channel privilege even when the privilege is not granted yet
 * QM: qmGetFrameAction() won't assert when corresponding STA-REC index is not found
 *
 * 07 26 2010 cp.wu
 *
 * re-commit code logic being overwriten.
 *
 * 07 24 2010 wh.su
 *
 * .support the Wi-Fi RSN
 *
 * 07 23 2010 cp.wu
 *
 * 1) re-enable AIS-FSM beacon timeout handling.
 * 2) scan done API revised
 *
 * 07 23 2010 cp.wu
 *
 * 1) enable Ad-Hoc
 * 2) disable beacon timeout handling temporally due to unexpected beacon timeout event.
 *
 * 07 23 2010 cp.wu
 *
 * indicate scan done for linux wireless extension
 *
 * 07 23 2010 cp.wu
 *
 * add AIS-FSM handling for beacon timeout event.
 *
 * 07 22 2010 cp.wu
 *
 * 1) refine AIS-FSM indent.
 * 2) when entering RF Test mode, flush 802.1X frames as well
 * 3) when entering D3 state, flush 802.1X frames as well
 *
 * 07 21 2010 cp.wu
 *
 * separate AIS-FSM states into different cases of channel request.
 *
 * 07 21 2010 cp.wu
 *
 * 1) change BG_SCAN to ONLINE_SCAN for consistent term
 * 2) only clear scanning result when scan is permitted to do
 *
 * 07 20 2010 cp.wu
 *
 * 1) [AIS] when new scan is issued, clear currently available scanning result except the connected one
 * 2) refine disconnection behaviour when issued during BG-SCAN process
 *
 * 07 20 2010 cp.wu
 *
 * 1) bugfix: do not stop timer for join after switched into normal_tr state, for providing chance for DHCP handshasking
 * 2) modify rsnPerformPolicySelection() invoking
 *
 * 07 19 2010 cp.wu
 *
 * 1) init AIS_BSS_INFO as channel number = 1 with band = 2.4GHz
 * 2) correct typo
 *
 * 07 19 2010 wh.su
 *
 * update for security supporting.
 *
 * 07 19 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration.
 * when IBSS is being merged-in, send command packet to PM for connected indication
 *
 * 07 19 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration.
 * Add Ad-Hoc support to AIS-FSM
 *
 * 07 19 2010 jeffrey.chang
 *
 * Linux port modification
 *
 * 07 16 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration.
 * bugfix for SCN migration
 * 1) modify QUEUE_CONCATENATE_QUEUES() so it could be used to concatence with an empty queue
 * 2) before AIS issues scan request, network(BSS) needs to be activated first
 * 3) only invoke COPY_SSID when using specified SSID for scan
 *
 * 07 15 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration.
 * for AIS scanning, driver specifies no extra IE for probe request
 *
 * 07 15 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration.
 * driver no longer generates probe request frames
 *
 * 07 14 2010 yarco.yang
 *
 * Remove CFG_MQM_MIGRATION
 *
 * 07 14 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration.
 * Refine AIS-FSM by divided into more states
 *
 * 07 13 2010 cm.chang
 *
 * Rename MSG_CH_RELEASE_T to MSG_CH_ABORT_T
 *
 * 07 09 2010 cp.wu
 *
 * 1) separate AIS_FSM state for two kinds of scanning. (OID triggered scan, and scan-for-connection)
 * 2) eliminate PRE_BSS_DESC_T, Beacon/PrebResp is now parsed in single pass
 * 3) implment DRV-SCN module, currently only accepts single scan request,
 * other request will be directly dropped by returning BUSY
 *
 * 07 09 2010 george.huang
 *
 * [WPD00001556] Migrate PM variables from FW to driver: for composing QoS Info
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 07 08 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * take use of RLM module for parsing/generating HT IEs for 11n capability
 *
 * 07 08 2010 cm.chang
 * [WPD00003841][LITE Driver] Migrate RLM/CNM to host driver
 * Rename MID_MNY_CNM_CH_RELEASE to MID_MNY_CNM_CH_ABORT
 *
 * 07 07 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * for first connection, if connecting failed do not enter into scan state.
 *
 * 07 06 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * once STA-REC is allocated and updated, invoke cnmStaRecChangeState() to sync. with firmware.
 *
 * 07 06 2010 george.huang
 * [WPD00001556]Basic power managemenet function
 * Update arguments for nicUpdateBeaconIETemplate()
 *
 * 07 06 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * STA-REC is maintained by CNM only.
 *
 * 07 05 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * remove unused definitions.
 *
 * 07 01 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * AIS-FSM integration with CNM channel request messages
 *
 * 07 01 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * implementation of DRV-SCN and related mailbox message handling.
 *
 * 06 30 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * sync. with CMD/EVENT document ver0.07.
 *
 * 06 29 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) sync to. CMD/EVENT document v0.03
 * 2) simplify DTIM period parsing in scan.c only, bss.c no longer parses it again.
 * 3) send command packet to indicate FW-PM after
 *     a) 1st beacon is received after AIS has connected to an AP
 *     b) IBSS-ALONE has been created
 *     c) IBSS-MERGE has occured
 *
 * 06 25 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * modify Beacon/ProbeResp to complete parsing,
 * because host software has looser memory usage restriction
 *
 * 06 23 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * integrate .
 *
 * 06 22 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * comment out RLM APIs by CFG_RLM_MIGRATION.
 *
 * 06 22 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) add command warpper for STA-REC/BSS-INFO sync.
 * 2) enhance command packet sending procedure for non-oid part
 * 3) add command packet definitions for STA-REC/BSS-INFO sync.
 *
 * 06 21 2010 yarco.yang
 * [WPD00003837][MT6620]Data Path Refine
 * Support CFG_MQM_MIGRATION flag
 *
 * 06 21 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * add scan_fsm into building.
 *
 * 06 21 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * RSN/PRIVACY compilation flag awareness correction
 *
 * 06 18 2010 cm.chang
 * [WPD00003841][LITE Driver] Migrate RLM/CNM to host driver
 * Provide cnmMgtPktAlloc() and alloc/free function of msg/buf
 *
 * 06 18 2010 wh.su
 * [WPD00003840][MT6620 5931] Security migration
 * migration from MT6620 firmware.
 *
 * 06 15 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * add scan.c.
 *
 * 06 14 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * restore utility function invoking via hem_mbox to direct calls
 *
 * 06 11 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * auth.c is migrated.
 *
 * 06 11 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * add bss.c.
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
 * 06 10 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) eliminate CFG_CMD_EVENT_VERSION_0_9
 * 2) when disconnected, indicate nic directly (no event is needed)
 *
 * 06 10 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * add buildable & linkable ais_fsm.c
 *
 * related reference are still waiting to be resolved
 *
 * 06 01 2010 cm.chang
 * [BORA00000018]Integrate WIFI part into BORA for the 1st time
 * Add conditionial compiling flag to choose default available bandwidth
 *
 * 05 28 2010 kevin.huang
 * [BORA00000794][WIFISYS][New Feature]Power Management Support
 * Add ClientList handling API - bssClearClientList, bssAddStaRecToClientList
 *
 * 05 24 2010 kevin.huang
 * [BORA00000794][WIFISYS][New Feature]Power Management Support
 * Refine authSendAuthFrame() for NULL STA_RECORD_T case and minimum deauth interval.
 *
 * 05 21 2010 kevin.huang
 * [BORA00000794][WIFISYS][New Feature]Power Management Support
 * Fix compile error if CFG_CMD_EVENT_VER_009 == 0 for prEventConnStatus->ucNetworkType.
 *
 * 05 21 2010 kevin.huang
 * [BORA00000794][WIFISYS][New Feature]Power Management Support
 * Refine txmInitWtblTxRateTable() - set TX initial rate according to AP's operation rate set
 *
 * 05 17 2010 kevin.huang
 * [BORA00000794][WIFISYS][New Feature]Power Management Support
 * Call pmAbort() and add ucNetworkType field in EVENT_CONNECTION_STATUS
 *
 * 05 14 2010 kevin.huang
 * [BORA00000794][WIFISYS][New Feature]Power Management Support
 * Fix compile warning - define of MQM_WMM_PARSING was removed
 *
 * 05 12 2010 kevin.huang
 * [BORA00000794][WIFISYS][New Feature]Power Management Support
 * Add Power Management - Legacy PS-POLL support.
 *
 * 04 28 2010 tehuang.liu
 * [BORA00000605][WIFISYS] Phase3 Integration
 * Removed the use of compiling flag MQM_WMM_PARSING
 *
 * 04 27 2010 kevin.huang
 * [BORA00000714][WIFISYS][New Feature]Beacon Timeout Support
 *
 * Fix typo
 *
 * 04 27 2010 kevin.huang
 * [BORA00000663][WIFISYS][New Feature] AdHoc Mode Support
 * Add Set Slot Time and Beacon Timeout Support for AdHoc Mode
 *
 * 04 19 2010 kevin.huang
 * [BORA00000714][WIFISYS][New Feature]Beacon Timeout Support
 * Add Send Deauth for Class 3 Error and Leave Network Support
 *
 * 04 15 2010 wh.su
 * [BORA00000680][MT6620] Support the statistic for Microsoft os query
 * fixed the protected bit at cap info for ad-hoc.
 *
 * 04 13 2010 kevin.huang
 * [BORA00000663][WIFISYS][New Feature] AdHoc Mode Support
 * Add new HW CH macro support
 *
 * 04 07 2010 chinghwa.yu
 * [BORA00000563]Add WiFi CoEx BCM module
 * Add TX Power Control RCPI function.
 *
 * 03 29 2010 wh.su
 * [BORA00000605][WIFISYS] Phase3 Integration
 * move the wlan table alloc / free to change state function.
 *
 * 03 25 2010 wh.su
 * [BORA00000676][MT6620] Support the frequency setting and query at build connection / connection event
 * modify the build connection and status event structure bu CMD_EVENT doc 0.09 draft, default is disable.
 *
 * 03 24 2010 wh.su
 * [BORA00000605][WIFISYS] Phase3 Integration
 * fixed some WHQL testing error.
 *
 * 03 24 2010 kevin.huang
 * [BORA00000654][WIFISYS][New Feature] CNM Module - Ch Manager Support
 * Add Set / Unset POWER STATE in AIS Network
 *
 * 03 16 2010 kevin.huang
 * [BORA00000663][WIFISYS][New Feature] AdHoc Mode Support
 * Add AdHoc Mode
 *
 * 03 10 2010 kevin.huang
 * [BORA00000654][WIFISYS][New Feature] CNM Module - Ch Manager Support
 * Add Channel Manager for arbitration of JOIN and SCAN Req
 *
 * 03 03 2010 kevin.huang
 * [BORA00000603][WIFISYS] [New Feature] AAA Module Support
 * Add PHY_CONFIG to change Phy Type
 *
 * 03 03 2010 chinghwa.yu
 * [BORA00000563]Add WiFi CoEx BCM module
 * Use bcmWiFiNotify to replace wifi_send_msg to pass infomation to BCM module.
 *
 * 03 03 2010 chinghwa.yu
 * [BORA00000563]Add WiFi CoEx BCM module
 * Remove wmt_task definition and add PTA function.
 *
 * 03 02 2010 tehuang.liu
 * [BORA00000569][WIFISYS] Phase 2 Integration Test
 * Init TXM and MQM testing procedures in aisFsmRunEventJoinComplete()
 *
 * 03 01 2010 tehuang.liu
 * [BORA00000569][WIFISYS] Phase 2 Integration Test
 * Modified aisUpdateBssInfo() to call TXM's functions for setting WTBL TX parameters
 *
 * 03 01 2010 wh.su
 * [BORA00000605][WIFISYS] Phase3 Integration
 * clear the pmkid cache while indicate media disconnect.
 *
 * 02 26 2010 tehuang.liu
 * [BORA00000569][WIFISYS] Phase 2 Integration Test
 * .
 *
 * 02 26 2010 tehuang.liu
 * [BORA00000569][WIFISYS] Phase 2 Integration Test
 * Enabled MQM parsing WMM IEs for non-AP mode
 *
 * 02 26 2010 kevin.huang
 * [BORA00000603][WIFISYS] [New Feature] AAA Module Support
 * Remove CFG_TEST_VIRTUAL_CMD and add support of Driver STA_RECORD_T activation
 *
 * 02 25 2010 wh.su
 * [BORA00000605][WIFISYS] Phase3 Integration
 * use the Rx0 dor event indicate.
 *
 * 02 23 2010 kevin.huang
 * [BORA00000603][WIFISYS] [New Feature] AAA Module Support
 * Support dynamic channel selection
 *
 * 02 23 2010 wh.su
 * [BORA00000621][MT6620 Wi-Fi] Add the RSSI indicate to avoid XP stalled for query rssi value
 * Adding the RSSI event support, using the HAL function to get the rcpi value and tranlsate to
 * RSSI and indicate to driver
 *
 * 02 12 2010 cm.chang
 * [BORA00000018]Integrate WIFI part into BORA for the 1st time
 * Use bss info array for concurrent handle
 *
 * 02 05 2010 kevin.huang
 * [BORA00000603][WIFISYS] [New Feature] AAA Module Support
 * Revise data structure to share the same BSS_INFO_T for avoiding coding error
 *
 * 02 04 2010 kevin.huang
 * [BORA00000603][WIFISYS] [New Feature] AAA Module Support
 * Add AAA Module Support, Revise Net Type to Net Type Index for array lookup
 *
 * 01 27 2010 tehuang.liu
 * [BORA00000569][WIFISYS] Phase 2 Integration Test
 * Set max AMDPU size supported by the peer to 64 KB,
 * removed mqmInit() and mqmTxSendAddBaReq() function calls in aisUpdateBssInfo()
 *
 * 01 27 2010 wh.su
 * [BORA00000476][Wi-Fi][firmware] Add the security module initialize code
 * add and fixed some security function.
 *
 * 01 22 2010 cm.chang
 * [BORA00000018]Integrate WIFI part into BORA for the 1st time
 * Support protection and bandwidth switch
 *
 * 01 20 2010 kevin.huang
 * [BORA00000569][WIFISYS] Phase 2 Integration Test
 * Add PHASE_2_INTEGRATION_WORK_AROUND and CFG_SUPPORT_BCM flags
 *
 * 01 15 2010 tehuang.liu
 * [BORA00000018]Integrate WIFI part into BORA for the 1st time
 * Configured the AMPDU factor to 3 for the APu1rwduu`wvpghlqg|q`mpdkb+ilp
 *
 * 01 14 2010 chinghwa.yu
 * [BORA00000563]Add WiFi CoEx BCM module
 * Add WiFi BCM module for the 1st time.
 *
 * 01 11 2010 kevin.huang
 * [BORA00000018]Integrate WIFI part into BORA for the 1st time
 * Add Deauth and Disassoc Handler
 *
 * 01 07 2010 kevin.huang
 * [BORA00000018]Integrate WIFI part into BORA for the 1st time
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 *
 * Refine JOIN Complete and seperate the function of Media State indication
 *
 * 01 04 2010 tehuang.liu
 * [BORA00000018]Integrate WIFI part into BORA for the 1st time
 * For working out the first connection Chariot-verified version
 *
 * 12 18 2009 cm.chang
 * [BORA00000018]Integrate WIFI part into BORA for the 1st time
 * .
 *
 * Dec 10 2009 mtk01088
 * [BORA00000476] [Wi-Fi][firmware] Add the security module initialize code
 * adding the sample code to update the wlan table rate,
 *
 * Dec 10 2009 mtk01104
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Different function prototype of wifi_send_msg()
 *
 * Dec 9 2009 mtk01104
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Call rlm related function to process HT info when join complete
 *
 * Dec 9 2009 mtk01088
 * [BORA00000476] [Wi-Fi][firmware] Add the security module initialize code
 * default the acquired wlan table entry code off
 *
 * Dec 9 2009 mtk01088
 * [BORA00000476] [Wi-Fi][firmware] Add the security module initialize code
 * adding the code to acquired the wlan table entry, and a sample code to update the BA bit at table
 *
 * Dec 7 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Fix the problem of prSwRfb overwrited by event packet in aisFsmRunEventJoinComplete()
 *
 * Dec 4 2009 mtk01088
 * [BORA00000476] [Wi-Fi][firmware] Add the security module initialize code
 * adding the code to integrate the security related code
 *
 * Dec 3 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Remove redundant declaration
 *
 * Dec 3 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Add code for JOIN init and JOIN complete
 *
 * Nov 30 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Rename u4RSSI to i4RSSI
 *
 * Nov 30 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Revise ENUM_MEDIA_STATE to ENUM_PARAM_MEDIA_STATE
 *
 * Nov 30 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Add fgIsScanReqIssued to CONNECTION_SETTINGS_T
 *
 * Nov 26 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Revise Virtual CMD handler due to structure changed
 *
 * Nov 25 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Add Virtual CMD & RESP for testing CMD PATH
 *
 * Nov 23 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Add aisFsmInitializeConnectionSettings()
 *
 * Nov 20 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 * Add CFG_TEST_MGMT_FSM flag for aisFsmTest()
 *
 * Nov 16 2009 mtk01461
 * [BORA00000018] Integrate WIFI part into BORA for the 1st time
 *
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define AIS_ROAMING_CONNECTION_TRIAL_LIMIT  2
#define AIS_JOIN_TIMEOUT                    7

#define CTIA_MAGIC_SSID                     "ctia_test_only_*#*#3646633#*#*"
#define CTIA_MAGIC_SSID_LEN                 30

#define AIS_FSM_STATE_SEARCH_ACTION_PHASE_0	0
#define AIS_FSM_STATE_SEARCH_ACTION_PHASE_1	1
#define AIS_FSM_STATE_SEARCH_ACTION_PHASE_2	2

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
#if DBG
/*lint -save -e64 Type mismatch */
static PUINT_8 apucDebugAisState[AIS_STATE_NUM] = {
	(PUINT_8) DISP_STRING("AIS_STATE_IDLE"),
	(PUINT_8) DISP_STRING("AIS_STATE_SEARCH"),
	(PUINT_8) DISP_STRING("AIS_STATE_SCAN"),
	(PUINT_8) DISP_STRING("AIS_STATE_ONLINE_SCAN"),
	(PUINT_8) DISP_STRING("AIS_STATE_LOOKING_FOR"),
	(PUINT_8) DISP_STRING("AIS_STATE_WAIT_FOR_NEXT_SCAN"),
	(PUINT_8) DISP_STRING("AIS_STATE_REQ_CHANNEL_JOIN"),
	(PUINT_8) DISP_STRING("AIS_STATE_JOIN"),
	(PUINT_8) DISP_STRING("AIS_STATE_JOIN_FAILURE"),
	(PUINT_8) DISP_STRING("AIS_STATE_IBSS_ALONE"),
	(PUINT_8) DISP_STRING("AIS_STATE_IBSS_MERGE"),
	(PUINT_8) DISP_STRING("AIS_STATE_NORMAL_TR"),
	(PUINT_8) DISP_STRING("AIS_STATE_DISCONNECTING"),
	(PUINT_8) DISP_STRING("AIS_STATE_REQ_REMAIN_ON_CHANNEL"),
	(PUINT_8) DISP_STRING("AIS_STATE_REMAIN_ON_CHANNEL")
};

/*lint -restore */
#endif				/* DBG */

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static VOID
aisFsmRunEventScanDoneTimeOut (
	IN P_ADAPTER_T prAdapter,
	ULONG ulParam
	);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* @brief the function is used to initialize the value of the connection settings for
*        AIS network
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisInitializeConnectionSettings(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_8 aucAnyBSSID[] = BC_BSSID;
	UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
	int i = 0;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* Setup default values for operation */
	COPY_MAC_ADDR(prConnSettings->aucMacAddress, aucZeroMacAddr);

	prConnSettings->ucDelayTimeOfDisconnectEvent = AIS_DELAY_TIME_OF_DISCONNECT_SEC;

	COPY_MAC_ADDR(prConnSettings->aucBSSID, aucAnyBSSID);
	prConnSettings->fgIsConnByBssidIssued = FALSE;

	prConnSettings->eReConnectLevel = RECONNECT_LEVEL_MIN;
	prConnSettings->fgIsConnReqIssued = FALSE;
	prConnSettings->fgIsDisconnectedByNonRequest = FALSE;

	prConnSettings->ucSSIDLen = 0;

	prConnSettings->eOPMode = NET_TYPE_INFRA;

	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;

	if (prRegInfo) {
		prConnSettings->ucAdHocChannelNum =
		    (UINT_8) nicFreq2ChannelNum(prRegInfo->u4StartFreq);
		prConnSettings->eAdHocBand = prRegInfo->u4StartFreq < 5000000 ? BAND_2G4 : BAND_5G;
		prConnSettings->eAdHocMode = (ENUM_PARAM_AD_HOC_MODE_T) (prRegInfo->u4AdhocMode);
	}

	prConnSettings->eAuthMode = AUTH_MODE_OPEN;

	prConnSettings->eEncStatus = ENUM_ENCRYPTION_DISABLED;

	prConnSettings->fgIsScanReqIssued = FALSE;

	/* MIB attributes */
	prConnSettings->u2BeaconPeriod = DOT11_BEACON_PERIOD_DEFAULT;

	prConnSettings->u2RTSThreshold = DOT11_RTS_THRESHOLD_DEFAULT;

	prConnSettings->u2DesiredNonHTRateSet = RATE_SET_ALL_ABG;

	/* prConnSettings->u4FreqInKHz; */ /* Center frequency */


	/* Set U-APSD AC */
	prConnSettings->bmfgApsdEnAc = PM_UAPSD_NONE;

	secInit(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* Features */
	prConnSettings->fgIsEnableRoaming = FALSE;

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	prConnSettings->fgSecModeChangeStartTimer = FALSE;
#endif

#if CFG_SUPPORT_ROAMING
	if (prRegInfo) {
		prConnSettings->fgIsEnableRoaming =
		    ((prRegInfo->fgDisRoaming > 0) ? (FALSE) : (TRUE));
	}
#endif				/* CFG_SUPPORT_ROAMING */

	prConnSettings->fgIsAdHocQoSEnable = FALSE;

#if CFG_SUPPORT_802_11AC
	prConnSettings->eDesiredPhyConfig = PHY_CONFIG_802_11ABGNAC;
#else
	prConnSettings->eDesiredPhyConfig = PHY_CONFIG_802_11ABGN;
#endif

	/* Set default bandwidth modes */
	prConnSettings->uc2G4BandwidthMode = CONFIG_BW_20M;
	prConnSettings->uc5GBandwidthMode = CONFIG_BW_20_40M;

	prConnSettings->rRsnInfo.ucElemId = 0x30;
	prConnSettings->rRsnInfo.u2Version = 0x0001;
	prConnSettings->rRsnInfo.u4GroupKeyCipherSuite = 0;
	prConnSettings->rRsnInfo.u4PairwiseKeyCipherSuiteCount = 0;
	for (i = 0; i < MAX_NUM_SUPPORTED_CIPHER_SUITES; i++)
		prConnSettings->rRsnInfo.au4PairwiseKeyCipherSuite[i] = 0;
	prConnSettings->rRsnInfo.u4AuthKeyMgtSuiteCount = 0;
	for (i = 0; i < MAX_NUM_SUPPORTED_AKM_SUITES; i++)
		prConnSettings->rRsnInfo.au4AuthKeyMgtSuite[i] = 0;
	prConnSettings->rRsnInfo.u2RsnCap = 0;
	prConnSettings->rRsnInfo.fgRsnCapPresent = FALSE;

	return;
}				/* end of aisFsmInitializeConnectionSettings() */


/*----------------------------------------------------------------------------*/
/*!
* @brief the function is used to initialize the value in AIS_FSM_INFO_T for
*        AIS FSM operation
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmInit(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;

	DEBUGFUNC("aisFsmInit()");
	DBGLOG(SW1, INFO, ("->aisFsmInit()\n"));

	prAdapter->prAisBssInfo = prAisBssInfo =
	    cnmGetBssInfoAndInit(prAdapter, NETWORK_TYPE_AIS, FALSE);
	ASSERT(prAisBssInfo);

	/* update MAC address */
	COPY_MAC_ADDR(prAdapter->prAisBssInfo->aucOwnMacAddr, prAdapter->rMyMacAddr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	/* 4 <1> Initiate FSM */
	prAisFsmInfo->ePreviousState = AIS_STATE_IDLE;
	prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;

	prAisFsmInfo->ucAvailableAuthTypes = 0;

	prAisFsmInfo->prTargetBssDesc = (P_BSS_DESC_T) NULL;

	prAisFsmInfo->ucSeqNumOfReqMsg = 0;
	prAisFsmInfo->ucSeqNumOfChReq = 0;
	prAisFsmInfo->ucSeqNumOfScanReq = 0;

	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;
#if CFG_SUPPORT_ROAMING
	prAisFsmInfo->fgIsRoamingScanPending = FALSE;
#endif				/* CFG_SUPPORT_ROAMING */
	prAisFsmInfo->fgIsChannelRequested = FALSE;
	prAisFsmInfo->fgIsChannelGranted = FALSE;

	/* 4 <1.1> Initiate FSM - Timer INIT */
	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rBGScanTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventBGSleepTimeOut, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rIbssAloneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventIbssAloneTimeOut, (ULONG) NULL);

	prAisFsmInfo->u4PostponeIndStartTime = 0;
	
	cnmTimerInitTimer(prAdapter,
					&prAisFsmInfo->rScanDoneTimer,
					(PFN_MGMT_TIMEOUT_FUNC)aisFsmRunEventScanDoneTimeOut,
					(ULONG)NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rJoinTimeoutTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventJoinTimeout, (ULONG) NULL);

	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rDeauthDoneTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventDeauthTimeout, (ULONG) NULL);
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	cnmTimerInitTimer(prAdapter,
			  &prAisFsmInfo->rSecModeChangeTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) aisFsmRunEventSecModeChangeTimeout, (ULONG) NULL);
#endif
	/* 4 <1.2> Initiate PWR STATE */
	SET_NET_PWR_STATE_IDLE(prAdapter, prAisBssInfo->ucBssIndex);


	/* 4 <2> Initiate BSS_INFO_T - common part */
	BSS_INFO_INIT(prAdapter, prAisBssInfo);
	COPY_MAC_ADDR(prAisBssInfo->aucOwnMacAddr, prAdapter->rWifiVar.aucMacAddress);

	/* 4 <3> Initiate BSS_INFO_T - private part */
	/* TODO */
	prAisBssInfo->eBand = BAND_2G4;
	prAisBssInfo->ucPrimaryChannel = 1;
	prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;

	/* 4 <4> Allocate MSDU_INFO_T for Beacon */
	prAisBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
						OFFSET_OF(WLAN_BEACON_FRAME_T,
							  aucInfoElem[0]) + MAX_IE_LENGTH);

	if (prAisBssInfo->prBeacon) {
		prAisBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
		prAisBssInfo->prBeacon->ucStaRecIndex = 0xFF;	/* NULL STA_REC */
	} else {
		ASSERT(0);
	}
	/* secGetBmcWlanIndex(prAdapter, NETWORK_TYPE_AIS, prAisBssInfo->ucBssIndex); */
	prAisBssInfo->ucBMCWlanIndex = WTBL_RESERVED_ENTRY;


#if 0
	prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = PM_UAPSD_ALL;
	prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = PM_UAPSD_ALL;
	prAisBssInfo->rPmProfSetupInfo.ucUapsdSp = WMM_MAX_SP_LENGTH_2;
#else
	if (prAdapter->u4UapsdAcBmp == 0) {
		prAdapter->u4UapsdAcBmp = CFG_INIT_UAPSD_AC_BMP;
		/* ASSERT(prAdapter->u4UapsdAcBmp); */
	}
	prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC = (UINT_8) prAdapter->u4UapsdAcBmp;
	prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC = (UINT_8) prAdapter->u4UapsdAcBmp;
	prAisBssInfo->rPmProfSetupInfo.ucUapsdSp = (UINT_8) prAdapter->u4MaxSpLen;
#endif

	/* request list initialization */
	LINK_INITIALIZE(&prAisFsmInfo->rPendingReqList);

	/* DBGPRINTF("[2] ucBmpDeliveryAC:0x%x, ucBmpTriggerAC:0x%x, ucUapsdSp:0x%x", */
	/* prAisBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC, */
	/* prAisBssInfo->rPmProfSetupInfo.ucBmpTriggerAC, */
	/* prAisBssInfo->rPmProfSetupInfo.ucUapsdSp); */

	/* Bind NetDev & BssInfo */
	/* wlanBindBssIdxToNetInterface(prAdapter->prGlueInfo, NET_DEV_WLAN_IDX, prAisBssInfo->ucBssIndex); */

	return;
}				/* end of aisFsmInit() */

/*----------------------------------------------------------------------------*/
/*!
* @brief the function is used to uninitialize the value in AIS_FSM_INFO_T for
*        AIS FSM operation
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmUninit(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;

	DEBUGFUNC("aisFsmUninit()");
	DBGLOG(SW1, INFO, ("->aisFsmUninit()\n"));

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

	/* 4 <1> Stop all timers */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rBGScanTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rSecModeChangeTimer);
#endif
	/* 4 <2> flush pending request */
	aisFsmFlushRequest(prAdapter);

	/* 4 <3> Reset driver-domain BSS-INFO */
	if (prAisBssInfo->prBeacon) {
		cnmMgtPktFree(prAdapter, prAisBssInfo->prBeacon);
		prAisBssInfo->prBeacon = NULL;
	}
#if CFG_SUPPORT_802_11W
	rsnStopSaQuery(prAdapter);
#endif

	if (prAisBssInfo) {
		cnmFreeBssInfo(prAdapter, prAisBssInfo);
		prAdapter->prAisBssInfo = NULL;
	}

	return;
}				/* end of aisFsmUninit() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Initialization of JOIN STATE
*
* @param[in] prBssDesc  The pointer of BSS_DESC_T which is the BSS we will try to join with.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateInit_JOIN(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_SPECIFIC_BSS_INFO_T prAisSpecificBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_STA_RECORD_T prStaRec;
	P_MSG_JOIN_REQ_T prJoinReqMsg;

	DEBUGFUNC("aisFsmStateInit_JOIN()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisSpecificBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	ASSERT(prBssDesc);

	/* 4 <1> We are going to connect to this BSS. */
	prBssDesc->fgIsConnecting = TRUE;


	/* 4 <2> Setup corresponding STA_RECORD_T */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter,
					      STA_TYPE_LEGACY_AP,
					      prAdapter->prAisBssInfo->ucBssIndex, prBssDesc);

	prAisFsmInfo->prTargetStaRec = prStaRec;

	/* 4 <2.1> sync. to firmware domain */
	if (prStaRec->ucStaState == STA_STATE_1)
		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);


	/* 4 <3> Update ucAvailableAuthTypes which we can choice during SAA */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {

		prStaRec->fgIsReAssoc = FALSE;

		switch (prConnSettings->eAuthMode) {
		case AUTH_MODE_OPEN:	/* Note: Omit break here. */
		case AUTH_MODE_WPA:
		case AUTH_MODE_WPA_PSK:
		case AUTH_MODE_WPA2:
		case AUTH_MODE_WPA2_PSK:
			prAisFsmInfo->ucAvailableAuthTypes = (UINT_8) AUTH_TYPE_OPEN_SYSTEM;
			break;


		case AUTH_MODE_SHARED:
			prAisFsmInfo->ucAvailableAuthTypes = (UINT_8) AUTH_TYPE_SHARED_KEY;
			break;


		case AUTH_MODE_AUTO_SWITCH:
			DBGLOG(AIS, LOUD, ("JOIN INIT: eAuthMode == AUTH_MODE_AUTO_SWITCH\n"));
			prAisFsmInfo->ucAvailableAuthTypes = (UINT_8) (AUTH_TYPE_OPEN_SYSTEM |
								       AUTH_TYPE_SHARED_KEY);
			break;

		default:
			ASSERT(!(prConnSettings->eAuthMode == AUTH_MODE_WPA_NONE));
			DBGLOG(AIS, ERROR,
			       ("JOIN INIT: Auth Algorithm : %d was not supported by JOIN\n",
				prConnSettings->eAuthMode));
			/* TODO(Kevin): error handling ? */
			return;
		}

		/* TODO(tyhsu): Assume that Roaming Auth Type is equal to ConnSettings eAuthMode */
		prAisSpecificBssInfo->ucRoamingAuthTypes = prAisFsmInfo->ucAvailableAuthTypes;

		prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT;

	} else {
		ASSERT(prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE);
		ASSERT(!prBssDesc->fgIsConnected);

		DBGLOG(AIS, LOUD, ("JOIN INIT: AUTH TYPE = %d for Roaming\n",
				   prAisSpecificBssInfo->ucRoamingAuthTypes));


		prStaRec->fgIsReAssoc = TRUE;	/* We do roaming while the medium is connected */

		/* TODO(Kevin): We may call a sub function to acquire the Roaming Auth Type */
		prAisFsmInfo->ucAvailableAuthTypes = prAisSpecificBssInfo->ucRoamingAuthTypes;

		prStaRec->ucTxAuthAssocRetryLimit = TX_AUTH_ASSOCI_RETRY_LIMIT_FOR_ROAMING;
	}


	/* 4 <4> Use an appropriate Authentication Algorithm Number among the ucAvailableAuthTypes */
	if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_OPEN_SYSTEM) {

		DBGLOG(AIS, LOUD,
		       ("JOIN INIT: Try to do Authentication with AuthType == OPEN_SYSTEM.\n"));
		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_OPEN_SYSTEM;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_OPEN_SYSTEM;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_SHARED_KEY) {

		DBGLOG(AIS, LOUD,
		       ("JOIN INIT: Try to do Authentication with AuthType == SHARED_KEY.\n"));

		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_SHARED_KEY;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_SHARED_KEY;
	} else if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_FAST_BSS_TRANSITION) {

		DBGLOG(AIS, LOUD,
		       ("JOIN INIT: Try to do Authentication with AuthType == FAST_BSS_TRANSITION.\n"));

		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_FAST_BSS_TRANSITION;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_FAST_BSS_TRANSITION;
	} else {
		ASSERT(0);
	}

	/* 4 <5> Overwrite Connection Setting for eConnectionPolicy == ANY (Used by Assoc Req) */
	if (prBssDesc->ucSSIDLen) {
		COPY_SSID(prConnSettings->aucSSID,
			  prConnSettings->ucSSIDLen, prBssDesc->aucSSID, prBssDesc->ucSSIDLen);
	}

	/* 4 <6> Send a Msg to trigger SAA to start JOIN process. */
	prJoinReqMsg =
	    (P_MSG_JOIN_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_REQ_T));
	if (!prJoinReqMsg) {

		ASSERT(0);	/* Can't trigger SAA FSM */
		return;
	}

	prJoinReqMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_START;
	prJoinReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinReqMsg->prStaRec = prStaRec;

	if (1) {
		int j;
		P_FRAG_INFO_T prFragInfo;
		for (j = 0; j < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; j++) {
			prFragInfo = &prStaRec->rFragInfo[j];

			if (prFragInfo->pr1stFrag) {
				/* nicRxReturnRFB(prAdapter, prFragInfo->pr1stFrag); */
				prFragInfo->pr1stFrag = (P_SW_RFB_T) NULL;
			}
		}
	}

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinReqMsg, MSG_SEND_METHOD_BUF);

	return;
}				/* end of aisFsmInit_JOIN() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Retry JOIN for AUTH_MODE_AUTO_SWITCH
*
* @param[in] prStaRec       Pointer to the STA_RECORD_T
*
* @retval TRUE      We will retry JOIN
* @retval FALSE     We will not retry JOIN
*/
/*----------------------------------------------------------------------------*/
BOOLEAN aisFsmStateInit_RetryJOIN(IN P_ADAPTER_T prAdapter, P_STA_RECORD_T prStaRec)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_JOIN_REQ_T prJoinReqMsg;

	DEBUGFUNC("aisFsmStateInit_RetryJOIN()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* Retry other AuthType if possible */
	if (!prAisFsmInfo->ucAvailableAuthTypes)
		return FALSE;


	if (prAisFsmInfo->ucAvailableAuthTypes & (UINT_8) AUTH_TYPE_SHARED_KEY) {

		DBGLOG(AIS, INFO,
		       ("RETRY JOIN INIT: Retry Authentication with AuthType == SHARED_KEY.\n"));

		prAisFsmInfo->ucAvailableAuthTypes &= ~(UINT_8) AUTH_TYPE_SHARED_KEY;

		prStaRec->ucAuthAlgNum = (UINT_8) AUTH_ALGORITHM_NUM_SHARED_KEY;
	} else {
		DBGLOG(AIS, ERROR,
		       ("RETRY JOIN INIT: Retry Authentication with Unexpected AuthType.\n"));
		ASSERT(0);
	}

	prAisFsmInfo->ucAvailableAuthTypes = 0;	/* No more available Auth Types */

	/* Trigger SAA to start JOIN process. */
	prJoinReqMsg =
	    (P_MSG_JOIN_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_REQ_T));
	if (!prJoinReqMsg) {

		ASSERT(0);	/* Can't trigger SAA FSM */
		return FALSE;
	}

	prJoinReqMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_START;
	prJoinReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinReqMsg->prStaRec = prStaRec;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinReqMsg, MSG_SEND_METHOD_BUF);

	return TRUE;

}				/* end of aisFsmRetryJOIN() */


#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
* @brief State Initialization of AIS_STATE_IBSS_ALONE
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateInit_IBSS_ALONE(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;

	/* 4 <1> Check if IBSS was created before ? */
	if (prAisBssInfo->fgIsBeaconActivated) {

		/* 4 <2> Start IBSS Alone Timer for periodic SCAN and then SEARCH */
#if !CFG_SLT_SUPPORT
		cnmTimerStartTimer(prAdapter,
				   &prAisFsmInfo->rIbssAloneTimer,
				   SEC_TO_MSEC(AIS_IBSS_ALONE_TIMEOUT_SEC));
#endif
	}

	aisFsmCreateIBSS(prAdapter);

	return;
}				/* end of aisFsmStateInit_IBSS_ALONE() */


/*----------------------------------------------------------------------------*/
/*!
* @brief State Initialization of AIS_STATE_IBSS_MERGE
*
* @param[in] prBssDesc  The pointer of BSS_DESC_T which is the IBSS we will try to merge with.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateInit_IBSS_MERGE(IN P_ADAPTER_T prAdapter, P_BSS_DESC_T prBssDesc)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	ASSERT(prBssDesc);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;

	/* 4 <1> We will merge with to this BSS immediately. */
	prBssDesc->fgIsConnecting = FALSE;
	prBssDesc->fgIsConnected = TRUE;

	/* 4 <2> Setup corresponding STA_RECORD_T */
	prStaRec = bssCreateStaRecFromBssDesc(prAdapter,
					      STA_TYPE_ADHOC_PEER,
					      prAdapter->prAisBssInfo->ucBssIndex, prBssDesc);

	prStaRec->fgIsMerging = TRUE;

	prAisFsmInfo->prTargetStaRec = prStaRec;

	/* 4 <2.1> sync. to firmware domain */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	/* 4 <3> IBSS-Merge */
	aisFsmMergeIBSS(prAdapter, prStaRec);

	return;
}				/* end of aisFsmStateInit_IBSS_MERGE() */

#endif				/* CFG_SUPPORT_ADHOC */


/*----------------------------------------------------------------------------*/
/*!
* @brief Process of JOIN Abort
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateAbort_JOIN(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_JOIN_ABORT_T prJoinAbortMsg;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* 1. Abort JOIN process */
	prJoinAbortMsg =
	    (P_MSG_JOIN_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_JOIN_ABORT_T));
	if (!prJoinAbortMsg) {

		ASSERT(0);	/* Can't abort SAA FSM */
		return;
	}

	prJoinAbortMsg->rMsgHdr.eMsgId = MID_AIS_SAA_FSM_ABORT;
	prJoinAbortMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfReqMsg;
	prJoinAbortMsg->prStaRec = prAisFsmInfo->prTargetStaRec;

	scanRemoveConnFlagOfBssDescByBssid(prAdapter, prAisFsmInfo->prTargetStaRec->aucMacAddr);

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prJoinAbortMsg, MSG_SEND_METHOD_BUF);

	/* 2. Return channel privilege */
	aisFsmReleaseCh(prAdapter);

	/* 3.1 stop join timeout timer */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);

	/* 3.2 reset local variable */
	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;

	return;
}				/* end of aisFsmAbortJOIN() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Process of SCAN Abort
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateAbort_SCAN(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_SCN_SCAN_CANCEL prScanCancelMsg;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

    DBGLOG(AIS, STATE, ("aisFsmStateAbort_SCAN \n"));

	/* Abort JOIN process. */
	prScanCancelMsg =
	    (P_MSG_SCN_SCAN_CANCEL) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						sizeof(MSG_SCN_SCAN_CANCEL));
	if (!prScanCancelMsg) {

		ASSERT(0);	/* Can't abort SCN FSM */
		return;
	}

	prScanCancelMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_CANCEL;
	prScanCancelMsg->ucSeqNum = prAisFsmInfo->ucSeqNumOfScanReq;
	prScanCancelMsg->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
	prScanCancelMsg->fgIsChannelExt = FALSE;

	/* unbuffered message to guarantee scan is cancelled in sequence */
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prScanCancelMsg, MSG_SEND_METHOD_UNBUF);

	return;
}				/* end of aisFsmAbortSCAN() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Process of NORMAL_TR Abort
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateAbort_NORMAL_TR(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* TODO(Kevin): Do abort other MGMT func */

	/* 1. Release channel to CNM */
	aisFsmReleaseCh(prAdapter);

	/* 2.1 stop join timeout timer */
	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rJoinTimeoutTimer);

	/* 2.2 reset local variable */
	prAisFsmInfo->fgIsInfraChannelFinished = TRUE;

	return;
}				/* end of aisFsmAbortNORMAL_TR() */


#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
* @brief Process of NORMAL_TR Abort
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmStateAbort_IBSS(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_DESC_T prBssDesc;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* reset BSS-DESC */
	if (prAisFsmInfo->prTargetStaRec) {
		prBssDesc = scanSearchBssDescByTA(prAdapter,
						  prAisFsmInfo->prTargetStaRec->aucMacAddr);

		if (prBssDesc) {
			prBssDesc->fgIsConnected = FALSE;
			prBssDesc->fgIsConnecting = FALSE;
		}
	}
	/* release channel privilege */
	aisFsmReleaseCh(prAdapter);

	return;
}
#endif				/* CFG_SUPPORT_ADHOC */


/*----------------------------------------------------------------------------*/
/*!
* @brief The Core FSM engine of AIS(Ad-hoc, Infra STA)
*
* @param[in] eNextState Enum value of next AIS STATE
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmSteps(IN P_ADAPTER_T prAdapter, ENUM_AIS_STATE_T eNextState)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_DESC_T prBssDesc;
	P_MSG_CH_REQ_T prMsgChReq;
	P_MSG_SCN_SCAN_REQ_V2 prScanReqMsg;
	P_AIS_REQ_HDR_T prAisReq;
	ENUM_BAND_T eBand;
	UINT_8 ucChannel;
	UINT_16 u2ScanIELen;

	BOOLEAN fgIsTransition = (BOOLEAN) FALSE;

	DEBUGFUNC("aisFsmSteps()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	do {

		/* Do entering Next State */
		prAisFsmInfo->ePreviousState = prAisFsmInfo->eCurrentState;

#if DBG
		DBGLOG(AIS, STATE, ("TRANSITION: [%s] -> [%s]\n",
				    apucDebugAisState[prAisFsmInfo->eCurrentState],
				    apucDebugAisState[eNextState]));
#else
		DBGLOG(AIS, STATE, ("[%d] TRANSITION: [%d] -> [%d]\n",
				    DBG_AIS_IDX, prAisFsmInfo->eCurrentState, eNextState));
#endif
		/* NOTE(Kevin): This is the only place to change the eCurrentState(except initial) */
		prAisFsmInfo->eCurrentState = eNextState;

		fgIsTransition = (BOOLEAN) FALSE;

		/* firstly, check if we have started postpone indication. if we're not in
			req channel/join/search state, it's the time to check postpone indication timeout.
			otherwise, give a chance to do join before indicate to host */
		if (prAisFsmInfo->u4PostponeIndStartTime > 0 &&
			prAisFsmInfo->eCurrentState != AIS_STATE_JOIN &&
			prAisFsmInfo->eCurrentState != AIS_STATE_SEARCH &&
			prAisFsmInfo->eCurrentState != AIS_STATE_REQ_CHANNEL_JOIN &&
			CHECK_FOR_TIMEOUT(kalGetTimeTick(), prAisFsmInfo->u4PostponeIndStartTime,
				SEC_TO_MSEC(prConnSettings->ucDelayTimeOfDisconnectEvent)))
			aisPostponedEventOfDisconnTimeout(prAdapter);

		/* Do tasks of the State that we just entered */
		switch (prAisFsmInfo->eCurrentState) {
			/* NOTE(Kevin): we don't have to rearrange the sequence of following
			 * switch case. Instead I would like to use a common lookup table of array
			 * of function pointer to speed up state search.
			 */
		case AIS_STATE_IDLE:

			prAisReq = aisFsmGetNextRequest(prAdapter);
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);

			if (prAisReq == NULL || prAisReq->eReqType == AIS_REQUEST_RECONNECT) {
				if (prConnSettings->fgIsConnReqIssued == TRUE &&
				    prConnSettings->fgIsDisconnectedByNonRequest == FALSE) {

					prAisFsmInfo->fgTryScan = TRUE;
					if (!IS_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex)) {
						SET_NET_ACTIVE(prAdapter,
						       prAdapter->prAisBssInfo->ucBssIndex);
						/* sync with firmware */
						nicActivateNetwork(prAdapter,
								   prAdapter->prAisBssInfo->ucBssIndex);
											prAisBssInfo->fgIsNetRequestInActive = FALSE;
					}
					SET_NET_PWR_STATE_ACTIVE(prAdapter,
								 prAdapter->prAisBssInfo->
								 ucBssIndex);
					/* reset trial count */
					prAisFsmInfo->ucConnTrialCount = 0;

					eNextState = AIS_STATE_SEARCH;
					fgIsTransition = TRUE;
				} else {
					SET_NET_PWR_STATE_IDLE(prAdapter,
							       prAdapter->prAisBssInfo->ucBssIndex);

					/* sync with firmware */
#if CFG_SUPPORT_PNO
                                prAisBssInfo->fgIsNetRequestInActive = TRUE;
                                if(prAisBssInfo->fgIsPNOEnable){
                                    DBGLOG(BSS, INFO, ("[wlan index][Network]=%d fgIsPNOEnable && OP_MODE_INFRASTRUCTURE, KEEP ACTIVE \n", prAisBssInfo->ucBssIndex));
                                }
                                else
#endif                                           
                                { 
					    UNSET_NET_ACTIVE(prAdapter,
							     prAdapter->prAisBssInfo->ucBssIndex);
                                            nicDeactivateNetwork(prAdapter, 
                                                             prAdapter->prAisBssInfo->ucBssIndex);
                                }

					/* check for other pending request */
					if (prAisReq && (aisFsmIsRequestPending
						    (prAdapter, AIS_REQUEST_SCAN, TRUE) == TRUE)) {
						wlanClearScanningResult(prAdapter);
						eNextState = AIS_STATE_SCAN;

						fgIsTransition = TRUE;
					}
				}

				if (prAisReq) {
					/* free the message */
					cnmMemFree(prAdapter, prAisReq);
				}
			} else if (prAisReq->eReqType == AIS_REQUEST_SCAN) {
#if CFG_SUPPORT_ROAMING
				prAisFsmInfo->fgIsRoamingScanPending = FALSE;
#endif				/* CFG_SUPPORT_ROAMING */
				wlanClearScanningResult(prAdapter);

				eNextState = AIS_STATE_SCAN;
				fgIsTransition = TRUE;

				/* free the message */
				cnmMemFree(prAdapter, prAisReq);
			} else if (prAisReq->eReqType == AIS_REQUEST_ROAMING_CONNECT
				   || prAisReq->eReqType == AIS_REQUEST_ROAMING_SEARCH) {
				/* ignore */
				/* free the message */
				cnmMemFree(prAdapter, prAisReq);
			} else if (prAisReq->eReqType == AIS_REQUEST_REMAIN_ON_CHANNEL) {
				eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;
				fgIsTransition = TRUE;

				/* free the message */
				cnmMemFree(prAdapter, prAisReq);
			}

			prAisFsmInfo->u4SleepInterval = AIS_BG_SCAN_INTERVAL_MIN_SEC;

			break;

		case AIS_STATE_SEARCH:
			/* 4 <1> Search for a matched candidate and save it to prTargetBssDesc. */
#if CFG_SLT_SUPPORT
			prBssDesc = prAdapter->rWifiVar.rSltInfo.prPseudoBssDesc;
#else
			prBssDesc =
			    scanSearchBssDescByPolicy(prAdapter,
						      prAdapter->prAisBssInfo->ucBssIndex);
#endif

			/* we are under Roaming Condition. */
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
				if (prAisFsmInfo->ucConnTrialCount >
				    AIS_ROAMING_CONNECTION_TRIAL_LIMIT) {
#if CFG_SUPPORT_ROAMING
					roamingFsmRunEventFail(prAdapter,
							       ROAMING_FAIL_REASON_CONNLIMIT);
#endif				/* CFG_SUPPORT_ROAMING */
					/* reset retry count */
					prAisFsmInfo->ucConnTrialCount = 0;

					/* abort connection trial */
					if (prConnSettings->eReConnectLevel < RECONNECT_LEVEL_BEACON_TIMEOUT) {
						prConnSettings->eReConnectLevel = RECONNECT_LEVEL_ROAMING_FAIL;
						prConnSettings->fgIsConnReqIssued = FALSE;
					} else {
						DBGLOG(AIS, INFO,
						       ("Do not set fgIsConnReqIssued, Level is %d\n",
						       prConnSettings->eReConnectLevel));
					}

					eNextState = AIS_STATE_NORMAL_TR;
					fgIsTransition = TRUE;

					break;
				}
			}
			/* 4 <2> We are not under Roaming Condition. */
			if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {

				/* 4 <2.a> If we have the matched one */
				if (prBssDesc) {

					/* 4 <A> Stored the Selected BSS security cipher. */
					/* or later asoc req compose IE */
					prAisBssInfo->u4RsnSelectedGroupCipher =
					    prBssDesc->u4RsnSelectedGroupCipher;
					prAisBssInfo->u4RsnSelectedPairwiseCipher =
					    prBssDesc->u4RsnSelectedPairwiseCipher;
					prAisBssInfo->u4RsnSelectedAKMSuite =
					    prBssDesc->u4RsnSelectedAKMSuite;

					/* 4 <B> Do STATE transition and update current Operation Mode. */
					if (prBssDesc->eBSSType == BSS_TYPE_INFRASTRUCTURE) {

						prAisBssInfo->eCurrentOPMode =
						    OP_MODE_INFRASTRUCTURE;

						/* Record the target BSS_DESC_T for next STATE. */
						prAisFsmInfo->prTargetBssDesc = prBssDesc;

						/* Transit to channel acquire */
						eNextState = AIS_STATE_REQ_CHANNEL_JOIN;
						fgIsTransition = TRUE;

						/* increase connection trial count */
						prAisFsmInfo->ucConnTrialCount++;
					}
#if CFG_SUPPORT_ADHOC
					else if (prBssDesc->eBSSType == BSS_TYPE_IBSS) {

						prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

						/* Record the target BSS_DESC_T for next STATE. */
						prAisFsmInfo->prTargetBssDesc = prBssDesc;

						eNextState = AIS_STATE_IBSS_MERGE;
						fgIsTransition = TRUE;
					}
#endif				/* CFG_SUPPORT_ADHOC */
					else {
						ASSERT(0);
						eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
						fgIsTransition = TRUE;
					}
				}
				/* 4 <2.b> If we don't have the matched one */
				else {

					/* increase connection trial count for infrastructure connection */
					if (prConnSettings->eOPMode == NET_TYPE_INFRA)
						prAisFsmInfo->ucConnTrialCount++;

					/* 4 <A> Try to SCAN */
					if (prAisFsmInfo->fgTryScan) {
						eNextState = AIS_STATE_LOOKING_FOR;

						fgIsTransition = TRUE;
					}
					/* 4 <B> We've do SCAN already, now wait in some STATE. */
					else {
						eNextState = aisFsmStateSearchAction(prAdapter,
										AIS_FSM_STATE_SEARCH_ACTION_PHASE_0);
						fgIsTransition = TRUE;
					}
				}
			}
			/* 4 <3> We are under Roaming Condition. */
			else {	/* prAdapter->eConnectionState == MEDIA_STATE_CONNECTED. */

				/* 4 <3.a> This BSS_DESC_T is our AP. */
				/* NOTE(Kevin 2008/05/16): Following cases will go back to NORMAL_TR.
				 * CASE I: During Roaming, APP(WZC/NDISTEST) change the connection
				 *         settings. That make we can NOT match the original AP, so the
				 *         prBssDesc is NULL.
				 * CASE II: The same reason as CASE I. Because APP change the
				 *          eOPMode to other network type in connection setting
				 *          (e.g. NET_TYPE_IBSS), so the BssDesc become the IBSS node.
				 * (For CASE I/II, before WZC/NDISTEST set the OID_SSID, it will change
				 * other parameters in connection setting first. So if we do roaming
				 * at the same time, it will hit these cases.)
				 *
				 * CASE III: Normal case, we can't find other candidate to roam
				 * out, so only the current AP will be matched.
				 *
				 * CASE VI: Timestamp of the current AP might be reset
				 */
				if (prAisBssInfo->ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION &&
					((!prBssDesc) || /* CASE I */
					(prBssDesc->eBSSType != BSS_TYPE_INFRASTRUCTURE) ||	/* CASE II */
					(prBssDesc->fgIsConnected) ||	/* CASE III */
					(EQUAL_MAC_ADDR(prBssDesc->aucBSSID, prAisBssInfo->aucBSSID))) /* CASE VI */) {
#if DBG
					if ((prBssDesc) && (prBssDesc->fgIsConnected)) {
						ASSERT(EQUAL_MAC_ADDR
						       (prBssDesc->aucBSSID,
							prAisBssInfo->aucBSSID));
					}
#endif				/* DBG */
					/* We already associated with it, go back to NORMAL_TR */
					/* TODO(Kevin): Roaming Fail */
#if CFG_SUPPORT_ROAMING
					roamingFsmRunEventFail(prAdapter,
							       ROAMING_FAIL_REASON_NOCANDIDATE);
#endif				/* CFG_SUPPORT_ROAMING */

					/* Retreat to NORMAL_TR state */
					eNextState = AIS_STATE_NORMAL_TR;
					fgIsTransition = TRUE;
				}
				/* 4 <3.b> Try to roam out for JOIN this BSS_DESC_T. */
				else {
					if (prBssDesc == NULL) {
						fgIsTransition = TRUE;
						eNextState = aisFsmStateSearchAction(prAdapter,
										AIS_FSM_STATE_SEARCH_ACTION_PHASE_1);
					} else {
						aisFsmStateSearchAction(prAdapter, AIS_FSM_STATE_SEARCH_ACTION_PHASE_2);
						/* 4 <A> Record the target BSS_DESC_T for next STATE. */
						prAisFsmInfo->prTargetBssDesc = prBssDesc;

						/* tyhsu: increase connection trial count */
						prAisFsmInfo->ucConnTrialCount++;

						/* Transit to channel acquire */
						eNextState = AIS_STATE_REQ_CHANNEL_JOIN;
						fgIsTransition = TRUE;
					}
				}
			}

			break;

		case AIS_STATE_WAIT_FOR_NEXT_SCAN:

			DBGLOG(AIS, LOUD,
			       ("SCAN: Idle Begin - Current Time = %u\n", kalGetTimeTick()));

			cnmTimerStartTimer(prAdapter,
					   &prAisFsmInfo->rBGScanTimer,
					   SEC_TO_MSEC(prAisFsmInfo->u4SleepInterval));

			SET_NET_PWR_STATE_IDLE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

			if (prAisFsmInfo->u4SleepInterval < AIS_BG_SCAN_INTERVAL_MAX_SEC)
				prAisFsmInfo->u4SleepInterval <<= 1;

			break;

		case AIS_STATE_SCAN:
		case AIS_STATE_ONLINE_SCAN:
		case AIS_STATE_LOOKING_FOR:

			if (!IS_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex)) {
				SET_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

				/* sync with firmware */
				nicActivateNetwork(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
                                prAisBssInfo->fgIsNetRequestInActive = FALSE;
			}

			/* IE length decision */
			if (prAisFsmInfo->u4ScanIELength > 0) {
				u2ScanIELen = (UINT_16) prAisFsmInfo->u4ScanIELength;
			} else {
#if CFG_SUPPORT_WPS2
				u2ScanIELen = prAdapter->prGlueInfo->u2WSCIELen;
#else
				u2ScanIELen = 0;
#endif
			}

			prScanReqMsg = (P_MSG_SCN_SCAN_REQ_V2) cnmMemAlloc(prAdapter,
									   RAM_TYPE_MSG,
									   OFFSET_OF
									   (MSG_SCN_SCAN_REQ_V2,
									    aucIE) + u2ScanIELen);
			if (!prScanReqMsg) {
				ASSERT(0);	/* Can't trigger SCAN FSM */
				return;
			}

			prScanReqMsg->rMsgHdr.eMsgId = MID_AIS_SCN_SCAN_REQ_V2;
			prScanReqMsg->ucSeqNum = ++prAisFsmInfo->ucSeqNumOfScanReq;
			prScanReqMsg->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;

#if CFG_SUPPORT_RDD_TEST_MODE
			prScanReqMsg->eScanType = SCAN_TYPE_PASSIVE_SCAN;
#else
			if (prAisFsmInfo->eCurrentState == AIS_STATE_SCAN
			    || prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) {
				if (prAisFsmInfo->ucScanSSIDNum == 0) {
#if CFG_SUPPORT_AIS_PASSIVE_SCAN
					prScanReqMsg->eScanType = SCAN_TYPE_PASSIVE_SCAN;

					prScanReqMsg->ucSSIDType = 0;
					prScanReqMsg->ucSSIDNum = 0;
#else
					prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;

					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
					prScanReqMsg->ucSSIDNum = 0;
#endif
				} else if (prAisFsmInfo->ucScanSSIDNum == 1
					   && prAisFsmInfo->arScanSSID[0].u4SsidLen == 0) {
					prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;

					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
					prScanReqMsg->ucSSIDNum = 0;
				} else {
					prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;

					prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
					prScanReqMsg->ucSSIDNum = prAisFsmInfo->ucScanSSIDNum;
					prScanReqMsg->prSsid = prAisFsmInfo->arScanSSID;
				}
			} else {
				prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;

				COPY_SSID(prAisFsmInfo->rRoamingSSID.aucSsid,
					  prAisFsmInfo->rRoamingSSID.u4SsidLen,
					  prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

				/* Scan for determined SSID */
				prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_SPECIFIED;
				prScanReqMsg->ucSSIDNum = 1;
				prScanReqMsg->prSsid = &(prAisFsmInfo->rRoamingSSID);
			}
#endif

			/* using default channel dwell time/timeout value */
			prScanReqMsg->u2ProbeDelay = 0;
			prScanReqMsg->u2ChannelDwellTime = 0;
			prScanReqMsg->u2TimeoutValue = 0;

			/* check if tethering is running and need to fix on specific channel */
			if (cnmAisInfraChannelFixed(prAdapter, &eBand, &ucChannel) == TRUE) {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
				prScanReqMsg->ucChannelListNum = 1;
				prScanReqMsg->arChnlInfoList[0].eBand = eBand;
				prScanReqMsg->arChnlInfoList[0].ucChannelNum = ucChannel;
			} else if ((prAdapter->prGlueInfo != NULL) && 
				(prAdapter->prGlueInfo->puScanChannel != NULL)) {
				/*handle partional scan channel info*/
				P_PARTIAL_SCAN_INFO channel_t;
				UINT_32	u4size;

				channel_t = (P_PARTIAL_SCAN_INFO)prAdapter->prGlueInfo->puScanChannel;
					
				/*set partional scan channel num*/
				prScanReqMsg->ucChannelListNum = channel_t->ucChannelListNum;
					
				/*set partional scan channel info*/
				u4size = sizeof(channel_t->arChnlInfoList);
					
				DBGLOG(AIS, INFO,
					("SCAN: channel num = %d, total size=%d\n",
					prScanReqMsg->ucChannelListNum, u4size));
					
				kalMemCopy(&(prScanReqMsg->arChnlInfoList), &(channel_t->arChnlInfoList),
					u4size);

				/*clear prGlueInfo partional scan info*/
				prAdapter->prGlueInfo->puScanChannel = NULL;
				/*free partional scan info*/
				kalMemFree(channel_t, VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));

				/*set scan channel type for partional scan*/
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_SPECIFIED;
				   
			} else if (prAdapter->aePreferBand[prAdapter->prAisBssInfo->ucBssIndex] ==
				   BAND_NULL) {
				if (prAdapter->fgEnable5GBand == TRUE)
					prScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
				else
					prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;

			} else if (prAdapter->aePreferBand[prAdapter->prAisBssInfo->ucBssIndex] ==
				   BAND_2G4) {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
			} else if (prAdapter->aePreferBand[prAdapter->prAisBssInfo->ucBssIndex] ==
				   BAND_5G) {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_5G;
			} else {
				prScanReqMsg->eScanChannel = SCAN_CHANNEL_FULL;
				ASSERT(0);
			}

			if (prAisFsmInfo->u4ScanIELength > 0) {
				kalMemCopy(prScanReqMsg->aucIE, prAisFsmInfo->aucScanIEBuf,
					   prAisFsmInfo->u4ScanIELength);
			} else {
#if CFG_SUPPORT_WPS2
				if (prAdapter->prGlueInfo->u2WSCIELen > 0) {
					kalMemCopy(prScanReqMsg->aucIE,
						   &prAdapter->prGlueInfo->aucWSCIE,
						   prAdapter->prGlueInfo->u2WSCIELen);
				}
			}
#endif

			prScanReqMsg->u2IELen = u2ScanIELen;

			mboxSendMsg(prAdapter,
				    MBOX_ID_0, (P_MSG_HDR_T) prScanReqMsg, MSG_SEND_METHOD_BUF);

			prAisFsmInfo->fgTryScan = FALSE;	/* Will enable background sleep for infrastructure */

			break;

		case AIS_STATE_REQ_CHANNEL_JOIN:
			/* send message to CNM for acquiring channel */
			prMsgChReq =
			    (P_MSG_CH_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
							 sizeof(MSG_CH_REQ_T));
			if (!prMsgChReq) {
				ASSERT(0);	/* Can't indicate CNM for channel acquiring */
				return;
			}

			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
			prMsgChReq->u4MaxInterval = AIS_JOIN_CH_REQUEST_INTERVAL;
			prMsgChReq->ucPrimaryChannel = prAisFsmInfo->prTargetBssDesc->ucChannelNum;
			prMsgChReq->eRfSco = prAisFsmInfo->prTargetBssDesc->eSco;
			prMsgChReq->eRfBand = prAisFsmInfo->prTargetBssDesc->eBand;

			/* To do: check if 80/160MHz bandwidth is needed here */
			prMsgChReq->eRfChannelWidth = prAisFsmInfo->prTargetBssDesc->eChannelWidth;
			prMsgChReq->ucRfCenterFreqSeg1 =
			    prAisFsmInfo->prTargetBssDesc->ucCenterFreqS1;
			prMsgChReq->ucRfCenterFreqSeg2 =
			    prAisFsmInfo->prTargetBssDesc->ucCenterFreqS2;

			mboxSendMsg(prAdapter,
				    MBOX_ID_0, (P_MSG_HDR_T) prMsgChReq, MSG_SEND_METHOD_BUF);

			prAisFsmInfo->fgIsChannelRequested = TRUE;
			break;

		case AIS_STATE_JOIN:
			aisFsmStateInit_JOIN(prAdapter, prAisFsmInfo->prTargetBssDesc);
			break;

		case AIS_STATE_JOIN_FAILURE:
			prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;
			prConnSettings->fgIsDisconnectedByNonRequest = TRUE;

#if CFG_SUPPORT_AUTORN
			if (prAisBssInfo->fgDisConnReassoc == TRUE) {
				nicMediaJoinFailure(prAdapter, prAdapter->prAisBssInfo->ucBssIndex,
							WLAN_STATUS_MEDIA_DISCONNECT);
				prAisBssInfo->fgDisConnReassoc = FALSE;
			} else
#endif
			{
				nicMediaJoinFailure(prAdapter, prAdapter->prAisBssInfo->ucBssIndex,
						    WLAN_STATUS_JOIN_TIMEOUT);
			}

			eNextState = AIS_STATE_IDLE;
			fgIsTransition = TRUE;

			break;

#if CFG_SUPPORT_ADHOC
		case AIS_STATE_IBSS_ALONE:
			aisFsmStateInit_IBSS_ALONE(prAdapter);
			break;

		case AIS_STATE_IBSS_MERGE:
			aisFsmStateInit_IBSS_MERGE(prAdapter, prAisFsmInfo->prTargetBssDesc);
			break;
#endif				/* CFG_SUPPORT_ADHOC */

		case AIS_STATE_NORMAL_TR:
			if (prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				/* Don't do anything when rJoinTimeoutTimer is still ticking */
			} else {
				/* 1. Process for pending scan */
				if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, TRUE) ==
				    TRUE) {
					wlanClearScanningResult(prAdapter);
					eNextState = AIS_STATE_ONLINE_SCAN;
					fgIsTransition = TRUE;
				}
				/* 2. Process for pending roaming scan */
				else if (aisFsmIsRequestPending
					 (prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE) == TRUE) {
					eNextState = AIS_STATE_LOOKING_FOR;
					fgIsTransition = TRUE;
				}
				/* 3. Process for pending roaming scan */
				else if (aisFsmIsRequestPending
					 (prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE) == TRUE) {
					eNextState = AIS_STATE_SEARCH;
					fgIsTransition = TRUE;
				} else
				    if (aisFsmIsRequestPending
					(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL, TRUE) == TRUE) {
					eNextState = AIS_STATE_REQ_REMAIN_ON_CHANNEL;
					fgIsTransition = TRUE;
				}
			}

			break;

		case AIS_STATE_DISCONNECTING:
			/* send for deauth frame for disconnection */
			authSendDeauthFrame(prAdapter,
					    prAisBssInfo,
					    prAisBssInfo->prStaRecOfAP,
					    (P_SW_RFB_T) NULL,
					    REASON_CODE_DEAUTH_LEAVING_BSS, aisDeauthXmitComplete);
			cnmTimerStartTimer(prAdapter, &prAisFsmInfo->rDeauthDoneTimer, 100);
			break;

		case AIS_STATE_REQ_REMAIN_ON_CHANNEL:
			/* send message to CNM for acquiring channel */
			prMsgChReq =
			    (P_MSG_CH_REQ_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
							 sizeof(MSG_CH_REQ_T));
			if (!prMsgChReq) {
				ASSERT(0);	/* Can't indicate CNM for channel acquiring */
				return;
			}

			/* zero-ize */
			kalMemZero(prMsgChReq, sizeof(MSG_CH_REQ_T));

			/* filling */
			prMsgChReq->rMsgHdr.eMsgId = MID_MNY_CNM_CH_REQ;
			prMsgChReq->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
			prMsgChReq->ucTokenID = ++prAisFsmInfo->ucSeqNumOfChReq;
			prMsgChReq->eReqType = CH_REQ_TYPE_JOIN;
			prMsgChReq->u4MaxInterval = prAisFsmInfo->rChReqInfo.u4DurationMs;
			prMsgChReq->ucPrimaryChannel = prAisFsmInfo->rChReqInfo.ucChannelNum;
			prMsgChReq->eRfSco = prAisFsmInfo->rChReqInfo.eSco;
			prMsgChReq->eRfBand = prAisFsmInfo->rChReqInfo.eBand;

			mboxSendMsg(prAdapter,
				    MBOX_ID_0, (P_MSG_HDR_T) prMsgChReq, MSG_SEND_METHOD_BUF);

			prAisFsmInfo->fgIsChannelRequested = TRUE;

			break;

		case AIS_STATE_REMAIN_ON_CHANNEL:
			SET_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

			/* sync with firmware */
			nicActivateNetwork(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
                        prAisBssInfo->fgIsNetRequestInActive = FALSE;
			break;

		default:
			ASSERT(0);	/* Make sure we have handle all STATEs */
			break;

		}
	} while (fgIsTransition);

	return;

}				/* end of aisFsmSteps() */

enum _ENUM_AIS_STATE_T
aisFsmStateSearchAction(IN struct _ADAPTER_T *prAdapter, UINT_8 ucPhase)
{
	struct _CONNECTION_SETTINGS_T *prConnSettings;
	struct _BSS_INFO_T *prAisBssInfo;
	struct _AIS_FSM_INFO_T *prAisFsmInfo;
	struct _BSS_DESC_T *prBssDesc;
	enum _ENUM_AIS_STATE_T eState = AIS_STATE_IDLE;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

#if CFG_SLT_SUPPORT
	prBssDesc = prAdapter->rWifiVar.rSltInfo.prPseudoBssDesc;
#else
	prBssDesc =
		scanSearchBssDescByPolicy(prAdapter,
					  prAdapter->prAisBssInfo->ucBssIndex);
#endif


	if (ucPhase == AIS_FSM_STATE_SEARCH_ACTION_PHASE_0) {
		if (prConnSettings->eOPMode == NET_TYPE_INFRA) {

			/* issue reconnect request, */
			/*and retreat to idle state for scheduling */
			aisFsmInsertRequest(prAdapter,
					    AIS_REQUEST_RECONNECT);
			eState = AIS_STATE_IDLE;
		}
#if CFG_SUPPORT_ADHOC
		else if ((prConnSettings->eOPMode == NET_TYPE_IBSS)
			 || (prConnSettings->eOPMode ==
			     NET_TYPE_AUTO_SWITCH)
			 || (prConnSettings->eOPMode ==
			     NET_TYPE_DEDICATED_IBSS)) {
			prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;
			prAisFsmInfo->prTargetBssDesc = NULL;
			eState = AIS_STATE_IBSS_ALONE;
		}
#endif				/* CFG_SUPPORT_ADHOC */
		else {
			ASSERT(0);
			eState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		}
	} else if (ucPhase == AIS_FSM_STATE_SEARCH_ACTION_PHASE_1) {
		/* increase connection trial count for infrastructure connection */
		if (prConnSettings->eOPMode == NET_TYPE_INFRA)
			prAisFsmInfo->ucConnTrialCount++;
		/* 4 <A> Try to SCAN */
		if (prAisFsmInfo->fgTryScan)
			eState = AIS_STATE_LOOKING_FOR;

		/* 4 <B> We've do SCAN already, now wait in some STATE. */
		else {
			if (prConnSettings->eOPMode ==
			    NET_TYPE_INFRA) {

				/* issue reconnect request, and */
				/* retreat to idle state for scheduling */
				aisFsmInsertRequest(prAdapter,
						    AIS_REQUEST_RECONNECT);

				eState = AIS_STATE_IDLE;
			}
#if CFG_SUPPORT_ADHOC
			else if ((prConnSettings->eOPMode ==
				  NET_TYPE_IBSS)
				 || (prConnSettings->eOPMode ==
				     NET_TYPE_AUTO_SWITCH)
				 || (prConnSettings->eOPMode ==
				     NET_TYPE_DEDICATED_IBSS)) {

				prAisBssInfo->eCurrentOPMode =
				    OP_MODE_IBSS;
				prAisFsmInfo->prTargetBssDesc =
				    NULL;

				eState = AIS_STATE_IBSS_ALONE;
			}
#endif				/* CFG_SUPPORT_ADHOC */
			else {
				ASSERT(0);
				eState =
				    AIS_STATE_WAIT_FOR_NEXT_SCAN;
			}
		}
	} else {
#if DBG
		if (prAisBssInfo->ucReasonOfDisconnect !=
		    DISCONNECT_REASON_CODE_REASSOCIATION) {
			ASSERT(UNEQUAL_MAC_ADDR
			       (prBssDesc->aucBSSID,
				prAisBssInfo->aucBSSID));
		}
#endif				/* DBG */
	}
	return eState;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventScanDone(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_SCN_SCAN_DONE prScanDoneMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	UINT_8 ucSeqNumOfCompMsg;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("aisFsmRunEventScanDone()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);
	DBGLOG(AIS, INFO, ("ScanDone\n"));

	DBGLOG(AIS, LOUD, ("EVENT-SCAN DONE: Current Time = %u\n", kalGetTimeTick()));

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) prMsgHdr;
	ASSERT(prScanDoneMsg->ucBssIndex == prAdapter->prAisBssInfo->ucBssIndex);

	ucSeqNumOfCompMsg = prScanDoneMsg->ucSeqNum;
	cnmMemFree(prAdapter, prMsgHdr);

	eNextState = prAisFsmInfo->eCurrentState;

	if (ucSeqNumOfCompMsg != prAisFsmInfo->ucSeqNumOfScanReq) {
		DBGLOG(AIS, WARN, ("SEQ NO of AIS SCN DONE MSG is not matched.\n"));
	} else {
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rScanDoneTimer);
		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_SCAN:
			prConnSettings->fgIsScanReqIssued = FALSE;

			/* reset scan IE buffer */
			prAisFsmInfo->u4ScanIELength = 0;

			kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX,
				    WLAN_STATUS_SUCCESS);
			eNextState = AIS_STATE_IDLE;
#if CFG_SUPPORT_AGPS_ASSIST
			scanReportScanResultToAgps(prAdapter);
#endif

			break;

		case AIS_STATE_ONLINE_SCAN:
			prConnSettings->fgIsScanReqIssued = FALSE;

			/* reset scan IE buffer */
			prAisFsmInfo->u4ScanIELength = 0;

			kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX,
				    WLAN_STATUS_SUCCESS);
#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
			eNextState = AIS_STATE_NORMAL_TR;
#endif				/* CFG_SUPPORT_ROAMING */
#if CFG_SUPPORT_AGPS_ASSIST
			scanReportScanResultToAgps(prAdapter);
#endif

			break;

		case AIS_STATE_LOOKING_FOR:
#if CFG_SUPPORT_ROAMING
			eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
			eNextState = AIS_STATE_SEARCH;
#endif				/* CFG_SUPPORT_ROAMING */
			break;

		default:
			break;

		}
	}

	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);


	return;
}				/* end of aisFsmRunEventScanDone() */


/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventAbort(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_AIS_ABORT_T prAisAbortMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	UINT_8 ucReasonOfDisconnect;
	BOOLEAN fgDelayIndication;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("aisFsmRunEventAbort()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Extract information of Abort Message and then free memory. */
	prAisAbortMsg = (P_MSG_AIS_ABORT_T) prMsgHdr;
	ucReasonOfDisconnect = prAisAbortMsg->ucReasonOfDisconnect;
	fgDelayIndication = prAisAbortMsg->fgDelayIndication;

	cnmMemFree(prAdapter, prMsgHdr);

#if DBG
	DBGLOG(AIS, LOUD, ("EVENT-ABORT: Current State %s\n",
			   apucDebugAisState[prAisFsmInfo->eCurrentState]));
#else
	DBGLOG(AIS, LOUD, ("[%d] EVENT-ABORT: Current State [%d]\n",
			   DBG_AIS_IDX, prAisFsmInfo->eCurrentState));
#endif

	/* record join request time */
	GET_CURRENT_SYSTIME(&(prAisFsmInfo->rJoinReqTime));

	/* 4 <2> clear previous pending connection request and insert new one */
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DEAUTHENTICATED
	    || ucReasonOfDisconnect == DISCONNECT_REASON_CODE_DISASSOCIATED) {
		prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
	} else {
		prConnSettings->fgIsDisconnectedByNonRequest = FALSE;
	}
	
	/* to support user space triggered roaming */
	if (ucReasonOfDisconnect == DISCONNECT_REASON_CODE_REASSOCIATION &&
			prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {

	    if(prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR &&
				prAisFsmInfo->fgIsInfraChannelFinished == TRUE) {
			aisFsmSteps(prAdapter, AIS_STATE_SEARCH);
	    }
	    else {
	        aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
	        aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_ROAMING_CONNECT);
	    }
		return;
	}

	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);
	aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);

	if (prAisFsmInfo->eCurrentState != AIS_STATE_DISCONNECTING) {
		/* 4 <3> invoke abort handler */
		aisFsmStateAbort(prAdapter, ucReasonOfDisconnect, fgDelayIndication);
	}

	return;
}				/* end of aisFsmRunEventAbort() */


/*----------------------------------------------------------------------------*/
/*!
* \brief        This function handles AIS-FSM abort event/command
*
* \param[in] prAdapter              Pointer of ADAPTER_T
*            ucReasonOfDisconnect   Reason for disonnection
*            fgDelayIndication      Option to delay disconnection indication
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
aisFsmStateAbort(IN P_ADAPTER_T prAdapter, UINT_8 ucReasonOfDisconnect, BOOLEAN fgDelayIndication)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	BOOLEAN fgIsCheckConnected;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	fgIsCheckConnected = FALSE;

	/* 4 <1> Save information of Abort Message and then free memory. */
	prAisBssInfo->ucReasonOfDisconnect = ucReasonOfDisconnect;

	/* 4 <2> Abort current job. */
	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IDLE:
	case AIS_STATE_SEARCH:
	case AIS_STATE_JOIN_FAILURE:
		break;

	case AIS_STATE_WAIT_FOR_NEXT_SCAN:
		/* Do cancel timer */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rBGScanTimer);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_SCAN:
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter);

		/* queue for later handling */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, FALSE) == FALSE)
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);


		break;

	case AIS_STATE_LOOKING_FOR:
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_REQ_CHANNEL_JOIN:
		/* Release channel to CNM */
		aisFsmReleaseCh(prAdapter);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_JOIN:
		/* Do abort JOIN */
		aisFsmStateAbort_JOIN(prAdapter);

		/* in case roaming is triggered */
		fgIsCheckConnected = TRUE;
		break;

#if CFG_SUPPORT_ADHOC
	case AIS_STATE_IBSS_ALONE:
	case AIS_STATE_IBSS_MERGE:
		aisFsmStateAbort_IBSS(prAdapter);
		break;
#endif				/* CFG_SUPPORT_ADHOC */

	case AIS_STATE_ONLINE_SCAN:
		/* Do abort SCAN */
		aisFsmStateAbort_SCAN(prAdapter);

		/* queue for later handling */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, FALSE) == FALSE)
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);


		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_NORMAL_TR:
		fgIsCheckConnected = TRUE;
		break;

	case AIS_STATE_DISCONNECTING:
		/* Do abort NORMAL_TR */
		aisFsmStateAbort_NORMAL_TR(prAdapter);

		break;

	case AIS_STATE_REQ_REMAIN_ON_CHANNEL:
		/* release channel */
		aisFsmReleaseCh(prAdapter);
		break;

	case AIS_STATE_REMAIN_ON_CHANNEL:
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);

		/* 2. stop channel timeout timer */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);

		break;

	default:
		break;
	}

	if (fgIsCheckConnected && (PARAM_MEDIA_STATE_CONNECTED == prAisBssInfo->eConnectionState)) {

		/* switch into DISCONNECTING state for sending DEAUTH if necessary */
		if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE &&
		    prAisBssInfo->ucReasonOfDisconnect == DISCONNECT_REASON_CODE_NEW_CONNECTION &&
		    prAisBssInfo->prStaRecOfAP && prAisBssInfo->prStaRecOfAP->fgIsInUse) {
			aisFsmSteps(prAdapter, AIS_STATE_DISCONNECTING);

			return;
		} else {
			/* Do abort NORMAL_TR */
			aisFsmStateAbort_NORMAL_TR(prAdapter);
		}
	}

	aisFsmDisconnect(prAdapter, fgDelayIndication);

	return;

}				/* end of aisFsmStateAbort() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Join Complete Event from SAA FSM for AIS FSM
*
* @param[in] prMsgHdr   Message of Join Complete of SAA FSM.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventJoinComplete(IN struct _ADAPTER_T *prAdapter, IN struct _MSG_HDR_T *prMsgHdr)
{
	struct _MSG_SAA_FSM_COMP_T *prJoinCompMsg;
	struct _AIS_FSM_INFO_T *prAisFsmInfo;
	enum _ENUM_AIS_STATE_T eNextState;
	struct _SW_RFB_T *prAssocRspSwRfb;

	DEBUGFUNC("aisFsmRunEventJoinComplete()");
	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prJoinCompMsg = (struct _MSG_SAA_FSM_COMP_T *) prMsgHdr;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;

	eNextState = prAisFsmInfo->eCurrentState;

	/* Check State and SEQ NUM */
	if (prAisFsmInfo->eCurrentState == AIS_STATE_JOIN) {
		/* Check SEQ NUM */
		if (prJoinCompMsg->ucSeqNum == prAisFsmInfo->ucSeqNumOfReqMsg)
			eNextState = aisFsmJoinCompleteAction(prAdapter, prMsgHdr);
#if DBG
		else {
			DBGLOG(AIS, WARN,
			       ("SEQ NO of AIS JOIN COMP MSG is not matched.\n"));
		}
#endif				/* DBG */
	}
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);

	if (prAssocRspSwRfb)
		nicRxReturnRFB(prAdapter, prAssocRspSwRfb);

	cnmMemFree(prAdapter, prMsgHdr);

	return;
}				/* end of aisFsmRunEventJoinComplete() */

enum _ENUM_AIS_STATE_T
aisFsmJoinCompleteAction(IN struct _ADAPTER_T *prAdapter, IN struct _MSG_HDR_T *prMsgHdr)
{
	struct _MSG_SAA_FSM_COMP_T *prJoinCompMsg;
	struct _AIS_FSM_INFO_T *prAisFsmInfo;
	enum _ENUM_AIS_STATE_T eNextState;
	struct _STA_RECORD_T *prStaRec;
	struct _SW_RFB_T *prAssocRspSwRfb;
	struct _BSS_INFO_T *prAisBssInfo;
	OS_SYSTIME rCurrentTime;
	UINT_8 aucP2pSsid[] = CTIA_MAGIC_SSID;

	DEBUGFUNC("aisFsmJoinCompleteAction()");

	ASSERT(prMsgHdr);

	GET_CURRENT_SYSTIME(&rCurrentTime);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prJoinCompMsg = (struct _MSG_SAA_FSM_COMP_T *) prMsgHdr;
	prStaRec = prJoinCompMsg->prStaRec;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;
	prAisBssInfo = prAdapter->prAisBssInfo;
	eNextState = prAisFsmInfo->eCurrentState;

#if CFG_SUPPORT_AUTORN
	GET_CURRENT_SYSTIME(&prAisBssInfo->rConnTime);
#endif

	do {
		/* 4 <1> JOIN was successful */
		if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {
#if CFG_SUPPORT_AUTORN
			prAisBssInfo->fgDisConnReassoc = FALSE;
#endif

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
			prAdapter->rWifiVar.rConnSettings.fgSecModeChangeStartTimer = FALSE;
#endif
			/* 1. Reset retry count */
			prAisFsmInfo->ucConnTrialCount = 0;
			prAdapter->rWifiVar.rConnSettings.eReConnectLevel = RECONNECT_LEVEL_MIN;

			/* Completion of roaming */
			if (prAisBssInfo->eConnectionState ==
			    PARAM_MEDIA_STATE_CONNECTED) {

#if CFG_SUPPORT_ROAMING
				/* 2. Deactivate previous BSS */
				aisFsmRoamingDisconnectPrevAP(prAdapter, prStaRec);

				/* 3. Update bss based on roaming staRec */
				aisUpdateBssInfoForRoamingAP(prAdapter, prStaRec,
							     prAssocRspSwRfb);
#endif				/* CFG_SUPPORT_ROAMING */
			} else {
				/* 4 <1.1> Change FW's Media State immediately. */
				aisChangeMediaState(prAdapter,
						    PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Deactivate previous AP's STA_RECORD_T in Driver if have. */
				if ((prAisBssInfo->prStaRecOfAP) &&
				    (prAisBssInfo->prStaRecOfAP != prStaRec) &&
				    (prAisBssInfo->prStaRecOfAP->fgIsInUse)) {

					cnmStaRecChangeState(prAdapter,
							     prAisBssInfo->
							     prStaRecOfAP,
							     STA_STATE_1);
					cnmStaRecFree(prAdapter,
						      prAisBssInfo->prStaRecOfAP);
				}

				/* For temp solution, need to refine */
				/* 4 <1.4> Update BSS_INFO_T */
				aisUpdateBssInfoForJOIN(prAdapter, prStaRec,
							prAssocRspSwRfb);

				/* 4 <1.3> Activate current AP's STA_RECORD_T in Driver. */
				cnmStaRecChangeState(prAdapter, prStaRec,
						     STA_STATE_3);

				/* 4 <1.5> Update RSSI if necessary */
				nicUpdateRSSI(prAdapter,
					      prAdapter->prAisBssInfo->ucBssIndex,
					      (INT_8) (RCPI_TO_dBm
						       (prStaRec->ucRCPI)), 0);

				/* 4 <1.6> Indicate Connected Event to Host immediately. */
				/* Require BSSID, Association ID, Beacon Interval */
				/* .. from AIS_BSS_INFO_T */
				aisIndicationOfMediaStateToHost(prAdapter,
								PARAM_MEDIA_STATE_CONNECTED,
								FALSE);

				if (EQUAL_SSID
				    (aucP2pSsid, CTIA_MAGIC_SSID_LEN,
				     prAisBssInfo->aucSSID,
				     prAisBssInfo->ucSSIDLen)) {
					nicEnterCtiaMode(prAdapter, TRUE,
							 FALSE);
				}
			}

#if CFG_SUPPORT_ROAMING
			 /* if user space roaming is enabled, we should disable driver/fw roaming*/
#ifdef CONFIG_CFG80211_ALLOW_RECONNECT
			if (prAdapter->rWifiVar.rConnSettings.eConnectionPolicy != CONNECT_BY_BSSID)
#endif
				roamingFsmRunEventStart(prAdapter);
#endif				/* CFG_SUPPORT_ROAMING */
			if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, FALSE) == FALSE)
				prAisFsmInfo->rJoinReqTime = 0;

			/* 4 <1.7> Set the Next State of AIS FSM */
			eNextState = AIS_STATE_NORMAL_TR;
		}
		/* 4 <2> JOIN was not successful */
		else {
			/* 4 <2.1> Redo JOIN process with other Auth Type if possible */
			if (aisFsmStateInit_RetryJOIN(prAdapter, prStaRec) == FALSE) {
				struct _BSS_DESC_T *prBssDesc;

				/* 1. Increase Failure Count */
				prStaRec->ucJoinFailureCount++;

				/* 2. release channel */
				aisFsmReleaseCh(prAdapter);

				/* 3.1 stop join timeout timer */
				cnmTimerStopTimer(prAdapter,
						  &prAisFsmInfo->rJoinTimeoutTimer);

				/* 3.2 reset local variable */
				prAisFsmInfo->fgIsInfraChannelFinished = TRUE;

				prBssDesc =
				    scanSearchBssDescByBssid(prAdapter,
							     prStaRec->aucMacAddr);

				if (prBssDesc == NULL)
					break;

				/* ASSERT(prBssDesc); */
				/* ASSERT(prBssDesc->fgIsConnecting); */
				prBssDesc->ucJoinFailureCount++;
				prBssDesc->u2StatusCode = prStaRec->u2StatusCode;
				if (prBssDesc->ucJoinFailureCount > SCN_BSS_JOIN_FAIL_THRESOLD) {
				        GET_CURRENT_SYSTIME(&prBssDesc->rJoinFailTime);
				        DBGLOG(AIS, INFO, ("Bss "MACSTR" join fail %d times, temp disable it at time: %u\n", 
				                            MAC2STR(prBssDesc->aucBSSID), SCN_BSS_JOIN_FAIL_THRESOLD, prBssDesc->rJoinFailTime));
				}

				if (prBssDesc)
					prBssDesc->fgIsConnecting = FALSE;


				/* 3.3 Free STA-REC */
				if (prStaRec != prAisBssInfo->prStaRecOfAP)
					cnmStaRecFree(prAdapter, prStaRec);

				if (prAisBssInfo->eConnectionState ==
				    PARAM_MEDIA_STATE_CONNECTED) {
#if CFG_SUPPORT_ROAMING
					eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
#endif				/* CFG_SUPPORT_ROAMING */
#if CFG_SUPPORT_AUTORN
				} else if (prAisBssInfo->fgDisConnReassoc == TRUE) {
					if (prStaRec)
						prAisBssInfo->u2DeauthReason = prStaRec->u2ReasonCode;
					eNextState = AIS_STATE_JOIN_FAILURE;
#endif
				} else if (prAisFsmInfo->rJoinReqTime != 0 &&
					   CHECK_FOR_TIMEOUT(rCurrentTime,
							     prAisFsmInfo->rJoinReqTime,
							     SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {

					/* 4.a temrminate join operation */
					eNextState = AIS_STATE_JOIN_FAILURE;
				} else {
					/* 4.b send reconnect request */
					aisFsmInsertRequest(prAdapter,
							    AIS_REQUEST_RECONNECT);

					eNextState = AIS_STATE_IDLE;
				}
			}
		}
	} while (0);
	return eNextState;
}


#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Grant Msg of IBSS Create which was sent by
*        CNM to indicate that channel was changed for creating IBSS.
*
* @param[in] prAdapter  Pointer of ADAPTER_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmCreateIBSS(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	do {
		/* Check State */
		if (prAisFsmInfo->eCurrentState == AIS_STATE_IBSS_ALONE)
			aisUpdateBssInfoForCreateIBSS(prAdapter);

	} while (FALSE);

	return;
}				/* end of aisFsmCreateIBSS() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Grant Msg of IBSS Merge which was sent by
*        CNM to indicate that channel was changed for merging IBSS.
*
* @param[in] prAdapter  Pointer of ADAPTER_T
* @param[in] prStaRec   Pointer of STA_RECORD_T for merge
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmMergeIBSS(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	P_BSS_INFO_T prAisBssInfo;

	ASSERT(prAdapter);
	ASSERT(prStaRec);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	do {

		eNextState = prAisFsmInfo->eCurrentState;

		switch (prAisFsmInfo->eCurrentState) {
		case AIS_STATE_IBSS_MERGE:
			{
				P_BSS_DESC_T prBssDesc;

				/* 4 <1.1> Change FW's Media State immediately. */
				aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Deactivate previous Peers' STA_RECORD_T in Driver if have. */
				bssInitializeClientList(prAdapter, prAisBssInfo);

				/* 4 <1.3> Unmark connection flag of previous BSS_DESC_T. */
				prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = FALSE;
				}
				/* 4 <1.4> Add Peers' STA_RECORD_T to Client List */
				bssAddClient(prAdapter, prAisBssInfo, prStaRec);

				/* 4 <1.5> Activate current Peer's STA_RECORD_T in Driver. */
				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

				/* 4 <1.6> Update BSS_INFO_T */
				aisUpdateBssInfoForMergeIBSS(prAdapter, prStaRec);

				/* 4 <1.7> Enable other features */

				/* 4 <1.8> Indicate Connected Event to Host immediately. */
				aisIndicationOfMediaStateToHost(prAdapter,
								PARAM_MEDIA_STATE_CONNECTED, FALSE);

				/* 4 <1.9> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_NORMAL_TR;

				/* 4 <1.10> Release channel privilege */
				aisFsmReleaseCh(prAdapter);

#if CFG_SLT_SUPPORT
				prAdapter->rWifiVar.rSltInfo.prPseudoStaRec = prStaRec;
#endif
			}
			break;

		default:
			break;
		}

		if (eNextState != prAisFsmInfo->eCurrentState)
			aisFsmSteps(prAdapter, eNextState);


	} while (FALSE);

	return;
}				/* end of aisFsmMergeIBSS() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will handle the Notification of existing IBSS was found
*        from SCN.
*
* @param[in] prMsgHdr   Message of Notification of an IBSS was present.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventFoundIBSSPeer(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_AIS_IBSS_PEER_FOUND_T prAisIbssPeerFoundMsg;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	P_STA_RECORD_T prStaRec;
	P_BSS_INFO_T prAisBssInfo;
	P_BSS_DESC_T prBssDesc;
	BOOLEAN fgIsMergeIn;


	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	prAisIbssPeerFoundMsg = (P_MSG_AIS_IBSS_PEER_FOUND_T) prMsgHdr;

	ASSERT(prAisIbssPeerFoundMsg->ucBssIndex == prAdapter->prAisBssInfo->ucBssIndex);

	prStaRec = prAisIbssPeerFoundMsg->prStaRec;
	ASSERT(prStaRec);

	fgIsMergeIn = prAisIbssPeerFoundMsg->fgIsMergeIn;

	cnmMemFree(prAdapter, prMsgHdr);


	eNextState = prAisFsmInfo->eCurrentState;
	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IBSS_ALONE:
		{
			/* 4 <1> An IBSS Peer 'merged in'. */
			if (fgIsMergeIn) {

				/* 4 <1.1> Change FW's Media State immediately. */
				aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

				/* 4 <1.2> Add Peers' STA_RECORD_T to Client List */
				bssAddClient(prAdapter, prAisBssInfo, prStaRec);

#if CFG_SLT_SUPPORT
				/* 4 <1.3> Mark connection flag of BSS_DESC_T. */
				prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);

				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = TRUE;
				} else {
					ASSERT(0);	/* Should be able to find a BSS_DESC_T here. */
				}

				/* 4 <1.4> Activate current Peer's STA_RECORD_T in Driver. */
				prStaRec->fgIsQoS = TRUE;	/* TODO(Kevin): TBD */
#else
				/* 4 <1.3> Mark connection flag of BSS_DESC_T. */
				prBssDesc = scanSearchBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);

				if (prBssDesc != NULL) {
					prBssDesc->fgIsConnecting = FALSE;
					prBssDesc->fgIsConnected = TRUE;
				} else {
					ASSERT(0);	/* Should be able to find a BSS_DESC_T here. */
				}


				/* 4 <1.4> Activate current Peer's STA_RECORD_T in Driver. */
				prStaRec->fgIsQoS = FALSE;	/* TODO(Kevin): TBD */

#endif

				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

				/* 4 <1.6> sync. to firmware */
				nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

				/* 4 <1.7> Indicate Connected Event to Host immediately. */
				aisIndicationOfMediaStateToHost(prAdapter,
								PARAM_MEDIA_STATE_CONNECTED, FALSE);

				/* 4 <1.8> indicate PM for connected */
				nicPmIndicateBssConnected(prAdapter,
							  prAdapter->prAisBssInfo->ucBssIndex);

				/* 4 <1.9> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_NORMAL_TR;

				/* 4 <1.10> Release channel privilege */
				aisFsmReleaseCh(prAdapter);
			}
			/* 4 <2> We need 'merge out' to this IBSS */
			else {

				/* 4 <2.1> Get corresponding BSS_DESC_T */
				prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);

				prAisFsmInfo->prTargetBssDesc = prBssDesc;

				/* 4 <2.2> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_IBSS_MERGE;
			}
		}
		break;

	case AIS_STATE_NORMAL_TR:
		{

			/* 4 <3> An IBSS Peer 'merged in'. */
			if (fgIsMergeIn) {

				/* 4 <3.1> Add Peers' STA_RECORD_T to Client List */
				bssAddClient(prAdapter, prAisBssInfo, prStaRec);

#if CFG_SLT_SUPPORT
				/* 4 <3.2> Activate current Peer's STA_RECORD_T in Driver. */
				prStaRec->fgIsQoS = TRUE;	/* TODO(Kevin): TBD */
#else
				/* 4 <3.2> Activate current Peer's STA_RECORD_T in Driver. */
				prStaRec->fgIsQoS = FALSE;	/* TODO(Kevin): TBD */
#endif

				cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);
				prStaRec->fgIsMerging = FALSE;

			}
			/* 4 <4> We need 'merge out' to this IBSS */
			else {

				/* 4 <4.1> Get corresponding BSS_DESC_T */
				prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);

				prAisFsmInfo->prTargetBssDesc = prBssDesc;

				/* 4 <4.2> Set the Next State of AIS FSM */
				eNextState = AIS_STATE_IBSS_MERGE;

			}
		}
		break;

	default:
		break;
	}

	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);


	return;
}				/* end of aisFsmRunEventFoundIBSSPeer() */
#endif				/* CFG_SUPPORT_ADHOC */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate the Media State to HOST
*
* @param[in] eConnectionState   Current Media State
* @param[in] fgDelayIndication  Set TRUE for postponing the Disconnect Indication.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
aisIndicationOfMediaStateToHost(IN P_ADAPTER_T prAdapter,
				ENUM_PARAM_MEDIA_STATE_T eConnectionState,
				BOOLEAN fgDelayIndication)
{
	EVENT_CONNECTION_STATUS rEventConnStatus;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("aisIndicationOfMediaStateToHost()");

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* NOTE(Kevin): Move following line to aisChangeMediaState() macro per CM's request. */
	/* prAisBssInfo->eConnectionState = eConnectionState; */

	/* For indicating the Disconnect Event only if current media state is
	 * disconnected and we didn't do indication yet.
	 */
	if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED) {
		if (prAisBssInfo->eConnectionStateIndicated == eConnectionState)
			return;

	}

	if (!fgDelayIndication) {
		/* 4 <0> Cancel Delay Timer */
		prAisFsmInfo->u4PostponeIndStartTime = 0;

		/* 4 <1> Fill EVENT_CONNECTION_STATUS */
		rEventConnStatus.ucMediaStatus = (UINT_8) eConnectionState;

		if (eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			rEventConnStatus.ucReasonOfDisconnect = DISCONNECT_REASON_CODE_RESERVED;

			if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
				rEventConnStatus.ucInfraMode = (UINT_8) NET_TYPE_INFRA;
				rEventConnStatus.u2AID = prAisBssInfo->u2AssocId;
				rEventConnStatus.u2ATIMWindow = 0;
			} else if (prAisBssInfo->eCurrentOPMode == OP_MODE_IBSS) {
				rEventConnStatus.ucInfraMode = (UINT_8) NET_TYPE_IBSS;
				rEventConnStatus.u2AID = 0;
				rEventConnStatus.u2ATIMWindow = prAisBssInfo->u2ATIMWindow;
			} else {
				ASSERT(0);
			}

			COPY_SSID(rEventConnStatus.aucSsid,
				  rEventConnStatus.ucSsidLen,
				  prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

			COPY_MAC_ADDR(rEventConnStatus.aucBssid, prAisBssInfo->aucBSSID);

			rEventConnStatus.u2BeaconPeriod = prAisBssInfo->u2BeaconInterval;
			rEventConnStatus.u4FreqInKHz =
			    nicChannelNum2Freq(prAisBssInfo->ucPrimaryChannel);

			switch (prAisBssInfo->ucNonHTBasicPhyType) {
			case PHY_TYPE_HR_DSSS_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_DS;
				break;

			case PHY_TYPE_ERP_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_OFDM24;
				break;

			case PHY_TYPE_OFDM_INDEX:
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_OFDM5;
				break;

			default:
				ASSERT(0);
				rEventConnStatus.ucNetworkType = (UINT_8) PARAM_NETWORK_TYPE_DS;
				break;
			}
		} else {
			/* Clear the pmkid cache while media disconnect */
			secClearPmkid(prAdapter);
			rEventConnStatus.ucReasonOfDisconnect = prAisBssInfo->ucReasonOfDisconnect;
		}

		/* 4 <2> Indication */
		nicMediaStateChange(prAdapter, prAdapter->prAisBssInfo->ucBssIndex,
				    &rEventConnStatus);
		prAisBssInfo->eConnectionStateIndicated = eConnectionState;
	} else {
		/* NOTE: Only delay the Indication of Disconnect Event */
		ASSERT(eConnectionState == PARAM_MEDIA_STATE_DISCONNECTED);

#if CFG_SUPPORT_AUTORN
		if (prAisBssInfo->fgDisConnReassoc) {
			DBGLOG(AIS, INFO, ("Reassoc the AP once beacause of receive deauth/deassoc\n"));
		} else
#endif
		{
			DBGLOG(AIS, INFO, ("Postpone the indication of Disconnect for %d seconds\n",
					   prConnSettings->ucDelayTimeOfDisconnectEvent));
	 		prAisFsmInfo->u4PostponeIndStartTime = kalGetTimeTick();
 		}
	}

	return;
}				/* end of aisIndicationOfMediaStateToHost() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Media Disconnect" to HOST
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisPostponedEventOfDisconnTimeout(IN P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	BOOLEAN fgFound = TRUE;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Deactivate previous AP's STA_RECORD_T in Driver if have. */
	if (prAisBssInfo->prStaRecOfAP) {
		/* cnmStaRecChangeState(prAdapter, prAisBssInfo->prStaRecOfAP, STA_STATE_1); */

		prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
	}
	/* 4 <2> Remove all pending connection request */
	while (fgFound)
		fgFound = aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR)
		prAisFsmInfo->eCurrentState = AIS_STATE_IDLE;
	prConnSettings->fgIsDisconnectedByNonRequest = TRUE;
	prAisBssInfo->u2DeauthReason = 100*REASON_CODE_BEACON_TIMEOUT+prAisBssInfo->u2DeauthReason;
	/* 4 <3> Indicate Disconnected Event to Host immediately. */
	aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED, FALSE);

	return;
}				/* end of aisPostponedEventOfDisconnTimeout() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will update the contain of BSS_INFO_T for AIS network once
*        the association was completed.
*
* @param[in] prStaRec               Pointer to the STA_RECORD_T
* @param[in] prAssocRspSwRfb        Pointer to SW RFB of ASSOC RESP FRAME.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
aisUpdateBssInfoForJOIN(IN P_ADAPTER_T prAdapter,
			P_STA_RECORD_T prStaRec, P_SW_RFB_T prAssocRspSwRfb)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_WLAN_ASSOC_RSP_FRAME_T prAssocRspFrame;
	P_BSS_DESC_T prBssDesc;
	UINT_16 u2IELength;
	PUINT_8 pucIE;

	DEBUGFUNC("aisUpdateBssInfoForJOIN()");

	ASSERT(prStaRec);
	ASSERT(prAssocRspSwRfb);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);
	prAssocRspFrame = (P_WLAN_ASSOC_RSP_FRAME_T) prAssocRspSwRfb->pvHeader;


	DBGLOG(AIS, INFO, ("Update AIS_BSS_INFO_T and apply settings to MAC\n"));


	/* 3 <1> Update BSS_INFO_T from AIS_FSM_INFO_T or User Settings */
	/* 4 <1.1> Setup Operation Mode */
	prAisBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE;

	/* 4 <1.2> Setup SSID */
	COPY_SSID(prAisBssInfo->aucSSID,
		  prAisBssInfo->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

	/* 4 <1.3> Setup Channel, Band */
	prAisBssInfo->ucPrimaryChannel = prAisFsmInfo->prTargetBssDesc->ucChannelNum;
	prAisBssInfo->eBand = prAisFsmInfo->prTargetBssDesc->eBand;


	/* 3 <2> Update BSS_INFO_T from STA_RECORD_T */
	/* 4 <2.1> Save current AP's STA_RECORD_T and current AID */
	prAisBssInfo->prStaRecOfAP = prStaRec;
	prAisBssInfo->u2AssocId = prStaRec->u2AssocId;

	/* 4 <2.2> Setup Capability */
	prAisBssInfo->u2CapInfo = prStaRec->u2CapInfo;	/* Use AP's Cap Info as BSS Cap Info */

	if (prAisBssInfo->u2CapInfo & CAP_INFO_SHORT_PREAMBLE)
		prAisBssInfo->fgIsShortPreambleAllowed = TRUE;
	else
		prAisBssInfo->fgIsShortPreambleAllowed = FALSE;


#if CFG_SUPPORT_TDLS
	prAisBssInfo->fgTdlsIsProhibited = prStaRec->fgTdlsIsProhibited;
	prAisBssInfo->fgTdlsIsChSwProhibited = prStaRec->fgTdlsIsChSwProhibited;
#endif				/* CFG_SUPPORT_TDLS */

	/* 4 <2.3> Setup PHY Attributes and Basic Rate Set/Operational Rate Set */
	prAisBssInfo->ucPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;

	prAisBssInfo->ucNonHTBasicPhyType = prStaRec->ucNonHTBasicPhyType;

	prAisBssInfo->u2OperationalRateSet = prStaRec->u2OperationalRateSet;
	prAisBssInfo->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;

	nicTxUpdateBssDefaultRate(prAisBssInfo);

	/* 3 <3> Update BSS_INFO_T from SW_RFB_T (Association Resp Frame) */
	/* 4 <3.1> Setup BSSID */
	COPY_MAC_ADDR(prAisBssInfo->aucBSSID, prAssocRspFrame->aucBSSID);


	u2IELength = (UINT_16) ((prAssocRspSwRfb->u2PacketLen - prAssocRspSwRfb->u2HeaderLen) -
				(OFFSET_OF(WLAN_ASSOC_RSP_FRAME_T, aucInfoElem[0]) -
				 WLAN_MAC_MGMT_HEADER_LEN));
	pucIE = prAssocRspFrame->aucInfoElem;


	/* 4 <3.2> Parse WMM and setup QBSS flag */
	/* Parse WMM related IEs and configure HW CRs accordingly */
	mqmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

	prAisBssInfo->fgIsQBSS = prStaRec->fgIsQoS;

	/* 3 <4> Update BSS_INFO_T from BSS_DESC_T */
	prBssDesc = scanSearchBssDescByBssid(prAdapter, prAssocRspFrame->aucBSSID);
	if (prBssDesc) {
		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;
		prBssDesc->ucJoinFailureCount = 0;
		prBssDesc->u2StatusCode = 0;
		/* 4 <4.1> Setup MIB for current BSS */
		prAisBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
	} else {
		/* should never happen */
		ASSERT(0);
	}

	/* NOTE: Defer ucDTIMPeriod updating to when beacon is received after connection */
	prAisBssInfo->ucDTIMPeriod = 0;
	prAisBssInfo->u2ATIMWindow = 0;

	prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_INFRA;

	/* 4 <4.2> Update HT information and set channel */
	/* Record HT related parameters in rStaRec and rBssInfo
	 * Note: it shall be called before nicUpdateBss()
	 */
	rlmProcessAssocRsp(prAdapter, prAssocRspSwRfb, pucIE, u2IELength);

	/* 4 <4.3> Sync with firmware for BSS-INFO */
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <4.4> *DEFER OPERATION* nicPmIndicateBssConnected() will be invoked */
	/* inside scanProcessBeaconAndProbeResp() after 1st beacon is received */

	return;
}				/* end of aisUpdateBssInfoForJOIN() */


#if CFG_SUPPORT_ADHOC
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will create an Ad-Hoc network and start sending Beacon Frames.
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisUpdateBssInfoForCreateIBSS(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (prAisBssInfo->fgIsBeaconActivated)
		return;

	/* 3 <1> Update BSS_INFO_T per Network Basis */
	/* 4 <1.1> Setup Operation Mode */
	prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

	/* 4 <1.2> Setup SSID */
	COPY_SSID(prAisBssInfo->aucSSID,
		  prAisBssInfo->ucSSIDLen, prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

	/* 4 <1.3> Clear current AP's STA_RECORD_T and current AID */
	prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
	prAisBssInfo->u2AssocId = 0;

	/* 4 <1.4> Setup Channel, Band and Phy Attributes */
	prAisBssInfo->ucPrimaryChannel = prConnSettings->ucAdHocChannelNum;
	prAisBssInfo->eBand = prConnSettings->eAdHocBand;

	if (prAisBssInfo->eBand == BAND_2G4) {
		/* Depend on eBand */
		prAisBssInfo->ucPhyTypeSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11BGN;
		/* Depend on eCurrentOPMode and ucPhyTypeSet */
		prAisBssInfo->ucConfigAdHocAPMode = AD_HOC_MODE_MIXED_11BG;
	} else {
		/* Depend on eBand */
		prAisBssInfo->ucPhyTypeSet = prAdapter->rWifiVar.ucAvailablePhyTypeSet & PHY_TYPE_SET_802_11ANAC;
		/* Depend on eCurrentOPMode and ucPhyTypeSet */
		prAisBssInfo->ucConfigAdHocAPMode = AD_HOC_MODE_11A;
	}

	/* 4 <1.5> Setup MIB for current BSS */
	prAisBssInfo->u2BeaconInterval = prConnSettings->u2BeaconPeriod;
	prAisBssInfo->ucDTIMPeriod = 0;
	prAisBssInfo->u2ATIMWindow = prConnSettings->u2AtimWindow;

	prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_ADHOC;

	if (prConnSettings->eEncStatus == ENUM_ENCRYPTION1_ENABLED ||
	    prConnSettings->eEncStatus == ENUM_ENCRYPTION2_ENABLED ||
	    prConnSettings->eEncStatus == ENUM_ENCRYPTION3_ENABLED) {
		prAisBssInfo->fgIsProtection = TRUE;
	} else {
		prAisBssInfo->fgIsProtection = FALSE;
	}

	/* 3 <2> Update BSS_INFO_T common part */
	ibssInitForAdHoc(prAdapter, prAisBssInfo);
	/* 4 <2.1> Initialize client list */
	bssInitializeClientList(prAdapter, prAisBssInfo);

	/* 3 <3> Set MAC HW */
	/* 4 <3.1> Setup channel and bandwidth */
	rlmBssInitForAPandIbss(prAdapter, prAisBssInfo);

	/* 4 <3.2> use command packets to inform firmware */
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <3.3> enable beaconing */
	bssUpdateBeaconContent(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <3.4> Update AdHoc PM parameter */
	nicPmIndicateBssCreated(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 3 <4> Set ACTIVE flag. */
	prAisBssInfo->fgIsBeaconActivated = TRUE;
	prAisBssInfo->fgHoldSameBssidForIBSS = TRUE;

	/* 3 <5> Start IBSS Alone Timer */
	cnmTimerStartTimer(prAdapter,
			   &prAisFsmInfo->rIbssAloneTimer, SEC_TO_MSEC(AIS_IBSS_ALONE_TIMEOUT_SEC));

	return;

}				/* end of aisCreateIBSS() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will update the contain of BSS_INFO_T for AIS network once
*        the existing IBSS was found.
*
* @param[in] prStaRec               Pointer to the STA_RECORD_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisUpdateBssInfoForMergeIBSS(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_DESC_T prBssDesc;
	/* UINT_16 u2IELength; */
	/* PUINT_8 pucIE; */


	ASSERT(prStaRec);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rIbssAloneTimer);

	if (!prAisBssInfo->fgIsBeaconActivated) {

		/* 3 <1> Update BSS_INFO_T per Network Basis */
		/* 4 <1.1> Setup Operation Mode */
		prAisBssInfo->eCurrentOPMode = OP_MODE_IBSS;

		/* 4 <1.2> Setup SSID */
		COPY_SSID(prAisBssInfo->aucSSID,
			  prAisBssInfo->ucSSIDLen,
			  prConnSettings->aucSSID, prConnSettings->ucSSIDLen);

		/* 4 <1.3> Clear current AP's STA_RECORD_T and current AID */
		prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
		prAisBssInfo->u2AssocId = 0;
	}
	/* 3 <2> Update BSS_INFO_T from STA_RECORD_T */
	/* 4 <2.1> Setup Capability */
	prAisBssInfo->u2CapInfo = prStaRec->u2CapInfo;	/* Use Peer's Cap Info as IBSS Cap Info */

	if (prAisBssInfo->u2CapInfo & CAP_INFO_SHORT_PREAMBLE) {
		prAisBssInfo->fgIsShortPreambleAllowed = TRUE;
		prAisBssInfo->fgUseShortPreamble = TRUE;
	} else {
		prAisBssInfo->fgIsShortPreambleAllowed = FALSE;
		prAisBssInfo->fgUseShortPreamble = FALSE;
	}

	/* 7.3.1.4 For IBSS, the Short Slot Time subfield shall be set to 0. */
	prAisBssInfo->fgUseShortSlotTime = FALSE;	/* Set to FALSE for AdHoc */
	prAisBssInfo->u2CapInfo &= ~CAP_INFO_SHORT_SLOT_TIME;

	if (prAisBssInfo->u2CapInfo & CAP_INFO_PRIVACY)
		prAisBssInfo->fgIsProtection = TRUE;
	else
		prAisBssInfo->fgIsProtection = FALSE;


	/* 4 <2.2> Setup PHY Attributes and Basic Rate Set/Operational Rate Set */
	prAisBssInfo->ucPhyTypeSet = prStaRec->ucDesiredPhyTypeSet;

	prAisBssInfo->ucNonHTBasicPhyType = prStaRec->ucNonHTBasicPhyType;

	prAisBssInfo->u2OperationalRateSet = prStaRec->u2OperationalRateSet;
	prAisBssInfo->u2BSSBasicRateSet = prStaRec->u2BSSBasicRateSet;

	rateGetDataRatesFromRateSet(prAisBssInfo->u2OperationalRateSet,
				    prAisBssInfo->u2BSSBasicRateSet,
				    prAisBssInfo->aucAllSupportedRates,
				    &prAisBssInfo->ucAllSupportedRatesLen);

	/* 3 <3> X Update BSS_INFO_T from SW_RFB_T (Association Resp Frame) */


	/* 3 <4> Update BSS_INFO_T from BSS_DESC_T */
	prBssDesc = scanSearchBssDescByTA(prAdapter, prStaRec->aucMacAddr);
	if (prBssDesc) {
		prBssDesc->fgIsConnecting = FALSE;
		prBssDesc->fgIsConnected = TRUE;

		/* 4 <4.1> Setup BSSID */
		COPY_MAC_ADDR(prAisBssInfo->aucBSSID, prBssDesc->aucBSSID);

		/* 4 <4.2> Setup Channel, Band */
		prAisBssInfo->ucPrimaryChannel = prBssDesc->ucChannelNum;
		prAisBssInfo->eBand = prBssDesc->eBand;

		/* 4 <4.3> Setup MIB for current BSS */
		prAisBssInfo->u2BeaconInterval = prBssDesc->u2BeaconInterval;
		prAisBssInfo->ucDTIMPeriod = 0;
		prAisBssInfo->u2ATIMWindow = 0;	/* TBD(Kevin) */

		prAisBssInfo->ucBeaconTimeoutCount = AIS_BEACON_TIMEOUT_COUNT_ADHOC;
	} else {
		/* should never happen */
		ASSERT(0);
	}


	/* 3 <5> Set MAC HW */
	/* 4 <5.1> Find Lowest Basic Rate Index for default TX Rate of MMPDU */
	nicTxUpdateBssDefaultRate(prAisBssInfo);

	/* 4 <5.2> Setup channel and bandwidth */
	rlmBssInitForAPandIbss(prAdapter, prAisBssInfo);

	/* 4 <5.3> use command packets to inform firmware */
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <5.4> enable beaconing */
	bssUpdateBeaconContent(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 4 <5.5> Update AdHoc PM parameter */
	nicPmIndicateBssConnected(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* 3 <6> Set ACTIVE flag. */
	prAisBssInfo->fgIsBeaconActivated = TRUE;
	prAisBssInfo->fgHoldSameBssidForIBSS = TRUE;

	return;
}				/* end of aisUpdateBssInfoForMergeIBSS() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Probe Request Frame and then return
*        result to BSS to indicate if need to send the corresponding Probe Response
*        Frame if the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[out] pu4ControlFlags   Control flags for replying the Probe Response
*
* @retval TRUE      Reply the Probe Response
* @retval FALSE     Don't reply the Probe Response
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
aisValidateProbeReq(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb, OUT PUINT_32 pu4ControlFlags)
{
	P_WLAN_MAC_MGMT_HEADER_T prMgtHdr;
	P_BSS_INFO_T prBssInfo;
	P_IE_SSID_T prIeSsid = (P_IE_SSID_T) NULL;
	PUINT_8 pucIE;
	UINT_16 u2IELength;
	UINT_16 u2Offset = 0;
	BOOLEAN fgReplyProbeResp = FALSE;


	ASSERT(prSwRfb);
	ASSERT(pu4ControlFlags);

	prBssInfo = prAdapter->prAisBssInfo;

	/* 4 <1> Parse Probe Req IE and Get IE ptr (SSID, Supported Rate IE, ...) */
	prMgtHdr = (P_WLAN_MAC_MGMT_HEADER_T) prSwRfb->pvHeader;

	u2IELength = prSwRfb->u2PacketLen - prSwRfb->u2HeaderLen;
	pucIE = (PUINT_8) ((ULONG) prSwRfb->pvHeader + prSwRfb->u2HeaderLen);

	IE_FOR_EACH(pucIE, u2IELength, u2Offset) {
		if (ELEM_ID_SSID == IE_ID(pucIE)) {
			if ((!prIeSsid) && (IE_LEN(pucIE) <= ELEM_MAX_LEN_SSID))
				prIeSsid = (P_IE_SSID_T) pucIE;

			break;
		}
	}			/* end of IE_FOR_EACH */

	/* 4 <2> Check network conditions */

	if (prBssInfo->eCurrentOPMode == OP_MODE_IBSS) {

		if ((prIeSsid) && ((prIeSsid->ucLength == BC_SSID_LEN) ||	/* WILDCARD SSID */
				   EQUAL_SSID(prBssInfo->aucSSID, prBssInfo->ucSSIDLen,	/* CURRENT SSID */
					      prIeSsid->aucSSID, prIeSsid->ucLength))) {
			fgReplyProbeResp = TRUE;
		}
	}

	return fgReplyProbeResp;

}				/* end of aisValidateProbeReq() */

#endif				/* CFG_SUPPORT_ADHOC */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will modify and update necessary information to firmware
*        for disconnection handling
*
* @param[in] prAdapter          Pointer to the Adapter structure.
*
* @retval None
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmDisconnect(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgDelayIndication)
{
	P_BSS_INFO_T prAisBssInfo;
	UINT_8 aucP2pSsid[] = CTIA_MAGIC_SSID;

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;

	nicPmIndicateBssAbort(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

#if CFG_SUPPORT_ADHOC
	if (prAisBssInfo->fgIsBeaconActivated) {
		nicUpdateBeaconIETemplate(prAdapter,
					  IE_UPD_METHOD_DELETE_ALL,
					  prAdapter->prAisBssInfo->ucBssIndex, 0, NULL, 0);

		prAisBssInfo->fgIsBeaconActivated = FALSE;
	}
#endif

	rlmBssAborted(prAdapter, prAisBssInfo);

	/* 4 <3> Unset the fgIsConnected flag of BSS_DESC_T and send Deauth if needed. */
	if (PARAM_MEDIA_STATE_CONNECTED == prAisBssInfo->eConnectionState) {

		/* add for ctia mode */
		{


			if (EQUAL_SSID
			    (aucP2pSsid, CTIA_MAGIC_SSID_LEN, prAisBssInfo->aucSSID,
			     prAisBssInfo->ucSSIDLen)) {
				nicEnterCtiaMode(prAdapter, FALSE, FALSE);
			}
		}

		if (prAisBssInfo->ucReasonOfDisconnect == DISCONNECT_REASON_CODE_RADIO_LOST) {
#if CFG_SUPPORT_AUTORN
			if (prAisBssInfo->fgDisConnReassoc == FALSE)
#endif
			{
				scanRemoveBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);

				/* remove from scanning results as well */
				wlanClearBssInScanningResult(prAdapter, prAisBssInfo->aucBSSID);
			}


			/* trials for re-association */
			if (fgDelayIndication) {
				aisFsmIsRequestPending(prAdapter, AIS_REQUEST_RECONNECT, TRUE);
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_RECONNECT);
			}
		} else {
			scanRemoveConnFlagOfBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);
		}

		if (fgDelayIndication) {
			if (OP_MODE_IBSS != prAisBssInfo->eCurrentOPMode)
				prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
		} else {
			prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
		}
	} else {
		prAisBssInfo->fgHoldSameBssidForIBSS = FALSE;
	}

	/* 4 <4> Change Media State immediately. */
	if (prAisBssInfo->ucReasonOfDisconnect != DISCONNECT_REASON_CODE_REASSOCIATION) {
		aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED);

		/* 4 <4.1> sync. with firmware */
		nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
	}

	if (!fgDelayIndication) {
		/* 4 <5> Deactivate previous AP's STA_RECORD_T or all Clients in Driver if have. */
		if (prAisBssInfo->prStaRecOfAP) {
			/* cnmStaRecChangeState(prAdapter, prAisBssInfo->prStaRecOfAP, STA_STATE_1); */
			prAisBssInfo->prStaRecOfAP = (P_STA_RECORD_T) NULL;
		}
	}
#if CFG_SUPPORT_ROAMING
	roamingFsmRunEventAbort(prAdapter);

	/* clear pending roaming connection request */
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
	aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);
#endif				/* CFG_SUPPORT_ROAMING */

	/* 4 <6> Indicate Disconnected Event to Host */
	aisIndicationOfMediaStateToHost(prAdapter,
					PARAM_MEDIA_STATE_DISCONNECTED, fgDelayIndication);


	/* 4 <7> Trigger AIS FSM */
	aisFsmSteps(prAdapter, AIS_STATE_IDLE);

	return;
}				/* end of aisFsmDisconnect() */

extern MTK_WCN_BOOL mtk_wcn_wmt_assert(ENUM_WMTDRV_TYPE_T type, UINT_32 reason);
static VOID
aisFsmRunEventScanDoneTimeOut (
    IN P_ADAPTER_T prAdapter,
    ULONG ulParam
    )
{
    P_AIS_FSM_INFO_T prAisFsmInfo;
    ENUM_AIS_STATE_T eNextState;
    P_CONNECTION_SETTINGS_T prConnSettings;

    DEBUGFUNC("aisFsmRunEventScanDoneTimeOut()");

    ASSERT(prAdapter);

    prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
    prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

    DBGLOG(AIS, STATE, ("aisFsmRunEventScanDoneTimeOut Current[%d]\n",prAisFsmInfo->eCurrentState));
#if (CFG_WATCH_DOG_ENABLE == 1)
    //assert if and only if in normal mode
    mtk_wcn_wmt_assert(WMTDRV_TYPE_WIFI, 40);
	return;
#endif

	/* report all scanned frames to upper layer to avoid scanned frame is timeout */
	/* must be put before kalScanDone */
//	scanReportBss2Cfg80211(prAdapter,BSS_TYPE_INFRASTRUCTURE,NULL);

    prConnSettings->fgIsScanReqIssued = FALSE;
    kalScanDone(prAdapter->prGlueInfo, KAL_NETWORK_TYPE_AIS_INDEX, WLAN_STATUS_SUCCESS);
    eNextState = prAisFsmInfo->eCurrentState;

    switch (prAisFsmInfo->eCurrentState) {
            case AIS_STATE_SCAN:
                prAisFsmInfo->u4ScanIELength = 0;
                eNextState = AIS_STATE_IDLE;
                break;
            case AIS_STATE_ONLINE_SCAN:
                /* reset scan IE buffer */
                prAisFsmInfo->u4ScanIELength = 0;
#if CFG_SUPPORT_ROAMING
                eNextState = aisFsmRoamingScanResultsUpdate(prAdapter);
#else
                eNextState = AIS_STATE_NORMAL_TR;
#endif /* CFG_SUPPORT_ROAMING */
                break;
            default:
                break;
        }

		/* try to stop scan in CONNSYS */
		aisFsmStateAbort_SCAN(prAdapter);

//	wlanQueryDebugCode(prAdapter); /* display current SCAN FSM in FW, debug use */

        if (eNextState != prAisFsmInfo->eCurrentState) {
            aisFsmSteps(prAdapter, eNextState);
        }

    return;
} /* end of aisFsmBGSleepTimeout() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Background Scan Time-Out" to AIS FSM.
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventBGSleepTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;

	DEBUGFUNC("aisFsmRunEventBGSleepTimeOut()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_WAIT_FOR_NEXT_SCAN:
		DBGLOG(AIS, LOUD,
		       ("EVENT - SCAN TIMER: Idle End - Current Time = %u\n", kalGetTimeTick()));

		eNextState = AIS_STATE_LOOKING_FOR;

		SET_NET_PWR_STATE_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

		break;

	default:
		break;
	}

	/* Call aisFsmSteps() when we are going to change AIS STATE */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);


	return;
}				/* end of aisFsmBGSleepTimeout() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "IBSS ALONE Time-Out" to AIS FSM.
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventIbssAloneTimeOut(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;

	DEBUGFUNC("aisFsmRunEventIbssAloneTimeOut()");

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	eNextState = prAisFsmInfo->eCurrentState;

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_IBSS_ALONE:

		/* There is no one participate in our AdHoc during this TIMEOUT Interval
		 * so go back to search for a valid IBSS again.
		 */

		DBGLOG(AIS, LOUD, ("EVENT-IBSS ALONE TIMER: Start pairing\n"));

		prAisFsmInfo->fgTryScan = TRUE;

		/* abort timer */
		aisFsmReleaseCh(prAdapter);

		/* Pull back to SEARCH to find candidate again */
		eNextState = AIS_STATE_SEARCH;

		break;

	default:
		break;
	}


	/* Call aisFsmSteps() when we are going to change AIS STATE */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);


	return;
}				/* end of aisIbssAloneTimeOut() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Join Time-Out" to AIS FSM.
*
* @param[in] u4Param  Unused timer parameter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventJoinTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	ENUM_AIS_STATE_T eNextState;
	OS_SYSTIME rCurrentTime;

	DEBUGFUNC("aisFsmRunEventJoinTimeout()");

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	eNextState = prAisFsmInfo->eCurrentState;

	GET_CURRENT_SYSTIME(&rCurrentTime);

	switch (prAisFsmInfo->eCurrentState) {
	case AIS_STATE_JOIN:
		DBGLOG(AIS, LOUD, ("EVENT- JOIN TIMEOUT\n"));

		/* 1. Do abort JOIN */
		aisFsmStateAbort_JOIN(prAdapter);

		/* 2. Increase Join Failure Count */
		prAisFsmInfo->prTargetStaRec->ucJoinFailureCount++;

		if (prAisFsmInfo->prTargetStaRec->ucJoinFailureCount < JOIN_MAX_RETRY_FAILURE_COUNT) {
			/* 3.1 Retreat to AIS_STATE_SEARCH state for next try */
			eNextState = AIS_STATE_SEARCH;
		} else if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
			/* roaming cases */
			/* 3.2 Retreat to AIS_STATE_WAIT_FOR_NEXT_SCAN state for next try */
			eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		} else
		    if (prAisFsmInfo->rJoinReqTime != 0 &&
		    	!CHECK_FOR_TIMEOUT(rCurrentTime,
					   prAisFsmInfo->rJoinReqTime,
					   SEC_TO_SYSTIME(AIS_JOIN_TIMEOUT))) {
			/* 3.3 Retreat to AIS_STATE_WAIT_FOR_NEXT_SCAN state for next try */
			eNextState = AIS_STATE_WAIT_FOR_NEXT_SCAN;
		} else {
			/* 3.4 Retreat to AIS_STATE_JOIN_FAILURE to terminate join operation */
			prAisFsmInfo->prTargetStaRec->u2StatusCode = STATUS_CODE_JOIN_TIMEOUT;
			eNextState = AIS_STATE_JOIN_FAILURE;
		}

		break;

	case AIS_STATE_NORMAL_TR:
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);
		prAisFsmInfo->fgIsInfraChannelFinished = TRUE;

		/* 2. process if there is pending scan */
		if (aisFsmIsRequestPending(prAdapter, AIS_REQUEST_SCAN, TRUE) == TRUE) {
			wlanClearScanningResult(prAdapter);
			eNextState = AIS_STATE_ONLINE_SCAN;
		}

		break;

	default:
		/* release channel */
		aisFsmReleaseCh(prAdapter);
		break;

	}


	/* Call aisFsmSteps() when we are going to change AIS STATE */
	if (eNextState != prAisFsmInfo->eCurrentState)
		aisFsmSteps(prAdapter, eNextState);


	return;
}				/* end of aisFsmRunEventJoinTimeout() */

VOID aisFsmRunEventDeauthTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	aisDeauthXmitComplete(prAdapter, NULL, TX_RESULT_LIFE_TIMEOUT);
}

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
VOID aisFsmRunEventSecModeChangeTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	DBGLOG(AIS, INFO, ("Beacon security mode change timeout, trigger disconnect!\n"));
	aisBssSecurityChanged(prAdapter);
}
#endif
#if defined(CFG_TEST_MGMT_FSM) && (CFG_TEST_MGMT_FSM != 0)
/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisTest(VOID)
{
	P_MSG_AIS_ABORT_T prAisAbortMsg;
	P_CONNECTION_SETTINGS_T prConnSettings;
	UINT_8 aucSSID[] = "pci-11n";
	UINT_8 ucSSIDLen = 7;

	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* Set Connection Request Issued Flag */
	prConnSettings->fgIsConnReqIssued = TRUE;
	prConnSettings->ucSSIDLen = ucSSIDLen;
	kalMemCopy(prConnSettings->aucSSID, aucSSID, ucSSIDLen);

	prAisAbortMsg =
	    (P_MSG_AIS_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_AIS_ABORT_T));
	if (!prAisAbortMsg) {

		ASSERT(0);	/* Can't trigger SCAN FSM */
		return;
	}

	prAisAbortMsg->rMsgHdr.eMsgId = MID_HEM_AIS_FSM_ABORT;

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prAisAbortMsg, MSG_SEND_METHOD_BUF);

	wifi_send_msg(INDX_WIFI, MSG_ID_WIFI_IST, 0);

	return;
}
#endif				/* CFG_TEST_MGMT_FSM */


/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is used to handle OID_802_11_BSSID_LIST_SCAN
*
* \param[in] prAdapter  Pointer of ADAPTER_T
* \param[in] prSsid     Pointer of SSID_T if specified
* \param[in] pucIe      Pointer to buffer of extra information elements to be attached
* \param[in] u4IeLength Length of information elements
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
aisFsmScanRequest(IN P_ADAPTER_T prAdapter,
		  IN P_PARAM_SSID_T prSsid, IN PUINT_8 pucIe, IN UINT_32 u4IeLength)
{
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("aisFsmScanRequest()");

	ASSERT(prAdapter);
	ASSERT(u4IeLength <= MAX_IE_LENGTH);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;

		if (prSsid == NULL) {
			prAisFsmInfo->ucScanSSIDNum = 0;
		} else {
			prAisFsmInfo->ucScanSSIDNum = 1;

			COPY_SSID(prAisFsmInfo->arScanSSID[0].aucSsid,
				  prAisFsmInfo->arScanSSID[0].u4SsidLen,
				  prSsid->aucSsid, prSsid->u4SsidLen);
		}

		if (u4IeLength > 0) {
			prAisFsmInfo->u4ScanIELength = u4IeLength;
			kalMemCopy(prAisFsmInfo->aucScanIEBuf, pucIe, u4IeLength);
		} else {
			prAisFsmInfo->u4ScanIELength = 0;
		}

		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE
			    && prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				/* 802.1x might not finished yet, pend it for later handling .. */
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(AIS, WARN,
					       ("Scan Request with channel granted for join operation: %d, %d",
						prAisFsmInfo->fgIsChannelGranted,
						prAisFsmInfo->fgIsChannelRequested));
				}

				/* start online scan */
				wlanClearScanningResult(prAdapter);
				aisFsmSteps(prAdapter, AIS_STATE_ONLINE_SCAN);
			}
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) {
			wlanClearScanningResult(prAdapter);
			aisFsmSteps(prAdapter, AIS_STATE_SCAN);
		} else {
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
		}
	} else {
		DBGLOG(AIS, WARN,
		       ("Scan Request dropped. (state: %d)\n", prAisFsmInfo->eCurrentState));
	}

	return;
}				/* end of aisFsmScanRequest() */


/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is used to handle OID_802_11_BSSID_LIST_SCAN
*
* \param[in] prAdapter  Pointer of ADAPTER_T
* \param[in] ucSsidNum  Number of SSID
* \param[in] prSsid     Pointer to the array of SSID_T if specified
* \param[in] pucIe      Pointer to buffer of extra information elements to be attached
* \param[in] u4IeLength Length of information elements
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
aisFsmScanRequestAdv(IN P_ADAPTER_T prAdapter,
		     IN UINT_8 ucSsidNum,
		     IN P_PARAM_SSID_T prSsid, IN PUINT_8 pucIe, IN UINT_32 u4IeLength)
{
	UINT_32 i;
	P_CONNECTION_SETTINGS_T prConnSettings;
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	DEBUGFUNC("aisFsmScanRequestAdv()");

	ASSERT(prAdapter);
	ASSERT(ucSsidNum <= SCN_SSID_MAX_NUM);
	ASSERT(u4IeLength <= MAX_IE_LENGTH);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	if (!prConnSettings->fgIsScanReqIssued) {
		prConnSettings->fgIsScanReqIssued = TRUE;

		if (ucSsidNum == 0) {
			prAisFsmInfo->ucScanSSIDNum = 0;
		} else {
			prAisFsmInfo->ucScanSSIDNum = ucSsidNum;

			for (i = 0; i < ucSsidNum; i++) {
				COPY_SSID(prAisFsmInfo->arScanSSID[i].aucSsid,
					  prAisFsmInfo->arScanSSID[i].u4SsidLen,
					  prSsid[i].aucSsid, prSsid[i].u4SsidLen);
			}
		}

		if (u4IeLength > 0) {
			prAisFsmInfo->u4ScanIELength = u4IeLength;
			kalMemCopy(prAisFsmInfo->aucScanIEBuf, pucIe, u4IeLength);
		} else {
			prAisFsmInfo->u4ScanIELength = 0;
		}

		if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
			if (prAisBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE
			    && prAisFsmInfo->fgIsInfraChannelFinished == FALSE) {
				/* 802.1x might not finished yet, pend it for later handling .. */
				aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
			} else {
				if (prAisFsmInfo->fgIsChannelGranted == TRUE) {
					DBGLOG(AIS, WARN,
					       ("Scan Request with channel granted for join operation: %d, %d",
						prAisFsmInfo->fgIsChannelGranted,
						prAisFsmInfo->fgIsChannelRequested));
				}

				/* start online scan */
				wlanClearScanningResult(prAdapter);
				aisFsmSteps(prAdapter, AIS_STATE_ONLINE_SCAN);
			}
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE) {
			wlanClearScanningResult(prAdapter);
			aisFsmSteps(prAdapter, AIS_STATE_SCAN);
		} else {
			aisFsmInsertRequest(prAdapter, AIS_REQUEST_SCAN);
		}
	} else {
		DBGLOG(AIS, WARN,
		       ("Scan Request dropped. (state: %d)\n", prAisFsmInfo->eCurrentState));
	}

	return;
}				/* end of aisFsmScanRequestAdv() */


/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is invoked when CNM granted channel privilege
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventChGrant(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_BSS_INFO_T prAisBssInfo;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_CH_GRANT_T prMsgChGrant;
	UINT_8 ucTokenID;
	UINT_32 u4GrantInterval;

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prMsgChGrant = (P_MSG_CH_GRANT_T) prMsgHdr;

	ucTokenID = prMsgChGrant->ucTokenID;
	u4GrantInterval = prMsgChGrant->u4GrantInterval;

	/* 1. free message */
	cnmMemFree(prAdapter, prMsgHdr);

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_CHANNEL_JOIN &&
	    prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		/* 2. channel privilege has been approved */
		prAisFsmInfo->u4ChGrantedInterval = u4GrantInterval;

		/* 3. state transition to join/ibss-alone/ibss-merge */
		/* 3.1 set timeout timer in cases join could not be completed */
		cnmTimerStartTimer(prAdapter,
				   &prAisFsmInfo->rJoinTimeoutTimer,
				   prAisFsmInfo->u4ChGrantedInterval - AIS_JOIN_CH_GRANT_THRESHOLD);
		/* 3.2 set local variable to indicate join timer is ticking */
		prAisFsmInfo->fgIsInfraChannelFinished = FALSE;

		/* 3.3 switch to join state */
		aisFsmSteps(prAdapter, AIS_STATE_JOIN);

		prAisFsmInfo->fgIsChannelGranted = TRUE;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_REMAIN_ON_CHANNEL &&
		   prAisFsmInfo->ucSeqNumOfChReq == ucTokenID) {
		/* 2. channel privilege has been approved */
		prAisFsmInfo->u4ChGrantedInterval = u4GrantInterval;

		/* 3.1 set timeout timer in cases upper layer cancel_remain_on_channel never comes */
		cnmTimerStartTimer(prAdapter,
				   &prAisFsmInfo->rChannelTimeoutTimer,
				   prAisFsmInfo->u4ChGrantedInterval);

		/* 3.2 switch to remain_on_channel state */
		aisFsmSteps(prAdapter, AIS_STATE_REMAIN_ON_CHANNEL);

		/* 3.3. indicate upper layer for channel ready */
		kalReadyOnChannel(prAdapter->prGlueInfo,
				  prAisFsmInfo->rChReqInfo.u8Cookie,
				  prAisFsmInfo->rChReqInfo.eBand,
				  prAisFsmInfo->rChReqInfo.eSco,
				  prAisFsmInfo->rChReqInfo.ucChannelNum,
				  prAisFsmInfo->rChReqInfo.u4DurationMs);

		prAisFsmInfo->fgIsChannelGranted = TRUE;
	} else {		/* mismatched grant */
		/* 2. return channel privilege to CNM immediately */
		aisFsmReleaseCh(prAdapter);
	}

	return;
}				/* end of aisFsmRunEventChGrant() */


/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to inform CNM that channel privilege
*           has been released
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmReleaseCh(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_CH_ABORT_T prMsgChAbort;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	if (prAisFsmInfo->fgIsChannelGranted == TRUE || prAisFsmInfo->fgIsChannelRequested == TRUE) {

		prAisFsmInfo->fgIsChannelRequested = FALSE;
		prAisFsmInfo->fgIsChannelGranted = FALSE;

		/* 1. return channel privilege to CNM immediately */
		prMsgChAbort =
		    (P_MSG_CH_ABORT_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_CH_ABORT_T));
		if (!prMsgChAbort) {
			ASSERT(0);	/* Can't release Channel to CNM */
			return;
		}

		prMsgChAbort->rMsgHdr.eMsgId = MID_MNY_CNM_CH_ABORT;
		prMsgChAbort->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;
		prMsgChAbort->ucTokenID = prAisFsmInfo->ucSeqNumOfChReq;

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChAbort, MSG_SEND_METHOD_BUF);
	}

	return;
}				/* end of aisFsmReleaseCh() */


/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to inform AIS that corresponding beacon has not
*           been received for a while and probing is not successful
*
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID aisBssBeaconTimeout(IN P_ADAPTER_T prAdapter)
{
	P_BSS_INFO_T prAisBssInfo;
	BOOLEAN fgDoAbortIndication = FALSE;
	P_CONNECTION_SETTINGS_T prConnSettings;

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* 4 <1> Diagnose Connection for Beacon Timeout Event */
	if (PARAM_MEDIA_STATE_CONNECTED == prAisBssInfo->eConnectionState) {
		if (OP_MODE_INFRASTRUCTURE == prAisBssInfo->eCurrentOPMode) {
			P_STA_RECORD_T prStaRec = prAisBssInfo->prStaRecOfAP;

			if (prStaRec)
				fgDoAbortIndication = TRUE;

		} else if (OP_MODE_IBSS == prAisBssInfo->eCurrentOPMode) {
			fgDoAbortIndication = TRUE;
		}
	}
	/* 4 <2> invoke abort handler */
	if (fgDoAbortIndication) {
		prConnSettings->fgIsDisconnectedByNonRequest = FALSE;
		if (prConnSettings->eReConnectLevel < RECONNECT_LEVEL_USER_SET) {
			prConnSettings->eReConnectLevel = RECONNECT_LEVEL_BEACON_TIMEOUT;
			prConnSettings->fgIsConnReqIssued = TRUE;
		}
		aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_RADIO_LOST, TRUE);
	}

	return;
}				/* end of aisBssBeaconTimeout() */

#if CFG_SUPPORT_DETECT_SECURITY_MODE_CHANGE
VOID aisBssSecurityChanged(P_ADAPTER_T prAdapter)
{
	prAdapter->rWifiVar.rConnSettings.fgIsDisconnectedByNonRequest = TRUE;
	aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_DEAUTHENTICATED, FALSE);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief    This function is to inform AIS that DEAUTH frame has been
*           sent and thus state machine could go ahead
*
* \param[in] prAdapter  Pointer of ADAPTER_T
* \param[in] prMsduInfo Pointer of MSDU_INFO_T for DEAUTH frame
* \param[in] prAdapter  Pointer of ADAPTER_T
*
* \return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
aisDeauthXmitComplete(IN P_ADAPTER_T prAdapter,
		      IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	if (rTxDoneStatus == TX_RESULT_SUCCESS)
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rDeauthDoneTimer);


	if (prAisFsmInfo->eCurrentState == AIS_STATE_DISCONNECTING) {
		if (rTxDoneStatus != TX_RESULT_DROPPED_IN_DRIVER
		    && rTxDoneStatus != TX_RESULT_QUEUE_CLEARANCE) {
			aisFsmStateAbort(prAdapter, DISCONNECT_REASON_CODE_NEW_CONNECTION, FALSE);
		}
	} else {
		DBGLOG(AIS, WARN, ("DEAUTH frame transmitted without further handling"));
	}

	return WLAN_STATUS_SUCCESS;

}				/* end of aisDeauthXmitComplete() */

#if CFG_SUPPORT_ROAMING
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will indicate an Event of "Looking for a candidate due to weak signal" to AIS FSM.
*
* @param[in] u4ReqScan  Requesting Scan or not
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRunEventRoamingDiscovery(IN P_ADAPTER_T prAdapter, UINT_32 u4ReqScan)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;
	ENUM_AIS_REQUEST_TYPE_T eAisRequest;

	DBGLOG(AIS, LOUD, ("aisFsmRunEventRoamingDiscovery()\n"));

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	/* search candidates by best rssi */
	prConnSettings->eConnectionPolicy = CONNECT_BY_SSID_BEST_RSSI;

	/* TODO: Stop roaming event in FW */
#if CFG_SUPPORT_WFD
#if CFG_ENABLE_WIFI_DIRECT
	{
		/* Check WFD is running */
		P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;
		prWfdCfgSettings = &(prAdapter->rWifiVar.rWfdConfigureSettings);
		if ((prWfdCfgSettings->ucWfdEnable != 0)) {
			DBGLOG(ROAMING, INFO, ("WFD is running. Stop roaming. [WfdEnable:%u]\n", prWfdCfgSettings->ucWfdEnable));
			roamingFsmRunEventRoam(prAdapter);
			roamingFsmRunEventFail(prAdapter, ROAMING_FAIL_REASON_NOCANDIDATE);
			return;
		}
	}
#endif
#endif

	/* results are still new */
	if (!u4ReqScan) {
		roamingFsmRunEventRoam(prAdapter);
		eAisRequest = AIS_REQUEST_ROAMING_CONNECT;
	} else {
		if (prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN
		    || prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR) {
			eAisRequest = AIS_REQUEST_ROAMING_CONNECT;
		} else {
			eAisRequest = AIS_REQUEST_ROAMING_SEARCH;
		}
	}

	if (prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR
	    && prAisFsmInfo->fgIsInfraChannelFinished == TRUE) {
		if (eAisRequest == AIS_REQUEST_ROAMING_SEARCH)
			aisFsmSteps(prAdapter, AIS_STATE_LOOKING_FOR);
		else
			aisFsmSteps(prAdapter, AIS_STATE_SEARCH);
	} else {
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_SEARCH, TRUE);
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_ROAMING_CONNECT, TRUE);

		aisFsmInsertRequest(prAdapter, eAisRequest);
	}

	return;
}				/* end of aisFsmRunEventRoamingDiscovery() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Update the time of ScanDone for roaming and transit to Roam state.
*
* @param (none)
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
ENUM_AIS_STATE_T aisFsmRoamingScanResultsUpdate(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_ROAMING_INFO_T prRoamingFsmInfo;
	ENUM_AIS_STATE_T eNextState;

	DBGLOG(AIS, LOUD, ("->aisFsmRoamingScanResultsUpdate()\n"));

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prRoamingFsmInfo = (P_ROAMING_INFO_T) &(prAdapter->rWifiVar.rRoamingInfo);

	roamingFsmScanResultsUpdate(prAdapter);

	eNextState = prAisFsmInfo->eCurrentState;
	if (prRoamingFsmInfo->eCurrentState == ROAMING_STATE_DISCOVERY) {
		roamingFsmRunEventRoam(prAdapter);
		eNextState = AIS_STATE_SEARCH;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_LOOKING_FOR) {
		eNextState = AIS_STATE_SEARCH;
	} else if (prAisFsmInfo->eCurrentState == AIS_STATE_ONLINE_SCAN) {
		eNextState = AIS_STATE_NORMAL_TR;
	}

	return eNextState;
}				/* end of aisFsmRoamingScanResultsUpdate() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will modify and update necessary information to firmware
*        for disconnection of last AP before switching to roaming bss.
*
* @param IN prAdapter          Pointer to the Adapter structure.
*           prTargetStaRec     Target of StaRec of roaming
*
* @retval None
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmRoamingDisconnectPrevAP(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prTargetStaRec)
{
	P_BSS_INFO_T prAisBssInfo;

	DBGLOG(AIS, LOUD, ("aisFsmRoamingDisconnectPrevAP()"));

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;

	nicPmIndicateBssAbort(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);

	/* Not invoke rlmBssAborted() here to avoid prAisBssInfo->fg40mBwAllowed
	 * to be reset. RLM related parameters will be reset again when handling
	 * association response in rlmProcessAssocRsp(). 20110413
	 */
	/* rlmBssAborted(prAdapter, prAisBssInfo); */

	/* 4 <3> Unset the fgIsConnected flag of BSS_DESC_T and send Deauth if needed. */
	if (PARAM_MEDIA_STATE_CONNECTED == prAisBssInfo->eConnectionState)
		scanRemoveConnFlagOfBssDescByBssid(prAdapter, prAisBssInfo->aucBSSID);

	/* 4 <4> Change Media State immediately. */
	aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_DISCONNECTED);

	/* 4 <4.1> sync. with firmware */
	prTargetStaRec->ucBssIndex = (MAX_BSS_INDEX + 1);	/* Virtial BSSID */
	nicUpdateBss(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
	prTargetStaRec->ucBssIndex = prAdapter->prAisBssInfo->ucBssIndex;

	return;
}				/* end of aisFsmRoamingDisconnectPrevAP() */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function will update the contain of BSS_INFO_T for AIS network once
*        the roaming was completed.
*
* @param IN prAdapter          Pointer to the Adapter structure.
*           prStaRec           StaRec of roaming AP
*           prAssocRspSwRfb
*
* @retval None
*/
/*----------------------------------------------------------------------------*/
VOID
aisUpdateBssInfoForRoamingAP(IN P_ADAPTER_T prAdapter,
			     IN P_STA_RECORD_T prStaRec, IN P_SW_RFB_T prAssocRspSwRfb)
{
	P_BSS_INFO_T prAisBssInfo;

	DBGLOG(AIS, LOUD, ("aisUpdateBssInfoForRoamingAP()"));

	ASSERT(prAdapter);

	prAisBssInfo = prAdapter->prAisBssInfo;

	/* 4 <1.1> Change FW's Media State immediately. */
	aisChangeMediaState(prAdapter, PARAM_MEDIA_STATE_CONNECTED);

	/* 4 <1.2> Deactivate previous AP's STA_RECORD_T in Driver if have. */
	if ((prAisBssInfo->prStaRecOfAP) &&
	    (prAisBssInfo->prStaRecOfAP != prStaRec) && (prAisBssInfo->prStaRecOfAP->fgIsInUse)) {
		/* cnmStaRecChangeState(prAdapter, prAisBssInfo->prStaRecOfAP, STA_STATE_1); */
		cnmStaRecFree(prAdapter, prAisBssInfo->prStaRecOfAP);
	}

	/* 4 <1.4> Update BSS_INFO_T */
	aisUpdateBssInfoForJOIN(prAdapter, prStaRec, prAssocRspSwRfb);

	/* 4 <1.3> Activate current AP's STA_RECORD_T in Driver. */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

	/* 4 <1.6> Indicate Connected Event to Host immediately. */
	/* Require BSSID, Association ID, Beacon Interval.. from AIS_BSS_INFO_T */
	aisIndicationOfMediaStateToHost(prAdapter, PARAM_MEDIA_STATE_CONNECTED, FALSE);

	return;
}				/* end of aisFsmRoamingUpdateBss() */

#endif				/* CFG_SUPPORT_ROAMING */


/*----------------------------------------------------------------------------*/
/*!
* @brief Check if there is any pending request and remove it (optional)
*
* @param prAdapter
*        eReqType
*        bRemove
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
aisFsmIsRequestPending(IN P_ADAPTER_T prAdapter,
		       IN ENUM_AIS_REQUEST_TYPE_T eReqType, IN BOOLEAN bRemove)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_REQ_HDR_T prPendingReqHdr, prPendingReqHdrNext;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	/* traverse through pending request list */
	LINK_FOR_EACH_ENTRY_SAFE(prPendingReqHdr,
				 prPendingReqHdrNext,
				 &(prAisFsmInfo->rPendingReqList), rLinkEntry, AIS_REQ_HDR_T) {
		/* check for specified type */
		if (prPendingReqHdr->eReqType == eReqType) {
			/* check if need to remove */
			if (bRemove == TRUE) {
				LINK_REMOVE_KNOWN_ENTRY(&(prAisFsmInfo->rPendingReqList),
							&(prPendingReqHdr->rLinkEntry));

				cnmMemFree(prAdapter, prPendingReqHdr);
			}

			return TRUE;
		}
	}

	return FALSE;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Get next pending request
*
* @param prAdapter
*
* @return P_AIS_REQ_HDR_T
*/
/*----------------------------------------------------------------------------*/
P_AIS_REQ_HDR_T aisFsmGetNextRequest(IN P_ADAPTER_T prAdapter)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_REQ_HDR_T prPendingReqHdr;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	LINK_REMOVE_HEAD(&(prAisFsmInfo->rPendingReqList), prPendingReqHdr, P_AIS_REQ_HDR_T);
	/*save partional scan puScanChannel info to prGlueInfo*/
	if ((prPendingReqHdr != NULL) && (prAdapter->prGlueInfo != NULL)) {
		if (prAdapter->prGlueInfo->puScanChannel != NULL) {
			DBGLOG(INIT, TRACE, ("prGlueInfo error puScanChannel=%p", prAdapter->prGlueInfo->puScanChannel));
		}

		if (prPendingReqHdr->pu8ChannelInfo != NULL) {
			prAdapter->prGlueInfo->puScanChannel = prPendingReqHdr->pu8ChannelInfo;
			prPendingReqHdr->pu8ChannelInfo = NULL;
		}
	}
	return prPendingReqHdr;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Insert a new request
*
* @param prAdapter
*        eReqType
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN aisFsmInsertRequest(IN P_ADAPTER_T prAdapter, IN ENUM_AIS_REQUEST_TYPE_T eReqType)
{
	P_AIS_REQ_HDR_T prAisReq;
	P_AIS_FSM_INFO_T prAisFsmInfo;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

	prAisReq = (P_AIS_REQ_HDR_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(AIS_REQ_HDR_T));

	if (!prAisReq) {
		ASSERT(0);	/* Can't generate new message */
		return FALSE;
	}

	prAisReq->eReqType = eReqType;
	prAisReq->pu8ChannelInfo = NULL;
	/*save partional scan puScanChannel info to pending scan */
	if ((prAdapter->prGlueInfo != NULL) &&
		(prAdapter->prGlueInfo->puScanChannel != NULL)) {
		prAisReq->pu8ChannelInfo = prAdapter->prGlueInfo->puScanChannel;
		prAdapter->prGlueInfo->puScanChannel = NULL;
	}

	/* attach request into pending request list */
	LINK_INSERT_TAIL(&prAisFsmInfo->rPendingReqList, &prAisReq->rLinkEntry);

	return TRUE;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Flush all pending requests
*
* @param prAdapter
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID aisFsmFlushRequest(IN P_ADAPTER_T prAdapter)
{
	P_AIS_REQ_HDR_T prAisReq;

	ASSERT(prAdapter);

	while ((prAisReq = aisFsmGetNextRequest(prAdapter)) != NULL) {
		/*for partional scan, if channnel infor exist, free channel info*/
		if (prAisReq->pu8ChannelInfo != NULL) {
			kalMemFree(prAisReq->pu8ChannelInfo, VIR_MEM_TYPE, sizeof(PARTIAL_SCAN_INFO));
		}
		cnmMemFree(prAdapter, prAisReq);
	}


	return;
}


VOID aisFsmRunEventRemainOnChannel(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_MSG_REMAIN_ON_CHANNEL_T prRemainOnChannel;
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_CONNECTION_SETTINGS_T prConnSettings;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	ASSERT(prAdapter);
	ASSERT(prMsgHdr);

	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prConnSettings = &(prAdapter->rWifiVar.rConnSettings);

	prRemainOnChannel = (P_MSG_REMAIN_ON_CHANNEL_T) prMsgHdr;

	/* record parameters */
	prAisFsmInfo->rChReqInfo.eBand = prRemainOnChannel->eBand;
	prAisFsmInfo->rChReqInfo.eSco = prRemainOnChannel->eSco;
	prAisFsmInfo->rChReqInfo.ucChannelNum = prRemainOnChannel->ucChannelNum;
	prAisFsmInfo->rChReqInfo.u4DurationMs = prRemainOnChannel->u4DurationMs;
	prAisFsmInfo->rChReqInfo.u8Cookie = prRemainOnChannel->u8Cookie;

	if (prAisFsmInfo->eCurrentState == AIS_STATE_IDLE
	    || prAisFsmInfo->eCurrentState == AIS_STATE_NORMAL_TR) {
		/* transit to next state */
		aisFsmSteps(prAdapter, AIS_STATE_REQ_REMAIN_ON_CHANNEL);
	} else {
		aisFsmInsertRequest(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL);
	}

	/* free messages */
	cnmMemFree(prAdapter, prMsgHdr);

	return;
}


VOID aisFsmRunEventCancelRemainOnChannel(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;
	P_MSG_CANCEL_REMAIN_ON_CHANNEL_T prCancelRemainOnChannel;

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	prCancelRemainOnChannel = (P_MSG_CANCEL_REMAIN_ON_CHANNEL_T) prMsgHdr;

	/* 1. Check the cookie first */
	if (prCancelRemainOnChannel->u8Cookie == prAisFsmInfo->rChReqInfo.u8Cookie) {

		/* 2. release channel privilege/request */
		if (prAisFsmInfo->eCurrentState == AIS_STATE_REQ_REMAIN_ON_CHANNEL) {
			/* 2.1 elease channel */
			aisFsmReleaseCh(prAdapter);
		} else if (prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) {
			/* 2.1 release channel */
			aisFsmReleaseCh(prAdapter);

			/* 2.2 stop channel timeout timer */
			cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);
		}

		/* 3. clear pending request of remain_on_channel */
		aisFsmIsRequestPending(prAdapter, AIS_REQUEST_REMAIN_ON_CHANNEL, TRUE);

		/* 4. decide which state to retreat */
		if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
			aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
		else
			aisFsmSteps(prAdapter, AIS_STATE_IDLE);

	}

	/* 5. free message */
	cnmMemFree(prAdapter, prMsgHdr);

	return;
}


VOID aisFsmRunEventMgmtFrameTx(IN P_ADAPTER_T prAdapter, IN P_MSG_HDR_T prMsgHdr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_MSG_MGMT_TX_REQUEST_T prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) NULL;

	do {
		ASSERT((prAdapter != NULL) && (prMsgHdr != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

		if (prAisFsmInfo == NULL)
			break;

		prMgmtTxMsg = (P_MSG_MGMT_TX_REQUEST_T) prMsgHdr;

		aisFuncTxMgmtFrame(prAdapter,
				   &prAisFsmInfo->rMgmtTxInfo,
				   prMgmtTxMsg->prMgmtMsduInfo, prMgmtTxMsg->u8Cookie);


	} while (FALSE);

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);

	return;
}				/* aisFsmRunEventMgmtFrameTx */


VOID aisFsmRunEventChannelTimeout(IN P_ADAPTER_T prAdapter, ULONG ulParamPtr)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_BSS_INFO_T prAisBssInfo;

	DEBUGFUNC("aisFsmRunEventRemainOnChannel()");

	ASSERT(prAdapter);
	prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
	prAisBssInfo = prAdapter->prAisBssInfo;

	if (prAisFsmInfo->eCurrentState == AIS_STATE_REMAIN_ON_CHANNEL) {
		/* 1. release channel */
		aisFsmReleaseCh(prAdapter);

		/* 2. stop channel timeout timer */
		cnmTimerStopTimer(prAdapter, &prAisFsmInfo->rChannelTimeoutTimer);

		/* 3. expiration indication to upper layer */
		kalRemainOnChannelExpired(prAdapter->prGlueInfo,
					  prAisFsmInfo->rChReqInfo.u8Cookie,
					  prAisFsmInfo->rChReqInfo.eBand,
					  prAisFsmInfo->rChReqInfo.eSco,
					  prAisFsmInfo->rChReqInfo.ucChannelNum);

		/* 4. decide which state to retreat */
		if (prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
			aisFsmSteps(prAdapter, AIS_STATE_NORMAL_TR);
		else
			aisFsmSteps(prAdapter, AIS_STATE_IDLE);

	} else {
		DBGLOG(AIS, WARN, ("Unexpected remain_on_channel timeout event\n"));
#if DBG
		DBGLOG(AIS, STATE,
		       ("CURRENT State: [%s]\n", apucDebugAisState[prAisFsmInfo->eCurrentState]));
#else
		DBGLOG(AIS, STATE,
		       ("[%d] CURRENT State: [%d]\n", DBG_AIS_IDX, prAisFsmInfo->eCurrentState));
#endif
	}

	return;
}


WLAN_STATUS
aisFsmRunEventMgmtFrameTxDone(IN P_ADAPTER_T prAdapter,
			      IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
	P_AIS_FSM_INFO_T prAisFsmInfo;
	P_AIS_MGMT_TX_REQ_INFO_T prMgmtTxReqInfo = (P_AIS_MGMT_TX_REQ_INFO_T) NULL;
	BOOLEAN fgIsSuccess = FALSE;

	do {
		ASSERT((prAdapter != NULL) && (prMsduInfo != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);
		prMgmtTxReqInfo = &(prAisFsmInfo->rMgmtTxInfo);

		if (rTxDoneStatus == TX_RESULT_SUCCESS)
			fgIsSuccess = TRUE;


		if (prMgmtTxReqInfo->prMgmtTxMsdu == prMsduInfo) {
			kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
						prMgmtTxReqInfo->u8Cookie,
						fgIsSuccess,
						prMsduInfo->prPacket,
						(UINT_32) prMsduInfo->u2FrameLength);

			prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
		}

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* aisFsmRunEventMgmtFrameTxDone */


WLAN_STATUS
aisFuncTxMgmtFrame(IN P_ADAPTER_T prAdapter,
		   IN P_AIS_MGMT_TX_REQ_INFO_T prMgmtTxReqInfo,
		   IN P_MSDU_INFO_T prMgmtTxMsdu, IN UINT_64 u8Cookie)
{
	WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
	P_MSDU_INFO_T prTxMsduInfo = (P_MSDU_INFO_T) NULL;
	P_WLAN_MAC_HEADER_T prWlanHdr = (P_WLAN_MAC_HEADER_T) NULL;
	P_STA_RECORD_T prStaRec = (P_STA_RECORD_T) NULL;

	do {
		ASSERT((prAdapter != NULL) && (prMgmtTxReqInfo != NULL));

		if (prMgmtTxReqInfo->fgIsMgmtTxRequested) {

			/* 1. prMgmtTxReqInfo->prMgmtTxMsdu != NULL */
			/* Packet on driver, not done yet, drop it. */
			prTxMsduInfo = prMgmtTxReqInfo->prMgmtTxMsdu;
			if (prTxMsduInfo != NULL) {

				kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
							prMgmtTxReqInfo->u8Cookie,
							FALSE,
							prTxMsduInfo->prPacket,
							(UINT_32) prTxMsduInfo->u2FrameLength);

				/* Leave it to TX Done handler. */
				/* cnmMgtPktFree(prAdapter, prTxMsduInfo); */
				prMgmtTxReqInfo->prMgmtTxMsdu = NULL;
			}
			/* 2. prMgmtTxReqInfo->prMgmtTxMsdu == NULL */
			/* Packet transmitted, wait tx done. (cookie issue) */
		}

		ASSERT(prMgmtTxReqInfo->prMgmtTxMsdu == NULL);

		prWlanHdr =
		    (P_WLAN_MAC_HEADER_T) ((ULONG) prMgmtTxMsdu->prPacket +
					   MAC_TX_RESERVED_FIELD);
		prStaRec =
		    cnmGetStaRecByAddress(prAdapter, prAdapter->prAisBssInfo->ucBssIndex,
					  prWlanHdr->aucAddr1);

		TX_SET_MMPDU(prAdapter,
			     prMgmtTxMsdu,
			     (prStaRec !=
			      NULL) ? (prStaRec->ucBssIndex) : (prAdapter->prAisBssInfo->
								ucBssIndex),
			     (prStaRec != NULL) ? (prStaRec->ucIndex) : (STA_REC_INDEX_NOT_FOUND),
			     WLAN_MAC_MGMT_HEADER_LEN, prMgmtTxMsdu->u2FrameLength,
			     aisFsmRunEventMgmtFrameTxDone, MSDU_RATE_MODE_AUTO);
		prMgmtTxReqInfo->u8Cookie = u8Cookie;
		prMgmtTxReqInfo->prMgmtTxMsdu = prMgmtTxMsdu;
		prMgmtTxReqInfo->fgIsMgmtTxRequested = TRUE;

		nicTxConfigPktControlFlag(prMgmtTxMsdu, MSDU_CONTROL_FLAG_FORCE_TX, TRUE);

		/* send to TX queue */
		nicTxEnqueueMsdu(prAdapter, prMgmtTxMsdu);

	} while (FALSE);

	return rWlanStatus;
}				/* aisFuncTxMgmtFrame */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will validate the Rx Action Frame and indicate to uppoer layer
*           if the specified conditions were matched.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
* @param[in] prSwRfb            Pointer to SW RFB data structure.
* @param[out] pu4ControlFlags   Control flags for replying the Probe Response
*
* @retval none
*/
/*----------------------------------------------------------------------------*/
VOID aisFuncValidateRxActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
	P_AIS_FSM_INFO_T prAisFsmInfo = (P_AIS_FSM_INFO_T) NULL;

	DEBUGFUNC("aisFuncValidateRxActionFrame");

	do {

		ASSERT((prAdapter != NULL) && (prSwRfb != NULL));

		prAisFsmInfo = &(prAdapter->rWifiVar.rAisFsmInfo);

		if (1 /* prAisFsmInfo->u4AisPacketFilter & PARAM_PACKET_FILTER_ACTION_FRAME */) {
			/* Leave the action frame to wpa_supplicant. */
			kalIndicateRxMgmtFrame(prAdapter->prGlueInfo, prSwRfb);
		}

	} while (FALSE);

	return;

}				/* aisFuncValidateRxActionFrame */
