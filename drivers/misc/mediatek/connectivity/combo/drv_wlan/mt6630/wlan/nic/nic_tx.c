/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_tx.c#2 $
*/

/*! \file   nic_tx.c
    \brief  Functions that provide TX operation in NIC Layer.

    This file provides TX functions which are responsible for both Hardware and
    Software Resource Management and keep their Synchronization.
*/



/*
** $Log: nic_tx.c $
**
** 09 16 2014 eason.tsai
** [ALPS01728937] [Need Patch] [Volunteer Patch] MET support
** support MET
**
** 09 16 2014 eason.tsai
** [ALPS01728937] [Need Patch] [Volunteer Patch] MET support
** MET support
**
** 08 23 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Reset MSDU_INFO for data packet to avoid unexpected Tx status
** 2. Drop Tx packet to non-associated STA in driver
**
** 08 23 2013 yuche.tsai
** [BORA00002761] [MT6630][Wi-Fi Direct][Driver] Group Interface formation
** Bug fix for possible KE.
**
** 08 20 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Reset MSDU_INFO PID field in MSDU set function
**
** 08 19 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Enable TC resource adjust feature
** 2. Set Non-QoS data frame to TC5
**
** 08 13 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Assign TXD.PID by wlan index
** 2. Some bug fix
**
** 08 06 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Set BMC packet retry limit to unlimit
**
** 08 05 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Add SW rate definition
** 2. Add HW default rate selection logic from FW
**
** 08 02 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Set VI/VO Tx life time to no limitation
** 2. Set VI/VO Tx retry limit to 7
**
** 07 31 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Fix NetDev binding issue
**
** 07 26 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Set NoACK to BMC packet
** 2. Add kalGetEthAddr function for Tx frame
** 3. Update RxIndicatePackets
**
** 07 26 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Reduce extra Tx frame header parsing 
** 2. Add TX port control
** 3. Add net interface to BSS binding
**
** 07 19 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** wapi 1x frame don't need encrypt
**
** 07 18 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** .
**
** 07 18 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Update TxDesc PF bit setting rule
** 2. Remove unnecessary QM function
**
** 07 18 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** At nicTxComposeDesc (Mgmt and Data) function, use security setting 
** to decide frame protect or not.
**
** 07 15 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Fix KE at aisFsm when turning off wifi
**
** 07 12 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Update VHT IE composing function
** 2. disable bow
** 3. Exchange bss/sta rec update sequence for temp solution
**
** 07 10 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Disable IP/TCP/UDP checksum temporally for 1st connection
**
** 07 10 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Fix KE at qmEnqueueTxPackets while turning off
** 2. Temp solution for Tx protected data packet
**
** 07 05 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** To let the 1x no PF at tx desc, data with PF, for WPA2-PSK test purpose!!!!!
**
** 07 04 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx path for 1x packet
**
** 07 04 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update for 1st Connection.
**
** 07 02 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** Refine security BMC wlan index assign
** Fix some compiling warning
**
** 06 27 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Refine management frame Tx function
**
** 06 26 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update VHT rate definition
**
** 06 26 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx DESC definition to lateset version
**
** 06 25 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update for 1st connection
**
** 06 18 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update for 1st connection
**
** 03 29 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. remove unused HIF definitions
** 2. enable NDIS 5.1 build success
**
** 03 22 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update MCU queue index from Q0 to Q1
**
** 03 19 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** .
**
** 03 14 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update packet remaining Tx time
**
** 03 12 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx utility function for management frame
**
** 03 05 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** .
**
** 03 04 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** .
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
** 01 21 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update TX path based on new ucBssIndex modifications.
**
** 01 15 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx done resource release mechanism.
**
** 12 27 2012 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update MQM index mapping mechanism
** 1. TID to ACI
** 2. ACI to SW TxQ
** 3. ACI to network TC resource
**
** 12 27 2012 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. remove unused definitions
** 2. correct page count and surpress compiler warning
**
** 12 26 2012 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update TXD format based on latest release.
**
** 12 21 2012 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update TxD template feature.
**
** 12 19 2012 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Add Tx desc composing function
**
** 12 18 2012 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Page count resource management.
**
** 11 01 2012 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** update to MT6630 CMD/EVENT definitions.
** 
** 10 25 2012 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** sync with MT6630 HIFSYS update.
** 
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
** 
** 08 28 2012 cp.wu
** [WCXRP00001270] [MT6620 Wi-Fi][Driver] Fix non-aggregated TX path for experimental purpose
** fix: pucTxCoalescingBufPtr is also used by non-aggregated TX path
 *
 * 06 13 2012 yuche.tsai
 * NULL
 * Update maintrunk driver.
 * Add support for driver compose assoc request frame.
 *
 * 11 18 2011 eddie.chen
 * [WCXRP00001096] [MT6620 Wi-Fi][Driver/FW] Enhance the log function (xlog)
 * Add log counter for tx
 *
 * 11 09 2011 eddie.chen
 * [WCXRP00001096] [MT6620 Wi-Fi][Driver/FW] Enhance the log function (xlog)
 * Add xlog for beacon timeout and sta aging timeout.
 *
 * 11 08 2011 eddie.chen
 * [WCXRP00001096] [MT6620 Wi-Fi][Driver/FW] Enhance the log function (xlog)
 * Add xlog function.
 *
 * 05 17 2011 cp.wu
 * [WCXRP00000732] [MT6620 Wi-Fi][AIS] No need to switch back to IDLE state when DEAUTH frame is dropped due to bss disconnection
 * when TX DONE status is TX_RESULT_DROPPED_IN_DRIVER, no need to switch back to IDLE state.
 *
 * 04 12 2011 eddie.chen
 * [WCXRP00000617] [MT6620 Wi-Fi][DRV/FW] Fix for sigma
 * Fix the sta index in processing security frame
 * Simple flow control for TC4 to avoid mgt frames for PS STA to occupy the TC4
 * Add debug message.
 *
 * 04 12 2011 cp.wu
 * [WCXRP00000631] [MT6620 Wi-Fi][Driver] Add an API for QM to retrieve current TC counter value and processing frame dropping cases for TC4 path
 * remove unused variables.
 *
 * 04 12 2011 cp.wu
 * [WCXRP00000631] [MT6620 Wi-Fi][Driver] Add an API for QM to retrieve current TC counter value and processing frame dropping cases for TC4 path
 * 1. add nicTxGetResource() API for QM to make decisions.
 * 2. if management frames is decided by QM for dropping, the call back is invoked to indicate such a case.
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
 * 02 16 2011 cp.wu
 * [WCXRP00000449] [MT6620 Wi-Fi][Driver] Refine CMD queue handling by adding an extra API for checking availble count and modify behavior
 * 1. add new API: nicTxGetFreeCmdCount()
 * 2. when there is insufficient command descriptor, nicTxEnqueueMsdu() will drop command packets directly
 *
 * 01 24 2011 cp.wu
 * [WCXRP00000382] [MT6620 Wi-Fi][Driver] Track forwarding packet number with notifying tx thread for serving
 * 1. add an extra counter for tracking pending forward frames.
 * 2. notify TX service thread as well when there is pending forward frame
 * 3. correct build errors leaded by introduction of Wi-Fi direct separation module
 *
 * 01 12 2011 cp.wu
 * [WCXRP00000356] [MT6620 Wi-Fi][Driver] fill mac header length for security frames 'cause hardware header translation needs such information
 * fill mac header length information for 802.1x frames.
 *
 * 12 31 2010 cp.wu
 * [WCXRP00000335] [MT6620 Wi-Fi][Driver] change to use milliseconds sleep instead of delay to avoid blocking to system scheduling
 * change to use msleep() and shorten waiting interval to reduce blocking to other task while Wi-Fi driver is being loaded
 *
 * 11 01 2010 yarco.yang
 * [WCXRP00000149] [MT6620 WI-Fi][Driver]Fine tune performance on MT6516 platform
 * Add GPIO debug function
 *
 * 10 18 2010 cp.wu
 * [WCXRP00000117] [MT6620 Wi-Fi][Driver] Add logic for suspending driver when MT6620 is not responding anymore
 * 1. when wlanAdapterStop() failed to send POWER CTRL command to firmware, do not poll for ready bit dis-assertion
 * 2. shorten polling count for shorter response time
 * 3. if bad I/O operation is detected during TX resource polling, then further operation is aborted as well
 *
 * 10 06 2010 cp.wu
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * code reorganization to improve isolation between GLUE and CORE layers.
 *
 * 09 29 2010 wh.su
 * [WCXRP00000072] [MT6620 Wi-Fi][Driver] Fix TKIP Counter Measure EAPoL callback register issue
 * [MT6620 Wi-Fi][Driver] Fix TKIP Counter Measure EAPoL callback register issue.
 *
 * 09 27 2010 wh.su
 * NULL
 * since the u2TxByteCount_UserPriority will or another setting, keep the overall buffer for avoid error
 *
 * 09 24 2010 wh.su
 * NULL
 * [WCXRP000000058][MT6620 Wi-Fi][Driver] Fail to handshake with WAPI AP due the 802.1x frame send to fw with extra bytes padding.
 *
 * 09 01 2010 cp.wu
 * NULL
 * HIFSYS Clock Source Workaround
 *
 * 08 30 2010 cp.wu
 * NULL
 * API added: nicTxPendingPackets(), for simplifying porting layer
 *
 * 08 30 2010 cp.wu
 * NULL
 * eliminate klockwork errors
 *
 * 08 20 2010 wh.su
 * NULL
 * adding the eapol callback setting.
 *
 * 08 18 2010 yarco.yang
 * NULL
 * 1. Fixed HW checksum offload function not work under Linux issue.
 * 2. Add debug message.
 *
 * 08 05 2010 yuche.tsai
 * NULL
 * .
 *
 * 08 03 2010 cp.wu
 * NULL
 * surpress compilation warning.
 *
 * 08 02 2010 jeffrey.chang
 * NULL
 * 1) modify tx service thread to avoid busy looping
 * 2) add spin lock declartion for linux build
 *
 * 07 29 2010 cp.wu
 * NULL
 * simplify post-handling after TX_DONE interrupt is handled.
 *
 * 07 19 2010 jeffrey.chang
 *
 * Linux port modification
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
 * 06 29 2010 yarco.yang
 * [WPD00003837][MT6620]Data Path Refine
 * replace g_rQM with Adpater->rQM
 *
 * 06 25 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * add API in que_mgt to retrieve sta-rec index for security frames.
 *
 * 06 24 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 802.1x and bluetooth-over-Wi-Fi security frames are now delievered to firmware via command path instead of data path.
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
 * add checking for TX descriptor poll.
 *
 * 06 21 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * TX descriptors are now allocated once for reducing allocation overhead
 *
 * 06 18 2010 cm.chang
 * [WPD00003841][LITE Driver] Migrate RLM/CNM to host driver
 * Provide cnmMgtPktAlloc() and alloc/free function of msg/buf
 *
 * 06 15 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * change zero-padding for TX port access to HAL.
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
 * fill extra information for revised HIF_TX_HEADER.
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
 * 06 09 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * add TX_PACKET_MGMT to indicate the frame is coming from management modules
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 05 10 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * fill network type field while doing frame identification.
 *
 * 04 23 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * surpress compiler warning
 *
 * 04 06 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * Tag the packet for QoS on Tx path
 *
 * 03 30 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * remove driver-land statistics.
 *
 * 03 29 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * improve none-glue code portability
 *
 * 03 24 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * initial import for Linux port
 *
 * 03 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * generate information for OID_GEN_RCV_OK & OID_GEN_XMIT_OK
 *  *  *  *  *
 *
* 03 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code clean: removing unused variables and structure definitions
 *
 * 03 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add another spin-lock to protect MsduInfoList due to it might be accessed by different thread.
 *  *  *  * 2) change own-back acquiring procedure to wait for up to 16.67 seconds
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add mutex to avoid multiple access to qmTxQueue simultaneously.
 *
 * 02 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * avoid refering to NDIS-specific data structure directly from non-glue layer.
 *
 * 02 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add Ethernet destination address information in packet info for TX
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove unused function in nic_rx.c [which has been handled in que_mgt.c]
 *  *  *  *  *  * 2) firmware image length is now retrieved via NdisFileOpen
 *  *  *  *  *  * 3) firmware image is not structured by (P_IMG_SEC_HDR_T) anymore
 *  *  *  *  *  * 4) nicRxWaitResponse() revised
 *  *  *  *  *  * 5) another set of TQ counter default value is added for fw-download state
 *  *  *  *  *  * 6) Wi-Fi load address is now retrieved from registry too
 *
 * 02 09 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. Permanent and current MAC address are now retrieved by CMD/EVENT packets instead of hard-coded address
 *  *  *  *  *  *  *  *  * 2. follow MSDN defined behavior when associates to another AP
 *  *  *  *  *  *  *  *  * 3. for firmware download, packet size could be up to 2048 bytes
 *
 * 02 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * prepare for implementing fw download logic
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 *  *  *  *  *  *  *  *  * 2. block TX/ordinary OID when RF test mode is engaged
 *  *  *  *  *  *  *  *  * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 *  *  *  *  *  *  *  *  * 4. correct some HAL implementation
 *
 * 01 13 2010 tehuang.liu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * Enabled the Burst_End Indication mechanism
 *
 * 01 13 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * TX: fill ucWlanHeaderLength/ucPktFormtId_Flags according to info provided by prMsduInfo
 *
 * 12 30 2009 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) According to CMD/EVENT documentation v0.8,
 *  *  *  *  *  *  *  *  *  * OID_CUSTOM_TEST_RX_STATUS & OID_CUSTOM_TEST_TX_STATUS is no longer used,
 *  *  *  *  *  *  *  *  *  * and result is retrieved by get ATInfo instead
 *  *  *  *  *  *  *  *  *  * 2) add 4 counter for recording aggregation statistics
**  \main\maintrunk.MT6620WiFiDriver_Prj\44 2009-12-10 16:52:15 GMT mtk02752
**  remove unused API
**  \main\maintrunk.MT6620WiFiDriver_Prj\43 2009-12-07 22:44:24 GMT mtk02752
**  correct assertion criterion
**  \main\maintrunk.MT6620WiFiDriver_Prj\42 2009-12-07 21:15:52 GMT mtk02752
**  correct trivial mistake
**  \main\maintrunk.MT6620WiFiDriver_Prj\41 2009-12-04 15:47:21 GMT mtk02752
**  + always append a dword of zero on TX path to avoid TX aggregation to triggered on uninitialized data
**  + add more assertion for packet size check
**  \main\maintrunk.MT6620WiFiDriver_Prj\40 2009-12-04 14:51:55 GMT mtk02752
**  nicTxMsduInfo(): save ptr for next entry before attaching to qDataPort
**  \main\maintrunk.MT6620WiFiDriver_Prj\39 2009-12-04 11:54:54 GMT mtk02752
**  add 2 assertion for size check
**  \main\maintrunk.MT6620WiFiDriver_Prj\38 2009-12-03 16:20:35 GMT mtk01461
**  Add debug message
**  \main\maintrunk.MT6620WiFiDriver_Prj\37 2009-11-30 10:57:10 GMT mtk02752
**  1st DW of WIFI_CMD_T is shared with HIF_TX_HEADER_T
**  \main\maintrunk.MT6620WiFiDriver_Prj\36 2009-11-30 09:20:43 GMT mtk02752
**  use TC4 instead of TC5 for command packet
**  \main\maintrunk.MT6620WiFiDriver_Prj\35 2009-11-27 11:08:11 GMT mtk02752
**  add flush for reset
**  \main\maintrunk.MT6620WiFiDriver_Prj\34 2009-11-26 20:31:22 GMT mtk02752
**  fill prMsduInfo->ucUserPriority
**  \main\maintrunk.MT6620WiFiDriver_Prj\33 2009-11-25 21:04:33 GMT mtk02752
**  fill u2SeqNo
**  \main\maintrunk.MT6620WiFiDriver_Prj\32 2009-11-24 20:52:12 GMT mtk02752
**  integration with SD1's data path API
**  \main\maintrunk.MT6620WiFiDriver_Prj\31 2009-11-24 19:54:25 GMT mtk02752
**  nicTxRetransmitOfOsSendQue & nicTxData but changed to use nicTxMsduInfoList
**  \main\maintrunk.MT6620WiFiDriver_Prj\30 2009-11-23 17:53:18 GMT mtk02752
**  add nicTxCmd() for SD1_SD3_DATAPATH_INTEGRATION, which will append only HIF_TX_HEADER. seqNum, WIFI_CMD_T will be created inside oid handler
**  \main\maintrunk.MT6620WiFiDriver_Prj\29 2009-11-20 15:10:24 GMT mtk02752
**  use TxAccquireResource instead of accessing TCQ directly.
**  \main\maintrunk.MT6620WiFiDriver_Prj\28 2009-11-17 22:40:57 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\27 2009-11-17 17:35:40 GMT mtk02752
**  add nicTxMsduInfoList () implementation
**  \main\maintrunk.MT6620WiFiDriver_Prj\26 2009-11-17 11:07:10 GMT mtk02752
**  add nicTxAdjustTcq() implementation
**  \main\maintrunk.MT6620WiFiDriver_Prj\25 2009-11-16 22:28:38 GMT mtk02752
**  move aucFreeBufferCount/aucMaxNumOfBuffer into another structure
**  \main\maintrunk.MT6620WiFiDriver_Prj\24 2009-11-16 21:45:32 GMT mtk02752
**  add SD1_SD3_DATAPATH_INTEGRATION data path handling
**  \main\maintrunk.MT6620WiFiDriver_Prj\23 2009-11-13 13:29:56 GMT mtk01084
**  modify TX hdr format, fix tx retransmission issue
**  \main\maintrunk.MT6620WiFiDriver_Prj\22 2009-11-11 10:36:21 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\21 2009-11-04 14:11:11 GMT mtk01084
**  modify TX SW data structure
**  \main\maintrunk.MT6620WiFiDriver_Prj\20 2009-10-29 19:56:17 GMT mtk01084
**  modify HAL part
**  \main\maintrunk.MT6620WiFiDriver_Prj\19 2009-10-13 21:59:23 GMT mtk01084
**  update for new HW design
**  \main\maintrunk.MT6620WiFiDriver_Prj\18 2009-10-02 14:00:18 GMT mtk01725
**  \main\maintrunk.MT6620WiFiDriver_Prj\17 2009-05-20 12:26:06 GMT mtk01461
**  Assign SeqNum to CMD Packet
**  \main\maintrunk.MT6620WiFiDriver_Prj\16 2009-05-19 10:54:04 GMT mtk01461
**  Add debug message
**  \main\maintrunk.MT6620WiFiDriver_Prj\15 2009-05-12 09:41:55 GMT mtk01461
**  Fix Query Command need resp issue
**  \main\maintrunk.MT6620WiFiDriver_Prj\14 2009-04-29 15:44:38 GMT mtk01461
**  Move OS dependent code to kalQueryTxOOBData()
**  \main\maintrunk.MT6620WiFiDriver_Prj\13 2009-04-28 10:40:03 GMT mtk01461
**  Add nicTxReleaseResource() for SDIO_STATUS_ENHANCE, and also fix the TX aggregation issue for 1x packet to TX1 port
**  \main\maintrunk.MT6620WiFiDriver_Prj\12 2009-04-21 09:50:47 GMT mtk01461
**  Update nicTxCmd() for moving wait RESP function call to wlanSendCommand()
**  \main\maintrunk.MT6620WiFiDriver_Prj\11 2009-04-17 19:56:32 GMT mtk01461
**  Move the CMD_INFO_T related function to cmd_buf.c
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-04-17 18:14:40 GMT mtk01426
**  Update OOB query for TX packet
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-04-14 15:51:32 GMT mtk01426
**  Support PKGUIO
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-04-02 17:26:40 GMT mtk01461
**  Add virtual OOB for HIF LOOPBACK SW PRETEST
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-04-01 10:54:43 GMT mtk01461
**  Add function for SDIO_TX_ENHANCE
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-03-23 21:53:47 GMT mtk01461
**  Add code for retransmit of rOsSendQueue, mpSendPacket(), and add code for TX Checksum offload, Loopback Test.
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-03-23 00:33:51 GMT mtk01461
**  Add code for TX Data & Cmd Packet
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-18 20:25:40 GMT mtk01461
**  Fix LINT warning
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-16 09:10:30 GMT mtk01461
**  Update TX PATH API
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:26:04 GMT mtk01426
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


#ifdef UDP_SKT_WIFI
#include <linux/ftrace_event.h>
#endif

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

static const TX_RESOURCE_CONTROL_T arTcResourceControl[TC_NUM] = {
    /* dest port index, dest queue index,   HIF TX queue index */
    /* First HW queue */
    {PORT_INDEX_LMAC,   MAC_TXQ_AC0_INDEX,  HIF_TX_AC0_INDEX},
    {PORT_INDEX_LMAC,   MAC_TXQ_AC1_INDEX,  HIF_TX_AC1_INDEX},
    {PORT_INDEX_LMAC,   MAC_TXQ_AC2_INDEX,  HIF_TX_AC2_INDEX},
    {PORT_INDEX_LMAC,   MAC_TXQ_AC3_INDEX,  HIF_TX_AC3_INDEX},
    {PORT_INDEX_MCU,    MCU_Q1_INDEX,       HIF_TX_CPU_INDEX},
    {PORT_INDEX_LMAC,   MAC_TXQ_AC4_INDEX,  HIF_TX_AC4_INDEX},

    /* Second HW queue */
#if NIC_TX_ENABLE_SECOND_HW_QUEUE    
    {PORT_INDEX_LMAC,   MAC_TXQ_AC10_INDEX, HIF_TX_AC10_INDEX},
    {PORT_INDEX_LMAC,   MAC_TXQ_AC11_INDEX, HIF_TX_AC11_INDEX},
    {PORT_INDEX_LMAC,   MAC_TXQ_AC12_INDEX, HIF_TX_AC12_INDEX},
    {PORT_INDEX_LMAC,   MAC_TXQ_AC13_INDEX, HIF_TX_AC13_INDEX},
    {PORT_INDEX_LMAC,   MAC_TXQ_AC14_INDEX, HIF_TX_AC14_INDEX},
#endif
};

/* Traffic settings per TC */
static const TX_TC_TRAFFIC_SETTING_T arTcTrafficSettings[NET_TC_NUM] = {
    /* Tx desc template format,                    Remaining Tx time,                               Retry count */
    /* For Data frame with StaRec, set Long Format to enable the following settings */
	{NIC_TX_DESC_SHORT_FORMAT_LENGTH, NIC_TX_AC_BE_REMAINING_TX_TIME,
	 NIC_TX_DATA_DEFAULT_RETRY_COUNT_LIMIT},
	{NIC_TX_DESC_SHORT_FORMAT_LENGTH, NIC_TX_AC_BK_REMAINING_TX_TIME,
	 NIC_TX_DATA_DEFAULT_RETRY_COUNT_LIMIT},
	{NIC_TX_DESC_SHORT_FORMAT_LENGTH, NIC_TX_AC_VI_REMAINING_TX_TIME,
	 NIC_TX_DATA_DEFAULT_RETRY_COUNT_LIMIT},
	{NIC_TX_DESC_SHORT_FORMAT_LENGTH, NIC_TX_AC_VO_REMAINING_TX_TIME,
	 NIC_TX_DATA_DEFAULT_RETRY_COUNT_LIMIT},

    /* MGMT frame */
	{NIC_TX_DESC_LONG_FORMAT_LENGTH, TX_DESC_TX_TIME_NO_LIMIT,
	 NIC_TX_MGMT_DEFAULT_RETRY_COUNT_LIMIT},
    
    /* non-StaRec frame (BMC, etc...) */
	{NIC_TX_DESC_LONG_FORMAT_LENGTH, TX_DESC_TX_TIME_NO_LIMIT,
	 NIC_TX_DATA_DEFAULT_RETRY_COUNT_LIMIT},
};

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will initial all variables in regard to SW TX Queues and
*        all free lists of MSDU_INFO_T and SW_TFCB_T.
*
* @param prAdapter  Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxInitialize(IN P_ADAPTER_T prAdapter)
{
    P_TX_CTRL_T prTxCtrl;
    PUINT_8 pucMemHandle;
    P_MSDU_INFO_T prMsduInfo;
    UINT_32 i;
    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("nicTxInitialize");

    ASSERT(prAdapter);
    prTxCtrl = &prAdapter->rTxCtrl;

	/* 4 <1> Initialization of Traffic Class Queue Parameters */
    nicTxResetResource(prAdapter);

    prTxCtrl->pucTxCoalescingBufPtr = prAdapter->pucCoalescingBufCached;

	/* allocate MSDU_INFO_T and link it into rFreeMsduInfoList */
    QUEUE_INITIALIZE(&prTxCtrl->rFreeMsduInfoList);

    pucMemHandle = prTxCtrl->pucTxCached;
	for (i = 0; i < CFG_TX_MAX_PKT_NUM; i++) {
		prMsduInfo = (P_MSDU_INFO_T) pucMemHandle;
        kalMemZero(prMsduInfo, sizeof(MSDU_INFO_T));

#if CFG_DBG_MGT_BUF
        prMsduInfo->fgIsUsed = FALSE;
        prMsduInfo->rLastAllocTime = kalGetTimeTick();
        prMsduInfo->rLastFreeTime = kalGetTimeTick();
#endif   

        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
		QUEUE_INSERT_TAIL(&prTxCtrl->rFreeMsduInfoList, (P_QUE_ENTRY_T) prMsduInfo);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);

        pucMemHandle += ALIGN_4(sizeof(MSDU_INFO_T));
    }

    ASSERT(prTxCtrl->rFreeMsduInfoList.u4NumElem == CFG_TX_MAX_PKT_NUM);
    /* Check if the memory allocation consist with this initialization function */
	ASSERT((UINT_32) (pucMemHandle - prTxCtrl->pucTxCached) == prTxCtrl->u4TxCachedSize);

    QUEUE_INITIALIZE(&prTxCtrl->rTxMgmtTxingQueue);
    prTxCtrl->i4TxMgmtPendingNum = 0;

#if CFG_HIF_STATISTICS
    prTxCtrl->u4TotalTxAccessNum = 0;
    prTxCtrl->u4TotalTxPacketNum = 0;
#endif

    prTxCtrl->i4PendingFwdFrameCount = 0;

    /* Assign init value */
    /* Tx sequence number */
    prAdapter->ucTxSeqNum = 0;
    /* PID pool */
	for (i = 0; i < WTBL_SIZE; i++) {
        prAdapter->aucPidPool[i] = NIC_TX_DESC_DRIVER_PID_MIN;
    }

    prTxCtrl->u4PageSize = NIC_TX_PAGE_SIZE;

    qmInit(prAdapter);

    TX_RESET_ALL_CNTS(prTxCtrl);
    prTxCtrl->u4ConsecutiveNoResouceCnt = 0;
    return;
} /* end of nicTxInitialize() */

BOOLEAN nicTxSanityCheckResource(IN P_ADAPTER_T prAdapter) 
{
    P_TX_CTRL_T prTxCtrl;
    UINT_8 ucTC;
    UINT_32 ucTotalMaxResource = 0;
    UINT_32 ucTotalFreeResource = 0;
    BOOLEAN fgError = FALSE;

    if(prAdapter->rWifiVar.ucTxDbg & BIT(0)) {
        prTxCtrl = &prAdapter->rTxCtrl;

        prTxCtrl->u4TotalPageNum = 546;

        for(ucTC = TC0_INDEX; ucTC < TC_NUM; ucTC++) {
            ucTotalMaxResource += prTxCtrl->rTc.au2MaxNumOfPage[ucTC];
            ucTotalFreeResource += prTxCtrl->rTc.au2FreePageCount[ucTC];
            
            if(prTxCtrl->rTc.au2FreePageCount[ucTC] > prTxCtrl->u4TotalPageNum) {
                DBGLOG(TX, ERROR, ("%s:%u\n error\n", __FUNCTION__, __LINE__));
                fgError = TRUE;
            }

            if(prTxCtrl->rTc.au2MaxNumOfPage[ucTC] > prTxCtrl->u4TotalPageNum) {
                DBGLOG(TX, ERROR, ("%s:%u\n error\n", __FUNCTION__, __LINE__));
                fgError = TRUE;
            }

            if(prTxCtrl->rTc.au2FreePageCount[ucTC] > prTxCtrl->rTc.au2MaxNumOfPage[ucTC]) {
                DBGLOG(TX, ERROR, ("%s:%u\n error\n", __FUNCTION__, __LINE__));
                fgError = TRUE;
            }
        }

        if(ucTotalMaxResource != prTxCtrl->u4TotalPageNum) {
            DBGLOG(TX, ERROR, ("%s:%u\n error\n", __FUNCTION__, __LINE__));
            fgError = TRUE;
        }

        if(ucTotalMaxResource < ucTotalFreeResource) {
            DBGLOG(TX, ERROR, ("%s:%u\n error\n", __FUNCTION__, __LINE__));
            fgError = TRUE;
        }    

        if(ucTotalFreeResource > prTxCtrl->u4TotalPageNum) {
            DBGLOG(TX, ERROR, ("%s:%u\n error\n", __FUNCTION__, __LINE__));
            fgError = TRUE;
        }     

        if(fgError) {
            DBGLOG(TX, ERROR, ("Total resource[%u]\n", prTxCtrl->u4TotalPageNum));
            qmDumpQueueStatus(prAdapter);
        }
    }

    return !fgError;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will check if has enough TC Buffer for incoming
*        packet and then update the value after promise to provide the resources.
*
* \param[in] prAdapter              Pointer to the Adapter structure.
* \param[in] ucTC                   Specify the resource of TC
*
* \retval WLAN_STATUS_SUCCESS   Resource is available and been assigned.
* \retval WLAN_STATUS_RESOURCES Resource is not available.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxAcquireResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC, IN UINT_8 ucPageCount)
{
    P_TX_CTRL_T prTxCtrl;
    WLAN_STATUS u4Status = WLAN_STATUS_RESOURCES;
    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    prTxCtrl = &prAdapter->rTxCtrl;

    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

    if (prTxCtrl->rTc.au2FreePageCount[ucTC] >= ucPageCount) {

        prTxCtrl->rTc.au2FreePageCount[ucTC] -= ucPageCount;

		prTxCtrl->rTc.au2FreeBufferCount[ucTC] =
		    (prTxCtrl->rTc.au2FreePageCount[ucTC] / NIC_TX_MAX_PAGE_PER_FRAME);

		DBGLOG(TX, EVENT,
		       ("Acquire: TC%d AcquirePageCnt[%u] FreeBufferCnt[%u] FreePageCnt[%u]\n",
			ucTC, ucPageCount, prTxCtrl->rTc.au2FreeBufferCount[ucTC],
			prTxCtrl->rTc.au2FreePageCount[ucTC]));

        u4Status = WLAN_STATUS_SUCCESS;
    }
   
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

    if(u4Status == WLAN_STATUS_RESOURCES) {
        prTxCtrl->u4ConsecutiveNoResouceCnt++;
        DBGLOG(TX, WARN, ("NO Res au2FreePageCount[%d] = %u, NoRes Cnt %u  \n",
			ucTC,
			prTxCtrl->rTc.au2FreePageCount[ucTC],
			prTxCtrl->u4ConsecutiveNoResouceCnt ));
        if(prTxCtrl->u4ConsecutiveNoResouceCnt > NIC_TX_NO_RESOURCE_THRESHOLD) {
            DBGLOG(TX, WARN, ("NO TX Res cnt meets threshold trigger whole chip reset   \n"));
            qmDumpQueueStatus(prAdapter); 
#if CFG_CHIP_RESET_SUPPORT
            glResetTrigger(prAdapter);
#endif
        }
    }
    else {
        prTxCtrl->u4ConsecutiveNoResouceCnt = 0;
    }
    return u4Status;

}				/* end of nicTxAcquireResourceAndTFCBs() */


/*----------------------------------------------------------------------------*/
/*!
* @brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will do polling if FW has return the resource.
*        Used when driver start up before enable interrupt.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param ucTC           Specify the resource of TC
*
* @retval WLAN_STATUS_SUCCESS   Resource is available.
* @retval WLAN_STATUS_FAILURE   Resource is not available.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxPollingResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC)
{
    P_TX_CTRL_T prTxCtrl;
    WLAN_STATUS u4Status = WLAN_STATUS_FAILURE;
    INT_32 i = NIC_TX_RESOURCE_POLLING_TIMEOUT;
    UINT_32 au4WTSR[8];

    ASSERT(prAdapter);
    prTxCtrl = &prAdapter->rTxCtrl;

    if (ucTC >= TC_NUM) {
        return WLAN_STATUS_FAILURE;
    }

    if (prTxCtrl->rTc.au2FreeBufferCount[ucTC] > 0) {
        return WLAN_STATUS_SUCCESS;
    }

    while (i-- > 0) {
        HAL_READ_TX_RELEASED_COUNT(prAdapter, au4WTSR);

		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
            u4Status = WLAN_STATUS_FAILURE;
            break;
		} else if (nicTxReleaseResource(prAdapter, (PUINT_16) au4WTSR)) {
            if (prTxCtrl->rTc.au2FreeBufferCount[ucTC] > 0) {
                u4Status = WLAN_STATUS_SUCCESS;
                break;
			} else {
                kalMsleep(NIC_TX_RESOURCE_POLLING_DELAY_MSEC);
            }
		} else {
            kalMsleep(NIC_TX_RESOURCE_POLLING_DELAY_MSEC);
        }
    }

#if DBG
    {
		INT_32 i4Times = NIC_TX_RESOURCE_POLLING_TIMEOUT - (i + 1);

        if (i4Times) {
            DBGLOG(TX, TRACE, ("Polling MCR_WTSR delay %ld times, %ld msec\n",
					   i4Times,
					   (i4Times * NIC_TX_RESOURCE_POLLING_DELAY_MSEC)));
        }
    }
#endif /* DBG */

    return u4Status;

} /* end of nicTxPollingResource() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will calculate TC page count according to
*        the given TX_STATUS COUNTER after TX Done.
*
* \param[in] prAdapter              Pointer to the Adapter structure.
* \param[in] au2TxRlsCnt           array of TX STATUS
* \param[in] au2FreeTcResource           array of free & avaliable resource count
*
* @return TRUE      there are avaliable resource to release
* @return FALSE     no avaliable resource to release
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
nicTxCalculateResource(IN P_ADAPTER_T prAdapter, IN UINT_16 *au2TxRlsCnt, 
    OUT UINT_16 *au2FreeTcResource)
{
    P_TX_TCQ_STATUS_T prTcqStatus;
    BOOLEAN bStatus = FALSE;
    UINT_8 ucTcIdx;
    UINT_16 u2TotalTxDoneCnt = 0;
    UINT_16 u2TotalExtraTxDone = 0;
    UINT_16 au2UsedCnt[TC_NUM]; 
    UINT_16 au2ExtraTxDone[TC_NUM];
    
    PUINT_16 au2TxDoneCnt;
    PUINT_16 au2PreUsedCnt;
    UINT_16 u2AvaliableCnt;
    BOOLEAN fgEnExtraTxDone;

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    prTcqStatus = &prAdapter->rTxCtrl.rTc;

    au2TxDoneCnt = prTcqStatus->au2TxDonePageCount;
    au2PreUsedCnt = prTcqStatus->au2PreUsedPageCount;
    u2AvaliableCnt = prTcqStatus->u2AvaliablePageCount;
    fgEnExtraTxDone = prAdapter->rWifiVar.ucExtraTxDone;
    
    /* Get used page count */
    if(fgEnExtraTxDone) {
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
        for (ucTcIdx = TC0_INDEX; ucTcIdx < TC_NUM; ucTcIdx++) {
            au2UsedCnt[ucTcIdx] = prTcqStatus->au2MaxNumOfPage[ucTcIdx] - 
                prTcqStatus->au2FreePageCount[ucTcIdx];
        }
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
    }

    /* Get Tx done & avaliable page count */
    u2AvaliableCnt += au2TxRlsCnt[HIF_TX_FFA_INDEX];
    for (ucTcIdx = TC0_INDEX; ucTcIdx < TC_NUM; ucTcIdx++) {

        /* Get Tx done count from Tx interrupt status */
        au2TxDoneCnt[ucTcIdx] += 
            au2TxRlsCnt[arTcResourceControl[ucTcIdx].ucHifTxQIndex];

        /* Get Avaliable EXTRA Tx done */
        if(fgEnExtraTxDone) {
            /* Release Tx done if there are pre-used resource */
            if(au2TxDoneCnt[ucTcIdx] >= au2PreUsedCnt[ucTcIdx]) {
                au2TxDoneCnt[ucTcIdx] -= au2PreUsedCnt[ucTcIdx];
                au2PreUsedCnt[ucTcIdx] = 0;
            }
            else {
                au2PreUsedCnt[ucTcIdx] -= au2TxDoneCnt[ucTcIdx];
                au2TxDoneCnt[ucTcIdx] = 0;
            }
            
            /* Calculate extra Tx done to share rest FFA resource */
            if(au2TxDoneCnt[ucTcIdx] >= au2UsedCnt[ucTcIdx]) {
                au2TxDoneCnt[ucTcIdx] = au2UsedCnt[ucTcIdx];
                au2ExtraTxDone[ucTcIdx] = 0;
            }
            else {
                au2ExtraTxDone[ucTcIdx] = au2UsedCnt[ucTcIdx] - 
                    au2TxDoneCnt[ucTcIdx];
            }
            u2TotalExtraTxDone += au2ExtraTxDone[ucTcIdx];
        }
        
        u2TotalTxDoneCnt += au2TxDoneCnt[ucTcIdx];
        
    }

    DBGLOG(TX, TRACE, ("TxDone result, FFA[%u] AC[%u:%u:%u:%u:%u] CPU[%u]\n", 
        au2TxRlsCnt[HIF_TX_FFA_INDEX], au2TxRlsCnt[HIF_TX_AC0_INDEX],
        au2TxRlsCnt[HIF_TX_AC1_INDEX], au2TxRlsCnt[HIF_TX_AC2_INDEX],
        au2TxRlsCnt[HIF_TX_AC3_INDEX], au2TxRlsCnt[HIF_TX_AC4_INDEX], 
        au2TxRlsCnt[HIF_TX_CPU_INDEX]));
#if 0
	DBGLOG(TX, TRACE, ("Tx Done INT result, AC5/6[%u:%u] BMC/BCN[%u:%u] "
	    "AC10~14[%u:%u:%u:%u:%u]\n",
		au2TxRlsCnt[HIF_TX_AC5_INDEX], au2TxRlsCnt[HIF_TX_AC6_INDEX],
		au2TxRlsCnt[HIF_TX_BMC_INDEX], au2TxRlsCnt[HIF_TX_BCN_INDEX],
		au2TxRlsCnt[HIF_TX_AC10_INDEX], au2TxRlsCnt[HIF_TX_AC11_INDEX],
		au2TxRlsCnt[HIF_TX_AC12_INDEX], au2TxRlsCnt[HIF_TX_AC13_INDEX],
        au2TxRlsCnt[HIF_TX_AC14_INDEX]));
#endif
    DBGLOG(TX, TRACE, ("TxDone Page count, TC[%u:%u:%u:%u:%u:%u]\n", 
        au2TxDoneCnt[TC0_INDEX], au2TxDoneCnt[TC1_INDEX], 
        au2TxDoneCnt[TC2_INDEX], au2TxDoneCnt[TC3_INDEX],
        au2TxDoneCnt[TC4_INDEX], au2TxDoneCnt[TC5_INDEX]));

    /* Calculate free Tc page count */
    if(u2AvaliableCnt && u2TotalTxDoneCnt) {
        /* Distribute resource by Tx done counter */
        if(u2AvaliableCnt >= u2TotalTxDoneCnt) {
            /* Fulfill all TC resource */
			kalMemCopy(au2FreeTcResource, prTcqStatus->au2TxDonePageCount, 
                sizeof(prTcqStatus->au2TxDonePageCount));

			kalMemZero(prTcqStatus->au2TxDonePageCount,
                sizeof(prTcqStatus->au2TxDonePageCount));

            u2AvaliableCnt -= u2TotalTxDoneCnt;
		} else {
            /* Round-robin distribute resource */
            ucTcIdx = prTcqStatus->ucNextTcIdx;
			while (u2AvaliableCnt) {
                /* Enough resource, fulfill this TC */
				if (u2AvaliableCnt >= au2TxDoneCnt[ucTcIdx]) {
					au2FreeTcResource[ucTcIdx] = au2TxDoneCnt[ucTcIdx];
					u2AvaliableCnt -= au2TxDoneCnt[ucTcIdx];
                    au2TxDoneCnt[ucTcIdx] = 0;
                
                    /* Round-robin get next TC */
                    ucTcIdx++;
                    ucTcIdx %= TC_NUM;                    
                }
                /* no more resource, distribute rest of resource to this TC */
                else {
					au2FreeTcResource[ucTcIdx] = u2AvaliableCnt;
					au2TxDoneCnt[ucTcIdx] -= u2AvaliableCnt;
                    u2AvaliableCnt = 0;
                }
            }
            prTcqStatus->ucNextTcIdx = ucTcIdx;
        }
        bStatus = TRUE;
	} 

    if(u2AvaliableCnt && u2TotalExtraTxDone && fgEnExtraTxDone) {
        /* Distribute resource by EXTRA Tx done counter */
        if(u2AvaliableCnt >= u2TotalExtraTxDone) {
            for (ucTcIdx = TC0_INDEX; ucTcIdx < TC_NUM; ucTcIdx++) {
                au2FreeTcResource[ucTcIdx] += au2ExtraTxDone[ucTcIdx];
                au2PreUsedCnt[ucTcIdx] += au2ExtraTxDone[ucTcIdx];
                au2ExtraTxDone[ucTcIdx] = 0;
            }

            u2AvaliableCnt -= u2TotalExtraTxDone;
        }
        else {
            /* Round-robin distribute resource */
            ucTcIdx = prTcqStatus->ucNextTcIdx;
            while(u2AvaliableCnt) {
                /* Enough resource, fulfill this TC */
                if(u2AvaliableCnt >= au2ExtraTxDone[ucTcIdx]) {
                    au2FreeTcResource[ucTcIdx] += au2ExtraTxDone[ucTcIdx];
                    au2PreUsedCnt[ucTcIdx] += au2ExtraTxDone[ucTcIdx];
                    u2AvaliableCnt -= au2ExtraTxDone[ucTcIdx];
                    au2ExtraTxDone[ucTcIdx] = 0;
                
                    /* Round-robin get next TC */
                    ucTcIdx++;
                    ucTcIdx %= TC_NUM;                    
                }
                /* no more resource, distribute rest of resource to this TC */
                else {
                    au2FreeTcResource[ucTcIdx] += u2AvaliableCnt;
                    au2PreUsedCnt[ucTcIdx] += u2AvaliableCnt;
                    au2ExtraTxDone[ucTcIdx] -= u2AvaliableCnt;
                    u2AvaliableCnt = 0;
                }
            }
            prTcqStatus->ucNextTcIdx = ucTcIdx;
        }
        bStatus = TRUE;
    }

    prTcqStatus->u2AvaliablePageCount = u2AvaliableCnt;

    return bStatus;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will release TC Buffer count according to
*        the given TX_STATUS COUNTER after TX Done.
*
* \param[in] prAdapter              Pointer to the Adapter structure.
* \param[in] u4TxStatusCnt          Value of TX STATUS
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN nicTxReleaseResource(IN P_ADAPTER_T prAdapter, IN UINT_16 *au2TxRlsCnt)
{
    P_TX_TCQ_STATUS_T prTcqStatus;
    BOOLEAN bStatus = FALSE;
    UINT_32 i, u4BufferCountToBeFreed;
	UINT_16 au2FreeTcResource[TC_NUM] = { 0 };

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    prTcqStatus = &prAdapter->rTxCtrl.rTc;

    /* Calculate free page count */
    if (nicTxCalculateResource(prAdapter, au2TxRlsCnt, au2FreeTcResource)) {
        
        /* Return free Tc page count */
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
        for (i = TC0_INDEX; i < TC_NUM; i++) {

            /* Real page counter */
            prTcqStatus->au2FreePageCount[i] += au2FreeTcResource[i];
            
            /* Buffer counter. For development only */
            /* Convert page count to buffer count */
			u4BufferCountToBeFreed =
			    (prTcqStatus->au2FreePageCount[i] / NIC_TX_MAX_PAGE_PER_FRAME);
            prTcqStatus->au2FreeBufferCount[i] = u4BufferCountToBeFreed;

			if (au2FreeTcResource[i]) {
				DBGLOG(TX, EVENT,
				       ("Release: TC%lu ReturnPageCnt[%u] FreePageCnt[%u] FreeBufferCnt[%u]\n",
					i, au2FreeTcResource[i], prTcqStatus->au2FreePageCount[i],
                     prTcqStatus->au2FreeBufferCount[i]));
            }
        }
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
        bStatus = TRUE;
    }

    if(!nicTxSanityCheckResource(prAdapter))
        DBGLOG(TX, ERROR, ("Tx Done INT result, FFA[%u] AC[%u:%u:%u:%u:%u] CPU[%u]\n", 
            au2TxRlsCnt[HIF_TX_FFA_INDEX], au2TxRlsCnt[HIF_TX_AC0_INDEX],
            au2TxRlsCnt[HIF_TX_AC1_INDEX], au2TxRlsCnt[HIF_TX_AC2_INDEX],
            au2TxRlsCnt[HIF_TX_AC3_INDEX], au2TxRlsCnt[HIF_TX_AC4_INDEX], 
            au2TxRlsCnt[HIF_TX_CPU_INDEX]));        


	DBGLOG(TX, LOUD,
	       ("TCQ Status Free Page:Buf[%03u:%02u, %03u:%02u, %03u:%02u, %03u:%02u, %03u:%02u, %03u:%02u]\n",
        prTcqStatus->au2FreePageCount[TC0_INDEX],
        prTcqStatus->au2FreeBufferCount[TC0_INDEX],
        prTcqStatus->au2FreePageCount[TC1_INDEX],
        prTcqStatus->au2FreeBufferCount[TC1_INDEX],
        prTcqStatus->au2FreePageCount[TC2_INDEX],
        prTcqStatus->au2FreeBufferCount[TC2_INDEX],
        prTcqStatus->au2FreePageCount[TC3_INDEX],
        prTcqStatus->au2FreeBufferCount[TC3_INDEX],
        prTcqStatus->au2FreePageCount[TC4_INDEX],
        prTcqStatus->au2FreeBufferCount[TC4_INDEX],     
        prTcqStatus->au2FreePageCount[TC5_INDEX],
        prTcqStatus->au2FreeBufferCount[TC5_INDEX]));

    return bStatus;
} /* end of nicTxReleaseResource() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Reset TC Buffer Count to initialized value
*
* \param[in] prAdapter              Pointer to the Adapter structure.
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxResetResource(IN P_ADAPTER_T prAdapter)
{
    P_TX_CTRL_T prTxCtrl;
    UINT_8 ucIdx;

    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("nicTxResetResource");

    ASSERT(prAdapter);
    prTxCtrl = &prAdapter->rTxCtrl;

    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

    /* Delta page count */
    kalMemZero(prTxCtrl->rTc.au2TxDonePageCount, sizeof(prTxCtrl->rTc.au2TxDonePageCount));   
    kalMemZero(prTxCtrl->rTc.au2PreUsedPageCount, sizeof(prTxCtrl->rTc.au2PreUsedPageCount));
    
    prTxCtrl->rTc.ucNextTcIdx = TC0_INDEX;
    prTxCtrl->rTc.u2AvaliablePageCount = 0;

    DBGLOG(TX, INFO, ("Default TCQ free resource [%u %u %u %u %u %u]\n", 
        prAdapter->rWifiVar.au4TcPageCount[TC0_INDEX],
        prAdapter->rWifiVar.au4TcPageCount[TC1_INDEX],
        prAdapter->rWifiVar.au4TcPageCount[TC2_INDEX],
        prAdapter->rWifiVar.au4TcPageCount[TC3_INDEX],
        prAdapter->rWifiVar.au4TcPageCount[TC4_INDEX],
        prAdapter->rWifiVar.au4TcPageCount[TC5_INDEX]));

    prAdapter->rTxCtrl.u4TotalPageNum = 0;
    prAdapter->rTxCtrl.u4TotalTxRsvPageNum = 0;

    for(ucIdx = TC0_INDEX; ucIdx < TC_NUM; ucIdx++) {
        /* Page Count */
        prTxCtrl->rTc.au2MaxNumOfPage[ucIdx] = prAdapter->rWifiVar.au4TcPageCount[ucIdx];
        prTxCtrl->rTc.au2FreePageCount[ucIdx] = prAdapter->rWifiVar.au4TcPageCount[ucIdx];

        DBGLOG(TX, INFO, ("Set TC%u Default[%u] Max[%u] Free[%u]\n", 
            ucIdx,
            prAdapter->rWifiVar.au4TcPageCount[ucIdx],
            prTxCtrl->rTc.au2MaxNumOfPage[ucIdx],
            prTxCtrl->rTc.au2FreePageCount[ucIdx]));

        /* Buffer count */
        prTxCtrl->rTc.au2MaxNumOfBuffer[ucIdx] = (prTxCtrl->rTc.au2MaxNumOfPage[ucIdx] / NIC_TX_MAX_PAGE_PER_FRAME);
        prTxCtrl->rTc.au2FreeBufferCount[ucIdx] = (prTxCtrl->rTc.au2FreePageCount[ucIdx] / NIC_TX_MAX_PAGE_PER_FRAME);

        DBGLOG(TX, INFO, ("Set TC%u Default[%u] Buffer Max[%u] Free[%u]\n", 
            ucIdx,
            prAdapter->rWifiVar.au4TcPageCount[ucIdx],
            prTxCtrl->rTc.au2MaxNumOfBuffer[ucIdx],
            prTxCtrl->rTc.au2FreeBufferCount[ucIdx]));       

        prAdapter->rTxCtrl.u4TotalPageNum += prTxCtrl->rTc.au2MaxNumOfPage[ucIdx];
    }

    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

    DBGLOG(TX, INFO, ("Reset TCQ free resource to Page:Buf [%u:%u %u:%u %u:%u %u:%u %u:%u %u:%u ]\n", 
        prTxCtrl->rTc.au2FreePageCount[TC0_INDEX],
        prTxCtrl->rTc.au2FreeBufferCount[TC0_INDEX],
        prTxCtrl->rTc.au2FreePageCount[TC1_INDEX],
        prTxCtrl->rTc.au2FreeBufferCount[TC1_INDEX],
        prTxCtrl->rTc.au2FreePageCount[TC2_INDEX],
        prTxCtrl->rTc.au2FreeBufferCount[TC2_INDEX],
        prTxCtrl->rTc.au2FreePageCount[TC3_INDEX],
        prTxCtrl->rTc.au2FreeBufferCount[TC3_INDEX],
        prTxCtrl->rTc.au2FreePageCount[TC4_INDEX],
        prTxCtrl->rTc.au2FreeBufferCount[TC4_INDEX],
        prTxCtrl->rTc.au2FreePageCount[TC5_INDEX],
        prTxCtrl->rTc.au2FreeBufferCount[TC5_INDEX]));
    
    DBGLOG(TX, INFO, ("Reset TCQ MAX resource to Page:Buf [%u:%u %u:%u %u:%u %u:%u %u:%u %u:%u]\n", 
        prTxCtrl->rTc.au2MaxNumOfPage[TC0_INDEX],
        prTxCtrl->rTc.au2MaxNumOfBuffer[TC0_INDEX],
        prTxCtrl->rTc.au2MaxNumOfPage[TC1_INDEX],
        prTxCtrl->rTc.au2MaxNumOfBuffer[TC1_INDEX],
        prTxCtrl->rTc.au2MaxNumOfPage[TC2_INDEX],
        prTxCtrl->rTc.au2MaxNumOfBuffer[TC2_INDEX],
        prTxCtrl->rTc.au2MaxNumOfPage[TC3_INDEX],
        prTxCtrl->rTc.au2MaxNumOfBuffer[TC3_INDEX],
        prTxCtrl->rTc.au2MaxNumOfPage[TC4_INDEX],
        prTxCtrl->rTc.au2MaxNumOfBuffer[TC4_INDEX],
        prTxCtrl->rTc.au2MaxNumOfPage[TC5_INDEX],
        prTxCtrl->rTc.au2MaxNumOfBuffer[TC5_INDEX]));    

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Driver maintain a variable that is synchronous with the usage of individual
*        TC Buffer Count. This function will return the value for other component
*        which needs this information for making decisions
*
* @param prAdapter      Pointer to the Adapter structure.
* @param ucTC           Specify the resource of TC
*
* @retval UINT_8        The number of corresponding TC number
*/
/*----------------------------------------------------------------------------*/
UINT_16 nicTxGetResource(IN P_ADAPTER_T prAdapter, IN UINT_8 ucTC)
{
    P_TX_CTRL_T prTxCtrl;

    ASSERT(prAdapter);
    prTxCtrl = &prAdapter->rTxCtrl;

    ASSERT(prTxCtrl);

    if (ucTC >= TC_NUM) {
        return 0;
	} else {
        return prTxCtrl->rTc.au2FreePageCount[ucTC];
    }
}

UINT_8 nicTxGetFrameResourceType(IN UINT_8 eFrameType, IN P_MSDU_INFO_T prMsduInfo)
{
    UINT_8 ucTC;

	switch (eFrameType) {
    case FRAME_TYPE_802_1X:
        ucTC = TC4_INDEX;
        break;

    case FRAME_TYPE_MMPDU:
		if (prMsduInfo) {
            ucTC = prMsduInfo->ucTC;
		} else {
            ucTC = TC4_INDEX;
        }
    break;

    default:
        DBGLOG(INIT, WARN, ("Undefined Frame Type(%u)\n", eFrameType));
        ucTC = TC4_INDEX;
        break;
    }

    return ucTC;    
}

UINT_8 nicTxGetCmdResourceType(IN P_CMD_INFO_T prCmdInfo)
{
    UINT_8 ucTC;

	switch (prCmdInfo->eCmdType) {
        case COMMAND_TYPE_NETWORK_IOCTL:
        case COMMAND_TYPE_GENERAL_IOCTL:
            ucTC = TC4_INDEX;
            break;
            
        case COMMAND_TYPE_SECURITY_FRAME:
            ucTC = nicTxGetFrameResourceType(FRAME_TYPE_802_1X, NULL);
            break;
            
        case COMMAND_TYPE_MANAGEMENT_FRAME:
			ucTC = nicTxGetFrameResourceType(FRAME_TYPE_MMPDU, 
                        prCmdInfo->prMsduInfo);
            break;

        default:
            DBGLOG(INIT, WARN, ("Undefined CMD Type(%u)\n", prCmdInfo->eCmdType));
            ucTC = TC4_INDEX;
            break;
    }

    return ucTC;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll aggregate frame(PACKET_INFO_T)
* corresponding to HIF TX port
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfoListHead     a link list of P_MSDU_INFO_T
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxMsduInfoList(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
    P_MSDU_INFO_T prMsduInfo, prNextMsduInfo;
    QUE_T qDataPort0, qDataPort1;
    P_QUE_T prDataPort0, prDataPort1;
    WLAN_STATUS status;

    ASSERT(prAdapter);
    ASSERT(prMsduInfoListHead);

    prMsduInfo = prMsduInfoListHead;

    prDataPort0 = &qDataPort0;
    prDataPort1 = &qDataPort1;

    QUEUE_INITIALIZE(prDataPort0);
    QUEUE_INITIALIZE(prDataPort1);

	/* Separate MSDU_INFO_T lists into 2 categories: for Port#0 & Port#1 */
	while (prMsduInfo) {
		prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);

		switch (prMsduInfo->ucTC) {
        case TC0_INDEX:
        case TC1_INDEX:
        case TC2_INDEX:
        case TC3_INDEX:
		case TC5_INDEX:	/* Broadcast/multicast data packets */
			QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo) = NULL;
			QUEUE_INSERT_TAIL(prDataPort0, (P_QUE_ENTRY_T) prMsduInfo);
			status =
			    nicTxAcquireResource(prAdapter, prMsduInfo->ucTC,
						 nicTxGetPageCount(prMsduInfo->u2FrameLength,
								   FALSE));
			ASSERT(status == WLAN_STATUS_SUCCESS);

            break;

		case TC4_INDEX:	/* Management packets */
			QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo) = NULL;
			QUEUE_INSERT_TAIL(prDataPort1, (P_QUE_ENTRY_T) prMsduInfo);

			status =
			    nicTxAcquireResource(prAdapter, prMsduInfo->ucTC,
						 nicTxGetPageCount(prMsduInfo->u2FrameLength,
								   FALSE));
			ASSERT(status == WLAN_STATUS_SUCCESS);

            break;

        default:
            ASSERT(0);
            break;
        }

        prMsduInfo = prNextMsduInfo;
    }

	if (prDataPort0->u4NumElem > 0) {
        nicTxMsduQueue(prAdapter, 0, prDataPort0);
    }

	if (prDataPort1->u4NumElem > 0) {
        nicTxMsduQueue(prAdapter, 1, prDataPort1);
    }

    return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_MULTITHREAD
/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll aggregate frame(PACKET_INFO_T)
* corresponding to HIF TX port
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfoListHead     a link list of P_MSDU_INFO_T
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxMsduInfoListMthread(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
    P_MSDU_INFO_T prMsduInfo, prNextMsduInfo;
    QUE_T qDataPort0, qDataPort1;
    P_QUE_T prDataPort0, prDataPort1;

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    ASSERT(prMsduInfoListHead);

    prMsduInfo = prMsduInfoListHead;

    prDataPort0 = &qDataPort0;
    prDataPort1 = &qDataPort1;

    QUEUE_INITIALIZE(prDataPort0);
    QUEUE_INITIALIZE(prDataPort1);

	/* Separate MSDU_INFO_T lists into 2 categories: for Port#0 & Port#1 */
	while (prMsduInfo) {
		prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);

		switch (prMsduInfo->ucTC) {
        case TC0_INDEX:
        case TC1_INDEX:
        case TC2_INDEX:
        case TC3_INDEX:
		case TC5_INDEX:	/* Broadcast/multicast data packets */
			QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo) = NULL;
			QUEUE_INSERT_TAIL(prDataPort0, (P_QUE_ENTRY_T) prMsduInfo);
            break;

		case TC4_INDEX:	/* Management packets */
			QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo) = NULL;
			QUEUE_INSERT_TAIL(prDataPort1, (P_QUE_ENTRY_T) prMsduInfo);
            break;

        default:
            ASSERT(0);
            break;
        }
        nicTxFillDesc(prAdapter, prMsduInfo, prMsduInfo->aucTxDescBuffer, NULL);

        prMsduInfo = prNextMsduInfo;
    }

	if (prDataPort0->u4NumElem > 0 || prDataPort1->u4NumElem > 0) {

        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);
        QUEUE_CONCATENATE_QUEUES((&(prAdapter->rTxP0Queue)), (prDataPort0));
        QUEUE_CONCATENATE_QUEUES((&(prAdapter->rTxP1Queue)), (prDataPort1));
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);

        kalSetTxEvent2Hif(prAdapter->prGlueInfo);
    }

    return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll write frame(PACKET_INFO_T) into HIF when apply multithread.
*
* @param prAdapter              Pointer to the Adapter structure.
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
UINT_32 nicTxMsduQueueMthread(IN P_ADAPTER_T prAdapter)
{
    QUE_T qDataPort0, qDataPort1;
    P_QUE_T prDataPort0, prDataPort1;
    UINT_32 u4TxLoopCount;
    
    KAL_SPIN_LOCK_DECLARATION();

    prDataPort0 = &qDataPort0;
    prDataPort1 = &qDataPort1;

    QUEUE_INITIALIZE(prDataPort0);
    QUEUE_INITIALIZE(prDataPort1);
    
    u4TxLoopCount = prAdapter->rWifiVar.u4HifTxloopCount;

    while(u4TxLoopCount--) {

        while(QUEUE_IS_NOT_EMPTY((&(prAdapter->rTxP0Queue)))) {
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);
    QUEUE_MOVE_ALL((prDataPort0), (&(prAdapter->rTxP0Queue)));
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);

        nicTxMsduQueue(prAdapter, 0, prDataPort0);
    }

        while(QUEUE_IS_NOT_EMPTY((&(prAdapter->rTxP1Queue)))) {
            KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);
            QUEUE_MOVE_ALL((prDataPort1), (&(prAdapter->rTxP1Queue)));
            KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);

            nicTxMsduQueue(prAdapter, 1, prDataPort1);
    }
    }
    
    return WLAN_STATUS_SUCCESS;
}
#endif


/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll compose the Tx descriptor of the MSDU.
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfo             Pointer to the Msdu info
* @param prTxDesc               Pointer to the Tx descriptor buffer
*
* @retval VOID
*/
/*----------------------------------------------------------------------------*/
VOID
nicTxComposeDesc(IN P_ADAPTER_T prAdapter,
    IN P_MSDU_INFO_T        prMsduInfo,
		 IN UINT_8 ucTxDescLength, IN BOOLEAN fgIsTemplate, OUT PUINT_8 prTxDescBuffer)
{
    P_HW_MAC_TX_DESC_T prTxDesc;
    P_STA_RECORD_T prStaRec;
    P_BSS_INFO_T prBssInfo;
    UINT_8 ucEtherTypeOffsetInWord;
    UINT_8 ucTxDescAndPaddingLength;

	prTxDesc = (P_HW_MAC_TX_DESC_T) prTxDescBuffer;
    prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);
    prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

    ucTxDescAndPaddingLength = ucTxDescLength + NIC_TX_DESC_PADDING_LENGTH;
    
    kalMemZero(prTxDesc, ucTxDescAndPaddingLength);

    /* Move to nicTxFillDesc */
	/* Tx byte count */
	/* HAL_MAC_TX_DESC_SET_TX_BYTE_COUNT(prTxDesc, ucTxDescAndPaddingLength + prMsduInfo->u2FrameLength); */

	/* Ether-type offset */
    if (prMsduInfo->fgIs802_11) {
        ucEtherTypeOffsetInWord =
		    (NIC_TX_DESC_AND_PADDING_LENGTH + prMsduInfo->ucMacHeaderLength +
		     prMsduInfo->ucLlcLength) >> 1;
	} else {
        ucEtherTypeOffsetInWord =
            ((ETHER_HEADER_LEN - ETHER_TYPE_LEN) + ucTxDescAndPaddingLength) >> 1;
    }    
    HAL_MAC_TX_DESC_SET_ETHER_TYPE_OFFSET(prTxDesc, ucEtherTypeOffsetInWord);

	/* Port index / queue index */
	HAL_MAC_TX_DESC_SET_PORT_INDEX(prTxDesc,
				       arTcResourceControl[prMsduInfo->ucTC].ucDestPortIndex);
	HAL_MAC_TX_DESC_SET_QUEUE_INDEX(prTxDesc,
					arTcResourceControl[prMsduInfo->ucTC].ucDestQueueIndex);

    /* BMC packet */
	if (prMsduInfo->ucStaRecIndex == STA_REC_INDEX_BMCAST) {
        HAL_MAC_TX_DESC_SET_BMC(prTxDesc);
        
        /* Must set No ACK to mask retry bit in FC */
        HAL_MAC_TX_DESC_SET_NO_ACK(prTxDesc);            
    }
	/* WLAN index */
	prMsduInfo->ucWlanIndex =
	    nicTxGetWlanIdx(prAdapter, prMsduInfo->ucBssIndex, prMsduInfo->ucStaRecIndex);

#if DBG
	DBGLOG(RSN, TRACE,
	       ("Tx WlanIndex = %d eAuthMode = %d\n", prMsduInfo->ucWlanIndex,
		prAdapter->rWifiVar.rConnSettings.eAuthMode));
#endif    
    HAL_MAC_TX_DESC_SET_WLAN_INDEX(prTxDesc, prMsduInfo->ucWlanIndex);

	/* Header format */
    if (prMsduInfo->fgIs802_11) {
        HAL_MAC_TX_DESC_SET_HEADER_FORMAT(prTxDesc, HEADER_FORMAT_802_11_NORMAL_MODE);
		HAL_MAC_TX_DESC_SET_802_11_HEADER_LENGTH(prTxDesc,
							 (prMsduInfo->ucMacHeaderLength >> 1));
	} else {
        HAL_MAC_TX_DESC_SET_HEADER_FORMAT(prTxDesc, HEADER_FORMAT_NON_802_11);
        HAL_MAC_TX_DESC_SET_ETHERNET_II(prTxDesc);
    }

	/* Header Padding */
    HAL_MAC_TX_DESC_SET_HEADER_PADDING(prTxDesc, NIC_TX_DESC_HEADER_PADDING_LENGTH);
    
	/* TID */
    HAL_MAC_TX_DESC_SET_TID(prTxDesc, prMsduInfo->ucUserPriority);

	/* Protection */
	if (secIsProtectedFrame(prAdapter, prMsduInfo, prStaRec)) {
        /* Update Packet option, PF bit will be set in nicTxFillDescByPktOption() */
        nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME, TRUE);
    }
	/* Own MAC */
    HAL_MAC_TX_DESC_SET_OWN_MAC_INDEX(prTxDesc, prBssInfo->ucOwnMacIndex);

	if (ucTxDescLength == NIC_TX_DESC_SHORT_FORMAT_LENGTH) {
        HAL_MAC_TX_DESC_SET_SHORT_FORMAT(prTxDesc);
        
		/* Update Packet option */
        nicTxFillDescByPktOption(prMsduInfo, prTxDesc);
        
        /* Short format, Skip DW 2~6 */
        return;
	} else {
        HAL_MAC_TX_DESC_SET_LONG_FORMAT(prTxDesc);
        
		/* Update Packet option */
        nicTxFillDescByPktOption(prMsduInfo, prTxDesc);   

        nicTxFillDescByPktControl(prMsduInfo, prTxDesc);
    }

	/* Type */
    if (prMsduInfo->fgIs802_11) {
        P_WLAN_MAC_HEADER_T prWlanHeader = 
		    (P_WLAN_MAC_HEADER_T) ((ULONG) (prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);
                    
        HAL_MAC_TX_DESC_SET_TYPE(prTxDesc, (prWlanHeader->u2FrameCtrl & MASK_FC_TYPE) >> 2);
		HAL_MAC_TX_DESC_SET_SUB_TYPE(prTxDesc,
					     (prWlanHeader->
					      u2FrameCtrl & MASK_FC_SUBTYPE) >>
					     OFFSET_OF_FC_SUBTYPE);
    }   
	/* PID */
	if (prMsduInfo->pfTxDoneHandler) {
        prMsduInfo->ucPID = nicTxAssignPID(prAdapter, prMsduInfo->ucWlanIndex);
        HAL_MAC_TX_DESC_SET_PID(prTxDesc, prMsduInfo->ucPID);
        HAL_MAC_TX_DESC_SET_TXS_TO_MCU(prTxDesc);
    }
	/* Remaining TX time */
	if (!(prMsduInfo->u4Option & MSDU_OPT_MANUAL_LIFE_TIME)) {
		prMsduInfo->u4RemainingLifetime =
		    arTcTrafficSettings[prMsduInfo->ucTC].u4RemainingTxTime;
    }
    HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME_IN_MS(prTxDesc, prMsduInfo->u4RemainingLifetime);

	/* Tx count limit */
	if (!(prMsduInfo->u4Option & MSDU_OPT_MANUAL_RETRY_LIMIT)) {
        /* Note: BMC packet retry limit is set to unlimited */
        prMsduInfo->ucRetryLimit = arTcTrafficSettings[prMsduInfo->ucTC].ucTxCountLimit;
    }      
    HAL_MAC_TX_DESC_SET_REMAINING_TX_COUNT(prTxDesc, prMsduInfo->ucRetryLimit);

	/* Power Offset */
    HAL_MAC_TX_DESC_SET_POWER_OFFSET(prTxDesc, prMsduInfo->cPowerOffset);

	/* Fix rate */
	switch (prMsduInfo->ucRateMode) {
        case MSDU_RATE_MODE_MANUAL_DESC:
            HAL_MAC_TX_DESC_SET_DW(prTxDesc, 6, 1, &prMsduInfo->u4FixedRateOption);
            HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_DESC(prTxDesc);
            HAL_MAC_TX_DESC_SET_FIXED_RATE_ENABLE(prTxDesc);
            break;
            
        case MSDU_RATE_MODE_MANUAL_CR:
            HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_CR(prTxDesc);
            HAL_MAC_TX_DESC_SET_FIXED_RATE_ENABLE(prTxDesc);
            break;
            
        case MSDU_RATE_MODE_AUTO:
        default:
            break;
    }
    
}

VOID
nicTxComposeSecurityFrameDesc(IN P_ADAPTER_T prAdapter,
	IN P_CMD_INFO_T prCmdInfo, OUT PUINT_8 prTxDescBuffer, 
	OUT PUINT_8 pucTxDescLength)
{
	P_HW_MAC_TX_DESC_T prTxDesc = (P_HW_MAC_TX_DESC_T)prTxDescBuffer;
	UINT_8 ucTxDescAndPaddingLength =
		NIC_TX_DESC_LONG_FORMAT_LENGTH + NIC_TX_DESC_PADDING_LENGTH;

	//P_STA_RECORD_T prStaRec = cnmGetStaRecByIndex(prAdapter, prCmdInfo->ucStaRecIndex);
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucTid = 0;
	UINT_8 ucTempTC = TC4_INDEX;
	P_NATIVE_PACKET prNativePacket;
	UINT_8 ucEtherTypeOffsetInWord;
	P_MSDU_INFO_T prMsduInfo;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prCmdInfo->ucBssIndex);
	prMsduInfo = prCmdInfo->prMsduInfo;
	prNativePacket = prMsduInfo->prPacket;

	ASSERT(prNativePacket);

	kalMemZero(prTxDesc, ucTxDescAndPaddingLength);

	/* WLAN index */
	prMsduInfo->ucWlanIndex = nicTxGetWlanIdx(prAdapter, prMsduInfo->ucBssIndex, 
		prMsduInfo->ucStaRecIndex);

	/* UC to a connected peer */
	HAL_MAC_TX_DESC_SET_WLAN_INDEX(prTxDesc, prMsduInfo->ucWlanIndex);
	/* Redirect Security frame to TID0 */
	/* ucTempTC = arNetwork2TcResource[prStaRec->ucBssIndex][aucTid2ACI[ucTid]]; */
		
	/* Tx byte count */
	HAL_MAC_TX_DESC_SET_TX_BYTE_COUNT(prTxDesc,
					  ucTxDescAndPaddingLength + prCmdInfo->u2InfoBufLen);
	
	/* Ether-type offset */
	ucEtherTypeOffsetInWord =
		((ETHER_HEADER_LEN - ETHER_TYPE_LEN) + ucTxDescAndPaddingLength) >> 1; 
	HAL_MAC_TX_DESC_SET_ETHER_TYPE_OFFSET(prTxDesc, ucEtherTypeOffsetInWord);	
	
	/* Port index / queue index */
	HAL_MAC_TX_DESC_SET_PORT_INDEX(prTxDesc, arTcResourceControl[ucTempTC].ucDestPortIndex);
	HAL_MAC_TX_DESC_SET_QUEUE_INDEX(prTxDesc, arTcResourceControl[ucTempTC].ucDestQueueIndex);

	/* Header format */
	HAL_MAC_TX_DESC_SET_HEADER_FORMAT(prTxDesc, HEADER_FORMAT_NON_802_11);

	/* Long Format */
	HAL_MAC_TX_DESC_SET_LONG_FORMAT(prTxDesc);

	/* Update Packet option */
	nicTxFillDescByPktOption(prMsduInfo, prTxDesc);

	if (!GLUE_TEST_PKT_FLAG(prNativePacket, ENUM_PKT_802_3)) {
		/* Set EthernetII */
		HAL_MAC_TX_DESC_SET_ETHERNET_II(prTxDesc);
	}
	/* Header Padding */
	HAL_MAC_TX_DESC_SET_HEADER_PADDING(prTxDesc, NIC_TX_DESC_HEADER_PADDING_LENGTH);
  
	/* TID */
	HAL_MAC_TX_DESC_SET_TID(prTxDesc, ucTid);

	/* Remaining TX time */
	HAL_MAC_TX_DESC_SET_REMAINING_LIFE_TIME_IN_MS(prTxDesc,
		arTcTrafficSettings[ucTempTC].u4RemainingTxTime);

	/* Tx count limit */
	HAL_MAC_TX_DESC_SET_REMAINING_TX_COUNT(prTxDesc,
		arTcTrafficSettings[ucTempTC].ucTxCountLimit);

	/* Set lowest BSS basic rate */
	HAL_MAC_TX_DESC_SET_FR_RATE(prTxDesc, prBssInfo->u2HwDefaultFixedRateCode);
	HAL_MAC_TX_DESC_SET_FIXED_RATE_MODE_TO_DESC(prTxDesc);
	HAL_MAC_TX_DESC_SET_FIXED_RATE_ENABLE(prTxDesc);
	
	/* Own MAC */
	HAL_MAC_TX_DESC_SET_OWN_MAC_INDEX(prTxDesc, prBssInfo->ucOwnMacIndex); 

	/* PID */
	if (prMsduInfo->pfTxDoneHandler) {
		prMsduInfo->ucPID = nicTxAssignPID(prAdapter, prMsduInfo->ucWlanIndex);
		HAL_MAC_TX_DESC_SET_PID(prTxDesc, prMsduInfo->ucPID);
		HAL_MAC_TX_DESC_SET_TXS_TO_MCU(prTxDesc);
	}

	if (pucTxDescLength) {
		*pucTxDescLength = ucTxDescAndPaddingLength;
	}
}
BOOLEAN
nicTxIsTXDTemplateAllowd(IN P_ADAPTER_T prAdapter, 
    IN P_MSDU_INFO_T prMsduInfo, IN P_STA_RECORD_T prStaRec){

    if(prMsduInfo->fgIsTXDTemplateValid && (prMsduInfo->ucControlFlag == 0) 
        && prStaRec && !prMsduInfo->pfTxDoneHandler) {

        return TRUE;
    }
    else {
        return FALSE;
    }
}

/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll compose the Tx descriptor of the MSDU.
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfo             Pointer to the Msdu info
* @param prTxDesc               Pointer to the Tx descriptor buffer
*
* @retval VOID
*/
/*----------------------------------------------------------------------------*/
VOID
nicTxFillDesc(IN P_ADAPTER_T prAdapter,
	      IN P_MSDU_INFO_T prMsduInfo, OUT PUINT_8 prTxDescBuffer, OUT PUINT_8 pucTxDescLength)
{
	P_HW_MAC_TX_DESC_T prTxDesc = (P_HW_MAC_TX_DESC_T) prTxDescBuffer;
    P_HW_MAC_TX_DESC_T prTxDescTemplate = NULL;
    P_STA_RECORD_T prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);
    UINT_8 ucTxDescLength;
#if CFG_TCP_IP_CHKSUM_OFFLOAD
    UINT_8 ucChksumFlag = 0;
#endif

/*
*------------------------------------------------------------------------------
* Fill up common fileds
*------------------------------------------------------------------------------
*/
    /* Get TXD from pre-allocated template */
	if (nicTxIsTXDTemplateAllowd(prAdapter, prMsduInfo, prStaRec)) {
        prTxDescTemplate = prStaRec->aprTxDescTemplate[prMsduInfo->ucUserPriority];
    
		if (HAL_MAC_TX_DESC_IS_LONG_FORMAT(prTxDescTemplate)) {
            ucTxDescLength = NIC_TX_DESC_LONG_FORMAT_LENGTH;
		} else {
            ucTxDescLength = NIC_TX_DESC_SHORT_FORMAT_LENGTH;
        }

		kalMemCopy(prTxDesc, prTxDescTemplate, ucTxDescLength);

        /* Overwrite fields for EOSP or More data */
        nicTxFillDescByPktOption(prMsduInfo, prTxDesc);
    }
    /* Compose TXD by Msdu info */
    else {
        ucTxDescLength = NIC_TX_DESC_LONG_FORMAT_LENGTH;
        nicTxComposeDesc(prAdapter, prMsduInfo, ucTxDescLength, FALSE, prTxDescBuffer);
    }
    
/*
*------------------------------------------------------------------------------
* Fill up remaining parts, per-packet variant fields
*------------------------------------------------------------------------------
*/
    /* Calculate Tx byte count */
	HAL_MAC_TX_DESC_SET_TX_BYTE_COUNT(prTxDesc,
					  ucTxDescLength + NIC_TX_DESC_PADDING_LENGTH +
					  prMsduInfo->u2FrameLength);

    /* Checksum offload */
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	if (prMsduInfo->eSrc == TX_PACKET_OS) {
		if (prAdapter->u4CSUMFlags &
		    (CSUM_OFFLOAD_EN_TX_TCP | CSUM_OFFLOAD_EN_TX_UDP | CSUM_OFFLOAD_EN_TX_IP)) {
            ASSERT(prMsduInfo->prPacket);                 
            kalQueryTxChksumOffloadParam(prMsduInfo->prPacket, &ucChksumFlag);
			if ((ucChksumFlag & TX_CS_IP_GEN)) {
                HAL_MAC_TX_DESC_SET_IP_CHKSUM(prTxDesc);
            }
			if ((ucChksumFlag & TX_CS_TCP_UDP_GEN)) {
                HAL_MAC_TX_DESC_SET_TCP_UDP_CHKSUM(prTxDesc);
            }               
        }
    }
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */    

    /* Set EtherType & VLAN for non 802.11 frame */
	if (!prMsduInfo->fgIs802_11) {
		if (prMsduInfo->fgIs802_3) {
            HAL_MAC_TX_DESC_UNSET_ETHERNET_II(prTxDesc);
        }
		if (prMsduInfo->fgIsVlanExists) {
            HAL_MAC_TX_DESC_SET_VLAN(prTxDesc);
        }
    }

	if (pucTxDescLength) {
        *pucTxDescLength = ucTxDescLength;
    }
}

VOID
nicTxCopyDesc(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucTarTxDesc, 
	IN PUINT_8 pucSrcTxDesc, OUT PUINT_8 pucTxDescLength)
{
	UINT_8 ucTxDescLength;

	if (HAL_MAC_TX_DESC_IS_LONG_FORMAT((P_HW_MAC_TX_DESC_T)pucSrcTxDesc)) {
		ucTxDescLength = NIC_TX_DESC_LONG_FORMAT_LENGTH;
	} else {
		ucTxDescLength = NIC_TX_DESC_SHORT_FORMAT_LENGTH;
	}
	
	kalMemCopy(pucTarTxDesc, pucSrcTxDesc, ucTxDescLength);

	if (pucTxDescLength) {
		*pucTxDescLength = ucTxDescLength;
	}
}


/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll generate Tx descriptor template for each TID.
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prStaRec              Pointer to the StaRec structure.
*
* @retval VOID
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxGenerateDescTemplate(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
    UINT_8  ucTid;
    UINT_8  ucTc;
    UINT_8  ucTxDescSize;
    P_HW_MAC_TX_DESC_T  prTxDesc;
    P_MSDU_INFO_T prMsduInfo;
    WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
    ASSERT(prAdapter);

    /* Free previous template, first */
	/* nicTxFreeDescTemplate(prAdapter, prStaRec); */
	for (ucTid = 0; ucTid < TX_DESC_TID_NUM; ucTid++) {
        prStaRec->aprTxDescTemplate[ucTid] = NULL;
    }

    prMsduInfo = cnmPktAlloc(prAdapter, 0);

	if (!prMsduInfo) {
        return WLAN_STATUS_RESOURCES;
    }

    /* Fill up MsduInfo template */  
    prMsduInfo->eSrc = TX_PACKET_OS;
    prMsduInfo->fgIs802_11 = FALSE;
    prMsduInfo->fgIs802_1x = FALSE;
    prMsduInfo->fgIs802_3 = FALSE;
    prMsduInfo->fgIsVlanExists = FALSE;
    prMsduInfo->pfTxDoneHandler = NULL;
    prMsduInfo->prPacket = NULL;
    prMsduInfo->u2FrameLength = 0;
    prMsduInfo->u4Option = 0;
    prMsduInfo->u4FixedRateOption = 0;
    prMsduInfo->ucRateMode = MSDU_RATE_MODE_AUTO;
    prMsduInfo->ucBssIndex = prStaRec->ucBssIndex;
    prMsduInfo->ucPacketType = TX_PACKET_TYPE_DATA;
    prMsduInfo->ucStaRecIndex = prStaRec->ucIndex;
    prMsduInfo->ucPID = NIC_TX_DESC_PID_RESERVED;

    ucTxDescSize = NIC_TX_DESC_SHORT_FORMAT_LENGTH;

	DBGLOG(QM, INFO,
	       ("Generate TXD template for STA[%u] QoS[%u]\n", prStaRec->ucIndex,
		prStaRec->fgIsQoS));

    /* Generate new template */
	if (prStaRec->fgIsQoS) {
        /* For QoS STA, generate 8 TXD template (TID0~TID7) */
		for (ucTid = 0; ucTid < TX_DESC_TID_NUM; ucTid++) {

			if (prAdapter->rWifiVar.ucTcRestrict < TC_NUM) {
                ucTc = prAdapter->rWifiVar.ucTcRestrict;
			} else {
				ucTc =
				    arNetwork2TcResource[prStaRec->ucBssIndex][aucTid2ACI[ucTid]];
            }
            ucTxDescSize = arTcTrafficSettings[ucTc].ucTxDescLength;
            
            prTxDesc = kalMemAlloc(ucTxDescSize, VIR_MEM_TYPE);
			if (!prTxDesc) {
                rStatus = WLAN_STATUS_RESOURCES;
                break;
            }
            
            /* Update MsduInfo TID & TC */
            prMsduInfo->ucUserPriority = ucTid;
            prMsduInfo->ucTC = ucTc;

            /* Compose Tx desc template */
			nicTxComposeDesc(prAdapter, prMsduInfo, ucTxDescSize, TRUE,
					 (PUINT_8) prTxDesc);

            prStaRec->aprTxDescTemplate[ucTid] = prTxDesc;
        }
	} else {
        /* For non-QoS STA, generate 1 TXD template (TID0) */
		do {
			if (prAdapter->rWifiVar.ucTcRestrict < TC_NUM) {
                ucTc = prAdapter->rWifiVar.ucTcRestrict;
			} else {
				ucTc =
				    arNetwork2TcResource[prStaRec->
							 ucBssIndex]
				    [NET_TC_NON_STAREC_NON_QOS_INDEX];
            }
			/* ucTxDescSize = arTcTrafficSettings[ucTc].ucTxDescLength; */
            ucTxDescSize = NIC_TX_DESC_SHORT_FORMAT_LENGTH;
            
            prTxDesc = kalMemAlloc(ucTxDescSize, VIR_MEM_TYPE);
			if (!prTxDesc) {
                rStatus = WLAN_STATUS_RESOURCES;
                break;
            }
            /* Update MsduInfo TID & TC */
            prMsduInfo->ucUserPriority = 0;
            prMsduInfo->ucTC = ucTc; 
            
            /* Compose Tx desc template */
			nicTxComposeDesc(prAdapter, prMsduInfo, ucTxDescSize, TRUE,
					 (PUINT_8) prTxDesc);
            
			for (ucTid = 0; ucTid < TX_DESC_TID_NUM; ucTid++) {
                prStaRec->aprTxDescTemplate[ucTid] = prTxDesc;
				DBGLOG(QM, TRACE,
				       ("TXD template: TID[%u] Ptr[0x%x]\n", ucTid,
					(ULONG) prTxDesc));
            }
		} while (FALSE);
    }

    nicTxReturnMsduInfo(prAdapter, prMsduInfo);

    return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll free Tx descriptor template for each TID.
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prStaRec              Pointer to the StaRec structure.
*
* @retval VOID
*/
/*----------------------------------------------------------------------------*/
VOID nicTxFreeDescTemplate(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec)
{
    UINT_8  ucTid;
    UINT_8  ucTxDescSize;
    P_HW_MAC_TX_DESC_T  prTxDesc;

	DBGLOG(QM, INFO,
	       ("Free TXD template for STA[%u] QoS[%u]\n", prStaRec->ucIndex, prStaRec->fgIsQoS));

	if (prStaRec->fgIsQoS) {
		for (ucTid = 0; ucTid < TX_DESC_TID_NUM; ucTid++) {
			prTxDesc = (P_HW_MAC_TX_DESC_T) prStaRec->aprTxDescTemplate[ucTid];
            
			if (prTxDesc) {
				if (HAL_MAC_TX_DESC_IS_LONG_FORMAT(prTxDesc)) {
                    ucTxDescSize = NIC_TX_DESC_LONG_FORMAT_LENGTH;
				} else {
                    ucTxDescSize = NIC_TX_DESC_SHORT_FORMAT_LENGTH;
                }
                
                kalMemFree(prTxDesc, VIR_MEM_TYPE, ucTxDescSize);
                
                prTxDesc = prStaRec->aprTxDescTemplate[ucTid] = NULL;
            }
        }
	} else {
		prTxDesc = (P_HW_MAC_TX_DESC_T) prStaRec->aprTxDescTemplate[0];
		if (prTxDesc) {
			if (HAL_MAC_TX_DESC_IS_LONG_FORMAT(prTxDesc)) {
                ucTxDescSize = NIC_TX_DESC_LONG_FORMAT_LENGTH;
			} else {
                ucTxDescSize = NIC_TX_DESC_SHORT_FORMAT_LENGTH;
            }
            
            kalMemFree(prTxDesc, VIR_MEM_TYPE, ucTxDescSize);
            prTxDesc = NULL;
        }        
		for (ucTid = 0; ucTid < TX_DESC_TID_NUM; ucTid++) {
            prStaRec->aprTxDescTemplate[ucTid] = NULL;
        }
    }
}

/*----------------------------------------------------------------------------*/
/*!
* @brief In this function, we'll write frame(PACKET_INFO_T) into HIF.
*
* @param prAdapter              Pointer to the Adapter structure.
* @param ucPortIdx              Port Number
* @param prQue                  a link list of P_MSDU_INFO_T
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxMsduQueue(IN P_ADAPTER_T prAdapter, UINT_8 ucPortIdx, P_QUE_T prQue)
{
    P_MSDU_INFO_T prMsduInfo, prNextMsduInfo;
    P_NATIVE_PACKET prNativePacket;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;	/* Pointer to Transmit Data Structure Frame */
    UINT_8 ucTxDescSize;
    UINT_32 u4ValidBufSize;
    UINT_32 u4TotalLength;
    P_TX_CTRL_T prTxCtrl;
    QUE_T rFreeQueue;
    P_QUE_T prFreeQueue;
    PUINT_8 pucBufferTxD;
#if ((CFG_SDIO_TX_AGG == 1) && (CFG_SDIO_TX_AGG_LIMIT != 0))    
    BOOLEAN fgWriteNow;
#endif

    ASSERT(prAdapter);
    ASSERT(ucPortIdx < 2);
    ASSERT(prQue);

    prTxCtrl = &prAdapter->rTxCtrl;
    u4ValidBufSize = prAdapter->u4CoalescingBufCachedSize;

#if CFG_HIF_STATISTICS
    prTxCtrl->u4TotalTxAccessNum++;
    prTxCtrl->u4TotalTxPacketNum += prQue->u4NumElem;
#endif

    prFreeQueue = &rFreeQueue;

    QUEUE_INITIALIZE(prFreeQueue);

	if (prQue->u4NumElem > 0) {
		prMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_HEAD(prQue);
        pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;
        u4TotalLength = 0;

		while (prMsduInfo) {

            prNativePacket = prMsduInfo->prPacket;

            ASSERT(prNativePacket);
            
#if CFG_SUPPORT_MULTITHREAD
            nicTxCopyDesc(prAdapter, (pucOutputBuf + u4TotalLength),
				prMsduInfo->aucTxDescBuffer, &ucTxDescSize);
#else
			nicTxFillDesc(prAdapter, prMsduInfo, (pucOutputBuf + u4TotalLength),
				      &ucTxDescSize);
#endif

            pucBufferTxD = (pucOutputBuf + u4TotalLength);
            
            u4TotalLength += (ucTxDescSize + NIC_TX_DESC_PADDING_LENGTH);

            if (prMsduInfo->eSrc == TX_PACKET_OS
                    || prMsduInfo->eSrc == TX_PACKET_FORWARDING) {
                kalCopyFrame(prAdapter->prGlueInfo,
					     prNativePacket, pucOutputBuf + u4TotalLength);
			} else if (prMsduInfo->eSrc == TX_PACKET_MGMT) {
                kalMemCopy(pucOutputBuf + u4TotalLength,
					   prNativePacket, prMsduInfo->u2FrameLength);
			} else {
                ASSERT(0);
            }                 

            u4TotalLength += ALIGN_4(prMsduInfo->u2FrameLength);

            prNextMsduInfo = (P_MSDU_INFO_T)
                        QUEUE_GET_NEXT_ENTRY(&prMsduInfo->rQueEntry);

            /* Free MSDU_INFO */
            if (prMsduInfo->eSrc == TX_PACKET_MGMT) {
                GLUE_DEC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);
			} 
            else if (prMsduInfo->eSrc == TX_PACKET_OS) {
                wlanTxProfilingTagMsdu(prAdapter, prMsduInfo, 
                    TX_PROF_TAG_DRV_TX_DONE);
                kalSendComplete(prAdapter->prGlueInfo, prNativePacket, 
                    WLAN_STATUS_SUCCESS);
                prMsduInfo->prPacket = NULL;
			} 
            else if (prMsduInfo->eSrc == TX_PACKET_FORWARDING) {
                GLUE_DEC_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);
            }

            if (prMsduInfo->pfTxDoneHandler) {
                KAL_SPIN_LOCK_DECLARATION();
    			DBGLOG(REQ, INFO, ("Wait WIDX:PID[%u:%u] SEQ[%u]\n",
                    prMsduInfo->ucWlanIndex, prMsduInfo->ucPID,
                    prMsduInfo->ucTxSeqNum));
                
                KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
    			QUEUE_INSERT_TAIL(&(prTxCtrl->rTxMgmtTxingQueue),
    					  (P_QUE_ENTRY_T) prMsduInfo);
                KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
            }
            else {
                if(prMsduInfo->eSrc == TX_PACKET_MGMT) {
                    cnmMgtPktFree(prAdapter, prMsduInfo);
                }
                else {
                    /* only free MSDU when it is not a MGMT frame */
    				QUEUE_INSERT_TAIL(prFreeQueue, (P_QUE_ENTRY_T) prMsduInfo);
                }
            }

#if (CFG_SDIO_TX_AGG == 0)
            ASSERT(u4TotalLength <= u4ValidBufSize);
            
            HAL_WRITE_TX_PORT(prAdapter,
					  u4TotalLength, (PUINT_8) pucOutputBuf, u4ValidBufSize);

			/* reset total length */
            u4TotalLength = 0;   

#elif ((CFG_SDIO_TX_AGG == 1) && (CFG_SDIO_TX_AGG_LIMIT != 0))
            fgWriteNow = TRUE;

			if (prNextMsduInfo) {
				if ((u4TotalLength + prNextMsduInfo->u2FrameLength +
				     NIC_TX_DESC_AND_PADDING_LENGTH) < CFG_SDIO_TX_AGG_LIMIT) {
                    fgWriteNow = FALSE;
                }
            }
            
            /* Write to HIF */
			if (fgWriteNow) {
                ASSERT(u4TotalLength <= u4ValidBufSize);
                
                HAL_WRITE_TX_PORT(prAdapter,
                        u4TotalLength,
						  (PUINT_8) pucOutputBuf, u4ValidBufSize);

				/* reset total length */
                u4TotalLength = 0;   
            }            
#endif            

            prMsduInfo = prNextMsduInfo;
        } 

#if ((CFG_SDIO_TX_AGG == 1) && (CFG_SDIO_TX_AGG_LIMIT == 0))
        if(u4TotalLength > u4ValidBufSize) {
            DBGLOG(TX, ERROR, ("Tx Error! Port[%u] u4TotalLength[%u] > u4ValidBufSize[%u]\n", 
                ucPortIdx, u4TotalLength, u4ValidBufSize));
            
            DBGLOG(TX, ERROR, ("Tx Error! TxQ count[%u], FreeQ count[%u]\n", 
                prQue->u4NumElem, prFreeQueue->u4NumElem));

            prMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_HEAD(prFreeQueue);
            DBGLOG(TX, ERROR, ("=== Dump MsduInfo ===\n"));            
            while(prMsduInfo) {

                DBGLOG(INIT, INFO, ("Msdu[0x%p] Src[%u] Len[%u] Bss[%u] Sta[%u] TC[%u]\n", 
                    prMsduInfo, prMsduInfo->eSrc, prMsduInfo->u2FrameLength, prMsduInfo->ucBssIndex,
                    prMsduInfo->ucStaRecIndex, prMsduInfo->ucTC));

                prMsduInfo = (P_MSDU_INFO_T)QUEUE_GET_NEXT_ENTRY(&prMsduInfo->rQueEntry);
            }
            DBGLOG(TX, ERROR, ("=== Dump done ===\n")); 
            
            qmDumpQueueStatus(prAdapter);
        }

        ASSERT(u4TotalLength <= u4ValidBufSize);
        
		HAL_WRITE_TX_PORT(prAdapter, u4TotalLength, (PUINT_8) pucOutputBuf, u4ValidBufSize);
#endif
		nicTxReturnMsduInfo(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(&rFreeQueue));

            }
      
    return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief In this function, we'll write Command(CMD_INFO_T) into HIF.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prPacketInfo   Pointer of CMD_INFO_T
* @param ucTC           Specify the resource of TC
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN UINT_8 ucTC)
{
    P_WIFI_CMD_T prWifiCmd;
    UINT_16 u2OverallBufferLength;
    UINT_8 ucTxDescLength;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;	/* Pointer to Transmit Data Structure Frame */
    P_NATIVE_PACKET prNativePacket;
    P_MSDU_INFO_T prMsduInfo;
    P_TX_CTRL_T prTxCtrl;

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    ASSERT(prCmdInfo);

    prTxCtrl = &prAdapter->rTxCtrl;
    pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;
	if (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME) {
        prMsduInfo = prCmdInfo->prMsduInfo;
        
#if CFG_SUPPORT_MULTITHREAD
        nicTxCopyDesc(prAdapter, &pucOutputBuf[0], prMsduInfo->aucTxDescBuffer, 
            &ucTxDescLength);
#else
		nicTxComposeSecurityFrameDesc(prAdapter, prCmdInfo, &pucOutputBuf[0],
            &ucTxDescLength);
#endif

        prNativePacket = prMsduInfo->prPacket;
		u2OverallBufferLength =
		    TFCB_FRAME_PAD_TO_DW((prCmdInfo->u2InfoBufLen + ucTxDescLength));

		/* <3> Copy Frame Body */
		kalCopyFrame(prAdapter->prGlueInfo, prNativePacket, pucOutputBuf + ucTxDescLength);

        DBGLOG(INIT, INFO, ("TX SEC Frame: BSS[%u] WIDX:PID[%u:%u] STA[%u]"
            "LEN[%u] ENC[%u] RSP[%u]\n", prCmdInfo->ucBssIndex, 
			HAL_MAC_TX_DESC_GET_WLAN_INDEX((P_HW_MAC_TX_DESC_T)&pucOutputBuf[0]),
            prMsduInfo->ucPID, prCmdInfo->ucStaRecIndex, 
            ucTxDescLength + prCmdInfo->u2InfoBufLen,
            HAL_MAC_TX_DESC_IS_PROTECTION((P_HW_MAC_TX_DESC_T)&pucOutputBuf[0]), 
            prMsduInfo->pfTxDoneHandler ? TRUE : FALSE));

        prMsduInfo->prPacket = NULL;

        if (prMsduInfo->pfTxDoneHandler) {
			/* DBGLOG(INIT, TRACE,("Wait Cmd TxSeqNum:%d\n", prMsduInfo->ucTxSeqNum)); */
            KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
			QUEUE_INSERT_TAIL(&(prTxCtrl->rTxMgmtTxingQueue),
					  (P_QUE_ENTRY_T) prMsduInfo);
            KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
		} else {
            /* Only return MSDU_INFO */
		    /* NativePacket will be freed at SEC frame CMD callback */
		    nicTxReturnMsduInfo(prAdapter, prMsduInfo);
        }
        
	} else if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME) {
		prMsduInfo = prCmdInfo->prMsduInfo;

        ASSERT(prMsduInfo->fgIs802_11 == TRUE);
        ASSERT(prMsduInfo->eSrc == TX_PACKET_MGMT);     

#if CFG_SUPPORT_MULTITHREAD
        nicTxCopyDesc(prAdapter, &pucOutputBuf[0], prMsduInfo->aucTxDescBuffer, 
            &ucTxDescLength);
#else
        nicTxFillDesc(prAdapter, prMsduInfo, &pucOutputBuf[0], &ucTxDescLength);
#endif

        u2OverallBufferLength = 
		    ucTxDescLength + NIC_TX_DESC_PADDING_LENGTH + prMsduInfo->u2FrameLength;

		/* <3> Copy Frame Body */
        kalMemCopy(pucOutputBuf + ucTxDescLength + NIC_TX_DESC_PADDING_LENGTH,
			   prMsduInfo->prPacket, prMsduInfo->u2FrameLength);

		/* <4> Management Frame Post-Processing */
        GLUE_DEC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);

		DBGLOG(INIT, INFO,
		       ("TX MGMT Frame: BSS[%u] WIDX:PID[%u:%u] SEQ[%u] STA[%u] LEN[%u] RSP[%u]\n",
			prCmdInfo->ucBssIndex, prMsduInfo->ucWlanIndex, prMsduInfo->ucPID,
			prMsduInfo->ucTxSeqNum, prMsduInfo->ucStaRecIndex, u2OverallBufferLength,
			prMsduInfo->pfTxDoneHandler ? TRUE : FALSE));

        if (prMsduInfo->pfTxDoneHandler) {
			/* DBGLOG(INIT, TRACE,("Wait Cmd TxSeqNum:%d\n", prMsduInfo->ucTxSeqNum)); */
            KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
			QUEUE_INSERT_TAIL(&(prTxCtrl->rTxMgmtTxingQueue),
					  (P_QUE_ENTRY_T) prMsduInfo);
            KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);            
		} else {
            cnmMgtPktFree(prAdapter, prMsduInfo);
        }
         
	} else {
		prWifiCmd = (P_WIFI_CMD_T) prCmdInfo->pucInfoBuffer;

		/* <2> Compose the Header of Transmit Data Structure for CMD Packet */
		u2OverallBufferLength = TFCB_FRAME_PAD_TO_DW((prCmdInfo->
							      u2InfoBufLen) & (UINT_16)
							     HIF_TX_HDR_TX_BYTE_COUNT_MASK);

        prWifiCmd->u2TxByteCount = u2OverallBufferLength;
        prWifiCmd->u2PQ_ID = CMD_PQ_ID;
		prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;

		/* <3> Copy CMD Header to command buffer (by using pucCoalescingBufCached) */
		kalMemCopy((PVOID) & pucOutputBuf[0],
			   (PVOID) prCmdInfo->pucInfoBuffer, prCmdInfo->u2InfoBufLen);

        ASSERT(u2OverallBufferLength <= prAdapter->u4CoalescingBufCachedSize);
        
        DBGLOG(INIT, INFO, ("TX CMD: ID[0x%02X] SEQ[%u] SET[%u] LEN[%u]\n", 
            prWifiCmd->ucCID, prWifiCmd->ucSeqNum, prWifiCmd->ucSetQuery, 
            u2OverallBufferLength));
    }

	/* <4> Write frame to data port */
    HAL_WRITE_TX_PORT(prAdapter,
			  (UINT_32) u2OverallBufferLength,
			  (PUINT_8) pucOutputBuf, (UINT_32) prAdapter->u4CoalescingBufCachedSize);

    return WLAN_STATUS_SUCCESS;
} /* end of nicTxCmd() */


/*----------------------------------------------------------------------------*/
/*!
* @brief This function will clean up all the pending frames in internal SW Queues
*        by return the pending TX packet to the system.
*
* @param prAdapter  Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxRelease(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgProcTxDoneHandler)
{
    P_TX_CTRL_T prTxCtrl;
    P_MSDU_INFO_T prMsduInfo;

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);

    prTxCtrl = &prAdapter->rTxCtrl;

    nicTxFlush(prAdapter);

	/* free MSDU_INFO_T from rTxMgmtMsduInfoList */
    do {
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
        QUEUE_REMOVE_HEAD(&prTxCtrl->rTxMgmtTxingQueue, prMsduInfo, P_MSDU_INFO_T);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);

		if (prMsduInfo) {
            DBGLOG(TX, INFO, ("%s: Get Msdu WIDX:PID[%u:%u] SEQ[%u] from Pending Q\n", 
					  __func__,
                prMsduInfo->ucWlanIndex, 
					  prMsduInfo->ucPID, prMsduInfo->ucTxSeqNum));
            
			/* invoke done handler */
			if (prMsduInfo->pfTxDoneHandler && fgProcTxDoneHandler) {
				prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo,
							    TX_RESULT_DROPPED_IN_DRIVER);
            }
			nicTxFreeMsduInfoPacket(prAdapter, prMsduInfo);
            nicTxReturnMsduInfo(prAdapter, prMsduInfo);
		} else {
            break;
        }
	} while (TRUE);

    return;
} /* end of nicTxRelease() */

VOID nicTxInterruptSanityCheck(IN P_ADAPTER_T prAdapter, 
    IN UINT_16 *au2TxRlsCnt) 
{
    UINT_8 ucIdx;
    BOOLEAN fgError = FALSE;

    if(prAdapter->rWifiVar.ucTxDbg & BIT(1)) {
        for(ucIdx = HIF_TX_AC0_INDEX; ucIdx < HIF_TX_NUM; ucIdx++) {
            if(au2TxRlsCnt[ucIdx] > CFG_MAX_TX_PAGE_COUNT) {
                fgError = TRUE;   
            }
        }

        if(fgError)
            DBGLOG(TX, ERROR, ("Tx Done INT result, FFA[%u] AC[%u:%u:%u:%u:%u] CPU[%u]\n", 
                au2TxRlsCnt[HIF_TX_FFA_INDEX], au2TxRlsCnt[HIF_TX_AC0_INDEX],
                au2TxRlsCnt[HIF_TX_AC1_INDEX], au2TxRlsCnt[HIF_TX_AC2_INDEX],
                au2TxRlsCnt[HIF_TX_AC3_INDEX], au2TxRlsCnt[HIF_TX_AC4_INDEX], 
                au2TxRlsCnt[HIF_TX_CPU_INDEX]));    
    }
}


/*----------------------------------------------------------------------------*/
/*!
* @brief Process the TX Done interrupt and pull in more pending frames in SW
*        Queues for transmission.
*
* @param prAdapter  Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicProcessTxInterrupt(IN P_ADAPTER_T prAdapter)
{
    P_TX_CTRL_T prTxCtrl;
#if CFG_SDIO_INTR_ENHANCE
    P_SDIO_CTRL_T prSDIOCtrl;
#else
    UINT_32 au4TxCount[2];
#endif /* CFG_SDIO_INTR_ENHANCE */

    ASSERT(prAdapter);

    prTxCtrl = &prAdapter->rTxCtrl;
    ASSERT(prTxCtrl);

     /* Get the TX STATUS */
#if CFG_SDIO_INTR_ENHANCE

    prSDIOCtrl = prAdapter->prSDIOCtrl;
#if DBG
	/* dumpMemory8((PUINT_8)prSDIOCtrl, sizeof(SDIO_CTRL_T)); */
#endif

    nicTxInterruptSanityCheck(prAdapter, (PUINT_16)&prSDIOCtrl->rTxInfo);
	nicTxReleaseResource(prAdapter, (PUINT_16)&prSDIOCtrl->rTxInfo);
    kalMemZero(&prSDIOCtrl->rTxInfo, sizeof(prSDIOCtrl->rTxInfo));

#else

    HAL_MCR_RD(prAdapter, MCR_WTSR0, &au4TxCount[0]);
    HAL_MCR_RD(prAdapter, MCR_WTSR1, &au4TxCount[1]);
    DBGLOG(EMU, TRACE, ("MCR_WTSR0: 0x%x, MCR_WTSR1: 0x%x\n", au4TxCount[0], au4TxCount[1]));

	nicTxReleaseResource(prAdapter, (PUINT_8) au4TxCount);

#endif /* CFG_SDIO_INTR_ENHANCE */

    nicTxAdjustTcq(prAdapter);

	/* Indicate Service Thread */
	if (kalGetTxPendingCmdCount(prAdapter->prGlueInfo) > 0
            || wlanGetTxPendingFrameCount(prAdapter) > 0) {
        kalSetEvent(prAdapter->prGlueInfo);
    }

    return;
} /* end of nicProcessTxInterrupt() */


/*----------------------------------------------------------------------------*/
/*!
* @brief this function frees packet of P_MSDU_INFO_T linked-list
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfoList         a link list of P_MSDU_INFO_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxFreeMsduInfoPacket(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
    P_NATIVE_PACKET prNativePacket;
    P_MSDU_INFO_T prMsduInfo = prMsduInfoListHead;
    P_TX_CTRL_T prTxCtrl;


    ASSERT(prAdapter);
    ASSERT(prMsduInfoListHead);

    prTxCtrl = &prAdapter->rTxCtrl;

	while (prMsduInfo) {
		prNativePacket = prMsduInfo->prPacket;

		if (prMsduInfo->eSrc == TX_PACKET_OS) {
			if(prNativePacket) {
				kalSendComplete(prAdapter->prGlueInfo, prNativePacket, 
					WLAN_STATUS_FAILURE);
			}
		} else if (prMsduInfo->eSrc == TX_PACKET_MGMT) {
			if (prMsduInfo->pfTxDoneHandler) {
				prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo,
								TX_RESULT_DROPPED_IN_DRIVER);
			}
			if(prNativePacket) {
				cnmMemFree(prAdapter, prNativePacket);
			}
		} else if (prMsduInfo->eSrc == TX_PACKET_FORWARDING) {
			GLUE_DEC_REF_CNT(prTxCtrl->i4PendingFwdFrameCount);
		}

		prMsduInfo = 
			(P_MSDU_INFO_T)QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);
	}
	

    return;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief this function returns P_MSDU_INFO_T of MsduInfoList to TxCtrl->rfreeMsduInfoList
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfoList         a link list of P_MSDU_INFO_T
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxReturnMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
    P_TX_CTRL_T prTxCtrl;
    P_MSDU_INFO_T prMsduInfo = prMsduInfoListHead, prNextMsduInfo;
#if CFG_DBG_MGT_BUF
    OS_SYSTIME rAllocTime;
#endif

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);

    prTxCtrl = &prAdapter->rTxCtrl;
    ASSERT(prTxCtrl);

	while (prMsduInfo) {
		prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);

		switch (prMsduInfo->eSrc) {
        case TX_PACKET_FORWARDING:
            wlanReturnPacket(prAdapter, prMsduInfo->prPacket);
            break;
        case TX_PACKET_OS:
        case TX_PACKET_OS_OID:
        case TX_PACKET_MGMT:
        default:
            break;
        }
        
#if CFG_DBG_MGT_BUF
        rAllocTime = prMsduInfo->rLastAllocTime;
#endif
        /* Reset MSDU_INFO fields */
        kalMemZero(prMsduInfo, sizeof(MSDU_INFO_T));

#if CFG_DBG_MGT_BUF
        prMsduInfo->fgIsUsed = FALSE;
        prMsduInfo->rLastFreeTime = kalGetTimeTick();
        prMsduInfo->rLastAllocTime = rAllocTime;
#endif  

        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
		QUEUE_INSERT_TAIL(&prTxCtrl->rFreeMsduInfoList, (P_QUE_ENTRY_T) prMsduInfo);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
        prMsduInfo = prNextMsduInfo;
    };

    return;
}



/*----------------------------------------------------------------------------*/
/*!
* @brief this function fills packet information to P_MSDU_INFO_T
*
* @param prAdapter              Pointer to the Adapter structure.
* @param prMsduInfo             P_MSDU_INFO_T
* @param prPacket               P_NATIVE_PACKET
*
* @retval TRUE      Success to extract information
* @retval FALSE     Fail to extract correct information
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
nicTxFillMsduInfo(IN P_ADAPTER_T prAdapter,
		  IN P_MSDU_INFO_T prMsduInfo, IN P_NATIVE_PACKET prPacket)
{
    P_GLUE_INFO_T   prGlueInfo;
	BOOLEAN fgIsUseFixRate = FALSE;

    ASSERT(prAdapter);

    kalMemZero(prMsduInfo, sizeof(MSDU_INFO_T));

    prGlueInfo = prAdapter->prGlueInfo;
    ASSERT(prGlueInfo); 
	
    kalGetEthDestAddr(prAdapter->prGlueInfo, prPacket, 
        prMsduInfo->aucEthDestAddr);

    prMsduInfo->prPacket = prPacket;
    prMsduInfo->ucBssIndex = GLUE_GET_PKT_BSS_IDX(prPacket);
    prMsduInfo->ucUserPriority = GLUE_GET_PKT_TID(prPacket);
    prMsduInfo->ucMacHeaderLength = GLUE_GET_PKT_HEADER_LEN(prPacket);
	prMsduInfo->u2FrameLength = (UINT_16) GLUE_GET_PKT_FRAME_LEN(prPacket);
    prMsduInfo->ucPageCount = nicTxGetPageCount(prMsduInfo->u2FrameLength, FALSE);

    if(GLUE_IS_PKT_FLAG_SET(prPacket)) {
        prMsduInfo->fgIs802_1x = GLUE_TEST_PKT_FLAG(prPacket, ENUM_PKT_1X);
        prMsduInfo->fgIs802_3 = GLUE_TEST_PKT_FLAG(prPacket, ENUM_PKT_802_3);
        prMsduInfo->fgIsVlanExists = GLUE_TEST_PKT_FLAG(prPacket, ENUM_PKT_VLAN_EXIST);
        
        if(GLUE_TEST_PKT_FLAG(prPacket, ENUM_PKT_DHCP) && prAdapter->rWifiVar.ucDhcpTxDone) {
        	fgIsUseFixRate = TRUE;
            prMsduInfo->pfTxDoneHandler = wlanDhcpTxDone;
        }
        else if(GLUE_TEST_PKT_FLAG(prPacket, ENUM_PKT_ARP) && prAdapter->rWifiVar.ucArpTxDone) {
        	fgIsUseFixRate = TRUE;
            prMsduInfo->pfTxDoneHandler = wlanArpTxDone;
        }
        else if (GLUE_TEST_PKT_FLAG(prPacket, ENUM_PKT_DNS) && prAdapter->rWifiVar.ucDnsTxDone) {
        	fgIsUseFixRate = TRUE;
			prMsduInfo->pfTxDoneHandler = wlanDnsTxDone;
		}

		if (fgIsUseFixRate == TRUE) {
			if (prMsduInfo->ucBssIndex <= MAX_BSS_INDEX) {
				P_BSS_INFO_T prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);
				prMsduInfo->u4FixedRateOption |= HAL_MAC_TX_DESC_SET_FIX_RATE(prBssInfo);
				prMsduInfo->ucRateMode = MSDU_RATE_MODE_MANUAL_DESC;
			}
		}
    }

    return TRUE;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief this function update TCQ values by passing current status to txAdjustTcQuotas
*
* @param prAdapter              Pointer to the Adapter structure.
*
* @retval WLAN_STATUS_SUCCESS   Updated successfully
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxAdjustTcq(IN P_ADAPTER_T prAdapter)
{
#if CFG_SUPPORT_MULTITHREAD
    TX_TCQ_ADJUST_T rTcqAdjust;
    P_TX_CTRL_T prTxCtrl;
    
    ASSERT(prAdapter);
    
    prTxCtrl = &prAdapter->rTxCtrl;
    ASSERT(prTxCtrl);
    
    qmAdjustTcQuotasMthread(prAdapter, &rTcqAdjust, &prTxCtrl->rTc);

#else

    UINT_32 u4Num;
    TX_TCQ_ADJUST_T rTcqAdjust;
    P_TX_CTRL_T prTxCtrl;
    P_TX_TCQ_STATUS_T prTcqStatus;
    KAL_SPIN_LOCK_DECLARATION();
    
    ASSERT(prAdapter);

    prTxCtrl = &prAdapter->rTxCtrl;
    prTcqStatus = &prAdapter->rTxCtrl.rTc;        
    ASSERT(prTxCtrl);

	if (qmAdjustTcQuotas(prAdapter, &rTcqAdjust, &prTxCtrl->rTc)) {
        
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);

        for (u4Num = 0 ; u4Num < TC_NUM ; u4Num++) {
            /* Page count */
			prTxCtrl->rTc.au2FreePageCount[u4Num] +=
			    (rTcqAdjust.acVariation[u4Num] * NIC_TX_MAX_PAGE_PER_FRAME);
			prTxCtrl->rTc.au2MaxNumOfPage[u4Num] +=
			    (rTcqAdjust.acVariation[u4Num] * NIC_TX_MAX_PAGE_PER_FRAME);

            /* Buffer count */
            prTxCtrl->rTc.au2FreeBufferCount[u4Num] += rTcqAdjust.acVariation[u4Num];
            prTxCtrl->rTc.au2MaxNumOfBuffer[u4Num] += rTcqAdjust.acVariation[u4Num];

            ASSERT(prTxCtrl->rTc.au2FreeBufferCount[u4Num] >= 0);
            ASSERT(prTxCtrl->rTc.au2MaxNumOfBuffer[u4Num] >= 0);
        }

        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_RESOURCE);
#if 0        
		DBGLOG(TX, LOUD,
		       ("TCQ Status Free Page:Buf[%03u:%02u, %03u:%02u, %03u:%02u, %03u:%02u, %03u:%02u, %03u:%02u]\n",
            prTcqStatus->au2FreePageCount[TC0_INDEX],
            prTcqStatus->au2FreeBufferCount[TC0_INDEX],
            prTcqStatus->au2FreePageCount[TC1_INDEX],
            prTcqStatus->au2FreeBufferCount[TC1_INDEX],
            prTcqStatus->au2FreePageCount[TC2_INDEX],
            prTcqStatus->au2FreeBufferCount[TC2_INDEX],
            prTcqStatus->au2FreePageCount[TC3_INDEX],
            prTcqStatus->au2FreeBufferCount[TC3_INDEX],
            prTcqStatus->au2FreePageCount[TC4_INDEX],
            prTcqStatus->au2FreeBufferCount[TC4_INDEX],     
            prTcqStatus->au2FreePageCount[TC5_INDEX],
            prTcqStatus->au2FreeBufferCount[TC5_INDEX]));
#endif
		DBGLOG(TX, LOUD,
		       ("TCQ Status Max Page:Buf[%03u:%02u, %03u:%02u, %03u:%02u, %03u:%02u, %03u:%02u, %03u:%02u]\n",
            prTcqStatus->au2MaxNumOfPage[TC0_INDEX],
            prTcqStatus->au2MaxNumOfBuffer[TC0_INDEX],
            prTcqStatus->au2MaxNumOfPage[TC1_INDEX],
            prTcqStatus->au2MaxNumOfBuffer[TC1_INDEX],
            prTcqStatus->au2MaxNumOfPage[TC2_INDEX],
            prTcqStatus->au2MaxNumOfBuffer[TC2_INDEX],
            prTcqStatus->au2MaxNumOfPage[TC3_INDEX],
            prTcqStatus->au2MaxNumOfBuffer[TC3_INDEX],
            prTcqStatus->au2MaxNumOfPage[TC4_INDEX],
            prTcqStatus->au2MaxNumOfBuffer[TC4_INDEX],     
            prTcqStatus->au2MaxNumOfPage[TC5_INDEX],
            prTcqStatus->au2MaxNumOfBuffer[TC5_INDEX]));

    }
#endif
    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief this function flushes all packets queued in STA/AC queue
*
* @param prAdapter              Pointer to the Adapter structure.
*
* @retval WLAN_STATUS_SUCCESS   Flushed successfully
*/
/*----------------------------------------------------------------------------*/

WLAN_STATUS nicTxFlush(IN P_ADAPTER_T prAdapter)
{
    P_MSDU_INFO_T prMsduInfo;
    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);

	/* ask Per STA/AC queue to be fllushed and return all queued packets */
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
    prMsduInfo = qmFlushTxQueues(prAdapter);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

	if (prMsduInfo != NULL) {
        nicTxFreeMsduInfoPacket(prAdapter, prMsduInfo);
        nicTxReturnMsduInfo(prAdapter, prMsduInfo);
    }

    return WLAN_STATUS_SUCCESS;
}


#if CFG_ENABLE_FW_DOWNLOAD
/*----------------------------------------------------------------------------*/
/*!
* \brief In this function, we'll write Command(CMD_INFO_T) into HIF.
*        However this function is used for INIT_CMD.
*
*        In order to avoid further maintainance issues, these 2 functions are separated
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prPacketInfo   Pointer of CMD_INFO_T
* @param ucTC           Specify the resource of TC
*
* @retval WLAN_STATUS_SUCCESS   Bus access ok.
* @retval WLAN_STATUS_FAILURE   Bus access fail.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxInitCmd(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
    UINT_16 u2OverallBufferLength;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;	/* Pointer to Transmit Data Structure Frame */
    P_TX_CTRL_T prTxCtrl;

    ASSERT(prAdapter);
    ASSERT(prCmdInfo);

    prTxCtrl = &prAdapter->rTxCtrl;
    pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;
	u2OverallBufferLength = TFCB_FRAME_PAD_TO_DW((prCmdInfo->
						      u2InfoBufLen) & (UINT_16)
						     HIF_TX_HDR_TX_BYTE_COUNT_MASK);

	/* <1> Copy CMD Header to command buffer (by using pucCoalescingBufCached) */
	kalMemCopy((PVOID) & pucOutputBuf[0],
		   (PVOID) prCmdInfo->pucInfoBuffer, prCmdInfo->u2InfoBufLen);

    ASSERT(u2OverallBufferLength <= prAdapter->u4CoalescingBufCachedSize);

	/* <2> Write frame to data port */
    HAL_WRITE_TX_PORT(prAdapter,
			  (UINT_32) u2OverallBufferLength,
			  (PUINT_8) pucOutputBuf, (UINT_32) prAdapter->u4CoalescingBufCachedSize);

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief In this function, we'll reset TX resource counter to initial value used
*        in F/W download state
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @retval WLAN_STATUS_SUCCESS   Reset is done successfully.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxInitResetResource(IN P_ADAPTER_T prAdapter)
{
    P_TX_CTRL_T prTxCtrl;
    UINT_8 ucIdx;

    DEBUGFUNC("nicTxInitResetResource");

    ASSERT(prAdapter);
    prTxCtrl = &prAdapter->rTxCtrl;

    /* Delta page count */
    kalMemZero(prTxCtrl->rTc.au2TxDonePageCount, sizeof(prTxCtrl->rTc.au2TxDonePageCount));   
    kalMemZero(prTxCtrl->rTc.au2PreUsedPageCount, sizeof(prTxCtrl->rTc.au2PreUsedPageCount));
    prTxCtrl->rTc.ucNextTcIdx = TC0_INDEX;
    prTxCtrl->rTc.u2AvaliablePageCount = 0;

    /* Page count */
    prTxCtrl->rTc.au2MaxNumOfPage[TC0_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC0;
    prTxCtrl->rTc.au2FreePageCount[TC0_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC0;

    prTxCtrl->rTc.au2MaxNumOfPage[TC1_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC1;
    prTxCtrl->rTc.au2FreePageCount[TC1_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC1;

    prTxCtrl->rTc.au2MaxNumOfPage[TC2_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC2;
    prTxCtrl->rTc.au2FreePageCount[TC2_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC2;

    prTxCtrl->rTc.au2MaxNumOfPage[TC3_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC3;
    prTxCtrl->rTc.au2FreePageCount[TC3_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC3;

    prTxCtrl->rTc.au2MaxNumOfPage[TC4_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC4;
    prTxCtrl->rTc.au2FreePageCount[TC4_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC4;

    prTxCtrl->rTc.au2MaxNumOfPage[TC5_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC5;
    prTxCtrl->rTc.au2FreePageCount[TC5_INDEX] = NIC_TX_INIT_PAGE_COUNT_TC5;   

    /* Buffer count */
    for(ucIdx = TC0_INDEX; ucIdx < TC_NUM; ucIdx++) {
        prTxCtrl->rTc.au2MaxNumOfBuffer[ucIdx] = prTxCtrl->rTc.au2MaxNumOfPage[ucIdx] / NIC_TX_MAX_PAGE_PER_FRAME;
        prTxCtrl->rTc.au2FreeBufferCount[ucIdx] = prTxCtrl->rTc.au2FreePageCount[ucIdx] / NIC_TX_MAX_PAGE_PER_FRAME;
    }

    return WLAN_STATUS_SUCCESS;

}

#endif

BOOLEAN nicTxProcessMngPacket(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
    UINT_16 u2RateCode;
    P_BSS_INFO_T prBssInfo;
    P_STA_RECORD_T prStaRec;

	if (prMsduInfo->eSrc != TX_PACKET_MGMT) {
        return FALSE;
    }

    /* Sanity check */
    if(!prMsduInfo->prPacket) {
        return FALSE;
    }

    if(!prMsduInfo->u2FrameLength) {
        return FALSE;
    }

    if(!prMsduInfo->ucMacHeaderLength) {
        return FALSE;
    }    

    prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsduInfo->ucBssIndex);
    prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

	/* MMPDU: force stick to TC4 */
    prMsduInfo->ucTC = TC4_INDEX;
    
	/* No Tx descriptor template for MMPDU */
    prMsduInfo->fgIsTXDTemplateValid = FALSE;

    /* Fixed Rate */
	if (prMsduInfo->ucRateMode == MSDU_RATE_MODE_AUTO) {
        prMsduInfo->ucRateMode = MSDU_RATE_MODE_MANUAL_DESC;

		if (prStaRec) {
            u2RateCode = prStaRec->u2HwDefaultFixedRateCode;
		} else {
            u2RateCode = prBssInfo->u2HwDefaultFixedRateCode;
        }

		nicTxSetPktFixedRateOption(prMsduInfo, u2RateCode, FIX_BW_NO_FIXED, FALSE, FALSE);
    }   
#if CFG_SUPPORT_MULTITHREAD
    nicTxFillDesc(prAdapter, prMsduInfo, prMsduInfo->aucTxDescBuffer, NULL);
#endif

    return TRUE;
}
VOID nicTxProcessTxDoneEvent(IN P_ADAPTER_T prAdapter, 
    IN P_WIFI_EVENT_T prEvent)
{
    P_EVENT_TX_DONE_T prTxDone;
    P_MSDU_INFO_T prMsduInfo;
    
	prTxDone = (P_EVENT_TX_DONE_T)(prEvent->aucBuffer);

    if(prTxDone->ucFlag & BIT(0)) {
        /* Tx Done with advanced info */
        DBGLOG(INIT, INFO,("EVENT_ID_TX_DONE WIDX:PID[%u:%u] Status[%u] SN[%u]"
            " CNT[%u] RATE[0x%04x]\n",
            prTxDone->ucWlanIndex, prTxDone->ucPacketSeq, prTxDone->ucStatus, 
            prTxDone->u2SequenceNumber, prTxDone->ucTxCount, prTxDone->u2TxRate));
    }
    else {
        DBGLOG(INIT, INFO,("EVENT_ID_TX_DONE WIDX:PID[%u:%u] Status[%u] SN[%u]\n",
            prTxDone->ucWlanIndex, prTxDone->ucPacketSeq, prTxDone->ucStatus, 
            prTxDone->u2SequenceNumber));
    }

    /* call related TX Done Handler */
	prMsduInfo = nicGetPendingTxMsduInfo(prAdapter, prTxDone->ucWlanIndex, 
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

/*----------------------------------------------------------------------------*/
/*!
* \brief this function enqueues MSDU_INFO_T into queue management,
*        or command queue
*
* @param prAdapter      Pointer to the Adapter structure.
*        prMsduInfo     Pointer to MSDU
*
* @retval WLAN_STATUS_SUCCESS   Reset is done successfully.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS nicTxEnqueueMsdu(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo)
{
    P_TX_CTRL_T prTxCtrl;
    P_MSDU_INFO_T prNextMsduInfo, prRetMsduInfo, prMsduInfoHead;
    QUE_T qDataPort0, qDataPort1;
    P_QUE_T prDataPort0, prDataPort1;
    P_CMD_INFO_T prCmdInfo;
    WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    ASSERT(prMsduInfo);

    prTxCtrl = &prAdapter->rTxCtrl;
    ASSERT(prTxCtrl);

    prDataPort0 = &qDataPort0;
    prDataPort1 = &qDataPort1;

    QUEUE_INITIALIZE(prDataPort0);
    QUEUE_INITIALIZE(prDataPort1);

    /* check how many management frame are being queued */
	while (prMsduInfo) {
		prNextMsduInfo = (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo);

		QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prMsduInfo) = NULL;

		if (prMsduInfo->eSrc == TX_PACKET_MGMT) {
            if(nicTxProcessMngPacket(prAdapter, prMsduInfo)) {
                /* Valid MGMT*/
			    QUEUE_INSERT_TAIL(prDataPort1, (P_QUE_ENTRY_T) prMsduInfo);
            }
            else {
                /* Invalid MGMT */
                DBGLOG(TX, WARN, ("Invalid MGMT[0x%p] BSS[%u] STA[%u],"
                    "free it\n", prMsduInfo, prMsduInfo->ucBssIndex, 
                    prMsduInfo->ucStaRecIndex));
                
                cnmMgtPktFree(prAdapter, prMsduInfo);
            }
		} else {
			QUEUE_INSERT_TAIL(prDataPort0, (P_QUE_ENTRY_T) prMsduInfo);
        }

        prMsduInfo = prNextMsduInfo;
    }

	if (prDataPort0->u4NumElem) {
        /* send to QM */
        KAL_SPIN_LOCK_DECLARATION();
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
		prRetMsduInfo =
		    qmEnqueueTxPackets(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort0));
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);

        /* post-process for dropped packets */
		if (prRetMsduInfo) {	/* unable to enqueue */
            nicTxFreeMsduInfoPacket(prAdapter, prRetMsduInfo);
            nicTxReturnMsduInfo(prAdapter, prRetMsduInfo);
        }
    }

	if (prDataPort1->u4NumElem) {
		prMsduInfoHead = (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort1);

		if (nicTxGetFreeCmdCount(prAdapter) < NIC_TX_CMD_INFO_RESERVED_COUNT) {
			/* not enough descriptors for sending */
            u4Status = WLAN_STATUS_FAILURE;

			/* free all MSDUs */
			while (prMsduInfoHead) {
				prNextMsduInfo =
				    (P_MSDU_INFO_T) QUEUE_GET_NEXT_ENTRY(&prMsduInfoHead->
									 rQueEntry);

                if (prMsduInfoHead->pfTxDoneHandler != NULL) {
					prMsduInfoHead->pfTxDoneHandler(prAdapter, prMsduInfoHead,
									TX_RESULT_DROPPED_IN_DRIVER);
                }

                cnmMgtPktFree(prAdapter, prMsduInfoHead);

                prMsduInfoHead = prNextMsduInfo;
            }
		} else {
            /* send to command queue */
			while (prMsduInfoHead) {
				prNextMsduInfo =
				    (P_MSDU_INFO_T)QUEUE_GET_NEXT_ENTRY(&prMsduInfoHead->rQueEntry);

                KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);
				QUEUE_REMOVE_HEAD(&prAdapter->rFreeCmdList, prCmdInfo,
						  P_CMD_INFO_T);
                KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_RESOURCE);

                if (prCmdInfo) {
                    GLUE_INC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);

                    kalMemZero(prCmdInfo, sizeof(CMD_INFO_T));

#if CFG_ENABLE_PKT_LIFETIME_PROFILE
                    /* Tag MGMT enqueue time */
                    GET_CURRENT_SYSTIME(&prMsduInfoHead->rPktProfile.rEnqueueTimestamp);
#endif 
                    prCmdInfo->eCmdType = COMMAND_TYPE_MANAGEMENT_FRAME;
                    prCmdInfo->u2InfoBufLen = prMsduInfoHead->u2FrameLength;
                    prCmdInfo->pucInfoBuffer = NULL;
					prCmdInfo->prMsduInfo = prMsduInfoHead;
                    prCmdInfo->ucStaRecIndex = prMsduInfoHead->ucStaRecIndex;
                    prCmdInfo->ucBssIndex = prMsduInfoHead->ucBssIndex;
                    prCmdInfo->pfCmdDoneHandler = NULL;
                    prCmdInfo->pfCmdTimeoutHandler = NULL;
                    prCmdInfo->fgIsOid = FALSE;
                    prCmdInfo->fgSetQuery = TRUE;
                    prCmdInfo->fgNeedResp = FALSE;
                    prCmdInfo->ucCmdSeqNum = prMsduInfoHead->ucTxSeqNum;
#if CFG_DBG_MGT_BUF
                    prCmdInfo->fgIsUsed = TRUE;
                    prCmdInfo->rLastAllocTime = kalGetTimeTick();
#endif
                    
					DBGLOG(TX, INFO, ("%s: EN-Q MSDU[0x%p] SEQ[%u] BSS[%u] "
                        "STA[%u] to CMD Q\n", __func__, prMsduInfoHead, 
                        prMsduInfoHead->ucTxSeqNum, prMsduInfoHead->ucBssIndex, 
                        prMsduInfoHead->ucStaRecIndex));
    
					kalEnqueueCommand(prAdapter->prGlueInfo,
							  (P_QUE_ENTRY_T) prCmdInfo);
				} else {
                    /* Cmd free count is larger than expected, but allocation fail. */
                    u4Status = WLAN_STATUS_FAILURE;

                    if (prMsduInfoHead->pfTxDoneHandler != NULL) {
						prMsduInfoHead->pfTxDoneHandler(prAdapter,
										prMsduInfoHead,
										TX_RESULT_DROPPED_IN_DRIVER);
                    }
                    
                    cnmMgtPktFree(prAdapter, prMsduInfoHead);
                }

                prMsduInfoHead = prNextMsduInfo;
            }
        }
    }

    /* indicate service thread for sending */
	if (prTxCtrl->i4TxMgmtPendingNum > 0
            || kalGetTxPendingFrameCount(prAdapter->prGlueInfo) > 0) {
        kalSetEvent(prAdapter->prGlueInfo);
    }

    return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief this function returns WLAN index
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @retval
*/
/*----------------------------------------------------------------------------*/
UINT_8 nicTxGetWlanIdx(P_ADAPTER_T prAdapter, UINT_8 ucBssIdx, UINT_8 ucStaRecIdx)
{
    P_STA_RECORD_T prStaRec;
    P_BSS_INFO_T prBssInfo;
    UINT_8 ucWlanIndex = NIC_TX_DEFAULT_WLAN_INDEX;

    prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaRecIdx);
    prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);

	if (prStaRec) {
        ucWlanIndex = prStaRec->ucWlanIndex;
	} else if ((ucStaRecIdx == STA_REC_INDEX_BMCAST) && prBssInfo->fgIsInUse) {
        ucWlanIndex = prBssInfo->ucBMCWlanIndex;        
	}

    if(ucWlanIndex >= WTBL_SIZE) {
        DBGLOG(TX, WARN, ("%s: Unexpected WIDX[%u] BSS[%u] STA[%u], "
            "set WIDX to default value[%u]\n", ucWlanIndex, ucBssIdx,
            ucStaRecIdx, NIC_TX_DEFAULT_WLAN_INDEX));
        
        ucWlanIndex = NIC_TX_DEFAULT_WLAN_INDEX;
    }

    return ucWlanIndex;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief 
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @retval
*/
/*----------------------------------------------------------------------------*/
BOOLEAN nicTxIsMgmtResourceEnough(IN P_ADAPTER_T prAdapter)
{
    if(nicTxGetFreeCmdCount(prAdapter) > (CFG_TX_MAX_CMD_PKT_NUM / 2))
        return TRUE;
    else 
        return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief this function returns available count in command queue
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @retval
*/
/*----------------------------------------------------------------------------*/
UINT_32 nicTxGetFreeCmdCount(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    return prAdapter->rFreeCmdList.u4NumElem;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief this function returns page count of frame
*
* @param u4FrameLength      frame length
*
* @retval page count of this frame
*/
/*----------------------------------------------------------------------------*/
UINT_8 nicTxGetPageCount(IN UINT_32 u4FrameLength, IN BOOLEAN fgIncludeDesc)
{
    UINT_32 u4RequiredBufferSize;
    UINT_8  ucPageCount;

  /* Frame Buffer
    *  |<--Tx Descriptor-->|<--Tx descriptor padding-->|<--802.3/802.11 Header-->|<--Header padding-->|<--Payload-->|
    */

	if (fgIncludeDesc) {
        u4RequiredBufferSize = u4FrameLength;
	} else {
        u4RequiredBufferSize = 
		    NIC_TX_DESC_LONG_FORMAT_LENGTH + NIC_TX_DESC_PADDING_LENGTH + u4FrameLength;
    }

	if (NIC_TX_PAGE_SIZE_IS_POWER_OF_2) {
		ucPageCount =
		    (UINT_8) ((u4RequiredBufferSize +
			       (NIC_TX_PAGE_SIZE - 1)) >> NIC_TX_PAGE_SIZE_IN_POWER_OF_2);
	} else {
		ucPageCount =
		    (UINT_8) ((u4RequiredBufferSize + (NIC_TX_PAGE_SIZE - 1)) / NIC_TX_PAGE_SIZE);
    }

    return ucPageCount;
}

UINT_8 nicTxGetCmdPageCount(IN P_CMD_INFO_T prCmdInfo)
{
    UINT_8 ucPageCount;

	switch (prCmdInfo->eCmdType) {
        case COMMAND_TYPE_NETWORK_IOCTL:
        case COMMAND_TYPE_GENERAL_IOCTL:
            ucPageCount = nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE);
            break;
            
        case COMMAND_TYPE_SECURITY_FRAME:
        case COMMAND_TYPE_MANAGEMENT_FRAME:
            ucPageCount = nicTxGetPageCount(prCmdInfo->u2InfoBufLen, FALSE);
            break;

        default:
            DBGLOG(INIT, WARN, ("Undefined CMD Type(%u)\n", prCmdInfo->eCmdType));
            ucPageCount = nicTxGetPageCount(prCmdInfo->u2InfoBufLen, FALSE);
            break;
    }

    return ucPageCount;
}

VOID 
nicTxSetMngPacket(P_ADAPTER_T prAdapter,
    P_MSDU_INFO_T           prMsduInfo,
    UINT_8                  ucBssIndex,
    UINT_8                  ucStaRecIndex,
    UINT_8                  ucMacHeaderLength,
		  UINT_16 u2FrameLength, PFN_TX_DONE_HANDLER pfTxDoneHandler, UINT_8 ucRateMode)
{
    ASSERT(prMsduInfo);

    prMsduInfo->ucBssIndex = ucBssIndex;
    prMsduInfo->ucStaRecIndex = ucStaRecIndex;
    prMsduInfo->ucMacHeaderLength = ucMacHeaderLength;
    prMsduInfo->u2FrameLength = u2FrameLength;
    prMsduInfo->pfTxDoneHandler = pfTxDoneHandler;
    prMsduInfo->ucRateMode = ucRateMode;

    /* Reset default value for MMPDU */
    prMsduInfo->fgIs802_11 = TRUE;
    prMsduInfo->fgIs802_1x = FALSE;    
    prMsduInfo->u4FixedRateOption = 0;
    prMsduInfo->cPowerOffset = 0;
    prMsduInfo->u4Option = 0;
    prMsduInfo->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
    prMsduInfo->ucPID = NIC_TX_DESC_PID_RESERVED;
    prMsduInfo->ucPacketType = TX_PACKET_TYPE_MGMT;
    prMsduInfo->ucUserPriority = 0;
    prMsduInfo->eSrc = TX_PACKET_MGMT;
}

VOID
nicTxSetDataPacket(P_ADAPTER_T prAdapter,
    P_MSDU_INFO_T           prMsduInfo,
    UINT_8                  ucBssIndex, 
    UINT_8                  ucStaRecIndex,
    UINT_8                  ucMacHeaderLength,
    UINT_16                 u2FrameLength,
    PFN_TX_DONE_HANDLER     pfTxDoneHandler,
    UINT_8                  ucRateMode,
    ENUM_TX_PACKET_SRC_T    eSrc,    
		   UINT_8 ucTID, BOOLEAN fgIs802_11Frame, BOOLEAN fgIs1xFrame)
{
    ASSERT(prMsduInfo);

    prMsduInfo->ucBssIndex = ucBssIndex;
    prMsduInfo->ucStaRecIndex = ucStaRecIndex;
    prMsduInfo->ucMacHeaderLength = ucMacHeaderLength;
    prMsduInfo->u2FrameLength = u2FrameLength;
    prMsduInfo->pfTxDoneHandler = pfTxDoneHandler;
    prMsduInfo->ucRateMode = ucRateMode;
    prMsduInfo->ucUserPriority = ucTID;
    prMsduInfo->fgIs802_11 = fgIs802_11Frame;
    prMsduInfo->eSrc = eSrc;
    prMsduInfo->fgIs802_1x = fgIs1xFrame;    

    /* Reset default value for data packet */
    prMsduInfo->u4FixedRateOption = 0;
    prMsduInfo->cPowerOffset = 0;
    prMsduInfo->u4Option = 0;
    prMsduInfo->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
    prMsduInfo->ucPID = NIC_TX_DESC_PID_RESERVED;
    prMsduInfo->ucPacketType = TX_PACKET_TYPE_DATA;
}

VOID nicTxFillDescByPktOption(P_MSDU_INFO_T prMsduInfo, P_HW_MAC_TX_DESC_T prTxDesc)
{
    UINT_32 u4PktOption = prMsduInfo->u4Option;
    BOOLEAN fgIsLongFormat;
    BOOLEAN fgProtected = FALSE;

    /* Skip this function if no options is set */
	if (!u4PktOption) {
        return;
    }

    fgIsLongFormat = HAL_MAC_TX_DESC_IS_LONG_FORMAT(prTxDesc);
    
    /* Fields in DW0 and DW1 (Short Format) */
	if (u4PktOption & MSDU_OPT_NO_ACK) {
        HAL_MAC_TX_DESC_SET_NO_ACK(prTxDesc);
    }

	if (u4PktOption & MSDU_OPT_PROTECTED_FRAME) {
        HAL_MAC_TX_DESC_SET_PROTECTION(prTxDesc);
        fgProtected = TRUE;
    }    

	switch (HAL_MAC_TX_DESC_GET_HEADER_FORMAT(prTxDesc)) {
        case HEADER_FORMAT_802_11_ENHANCE_MODE:
		if (u4PktOption & MSDU_OPT_EOSP) {
                HAL_MAC_TX_DESC_SET_EOSP(prTxDesc);
            }     

		if (u4PktOption & MSDU_OPT_AMSDU) {
                HAL_MAC_TX_DESC_SET_AMSDU(prTxDesc);
            }              
            break;
            
        case HEADER_FORMAT_NON_802_11:
		if (u4PktOption & MSDU_OPT_EOSP) {
                HAL_MAC_TX_DESC_SET_EOSP(prTxDesc);
            }

		if (u4PktOption & MSDU_OPT_MORE_DATA) {
                HAL_MAC_TX_DESC_SET_MORE_DATA(prTxDesc);
            }          

		if (u4PktOption & MSDU_OPT_REMOVE_VLAN) {
                HAL_MAC_TX_DESC_SET_REMOVE_VLAN(prTxDesc);
            }                                                     
            break;            

        case HEADER_FORMAT_802_11_NORMAL_MODE:
		if (fgProtected && prMsduInfo->prPacket) {
                P_WLAN_MAC_HEADER_T prWlanHeader = 
			    (P_WLAN_MAC_HEADER_T) ((ULONG) (prMsduInfo->prPacket) +
						   MAC_TX_RESERVED_FIELD);

                prWlanHeader->u2FrameCtrl |= MASK_FC_PROTECTED_FRAME;
            }
            break;
            
        default:
            break;            
    }        

	if (!fgIsLongFormat) {
        return;
    }

    /* Fields in DW2~6 (Long Format) */
	if (u4PktOption & MSDU_OPT_NO_AGGREGATE) {
        HAL_MAC_TX_DESC_SET_BA_DISABLE(prTxDesc);
    }

	if (u4PktOption & MSDU_OPT_TIMING_MEASURE) {
        HAL_MAC_TX_DESC_SET_TIMING_MEASUREMENT(prTxDesc);
    }

	if (u4PktOption & MSDU_OPT_NDP) {
        HAL_MAC_TX_DESC_SET_NDP(prTxDesc);
    }
    
	if (u4PktOption & MSDU_OPT_NDPA) {
        HAL_MAC_TX_DESC_SET_NDPA(prTxDesc);
    }
    
	if (u4PktOption & MSDU_OPT_SOUNDING) {
        HAL_MAC_TX_DESC_SET_SOUNDING_FRAME(prTxDesc);
    }
    
	if (u4PktOption & MSDU_OPT_FORCE_RTS) {
        HAL_MAC_TX_DESC_SET_FORCE_RTS_CTS(prTxDesc);
    }
    
	if (u4PktOption & MSDU_OPT_BIP) {
        HAL_MAC_TX_DESC_SET_BIP(prTxDesc);
    }

    /* SW field */    
	if (u4PktOption & MSDU_OPT_SW_DURATION) {
        HAL_MAC_TX_DESC_SET_DURATION_CONTROL_BY_SW(prTxDesc);
    }
    
	if (u4PktOption & MSDU_OPT_SW_PS_BIT) {
        HAL_MAC_TX_DESC_SET_SW_PM_CONTROL(prTxDesc);
    }
    
	if (u4PktOption & MSDU_OPT_SW_HTC) {
        HAL_MAC_TX_DESC_SET_HTC_EXIST(prTxDesc);
    }
    
	if (u4PktOption & MSDU_OPT_SW_BAR_SN) {
        HAL_MAC_TX_DESC_SET_SW_BAR_SSN(prTxDesc);
    }    

	if (u4PktOption & MSDU_OPT_MANUAL_SN) {
        HAL_MAC_TX_DESC_SET_TXD_SN_VALID(prTxDesc);
        HAL_MAC_TX_DESC_SET_SEQUENCE_NUMBER(prTxDesc, prMsduInfo->u2SwSN);
    }
    
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Extra configuration for Tx packet
*
* @retval
*/
/*----------------------------------------------------------------------------*/
VOID nicTxConfigPktOption(P_MSDU_INFO_T prMsduInfo, UINT_32 u4OptionMask, BOOLEAN fgSetOption)
{
	if (fgSetOption) {
        prMsduInfo->u4Option |= u4OptionMask;
	} else {
        prMsduInfo->u4Option &= ~u4OptionMask;
    }
}

VOID nicTxFillDescByPktControl(P_MSDU_INFO_T prMsduInfo, P_HW_MAC_TX_DESC_T prTxDesc)
{
    UINT_8 ucPktControl = prMsduInfo->ucControlFlag;
    UINT_8 ucSwReserved;

    /* Skip this function if no options is set */
	if (!ucPktControl) {
        return;
    }

	if (HAL_MAC_TX_DESC_IS_LONG_FORMAT(prTxDesc)) {
        ucSwReserved = HAL_MAC_TX_DESC_GET_SW_RESERVED(prTxDesc);
        
		if (ucPktControl & MSDU_CONTROL_FLAG_FORCE_TX) {
            ucSwReserved |= MSDU_CONTROL_FLAG_FORCE_TX;
        }

        HAL_MAC_TX_DESC_SET_SW_RESERVED(prTxDesc, ucSwReserved);
    }
}


VOID
nicTxConfigPktControlFlag(P_MSDU_INFO_T prMsduInfo, UINT_8 ucControlFlagMask, BOOLEAN fgSetFlag)
{
    /* Set control flag */
	if (fgSetFlag) {
        prMsduInfo->ucControlFlag |= ucControlFlagMask;
    }
    /* Clear control flag */
    else {
        prMsduInfo->ucControlFlag &= ~ucControlFlagMask;
    }
}

VOID nicTxSetPktLifeTime(P_MSDU_INFO_T prMsduInfo, UINT_32 u4TxLifeTimeInMs)
{
    prMsduInfo->u4RemainingLifetime = u4TxLifeTimeInMs;
    prMsduInfo->u4Option |= MSDU_OPT_MANUAL_LIFE_TIME;
}

VOID nicTxSetPktRetryLimit(P_MSDU_INFO_T prMsduInfo, UINT_8 ucRetryLimit)
{
    prMsduInfo->ucRetryLimit = ucRetryLimit;
    prMsduInfo->u4Option |= MSDU_OPT_MANUAL_RETRY_LIMIT;
}

VOID nicTxSetPktPowerOffset(P_MSDU_INFO_T prMsduInfo, INT_8 cPowerOffset)
{
    prMsduInfo->cPowerOffset = cPowerOffset;
    prMsduInfo->u4Option |= MSDU_OPT_MANUAL_POWER_OFFSET;
}

VOID nicTxSetPktSequenceNumber(P_MSDU_INFO_T prMsduInfo, UINT_16 u2SN)
{
    prMsduInfo->u2SwSN = u2SN;
    prMsduInfo->u4Option |= MSDU_OPT_MANUAL_SN;
}

VOID nicTxSetPktMacTxQue(P_MSDU_INFO_T prMsduInfo, UINT_8 ucMacTxQue)
{
    UINT_8 ucTcIdx;

	for (ucTcIdx = TC0_INDEX; ucTcIdx < TC_NUM; ucTcIdx++) {
		if (arTcResourceControl[ucTcIdx].ucDestQueueIndex == ucMacTxQue) {
            break;
        }
    }

	if (ucTcIdx < TC_NUM) {
        prMsduInfo->ucTC = ucTcIdx;
        prMsduInfo->u4Option |= MSDU_OPT_MANUAL_TX_QUE;
    }
}

VOID
nicTxSetPktFixedRateOptionFull(P_MSDU_INFO_T prMsduInfo,
    UINT_16         u2RateCode,
    UINT_8          ucBandwidth,
    BOOLEAN         fgShortGI,
    BOOLEAN         fgLDPC,
    BOOLEAN         fgDynamicBwRts,
    BOOLEAN         fgSpatialExt,
    BOOLEAN         fgEtxBeamforming,
    BOOLEAN         fgItxBeamforming,
			       UINT_8 ucAntennaIndex, UINT_8 ucAntennaPriority)
{
    HW_MAC_TX_DESC_T rTxDesc;
    P_HW_MAC_TX_DESC_T prTxDesc = &rTxDesc;

    kalMemZero(prTxDesc, NIC_TX_DESC_LONG_FORMAT_LENGTH);

    /* Follow the format of Tx descriptor DW 6 */
    HAL_MAC_TX_DESC_SET_FR_RATE(prTxDesc, u2RateCode);

	if (ucBandwidth) {
        HAL_MAC_TX_DESC_SET_FR_BW(prTxDesc, ucBandwidth);
    }

	if (fgShortGI) {
        HAL_MAC_TX_DESC_SET_FR_SHORT_GI(prTxDesc);
    }

	if (fgLDPC) {
        HAL_MAC_TX_DESC_SET_FR_LDPC(prTxDesc);
    }

	if (fgDynamicBwRts) {
        HAL_MAC_TX_DESC_SET_FR_DYNAMIC_BW_RTS(prTxDesc);
    }

	if (fgSpatialExt) {
        HAL_MAC_TX_DESC_SET_FR_SPE_EN(prTxDesc);
    }

	if (fgEtxBeamforming) {
        HAL_MAC_TX_DESC_SET_FR_ETX_BF(prTxDesc);
    }
    
	if (fgItxBeamforming) {
        HAL_MAC_TX_DESC_SET_FR_ITX_BF(prTxDesc);
    }    

    HAL_MAC_TX_DESC_SET_FR_ANTENNA_ID(prTxDesc, ucAntennaIndex);

    HAL_MAC_TX_DESC_SET_FR_ANTENNA_PRIORITY(prTxDesc, ucAntennaPriority);

    /* Write back to RateOption of MSDU_INFO */
    HAL_MAC_TX_DESC_GET_DW(prTxDesc, 6, 1, &prMsduInfo->u4FixedRateOption);

}

VOID
nicTxSetPktFixedRateOption(P_MSDU_INFO_T prMsduInfo,
    UINT_16         u2RateCode,
			   UINT_8 ucBandwidth, BOOLEAN fgShortGI, BOOLEAN fgDynamicBwRts)
{
    HW_MAC_TX_DESC_T rTxDesc;
    P_HW_MAC_TX_DESC_T prTxDesc = &rTxDesc;

    kalMemZero(prTxDesc, NIC_TX_DESC_LONG_FORMAT_LENGTH);

    /* Follow the format of Tx descriptor DW 6 */    
    HAL_MAC_TX_DESC_SET_FR_RATE(prTxDesc, u2RateCode);

	if (ucBandwidth) {
        HAL_MAC_TX_DESC_SET_FR_BW(prTxDesc, ucBandwidth);
    }

	if (fgShortGI) {
        HAL_MAC_TX_DESC_SET_FR_SHORT_GI(prTxDesc);
    }

	if (fgDynamicBwRts) {
        HAL_MAC_TX_DESC_SET_FR_DYNAMIC_BW_RTS(prTxDesc);
    }

    /* Write back to RateOption of MSDU_INFO */
    HAL_MAC_TX_DESC_GET_DW(prTxDesc, 6, 1, &prMsduInfo->u4FixedRateOption);

}

VOID nicTxSetPktMoreData(P_MSDU_INFO_T prCurrentMsduInfo, BOOLEAN fgSetMoreDataBit)
{
    P_WLAN_MAC_HEADER_T prWlanMacHeader = NULL;

	if (prCurrentMsduInfo->fgIs802_11) {
		prWlanMacHeader =
		    (P_WLAN_MAC_HEADER_T) (((PUINT_8) (prCurrentMsduInfo->prPacket)) +
					   MAC_TX_RESERVED_FIELD);
    }
    
	if (fgSetMoreDataBit) {
		if (!prCurrentMsduInfo->fgIs802_11) {
            prCurrentMsduInfo->u4Option |= MSDU_OPT_MORE_DATA;
		} else {
            prWlanMacHeader->u2FrameCtrl |= MASK_FC_MORE_DATA;
        }
	} else {
		if (!prCurrentMsduInfo->fgIs802_11) {
            prCurrentMsduInfo->u4Option &= ~MSDU_OPT_MORE_DATA;
		} else {
            prWlanMacHeader->u2FrameCtrl &= ~MASK_FC_MORE_DATA;
        }
    }
}

UINT_8 nicTxAssignPID(IN P_ADAPTER_T prAdapter, IN UINT_8 ucWlanIndex)
{
    UINT_8 ucRetval;
    PUINT_8 pucPidPool;

    ASSERT(prAdapter);

    pucPidPool = &prAdapter->aucPidPool[ucWlanIndex];

    ucRetval = *pucPidPool;

    /* Driver side Tx Sequence number: 1~127 */
    (*pucPidPool)++;

	if (*pucPidPool > NIC_TX_DESC_DRIVER_PID_MAX) {
        *pucPidPool = NIC_TX_DESC_DRIVER_PID_MIN;
    }

    return ucRetval;
}

VOID nicTxSetPktEOSP(P_MSDU_INFO_T prCurrentMsduInfo, BOOLEAN fgSetEOSPBit)
{
    P_WLAN_MAC_HEADER_QOS_T prWlanMacHeader = NULL;
    BOOLEAN fgWriteToDesc = TRUE;

	if (prCurrentMsduInfo->fgIs802_11) {
		prWlanMacHeader =
		    (P_WLAN_MAC_HEADER_QOS_T) (((PUINT_8) (prCurrentMsduInfo->prPacket)) +
					       MAC_TX_RESERVED_FIELD);
        fgWriteToDesc = FALSE;
    }
    
	if (fgSetEOSPBit) {
		if (fgWriteToDesc) {
            prCurrentMsduInfo->u4Option |= MSDU_OPT_EOSP;
		} else {
            prWlanMacHeader->u2QosCtrl |= MASK_QC_EOSP;
        }
	} else {
		if (fgWriteToDesc) {
            prCurrentMsduInfo->u4Option &= ~MSDU_OPT_EOSP;
		} else {
            prWlanMacHeader->u2QosCtrl &= ~MASK_QC_EOSP;
        }
    }
}

WLAN_STATUS
nicTxDummyTxDone(IN P_ADAPTER_T prAdapter,
		 IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
    DBGLOG(TX, TRACE, ("Msdu WIDX:PID[%u:%u] SEQ[%u] Tx Status[%u]\n", 
        prMsduInfo->ucWlanIndex,
			   prMsduInfo->ucPID, prMsduInfo->ucTxSeqNum, rTxDoneStatus));

    return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Update BSS Tx Params
*
* @param prStaRec The peer
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxUpdateBssDefaultRate(P_BSS_INFO_T prBssInfo)
{
    UINT_8              ucLowestBasicRateIndex;
    
    prBssInfo->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
    
	/* 4 <1> Find Lowest Basic Rate Index for default TX Rate of MMPDU */
	if (rateGetLowestRateIndexFromRateSet
	    (prBssInfo->u2BSSBasicRateSet, &ucLowestBasicRateIndex)) {
		nicRateIndex2RateCode(PREAMBLE_DEFAULT_LONG_NONE, ucLowestBasicRateIndex,
            &prBssInfo->u2HwDefaultFixedRateCode);
	} else {
		switch (prBssInfo->ucNonHTBasicPhyType) {
            case PHY_TYPE_ERP_INDEX:
            case PHY_TYPE_OFDM_INDEX:
                prBssInfo->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
                break;

            default:
                prBssInfo->u2HwDefaultFixedRateCode = RATE_CCK_1M_LONG;
                break;
        }
    }   
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Update StaRec Tx parameters
*
* @param prStaRec The peer
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
VOID nicTxUpdateStaRecDefaultRate(P_STA_RECORD_T prStaRec)
{
    UINT_8                      ucLowestBasicRateIndex;

    prStaRec->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
     
	/* 4 <1> Find Lowest Basic Rate Index for default TX Rate of MMPDU */
    if (rateGetLowestRateIndexFromRateSet(prStaRec->u2BSSBasicRateSet, &ucLowestBasicRateIndex)) {
		nicRateIndex2RateCode(PREAMBLE_DEFAULT_LONG_NONE,
				      ucLowestBasicRateIndex, &prStaRec->u2HwDefaultFixedRateCode);
	} else {
        if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_SET_802_11B) {
            prStaRec->u2HwDefaultFixedRateCode = RATE_CCK_1M_LONG;
		} else if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_SET_802_11G) {
            prStaRec->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
		} else if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_SET_802_11A) {
            prStaRec->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
		} else if (prStaRec->ucDesiredPhyTypeSet & PHY_TYPE_SET_802_11N) {
            prStaRec->u2HwDefaultFixedRateCode = RATE_MM_MCS_0;
        }
    }
}
