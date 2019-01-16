/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/wlan_lib.h#3 $
*/

/*! \file   "wlan_lib.h"
    \brief  The declaration of the functions of the wlanAdpater objects

    Detail description.
*/



/*
** $Log: wlan_lib.h $
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
** 07 26 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** 1. Reduce extra Tx frame header parsing
** 2. Add TX port control
** 3. Add net interface to BSS binding
**
** 06 19 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** update MAC address handling logic
**
** 02 06 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** add reset option for firmware download configuration
**
** 02 01 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. eliminate MT5931/MT6620/MT6628 logic
** 2. add firmware download control sequence
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
 *
 * 06 08 2012 eason.tsai
 * NULL
 * Nvram context covert from 6620 to 6628 for old 6620 meta tool
 *
 * 01 16 2012 cp.wu
 * [MT6620 Wi-Fi][Driver] API and behavior modification for preferred band configuration with corresponding network configuration
 * add wlanSetPreferBandByNetwork() for glue layer to invoke for setting preferred band configuration corresponding to network type.
 *
 * 01 05 2012 wh.su
 * [WCXRP00001153] [MT6620 Wi-Fi][Driver] Adding the get_ch_list and set_tx_power proto type function
 * Adding the related ioctl / wlan oid function to set the Tx power cfg.
 *
 * 10 03 2011 cp.wu
 * [WCXRP00001022] [MT6628 Driver][Firmware Download] Add multi section independent download functionality
 * eliminate win32 native data types.
 *
 * 10 03 2011 cp.wu
 * [WCXRP00001022] [MT6628 Driver][Firmware Download] Add multi section independent download functionality
 * add firmware download path in divided scatters.
 *
 * 10 03 2011 cp.wu
 * [MT6628 Driver][Firmware Download] Add multi section independent download functionality
 * add firmware downloading aggregated path.
 *
 * 09 20 2011 tsaiyuan.hsu
 * [WCXRP00000931] [MT5931 Wi-Fi][DRV/FW] add swcr to disable roaming from driver
 * change window registry of driver for roaming.
 *
 * 09 08 2011 cm.chang
 * [WCXRP00000969] [MT6620 Wi-Fi][Driver][FW] Channel list for 5G band based on country code
 * Use new fields ucChannelListMap and ucChannelListIndex in NVRAM
 *
 * 08 31 2011 cm.chang
 * [WCXRP00000969] [MT6620 Wi-Fi][Driver][FW] Channel list for 5G band based on country code
 * .
 *
 * 08 25 2011 chinghwa.yu
 * [WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Add DFS switch.
 *
 * 08 24 2011 chinghwa.yu
 * [WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Update RDD test mode cases.
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
 * 05 27 2011 cp.wu
 * [WCXRP00000749] [MT6620 Wi-Fi][Driver] Add band edge tx power control to Wi-Fi NVRAM
 * invoke CMD_ID_SET_EDGE_TXPWR_LIMIT when there is valid data exist in NVRAM content.
 *
 * 05 11 2011 cp.wu
 * [WCXRP00000718] [MT6620 Wi-Fi] modify the behavior of setting tx power
 * ACPI APIs migrate to wlan_lib.c for glue layer to invoke.
 *
 * 04 18 2011 cp.wu
 * [WCXRP00000636] [WHQL][MT5931 Driver] 2c_PMHibernate (hang on 2h)
 * 1) add API for glue layer to query ACPI state
 * 2) Windows glue should not access to hardware after switched into D3 state
 *
 * 03 10 2011 cp.wu
 * [WCXRP00000532] [MT6620 Wi-Fi][Driver] Migrate NVRAM configuration procedures from MT6620 E2 to MT6620 E3
 * deprecate configuration used by MT6620 E2
 *
 * 01 27 2011 tsaiyuan.hsu
 * [WCXRP00000392] [MT6620 Wi-Fi][Driver] Add Roaming Support
 * add roaming fsm
 * 1. not support 11r, only use strength of signal to determine roaming.
 * 2. not enable CFG_SUPPORT_ROAMING until completion of full test.
 * 3. in 6620, adopt work-around to avoid sign extension problem of cck of hw
 * 4. assume that change of link quality in smooth way.
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
 * 01 10 2011 cp.wu
 * [WCXRP00000351] [MT6620 Wi-Fi][Driver] remove from scanning result in OID handling layer when the corresponding BSS is disconnected due to beacon timeout
 * remove from scanning result when the BSS is disconnected due to beacon timeout.
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
 * 10 18 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000086] [MT6620 Wi-Fi][Driver] The mac address is all zero at android
 * complete implementation of Android NVRAM access
 *
 * 10 08 2010 cp.wu
 * [WCXRP00000084] [MT6620 Wi-Fi][Driver][FW] Add fixed rate support for distance test
 * adding fixed rate support for distance test. (from registry setting)
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
 * 09 24 2010 cp.wu
 * [WCXRP00000057] [MT6620 Wi-Fi][Driver] Modify online scan to a run-time switchable feature
 * Modify online scan as a run-time adjustable option (for Windows, in registry)
 *
 * 09 23 2010 cp.wu
 * [WCXRP00000051] [MT6620 Wi-Fi][Driver] WHQL test fail in MAC address changed item
 * use firmware reported mac address right after wlanAdapterStart() as permanent address
 *
 * 09 23 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check
 * add skeleton for NVRAM integration
 *
 * 08 26 2010 yuche.tsai
 * NULL
 * Add AT GO test configure mode under WinXP.
 * Please enable 1. CFG_ENABLE_WIFI_DIRECT, 2. CFG_TEST_WIFI_DIRECT_GO, 3. CFG_SUPPORT_AAA
 *
 * 08 25 2010 george.huang
 * NULL
 * .
 *
 * 07 21 2010 cp.wu
 *
 * 1) change BG_SCAN to ONLINE_SCAN for consistent term
 * 2) only clear scanning result when scan is permitted to do
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
 * 06 24 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 802.1x and bluetooth-over-Wi-Fi security frames are now delievered to firmware via command path instead of data path.
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
 * 06 08 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * cnm_timer has been migrated.
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
 * 05 12 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add extra 64 adjustable parameters for CoEX scenario.
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code refine: fgTestMode should be at adapter rather than glue due to the device/fw is also involved
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) for some OID, never do timeout expiration
 *  * 2) add 2 kal API for later integration
 *
 * 04 01 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * change to use WIFI_TCM_ALWAYS_ON as firmware image
 *
 * 03 31 2010 wh.su
 * [WPD00003816][MT6620 Wi-Fi] Adding the security support
 * modify the wapi related code for new driver's design.
 *
 * 03 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * always send CMD_NIC_POWER_CTRL packet when nic is being halted
 *
 * 03 12 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add two option for ACK and ENCRYPTION for firmware download
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
 * 1. add logic for firmware download
 *  *  * 2. firmware image filename and start/load address are now retrieved from registry
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove unused function in nic_rx.c [which has been handled in que_mgt.c]
 *  *  *  * 2) firmware image length is now retrieved via NdisFileOpen
 *  *  *  * 3) firmware image is not structured by (P_IMG_SEC_HDR_T) anymore
 *  *  *  * 4) nicRxWaitResponse() revised
 *  *  *  * 5) another set of TQ counter default value is added for fw-download state
 *  *  *  * 6) Wi-Fi load address is now retrieved from registry too
 *
 * 02 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * prepare for implementing fw download logic
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 *  *  *  *  *  *  * 2. block TX/ordinary OID when RF test mode is engaged
 *  *  *  *  *  *  * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 *  *  *  *  *  *  * 4. correct some HAL implementation
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-12-10 16:39:55 GMT mtk02752
**  eliminate unused API
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-10-13 21:58:41 GMT mtk01084
**  update for new macro define
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-05-19 10:43:06 GMT mtk01461
**  Add wlanReleasePendingOid()
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-04-13 16:38:44 GMT mtk01084
**  add WIFI start function
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-04-08 16:51:14 GMT mtk01084
**  Update for the image download part
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-04-01 10:57:38 GMT mtk01461
**  Add wlanSendLeftClusteredFrames() for SDIO_TX_ENHANCE
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-23 00:31:02 GMT mtk01461
**  Add declaration of FW Image download reference code
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-16 09:08:31 GMT mtk01461
**  Update TX PATH API
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:12:04 GMT mtk01426
**  Init for develop
**
*/

#ifndef _WLAN_LIB_H
#define _WLAN_LIB_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "CFG_Wifi_File.h"
#include "rlm_domain.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define MAX_NUM_GROUP_ADDR                      32	/* max number of group addresses */



#define TX_CS_TCP_UDP_GEN        BIT(1)
#define TX_CS_IP_GEN             BIT(0)


#define CSUM_OFFLOAD_EN_TX_TCP      BIT(0)
#define CSUM_OFFLOAD_EN_TX_UDP      BIT(1)
#define CSUM_OFFLOAD_EN_TX_IP       BIT(2)
#define CSUM_OFFLOAD_EN_RX_TCP      BIT(3)
#define CSUM_OFFLOAD_EN_RX_UDP      BIT(4)
#define CSUM_OFFLOAD_EN_RX_IPv4     BIT(5)
#define CSUM_OFFLOAD_EN_RX_IPv6     BIT(6)
#define CSUM_OFFLOAD_EN_TX_MASK     BITS(0, 2)
#define CSUM_OFFLOAD_EN_ALL         BITS(0, 6)

/* TCP, UDP, IP Checksum */
#define RX_CS_TYPE_UDP           BIT(7)
#define RX_CS_TYPE_TCP           BIT(6)
#define RX_CS_TYPE_IPv6          BIT(5)
#define RX_CS_TYPE_IPv4          BIT(4)

#define RX_CS_STATUS_UDP         BIT(3)
#define RX_CS_STATUS_TCP         BIT(2)
#define RX_CS_STATUS_IP          BIT(0)

#define CSUM_NOT_SUPPORTED      0x0

#define TXPWR_USE_PDSLOPE 0

/* NVRAM error code definitions */
#define NVRAM_ERROR_VERSION_MISMATCH        BIT(1)
#define NVRAM_ERROR_INVALID_TXPWR           BIT(2)
#define NVRAM_ERROR_INVALID_DPD             BIT(3)
#define NVRAM_ERROR_INVALID_MAC_ADDR        BIT(4)
#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
#define NVRAM_POWER_LIMIT_TABLE_INVALID     BIT(5)
#endif

#define NUM_TC_RESOURCE_TO_STATISTICS       4

#define WLAN_CFG_ARGV_MAX 8
#define WLAN_CFG_ENTRY_NUM_MAX 128
#define WLAN_CFG_KEY_LEN_MAX 32	/* include \x00  EOL */
#define WLAN_CFG_VALUE_LEN_MAX 32	/* include \x00 EOL */
#define WLAN_CFG_FLAG_SKIP_CB BIT(0)
#define WLAN_CFG_FILE_BUF_SIZE 2048

#define WLAN_CFG_SET_CHIP_LEN_MAX 10
#define WLAN_CFG_SET_DEBUG_LEVEL_LEN_MAX 10
#define WLAN_CFG_SET_SW_CTRL_LEN_MAX 10


#define WLAN_OID_TIMEOUT_THRESHOLD                  2000	/* OID timeout (in ms) */
#define WLAN_OID_TIMEOUT_THRESHOLD_IN_RESETING      300	/* OID timeout during chip-reseting  (in ms) */

#define WLAN_OID_NO_ACK_THRESHOLD                   3

#define WLAN_TX_THREAD_TASK_PRIORITY        0	/* If not setting the priority, 0 is the default */
#define WLAN_TX_THREAD_TASK_NICE            (-10)	/* If not setting the nice, -10 is the default */
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef WLAN_STATUS(*PFN_OID_HANDLER_FUNC) (IN P_ADAPTER_T prAdapter,
					    IN PVOID pvBuf,
					    IN UINT_32 u4BufLen, OUT PUINT_32 pu4OutInfoLen);

typedef enum _ENUM_CSUM_TYPE_T {
	CSUM_TYPE_IPV4,
	CSUM_TYPE_IPV6,
	CSUM_TYPE_TCP,
	CSUM_TYPE_UDP,
	CSUM_TYPE_NUM
} ENUM_CSUM_TYPE_T, *P_ENUM_CSUM_TYPE_T;

typedef enum _ENUM_CSUM_RESULT_T {
	CSUM_RES_NONE,
	CSUM_RES_SUCCESS,
	CSUM_RES_FAILED,
	CSUM_RES_NUM
} ENUM_CSUM_RESULT_T, *P_ENUM_CSUM_RESULT_T;

typedef enum _ENUM_PHY_MODE_T {
	ENUM_PHY_2G4_CCK,
	ENUM_PHY_2G4_OFDM_BPSK,
	ENUM_PHY_2G4_OFDM_QPSK,
	ENUM_PHY_2G4_OFDM_16QAM,
	ENUM_PHY_2G4_OFDM_48M,
	ENUM_PHY_2G4_OFDM_54M,
	ENUM_PHY_2G4_HT20_BPSK,
	ENUM_PHY_2G4_HT20_QPSK,
	ENUM_PHY_2G4_HT20_16QAM,
	ENUM_PHY_2G4_HT20_MCS5,
	ENUM_PHY_2G4_HT20_MCS6,
	ENUM_PHY_2G4_HT20_MCS7,
	ENUM_PHY_2G4_HT40_BPSK,
	ENUM_PHY_2G4_HT40_QPSK,
	ENUM_PHY_2G4_HT40_16QAM,
	ENUM_PHY_2G4_HT40_MCS5,
	ENUM_PHY_2G4_HT40_MCS6,
	ENUM_PHY_2G4_HT40_MCS7,
	ENUM_PHY_5G_OFDM_BPSK,
	ENUM_PHY_5G_OFDM_QPSK,
	ENUM_PHY_5G_OFDM_16QAM,
	ENUM_PHY_5G_OFDM_48M,
	ENUM_PHY_5G_OFDM_54M,
	ENUM_PHY_5G_HT20_BPSK,
	ENUM_PHY_5G_HT20_QPSK,
	ENUM_PHY_5G_HT20_16QAM,
	ENUM_PHY_5G_HT20_MCS5,
	ENUM_PHY_5G_HT20_MCS6,
	ENUM_PHY_5G_HT20_MCS7,
	ENUM_PHY_5G_HT40_BPSK,
	ENUM_PHY_5G_HT40_QPSK,
	ENUM_PHY_5G_HT40_16QAM,
	ENUM_PHY_5G_HT40_MCS5,
	ENUM_PHY_5G_HT40_MCS6,
	ENUM_PHY_5G_HT40_MCS7,
	ENUM_PHY_MODE_NUM
} ENUM_PHY_MODE_T, *P_ENUM_PHY_MODE_T;

typedef enum _ENUM_POWER_SAVE_POLL_MODE_T {
	ENUM_POWER_SAVE_POLL_DISABLE,
	ENUM_POWER_SAVE_POLL_LEGACY_NULL,
	ENUM_POWER_SAVE_POLL_QOS_NULL,
	ENUM_POWER_SAVE_POLL_NUM
} ENUM_POWER_SAVE_POLL_MODE_T, *P_ENUM_POWER_SAVE_POLL_MODE_T;

typedef enum _ENUM_AC_TYPE_T {
	ENUM_AC_TYPE_AC0,
	ENUM_AC_TYPE_AC1,
	ENUM_AC_TYPE_AC2,
	ENUM_AC_TYPE_AC3,
	ENUM_AC_TYPE_AC4,
	ENUM_AC_TYPE_AC5,
	ENUM_AC_TYPE_AC6,
	ENUM_AC_TYPE_BMC,
	ENUM_AC_TYPE_NUM
} ENUM_AC_TYPE_T, *P_ENUM_AC_TYPE_T;

typedef enum _ENUM_ADV_AC_TYPE_T {
	ENUM_ADV_AC_TYPE_RX_NSW,
	ENUM_ADV_AC_TYPE_RX_PTA,
	ENUM_ADV_AC_TYPE_RX_SP,
	ENUM_ADV_AC_TYPE_TX_PTA,
	ENUM_ADV_AC_TYPE_TX_RSP,
	ENUM_ADV_AC_TYPE_NUM
} ENUM_ADV_AC_TYPE_T, *P_ENUM_ADV_AC_TYPE_T;

typedef enum _ENUM_REG_CH_MAP_T {
	REG_CH_MAP_COUNTRY_CODE,
	REG_CH_MAP_TBL_IDX,
	REG_CH_MAP_CUSTOMIZED,
	REG_CH_MAP_NUM
} ENUM_REG_CH_MAP_T, *P_ENUM_REG_CH_MAP_T;

typedef enum _ENUM_FEATURE_OPTION_T {
	FEATURE_DISABLED,
	FEATURE_ENABLED,
	FEATURE_FORCE_ENABLED
} ENUM_FEATURE_OPTION_T, *P_ENUM_FEATURE_OPTION_T;

/* This enum is for later added feature options which use command reserved field as option switch */
typedef enum _ENUM_FEATURE_OPTION_IN_CMD_T {
	FEATURE_OPT_CMD_AUTO,
	FEATURE_OPT_CMD_DISABLED,
	FEATURE_OPT_CMD_ENABLED,
	FEATURE_OPT_CMD_FORCE_ENABLED
} ENUM_FEATURE_OPTION_IN_CMD_T, *P_ENUM_FEATURE_OPTION_IN_CMD_T;

#define DEBUG_MSG_SIZE_MAX 1200
enum {
	DEBUG_MSG_ID_UNKNOWN = 0x00,
	DEBUG_MSG_ID_PRINT = 0x01,
	DEBUG_MSG_ID_FWLOG = 0x02,
	DEBUG_MSG_ID_END
};

enum {
	DEBUG_MSG_TYPE_UNKNOWN = 0x00,
	DEBUG_MSG_TYPE_MEM8 = 0x01,
	DEBUG_MSG_TYPE_MEM32 = 0x02,
	DEBUG_MSG_TYPE_ASCII = 0x03,
	DEBUG_MSG_TYPE_BINARY = 0x04,
	DEBUG_MSG_TYPE_END
};

#define CHIP_CONFIG_RESP_SIZE 320
enum {
	CHIP_CONFIG_TYPE_WO_RESPONSE = 0x00,
	CHIP_CONFIG_TYPE_MEM8 = 0x01,
	CHIP_CONFIG_TYPE_MEM32 = 0x02,
	CHIP_CONFIG_TYPE_ASCII = 0x03,
	CHIP_CONFIG_TYPE_BINARY = 0x04,
	CHIP_CONFIG_TYPE_DRV_PASSTHROUGH = 0x05,
	CHIP_CONFIG_TYPE_END
};

typedef struct _SET_TXPWR_CTRL_T {
	INT_8 c2GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c2GHotspotPwrOffset;
	INT_8 c2GP2pPwrOffset;
	INT_8 c2GBowPwrOffset;
	INT_8 c5GLegacyStaPwrOffset;	/* Unit: 0.5dBm, default: 0 */
	INT_8 c5GHotspotPwrOffset;
	INT_8 c5GP2pPwrOffset;
	INT_8 c5GBowPwrOffset;
	UINT_8 ucConcurrencePolicy;	/* TX power policy when concurrence
					   in the same channel
					   0: Highest power has priority
					   1: Lowest power has priority */
	INT_8 acReserved1[3];	/* Must be zero */

	/* Power limit by channel for all data rates */
	INT_8 acTxPwrLimit2G[14];	/* Channel 1~14, Unit: 0.5dBm */
	INT_8 acTxPwrLimit5G[4];	/* UNII 1~4 */
	INT_8 acReserved2[2];	/* Must be zero */
} SET_TXPWR_CTRL_T, *P_SET_TXPWR_CTRL_T;

typedef enum _ENUM_NVRAM_MTK_FEATURE_T {
  MTK_FEATURE_2G_256QAM_DISABLED = 0,
  MTK_FEATURE_NUM
} ENUM_NVRAM_MTK_FEATURES_T, *P_ENUM_NVRAM_MTK_FEATURES_T;

/* For storing driver initialization value from glue layer */
typedef struct _REG_INFO_T {
	UINT_32 u4SdBlockSize;	/* SDIO block size */
	UINT_32 u4SdBusWidth;	/* SDIO bus width. 1 or 4 */
	UINT_32 u4SdClockRate;	/* SDIO clock rate. (in unit of HZ) */
	UINT_32 u4StartAddress;	/* Starting address of Wi-Fi Firmware */
	UINT_32 u4LoadAddress;	/* Load address of Wi-Fi Firmware */
	UINT_16 aucFwImgFilename[65];	/* Firmware filename */
	UINT_16 aucFwImgFilenameE6[65];	/* Firmware filename for E6 */
	UINT_32 u4StartFreq;	/* Start Frequency for Ad-Hoc network : in unit of KHz */
	UINT_32 u4AdhocMode;	/* Default mode for Ad-Hoc network : ENUM_PARAM_AD_HOC_MODE_T */
	UINT_32 u4RddStartFreq;
	UINT_32 u4RddStopFreq;
	UINT_32 u4RddTestMode;
	UINT_32 u4RddShutFreq;
	UINT_32 u4RddDfs;
	INT_32 i4HighRssiThreshold;
	INT_32 i4MediumRssiThreshold;
	INT_32 i4LowRssiThreshold;
	INT_32 au4TxPriorityTag[ENUM_AC_TYPE_NUM];
	INT_32 au4RxPriorityTag[ENUM_AC_TYPE_NUM];
	INT_32 au4AdvPriorityTag[ENUM_ADV_AC_TYPE_NUM];
	UINT_32 u4FastPSPoll;
	UINT_32 u4PTA;		/* 0: disable, 1: enable */
	UINT_32 u4TXLimit;	/* 0: disable, 1: enable */
	UINT_32 u4SilenceWindow;	/* range: 100 - 625, unit: us */
	UINT_32 u4TXLimitThreshold;	/* range: 250 - 1250, unit: us */
	UINT_32 u4PowerMode;
	UINT_32 fgEnArpFilter;
	UINT_32 u4PsCurrentMeasureEn;
	UINT_32 u4UapsdAcBmp;
	UINT_32 u4MaxSpLen;
	UINT_32 fgDisOnlineScan;	/* 0: enable online scan, non-zero: disable online scan */
	UINT_32 fgDisBcnLostDetection;	/* 0: enable online scan, non-zero: disable online scan */
	UINT_32 u4FixedRate;	/* 0: automatic, non-zero: fixed rate */
	UINT_32 u4ArSysParam0;
	UINT_32 u4ArSysParam1;
	UINT_32 u4ArSysParam2;
	UINT_32 u4ArSysParam3;
	UINT_32 fgDisRoaming;	/* 0:enable roaming 1:disable */

	/* NVRAM - MP Data -START- */
#if 1
	UINT_16 u2Part1OwnVersion;
	UINT_16 u2Part1PeerVersion;
#endif
	UINT_8 aucMacAddr[6];
	UINT_16 au2CountryCode[4];	/* Country code (in ISO 3166-1 expression, ex: "US", "TW")  */
	TX_PWR_PARAM_T rTxPwr;
	UINT_8 aucEFUSE[144];
	UINT_8 ucTxPwrValid;
	UINT_8 ucSupport5GBand;
	UINT_8 fg2G4BandEdgePwrUsed;
	INT_8 cBandEdgeMaxPwrCCK;
	INT_8 cBandEdgeMaxPwrOFDM20;
	INT_8 cBandEdgeMaxPwrOFDM40;
	ENUM_REG_CH_MAP_T eRegChannelListMap;
	UINT_8 ucRegChannelListIndex;
	DOMAIN_INFO_ENTRY rDomainInfo;
	RSSI_PATH_COMPASATION_T rRssiPathCompasation;
	UINT_8 ucRssiPathCompasationUsed;
	/* NVRAM - MP Data -END- */

	/* NVRAM - Functional Data -START- */
	UINT_8 uc2G4BwFixed20M;
	UINT_8 uc5GBwFixed20M;
	UINT_8 ucEnable5GBand;
	UINT_8 ucGpsDesense;
	UINT_8 ucRxDiversity;
	/* NVRAM - Functional Data -END- */

	P_NEW_EFUSE_MAPPING2NVRAM_T prOldEfuseMapping;

	UINT_8 aucNvram[512];
	P_WIFI_CFG_PARAM_STRUCT prNvramSettings;

} REG_INFO_T, *P_REG_INFO_T;

/* for divided firmware loading */
typedef struct _FWDL_SECTION_INFO_T {
	UINT_32 u4Offset;
	UINT_32 u4Reserved;
	UINT_32 u4Length;
	UINT_32 u4DestAddr;
} FWDL_SECTION_INFO_T, *P_FWDL_SECTION_INFO_T;

typedef struct _FIRMWARE_DIVIDED_DOWNLOAD_T {
	UINT_32 u4Signature;
	UINT_32 u4CRC;		/* CRC calculated without first 8 bytes included */
	UINT_32 u4NumOfEntries;
	UINT_32 u4Reserved;
	FWDL_SECTION_INFO_T arSection[];
} FIRMWARE_DIVIDED_DOWNLOAD_T, *P_FIRMWARE_DIVIDED_DOWNLOAD_T;

typedef struct _PARAM_MCR_RW_STRUC_T {
	UINT_32 u4McrOffset;
	UINT_32 u4McrData;
} PARAM_MCR_RW_STRUC_T, *P_PARAM_MCR_RW_STRUC_T;

typedef struct _PARAM_GET_STA_STATISTICS {
	/* Per-STA statistic */
	UINT_8 aucMacAddr[MAC_ADDR_LEN];

	UINT_32 u4Flag;

	UINT_8 ucReadClear;

	/* From driver */
	UINT_32 u4TxTotalCount;
	UINT_32 u4TxExceedThresholdCount;

	UINT_32 u4TxMaxTime;
	UINT_32 u4TxAverageProcessTime;

	UINT_32 u4RxTotalCount;

	UINT_32 au4TcResourceEmptyCount[NUM_TC_RESOURCE_TO_STATISTICS];
	UINT_32 au4TcQueLen[NUM_TC_RESOURCE_TO_STATISTICS];

	/* From FW */
	UINT_8 ucPer;		/* base: 128 */
	UINT_8 ucRcpi;
	UINT_32 u4PhyMode;
	UINT_16 u2LinkSpeed;	/* unit is 0.5 Mbits */

	UINT_32 u4TxFailCount;
	UINT_32 u4TxLifeTimeoutCount;

	UINT_32 u4TxAverageAirTime;
	UINT_32 u4TransmitCount;	/* Transmit in the air (wtbl) */
	UINT_32 u4TransmitFailCount;	/* Transmit without ack/ba in the air (wtbl) */

	/* Global queue management statistic */
	UINT_32 au4TcAverageQueLen[NUM_TC_RESOURCE_TO_STATISTICS];
	UINT_32 au4TcCurrentQueLen[NUM_TC_RESOURCE_TO_STATISTICS];

	/* Reserved fields */
	UINT_8 au4Reserved[32];
} PARAM_GET_STA_STA_STATISTICS, *P_PARAM_GET_STA_STATISTICS;


typedef struct _NET_INTERFACE_INFO_T {
	UINT_8 ucBssIndex;
	PVOID pvNetInterface;
} NET_INTERFACE_INFO_T, *P_NET_INTERFACE_INFO_T;
#if 0
typedef struct _SEC_FRAME_INFO_T {
	BOOLEAN fgIsProtected;
#if CFG_SUPPORT_MULTITHREAD
	/* Compose TxDesc in tx_thread and place here */
	UINT_8 ucTxDescBuffer[DWORD_TO_BYTE(7)];
#endif
} SEC_FRAME_INFO_T, *P_SEC_FRAME_INFO_T;
#endif

typedef enum _ENUM_TX_RESULT_CODE_T {
	TX_RESULT_SUCCESS = 0,
	TX_RESULT_LIFE_TIMEOUT,
	TX_RESULT_RTS_ERROR,
	TX_RESULT_MPDU_ERROR,
	TX_RESULT_AGING_TIMEOUT,
	TX_RESULT_FLUSHED,
	TX_RESULT_BIP_ERROR,
	TX_RESULT_UNSPECIFIED_ERROR,
	TX_RESULT_DROPPED_IN_DRIVER = 32,
	TX_RESULT_DROPPED_IN_FW,
	TX_RESULT_QUEUE_CLEARANCE,
	TX_RESULT_NUM
} ENUM_TX_RESULT_CODE_T, *P_ENUM_TX_RESULT_CODE_T;

/* enum of BEACON_TIMEOUT_REASON */
typedef enum _ENUM_PM_BEACON_TIME_OUT_REACON_CODE_T {
	BEACON_TIMEOUT_DUE_2_HW_BEACON_LOST_NONADHOC,
	BEACON_TIMEOUT_DUE_2_HW_BEACON_LOST_ADHOC,
	BEACON_TIMEOUT_DUE_2_HW_TSF_DRIFT,
	BEACON_TIMEOUT_DUE_2_NULL_FRAME_THRESHOLD,
	BEACON_TIMEOUT_DUE_2_AGING_THRESHOLD,
	BEACON_TIMEOUT_DUE_2_BSSID_BEACON_PEIROD_NOT_ILLIGAL,
	BEACON_TIMEOUT_DUE_2_CONNECTION_FAIL,
	BEACON_TIMEOUT_DUE_2_ALLOCAT_NULL_PKT_FAIL_THRESHOLD,
	BEACON_TIMEOUT_DUE_2_NO_TX_DONE_EVENT,
	BEACON_TIMEOUT_DUE_2_UNSPECIF_REASON,
	BEACON_TIMEOUT_DUE_2_SET_CHIP,
	BEACON_TIMEOUT_DUE_2_KEEP_SCAN_AP_MISS_CHECK_FAIL,
	BEACON_TIMEOUT_DUE_2_KEEP_UNCHANGED_LOW_RSSI_CHECK_FAIL,
	BEACON_TIMEOUT_DUE_2_NULL_FRAME_LIFE_TIMEOUT,
	BEACON_TIMEOUT_DUE_2_APR_NO_RESPONSE,
	BEACON_TIMEOUT_DUE_2_NUM
}ENUM_PM_BEACON_TIME_OUT_REACON_CODE_T, *P_ENUM_PM_BEACON_TIME_OUT_REACON_CODE_T;

struct _WLAN_CFG_ENTRY_T {
	UINT_8 aucKey[WLAN_CFG_KEY_LEN_MAX];
	UINT_8 aucValue[WLAN_CFG_VALUE_LEN_MAX];
	WLAN_CFG_SET_CB pfSetCb;
	PVOID pPrivate;
	UINT_32 u4Flags;
};

struct _WLAN_CFG_T {
	UINT_32 u4WlanCfgEntryNumMax;
	UINT_32 u4WlanCfgKeyLenMax;
	UINT_32 u4WlanCfgValueLenMax;
	WLAN_CFG_ENTRY_T arWlanCfgBuf[WLAN_CFG_ENTRY_NUM_MAX];
};

typedef enum _ENUM_MAX_BANDWIDTH_SETTING_T {
	MAX_BW_20MHZ = 0,
	MAX_BW_40MHZ,
	MAX_BW_80MHZ,
	MAX_BW_160MHZ,
	MAX_BW_80_80_MHZ
} ENUM_MAX_BANDWIDTH_SETTING, *P_ENUM_MAX_BANDWIDTH_SETTING_T;
#if CFG_AUTO_CHANNEL_SEL_SUPPORT

typedef struct _LTE_SAFE_CH_INFO_T {
    	UINT_32 u4SafeChannelBitmask[4];
} LTE_SAFE_CH_INFO_T, *P_CMD_LTE_SAFE_CH_INFO_T;

/* Record Each CH Load */
typedef struct _PARAM_CHN_LOAD_INFO {
	/* Per-CHN Load */
	UINT_32 u4Flag;
	UINT_8 ucChannel;
	UINT_16 u2ChannelLoad;
	UINT_8 au4Reserved0[1];
	UINT_16 u2APNum;
	UINT_16 u2APNumTmpCountingBuf;
	/* Reserved fields */
	UINT_8 au4Reserved1[8];
} PARAM_CHN_LOAD_INFO, *P_PARAM_CHN_LOAD_INFO;

typedef struct _PARAM_GET_CHN_LOAD {
	LTE_SAFE_CH_INFO_T rLteSafeChnList;
	PARAM_CHN_LOAD_INFO rEachChnLoad[MAX_AUTO_CHAL_NUM];
	BOOLEAN fgDataReadyBit;
	UINT_8 au4Reserved[3];
} PARAM_GET_CHN_LOAD, *P_PARAM_GET_CHN_LOAD;

typedef struct _PARAM_PREFER_CHN_INFO {
	UINT_8 ucChannel;
	UINT_16 u2APNum;
	UINT_8 au4Reserved[1];
} PARAM_PREFER_CHN_INFO, *P_PARAM_PREFER_CHN_INFO;


typedef struct _PARAM_GET_LTE_MODE {
	/* Event Body */
	UINT_8 ucVersion;
	UINT_8 aucReserved0[3];
	UINT_32 u4Flags;	/* Bit0: valid */
	LTE_SAFE_CH_INFO_T LTE_MODE;
	UINT_8 aucReserved1[4];
	UINT_8 aucReserved2[4];
} PARAM_GET_LTE_MODE, *P_PARAM_GET_LTE_MODE;

#endif

typedef struct _TX_PACKET_INFO {
    UINT_8 ucPriorityParam;
    UINT_32 u4PacketLen;
    UINT_8 aucEthDestAddr[MAC_ADDR_LEN];
    UINT_16 u2Flag;
} TX_PACKET_INFO, *P_TX_PACKET_INFO;

typedef enum _ENUM_TX_PROFILING_TAG_T {
    TX_PROF_TAG_OS_TO_DRV = 0,
    TX_PROF_TAG_DRV_ENQUE,
    TX_PROF_TAG_DRV_DEQUE,
    TX_PROF_TAG_DRV_TX_DONE,
    TX_PROF_TAG_MAC_TX_DONE
} ENUM_TX_PROFILING_TAG_T, *P_ENUM_TX_PROFILING_TAG_T;

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
#define BUILD_SIGN(ch0, ch1, ch2, ch3) \
    ((UINT_32)(UINT_8)(ch0) | ((UINT_32)(UINT_8)(ch1) << 8) |   \
     ((UINT_32)(UINT_8)(ch2) << 16) | ((UINT_32)(UINT_8)(ch3) << 24))

#define MTK_WIFI_SIGNATURE BUILD_SIGN('M', 'T', 'K', 'W')

#define IS_FEATURE_ENABLED(_ucFeature) \
    (((_ucFeature) == FEATURE_ENABLED) || ((_ucFeature) == FEATURE_FORCE_ENABLED))
#define IS_FEATURE_FORCE_ENABLED(_ucFeature) ((_ucFeature) == FEATURE_FORCE_ENABLED)
#define IS_FEATURE_DISABLED(_ucFeature) ((_ucFeature) == FEATURE_DISABLED)

/* This macro is for later added feature options which use command reserved field as option switch */
/* 0: AUTO
 * 1: Disabled
 * 2: Enabled
 * 3: Force disabled
 */
#define FEATURE_OPT_IN_COMMAND(_ucFeature) ((_ucFeature) + 1)


/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

P_ADAPTER_T wlanAdapterCreate(IN P_GLUE_INFO_T prGlueInfo);

VOID wlanAdapterDestroy(IN P_ADAPTER_T prAdapter);

VOID wlanCardEjected(IN P_ADAPTER_T prAdapter);

VOID wlanIST(IN P_ADAPTER_T prAdapter);

BOOL wlanISR(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgGlobalIntrCtrl);

WLAN_STATUS wlanProcessCommandQueue(IN P_ADAPTER_T prAdapter, IN P_QUE_T prCmdQue);

WLAN_STATUS wlanSendCommand(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

#if CFG_SUPPORT_MULTITHREAD
WLAN_STATUS wlanSendCommandMthread(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

WLAN_STATUS wlanTxCmdMthread(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanTxCmdDoneMthread(IN P_ADAPTER_T prAdapter);

VOID wlanClearTxCommandQueue(IN P_ADAPTER_T prAdapter);

VOID wlanClearTxCommandDoneQueue(IN P_ADAPTER_T prAdapter);

VOID wlanClearDataQueue(IN P_ADAPTER_T prAdapter);

VOID wlanClearRxToOsQueue(IN P_ADAPTER_T prAdapter);
#endif

VOID
wlanReleaseCommand(IN P_ADAPTER_T prAdapter,
		   IN P_CMD_INFO_T prCmdInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

VOID wlanReleasePendingOid(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);

VOID wlanReleasePendingCMDbyBssIdx(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

VOID wlanReturnPacketDelaySetupTimeout(IN P_ADAPTER_T prAdapter, IN ULONG ulParamPtr);

VOID wlanReturnPacket(IN P_ADAPTER_T prAdapter, IN PVOID pvPacket);

WLAN_STATUS
wlanQueryInformation(IN P_ADAPTER_T prAdapter,
		     IN PFN_OID_HANDLER_FUNC pfOidQryHandler,
		     IN PVOID pvInfoBuf, IN UINT_32 u4InfoBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
wlanSetInformation(IN P_ADAPTER_T prAdapter,
		   IN PFN_OID_HANDLER_FUNC pfOidSetHandler,
		   IN PVOID pvInfoBuf, IN UINT_32 u4InfoBufLen, OUT PUINT_32 pu4SetInfoLen);


WLAN_STATUS
wlanAdapterStart(IN P_ADAPTER_T prAdapter,
		 IN P_REG_INFO_T prRegInfo,
		 IN PVOID pvFwImageMapFile, IN UINT_32 u4FwImageFileLength);

WLAN_STATUS wlanAdapterStop(IN P_ADAPTER_T prAdapter);

#if CFG_SUPPORT_WAPI
BOOLEAN wlanQueryWapiMode(IN P_ADAPTER_T prAdapter);
#endif

VOID wlanReturnRxPacket(IN PVOID pvAdapter, IN PVOID pvPacket);

VOID wlanRxSetBroadcast(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnableBroadcast);

BOOLEAN wlanIsHandlerNeedHwAccess(IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN BOOLEAN fgSetInfo);

VOID wlanSetPromiscuousMode(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnablePromiscuousMode);

#if CFG_ENABLE_FW_DOWNLOAD
WLAN_STATUS
wlanImageSectionConfig(IN P_ADAPTER_T prAdapter,
		       IN UINT_32 u4DestAddr, IN UINT_32 u4ImgSecSize, IN BOOLEAN fgReset);

WLAN_STATUS
wlanImageSectionDownload(IN P_ADAPTER_T prAdapter,
			 IN UINT_32 u4ImgSecSize, IN PUINT_8 pucImgSecBuf);

VOID
wlanFwDvdDwnloadHandler(IN P_ADAPTER_T prAdapter,
		IN P_FIRMWARE_DIVIDED_DOWNLOAD_T prFwHead, IN PVOID pvFwImageMapFile, OUT WLAN_STATUS *u4Status);

VOID
wlanFwDwnloadHandler(IN P_ADAPTER_T prAdapter,
	IN UINT_32 u4FwImgLength, IN PVOID pvFwImageMapFile, OUT WLAN_STATUS *u4Status);



#if !CFG_ENABLE_FW_DOWNLOAD_ACK
WLAN_STATUS wlanImageQueryStatus(IN P_ADAPTER_T prAdapter);
#else
WLAN_STATUS wlanImageSectionDownloadStatus(IN P_ADAPTER_T prAdapter, IN UINT_8 ucCmdSeqNum);
#endif

WLAN_STATUS
wlanConfigWifiFunc(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgEnable, IN UINT_32 u4StartAddress);

UINT_32 wlanCRC32(PUINT_8 buf, UINT_32 len);

#endif

WLAN_STATUS wlanSendNicPowerCtrlCmd(IN P_ADAPTER_T prAdapter, IN UINT_8 ucPowerMode);

BOOLEAN wlanIsHandlerAllowedInRFTest(IN PFN_OID_HANDLER_FUNC pfnOidHandler, IN BOOLEAN fgSetInfo);

WLAN_STATUS wlanProcessQueuedSwRfb(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfbListHead);

WLAN_STATUS
wlanProcessQueuedMsduInfo(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfoListHead);

BOOLEAN wlanoidTimeoutCheck(IN P_ADAPTER_T prAdapter, IN PFN_OID_HANDLER_FUNC pfnOidHandler);

VOID wlanoidClearTimeoutCheck(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanUpdateNetworkAddress(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanUpdateBasicConfig(IN P_ADAPTER_T prAdapter);

BOOLEAN wlanQueryTestMode(IN P_ADAPTER_T prAdapter);

BOOLEAN wlanProcessTxFrame(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket);

/* Security Frame Handling */
BOOLEAN wlanProcessSecurityFrame(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prPacket);

VOID
wlanSecurityFrameTxDone(IN P_ADAPTER_T prAdapter,
			IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf);

VOID wlanSecurityFrameTxTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo);

/*----------------------------------------------------------------------------*/
/* OID/IOCTL Handling                                                         */
/*----------------------------------------------------------------------------*/
VOID wlanClearScanningResult(IN P_ADAPTER_T prAdapter);

VOID wlanClearBssInScanningResult(IN P_ADAPTER_T prAdapter, IN PUINT_8 arBSSID);

#if CFG_TEST_WIFI_DIRECT_GO
VOID wlanEnableP2pFunction(IN P_ADAPTER_T prAdapter);

VOID wlanEnableATGO(IN P_ADAPTER_T prAdapter);
#endif

/*----------------------------------------------------------------------------*/
/* NIC Capability Retrieve by Polling                                         */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanQueryNicCapability(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* PD MCR Retrieve by Polling                                                 */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanQueryPdMcr(IN P_ADAPTER_T prAdapter, IN P_PARAM_MCR_RW_STRUC_T prMcrRdInfo);

/*----------------------------------------------------------------------------*/
/* Loading Manufacture Data                                                   */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanLoadManufactureData(IN P_ADAPTER_T prAdapter, IN P_REG_INFO_T prRegInfo);


/*----------------------------------------------------------------------------*/
/* Media Stream Mode                                                          */
/*----------------------------------------------------------------------------*/
BOOLEAN wlanResetMediaStreamMode(IN P_ADAPTER_T prAdapter);


/*----------------------------------------------------------------------------*/
/* Timer Timeout Check (for Glue Layer)                                       */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanTimerTimeoutCheck(IN P_ADAPTER_T prAdapter);


/*----------------------------------------------------------------------------*/
/* Mailbox Message Check (for Glue Layer)                                     */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanProcessMboxMessage(IN P_ADAPTER_T prAdapter);


/*----------------------------------------------------------------------------*/
/* TX Pending Packets Handling (for Glue Layer)                               */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanEnqueueTxPacket(IN P_ADAPTER_T prAdapter, IN P_NATIVE_PACKET prNativePacket);

WLAN_STATUS wlanFlushTxPendingPackets(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanTxPendingPackets(IN P_ADAPTER_T prAdapter, IN OUT PBOOLEAN pfgHwAccess);


/*----------------------------------------------------------------------------*/
/* Low Power Acquire/Release (for Glue Layer)                                 */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanAcquirePowerControl(IN P_ADAPTER_T prAdapter);

WLAN_STATUS wlanReleasePowerControl(IN P_ADAPTER_T prAdapter);


/*----------------------------------------------------------------------------*/
/* Pending Packets Number Reporting (for Glue Layer)                          */
/*----------------------------------------------------------------------------*/
UINT_32 wlanGetTxPendingFrameCount(IN P_ADAPTER_T prAdapter);


/*----------------------------------------------------------------------------*/
/* ACPI state inquiry (for Glue Layer)                                        */
/*----------------------------------------------------------------------------*/
ENUM_ACPI_STATE_T wlanGetAcpiState(IN P_ADAPTER_T prAdapter);

VOID wlanSetAcpiState(IN P_ADAPTER_T prAdapter, IN ENUM_ACPI_STATE_T ePowerState);

VOID wlanDefTxPowerCfg(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* get ECO version from Revision ID register (for Win32)                      */
/*----------------------------------------------------------------------------*/
UINT_8 wlanGetEcoVersion(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* set preferred band configuration corresponding to network type             */
/*----------------------------------------------------------------------------*/
VOID
wlanSetPreferBandByNetwork(IN P_ADAPTER_T prAdapter, IN ENUM_BAND_T eBand, IN UINT_8 ucBssIndex);

/*----------------------------------------------------------------------------*/
/* get currently operating channel information                                */
/*----------------------------------------------------------------------------*/
UINT_8 wlanGetChannelNumberByNetwork(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex);

/*----------------------------------------------------------------------------*/
/* check for system configuration to generate message on scan list            */
/*----------------------------------------------------------------------------*/
WLAN_STATUS wlanCheckSystemConfiguration(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* query sta statistics information from driver and firmware                  */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
wlanoidQueryStaStatistics(IN P_ADAPTER_T prAdapter,
			  IN PVOID pvQueryBuffer,
			  IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

/*----------------------------------------------------------------------------*/
/* query NIC resource information from chip and reset Tx resource for normal operation        */
/*----------------------------------------------------------------------------*/
VOID wlanQueryNicResourceInformation(IN P_ADAPTER_T prAdapter);

/*----------------------------------------------------------------------------*/
/* GET/SET BSS index mapping for network interfaces                                                    */
/*----------------------------------------------------------------------------*/
VOID
wlanBindNetInterface(IN P_GLUE_INFO_T prGlueInfo,
		     IN UINT_8 ucNetInterfaceIndex, IN PVOID pvNetInterface);

VOID
wlanBindBssIdxToNetInterface(IN P_GLUE_INFO_T prGlueInfo,
			     IN UINT_8 ucBssIndex, IN PVOID pvNetInterface);

UINT_8 wlanGetBssIdxByNetInterface(IN P_GLUE_INFO_T prGlueInfo, IN PVOID pvNetInterface);

PVOID wlanGetNetInterfaceByBssIdx(IN P_GLUE_INFO_T prGlueInfo, IN UINT_8 ucBssIndex);

/* for windows as windows glue cannot see through P_ADAPTER_T */
UINT_8 wlanGetAisBssIndex(IN P_ADAPTER_T prAdapter);

VOID wlanInitFeatureOption(IN P_ADAPTER_T prAdapter);

VOID wlanCfgSetSwCtrl(IN P_ADAPTER_T prAdapter);

VOID wlanCfgSetChip(IN P_ADAPTER_T prAdapter);
    
VOID wlanCfgSetDebugLevel(IN P_ADAPTER_T prAdapter);

VOID wlanCfgSetCountryCode(IN P_ADAPTER_T prAdapter);

P_WLAN_CFG_ENTRY_T wlanCfgGetEntry(IN P_ADAPTER_T prAdapter, const PCHAR pucKey);

WLAN_STATUS
wlanCfgGet(IN P_ADAPTER_T prAdapter,
	   const PCHAR pucKey, PCHAR pucValue, PCHAR pucValueDef, UINT_32 u4Flags);

UINT_32 wlanCfgGetUint32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, UINT_32 u4ValueDef);

INT_32 wlanCfgGetInt32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, INT_32 i4ValueDef);

WLAN_STATUS wlanCfgSetUint32(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, UINT_32 u4Value);

WLAN_STATUS
wlanCfgSet(IN P_ADAPTER_T prAdapter, const PCHAR pucKey, PCHAR pucValue, UINT_32 u4Flags);

WLAN_STATUS
wlanCfgSetCb(IN P_ADAPTER_T prAdapter,
	     const PCHAR pucKey, WLAN_CFG_SET_CB pfSetCb, void *pPrivate, UINT_32 u4Flags);

WLAN_STATUS
wlanCfgInit(IN P_ADAPTER_T prAdapter,
	    PUINT_8 pucConfigBuf, UINT_32 u4ConfigBufLen, UINT_32 u4Flags);

WLAN_STATUS wlanCfgParseArgument(CHAR *cmdLine, INT_32 *argc, CHAR *argv[]
    );


INT_32 wlanHexToNum(CHAR c);
INT_32 wlanHexToByte(PCHAR hex);

INT_32 wlanHwAddrToBin(PCHAR txt, UINT_8 *addr);

BOOLEAN wlanIsChipNoAck(IN P_ADAPTER_T prAdapter);

VOID wlanTxProfilingTagPacket(IN P_ADAPTER_T prAdapter,
    IN P_NATIVE_PACKET prPacket, IN ENUM_TX_PROFILING_TAG_T eTag);

VOID wlanTxProfilingTagMsdu(IN P_ADAPTER_T prAdapter,
    IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_PROFILING_TAG_T eTag);
WLAN_STATUS wlanDhcpTxDone(IN P_ADAPTER_T prAdapter, 
    IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS wlanArpTxDone(IN P_ADAPTER_T prAdapter, 
    IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);

WLAN_STATUS wlanDnsTxDone(IN P_ADAPTER_T prAdapter, 
	IN P_MSDU_INFO_T prMsduInfo, IN ENUM_TX_RESULT_CODE_T rTxDoneStatus);
#endif				/* _WLAN_LIB_H */
