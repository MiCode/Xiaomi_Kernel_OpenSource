/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_rx.c#5 $
*/

/*! \file   nic_rx.c
    \brief  Functions that provide many rx-related functions

    This file includes the functions used to process RFB and dispatch RFBs to
    the appropriate related rx functions for protocols.
*/



/*
** $Log: nic_rx.c $
**
** 06 12 2014 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch]
**	update BLBIST dump burst mode
**
**	Review: http://mtksap20:8080/go?page=NewReview&reviewid=110351
**
** 03 11 2014 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch][MT6630][Driver]MT6630 Wi-Fi Patch
** update rssi command
**
** 09 03 2013 tsaiyuan.hsu
** [BORA00002775] MT6630 unified MAC ROAMING
** 1. modify roaming fsm.
** 2. add roaming control.
**
** 08 26 2013 eason.tsai
** [BORA00002255] [MT6630 Wi-Fi][Driver] develop
** revise host code for ICAP structure
**
** 08 23 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Reset MSDU_INFO for data packet to avoid unexpected Tx status
** 2. Drop Tx packet to non-associated STA in driver
**
** 08 20 2013 eason.tsai
** [BORA00002255] [MT6630 Wi-Fi][Driver] develop
** Icap function
**
** 08 19 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** .Adjust some debug message, refine the wlan table assign for bss
**
** 08 13 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Update driver for P2P scan & listen.
**
** 08 13 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Assign TXD.PID by wlan index
** 2. Some bug fix
**
** 08 02 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Fix BMC forwarding packet KE issue
**
** 07 31 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Fix NetDev binding issue
**
** 07 31 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** remove unnecessary reference pointer.
**
** 07 30 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** move CIPHER_MISMATCH forom Rx HiF RFB to Data process.
**
** 07 30 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** update some debug code
**
** 07 30 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** add defragmentation.
**
** 07 30 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Driver update for Hot-Spot mode.
**
** 07 30 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** MT6630 Driver Update for Hot-Spot.
**
** 07 29 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** enable rx duplicate check.
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
** Modify some security code for 11w and p2p
**
** 07 23 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Sync the latest jb2.mp 11w code as draft version
** Not the CM bit for avoid wapi 1x drop at re-key
**
** 07 22 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Handle the add key done event
**
** 07 17 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** fix and modify some security code
**
** 07 12 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** 1. fix rx groups retrival.
** 2. avoid reordering if bmc packets
**
** 07 05 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Fix to let the wpa-psk ok
**
** 07 04 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Add the function to got the STA index via the wlan index 
** report at Rx status
**
** 07 04 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** fix seq number parser for duplication detection.
**
** 07 03 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** .
**
** 07 03 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** 1. correct header offset
** 2. tentatively disable duplicate check .
**
** 07 02 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Refine security BMC wlan index assign
** Fix some compiling warning
**
** 06 18 2013 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Get MAC address by NIC_CAPABILITY command
**
** 06 18 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update for 1st connection
**
** 05 08 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** add option to workaround HIFSYS RX status 4-bytes count-in issue
**
** 03 29 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. remove unused HIF definitions
** 2. enable NDIS 5.1 build success
**
** 03 20 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** add rx duplicate check.
**
** 03 13 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** .
**
** 03 12 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** remove hif_rx_hdr usage.
**
** 03 12 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** add rx data and mangement processing.
**
** 03 07 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** use rx_status to locate packet type instead of hif_rx_header.
**
** 02 27 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Add aaa_fsm.c, p2p_ie.c, fix compile warning & error.
**
** 02 27 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Add new code, fix compile warning.
**
** 02 19 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** take use of GET_BSS_INFO_BY_INDEX() and MAX_BSS_INDEX macros
** for correctly indexing of BSS-INFO pointers
**
** 02 19 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** enable build for nic_rx.c & nic_cmd_event.c
**
** 02 06 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Fix BSS index to BSS Info MACRO
**
** 02 01 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. eliminate MT5931/MT6620/MT6628 logic
** 2. add firmware download control sequence
**
** 01 22 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** modification for ucBssIndex migration
**
** 11 01 2012 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** update to MT6630 CMD/EVENT definitions.
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
**
** 08 30 2012 yuche.tsai
** NULL
** Fix disconnect issue possible leads KE.
**
** 08 24 2012 yuche.tsai
** NULL
** Fix bug of invitation request.
 *
 * 07 17 2012 yuche.tsai
 * NULL
 * Let netdev bring up.
 *
 * 07 17 2012 yuche.tsai
 * NULL
 * Compile no error before trial run.
 *
 * 03 02 2012 terry.wu
 * NULL
 * Sync CFG80211 modification from branch 2,2.
 *
 * 02 14 2012 cp.wu
 * NULL
 * remove another assertion by error message dump
 *
 * 01 05 2012 tsaiyuan.hsu
 * [WCXRP00001157] [MT6620 Wi-Fi][FW][DRV] add timing measurement support for 802.11v
 * add timing measurement support for 802.11v.
 *
 * 11 19 2011 yuche.tsai
 * NULL
 * Update RSSI for P2P.
 *
 * 11 18 2011 yuche.tsai
 * NULL
 * CONFIG P2P support RSSI query, default turned off.
 *
 * 11 17 2011 tsaiyuan.hsu
 * [WCXRP00001115] [MT6620 Wi-Fi][DRV] avoid deactivating staRec when changing state 3 to 3.
 * avoid deactivating staRec when changing state from 3 to 3.
 *
 * 11 11 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * modify the xlog related code.
 *
 * 11 10 2011 eddie.chen
 * [WCXRP00001096] [MT6620 Wi-Fi][Driver/FW] Enhance the log function (xlog)
 * Modify the QM xlog level and remove LOG_FUNC.
 *
 * 11 09 2011 eddie.chen
 * [WCXRP00001096] [MT6620 Wi-Fi][Driver/FW] Enhance the log function (xlog)
 * Add xlog for beacon timeout and sta aging timeout.
 *
 * 11 08 2011 eddie.chen
 * [WCXRP00001096] [MT6620 Wi-Fi][Driver/FW] Enhance the log function (xlog)
 * Add xlog function.
 *
 * 11 07 2011 tsaiyuan.hsu
 * [WCXRP00001083] [MT6620 Wi-Fi][DRV]] dump debug counter or frames when debugging is triggered
 * add debug counters and periodically dump counters for debugging.
 *
 * 10 21 2011 eddie.chen
 * [WCXRP00001051] [MT6620 Wi-Fi][Driver/Fw] Adjust the STA aging timeout
 * Add switch to ignore the STA aging timeout.
 *
 * 10 12 2011 wh.su
 * [WCXRP00001036] [MT6620 Wi-Fi][Driver][FW] Adding the 802.11w code for MFP
 * adding the 802.11w related function and define .
 *
 * 08 26 2011 cp.wu
 * [WCXRP00000958] [MT6620 Wi-Fi][Driver] Extend polling timeout from 25ms to 1sec due to RF calibration might took up to 600ms
 * extend polling RX response timeout period from 25ms to 1000ms.
 *
 * 08 11 2011 cp.wu
 * [WCXRP00000830] [MT6620 Wi-Fi][Firmware] Use MDRDY counter to detect empty channel for shortening scan time
 * sparse channel detection:
 * driver: collect sparse channel information with scan-done event
 *
 * 07 28 2011 chinghwa.yu
 * [WCXRP00000063] Update BCM CoEx design and settings
 * Add BWCS cmd and event.
 *
 * 07 27 2011 cp.wu
 * [WCXRP00000876] [MT5931][Drver] Decide to retain according to currently availble RX counter and QUE_MGT used count
 * correct comment.
 *
 * 07 27 2011 cp.wu
 * [WCXRP00000876] [MT5931][Drver] Decide to retain according to currently availble RX counter and QUE_MGT used count
 * take use of QUE_MGT exported function to estimate currently RX buffer usage count.
 *
 * 07 18 2011 chinghwa.yu
 * [WCXRP00000063] Update BCM CoEx design and settings[WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Add CMD/Event for RDD and BWCS.
 *
 * 06 09 2011 tsaiyuan.hsu
 * [WCXRP00000760] [MT5931 Wi-Fi][FW] Refine rxmHandleMacRxDone to reduce code size
 * move send_auth at rxmHandleMacRxDone in firmware to driver to reduce code size.
 *
 * 05 11 2011 eddie.chen
 * [WCXRP00000709] [MT6620 Wi-Fi][Driver] Check free number before copying broadcast packet
 * Fix dest type when GO packet copying.
 *
 * 05 09 2011 eddie.chen
 * [WCXRP00000709] [MT6620 Wi-Fi][Driver] Check free number before copying broadcast packet
 * Check free number before copying broadcast packet.
 *
 * 05 05 2011 cp.wu
 * [WCXRP00000702] [MT5931][Driver] Modify initialization sequence for E1 ASIC
 * add delay after whole-chip resetting for MT5931 E1 ASIC.
 *
 * 04 18 2011 terry.wu
 * [WCXRP00000660] [MT6620 Wi-Fi][Driver] Remove flag CFG_WIFI_DIRECT_MOVED
 * Remove flag CFG_WIFI_DIRECT_MOVED.
 *
 * 04 12 2011 cm.chang
 * [WCXRP00000634] [MT6620 Wi-Fi][Driver][FW] 2nd BSS will not support 40MHz bandwidth for concurrency
 * .
 *
 * 04 08 2011 yuche.tsai
 * [WCXRP00000624] [Volunteer Patch][MT6620][Driver] Add device discoverability support for GO.
 * Add device discoverability support for GO.
 *
 * 04 01 2011 tsaiyuan.hsu
 * [WCXRP00000615] [MT 6620 Wi-Fi][Driver] Fix klocwork issues
 * fix the klocwork issues, 57500, 57501, 57502 and 57503.
 *
 * 03 19 2011 yuche.tsai
 * [WCXRP00000584] [Volunteer Patch][MT6620][Driver] Add beacon timeout support for WiFi Direct.
 * Add beacon timeout support for WiFi Direct Network.
 *
 * 03 18 2011 wh.su
 * [WCXRP00000530] [MT6620 Wi-Fi] [Driver] skip doing p2pRunEventAAAComplete after send assoc response Tx Done
 * enable the Anti_piracy check at driver .
 *
 * 03 17 2011 cp.wu
 * [WCXRP00000562] [MT6620 Wi-Fi][Driver] I/O buffer pre-allocation to avoid physically continuous memory shortage after system running for a long period
 * use pre-allocated buffer for storing enhanced interrupt response as well
 *
 * 03 15 2011 cp.wu
 * [WCXRP00000559] [MT6620 Wi-Fi][Driver] Combine TX/RX DMA buffers into a single one to reduce physically continuous memory consumption
 * 1. deprecate CFG_HANDLE_IST_IN_SDIO_CALLBACK
 * 2. Use common coalescing buffer for both TX/RX directions
 *
 *
 * 03 07 2011 wh.su
 * [WCXRP00000506] [MT6620 Wi-Fi][Driver][FW] Add Security check related code
 * rename the define to anti_pviracy.
 *
 * 03 05 2011 wh.su
 * [WCXRP00000506] [MT6620 Wi-Fi][Driver][FW] Add Security check related code
 * add the code to get the check rsponse and indicate to app.
 *
 * 03 02 2011 wh.su
 * [WCXRP00000506] [MT6620 Wi-Fi][Driver][FW] Add Security check related code
 * Add security check code.
 *
 * 03 02 2011 cp.wu
 * [WCXRP00000503] [MT6620 Wi-Fi][Driver] Take RCPI brought by association response as initial RSSI right after connection is built.
 * use RCPI brought by ASSOC-RESP after connection is built as initial RCPI to avoid using a uninitialized MAC-RX RCPI.
 *
 * 02 10 2011 yuche.tsai
 * [WCXRP00000419] [Volunteer Patch][MT6620/MT5931][Driver] Provide function of disconnect to target station for AAA module.
 * Remove Station Record after Aging timeout.
 *
 * 02 10 2011 cp.wu
 * [WCXRP00000434] [MT6620 Wi-Fi][Driver] Obsolete unused event packet handlers
 * EVENT_ID_CONNECTION_STATUS has been obsoleted and no need to handle.
 *
 * 02 09 2011 yuche.tsai
 * [WCXRP00000431] [Volunteer Patch][MT6620][Driver] Add MLME support for deauthentication under AP(Hot-Spot) mode.
 * Add MLME deauthentication support for Hot-Spot mode.
 *
 * 02 09 2011 eddie.chen
 * [WCXRP00000426] [MT6620 Wi-Fi][FW/Driver] Add STA aging timeout and defualtHwRatein AP mode
 * Adjust variable order.
 *
 * 02 08 2011 eddie.chen
 * [WCXRP00000426] [MT6620 Wi-Fi][FW/Driver] Add STA aging timeout and defualtHwRatein AP mode
 * Add event STA agint timeout
 *
 * 01 27 2011 tsaiyuan.hsu
 * [WCXRP00000392] [MT6620 Wi-Fi][Driver] Add Roaming Support
 * add roaming fsm
 * 1. not support 11r, only use strength of signal to determine roaming.
 * 2. not enable CFG_SUPPORT_ROAMING until completion of full test.
 * 3. in 6620, adopt work-around to avoid sign extension problem of cck of hw
 * 4. assume that change of link quality in smooth way.
 *
 * 01 26 2011 cm.chang
 * [WCXRP00000395] [MT6620 Wi-Fi][Driver][FW] Search STA_REC with additional net type index argument
 * .
 *
 * 01 24 2011 eddie.chen
 * [WCXRP00000385] [MT6620 Wi-Fi][DRV] Add destination decision for forwarding packets
 * Remove comments.
 *
 * 01 24 2011 eddie.chen
 * [WCXRP00000385] [MT6620 Wi-Fi][DRV] Add destination decision for forwarding packets
 * Add destination decision in AP mode.
 *
 * 01 24 2011 cm.chang
 * [WCXRP00000384] [MT6620 Wi-Fi][Driver][FW] Handle 20/40 action frame in AP mode and stop ampdu timer when sta_rec is freed
 * Process received 20/40 coexistence action frame for AP mode
 *
 * 01 24 2011 cp.wu
 * [WCXRP00000382] [MT6620 Wi-Fi][Driver] Track forwarding packet number with notifying tx thread for serving
 * 1. add an extra counter for tracking pending forward frames.
 * 2. notify TX service thread as well when there is pending forward frame
 * 3. correct build errors leaded by introduction of Wi-Fi direct separation module
 *
 * 01 12 2011 cp.wu
 * [WCXRP00000357] [MT6620 Wi-Fi][Driver][Bluetooth over Wi-Fi] add another net device interface for BT AMP
 * implementation of separate BT_OVER_WIFI data path.
 *
 * 12 29 2010 eddie.chen
 * [WCXRP00000322] Add WMM IE in beacon,
Add per station flow control when STA is in PS

 * 1) PS flow control event
 *
 * 2) WMM IE in beacon, assoc resp, probe resp
 *
 * 12 15 2010 george.huang
 * [WCXRP00000152] [MT6620 Wi-Fi] AP mode power saving function
 * update beacon for NoA
 *
 * 11 01 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000150] [MT6620 Wi-Fi][Driver] Add implementation for querying current TX rate from firmware auto rate module
 * 1) Query link speed (TX rate) from firmware directly with buffering mechanism to reduce overhead
 * 2) Remove CNM CH-RECOVER event handling
 * 3) cfg read/write API renamed with kal prefix for unified naming rules.
 *
 * 10 27 2010 george.huang
 * [WCXRP00000127] [MT6620 Wi-Fi][Driver] Add a registry to disable Beacon Timeout function for SQA test by using E1 EVB
 * Support registry option for disable beacon lost detection.
 *
 * 10 20 2010 wh.su
 * NULL
 * add a cmd to reset the p2p key
 *
 * 10 20 2010 wh.su
 * [WCXRP00000124] [MT6620 Wi-Fi] [Driver] Support the dissolve P2P Group
 * Add the code to support disconnect p2p group
 *
 * 09 29 2010 wh.su
 * [WCXRP00000072] [MT6620 Wi-Fi][Driver] Fix TKIP Counter Measure EAPoL callback register issue
 * fixed compilier error.
 *
 * 09 29 2010 wh.su
 * [WCXRP00000072] [MT6620 Wi-Fi][Driver] Fix TKIP Counter Measure EAPoL callback register issue
 * [MT6620 Wi-Fi][Driver] Fix TKIP Counter Measure EAPoL callback register issue.
 *
 * 09 23 2010 cp.wu
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * eliminate reference of CFG_RESPONSE_MAX_PKT_SIZE
 *
 * 09 21 2010 cp.wu
 * [WCXRP00000053] [MT6620 Wi-Fi][Driver] Reset incomplete and might leads to BSOD when entering RF test with AIS associated
 * release RX packet to packet pool when in RF test mode
 *
 * 09 21 2010 cp.wu
 * [WCXRP00000053] [MT6620 Wi-Fi][Driver] Reset incomplete and might leads to BSOD when entering RF test with AIS associated
 * Do a complete reset with STA-REC null checking for RF test re-entry
 *
 * 09 08 2010 cp.wu
 * NULL
 * use static memory pool for storing IEs of scanning result.
 *
 * 09 07 2010 yuche.tsai
 * NULL
 * Add a common buffer, store the IE of a P2P device in this common buffer.
 *
 * 09 03 2010 kevin.huang
 * NULL
 * Refine #include sequence and solve recursive/nested #include issue
 *
 * 08 31 2010 kevin.huang
 * NULL
 * Use LINK LIST operation to process SCAN result
 *
 * 08 30 2010 cp.wu
 * NULL
 * eliminate klockwork errors
 *
 * 08 20 2010 cm.chang
 * NULL
 * Migrate RLM code to host from FW
 *
 * 08 20 2010 yuche.tsai
 * NULL
 * When enable WiFi Direct function, check each packet to tell which interface to indicate.
 *
 * 08 05 2010 yuche.tsai
 * NULL
 * Add P2P Device Discovery Function.
 *
 * 08 03 2010 cp.wu
 * NULL
 * surpress compilation warning.
 *
 * 08 03 2010 george.huang
 * NULL
 * handle event for updating NOA parameters indicated from FW
 *
 * 08 02 2010 yuche.tsai
 * NULL
 * Add support API for RX public action frame.
 *
 * 08 02 2010 jeffrey.chang
 * NULL
 * 1) modify tx service thread to avoid busy looping
 * 2) add spin lock declartion for linux build
 *
 * 07 30 2010 cp.wu
 * NULL
 * 1) BoW wrapper: use definitions instead of hard-coded constant for error code
 * 2) AIS-FSM: eliminate use of desired RF parameters, use prTargetBssDesc instead
 * 3) add handling for RX_PKT_DESTINATION_HOST_WITH_FORWARD for GO-broadcast frames
 *
 * 07 26 2010 yuche.tsai
 *
 * Update Device Capability Bitmap & Group Capability Bitmap from 16 bits to 8 bits.
 *
 * 07 24 2010 wh.su
 *
 * .support the Wi-Fi RSN
 *
 * 07 23 2010 cp.wu
 *
 * add AIS-FSM handling for beacon timeout event.
 *
 * 07 21 2010 yuche.tsai
 *
 * Add P2P Scan & Scan Result Parsing & Saving.
 *
 * 07 19 2010 cm.chang
 *
 * Set RLM parameters and enable CNM channel manager
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
 * 07 16 2010 yarco.yang
 *
 * 1. Support BSS Absence/Presence Event
 * 2. Support STA change PS mode Event
 * 3. Support BMC forwarding for AP mode.
 *
 * 07 15 2010 cp.wu
 *
 * sync. bluetooth-over-Wi-Fi interface to driver interface document v0.2.6.
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 07 07 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * fill ucStaRecIdx into SW_RFB_T.
 *
 * 07 02 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) for event packet, no need to fill RFB.
 * 2) when wlanAdapterStart() failed, no need to initialize state machines
 * 3) after Beacon/ProbeResp parsing, corresponding BSS_DESC_T should be marked as IE-parsed
 *
 * 07 01 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * implementation of DRV-SCN and related mailbox message handling.
 *
 * 06 29 2010 yarco.yang
 * [WPD00003837][MT6620]Data Path Refine
 * replace g_rQM with Adpater->rQM
 *
 * 06 23 2010 yarco.yang
 * [WPD00003837][MT6620]Data Path Refine
 * Merge g_arStaRec[] into adapter->arStaRec[]
 *
 * 06 22 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) add command warpper for STA-REC/BSS-INFO sync.
 * 2) enhance command packet sending procedure for non-oid part
 * 3) add command packet definitions for STA-REC/BSS-INFO sync.
 *
 * 06 21 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * refine TX-DONE callback.
 *
 * 06 21 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * implement TX_DONE callback path.
 *
 * 06 21 2010 yarco.yang
 * [WPD00003837][MT6620]Data Path Refine
 * Add TX Done Event handle entry
 *
 * 06 21 2010 wh.su
 * [WPD00003840][MT6620 5931] Security migration
 * remove duplicate variable for migration.
 *
 * 06 15 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * .
 *
 * 06 15 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * .
 *
 * 06 14 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * saa_fsm.c is migrated.
 *
 * 06 14 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * add management dispatching function table.
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
 * 1) eliminate CFG_CMD_EVENT_VERSION_0_9
 * 2) when disconnected, indicate nic directly (no event is needed)
 *
 * 06 08 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * cnm_timer has been migrated.
 *
 * 06 07 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * merge wlan_def.h.
 *
 * 06 07 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * sync with MT6620 driver for scan result replacement policy
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 05 20 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) integrate OID_GEN_NETWORK_LAYER_ADDRESSES with CMD_ID_SET_IP_ADDRESS
 * 2) buffer statistics data for 2 seconds
 * 3) use default value for adhoc parameters instead of 0
 *
 * 05 19 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) do not take timeout mechanism for power mode oids
 * 2) retrieve network type from connection status
 * 3) after disassciation, set radio state to off
 * 4) TCP option over IPv6 is supported
 *
 * 04 29 2010 wh.su
 * [WPD00003816][MT6620 Wi-Fi] Adding the security support
 * fixing the PMKID candicate indicate code.
 *
 * 04 28 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * change prefix for data structure used to communicate with 802.11 PAL
 * to avoid ambiguous naming with firmware interface
 *
 * 04 27 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * basic implementation for EVENT_BT_OVER_WIFI
 *
 * 04 23 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * surpress compiler warning
 *
 * 04 22 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 *
 * 1) modify rx path code for supporting Wi-Fi direct
 * 2) modify config.h since Linux dont need to consider retaining packet
 *
 * 04 16 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * treat BUS access failure as kind of card removal.
 *
 * 04 14 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * nicRxProcessEvent packet doesn't access spin-lock directly from now on.
 *
 * 04 14 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * do not need to release the spin lock due to it is done inside nicGetPendingCmdInfo()
 *
 * 04 13 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add framework for BT-over-Wi-Fi support.
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 1) prPendingCmdInfo is replaced by queue for multiple handler capability
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 2) command sequence number is now increased atomically
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 3) private data could be hold and taken use for other purpose
 *
 * 04 12 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add channel frequency <-> number conversion
 *
 * 04 09 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * 1) add spinlock
 * 2) add KAPI for handling association info
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * rWlanInfo should be placed at adapter rather than glue due to most operations
 *  *  *  *  * are done in adapter layer.
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * eliminate direct access to prGlueInfo->eParamMediaStateIndicated from non-glue layer
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * eliminate direct access for prGlueInfo->fgIsCardRemoved in non-glue layer
 *
 * 04 01 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * improve Linux supplicant compliance
 *
 * 03 31 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * fix ioctl which may cause cmdinfo memory leak
 *
 * 03 30 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * remove driver-land statistics.
 *
 * 03 29 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * improve none-glue code portability
 *
 * 03 28 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * rWlanInfo is modified before data is indicated to OS
 *
 * 03 28 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * rWlanInfo is modified before data is indicated to OS
 *
 * 03 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add a temporary flag for integration with CMD/EVENT v0.9.
 *
 * 03 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) correct OID_802_11_CONFIGURATION with frequency setting behavior.
 *  *  * the frequency is used for adhoc connection only
 *  *  * 2) update with SD1 v0.9 CMD/EVENT documentation
 *
 * 03 24 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * initial import for Linux port
 *
 * 03 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * .
 *
 * 03 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * generate information for OID_GEN_RCV_OK & OID_GEN_XMIT_OK
 *  *  *  *
 *
 * 03 19 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add ACPI D0/D3 state switching support
 *  *  *  *  *  *  *  *  * 2) use more formal way to handle interrupt when the status is retrieved from enhanced RX response
 *
 * 03 15 2010 kevin.huang
 * [WPD00003820][MT6620 Wi-Fi] Modify the code for meet the WHQL test
 * Add event for activate STA_RECORD_T
 *
 * 03 12 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * correct fgSetQuery/fgNeedResp check
 *
 * 03 11 2010 cp.wu
 * [WPD00003821][BUG] Host driver stops processing RX packets from HIF RX0
 * add RX starvation warning debug message controlled by CFG_HIF_RX_STARVATION_WARNING
 *
 * 03 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code clean: removing unused variables and structure definitions
 *
 * 03 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add another spin-lock to protect MsduInfoList due to it might be accessed by different thread.
 *  *  * 2) change own-back acquiring procedure to wait for up to 16.67 seconds
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) the use of prPendingOid revised, all accessing are now protected by spin lock
 *  *  *  * 2) ensure wlanReleasePendingOid will clear all command queues
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add mutex to avoid multiple access to qmTxQueue simultaneously.
 *
 * 02 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * move EVENT_ID_ASSOC_INFO from nic_rx.c to gl_kal_ndis_51.c
 *  * 'cause it involves OS dependent data structure handling
 *
 * 02 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * correct behavior to prevent duplicated RX handling for RX0_DONE and RX1_DONE
 *
 * 02 24 2010 tehuang.liu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * Updated API interfaces for qmHandleEventRxAddBa() and qmHandleEventRxDelBa()
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement host-side firmware download logic
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove unused function in nic_rx.c [which has been handled in que_mgt.c]
 *  *  *  *  * 2) firmware image length is now retrieved via NdisFileOpen
 *  *  *  *  * 3) firmware image is not structured by (P_IMG_SEC_HDR_T) anymore
 *  *  *  *  * 4) nicRxWaitResponse() revised
 *  *  *  *  * 5) another set of TQ counter default value is added for fw-download state
 *  *  *  *  * 6) Wi-Fi load address is now retrieved from registry too
 *
 * 02 09 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. Permanent and current MAC address are now retrieved by CMD/EVENT packets instead of hard-coded address
 *  *  *  *  *  *  *  * 2. follow MSDN defined behavior when associates to another AP
 *  *  *  *  *  *  *  * 3. for firmware download, packet size could be up to 2048 bytes
 *
 * 01 27 2010 wh.su
 * [WPD00003816][MT6620 Wi-Fi] Adding the security support
 * .
 *
 * 01 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement following 802.11 OIDs:
 *  *  *  *  *  * OID_802_11_RSSI,
 *  *  *  *  *  * OID_802_11_RSSI_TRIGGER,
 *  *  *  *  *  * OID_802_11_STATISTICS,
 *  *  *  *  *  * OID_802_11_DISASSOCIATE,
 *  *  *  *  *  * OID_802_11_POWER_MODE
 *
 * 12 30 2009 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) According to CMD/EVENT documentation v0.8,
 *  *  *  *  *  *  *  *  * OID_CUSTOM_TEST_RX_STATUS & OID_CUSTOM_TEST_TX_STATUS is no longer used,
 *  *  *  *  *  *  *  *  * and result is retrieved by get ATInfo instead
 *  *  *  *  *  *  *  *  * 2) add 4 counter for recording aggregation statistics
 *
 * 12 23 2009 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add a precheck: if free sw rfb is not enough, do not invoke read transactionu1rwduu`wvpghlqg|fu+rp
 *
 * 12 22 2009 cp.wu
 * [WPD00003809][Bug] Host driver will crash when processing reordered MSDUs
 * The root cause is pointer accessing by mistake. After dequeued from reordering-buffer, handling logic should access returned pointer instead of pointer which has been passed in before.
**  \main\maintrunk.MT6620WiFiDriver_Prj\58 2009-12-17 13:40:33 GMT mtk02752
**  always update prAdapter->rSDIOCtrl when enhanced response is read by RX
**  \main\maintrunk.MT6620WiFiDriver_Prj\57 2009-12-16 18:01:38 GMT mtk02752
**  if interrupt enhanced response is fetched by RX enhanced response, RX needs to invoke interrupt handlers too
**  \main\maintrunk.MT6620WiFiDriver_Prj\56 2009-12-16 14:16:52 GMT mtk02752
**  \main\maintrunk.MT6620WiFiDriver_Prj\55 2009-12-15 20:03:12 GMT mtk02752
**  ASSERT when RX FreeSwRfb is not enough
**  \main\maintrunk.MT6620WiFiDriver_Prj\54 2009-12-15 17:01:29 GMT mtk02752
**  when CFG_SDIO_RX_ENHANCE is enabled, after enhanced response is read, rx procedure should process 1) TX_DONE_INT 2) D2H INT as well
**  \main\maintrunk.MT6620WiFiDriver_Prj\53 2009-12-14 20:45:28 GMT mtk02752
**  when CFG_SDIO_RX_ENHANCE is set, TC counter must be updated each time RX enhance response is read
**
**  \main\maintrunk.MT6620WiFiDriver_Prj\52 2009-12-14 11:34:16 GMT mtk02752
**  correct a trivial logic issue
**  \main\maintrunk.MT6620WiFiDriver_Prj\51 2009-12-14 10:28:25 GMT mtk02752
**  add a protection to avoid out-of-boundary access
**  \main\maintrunk.MT6620WiFiDriver_Prj\50 2009-12-10 16:55:18 GMT mtk02752
**  code clean
**  \main\maintrunk.MT6620WiFiDriver_Prj\49 2009-12-09 14:06:47 GMT MTK02468
**  Added parsing event packets with EVENT_ID_RX_ADDBA or EVENT_ID_RX_DELBA
**  \main\maintrunk.MT6620WiFiDriver_Prj\48 2009-12-08 17:37:51 GMT mtk02752
**  handle EVENT_ID_TEST_STATUS as well
**  \main\maintrunk.MT6620WiFiDriver_Prj\47 2009-12-04 17:59:11 GMT mtk02752
**  to pass free-build compilation check
**  \main\maintrunk.MT6620WiFiDriver_Prj\46 2009-12-04 12:09:52 GMT mtk02752
**  correct trivial mistake
**  \main\maintrunk.MT6620WiFiDriver_Prj\45 2009-12-04 11:53:37 GMT mtk02752
**  all API should be compilable under SD1_SD3_DATAPATH_INTEGRATION == 0
**  \main\maintrunk.MT6620WiFiDriver_Prj\44 2009-12-03 16:19:48 GMT mtk01461
**  Fix the Connected Event
**  \main\maintrunk.MT6620WiFiDriver_Prj\43 2009-11-30 10:56:18 GMT mtk02752
**  1st DW of WIFI_EVENT_T is shared with HIF_RX_HEADER_T
**  \main\maintrunk.MT6620WiFiDriver_Prj\42 2009-11-30 10:11:27 GMT mtk02752
**  implement replacement for bss scan result
**  \main\maintrunk.MT6620WiFiDriver_Prj\41 2009-11-27 11:08:05 GMT mtk02752
**  add flush for reset
**  \main\maintrunk.MT6620WiFiDriver_Prj\40 2009-11-26 09:38:59 GMT mtk02752
**  \main\maintrunk.MT6620WiFiDriver_Prj\39 2009-11-26 09:29:40 GMT mtk02752
**  enable packet forwarding path (for AP mode)
**  \main\maintrunk.MT6620WiFiDriver_Prj\38 2009-11-25 21:37:00 GMT mtk02752
**  sync. with EVENT_SCAN_RESULT_T change, and add an assert for checking event size
**  \main\maintrunk.MT6620WiFiDriver_Prj\37 2009-11-25 20:17:41 GMT mtk02752
**  fill HIF_TX_HEADER_T.u2SeqNo
**  \main\maintrunk.MT6620WiFiDriver_Prj\36 2009-11-25 18:18:57 GMT mtk02752
**  buffer scan result to prGlueInfo->rWlanInfo.arScanResult directly.
**  \main\maintrunk.MT6620WiFiDriver_Prj\35 2009-11-24 22:42:45 GMT mtk02752
**  add nicRxAddScanResult() to prepare to handle SCAN_RESULT event (not implemented yet)
**  \main\maintrunk.MT6620WiFiDriver_Prj\34 2009-11-24 20:51:41 GMT mtk02752
**  integrate with SD1's data path API
**  \main\maintrunk.MT6620WiFiDriver_Prj\33 2009-11-24 19:56:17 GMT mtk02752
**  adopt P_HIF_RX_HEADER_T in new path
**  \main\maintrunk.MT6620WiFiDriver_Prj\32 2009-11-23 20:31:21 GMT mtk02752
**  payload to send into pfCmdDoneHandler() will not include WIFI_EVENT_T
**  \main\maintrunk.MT6620WiFiDriver_Prj\31 2009-11-23 17:51:34 GMT mtk02752
**  when event packet corresponding to some pendingOID is received, pendingOID should be cleared
**  \main\maintrunk.MT6620WiFiDriver_Prj\30 2009-11-23 14:46:54 GMT mtk02752
**  implement nicRxProcessEventPacket()
**  \main\maintrunk.MT6620WiFiDriver_Prj\29 2009-11-17 22:40:54 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\28 2009-11-16 21:48:22 GMT mtk02752
**  add SD1_SD3_DATAPATH_INTEGRATION data path handling
**  \main\maintrunk.MT6620WiFiDriver_Prj\27 2009-11-16 15:41:18 GMT mtk01084
**  modify the length to be read in emu mode
**  \main\maintrunk.MT6620WiFiDriver_Prj\26 2009-11-13 17:00:12 GMT mtk02752
**  add blank function for event packet
**  \main\maintrunk.MT6620WiFiDriver_Prj\25 2009-11-13 13:54:24 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\24 2009-11-11 14:41:51 GMT mtk02752
**  fix typo
**  \main\maintrunk.MT6620WiFiDriver_Prj\23 2009-11-11 14:33:46 GMT mtk02752
**  add protection when there is no packet avilable
**  \main\maintrunk.MT6620WiFiDriver_Prj\22 2009-11-11 12:33:36 GMT mtk02752
**  add RX1 read path for aggregated/enhanced/normal packet read procedures
**  \main\maintrunk.MT6620WiFiDriver_Prj\21 2009-11-11 10:36:18 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\20 2009-11-04 14:11:08 GMT mtk01084
**  modify lines in RX aggregation
**  \main\maintrunk.MT6620WiFiDriver_Prj\19 2009-10-30 18:17:23 GMT mtk01084
**  modify RX aggregation handling
**  \main\maintrunk.MT6620WiFiDriver_Prj\18 2009-10-29 19:56:12 GMT mtk01084
**  modify HAL part
**  \main\maintrunk.MT6620WiFiDriver_Prj\17 2009-10-23 16:08:34 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\16 2009-10-13 21:59:20 GMT mtk01084
**  update for new HW design
**  \main\maintrunk.MT6620WiFiDriver_Prj\15 2009-10-02 13:59:08 GMT mtk01725
**  \main\maintrunk.MT6620WiFiDriver_Prj\14 2009-05-21 23:39:05 GMT mtk01461
**  Fix the paste error of RX STATUS in OOB of HIF Loopback CTRL
**  \main\maintrunk.MT6620WiFiDriver_Prj\13 2009-05-20 12:25:32 GMT mtk01461
**  Fix process of Read Done, and add u4MaxEventBufferLen to nicRxWaitResponse()
**  \main\maintrunk.MT6620WiFiDriver_Prj\12 2009-05-18 21:13:18 GMT mtk01426
**  Fixed compiler error
**  \main\maintrunk.MT6620WiFiDriver_Prj\11 2009-05-18 21:05:29 GMT mtk01426
**  Fixed nicRxSDIOAggReceiveRFBs() ASSERT issue
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-04-28 10:38:43 GMT mtk01461
**  Fix RX STATUS is DW align for SDIO_STATUS_ENHANCE mode and refine  nicRxSDIOAggeceiveRFBs() for RX Aggregation
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-04-22 09:12:17 GMT mtk01461
**  Fix nicRxProcessHIFLoopbackPacket(), the size of HIF CTRL LENTH field is 1 byte
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-04-14 15:51:26 GMT mtk01426
**  Update RX OOB Setting
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-04-03 14:58:58 GMT mtk01426
**  Fixed logical error
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-04-01 10:58:31 GMT mtk01461
**  Rename the HIF_PKT_TYPE_DATA
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-03-23 21:51:18 GMT mtk01461
**  Fix u4HeaderOffset in nicRxProcessHIFLoopbackPacket()
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-18 21:02:58 GMT mtk01426
**  Add CFG_SDIO_RX_ENHANCE and CFG_HIF_LOOPBACK support
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-17 20:20:59 GMT mtk01426
**  Add nicRxWaitResponse function
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:26:01 GMT mtk01426
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
#include "precomp.h"
#include "que_mgt.h"

#ifndef LINUX
#include <limits.h>
#else
#include <linux/limits.h>
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define RX_RESPONSE_TIMEOUT (1000)

#if CFG_SUPPORT_SNIFFER
/* in unit of 100kb/s */
const EMU_MAC_RATE_INFO_T arMcsRate2PhyRate[] = 
{
    //Phy Rate Code,           BW20,  BW20 SGI, BW40, BW40 SGI, BW80, BW80 SGI, BW160, BW160 SGI 
    RATE_INFO(PHY_RATE_MCS0 ,   65,     72,     135,    150,    293,    325,    585,    650),
    RATE_INFO(PHY_RATE_MCS1 ,   130,    144,    270,    300,    585,    650,    1170,   1300),
    RATE_INFO(PHY_RATE_MCS2 ,   195,    217,    405,    450,    878,    975,    1755,   1950),
    RATE_INFO(PHY_RATE_MCS3 ,   260,    289,    540,    600,    1170,   1300,   2340,   2600),
    RATE_INFO(PHY_RATE_MCS4 ,   390,    433,    810,    900,    1755,   1950,   3510,   3900),
    RATE_INFO(PHY_RATE_MCS5 ,   520,    578,    1080,   1200,   2340,   2600,   4680,   5200),
    RATE_INFO(PHY_RATE_MCS6 ,   585,    650,    1215,   1350,   2633,   2925,   5265,   5850),
    RATE_INFO(PHY_RATE_MCS7 ,   650,    722,    1350,   1500,   2925,   3250,   5850,   6500),
    RATE_INFO(PHY_RATE_MCS8 ,   780,    867,    1620,   1800,   3510,   3900,   7020,   7800),
    RATE_INFO(PHY_RATE_MCS9 ,   0,      0,      1800,   2000,   3900,   4333,   7800,   8667),
    RATE_INFO(PHY_RATE_MCS32 ,  0,      0,      60,     67,      0,      0,      0,      0)
};

/* in uint of 500kb/s */
const UINT_8 aucHwRate2PhyRate[] = {
    RATE_1M,    /*1M long*/
    RATE_2M,    /*2M long*/
    RATE_5_5M,  /*5.5M long*/
    RATE_11M,   /*11M long*/
    RATE_1M,    /*1M short invalid*/
    RATE_2M,    /*2M short*/
    RATE_5_5M,  /*5.5M short*/
    RATE_11M,   /*11M short*/
    RATE_48M,   /*48M*/
    RATE_24M,   /*24M*/
    RATE_12M,   /*12M*/
    RATE_6M,    /*6M*/
    RATE_54M,   /*54M*/
    RATE_36M,   /*36M*/
    RATE_18M,   /*18M*/
    RATE_9M     /*9M*/
};
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

#if CFG_MGMT_FRAME_HANDLING
static PROCESS_RX_MGT_FUNCTION apfnProcessRxMgtFrame[MAX_NUM_OF_FC_SUBTYPES] = {
#if CFG_SUPPORT_AAA
    aaaFsmRunEventRxAssoc,              /* subtype 0000: Association request */
#else
    NULL,                               /* subtype 0000: Association request */
#endif				/* CFG_SUPPORT_AAA */
    saaFsmRunEventRxAssoc,              /* subtype 0001: Association response */
#if CFG_SUPPORT_AAA
    aaaFsmRunEventRxAssoc,              /* subtype 0010: Reassociation request */
#else
    NULL,                               /* subtype 0010: Reassociation request */
#endif				/* CFG_SUPPORT_AAA */
    saaFsmRunEventRxAssoc,              /* subtype 0011: Reassociation response */
#if CFG_SUPPORT_ADHOC || CFG_ENABLE_WIFI_DIRECT
    bssProcessProbeRequest,             /* subtype 0100: Probe request */
#else
    NULL,                               /* subtype 0100: Probe request */
#endif				/* CFG_SUPPORT_ADHOC */
    scanProcessBeaconAndProbeResp,      /* subtype 0101: Probe response */
    NULL,                               /* subtype 0110: reserved */
    NULL,                               /* subtype 0111: reserved */
    scanProcessBeaconAndProbeResp,      /* subtype 1000: Beacon */
    NULL,                               /* subtype 1001: ATIM */
    saaFsmRunEventRxDisassoc,           /* subtype 1010: Disassociation */
    authCheckRxAuthFrameTransSeq,       /* subtype 1011: Authentication */
    saaFsmRunEventRxDeauth,             /* subtype 1100: Deauthentication */
    nicRxProcessActionFrame,            /* subtype 1101: Action */
    NULL,                               /* subtype 1110: reserved */
    NULL                                /* subtype 1111: reserved */
};
#endif


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
* @brief Initialize the RFBs
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxInitialize(IN P_ADAPTER_T prAdapter)
{
    P_RX_CTRL_T prRxCtrl;
    PUINT_8 pucMemHandle;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
    UINT_32 i;

    DEBUGFUNC("nicRxInitialize");

    ASSERT(prAdapter);
    prRxCtrl = &prAdapter->rRxCtrl;

	/* 4 <0> Clear allocated memory. */
    kalMemZero((PVOID) prRxCtrl->pucRxCached, prRxCtrl->u4RxCachedSize);

	/* 4 <1> Initialize the RFB lists */
    QUEUE_INITIALIZE(&prRxCtrl->rFreeSwRfbList);
    QUEUE_INITIALIZE(&prRxCtrl->rReceivedRfbList);
    QUEUE_INITIALIZE(&prRxCtrl->rIndicatedRfbList);

    pucMemHandle = prRxCtrl->pucRxCached;
    for (i = CFG_RX_MAX_PKT_NUM; i != 0; i--) {
		prSwRfb = (P_SW_RFB_T) pucMemHandle;

        nicRxSetupRFB(prAdapter, prSwRfb);
        nicRxReturnRFB(prAdapter, prSwRfb);

        pucMemHandle += ALIGN_4(sizeof(SW_RFB_T));
    }

    ASSERT(prRxCtrl->rFreeSwRfbList.u4NumElem == CFG_RX_MAX_PKT_NUM);
    /* Check if the memory allocation consist with this initialization function */
	ASSERT((UINT_32) (pucMemHandle - prRxCtrl->pucRxCached) == prRxCtrl->u4RxCachedSize);

	/* 4 <2> Clear all RX counters */
    RX_RESET_ALL_CNTS(prRxCtrl);

#if CFG_SDIO_RX_AGG
    prRxCtrl->pucRxCoalescingBufPtr = prAdapter->pucCoalescingBufCached;
    HAL_CFG_MAX_HIF_RX_LEN_NUM(prAdapter, CFG_SDIO_MAX_RX_AGG_NUM);
#else
    HAL_CFG_MAX_HIF_RX_LEN_NUM(prAdapter, 1);
#endif

#if CFG_HIF_STATISTICS
    prRxCtrl->u4TotalRxAccessNum = 0;
    prRxCtrl->u4TotalRxPacketNum = 0;
#endif

#if CFG_HIF_RX_STARVATION_WARNING
    prRxCtrl->u4QueuedCnt = 0;
    prRxCtrl->u4DequeuedCnt = 0;
#endif

    return;
} /* end of nicRxInitialize() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Uninitialize the RFBs
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxUninitialize(IN P_ADAPTER_T prAdapter)
{
    P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

    nicRxFlush(prAdapter);

    do {
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
        QUEUE_REMOVE_HEAD(&prRxCtrl->rReceivedRfbList, prSwRfb, P_SW_RFB_T);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
		if (prSwRfb) {
            if (prSwRfb->pvPacket) {
                kalPacketFree(prAdapter->prGlueInfo, prSwRfb->pvPacket);
            }
            prSwRfb->pvPacket = NULL;
		} else {
            break;
        }
	} while (TRUE);

    do {
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
        QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		if (prSwRfb) {
            if (prSwRfb->pvPacket) {
                kalPacketFree(prAdapter->prGlueInfo, prSwRfb->pvPacket);
            }
            prSwRfb->pvPacket = NULL;
		} else {
            break;
        }
	} while (TRUE);

    return;
} /* end of nicRxUninitialize() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Fill RFB
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb   specify the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxFillRFB(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
    P_HW_MAC_RX_DESC_T prRxStatus;

    UINT_32 u4PktLen = 0;
	/* UINT_32 u4MacHeaderLen; */
    UINT_32 u4HeaderOffset;
    UINT_16 u2RxStatusOffset;

    DEBUGFUNC("nicRxFillRFB");

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    prRxStatus = prSwRfb->prRxStatus;
    ASSERT(prRxStatus);

	u4PktLen = (UINT_32) HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus);
	u4HeaderOffset = (UINT_32) (HAL_RX_STATUS_GET_HEADER_OFFSET(prRxStatus));
	/* u4MacHeaderLen = (UINT_32)(HAL_RX_STATUS_GET_HEADER_LEN(prRxStatus)); */

	/* DBGLOG(RX, TRACE, ("u4HeaderOffset = %d, u4MacHeaderLen = %d\n", */
	/* u4HeaderOffset, u4MacHeaderLen)); */
    u2RxStatusOffset = sizeof(HW_MAC_RX_DESC_T);
	prSwRfb->ucGroupVLD = (UINT_8) HAL_RX_STATUS_GET_GROUP_VLD(prRxStatus);
    if (prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_4)) {
		prSwRfb->prRxStatusGroup4 =
		    (P_HW_MAC_RX_STS_GROUP_4_T) ((P_UINT_8) prRxStatus + u2RxStatusOffset);
        u2RxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_4_T);
    }
	
    if (prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_1)) {
		prSwRfb->prRxStatusGroup1 =
		    (P_HW_MAC_RX_STS_GROUP_1_T) ((P_UINT_8) prRxStatus + u2RxStatusOffset);
        u2RxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_1_T);
    }
	
    if (prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_2)) {
		prSwRfb->prRxStatusGroup2 =
		    (P_HW_MAC_RX_STS_GROUP_2_T) ((P_UINT_8) prRxStatus + u2RxStatusOffset);
        u2RxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_2_T);
    }
	
    if (prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_3)) {
		prSwRfb->prRxStatusGroup3 =
		    (P_HW_MAC_RX_STS_GROUP_3_T) ((P_UINT_8) prRxStatus + u2RxStatusOffset);
        u2RxStatusOffset += sizeof(HW_MAC_RX_STS_GROUP_3_T);
    }
	
    prSwRfb->u2RxStatusOffst = u2RxStatusOffset;
	prSwRfb->pvHeader = (PUINT_8) prRxStatus + u2RxStatusOffset + u4HeaderOffset;
	prSwRfb->u2PacketLen = (UINT_16) (u4PktLen - (u2RxStatusOffset + u4HeaderOffset));
	prSwRfb->u2HeaderLen = (UINT_16) HAL_RX_STATUS_GET_HEADER_LEN(prRxStatus);
	prSwRfb->ucWlanIdx = (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prRxStatus);
	prSwRfb->ucStaRecIdx =
	    secGetStaIdxByWlanIdx(prAdapter, (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prRxStatus));
    prSwRfb->prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	/* DBGLOG(RX, TRACE, ("Dump Rx packet, u2PacketLen = %d\n", prSwRfb->u2PacketLen)); */
	/* DBGLOG_MEM8(RX, TRACE, prSwRfb->pvHeader, prSwRfb->u2PacketLen); */

#if 0
	if (prHifRxHdr->ucReorder & HIF_RX_HDR_80211_HEADER_FORMAT) {
        prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_802_11_FORMAT;
        DBGLOG(RX, TRACE, ("HIF_RX_HDR_FLAG_802_11_FORMAT\n"));
    }

	if (prHifRxHdr->ucReorder & HIF_RX_HDR_DO_REORDER) {
        prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_DO_REORDERING;
        DBGLOG(RX, TRACE, ("HIF_RX_HDR_FLAG_DO_REORDERING\n"));

        /* Get Seq. No and TID, Wlan Index info */
		if (prHifRxHdr->u2SeqNoTid & HIF_RX_HDR_BAR_FRAME) {
            prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_BAR_FRAME;
            DBGLOG(RX, TRACE, ("HIF_RX_HDR_FLAG_BAR_FRAME\n"));
        }

        prSwRfb->u2SSN = prHifRxHdr->u2SeqNoTid & HIF_RX_HDR_SEQ_NO_MASK;
		prSwRfb->ucTid = (UINT_8) ((prHifRxHdr->u2SeqNoTid & HIF_RX_HDR_TID_MASK)
                        >> HIF_RX_HDR_TID_OFFSET);
		DBGLOG(RX, TRACE, ("u2SSN = %d, ucTid = %d\n", prSwRfb->u2SSN, prSwRfb->ucTid));
    }

	if (prHifRxHdr->ucReorder & HIF_RX_HDR_WDS) {
        prSwRfb->u4HifRxHdrFlag |= HIF_RX_HDR_FLAG_AMP_WDS;
        DBGLOG(RX, TRACE, ("HIF_RX_HDR_FLAG_AMP_WDS\n"));
    }
#endif
}


#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
/*----------------------------------------------------------------------------*/
/*!
* @brief Fill checksum status in RFB
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
* @param u4TcpUdpIpCksStatus specify the Checksum status
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID
nicRxFillChksumStatus(IN P_ADAPTER_T prAdapter,
		      IN OUT P_SW_RFB_T prSwRfb, IN UINT_32 u4TcpUdpIpCksStatus)
{

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

	if (prAdapter->u4CSUMFlags != CSUM_NOT_SUPPORTED) {
		if (u4TcpUdpIpCksStatus & RX_CS_TYPE_IPv4) {	/* IPv4 packet */
            prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_NONE;
			if (u4TcpUdpIpCksStatus & RX_CS_STATUS_IP) {	/* IP packet csum failed */
                prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_FAILED;
            } else {
                prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_SUCCESS;
            }

			if (u4TcpUdpIpCksStatus & RX_CS_TYPE_TCP) {	/* TCP packet */
                prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_TCP) {	/* TCP packet csum failed */
                    prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_FAILED;
                } else {
                    prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_SUCCESS;
                }
			} else if (u4TcpUdpIpCksStatus & RX_CS_TYPE_UDP) {	/* UDP packet */
                prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_UDP) {	/* UDP packet csum failed */
                    prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_FAILED;
                } else {
                    prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_SUCCESS;
                }
			} else {
                prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
                prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
            }
		} else if (u4TcpUdpIpCksStatus & RX_CS_TYPE_IPv6) {	/* IPv6 packet */
            prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_NONE;
            prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_SUCCESS;

			if (u4TcpUdpIpCksStatus & RX_CS_TYPE_TCP) {	/* TCP packet */
                prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_TCP) {	/* TCP packet csum failed */
                    prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_FAILED;
                } else {
                    prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_SUCCESS;
                }
			} else if (u4TcpUdpIpCksStatus & RX_CS_TYPE_UDP) {	/* UDP packet */
                prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
				if (u4TcpUdpIpCksStatus & RX_CS_STATUS_UDP) {	/* UDP packet csum failed */
                    prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_FAILED;
                } else {
                    prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_SUCCESS;
                }
			} else {
                prSwRfb->aeCSUM[CSUM_TYPE_UDP] = CSUM_RES_NONE;
                prSwRfb->aeCSUM[CSUM_TYPE_TCP] = CSUM_RES_NONE;
            }
		} else {
            prSwRfb->aeCSUM[CSUM_TYPE_IPV4] = CSUM_RES_NONE;
            prSwRfb->aeCSUM[CSUM_TYPE_IPV6] = CSUM_RES_NONE;
        }
    }

}
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */


/*----------------------------------------------------------------------------*/
/*!
* \brief rxDefragMPDU() is used to defragment the incoming packets.
*
* \param[in] prSWRfb        The RFB which is being processed.
* \param[in] UINT_16     u2FrameCtrl
*
* \retval NOT NULL  Receive the last fragment data
* \retval NULL      Receive the fragment packet which is not the last
*/
/*----------------------------------------------------------------------------*/
P_SW_RFB_T
incRxDefragMPDU(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSWRfb, OUT P_QUE_T prReturnedQue)
{

	P_SW_RFB_T prOutputSwRfb = (P_SW_RFB_T) NULL;
#if 1
    P_RX_CTRL_T prRxCtrl;
    P_FRAG_INFO_T prFragInfo;
    UINT_32 i = 0, j;
    UINT_16 u2SeqCtrl, u2FrameCtrl;
    UINT_8 ucFragNum;
    BOOLEAN fgFirst = FALSE;
    BOOLEAN fgLast = FALSE;
    OS_SYSTIME rCurrentTime;
    P_WLAN_MAC_HEADER_T prWlanHeader = NULL;
    P_HW_MAC_RX_DESC_T prRxStatus = NULL;
    P_HW_MAC_RX_STS_GROUP_4_T prRxStatusGroup4 = NULL;

    DEBUGFUNC("nicRx: rxmDefragMPDU\n");

    ASSERT(prSWRfb);

    prRxCtrl = &prAdapter->rRxCtrl;

    prRxStatus = prSWRfb->prRxStatus;
    ASSERT(prRxStatus);

    if (HAL_RX_STATUS_IS_HEADER_TRAN(prRxStatus) == FALSE) {
		prWlanHeader = (P_WLAN_MAC_HEADER_T) prSWRfb->pvHeader;
        prSWRfb->u2SequenceControl = prWlanHeader->u2SeqCtrl;
        u2FrameCtrl = prWlanHeader->u2FrameCtrl;
	} else {
        prRxStatusGroup4 = prSWRfb->prRxStatusGroup4;
        prSWRfb->u2SequenceControl = HAL_RX_STATUS_GET_SEQFrag_NUM(prRxStatusGroup4);
        u2FrameCtrl = HAL_RX_STATUS_GET_FRAME_CTL_FIELD(prRxStatusGroup4);
    }
    u2SeqCtrl = prSWRfb->u2SequenceControl;
	ucFragNum = (UINT_8) (u2SeqCtrl & MASK_SC_FRAG_NUM);
    prSWRfb->u2FrameCtrl = u2FrameCtrl; 

    if (!(u2FrameCtrl & MASK_FC_MORE_FRAG)) {
        /* The last fragment frame */
        if (ucFragNum) {
			DBGLOG(RX, LOUD,
			       ("FC %04x M %04x SQ %04x\n", u2FrameCtrl,
				(u2FrameCtrl & MASK_FC_MORE_FRAG), u2SeqCtrl));
            fgLast = TRUE;
        }
        /* Non-fragment frame */
        else {
            return prSWRfb;
        }
    }
    /* The fragment frame except the last one */
    else {
        if (ucFragNum == 0) {
			DBGLOG(RX, LOUD,
			       ("FC %04x M %04x SQ %04x\n", u2FrameCtrl,
				(u2FrameCtrl & MASK_FC_MORE_FRAG), u2SeqCtrl));
            fgFirst = TRUE;
		} else {
			DBGLOG(RX, LOUD,
			       ("FC %04x M %04x SQ %04x\n", u2FrameCtrl,
				(u2FrameCtrl & MASK_FC_MORE_FRAG), u2SeqCtrl));
        }
    }

    GET_CURRENT_SYSTIME(&rCurrentTime);

    for (j = 0; j < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; j++) {
         prFragInfo = &prSWRfb->prStaRec->rFragInfo[j];
         if (prFragInfo->pr1stFrag) {
         /* I. If the receive timer for the MSDU or MMPDU that is stored in the
                 * fragments queue exceeds dot11MaxReceiveLifetime, we discard the
                 * uncompleted fragments.
                 * II. If we didn't receive the last MPDU for a period, we use
                 * this function for remove frames.
               */
            if (CHECK_FOR_EXPIRATION(rCurrentTime, prFragInfo->rReceiveLifetimeLimit)) {

				/* cnmPktFree((P_PKT_INFO_T)prFragInfo->pr1stFrag, TRUE); */
                prFragInfo->pr1stFrag->eDst = RX_PKT_DESTINATION_NULL;
				QUEUE_INSERT_TAIL(prReturnedQue,
						  (P_QUE_ENTRY_T) prFragInfo->pr1stFrag);

				prFragInfo->pr1stFrag = (P_SW_RFB_T) NULL;
            }
        }
    }


    for (i = 0; i < MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS; i++) {

        prFragInfo = &prSWRfb->prStaRec->rFragInfo[i];

		if (fgFirst) {	/* looking for timed-out frag buffer */

			if (prFragInfo->pr1stFrag == (P_SW_RFB_T) NULL) {	/* find a free frag buffer */
                break;
            }
		} else {	/* looking for a buffer with desired next seqctrl */

			if (prFragInfo->pr1stFrag == (P_SW_RFB_T) NULL) {
                continue;
            }

            if (RXM_IS_QOS_DATA_FRAME(u2FrameCtrl)) {
                if (RXM_IS_QOS_DATA_FRAME(prFragInfo->pr1stFrag->u2FrameCtrl)) {
                    if (u2SeqCtrl == prFragInfo->u2NextFragSeqCtrl) {
                        break;
                    }
                }
			} else {
                if (!RXM_IS_QOS_DATA_FRAME(prFragInfo->pr1stFrag->u2FrameCtrl)) {
                    if (u2SeqCtrl == prFragInfo->u2NextFragSeqCtrl) {
                        break;
                    }
                }
            }
        }
    }

    if (i >= MAX_NUM_CONCURRENT_FRAGMENTED_MSDUS) {

        /* Can't find a proper FRAG_INFO_T.
         * I. 1st Fragment MPDU, all of the FragInfo are exhausted
         * II. 2nd ~ (n-1)th Fragment MPDU, can't find the right FragInfo for defragment.
         * Because we won't process fragment frame outside this function, so
         * we should free it right away.
         */
        nicRxReturnRFB(prAdapter, prSWRfb);

		return (P_SW_RFB_T) NULL;
    }

    ASSERT(prFragInfo);

	/* retrive Rx payload */
    prSWRfb->u2HeaderLen = HAL_RX_STATUS_GET_HEADER_LEN(prRxStatus);
	prSWRfb->pucPayload =
	    (PUINT_8) (((ULONG)prSWRfb->pvHeader) + prSWRfb->u2HeaderLen);
	prSWRfb->u2PayloadLength =
	    (UINT_16) (HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus) -
		       ((ULONG) prSWRfb->pucPayload - (ULONG) prRxStatus));

    if (fgFirst) {
        DBGLOG(RX, LOUD, ("rxDefragMPDU first\n"));

        SET_EXPIRATION_TIME(prFragInfo->rReceiveLifetimeLimit,
                            TU_TO_SYSTIME(DOT11_RECEIVE_LIFETIME_TU_DEFAULT));

        prFragInfo->pr1stFrag = prSWRfb;
        
        prFragInfo->pucNextFragStart =
            (PUINT_8) prSWRfb->pucRecvBuff + HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus);

        prFragInfo->u2NextFragSeqCtrl = u2SeqCtrl + 1;
        DBGLOG(RX, LOUD, ("First: nextFragmentSeqCtrl = %04x, u2SeqCtrl = %04x\n",
            prFragInfo->u2NextFragSeqCtrl, u2SeqCtrl));

		/* prSWRfb->fgFragmented = TRUE; */
		/* whsu: todo for checksum */
	} else {
        prFragInfo->pr1stFrag->prRxStatus->u2RxByteCount += prSWRfb->u2PayloadLength;        

        if (prFragInfo->pr1stFrag->prRxStatus->u2RxByteCount > CFG_RX_MAX_PKT_SIZE) {

            prFragInfo->pr1stFrag->eDst = RX_PKT_DESTINATION_NULL;
			QUEUE_INSERT_TAIL(prReturnedQue, (P_QUE_ENTRY_T) prFragInfo->pr1stFrag);

			prFragInfo->pr1stFrag = (P_SW_RFB_T) NULL;

            nicRxReturnRFB(prAdapter, prSWRfb);
		} else {
            kalMemCopy(prFragInfo->pucNextFragStart,
                prSWRfb->pucPayload, prSWRfb->u2PayloadLength);            
			/* [6630] update rx byte count and packet length */
            prFragInfo->pr1stFrag->u2PacketLen += prSWRfb->u2PayloadLength;  
            prFragInfo->pr1stFrag->u2PayloadLength += prSWRfb->u2PayloadLength;
                
			if (fgLast) {	/* The last one, free the buffer */
                DBGLOG(RX, LOUD, ("Defrag: finished\n"));

                prOutputSwRfb = prFragInfo->pr1stFrag;

				prFragInfo->pr1stFrag = (P_SW_RFB_T) NULL;
			} else {
                DBGLOG(RX, LOUD, ("Defrag: mid fraged\n"));

                prFragInfo->pucNextFragStart += prSWRfb->u2PayloadLength;                

                prFragInfo->u2NextFragSeqCtrl++;
            }

            nicRxReturnRFB(prAdapter, prSWRfb);
        }
    }

	/* DBGLOG_MEM8(RXM, INFO, */
	/* prFragInfo->pr1stFrag->pucPayload, */
	/* prFragInfo->pr1stFrag->u2PayloadLength); */
#endif
    return prOutputSwRfb;
} /* end of rxmDefragMPDU() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Do duplicate detection
*
* @param prSwRfb Pointer to the RX packet
*
* @return TRUE: a duplicate, FALSE: not a duplicate
*/
/*----------------------------------------------------------------------------*/
BOOLEAN nicRxIsDuplicateFrame(IN OUT P_SW_RFB_T prSwRfb)
{

  /* Non-QoS Unicast Data or Unicast MMPDU: SC Cache #4;
    *   QoS Unicast Data: SC Cache #0~3;
    *   Broadcast/Multicast: RetryBit == 0
    */
    UINT_32 u4SeqCtrlCacheIdx;
    UINT_16 u2SequenceControl, u2FrameCtrl;
    BOOLEAN fgIsDuplicate = FALSE, fgIsAmsduSubframe = FALSE;
    P_WLAN_MAC_HEADER_T prWlanHeader = NULL;
    P_HW_MAC_RX_DESC_T prRxStatus = NULL;
    P_HW_MAC_RX_STS_GROUP_4_T prRxStatusGroup4 = NULL;

    DEBUGFUNC("nicRx: Enter rxmIsDuplicateFrame()\n");

    ASSERT(prSwRfb);

    /* Situations in which the STC_REC is missing include:
    *   (1) Probe Request (2) (Re)Association Request (3) IBSS data frames (4) Probe Response
    */
	if (!prSwRfb->prStaRec) {
        return FALSE;
    }

    prRxStatus = prSwRfb->prRxStatus;
    ASSERT(prRxStatus);

    fgIsAmsduSubframe = HAL_RX_STATUS_GET_PAYLOAD_FORMAT(prRxStatus);
    if (HAL_RX_STATUS_IS_HEADER_TRAN(prRxStatus) == FALSE) {
		prWlanHeader = (P_WLAN_MAC_HEADER_T) prSwRfb->pvHeader;
        u2SequenceControl = prWlanHeader->u2SeqCtrl;
        u2FrameCtrl = prWlanHeader->u2FrameCtrl;
	} else {
        prRxStatusGroup4 = prSwRfb->prRxStatusGroup4;
        u2SequenceControl = HAL_RX_STATUS_GET_SEQFrag_NUM(prRxStatusGroup4);
        u2FrameCtrl = HAL_RX_STATUS_GET_FRAME_CTL_FIELD(prRxStatusGroup4);
    }
    prSwRfb->u2SequenceControl = u2SequenceControl;


    /* Case 1: Unicast QoS data */
	if (RXM_IS_QOS_DATA_FRAME(u2FrameCtrl)) {	/* WLAN header shall exist when doing duplicate detection */
		if (prSwRfb->prStaRec->aprRxReorderParamRefTbl[prSwRfb->ucTid]) {

            /* QoS data with an RX BA agreement
            *  Case 1: The packet is not an AMPDU subframe, so the RetryBit may be set to 1 (TBC).
            *  Case 2: The RX BA agreement was just established. Some enqueued packets may not be
            *          sent with aggregation.
            */

			DBGLOG(RX, LOUD, ("RX: SC=0x%X (BA Entry present)\n", u2SequenceControl));

            /* Update the SN cache in order to ensure the correctness of duplicate removal in case the BA agreement is deleted */
            prSwRfb->prStaRec->au2CachedSeqCtrl[prSwRfb->ucTid] = u2SequenceControl;


			/* debug */
#if 0
			DBGLOG(RXM, LOUD, ("RXM: SC= 0x%X (Cache[%d] updated) with BA\n",
                u2SequenceControl, prSwRfb->ucTID));

			if (g_prMqm->arRxBaTable[prSwRfb->prStaRec->aucRxBaTable[prSwRfb->ucTID]].
			    ucStatus == BA_ENTRY_STATUS_DELETING) {
				DBGLOG(RXM, LOUD,
				       ("RXM: SC= 0x%X (Cache[%d] updated) with DELETING BA ****************\n",
                    u2SequenceControl, prSwRfb->ucTID));
            }
#endif

            /* HW scoreboard shall take care Case 1. Let the layer layer handle Case 2. */
            return FALSE; /* Not a duplicate */
		} else {

			if (prSwRfb->prStaRec->
			    ucDesiredPhyTypeSet & (PHY_TYPE_BIT_HT | PHY_TYPE_BIT_VHT)) {
                u4SeqCtrlCacheIdx = prSwRfb->ucTid;
			} else {
				if (prSwRfb->ucTid < 8) {	/* UP = 0~7 */
                    u4SeqCtrlCacheIdx = aucTid2ACI[prSwRfb->ucTid];
				} else {
					DBGLOG(RX, WARN,
					       ("RXM: (Warning) Unkown QoS Data with TID=%d\n",
                        prSwRfb->ucTid));

                    return TRUE; /* Will be dropped */
                }
            }

        }
    }
    /* Case 2: Unicast non-QoS data or MMPDUs */
	else {
        u4SeqCtrlCacheIdx = TID_NUM;
    }


    /* If this is a retransmission */
	if (u2FrameCtrl & MASK_FC_RETRY) {
		if (u2SequenceControl != prSwRfb->prStaRec->au2CachedSeqCtrl[u4SeqCtrlCacheIdx]) {
            prSwRfb->prStaRec->au2CachedSeqCtrl[u4SeqCtrlCacheIdx] = u2SequenceControl;
			if (fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_FIRST_SUB_AMSDU) {
				prSwRfb->prStaRec->afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx] =
				    TRUE;
            }
			DBGLOG(RX, LOUD, ("RXM: SC= 0x%X (Cache[%lu] updated)\n",
                u2SequenceControl, u4SeqCtrlCacheIdx));
		} else {
            /* A duplicate. */
            if (prSwRfb->prStaRec->afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx]) {
                if (fgIsAmsduSubframe == RX_PAYLOAD_FORMAT_LAST_SUB_AMSDU) {
					prSwRfb->prStaRec->
					    afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx] = FALSE;
                }
			} else {
                fgIsDuplicate = TRUE;
				DBGLOG(RX, LOUD, ("RXM: SC= 0x%X (Cache[%lu] duplicate)\n",
                    u2SequenceControl, u4SeqCtrlCacheIdx));
            }
        }
    }

    /* Not a retransmission */
	else {

        prSwRfb->prStaRec->au2CachedSeqCtrl[u4SeqCtrlCacheIdx] = u2SequenceControl;
        prSwRfb->prStaRec->afgIsIgnoreAmsduDuplicate[u4SeqCtrlCacheIdx] = FALSE;

		DBGLOG(RX, LOUD, ("RXM: SC= 0x%X (Cache[%lu] updated)\n",
            u2SequenceControl, u4SeqCtrlCacheIdx));
    }

    return fgIsDuplicate;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Process packet doesn't need to do buffer reordering
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessPktWithoutReorder(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
    P_RX_CTRL_T prRxCtrl;
    P_TX_CTRL_T prTxCtrl;
    BOOL fgIsRetained = FALSE;
    UINT_32 u4CurrentRxBufferCount;
	/* P_STA_RECORD_T prStaRec = (P_STA_RECORD_T)NULL; */
#if CFG_SUPPORT_MULTITHREAD
    KAL_SPIN_LOCK_DECLARATION();
#endif    
    DEBUGFUNC("nicRxProcessPktWithoutReorder");
	/* DBGLOG(RX, TRACE, ("\n")); */

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

    prTxCtrl = &prAdapter->rTxCtrl;
    ASSERT(prTxCtrl);

    u4CurrentRxBufferCount = prRxCtrl->rFreeSwRfbList.u4NumElem;
    /* QM USED = $A, AVAILABLE COUNT = $B, INDICATED TO OS = $C
     * TOTAL = $A + $B + $C
     *
     * Case #1 (Retain)
     * -------------------------------------------------------
     * $A + $B < THRESHOLD := $A + $B + $C < THRESHOLD + $C := $TOTAL - THRESHOLD < $C
     * => $C used too much, retain
     *
     * Case #2 (Non-Retain)
     * -------------------------------------------------------
     * $A + $B > THRESHOLD := $A + $B + $C > THRESHOLD + $C := $TOTAL - THRESHOLD > $C
     * => still availble for $C to use
     *
     */

#if defined(LINUX)
    fgIsRetained = FALSE;
#else
    fgIsRetained = (((u4CurrentRxBufferCount +
                    qmGetRxReorderQueuedBufferCount(prAdapter) +
                    prTxCtrl->i4PendingFwdFrameCount) < CFG_RX_RETAINED_PKT_THRESHOLD) ?
                           TRUE : FALSE);
#endif

	/* DBGLOG(RX, INFO, ("fgIsRetained = %d\n", fgIsRetained)); */
#if CFG_ENABLE_PER_STA_STATISTICS
	if (prSwRfb->prStaRec && (prAdapter->rWifiVar.rWfdConfigureSettings.ucWfdEnable > 0)) {
        prSwRfb->prStaRec->u4TotalRxPktsNumber++;
    }
#endif
    if (kalProcessRxPacket(prAdapter->prGlueInfo,
                         prSwRfb->pvPacket,
                         prSwRfb->pvHeader,
			       (UINT_32) prSwRfb->u2PacketLen,
			       fgIsRetained, prSwRfb->aeCSUM) != WLAN_STATUS_SUCCESS) {
        DBGLOG(RX, ERROR, ("kalProcessRxPacket return value != WLAN_STATUS_SUCCESS\n"));
        ASSERT(0);

        nicRxReturnRFB(prAdapter, prSwRfb);
        return;
	} else {
#if !CFG_SUPPORT_MULTITHREAD 
        prRxCtrl->apvIndPacket[prRxCtrl->ucNumIndPacket] = prSwRfb->pvPacket;
        prRxCtrl->ucNumIndPacket++;
#endif        
    }

#if CFG_SUPPORT_MULTITHREAD
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
	QUEUE_INSERT_TAIL(&(prAdapter->rRxQueue),
			  (P_QUE_ENTRY_T) GLUE_GET_PKT_QUEUE_ENTRY(prSwRfb->pvPacket));
        prRxCtrl->ucNumIndPacket++;
#endif    

    if (fgIsRetained) {
        prRxCtrl->apvRetainedPacket[prRxCtrl->ucNumRetainedPacket] = prSwRfb->pvPacket;
        prRxCtrl->ucNumRetainedPacket++;
            /* TODO : error handling of nicRxSetupRFB */
        nicRxSetupRFB(prAdapter, prSwRfb);
        nicRxReturnRFB(prAdapter, prSwRfb);
	} else {
        prSwRfb->pvPacket = NULL;
        nicRxReturnRFB(prAdapter, prSwRfb);
    }

#if CFG_SUPPORT_MULTITHREAD
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
#endif      
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Process forwarding data packet
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessForwardPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
    P_MSDU_INFO_T prMsduInfo, prRetMsduInfoList;
    P_TX_CTRL_T prTxCtrl;
    P_RX_CTRL_T prRxCtrl;
    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("nicRxProcessForwardPkt");

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    prTxCtrl = &prAdapter->rTxCtrl;
    prRxCtrl = &prAdapter->rRxCtrl;
    prMsduInfo = cnmPktAlloc(prAdapter, 0);

	if (prMsduInfo &&
       kalProcessRxPacket(prAdapter->prGlueInfo,
                prSwRfb->pvPacket,
                prSwRfb->pvHeader,
			       (UINT_32) prSwRfb->u2PacketLen,
			       prRxCtrl->rFreeSwRfbList.u4NumElem <
			       CFG_RX_RETAINED_PKT_THRESHOLD ? TRUE : FALSE,
                prSwRfb->aeCSUM) == WLAN_STATUS_SUCCESS) {

#if CFG_DBG_MGT_BUF
        prMsduInfo->fgIsUsed = TRUE;
        prMsduInfo->rLastAllocTime = kalGetTimeTick();
#endif


		/* parsing forward frame */
		wlanProcessTxFrame(prAdapter, (P_NATIVE_PACKET) (prSwRfb->pvPacket));
		/* pack into MSDU_INFO_T */
		nicTxFillMsduInfo(prAdapter, prMsduInfo, (P_NATIVE_PACKET) (prSwRfb->pvPacket));

		prMsduInfo->eSrc = TX_PACKET_FORWARDING;
        prMsduInfo->ucBssIndex = secGetBssIdxByWlanIdx(prAdapter, prSwRfb->ucWlanIdx);

		/* release RX buffer (to rIndicatedRfbList) */
        prSwRfb->pvPacket = NULL;
        nicRxReturnRFB(prAdapter, prSwRfb);

		/* increase forward frame counter */
        GLUE_INC_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);

		/* send into TX queue */
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
        prRetMsduInfoList = qmEnqueueTxPackets(prAdapter, prMsduInfo);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

		if (prRetMsduInfoList != NULL) {	/* TX queue refuses queuing the packet */
            nicTxFreeMsduInfoPacket(prAdapter, prRetMsduInfoList);
            nicTxReturnMsduInfo(prAdapter, prRetMsduInfoList);
        }
        /* indicate service thread for sending */
		if (prTxCtrl->i4PendingFwdFrameCount > 0) {
            kalSetEvent(prAdapter->prGlueInfo);
        }
	} else {		/* no TX resource */
        DBGLOG(QM, INFO, ("No Tx MSDU_INFO for forwarding frames\n"));
        nicRxReturnRFB(prAdapter, prSwRfb);
    }

    return;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Process broadcast data packet for both host and forwarding
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessGOBroadcastPkt(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
    P_SW_RFB_T prSwRfbDuplicated;
    P_TX_CTRL_T prTxCtrl;
    P_RX_CTRL_T prRxCtrl;
    P_HW_MAC_RX_DESC_T prRxStatus;

    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("nicRxProcessGOBroadcastPkt");

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    prTxCtrl = &prAdapter->rTxCtrl;
    prRxCtrl = &prAdapter->rRxCtrl;

    prRxStatus = prSwRfb->prRxStatus;
    ASSERT(prRxStatus);

    ASSERT(CFG_NUM_OF_QM_RX_PKT_NUM >= 16);

	if (prRxCtrl->rFreeSwRfbList.u4NumElem
	    >= (CFG_RX_MAX_PKT_NUM - (CFG_NUM_OF_QM_RX_PKT_NUM - 16 /* Reserved for others */))) {

        /* 1. Duplicate SW_RFB_T */
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
        QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfbDuplicated, P_SW_RFB_T);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

		if (prSwRfbDuplicated) {
            kalMemCopy(prSwRfbDuplicated->pucRecvBuff,
                    prSwRfb->pucRecvBuff,
				   ALIGN_4(prRxStatus->u2RxByteCount + HIF_RX_HW_APPENDED_LEN));

            prSwRfbDuplicated->ucPacketType = RX_PKT_TYPE_RX_DATA;
            prSwRfbDuplicated->ucStaRecIdx = prSwRfb->ucStaRecIdx;
            nicRxFillRFB(prAdapter, prSwRfbDuplicated);

            /* 2. Modify eDst */
            prSwRfbDuplicated->eDst = RX_PKT_DESTINATION_FORWARD;

			GLUE_SET_PKT_BSS_IDX(prSwRfbDuplicated->pvPacket,
				     secGetBssIdxByWlanIdx(prAdapter, prSwRfbDuplicated->ucWlanIdx));

            /* 4. Forward */
            nicRxProcessForwardPkt(prAdapter, prSwRfbDuplicated);
        }
	} else {
		DBGLOG(RX, WARN,
		       ("Stop to forward BMC packet due to less free Sw Rfb %lu\n",
			prRxCtrl->rFreeSwRfbList.u4NumElem));
    }

    /* 3. Indicate to host */
    prSwRfb->eDst = RX_PKT_DESTINATION_HOST;
    nicRxProcessPktWithoutReorder(prAdapter, prSwRfb);

    return;
}

#if CFG_SUPPORT_SNIFFER
VOID nicRxFillRadiotapMCS(IN OUT P_MONITOR_RADIOTAP_T prMonitorRadiotap, IN P_HW_MAC_RX_STS_GROUP_3_T prRxStatusGroup3)
{
	UINT_8 ucFrMode;
	UINT_8 ucShortGI;
    UINT_8 ucRxMode;
	UINT_8 ucLDPC;
    UINT_8 ucSTBC;
	UINT_8 ucNess;

    ucFrMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET); /* VHTA1 B0-B1 */
    ucShortGI = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_SHORT_GI) ? 1 : 0; /* HT_shortgi */
    ucRxMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET);
    ucLDPC = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_LDPC) ? 1 : 0; /* HT_adcode */
    ucSTBC = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET); /* HT_stbc */
    ucNess = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_NESS_MASK) >> RX_VT_NESS_OFFSET); /* HT_extltf */
   
    prMonitorRadiotap->ucMcsKnown = ( BITS(0,6) |
                                    (((ucNess & BIT(1)) >> 1) << 7)
                                    );
    
    prMonitorRadiotap->ucMcsFlags = ( (ucFrMode) |
                                    (ucShortGI << 2) |
                                    ((ucRxMode & BIT(0)) << 3) |
                                    (ucLDPC << 4) |
                                    (ucSTBC << 5) |
                                    ((ucNess & BIT(0)) << 7)
                                    );  
    /* Bit[6:0] for 802.11n, mcs0 ~ mcs7 */
    prMonitorRadiotap->ucMcsMcs = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_RATE_MASK);  
}

VOID nicRxFillRadiotapVHT(IN OUT P_MONITOR_RADIOTAP_T prMonitorRadiotap, IN P_HW_MAC_RX_STS_GROUP_3_T prRxStatusGroup3)
{
	UINT_8 ucSTBC;
	UINT_8 ucTxopPsNotAllow;
	UINT_8 ucShortGI;
	UINT_8 ucNsym;
	UINT_8 ucLDPC;
	UINT_8 ucBeamFormed;
	UINT_8 ucFrMode;
	UINT_8 ucNsts;
	UINT_8 ucMcs;

    prMonitorRadiotap->u2VhtKnown = BITS(0,8);

    ucSTBC = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET); /* BIT[7]: VHTA1 B3 */
    ucTxopPsNotAllow = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_TXOP_PS_NOT_ALLOWED) ? 1 : 0; /* VHTA1 B22 */
    ucShortGI = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_SHORT_GI) ? 1 : 0; /* VHTA2 B0 */
    ucNsym = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_SHORT_GI_NSYM) ? 1 : 0; /* VHTA2 B1 */
    ucLDPC = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_LDPC) ? 1 : 0; /* HT_adcode */
    ucBeamFormed = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_BEAMFORMED) ? 1 : 0; /* VHTA2 B8 */
    prMonitorRadiotap->ucVhtFlags = ( (ucSTBC) | 
                                    (ucTxopPsNotAllow << 1) | 
                                    (ucShortGI << 2) |
                                    (ucNsym << 3) |
                                    (ucLDPC << 4) | 
                                    (ucBeamFormed << 5)
                                    );
                                
    ucFrMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET); /* VHTA1 B0-B1 */
    switch (ucFrMode) {
    case RX_VT_FR_MODE_20:
        prMonitorRadiotap->ucVhtBandwidth = 0;
        break;
    case RX_VT_FR_MODE_40:
        prMonitorRadiotap->ucVhtBandwidth = 1;
        break;
    case RX_VT_FR_MODE_80:
        prMonitorRadiotap->ucVhtBandwidth = 4;
        break;
    case RX_VT_FR_MODE_160:
        prMonitorRadiotap->ucVhtBandwidth = 11;
        break;
    default:
        prMonitorRadiotap->ucVhtBandwidth = 0;
    }
    
    /* Set to 0~7 for 1~8 space time streams */
    ucNsts = (((prRxStatusGroup3)->u4RxVector[1] & RX_VT_NSTS_MASK) >> RX_VT_NSTS_OFFSET) + 1; /* VHTA1 B10-B12 */
    
    /* Bit[3:0] for 802.11ac, mcs0 ~ mcs9 */
    ucMcs = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_RATE_AC_MASK);  
    
    prMonitorRadiotap->aucVhtMcsNss[0] = ((ucMcs << 4) | (ucNsts - ucSTBC)); /* STBC	= Nsts - Nss */
    
    prMonitorRadiotap->ucVhtCoding = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_CODING_MASK) >> RX_VT_CODING_OFFSET); /* VHTA2 B2-B3 */
    
    prMonitorRadiotap->ucVhtGroupId = (((((prRxStatusGroup3)->u4RxVector[1] & RX_VT_GROUPID_1_MASK) >> RX_VT_GROUPID_1_OFFSET) << 2) |
                                    (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_GROUPID_0_MASK) >> RX_VT_GROUPID_0_OFFSET)); /* VHTA1 B4-B9 */
    
    prMonitorRadiotap->u2VhtPartialAid = ((((prRxStatusGroup3)->u4RxVector[2] & RX_VT_AID_1_MASK) << 4) |
                                    (((prRxStatusGroup3)->u4RxVector[1] & RX_VT_AID_0_MASK) >> RX_VT_AID_0_OFFSET)); /* VHTA1 B13-B21 */

}

/*----------------------------------------------------------------------------*/
/*!
* @brief Process HIF monitor packet
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessMonitorPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
	struct sk_buff *prSkb = NULL;
	P_RX_CTRL_T prRxCtrl;
	P_HW_MAC_RX_DESC_T prRxStatus;
	P_HW_MAC_RX_STS_GROUP_2_T prRxStatusGroup2;
	P_HW_MAC_RX_STS_GROUP_3_T prRxStatusGroup3;
	MONITOR_RADIOTAP_T rMonitorRadiotap;
	RADIOTAP_FIELD_VENDOR_T rRadiotapFieldVendor;
	PUINT_8 prVendorNsOffset;
	UINT_32 u4VendorNsLen;
	UINT_32 u4RadiotapLen;
	UINT_32 u4ItPresent;
	UINT_8 aucMtkOui[] = VENDOR_OUI_MTK;
	UINT_8 ucRxRate;
	UINT_8 ucRxMode;
	UINT_8 ucChanNum;
	UINT_8 ucMcs;
	UINT_8 ucFrMode;
	UINT_8 ucShortGI;

#if CFG_SUPPORT_MULTITHREAD
    KAL_SPIN_LOCK_DECLARATION();
#endif 

	DEBUGFUNC("nicRxProcessMonitorPacket");

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    prRxCtrl = &prAdapter->rRxCtrl;	
														
	nicRxFillRFB(prAdapter, prSwRfb);
	
	/* can't parse radiotap info if no rx vector */
	if (((prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_2)) == 0) ||
		((prSwRfb->ucGroupVLD & BIT(RX_GROUP_VLD_3)) == 0)) {
		nicRxReturnRFB(prAdapter, prSwRfb);
		return;
	}
	
	prRxStatus = prSwRfb->prRxStatus;				
	prRxStatusGroup2 = prSwRfb->prRxStatusGroup2;					
	prRxStatusGroup3 = prSwRfb->prRxStatusGroup3;

    /* Bit Number 30 Vendor Namespace */
    u4VendorNsLen = sizeof(RADIOTAP_FIELD_VENDOR_T);
    rRadiotapFieldVendor.aucOUI[0] = aucMtkOui[0];
    rRadiotapFieldVendor.aucOUI[1] = aucMtkOui[1];
    rRadiotapFieldVendor.aucOUI[2] = aucMtkOui[2];
    rRadiotapFieldVendor.ucSubNamespace = 0;
    rRadiotapFieldVendor.u2DataLen = u4VendorNsLen - 6;
    rRadiotapFieldVendor.ucData = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET); /* VHTA1 B0-B1 */
        
	ucRxMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET);
    
	if (ucRxMode == RX_VT_VHT_MODE) {
		u4RadiotapLen = RADIOTAP_LEN_VHT;
		u4ItPresent = RADIOTAP_FIELDS_VHT;
	}
	else if ((ucRxMode == RX_VT_MIXED_MODE) || (ucRxMode == RX_VT_GREEN_MODE)) {
		u4RadiotapLen = RADIOTAP_LEN_HT;
		u4ItPresent = RADIOTAP_FIELDS_HT;
	}
	else {
		u4RadiotapLen = RADIOTAP_LEN_LEGACY;
		u4ItPresent = RADIOTAP_FIELDS_LEGACY;
	}
	
	/* Radiotap Header & Bit Number 30 Vendor Namespace */
    prVendorNsOffset = (PUINT_8)&rMonitorRadiotap + u4RadiotapLen;
    u4RadiotapLen += u4VendorNsLen;
	kalMemSet(&rMonitorRadiotap, 0, sizeof(MONITOR_RADIOTAP_T));
    kalMemCopy(prVendorNsOffset, (PUINT_8)&rRadiotapFieldVendor, u4VendorNsLen);
	rMonitorRadiotap.u2ItLen = cpu_to_le16(u4RadiotapLen);
	rMonitorRadiotap.u4ItPresent = u4ItPresent;
									
	/* Bit Number 0 TSFT */
	rMonitorRadiotap.u8MacTime = (prRxStatusGroup2->u4Timestamp);

	/* Bit Number 1 FLAGS */
	if (HAL_RX_STATUS_IS_FRAG(prRxStatus) == TRUE) {
		rMonitorRadiotap.ucFlags |= BIT(3);
	}
					
	if (HAL_RX_STATUS_IS_FCS_ERROR(prRxStatus) == TRUE) {
		rMonitorRadiotap.ucFlags |= BIT(6);
	}

	/* Bit Number 2 RATE */	
	if ((ucRxMode == RX_VT_LEGACY_CCK) || (ucRxMode == RX_VT_LEGACY_OFDM)) {
		/* Bit[2:0] for Legacy CCK, Bit[3:0] for Legacy OFDM */
		ucRxRate = ((prRxStatusGroup3)->u4RxVector[0] & BITS(0,3));
		rMonitorRadiotap.ucRate = aucHwRate2PhyRate[ucRxRate];
	}
	else {
		ucMcs = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_RX_RATE_AC_MASK);
		ucFrMode = (((prRxStatusGroup3)->u4RxVector[0] & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET); /* VHTA1 B0-B1 */
		ucShortGI = ((prRxStatusGroup3)->u4RxVector[0] & RX_VT_SHORT_GI) ? 1 : 0; /* VHTA2 B0 */

		/* ucRate(500kbs) = u4PhyRate(100kbps) / 5, max ucRate = 0xFF */
		if (arMcsRate2PhyRate[ucMcs].u4PhyRate[ucFrMode][ucShortGI] > 1275)
			rMonitorRadiotap.ucRate = 0xFF;
		else
			rMonitorRadiotap.ucRate = arMcsRate2PhyRate[ucMcs].u4PhyRate[ucFrMode][ucShortGI] / 5;
	}
					
	/* Bit Number 3 CHANNEL */
	if (ucRxMode == RX_VT_LEGACY_CCK) {
		rMonitorRadiotap.u2ChFlags |= BIT(5);
	}
	else { /* OFDM */
		rMonitorRadiotap.u2ChFlags |= BIT(6);
	}
	
	ucChanNum = HAL_RX_STATUS_GET_CHNL_NUM(prRxStatus);
	if (HAL_RX_STATUS_GET_RF_BAND(prRxStatus) == BAND_2G4) {
		rMonitorRadiotap.u2ChFlags |= BIT(7);
		rMonitorRadiotap.u2ChFrequency = (ucChanNum * 5 + 2407);
	}
	else { /* BAND_5G */
		rMonitorRadiotap.u2ChFlags |= BIT(8);
		rMonitorRadiotap.u2ChFrequency = (ucChanNum * 5 + 5000);
	}
					
	/* Bit Number 5 ANT SIGNAL */
	rMonitorRadiotap.ucAntennaSignal = (((prRxStatusGroup3)->u4RxVector[3] & RX_VT_IB_RSSI_MASK));
					
	/* Bit Number 6 ANT NOISE */
	rMonitorRadiotap.ucAntennaNoise = ((((prRxStatusGroup3)->u4RxVector[5] & RX_VT_NF0_MASK) >> 1) + 128);
					
	/* Bit Number 11 ANT */
	rMonitorRadiotap.ucAntenna = ((prRxStatusGroup3)->u4RxVector[2] & RX_VT_SEL_ANT) ? 1 : 0;

	/* Bit Number 19 MCS */
	if ((u4ItPresent & RADIOTAP_FIELD_MCS)) {
        nicRxFillRadiotapMCS(&rMonitorRadiotap, prRxStatusGroup3);
	}
	
	/* Bit Number 20 AMPDU */
	if (HAL_RX_STATUS_IS_AMPDU_SUB_FRAME(prRxStatus)) {
		if (HAL_RX_STATUS_GET_RXV_SEQ_NO(prRxStatus)) {
			++prRxCtrl->u4AmpduRefNum;
		}
		rMonitorRadiotap.u4AmpduRefNum = prRxCtrl->u4AmpduRefNum;
	} 

	/* Bit Number 21 VHT */
	if ((u4ItPresent & RADIOTAP_FIELD_VHT)) {
        nicRxFillRadiotapVHT(&rMonitorRadiotap, prRxStatusGroup3);
	}
						
	prSwRfb->pvHeader -= u4RadiotapLen;
	kalMemCopy(prSwRfb->pvHeader, &rMonitorRadiotap, u4RadiotapLen);
   
    prSkb = (struct sk_buff *)(prSwRfb->pvPacket);  
    prSkb->data = (unsigned char *)(prSwRfb->pvHeader);
    
    skb_reset_tail_pointer(prSkb);
    skb_trim(prSkb, 0);
    skb_put(prSkb, (u4RadiotapLen + prSwRfb->u2PacketLen));

#if CFG_SUPPORT_MULTITHREAD
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
    QUEUE_INSERT_TAIL(&(prAdapter->rRxQueue), (P_QUE_ENTRY_T) GLUE_GET_PKT_QUEUE_ENTRY(prSwRfb->pvPacket));
    prRxCtrl->ucNumIndPacket++;
#else
    prRxCtrl->apvIndPacket[prRxCtrl->ucNumIndPacket] = prSwRfb->pvPacket;   
    prRxCtrl->ucNumIndPacket++;
#endif

    prSwRfb->pvPacket = NULL;
    nicRxReturnRFB(prAdapter, prSwRfb);
    
#if CFG_SUPPORT_MULTITHREAD
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
#endif 
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* @brief Process HIF data packet
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
static UINT_32 u4LastRxPacketTime = 0;
VOID nicRxProcessDataPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
    P_RX_CTRL_T prRxCtrl;
    P_SW_RFB_T prRetSwRfb, prNextSwRfb;
    P_HW_MAC_RX_DESC_T prRxStatus;
    BOOLEAN fgDrop;
	P_STA_RECORD_T prStaRec;

    DEBUGFUNC("nicRxProcessDataPacket");
	/* DBGLOG(INIT, TRACE, ("\n")); */

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    fgDrop = FALSE;

    prRxStatus = prSwRfb->prRxStatus;
    prRxCtrl = &prAdapter->rRxCtrl;

	/* Check AMPDU_nERR_Bitmap */
    prSwRfb->fgDataFrame = TRUE;
    prSwRfb->fgFragFrame = FALSE;
    prSwRfb->fgReorderBuffer = FALSE;

	/* BA session */
    if ((prRxStatus->u2StatusFlag & RXS_DW2_AMPDU_nERR_BITMAP) == RXS_DW2_AMPDU_nERR_VALUE) {
        prSwRfb->fgReorderBuffer = TRUE;
    }
	/* non BA session */
    else if ((prRxStatus->u2StatusFlag & RXS_DW2_RX_nERR_BITMAP) == RXS_DW2_RX_nERR_VALUE) {
        if ((prRxStatus->u2StatusFlag & RXS_DW2_RX_nDATA_BITMAP) == RXS_DW2_RX_nDATA_VALUE) {
            prSwRfb->fgDataFrame = FALSE;
        }
        if ((prRxStatus->u2StatusFlag & RXS_DW2_RX_FRAG_BITMAP) == RXS_DW2_RX_FRAG_VALUE) {
            prSwRfb->fgFragFrame = TRUE;
        }
	} else {
        fgDrop = TRUE;
		if (!HAL_RX_STATUS_IS_ICV_ERROR(prRxStatus)
		    && HAL_RX_STATUS_IS_TKIP_MIC_ERROR(prRxStatus)) {
            
            prStaRec = cnmGetStaRecByAddress(prAdapter,
                    prAdapter->prAisBssInfo->ucBssIndex,
							 prAdapter->rWlanInfo.rCurrBssId.
							 arMacAddress);
            if (prStaRec) {
                DBGLOG(RSN, EVENT, ("MIC_ERR_PKT\n"));
                rsnTkipHandleMICFailure(prAdapter, prStaRec, 0);
            }
		} else if (HAL_RX_STATUS_IS_LLC_MIS(prRxStatus)) {
            DBGLOG(RSN, EVENT, ("LLC_MIS_ERR\n"));
            fgDrop = TRUE; /* Drop after send de-auth  */
        }
    }

#if 0 /* Check 1x Pkt */
    if (prSwRfb->u2PacketLen > 14) {
		PUINT_8 pc = (PUINT_8) prSwRfb->pvHeader;
        UINT_16 u2Etype = 0;

        u2Etype = (pc[ETHER_TYPE_LEN_OFFSET] << 8) | (pc[ETHER_TYPE_LEN_OFFSET + 1]);

#if CFG_SUPPORT_WAPI
        if (u2Etype == ETH_P_1X || u2Etype == ETH_WPI_1X) {
            DBGLOG(RSN, INFO, ("R1X len=%d\n", prSwRfb->u2PacketLen));
        }
#else
        if (u2Etype == ETH_P_1X) {
            DBGLOG(RSN, INFO, ("R1X len=%d\n", prSwRfb->u2PacketLen));
        }
#endif
        else if (u2Etype == ETH_P_PRE_1X) {
            DBGLOG(RSN, INFO, ("Pre R1X len=%d\n", prSwRfb->u2PacketLen));
        }
    }
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
	if (fgDrop == FALSE) {
        UINT_32 u4TcpUdpIpCksStatus;
		PUINT_32 pu4Temp;
		pu4Temp = (PUINT_32)prRxStatus;
		u4TcpUdpIpCksStatus = *(pu4Temp+(ALIGN_4(prRxStatus->u2RxByteCount)>>2));
        nicRxFillChksumStatus(prAdapter, prSwRfb, u4TcpUdpIpCksStatus);
    }
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	/* if(secCheckClassError(prAdapter, prSwRfb, prStaRec) == TRUE && */
    if (prAdapter->fgTestMode == FALSE && fgDrop == FALSE) {
#if CFG_HIF_RX_STARVATION_WARNING
        prRxCtrl->u4QueuedCnt++;
#endif
        nicRxFillRFB(prAdapter, prSwRfb);
		GLUE_SET_PKT_BSS_IDX(prSwRfb->pvPacket,
				     secGetBssIdxByWlanIdx(prAdapter, prSwRfb->ucWlanIdx));

		prRetSwRfb = qmHandleRxPackets(prAdapter, prSwRfb);
		if (prRetSwRfb != NULL) {
            do {
				/* save next first */
				prNextSwRfb =
				    (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prRetSwRfb);

				struct sk_buff *skb = (struct sk_buff *)(prRetSwRfb->pvPacket);
				skb->data = (unsigned char *)prRetSwRfb->pvHeader;
				UINT_32 uPackLen = prRetSwRfb->u2PacketLen;
				/* Reset skb ,if not skb->end is not correct*/
				skb_reset_tail_pointer(skb);
				skb_trim(skb, 0);
				BOOL bErrLen = ((skb->tail + uPackLen) > skb->end)?TRUE:FALSE;
				if(bErrLen == TRUE) {
					DBGLOG(RX, ERROR, ("tail[%p], uPackLen[%d],and end[%p] \n",
										skb->tail, uPackLen, skb->end));
					prRetSwRfb->eDst = RX_PKT_DESTINATION_NULL;
				}

				switch (prRetSwRfb->eDst) {
                case RX_PKT_DESTINATION_HOST:

					prStaRec = cnmGetStaRecByIndex(prAdapter, prRetSwRfb->ucStaRecIdx);
					if (prStaRec && IS_STA_IN_AIS(prStaRec)) {
#if ARP_MONITER_ENABLE
						qmHandleRxArpPackets(prAdapter, prRetSwRfb);
#endif
						u4LastRxPacketTime = kalGetTimeTick();
					}

                    nicRxProcessPktWithoutReorder(prAdapter, prRetSwRfb);
                    break;

                case RX_PKT_DESTINATION_FORWARD:
                    nicRxProcessForwardPkt(prAdapter, prRetSwRfb);
                    break;

                case RX_PKT_DESTINATION_HOST_WITH_FORWARD:
                    nicRxProcessGOBroadcastPkt(prAdapter, prRetSwRfb);
                    break;

                case RX_PKT_DESTINATION_NULL:
                    nicRxReturnRFB(prAdapter, prRetSwRfb);
                    RX_INC_CNT(prRxCtrl, RX_DST_NULL_DROP_COUNT);
                    RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
                    break;

                default:
                    break;
                }
#if CFG_HIF_RX_STARVATION_WARNING
                prRxCtrl->u4DequeuedCnt++;
#endif
                prRetSwRfb = prNextSwRfb;
			} while (prRetSwRfb);
        }
	} else {
        nicRxReturnRFB(prAdapter, prSwRfb);
        RX_INC_CNT(prRxCtrl, RX_CLASS_ERR_DROP_COUNT);
        RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
    }
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Process HIF event packet
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessEventPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
    P_CMD_INFO_T prCmdInfo;
    //P_MSDU_INFO_T prMsduInfo;
    P_WIFI_EVENT_T prEvent;
    P_GLUE_INFO_T prGlueInfo;
    BOOLEAN fgIsNewVersion;

    DEBUGFUNC("nicRxProcessEventPacket");
	/* DBGLOG(INIT, TRACE, ("\n")); */

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;
    prGlueInfo = prAdapter->prGlueInfo;

    DBGLOG(INIT, INFO, ("RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n", 
			    prEvent->ucEID, prEvent->ucSeqNum, prEvent->u2PacketLength));
    
	/* Event Handling */
	switch (prEvent->ucEID) {
#if 0 /* It is removed now */
    case EVENT_ID_CMD_RESULT:
        prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
            P_EVENT_CMD_RESULT prCmdResult;
			prCmdResult = (P_EVENT_CMD_RESULT) ((PUINT_8) prEvent + EVENT_HDR_SIZE);

            /* CMD_RESULT should be only in response to Set commands */
            ASSERT(prCmdInfo->fgSetQuery == FALSE || prCmdInfo->fgNeedResp == TRUE);

			if (prCmdResult->ucStatus == 0) {	/* success */
				if (prCmdInfo->pfCmdDoneHandler) {
					prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
								    prEvent->aucBuffer);
				} else if (prCmdInfo->fgIsOid == TRUE) {
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_SUCCESS);
                }
			} else if (prCmdResult->ucStatus == 1) {	/* reject */
				if (prCmdInfo->fgIsOid == TRUE)
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_FAILURE);
			} else if (prCmdResult->ucStatus == 2) {	/* unknown CMD */
				if (prCmdInfo->fgIsOid == TRUE)
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_NOT_SUPPORTED);
                }
			/* return prCmdInfo */
            cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
        }

        break;
#endif

#if 0
    case EVENT_ID_CONNECTION_STATUS:
        /* OBSELETE */
        {
            P_EVENT_CONNECTION_STATUS prConnectionStatus;
            prConnectionStatus = (P_EVENT_CONNECTION_STATUS) (prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_CONNECTION_STATUS = %d\n",
				 prConnectionStatus->ucMediaStatus);
			if (prConnectionStatus->ucMediaStatus == PARAM_MEDIA_STATE_DISCONNECTED) {	/* disconnected */
				if (kalGetMediaStateIndicated(prGlueInfo) !=
				    PARAM_MEDIA_STATE_DISCONNECTED) {

                    kalIndicateStatusAndComplete(prGlueInfo,
                            WLAN_STATUS_MEDIA_DISCONNECT,
								     NULL, 0);

                    prAdapter->rWlanInfo.u4SysTime = kalGetTimeTick();
                }
			} else if (prConnectionStatus->ucMediaStatus == PARAM_MEDIA_STATE_CONNECTED) {	/* connected */
                prAdapter->rWlanInfo.u4SysTime = kalGetTimeTick();

				/* fill information for association result */
                prAdapter->rWlanInfo.rCurrBssId.rSsid.u4SsidLen
                    = prConnectionStatus->ucSsidLen;
                kalMemCopy(prAdapter->rWlanInfo.rCurrBssId.rSsid.aucSsid,
                        prConnectionStatus->aucSsid,
                        prConnectionStatus->ucSsidLen);

                kalMemCopy(prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
					   prConnectionStatus->aucBssid, MAC_ADDR_LEN);

				prAdapter->rWlanInfo.rCurrBssId.u4Privacy = prConnectionStatus->ucEncryptStatus;	/* @FIXME */
				prAdapter->rWlanInfo.rCurrBssId.rRssi = 0;	/* @FIXME */
				prAdapter->rWlanInfo.rCurrBssId.eNetworkTypeInUse = PARAM_NETWORK_TYPE_AUTOMODE;	/* @FIXME */
                prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4BeaconPeriod
                    = prConnectionStatus->u2BeaconPeriod;
                prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4ATIMWindow
                    = prConnectionStatus->u2ATIMWindow;
                prAdapter->rWlanInfo.rCurrBssId.rConfiguration.u4DSConfig
                    = prConnectionStatus->u4FreqInKHz;
                prAdapter->rWlanInfo.ucNetworkType
                    = prConnectionStatus->ucNetworkType;

				switch (prConnectionStatus->ucInfraMode) {
                case 0:
                    prAdapter->rWlanInfo.rCurrBssId.eOpMode = NET_TYPE_IBSS;
                    break;
                case 1:
                    prAdapter->rWlanInfo.rCurrBssId.eOpMode = NET_TYPE_INFRA;
                    break;
                case 2:
                default:
					prAdapter->rWlanInfo.rCurrBssId.eOpMode =
					    NET_TYPE_AUTO_SWITCH;
                    break;
                }
				/* always indicate to OS according to MSDN (re-association/roaming) */
                kalIndicateStatusAndComplete(prGlueInfo,
							     WLAN_STATUS_MEDIA_CONNECT, NULL, 0);
            }
        }
        break;

    case EVENT_ID_SCAN_RESULT:
        /* OBSELETE */
        break;
#endif

    case EVENT_ID_RX_ADDBA:
        /* The FW indicates that an RX BA agreement will be established */
        qmHandleEventRxAddBa(prAdapter, prEvent);
        break;

    case EVENT_ID_RX_DELBA:
        /* The FW indicates that an RX BA agreement has been deleted */
        qmHandleEventRxDelBa(prAdapter, prEvent);
        break;

    case EVENT_ID_CHECK_REORDER_BUBBLE:
        qmHandleEventCheckReorderBubble(prAdapter, prEvent);
        break;

    case EVENT_ID_LINK_QUALITY:
#if CFG_ENABLE_WIFI_DIRECT && CFG_SUPPORT_P2P_RSSI_QUERY
        if (prEvent->u2PacketLen == EVENT_HDR_SIZE + sizeof(EVENT_LINK_QUALITY_EX)) {
			P_EVENT_LINK_QUALITY_EX prLqEx =
			    (P_EVENT_LINK_QUALITY_EX) (prEvent->aucBuffer);

            if (prLqEx->ucIsLQ0Rdy) {
				nicUpdateLinkQuality(prAdapter, 0, (P_EVENT_LINK_QUALITY) prLqEx);
            }
            if (prLqEx->ucIsLQ1Rdy) {
				nicUpdateLinkQuality(prAdapter, 1, (P_EVENT_LINK_QUALITY) prLqEx);
            }
		} else {
            /* For old FW, P2P may invoke link quality query, and make driver flag becone TRUE. */
            DBGLOG(P2P, WARN, ("Old FW version, not support P2P RSSI query.\n"));

            /* Must not use NETWORK_TYPE_P2P_INDEX, cause the structure is mismatch. */
			nicUpdateLinkQuality(prAdapter, 0,
					     (P_EVENT_LINK_QUALITY) (prEvent->aucBuffer));
        }
#else
		/*only support ais query */
        {
            UINT_8          ucBssIndex;
            P_BSS_INFO_T    prBssInfo;
            for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
                prBssInfo = prAdapter->aprBssInfo[ucBssIndex];

				if ((prBssInfo->eNetworkType == NETWORK_TYPE_AIS)
				    && (prBssInfo->fgIsInUse))
                    break;
            }

            if (ucBssIndex >= BSS_INFO_NUM) {
                ucBssIndex = 1; /* No hit(bss1 for default ais network) */
            }
			/* printk("=======> rssi with bss%d ,%d\n",ucBssIndex,((P_EVENT_LINK_QUALITY_V2)(prEvent->aucBuffer))->rLq[ucBssIndex].cRssi); */
			nicUpdateLinkQuality(prAdapter, ucBssIndex,
					     (P_EVENT_LINK_QUALITY_V2) (prEvent->aucBuffer));
        }
        
#endif

        /* command response handling */
        prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
            if (prCmdInfo->pfCmdDoneHandler) {
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
							    prEvent->aucBuffer);
			} else if (prCmdInfo->fgIsOid) {
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
					       WLAN_STATUS_SUCCESS);
            }
			/* return prCmdInfo */
            cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
        }
#ifndef LINUX
		if (prAdapter->rWlanInfo.eRssiTriggerType == ENUM_RSSI_TRIGGER_GREATER &&
		    prAdapter->rWlanInfo.rRssiTriggerValue >=
		    (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi)) {
            prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_TRIGGERED;

            kalIndicateStatusAndComplete(prGlueInfo,
                    WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID) & (prAdapter->rWlanInfo.
								rRssiTriggerValue),
						     sizeof(PARAM_RSSI));
		} else if (prAdapter->rWlanInfo.eRssiTriggerType == ENUM_RSSI_TRIGGER_LESS
			   && prAdapter->rWlanInfo.rRssiTriggerValue <=
			   (PARAM_RSSI) (prAdapter->rLinkQuality.cRssi)) {
            prAdapter->rWlanInfo.eRssiTriggerType = ENUM_RSSI_TRIGGER_TRIGGERED;

            kalIndicateStatusAndComplete(prGlueInfo,
                    WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID) & (prAdapter->rWlanInfo.
								rRssiTriggerValue),
						     sizeof(PARAM_RSSI));
        }
#endif

        break;

    case EVENT_ID_MIC_ERR_INFO:
        {
            P_EVENT_MIC_ERR_INFO prMicError;
			/* P_PARAM_AUTH_EVENT_T prAuthEvent; */
            P_STA_RECORD_T prStaRec;

            DBGLOG(RSN, EVENT, ("EVENT_ID_MIC_ERR_INFO\n"));

			prMicError = (P_EVENT_MIC_ERR_INFO) (prEvent->aucBuffer);
            prStaRec = cnmGetStaRecByAddress(prAdapter,
                    prAdapter->prAisBssInfo->ucBssIndex,
							 prAdapter->rWlanInfo.rCurrBssId.
							 arMacAddress);
            ASSERT(prStaRec);

            if (prStaRec) {
				rsnTkipHandleMICFailure(prAdapter, prStaRec,
							(BOOLEAN) prMicError->u4Flags);
			} else {
                DBGLOG(RSN, INFO, ("No STA rec!!\n"));
            }
#if 0
			prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

            /* Status type: Authentication Event */
            prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_AUTHENTICATION;

            /* Authentication request */
            prAuthEvent->arRequest[0].u4Length = sizeof(PARAM_AUTH_REQUEST_T);
			kalMemCopy((PVOID) prAuthEvent->arRequest[0].arBssid, (PVOID) prAdapter->rWlanInfo.rCurrBssId.arMacAddress,	/* whsu:Todo? */
                PARAM_MAC_ADDR_LEN);

            if (prMicError->u4Flags != 0) {
                prAuthEvent->arRequest[0].u4Flags = PARAM_AUTH_REQUEST_GROUP_ERROR;
			} else {
				prAuthEvent->arRequest[0].u4Flags =
				    PARAM_AUTH_REQUEST_PAIRWISE_ERROR;
            }

            kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
                WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID) prAuthEvent,
						     sizeof(PARAM_STATUS_INDICATION_T) +
						     sizeof(PARAM_AUTH_REQUEST_T));
#endif
        }
        break;

#if 0 /* Marked for MT6630 */
    case EVENT_ID_ASSOC_INFO:
        {
            P_EVENT_ASSOC_INFO prAssocInfo;
			prAssocInfo = (P_EVENT_ASSOC_INFO) (prEvent->aucBuffer);

            kalHandleAssocInfo(prAdapter->prGlueInfo, prAssocInfo);
        }
        break;

    case EVENT_ID_802_11_PMKID:
        {
            P_PARAM_AUTH_EVENT_T           prAuthEvent;
            PUINT_8                        cp;
            UINT_32                        u4LenOfUsedBuffer;

			prAuthEvent = (P_PARAM_AUTH_EVENT_T) prAdapter->aucIndicationEventBuffer;

            prAuthEvent->rStatus.eStatusType = ENUM_STATUS_TYPE_CANDIDATE_LIST;

			u4LenOfUsedBuffer = (UINT_32) (prEvent->u2PacketLength - 8);

            prAuthEvent->arRequest[0].u4Length = u4LenOfUsedBuffer;

			cp = (PUINT_8) &prAuthEvent->arRequest[0];

            /* Status type: PMKID Candidatelist Event */
			kalMemCopy(cp, (P_EVENT_PMKID_CANDIDATE_LIST_T) (prEvent->aucBuffer),
				   prEvent->u2PacketLength - 8);

            kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
                WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
						     (PVOID) prAuthEvent,
						     sizeof(PARAM_STATUS_INDICATION_T) +
						     u4LenOfUsedBuffer);
        }
        break;
#endif

#if 0
    case EVENT_ID_ACTIVATE_STA_REC_T:
        {
            P_EVENT_ACTIVATE_STA_REC_T prActivateStaRec;
			prActivateStaRec = (P_EVENT_ACTIVATE_STA_REC_T) (prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_ACTIVATE_STA_REC_T Index:%d, MAC:[" MACSTR
				 "]\n", prActivateStaRec->ucStaRecIdx,
                MAC2STR(prActivateStaRec->aucMacAddr));

            qmActivateStaRec(prAdapter,
					 (UINT_32) prActivateStaRec->ucStaRecIdx,
					 ((prActivateStaRec->fgIsQoS) ? TRUE : FALSE),
                             prActivateStaRec->ucNetworkTypeIndex,
					 ((prActivateStaRec->fgIsAP) ? TRUE : FALSE),
                             prActivateStaRec->aucMacAddr);

        }
        break;

    case EVENT_ID_DEACTIVATE_STA_REC_T:
        {
            P_EVENT_DEACTIVATE_STA_REC_T prDeactivateStaRec;
			prDeactivateStaRec = (P_EVENT_DEACTIVATE_STA_REC_T) (prEvent->aucBuffer);

			DbgPrint("RX EVENT: EVENT_ID_DEACTIVATE_STA_REC_T Index:%d, MAC:[" MACSTR
				 "]\n", prDeactivateStaRec->ucStaRecIdx);

			qmDeactivateStaRec(prAdapter, prDeactivateStaRec->ucStaRecIdx);
        }
        break;
#endif

    case EVENT_ID_SCAN_DONE:
        fgIsNewVersion = FALSE;
        if (prEvent->u2PacketLength > (EVENT_HDR_SIZE + sizeof(EVENT_SCAN_DONE)- SCAN_DONE_DIFFERENCE)){
            fgIsNewVersion = TRUE;
        }
		scnEventScanDone(prAdapter, (P_EVENT_SCAN_DONE)(prEvent->aucBuffer), fgIsNewVersion  );
        break;

    case EVENT_ID_NLO_DONE:
        DBGLOG(INIT, INFO, ("EVENT_ID_NLO_DONE \n"));
		scnEventNloDone(prAdapter, (P_EVENT_NLO_DONE_T) (prEvent->aucBuffer));
#if CFG_SUPPORT_PNO        
        prAdapter->prAisBssInfo->fgIsPNOEnable = FALSE;
        if(prAdapter->prAisBssInfo->fgIsNetRequestInActive && prAdapter->prAisBssInfo->fgIsPNOEnable){
            UNSET_NET_ACTIVE(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);        
            DBGLOG(INIT, INFO, ("INACTIVE  AIS from  ACTIVEto disable PNO \n"));
            // sync with firmware
            nicDeactivateNetwork(prAdapter, prAdapter->prAisBssInfo->ucBssIndex);
        }
#endif  
        break;

    case EVENT_ID_TX_DONE:
#if 1
		nicTxProcessTxDoneEvent(prAdapter, prEvent);
#else
		{
			P_EVENT_TX_DONE_T prTxDone;
			prTxDone = (P_EVENT_TX_DONE_T) (prEvent->aucBuffer);

			DBGLOG(INIT, INFO,("EVENT_ID_TX_DONE WIDX:PID[%u:%u] Status[%u] SN[%u]\n",
				prTxDone->ucWlanIndex, prTxDone->ucPacketSeq, prTxDone->ucStatus, prTxDone->u2SequenceNumber));

			/* call related TX Done Handler */
			prMsduInfo =
				nicGetPendingTxMsduInfo(prAdapter, prTxDone->ucWlanIndex,
							prTxDone->ucPacketSeq);

#if CFG_SUPPORT_802_11V_TIMING_MEASUREMENT
			DBGLOG(INIT, TRACE, ("EVENT_ID_TX_DONE u4TimeStamp = %x u2AirDelay = %x\n",
				prTxDone->au4Reserved1, prTxDone->au4Reserved2));

			wnmReportTimingMeas(prAdapter, prMsduInfo->ucStaRecIndex,
						prTxDone->au4Reserved1,
						prTxDone->au4Reserved1 + prTxDone->au4Reserved2);
#endif

			if (prMsduInfo) {
				prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo,
								(ENUM_TX_RESULT_CODE_T)(prTxDone->ucStatus));
				
				if(prMsduInfo->eSrc == TX_PACKET_MGMT) {
					cnmMgtPktFree(prAdapter, prMsduInfo);
				}
				else {
					nicTxReturnMsduInfo(prAdapter, prMsduInfo);
				}
			}
		}
#endif

        break;

    case EVENT_ID_SLEEPY_INFO:
        {
            P_EVENT_SLEEPY_INFO_T prEventSleepyNotify;
			prEventSleepyNotify = (P_EVENT_SLEEPY_INFO_T) (prEvent->aucBuffer);

			/* DBGLOG(RX, INFO, ("ucSleepyState = %d\n", prEventSleepyNotify->ucSleepyState)); */

			prAdapter->fgWiFiInSleepyState =
			    (BOOLEAN) (prEventSleepyNotify->ucSleepyState);

#if CFG_SUPPORT_MULTITHREAD
			if (prEventSleepyNotify->ucSleepyState) {
                kalSetFwOwnEvent2Hif(prGlueInfo);
            }
#endif            
        }
        break;
    case EVENT_ID_BT_OVER_WIFI:
#if CFG_ENABLE_BT_OVER_WIFI
        {
            UINT_8 aucTmp[sizeof(AMPC_EVENT) + sizeof(BOW_LINK_DISCONNECTED)];
            P_EVENT_BT_OVER_WIFI prEventBtOverWifi;
            P_AMPC_EVENT prBowEvent;
            P_BOW_LINK_CONNECTED prBowLinkConnected;
            P_BOW_LINK_DISCONNECTED prBowLinkDisconnected;

			prEventBtOverWifi = (P_EVENT_BT_OVER_WIFI) (prEvent->aucBuffer);

			/* construct event header */
			prBowEvent = (P_AMPC_EVENT) aucTmp;

			if (prEventBtOverWifi->ucLinkStatus == 0) {
				/* Connection */
                prBowEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_CONNECTED;
                prBowEvent->rHeader.ucSeqNumber = 0;
                prBowEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_CONNECTED);

				/* fill event body */
				prBowLinkConnected =
				    (P_BOW_LINK_CONNECTED) (prBowEvent->aucPayload);
				prBowLinkConnected->rChannel.ucChannelNum =
				    prEventBtOverWifi->ucSelectedChannel;
				kalMemZero(prBowLinkConnected->aucPeerAddress, MAC_ADDR_LEN);	/* @FIXME */

                kalIndicateBOWEvent(prAdapter->prGlueInfo, prBowEvent);
			} else {
				/* Disconnection */
                prBowEvent->rHeader.ucEventId = BOW_EVENT_ID_LINK_DISCONNECTED;
                prBowEvent->rHeader.ucSeqNumber = 0;
                prBowEvent->rHeader.u2PayloadLength = sizeof(BOW_LINK_DISCONNECTED);

				/* fill event body */
				prBowLinkDisconnected =
				    (P_BOW_LINK_DISCONNECTED) (prBowEvent->aucPayload);
				prBowLinkDisconnected->ucReason = 0;	/* @FIXME */
				kalMemZero(prBowLinkDisconnected->aucPeerAddress, MAC_ADDR_LEN);	/* @FIXME */

                kalIndicateBOWEvent(prAdapter->prGlueInfo, prBowEvent);
            }
        }
        break;
#endif
    case EVENT_ID_STATISTICS:
        /* buffer statistics for further query */
        prAdapter->fgIsStatValid = TRUE;
        prAdapter->rStatUpdateTime = kalGetTimeTick();
        kalMemCopy(&prAdapter->rStatStruct, prEvent->aucBuffer, sizeof(EVENT_STATISTICS));

        /* command response handling */
        prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
            if (prCmdInfo->pfCmdDoneHandler) {
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
							    prEvent->aucBuffer);
			} else if (prCmdInfo->fgIsOid) {
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
					       WLAN_STATUS_SUCCESS);
            }
			/* return prCmdInfo */
            cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
        }

        break;

    case EVENT_ID_CH_PRIVILEGE:
        cnmChMngrHandleChEvent(prAdapter, prEvent);
        break;

    case EVENT_ID_BSS_ABSENCE_PRESENCE:
        qmHandleEventBssAbsencePresence(prAdapter, prEvent);
        break;

    case EVENT_ID_STA_CHANGE_PS_MODE:
        qmHandleEventStaChangePsMode(prAdapter, prEvent);
        break;
#if CFG_ENABLE_WIFI_DIRECT
    case EVENT_ID_STA_UPDATE_FREE_QUOTA:
        qmHandleEventStaUpdateFreeQuota(prAdapter, prEvent);
        break;
#endif
    case EVENT_ID_BSS_BEACON_TIMEOUT:
		DBGLOG(INIT, INFO, ("EVENT_ID_BSS_BEACON_TIMEOUT\n"));

        if (prAdapter->fgDisBcnLostDetection == FALSE) {
			P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;
            P_EVENT_BSS_BEACON_TIMEOUT_T prEventBssBeaconTimeout;
			prEventBssBeaconTimeout =
			    (P_EVENT_BSS_BEACON_TIMEOUT_T) (prEvent->aucBuffer);

            if (prEventBssBeaconTimeout->ucBssIndex >= BSS_INFO_NUM) {
                break;
            }

            DBGLOG(INIT, INFO, ("Reason code: %d \n", prEventBssBeaconTimeout->ucReasonCode));

			prBssInfo =
			    GET_BSS_INFO_BY_INDEX(prAdapter, prEventBssBeaconTimeout->ucBssIndex);

			if (prEventBssBeaconTimeout->ucBssIndex ==
			    prAdapter->prAisBssInfo->ucBssIndex) {
			    if (prEventBssBeaconTimeout->ucReasonCode == BEACON_TIMEOUT_DUE_2_NO_TX_DONE_EVENT)
					break;

				if (!CHECK_FOR_TIMEOUT(kalGetTimeTick(), u4LastRxPacketTime, SEC_TO_MSEC(2))) {
					DBGLOG(RX, INFO, ("Ignore beacon timeout\n"));
					break;
				}
				prAdapter->prAisBssInfo->u2DeauthReason = prEventBssBeaconTimeout->ucReasonCode;
                aisBssBeaconTimeout(prAdapter);
            }
#if CFG_ENABLE_WIFI_DIRECT
			else if ((prBssInfo->eNetworkType == NETWORK_TYPE_P2P)) {
                p2pRoleFsmRunEventBeaconTimeout(prAdapter, prBssInfo);
            }
#endif
#if CFG_ENABLE_BT_OVER_WIFI
			else if (GET_BSS_INFO_BY_INDEX
				 (prAdapter,
				  prEventBssBeaconTimeout->ucBssIndex)->eNetworkType ==
				 NETWORK_TYPE_BOW) {
            }
#endif
            else {
				DBGLOG(RX, ERROR,
				       ("EVENT_ID_BSS_BEACON_TIMEOUT: (ucBssIndex = %d)\n",
                            prEventBssBeaconTimeout->ucBssIndex));
            }
        }

        break;
    case EVENT_ID_UPDATE_NOA_PARAMS:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered) {
            P_EVENT_UPDATE_NOA_PARAMS_T prEventUpdateNoaParam;
			prEventUpdateNoaParam = (P_EVENT_UPDATE_NOA_PARAMS_T) (prEvent->aucBuffer);

			if (GET_BSS_INFO_BY_INDEX(prAdapter, prEventUpdateNoaParam->ucBssIndex)->
			    eNetworkType == NETWORK_TYPE_P2P) {
                p2pProcessEvent_UpdateNOAParam(prAdapter,
                                                prEventUpdateNoaParam->ucBssIndex,
                                                prEventUpdateNoaParam);
            } else {
                ASSERT(0);
            }
        }
#else
        ASSERT(0);
#endif
        break;

    case EVENT_ID_STA_AGING_TIMEOUT:
#if CFG_ENABLE_WIFI_DIRECT
        {
            if (prAdapter->fgDisStaAgingTimeoutDetection == FALSE) {
                P_EVENT_STA_AGING_TIMEOUT_T prEventStaAgingTimeout;
                P_STA_RECORD_T prStaRec;
				P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T) NULL;

				prEventStaAgingTimeout =
				    (P_EVENT_STA_AGING_TIMEOUT_T) (prEvent->aucBuffer);
				prStaRec =
				    cnmGetStaRecByIndex(prAdapter,
							prEventStaAgingTimeout->ucStaRecIdx);
                if (prStaRec == NULL) {
                    break;
                }

				DBGLOG(INIT, INFO, ("EVENT_ID_STA_AGING_TIMEOUT %u " MACSTR "\n",
						    prEventStaAgingTimeout->ucStaRecIdx,
						    MAC2STR(prStaRec->aucMacAddr)));

                prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prStaRec->ucBssIndex);

                bssRemoveClient(prAdapter, prBssInfo, prStaRec);

                /* Call False Auth */
                if (prAdapter->fgIsP2PRegistered) {
					p2pFuncDisconnect(prAdapter, prBssInfo, prStaRec, TRUE,
							  REASON_CODE_DISASSOC_INACTIVITY);
                }


			}
			/* gDisStaAgingTimeoutDetection */
        }
#endif
        break;

    case EVENT_ID_AP_OBSS_STATUS:
#if CFG_ENABLE_WIFI_DIRECT
		if (prAdapter->fgIsP2PRegistered) {
			rlmHandleObssStatusEventPkt(prAdapter,
						    (P_EVENT_AP_OBSS_STATUS_T) prEvent->aucBuffer);
        }
#endif
        break;

    case EVENT_ID_ROAMING_STATUS:
#if CFG_SUPPORT_ROAMING
        {
            P_CMD_ROAMING_TRANSIT_T prTransit;

			prTransit = (P_CMD_ROAMING_TRANSIT_T) (prEvent->aucBuffer);
            roamingFsmProcessEvent(prAdapter, prTransit);
        }
#endif /* CFG_SUPPORT_ROAMING */
        break;
    case EVENT_ID_SEND_DEAUTH:
#if DBG
        {
            P_WLAN_MAC_HEADER_T prWlanMacHeader;

			prWlanMacHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
			DBGLOG(RX, INFO,
			       ("nicRx: aucAddr1: " MACSTR "\n",
				MAC2STR(prWlanMacHeader->aucAddr1)));
			DBGLOG(RX, INFO,
			       ("nicRx: aucAddr2: " MACSTR "\n",
				MAC2STR(prWlanMacHeader->aucAddr2)));
        }
#endif
          /* receive packets without StaRec */
		prSwRfb->pvHeader = (P_WLAN_MAC_HEADER_T) &prEvent->aucBuffer[0];
          if (WLAN_STATUS_SUCCESS == authSendDeauthFrame(prAdapter,
                                                       NULL,
                                                       NULL,
                                                       prSwRfb,
                                                       REASON_CODE_CLASS_3_ERR,
							       (PFN_TX_DONE_HANDLER) NULL)) {
            DBGLOG(RX, INFO, ("Send Deauth Error\n"));
        }
          break;

#if CFG_SUPPORT_RDD_TEST_MODE
    case EVENT_ID_UPDATE_RDD_STATUS:
        {
            P_EVENT_RDD_STATUS_T prEventRddStatus;

            prEventRddStatus = (P_EVENT_RDD_STATUS_T) (prEvent->aucBuffer);

            prAdapter->ucRddStatus = prEventRddStatus->ucRddStatus;
        }

        break;
#endif

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
    case EVENT_ID_UPDATE_BWCS_STATUS:
        {
            P_PTA_IPC_T prEventBwcsStatus;

            prEventBwcsStatus = (P_PTA_IPC_T) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
			printk(KERN_INFO DRV_NAME "BCM BWCS Event: %02x%02x%02x%02x\n",
			       prEventBwcsStatus->u.aucBTPParams[0],
                prEventBwcsStatus->u.aucBTPParams[1],
                prEventBwcsStatus->u.aucBTPParams[2],
                prEventBwcsStatus->u.aucBTPParams[3]);

			printk(KERN_INFO DRV_NAME
			       "BCM BWCS Event: aucBTPParams[0] = %02x, aucBTPParams[1] = %02x, aucBTPParams[2] = %02x, aucBTPParams[3] = %02x\n",
                prEventBwcsStatus->u.aucBTPParams[0],
                prEventBwcsStatus->u.aucBTPParams[1],
                prEventBwcsStatus->u.aucBTPParams[2],
                prEventBwcsStatus->u.aucBTPParams[3]);
#endif

            kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
                WLAN_STATUS_BWCS_UPDATE,
						     (PVOID) prEventBwcsStatus, sizeof(PTA_IPC_T));
        }

        break;

    case EVENT_ID_UPDATE_BCM_DEBUG:
        {
            P_PTA_IPC_T prEventBwcsStatus;

            prEventBwcsStatus = (P_PTA_IPC_T) (prEvent->aucBuffer);

#if CFG_SUPPORT_BCM_BWCS_DEBUG
			printk(KERN_INFO DRV_NAME "BCM FW status: %02x%02x%02x%02x\n",
			       prEventBwcsStatus->u.aucBTPParams[0],
                prEventBwcsStatus->u.aucBTPParams[1],
                prEventBwcsStatus->u.aucBTPParams[2],
                prEventBwcsStatus->u.aucBTPParams[3]);

			printk(KERN_INFO DRV_NAME
			       "BCM FW status: aucBTPParams[0] = %02x, aucBTPParams[1] = %02x, aucBTPParams[2] = %02x, aucBTPParams[3] = %02x\n",
                prEventBwcsStatus->u.aucBTPParams[0],
                prEventBwcsStatus->u.aucBTPParams[1],
                prEventBwcsStatus->u.aucBTPParams[2],
                prEventBwcsStatus->u.aucBTPParams[3]);
#endif
        }

        break;
#endif
    case EVENT_ID_ADD_PKEY_DONE:
        {
            P_EVENT_ADD_KEY_DONE_INFO prAddKeyDone;
            P_STA_RECORD_T prStaRec;

			prAddKeyDone = (P_EVENT_ADD_KEY_DONE_INFO) (prEvent->aucBuffer);

			DBGLOG(RSN, EVENT,
			       ("EVENT_ID_ADD_PKEY_DONE BSSIDX=%d " MACSTR "\n",
				prAddKeyDone->ucBSSIndex, MAC2STR(prAddKeyDone->aucStaAddr)));

            prStaRec = cnmGetStaRecByAddress(prAdapter,
                    prAddKeyDone->ucBSSIndex,
                    prAddKeyDone->aucStaAddr);

            if (prStaRec) {
				DBGLOG(RSN, EVENT,
				       ("STA " MACSTR " Add Key Done!!\n",
					MAC2STR(prStaRec->aucMacAddr)));
                prStaRec->fgIsTxKeyReady = TRUE;
                qmUpdateStaRec(prAdapter, prStaRec);
            }
        }
        break;
    case EVENT_ID_ICAP_DONE:
        {
            P_EVENT_ICAP_STATUS_T prEventIcapStatus;
            PARAM_CUSTOM_MEM_DUMP_STRUC_T rMemDumpInfo;
            UINT_32     u4QueryInfo;

            prEventIcapStatus = (P_EVENT_ICAP_STATUS_T) (prEvent->aucBuffer);

            rMemDumpInfo.u4Address = prEventIcapStatus->u4StartAddress;
            rMemDumpInfo.u4Length = prEventIcapStatus->u4IcapSieze;

			wlanoidQueryMemDump(prAdapter, &rMemDumpInfo, sizeof(rMemDumpInfo),
					    &u4QueryInfo);
            
        }

        break;
    case EVENT_ID_DEBUG_MSG:
        {
            P_EVENT_DEBUG_MSG_T prEventDebugMsg;
            UINT_16  u2DebugMsgId;
            UINT_8   ucMsgType;
            UINT_8   ucFlags;
            UINT_32  u4Value;
            UINT_16  u2MsgSize;
            P_UINT_8 pucMsg;

            prEventDebugMsg = (P_EVENT_DEBUG_MSG_T) (prEvent->aucBuffer);

            u2DebugMsgId = prEventDebugMsg->u2DebugMsgId;
            ucMsgType = prEventDebugMsg->ucMsgType;
            ucFlags = prEventDebugMsg->ucFlags;
            u4Value = prEventDebugMsg->u4Value;
            u2MsgSize = prEventDebugMsg->u2MsgSize;
            pucMsg = prEventDebugMsg->aucMsg;

			DBGLOG(SW4, INFO, ("DEBUG_MSG Id %u Type %u Fg 0x%x Val 0x%x Size %u\n",
					   u2DebugMsgId, ucMsgType, ucFlags, u4Value, u2MsgSize));

			if (u2MsgSize <= DEBUG_MSG_SIZE_MAX) {
				if (ucMsgType >= DEBUG_MSG_TYPE_END) {
                    ucMsgType = DEBUG_MSG_TYPE_MEM32;
                }

				if (ucMsgType == DEBUG_MSG_TYPE_ASCII) {
                    pucMsg[u2MsgSize] = '\0';
                    DBGLOG(SW4, INFO, ("%s\n", pucMsg));
				} else if (ucMsgType == DEBUG_MSG_TYPE_MEM32) {

#if CFG_SUPPORT_XLOG
					/* dumpMemory32(ANDROID_LOG_INFO, pucMsg, u2MsgSize); */
#else
					/* dumpMemory32(pucMsg, u2MsgSize); */
#endif
					DBGLOG_MEM32(SW4, INFO, pucMsg, u2MsgSize);
				} else if (prEventDebugMsg->ucMsgType == DEBUG_MSG_TYPE_MEM8) {
#if CFG_SUPPORT_XLOG
/* dumpMemory8(ANDROID_LOG_INFO, pucMsg, u2MsgSize); */
#else
					/* dumpMemory8(pucMsg, u2MsgSize); */
#endif
					DBGLOG_MEM8(SW4, INFO, pucMsg, u2MsgSize);
				} else {
#if CFG_SUPPORT_XLOG
					/* dumpMemory32(ANDROID_LOG_INFO, pucMsg, u2MsgSize); */
#else
					/* dumpMemory32(pucMsg, u2MsgSize); */
#endif
					DBGLOG_MEM32(SW4, INFO, pucMsg, u2MsgSize);
                }
            } /* DEBUG_MSG_SIZE_MAX */
            else {
				DBGLOG(SW4, INFO, ("Debug msg size %u is too large.\n", u2MsgSize));
            }
        }
        break;
        
#if CFG_SUPPORT_BATCH_SCAN
    case EVENT_ID_BATCH_RESULT:
        DBGLOG(SCN, TRACE, ("Got EVENT_ID_BATCH_RESULT"));

        /* command response handling */
        prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
            if (prCmdInfo->pfCmdDoneHandler) {
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
							    prEvent->aucBuffer);
			} else if (prCmdInfo->fgIsOid) {
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
					       WLAN_STATUS_SUCCESS);
            }
			/* return prCmdInfo */
            cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
        }

        break;
#endif /* CFG_SUPPORT_BATCH_SCAN */


#if CFG_SUPPORT_TDLS
        case EVENT_ID_TDLS:
            
		TdlsexEventHandle(prAdapter->prGlueInfo,
				  (UINT_8 *) prEvent->aucBuffer,
				  (UINT_32) (prEvent->u2PacketLength - 8));
            break;
#endif /* CFG_SUPPORT_TDLS */

    case EVENT_ID_DUMP_MEM:
		DBGLOG(INIT, INFO, ("%s: EVENT_ID_DUMP_MEM\n", __func__));
        
        prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
			DBGLOG(INIT, INFO, (": ==> 1\n"));
            if (prCmdInfo->pfCmdDoneHandler) {
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
							    prEvent->aucBuffer);
			} else if (prCmdInfo->fgIsOid) {
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
					       WLAN_STATUS_SUCCESS);
            }
			/* return prCmdInfo */
            cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		} else {
			/* Burst mode */
			DBGLOG(INIT, INFO, (": ==> 2\n"));
			nicEventQueryMemDump(prAdapter, prEvent->aucBuffer);
        }
        break;

    case EVENT_ID_ACCESS_REG:
    case EVENT_ID_NIC_CAPABILITY:
		/* case EVENT_ID_MAC_MCAST_ADDR: */
    case EVENT_ID_ACCESS_EEPROM:
    case EVENT_ID_TEST_STATUS:
    default:
        prCmdInfo = nicGetPendingCmdInfo(prAdapter, prEvent->ucSeqNum);

		if (prCmdInfo != NULL) {
            if (prCmdInfo->pfCmdDoneHandler) {
				prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
							    prEvent->aucBuffer);
			} else if (prCmdInfo->fgIsOid) {
				kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0,
					       WLAN_STATUS_SUCCESS);
            }
			/* return prCmdInfo */
            cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
        }

        break;
    }

    /* Reset Chip NoAck flag */
	if (prGlueInfo->prAdapter->fgIsChipNoAck) {
        DBGLOG(INIT, WARN, ("Got response from chip, clear NoAck flag!\n"));
        WARN_ON(TRUE);
    }
    prGlueInfo->prAdapter->ucOidTimeoutCount = 0;
    prGlueInfo->prAdapter->fgIsChipNoAck = FALSE;

    nicRxReturnRFB(prAdapter, prSwRfb);
}


/*----------------------------------------------------------------------------*/
/*!
* @brief nicRxProcessMgmtPacket is used to dispatch management frames
*        to corresponding modules
*
* @param prAdapter Pointer to the Adapter structure.
* @param prSWRfb the RFB to receive rx data
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessMgmtPacket(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
    UINT_8 ucSubtype;
#if CFG_SUPPORT_802_11W
	/* BOOL   fgMfgDrop = FALSE; */
#endif
    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    nicRxFillRFB(prAdapter, prSwRfb);
	if (prSwRfb->prRxStatusGroup3 == NULL) {
		DBGLOG(RX, WARN, ("rxStatusGroup3 for MGMT frame is NULL, drop this packet\n"));
		DBGLOG_MEM8(RX, WARN, (PUINT_8) prSwRfb->prRxStatus,
						prSwRfb->prRxStatus->u2RxByteCount > 12 ? prSwRfb->prRxStatus->u2RxByteCount:12);
		nicRxReturnRFB(prAdapter, prSwRfb);
        RX_INC_CNT(&prAdapter->rRxCtrl, RX_DROP_TOTAL_COUNT);
#if CFG_CHIP_RESET_SUPPORT
		glResetTrigger(prAdapter);
#endif
		return;
	}
	ucSubtype = (*(PUINT_8) (prSwRfb->pvHeader) & MASK_FC_SUBTYPE) >> OFFSET_OF_FC_SUBTYPE;

#if CFG_RX_PKTS_DUMP
    {
        P_WLAN_MAC_MGMT_HEADER_T prWlanMgmtHeader;
        UINT_16 u2TxFrameCtrl;

		u2TxFrameCtrl = (*(PUINT_8) (prSwRfb->pvHeader) & MASK_FRAME_TYPE);
        if (prAdapter->rRxCtrl.u4RxPktsDumpTypeMask & BIT(HIF_RX_PKT_TYPE_MANAGEMENT)) {
            if (u2TxFrameCtrl == MAC_FRAME_BEACON ||
                  u2TxFrameCtrl == MAC_FRAME_PROBE_RSP) {

				prWlanMgmtHeader = (P_WLAN_MAC_MGMT_HEADER_T) (prSwRfb->pvHeader);

				DBGLOG(SW4, INFO, ("QM RX MGT: net %u sta idx %u wlan idx %u ssn %u ptype %u subtype %u 11 %u\n", prSwRfb->prStaRec->ucBssIndex, prSwRfb->ucStaRecIdx, prSwRfb->ucWlanIdx, prWlanMgmtHeader->u2SeqCtrl,	/* The new SN of the frame */
						   prSwRfb->ucPacketType, ucSubtype));
				/* HIF_RX_HDR_GET_80211_FLAG(prHifRxHdr))); */

				DBGLOG_MEM8(SW4, TRACE, (PUINT_8) prSwRfb->pvHeader,
					    prSwRfb->u2PacketLen);
            }
        }
    }
#endif
#if CFG_SUPPORT_802_11W
    if (HAL_RX_STATUS_IS_ICV_ERROR(prSwRfb->prRxStatus)) {
        if (HAL_RX_STATUS_GET_SEC_MODE(prSwRfb->prRxStatus) == CIPHER_SUITE_BIP) {
			DBGLOG(RSN, INFO, ("[MFP] RX with BIP ICV ERROR\n"));
		} else {
			DBGLOG(RSN, INFO, ("[MFP] RX with ICV ERROR\n"));
        }
        nicRxReturnRFB(prAdapter, prSwRfb);
        RX_INC_CNT(&prAdapter->rRxCtrl, RX_DROP_TOTAL_COUNT);
        return;     
    }
#endif

	if (prAdapter->fgTestMode == FALSE) {
#if CFG_MGMT_FRAME_HANDLING
		if (apfnProcessRxMgtFrame[ucSubtype]) {
			switch (apfnProcessRxMgtFrame[ucSubtype] (prAdapter, prSwRfb)) {
            case WLAN_STATUS_PENDING:
                return;
            case WLAN_STATUS_SUCCESS:
            case WLAN_STATUS_FAILURE:
                break;

            default:
				DBGLOG(RX, WARN,
				       ("Unexpected MMPDU(0x%02X) returned with abnormal status\n",
					ucSubtype));
                break;
            }
        }
#endif
    }

    nicRxReturnRFB(prAdapter, prSwRfb);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief nicProcessRFBs is used to process RFBs in the rReceivedRFBList queue.
*
* @param prAdapter Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxProcessRFBs(IN P_ADAPTER_T prAdapter)
{
    P_RX_CTRL_T prRxCtrl;
    P_SW_RFB_T prSwRfb = (P_SW_RFB_T)NULL;
    QUE_T rTempRfbList;
    P_QUE_T prTempRfbList = &rTempRfbList;
    UINT_32 u4RxLoopCount;
    
    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("nicRxProcessRFBs");

    ASSERT(prAdapter);

    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

    prRxCtrl->ucNumIndPacket = 0;
    prRxCtrl->ucNumRetainedPacket = 0;
    u4RxLoopCount = prAdapter->rWifiVar.u4TxRxLoopCount;

    QUEUE_INITIALIZE(prTempRfbList);

    while(u4RxLoopCount--) {
        while(QUEUE_IS_NOT_EMPTY(&prRxCtrl->rReceivedRfbList)) {
        
            KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
            QUEUE_MOVE_ALL(prTempRfbList, &prRxCtrl->rReceivedRfbList);
            KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

            while(QUEUE_IS_NOT_EMPTY(prTempRfbList)) {
                QUEUE_REMOVE_HEAD(prTempRfbList, prSwRfb, P_SW_RFB_T);
                
                switch(prSwRfb->ucPacketType){
                case RX_PKT_TYPE_RX_DATA:
#if CFG_SUPPORT_SNIFFER
                    if (prAdapter->prGlueInfo->fgIsEnableMon) {
                        nicRxProcessMonitorPacket(prAdapter, prSwRfb);
                        break;
                    }
#endif	
                    nicRxProcessDataPacket(prAdapter, prSwRfb);
                    break;

                case RX_PKT_TYPE_SW_DEFINED:
                    //HIF_RX_PKT_TYPE_EVENT
                    if ((prSwRfb->prRxStatus->u2PktTYpe & RXM_RXD_PKT_TYPE_SW_BITMAP) == RXM_RXD_PKT_TYPE_SW_EVENT) {
                        nicRxProcessEventPacket(prAdapter, prSwRfb);
                    }
                    //case HIF_RX_PKT_TYPE_MANAGEMENT:
                    else if ((prSwRfb->prRxStatus->u2PktTYpe & RXM_RXD_PKT_TYPE_SW_BITMAP) == RXM_RXD_PKT_TYPE_SW_FRAME) {
                        nicRxProcessMgmtPacket(prAdapter, prSwRfb);
                    }
                    else {
                        DBGLOG(RX, ERROR, ("[%s]ERROR: u2PktTYpe(0x%04X) is OUT OF DEF.!!!\n", __func__, prSwRfb->prRxStatus->u2PktTYpe));
                        ASSERT(0);
                    }
                    break;

                //case HIF_RX_PKT_TYPE_TX_LOOPBACK:
                //case HIF_RX_PKT_TYPE_MANAGEMENT:
                case RX_PKT_TYPE_TX_STATUS:
                case RX_PKT_TYPE_RX_VECTOR:
                case RX_PKT_TYPE_TM_REPORT:
                default:
                    RX_INC_CNT(prRxCtrl, RX_TYPE_ERR_DROP_COUNT);
                    RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT);
                    DBGLOG(RX, ERROR, ("ucPacketType = %d\n", prSwRfb->ucPacketType));
                    break;
                }
				do {
					P_WIFI_EVENT_T prEvent;
					PUINT_8 pvHeader = (PUINT_8)(prSwRfb->pvHeader);
					UINT_8 ucSubtype;
					UINT_16 u2Temp = 0;
					P_WLAN_MAC_MGMT_HEADER_T prWlanMgmtHeader;
					
					if (!kalIsWakeupByWlan(prAdapter))
						break;
	
					switch (prSwRfb->ucPacketType) {
					case RX_PKT_TYPE_RX_DATA:
						if (!pvHeader) {
							DBGLOG(INIT, ERROR, ("Rx Data Packet, but pvHeader is NULL!\n"));
							break;
						}
						if (!prSwRfb->fgReorderBuffer && !prSwRfb->fgDataFrame) {
							P_WLAN_MAC_HEADER_T prWlanMacHeader = (P_WLAN_MAC_HEADER_T)pvHeader;

							if ((prWlanMacHeader->u2FrameCtrl & MASK_FRAME_TYPE) ==
								MAC_FRAME_BLOCK_ACK_REQ) {
								DBGLOG(RX, INFO, ("BAR frame[SSN:%d, TID:%d] wakeup host\n",
									prSwRfb->u2SSN, prSwRfb->ucTid));
								break;
							}
						}
						u2Temp = (pvHeader[ETH_TYPE_LEN_OFFSET] << 8) |
										(pvHeader[ETH_TYPE_LEN_OFFSET + 1]);
						switch (u2Temp) {
						case ETH_P_IPV4:
							u2Temp = *(UINT_16 *) &pvHeader[ETH_HLEN + 4];
							DBGLOG(INIT, INFO, ("IP Packet from:%d.%d.%d.%d, IP ID 0x%04x wakeup host\n",
									pvHeader[ETH_HLEN + 12], pvHeader[ETH_HLEN + 13],
									pvHeader[ETH_HLEN + 14], pvHeader[ETH_HLEN + 15], u2Temp));
							break;
						case ETH_P_1X:
						case ETH_P_PRE_1X:
#if CFG_SUPPORT_WAPI
						case ETH_WPI_1X:
#endif
						case ETH_P_AARP:
						case ETH_P_IPV6:
						case ETH_P_IPX:
						case ETH_P_VLAN:
						case 0x890d: /* TDLS */
							DBGLOG(INIT, INFO, ("Data Packet, EthType 0x%04x wakeup host\n", u2Temp));
							break;
						default:
							DBGLOG(INIT, WARN, ("maybe abnormal data packet, EthType 0x%04x wakeup host, dump it\n", u2Temp));
							DBGLOG_MEM8(INIT, INFO, pvHeader, prSwRfb->u2PacketLen > 50 ? 50:prSwRfb->u2PacketLen);
							break;
						}
						break;
					case RX_PKT_TYPE_SW_DEFINED:
						if ((prSwRfb->prRxStatus->u2PktTYpe & RXM_RXD_PKT_TYPE_SW_BITMAP) == RXM_RXD_PKT_TYPE_SW_EVENT) {
							prEvent = (P_WIFI_EVENT_T) prSwRfb->pucRecvBuff;
							DBGLOG(INIT, INFO, ("Event 0x%02x wakeup host\n", prEvent->ucEID));
						}
						//case HIF_RX_PKT_TYPE_MANAGEMENT:
						else if ((prSwRfb->prRxStatus->u2PktTYpe & RXM_RXD_PKT_TYPE_SW_BITMAP) == RXM_RXD_PKT_TYPE_SW_FRAME) {
							if (!pvHeader) {
								DBGLOG(INIT, ERROR, ("Rx MGMT Packet, but pvHeader is NULL!\n"));
								break;
							}
							prWlanMgmtHeader = (P_WLAN_MAC_MGMT_HEADER_T)pvHeader;
							ucSubtype = (prWlanMgmtHeader->u2FrameCtrl & MASK_FC_SUBTYPE ) >> OFFSET_OF_FC_SUBTYPE;
							DBGLOG(INIT, INFO, ("MGMT frame subtype: %d SeqCtrl %d wakeup host\n",
									ucSubtype, prWlanMgmtHeader->u2SeqCtrl));
						}
						else {
							DBGLOG(RX, ERROR, ("[%s]ERROR: u2PktTYpe(0x%04X) is OUT OF DEF.!!!\n", __func__, prSwRfb->prRxStatus->u2PktTYpe));
							ASSERT(0);
						}
						break;
					default:
						DBGLOG(INIT, INFO, ("Unknow Packet %d wakeup host\n", prSwRfb->ucPacketType));
						break;
					}
				} while (FALSE);

            }

            if (prRxCtrl->ucNumIndPacket > 0) {
                RX_ADD_CNT(prRxCtrl, RX_DATA_INDICATION_COUNT, prRxCtrl->ucNumIndPacket);
                RX_ADD_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT, prRxCtrl->ucNumRetainedPacket);
#if CFG_SUPPORT_MULTITHREAD 
                kalSetTxEvent2Rx(prAdapter->prGlueInfo);
#else
                //DBGLOG(RX, INFO, ("%d packets indicated, Retained cnt = %d\n",
                //    prRxCtrl->ucNumIndPacket, prRxCtrl->ucNumRetainedPacket));
#if CFG_NATIVE_802_11
                kalRxIndicatePkts(prAdapter->prGlueInfo, (UINT_32)prRxCtrl->ucNumIndPacket, (UINT_32)prRxCtrl->ucNumRetainedPacket);
#else
                kalRxIndicatePkts(prAdapter->prGlueInfo, prRxCtrl->apvIndPacket, (UINT_32)prRxCtrl->ucNumIndPacket);
#endif
#endif     
            }      
        }
    }
} /* end of nicRxProcessRFBs() */


#if !CFG_SDIO_INTR_ENHANCE
/*----------------------------------------------------------------------------*/
/*!
* @brief Read the rx data from data port and setup RFB
*
* @param prAdapter pointer to the Adapter handler
* @param prSWRfb the RFB to receive rx data
*
* @retval WLAN_STATUS_SUCCESS: SUCCESS
* @retval WLAN_STATUS_FAILURE: FAILURE
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicRxReadBuffer(IN P_ADAPTER_T prAdapter, IN OUT P_SW_RFB_T prSwRfb)
{
    P_RX_CTRL_T prRxCtrl;
    PUINT_8 pucBuf;
    P_HW_MAC_RX_DESC_T prRxStatus;
    UINT_32 u4PktLen = 0, u4ReadBytes;
    WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
    BOOL fgResult = TRUE;
    UINT_32 u4RegValue;
    UINT_32 rxNum;

    DEBUGFUNC("nicRxReadBuffer");

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

    pucBuf = prSwRfb->pucRecvBuff;
    prRxStatus = prSwRfb->prRxStatus;

    ASSERT(prRxStatus);
    ASSERT(pucBuf);
    DBGLOG(RX, TRACE, ("pucBuf= 0x%x, prRxStatus= 0x%x\n", pucBuf, prRxStatus));

    do {
        /* Read the RFB DW length and packet length */
        HAL_MCR_RD(prAdapter, MCR_WRPLR, &u4RegValue);
        if (!fgResult) {
            DBGLOG(RX, ERROR, ("Read RX Packet Lentgh Error\n"));
            return WLAN_STATUS_FAILURE;
        }
		/* 20091021 move the line to get the HIF RX header (for RX0/1) */
		if (u4RegValue == 0) {
            DBGLOG(RX, ERROR, ("No RX packet\n"));
            return WLAN_STATUS_FAILURE;
        }

        u4PktLen = u4RegValue & BITS(0, 15);
		if (u4PktLen != 0) {
            rxNum = 0;
		} else {
            rxNum = 1;
            u4PktLen = (u4RegValue & BITS(16, 31)) >> 16;
        }

        DBGLOG(RX, TRACE, ("RX%d: u4PktLen = %d\n", rxNum, u4PktLen));

		/* 4 <4> Read Entire RFB and packet, include HW appended DW (Checksum Status) */
        u4ReadBytes = ALIGN_4(u4PktLen) + 4;
        HAL_READ_RX_PORT(prAdapter, rxNum, u4ReadBytes, pucBuf, CFG_RX_MAX_PKT_SIZE);

		/* 20091021 move the line to get the HIF RX header */
		/* u4PktLen = (UINT_32)prHifRxHdr->u2PacketLen; */
		if (u4PktLen != (UINT_32) HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus)) {
           DBGLOG(RX, ERROR, ("Read u4PktLen = %d, prHifRxHdr->u2PacketLen: %d\n",
                                u4PktLen, HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus)));
#if DBG
			dumpMemory8((PUINT_8) prRxStatus,
				    (HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus) >
				     4096) ? 4096 : prRxStatus->u2RxByteCount);
#endif
            ASSERT(0);
        }
        /* u4PktLen is byte unit, not inlude HW appended DW */

		prSwRfb->ucPacketType = (UINT_8) HAL_RX_STATUS_GET_PKT_TYPE(prRxStatus);
        DBGLOG(RX, TRACE, ("ucPacketType = %d\n", prSwRfb->ucPacketType));

		prSwRfb->ucStaRecIdx =
		    secGetStaIdxByWlanIdx(prAdapter,
					  (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prRxStatus));

        /* fgResult will be updated in MACRO */
        if (!fgResult) {
            return WLAN_STATUS_FAILURE;
        }

        DBGLOG(RX, TRACE, ("Dump RX buffer, length = 0x%x\n", u4ReadBytes));
        DBGLOG_MEM8(RX, TRACE, pucBuf, u4ReadBytes);
	} while (FALSE);

    return u4Status;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port, fill RFB
*        and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter   Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxReceiveRFBs(IN P_ADAPTER_T prAdapter)
{
    P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
    P_HW_MAC_RX_DESC_T prRxStatus;
    UINT_32 u4HwAppendDW;
	PUINT_32 pu4Temp;

    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("nicRxReceiveRFBs");

    ASSERT(prAdapter);

    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

    do {
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
        QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

        if (!prSwRfb) {
            DBGLOG(RX, TRACE, ("No More RFB\n"));
            break;
        }
		/* need to consider */
        if (nicRxReadBuffer(prAdapter, prSwRfb) == WLAN_STATUS_FAILURE) {
            DBGLOG(RX, TRACE, ("halRxFillRFB failed\n"));
            nicRxReturnRFB(prAdapter, prSwRfb);
            break;
        }

        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
        QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
        RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

        prRxStatus = prSwRfb->prRxStatus;
        ASSERT(prRxStatus);

		pu4Temp = (PUINT_32)prRxStatus;
		u4HwAppendDW = *(pu4Temp+(ALIGN_4(prRxStatus->u2RxByteCount)>>2));
        DBGLOG(RX, TRACE, ("u4HwAppendDW = 0x%x\n", u4HwAppendDW));
        DBGLOG(RX, TRACE, ("u2PacketLen = 0x%x\n", HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus)));
	} while (FALSE);

    return;

} /* end of nicReceiveRFBs() */

#else
/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port, fill RFB
*        and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param u4DataPort     Specify which port to read
* @param u2RxLength     Specify to the the rx packet length in Byte.
* @param prSwRfb        the RFB to receive rx data.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

WLAN_STATUS
nicRxEnhanceReadBuffer(IN P_ADAPTER_T prAdapter,
		       IN UINT_32 u4DataPort, IN UINT_16 u2RxLength, IN OUT P_SW_RFB_T prSwRfb)
{
    P_RX_CTRL_T prRxCtrl;
    PUINT_8 pucBuf;
    P_HW_MAC_RX_DESC_T prRxStatus;
    UINT_32 u4PktLen = 0;
    WLAN_STATUS u4Status = WLAN_STATUS_FAILURE;
    BOOL fgResult = TRUE;

    DEBUGFUNC("nicRxEnhanceReadBuffer");

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

    pucBuf = prSwRfb->pucRecvBuff;
    ASSERT(pucBuf);

    prRxStatus = prSwRfb->prRxStatus;
    ASSERT(prRxStatus);

	/* DBGLOG(RX, TRACE, ("u2RxLength = %d\n", u2RxLength)); */

    do {
		/* 4 <1> Read RFB frame from MCR_WRDR0, include HW appended DW */
        HAL_READ_RX_PORT(prAdapter,
                         u4DataPort,
                         ALIGN_4(u2RxLength + HIF_RX_HW_APPENDED_LEN),
				 pucBuf, CFG_RX_MAX_PKT_SIZE);

        if (!fgResult) {
            DBGLOG(RX, ERROR, ("Read RX Packet Lentgh Error\n"));
            break;
        }

		u4PktLen = (UINT_32) (HAL_RX_STATUS_GET_RX_BYTE_CNT(prRxStatus));
		/* DBGLOG(RX, TRACE, ("u4PktLen = %d\n", u4PktLen)); */

		prSwRfb->ucPacketType = (UINT_8) HAL_RX_STATUS_GET_PKT_TYPE(prRxStatus);
		/* DBGLOG(RX, TRACE, ("ucPacketType = %d\n", prSwRfb->ucPacketType)); */

		prSwRfb->ucStaRecIdx =
		    secGetStaIdxByWlanIdx(prAdapter,
					  (UINT_8) HAL_RX_STATUS_GET_WLAN_IDX(prRxStatus));

		/* 4 <2> if the RFB dw size or packet size is zero */
        if (u4PktLen == 0) {
            DBGLOG(RX, ERROR, ("Packet Length = %lu\n", u4PktLen));
            ASSERT(0);
            break;
        }
		/* 4 <3> if the packet is too large or too small */
		/* ToDo[6630]: adjust CFG_RX_MAX_PKT_SIZE */
        if (u4PktLen > CFG_RX_MAX_PKT_SIZE) {
            DBGLOG(RX, TRACE, ("Read RX Packet Lentgh Error (%lu)\n", u4PktLen));
            ASSERT(0);
            break;
        }

        u4Status = WLAN_STATUS_SUCCESS;
	} while (FALSE);

    DBGLOG_MEM8(RX, TRACE, pucBuf, ALIGN_4(u2RxLength + HIF_RX_HW_APPENDED_LEN));
    return u4Status;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port for SDIO
*        I/F, fill RFB and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxSDIOReceiveRFBs(IN P_ADAPTER_T prAdapter)
{
    P_SDIO_CTRL_T prSDIOCtrl;
    P_RX_CTRL_T prRxCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
    UINT_32 i, rxNum;
    UINT_16 u2RxPktNum, u2RxLength = 0, u2Tmp = 0;
    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("nicRxSDIOReceiveRFBs");

    ASSERT(prAdapter);

    prSDIOCtrl = prAdapter->prSDIOCtrl;
    ASSERT(prSDIOCtrl);

    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

	for (rxNum = 0; rxNum < 2; rxNum++) {
		u2RxPktNum =
		    (rxNum ==
		     0 ? prSDIOCtrl->rRxInfo.u.u2NumValidRx0Len : prSDIOCtrl->rRxInfo.u.
		     u2NumValidRx1Len);

		if (u2RxPktNum == 0) {
            continue;
        }

        for (i = 0; i < u2RxPktNum; i++) {
			if (rxNum == 0) {
                HAL_READ_RX_LENGTH(prAdapter, &u2RxLength, &u2Tmp);
			} else if (rxNum == 1) {
                HAL_READ_RX_LENGTH(prAdapter, &u2Tmp, &u2RxLength);
            }

            if (!u2RxLength) {
                break;
            }


            KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
            QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
            KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

            if (!prSwRfb) {
                DBGLOG(RX, TRACE, ("No More RFB\n"));
                break;
            }
            ASSERT(prSwRfb);

			if (nicRxEnhanceReadBuffer(prAdapter, rxNum, u2RxLength, prSwRfb) ==
			    WLAN_STATUS_FAILURE) {
                DBGLOG(RX, TRACE, ("nicRxEnhanceRxReadBuffer failed\n"));
                nicRxReturnRFB(prAdapter, prSwRfb);
                break;
            }
			/* prSDIOCtrl->au4RxLength[i] = 0; */

            KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
            QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
            RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
            KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
        }
    }

    prSDIOCtrl->rRxInfo.u.u2NumValidRx0Len = 0;
    prSDIOCtrl->rRxInfo.u.u2NumValidRx1Len = 0;

    return;
}				/* end of nicRxSDIOReceiveRFBs() */

#endif /* CFG_SDIO_INTR_ENHANCE */



#if CFG_SDIO_RX_AGG
/*----------------------------------------------------------------------------*/
/*!
* @brief Read frames from the data port for SDIO with Rx aggregation enabled
*        I/F, fill RFB and put each frame into the rReceivedRFBList queue.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxSDIOAggReceiveRFBs(IN P_ADAPTER_T prAdapter)
{
    P_ENHANCE_MODE_DATA_STRUCT_T prEnhDataStr;
    P_RX_CTRL_T prRxCtrl;
    P_SDIO_CTRL_T prSDIOCtrl;
	P_SW_RFB_T prSwRfb = (P_SW_RFB_T) NULL;
    UINT_32 u4RxLength;
    UINT_32 i, rxNum;
    UINT_32 u4RxAggCount = 0, u4RxAggLength = 0;
    UINT_32 u4RxAvailAggLen, u4CurrAvailFreeRfbCnt;
    PUINT_8 pucSrcAddr;
    P_HW_MAC_RX_DESC_T prRxStatus;
    BOOL fgResult = TRUE;
    BOOLEAN fgIsRxEnhanceMode;
    UINT_16 u2RxPktNum;
#if CFG_SDIO_RX_ENHANCE
    UINT_32 u4MaxLoopCount = CFG_MAX_RX_ENHANCE_LOOP_COUNT;
#endif

    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("nicRxSDIOAggReceiveRFBs");

    ASSERT(prAdapter);
    prEnhDataStr = prAdapter->prSDIOCtrl;
    prRxCtrl = &prAdapter->rRxCtrl;
    prSDIOCtrl = prAdapter->prSDIOCtrl;

#if CFG_SDIO_RX_ENHANCE
    fgIsRxEnhanceMode = TRUE;
#else
    fgIsRxEnhanceMode = FALSE;
#endif

    do {
#if CFG_SDIO_RX_ENHANCE
        /* to limit maximum loop for RX */
        u4MaxLoopCount--;
        if (u4MaxLoopCount == 0) {
            break;
        }
#endif

		if (prEnhDataStr->rRxInfo.u.u2NumValidRx0Len == 0 &&
                prEnhDataStr->rRxInfo.u.u2NumValidRx1Len == 0) {
            break;
        }

		for (rxNum = 0; rxNum < 2; rxNum++) {
			u2RxPktNum =
			    (rxNum ==
			     0 ? prEnhDataStr->rRxInfo.u.u2NumValidRx0Len : prEnhDataStr->rRxInfo.u.
			     u2NumValidRx1Len);

			/* if this assertion happened, it is most likely a F/W bug */
            ASSERT(u2RxPktNum <= 16);

            if (u2RxPktNum > 16)
                  continue;

			if (u2RxPktNum == 0)
                continue;

#if CFG_HIF_STATISTICS
            prRxCtrl->u4TotalRxAccessNum++;
            prRxCtrl->u4TotalRxPacketNum += u2RxPktNum;
#endif

            u4CurrAvailFreeRfbCnt = prRxCtrl->rFreeSwRfbList.u4NumElem;

			/* if SwRfb is not enough, abort reading this time */
			if (u4CurrAvailFreeRfbCnt < u2RxPktNum) {
#if CFG_HIF_RX_STARVATION_WARNING
				DbgPrint("FreeRfb is not enough: %d available, need %d\n",
					 u4CurrAvailFreeRfbCnt, u2RxPktNum);
				DbgPrint("Queued Count: %d / Dequeud Count: %d\n",
					 prRxCtrl->u4QueuedCnt, prRxCtrl->u4DequeuedCnt);
#endif
                continue;
            }
#if CFG_SDIO_RX_ENHANCE
			u4RxAvailAggLen =
			    CFG_RX_COALESCING_BUFFER_SIZE - (sizeof(ENHANCE_MODE_DATA_STRUCT_T) +
							     4 /* extra HW padding */);
#else
            u4RxAvailAggLen = CFG_RX_COALESCING_BUFFER_SIZE;
#endif
            u4RxAggCount = 0;

			for (i = 0; i < u2RxPktNum; i++) {
                u4RxLength = (rxNum == 0 ?
					      (UINT_32) prEnhDataStr->rRxInfo.u.au2Rx0Len[i] :
					      (UINT_32) prEnhDataStr->rRxInfo.u.au2Rx1Len[i]);

                if (!u4RxLength) {
                    ASSERT(0);
                    break;
                }

                if (ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN) < u4RxAvailAggLen) {
                    if (u4RxAggCount < u4CurrAvailFreeRfbCnt) {
						u4RxAvailAggLen -=
						    ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN);
                        u4RxAggCount++;
					} else {
						/* no FreeSwRfb for rx packet */
						DBGLOG(RX, ERROR,
						       ("[%s] RxAggCount(%d) is greater than AvailableFreeCount(%d)\n",
							__func__, u4RxAggCount,
                            u4CurrAvailFreeRfbCnt));                        
                        
                        ASSERT(0);
                        break;
                    }
				} else {
					/* CFG_RX_COALESCING_BUFFER_SIZE is not large enough */
					DBGLOG(RX, ERROR,
					       ("[%s] Request_len(%d) is greater than Available_len(%d)\n",
                        __func__,
                        (ALIGN_4(u4RxLength + HIF_RX_HW_APPENDED_LEN)),
                        u4RxAvailAggLen));                    
                    ASSERT(0);
                    break;
                }
            }

            u4RxAggLength = (CFG_RX_COALESCING_BUFFER_SIZE - u4RxAvailAggLen);
			/* DBGLOG(RX, INFO, ("u4RxAggCount = %d, u4RxAggLength = %d\n", */
			/* u4RxAggCount, u4RxAggLength)); */

            HAL_READ_RX_PORT(prAdapter,
                         rxNum,
                         u4RxAggLength,
                         prRxCtrl->pucRxCoalescingBufPtr,
                         CFG_RX_COALESCING_BUFFER_SIZE);
            if (!fgResult) {
                DBGLOG(RX, ERROR, ("Read RX Agg Packet Error\n"));
                continue;
            }

            pucSrcAddr = prRxCtrl->pucRxCoalescingBufPtr;
            for (i = 0; i < u4RxAggCount; i++) {
                UINT_16 u2PktLength;

                u2PktLength = (rxNum == 0 ?
                        prEnhDataStr->rRxInfo.u.au2Rx0Len[i] :
                        prEnhDataStr->rRxInfo.u.au2Rx1Len[i]);

				if (ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN) > CFG_RX_MAX_PKT_SIZE) {
				   DBGLOG(RX, ERROR, ("[%s] Request_len(%d) is greater than CFG_RX_MAX_PKT_SIZE(%d)...Drop the unexpected packet...\n",
					   __func__,
					   (ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN)),
					   CFG_RX_MAX_PKT_SIZE));				   
				   DBGLOG_MEM32(RX, ERROR, pucSrcAddr, ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN));
				   
				   pucSrcAddr += ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN);
				   RX_INC_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT); 
				   continue;
				}

                KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
                QUEUE_REMOVE_HEAD(&prRxCtrl->rFreeSwRfbList, prSwRfb, P_SW_RFB_T);
                KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

                ASSERT(prSwRfb);
                kalMemCopy(prSwRfb->pucRecvBuff, pucSrcAddr,
                        ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN));

				/* prHifRxHdr = prSwRfb->prHifRxHdr; */
				/* ASSERT(prHifRxHdr); */

                prRxStatus = prSwRfb->prRxStatus;
                ASSERT(prRxStatus);

				prSwRfb->ucPacketType =
				    (UINT_8) HAL_RX_STATUS_GET_PKT_TYPE(prRxStatus);
				/* DBGLOG(RX, TRACE, ("ucPacketType = %d\n", prSwRfb->ucPacketType)); */
#if DBG
				DBGLOG(RX, TRACE,
				       ("Rx status flag = %x wlan index = %d SecMode = %d\n",
					prRxStatus->u2StatusFlag, prRxStatus->ucWlanIdx,
                             HAL_RX_STATUS_GET_SEC_MODE(prRxStatus)));
#endif

                KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
                QUEUE_INSERT_TAIL(&prRxCtrl->rReceivedRfbList, &prSwRfb->rQueEntry);
                RX_INC_CNT(prRxCtrl, RX_MPDU_TOTAL_COUNT);
                KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

                pucSrcAddr += ALIGN_4(u2PktLength + HIF_RX_HW_APPENDED_LEN);
				/* prEnhDataStr->au4RxLength[i] = 0; */
            }

#if CFG_SDIO_RX_ENHANCE
			kalMemCopy(prAdapter->prSDIOCtrl, (pucSrcAddr + 4),
				   sizeof(ENHANCE_MODE_DATA_STRUCT_T));

            /* do the same thing what nicSDIOReadIntStatus() does */
			if ((prSDIOCtrl->u4WHISR & WHISR_TX_DONE_INT) == 0 &&
                    (prSDIOCtrl->rTxInfo.au4WTSR[0] | prSDIOCtrl->rTxInfo.au4WTSR[1])) {
                prSDIOCtrl->u4WHISR |= WHISR_TX_DONE_INT;
            }

			if ((prSDIOCtrl->u4WHISR & BIT(31)) == 0 &&
                    HAL_GET_MAILBOX_READ_CLEAR(prAdapter) == TRUE &&
                    (prSDIOCtrl->u4RcvMailbox0 != 0 || prSDIOCtrl->u4RcvMailbox1 != 0)) {
                prSDIOCtrl->u4WHISR |= BIT(31);
            }

            /* dispatch to interrupt handler with RX bits masked */
			nicProcessIST_impl(prAdapter,
					   prSDIOCtrl->
					   u4WHISR & (~(WHISR_RX0_DONE_INT | WHISR_RX1_DONE_INT)));
#endif
        }

#if !CFG_SDIO_RX_ENHANCE
        prEnhDataStr->rRxInfo.u.u2NumValidRx0Len = 0;
        prEnhDataStr->rRxInfo.u.u2NumValidRx1Len = 0;
#endif
	} while ((prEnhDataStr->rRxInfo.u.u2NumValidRx0Len
                || prEnhDataStr->rRxInfo.u.u2NumValidRx1Len)
            && fgIsRxEnhanceMode);

    return;
}
#endif /* CFG_SDIO_RX_AGG */


/*----------------------------------------------------------------------------*/
/*!
* @brief Setup a RFB and allocate the os packet to the RFB
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prSwRfb        Pointer to the RFB
*
* @retval WLAN_STATUS_SUCCESS
* @retval WLAN_STATUS_RESOURCES
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicRxSetupRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
    PVOID   pvPacket;
    PUINT_8 pucRecvBuff;

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    if (!prSwRfb->pvPacket) {
        kalMemZero(prSwRfb, sizeof(SW_RFB_T));
		pvPacket = kalPacketAlloc(prAdapter->prGlueInfo, CFG_RX_MAX_PKT_SIZE, &pucRecvBuff);
        if (pvPacket == NULL) {
            return WLAN_STATUS_RESOURCES;
        }

        prSwRfb->pvPacket = pvPacket;
		prSwRfb->pucRecvBuff = (PVOID) pucRecvBuff;
	} else {
		kalMemZero(((PUINT_8) prSwRfb + OFFSET_OF(SW_RFB_T, prRxStatus)),
			   (sizeof(SW_RFB_T) - OFFSET_OF(SW_RFB_T, prRxStatus)));
    }

	/* ToDo: remove prHifRxHdr */
	/* prSwRfb->prHifRxHdr = (P_HIF_RX_HEADER_T)(prSwRfb->pucRecvBuff); */
	prSwRfb->prRxStatus = (P_HW_MAC_RX_DESC_T) (prSwRfb->pucRecvBuff);

    return WLAN_STATUS_SUCCESS;

} /* end of nicRxSetupRFB() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This routine is called to put a RFB back onto the "RFB with Buffer" list
*        or "RFB without buffer" list according to pvPacket.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prSwRfb          Pointer to the RFB
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxReturnRFB(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
    P_RX_CTRL_T prRxCtrl;
    P_QUE_ENTRY_T prQueEntry;
    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    ASSERT(prSwRfb);
    prRxCtrl = &prAdapter->rRxCtrl;
    prQueEntry = &prSwRfb->rQueEntry;

    ASSERT(prQueEntry);

    /* The processing on this RFB is done, so put it back on the tail of
       our list */
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

    if (prSwRfb->pvPacket) {
        QUEUE_INSERT_TAIL(&prRxCtrl->rFreeSwRfbList, prQueEntry);
	} else {
        QUEUE_INSERT_TAIL(&prRxCtrl->rIndicatedRfbList, prQueEntry);
    }

    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
    return;
} /* end of nicRxReturnRFB() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Process rx interrupt. When the rx
*        Interrupt is asserted, it means there are frames in queue.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicProcessRxInterrupt(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

#if CFG_SDIO_INTR_ENHANCE
#if CFG_SDIO_RX_AGG
        nicRxSDIOAggReceiveRFBs(prAdapter);
#else
        nicRxSDIOReceiveRFBs(prAdapter);
#endif
#else
    nicRxReceiveRFBs(prAdapter);
#endif /* CFG_SDIO_INTR_ENHANCE */

#if CFG_SUPPORT_MULTITHREAD
	set_bit(GLUE_FLAG_RX_BIT, &(prAdapter->prGlueInfo->ulFlag));
    wake_up_interruptible(&(prAdapter->prGlueInfo->waitq));
#else
    nicRxProcessRFBs(prAdapter);
#endif

    return;

} /* end of nicProcessRxInterrupt() */


#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*----------------------------------------------------------------------------*/
/*!
* @brief Used to update IP/TCP/UDP checksum statistics of RX Module.
*
* @param prAdapter  Pointer to the Adapter structure.
* @param aeCSUM     The array of checksum result.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxUpdateCSUMStatistics(IN P_ADAPTER_T prAdapter, IN const ENUM_CSUM_RESULT_T aeCSUM[]
    )
{
    P_RX_CTRL_T prRxCtrl;

    ASSERT(prAdapter);
    ASSERT(aeCSUM);

    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

    if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_SUCCESS) ||
        (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_SUCCESS)) {

        RX_INC_CNT(prRxCtrl, RX_CSUM_IP_SUCCESS_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_FAILED) ||
             (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_FAILED)) {

        RX_INC_CNT(prRxCtrl, RX_CSUM_IP_FAILED_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_IPV4] == CSUM_RES_NONE) &&
             (aeCSUM[CSUM_TYPE_IPV6] == CSUM_RES_NONE)) {

        RX_INC_CNT(prRxCtrl, RX_CSUM_UNKNOWN_L3_PKT_COUNT);
	} else {
        ASSERT(0);
    }

    if (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_SUCCESS) {
        RX_INC_CNT(prRxCtrl, RX_CSUM_TCP_SUCCESS_COUNT);
	} else if (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_FAILED) {
        RX_INC_CNT(prRxCtrl, RX_CSUM_TCP_FAILED_COUNT);
	} else if (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_SUCCESS) {
        RX_INC_CNT(prRxCtrl, RX_CSUM_UDP_SUCCESS_COUNT);
	} else if (aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_FAILED) {
        RX_INC_CNT(prRxCtrl, RX_CSUM_UDP_FAILED_COUNT);
	} else if ((aeCSUM[CSUM_TYPE_UDP] == CSUM_RES_NONE) &&
             (aeCSUM[CSUM_TYPE_TCP] == CSUM_RES_NONE)) {

        RX_INC_CNT(prRxCtrl, RX_CSUM_UNKNOWN_L4_PKT_COUNT);
	} else {
        ASSERT(0);
    }

    return;
} /* end of nicRxUpdateCSUMStatistics() */
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to query current status of RX Module.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param pucBuffer      Pointer to the message buffer.
* @param pu4Count      Pointer to the buffer of message length count.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxQueryStatus(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count)
{
    P_RX_CTRL_T prRxCtrl;
    PUINT_8 pucCurrBuf = pucBuffer;


    ASSERT(prAdapter);
    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

	/* if (pucBuffer) {} *//* For Windows, we'll print directly instead of sprintf() */
    ASSERT(pu4Count);

    SPRINTF(pucCurrBuf, ("\n\nRX CTRL STATUS:"));
    SPRINTF(pucCurrBuf, ("\n==============="));
    SPRINTF(pucCurrBuf, ("\nFREE RFB w/i BUF LIST :%9ld", prRxCtrl->rFreeSwRfbList.u4NumElem));
	SPRINTF(pucCurrBuf,
		("\nFREE RFB w/o BUF LIST :%9ld", prRxCtrl->rIndicatedRfbList.u4NumElem));
	SPRINTF(pucCurrBuf,
		("\nRECEIVED RFB LIST     :%9ld", prRxCtrl->rReceivedRfbList.u4NumElem));

    SPRINTF(pucCurrBuf, ("\n\n"));

	/* *pu4Count = (UINT_32)((UINT_32)pucCurrBuf - (UINT_32)pucBuffer); */

    return;
} /* end of nicRxQueryStatus() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Clear RX related counters
*
* @param prAdapter Pointer of Adapter Data Structure
*
* @return - (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxClearStatistics(IN P_ADAPTER_T prAdapter)
{
    P_RX_CTRL_T prRxCtrl;

    ASSERT(prAdapter);
    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

    RX_RESET_ALL_CNTS(prRxCtrl);
    return;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to query current statistics of RX Module.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param pucBuffer      Pointer to the message buffer.
* @param pu4Count      Pointer to the buffer of message length count.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxQueryStatistics(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucBuffer, OUT PUINT_32 pu4Count)
{
    P_RX_CTRL_T prRxCtrl;
    PUINT_8 pucCurrBuf = pucBuffer;

    ASSERT(prAdapter);
    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

	/* if (pucBuffer) {} *//* For Windows, we'll print directly instead of sprintf() */
    ASSERT(pu4Count);

#define SPRINTF_RX_COUNTER(eCounter) \
    SPRINTF(pucCurrBuf, ("%-30s : %ld\n", #eCounter, (UINT_32)prRxCtrl->au8Statistics[eCounter]))

    SPRINTF_RX_COUNTER(RX_MPDU_TOTAL_COUNT);
    SPRINTF_RX_COUNTER(RX_SIZE_ERR_DROP_COUNT);
    SPRINTF_RX_COUNTER(RX_DATA_INDICATION_COUNT);
    SPRINTF_RX_COUNTER(RX_DATA_RETURNED_COUNT);
    SPRINTF_RX_COUNTER(RX_DATA_RETAINED_COUNT);

#if CFG_TCP_IP_CHKSUM_OFFLOAD || CFG_TCP_IP_CHKSUM_OFFLOAD_NDIS_60
    SPRINTF_RX_COUNTER(RX_CSUM_TCP_FAILED_COUNT);
    SPRINTF_RX_COUNTER(RX_CSUM_UDP_FAILED_COUNT);
    SPRINTF_RX_COUNTER(RX_CSUM_IP_FAILED_COUNT);
    SPRINTF_RX_COUNTER(RX_CSUM_TCP_SUCCESS_COUNT);
    SPRINTF_RX_COUNTER(RX_CSUM_UDP_SUCCESS_COUNT);
    SPRINTF_RX_COUNTER(RX_CSUM_IP_SUCCESS_COUNT);
    SPRINTF_RX_COUNTER(RX_CSUM_UNKNOWN_L4_PKT_COUNT);
    SPRINTF_RX_COUNTER(RX_CSUM_UNKNOWN_L3_PKT_COUNT);
    SPRINTF_RX_COUNTER(RX_IP_V6_PKT_CCOUNT);
#endif

	/* *pu4Count = (UINT_32)(pucCurrBuf - pucBuffer); */

    nicRxClearStatistics(prAdapter);

    return;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Read the Response data from data port
*
* @param prAdapter pointer to the Adapter handler
* @param pucRspBuffer pointer to the Response buffer
*
* @retval WLAN_STATUS_SUCCESS: Response packet has been read
* @retval WLAN_STATUS_FAILURE: Read Response packet timeout or error occurred
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
nicRxWaitResponse(IN P_ADAPTER_T prAdapter,
    IN UINT_8 ucPortIdx,
		  OUT PUINT_8 pucRspBuffer, IN UINT_32 u4MaxRespBufferLen, OUT PUINT_32 pu4Length)
{
    UINT_32 u4Value = 0, u4PktLen = 0, i = 0;
    WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
    BOOL fgResult = TRUE;
    UINT_32 u4Time, u4Current;
    P_RX_CTRL_T prRxCtrl;
    P_WIFI_EVENT_T prEvent;

    DEBUGFUNC("nicRxWaitResponse");

    ASSERT(prAdapter);
    ASSERT(pucRspBuffer);
    ASSERT(ucPortIdx < 2);

    prRxCtrl = &prAdapter->rRxCtrl;

	u4Time = (UINT_32) kalGetTimeTick();

    do {
        /* Read the packet length */
        HAL_MCR_RD(prAdapter, MCR_WRPLR, &u4Value);

        if (!fgResult) {
            DBGLOG(RX, ERROR, ("Read Response Packet Error\n"));
            return WLAN_STATUS_FAILURE;
        }

		if (ucPortIdx == 0) {
            u4PktLen = u4Value & 0xFFFF;
		} else {
            u4PktLen = (u4Value >> 16) & 0xFFFF;
        }

        DBGLOG(RX, TRACE, ("i = %lu, u4PktLen = %lu\n", i, u4PktLen));

        if (u4PktLen == 0) {
            /* timeout exceeding check */
			u4Current = (UINT_32) kalGetTimeTick();

			if ((u4Current > u4Time) && ((u4Current - u4Time) > RX_RESPONSE_TIMEOUT)) {
                return WLAN_STATUS_FAILURE;
			} else if (u4Current < u4Time
				   && ((u4Current + (0xFFFFFFFF - u4Time)) > RX_RESPONSE_TIMEOUT)) {
                return WLAN_STATUS_FAILURE;
            }

            /* Response packet is not ready */
            kalUdelay(50);

            i++;
		} else if (u4PktLen > u4MaxRespBufferLen) {
			DBGLOG(RX, WARN,
			       ("Not enough Event Buffer: required length = 0x%lx, available buffer length = %lu\n",
                u4PktLen, u4MaxRespBufferLen));

            return WLAN_STATUS_FAILURE;
		} else {
#if (CFG_ENABLE_READ_EXTRA_4_BYTES == 1)
#if CFG_SDIO_RX_AGG
            HAL_PORT_RD(prAdapter,
                        ucPortIdx == 0 ? MCR_WRDR0 : MCR_WRDR1,
                        ALIGN_4(u4PktLen + 4),
				    prRxCtrl->pucRxCoalescingBufPtr, CFG_RX_COALESCING_BUFFER_SIZE);
            kalMemCopy(pucRspBuffer, prRxCtrl->pucRxCoalescingBufPtr, u4PktLen);
#else
#error "Please turn on RX coalescing"
#endif
#else
            HAL_PORT_RD(prAdapter,
                        ucPortIdx == 0 ? MCR_WRDR0 : MCR_WRDR1,
				    u4PktLen, pucRspBuffer, u4MaxRespBufferLen);
#endif

            /* fgResult will be updated in MACRO */
            if (!fgResult) {
                DBGLOG(RX, ERROR, ("Read Response Packet Error\n"));
                return WLAN_STATUS_FAILURE;
            }

			DBGLOG(RX, TRACE, ("Dump Response buffer, length = 0x%lx\n", u4PktLen));
            DBGLOG_MEM8(RX, TRACE, pucRspBuffer, u4PktLen);

			prEvent = (P_WIFI_EVENT_T) pucRspBuffer;
            DBGLOG(INIT, TRACE, ("RX EVENT: ID[0x%02X] SEQ[%u] LEN[%u]\n", 
                prEvent->ucEID,
					     prEvent->ucSeqNum, prEvent->u2PacketLength));

            *pu4Length = u4PktLen;
            break;
        }
	} while (TRUE);

    return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Set filter to enable Promiscuous Mode
*
* @param prAdapter          Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxEnablePromiscuousMode(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    return;
} /* end of nicRxEnablePromiscuousMode() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Set filter to disable Promiscuous Mode
*
* @param prAdapter  Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicRxDisablePromiscuousMode(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    return;
} /* end of nicRxDisablePromiscuousMode() */


/*----------------------------------------------------------------------------*/
/*!
* @brief this function flushes all packets queued in reordering module
*
* @param prAdapter              Pointer to the Adapter structure.
*
* @retval WLAN_STATUS_SUCCESS   Flushed successfully
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicRxFlush(IN P_ADAPTER_T prAdapter)
{
    P_SW_RFB_T prSwRfb;

    ASSERT(prAdapter);
	prSwRfb = qmFlushRxQueues(prAdapter);
	if (prSwRfb != NULL) {
        do {
            P_SW_RFB_T prNextSwRfb;

			/* save next first */
			prNextSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prSwRfb);

			/* free */
            nicRxReturnRFB(prAdapter, prSwRfb);

            prSwRfb = prNextSwRfb;
		} while (prSwRfb);
    }

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param
*
* @retval
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicRxProcessActionFrame(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb)
{
    P_WLAN_ACTION_FRAME prActFrame;
#if CFG_SUPPORT_802_11W
    BOOL               fgRobustAction = FALSE;
    P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo;
#endif

    ASSERT(prAdapter);
    ASSERT(prSwRfb);

    DBGLOG(RSN, TRACE, ("[Rx] nicRxProcessActionFrame\n"));

    if (prSwRfb->u2PacketLen < sizeof(WLAN_ACTION_FRAME) - 1) {
        return WLAN_STATUS_INVALID_PACKET;
    }
    prActFrame = (P_WLAN_ACTION_FRAME) prSwRfb->pvHeader;

	/* DBGLOG(RSN, TRACE, ("[Rx] nicRxProcessActionFrame\n")); */
    if (!prSwRfb->prStaRec)
		nicRxMgmtNoWTBLHandling(prAdapter, prSwRfb);
#if CFG_SUPPORT_802_11W
    if ((prActFrame->ucCategory <= CATEGORY_PROTECTED_DUAL_OF_PUBLIC_ACTION && 
         prActFrame->ucCategory != CATEGORY_PUBLIC_ACTION && 
         prActFrame->ucCategory != CATEGORY_HT_ACTION) /* At 11W spec Code 7 is reserved */ || 
        (prActFrame->ucCategory == CATEGORY_VENDOR_SPECIFIC_ACTION_PROTECTED)) {
        fgRobustAction = TRUE;
    }
	/* DBGLOG(RSN, TRACE, ("[Rx] fgRobustAction=%d\n", fgRobustAction)); */

    if (fgRobustAction && prSwRfb->prStaRec && 
	    GET_BSS_INFO_BY_INDEX(prAdapter,
				  prSwRfb->prStaRec->ucBssIndex)->eNetworkType ==
	    NETWORK_TYPE_AIS) {
        prAisSpecBssInfo = &(prAdapter->rWifiVar.rAisSpecificBssInfo);

		DBGLOG(RSN, INFO,
		       ("[Rx]RobustAction %x %x %x\n", prSwRfb->prRxStatus->u2StatusFlag,
			prSwRfb->prRxStatus->ucWlanIdx, prSwRfb->prRxStatus->ucTidSecMode));

		if (prAisSpecBssInfo->fgMgmtProtection
		    && (!(prActFrame->u2FrameCtrl & MASK_FC_PROTECTED_FRAME)
			&& (HAL_RX_STATUS_GET_SEC_MODE(prSwRfb->prRxStatus) ==
			    CIPHER_SUITE_CCMP))) {
			DBGLOG(RSN, INFO,
			       ("[MFP] Not handle and drop un-protected robust action frame!!\n"));
            return WLAN_STATUS_INVALID_PACKET;
        }
    }
	/* DBGLOG(RSN, TRACE, ("[Rx] pre check done, handle cateory %d\n", prActFrame->ucCategory)); */
#endif

    switch (prActFrame->ucCategory) {
#if CFG_M0VE_BA_TO_DRIVER
    case CATEGORY_BLOCK_ACK_ACTION:
        DBGLOG(RX, WARN, ("[Puff][%s] Rx CATEGORY_BLOCK_ACK_ACTION\n", __func__));

        if (prSwRfb->prStaRec) {
            mqmHandleBaActionFrame(prAdapter, prSwRfb);
        }

        break;
#endif
    case CATEGORY_PUBLIC_ACTION:
#if 0				/* CFG_SUPPORT_802_11W */
/* Sigma */
#else
		if (prAdapter->prAisBssInfo &&
                prSwRfb->prStaRec &&
                prSwRfb->prStaRec->ucBssIndex == prAdapter->prAisBssInfo->ucBssIndex) {
            aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
        }
#endif

		if (prAdapter->prAisBssInfo &&
        prAdapter->prAisBssInfo->ucBssIndex == KAL_NETWORK_TYPE_AIS_INDEX) {
            aisFuncValidateRxActionFrame(prAdapter, prSwRfb);
    }
#if CFG_ENABLE_WIFI_DIRECT
        if (prAdapter->fgIsP2PRegistered) {
            rlmProcessPublicAction(prAdapter, prSwRfb);

			p2pFuncValidateRxActionFrame(prAdapter, prSwRfb);

        }
#endif
        break;

    case CATEGORY_HT_ACTION:
#if CFG_ENABLE_WIFI_DIRECT
        if (prAdapter->fgIsP2PRegistered) {
            rlmProcessHtAction(prAdapter, prSwRfb);
        }
#endif
        break;
    case CATEGORY_VENDOR_SPECIFIC_ACTION:
#if CFG_ENABLE_WIFI_DIRECT
        if (prAdapter->fgIsP2PRegistered) {
            p2pFuncValidateRxActionFrame(prAdapter, prSwRfb);
        }
#endif
        break;
#if CFG_SUPPORT_802_11W
    case CATEGORY_SA_QUERY_ACTION:
        {
            P_BSS_INFO_T prBssInfo;

            if (prSwRfb->prStaRec) {
				prBssInfo =
				    GET_BSS_INFO_BY_INDEX(prAdapter, prSwRfb->prStaRec->ucBssIndex);
                ASSERT(prBssInfo);
                if ((prBssInfo->eNetworkType == NETWORK_TYPE_AIS) &&
				    prAdapter->rWifiVar.rAisSpecificBssInfo.
				    fgMgmtProtection /* Use MFP */) {
                    /* MFP test plan 5.3.3.4 */
                    rsnSaQueryAction(prAdapter, prSwRfb);
                }
            }
        }
        break;
#endif
#if CFG_SUPPORT_802_11V
    case CATEGORY_WNM_ACTION:
        {
            wnmWNMAction(prAdapter, prSwRfb);
        }
        break;
#endif

#if CFG_SUPPORT_DFS 
	case CATEGORY_SPEC_MGT:
		{
			if (prAdapter->fgEnable5GBand) { 
				DBGLOG(RLM, INFO, ("[Channel Switch]nicRxProcessActionFrame\n"));
				rlmProcessSpecMgtAction(prAdapter, prSwRfb);
			} 
		}
		break;
#endif
    default:
        break;
    } /* end of switch case */


    return WLAN_STATUS_SUCCESS;
}

VOID nicRxMgmtNoWTBLHandling(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb)
{
	/* WTBL error handling. if no WTBL */
	P_WLAN_MAC_MGMT_HEADER_T prMgmtHdr = (P_WLAN_MAC_MGMT_HEADER_T)prSwRfb->pvHeader;

	prSwRfb->ucStaRecIdx = secLookupStaRecIndexFromTA(prAdapter, prMgmtHdr->aucSrcAddr);
	if (prSwRfb->ucStaRecIdx >= CFG_NUM_OF_STA_RECORD)
		return;
	prSwRfb->prStaRec = cnmGetStaRecByIndex(prAdapter, prSwRfb->ucStaRecIdx);

	if (prSwRfb->prStaRec) {
		prSwRfb->ucWlanIdx = prSwRfb->prStaRec->ucWlanIndex;
		DBGLOG(RLM, INFO, ("current wlan index is %d\n"));
	} else {
		DBGLOG(RLM, INFO, ("not find station record base on TA\n"));
		DBGLOG_MEM8(RLM, INFO, (PUINT_8) prSwRfb->prRxStatus,
						prSwRfb->prRxStatus->u2RxByteCount > 12 ? prSwRfb->prRxStatus->u2RxByteCount:12);
        DBGLOG_MEM8(RLM, INFO, prSwRfb->pvHeader, prSwRfb->u2PacketLen);
	}
}
