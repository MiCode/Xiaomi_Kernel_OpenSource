/*! \file   wlan_lib.c
    \brief  Internal driver stack will export the required procedures here for GLUE Layer.

    This file contains all routines which are exported from MediaTek 802.11 Wireless
    LAN driver stack to GLUE Layer.
*/

/*
** $Log: wlan_lib.c $
**
** 04 02 2014 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch]
** revise apusd as enable
**
** 01 15 2014 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch][MT6630][Driver]MT6630 Wi-Fi Patch
** Merging
**  	
** 	//ALPS_SW/DEV/ALPS.JB2.MT6630.DEV/alps/mediatek/kernel/drivers/combo/drv_wlan/mt6630/wlan/...
**  	
** 	to //ALPS_SW/TRUNK/KK/alps/mediatek/kernel/drivers/combo/drv_wlan/mt6630/wlan/...
**
** 10 09 2013 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch][MT6630][Driver]MT6630 Wi-Fi Patch
** turn off nvram power
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
** 08 05 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** for windows build success
**
** 07 31 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Fix NetDev binding issue
**
** 07 30 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Temp fix Hot-spot data path issue.
**
** 07 28 2013 eddie.chen
** [BORA00002450] [WIFISYS][MT6630] New design for mt6630
** Save the compileflag and featureflag
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
** 07 04 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx path for 1x packet
**
** 07 04 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update for 1st Connection.
**
** 06 27 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Refine management frame Tx function
**
** 06 19 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** update MAC address handling logic
**
** 06 18 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** use RX port #0 instead of #1 for communicating with Wi-Fi download agent
**
** 06 18 2013 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Get MAC address by NIC_CAPABILITY command
**
** 03 12 2013 tsaiyuan.hsu
** [BORA00002222] MT6630 unified MAC RXM
** remove hif_rx_hdr usage.
**
** 03 12 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx utility function for management frame
**
** 03 06 2013 wh.su
** [BORA00002446] [MT6630] [Wi-Fi] [Driver] Update the security function code
** submit some code related with security.
**
** 02 19 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** take use of GET_BSS_INFO_BY_INDEX() and MAX_BSS_INDEX macros
** for correctly indexing of BSS-INFO pointers
**
** 02 06 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** add reset option for firmware download configuration
**
** 02 05 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. change to use long format (FT=1) for initial command
** 2. fix a typo
**
** 02 01 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. eliminate MT5931/MT6620/MT6628 logic
** 2. add firmware download control sequence
**
** 01 24 2013 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Mark some code segment for compiling error
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
** 01 21 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update TX path based on new ucBssIndex modifications.
**
** 01 17 2013 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Use ucBssIndex to replace eNetworkTypeIndex
**
** 01 15 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update Tx done resource release mechanism.
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
** 09 04 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** sync for NVRAM warning scan result generation for CFG80211.
**
** 08 24 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** .
**
** 08 24 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** cfg80211 support merge back from ALPS.JB to DaVinci - MT6620 Driver v2.3 branch.
**
** 08 15 2012 eason.tsai
** NULL
** fix build warning.
 *
 * 07 13 2012 cp.wu
 * [WCXRP00001259] [MT6620 Wi-Fi][Driver][Firmware] Send a signal to firmware for termination after SDIO error has happened
 * [driver domain] add force reset by host-to-device interrupt mechanism
 *
 * 06 11 2012 cp.wu
 * [WCXRP00001252] [MT6620 Wi-Fi][Driver] Add debug message while encountering firmware response timeout
 * output message while timeout event occurs
 *
 * 06 11 2012 eason.tsai
 * NULL
 * change from binay to hex code
 *
 * 06 08 2012 eason.tsai
 * NULL
 * Nvram context covert from 6620 to 6628 for old 6620 meta tool
 *
 * 05 11 2012 cp.wu
 * [WCXRP00001237] [MT6620 Wi-Fi][Driver] Show MAC address and MAC address source for ACS's convenience
 * show MAC address & source while initiliazation
 *
 * 03 29 2012 eason.tsai
 * [WCXRP00001216] [MT6628 Wi-Fi][Driver]add conditional define
 * add conditional define.
 *
 * 03 04 2012 eason.tsai
 * NULL
 * modify the cal fail report code.
 *
 * 03 02 2012 terry.wu
 * NULL
 * Sync CFG80211 modification from branch 2,2.
 *
 * 01 16 2012 cp.wu
 * [WCXRP00001169] [MT6620 Wi-Fi][Driver] API and behavior modification for preferred band configuration with corresponding network configuration
 * correct scan result removing policy.
 *
 * 01 16 2012 cp.wu
 * [MT6620 Wi-Fi][Driver] API and behavior modification for preferred band configuration with corresponding network configuration
 * add wlanSetPreferBandByNetwork() for glue layer to invoke for setting preferred band configuration corresponding to network type.
 *
 * 01 05 2012 wh.su
 * [WCXRP00001153] [MT6620 Wi-Fi][Driver] Adding the get_ch_list and set_tx_power proto type function
 * Adding the related ioctl / wlan oid function to set the Tx power cfg.
 *
 * 11 28 2011 cp.wu
 * [WCXRP00001125] [MT6620 Wi-Fi][Firmware] Strengthen Wi-Fi power off sequence to have a clearroom environment when returining to ROM code
 * 1. Due to firmware now stops HIF DMA for powering off, do not try to receive any packet from firmware
 * 2. Take use of prAdapter->fgIsEnterD3ReqIssued for tracking whether it is powering off or not
 *
 * 11 14 2011 cm.chang
 * [WCXRP00001104] [All Wi-Fi][FW] Show init process by HW mail-box register
 * Show FW initial ID when timeout to wait for ready bit
 *
 * 11 11 2011 wh.su
 * [WCXRP00001078] [MT6620 Wi-Fi][Driver] Adding the mediatek log improment support : XLOG
 * modify the xlog related code.
 *
 * 10 18 2011 cp.wu
 * [WCXRP00001022] [MT6628 Driver][Firmware Download] Add multi section independent download functionality
 * when powering off, always clear pending interrupts, then wait for RDY to be de-asserted
 *
 * 10 14 2011 cp.wu
 * [WCXRP00001022] [MT6628 Driver][Firmware Download] Add multi section independent download functionality
 * shorten the packet length for firmware download if no more than 2048 bytes.
 *
 * 10 03 2011 cp.wu
 * [WCXRP00001022] [MT6628 Driver][Firmware Download] Add multi section independent download functionality
 * add firmware download path in divided scatters.
 *
 * 10 03 2011 cp.wu
 * [MT6628 Driver][Firmware Download] Add multi section independent download functionality
 * add firmware downloading aggregated path.
 *
 * 09 30 2011 cm.chang
 * [WCXRP00001020] [MT6620 Wi-Fi][Driver] Handle secondary channel offset of AP in 5GHz band
 * .
 *
 * 09 20 2011 cp.wu
 * [WCXRP00000994] [MT6620 Wi-Fi][Driver] dump message for bus error and reset bus error flag while re-initialized
 * 1. always show error message for SDIO bus errors.
 * 2. reset bus error flag when re-initialization
 *
 * 08 26 2011 cm.chang
 * [WCXRP00000952] [MT5931 Wi-Fi][FW] Handshake with BWCS before DPD/TX power calibration
 * Fix compiling error for WinXP MT5931 driver
 *
 * 08 25 2011 chinghwa.yu
 * [WCXRP00000063] Update BCM CoEx design and settings
 * Add BWCS Sync ready for WinXP.
 *
 * 08 25 2011 chinghwa.yu
 * [WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Add DFS switch.
 *
 * 08 24 2011 chinghwa.yu
 * [WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Update RDD test mode cases.
 *
 * 08 19 2011 cp.wu
 * [WCXRP00000702] [MT5931][Driver] Modify initialization sequence for E1 ASIC
 * escape from normal path if any error is occured.
 *
 * 08 15 2011 cp.wu
 * [WCXRP00000851] [MT6628 Wi-Fi][Driver] Add HIFSYS related definition to driver source tree
 * reuse firmware download logic of MT6620 for MT6628.
 *
 * 08 15 2011 cp.wu
 * [WCXRP00000913] [MT6620 Wi-Fi] create repository of source code dedicated for MT6620 E6 ASIC
 * support to load different firmware image for E3/E4/E5 and E6 ASIC on win32 platforms.
 *
 * 08 02 2011 yuche.tsai
 * [WCXRP00000896] [Volunteer Patch][WiFi Direct][Driver] GO with multiple client, TX deauth to a disconnecting device issue.
 * Fix GO send deauth frame issue.
 *
 * 07 22 2011 jeffrey.chang
 * [WCXRP00000864] [MT5931] Add command to adjust OSC stable time
 * modify driver to set OSC stable time after f/w download
 *
 * 07 18 2011 chinghwa.yu
 * [WCXRP00000063] Update BCM CoEx design and settings[WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Add CMD/Event for RDD and BWCS.
 *
 * 06 24 2011 cp.wu
 * [WCXRP00000812] [MT6620 Wi-Fi][Driver] not show NVRAM when there is no valid MAC address in NVRAM content
 * if there is no valid address in chip, generate a new one from driver domain instead of firmware domain due to sufficient randomness
 *
 * 06 23 2011 cp.wu
 * [WCXRP00000812] [MT6620 Wi-Fi][Driver] not show NVRAM when there is no valid MAC address in NVRAM content
 * check with firmware for valid MAC address.
 *
 * 06 20 2011 cp.wu
 * [WCXRP00000702] [MT5931][Driver] Modify initialization sequence for E1 ASIC
 * disable whole-chip resetting mechanism due to the need of further ECO to work as expected.
 *
 * 05 31 2011 cp.wu
 * [WCXRP00000749] [MT6620 Wi-Fi][Driver] Add band edge tx power control to Wi-Fi NVRAM
 * changed to use non-zero checking for valid bit in NVRAM content
 *
 * 05 27 2011 cp.wu
 * [WCXRP00000749] [MT6620 Wi-Fi][Driver] Add band edge tx power control to Wi-Fi NVRAM
 * invoke CMD_ID_SET_EDGE_TXPWR_LIMIT when there is valid data exist in NVRAM content.
 *
 * 05 18 2011 cp.wu
 * [WCXRP00000734] [MT6620 Wi-Fi][Driver] Pass PHY_PARAM in NVRAM to firmware domain
 * pass PHY_PARAM in NVRAM from driver to firmware.
 *
 * 05 11 2011 cp.wu
 * [WCXRP00000718] [MT6620 Wi-Fi] modify the behavior of setting tx power
 * correct assertion.
 *
 * 05 11 2011 cp.wu
 * [WCXRP00000718] [MT6620 Wi-Fi] modify the behavior of setting tx power
 * ACPI APIs migrate to wlan_lib.c for glue layer to invoke.
 *
 * 05 11 2011 cm.chang
 * [WCXRP00000717] [MT5931 Wi-Fi][Driver] Handle wrong NVRAM content about AP bandwidth setting
 * .
 *
 * 05 05 2011 cp.wu
 * [WCXRP00000702] [MT5931][Driver] Modify initialization sequence for E1 ASIC
 * change delay from 100ms to 120ms upon DE's suggestion.
 *
 * 05 05 2011 cp.wu
 * [WCXRP00000702] [MT5931][Driver] Modify initialization sequence for E1 ASIC
 * add delay after whole-chip resetting for MT5931 E1 ASIC.
 *
 * 04 22 2011 cp.wu
 * [WCXRP00000598] [MT6620 Wi-Fi][Driver] Implementation of interface for communicating with user space process for RESET_START and RESET_END events
 * skip power-off handshaking when RESET indication is received.
 *
 * 04 22 2011 george.huang
 * [WCXRP00000621] [MT6620 Wi-Fi][Driver] Support P2P supplicant to set power mode
 * .
 *
 * 04 18 2011 cp.wu
 * [WCXRP00000636] [WHQL][MT5931 Driver] 2c_PMHibernate (hang on 2h)
 * 1) add API for glue layer to query ACPI state
 * 2) Windows glue should not access to hardware after switched into D3 state
 *
 * 04 15 2011 cp.wu
 * [WCXRP00000654] [MT6620 Wi-Fi][Driver] Add loop termination criterion for wlanAdapterStop().
 * add loop termination criteria for wlanAdapterStop().
 *
 * 04 12 2011 eddie.chen
 * [WCXRP00000617] [MT6620 Wi-Fi][DRV/FW] Fix for sigma
 * Fix the sta index in processing security frame
 * Simple flow control for TC4 to avoid mgt frames for PS STA to occupy the TC4
 * Add debug message.
 *
 * 04 12 2011 cp.wu
 * [WCXRP00000631] [MT6620 Wi-Fi][Driver] Add an API for QM to retrieve current TC counter value and processing frame dropping cases for TC4 path
 * 1. add nicTxGetResource() API for QM to make decisions.
 * 2. if management frames is decided by QM for dropping, the call back is invoked to indicate such a case.
 *
 * 04 06 2011 cp.wu
 * [WCXRP00000616] [MT6620 Wi-Fi][Driver] Free memory to pool and kernel in case any unexpected failure happend inside wlanAdapterStart
 * invoke nicReleaseAdapterMemory() as failure handling in case wlanAdapterStart() failed unexpectedly
 *
 * 03 29 2011 wh.su
 * [WCXRP00000248] [MT6620 Wi-Fi][FW]Fixed the Klockwork error
 * fixed the kclocwork error.
 *
 * 03 15 2011 cp.wu
 * [WCXRP00000559] [MT6620 Wi-Fi][Driver] Combine TX/RX DMA buffers into a single one to reduce physically continuous memory consumption
 * 1. deprecate CFG_HANDLE_IST_IN_SDIO_CALLBACK
 * 2. Use common coalescing buffer for both TX/RX directions
 *
 *
 * 03 10 2011 cp.wu
 * [WCXRP00000532] [MT6620 Wi-Fi][Driver] Migrate NVRAM configuration procedures from MT6620 E2 to MT6620 E3
 * deprecate configuration used by MT6620 E2
 *
 * 03 07 2011 terry.wu
 * [WCXRP00000521] [MT6620 Wi-Fi][Driver] Remove non-standard debug message
 * Toggle non-standard debug messages to comments.
 *
 * 02 25 2011 cp.wu
 * [WCXRP00000496] [MT5931][Driver] Apply host-triggered chip reset before initializing firmware download procedures
 * apply host-triggered chip reset mechanism before initializing firmware download procedures.
 *
 * 02 17 2011 eddie.chen
 * [WCXRP00000458] [MT6620 Wi-Fi][Driver] BOW Concurrent - ProbeResp was exist in other channel
 * 1) Chnage GetFrameAction decision when BSS is absent.
 * 2) Check channel and resource in processing ProbeRequest
 *
 * 02 16 2011 cm.chang
 * [WCXRP00000447] [MT6620 Wi-Fi][FW] Support new NVRAM update mechanism
 * .
 *
 * 02 01 2011 george.huang
 * [WCXRP00000333] [MT5931][FW] support SRAM power control drivers
 * init variable for CTIA.
 *
 * 01 27 2011 george.huang
 * [WCXRP00000355] [MT6620 Wi-Fi] Set WMM-PS related setting with qualifying AP capability
 * Support current measure mode, assigned by registry (XP only).
 *
 * 01 24 2011 cp.wu
 * [WCXRP00000382] [MT6620 Wi-Fi][Driver] Track forwarding packet number with notifying tx thread for serving
 * 1. add an extra counter for tracking pending forward frames.
 * 2. notify TX service thread as well when there is pending forward frame
 * 3. correct build errors leaded by introduction of Wi-Fi direct separation module
 *
 * 01 12 2011 cm.chang
 * [WCXRP00000354] [MT6620 Wi-Fi][Driver][FW] Follow NVRAM bandwidth setting
 * User-defined bandwidth is for 2.4G and 5G individually
 *
 * 01 10 2011 cp.wu
 * [WCXRP00000351] [MT6620 Wi-Fi][Driver] remove from scanning result in OID handling layer when the corresponding BSS is disconnected due to beacon timeout
 * remove from scanning result when the BSS is disconnected due to beacon timeout.
 *
 * 01 04 2011 cp.wu
 * [WCXRP00000338] [MT6620 Wi-Fi][Driver] Separate kalMemAlloc into kmalloc and vmalloc implementations to ease physically continous memory demands
 * separate kalMemAlloc() into virtually-continous and physically-continous type to ease slab system pressure
 *
 * 12 31 2010 cp.wu
 * [WCXRP00000327] [MT6620 Wi-Fi][Driver] Improve HEC WHQA 6972 workaround coverage in driver side
 * while being unloaded, clear all pending interrupt then set LP-own to firmware
 *
 * 12 31 2010 cp.wu
 * [WCXRP00000335] [MT6620 Wi-Fi][Driver] change to use milliseconds sleep instead of delay to avoid blocking to system scheduling
 * change to use msleep() and shorten waiting interval to reduce blocking to other task while Wi-Fi driver is being loaded
 *
 * 12 28 2010 cp.wu
 * [WCXRP00000269] [MT6620 Wi-Fi][Driver][Firmware] Prepare for v1.1 branch release
 * report EEPROM used flag via NIC_CAPABILITY
 *
 * 12 28 2010 cp.wu
 * [WCXRP00000269] [MT6620 Wi-Fi][Driver][Firmware] Prepare for v1.1 branch release
 * integrate with 'EEPROM used' flag for reporting correct capability to Engineer Mode/META and other tools
 *
 * 12 22 2010 eddie.chen
 * [WCXRP00000218] [MT6620 Wi-Fi][Driver] Add auto rate window control in registry
 * Remove controling auto rate from initial setting. The initial setting is defined by FW code.
 *
 * 12 15 2010 cp.wu
 * NULL
 * sync. with ALPS code by enabling interrupt just before leaving wlanAdapterStart()
 *
 * 12 08 2010 yuche.tsai
 * [WCXRP00000245] [MT6620][Driver] Invitation & Provision Discovery Feature Check-in
 * Change Param name for invitation connection.
 *
 * 12 07 2010 cm.chang
 * [WCXRP00000238] MT6620 Wi-Fi][Driver][FW] Support regulation domain setting from NVRAM and supplicant
 * 1. Country code is from NVRAM or supplicant
 * 2. Change band definition in CMD/EVENT.
 *
 * 11 03 2010 cp.wu
 * [WCXRP00000083] [MT5931][Driver][FW] Add necessary logic for MT5931 first connection
 * 1) use 8 buffers for MT5931 which is equipped with less memory
 * 2) modify MT5931 debug level to TRACE when download is successful
 *
 * 11 02 2010 cp.wu
 * [WCXRP00000083] [MT5931][Driver][FW] Add necessary logic for MT5931 first connection
 * for MT5931, adapter initialization is done *after* firmware is downloaded.
 *
 * 11 02 2010 cp.wu
 * [WCXRP00000083] [MT5931][Driver][FW] Add necessary logic for MT5931 first connection
 * correct MT5931 firmware download procedure:
 * MT5931 will download firmware first then acquire LP-OWN
 *
 * 11 02 2010 cp.wu
 * [WCXRP00000083] [MT5931][Driver][FW] Add necessary logic for MT5931 first connection
 * 1) update MT5931 firmware encryption tool. (using 64-bytes unit)
 * 2) update MT5931 firmware download procedure
 *
 * 11 01 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000150] [MT6620 Wi-Fi][Driver] Add implementation for querying current TX rate from firmware auto rate module
 * 1) Query link speed (TX rate) from firmware directly with buffering mechanism to reduce overhead
 * 2) Remove CNM CH-RECOVER event handling
 * 3) cfg read/write API renamed with kal prefix for unified naming rules.
 *
 * 11 01 2010 yarco.yang
 * [WCXRP00000149] [MT6620 WI-Fi][Driver]Fine tune performance on MT6516 platform
 * Add code to run WlanIST in SDIO callback.
 *
 * 10 27 2010 george.huang
 * [WCXRP00000127] [MT6620 Wi-Fi][Driver] Add a registry to disable Beacon Timeout function for SQA test by using E1 EVB
 * Support registry option for disable beacon lost detection.
 *
 * 10 26 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000137] [MT6620 Wi-Fi] [FW] Support NIC capability query command
 * 1) update NVRAM content template to ver 1.02
 * 2) add compile option for querying NIC capability (default: off)
 * 3) modify AIS 5GHz support to run-time option, which could be turned on by registry or NVRAM setting
 * 4) correct auto-rate compiler error under linux (treat warning as error)
 * 5) simplify usage of NVRAM and REG_INFO_T
 * 6) add version checking between driver and firmware
 *
 * 10 26 2010 eddie.chen
 * [WCXRP00000134] [MT6620 Wi-Fi][Driver] Add a registry to enable auto rate for SQA test by using E1 EVB
 * Add auto rate parameter in registry.
 *
 * 10 25 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check
 * add option for enable/disable TX PWR gain adjustment (default: off)
 *
 * 10 18 2010 cp.wu
 * [WCXRP00000117] [MT6620 Wi-Fi][Driver] Add logic for suspending driver when MT6620 is not responding anymore
 * 1. when wlanAdapterStop() failed to send POWER CTRL command to firmware, do not poll for ready bit dis-assertion
 * 2. shorten polling count for shorter response time
 * 3. if bad I/O operation is detected during TX resource polling, then further operation is aborted as well
 *
 * 10 18 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000086] [MT6620 Wi-Fi][Driver] The mac address is all zero at android
 * complete implementation of Android NVRAM access
 *
 * 10 15 2010 cp.wu
 * [WCXRP00000103] [MT6620 Wi-Fi][Driver] Driver crashed when using WZC to connect to AP#B with connection with AP#A
 * bugfix: always reset pointer to IEbuf to zero when keeping scanning result for the connected AP
 *
 * 10 08 2010 cp.wu
 * [WCXRP00000084] [MT6620 Wi-Fi][Driver][FW] Add fixed rate support for distance test
 * adding fixed rate support for distance test. (from registry setting)
 *
 * 10 07 2010 cp.wu
 * [WCXRP00000083] [MT5931][Driver][FW] Add necessary logic for MT5931 first connection
 * add firmware download for MT5931.
 *
 * 10 06 2010 cp.wu
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * divide a single function into 2 part to surpress a weird compiler warning from gcc-4.4.0
 *
 * 10 06 2010 cp.wu
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * code reorganization to improve isolation between GLUE and CORE layers.
 *
 * 10 05 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check
 * load manufacture data when CFG_SUPPORT_NVRAM is set to 1
 *
 * 10 04 2010 cp.wu
 * [WCXRP00000077] [MT6620 Wi-Fi][Driver][FW] Eliminate use of ENUM_NETWORK_TYPE_T and replaced by ENUM_NETWORK_TYPE_INDEX_T only
 * remove ENUM_NETWORK_TYPE_T definitions
 *
 * 09 29 2010 wh.su
 * [WCXRP00000072] [MT6620 Wi-Fi][Driver] Fix TKIP Counter Measure EAPoL callback register issue
 * [MT6620 Wi-Fi][Driver] Fix TKIP Counter Measure EAPoL callback register issue.
 *
 * 09 24 2010 cp.wu
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * eliminate unused variables which lead gcc to argue
 *
 * 09 24 2010 cp.wu
 * [WCXRP00000057] [MT6620 Wi-Fi][Driver] Modify online scan to a run-time switchable feature
 * Modify online scan as a run-time adjustable option (for Windows, in registry)
 *
 * 09 23 2010 cp.wu
 * [WCXRP00000051] [MT6620 Wi-Fi][Driver] WHQL test fail in MAC address changed item
 * use firmware reported mac address right after wlanAdapterStart() as permanent address
 *
 * 09 23 2010 cp.wu
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * eliminate reference of CFG_RESPONSE_MAX_PKT_SIZE
 *
 * 09 21 2010 cp.wu
 * [WCXRP00000053] [MT6620 Wi-Fi][Driver] Reset incomplete and might leads to BSOD when entering RF test with AIS associated
 * Do a complete reset with STA-REC null checking for RF test re-entry
 *
 * 09 21 2010 kevin.huang
 * [WCXRP00000052] [MT6620 Wi-Fi][Driver] Eliminate Linux Compile Warning
 * Eliminate Linux Compile Warning
 *
 * 09 13 2010 cp.wu
 * NULL
 * acquire & release power control in oid handing wrapper.
 *
 * 09 09 2010 cp.wu
 * NULL
 * move IE to buffer head when the IE pointer is not pointed at head.
 *
 * 09 08 2010 cp.wu
 * NULL
 * use static memory pool for storing IEs of scanning result.
 *
 * 09 01 2010 cp.wu
 * NULL
 * HIFSYS Clock Source Workaround
 *
 * 09 01 2010 wh.su
 * NULL
 * adding the wapi support for integration test.
 *
 * 09 01 2010 cp.wu
 * NULL
 * move HIF CR initialization from where after sdioSetupCardFeature() to wlanAdapterStart()
 *
 * 08 30 2010 cp.wu
 * NULL
 * eliminate klockwork errors
 *
 * 08 26 2010 yuche.tsai
 * NULL
 * Add AT GO test configure mode under WinXP.
 * Please enable 1. CFG_ENABLE_WIFI_DIRECT, 2. CFG_TEST_WIFI_DIRECT_GO, 3. CFG_SUPPORT_AAA
 *
 * 08 25 2010 george.huang
 * NULL
 * update OID/ registry control path for PM related settings
 *
 * 08 24 2010 cp.wu
 * NULL
 * 1) initialize variable for enabling short premable/short time slot.
 * 2) add compile option for disabling online scan
 *
 * 08 13 2010 cp.wu
 * NULL
 * correction issue: desired phy type not initialized as ABGN mode.
 *
 * 08 12 2010 cp.wu
 * NULL
 * [AIS-FSM] honor registry setting for adhoc running mode. (A/B/G)
 *
 * 08 10 2010 cm.chang
 * NULL
 * Support EEPROM read/write in RF test mode
 *
 * 08 03 2010 cp.wu
 * NULL
 * surpress compilation warning.
 *
 * 08 03 2010 cp.wu
 * NULL
 * Centralize mgmt/system service procedures into independent calls.
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
 * 07 28 2010 cp.wu
 * NULL
 * 1) eliminate redundant variable eOPMode in prAdapter->rWlanInfo
 * 2) change nicMediaStateChange() API prototype
 *
 * 07 21 2010 cp.wu
 *
 * 1) change BG_SCAN to ONLINE_SCAN for consistent term
 * 2) only clear scanning result when scan is permitted to do
 *
 * 07 19 2010 cm.chang
 *
 * Set RLM parameters and enable CNM channel manager
 *
 * 07 19 2010 jeffrey.chang
 *
 * Linux port modification
 *
 * 07 13 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration.
 * Reduce unnecessary type casting
 *
 * 07 13 2010 cp.wu
 *
 * use multiple queues to keep 1x/MMPDU/CMD's strict order even when there is incoming 1x frames.
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
 * 07 05 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) ignore RSN checking when RSN is not turned on.
 * 2) set STA-REC deactivation callback as NULL
 * 3) add variable initialization API based on PHY configuration
 *
 * 07 02 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) for event packet, no need to fill RFB.
 * 2) when wlanAdapterStart() failed, no need to initialize state machines
 * 3) after Beacon/ProbeResp parsing, corresponding BSS_DESC_T should be marked as IE-parsed
 *
 * 07 01 2010 cm.chang
 * [WPD00003841][LITE Driver] Migrate RLM/CNM to host driver
 * Support sync command of STA_REC
 *
 * 07 01 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * add scan uninitialization procedure
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
 * 06 21 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * initialize mbox & ais_fsm in wlanAdapterStart()
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
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 05 28 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * disable interrupt then send power control command packet.
 *
 * 05 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) when stopping adapter, wait til RDY bit has been cleaerd.
 * 2) set TASK_OFFLOAD as driver-core OIDs
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
 * 05 17 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add CFG_STARTUP_DEBUG for debugging starting up issue.
 *
 * 05 17 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * 1) add timeout handler mechanism for pending command packets
 * 2) add p2p add/removal key
 *
 * 04 23 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * surpress compiler warning
 *
 * 04 20 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * roll-back to rev.60.
 *
 * 04 20 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove redundant firmware image unloading
 * 2) use compile-time macros to separate logic related to accquiring own
 *
 * 04 16 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * treat BUS access failure as kind of card removal.
 *
 * 04 14 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * always set fw-own before driver is unloaded.
 *
 * 04 13 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add framework for BT-over-Wi-Fi support.
 *  *  * 1) prPendingCmdInfo is replaced by queue for multiple handler capability
 *  *  * 2) command sequence number is now increased atomically
 *  *  * 3) private data could be hold and taken use for other purpose
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * finish non-glue layer access to glue variables
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * rWlanInfo should be placed at adapter rather than glue due to most operations
 * are done in adapter layer.
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * ePowerCtrl is not necessary as a glue variable.
 *
 * 04 06 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * add timeout check in the kalOidComplete
 *
 * 04 06 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * improve none-glue code portability
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
 * eliminate direct access for prGlueInfo->fgIsCardRemoved in non-glue layer
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) for some OID, never do timeout expiration
 * 2) add 2 kal API for later integration
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) eliminate unused definitions
 * 2) ready bit will be polled for limited iteration
 *
 * 04 06 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * kalOidComplete is not necessary in linux
 *
 * 04 01 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * change to use pass-in prRegInfo instead of accessing prGlueInfo directly
 *
 * 04 01 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * change to use WIFI_TCM_ALWAYS_ON as firmware image
 *
 * 04 01 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * .
 *
 * 03 31 2010 wh.su
 * [WPD00003816][MT6620 Wi-Fi] Adding the security support
 * modify the wapi related code for new driver's design.
 *
 * 03 30 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * adding none-glue code portability
 *
 * 03 30 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * adding non-glue code portability
 *
 * 03 29 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * improve non-glue code portability
 *
 * 03 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * firmware download load adress & start address are now configured from config.h
 * due to the different configurations on FPGA and ASIC
 *
 * 03 24 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * [WPD00003826] Initial import for Linux port
 * initial import for Linux port
 *
 * 03 24 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * initial import for Linux port
 *
 * 03 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * only send CMD_NIC_POWER_CTRL in wlanAdapterStop() when card is not removed and is not in D3 state
 *
 * 03 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * always send CMD_NIC_POWER_CTRL packet when nic is being halted
 *
 * 03 19 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add ACPI D0/D3 state switching support
 * 2) use more formal way to handle interrupt when the status is retrieved from enhanced RX response
 *
* 03 12 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add two option for ACK and ENCRYPTION for firmware download
 *
 * 03 11 2010 cp.wu
 * [WPD00003821][BUG] Host driver stops processing RX packets from HIF RX0
 * add RX starvation warning debug message controlled by CFG_HIF_RX_STARVATION_WARNING
 *
 * 03 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add another spin-lock to protect MsduInfoList due to it might be accessed by different thread.
 * 2) change own-back acquiring procedure to wait for up to 16.67 seconds
 *
 * 03 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * when starting adapter, read local adminsitrated address from registry and send to firmware via CMD_BASIC_CONFIG.
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) the use of prPendingOid revised, all accessing are now protected by spin lock
 * 2) ensure wlanReleasePendingOid will clear all command queues
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add mutex to avoid multiple access to qmTxQueue simultaneously.
 *
 * 03 01 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add command/event definitions for initial states
 *
 * 02 24 2010 tehuang.liu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * Added code for QM_TEST_MODE
 *
 * 02 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * correct function name ..
 *
 * 02 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * separate wlanProcesQueuePacket() into 2 APIs upon request
 *
 * 02 23 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add new API: wlanProcessQueuedPackets()
 *
 * 02 11 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * correct wlanAdapterStart
 *
 * 02 11 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. add logic for firmware download
 * 2. firmware image filename and start/load address are now retrieved from registry
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement host-side firmware download logic
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove unused function in nic_rx.c [which has been handled in que_mgt.c]
 * 2) firmware image length is now retrieved via NdisFileOpen
 * 3) firmware image is not structured by (P_IMG_SEC_HDR_T) anymore
 * 4) nicRxWaitResponse() revised
 * 5) another set of TQ counter default value is added for fw-download state
 * 6) Wi-Fi load address is now retrieved from registry too
 *
 * 02 09 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. Permanent and current MAC address are now retrieved by CMD/EVENT packets instead of hard-coded address
 * 2. follow MSDN defined behavior when associates to another AP
 * 3. for firmware download, packet size could be up to 2048 bytes
 *
 * 02 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * prepare for implementing fw download logic
 *
 * 02 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * wlanoidSetFrequency is now implemented by RF test command.
 *
 * 02 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * QueryRssi is no longer w/o hardware access, it is now implemented by command/event handling loop
 *
 * 02 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. clear prPendingCmdInfo properly
 * 2. while allocating memory for cmdinfo, no need to add extra 4 bytes.
 *
 * 01 28 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * allow MCR read/write OIDs in RF test mode
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) implement timeout mechanism when OID is pending for longer than 1 second
 * 2) allow OID_802_11_CONFIGURATION to be executed when RF test mode is turned on
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 * 2. block TX/ordinary OID when RF test mode is engaged
 * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 * 4. correct some HAL implementation
 *
 * 01 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * Under WinXP with SDIO, use prGlueInfo->rHifInfo.pvInformationBuffer instead of prGlueInfo->pvInformationBuffer
**  \main\maintrunk.MT6620WiFiDriver_Prj\36 2009-12-10 16:54:36 GMT mtk02752
**  code clean
**  \main\maintrunk.MT6620WiFiDriver_Prj\35 2009-12-09 20:04:59 GMT mtk02752
**  only report as connected when CFG_HIF_EMULATION_TEST is set to 1
**  \main\maintrunk.MT6620WiFiDriver_Prj\34 2009-12-08 17:39:41 GMT mtk02752
**  wlanoidRftestQueryAutoTest could be executed without touching hardware
**  \main\maintrunk.MT6620WiFiDriver_Prj\33 2009-12-03 16:10:26 GMT mtk01461
**  Add debug message
**  \main\maintrunk.MT6620WiFiDriver_Prj\32 2009-12-02 22:05:33 GMT mtk02752
**  kalOidComplete() will decrease i4OidPendingCount
**  \main\maintrunk.MT6620WiFiDriver_Prj\31 2009-12-01 23:02:36 GMT mtk02752
**  remove unnecessary spinlock
**  \main\maintrunk.MT6620WiFiDriver_Prj\30 2009-12-01 22:50:38 GMT mtk02752
**  use TC4 for command, maintein i4OidPendingCount
**  \main\maintrunk.MT6620WiFiDriver_Prj\29 2009-11-27 12:45:34 GMT mtk02752
**  prCmdInfo should be freed when invoking wlanReleasePendingOid() to clear pending oid
**  \main\maintrunk.MT6620WiFiDriver_Prj\28 2009-11-24 19:55:51 GMT mtk02752
**  wlanSendPacket & wlanRetransmitOfPendingFrames is only used in old data path
**  \main\maintrunk.MT6620WiFiDriver_Prj\27 2009-11-23 17:59:55 GMT mtk02752
**  clear prPendingOID inside wlanSendCommand() when the OID didn't need to be replied.
**  \main\maintrunk.MT6620WiFiDriver_Prj\26 2009-11-23 14:45:29 GMT mtk02752
**  add another version of wlanSendCommand() for command-sending only without blocking for response
**  \main\maintrunk.MT6620WiFiDriver_Prj\25 2009-11-17 22:40:44 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\24 2009-11-11 10:14:56 GMT mtk01084
**  modify place to invoke wlanIst
**  \main\maintrunk.MT6620WiFiDriver_Prj\23 2009-10-30 18:17:07 GMT mtk01084
**  fix compiler warning
**  \main\maintrunk.MT6620WiFiDriver_Prj\22 2009-10-29 19:46:15 GMT mtk01084
**  invoke interrupt process routine
**  \main\maintrunk.MT6620WiFiDriver_Prj\21 2009-10-13 21:58:24 GMT mtk01084
**  modify for new HW architecture
**  \main\maintrunk.MT6620WiFiDriver_Prj\20 2009-09-09 17:26:01 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\19 2009-05-20 12:21:27 GMT mtk01461
**  Add SeqNum check when process Event Packet
**  \main\maintrunk.MT6620WiFiDriver_Prj\18 2009-05-19 10:38:44 GMT mtk01461
**  Add wlanReleasePendingOid() for mpReset() if there is a pending OID and no available TX resource to send it.
**  \main\maintrunk.MT6620WiFiDriver_Prj\17 2009-04-29 15:41:34 GMT mtk01461
**  Add handle of EVENT of CMD Result in wlanSendCommand()
**  \main\maintrunk.MT6620WiFiDriver_Prj\16 2009-04-22 09:11:23 GMT mtk01461
**  Fix wlanSendCommand() for Driver Domain CR
**  \main\maintrunk.MT6620WiFiDriver_Prj\15 2009-04-21 09:33:56 GMT mtk01461
**  Update wlanSendCommand() for Driver Domain Response and handle Event Packet, wlanQuery/SetInformation() for enqueue CMD_INFO_T
**  \main\maintrunk.MT6620WiFiDriver_Prj\14 2009-04-17 20:00:08 GMT mtk01461
**  Update wlanImageSectionDownload for optimized CMD process
**  \main\maintrunk.MT6620WiFiDriver_Prj\13 2009-04-14 20:50:51 GMT mtk01426
**  Fixed compile error
**  \main\maintrunk.MT6620WiFiDriver_Prj\12 2009-04-13 16:38:40 GMT mtk01084
**  add wifi start function
**  \main\maintrunk.MT6620WiFiDriver_Prj\11 2009-04-13 14:26:44 GMT mtk01084
**  modify a parameter about FW download length
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-04-10 21:53:42 GMT mtk01461
**  Update wlanSendCommand()
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-04-08 16:51:04 GMT mtk01084
**  Update for the image download part
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-04-01 10:32:47 GMT mtk01461
**  Add wlanSendLeftClusteredFrames() for SDIO_TX_ENHANCE
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-03-23 21:44:13 GMT mtk01461
**  Refine TC assignment for WmmAssoc flag
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-03-23 16:51:57 GMT mtk01084
**  modify the input argument of caller to RECLAIM_POWER_CONTROL_TO_PM()
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-03-23 00:27:13 GMT mtk01461
**  Add reference code of FW Image Download
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-19 18:32:37 GMT mtk01084
**  update for basic power management functions
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-16 09:09:08 GMT mtk01461
**  Update TX PATH API
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 16:28:45 GMT mtk01426
**  Init develop
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
#include "mgmt/ais_fsm.h"
#include <linux/aee.h>

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* 6.1.1.2 Interpretation of priority parameter in MAC service primitives */
/* Static convert the Priority Parameter/TID(User Priority/TS Identifier) to Traffic Class */
const UINT_8 aucPriorityParam2TC[] = {
    TC1_INDEX,
    TC0_INDEX,
    TC0_INDEX,
    TC1_INDEX,
    TC2_INDEX,
    TC2_INDEX,
    TC3_INDEX,
    TC3_INDEX
};

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef struct _CODE_MAPPING_T {
    UINT_32         u4RegisterValue;
    INT_32         u4TxpowerOffset;
} CODE_MAPPING_T, *P_CODE_MAPPING_T;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
BOOLEAN fgIsBusAccessFailed = FALSE;

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define SIGNED_EXTEND(n, _sValue) \
	(((_sValue) & BIT((n)-1)) ? ((_sValue) | BITS(n, 31)) : \
	 ((_sValue) & ~BITS(n, 31)))

/* TODO: Check */
/* OID set handlers without the need to access HW register */
PFN_OID_HANDLER_FUNC apfnOidSetHandlerWOHwAccess[] = {
    wlanoidSetChannel,
    wlanoidSetBeaconInterval,
    wlanoidSetAtimWindow,
    wlanoidSetFrequency,
};

/* TODO: Check */
/* OID query handlers without the need to access HW register */
PFN_OID_HANDLER_FUNC apfnOidQueryHandlerWOHwAccess[] = {
    wlanoidQueryBssid,
    wlanoidQuerySsid,
    wlanoidQueryInfrastructureMode,
    wlanoidQueryAuthMode,
    wlanoidQueryEncryptionStatus,
    wlanoidQueryPmkid,
    wlanoidQueryNetworkTypeInUse,
    wlanoidQueryBssidList,
    wlanoidQueryAcpiDevicePowerState,
    wlanoidQuerySupportedRates,
    wlanoidQueryDesiredRates,
    wlanoidQuery802dot11PowerSaveProfile,
    wlanoidQueryBeaconInterval,
    wlanoidQueryAtimWindow,
    wlanoidQueryFrequency,
};

/* OID set handlers allowed in RF test mode */
PFN_OID_HANDLER_FUNC apfnOidSetHandlerAllowedInRFTest[] = {
    wlanoidRftestSetTestMode,
    wlanoidRftestSetAbortTestMode,
    wlanoidRftestSetAutoTest,
    wlanoidSetMcrWrite,
    wlanoidSetEepromWrite
};

/* OID query handlers allowed in RF test mode */
PFN_OID_HANDLER_FUNC apfnOidQueryHandlerAllowedInRFTest[] = {
    wlanoidRftestQueryAutoTest,
    wlanoidQueryMcrRead,
    wlanoidQueryEepromRead
}

;

PFN_OID_HANDLER_FUNC apfnOidWOTimeoutCheck[] = {
    wlanoidRftestSetTestMode,
    wlanoidRftestSetAbortTestMode,
    wlanoidSetAcpiDevicePowerState,
};


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

static WLAN_STATUS
wlanImageSectionDownloadStage(IN P_ADAPTER_T prAdapter,
	IN PVOID pvFwImageMapFile, IN UINT_32 index, IN UINT_32 u4FwImageFileLength, IN BOOLEAN fgValidHead, IN UINT_32 u4FwLoadAddr);


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* \brief This is a private routine, which is used to check if HW access is needed
*        for the OID query/ set handlers.
*
* \param[IN] pfnOidHandler Pointer to the OID handler.
* \param[IN] fgSetInfo     It is a Set information handler.
*
* \retval TRUE This function needs HW access
* \retval FALSE This function does not need HW access
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanIsHandlerNeedHwAccess(IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN BOOLEAN fgSetInfo)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerWOHwAccess;
    UINT_32 i;
    UINT_32 u4NumOfElem;

    if (fgSetInfo) {
        apfnOidHandlerWOHwAccess = apfnOidSetHandlerWOHwAccess;
        u4NumOfElem = sizeof(apfnOidSetHandlerWOHwAccess) / sizeof(PFN_OID_HANDLER_FUNC);
	} else {
        apfnOidHandlerWOHwAccess = apfnOidQueryHandlerWOHwAccess;
        u4NumOfElem = sizeof(apfnOidQueryHandlerWOHwAccess) / sizeof(PFN_OID_HANDLER_FUNC);
    }

    for (i = 0; i < u4NumOfElem; i++) {
        if (apfnOidHandlerWOHwAccess[i] == pfnOidHandler) {
            return FALSE;
        }
    }

    return TRUE;
}   /* wlanIsHandlerNeedHwAccess */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set flag for later handling card
*        ejected event.
*
* \param[in] prAdapter Pointer to the Adapter structure.
*
* \return (none)
*
* \note When surprised removal happens, Glue layer should invoke this
*       function to notify WPDD not to do any hw access.
*/
/*----------------------------------------------------------------------------*/
VOID wlanCardEjected(IN P_ADAPTER_T prAdapter)
{
    DEBUGFUNC("wlanCardEjected");
	/* INITLOG(("\n")); */

    ASSERT(prAdapter);

     /* mark that the card is being ejected, NDIS will shut us down soon */
    nicTxRelease(prAdapter, FALSE);

} /* wlanCardEjected */


/*----------------------------------------------------------------------------*/
/*!
* \brief Create adapter object
*
* \param prAdapter This routine is call to allocate the driver software objects.
*                  If fails, return NULL.
* \retval NULL If it fails, NULL is returned.
* \retval NOT NULL If the adapter was initialized successfully.
*/
/*----------------------------------------------------------------------------*/
P_ADAPTER_T wlanAdapterCreate(IN P_GLUE_INFO_T prGlueInfo)
{
	P_ADAPTER_T prAdpater = (P_ADAPTER_T) NULL;

    DEBUGFUNC("wlanAdapterCreate");

    do {
        prAdpater = (P_ADAPTER_T) kalMemAlloc(sizeof(ADAPTER_T), VIR_MEM_TYPE);

        if (!prAdpater) {
            DBGLOG(INIT, ERROR, ("Allocate ADAPTER memory ==> FAILED\n"));
            break;
        }
#if QM_TEST_MODE
        g_rQM.prAdapter = prAdpater;
#endif
        kalMemZero(prAdpater, sizeof(ADAPTER_T));
        prAdpater->prGlueInfo = prGlueInfo;

	} while (FALSE);

    return prAdpater;
} /* wlanAdapterCreate */


/*----------------------------------------------------------------------------*/
/*!
* \brief Destroy adapter object
*
* \param prAdapter This routine is call to destroy the driver software objects.
*                  If fails, return NULL.
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanAdapterDestroy(IN P_ADAPTER_T prAdapter)
{

    if (!prAdapter) {
        return;
    }

    kalMemFree(prAdapter, VIR_MEM_TYPE, sizeof(ADAPTER_T));

    return;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Initialize the adapter. The sequence is
*        1. Disable interrupt
*        2. Read adapter configuration from EEPROM and registry, verify chip ID.
*        3. Create NIC Tx/Rx resource.
*        4. Initialize the chip
*        5. Initialize the protocol
*        6. Enable Interrupt
*
* \param prAdapter      Pointer of Adapter Data Structure
*
* \retval WLAN_STATUS_SUCCESS: Success
* \retval WLAN_STATUS_FAILURE: Failed
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanAdapterStart(IN P_ADAPTER_T prAdapter,
    IN P_REG_INFO_T prRegInfo,
		 IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength)
{
    WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
    UINT_32     i, u4Value = 0;
    UINT_32     u4WHISR = 0;
    UINT_16     au2TxCount[16];
#if CFG_ENABLE_FW_DOWNLOAD
	UINT_32 u4FwLoadAddr;
#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
    P_FIRMWARE_DIVIDED_DOWNLOAD_T prFwHead;
    BOOLEAN fgValidHead;
    const UINT_32 u4CRCOffset = offsetof(FIRMWARE_DIVIDED_DOWNLOAD_T, u4NumOfEntries);
#endif
#endif

    ASSERT(prAdapter);

    DEBUGFUNC("wlanAdapterStart");

	/* 4 <0> Reset variables in ADAPTER_T */
	//prAdapter->fgIsFwOwn = TRUE;
    prAdapter->fgIsEnterD3ReqIssued = FALSE;

    prAdapter->u4OwnFailedCount = 0;
    prAdapter->u4OwnFailedLogCount = 0;

    QUEUE_INITIALIZE(&(prAdapter->rPendingCmdQueue));
#if CFG_SUPPORT_MULTITHREAD
    QUEUE_INITIALIZE(&prAdapter->rTxCmdQueue);
    QUEUE_INITIALIZE(&prAdapter->rTxCmdDoneQueue);
    QUEUE_INITIALIZE(&prAdapter->rTxP0Queue);
    QUEUE_INITIALIZE(&prAdapter->rTxP1Queue);
    QUEUE_INITIALIZE(&prAdapter->rRxQueue);
#endif

    /* Initialize rWlanInfo */
    kalMemSet(&(prAdapter->rWlanInfo), 0, sizeof(WLAN_INFO_T));

    /* Initialize aprBssInfo[].
     * Important: index shall be same when mapping between aprBssInfo[]
     *            and arBssInfoPool[]. rP2pDevInfo is indexed to final one.
     */
    for (i = 0; i < BSS_INFO_NUM; i++) {
        prAdapter->aprBssInfo[i] = &prAdapter->rWifiVar.arBssInfoPool[i];
    }
	prAdapter->aprBssInfo[P2P_DEV_BSS_INDEX] = &prAdapter->rWifiVar.rP2pDevInfo;


	/* 4 <0.1> reset fgIsBusAccessFailed */
    fgIsBusAccessFailed = FALSE;
	atomic_set(&prAdapter->fgIsSuspended, 0);
    do {
	    u4Status = nicAllocateAdapterMemory(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
            DBGLOG(INIT, ERROR, ("nicAllocateAdapterMemory Error!\n"));
            u4Status = WLAN_STATUS_FAILURE;
            break;
        }

        prAdapter->u4OsPacketFilter = PARAM_PACKET_FILTER_SUPPORTED;

#if defined(MT6630)
		DBGLOG(INIT, INFO, ("wlanAdapterStart(): Acquiring LP-OWN\n"));
        ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
		DBGLOG(INIT, INFO, ("wlanAdapterStart(): Acquiring LP-OWN-end\n"));
		
#if !CFG_ENABLE_FULL_PM
        nicpmSetDriverOwn(prAdapter);
#endif

		if (prAdapter->fgIsFwOwn == TRUE) {
            DBGLOG(INIT, ERROR, ("nicpmSetDriverOwn() failed!\n"));
            u4Status = WLAN_STATUS_FAILURE;
            break;
        }
		/* 4 <1> Initialize the Adapter */
		u4Status = nicInitializeAdapter(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
            DBGLOG(INIT, ERROR, ("nicInitializeAdapter failed!\n"));
            u4Status = WLAN_STATUS_FAILURE;
            break;
        }
#endif

		/* 4 <2.1> Initialize System Service (MGMT Memory pool and STA_REC) */
        nicInitSystemService(prAdapter);

		/* 4 <2.2> Initialize Feature Options */
        wlanInitFeatureOption(prAdapter);
#if CFG_SUPPORT_MTK_SYNERGY
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE) {
	        if (prRegInfo->prNvramSettings->u2FeatureReserved & BIT(MTK_FEATURE_2G_256QAM_DISABLED)) {
	            prAdapter->rWifiVar.aucMtkFeature[0] &= ~(MTK_SYNERGY_CAP_SUPPORT_24G_MCS89);
	        }
	}
#endif

        /* 4 <2.3> Overwrite debug level settings */
        wlanCfgSetDebugLevel(prAdapter);

		/* 4 <3> Initialize Tx */
        nicTxInitialize(prAdapter);
        wlanDefTxPowerCfg(prAdapter);

		/* 4 <4> Initialize Rx */
        nicRxInitialize(prAdapter);
    
#if CFG_ENABLE_FW_DOWNLOAD
#if defined(MT6630)
        if (pvFwImageMapFile) {
            /* 1. disable interrupt, download is done by polling mode only */
            nicDisableInterrupt(prAdapter);

            /* 2. Initialize Tx Resource to fw download state */
            nicTxInitResetResource(prAdapter);

            /* 3. FW download here */
            u4FwLoadAddr = prRegInfo->u4LoadAddress;

			DBGLOG(INIT, INFO, ("FW download Start\n"));
			
#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
			/* 3a. parse file header for decision of divided firmware download or not */
			prFwHead = (P_FIRMWARE_DIVIDED_DOWNLOAD_T) pvFwImageMapFile;

			if (prFwHead->u4Signature == MTK_WIFI_SIGNATURE &&
			    prFwHead->u4CRC == wlanCRC32((PUINT_8) pvFwImageMapFile + u4CRCOffset,
							 u4FwImageFileLength - u4CRCOffset)) {
                fgValidHead = TRUE;
			} else {
                fgValidHead = FALSE;
            }

            /* 3b. engage divided firmware downloading */
			if (fgValidHead == TRUE) {
				for (i = 0; i < prFwHead->u4NumOfEntries; i++) {
					u4Status = wlanImageSectionDownloadStage(prAdapter,
						pvFwImageMapFile, i, u4FwImageFileLength, TRUE, u4FwLoadAddr);
					if (u4Status == WLAN_STATUS_FAILURE) {
                        break;
                    }
                            }
			} else
#endif
            {
				u4Status = wlanImageSectionDownloadStage(prAdapter,
					pvFwImageMapFile, 0, u4FwImageFileLength, FALSE, u4FwLoadAddr);
				if (u4Status == WLAN_STATUS_FAILURE) {
                    break;
                }
                }

            /* escape to top */
			if (u4Status != WLAN_STATUS_SUCCESS) {
                break;
            }
#if !CFG_ENABLE_FW_DOWNLOAD_ACK
			/* Send INIT_CMD_ID_QUERY_PENDING_ERROR command and wait for response */
			if (wlanImageQueryStatus(prAdapter) != WLAN_STATUS_SUCCESS) {
                DBGLOG(INIT, ERROR, ("Firmware download failed!\n"));
                u4Status = WLAN_STATUS_FAILURE;
                break;
            }
#endif
		} else {
            DBGLOG(INIT, ERROR, ("No Firmware found!\n"));
            u4Status = WLAN_STATUS_FAILURE;
            break;
        }
		DBGLOG(INIT, INFO, ("FW download End\n"));
        /* 4. send Wi-Fi Start command */
#if CFG_OVERRIDE_FW_START_ADDRESS
		wlanConfigWifiFunc(prAdapter, TRUE, prRegInfo->u4StartAddress);
#else
		wlanConfigWifiFunc(prAdapter, FALSE, 0);
#endif
#endif
#endif

        DBGLOG(INIT, TRACE, ("wlanAdapterStart(): Waiting for Ready bit..\n"));
		/* 4 <5> check Wi-Fi FW asserts ready bit */
        i = 0;
		while (1) {
            HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

            if (u4Value & WCIR_WLAN_READY) {
                DBGLOG(INIT, TRACE, ("Ready bit asserted\n"));
                break;
			} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
                    || fgIsBusAccessFailed == TRUE) {
                u4Status = WLAN_STATUS_FAILURE;
                break;
			} else if (i >= CFG_RESPONSE_POLLING_TIMEOUT) {
                UINT_32     u4MailBox0;

                nicGetMailbox(prAdapter, 0, &u4MailBox0);
				DBGLOG(INIT, ERROR, ("Waiting for Ready bit: Timeout, ID=%d\n",
                        (u4MailBox0 & 0x0000FFFF)));
                u4Status = WLAN_STATUS_FAILURE;
                break;
			} else {
                i++;
                kalMsleep(10);
            }
        }

		if (u4Status == WLAN_STATUS_SUCCESS) {
			/* 1. reset interrupt status */
			HAL_READ_INTR_STATUS(prAdapter, 4, (PUINT_8) & u4WHISR);
			if (HAL_IS_TX_DONE_INTR(u4WHISR)) {
                HAL_READ_TX_RELEASED_COUNT(prAdapter, au2TxCount);
            }

            /* 2. query & reset TX Resource for normal operation */
            wlanQueryNicResourceInformation(prAdapter);

#if (CFG_SUPPORT_NIC_CAPABILITY == 1)
            /* 3. query for NIC capability */
            wlanQueryNicCapability(prAdapter);
#endif

            /* 4. update basic configuration */
            wlanUpdateBasicConfig(prAdapter);

            /* 5. Override network address */
            wlanUpdateNetworkAddress(prAdapter);

            /* 6. Apply Network Address */
            nicApplyNetworkAddress(prAdapter);

            /* 7. indicate disconnection as default status */
            kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
						     WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
        }

        RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

		if (u4Status != WLAN_STATUS_SUCCESS) {
            break;
        }

        /* OID timeout timer initialize */
        cnmTimerInitTimer(prAdapter,
                &prAdapter->rOidTimeoutTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) wlanReleasePendingOid, (ULONG) NULL);

        prAdapter->ucOidTimeoutCount = 0;

        prAdapter->fgIsChipNoAck = FALSE;

        /* Return Indicated Rfb list timer */
        cnmTimerInitTimer(prAdapter,
                &prAdapter->rPacketDelaySetupTimer,
				  (PFN_MGMT_TIMEOUT_FUNC) wlanReturnPacketDelaySetupTimeout,
				  (ULONG) NULL);

        /* Power state initialization */
        prAdapter->fgWiFiInSleepyState = FALSE;
        prAdapter->rAcpiState = ACPI_STATE_D0;

        /* Online scan option */
		if (prRegInfo->fgDisOnlineScan == 0) {
            prAdapter->fgEnOnlineScan = TRUE;
		} else {
            prAdapter->fgEnOnlineScan = FALSE;
        }

        /* Beacon lost detection option */
		if (prRegInfo->fgDisBcnLostDetection != 0) {
            prAdapter->fgDisBcnLostDetection = TRUE;
        }

        /* Load compile time constant */
        prAdapter->rWlanInfo.u2BeaconPeriod = CFG_INIT_ADHOC_BEACON_INTERVAL;
        prAdapter->rWlanInfo.u2AtimWindow = CFG_INIT_ADHOC_ATIM_WINDOW;

#if 1				/* set PM parameters */
        prAdapter->fgEnArpFilter = prRegInfo->fgEnArpFilter;
        prAdapter->u4PsCurrentMeasureEn = prRegInfo->u4PsCurrentMeasureEn;

        prAdapter->u4UapsdAcBmp = prRegInfo->u4UapsdAcBmp;

        prAdapter->u4MaxSpLen = prRegInfo->u4MaxSpLen;

        DBGLOG(INIT, TRACE, ("[1] fgEnArpFilter:0x%x, u4UapsdAcBmp:0x%x, u4MaxSpLen:0x%x",
                prAdapter->fgEnArpFilter,
				     prAdapter->u4UapsdAcBmp, prAdapter->u4MaxSpLen));

        prAdapter->fgEnCtiaPowerMode = FALSE;

#endif

        /* MGMT Initialization */
        nicInitMGMT(prAdapter, prRegInfo);

        /* Enable WZC Disassociation */
        prAdapter->rWifiVar.fgSupportWZCDisassociation = TRUE;

        /* Apply Rate Setting */
		if ((ENUM_REGISTRY_FIXED_RATE_T) (prRegInfo->u4FixedRate) < FIXED_RATE_NUM) {
			prAdapter->rWifiVar.eRateSetting =
			    (ENUM_REGISTRY_FIXED_RATE_T) (prRegInfo->u4FixedRate);
		} else {
            prAdapter->rWifiVar.eRateSetting = FIXED_RATE_NONE;
        }

		if (prAdapter->rWifiVar.eRateSetting == FIXED_RATE_NONE) {
            /* Enable Auto (Long/Short) Preamble */
            prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_AUTO;
		} else if ((prAdapter->rWifiVar.eRateSetting >= FIXED_RATE_MCS0_20M_400NS &&
                    prAdapter->rWifiVar.eRateSetting <= FIXED_RATE_MCS7_20M_400NS)
                || (prAdapter->rWifiVar.eRateSetting >= FIXED_RATE_MCS0_40M_400NS &&
                        prAdapter->rWifiVar.eRateSetting <= FIXED_RATE_MCS32_400NS)) {
            /* Force Short Preamble */
            prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_SHORT;
		} else {
            /* Force Long Preamble */
            prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_LONG;
        }

        /* Disable Hidden SSID Join */
        prAdapter->rWifiVar.fgEnableJoinToHiddenSSID = FALSE;

        /* Enable Short Slot Time */
        prAdapter->rWifiVar.fgIsShortSlotTimeOptionEnable = TRUE;

        /* configure available PHY type set */
        nicSetAvailablePhyTypeSet(prAdapter);

#if 0 /* Marked for MT6630 */
#if 1				/* set PM parameters */
        {
#if CFG_SUPPORT_PWR_MGT
        prAdapter->u4PowerMode = prRegInfo->u4PowerMode;
#if CFG_ENABLE_WIFI_DIRECT
			prAdapter->rWlanInfo.arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].
			    ucNetTypeIndex = NETWORK_TYPE_P2P_INDEX;
			prAdapter->rWlanInfo.arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].ucPsProfile =
			    ENUM_PSP_FAST_SWITCH;
#endif
#else
        prAdapter->u4PowerMode = ENUM_PSP_CONTINUOUS_ACTIVE;
#endif

			nicConfigPowerSaveProfile(prAdapter,
            prAdapter->prAisBssInfo->ucBssIndex,
						  prAdapter->u4PowerMode, FALSE);
        }

#endif
#endif

#if CFG_SUPPORT_NVRAM
        /* load manufacture data */
		if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE) {
			wlanLoadManufactureData(prAdapter, prRegInfo);
		} else {
			DBGLOG(INIT, WARN, ("%s: load manufacture data fail\n", __func__));
		}	
#endif

#if 0
    /* Update Auto rate parameters in FW */
    nicRlmArUpdateParms(prAdapter,
        prRegInfo->u4ArSysParam0,
        prRegInfo->u4ArSysParam1,
				    prRegInfo->u4ArSysParam2, prRegInfo->u4ArSysParam3);
#endif
	} while (FALSE);

	if (u4Status == WLAN_STATUS_SUCCESS) {
		/* restore to hardware default */
        HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter);
        HAL_SET_MAILBOX_READ_CLEAR(prAdapter, FALSE);

        /* Enable interrupt */
        nicEnableInterrupt(prAdapter);

	} else {
		/* release allocated memory */
        nicReleaseAdapterMemory(prAdapter);
    }

    return u4Status;
} /* wlanAdapterStart */

/* Code Refactoring for AOSP */
static WLAN_STATUS
wlanImageSectionDownloadStage(IN P_ADAPTER_T prAdapter,
	IN PVOID pvFwImageMapFile, IN UINT_32 index, IN UINT_32 u4FwImageFileLength, IN BOOLEAN fgValidHead, IN UINT_32 u4FwLoadAddr)
{
#if CFG_ENABLE_FW_DOWNLOAD
	UINT_32 u4ImgSecSize;
#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
	UINT_32 j;
	P_FIRMWARE_DIVIDED_DOWNLOAD_T prFwHead;
	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
#endif
#endif

#if CFG_ENABLE_FW_DOWNLOAD
#if defined(MT6630)
#if CFG_ENABLE_FW_DIVIDED_DOWNLOAD
	/* 3a. parse file header for decision of divided firmware download or not */
	prFwHead = (P_FIRMWARE_DIVIDED_DOWNLOAD_T) pvFwImageMapFile;
	do {
		if (fgValidHead == TRUE) {
			if (wlanImageSectionConfig(prAdapter,
						   prFwHead->arSection[index].u4DestAddr,
						   prFwHead->arSection[index].u4Length,
						   index == 0 ? TRUE : FALSE) !=
			    WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR,
				       ("Firmware download configuration failed!\n"));

				u4Status = WLAN_STATUS_FAILURE;
				break;
			} else {
				for (j = 0; j < prFwHead->arSection[index].u4Length;
				     j += CMD_PKT_SIZE_FOR_IMAGE) {
					if (j + CMD_PKT_SIZE_FOR_IMAGE <
					    prFwHead->arSection[index].u4Length)
						u4ImgSecSize =
						    CMD_PKT_SIZE_FOR_IMAGE;
					else
						u4ImgSecSize =
						    prFwHead->arSection[index].u4Length - j;

					if (wlanImageSectionDownload(prAdapter,
								     u4ImgSecSize,
								     (PUINT_8)
								     pvFwImageMapFile
								     +
								     prFwHead->arSection[index].u4Offset + j) !=
					    WLAN_STATUS_SUCCESS) {
						DBGLOG(INIT, ERROR,
						       ("Firmware scatter download failed!\n"));

						u4Status = WLAN_STATUS_FAILURE;
						break;
					}
				}

				/* escape from loop if any pending error occurs */
				if (u4Status == WLAN_STATUS_FAILURE) {
					break;
				}
			}
		} else {
			if (wlanImageSectionConfig(prAdapter,
						   u4FwLoadAddr,
						   u4FwImageFileLength,
						   TRUE) != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR,
				       ("Firmware download configuration failed!\n"));

				u4Status = WLAN_STATUS_FAILURE;
				break;
			} else {
				for (j = 0; j < u4FwImageFileLength;
				     j += CMD_PKT_SIZE_FOR_IMAGE) {
					if (j + CMD_PKT_SIZE_FOR_IMAGE < u4FwImageFileLength)
						u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
					else
						u4ImgSecSize = u4FwImageFileLength - j;

					if (wlanImageSectionDownload(prAdapter,
								     u4ImgSecSize,
								     (PUINT_8)pvFwImageMapFile + j) !=
					    WLAN_STATUS_SUCCESS) {
						DBGLOG(INIT, ERROR,
						       ("Firmware scatter download failed!\n"));

						u4Status = WLAN_STATUS_FAILURE;
						break;
					}
				}
			}
		}
	} while (0);
	return u4Status;
#endif
#endif
#endif
}



/*----------------------------------------------------------------------------*/
/*!
* \brief Uninitialize the adapter
*
* \param prAdapter      Pointer of Adapter Data Structure
*
* \retval WLAN_STATUS_SUCCESS: Success
* \retval WLAN_STATUS_FAILURE: Failed
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanAdapterStop(IN P_ADAPTER_T prAdapter)
{
    UINT_32 i, u4Value = 0;
    WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

    ASSERT(prAdapter);

    /* Release all CMD/MGMT/SEC frame in command queue */
    kalClearCommandQueue(prAdapter->prGlueInfo);

#if CFG_SUPPORT_MULTITHREAD

    /* Flush all items in queues for multi-thread */
    wlanClearTxCommandQueue(prAdapter);

    wlanClearTxCommandDoneQueue(prAdapter);

    wlanClearDataQueue(prAdapter);

    wlanClearRxToOsQueue(prAdapter);

#endif

	if (prAdapter->rAcpiState == ACPI_STATE_D0 &&
	    !wlanIsChipNoAck(prAdapter) && !kalIsCardRemoved(prAdapter->prGlueInfo)) {

        /* 0. Disable interrupt, this can be done without Driver own */
        nicDisableInterrupt(prAdapter);
		DBGLOG(INIT, WARN, ("TL: after nicDisableInterrupt\n"));

        ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

		DBGLOG(INIT, WARN, ("TL: after ACQUIRE_POWER_CONTROL_FROM_PM\n"));
        /* 1. Set CMD to FW to tell WIFI to stop (enter power off state) */
		if (prAdapter->fgIsFwOwn == FALSE
		    && wlanSendNicPowerCtrlCmd(prAdapter, 1) == WLAN_STATUS_SUCCESS) {
            /* 2. Clear pending interrupt */
            i = 0;
			while (i < CFG_IST_LOOP_COUNT
			       && nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
                i++;
				DBGLOG(INIT, WARN, ("TL: While nicProcessIST count is %d\n", i));
            };

            /* 3. Wait til RDY bit has been cleaerd */
            i = 0;
			while (1) {
                HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

                if ((u4Value & WCIR_WLAN_READY) == 0)
                    break;
				else if (kalIsCardRemoved(prAdapter->prGlueInfo)
                        || fgIsBusAccessFailed
                        || (i >= CFG_RESPONSE_POLLING_TIMEOUT)) {
                        
					DBGLOG(INIT, WARN,
					       ("%s: Failure to get RDY bit cleared! CardRemoved[%u] BusFailed[%u] Timeout[%u]",
						__func__,
                        kalIsCardRemoved(prAdapter->prGlueInfo),
						fgIsBusAccessFailed, i));
					
#if CFG_CHIP_RESET_SUPPORT
			glResetTrigger(prAdapter);
#endif
                    
                    break;
				} else {
                    i++;
                    kalMsleep(1);
					DBGLOG(INIT, WARN, ("TL: while: kalMsleep %d times\n", i));
                }
            }
        }

        /* 4. Set Onwership to F/W */
        nicpmSetFWOwn(prAdapter, FALSE);
		DBGLOG(INIT, WARN, ("TL: After nicpmSetFWOwn\n"));

#if CFG_FORCE_RESET_UNDER_BUS_ERROR
		if (HAL_TEST_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR) == TRUE) {
            /* force acquire firmware own */
            kalDevRegWrite(prAdapter->prGlueInfo, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_CLR);

            /* delay for 10ms */
            kalMdelay(10);

            /* force firmware reset via software interrupt */
            kalDevRegWrite(prAdapter->prGlueInfo, MCR_WSICR, WSICR_H2D_SW_INT_SET);

            /* force release firmware own */
            kalDevRegWrite(prAdapter->prGlueInfo, MCR_WHLPCR, WHLPCR_FW_OWN_REQ_SET);
        }
#endif

        RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
		DBGLOG(INIT, WARN, ("TL: After RECLAIM_POWER_CONTROL_TO_PM\n"));
    }

    nicRxUninitialize(prAdapter);
	DBGLOG(INIT, WARN, ("TL: After nicRxUninitialize\n"));

    nicTxRelease(prAdapter, FALSE);
	/* MGMT - unitialization */
	nicUninitMGMT(prAdapter);

    /* System Service Uninitialization */
    nicUninitSystemService(prAdapter);

    nicCheckAdapterMemory(prAdapter);
    nicReleaseAdapterMemory(prAdapter);

#if defined(_HIF_SPI)
    /* Note: restore the SPI Mode Select from 32 bit to default */
    nicRestoreSpiDefMode(prAdapter);
#endif

    return u4Status;
} /* wlanAdapterStop */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by ISR (interrupt).
*
* \param prAdapter      Pointer of Adapter Data Structure
*
* \retval TRUE: NIC's interrupt
* \retval FALSE: Not NIC's interrupt
*/
/*----------------------------------------------------------------------------*/
BOOL wlanISR(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgGlobalIntrCtrl)
{
    ASSERT(prAdapter);

    if (fgGlobalIntrCtrl) {
        nicDisableInterrupt(prAdapter);

		/* wlanIST(prAdapter); */
    }

    return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by IST (task_let).
*
* \param prAdapter      Pointer of Adapter Data Structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanIST(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

    nicProcessIST(prAdapter);

	if (KAL_WAKE_LOCK_ACTIVE(prAdapter, &prAdapter->prGlueInfo->rIntrWakeLock)) {
        KAL_WAKE_UNLOCK(prAdapter, &prAdapter->prGlueInfo->rIntrWakeLock);
    }

    nicEnableInterrupt(prAdapter);

    RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

    return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This function will check command queue to find out if any could be dequeued
*        and/or send to HIF to MT6620
*
* \param prAdapter      Pointer of Adapter Data Structure
* \param prCmdQue       Pointer of Command Queue (in Glue Layer)
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanProcessCommandQueue(IN P_ADAPTER_T prAdapter, IN P_QUE_T prCmdQue)
{
    WLAN_STATUS rStatus;
    QUE_T rTempCmdQue, rMergeCmdQue, rStandInCmdQue;
    P_QUE_T prTempCmdQue, prMergeCmdQue, prStandInCmdQue;
    P_QUE_ENTRY_T prQueueEntry;
    P_CMD_INFO_T prCmdInfo;
    P_MSDU_INFO_T prMsduInfo;
    ENUM_FRAME_ACTION_T eFrameAction = FRAME_ACTION_DROP_PKT;

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    ASSERT(prCmdQue);

    prTempCmdQue = &rTempCmdQue;
    prMergeCmdQue = &rMergeCmdQue;
    prStandInCmdQue = &rStandInCmdQue;

    QUEUE_INITIALIZE(prTempCmdQue);
    QUEUE_INITIALIZE(prMergeCmdQue);
    QUEUE_INITIALIZE(prStandInCmdQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);
    QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
    QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		switch (prCmdInfo->eCmdType) {
        case COMMAND_TYPE_GENERAL_IOCTL:
        case COMMAND_TYPE_NETWORK_IOCTL:
            /* command packet will be always sent */
            eFrameAction = FRAME_ACTION_TX_PKT;
            break;

        case COMMAND_TYPE_SECURITY_FRAME:
            /* inquire with QM */
            eFrameAction = qmGetFrameAction(prAdapter, prCmdInfo->ucBssIndex,
                    prCmdInfo->ucStaRecIndex, NULL, FRAME_TYPE_802_1X,
                    prCmdInfo->u2InfoBufLen);
            break;

        case COMMAND_TYPE_MANAGEMENT_FRAME:
            /* inquire with QM */
			prMsduInfo = prCmdInfo->prMsduInfo;

            eFrameAction = qmGetFrameAction(prAdapter, prMsduInfo->ucBssIndex,
                    prMsduInfo->ucStaRecIndex, prMsduInfo, FRAME_TYPE_MMPDU, 
                    prMsduInfo->u2FrameLength);
            break;

        default:
            ASSERT(0);
            break;
        }

		/* 4 <3> handling upon dequeue result */
		if (eFrameAction == FRAME_ACTION_DROP_PKT) {
            DBGLOG(INIT, INFO, ("DROP CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
                prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum));
            wlanReleaseCommand(prAdapter, prCmdInfo, TX_RESULT_DROPPED_IN_DRIVER);
            cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
        }
        else if(eFrameAction == FRAME_ACTION_QUEUE_PKT) {
            DBGLOG(INIT, TRACE, ("QUE back CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
                prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum));
                
            QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
		} else if (eFrameAction == FRAME_ACTION_TX_PKT) {
			/* 4 <4> Send the command */
#if CFG_SUPPORT_MULTITHREAD
            rStatus = wlanSendCommandMthread(prAdapter, prCmdInfo);

            if(rStatus == WLAN_STATUS_RESOURCES) {
                // no more TC4 resource for further transmission
                DBGLOG(INIT, WARN, ("NO Res CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n", 
                    prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum));
                
                QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
                break;
			} else if (rStatus == WLAN_STATUS_PENDING) {
			} else if (rStatus == WLAN_STATUS_SUCCESS) {
			} else {
				P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) prQueueEntry;
                if (prCmdInfo->fgIsOid) {
					kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery,
						       prCmdInfo->u4SetInfoLen, rStatus);
                }
                cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
            }
            
#else            
            rStatus = wlanSendCommand(prAdapter, prCmdInfo);

            if(rStatus == WLAN_STATUS_RESOURCES) {
                // no more TC4 resource for further transmission

                DBGLOG(INIT, WARN, ("NO Resource for CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n", 
                    prCmdInfo->eCmdType, prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum));
                
                QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
                break;
			} else if (rStatus == WLAN_STATUS_PENDING) {
				/* command packet which needs further handling upon response */
                KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
                QUEUE_INSERT_TAIL(&(prAdapter->rPendingCmdQueue), prQueueEntry);
                KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
			} else {
				P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

                if (rStatus == WLAN_STATUS_SUCCESS) {
                    if (prCmdInfo->pfCmdDoneHandler) {
						prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
						    prCmdInfo->pucInfoBuffer);
                    }
				} else {
                    if (prCmdInfo->fgIsOid) {
						kalOidComplete(prAdapter->prGlueInfo, 
                            prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, 
                            rStatus);
                    }
                }

                cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
            }
#endif            
		} else {
            ASSERT(0);
        }

        QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
    }

	/* 4 <3> Merge back to original queue */
	/* 4 <3.1> Merge prMergeCmdQue & prTempCmdQue */
    QUEUE_CONCATENATE_QUEUES(prMergeCmdQue, prTempCmdQue);

	/* 4 <3.2> Move prCmdQue to prStandInQue, due to prCmdQue might differ due to incoming 802.1X frames */
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);
    QUEUE_MOVE_ALL(prStandInCmdQue, prCmdQue);

	/* 4 <3.3> concatenate prStandInQue to prMergeCmdQue */
    QUEUE_CONCATENATE_QUEUES(prMergeCmdQue, prStandInCmdQue);

	/* 4 <3.4> then move prMergeCmdQue to prCmdQue */
    QUEUE_MOVE_ALL(prCmdQue, prMergeCmdQue);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);
    
#if CFG_SUPPORT_MULTITHREAD
    kalSetTxCmdEvent2Hif(prAdapter->prGlueInfo);
#endif


    return WLAN_STATUS_SUCCESS;
} /* end of wlanProcessCommandQueue() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will take CMD_INFO_T which carry some informations of
*        incoming OID and notify the NIC_TX to send CMD.
*
* \param prAdapter      Pointer of Adapter Data Structure
* \param prCmdInfo      Pointer of P_CMD_INFO_T
*
* \retval WLAN_STATUS_SUCCESS   : CMD was written to HIF and be freed(CMD Done) immediately.
* \retval WLAN_STATUS_RESOURCE  : No resource for current command, need to wait for previous
*                                 frame finishing their transmission.
* \retval WLAN_STATUS_FAILURE   : Get failure while access HIF or been rejected.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanSendCommand(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
    P_TX_CTRL_T prTxCtrl;
    UINT_8 ucTC; /* "Traffic Class" SW(Driver) resource classification */
    WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

    ASSERT(prAdapter);
    ASSERT(prCmdInfo);
    prTxCtrl = &prAdapter->rTxCtrl;

    do {
		/* <0> card removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
            rStatus = WLAN_STATUS_FAILURE;
            break;
        }
		/* <1> Normal case of sending CMD Packet */
        if (!prCmdInfo->fgDriverDomainMCR) {
			/* <1.1> Assign Traffic Class(TC) */
            ucTC = nicTxGetCmdResourceType(prCmdInfo);

			/* <1.2> Check if pending packet or resource was exhausted */
			rStatus = nicTxAcquireResource(prAdapter, ucTC, nicTxGetCmdPageCount(prCmdInfo));
			if (rStatus == WLAN_STATUS_RESOURCES) {
            	DBGLOG(INIT, INFO, ("NO Resource:%d\n", ucTC));
                break;
            }
			/* <1.3> Forward CMD_INFO_T to NIC Layer */
            rStatus = nicTxCmd(prAdapter, prCmdInfo, ucTC);

			/* <1.4> Set Pending in response to Query Command/Need Response */
            if (rStatus == WLAN_STATUS_SUCCESS) {
                if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
                    rStatus = WLAN_STATUS_PENDING;
                }
            }
        }
		/* <2> Special case for access Driver Domain MCR */
        else {
            P_CMD_ACCESS_REG prCmdAccessReg;
			prCmdAccessReg =
			    (P_CMD_ACCESS_REG) (prCmdInfo->pucInfoBuffer + CMD_HDR_SIZE);

            if (prCmdInfo->fgSetQuery) {
				HAL_MCR_WR(prAdapter, (prCmdAccessReg->u4Address & BITS(2, 31)),	/* address is in DWORD unit */
                        prCmdAccessReg->u4Data);
			} else {
                P_CMD_ACCESS_REG prEventAccessReg;
                UINT_32 u4Address;

                u4Address = prCmdAccessReg->u4Address;
				prEventAccessReg = (P_CMD_ACCESS_REG) prCmdInfo->pucInfoBuffer;
                prEventAccessReg->u4Address = u4Address;

				HAL_MCR_RD(prAdapter, prEventAccessReg->u4Address & BITS(2, 31),	/* address is in DWORD unit */
                       &prEventAccessReg->u4Data);
            }
        }

	} while (FALSE);

    return rStatus;
} /* end of wlanSendCommand() */

#if CFG_SUPPORT_MULTITHREAD

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will take CMD_INFO_T which carry some informations of
*        incoming OID and notify the NIC_TX to send CMD.
*
* \param prAdapter      Pointer of Adapter Data Structure
* \param prCmdInfo      Pointer of P_CMD_INFO_T
*
* \retval WLAN_STATUS_SUCCESS   : CMD was written to HIF and be freed(CMD Done) immediately.
* \retval WLAN_STATUS_RESOURCE  : No resource for current command, need to wait for previous
*                                 frame finishing their transmission.
* \retval WLAN_STATUS_FAILURE   : Get failure while access HIF or been rejected.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanSendCommandMthread(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
    P_TX_CTRL_T prTxCtrl;
    UINT_8 ucTC; /* "Traffic Class" SW(Driver) resource classification */
    WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

    QUE_T rTempCmdQue;
    P_QUE_T prTempCmdQue;

    KAL_SPIN_LOCK_DECLARATION();
 
    ASSERT(prAdapter);
    ASSERT(prCmdInfo);
    prTxCtrl = &prAdapter->rTxCtrl;

    prTempCmdQue = &rTempCmdQue;
    QUEUE_INITIALIZE(prTempCmdQue);
    
    do {
		/* <0> card removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
            rStatus = WLAN_STATUS_FAILURE;
            break;
        }
		/* <1> Normal case of sending CMD Packet */
        if (!prCmdInfo->fgDriverDomainMCR) {
			/* <1.1> Assign Traffic Class(TC) */
            ucTC = nicTxGetCmdResourceType(prCmdInfo);

            /* <1.2> Check if pending packet or resource was exhausted */
            rStatus = nicTxAcquireResource(prAdapter, ucTC, nicTxGetCmdPageCount(prCmdInfo));
            if (rStatus == WLAN_STATUS_RESOURCES) {
#if 0                
                DBGLOG(INIT, WARN, ("%s: NO Resource for CMD TYPE[%u] ID[0x%02X] SEQ[%u] TC[%u]\n", 
                    __FUNCTION__,
                    prCmdInfo->eCmdType, 
                    prCmdInfo->ucCID,
                    prCmdInfo->ucCmdSeqNum,
                    ucTC));                
#endif
                break;
            }
            
			/* Process to pending command queue firest */
            if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
				/* command packet which needs further handling upon response */
                /*
                KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
                QUEUE_INSERT_TAIL(&(prAdapter->rPendingCmdQueue), (P_QUE_ENTRY_T)prCmdInfo);
                KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
                */
            } 
			QUEUE_INSERT_TAIL(prTempCmdQue, (P_QUE_ENTRY_T) prCmdInfo);
           
			/* <1.4> Set Pending in response to Query Command/Need Response */
            if (rStatus == WLAN_STATUS_SUCCESS) {
                if ((!prCmdInfo->fgSetQuery) || 
                    (prCmdInfo->fgNeedResp)  || 
                    (prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME)) {
                    rStatus = WLAN_STATUS_PENDING;      
                }
            }
        }
		/* <2> Special case for access Driver Domain MCR */
        else {
			QUEUE_INSERT_TAIL(prTempCmdQue, (P_QUE_ENTRY_T) prCmdInfo);
            rStatus = WLAN_STATUS_PENDING;
        }
	} while (FALSE);

    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
    QUEUE_CONCATENATE_QUEUES(&(prAdapter->rTxCmdQueue), prTempCmdQue);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

    return rStatus;
} /* end of wlanSendCommandMthread() */

WLAN_STATUS wlanTxCmdMthread(IN P_ADAPTER_T prAdapter)
{
    QUE_T rTempCmdQue;
    P_QUE_T prTempCmdQue;
    QUE_T rTempCmdDoneQue;
    P_QUE_T prTempCmdDoneQue;
    P_QUE_ENTRY_T prQueueEntry;
    P_CMD_INFO_T prCmdInfo;
    P_CMD_ACCESS_REG prCmdAccessReg;
    P_CMD_ACCESS_REG prEventAccessReg;
    UINT_32 u4Address;
    UINT_32 u4TxDoneQueueSize;

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);

    prTempCmdQue = &rTempCmdQue;
    QUEUE_INITIALIZE(prTempCmdQue);

    prTempCmdDoneQue = &rTempCmdDoneQue;
    QUEUE_INITIALIZE(prTempCmdDoneQue);

    KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
    
    /* TX Command Queue */
    /* 4 <1> Move whole list of CMD_INFO to temp queue */
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
    QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
    QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

        

        if (!prCmdInfo->fgDriverDomainMCR) {
            nicTxCmd(prAdapter, prCmdInfo, TC4_INDEX);

            if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
                KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);
				QUEUE_INSERT_TAIL(&(prAdapter->rPendingCmdQueue),
						  (P_QUE_ENTRY_T) prCmdInfo);
                KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);                
			} else {
                QUEUE_INSERT_TAIL(prTempCmdDoneQue, prQueueEntry);
            } 
			/* DBGLOG(INIT, INFO, ("==> TX CMD QID: %d (Q:%d)\n", prCmdInfo->ucCID, prTempCmdQue->u4NumElem)); */
		} else {
			prCmdAccessReg =
			    (P_CMD_ACCESS_REG) (prCmdInfo->pucInfoBuffer + CMD_HDR_SIZE);

            if (prCmdInfo->fgSetQuery) {
				HAL_MCR_WR(prAdapter, (prCmdAccessReg->u4Address & BITS(2, 31)),	/* address is in DWORD unit */
                        prCmdAccessReg->u4Data);
			} else {
                u4Address = prCmdAccessReg->u4Address;
				prEventAccessReg = (P_CMD_ACCESS_REG) prCmdInfo->pucInfoBuffer;
                prEventAccessReg->u4Address = u4Address;

				HAL_MCR_RD(prAdapter, prEventAccessReg->u4Address & BITS(2, 31),	/* address is in DWORD unit */
                       &prEventAccessReg->u4Data);
            }
            QUEUE_INSERT_TAIL(prTempCmdDoneQue, prQueueEntry);
        }
        QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
    }

    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
    QUEUE_CONCATENATE_QUEUES(&prAdapter->rTxCmdDoneQueue, prTempCmdDoneQue);
    u4TxDoneQueueSize = prAdapter->rTxCmdDoneQueue.u4NumElem;
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

    KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
    
    if (u4TxDoneQueueSize > 0) {
		/* call tx thread to work */
		set_bit(GLUE_FLAG_TX_CMD_DONE_BIT, &prAdapter->prGlueInfo->ulFlag);
        wake_up_interruptible(&prAdapter->prGlueInfo->waitq);
    }

    return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS wlanTxCmdDoneMthread(IN P_ADAPTER_T prAdapter)
{
    QUE_T rTempCmdQue;
    P_QUE_T prTempCmdQue;
    P_QUE_ENTRY_T prQueueEntry;
    P_CMD_INFO_T prCmdInfo;
    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);

    prTempCmdQue = &rTempCmdQue;
    QUEUE_INITIALIZE(prTempCmdQue);
    
	/* 4 <1> Move whole list of CMD_INFO to temp queue */
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
    QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdDoneQueue);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
    QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

        if (prCmdInfo->pfCmdDoneHandler) {
            prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prCmdInfo->pucInfoBuffer);
        }
		/* Not pending cmd, free it after TX succeed! */
        cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

        QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
    }

    return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all commands in TX command queue
* \param prAdapter  Pointer of Adapter Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID wlanClearTxCommandQueue(IN P_ADAPTER_T prAdapter)
{
    QUE_T rTempCmdQue;
    P_QUE_T prTempCmdQue = &rTempCmdQue;      
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

    KAL_SPIN_LOCK_DECLARATION();
    QUEUE_INITIALIZE(prTempCmdQue);

    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
    QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

    QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
    while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

		if (prCmdInfo->pfCmdTimeoutHandler) {
            prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
		} else {
            wlanReleaseCommand(prAdapter, prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);
        }

        cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

        QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
    }
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear OID commands in TX command queue
* \param prAdapter  Pointer of Adapter Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID
wlanClearTxOidCommand(
    IN P_ADAPTER_T              prAdapter
    )
{
    QUE_T rTempCmdQue;
    P_QUE_T prTempCmdQue = &rTempCmdQue;      
    P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T)NULL;
    P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T)NULL;

    KAL_SPIN_LOCK_DECLARATION();
    QUEUE_INITIALIZE(prTempCmdQue);

    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
    
    QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);
    
    QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
    
    while (prQueueEntry) {
        prCmdInfo = (P_CMD_INFO_T)prQueueEntry;

        if (prCmdInfo->fgIsOid) {

            if(prCmdInfo->pfCmdTimeoutHandler) {
                prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
            }
            else {
                wlanReleaseCommand(prAdapter, prCmdInfo, TX_RESULT_QUEUE_CLEARANCE);
            }
            
            cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

        }
        else {
            QUEUE_INSERT_TAIL(&prAdapter->rTxCmdQueue, prQueueEntry);
        }

        QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
    }

    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all commands in TX command done queue
* \param prAdapter  Pointer of Adapter Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID wlanClearTxCommandDoneQueue(IN P_ADAPTER_T prAdapter)
{
    QUE_T rTempCmdDoneQue;
    P_QUE_T prTempCmdDoneQue = &rTempCmdDoneQue;      
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

    KAL_SPIN_LOCK_DECLARATION();
    QUEUE_INITIALIZE(prTempCmdDoneQue);
    
	/* 4 <1> Move whole list of CMD_INFO to temp queue */
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
    QUEUE_MOVE_ALL(prTempCmdDoneQue, &prAdapter->rTxCmdDoneQueue);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
    QUEUE_REMOVE_HEAD(prTempCmdDoneQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

        if (prCmdInfo->pfCmdDoneHandler) {
            prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo, prCmdInfo->pucInfoBuffer);
        }
		/* Not pending cmd, free it after TX succeed! */
        cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

        QUEUE_REMOVE_HEAD(prTempCmdDoneQue, prQueueEntry, P_QUE_ENTRY_T);
    }
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all buffer in port 0/1 queue
* \param prAdapter  Pointer of Adapter Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID wlanClearDataQueue(IN P_ADAPTER_T prAdapter)
{
    QUE_T qDataPort0, qDataPort1;
    P_QUE_T prDataPort0, prDataPort1;
    
    KAL_SPIN_LOCK_DECLARATION();

    prDataPort0 = &qDataPort0;
    prDataPort1 = &qDataPort1;

    QUEUE_INITIALIZE(prDataPort0);
    QUEUE_INITIALIZE(prDataPort1);
        
	/* 4 <1> Move whole list of CMD_INFO to temp queue */
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);
    QUEUE_MOVE_ALL(prDataPort0, &prAdapter->rTxP0Queue);
    QUEUE_MOVE_ALL(prDataPort1, &prAdapter->rTxP1Queue);    
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);

	/* 4 <2> Return sk buffer */
	nicTxReturnMsduInfo(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort0));
	nicTxReturnMsduInfo(prAdapter, (P_MSDU_INFO_T) QUEUE_GET_HEAD(prDataPort1));

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to clear all buffer in port 0/1 queue
* \param prAdapter  Pointer of Adapter Data Structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID wlanClearRxToOsQueue(IN P_ADAPTER_T prAdapter)
{
    QUE_T rTempRxQue;
    P_QUE_T prTempRxQue = &rTempRxQue;      
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;

    KAL_SPIN_LOCK_DECLARATION();
    QUEUE_INITIALIZE(prTempRxQue);
    
	/* 4 <1> Move whole list of CMD_INFO to temp queue */
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
    QUEUE_MOVE_ALL(prTempRxQue, &prAdapter->rRxQueue);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE); 

	/* 4 <2> Remove all skbuf */
    QUEUE_REMOVE_HEAD(prTempRxQue, prQueueEntry, P_QUE_ENTRY_T);
	while (prQueueEntry) {
		kalRxIndicateOnePkt(prAdapter->prGlueInfo,
				    (PVOID) GLUE_GET_PKT_DESCRIPTOR(prQueueEntry));
        QUEUE_REMOVE_HEAD(prTempRxQue, prQueueEntry, P_QUE_ENTRY_T);
    }    
     
}
#endif


/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will release thd CMD_INFO upon its attribution
 *
 * \param prAdapter  Pointer of Adapter Data Structure
 * \param prCmdInfo  Pointer of CMD_INFO_T
 * \param rTxDoneStatus  Tx done status 
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
VOID
wlanReleaseCommand(IN P_ADAPTER_T prAdapter,
		   IN P_CMD_INFO_T prCmdInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
    P_TX_CTRL_T prTxCtrl;
    P_MSDU_INFO_T prMsduInfo;

    ASSERT(prAdapter);
    ASSERT(prCmdInfo);

    prTxCtrl = &prAdapter->rTxCtrl;

	switch (prCmdInfo->eCmdType) {
    case COMMAND_TYPE_GENERAL_IOCTL:
    case COMMAND_TYPE_NETWORK_IOCTL:
        DBGLOG(INIT, INFO, ("Free CMD: BSS[%u] ID[0x%x] SeqNum[%u] OID[%u]\n",
            prCmdInfo->ucBssIndex,
            prCmdInfo->ucCID,
            prCmdInfo->ucCmdSeqNum,
            prCmdInfo->fgIsOid));

        if (prCmdInfo->fgIsOid) {
            kalOidComplete(prAdapter->prGlueInfo,
                    prCmdInfo->fgSetQuery,
				       prCmdInfo->u4SetInfoLen, WLAN_STATUS_FAILURE);
        }
        break;

    case COMMAND_TYPE_SECURITY_FRAME:
    case COMMAND_TYPE_MANAGEMENT_FRAME:
		prMsduInfo = prCmdInfo->prMsduInfo;

        if(prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME) {
            kalSecurityFrameSendComplete(prAdapter->prGlueInfo,
                prCmdInfo->prPacket, WLAN_STATUS_FAILURE);
            /* Avoid skb multiple free */
			prMsduInfo->prPacket = NULL;
        }

        DBGLOG(INIT, INFO, ("Free %s Frame: BSS[%u] WIDX:PID[%u:%u] SEQ[%u] " 
            "STA[%u] RSP[%u] CMDSeq[%u]\n",
            prCmdInfo->eCmdType == COMMAND_TYPE_SECURITY_FRAME?"SEC":"MGMT",
            prMsduInfo->ucBssIndex,
            prMsduInfo->ucWlanIndex,
            prMsduInfo->ucPID,
            prMsduInfo->ucTxSeqNum,
            prMsduInfo->ucStaRecIndex,
            prMsduInfo->pfTxDoneHandler ? TRUE:FALSE, 
            prCmdInfo->ucCmdSeqNum));

        /* invoke callbacks */
		if (prMsduInfo->pfTxDoneHandler != NULL) {
            prMsduInfo->pfTxDoneHandler(prAdapter, prMsduInfo, rTxDoneStatus);
        }

		if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME)
			GLUE_DEC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);

        cnmMgtPktFree(prAdapter, prMsduInfo);
        break;

    default:
        ASSERT(0);
        break;
    }

} /* end of wlanReleaseCommand() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function will search the CMD Queue to look for the pending OID and
*        compelete it immediately when system request a reset.
*
* \param prAdapter  ointer of Adapter Data Structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanReleasePendingOid(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
    P_QUE_T prCmdQue;
    QUE_T rTempCmdQue;
    P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("wlanReleasePendingOid");

    ASSERT(prAdapter);

	if (prAdapter->prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
        DBGLOG(INIT, INFO, ("tx_thread stopped! Releasing pending OIDs ..\n"));
	} else {
        DBGLOG(INIT, ERROR, ("OID Timeout! Releasing pending OIDs ..\n"));
        prAdapter->ucOidTimeoutCount++;
        
		if (prAdapter->ucOidTimeoutCount >= WLAN_OID_NO_ACK_THRESHOLD) {
			if (!prAdapter->fgIsChipNoAck) {
				DBGLOG(INIT, WARN,
				       ("No response from chip for %u times, set NoAck flag!\n",
					prAdapter->ucOidTimeoutCount));
            }
        
            prAdapter->fgIsChipNoAck = TRUE;           
        }
    }

    do {
#if CFG_SUPPORT_MULTITHREAD        
        KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
#endif

        /* 1: Clear Pending OID in prAdapter->rPendingCmdQueue */
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

        prCmdQue = &prAdapter->rPendingCmdQueue;
        QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

        QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
        while (prQueueEntry) {
			prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

            if (prCmdInfo->fgIsOid) {
                if (prCmdInfo->pfCmdTimeoutHandler) {
                    prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
				} else {
                    kalOidComplete(prAdapter->prGlueInfo,
                            prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_FAILURE);
                }    

                cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
                QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
            }

            QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
        }

        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

#if CFG_SUPPORT_MULTITHREAD
        /* Clear pending OID in tx_thread to hif_thread command queue */
        wlanClearTxOidCommand(prAdapter);
#endif

		/* 2: Clear pending OID in glue layer command queue */
        kalOidCmdClearance(prAdapter->prGlueInfo);

		/* 3: Clear pending OID queued in pvOidEntry with REQ_FLAG_OID set */
        kalOidClearance(prAdapter->prGlueInfo);

#if CFG_SUPPORT_MULTITHREAD   
        KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
#endif
    } while(FALSE);

    return;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This function will search the CMD Queue to look for the pending CMD/OID for specific
*        NETWORK TYPE and compelete it immediately when system request a reset.
*
* \param prAdapter  ointer of Adapter Data Structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanReleasePendingCMDbyBssIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex)
{
    P_QUE_T prCmdQue;
    QUE_T rTempCmdQue;
    P_QUE_T prTempCmdQue = &rTempCmdQue;
	P_QUE_ENTRY_T prQueueEntry = (P_QUE_ENTRY_T) NULL;
	P_CMD_INFO_T prCmdInfo = (P_CMD_INFO_T) NULL;

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);

    do {
		/* 1: Clear Pending OID in prAdapter->rPendingCmdQueue */
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);

        prCmdQue = &prAdapter->rPendingCmdQueue;
        QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

        QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
        while (prQueueEntry) {
			prCmdInfo = (P_CMD_INFO_T) prQueueEntry;

			DBGLOG(P2P, TRACE, ("Pending CMD for BSS:%d\n", prCmdInfo->ucBssIndex));

            if (prCmdInfo->ucBssIndex == ucBssIndex) {
                if (prCmdInfo->pfCmdTimeoutHandler) {
                    prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
				} else if (prCmdInfo->fgIsOid) {
                    kalOidComplete(prAdapter->prGlueInfo,
                            prCmdInfo->fgSetQuery,
						       0, WLAN_STATUS_FAILURE);
                }

                cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
                QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
            }

            QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, P_QUE_ENTRY_T);
        }

        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_PENDING);


	} while (FALSE);

    return;
} /* wlanReleasePendingCMDbyBssIdx */

/*----------------------------------------------------------------------------*/
/*!
* \brief Return the indicated packet buffer and reallocate one to the RFB
*
* \param prAdapter      Pointer of Adapter Data Structure
* \param pvPacket       Pointer of returned packet
*
* \retval WLAN_STATUS_SUCCESS: Success
* \retval WLAN_STATUS_FAILURE: Failed
*/
/*----------------------------------------------------------------------------*/
VOID wlanReturnPacketDelaySetupTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr)
{
    P_RX_CTRL_T prRxCtrl;
    P_SW_RFB_T prSwRfb = NULL;
    KAL_SPIN_LOCK_DECLARATION();
    WLAN_STATUS status = WLAN_STATUS_SUCCESS;
    P_QUE_T prQueList;

    ASSERT(prAdapter);

    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);
    
    prQueList = &prRxCtrl->rIndicatedRfbList;
	DBGLOG(RX, WARN, ("%s: IndicatedRfbList num = %u\n", __func__, prQueList->u4NumElem));

	while (QUEUE_IS_NOT_EMPTY(&prRxCtrl->rIndicatedRfbList)) {
        KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
        QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb, P_SW_RFB_T);
        KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

        status = nicRxSetupRFB(prAdapter, prSwRfb);
        nicRxReturnRFB(prAdapter, prSwRfb);

		if (status != WLAN_STATUS_SUCCESS) {
            break;
        }
    }
    
	if (status != WLAN_STATUS_SUCCESS) {
		DBGLOG(RX, WARN,
		       ("Restart ReturnIndicatedRfb Timer (%u)\n",
			RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
		/* restart timer */
        cnmTimerStartTimer(prAdapter,
                &prAdapter->rPacketDelaySetupTimer,
                SEC_TO_MSEC(RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
    }
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Return the packet buffer and reallocate one to the RFB
*
* \param prAdapter      Pointer of Adapter Data Structure
* \param pvPacket       Pointer of returned packet
*
* \retval WLAN_STATUS_SUCCESS: Success
* \retval WLAN_STATUS_FAILURE: Failed
*/
/*----------------------------------------------------------------------------*/
VOID wlanReturnPacket(IN P_ADAPTER_T prAdapter, IN PVOID pvPacket)
{
    P_RX_CTRL_T prRxCtrl;
    P_SW_RFB_T prSwRfb = NULL;
    KAL_SPIN_LOCK_DECLARATION();

    DEBUGFUNC("wlanReturnPacket");

    ASSERT(prAdapter);

    prRxCtrl = &prAdapter->rRxCtrl;
    ASSERT(prRxCtrl);

    if (pvPacket) {
        kalPacketFree(prAdapter->prGlueInfo, pvPacket);
        RX_ADD_CNT(prRxCtrl, RX_DATA_RETURNED_COUNT, 1);
#if CFG_NATIVE_802_11
        if (GLUE_TEST_FLAG(prAdapter->prGlueInfo, GLUE_FLAG_HALT)) {
        }
#endif
    }

    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
    QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb, P_SW_RFB_T);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	if (!prSwRfb) {
        DBGLOG(RX, WARN, ("No free SwRfb!\n"));
        return;
    }

	if (nicRxSetupRFB(prAdapter, prSwRfb)) {
        DBGLOG(RX, WARN, ("Cannot allocate packet buffer for SwRfb!\n"));
		if (!timerPendingTimer(&prAdapter->rPacketDelaySetupTimer)) {
			DBGLOG(RX, WARN,
			       ("Start ReturnIndicatedRfb Timer (%u)\n",
				RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
			cnmTimerStartTimer(prAdapter, &prAdapter->rPacketDelaySetupTimer,
                    SEC_TO_MSEC(RX_RETURN_INDICATED_RFB_TIMEOUT_SEC));
        }
    }
    nicRxReturnRFB(prAdapter, prSwRfb);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function that returns information about
*        the capabilities and status of the driver and/or its network adapter.
*
* \param[IN] prAdapter        Pointer to the Adapter structure.
* \param[IN] pfnOidQryHandler Function pointer for the OID query handler.
* \param[IN] pvInfoBuf        Points to a buffer for return the query information.
* \param[IN] u4QueryBufferLen Specifies the number of bytes at pvInfoBuf.
* \param[OUT] pu4QueryInfoLen  Points to the number of bytes it written or is needed.
*
* \retval WLAN_STATUS_xxx Different WLAN_STATUS code returned by different handlers.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanQueryInformation(IN P_ADAPTER_T prAdapter,
    IN PFN_OID_HANDLER_FUNC pfnOidQryHandler,
		     IN PVOID pvInfoBuf, IN UINT_32 u4InfoBufLen, OUT PUINT_32 pu4QryInfoLen)
{
    WLAN_STATUS status = WLAN_STATUS_FAILURE;

    ASSERT(prAdapter);
    ASSERT(pu4QryInfoLen);

	/* ignore any OID request after connected, under PS current measurement mode */
    if (prAdapter->u4PsCurrentMeasureEn &&
        (prAdapter->prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED)) {
		return WLAN_STATUS_SUCCESS;	/* note: return WLAN_STATUS_FAILURE or WLAN_STATUS_SUCCESS for blocking OIDs during current measurement ?? */
    }
#if 1
    /* most OID handler will just queue a command packet */
	status = pfnOidQryHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4QryInfoLen);
#else
    if (wlanIsHandlerNeedHwAccess(pfnOidQryHandler, FALSE)) {
        ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

        /* Reset sleepy state */
		if (prAdapter->fgWiFiInSleepyState == TRUE) {
            prAdapter->fgWiFiInSleepyState = FALSE;
        }

		status = pfnOidQryHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4QryInfoLen);

        RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
	} else {
		status = pfnOidQryHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4QryInfoLen);
    }
#endif

    return status;

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function that allows bound protocol drivers,
*        or NDIS, to request changes in the state information that the miniport
*        maintains for particular object identifiers, such as changes in multicast
*        addresses.
*
* \param[IN] prAdapter     Pointer to the Glue info structure.
* \param[IN] pfnOidSetHandler     Points to the OID set handlers.
* \param[IN] pvInfoBuf     Points to a buffer containing the OID-specific data for the set.
* \param[IN] u4InfoBufLen  Specifies the number of bytes at prSetBuffer.
* \param[OUT] pu4SetInfoLen Points to the number of bytes it read or is needed.
*
* \retval WLAN_STATUS_xxx Different WLAN_STATUS code returned by different handlers.
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanSetInformation(IN P_ADAPTER_T prAdapter,
    IN PFN_OID_HANDLER_FUNC pfnOidSetHandler,
		   IN PVOID pvInfoBuf, IN UINT_32 u4InfoBufLen, OUT PUINT_32 pu4SetInfoLen)
{
    WLAN_STATUS status = WLAN_STATUS_FAILURE;

    ASSERT(prAdapter);
    ASSERT(pu4SetInfoLen);

	/* ignore any OID request after connected, under PS current measurement mode */
    if (prAdapter->u4PsCurrentMeasureEn &&
        (prAdapter->prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED)) {
		return WLAN_STATUS_SUCCESS;	/* note: return WLAN_STATUS_FAILURE or WLAN_STATUS_SUCCESS for blocking OIDs during current measurement ?? */
    }
#if 1
    /* most OID handler will just queue a command packet
     * for power state transition OIDs, handler will acquire power control by itself
     */
	status = pfnOidSetHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4SetInfoLen);
#else
    if (wlanIsHandlerNeedHwAccess(pfnOidSetHandler, TRUE)) {
        ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

        /* Reset sleepy state */
		if (prAdapter->fgWiFiInSleepyState == TRUE) {
            prAdapter->fgWiFiInSleepyState = FALSE;
        }

		status = pfnOidSetHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4SetInfoLen);

        RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
	} else {
		status = pfnOidSetHandler(prAdapter, pvInfoBuf, u4InfoBufLen, pu4SetInfoLen);
    }
#endif

    return status;
}


#if CFG_SUPPORT_WAPI
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a used to query driver's config wapi mode or not
*
* \param[IN] prAdapter     Pointer to the Glue info structure.
*
* \retval TRUE for use wapi mode
*
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanQueryWapiMode(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    return prAdapter->rWifiVar.rConnSettings.fgWapiMode;
}
#endif


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to set RX filter to Promiscuous Mode.
*
* \param[IN] prAdapter        Pointer to the Adapter structure.
* \param[IN] fgEnablePromiscuousMode Enable/ disable RX Promiscuous Mode.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanSetPromiscuousMode(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnablePromiscuousMode)
{
    ASSERT(prAdapter);

}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to set RX filter to allow to receive
*        broadcast address packets.
*
* \param[IN] prAdapter        Pointer to the Adapter structure.
* \param[IN] fgEnableBroadcast Enable/ disable broadcast packet to be received.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID wlanRxSetBroadcast(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnableBroadcast)
{
    ASSERT(prAdapter);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to send out CMD_NIC_POWER_CTRL command packet
*
* \param[IN] prAdapter        Pointer to the Adapter structure.
* \param[IN] ucPowerMode      refer to CMD/EVENT document
*
* \return WLAN_STATUS_SUCCESS
* \return WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanSendNicPowerCtrlCmd(IN P_ADAPTER_T prAdapter, IN UINT_8 ucPowerMode)
{
    WLAN_STATUS status = WLAN_STATUS_SUCCESS;
    P_GLUE_INFO_T prGlueInfo;
    P_CMD_INFO_T prCmdInfo;
    P_WIFI_CMD_T prWifiCmd;
    UINT_8 ucTC, ucCmdSeqNum;

    ASSERT(prAdapter);

    prGlueInfo = prAdapter->prGlueInfo;

    /* 1. Prepare CMD */
    prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, (CMD_HDR_SIZE + sizeof(CMD_NIC_POWER_CTRL)));
    if (!prCmdInfo) {
        DBGLOG(INIT, ERROR, ("Allocate CMD_INFO_T ==> FAILED.\n"));
        return WLAN_STATUS_FAILURE;
    }

    /* 2.1 increase command sequence number */
    ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);
    DBGLOG(REQ, TRACE, ("ucCmdSeqNum =%d\n", ucCmdSeqNum));

    /* 2.2 Setup common CMD Info Packet */
    prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = (UINT_16) (CMD_HDR_SIZE + sizeof(CMD_NIC_POWER_CTRL));
    prCmdInfo->pfCmdDoneHandler = NULL;
    prCmdInfo->pfCmdTimeoutHandler = NULL;
    prCmdInfo->fgIsOid = TRUE;
    prCmdInfo->ucCID = CMD_ID_NIC_POWER_CTRL;
    prCmdInfo->fgSetQuery = TRUE;
    prCmdInfo->fgNeedResp = FALSE;
    prCmdInfo->fgDriverDomainMCR = FALSE;
    prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
    prCmdInfo->u4SetInfoLen = sizeof(CMD_NIC_POWER_CTRL);

    /* 2.3 Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
    prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
    prWifiCmd->u2PQ_ID = CMD_PQ_ID;
    prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
    prWifiCmd->ucCID = prCmdInfo->ucCID;
    prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
    prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

    kalMemZero(prWifiCmd->aucBuffer, sizeof(CMD_NIC_POWER_CTRL));
	((P_CMD_NIC_POWER_CTRL) (prWifiCmd->aucBuffer))->ucPowerMode = ucPowerMode;

    /* 3. Issue CMD for entering specific power mode */
    ucTC = TC4_INDEX;

	while (1) {
		/* 3.0 Removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
            status = WLAN_STATUS_FAILURE;
            break;
        }
		/* 3.1 Acquire TX Resource */
		if (nicTxAcquireResource(prAdapter, ucTC, nicTxGetCmdPageCount(prCmdInfo)) ==
		    WLAN_STATUS_RESOURCES) {
            if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR,
				       ("Fail to get TX resource return within timeout\n"));
                status = WLAN_STATUS_FAILURE;
                prAdapter->fgIsChipNoAck = TRUE;
                //break;
			} else {
				DBGLOG(INIT, WARN, ("TL:wlanSendNicPowerCtrlCmd:continue\n"));
                continue;
            }
        }
		/* 3.2 Send CMD Info Packet */
        if (nicTxCmd(prAdapter, prCmdInfo, ucTC) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, ("Fail to transmit CMD_NIC_POWER_CTRL command\n"));
            status = WLAN_STATUS_FAILURE;
        }

        break;
    };

	/* 4. Free CMD Info Packet. */
    cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	/* 5. Add flag */
	if (ucPowerMode == 1) {
        prAdapter->fgIsEnterD3ReqIssued = TRUE;
    }

    return status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to check if it is RF test mode and
*        the OID is allowed to be called or not
*
* \param[IN] prAdapter        Pointer to the Adapter structure.
* \param[IN] fgEnableBroadcast Enable/ disable broadcast packet to be received.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanIsHandlerAllowedInRFTest(IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN BOOLEAN fgSetInfo)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerAllowedInRFTest;
    UINT_32 i;
    UINT_32 u4NumOfElem;

    if (fgSetInfo) {
        apfnOidHandlerAllowedInRFTest = apfnOidSetHandlerAllowedInRFTest;
		u4NumOfElem =
		    sizeof(apfnOidSetHandlerAllowedInRFTest) / sizeof(PFN_OID_HANDLER_FUNC);
	} else {
        apfnOidHandlerAllowedInRFTest = apfnOidQueryHandlerAllowedInRFTest;
		u4NumOfElem =
		    sizeof(apfnOidQueryHandlerAllowedInRFTest) / sizeof(PFN_OID_HANDLER_FUNC);
    }

    for (i = 0; i < u4NumOfElem; i++) {
        if (apfnOidHandlerAllowedInRFTest[i] == pfnOidHandler) {
            return TRUE;
        }
    }

    return FALSE;
}

#if CFG_ENABLE_FW_DOWNLOAD
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to configure FWDL parameters
*
* @param prAdapter      Pointer to the Adapter structure.
*        u4DestAddr     Address of destination address
*        u4ImgSecSize   Length of the firmware block
*        fgReset        should be set to TRUE if this is the 1st configuration
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanImageSectionConfig(IN P_ADAPTER_T prAdapter,
		       IN UINT_32 u4DestAddr, IN UINT_32 u4ImgSecSize, IN BOOLEAN fgReset)
{
    P_CMD_INFO_T prCmdInfo;
    P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
    P_INIT_CMD_DOWNLOAD_CONFIG prInitCmdDownloadConfig;
    UINT_8 ucTC, ucCmdSeqNum;
    WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

    ASSERT(prAdapter);

    DEBUGFUNC("wlanImageSectionConfig");

    if (u4ImgSecSize == 0) {
        return WLAN_STATUS_SUCCESS;
    }
	/* 1. Allocate CMD Info Packet and its Buffer. */
    prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
					  sizeof(INIT_HIF_TX_HEADER_T) +
					  sizeof(INIT_CMD_DOWNLOAD_CONFIG));

    if (!prCmdInfo) {
        DBGLOG(INIT, ERROR, ("Allocate CMD_INFO_T ==> FAILED.\n"));
        return WLAN_STATUS_FAILURE;
    }

    prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_DOWNLOAD_CONFIG);

	/* 2. Use TC4's resource to download image. (TC4 as CPU) */
    ucTC = TC4_INDEX;

	/* 3. increase command sequence number */
    ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
    prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
    prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;

    prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_DOWNLOAD_CONFIG;
    prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PACKET_TYPE_ID;
    prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	/* 5. Setup CMD_DOWNLOAD_CONFIG */
	prInitCmdDownloadConfig =
	    (P_INIT_CMD_DOWNLOAD_CONFIG) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
    prInitCmdDownloadConfig->u4Address = u4DestAddr;
    prInitCmdDownloadConfig->u4Length = u4ImgSecSize;
    prInitCmdDownloadConfig->u4DataMode = 0
#if CFG_ENABLE_FW_DOWNLOAD_ACK
	    | DOWNLOAD_CONFIG_ACK_OPTION	/* ACK needed */
#endif
#if CFG_ENABLE_FW_ENCRYPTION
        | DOWNLOAD_CONFIG_ENCRYPTION_MODE
#endif
        ;

	if (fgReset == TRUE) {
        prInitCmdDownloadConfig->u4DataMode |= DOWNLOAD_CONFIG_RESET_OPTION;
    }
	/* 6. Send FW_Download command */
	while (1) {
		/* 6.1 Acquire TX Resource */
		if (nicTxAcquireResource
		    (prAdapter, ucTC,
		     nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE)) == WLAN_STATUS_RESOURCES) {
            if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
                u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR,
				       ("Fail to get TX resource return within timeout\n"));
                break;
			} else {
                continue;
            }
        }
		/* 6.2 Send CMD Info Packet */
        if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
            u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, ("Fail to transmit image download command\n"));
        }

        break;
    };

#if CFG_ENABLE_FW_DOWNLOAD_ACK
	/* 7. Wait for INIT_EVENT_ID_CMD_RESULT */
    u4Status = wlanImageSectionDownloadStatus(prAdapter, ucCmdSeqNum);
#endif

	/* 8. Free CMD Info Packet. */
    cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

    return u4Status;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to download FW image.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanImageSectionDownload(IN P_ADAPTER_T prAdapter, IN UINT_32 u4ImgSecSize, IN PUINT_8 pucImgSecBuf)
{
    P_CMD_INFO_T prCmdInfo;
    P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
    WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

    ASSERT(prAdapter);
    ASSERT(pucImgSecBuf);
    ASSERT(u4ImgSecSize <= CMD_PKT_SIZE_FOR_IMAGE);

    DEBUGFUNC("wlanImageSectionDownload");

    if (u4ImgSecSize == 0) {
        return WLAN_STATUS_SUCCESS;
    }
	/* 1. Allocate CMD Info Packet and its Buffer. */
    prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, sizeof(INIT_HIF_TX_HEADER_T) + u4ImgSecSize);

    if (!prCmdInfo) {
        DBGLOG(INIT, ERROR, ("Allocate CMD_INFO_T ==> FAILED.\n"));
        return WLAN_STATUS_FAILURE;
    }

	prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T) + (UINT_16) u4ImgSecSize;

	/* 2. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
    prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
    prInitHifTxHeader->u2PQ_ID = INIT_CMD_PDA_PQ_ID;

    prInitHifTxHeader->rInitWifiCmd.ucCID = 0;
    prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PDA_PACKET_TYPE_ID;
    prInitHifTxHeader->rInitWifiCmd.ucSeqNum = 0;

	/* 3. Setup DOWNLOAD_BUF */
    kalMemCopy(prInitHifTxHeader->rInitWifiCmd.aucBuffer, pucImgSecBuf, u4ImgSecSize);

	/* 4. Send FW_Download command */
    if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
        u4Status = WLAN_STATUS_FAILURE;
		DBGLOG(INIT, ERROR, ("Fail to transmit image download command\n"));
    }
	/* 5. Free CMD Info Packet. */
    cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

    return u4Status;
}

/* for AOSP */
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to download FW by ilm and dlm section.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

VOID
wlanFwDvdDwnloadHandler(IN P_ADAPTER_T prAdapter,
		IN P_FIRMWARE_DIVIDED_DOWNLOAD_T prFwHead, IN PVOID pvFwImageMapFile, OUT WLAN_STATUS *u4Status)
{
	UINT_32 u4ImgSecSize, i, j;
	
	for (i = 0; i < prFwHead->u4NumOfEntries; i++) {
		if (wlanImageSectionConfig(prAdapter,
					prFwHead->arSection[i].u4DestAddr,
					prFwHead->arSection[i].u4Length,
					i == 0 ? TRUE : FALSE) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, ("Firmware download configuration failed!\n"));
			*u4Status = WLAN_STATUS_FAILURE;
			break;
		} else {
			for (j = 0; j < prFwHead->arSection[i].u4Length; j += CMD_PKT_SIZE_FOR_IMAGE) {
				if (j + CMD_PKT_SIZE_FOR_IMAGE < prFwHead->arSection[i].u4Length)
					u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
				else
					u4ImgSecSize = prFwHead->arSection[i].u4Length - j;

				if (wlanImageSectionDownload(prAdapter,
							     u4ImgSecSize,
							     (PUINT_8)pvFwImageMapFile + prFwHead->arSection[i].u4Offset + j) != WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, ERROR, ("Firmware scatter download failed!\n"));
					*u4Status = WLAN_STATUS_FAILURE;
					break;
				}
			}

			/* escape from loop if any pending error occurs */
			if (*u4Status == WLAN_STATUS_FAILURE) {
				break;
			}
		}
	}
}

/* for AOSP */
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to download FW
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/

VOID
wlanFwDwnloadHandler(IN P_ADAPTER_T prAdapter,
	IN UINT_32 u4FwImgLength, IN PVOID pvFwImageMapFile, OUT WLAN_STATUS *u4Status)
{
	UINT_32 u4ImgSecSize, i;
	
	for (i = 0; i < u4FwImgLength; i += CMD_PKT_SIZE_FOR_IMAGE) {
		if (i + CMD_PKT_SIZE_FOR_IMAGE < u4FwImgLength)
			u4ImgSecSize = CMD_PKT_SIZE_FOR_IMAGE;
		else
			u4ImgSecSize = u4FwImgLength - i;

		if (wlanImageSectionDownload(prAdapter,
					     u4ImgSecSize,
					     (PUINT_8)pvFwImageMapFile + i) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, ("wlanImageSectionDownload failed!\n"));
			*u4Status = WLAN_STATUS_FAILURE;
			break;
		}
	}
}

#if !CFG_ENABLE_FW_DOWNLOAD_ACK
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to confirm previously firmware download is done without error
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanImageQueryStatus(IN P_ADAPTER_T prAdapter)
{
    P_CMD_INFO_T prCmdInfo;
    P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
    UINT_8 aucBuffer[sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_EVENT_PENDING_ERROR)];
    UINT_32 u4RxPktLength;
    P_INIT_HIF_RX_HEADER_T prInitHifRxHeader;
    P_INIT_EVENT_PENDING_ERROR prEventPendingError;
    WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;
    UINT_8 ucTC, ucCmdSeqNum;

    ASSERT(prAdapter);

    DEBUGFUNC("wlanImageQueryStatus");

	/* 1. Allocate CMD Info Packet and it Buffer. */
    prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, sizeof(INIT_HIF_TX_HEADER_T));

    if (!prCmdInfo) {
        DBGLOG(INIT, ERROR, ("Allocate CMD_INFO_T ==> FAILED.\n"));
        return WLAN_STATUS_FAILURE;
    }

    kalMemZero(prCmdInfo->pucInfoBuffer, sizeof(INIT_HIF_TX_HEADER_T));
    prCmdInfo->u2InfoBufLen = sizeof(INIT_HIF_TX_HEADER_T);

	/* 2. Use TC0's resource to download image. (only TC0 is allowed) */
    ucTC = TC0_INDEX;

	/* 3. increase command sequence number */
    ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);

    prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
    prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;

    prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_QUERY_PENDING_ERROR;
    prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PACKET_TYPE_ID;
    prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	/* 5. Send command */
	while (1) {
		/* 5.1 Acquire TX Resource */
		if (nicTxAcquireResource
		    (prAdapter, ucTC,
		     nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE)) == WLAN_STATUS_RESOURCES) {
            if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
                u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR,
				       ("Fail to get TX resource return within timeout\n"));
                break;
			} else {
                continue;
            }
        }
		/* 5.2 Send CMD Info Packet */
        if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
            u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, ("Fail to transmit image download command\n"));
        }

        break;
    };

	/* 6. Wait for INIT_EVENT_ID_PENDING_ERROR */
    do {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
            u4Status = WLAN_STATUS_FAILURE;
		} else if (nicRxWaitResponse(prAdapter,
                    0,
                    aucBuffer,
					     sizeof(INIT_HIF_RX_HEADER_T) +
					     sizeof(INIT_EVENT_PENDING_ERROR),
                    &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
            u4Status = WLAN_STATUS_FAILURE;
		} else {
            prInitHifRxHeader = (P_INIT_HIF_RX_HEADER_T) aucBuffer;

			/* EID / SeqNum check */
			if (prInitHifRxHeader->rInitWifiEvent.ucEID != INIT_EVENT_ID_PENDING_ERROR) {
                u4Status = WLAN_STATUS_FAILURE;
			} else if (prInitHifRxHeader->rInitWifiEvent.ucSeqNum != ucCmdSeqNum) {
                u4Status = WLAN_STATUS_FAILURE;
			} else {
				prEventPendingError =
				    (P_INIT_EVENT_PENDING_ERROR) (prInitHifRxHeader->rInitWifiEvent.
								  aucBuffer);
				if (prEventPendingError->ucStatus != 0) {	/* 0 for download success */
                    u4Status = WLAN_STATUS_FAILURE;
				} else {
                    u4Status = WLAN_STATUS_SUCCESS;
                }
            }
        }
    } while (FALSE);

	/* 7. Free CMD Info Packet. */
    cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

    return u4Status;
}


#else
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to confirm the status of
*        previously downloaded firmware scatter
*
* @param prAdapter      Pointer to the Adapter structure.
*        ucCmdSeqNum    Sequence number of previous firmware scatter
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanImageSectionDownloadStatus(IN P_ADAPTER_T prAdapter, IN UINT_8 ucCmdSeqNum)
{
    UINT_8 aucBuffer[sizeof(INIT_HIF_RX_HEADER_T) + sizeof(INIT_EVENT_CMD_RESULT)];
    P_INIT_HIF_RX_HEADER_T prInitHifRxHeader;
    P_INIT_EVENT_CMD_RESULT prEventCmdResult;
    UINT_32 u4RxPktLength;
    WLAN_STATUS u4Status;

    ASSERT(prAdapter);

    do {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
            u4Status = WLAN_STATUS_FAILURE;
		} else if (nicRxWaitResponse(prAdapter,
                    0,
                    aucBuffer,
					     sizeof(INIT_HIF_RX_HEADER_T) +
					     sizeof(INIT_EVENT_CMD_RESULT),
                    &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
            u4Status = WLAN_STATUS_FAILURE;
		} else {
            prInitHifRxHeader = (P_INIT_HIF_RX_HEADER_T) aucBuffer;

			/* EID / SeqNum check */
			if (prInitHifRxHeader->rInitWifiEvent.ucEID != INIT_EVENT_ID_CMD_RESULT) {
                u4Status = WLAN_STATUS_FAILURE;
			} else if (prInitHifRxHeader->rInitWifiEvent.ucSeqNum != ucCmdSeqNum) {
                u4Status = WLAN_STATUS_FAILURE;
			} else {
				prEventCmdResult =
				    (P_INIT_EVENT_CMD_RESULT) (prInitHifRxHeader->rInitWifiEvent.
							       aucBuffer);
				if (prEventCmdResult->ucStatus != 0) {	/* 0 for download success */
                    u4Status = WLAN_STATUS_FAILURE;
				} else {
                    u4Status = WLAN_STATUS_SUCCESS;
                }
            }
        }
    } while (FALSE);

    return u4Status;
}


#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to start FW normal operation.
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanConfigWifiFunc(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnable, IN UINT_32 u4StartAddress)
{
    P_CMD_INFO_T prCmdInfo;
    P_INIT_HIF_TX_HEADER_T prInitHifTxHeader;
    P_INIT_CMD_WIFI_START prInitCmdWifiStart;
    UINT_8 ucTC, ucCmdSeqNum;
    WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS;

    ASSERT(prAdapter);

    DEBUGFUNC("wlanConfigWifiFunc");

	/* 1. Allocate CMD Info Packet and its Buffer. */
    prCmdInfo = cmdBufAllocateCmdInfo(prAdapter,
					  sizeof(INIT_HIF_TX_HEADER_T) +
					  sizeof(INIT_CMD_WIFI_START));

    if (!prCmdInfo) {
        DBGLOG(INIT, ERROR, ("Allocate CMD_INFO_T ==> FAILED.\n"));
        return WLAN_STATUS_FAILURE;
    }

	kalMemZero(prCmdInfo->pucInfoBuffer, 
        sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_WIFI_START));
	prCmdInfo->u2InfoBufLen = 
        sizeof(INIT_HIF_TX_HEADER_T) + sizeof(INIT_CMD_WIFI_START);

	/* 2. Always use TC0 */
    ucTC = TC0_INDEX;

	/* 3. increase command sequence number */
    ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* 4. Setup common CMD Info Packet */
	prInitHifTxHeader = (P_INIT_HIF_TX_HEADER_T) (prCmdInfo->pucInfoBuffer);
    prInitHifTxHeader->u2TxByteCount = prCmdInfo->u2InfoBufLen;
    prInitHifTxHeader->u2PQ_ID = INIT_CMD_PQ_ID;

    prInitHifTxHeader->rInitWifiCmd.ucCID = INIT_CMD_ID_WIFI_START;
    prInitHifTxHeader->rInitWifiCmd.ucPktTypeID = INIT_CMD_PACKET_TYPE_ID;
    prInitHifTxHeader->rInitWifiCmd.ucSeqNum = ucCmdSeqNum;

	prInitCmdWifiStart = (P_INIT_CMD_WIFI_START) (prInitHifTxHeader->rInitWifiCmd.aucBuffer);
    prInitCmdWifiStart->u4Override = (fgEnable == TRUE ? 1 : 0);
    prInitCmdWifiStart->u4Address = u4StartAddress;

	/* 5. Seend WIFI start command */
	while (1) {
		/* 5.1 Acquire TX Resource */
		if (nicTxAcquireResource
		    (prAdapter, ucTC,
		     nicTxGetPageCount(prCmdInfo->u2InfoBufLen, TRUE)) == WLAN_STATUS_RESOURCES) {
            if (nicTxPollingResource(prAdapter, ucTC) != WLAN_STATUS_SUCCESS) {
                u4Status = WLAN_STATUS_FAILURE;
				DBGLOG(INIT, ERROR,
				       ("Fail to get TX resource return within timeout\n"));
                break;
			} else {
                continue;
            }
        }
		/* 5.2 Send CMD Info Packet */
        if (nicTxInitCmd(prAdapter, prCmdInfo) != WLAN_STATUS_SUCCESS) {
            u4Status = WLAN_STATUS_FAILURE;
			DBGLOG(INIT, ERROR, ("Fail to transmit WIFI start command\n"));
        }

        break;
    };

	/* 6. Free CMD Info Packet. */
    cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

    return u4Status;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is used to generate CRC32 checksum
*
* @param buf Pointer to the data.
* @param len data length
*
* @return crc32 value
*/
/*----------------------------------------------------------------------------*/
UINT_32 wlanCRC32(PUINT_8 buf, UINT_32 len)
{
    UINT_32 i, crc32 = 0xFFFFFFFF;
    const UINT_32 crc32_ccitt_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
        0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
        0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
        0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
        0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
        0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
        0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
        0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
        0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
        0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
        0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
        0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
        0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
        0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
        0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
        0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
        0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
        0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
        0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
        0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
        0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
        0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
        0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
        0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
        0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
        0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
        0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
        0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
        0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
        0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
        0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
        0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
        0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
        0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
        0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
        0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
		0x2d02ef8d
	};

    for (i = 0; i < len; i++)
        crc32 = crc32_ccitt_table[(crc32 ^ buf[i]) & 0xff] ^ (crc32 >> 8);

	return ~crc32;
}
#endif


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to process queued RX packets
*
* @param prAdapter          Pointer to the Adapter structure.
*        prSwRfbListHead    Pointer to head of RX packets link list
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanProcessQueuedSwRfb(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfbListHead)
{
    P_SW_RFB_T prSwRfb, prNextSwRfb;
    P_TX_CTRL_T prTxCtrl;
    P_RX_CTRL_T prRxCtrl;

    ASSERT(prAdapter);
    ASSERT(prSwRfbListHead);

    prTxCtrl = &prAdapter->rTxCtrl;
    prRxCtrl = &prAdapter->rRxCtrl;

    prSwRfb = prSwRfbListHead;

    do {
		/* save next first */
		prNextSwRfb = (P_SW_RFB_T) QUEUE_GET_NEXT_ENTRY((P_QUE_ENTRY_T) prSwRfb);

		switch (prSwRfb->eDst) {
        case RX_PKT_DESTINATION_HOST:
            nicRxProcessPktWithoutReorder(prAdapter, prSwRfb);
            break;

        case RX_PKT_DESTINATION_FORWARD:
            nicRxProcessForwardPkt(prAdapter, prSwRfb);
            break;

        case RX_PKT_DESTINATION_HOST_WITH_FORWARD:
            nicRxProcessGOBroadcastPkt(prAdapter, prSwRfb);
            break;

        case RX_PKT_DESTINATION_NULL:
            nicRxReturnRFB(prAdapter, prSwRfb);
            break;

        default:
            break;
        }

#if CFG_HIF_RX_STARVATION_WARNING
        prRxCtrl->u4DequeuedCnt++;
#endif
        prSwRfb = prNextSwRfb;
	} while (prSwRfb);

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to purge queued TX packets
*        by indicating failure to OS and returned to free list
*
* @param prAdapter          Pointer to the Adapter structure.
*        prMsduInfoListHead Pointer to head of TX packets link list
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanProcessQueuedMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead)
{
    ASSERT(prAdapter);
    ASSERT(prMsduInfoListHead);

    nicTxFreeMsduInfoPacket(prAdapter, prMsduInfoListHead);
    nicTxReturnMsduInfo(prAdapter, prMsduInfoListHead);

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to check if the OID handler needs timeout
*
* @param prAdapter          Pointer to the Adapter structure.
*        pfnOidHandler      Pointer to the OID handler
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanoidTimeoutCheck(IN P_ADAPTER_T prAdapter, IN PFN_OID_HANDLER_FUNC pfnOidHandler)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerWOTimeoutCheck;
    UINT_32 i;
    UINT_32 u4NumOfElem;
    UINT_32 u4OidTimeout;

    apfnOidHandlerWOTimeoutCheck = apfnOidWOTimeoutCheck;
    u4NumOfElem = sizeof(apfnOidWOTimeoutCheck) / sizeof(PFN_OID_HANDLER_FUNC);

    for (i = 0; i < u4NumOfElem; i++) {
        if (apfnOidHandlerWOTimeoutCheck[i] == pfnOidHandler) {
            return FALSE;
        }
    }

    /* Decrease OID timeout threshold if chip NoAck/resetting */
	if (wlanIsChipNoAck(prAdapter)) {
        u4OidTimeout = WLAN_OID_TIMEOUT_THRESHOLD_IN_RESETING;
		DBGLOG(INIT, INFO,
		       ("Decrease OID timeout to %ums due to NoACK/CHIP-RESET\n", u4OidTimeout));
	} else {
        u4OidTimeout = WLAN_OID_TIMEOUT_THRESHOLD;
    }
    
    /* Set OID timer for timeout check */
	cnmTimerStartTimer(prAdapter, &(prAdapter->rOidTimeoutTimer), u4OidTimeout);

    return TRUE;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to clear any pending OID timeout check
*
* @param prAdapter          Pointer to the Adapter structure.
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID wlanoidClearTimeoutCheck(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    cnmTimerStopTimer(prAdapter, &(prAdapter->rOidTimeoutTimer));
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to override network address
*        if NVRAM has a valid value
*
* @param prAdapter          Pointer to the Adapter structure.
*
* @return WLAN_STATUS_FAILURE   The request could not be processed
*         WLAN_STATUS_PENDING   The request has been queued for later processing
*         WLAN_STATUS_SUCCESS   The request has been processed
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanUpdateNetworkAddress(IN P_ADAPTER_T prAdapter)
{
    const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
    PARAM_MAC_ADDRESS rMacAddr;
    UINT_32 u4SysTime;

    DEBUGFUNC("wlanUpdateNetworkAddress");

    ASSERT(prAdapter);

	if (kalRetrieveNetworkAddress(prAdapter->prGlueInfo, &rMacAddr) == FALSE
            || IS_BMCAST_MAC_ADDR(rMacAddr)
            || EQUAL_MAC_ADDR(aucZeroMacAddr, rMacAddr)) {
		/* eFUSE has a valid address, don't do anything */
		if (prAdapter->fgIsEmbbededMacAddrValid == TRUE) {
#if CFG_SHOW_MACADDR_SOURCE
            DBGLOG(INIT, INFO, ("Using embedded MAC address"));
#endif
            return WLAN_STATUS_SUCCESS;
		} else {
#if CFG_SHOW_MACADDR_SOURCE
            DBGLOG(INIT, INFO, ("Using dynamically generated MAC address"));
#endif
			/* dynamic generate */
            u4SysTime = (UINT_32) kalGetTimeTick();

            rMacAddr[0] = 0x00;
            rMacAddr[1] = 0x08;
            rMacAddr[2] = 0x22;

            kalMemCopy(&rMacAddr[3], &u4SysTime, 3);
        }
	} else {
#if CFG_SHOW_MACADDR_SOURCE
        DBGLOG(INIT, INFO, ("Using host-supplied MAC address"));
#endif
    }

    COPY_MAC_ADDR(prAdapter->rWifiVar.aucMacAddress, rMacAddr);

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to update basic configuration into firmware domain
*
* @param prAdapter          Pointer to the Adapter structure.
*
* @return WLAN_STATUS_FAILURE   The request could not be processed
*         WLAN_STATUS_PENDING   The request has been queued for later processing
*         WLAN_STATUS_SUCCESS   The request has been processed
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanUpdateBasicConfig(IN P_ADAPTER_T prAdapter)
{
    UINT_8 ucCmdSeqNum;
    P_CMD_INFO_T prCmdInfo;
    P_WIFI_CMD_T prWifiCmd;
    P_CMD_BASIC_CONFIG_T prCmdBasicConfig;

    DEBUGFUNC("wlanUpdateBasicConfig");

    ASSERT(prAdapter);

    prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(CMD_BASIC_CONFIG_T));

    if (!prCmdInfo) {
        DBGLOG(INIT, ERROR, ("Allocate CMD_INFO_T ==> FAILED.\n"));
        return WLAN_STATUS_FAILURE;
    }
	/* increase command sequence number */
    ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
    prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
    prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(CMD_BASIC_CONFIG_T);
    prCmdInfo->pfCmdDoneHandler = NULL;
    prCmdInfo->pfCmdTimeoutHandler = NULL;
    prCmdInfo->fgIsOid = FALSE;
    prCmdInfo->ucCID = CMD_ID_BASIC_CONFIG;
    prCmdInfo->fgSetQuery = TRUE;
    prCmdInfo->fgNeedResp = FALSE;
    prCmdInfo->fgDriverDomainMCR = FALSE;
    prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
    prCmdInfo->u4SetInfoLen = sizeof(CMD_BASIC_CONFIG_T);

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
    prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
    prWifiCmd->u2PQ_ID = CMD_PQ_ID;
    prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
    prWifiCmd->ucCID = prCmdInfo->ucCID;
    prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
    prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

	/* configure CMD_BASIC_CONFIG */
	prCmdBasicConfig = (P_CMD_BASIC_CONFIG_T) (prWifiCmd->aucBuffer);
    prCmdBasicConfig->ucNative80211 = 0;
    prCmdBasicConfig->rCsumOffload.u2RxChecksum = 0;
    prCmdBasicConfig->rCsumOffload.u2TxChecksum = 0;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_TCP)
        prCmdBasicConfig->rCsumOffload.u2TxChecksum |= BIT(2);

	if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_UDP)
        prCmdBasicConfig->rCsumOffload.u2TxChecksum |= BIT(1);

	if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_IP)
        prCmdBasicConfig->rCsumOffload.u2TxChecksum |= BIT(0);

	if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_RX_TCP)
        prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(2);

	if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_RX_UDP)
        prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(1);

	if (prAdapter->u4CSUMFlags & (CSUM_OFFLOAD_EN_RX_IPv4 | CSUM_OFFLOAD_EN_RX_IPv6))
        prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(0);
#endif

	if (wlanSendCommand(prAdapter, prCmdInfo) == WLAN_STATUS_RESOURCES) {
		kalEnqueueCommand(prAdapter->prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

        return WLAN_STATUS_PENDING;
	} else {
        cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
        
        return WLAN_STATUS_SUCCESS;
    }
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to check if the device is in RF test mode
*
* @param pfnOidHandler      Pointer to the OID handler
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanQueryTestMode(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    return prAdapter->fgTestMode;
}

BOOLEAN wlanProcessTxFrame(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket)
{
    UINT_32         u4SysTime;
    UINT_8          ucMacHeaderLen;
    TX_PACKET_INFO rTxPacketInfo;

    ASSERT(prAdapter);
    ASSERT(prPacket);

    if (kalQoSFrameClassifierAndPacketInfo(prAdapter->prGlueInfo,
					       prPacket, &rTxPacketInfo)) {
                
        /* Save the value of Priority Parameter */
		GLUE_SET_PKT_TID(prPacket, rTxPacketInfo.ucPriorityParam);


		if(rTxPacketInfo.u2Flag) {
			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_1X)) {
				P_STA_RECORD_T			prStaRec;

				DBGLOG(RSN, INFO, ("T1X len=%d\n", rTxPacketInfo.u4PacketLen));

				prStaRec = cnmGetStaRecByAddress(prAdapter, 
					GLUE_GET_PKT_BSS_IDX(prPacket), rTxPacketInfo.aucEthDestAddr);

				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_1X);
				
				if (secIsProtected1xFrame(prAdapter, prStaRec) && !secIs24Of4Packet(prPacket)) {
					GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_PROTECTED_1X);
				}
			}

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_802_3)) {
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_802_3);
			}

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_VLAN_EXIST)) {
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_VLAN_EXIST);
			}

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_DHCP)) {
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_DHCP);
			}

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_ARP)) {
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_ARP);
			}

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_DNS)) {
                GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_DNS);
            }
		}


        ucMacHeaderLen = ETHER_HEADER_LEN;

        /* Save the value of Header Length */
        GLUE_SET_PKT_HEADER_LEN(prPacket, ucMacHeaderLen);

        /* Save the value of Frame Length */
		GLUE_SET_PKT_FRAME_LEN(prPacket, (UINT_16)rTxPacketInfo.u4PacketLen);

		/* Save the value of Arrival Time */
		u4SysTime = (OS_SYSTIME) kalGetTimeTick();
        GLUE_SET_PKT_ARRIVAL_TIME(prPacket, u4SysTime);
        
        return TRUE;
    }

    return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to identify 802.1x and Bluetooth-over-Wi-Fi
*        security frames, and queued into command queue for strict ordering
*        due to 802.1x frames before add-key OIDs are not to be encrypted
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param prPacket       Pointer of native packet
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanProcessSecurityFrame(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket)
{
	P_CMD_INFO_T			prCmdInfo;
	P_STA_RECORD_T			prStaRec;
	UINT_8					ucBssIndex;  
	UINT_32 				u4PacketLen;
	UINT_8					aucEthDestAddr[PARAM_MAC_ADDR_LEN];
	P_MSDU_INFO_T			prMsduInfo;

	ASSERT(prAdapter);
	ASSERT(prPacket);

	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, 0);
	
	/* Get MSDU_INFO for TxDone */
	prMsduInfo = cnmPktAlloc(prAdapter, 0);

	u4PacketLen = (UINT_32) GLUE_GET_PKT_FRAME_LEN(prPacket);

	if (prCmdInfo && prMsduInfo) {
		ucBssIndex = GLUE_GET_PKT_BSS_IDX(prPacket);

		kalGetEthDestAddr(prAdapter->prGlueInfo, prPacket, aucEthDestAddr);

		prStaRec = cnmGetStaRecByAddress(prAdapter, ucBssIndex, aucEthDestAddr);

		prCmdInfo->eCmdType 			= COMMAND_TYPE_SECURITY_FRAME;
		prCmdInfo->u2InfoBufLen 		= (UINT_16)u4PacketLen;
		prCmdInfo->prPacket 			= prPacket;
		prCmdInfo->prMsduInfo			= prMsduInfo;
		if (prStaRec) {
			prCmdInfo->ucStaRecIndex	=  prStaRec->ucIndex;
		} else {
			prCmdInfo->ucStaRecIndex	=  STA_REC_INDEX_NOT_FOUND;
		}
		prCmdInfo->ucBssIndex			= ucBssIndex;
		prCmdInfo->pfCmdDoneHandler 	= wlanSecurityFrameTxDone;
		prCmdInfo->pfCmdTimeoutHandler	= wlanSecurityFrameTxTimeout;
		prCmdInfo->fgIsOid				= FALSE;
		prCmdInfo->fgSetQuery			= TRUE;
		prCmdInfo->fgNeedResp			= FALSE;

		/* Fill-up MSDU_INFO */
		nicTxSetDataPacket(prAdapter, prMsduInfo, ucBssIndex, 
			prCmdInfo->ucStaRecIndex, 0, u4PacketLen, nicTxDummyTxDone, 
			MSDU_RATE_MODE_AUTO, TX_PACKET_OS, 0, FALSE, TRUE);

		prMsduInfo->prPacket = prPacket; 
		/* No Tx descriptor template for MMPDU */
		prMsduInfo->fgIsTXDTemplateValid = FALSE;
		
		if(GLUE_TEST_PKT_FLAG(prPacket, ENUM_PKT_PROTECTED_1X)) {
			nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME, TRUE);
		}
		
#if CFG_SUPPORT_MULTITHREAD
		nicTxComposeSecurityFrameDesc(prAdapter, prCmdInfo, 
			prMsduInfo->aucTxDescBuffer, NULL);
#endif

		kalEnqueueCommand(prAdapter->prGlueInfo, (P_QUE_ENTRY_T) prCmdInfo);

		GLUE_SET_EVENT(prAdapter->prGlueInfo);

		return TRUE;
	} else {
		DBGLOG(RSN, INFO, ("Failed to alloc CMD/MGMT INFO for 1X frame!!\n"));
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		cnmPktFree(prAdapter, prMsduInfo);
	}

	return FALSE;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when 802.1x or Bluetooth-over-Wi-Fi
*        security frames has been sent to firmware
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param prCmdInfo      Pointer of CMD_INFO_T
* @param pucEventBuf    meaningless, only for API compatibility
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID
wlanSecurityFrameTxDone(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
    ASSERT(prAdapter);
    ASSERT(prCmdInfo);

	if (GET_BSS_INFO_BY_INDEX(prAdapter, prCmdInfo->ucBssIndex)->eNetworkType ==
	    NETWORK_TYPE_AIS && prAdapter->rWifiVar.rAisSpecificBssInfo.fgCounterMeasure) {
        P_STA_RECORD_T prSta = cnmGetStaRecByIndex(prAdapter, prCmdInfo->ucBssIndex);
        if (prSta) {
            kalMsleep(10);
            if (authSendDeauthFrame(prAdapter,
						GET_BSS_INFO_BY_INDEX(prAdapter,
								      prCmdInfo->ucBssIndex), prSta,
						(P_SW_RFB_T) NULL, REASON_CODE_MIC_FAILURE,
						(PFN_TX_DONE_HANDLER) NULL
						/* secFsmEventDeauthTxDone left upper layer handle the 60 timer */
						) != WLAN_STATUS_SUCCESS) {
                ASSERT(FALSE);
            }
			/* secFsmEventEapolTxDone(prAdapter, prSta, TX_RESULT_SUCCESS); */
        }
    }

	DBGLOG(RSN, INFO, ("SECURITY PKT TX DONE\n"));
    kalSecurityFrameSendComplete(prAdapter->prGlueInfo,
				     prCmdInfo->prPacket, WLAN_STATUS_SUCCESS);
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when 802.1x or Bluetooth-over-Wi-Fi
*        security frames has failed sending to firmware
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param prCmdInfo      Pointer of CMD_INFO_T
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID wlanSecurityFrameTxTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
    ASSERT(prAdapter);
    ASSERT(prCmdInfo);

    kalSecurityFrameSendComplete(prAdapter->prGlueInfo,
				     prCmdInfo->prPacket, WLAN_STATUS_FAILURE);
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called before AIS is starting a new scan
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID wlanClearScanningResult(IN P_ADAPTER_T prAdapter)
{
    BOOLEAN fgKeepCurrOne = FALSE;
    UINT_32 i;

    ASSERT(prAdapter);

	/* clear scanning result */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) == PARAM_MEDIA_STATE_CONNECTED) {
		for (i = 0; i < prAdapter->rWlanInfo.u4ScanResultNum; i++) {
			if (EQUAL_MAC_ADDR(prAdapter->rWlanInfo.rCurrBssId.arMacAddress,
                        prAdapter->rWlanInfo.arScanResult[i].arMacAddress)) {
                fgKeepCurrOne = TRUE;

				if (i != 0) {
					/* copy structure */
                    kalMemCopy(&(prAdapter->rWlanInfo.arScanResult[0]),
                            &(prAdapter->rWlanInfo.arScanResult[i]),
                            OFFSET_OF(PARAM_BSSID_EX_T, aucIEs));
                }

				if (prAdapter->rWlanInfo.arScanResult[i].u4IELength > 0) {
					if (prAdapter->rWlanInfo.apucScanResultIEs[i] !=
					    &(prAdapter->rWlanInfo.aucScanIEBuf[0])) {
						/* move IEs to head */
                        kalMemCopy(prAdapter->rWlanInfo.aucScanIEBuf,
							   prAdapter->rWlanInfo.
							   apucScanResultIEs[i],
							   prAdapter->rWlanInfo.arScanResult[i].
							   u4IELength);
                    }
					/* modify IE pointer */
					prAdapter->rWlanInfo.apucScanResultIEs[0] =
					    &(prAdapter->rWlanInfo.aucScanIEBuf[0]);
				} else {
                    prAdapter->rWlanInfo.apucScanResultIEs[0] = NULL;
                }

                break;
            }
        }
    }

	if (fgKeepCurrOne == TRUE) {
        prAdapter->rWlanInfo.u4ScanResultNum = 1;
        prAdapter->rWlanInfo.u4ScanIEBufferUsage =
            ALIGN_4(prAdapter->rWlanInfo.arScanResult[0].u4IELength);
	} else {
        prAdapter->rWlanInfo.u4ScanResultNum = 0;
        prAdapter->rWlanInfo.u4ScanIEBufferUsage = 0;
    }

    return;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when AIS received a beacon timeout event
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param arBSSID        MAC address of the specified BSS
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID wlanClearBssInScanningResult(IN P_ADAPTER_T prAdapter, IN PUINT_8 arBSSID)
{
    UINT_32 i, j, u4IELength = 0, u4IEMoveLength;
    PUINT_8 pucIEPtr;

    ASSERT(prAdapter);

	/* clear scanning result */
    i = 0;
	while (1) {
		if (i >= prAdapter->rWlanInfo.u4ScanResultNum) {
            break;
        }

		if (EQUAL_MAC_ADDR(arBSSID, prAdapter->rWlanInfo.arScanResult[i].arMacAddress)) {
			/* backup current IE length */
            u4IELength = ALIGN_4(prAdapter->rWlanInfo.arScanResult[i].u4IELength);
            pucIEPtr = prAdapter->rWlanInfo.apucScanResultIEs[i];

			/* removed from middle */
			for (j = i + 1; j < prAdapter->rWlanInfo.u4ScanResultNum; j++) {
				kalMemCopy(&(prAdapter->rWlanInfo.arScanResult[j - 1]),
                        &(prAdapter->rWlanInfo.arScanResult[j]),
                        OFFSET_OF(PARAM_BSSID_EX_T, aucIEs));

				prAdapter->rWlanInfo.apucScanResultIEs[j - 1] =
                    prAdapter->rWlanInfo.apucScanResultIEs[j];
            }

            prAdapter->rWlanInfo.u4ScanResultNum--;

			/* remove IE buffer if needed := move rest of IE buffer */
			if (u4IELength > 0) {
                u4IEMoveLength = prAdapter->rWlanInfo.u4ScanIEBufferUsage -
				    (((ULONG) pucIEPtr) + u4IELength -
				     ((ULONG) (&(prAdapter->rWlanInfo.aucScanIEBuf[0]))));

                kalMemCopy(pucIEPtr,
					   (PUINT_8) (((ULONG) pucIEPtr) + u4IELength),
                        u4IEMoveLength);

                prAdapter->rWlanInfo.u4ScanIEBufferUsage -= u4IELength;

				/* correction of pointers to IE buffer */
				for (j = 0; j < prAdapter->rWlanInfo.u4ScanResultNum; j++) {
					if (prAdapter->rWlanInfo.apucScanResultIEs[j] > pucIEPtr) {
                        prAdapter->rWlanInfo.apucScanResultIEs[j] =
						    (PUINT_8) ((ULONG)
							       (prAdapter->rWlanInfo.
								apucScanResultIEs[j]) - u4IELength);
                    }
                }
            }
        }

        i++;
    }

    return;
}


#if CFG_TEST_WIFI_DIRECT_GO
VOID wlanEnableP2pFunction(IN P_ADAPTER_T prAdapter)
{
#if 0
	P_MSG_P2P_FUNCTION_SWITCH_T prMsgFuncSwitch = (P_MSG_P2P_FUNCTION_SWITCH_T) NULL;

	prMsgFuncSwitch =
	    (P_MSG_P2P_FUNCTION_SWITCH_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
						      sizeof(MSG_P2P_FUNCTION_SWITCH_T));
    if (!prMsgFuncSwitch) {
        ASSERT(FALSE);
        return;
    }


    prMsgFuncSwitch->rMsgHdr.eMsgId = MID_MNY_P2P_FUN_SWITCH;
    prMsgFuncSwitch->fgIsFuncOn = TRUE;


	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgFuncSwitch, MSG_SEND_METHOD_BUF);
#endif
    return;
}

VOID wlanEnableATGO(IN P_ADAPTER_T prAdapter)
{

	P_MSG_P2P_CONNECTION_REQUEST_T prMsgConnReq = (P_MSG_P2P_CONNECTION_REQUEST_T) NULL;
	UINT_8 aucTargetDeviceID[MAC_ADDR_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	prMsgConnReq =
	    (P_MSG_P2P_CONNECTION_REQUEST_T) cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
							 sizeof(MSG_P2P_CONNECTION_REQUEST_T));
    if (!prMsgConnReq) {
        ASSERT(FALSE);
        return;
    }

    prMsgConnReq->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_REQ;

    /*=====Param Modified for test=====*/
    COPY_MAC_ADDR(prMsgConnReq->aucDeviceID, aucTargetDeviceID);
    prMsgConnReq->fgIsTobeGO = TRUE;
    prMsgConnReq->fgIsPersistentGroup = FALSE;

    /*=====Param Modified for test=====*/

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgConnReq, MSG_SEND_METHOD_BUF);

    return;
}
#endif


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to retrieve NIC capability from firmware
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanQueryNicCapability(IN P_ADAPTER_T prAdapter)
{
    UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
    UINT_8 ucCmdSeqNum;
    P_CMD_INFO_T prCmdInfo;
    P_WIFI_CMD_T prWifiCmd;
    UINT_32 u4RxPktLength;
    UINT_8 aucBuffer[sizeof(WIFI_EVENT_T) + sizeof(EVENT_NIC_CAPABILITY_T)];
    P_HW_MAC_RX_DESC_T prRxStatus;
    P_WIFI_EVENT_T prEvent;
    P_EVENT_NIC_CAPABILITY_T prEventNicCapability;

    ASSERT(prAdapter);

    DEBUGFUNC("wlanQueryNicCapability");

	/* 1. Allocate CMD Info Packet and its Buffer */
    prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(EVENT_NIC_CAPABILITY_T));
    if (!prCmdInfo) {
        DBGLOG(INIT, ERROR, ("Allocate CMD_INFO_T ==> FAILED.\n"));
        return WLAN_STATUS_FAILURE;
    }
	/* increase command sequence number */
    ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
    prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
    prCmdInfo->u2InfoBufLen = CMD_HDR_SIZE + sizeof(EVENT_NIC_CAPABILITY_T);
    prCmdInfo->pfCmdDoneHandler = NULL;
    prCmdInfo->fgIsOid = FALSE;
    prCmdInfo->ucCID = CMD_ID_GET_NIC_CAPABILITY;
    prCmdInfo->fgSetQuery = FALSE;
    prCmdInfo->fgNeedResp = TRUE;
    prCmdInfo->fgDriverDomainMCR = FALSE;
    prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
    prCmdInfo->u4SetInfoLen = 0;

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
    prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
    prWifiCmd->u2PQ_ID = CMD_PQ_ID;
    prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
    prWifiCmd->ucCID = prCmdInfo->ucCID;
    prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
    prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;

    wlanSendCommand(prAdapter, prCmdInfo);
    cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	if (nicRxWaitResponse(prAdapter,
                1,
                aucBuffer,
                sizeof(WIFI_EVENT_T) + sizeof(EVENT_NIC_CAPABILITY_T),
                &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
        return WLAN_STATUS_FAILURE;
    }
	/* header checking .. */
	prRxStatus = (P_HW_MAC_RX_DESC_T) aucBuffer;
	if (prRxStatus->u2PktTYpe != RXM_RXD_PKT_TYPE_SW_EVENT) {
        return WLAN_STATUS_FAILURE;
    }

	prEvent = (P_WIFI_EVENT_T) aucBuffer;
	if (prEvent->ucEID != EVENT_ID_NIC_CAPABILITY) {
        return WLAN_STATUS_FAILURE;
    }

	prEventNicCapability = (P_EVENT_NIC_CAPABILITY_T) (prEvent->aucBuffer);

    prAdapter->rVerInfo.u2FwProductID     = prEventNicCapability->u2ProductID;
    prAdapter->rVerInfo.u2FwOwnVersion    = prEventNicCapability->u2FwVersion;
    prAdapter->rVerInfo.u2FwPeerVersion   = prEventNicCapability->u2DriverVersion;
	prAdapter->fgIsHw5GBandDisabled = (BOOLEAN) prEventNicCapability->ucHw5GBandDisabled;
	prAdapter->fgIsEepromUsed = (BOOLEAN) prEventNicCapability->ucEepromUsed;
    prAdapter->fgIsEmbbededMacAddrValid   = (BOOLEAN)
        (!IS_BMCAST_MAC_ADDR(prEventNicCapability->aucMacAddr) &&
         !EQUAL_MAC_ADDR(aucZeroMacAddr, prEventNicCapability->aucMacAddr));

    COPY_MAC_ADDR(prAdapter->rWifiVar.aucPermanentAddress, prEventNicCapability->aucMacAddr);
    COPY_MAC_ADDR(prAdapter->rWifiVar.aucMacAddress, prEventNicCapability->aucMacAddr);

    prAdapter->u4FwCompileFlag0 = prEventNicCapability->u4CompileFlag0;
    prAdapter->u4FwCompileFlag1 = prEventNicCapability->u4CompileFlag1;
    prAdapter->u4FwFeatureFlag0 = prEventNicCapability->u4FeatureFlag0;
    prAdapter->u4FwFeatureFlag1 = prEventNicCapability->u4FeatureFlag1;
 
#if CFG_ENABLE_CAL_LOG
    DBGLOG(INIT, INFO, (" RF CAL FAIL  = (%d),BB CAL FAIL  = (%d)\n",
			    prEventNicCapability->ucRfCalFail, prEventNicCapability->ucBbCalFail));
#endif

    DBGLOG(INIT, INFO, ("FW Ver DEC[%u.%u] HEX[%x.%x], Driver Ver[%u.%u]\n", 
        (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
        (prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
        (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
        (prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
        (prAdapter->rVerInfo.u2FwPeerVersion >> 8), 
        (prAdapter->rVerInfo.u2FwPeerVersion & BITS(0, 7))));

    return WLAN_STATUS_SUCCESS;
}

#ifdef MT6628
static INT_32 wlanChangeCodeWord(INT_32 au4Input)
{

    UINT_16     i;
#if TXPWR_USE_PDSLOPE
    CODE_MAPPING_T arCodeTable[] = {
        {0X100,    -40},
        {0X104,    -35},
        {0X128,    -30},
        {0X14C,    -25},
        {0X170,    -20},
        {0X194,    -15},
        {0X1B8,    -10},
		{0X1DC, -5},
		{0, 0},
		{0X24, 5},
		{0X48, 10},
		{0X6C, 15},
		{0X90, 20},
		{0XB4, 25},
		{0XD8, 30},
		{0XFC, 35},
		{0XFF, 40},

    };
#else
    CODE_MAPPING_T arCodeTable[] = {
        {0X100,    0x80},
        {0X104,    0x80},
        {0X128,    0x80},
        {0X14C,    0x80},
        {0X170,    0x80},
        {0X194,    0x94},
        {0X1B8,    0XB8},
        {0X1DC,    0xDC},
		{0, 0},
		{0X24, 0x24},
		{0X48, 0x48},
		{0X6C, 0x6c},
		{0X90, 0x7F},
		{0XB4, 0x7F},
		{0XD8, 0x7F},
		{0XFC, 0x7F},
		{0XFF, 0x7F},

    };
#endif

    for (i = 0; i < sizeof(arCodeTable) / sizeof(CODE_MAPPING_T); i++) {

		if (arCodeTable[i].u4RegisterValue == au4Input) {
			return arCodeTable[i].u4TxpowerOffset;
        }
    }


    return 0;
}
#endif
#if TXPWR_USE_PDSLOPE

/*----------------------------------------------------------------------------*/
/*!
* @brief
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanQueryPdMcr(IN P_ADAPTER_T prAdapter, P_PARAM_MCR_RW_STRUC_T prMcrRdInfo)
{
    UINT_8 ucCmdSeqNum;
    P_CMD_INFO_T prCmdInfo;
    P_WIFI_CMD_T prWifiCmd;
    UINT_32 u4RxPktLength;
    UINT_8 aucBuffer[sizeof(WIFI_EVENT_T) + sizeof(CMD_ACCESS_REG)];
    P_HW_MAC_RX_DESC_T prRxStatus;
    P_WIFI_EVENT_T prEvent;
    P_CMD_ACCESS_REG prCmdMcrQuery;
    ASSERT(prAdapter);


	/* 1. Allocate CMD Info Packet and its Buffer */
    prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, CMD_HDR_SIZE + sizeof(CMD_ACCESS_REG));

    if (!prCmdInfo) {
        DBGLOG(INIT, ERROR, ("Allocate CMD_INFO_T ==> FAILED.\n"));
        return WLAN_STATUS_FAILURE;
    }
	/* increase command sequence number */
    ucCmdSeqNum = nicIncreaseCmdSeqNum(prAdapter);

	/* compose CMD_BUILD_CONNECTION cmd pkt */
    prCmdInfo->eCmdType = COMMAND_TYPE_GENERAL_IOCTL;
	prCmdInfo->u2InfoBufLen = (UINT_16) (CMD_HDR_SIZE + sizeof(CMD_ACCESS_REG));
    prCmdInfo->pfCmdDoneHandler = NULL;
    prCmdInfo->pfCmdTimeoutHandler = nicOidCmdTimeoutCommon;
    prCmdInfo->fgIsOid = FALSE;
    prCmdInfo->ucCID = CMD_ID_ACCESS_REG;
    prCmdInfo->fgSetQuery = FALSE;
    prCmdInfo->fgNeedResp = TRUE;
    prCmdInfo->fgDriverDomainMCR = FALSE;
    prCmdInfo->ucCmdSeqNum = ucCmdSeqNum;
    prCmdInfo->u4SetInfoLen = sizeof(CMD_ACCESS_REG);

	/* Setup WIFI_CMD_T */
	prWifiCmd = (P_WIFI_CMD_T) (prCmdInfo->pucInfoBuffer);
    prWifiCmd->u2TxByteCount = prCmdInfo->u2InfoBufLen;
    prWifiCmd->u2PQ_ID = CMD_PQ_ID;
    prWifiCmd->ucPktTypeID = CMD_PACKET_TYPE_ID;
    prWifiCmd->ucCID = prCmdInfo->ucCID;
    prWifiCmd->ucSetQuery = prCmdInfo->fgSetQuery;
    prWifiCmd->ucSeqNum = prCmdInfo->ucCmdSeqNum;
    kalMemCopy(prWifiCmd->aucBuffer, prMcrRdInfo, sizeof(CMD_ACCESS_REG));

    wlanSendCommand(prAdapter, prCmdInfo);
    cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

	if (nicRxWaitResponse(prAdapter,
                1,
                aucBuffer,
                sizeof(WIFI_EVENT_T) + sizeof(CMD_ACCESS_REG),
                &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
        return WLAN_STATUS_FAILURE;
    }
	/* header checking .. */
	prRxStatus = (P_HW_MAC_RX_DESC_T) aucBuffer;
	if (prRxStatus->u2PktTYpe != RXM_RXD_PKT_TYPE_SW_EVENT) {
        return WLAN_STATUS_FAILURE;
    }


	prEvent = (P_WIFI_EVENT_T) aucBuffer;

	if (prEvent->ucEID != EVENT_ID_ACCESS_REG) {
        return WLAN_STATUS_FAILURE;
    }

	prCmdMcrQuery = (P_CMD_ACCESS_REG) (prEvent->aucBuffer);
    prMcrRdInfo->u4McrOffset = prCmdMcrQuery->u4Address;
    prMcrRdInfo->u4McrData = prCmdMcrQuery->u4Data;

    return WLAN_STATUS_SUCCESS;
}

static INT_32 wlanIntRound(INT_32 au4Input)
{


	if (au4Input >= 0) {
		if ((au4Input % 10) == 5) {
            au4Input = au4Input + 5;
            return au4Input;
        }
    }

	if (au4Input < 0) {
		if ((au4Input % 10) == -5) {
            au4Input = au4Input - 5;
            return au4Input;
        }
    }

    return au4Input;
}

static INT_32 wlanCal6628EfuseForm(IN P_ADAPTER_T prAdapter, INT_32 au4Input)
{

    PARAM_MCR_RW_STRUC_T rMcrRdInfo;
	INT_32 au4PdSlope, au4TxPwrOffset, au4TxPwrOffset_Round;
    INT_8          auTxPwrOffset_Round;

    rMcrRdInfo.u4McrOffset = 0x60205c68;
    rMcrRdInfo.u4McrData = 0;
    au4TxPwrOffset = au4Input;
	wlanQueryPdMcr(prAdapter, &rMcrRdInfo);

	au4PdSlope = (rMcrRdInfo.u4McrData) & BITS(0, 6);
	au4TxPwrOffset_Round = wlanIntRound((au4TxPwrOffset * au4PdSlope)) / 10;

    au4TxPwrOffset_Round = -au4TxPwrOffset_Round;

	if (au4TxPwrOffset_Round < -128) {
        au4TxPwrOffset_Round = 128;
	} else if (au4TxPwrOffset_Round < 0) {
        au4TxPwrOffset_Round += 256;
	} else if (au4TxPwrOffset_Round > 127) {
        au4TxPwrOffset_Round = 127;
    }

	auTxPwrOffset_Round = (UINT_8) au4TxPwrOffset_Round;

    return au4TxPwrOffset_Round;
}

#endif

#ifdef MT6628
static VOID wlanChangeNvram6620to6628(PUINT_8 pucEFUSE)
{


#define EFUSE_CH_OFFSET1_L_MASK_6620         BITS(0, 8)
#define EFUSE_CH_OFFSET1_L_SHIFT_6620        0
#define EFUSE_CH_OFFSET1_M_MASK_6620         BITS(9, 17)
#define EFUSE_CH_OFFSET1_M_SHIFT_6620        9
#define EFUSE_CH_OFFSET1_H_MASK_6620         BITS(18, 26)
#define EFUSE_CH_OFFSET1_H_SHIFT_6620        18
#define EFUSE_CH_OFFSET1_VLD_MASK_6620       BIT(27)
#define EFUSE_CH_OFFSET1_VLD_SHIFT_6620      27

#define EFUSE_CH_OFFSET1_L_MASK_5931         BITS(0, 7)
#define EFUSE_CH_OFFSET1_L_SHIFT_5931        0
#define EFUSE_CH_OFFSET1_M_MASK_5931         BITS(8, 15)
#define EFUSE_CH_OFFSET1_M_SHIFT_5931        8
#define EFUSE_CH_OFFSET1_H_MASK_5931         BITS(16, 23)
#define EFUSE_CH_OFFSET1_H_SHIFT_5931        16
#define EFUSE_CH_OFFSET1_VLD_MASK_5931       BIT(24)
#define EFUSE_CH_OFFSET1_VLD_SHIFT_5931      24
#define EFUSE_ALL_CH_OFFSET1_MASK_5931       BITS(25, 27)
#define EFUSE_ALL_CH_OFFSET1_SHIFT_5931      25




    INT_32         au4ChOffset;
	INT_16 au2ChOffsetL, au2ChOffsetM, au2ChOffsetH;


	au4ChOffset = *(UINT_32 *) (pucEFUSE + 72);

	if ((au4ChOffset & EFUSE_CH_OFFSET1_VLD_MASK_6620) && ((*(UINT_32 *) (pucEFUSE + 28)) == 0)) {


        au2ChOffsetL = ((au4ChOffset & EFUSE_CH_OFFSET1_L_MASK_6620) >>
            EFUSE_CH_OFFSET1_L_SHIFT_6620);

        au2ChOffsetM = ((au4ChOffset & EFUSE_CH_OFFSET1_M_MASK_6620) >>
            EFUSE_CH_OFFSET1_M_SHIFT_6620);

        au2ChOffsetH = ((au4ChOffset & EFUSE_CH_OFFSET1_H_MASK_6620) >>
            EFUSE_CH_OFFSET1_H_SHIFT_6620);

	    au2ChOffsetL = wlanChangeCodeWord(au2ChOffsetL);
	    au2ChOffsetM = wlanChangeCodeWord(au2ChOffsetM);
	    au2ChOffsetH = wlanChangeCodeWord(au2ChOffsetH);

	    au4ChOffset =  0;
		au4ChOffset |= *(UINT_32 *) (pucEFUSE + 72)
		    >> (EFUSE_CH_OFFSET1_VLD_SHIFT_6620 -
			EFUSE_CH_OFFSET1_VLD_SHIFT_5931) & EFUSE_CH_OFFSET1_VLD_MASK_5931;



		au4ChOffset |=
		    ((((UINT_32) au2ChOffsetL) << EFUSE_CH_OFFSET1_L_SHIFT_5931) &
		     EFUSE_CH_OFFSET1_L_MASK_5931);
		au4ChOffset |=
		    ((((UINT_32) au2ChOffsetM) << EFUSE_CH_OFFSET1_M_SHIFT_5931) &
		     EFUSE_CH_OFFSET1_M_MASK_5931);
		au4ChOffset |=
		    ((((UINT_32) au2ChOffsetH) << EFUSE_CH_OFFSET1_H_SHIFT_5931) &
		     EFUSE_CH_OFFSET1_H_MASK_5931);

		*((INT_32 *) ((pucEFUSE + 28))) = au4ChOffset;



    }

}
#endif

#if CFG_SUPPORT_NVRAM_5G
WLAN_STATUS wlanLoadManufactureData_5G(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo)
{

    
    P_BANDEDGE_5G_T pr5GBandEdge;

    ASSERT(prAdapter);

    pr5GBandEdge = &prRegInfo->prOldEfuseMapping->r5GBandEdgePwr;

     /* 1. set band edge tx power if available */
	if (pr5GBandEdge->uc5GBandEdgePwrUsed != 0) {
        CMD_EDGE_TXPWR_LIMIT_T rCmdEdgeTxPwrLimit;

		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrCCK = 0;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20 = pr5GBandEdge->c5GBandEdgeMaxPwrOFDM20;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40 = pr5GBandEdge->c5GBandEdgeMaxPwrOFDM40;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM80 = pr5GBandEdge->c5GBandEdgeMaxPwrOFDM80;

        wlanSendSetQueryCmd(prAdapter,
                CMD_ID_SET_EDGE_TXPWR_LIMIT_5G,
                TRUE,
                FALSE,
                FALSE,
                NULL,
                NULL,
                sizeof(CMD_EDGE_TXPWR_LIMIT_T),
				    (PUINT_8) &rCmdEdgeTxPwrLimit, NULL, 0);

		/* dumpMemory8(&rCmdEdgeTxPwrLimit,4); */
    }

    kalPrint("wlanLoadManufactureData_5G");
    

	/*2.set channel offset for 8 sub-band */
	if (prRegInfo->prOldEfuseMapping->uc5GChannelOffsetVaild) {
        CMD_POWER_OFFSET_T rCmdPowerOffset;
        UINT_8      i;

        rCmdPowerOffset.ucBand = BAND_5G;
		for (i = 0; i < MAX_SUBBAND_NUM_5G; i++) {
			rCmdPowerOffset.ucSubBandOffset[i] =
			    prRegInfo->prOldEfuseMapping->auc5GChOffset[i];
        }

        wlanSendSetQueryCmd(prAdapter,
                CMD_ID_SET_CHANNEL_PWR_OFFSET,
                TRUE,
                FALSE,
                FALSE,
                NULL,
                NULL,
				    sizeof(rCmdPowerOffset), (PUINT_8) &rCmdPowerOffset, NULL, 0);
		/* dumpMemory8(&rCmdPowerOffset,9); */
    }

	/*3.set 5G AC power */
    if (prRegInfo->prOldEfuseMapping->uc11AcTxPwrValid) {
        
        CMD_TX_AC_PWR_T     rCmdAcPwr;
		kalMemCopy(&rCmdAcPwr.rAcPwr, &prRegInfo->prOldEfuseMapping->r11AcTxPwr,
			   sizeof(AC_PWR_SETTING_STRUCT));
        rCmdAcPwr.ucBand = BAND_5G;

         wlanSendSetQueryCmd(prAdapter,
            CMD_ID_SET_80211AC_TX_PWR,
            TRUE,
            FALSE,
            FALSE,
            NULL,
				    NULL, sizeof(CMD_TX_AC_PWR_T), (PUINT_8) &rCmdAcPwr, NULL, 0);
		/* dumpMemory8(&rCmdAcPwr,9); */
    }
    

    return WLAN_STATUS_SUCCESS;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to load manufacture data from NVRAM
* if available and valid
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param prRegInfo      Pointer of REG_INFO_T
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanLoadManufactureData(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo)
{
#if CFG_SUPPORT_RDD_TEST_MODE
    CMD_RDD_CH_T rRddParam;
#endif
    CMD_NVRAM_SETTING_T rCmdNvramSettings;

    ASSERT(prAdapter);

    /* 1. Version Check */
    kalGetConfigurationVersion(prAdapter->prGlueInfo,
            &(prAdapter->rVerInfo.u2Part1CfgOwnVersion),
            &(prAdapter->rVerInfo.u2Part1CfgPeerVersion),
            &(prAdapter->rVerInfo.u2Part2CfgOwnVersion),
            &(prAdapter->rVerInfo.u2Part2CfgPeerVersion));

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
	if (CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2Part1CfgPeerVersion
            || CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2Part2CfgPeerVersion
            || prAdapter->rVerInfo.u2Part1CfgOwnVersion < CFG_DRV_PEER_VERSION
            || prAdapter->rVerInfo.u2Part2CfgOwnVersion < CFG_DRV_PEER_VERSION) {
        return WLAN_STATUS_FAILURE;
    }
#endif

	/* MT6620 E1/E2 would be ignored directly */
	if (prAdapter->rVerInfo.u2Part1CfgOwnVersion == 0x0001) {
        prRegInfo->ucTxPwrValid = 1;
	} else {
        /* 2. Load TX power gain parameters if valid */
		if (prRegInfo->ucTxPwrValid != 0) {
			/* send to F/W */
            
			nicUpdateTxPower(prAdapter, (P_CMD_TX_PWR_T) (&(prRegInfo->rTxPwr)));
        }
    }

    /* 3. Check if needs to support 5GHz */
	if (prRegInfo->ucEnable5GBand) {
#if CFG_SUPPORT_NVRAM_5G
		wlanLoadManufactureData_5G(prAdapter, prRegInfo);
#endif
		/* check if it is disabled by hardware */
		if (prAdapter->fgIsHw5GBandDisabled || prRegInfo->ucSupport5GBand == 0) {
            prAdapter->fgEnable5GBand = FALSE;
		} else {
            prAdapter->fgEnable5GBand = TRUE;
        }
	} else {
        prAdapter->fgEnable5GBand = FALSE;
    }

    /* 4. Send EFUSE data */
#if  defined(MT6628)
    wlanChangeNvram6620to6628(prRegInfo->aucEFUSE);
#endif

#if CFG_SUPPORT_NVRAM_5G
    /* If NvRAM read failed, this pointer will be NULL */
	if (prRegInfo->prOldEfuseMapping) {
		/*2.set channel offset for 3 sub-band */
		if (prRegInfo->prOldEfuseMapping->ucChannelOffsetVaild) {
            CMD_POWER_OFFSET_T rCmdPowerOffset;
            UINT_8      i;
    
            rCmdPowerOffset.ucBand = BAND_2G4;
			for (i = 0; i < 3; i++) {
				rCmdPowerOffset.ucSubBandOffset[i] =
				    prRegInfo->prOldEfuseMapping->aucChOffset[i];
            }
			rCmdPowerOffset.ucSubBandOffset[i] =
			    prRegInfo->prOldEfuseMapping->acAllChannelOffset;
    
            wlanSendSetQueryCmd(prAdapter,
                    CMD_ID_SET_CHANNEL_PWR_OFFSET,
                    TRUE,
                    FALSE,
                    FALSE,
                    NULL,
                    NULL,
                    sizeof(rCmdPowerOffset),
					    (PUINT_8) &rCmdPowerOffset, NULL, 0);
			/* dumpMemory8(&rCmdPowerOffset,9); */
        }
    }
#else

    wlanSendSetQueryCmd(prAdapter,
            CMD_ID_SET_PHY_PARAM,
            TRUE,
            FALSE,
            FALSE,
            NULL,
            NULL,
			    sizeof(CMD_PHY_PARAM_T), (PUINT_8) (prRegInfo->aucEFUSE), NULL, 0);


#endif
	/*RSSI path compasation */
    if (prRegInfo->ucRssiPathCompasationUsed) {
       CMD_RSSI_PATH_COMPASATION_T rCmdRssiPathCompasation; 

		rCmdRssiPathCompasation.c2GRssiCompensation =
		    prRegInfo->rRssiPathCompasation.c2GRssiCompensation;
		rCmdRssiPathCompasation.c5GRssiCompensation =
		    prRegInfo->rRssiPathCompasation.c5GRssiCompensation;

       wlanSendSetQueryCmd(prAdapter,
                CMD_ID_SET_PATH_COMPASATION,
                TRUE,
                FALSE,
                FALSE,
                NULL,
                NULL,
                sizeof(rCmdRssiPathCompasation),
				    (PUINT_8) &rCmdRssiPathCompasation, NULL, 0);
    }
#if CFG_SUPPORT_RDD_TEST_MODE
    rRddParam.ucRddTestMode = (UINT_8) prRegInfo->u4RddTestMode;
    rRddParam.ucRddShutCh = (UINT_8) prRegInfo->u4RddShutFreq;
    rRddParam.ucRddStartCh = (UINT_8) nicFreq2ChannelNum(prRegInfo->u4RddStartFreq);
    rRddParam.ucRddStopCh = (UINT_8) nicFreq2ChannelNum(prRegInfo->u4RddStopFreq);
    rRddParam.ucRddDfs = (UINT_8) prRegInfo->u4RddDfs;
    prAdapter->ucRddStatus = 0;
	nicUpdateRddTestMode(prAdapter, (P_CMD_RDD_CH_T) (&rRddParam));
#endif

    /* 5. Get 16-bits Country Code and Bandwidth */
    prAdapter->rWifiVar.rConnSettings.u2CountryCode =
        (((UINT_16) prRegInfo->au2CountryCode[0]) << 8) |
	    (((UINT_16) prRegInfo->au2CountryCode[1]) & BITS(0, 7));

#if 0 /* Bandwidth control will be controlled by GUI. 20110930
       * So ignore the setting from registry/NVRAM
       */
    prAdapter->rWifiVar.rConnSettings.uc2G4BandwidthMode =
            prRegInfo->uc2G4BwFixed20M ? CONFIG_BW_20M : CONFIG_BW_20_40M;
    prAdapter->rWifiVar.rConnSettings.uc5GBandwidthMode =
            prRegInfo->uc5GBwFixed20M ? CONFIG_BW_20M : CONFIG_BW_20_40M;
#endif

    /* 6. Set domain and channel information to chip */
    rlmDomainSendCmd(prAdapter, FALSE);

    /* 7. set band edge tx power if available */
	if (prRegInfo->fg2G4BandEdgePwrUsed) {
        CMD_EDGE_TXPWR_LIMIT_T rCmdEdgeTxPwrLimit;

		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrCCK = prRegInfo->cBandEdgeMaxPwrCCK;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM20 = prRegInfo->cBandEdgeMaxPwrOFDM20;
		rCmdEdgeTxPwrLimit.cBandEdgeMaxPwrOFDM40 = prRegInfo->cBandEdgeMaxPwrOFDM40;

        wlanSendSetQueryCmd(prAdapter,
                CMD_ID_SET_EDGE_TXPWR_LIMIT,
                TRUE,
                FALSE,
                FALSE,
                NULL,
                NULL,
                sizeof(CMD_EDGE_TXPWR_LIMIT_T),
				    (PUINT_8) &rCmdEdgeTxPwrLimit, NULL, 0);
    }
	/*8. Set 2.4G AC power */
    if (prRegInfo->prOldEfuseMapping->uc11AcTxPwrValid2G) {
        
        CMD_TX_AC_PWR_T     rCmdAcPwr;
		kalMemCopy(&rCmdAcPwr.rAcPwr, &prRegInfo->prOldEfuseMapping->r11AcTxPwr2G,
			   sizeof(AC_PWR_SETTING_STRUCT));
        rCmdAcPwr.ucBand = BAND_2G4;

         wlanSendSetQueryCmd(prAdapter,
            CMD_ID_SET_80211AC_TX_PWR,
            TRUE,
            FALSE,
            FALSE,
            NULL,
				    NULL, sizeof(CMD_TX_AC_PWR_T), (PUINT_8) &rCmdAcPwr, NULL, 0);
		/* dumpMemory8(&rCmdAcPwr,9); */
    }
    /* 9. Send the full Parameters of NVRAM to FW */

    
	kalMemCopy(&rCmdNvramSettings.rNvramSettings,
		   &prRegInfo->prNvramSettings->u2Part1OwnVersion, sizeof(WIFI_CFG_PARAM_STRUCT));
    ASSERT(sizeof(WIFI_CFG_PARAM_STRUCT) == 512);
            wlanSendSetQueryCmd(prAdapter,
                    CMD_ID_SET_NVRAM_SETTINGS,
                    TRUE,
                    FALSE,
                    FALSE,
                    NULL,
                    NULL,
			    sizeof(rCmdNvramSettings), (PUINT_8) &rCmdNvramSettings, NULL, 0);

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to check
*        Media Stream Mode is set to non-default value or not,
*        and clear to default value if above criteria is met
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return TRUE
*           The media stream mode was non-default value and has been reset
*         FALSE
*           The media stream mode is default value
*/
/*----------------------------------------------------------------------------*/
BOOLEAN wlanResetMediaStreamMode(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

	if (prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode != 0) {
        prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode = 0;

        return TRUE;
	} else {
        return FALSE;
    }
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to check if any pending timer has expired
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanTimerTimeoutCheck(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    cnmTimerDoTimeOutCheck(prAdapter);

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to check if any pending mailbox message
*        to be handled
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanProcessMboxMessage(IN P_ADAPTER_T prAdapter)
{
    UINT_32 i;

    ASSERT(prAdapter);

	for (i = 0; i < MBOX_ID_TOTAL_NUM; i++) {
		mboxRcvAllMsg(prAdapter, (ENUM_MBOX_ID_T) i);
    }

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to enqueue a single TX packet into CORE
*
* @param prAdapter      Pointer of Adapter Data Structure
*        prNativePacket Pointer of Native Packet
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_RESOURCES
*         WLAN_STATUS_INVALID_PACKET
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanEnqueueTxPacket(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prNativePacket)
{
	P_TX_CTRL_T prTxCtrl;
	P_MSDU_INFO_T prMsduInfo;

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;

	prMsduInfo = cnmPktAlloc(prAdapter, 0);

	if (!prMsduInfo) {
		return WLAN_STATUS_RESOURCES;
	}
	
#if CFG_DBG_MGT_BUF
	prMsduInfo->fgIsUsed = TRUE;
	prMsduInfo->rLastAllocTime = kalGetTimeTick();
#endif
	if (nicTxFillMsduInfo(prAdapter, prMsduInfo, prNativePacket)) {
		//prMsduInfo->eSrc = TX_PACKET_OS;
		
		/* Tx profiling */
		wlanTxProfilingTagMsdu(prAdapter, prMsduInfo, TX_PROF_TAG_DRV_ENQUE);
	
		/* enqueue to QM */
		nicTxEnqueueMsdu(prAdapter, prMsduInfo);

		return WLAN_STATUS_SUCCESS; 	   
	} else {
		kalSendComplete(prAdapter->prGlueInfo,
				prNativePacket, WLAN_STATUS_INVALID_PACKET);

		nicTxReturnMsduInfo(prAdapter, prMsduInfo);

		return WLAN_STATUS_INVALID_PACKET;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to flush pending TX packets in CORE
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanFlushTxPendingPackets(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    return nicTxFlush(prAdapter);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief this function sends pending MSDU_INFO_T to MT6620
*
* @param prAdapter      Pointer to the Adapter structure.
* @param pfgHwAccess    Pointer for tracking LP-OWN status
*
* @retval WLAN_STATUS_SUCCESS   Reset is done successfully.
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanTxPendingPackets(IN P_ADAPTER_T prAdapter, IN OUT PBOOLEAN pfgHwAccess)
{
    P_TX_CTRL_T prTxCtrl;
    P_MSDU_INFO_T prMsduInfo;

    KAL_SPIN_LOCK_DECLARATION();

    ASSERT(prAdapter);
    prTxCtrl = &prAdapter->rTxCtrl;

#if !CFG_SUPPORT_MULTITHREAD 
    ASSERT(pfgHwAccess);
#endif

	/* <1> dequeue packet by txDequeuTxPackets() */
#if CFG_SUPPORT_MULTITHREAD 
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
    prMsduInfo = qmDequeueTxPacketsMthread(prAdapter, &prTxCtrl->rTc);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
#else
    KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
    prMsduInfo = qmDequeueTxPackets(prAdapter, &prTxCtrl->rTc);
    KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
#endif
	if (prMsduInfo != NULL) {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == FALSE) {
#if !CFG_SUPPORT_MULTITHREAD            
            /* <2> Acquire LP-OWN if necessary */
			if (*pfgHwAccess == FALSE) {
                *pfgHwAccess = TRUE;

                wlanAcquirePowerControl(prAdapter);
            }
#endif
			/* <3> send packets */
#if CFG_SUPPORT_MULTITHREAD
            nicTxMsduInfoListMthread(prAdapter, prMsduInfo);
#else
            nicTxMsduInfoList(prAdapter, prMsduInfo);
#endif
			/* <4> update TC by txAdjustTcQuotas() */
            nicTxAdjustTcq(prAdapter);
		} else {
            wlanProcessQueuedMsduInfo(prAdapter, prMsduInfo);
        }
    }

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to acquire power control from firmware
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanAcquirePowerControl(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

	/* DBGLOG(INIT, INFO, ("Acquire Power Ctrl\n")); */

    ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);

    /* Reset sleepy state */
	if (prAdapter->fgWiFiInSleepyState == TRUE) {
        prAdapter->fgWiFiInSleepyState = FALSE;
    }

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to release power control to firmware
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanReleasePowerControl(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

	/* DBGLOG(INIT, INFO, ("Release Power Ctrl\n")); */

    RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

    return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to report currently pending TX frames count
*        (command packets are not included)
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return number of pending TX frames
*/
/*----------------------------------------------------------------------------*/
UINT_32 wlanGetTxPendingFrameCount(IN P_ADAPTER_T prAdapter)
{
    P_TX_CTRL_T prTxCtrl;
    UINT_32 u4Num;

    ASSERT(prAdapter);
    prTxCtrl = &prAdapter->rTxCtrl;

	u4Num =
	    kalGetTxPendingFrameCount(prAdapter->prGlueInfo) +
	    (UINT_32) (prTxCtrl->i4PendingFwdFrameCount);

    return u4Num;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to report current ACPI state
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return ACPI_STATE_D0 Normal Operation Mode
*         ACPI_STATE_D3 Suspend Mode
*/
/*----------------------------------------------------------------------------*/
ENUM_ACPI_STATE_T wlanGetAcpiState(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);

    return prAdapter->rAcpiState;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to update current ACPI state only
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param ePowerState    ACPI_STATE_D0 Normal Operation Mode
*                       ACPI_STATE_D3 Suspend Mode
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID wlanSetAcpiState(IN P_ADAPTER_T prAdapter, IN ENUM_ACPI_STATE_T ePowerState)
{
    ASSERT(prAdapter);
    ASSERT(ePowerState <= ACPI_STATE_D3);

    prAdapter->rAcpiState = ePowerState;

    return;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to query ECO version from HIFSYS CR
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return zero      Unable to retrieve ECO version information
*         non-zero  ECO version (1-based)
*/
/*----------------------------------------------------------------------------*/
UINT_8 wlanGetEcoVersion(IN P_ADAPTER_T prAdapter)
{
    UINT_8 ucEcoVersion;
    
    ASSERT(prAdapter);
    
#if CFG_MULTI_ECOVER_SUPPORT
    ucEcoVersion = mtk_wcn_wmt_hwver_get() + 1;
	/* DBGLOG(INIT, INFO, ("%s: %u\n", __FUNCTION__, ucEcoVersion)); */
    return ucEcoVersion;
#else
	if (nicVerifyChipID(prAdapter) == TRUE) {
		return prAdapter->ucRevID + 1;
	} else {
        return 0;
    }
#endif    
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to setting the default Tx Power configuration
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return zero      Unable to retrieve ECO version information
*         non-zero  ECO version (1-based)
*/
/*----------------------------------------------------------------------------*/
VOID wlanDefTxPowerCfg(IN P_ADAPTER_T prAdapter)
{
    UINT_8 i;
    P_GLUE_INFO_T       prGlueInfo = prAdapter->prGlueInfo;
    P_SET_TXPWR_CTRL_T  prTxpwr;

    ASSERT(prGlueInfo);

    prTxpwr = &prGlueInfo->rTxPwr;

    prTxpwr->c2GLegacyStaPwrOffset = 0;
    prTxpwr->c2GHotspotPwrOffset = 0;
    prTxpwr->c2GP2pPwrOffset = 0;
    prTxpwr->c2GBowPwrOffset = 0;
    prTxpwr->c5GLegacyStaPwrOffset = 0;
    prTxpwr->c5GHotspotPwrOffset = 0;
    prTxpwr->c5GP2pPwrOffset = 0;
    prTxpwr->c5GBowPwrOffset = 0;
    prTxpwr->ucConcurrencePolicy = 0;
	for (i = 0; i < 3; i++)
        prTxpwr->acReserved1[i] = 0;

	for (i = 0; i < 14; i++)
        prTxpwr->acTxPwrLimit2G[i] = 63;

	for (i = 0; i < 4; i++)
        prTxpwr->acTxPwrLimit5G[i] = 63;

	for (i = 0; i < 2; i++)
        prTxpwr->acReserved2[i] = 0;

}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to
*        set preferred band configuration corresponding to network type
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param eBand          Given band
* @param ucBssIndex     BSS Info Index
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID
wlanSetPreferBandByNetwork(IN P_ADAPTER_T prAdapter, IN ENUM_BAND_T eBand, IN UINT_8 ucBssIndex)
{
    ASSERT(prAdapter);
    ASSERT(eBand <= BAND_NUM);
    ASSERT(ucBssIndex <= MAX_BSS_INDEX);
    ASSERT(ucBssIndex <= BSS_INFO_NUM);

    /* 1. set prefer band according to network type */
    prAdapter->aePreferBand[ucBssIndex] = eBand;

    /* 2. remove buffered BSS descriptors correspondingly */
	if (eBand == BAND_2G4) {
        scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_5G, ucBssIndex);
	} else if (eBand == BAND_5G) {
        scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_2G4, ucBssIndex);
    }

    return;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to
*        get channel information corresponding to specified network type
*
* @param prAdapter      Pointer of Adapter Data Structure
* @param ucBssIndex     BSS Info Index
*
* @return channel number
*/
/*----------------------------------------------------------------------------*/
UINT_8 wlanGetChannelNumberByNetwork(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex)
{
    P_BSS_INFO_T prBssInfo;

    ASSERT(prAdapter);
    ASSERT(ucBssIndex <= MAX_BSS_INDEX);

    prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

    return prBssInfo->ucPrimaryChannel;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to
*        check unconfigured system properties and generate related message on
*        scan list to notify users
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanCheckSystemConfiguration(IN P_ADAPTER_T prAdapter)
{
#if (CFG_NVRAM_EXISTENCE_CHECK == 1) || (CFG_SW_NVRAM_VERSION_CHECK == 1)
    const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR;
    const UINT_8 aucBCAddr[] = BC_MAC_ADDR;
    BOOLEAN fgIsConfExist = TRUE;
    BOOLEAN fgGenErrMsg = FALSE;
    P_REG_INFO_T prRegInfo = NULL;
    P_WLAN_BEACON_FRAME_T prBeacon = NULL;
    P_IE_SSID_T prSsid = NULL;
    UINT_32 u4ErrCode = 0;
    UINT_8 aucErrMsg[32];
    PARAM_SSID_T rSsid;
    PARAM_802_11_CONFIG_T rConfiguration;
    PARAM_RATES_EX rSupportedRates;
#endif

    DEBUGFUNC("wlanCheckSystemConfiguration");

    ASSERT(prAdapter);

#if (CFG_NVRAM_EXISTENCE_CHECK == 1)
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == FALSE) {
        fgIsConfExist = FALSE;
        fgGenErrMsg = TRUE;
    }
#endif

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
    prRegInfo = kalGetConfiguration(prAdapter->prGlueInfo);

#if (CFG_SUPPORT_PWR_LIMIT_COUNTRY == 1)
	if (fgIsConfExist == TRUE && (CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2Part1CfgPeerVersion || CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2Part2CfgPeerVersion || prAdapter->rVerInfo.u2Part1CfgOwnVersion < CFG_DRV_PEER_VERSION || prAdapter->rVerInfo.u2Part2CfgOwnVersion < CFG_DRV_PEER_VERSION	/* NVRAM */
            || CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2FwPeerVersion
            || prAdapter->rVerInfo.u2FwOwnVersion < CFG_DRV_PEER_VERSION
            || (prAdapter->fgIsEmbbededMacAddrValid == FALSE &&
                (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
					   || EQUAL_MAC_ADDR(aucZeroMacAddr,
							     prRegInfo->aucMacAddr)))
            || prRegInfo->ucTxPwrValid == 0
            || prAdapter->fgIsPowerLimitTableValid == FALSE )) {
        fgGenErrMsg = TRUE;
    }
#else
	if (fgIsConfExist == TRUE && (CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2Part1CfgPeerVersion || CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2Part2CfgPeerVersion || prAdapter->rVerInfo.u2Part1CfgOwnVersion < CFG_DRV_PEER_VERSION || prAdapter->rVerInfo.u2Part2CfgOwnVersion < CFG_DRV_PEER_VERSION	/* NVRAM */
            || CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2FwPeerVersion
            || prAdapter->rVerInfo.u2FwOwnVersion < CFG_DRV_PEER_VERSION
            || (prAdapter->fgIsEmbbededMacAddrValid == FALSE &&
                (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
					   || EQUAL_MAC_ADDR(aucZeroMacAddr,
							     prRegInfo->aucMacAddr)))
            || prRegInfo->ucTxPwrValid == 0)) {
        fgGenErrMsg = TRUE;
    }
#endif

#endif

	if (fgGenErrMsg == TRUE) {
		prBeacon =
		    cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				sizeof(WLAN_BEACON_FRAME_T) + sizeof(IE_SSID_T));

		/* initialization */
        kalMemZero(prBeacon, sizeof(WLAN_BEACON_FRAME_T) + sizeof(IE_SSID_T));

		/* prBeacon initialization */
        prBeacon->u2FrameCtrl = MAC_FRAME_BEACON;
        COPY_MAC_ADDR(prBeacon->aucDestAddr, aucBCAddr);
        COPY_MAC_ADDR(prBeacon->aucSrcAddr, aucZeroMacAddr);
        COPY_MAC_ADDR(prBeacon->aucBSSID, aucZeroMacAddr);
        prBeacon->u2BeaconInterval = 100;
        prBeacon->u2CapInfo = CAP_INFO_ESS;

		/* prSSID initialization */
		prSsid = (P_IE_SSID_T) (&prBeacon->aucInfoElem[0]);
        prSsid->ucId = ELEM_ID_SSID;

		/* rConfiguration initialization */
        rConfiguration.u4Length             = sizeof(PARAM_802_11_CONFIG_T);
        rConfiguration.u4BeaconPeriod       = 100;
        rConfiguration.u4ATIMWindow         = 1;
        rConfiguration.u4DSConfig           = 2412;
        rConfiguration.rFHConfig.u4Length   = sizeof(PARAM_802_11_CONFIG_FH_T);

		/* rSupportedRates initialization */
        kalMemZero(rSupportedRates, sizeof(PARAM_RATES_EX));
    }
#if (CFG_NVRAM_EXISTENCE_CHECK == 1)
#define NVRAM_ERR_MSG "NVRAM WARNING: Err = 0x01"
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == FALSE) {
        COPY_SSID(prSsid->aucSSID,
			  prSsid->ucLength, NVRAM_ERR_MSG, (UINT_8) (strlen(NVRAM_ERR_MSG)));

        kalIndicateBssInfo(prAdapter->prGlueInfo,
				   (PUINT_8) prBeacon,
				   OFFSET_OF(WLAN_BEACON_FRAME_T,
					     aucInfoElem) + OFFSET_OF(IE_SSID_T,
								      aucSSID) + prSsid->ucLength,
				   1, 0);

        COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, NVRAM_ERR_MSG, strlen(NVRAM_ERR_MSG));
        nicAddScanResult(prAdapter,
                prBeacon->aucBSSID,
                &rSsid,
                0,
                0,
                PARAM_NETWORK_TYPE_FH,
                &rConfiguration,
                NET_TYPE_INFRA,
                rSupportedRates,
				 OFFSET_OF(WLAN_BEACON_FRAME_T, aucInfoElem) + OFFSET_OF(IE_SSID_T,
											 aucSSID) +
				 prSsid->ucLength - WLAN_MAC_MGMT_HEADER_LEN,
				 (PUINT_8) ((ULONG) (prBeacon) + WLAN_MAC_MGMT_HEADER_LEN));
    }
#endif

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
#define VER_ERR_MSG     "NVRAM WARNING: Err = 0x%02X"
	if (fgIsConfExist == TRUE) {
		if ((CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2Part1CfgPeerVersion || CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2Part2CfgPeerVersion || prAdapter->rVerInfo.u2Part1CfgOwnVersion < CFG_DRV_PEER_VERSION || prAdapter->rVerInfo.u2Part2CfgOwnVersion < CFG_DRV_PEER_VERSION	/* NVRAM */
                    || CFG_DRV_OWN_VERSION < prAdapter->rVerInfo.u2FwPeerVersion
                    || prAdapter->rVerInfo.u2FwOwnVersion < CFG_DRV_PEER_VERSION)) {
            u4ErrCode |= NVRAM_ERROR_VERSION_MISMATCH;
        }


		if (prRegInfo->ucTxPwrValid == 0) {
            u4ErrCode |= NVRAM_ERROR_INVALID_TXPWR;
        }

		if (prAdapter->fgIsEmbbededMacAddrValid == FALSE &&
		    (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
		     || EQUAL_MAC_ADDR(aucZeroMacAddr, prRegInfo->aucMacAddr))) {
            u4ErrCode |= NVRAM_ERROR_INVALID_MAC_ADDR;
        }
#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
        if(prAdapter->fgIsPowerLimitTableValid == FALSE) {
			u4ErrCode |= NVRAM_POWER_LIMIT_TABLE_INVALID;
        }
#endif
        if(u4ErrCode != 0) {
            sprintf(aucErrMsg, VER_ERR_MSG, (unsigned int)u4ErrCode);
            COPY_SSID(prSsid->aucSSID,
				  prSsid->ucLength, aucErrMsg, (UINT_8) (strlen(aucErrMsg)));

            kalIndicateBssInfo(prAdapter->prGlueInfo,
					   (PUINT_8) prBeacon,
					   OFFSET_OF(WLAN_BEACON_FRAME_T,
						     aucInfoElem) + OFFSET_OF(IE_SSID_T,
									      aucSSID) +
					   prSsid->ucLength, 1, 0);

			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, NVRAM_ERR_MSG,
				  strlen(NVRAM_ERR_MSG));
			nicAddScanResult(prAdapter, prBeacon->aucBSSID, &rSsid, 0, 0,
					 PARAM_NETWORK_TYPE_FH, &rConfiguration, NET_TYPE_INFRA,
					 rSupportedRates, OFFSET_OF(WLAN_BEACON_FRAME_T,
								    aucInfoElem) +
					 OFFSET_OF(IE_SSID_T,
						   aucSSID) + prSsid->ucLength -
					 WLAN_MAC_MGMT_HEADER_LEN,
					 (PUINT_8) ((ULONG) (prBeacon) +
						    WLAN_MAC_MGMT_HEADER_LEN));
        }
    }
#endif

	if (fgGenErrMsg == TRUE) {
        cnmMemFree(prAdapter, prBeacon);
    }

    return WLAN_STATUS_SUCCESS;
}


WLAN_STATUS 
wlanoidQueryStaStatistics(IN P_ADAPTER_T prAdapter,
    IN PVOID        pvQueryBuffer,
			  IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
	P_STA_RECORD_T prStaRec, prTempStaRec;
	P_PARAM_GET_STA_STATISTICS prQueryStaStatistics;
    UINT_8 ucStaRecIdx;
    P_QUE_MGT_T prQM = &prAdapter->rQM;
    CMD_GET_STA_STATISTICS_T rQueryCmdStaStatistics;
    UINT_8 ucIdx;
	
	DEBUGFUNC("wlanoidQueryStaStatistics");
	do {
        ASSERT(pvQueryBuffer);

		/* 4 1. Sanity test */
        if ((prAdapter == NULL) || (pu4QueryInfoLen == NULL)) {
            break;
        }

		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL)) {
			break;
		}

		if (u4QueryBufferLen < sizeof(PARAM_GET_STA_STA_STATISTICS)) {
			*pu4QueryInfoLen = sizeof(PARAM_GET_STA_STA_STATISTICS);
			rResult = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}
			 		
		prQueryStaStatistics = (P_PARAM_GET_STA_STATISTICS) pvQueryBuffer;
        *pu4QueryInfoLen = sizeof(PARAM_GET_STA_STA_STATISTICS);

		/* 4 5. Get driver global QM counter */
		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
			prQueryStaStatistics->au4TcAverageQueLen[ucIdx] =
			    prQM->au4AverageQueLen[ucIdx];
			prQueryStaStatistics->au4TcCurrentQueLen[ucIdx] =
			    prQM->au4CurrentTcResource[ucIdx];
        }

		/* 4 2. Get StaRec by MAC address */
        prStaRec = NULL;

		
		for (ucStaRecIdx = 0; ucStaRecIdx < CFG_NUM_OF_STA_RECORD; ucStaRecIdx++) {
            prTempStaRec = &(prAdapter->arStaRec[ucStaRecIdx]);
			if (prTempStaRec->fgIsValid && prTempStaRec->fgIsInUse) {
				if (EQUAL_MAC_ADDR
				    (prTempStaRec->aucMacAddr, prQueryStaStatistics->aucMacAddr)) {
                    prStaRec = prTempStaRec;
                    break;
                }
            }
        }

		if (!prStaRec) {
            rResult = WLAN_STATUS_INVALID_DATA;
            break;
        }

        prQueryStaStatistics->u4Flag |= BIT(0);

#if CFG_ENABLE_PER_STA_STATISTICS
		/* 4 3. Get driver statistics */
        prQueryStaStatistics->u4TxTotalCount = prStaRec->u4TotalTxPktsNumber;
        prQueryStaStatistics->u4RxTotalCount = prStaRec->u4TotalRxPktsNumber;
        prQueryStaStatistics->u4TxExceedThresholdCount = prStaRec->u4ThresholdCounter;
        prQueryStaStatistics->u4TxMaxTime = prStaRec->u4MaxTxPktsTime;
		if (prStaRec->u4TotalTxPktsNumber) {
			prQueryStaStatistics->u4TxAverageProcessTime =
			    (prStaRec->u4TotalTxPktsTime / prStaRec->u4TotalTxPktsNumber);
		} else {
            prQueryStaStatistics->u4TxAverageProcessTime = 0;
        }

		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
			prQueryStaStatistics->au4TcResourceEmptyCount[ucIdx] =
                    prQM->au4QmTcResourceEmptyCounter[prStaRec->ucBssIndex][ucIdx];
            /* Reset */
            prQM->au4QmTcResourceEmptyCounter[prStaRec->ucBssIndex][ucIdx] = 0;
        }

		/* 4 4.1 Reset statistics */
		if (prQueryStaStatistics->ucReadClear) {
            prStaRec->u4ThresholdCounter = 0;
            prStaRec->u4TotalTxPktsNumber = 0;
            prStaRec->u4TotalTxPktsTime = 0;
            prStaRec->u4TotalRxPktsNumber = 0;
            prStaRec->u4MaxTxPktsTime = 0;
        }
#endif

		for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
			prQueryStaStatistics->au4TcQueLen[ucIdx] =
			    prStaRec->arTxQueue[ucIdx].u4NumElem;
        }
        
        rResult = WLAN_STATUS_SUCCESS;

		/* 4 6. Ensure FW supports get station link status */
		if (prAdapter->u4FwCompileFlag0 & COMPILE_FLAG0_GET_STA_LINK_STATUS) {

            rQueryCmdStaStatistics.ucIndex = prStaRec->ucIndex;
			COPY_MAC_ADDR(rQueryCmdStaStatistics.aucMacAddr,
				      prQueryStaStatistics->aucMacAddr);
            rQueryCmdStaStatistics.ucReadClear = prQueryStaStatistics->ucReadClear;
            
            rResult = wlanSendSetQueryCmd(prAdapter,
                            CMD_ID_GET_STA_STATISTICS,
                            FALSE,
                            TRUE,
                            TRUE,
                            nicCmdEventQueryStaStatistics,
                            nicOidCmdTimeoutCommon,
                            sizeof(CMD_GET_STA_STATISTICS_T),
						      (PUINT_8) &rQueryCmdStaStatistics,
						      pvQueryBuffer, u4QueryBufferLen);
            
            prQueryStaStatistics->u4Flag |= BIT(1);
		} else {
            rResult = WLAN_STATUS_NOT_SUPPORTED;
        }
        
	} while (FALSE);
    
	return rResult;
} /* wlanoidQueryP2pVersion */



/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to query Nic resource information
*
* @param prAdapter      Pointer of Adapter Data Structure
*
* @return WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
VOID wlanQueryNicResourceInformation(IN P_ADAPTER_T prAdapter)
{
	/* 3 1. Get Nic resource information from FW */

	/* 3 2. Setup resource parameter */

	/* 3 3. Reset Tx resource */
    nicTxResetResource(prAdapter);
}

#if 0
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to SET network interface index for a network interface.
*           A network interface is a TX/RX data port hooked to OS.
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
* @param ucNetInterfaceIndex            Index of network interface
* @param ucBssIndex                     Index of BSS
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
VOID
wlanBindNetInterface(IN P_GLUE_INFO_T prGlueInfo,
		     IN UINT_8 ucNetInterfaceIndex, IN PVOID pvNetInterface)
{
    P_NET_INTERFACE_INFO_T prNetIfInfo;

    prNetIfInfo = &prGlueInfo->arNetInterfaceInfo[ucNetInterfaceIndex];

    prNetIfInfo->pvNetInterface = pvNetInterface;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to SET BSS index for a network interface.
*           A network interface is a TX/RX data port hooked to OS.
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
* @param ucNetInterfaceIndex            Index of network interface
* @param ucBssIndex                     Index of BSS
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
VOID
wlanBindBssIdxToNetInterface(IN P_GLUE_INFO_T prGlueInfo,
			     IN UINT_8 ucBssIndex, IN PVOID pvNetInterface)
{
    P_NET_INTERFACE_INFO_T prNetIfInfo;

    if (ucBssIndex > MAX_BSS_INDEX) {
        return;
    }

    prNetIfInfo = &prGlueInfo->arNetInterfaceInfo[ucBssIndex];

    prNetIfInfo->ucBssIndex = ucBssIndex;
    prNetIfInfo->pvNetInterface = pvNetInterface;
	/* prGlueInfo->aprBssIdxToNetInterfaceInfo[ucBssIndex] = prNetIfInfo; */
}

#if 0
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to GET BSS index for a network interface.
*           A network interface is a TX/RX data port hooked to OS.
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
* @param ucNetInterfaceIndex       Index of network interface
*
* @return UINT_8                         Index of BSS
*/
/*----------------------------------------------------------------------------*/
UINT_8 wlanGetBssIdxByNetInterface(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvNetInterface)
{
    UINT_8 ucIdx = 0;

    for (ucIdx = 0; ucIdx < HW_BSSID_NUM; ucIdx++) {
        if (prGlueInfo->arNetInterfaceInfo[ucIdx].pvNetInterface == pvNetInterface)
            break;
    }
    
    return ucIdx;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to GET network interface for a BSS.
*           A network interface is a TX/RX data port hooked to OS.
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
* @param ucBssIndex                     Index of BSS
*
* @return PVOID                         pointer of network interface structure
*/
/*----------------------------------------------------------------------------*/
PVOID wlanGetNetInterfaceByBssIdx(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex)
{
	if (ucBssIndex < HW_BSSID_NUM)
	    return prGlueInfo->arNetInterfaceInfo[ucBssIndex].pvNetInterface;
	aee_kernel_dal_show("meet WTBL issue!\n");
	return NULL;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to get BSS-INDEX for AIS network.
*
* @param prAdapter  Pointer of ADAPTER_T
*
* @return value, as corresponding index of BSS
*/
/*----------------------------------------------------------------------------*/
UINT_8 wlanGetAisBssIndex(IN P_ADAPTER_T prAdapter)
{
    ASSERT(prAdapter);
    ASSERT(prAdapter->prAisBssInfo);

    return prAdapter->prAisBssInfo->ucBssIndex;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to initialize WLAN feature options
*
* @param prAdapter  Pointer of ADAPTER_T
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID wlanInitFeatureOption(IN P_ADAPTER_T prAdapter)
{
    P_WIFI_VAR_T prWifiVar = &prAdapter->rWifiVar;
    P_QUE_MGT_T prQM = &prAdapter->rQM;

    /* Feature options will be filled by config file */

	prWifiVar->ucQoS = (UINT_8) wlanCfgGetUint32(prAdapter, "Qos", FEATURE_ENABLED);
    
	prWifiVar->ucStaHt = (UINT_8) wlanCfgGetUint32(prAdapter, "StaHT", FEATURE_ENABLED);
	prWifiVar->ucStaVht = (UINT_8) wlanCfgGetUint32(prAdapter, "StaVHT", FEATURE_ENABLED);

	prWifiVar->ucApHt = (UINT_8) wlanCfgGetUint32(prAdapter, "ApHT", FEATURE_ENABLED);
	prWifiVar->ucApVht = (UINT_8) wlanCfgGetUint32(prAdapter, "ApVHT", FEATURE_ENABLED);
    
	prWifiVar->ucP2pGoHt = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pGoHT", FEATURE_ENABLED);
	prWifiVar->ucP2pGoVht = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pGoVHT", FEATURE_ENABLED);
    
	prWifiVar->ucP2pGcHt = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pGcHT", FEATURE_ENABLED);
	prWifiVar->ucP2pGcVht = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pGcVHT", FEATURE_ENABLED);

	prWifiVar->ucAmpduRx = (UINT_8) wlanCfgGetUint32(prAdapter, "AmpduRx", FEATURE_ENABLED);
	prWifiVar->ucAmpduTx = (UINT_8) wlanCfgGetUint32(prAdapter, "AmpduTx", FEATURE_ENABLED);

	prWifiVar->ucTspec = (UINT_8) wlanCfgGetUint32(prAdapter, "Tspec", FEATURE_DISABLED);
    
	prWifiVar->ucUapsd = (UINT_8) wlanCfgGetUint32(prAdapter, "Uapsd", FEATURE_ENABLED);
	prWifiVar->ucStaUapsd = (UINT_8) wlanCfgGetUint32(prAdapter, "StaUapsd", FEATURE_DISABLED);
	prWifiVar->ucApUapsd = (UINT_8) wlanCfgGetUint32(prAdapter, "ApUapsd", FEATURE_DISABLED);
	prWifiVar->ucP2pUapsd = (UINT_8) wlanCfgGetUint32(prAdapter, "P2pUapsd", FEATURE_ENABLED);

	prWifiVar->ucTxShortGI = (UINT_8) wlanCfgGetUint32(prAdapter, "SgiTx", FEATURE_ENABLED);
	prWifiVar->ucRxShortGI = (UINT_8) wlanCfgGetUint32(prAdapter, "SgiRx", FEATURE_ENABLED);

	prWifiVar->ucTxLdpc = (UINT_8) wlanCfgGetUint32(prAdapter, "LdpcTx", FEATURE_ENABLED);
	prWifiVar->ucRxLdpc = (UINT_8) wlanCfgGetUint32(prAdapter, "LdpcRx", FEATURE_ENABLED);

	prWifiVar->ucTxStbc = (UINT_8) wlanCfgGetUint32(prAdapter, "StbcTx", FEATURE_DISABLED);
	prWifiVar->ucRxStbc = (UINT_8) wlanCfgGetUint32(prAdapter, "StbcRx", FEATURE_ENABLED);

	prWifiVar->ucTxGf = (UINT_8) wlanCfgGetUint32(prAdapter, "GfTx", FEATURE_ENABLED);
	prWifiVar->ucRxGf = (UINT_8) wlanCfgGetUint32(prAdapter, "GfRx", FEATURE_ENABLED);

	prWifiVar->ucSigTaRts = (UINT_8) wlanCfgGetUint32(prAdapter, "SigTaRts", FEATURE_DISABLED);
	prWifiVar->ucDynBwRts = (UINT_8) wlanCfgGetUint32(prAdapter, "DynBwRts", FEATURE_DISABLED);
	prWifiVar->ucTxopPsTx = (UINT_8) wlanCfgGetUint32(prAdapter, "TxopPsTx", FEATURE_DISABLED);

    prWifiVar->ucStaHtBfee = (UINT_8)wlanCfgGetUint32(prAdapter, "StaHTBfee", FEATURE_DISABLED);
    prWifiVar->ucStaVhtBfee = (UINT_8)wlanCfgGetUint32(prAdapter, "StaVHTBfee", FEATURE_ENABLED);
	prWifiVar->ucStaVhtMuBfee = (UINT_8)wlanCfgGetUint32(prAdapter, "StaVHTMuBfee", FEATURE_DISABLED);
    prWifiVar->ucStaBfer = (UINT_8)wlanCfgGetUint32(prAdapter, "StaBfer", FEATURE_DISABLED);

	prWifiVar->ucApWpsMode = (UINT_8) wlanCfgGetUint32(prAdapter, "ApWpsMode", 0);
    DBGLOG(INIT, INFO, ("ucApWpsMode = %u\n", prWifiVar->ucApWpsMode));

	prWifiVar->ucThreadScheduling = (UINT_8) wlanCfgGetUint32(prAdapter, "ThreadSched", 0);
	prWifiVar->ucThreadPriority =
	    (UINT_8) wlanCfgGetUint32(prAdapter, "ThreadPriority", WLAN_TX_THREAD_TASK_PRIORITY);
	prWifiVar->cThreadNice =
	    (INT_8) wlanCfgGetInt32(prAdapter, "ThreadNice", WLAN_TX_THREAD_TASK_NICE);

    prAdapter->rQM.u4MaxForwardBufferCount = 
	    (UINT_32) wlanCfgGetUint32(prAdapter, "ApForwardBufferCnt", QM_FWD_PKT_QUE_THRESHOLD);

    /* AP channel setting
     * 0: auto
     */
	prWifiVar->ucApChannel = (UINT_8) wlanCfgGetUint32(prAdapter, "ApChannel", 0);

    /*
     * 0: SCN
     * 1: SCA
     * 2: RES
     * 3: SCB
     */
    prWifiVar->ucApSco = (UINT_8)wlanCfgGetUint32(prAdapter, "ApSco", 0);
    prWifiVar->ucP2pGoSco = (UINT_8)wlanCfgGetUint32(prAdapter, "P2pGoSco", 0);

    /* Max bandwidth setting
     * 0: 20Mhz
     * 1: 40Mhz
     * 2: 80Mhz
     * 3: 80+80 or 160Mhz
     * Note: For VHT STA, BW 80Mhz is a must!
     */
	prWifiVar->ucStaBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "StaBw", MAX_BW_80MHZ);
	prWifiVar->ucSta2gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "Sta2gBw", MAX_BW_40MHZ);
	prWifiVar->ucSta5gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "Sta5gBw", MAX_BW_80MHZ);
	prWifiVar->ucP2p2gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "P2p2gBw", MAX_BW_20MHZ);
	prWifiVar->ucP2p5gBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "P2p5gBw", MAX_BW_40MHZ);
	prWifiVar->ucApBandwidth = (UINT_8) wlanCfgGetUint32(prAdapter, "ApBw", MAX_BW_20MHZ);
    
	prWifiVar->ucStaDisconnectDetectTh =
	    (UINT_8) wlanCfgGetUint32(prAdapter, "StaDisconnectDetectTh", 0);
	prWifiVar->ucApDisconnectDetectTh =
	    (UINT_8) wlanCfgGetUint32(prAdapter, "ApDisconnectDetectTh", 0);
	prWifiVar->ucP2pDisconnectDetectTh =
	    (UINT_8) wlanCfgGetUint32(prAdapter, "P2pDisconnectDetectTh", 0);
    
	prWifiVar->ucTcRestrict = (UINT_8) wlanCfgGetUint32(prAdapter, "TcRestrict", 0xFF);
    /* Max Tx dequeue limit: 0 => auto */
	prWifiVar->u4MaxTxDeQLimit =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "MaxTxDeQLimit", 0x0);
	prWifiVar->ucAlwaysResetUsedRes =
	    (UINT_32) wlanCfgGetUint32(prAdapter, "AlwaysResetUsedRes", 0x0);

#if CFG_SUPPORT_MTK_SYNERGY
	prWifiVar->ucMtkOui = (UINT_8) wlanCfgGetUint32(prAdapter, "MtkOui", FEATURE_ENABLED);
	prWifiVar->u4MtkOuiCap = (UINT_32) wlanCfgGetUint32(prAdapter, "MtkOuiCap", 0);
    prWifiVar->aucMtkFeature[0] = 0xff;
    prWifiVar->aucMtkFeature[1] = 0xff;
    prWifiVar->aucMtkFeature[2] = 0xff;
    prWifiVar->aucMtkFeature[3] = 0xff;
#endif 

    prWifiVar->ucCmdRsvResource = (UINT_8)wlanCfgGetUint32(prAdapter, "TxCmdRsv", QM_CMD_RESERVED_THRESHOLD);
    prWifiVar->u4MgmtQueueDelayTimeout = (UINT_32)wlanCfgGetUint32(prAdapter, "TxMgmtQueTO", QM_MGMT_QUEUED_TIMEOUT); /* ms */

    /* Performance related */
    prWifiVar->u4HifIstLoopCount = (UINT_32)wlanCfgGetUint32(prAdapter, "IstLoop", CFG_IST_LOOP_COUNT);
    prWifiVar->u4Rx2OsLoopCount = (UINT_32)wlanCfgGetUint32(prAdapter, "Rx2OsLoop", 4);
    prWifiVar->u4HifTxloopCount = (UINT_32)wlanCfgGetUint32(prAdapter, "HifTxLoop", 1);
    prWifiVar->u4TxFromOsLoopCount = (UINT_32)wlanCfgGetUint32(prAdapter, "OsTxLoop", 1);
    prWifiVar->u4TxRxLoopCount = (UINT_32)wlanCfgGetUint32(prAdapter, "Rx2ReorderLoop", 1);

    prWifiVar->u4NetifStopTh = (UINT_32)wlanCfgGetUint32(prAdapter, "NetifStopTh", CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD);
    prWifiVar->u4NetifStartTh = (UINT_32)wlanCfgGetUint32(prAdapter, "NetifStartTh", CFG_TX_START_NETIF_PER_QUEUE_THRESHOLD);
    prWifiVar->ucTxBaSize = (UINT_8)wlanCfgGetUint32(prAdapter, "TxBaSize", 64);
    prWifiVar->ucRxHtBaSize = (UINT_8)wlanCfgGetUint32(prAdapter, "RxHtBaSize", 64);
    prWifiVar->ucRxVhtBaSize = (UINT_8)wlanCfgGetUint32(prAdapter, "RxVhtBaSize", 32);

    /* Tx Buffer Management */
    prWifiVar->ucExtraTxDone = (UINT_32)wlanCfgGetUint32(prAdapter, "ExtraTxDone", 1);
    prWifiVar->ucTxDbg = (UINT_32)wlanCfgGetUint32(prAdapter, "TxDbg", 0);
    
    kalMemZero(prWifiVar->au4TcPageCount, sizeof(prWifiVar->au4TcPageCount));
    
    prWifiVar->au4TcPageCount[TC0_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc0Page", NIC_TX_PAGE_COUNT_TC0);
    prWifiVar->au4TcPageCount[TC1_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc1Page", NIC_TX_PAGE_COUNT_TC1);
    prWifiVar->au4TcPageCount[TC2_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc2Page", NIC_TX_PAGE_COUNT_TC2);
    prWifiVar->au4TcPageCount[TC3_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc3Page", NIC_TX_PAGE_COUNT_TC3);
    prWifiVar->au4TcPageCount[TC4_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc4Page", NIC_TX_PAGE_COUNT_TC4);   
    prWifiVar->au4TcPageCount[TC5_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc5Page", NIC_TX_PAGE_COUNT_TC5);       
    
    prQM->au4MinReservedTcResource[TC0_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc0MinRsv", QM_MIN_RESERVED_TC0_RESOURCE);
    prQM->au4MinReservedTcResource[TC1_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc1MinRsv", QM_MIN_RESERVED_TC1_RESOURCE);
    prQM->au4MinReservedTcResource[TC2_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc2MinRsv", QM_MIN_RESERVED_TC2_RESOURCE);
    prQM->au4MinReservedTcResource[TC3_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc3MinRsv", QM_MIN_RESERVED_TC3_RESOURCE);
    prQM->au4MinReservedTcResource[TC4_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc4MinRsv", QM_MIN_RESERVED_TC4_RESOURCE);
    prQM->au4MinReservedTcResource[TC5_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc5MinRsv", QM_MIN_RESERVED_TC5_RESOURCE);

    prQM->au4GuaranteedTcResource[TC0_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc0Grt", QM_GUARANTEED_TC0_RESOURCE);
    prQM->au4GuaranteedTcResource[TC1_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc1Grt", QM_GUARANTEED_TC1_RESOURCE);
    prQM->au4GuaranteedTcResource[TC2_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc2Grt", QM_GUARANTEED_TC2_RESOURCE);
    prQM->au4GuaranteedTcResource[TC3_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc3Grt", QM_GUARANTEED_TC3_RESOURCE);
    prQM->au4GuaranteedTcResource[TC4_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc4Grt", QM_GUARANTEED_TC4_RESOURCE);
    prQM->au4GuaranteedTcResource[TC5_INDEX] = (UINT_32)wlanCfgGetUint32(prAdapter, "Tc5Grt", QM_GUARANTEED_TC5_RESOURCE);

    prQM->u4TimeToAdjustTcResource = (UINT_32)wlanCfgGetUint32(prAdapter, "TcAdjustTime", QM_INIT_TIME_TO_ADJUST_TC_RSC);
    prQM->u4TimeToUpdateQueLen = (UINT_32)wlanCfgGetUint32(prAdapter, "QueLenUpdateTime", QM_INIT_TIME_TO_UPDATE_QUE_LEN);
    prQM->u4QueLenMovingAverage = (UINT_32)wlanCfgGetUint32(prAdapter, "QueLenMovingAvg", QM_QUE_LEN_MOVING_AVE_FACTOR);
    prQM->u4ExtraReservedTcResource = (UINT_32)wlanCfgGetUint32(prAdapter, "TcExtraRsv", QM_EXTRA_RESERVED_RESOURCE_WHEN_BUSY);
 
	prWifiVar->ucDhcpTxDone = (UINT_8)wlanCfgGetUint32(prAdapter, "DhcpTxDone", 1);
    prWifiVar->ucArpTxDone = (UINT_8)wlanCfgGetUint32(prAdapter, "ArpTxDone", 1);
    prWifiVar->ucDnsTxDone = (UINT_8)wlanCfgGetUint32(prAdapter, "DnsTxDone", 1);
}

VOID
wlanCfgSetSwCtrl(
    IN P_ADAPTER_T prAdapter
    )
{
    UINT_32 i = 0;
    CHAR   aucKey[WLAN_CFG_VALUE_LEN_MAX];
    CHAR   aucValue[WLAN_CFG_VALUE_LEN_MAX];
    const CHAR   acDelim[] = " ";    
    CHAR   *pcPtr = NULL;
    CHAR   *pcDupValue = NULL;
    
    UINT_32 au4Values[2];
    UINT_32 u4TokenCount = 0;
    UINT_32 u4BufLen = 0;
    WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
    P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
    PARAM_CUSTOM_SW_CTRL_STRUC_T rSwCtrlInfo;

    for(i = 0;i < WLAN_CFG_SET_SW_CTRL_LEN_MAX; i++) {
        kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
        kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);        
        kalSprintf(aucKey, "SwCtrl%d", i);

        /* get nothing */
        if(wlanCfgGet(prAdapter, aucKey, aucValue, "", 0) != WLAN_STATUS_SUCCESS) continue;
        if(!kalStrCmp(aucValue, "")) continue;

        pcDupValue = aucValue;
        u4TokenCount = 0;

        while((pcPtr = kalStrSep((char **)(&pcDupValue), acDelim)) != NULL) {

            if(!kalStrCmp(pcPtr, "")) continue;

            au4Values[u4TokenCount] = kalStrtoul(pcPtr, NULL, 0);
            u4TokenCount++;

            /* Only need 2 tokens */
            if(u4TokenCount >= 2) break;
        }

        if(u4TokenCount != 2) continue;

        rSwCtrlInfo.u4Id = au4Values[0];
        rSwCtrlInfo.u4Data = au4Values[1]; 

        rStatus = kalIoctl(prGlueInfo,
            wlanoidSetSwCtrlWrite,
            &rSwCtrlInfo,
            sizeof(rSwCtrlInfo),
            FALSE,
            FALSE,
            TRUE,
            &u4BufLen);

    } 
}

VOID
wlanCfgSetChip(
    IN P_ADAPTER_T prAdapter
    )
{
    UINT_32 i = 0;
    CHAR   aucKey[WLAN_CFG_VALUE_LEN_MAX];
    CHAR   aucValue[WLAN_CFG_VALUE_LEN_MAX];   

    UINT_32 u4BufLen = 0;
    WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
    P_GLUE_INFO_T prGlueInfo = prAdapter->prGlueInfo;
    PARAM_CUSTOM_CHIP_CONFIG_STRUC_T rChipConfigInfo;

    for(i = 0;i < WLAN_CFG_SET_CHIP_LEN_MAX; i++) {
        kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
        kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);        
        kalSprintf(aucKey, "SetChip%d", i);

        /* get nothing */
        if(wlanCfgGet(prAdapter, aucKey, aucValue, "", 0) != WLAN_STATUS_SUCCESS) continue;
        if(!kalStrCmp(aucValue, "")) continue;

        kalMemZero(&rChipConfigInfo, sizeof(rChipConfigInfo)); 

        rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE; 
        rChipConfigInfo.u2MsgSize = kalStrnLen(aucValue, WLAN_CFG_VALUE_LEN_MAX);
        kalStrnCpy(rChipConfigInfo.aucCmd, aucValue, CHIP_CONFIG_RESP_SIZE);

        rStatus = kalIoctl(prGlueInfo,
            wlanoidSetChipConfig,
            &rChipConfigInfo,
            sizeof(rChipConfigInfo),
            FALSE,
            FALSE,
            TRUE,
            &u4BufLen);  
    }
        
}

VOID
wlanCfgSetDebugLevel(
    IN P_ADAPTER_T prAdapter
    )
{
    UINT_32 i = 0;
    CHAR   aucKey[WLAN_CFG_VALUE_LEN_MAX];
    CHAR   aucValue[WLAN_CFG_VALUE_LEN_MAX];
    const CHAR   acDelim[] = " ";
    CHAR   *pcDupValue;    
    CHAR   *pcPtr = NULL;

    UINT_32 au4Values[2];
    UINT_32 u4TokenCount = 0;
    UINT_32 u4DbgIdx = 0;
    UINT_32 u4DbgMask = 0;    

    for(i = 0;i < WLAN_CFG_SET_DEBUG_LEVEL_LEN_MAX; i++) {
        kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
        kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
        kalSprintf(aucKey, "DbgLevel%d", i);
        
        /* get nothing */
        if(wlanCfgGet(prAdapter, aucKey, aucValue, "", 0) != WLAN_STATUS_SUCCESS) continue;
        if(!kalStrCmp(aucValue, "")) continue;

        pcDupValue = aucValue;
        u4TokenCount = 0;

        while((pcPtr = kalStrSep((char **)(&pcDupValue), acDelim)) != NULL) {

            if(!kalStrCmp(pcPtr, "")) continue;

            au4Values[u4TokenCount] = kalStrtoul(pcPtr, NULL, 0);
            u4TokenCount++;

            /* Only need 2 tokens */
            if(u4TokenCount >= 2) break;
        }

        if(u4TokenCount != 2) continue;
            
        u4DbgIdx = au4Values[0];
        u4DbgMask = au4Values[1];

        /* DBG level special control */
        if(u4DbgIdx == 0xFFFFFFFF) {
            wlanSetDebugLevel(DBG_ALL_MODULE_IDX, u4DbgMask);
            DBGLOG(INIT, INFO, ("Set ALL DBG module log level to [0x%02x]!", (UINT_8)u4DbgMask));
        }
        else if(u4DbgIdx == 0xFFFFFFFE) {
            wlanDebugInit();
            DBGLOG(INIT, INFO, ("Reset ALL DBG module log level to DEFAULT!"));
        }
        else if(u4DbgIdx < DBG_MODULE_NUM) {
            wlanSetDebugLevel(u4DbgIdx, u4DbgMask);
            DBGLOG(INIT, INFO, ("Set DBG module[%lu] log level to [0x%02x]!", u4DbgIdx, (UINT_8)u4DbgMask));
        }        
    }
    
}

VOID
wlanCfgSetCountryCode(
    IN P_ADAPTER_T prAdapter
    )
{
    CHAR   aucValue[WLAN_CFG_VALUE_LEN_MAX];
    	
    /* Apply COUNTRY Config */
    if(wlanCfgGet(prAdapter, "Country", aucValue, "", 0) == WLAN_STATUS_SUCCESS) {	
        prAdapter->rWifiVar.rConnSettings.u2CountryCode =
        (((UINT_16) aucValue[0]) << 8) | ((UINT_16) aucValue[1]) ;

        prAdapter->prDomainInfo = NULL; /* Force to re-search country code */
        rlmDomainSendCmd(prAdapter, TRUE);    	
    }
}


#if CFG_SUPPORT_CFG_FILE

P_WLAN_CFG_ENTRY_T wlanCfgGetEntry(IN P_ADAPTER_T prAdapter, const PCHAR pucKey)
{

    P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
    P_WLAN_CFG_T prWlanCfg;
    UINT_32 i;
    prWlanCfg = prAdapter->prWlanCfg;

    ASSERT(prWlanCfg);
    ASSERT(pucKey);

    prWlanCfgEntry = NULL;

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
        prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[i];
		if (prWlanCfgEntry->aucKey[0] != '\0') {
			DBGLOG(INIT, LOUD,
			       ("compare key %s saved key %s\n", pucKey, prWlanCfgEntry->aucKey));
			if (kalStrnCmp(pucKey, prWlanCfgEntry->aucKey, WLAN_CFG_KEY_LEN_MAX - 1) ==
			    0) {
                return prWlanCfgEntry;
            }
        }
    }

    DBGLOG(INIT, TRACE, ("wifi config there is no entry \'%s\'\n", pucKey));
    return NULL;

}

WLAN_STATUS
wlanCfgGet(IN P_ADAPTER_T prAdapter,
	   const PCHAR pucKey, PCHAR pucValue, PCHAR pucValueDef, UINT_32 u4Flags)
{

    P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
    P_WLAN_CFG_T prWlanCfg;
    prWlanCfg = prAdapter->prWlanCfg;

    ASSERT(prWlanCfg);
    ASSERT(pucValue);

    /* Find the exist */
    prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (prWlanCfgEntry) {
		kalStrnCpy(pucValue, prWlanCfgEntry->aucValue, WLAN_CFG_VALUE_LEN_MAX - 1);
        return WLAN_STATUS_SUCCESS;
	} else {
		if (pucValueDef)
			kalStrnCpy(pucValue, pucValueDef, WLAN_CFG_VALUE_LEN_MAX - 1);
        return WLAN_STATUS_FAILURE;
    }

}

UINT_32 wlanCfgGetUint32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, UINT_32 u4ValueDef)
{
    P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
    P_WLAN_CFG_T prWlanCfg;
    UINT_32 u4Ret;
    prWlanCfg = prAdapter->prWlanCfg;

    ASSERT(prWlanCfg);

    u4Ret = u4ValueDef;
    /* Find the exist */
    prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (prWlanCfgEntry) {
		u4Ret = kalStrtoul(prWlanCfgEntry->aucValue, NULL, 0);
    }
    return u4Ret;
}

INT_32 wlanCfgGetInt32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, INT_32 i4ValueDef)
{
    P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
    P_WLAN_CFG_T prWlanCfg;
    INT_32 i4Ret;
    prWlanCfg = prAdapter->prWlanCfg;

    ASSERT(prWlanCfg);

    i4Ret = i4ValueDef;
    /* Find the exist */
    prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (prWlanCfgEntry) {
		i4Ret = kalStrtol(prWlanCfgEntry->aucValue, NULL, 0);
    }
    return i4Ret;
}



WLAN_STATUS
wlanCfgSet(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, PCHAR pucValue, UINT_32 u4Flags)
{

    P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
    P_WLAN_CFG_T prWlanCfg;
    UINT_32 u4EntryIndex;
    UINT_32 i;
    UINT_8 ucExist;

    prWlanCfg = prAdapter->prWlanCfg;
    ASSERT(prWlanCfg);
    ASSERT(pucKey);

    /* Find the exist */
    ucExist = 0;
    prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (!prWlanCfgEntry) {
        /* Find the empty */
		for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
            prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[i];
			if (prWlanCfgEntry->aucKey[0] == '\0') {
                break;
            }
        }

        u4EntryIndex = i;
		if (u4EntryIndex < WLAN_CFG_ENTRY_NUM_MAX) {
            prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[u4EntryIndex];
			kalMemZero(prWlanCfgEntry, sizeof(WLAN_CFG_ENTRY_T));
		} else {
            prWlanCfgEntry = NULL;
            DBGLOG(INIT, ERROR, ("wifi config there is no empty entry\n"));
        }
    } /* !prWlanCfgEntry */
    else {
        ucExist = 1;
    }

	if (prWlanCfgEntry) {
		if (ucExist == 0) {
			kalStrnCpy(prWlanCfgEntry->aucKey, pucKey, WLAN_CFG_KEY_LEN_MAX - 1);
			prWlanCfgEntry->aucKey[WLAN_CFG_KEY_LEN_MAX - 1] = '\0';
        }

		if (pucValue && pucValue[0] != '\0') {
			kalStrnCpy(prWlanCfgEntry->aucValue, pucValue, WLAN_CFG_VALUE_LEN_MAX - 1);
			prWlanCfgEntry->aucValue[WLAN_CFG_VALUE_LEN_MAX - 1] = '\0';

			if (ucExist) {
				if (prWlanCfgEntry->pfSetCb)
                    prWlanCfgEntry->pfSetCb(prAdapter, 
                            prWlanCfgEntry->aucKey, 
                            prWlanCfgEntry->aucValue, 
                            prWlanCfgEntry->pPrivate, 0);
            }
		} else {
            /* Call the pfSetCb if value is empty ? */
            /* remove the entry if value is empty */
			kalMemZero(prWlanCfgEntry, sizeof(WLAN_CFG_ENTRY_T));
        }

	}
	/* prWlanCfgEntry */
	if (prWlanCfgEntry) {
        DBGLOG(INIT, INFO, ("Set wifi config exist %u \'%s\' \'%s\'\n",
                    ucExist, prWlanCfgEntry->aucKey, prWlanCfgEntry->aucValue));
        return WLAN_STATUS_SUCCESS;
	} else {
		if (pucKey) {
            DBGLOG(INIT, ERROR, ("Set wifi config error key \'%s\'\n", pucKey));
        }
		if (pucValue) {
            DBGLOG(INIT, ERROR, ("Set wifi config error value \'%s\'\n", pucValue));
        }
        return WLAN_STATUS_FAILURE;
    }

}

WLAN_STATUS
wlanCfgSetCb(IN P_ADAPTER_T prAdapter,
	     const PCHAR pucKey, WLAN_CFG_SET_CB pfSetCb, void *pPrivate, UINT_32 u4Flags)
{

    P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
    P_WLAN_CFG_T prWlanCfg;

    prWlanCfg = prAdapter->prWlanCfg;
    ASSERT(prWlanCfg);

    /* Find the exist */
    prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey);

	if (prWlanCfgEntry) {
        prWlanCfgEntry->pfSetCb = pfSetCb;
        prWlanCfgEntry->pPrivate = pPrivate;
    }

	if (prWlanCfgEntry) {
        return WLAN_STATUS_SUCCESS;
	} else {
        return WLAN_STATUS_FAILURE;
    }

}

WLAN_STATUS wlanCfgSetUint32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, UINT_32 u4Value)
{

    P_WLAN_CFG_T prWlanCfg;
    UINT_8 aucBuf[WLAN_CFG_VALUE_LEN_MAX];

    prWlanCfg = prAdapter->prWlanCfg;

    ASSERT(prWlanCfg);

	kalMemZero(aucBuf, sizeof(aucBuf));

	kalSnprintf(aucBuf, WLAN_CFG_VALUE_LEN_MAX, "0x%x", (unsigned int)u4Value);

	return wlanCfgSet(prAdapter, pucKey, aucBuf, 0);
}

enum {
    STATE_EOF = 0,
    STATE_TEXT = 1,
    STATE_NEWLINE = 2
};

struct WLAN_CFG_PARSE_STATE_S {
    CHAR *ptr;
    CHAR *text;
    INT_32 nexttoken;
    UINT_32 maxSize;
};

INT_32 wlanCfgFindNextToken(struct WLAN_CFG_PARSE_STATE_S *state)
{
    CHAR *x = state->ptr;
    CHAR *s;

    if (state->nexttoken) {
        INT_32 t = state->nexttoken;
        state->nexttoken = 0;
        return t;
    }

    for (;;) {
        switch (*x) {
        case 0:
            state->ptr = x;
            return STATE_EOF;
        case '\n':
            x++;
            state->ptr = x;
            return STATE_NEWLINE;
        case ' ':
        case '\t':
        case '\r':
            x++;
            continue;
        case '#':
			while (*x && (*x != '\n'))
				x++;
            if (*x == '\n') {
				state->ptr = x + 1;
                return STATE_NEWLINE;
            } else {
                state->ptr = x;
                return STATE_EOF;
            }
        default:
            goto text;
        }
    }

textdone:
    state->ptr = x;
    *s = 0;
    return STATE_TEXT;
text:
    state->text = s = x;
textresume:
    for (;;) {
        switch (*x) {
        case 0:
            goto textdone;
        case ' ':
        case '\t':
        case '\r':
            x++;
            goto textdone;
        case '\n':
            state->nexttoken = STATE_NEWLINE;
            x++;
            goto textdone;
        case '"':
            x++;
            for (;;) {
                switch (*x) {
                case 0:
                        /* unterminated quoted thing */
                    state->ptr = x;
                    return STATE_EOF;
                case '"':
                    x++;
                    goto textresume;
                default:
                    *s++ = *x++;
                }
            }
            break;
        case '\\':
            x++;
            switch (*x) {
            case 0:
                goto textdone;
            case 'n':
                *s++ = '\n';
                break;
            case 'r':
                *s++ = '\r';
                break;
            case 't':
                *s++ = '\t';
                break;
            case '\\':
                *s++ = '\\';
                break;
            case '\r':
                    /* \ <cr> <lf> -> line continuation */
                if (x[1] != '\n') {
                    x++;
                    continue;
                }
            case '\n':
                    /* \ <lf> -> line continuation */
                x++;
                    /* eat any extra whitespace */
				while ((*x == ' ') || (*x == '\t'))
					x++;
                continue;
            default:
                    /* unknown escape -- just copy */
                *s++ = *x++;
            }
            continue;
        default:
            *s++ = *x++;
        }
    }
    return STATE_EOF;
}

WLAN_STATUS wlanCfgParseArgument(CHAR *cmdLine, INT_32 *argc, CHAR *argv[]
        )
{  
    struct WLAN_CFG_PARSE_STATE_S state;  
    CHAR **args;  
    INT_32 nargs;  

	if (cmdLine == NULL || argc == NULL || argv == NULL) {
        ASSERT(0);
        return WLAN_STATUS_FAILURE;
    }
    args = argv;
    nargs = 0;  
    state.ptr = cmdLine;  
    state.nexttoken = 0;
    state.maxSize = 0;

	if (kalStrnLen(cmdLine, 512) >= 512) {
        ASSERT(0);
        return WLAN_STATUS_FAILURE;
    }
  
    for (;;) {  
        switch (wlanCfgFindNextToken(&state)) {  
        case STATE_EOF:  
            goto exit;
        case STATE_NEWLINE:  
            goto exit;
        case STATE_TEXT:  
            if (nargs < WLAN_CFG_ARGV_MAX) {  
                args[nargs++] = state.text;  
            }  
            break;  
        }  
    }  
  
exit:
    *argc = nargs;
    return WLAN_STATUS_SUCCESS;
}  



WLAN_STATUS
wlanCfgParseAddEntry(IN P_ADAPTER_T prAdapter,
    PUINT_8    pucKeyHead, 
		     PUINT_8 pucKeyTail, PUINT_8 pucValueHead, PUINT_8 pucValueTail)
{

    UINT_8 aucKey[WLAN_CFG_KEY_LEN_MAX];
    UINT_8 aucValue[WLAN_CFG_VALUE_LEN_MAX];
    UINT_32 u4Len;

    kalMemZero(aucKey, sizeof(aucKey));
    kalMemZero(aucValue, sizeof(aucValue));

	if ((pucKeyHead == NULL)
            || (pucValueHead == NULL) 
	    ) {
        return WLAN_STATUS_FAILURE;
    }

	if (pucKeyTail) {
		if (pucKeyHead > pucKeyTail)
            return WLAN_STATUS_FAILURE;
		u4Len = pucKeyTail - pucKeyHead + 1;
	} else {
		u4Len = kalStrnLen(pucKeyHead, WLAN_CFG_KEY_LEN_MAX - 1);
    }

	if (u4Len >= WLAN_CFG_KEY_LEN_MAX) {
		u4Len = WLAN_CFG_KEY_LEN_MAX - 1;
    }
    kalStrnCpy(aucKey, pucKeyHead, u4Len);

	if (pucValueTail) {
		if (pucValueHead > pucValueTail)
            return WLAN_STATUS_FAILURE;
		u4Len = pucValueTail - pucValueHead + 1;
	} else {
		u4Len = kalStrnLen(pucValueHead, WLAN_CFG_VALUE_LEN_MAX - 1);
    }

	if (u4Len >= WLAN_CFG_VALUE_LEN_MAX) {
		u4Len = WLAN_CFG_VALUE_LEN_MAX - 1;
    }
    kalStrnCpy(aucValue, pucValueHead, u4Len);

    return wlanCfgSet(prAdapter, aucKey, aucValue, 0);
}

enum {
    WAIT_KEY_HEAD = 0,
    WAIT_KEY_TAIL,
    WAIT_VALUE_HEAD,
    WAIT_VALUE_TAIL,
    WAIT_COMMENT_TAIL
};


WLAN_STATUS wlanCfgParse(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf, UINT_32 u4ConfigBufLen)
{

    struct WLAN_CFG_PARSE_STATE_S state;  
    PCHAR  apcArgv[WLAN_CFG_ARGV_MAX];
    CHAR **args;  
    INT_32 nargs;  


	if (pucConfigBuf == NULL) {
        ASSERT(0);
        return WLAN_STATUS_FAILURE;
    }
	if (kalStrnLen(pucConfigBuf, 4000) >= 4000) {
        ASSERT(0);
        return WLAN_STATUS_FAILURE;
    }
	if (u4ConfigBufLen == 0) {
        return WLAN_STATUS_FAILURE;
    }
    args = apcArgv;
    nargs = 0;  
    state.ptr = pucConfigBuf;
    state.nexttoken = 0;
    state.maxSize = u4ConfigBufLen;
 
    for (;;) {  
        switch (wlanCfgFindNextToken(&state)) {  
        case STATE_EOF:  
			if (nargs > 1) {
				wlanCfgParseAddEntry(prAdapter, args[0], NULL, args[1], NULL);
            }
            goto exit;
        case STATE_NEWLINE:
			if (nargs > 1) {
				wlanCfgParseAddEntry(prAdapter, args[0], NULL, args[1], NULL);
            }
            nargs = 0;
            break;
        case STATE_TEXT:  
            if (nargs < WLAN_CFG_ARGV_MAX) {  
                args[nargs++] = state.text;  
            }  
            break;  
        }  
    }  
  
exit:
    return WLAN_STATUS_SUCCESS;

#if 0
    /* Old version */ 
    UINT_32 i;
    UINT_8 c;
    PUINT_8 pbuf;
    UINT_8 ucState;
    PUINT_8    pucKeyTail = NULL;
    PUINT_8    pucKeyHead = NULL;
    PUINT_8    pucValueHead = NULL;
    PUINT_8    pucValueTail = NULL;

    ucState = WAIT_KEY_HEAD;
    pbuf = pucConfigBuf;

	for (i = 0; i < u4ConfigBufLen; i++) {
        c = pbuf[i];
		if (c == '\r' || c == '\n') {

			if (ucState == WAIT_VALUE_TAIL) {
                /* Entry found */
				if (pucValueHead) {
					wlanCfgParseAddEntry(prAdapter, pucKeyHead, pucKeyTail,
							     pucValueHead, pucValueTail);
                }
            }
            ucState = WAIT_KEY_HEAD;
            pucKeyTail = NULL;
            pucKeyHead = NULL;
            pucValueHead = NULL;
            pucValueTail = NULL;

		} else if (c == '=') {
			if (ucState == WAIT_KEY_TAIL) {
				pucKeyTail = &pbuf[i - 1];
                ucState = WAIT_VALUE_HEAD;
            }
		} else if (c == ' ' || c == '\t') {
			if (ucState == WAIT_KEY_HEAD) {


			} else if (ucState == WAIT_KEY_TAIL) {
				pucKeyTail = &pbuf[i - 1];
                ucState = WAIT_VALUE_HEAD;
            }
		} else {

			if (c == '#') {
                /* comments */
				if (ucState == WAIT_KEY_HEAD) {
                    ucState = WAIT_COMMENT_TAIL;
				} else if (ucState == WAIT_VALUE_TAIL) {
                    pucValueTail = &pbuf[i];
                }

			} else {
				if (ucState == WAIT_KEY_HEAD) {
                    pucKeyHead = &pbuf[i];
					pucKeyTail = &pbuf[i];
                    ucState = WAIT_KEY_TAIL;
				} else if (ucState == WAIT_VALUE_HEAD) {
                    pucValueHead = &pbuf[i];
                    pucValueTail = &pbuf[i];
                    ucState = WAIT_VALUE_TAIL;
				} else if (ucState == WAIT_VALUE_TAIL) {
                    pucValueTail = &pbuf[i];
                }
            }
        }

    } /* for */

	if (ucState == WAIT_VALUE_TAIL) {
        /* Entry found */
		if (pucValueTail) {
			wlanCfgParseAddEntry(prAdapter, pucKeyHead, pucKeyTail, pucValueHead,
					     pucValueTail);
        }
    }
#endif

    return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanCfgInit(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf, UINT_32 u4ConfigBufLen, UINT_32 u4Flags)
{
    P_WLAN_CFG_T prWlanCfg;
	/* P_WLAN_CFG_ENTRY_T prWlanCfgEntry; */
    prAdapter->prWlanCfg = &prAdapter->rWlanCfg;
    prWlanCfg = prAdapter->prWlanCfg;

    kalMemZero(prWlanCfg, sizeof(WLAN_CFG_T));
    ASSERT(prWlanCfg);
    prWlanCfg->u4WlanCfgEntryNumMax = WLAN_CFG_ENTRY_NUM_MAX;
    prWlanCfg->u4WlanCfgKeyLenMax = WLAN_CFG_KEY_LEN_MAX;
    prWlanCfg->u4WlanCfgValueLenMax = WLAN_CFG_VALUE_LEN_MAX;

    DBGLOG(INIT, INFO, ("Init wifi config len %u max entry %u\n", 
                u4ConfigBufLen, prWlanCfg->u4WlanCfgEntryNumMax));
#if DBG
    /* self test */
	wlanCfgSet(prAdapter, "ConfigValid", "0x123", 0);
	if (wlanCfgGetUint32(prAdapter, "ConfigValid", 0) != 0x123) {
		DBGLOG(INIT, INFO, ("wifi config error %u\n", __LINE__));
    }
	wlanCfgSet(prAdapter, "ConfigValid", "1", 0);
	if (wlanCfgGetUint32(prAdapter, "ConfigValid", 0) != 1) {
		DBGLOG(INIT, INFO, ("wifi config error %u\n", __LINE__));
    }
#endif

    /* Parse the pucConfigBuff */

	if (pucConfigBuf && (u4ConfigBufLen > 0)) {
		wlanCfgParse(prAdapter, pucConfigBuf, u4ConfigBufLen);
    }


    return WLAN_STATUS_SUCCESS;
}

#endif /* CFG_SUPPORT_CFG_FILE */



INT_32 wlanHexToNum(CHAR c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}


INT_32 wlanHexToByte(PCHAR hex)
{
	INT_32 a, b;
	a = wlanHexToNum(*hex++);
	if (a < 0)
		return -1;
	b = wlanHexToNum(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}



INT_32 wlanHwAddrToBin(PCHAR txt, UINT_8 *addr)
{
	INT_32 i;
	PCHAR pos = txt;

	for (i = 0; i < 6; i++) {
		INT_32 a, b;

		while (*pos == ':' || *pos == '.' || *pos == '-')
			pos++;

		a = wlanHexToNum(*pos++);
		if (a < 0)
			return -1;
		b = wlanHexToNum(*pos++);
		if (b < 0)
			return -1;
		*addr++ = (a << 4) | b;
	}

	return pos - txt;
}

BOOLEAN wlanIsChipNoAck(IN P_ADAPTER_T prAdapter)
{
    BOOLEAN fgIsNoAck;

	fgIsNoAck = prAdapter->fgIsChipNoAck || kalIsResetting()
                || fgIsBusAccessFailed;

    return fgIsNoAck;
}

#if CFG_AUTO_CHANNEL_SEL_SUPPORT

/* 4   Auto Channel Selection */
WLAN_STATUS 
wlanoidQueryACSChannelList(IN P_ADAPTER_T prAdapter,
    IN PVOID        pvQueryBuffer,
			   IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
        WLAN_STATUS rResult = WLAN_STATUS_FAILURE;
        P_PARAM_GET_LTE_MODE prLteMode;
        UINT_8 ucIdx;	
        P_PARAM_CHN_LOAD_INFO prChnLoad;
	
	DBGLOG(P2P, INFO, ("[Auto Channel]wlanoidQueryACSChannelList\n"));
	do {
        ASSERT(pvQueryBuffer);

        /* 1. Sanity test */
        if ((prAdapter == NULL) || (pu4QueryInfoLen == NULL))
			break;
		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL))
			break;
		
		prLteMode = (P_PARAM_GET_LTE_MODE) pvQueryBuffer;
		/* 2. Check AP Numbers */
		for (ucIdx = 0; ucIdx < MAX_AUTO_CHAL_NUM; ucIdx++) {
			prChnLoad =
			    (P_PARAM_CHN_LOAD_INFO) &(prAdapter->rWifiVar.rChnLoadInfo.
						       rEachChnLoad[ucIdx]);
			
			DBGLOG(P2P, INFO,
			       ("[Auto Channel] AP Num: Chn[%d]=%d\n", ucIdx + 1,
				prChnLoad->u2APNum));
		}
		/* 3. Ensure FW supports get station link status */		
#if 0
		if (prAdapter->u4FwCompileFlag0 & COMPILE_FLAG0_GET_STA_LINK_STATUS) {
			printk("wlanoidQueryACSChannelList\n");
			CMD_ACCESS_REG rCmdAccessReg;
         	rCmdAccessReg.u4Address = 0xFFFFFFFF;
            rCmdAccessReg.u4Data = ELEM_RM_TYPE_ACS_CHN;
           
			rResult = wlanSendSetQueryCmd(prAdapter, CMD_ID_ACCESS_REG, TRUE, TRUE, TRUE, nicCmdEventQueryChannelLoad,	/* The handler to receive firmware notification */
			   nicOidCmdTimeoutCommon,
			   sizeof(CMD_ACCESS_REG),
						      (PUINT_8) &rCmdAccessReg,
						      pvQueryBuffer, u4QueryBufferLen);
            
            prQueryChnLoad->u4Flag |= BIT(1);
		} else {
            rResult = WLAN_STATUS_NOT_SUPPORTED;
        }
#endif
		/* 4. Avoid LTE Channels */		
		prLteMode->u4Flags &= BIT(0);
		/*if(prAdapter->u4FwCompileFlag0 & COMPILE_FLAG0_GET_STA_LINK_STATUS) */
		{
			CMD_GET_LTE_SAFE_CHN_T rQuery_LTE_SAFE_CHN;
			rResult = wlanSendSetQueryCmd(prAdapter, CMD_ID_GET_LTE_CHN, FALSE, TRUE, TRUE,	/* Query ID */
						      nicCmdEventQueryLTESafeChn,	/* The handler to receive firmware notification */
										nicOidCmdTimeoutCommon,
										sizeof(CMD_GET_LTE_SAFE_CHN_T),
						      (PUINT_8) &rQuery_LTE_SAFE_CHN,
						      pvQueryBuffer, u4QueryBufferLen);
			DBGLOG(P2P, INFO, ("[Auto Channel] Get LTE Channels\n"));
			prLteMode->u4Flags |= BIT(1);
		}
		/*
		else {
			rResult = WLAN_STATUS_NOT_SUPPORTED;
		}
		*/

		/* 5. Calc the value */		
		DBGLOG(P2P, INFO, ("[Auto Channel] Candidated Channels\n"));
	} while (FALSE);
    
	return rResult;
} /* wlanoidQueryP2pVersion */
#endif

#if CFG_ENABLE_PER_STA_STATISTICS
VOID
wlanTxLifetimeUpdateStaStats(IN P_ADAPTER_T prAdapter, 
    IN P_MSDU_INFO_T prMsduInfo)
{
    P_STA_RECORD_T prStaRec;
    UINT_32 u4DeltaTime;
    P_QUE_MGT_T prQM = &prAdapter->rQM;
    P_PKT_PROFILE_T prPktProfile = &prMsduInfo->rPktProfile;
    UINT_32 u4PktPrintPeriod = 0;

    prStaRec = cnmGetStaRecByIndex(prAdapter, prMsduInfo->ucStaRecIndex);

    if (prStaRec) {
    	u4DeltaTime = (UINT_32)(prPktProfile->rHifTxDoneTimestamp -
    		       prPktProfile->rHardXmitArrivalTimestamp);

        /* Update StaRec statistics */
    	prStaRec->u4TotalTxPktsNumber++;
    	prStaRec->u4TotalTxPktsTime += u4DeltaTime;
        
    	if (u4DeltaTime > prStaRec->u4MaxTxPktsTime) {
    		prStaRec->u4MaxTxPktsTime = u4DeltaTime;
    	}
    	if (u4DeltaTime >= NIC_TX_TIME_THRESHOLD) {
    		prStaRec->u4ThresholdCounter++;
    	}

    	if (u4PktPrintPeriod && (prStaRec->u4TotalTxPktsNumber >= u4PktPrintPeriod)) {
    		DBGLOG(TX, TRACE, ("[%u]N[%4lu]A[%5lu]M[%4lu]T[%4lu]E[%4lu]\n",
    			prStaRec->ucIndex,
    			prStaRec->u4TotalTxPktsNumber,
    			(prStaRec->u4TotalTxPktsTime/prStaRec->u4TotalTxPktsNumber),
    			prStaRec->u4MaxTxPktsTime,
    			prStaRec->u4ThresholdCounter,
    			prQM->au4QmTcResourceEmptyCounter[prStaRec->ucBssIndex][TC2_INDEX]));

    		prStaRec->u4TotalTxPktsNumber = 0;
    		prStaRec->u4TotalTxPktsTime = 0;
    		prStaRec->u4MaxTxPktsTime = 0;
    		prStaRec->u4ThresholdCounter = 0;
    		prQM->au4QmTcResourceEmptyCounter[prStaRec->ucBssIndex][TC2_INDEX] = 0;
    	}
    }
}
#endif

BOOLEAN
wlanTxLifetimeIsProfilingEnabled(IN P_ADAPTER_T prAdapter)
{
    BOOLEAN fgEnabled = FALSE;
#if CFG_SUPPORT_WFD
	P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;

	prWfdCfgSettings = &prAdapter->rWifiVar.rWfdConfigureSettings;

    if(prWfdCfgSettings->ucWfdEnable > 0)
        fgEnabled = TRUE;
#endif

    return fgEnabled;
}

BOOLEAN
wlanTxLifetimeIsTargetMsdu(IN P_ADAPTER_T prAdapter, 
    IN P_MSDU_INFO_T prMsduInfo)
{
    BOOLEAN fgResult = TRUE;

#if 0
	switch (prMsduInfo->ucTID) {
		/* BK */
    case 1: 
    case 2: 

		/* BE */
    case 0:
    case 3:
        fgResult = FALSE;
        break;
		/* VI */
	case 4:
	case 5:

		/* VO */
	case 6:
	case 7:
		fgResult = TRUE;
		break;
	default:
		break;
	}
#endif    
    return fgResult;
}

VOID
wlanTxLifetimeTagPacket(IN P_ADAPTER_T prAdapter,
    IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_PROFILING_TAG_T eTag)
{
    P_PKT_PROFILE_T prPktProfile = &prMsduInfo->rPktProfile;

    if(!wlanTxLifetimeIsProfilingEnabled(prAdapter))
        return;
    
    switch(eTag) {
    case TX_PROF_TAG_OS_TO_DRV:
        /* arrival time is tagged in wlanProcessTxFrame */
        break;

    case TX_PROF_TAG_DRV_ENQUE:
    	/* Reset packet profile */
    	prPktProfile->fgIsValid = FALSE;
        if(wlanTxLifetimeIsTargetMsdu(prAdapter, prMsduInfo)) {
        	/* Enable packet lifetime profiling */
        	prPktProfile->fgIsValid = TRUE;

        	/* Packet arrival time at kernel Hard Xmit */
        	prPktProfile->rHardXmitArrivalTimestamp = GLUE_GET_PKT_ARRIVAL_TIME(prMsduInfo->prPacket);

        	/* Packet enqueue time */
        	prPktProfile->rEnqueueTimestamp = (OS_SYSTIME)kalGetTimeTick();
        }
        break;

    case TX_PROF_TAG_DRV_DEQUE:
        if(prPktProfile->fgIsValid)
            prPktProfile->rDequeueTimestamp = (OS_SYSTIME)kalGetTimeTick();
        break;
        
    case TX_PROF_TAG_DRV_TX_DONE:
        if(prPktProfile->fgIsValid) {
            BOOLEAN fgPrintCurPkt = FALSE;
            
            prPktProfile->rHifTxDoneTimestamp = (OS_SYSTIME)kalGetTimeTick();
            
			if (fgPrintCurPkt)
				PRINT_PKT_PROFILE(prPktProfile, "C");
            
#if CFG_ENABLE_PER_STA_STATISTICS
            wlanTxLifetimeUpdateStaStats(prAdapter, prMsduInfo);
#endif
        }
        break;
        
    case TX_PROF_TAG_MAC_TX_DONE:
        break;
        
    default:
        break;        
    }
}

VOID
wlanTxProfilingTagPacket(IN P_ADAPTER_T prAdapter,
    IN P_NATIVE_PACKET prPacket, IN ENUM_TX_PROFILING_TAG_T eTag)
{
#if CFG_MET_PACKET_TRACE_SUPPORT
    kalMetTagPacket(prAdapter->prGlueInfo, prPacket, eTag);
#endif
}

VOID
wlanTxProfilingTagMsdu(IN P_ADAPTER_T prAdapter,
    IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_PROFILING_TAG_T eTag)
{
    wlanTxLifetimeTagPacket(prAdapter, prMsduInfo, eTag);
    
    wlanTxProfilingTagPacket(prAdapter, prMsduInfo->prPacket, eTag);
}
/* Modifed by MTK */
WLAN_STATUS
wlanDhcpTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, 
    IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
    DBGLOG(SW4, INFO, ("DHCP PKT TX DONE WIDX:PID[%u:%u] Status[%u]\n", 
        prMsduInfo->ucWlanIndex, prMsduInfo->ucPID, 
        rTxDoneStatus));

    return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanArpTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, 
    IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
    DBGLOG(SW4, INFO, ("ARP PKT TX DONE STAIDX:WIDX:PID[%u:%u:%u] Status[%u]\n",
			   prMsduInfo->ucStaRecIndex, prMsduInfo->ucWlanIndex, prMsduInfo->ucPID, rTxDoneStatus));

    return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS
wlanDnsTxDone(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo, 
    IN ENUM_TX_RESULT_CODE_T rTxDoneStatus)
{
    DBGLOG(SW4, INFO, ("DNS PKT TX DONE WIDX:PID[%u:%u] Status[%u]\n", 
        prMsduInfo->ucWlanIndex, prMsduInfo->ucPID, 
        rTxDoneStatus));

    return WLAN_STATUS_SUCCESS;
}
