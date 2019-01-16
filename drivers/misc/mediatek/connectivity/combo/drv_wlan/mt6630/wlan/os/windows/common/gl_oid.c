/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/common/gl_oid.c#1 $
*/

/*! \file   gl_oid.c
    \brief  Main routines for Windows driver IOCTRL

*/



/*
** $Log: gl_oid.c $
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
** 07 31 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** set default firmware file as WIFI_RAM_CODE_MT6630
**
** 02 01 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. eliminate MT5931/MT6620/MT6628 logic
** 2. add firmware download control sequence
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 04 27 2012 cp.wu
 * [WCXRP00001223] [MT5931][Driver] Add TX power control field for Win32 driver
 * read power control fields as integer instead of binary.
 *
 * 04 18 2012 cp.wu
 * [WCXRP00001223] [MT5931][Driver] Add TX power control field for Win32 driver
 * [Win32 Driver] add control fields for 2.4GHz TX power control.
 *
 * 11 02 2011 chinghwa.yu
 * [WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Add RDD certification features.
 *
 * 09 20 2011 tsaiyuan.hsu
 * [WCXRP00000931] [MT5931 Wi-Fi][DRV/FW] add swcr to disable roaming from driver
 * change window registry of driver for roaming.
 *
 * 08 25 2011 chinghwa.yu
 * [WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Add DFS switch.
 *
 * 08 25 2011 chinghwa.yu
 * [WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Update DFS certification case.
 *
 * 08 24 2011 chinghwa.yu
 * [WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Update RDD test mode cases.
 *
 * 08 15 2011 cp.wu
 * [WCXRP00000913] [MT6620 Wi-Fi] create repository of source code dedicated for MT6620 E6 ASIC
 * support to load different firmware image for E3/E4/E5 and E6 ASIC on win32 platforms.
 *
 * 07 18 2011 chinghwa.yu
 * [WCXRP00000063] Update BCM CoEx design and settings[WCXRP00000612] [MT6620 Wi-Fi] [FW] CSD update SWRDD algorithm
 * Add CMD/Event for RDD and BWCS.
 *
 * 03 10 2011 cp.wu
 * [WCXRP00000532] [MT6620 Wi-Fi][Driver] Migrate NVRAM configuration procedures from MT6620 E2 to MT6620 E3
 * deprecate configuration used by MT6620 E2
 *
 * 03 05 2011 wh.su
 * [WCXRP00000506] [MT6620 Wi-Fi][Driver][FW] Add Security check related code
 * add the code to get the check rsponse and indicate to app.
 *
 * 03 02 2011 george.huang
 * [WCXRP00000504] [MT6620 Wi-Fi][FW] Support Sigma CAPI for power saving related command
 * Support UAPSD/OppPS/NoA parameter setting
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
 * 01 20 2011 eddie.chen
 * [WCXRP00000374] [MT6620 Wi-Fi][DRV] SW debug control
 * Adjust OID order.
 *
 * 01 20 2011 eddie.chen
 * [WCXRP00000374] [MT6620 Wi-Fi][DRV] SW debug control
 * Add Oid for sw control debug command
 *
 * 12 13 2010 cp.wu
 * [WCXRP00000259] [MT6620 Wi-Fi][Driver] Disable NVRAM OID path on Win32 platform
 * disable NVRAM/configuration type query OID on Win32 platform due to win32 always refer to registry.
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
 * 10 04 2010 cp.wu
 * [WCXRP00000080] [MT6620 Wi-Fi][Driver] Use signed 8-bit integer properly for TX power control on Windows platform
 * aware field size when loading default value of registry information
 *
 * 10 04 2010 cp.wu
 * [WCXRP00000080] [MT6620 Wi-Fi][Driver] Use signed 8-bit integer properly for TX power control on Windows platform
 * (Windows) use binary data for TX power control which ranges from -128 to 127.
 *
 * 09 24 2010 cp.wu
 * [WCXRP00000057] [MT6620 Wi-Fi][Driver] Modify online scan to a run-time switchable feature
 * Modify online scan as a run-time adjustable option (for Windows, in registry)
 *
 * 09 23 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check
 * add skeleton for NVRAM integration
 *
 * 09 16 2010 cp.wu
 * NULL
 * NDIS thought NDIS_STATUS_NOT_ACCEPTED is from a corrupted driver,
 * so use NDIS_STATUS_NOT_SUPPORTED instead
 *
 * 08 29 2010 yuche.tsai
 * NULL
 * Finish SLT TX/RX & Rate Changing Support.
 *
 * 08 25 2010 george.huang
 * NULL
 * update OID/ registry control path for PM related settings
 *
 * 08 04 2010 cp.wu
 * NULL
 * revert changelist #15371, efuse read/write access will be done by RF test approach
 *
 * 08 04 2010 cp.wu
 * NULL
 * add OID definitions for EFUSE read/write access.
 *
 * 08 02 2010 george.huang
 * NULL
 * add WMM-PS test related OID/ CMD handlers
 *
 * 07 19 2010 yuche.tsai
 *
 * Add for SLT support.
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 05 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * TCP/IP checksum offloading setting is handled with GLUE_EXTENSION.
 *
 * 05 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) when stopping adapter, wait til RDY bit has been cleaerd.
 * 2) set TASK_OFFLOAD as driver-core OIDs
 *
 * 05 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) when acquiring LP-own, write for clr-own with lower frequency compared to read poll
 * 2) correct address list parsing
 *
 * 05 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * some OIDs should be DRIVER_CORE instead of GLUE_EXTENSION
 *
 * 05 20 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) integrate OID_GEN_NETWORK_LAYER_ADDRESSES with CMD_ID_SET_IP_ADDRESS
 * 2) buffer statistics data for 2 seconds
 * 3) use default value for adhoc parameters instead of 0
 *
 * 05 19 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * let OID_PNP_SET_POWER invoke wlanoidSetAcpiDevicePowerState() directly.
 *
 * 05 18 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement Wakeup-on-LAN except firmware integration part
 *
 * 05 13 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add NULL OID implementation for WOL-related OIDs.
 *
 * 05 12 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add extra 64 adjustable parameters for CoEX scenario.
 *
 * 04 28 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * Start address overriding is done in common part, removing awareness in glue layer
 *
 * 04 22 2010 cp.wu
 * [WPD00003830]add OID_802_11_PRIVACY_FILTER support
 * enable RX filter OID
 *
 * 04 21 2010 cp.wu
 * [WPD00003828][MT6620 Wi-Fi] WHQL 1c_PrivateDevIoCtl fail with erro
 * correct OID_802_11_STATISTICS order
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code refine: fgTestMode should be at adapter rather than glue due to the device/fw is also involved
 *
 * 04 01 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * change to use WIFI_TCM_ALWAYS_ON as firmware image
 *
 * 03 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * firmware download load adress & start address are now configured from config.h
 *  *  *  * due to the different configurations on FPGA and ASIC
 *
 * 03 17 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * correct wide-string registry parsing
 *
 * 03 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * when starting adapter, read local adminsitrated address from registry and send to firmware via CMD_BASIC_CONFIG.
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) the use of prPendingOid revised, all accessing are now protected by spin lock
 *  *  *  *  *  *  * 2) ensure wlanReleasePendingOid will clear all command queues
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add mutex to avoid multiple access to qmTxQueue simultaneously.
 *
 * 02 23 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add checksum offloading support.
 *
 * 02 11 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. add logic for firmware download
 *  *  *  *  * 2. firmware image filename and start/load address are now retrieved from registry
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove unused function in nic_rx.c [which has been handled in que_mgt.c]
 *  *  *  *  *  *  *  * 2) firmware image length is now retrieved via NdisFileOpen
 *  *  *  *  *  *  *  * 3) firmware image is not structured by (P_IMG_SEC_HDR_T) anymore
 *  *  *  *  *  *  *  * 4) nicRxWaitResponse() revised
 *  *  *  *  *  *  *  * 5) another set of TQ counter default value is added for fw-download state
 *  *  *  *  *  *  *  * 6) Wi-Fi load address is now retrieved from registry too
 *
 * 02 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * prepare for implementing fw download logic
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * OID handler table is necessary to be in-ordered.
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * [WPD00001943] Create WiFi test driver framework on WinXP.
 * correct OID blocking rules
 *
 * 01 27 2010 wh.su
 * [WPD00003816][MT6620 Wi-Fi] Adding the security support
 * .
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 *  *  *  *  *  *  *  *  *  *  *  * 2. block TX/ordinary OID when RF test mode is engaged
 *  *  *  *  *  *  *  *  *  *  *  * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 *  *  *  *  *  *  *  *  *  *  *  * 4. correct some HAL implementation
 *
 * 01 21 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement OID_802_11_MEDIA_STREAM_MODE
 *
 * 12 30 2009 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) According to CMD/EVENT documentation v0.8,
 *  *  *  *  *  *  *  *  *  *  * OID_CUSTOM_TEST_RX_STATUS & OID_CUSTOM_TEST_TX_STATUS is no longer used,
 *  *  *  *  *  *  *  *  *  *  * and result is retrieved by get ATInfo instead
 *  *  *  *  *  *  *  *  *  *  * 2) add 4 counter for recording aggregation statistics
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-12-08 17:39:13 GMT mtk02752
**  correct size check for TX/RX status query
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-12-08 11:31:43 GMT mtk02752
**  add RF test related OIDs
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-11-26 20:57:25 GMT mtk02752
**  i4OidPendingCount should be increased when oid is pended
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-11-23 18:13:12 GMT mtk02752
**  utilize wlanIsHandlerNeedHwAccess() to decide the Set/Query could be served immediately without HW access or should be queued
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-10-13 21:59:31 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-09-09 17:26:25 GMT mtk01084
**  modify for DDK related functions
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-04-21 09:45:30 GMT mtk01461
**  Revise windowsSet/QueryInformation() for savfing OID information
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-04-08 17:48:05 GMT mtk01084
**  modify the interface of downloading image from D3 to D0
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:36:59 GMT mtk01426
**  Init for develop
**
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/


/*******************************************************************************
*                E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "config.h"
#include "gl_os.h"

/*******************************************************************************
*                          C O N S T A N T S
********************************************************************************
*/
#define OID_CUSTOM_OID_INTERFACE_VERSION                0xFFA0C000

#if CFG_SLT_SUPPORT
#define OID_CUSTOM_SLT                               0xFFA0C587
#endif

/* Precedent OIDs */
#define OID_CUSTOM_WMM_PS_TEST                          0xFFA0C589
#define OID_CUSTOM_MCR_RW                               0xFFA0C801
#define OID_CUSTOM_EEPROM_RW                            0xFFA0C803
#define OID_CUSTOM_SW_CTRL                              0xFFA0C805
#define OID_CUSTOM_SEC_CHECK                            0xFFA0C806


/* RF Test specific OIDs */
#define OID_CUSTOM_TEST_MODE                            0xFFA0C901
#define OID_CUSTOM_TEST_RX_STATUS                       0xFFA0C903
#define OID_CUSTOM_TEST_TX_STATUS                       0xFFA0C905
#define OID_CUSTOM_ABORT_TEST_MODE                      0xFFA0C906
#define OID_CUSTOM_MTK_WIFI_TEST                        0xFFA0C911
#define OID_CUSTOM_TEST_ICAP_MODE                       0xFFA0C913

/* NVRAM */
#define OID_CUSTOM_MTK_NVRAM_RW                         0xFFA0C941
#define OID_CUSTOM_CFG_SRC_TYPE                         0xFFA0C942

#if CFG_SUPPORT_WAPI
#define OID_802_11_WAPI_MODE                            0xFFA0CA00
#define OID_802_11_WAPI_ASSOC_INFO                      0xFFA0CA01
#define OID_802_11_SET_WAPI_KEY                         0xFFA0CA02
#endif

/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/
typedef struct _WLAN_REG_ENTRY_T {
	NDIS_STRING regName;	/* parameter name text */
	PUINT_8 ansiRegName;	/* ANSI parameter name text */
	UINT mandatory;		/* 1 -> mandatory, 0 -> optional */
	UINT fieldSize;		/* size (in bytes) of the field.
				   If the prarmenter is string,
				   this filed is the length of default string
				   including '\0'.
				   If the prarmenter is integer,
				   this filed is set to 4 */
	NDIS_PARAMETER_TYPE ParameterType;
	union {
		UINT_32 u4IntData;
		PWSTR pucStrData;
	} rDefaultValue;
	UINT min;		/* minimum value allowed */
	UINT max;		/* maximum value allowed */
	UINT u4GlueOffset;	/* target offset in GlueInfo */
} WLAN_REG_ENTRY_T, *P_WLAN_REG_ENTRY_T;


/*******************************************************************************
*                        P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*                       P R I V A T E   D A T A
********************************************************************************
*/
/* OID processing table */
/* Order is important here because the OIDs should be in order of
   increasing value for binary searching. */
/* TODO: Check */
WLAN_REQ_ENTRY arWlanOidReqTable[] = {
	/* General Operational Characteristics */
	/* (NDIS_OID)rOid,                          (PUINT_8)pucOidName,                 fgQryBufLenChecking, fgSetBufLenChecking, eOidMethod, u4InfoBufLen,                                pfOidQueryHandler,                  pfOidSetHandler} */
	{OID_GEN_SUPPORTED_LIST, DISP_STRING("OID_GEN_SUPPORTED_LIST"), FALSE, FALSE,
	 ENUM_OID_GLUE_ONLY, 0, reqQuerySupportedList, NULL},
	{OID_GEN_HARDWARE_STATUS, DISP_STRING("OID_GEN_HARDWARE_STATUS"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, sizeof(NDIS_HARDWARE_STATUS), reqQueryHardwareStatus, NULL}
	,
	{OID_GEN_MEDIA_SUPPORTED, DISP_STRING("OID_GEN_MEDIA_SUPPORTED"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, sizeof(NDIS_MEDIUM), reqQueryMedia, NULL}
	,
	{OID_GEN_MEDIA_IN_USE, DISP_STRING("OID_GEN_MEDIA_IN_USE"), TRUE, FALSE, ENUM_OID_GLUE_ONLY,
	 sizeof(NDIS_MEDIUM), reqQueryMedia, NULL}
	,
	{OID_GEN_MAXIMUM_LOOKAHEAD, DISP_STRING("OID_GEN_MAXIMUM_LOOKAHEAD"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryMaxFrameSize, NULL}
	,
	{OID_GEN_MAXIMUM_FRAME_SIZE, DISP_STRING("OID_GEN_MAXIMUM_FRAME_SIZE"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryMaxFrameSize, NULL}
	,
	{OID_GEN_LINK_SPEED, DISP_STRING("OID_GEN_LINK_SPEED"), TRUE, FALSE, ENUM_OID_DRIVER_CORE,
	 4, wlanoidQueryLinkSpeed, NULL}
	,
	{OID_GEN_TRANSMIT_BUFFER_SPACE, DISP_STRING("OID_GEN_TRANSMIT_BUFFER_SPACE"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryTxBufferSpace, NULL}
	,
	{OID_GEN_RECEIVE_BUFFER_SPACE, DISP_STRING("OID_GEN_RECEIVE_BUFFER_SPACE"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryRxBufferSpace, NULL}
	,
	{OID_GEN_TRANSMIT_BLOCK_SIZE, DISP_STRING("OID_GEN_TRANSMIT_BLOCK_SIZE"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryMaxTotalSize, NULL}
	,
	{OID_GEN_RECEIVE_BLOCK_SIZE, DISP_STRING("OID_GEN_RECEIVE_BLOCK_SIZE"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryMaxTotalSize, NULL}
	,
	{OID_GEN_VENDOR_ID, DISP_STRING("OID_GEN_VENDOR_ID"), TRUE, FALSE, ENUM_OID_GLUE_EXTENSION,
	 4, wlanoidQueryVendorId, NULL}
	,
	{OID_GEN_VENDOR_DESCRIPTION, DISP_STRING("OID_GEN_VENDOR_DESCRIPTION"), FALSE, FALSE,
	 ENUM_OID_GLUE_ONLY, 0, reqQueryVendorDescription, NULL}
	,
	{OID_GEN_CURRENT_PACKET_FILTER, DISP_STRING("OID_GEN_CURRENT_PACKET_FILTER"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, 4, wlanoidQueryCurrentPacketFilter, wlanoidSetCurrentPacketFilter}
	,
	{OID_GEN_CURRENT_LOOKAHEAD, DISP_STRING("OID_GEN_CURRENT_LOOKAHEAD"), TRUE, TRUE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryMaxFrameSize, reqSetCurrentLookahead}
	,
	{OID_GEN_DRIVER_VERSION, DISP_STRING("OID_GEN_DRIVER_VERSION"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 2, reqQueryDriverVersion, NULL}
	,
	{OID_GEN_MAXIMUM_TOTAL_SIZE, DISP_STRING("OID_GEN_MAXIMUM_TOTAL_SIZE"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryMaxTotalSize, NULL}
	,
	{OID_GEN_MAC_OPTIONS, DISP_STRING("OID_GEN_MAC_OPTIONS"), TRUE, FALSE, ENUM_OID_GLUE_ONLY,
	 4, reqQueryMacOptions, NULL}
	,
	{OID_GEN_MEDIA_CONNECT_STATUS, DISP_STRING("OID_GEN_MEDIA_CONNECT_STATUS"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, sizeof(NDIS_MEDIA_STATE), reqQueryMediaConnectStatus, NULL}
	,
	{OID_GEN_MAXIMUM_SEND_PACKETS, DISP_STRING("OID_GEN_MAXIMUM_SEND_PACKETS"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryMaxSendPackets, NULL}
	,
	{OID_GEN_VENDOR_DRIVER_VERSION, DISP_STRING("OID_GEN_VENDOR_DRIVER_VERSION"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryVendorDriverVersion, NULL}
	,
	{OID_GEN_NETWORK_LAYER_ADDRESSES, DISP_STRING("OID_GEN_NETWORK_LAYER_ADDRESSES"), FALSE,
	 FALSE, ENUM_OID_DRIVER_CORE, 0, NULL, wlanoidSetNetworkAddress}
	,
	{OID_GEN_PHYSICAL_MEDIUM, DISP_STRING("OID_GEN_PHYSICAL_MEDIUM"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, sizeof(NDIS_PHYSICAL_MEDIUM), reqQueryPhysicalMedium, NULL}
	,
	/* General Statistics */
	{OID_GEN_XMIT_OK, DISP_STRING("OID_GEN_XMIT_OK"), FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 wlanoidQueryXmitOk, NULL}
	,
	{OID_GEN_RCV_OK, DISP_STRING("OID_GEN_RCV_OK"), FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 wlanoidQueryRcvOk, NULL}
	,
	{OID_GEN_XMIT_ERROR, DISP_STRING("OID_GEN_XMIT_ERROR"), FALSE, FALSE, ENUM_OID_DRIVER_CORE,
	 0, wlanoidQueryXmitError, NULL}
	,
	{OID_GEN_RCV_ERROR, DISP_STRING("OID_GEN_RCV_ERROR"), FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 wlanoidQueryRcvError, NULL}
	,
	{OID_GEN_RCV_NO_BUFFER, DISP_STRING("OID_GEN_RCV_NO_BUFFER"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, wlanoidQueryRcvNoBuffer, NULL}
	,
	{OID_GEN_RCV_CRC_ERROR, DISP_STRING("OID_GEN_RCV_CRC_ERROR"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, wlanoidQueryRcvCrcError, NULL}
	,

	/* Ethernet Operational Characteristics */
	{OID_802_3_PERMANENT_ADDRESS, DISP_STRING("OID_802_3_PERMANENT_ADDRESS"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, 6, wlanoidQueryPermanentAddr, NULL}
	,
	{OID_802_3_CURRENT_ADDRESS, DISP_STRING("OID_802_3_CURRENT_ADDRESS"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, 6, wlanoidQueryCurrentAddr, NULL}
	,
	{OID_802_3_MULTICAST_LIST, DISP_STRING("OID_802_3_MULTICAST_LIST"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, wlanoidQueryMulticastList, wlanoidSetMulticastList}
	,
	{OID_802_3_MAXIMUM_LIST_SIZE, DISP_STRING("OID_802_3_MAXIMUM_LIST_SIZE"), FALSE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryMaxListSize, NULL}
	,

	/* Ethernet Statistics */
	{OID_802_3_RCV_ERROR_ALIGNMENT, DISP_STRING("OID_802_3_RCV_ERROR_ALIGNMENT"), TRUE, FALSE,
	 ENUM_OID_GLUE_ONLY, 4, reqQueryRcvErrorAlignment, NULL}
	,
	{OID_802_3_XMIT_ONE_COLLISION, DISP_STRING("OID_802_3_XMIT_ONE_COLLISION"), TRUE, FALSE,
	 ENUM_OID_DRIVER_CORE, 4, wlanoidQueryXmitOneCollision, NULL}
	,
	{OID_802_3_XMIT_MORE_COLLISIONS, DISP_STRING("OID_802_3_XMIT_MORE_COLLISIONS"), TRUE, FALSE,
	 ENUM_OID_DRIVER_CORE, 4, wlanoidQueryXmitMoreCollisions, NULL}
	,
	{OID_802_3_XMIT_MAX_COLLISIONS, DISP_STRING("OID_802_3_XMIT_MAX_COLLISIONS"), TRUE, FALSE,
	 ENUM_OID_DRIVER_CORE, 4, wlanoidQueryXmitMaxCollisions, NULL}
	,

	/* NDIS 802.11 Wireless LAN OIDs */
	{OID_802_11_BSSID, DISP_STRING("OID_802_11_BSSID"), TRUE, TRUE, ENUM_OID_DRIVER_CORE,
	 sizeof(NDIS_802_11_MAC_ADDRESS), wlanoidQueryBssid, wlanoidSetBssid}
	,
	{OID_802_11_SSID, DISP_STRING("OID_802_11_SSID"), TRUE, TRUE, ENUM_OID_DRIVER_CORE,
	 sizeof(NDIS_802_11_SSID), wlanoidQuerySsid, wlanoidSetSsid}
	,
	{OID_802_11_INFRASTRUCTURE_MODE, DISP_STRING("OID_802_11_INFRASTRUCTURE_MODE"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_NETWORK_INFRASTRUCTURE),
	 wlanoidQueryInfrastructureMode, wlanoidSetInfrastructureMode}
	,
	{OID_802_11_ADD_WEP, DISP_STRING("OID_802_11_ADD_WEP"), FALSE, FALSE, ENUM_OID_DRIVER_CORE,
	 0, NULL, wlanoidSetAddWep}
	,
	{OID_802_11_REMOVE_WEP, DISP_STRING("OID_802_11_REMOVE_WEP"), FALSE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_KEY_INDEX), NULL, wlanoidSetRemoveWep}
	,
	{OID_802_11_DISASSOCIATE, DISP_STRING("OID_802_11_DISASSOCIATE"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, NULL, wlanoidSetDisassociate}
	,
	{OID_802_11_AUTHENTICATION_MODE, DISP_STRING("OID_802_11_AUTHENTICATION_MODE"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_AUTHENTICATION_MODE), wlanoidQueryAuthMode,
	 wlanoidSetAuthMode}
	,
/* { OID_802_11_PRIVACY_FILTER,              DISP_STRING("OID_802_11_PRIVACY_FILTER"),           TRUE,   TRUE,   ENUM_OID_DRIVER_CORE,  sizeof(NDIS_802_11_PRIVACY_FILTER),         wlanoidQueryPrivacyFilter,          wlanoidSetPirvacyFilter }, */
	{OID_802_11_BSSID_LIST_SCAN, DISP_STRING("OID_802_11_BSSID_LIST_SCAN"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, NULL, wlanoidSetBssidListScan}
	,
	{OID_802_11_ENCRYPTION_STATUS, DISP_STRING("OID_802_11_ENCRYPTION_STATUS"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_ENCRYPTION_STATUS), wlanoidQueryEncryptionStatus,
	 wlanoidSetEncryptionStatus}
	,
	{OID_802_11_RELOAD_DEFAULTS, DISP_STRING("OID_802_11_RELOAD_DEFAULTS"), FALSE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_RELOAD_DEFAULTS), NULL, wlanoidSetReloadDefaults}
	,
	{OID_802_11_ADD_KEY, DISP_STRING("OID_802_11_ADD_KEY"), FALSE, FALSE, ENUM_OID_DRIVER_CORE,
	 0, NULL, wlanoidSetAddKey}
	,
	{OID_802_11_REMOVE_KEY, DISP_STRING("OID_802_11_REMOVE_KEY"), FALSE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_REMOVE_KEY), NULL, wlanoidSetRemoveKey}
	,
	{OID_802_11_ASSOCIATION_INFORMATION, DISP_STRING("OID_802_11_ASSOCIATION_INFORMATION"),
	 FALSE, FALSE, ENUM_OID_GLUE_ONLY, 0, reqQueryAssocInfo, NULL}
	,
	{OID_802_11_TEST, DISP_STRING("OID_802_11_TEST"), FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 NULL, wlanoidSetTest}
	,
	{OID_802_11_MEDIA_STREAM_MODE, DISP_STRING("OID_802_11_MEDIA_STREAM_MODE"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_MEDIA_STREAM_MODE), wlanoidQueryMediaStreamMode,
	 wlanoidSetMediaStreamMode}
	,
	{OID_802_11_CAPABILITY, DISP_STRING("OID_802_11_CAPABILITY"), TRUE, FALSE,
	 ENUM_OID_DRIVER_CORE,
	 4 * sizeof(ULONG) + 14 * sizeof(NDIS_802_11_AUTHENTICATION_ENCRYPTION),
	 wlanoidQueryCapability, NULL}
	,
	{OID_802_11_PMKID, DISP_STRING("OID_802_11_PMKID"), FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 wlanoidQueryPmkid, wlanoidSetPmkid}
	,
	{OID_802_11_NETWORK_TYPES_SUPPORTED, DISP_STRING("OID_802_11_NETWORK_TYPES_SUPPORTED"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0, wlanoidQueryNetworkTypesSupported, NULL}
	,
	{OID_802_11_NETWORK_TYPE_IN_USE, DISP_STRING("OID_802_11_NETWORK_TYPE_IN_USE"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_NETWORK_TYPE), wlanoidQueryNetworkTypeInUse,
	 wlanoidSetNetworkTypeInUse}
	,
	{OID_802_11_RSSI, DISP_STRING("OID_802_11_RSSI"), TRUE, FALSE, ENUM_OID_DRIVER_CORE,
	 sizeof(NDIS_802_11_RSSI), wlanoidQueryRssi, NULL}
	,
	{OID_802_11_RSSI_TRIGGER, DISP_STRING("OID_802_11_RSSI_TRIGGER"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_RSSI), wlanoidQueryRssiTrigger,
	 wlanoidSetRssiTrigger}
	,
	{OID_802_11_FRAGMENTATION_THRESHOLD, DISP_STRING("OID_802_11_FRAGMENTATION_THRESHOLD"),
	 TRUE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_FRAGMENTATION_THRESHOLD),
	 wlanoidQueryFragThreshold, wlanoidSetFragThreshold}
	,
	{OID_802_11_RTS_THRESHOLD, DISP_STRING("OID_802_11_RTS_THRESHOLD"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_RTS_THRESHOLD), wlanoidQueryRtsThreshold,
	 wlanoidSetRtsThreshold}
	,
	{OID_802_11_SUPPORTED_RATES, DISP_STRING("OID_802_11_SUPPORTED_RATES"), TRUE, FALSE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_RATES), wlanoidQuerySupportedRates, NULL}
	,
	{OID_802_11_DESIRED_RATES, DISP_STRING("OID_802_11_DESIRED_RATES"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_RATES), wlanoidQueryDesiredRates,
	 wlanoidSetDesiredRates}
	,
	{OID_802_11_CONFIGURATION, DISP_STRING("OID_802_11_CONFIGURATION"), TRUE, TRUE,
	 ENUM_OID_GLUE_EXTENSION, sizeof(NDIS_802_11_CONFIGURATION), reqExtQueryConfiguration,
	 reqExtSetConfiguration}
	,
	{OID_802_11_POWER_MODE, DISP_STRING("OID_802_11_POWER_MODE"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_POWER_MODE), wlanoidQuery802dot11PowerSaveProfile,
	 wlanoidSet802dot11PowerSaveProfile}
	,
	{OID_802_11_BSSID_LIST, DISP_STRING("OID_802_11_BSSID_LIST"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, wlanoidQueryBssidList, NULL}
	,
	{OID_802_11_STATISTICS, DISP_STRING("OID_802_11_STATISTICS"), TRUE, FALSE,
	 ENUM_OID_DRIVER_CORE, sizeof(NDIS_802_11_STATISTICS), wlanoidQueryStatistics, NULL}
	,
#if CFG_TCP_IP_CHKSUM_OFFLOAD
	{OID_TCP_TASK_OFFLOAD, DISP_STRING("OID_TCP_TASK_OFFLOAD"), FALSE, FALSE,
	 ENUM_OID_GLUE_EXTENSION, sizeof(NDIS_TASK_OFFLOAD), reqQueryTaskOffload,
	 reqExtSetTaskOffload}
	,
#endif
	{OID_PNP_CAPABILITIES, DISP_STRING("OID_PNP_CAPABILITIES"), TRUE, FALSE, ENUM_OID_GLUE_ONLY,
	 sizeof(NDIS_PNP_CAPABILITIES), reqQueryPnPCapabilities, NULL}
	,
	{OID_PNP_SET_POWER, DISP_STRING("OID_PNP_SET_POWER"), TRUE, FALSE, ENUM_OID_DRIVER_CORE,
	 sizeof(NDIS_DEVICE_POWER_STATE), NULL, wlanoidSetAcpiDevicePowerState}
	,
	{OID_PNP_QUERY_POWER, DISP_STRING("OID_PNP_QUERY_POWER"), TRUE, TRUE, ENUM_OID_DRIVER_CORE,
	 sizeof(NDIS_DEVICE_POWER_STATE), wlanoidQueryAcpiDevicePowerState, NULL}
	,
#if CFG_ENABLE_WAKEUP_ON_LAN
	{OID_PNP_ADD_WAKE_UP_PATTERN, DISP_STRING("OID_PNP_ADD_WAKE_UP_PATTERN"), FALSE, FALSE,
	 ENUM_OID_GLUE_EXTENSION, 0, NULL, wlanoidSetAddWakeupPattern}
	,
	{OID_PNP_REMOVE_WAKE_UP_PATTERN, DISP_STRING("OID_PNP_REMOVE_WAKE_UP_PATTERN"), FALSE,
	 FALSE, ENUM_OID_GLUE_EXTENSION 0, NULL, wlanoidSetRemoveWakeupPattern}
	,
	{OID_PNP_ENABLE_WAKE_UP, DISP_STRING("OID_PNP_ENABLE_WAKE_UP"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(UINT_32), wlanoidQueryEnableWakeup, wlanoidSetEnableWakeup}
	,
#endif
	{OID_CUSTOM_OID_INTERFACE_VERSION, DISP_STRING("OID_CUSTOM_OID_INTERFACE_VERSION"), TRUE,
	 FALSE, ENUM_OID_DRIVER_CORE, 4, wlanoidQueryOidInterfaceVersion, NULL}
	,

#if CFG_SLT_SUPPORT
	{OID_CUSTOM_SLT, DISP_STRING("OID_CUSTOM_SLT"), FALSE, FALSE, ENUM_OID_DRIVER_CORE,
	 sizeof(PARAM_MTK_SLT_TEST_STRUC_T), wlanoidQuerySLTStatus, wlanoidUpdateSLTMode}
	,
#endif
	{OID_CUSTOM_WMM_PS_TEST, DISP_STRING("OID_CUSTOM_WMM_PS_TEST"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, 4, NULL, wlanoidSetWiFiWmmPsTest}
	,
	{OID_CUSTOM_MCR_RW, DISP_STRING("OID_CUSTOM_MCR_RW"), TRUE, TRUE, ENUM_OID_DRIVER_CORE,
	 sizeof(PARAM_CUSTOM_MCR_RW_STRUC_T), wlanoidQueryMcrRead, wlanoidSetMcrWrite}
	,
	{OID_CUSTOM_EEPROM_RW, DISP_STRING("OID_CUSTOM_EEPROM_RW"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_EEPROM_RW_STRUC_T), wlanoidQueryEepromRead,
	 wlanoidSetEepromWrite}
	,
	{OID_CUSTOM_SW_CTRL, DISP_STRING("OID_CUSTOM_SW_CTRL"), TRUE, TRUE, ENUM_OID_DRIVER_CORE,
	 sizeof(PARAM_CUSTOM_SW_CTRL_STRUC_T), wlanoidQuerySwCtrlRead, wlanoidSetSwCtrlWrite}
	,
	{OID_CUSTOM_TEST_MODE, DISP_STRING("OID_CUSTOM_TEST_MODE"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, NULL, wlanoidRftestSetTestMode}
	,
	{OID_CUSTOM_ABORT_TEST_MODE, DISP_STRING("OID_CUSTOM_ABORT_TEST_MODE"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, NULL, wlanoidRftestSetAbortTestMode}
	,
	{OID_CUSTOM_MTK_WIFI_TEST, DISP_STRING("OID_CUSTOM_MTK_WIFI_TEST"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(PARAM_MTK_WIFI_TEST_STRUC_T), wlanoidRftestQueryAutoTest,
	 wlanoidRftestSetAutoTest}
	,
	{OID_CUSTOM_TEST_ICAP_MODE, DISP_STRING("OID_CUSTOM_TEST_ICAP_MODE"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, NULL, wlanoidRftestSetTestIcapMode}
	,
/* { OID_CUSTOM_TEST_RX_STATUS,                DISP_STRING("OID_CUSTOM_TEST_RX_STATUS"),           TRUE,   FALSE,  ENUM_OID_DRIVER_CORE,  sizeof(PARAM_CUSTOM_RFTEST_RX_STATUS_STRUC_T),   wlanoidQueryRfTestRxStatus,    NULL }, */
/* { OID_CUSTOM_TEST_TX_STATUS,                DISP_STRING("OID_CUSTOM_TEST_TX_STATUS"),           TRUE,   FALSE,  ENUM_OID_DRIVER_CORE,  sizeof(PARAM_CUSTOM_RFTEST_TX_STATUS_STRUC_T),   wlanoidQueryRfTestTxStatus,    NULL }, */
	/* (NDIS_OID)rOid,                          (PUINT_8)pucOidName,                 fgQryBufLenChecking, fgSetBufLenChecking, fgIsHandleInGlueLayerOnly, u4InfoBufLen, pfOidQueryHandler,                  pfOidSetHandler} */
#if 0
	{OID_CUSTOM_MTK_NVRAM_RW, DISP_STRING("OID_CUSTOM_MTK_NVRAM_RW"), TRUE, TRUE,
	 ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_NVRAM_RW_STRUCT_T), wlanoidQueryNvramRead,
	 wlanoidSetNvramWrite}
	,
	{OID_CUSTOM_CFG_SRC_TYPE, DISP_STRING("OID_CUSTOM_CFG_SRC_TYPE"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, sizeof(ENUM_CFG_SRC_TYPE_T), wlanoidQueryCfgSrcType, NULL}
	,
#endif
#if CFG_SUPPORT_WAPI
	{OID_802_11_WAPI_MODE, DISP_STRING("OID_802_11_WAPI_MODE"), FALSE, TRUE,
	 ENUM_OID_DRIVER_CORE, 4, NULL, wlanoidSetWapiMode}
	,
	{OID_802_11_WAPI_ASSOC_INFO, DISP_STRING("OID_802_11_WAPI_ASSOC_INFO"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, 0, NULL, wlanoidSetWapiAssocInfo}
	,
	{OID_802_11_SET_WAPI_KEY, DISP_STRING("OID_802_11_SET_WAPI_KEY"), FALSE, FALSE,
	 ENUM_OID_DRIVER_CORE, sizeof(PARAM_WPI_KEY_T), NULL, wlanoidSetWapiKey}
	,
#endif
};

/* NIC Registry Parameters Table
   This table contains a list of all of the configuration parameters
   that the driver supports.  The driver will attempt to find these
   parameters in the registry and use the registry value for these
   parameters.  If the parameter is not found in the registry, then the
   default value is used. */

WLAN_REG_ENTRY_T arWlanRegTable[] = {
	/* Registry parameter name              ANSI name           mandatory(1)    fieldSize       type ,                  Default Value,              Min         Max             GLUE offset   */
	{NDIS_STRING_CONST("SDBlockSize"), "SDBlockSize", 0, 4, NdisParameterHexInteger, 512, 0,
	 2048, GLUE_GET_REG_OFFSET(u4SdBlockSize)}
	,
	{NDIS_STRING_CONST("SDBusWidth"), "SDBusWidth", 0, 4, NdisParameterHexInteger, 4, 0, 4,
	 GLUE_GET_REG_OFFSET(u4SdBusWidth)}
	,
	{NDIS_STRING_CONST("SDClockRate"), "SDClockRate", 0, 4, NdisParameterHexInteger, 16500, 0,
	 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4SdClockRate)}
	,
	{NDIS_STRING_CONST("CountryCode"), "CountryCode", 0, 3, NdisParameterString,
	 (UINT_32) CFG_COUNTRY_CODE, 0, 0xffff, GLUE_GET_REG_OFFSET(au2CountryCode)}
	,
	{NDIS_STRING_CONST("StartAddress"), "StartAddress", 0, 4, NdisParameterHexInteger,
	 CFG_FW_START_ADDRESS, 0, 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4StartAddress)}
	,
	{NDIS_STRING_CONST("LoadAddress"), "LoadAddress", 0, 4, NdisParameterHexInteger,
	 CFG_FW_LOAD_ADDRESS, 0, 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4LoadAddress)}
	,
	{NDIS_STRING_CONST("FwImgFilename"), "FwImgFilename", 0, 14, NdisParameterString,
	 (UINT_32) CFG_FW_FILENAME, 0, 64, GLUE_GET_REG_OFFSET(aucFwImgFilename)}
	,
	{NDIS_STRING_CONST("StartFreq"), "StartFreq", 0, 4, NdisParameterInteger,
	 CFG_INIT_ADHOC_FREQ, 0, 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4StartFreq)}
	,
	{NDIS_STRING_CONST("AdHocMode"), "AdHocMode", 0, 4, NdisParameterInteger,
	 CFG_INIT_ADHOC_MODE, 0, 64, GLUE_GET_REG_OFFSET(u4AdhocMode)}
	,
	{NDIS_STRING_CONST("RddStartFreq"), "RddStartFreq", 0, 4, NdisParameterInteger, 0, 0,
	 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4RddStartFreq)}
	,
	{NDIS_STRING_CONST("RddStopFreq"), "RddStopFreq", 0, 4, NdisParameterInteger, 0, 0,
	 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4RddStopFreq)}
	,
	{NDIS_STRING_CONST("RddTestMode"), "RddTestMode", 0, 4, NdisParameterInteger, 0, 0, 2,
	 GLUE_GET_REG_OFFSET(u4RddTestMode)}
	,
	{NDIS_STRING_CONST("RddShutFreq"), "RddShutFreq", 0, 4, NdisParameterInteger, 0, 0, 1,
	 GLUE_GET_REG_OFFSET(u4RddShutFreq)}
	,
	{NDIS_STRING_CONST("RddDfs"), "RddDfs", 0, 4, NdisParameterInteger, 0, 0, 3,
	 GLUE_GET_REG_OFFSET(u4RddDfs)}
	,
	{NDIS_STRING_CONST("TxPwr2G4CCK"), "TxPwr2G4CCK", 0, 1, NdisParameterInteger, 0, 0, 0xFF,
	 GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4Cck)}
	,
	{NDIS_STRING_CONST("TxPwr2G4DSSS"), "TxPwr2G4DSSS", 0, 1, NdisParameterInteger, 0, 0, 0xFF,
	 GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4Dsss)}
	,
	{NDIS_STRING_CONST("TxPwr2G4OfdmBpsk"), "TxPwr2G4OfdmBpsk", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4OFDM_BPSK)}
	,
	{NDIS_STRING_CONST("TxPwr2G4OfdmQpsk"), "TxPwr2G4OfdmQpsk", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4OFDM_QPSK)}
	,
	{NDIS_STRING_CONST("TxPwr2G4Ofdm16Qam"), "TxPwr2G4Ofdm16Qam", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4OFDM_16QAM)}
	,
	{NDIS_STRING_CONST("TxPwr2G4Ofdm48M"), "TxPwr2G4Ofdm48M", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4OFDM_48Mbps)}
	,
	{NDIS_STRING_CONST("TxPwr2G4Ofdm54M"), "TxPwr2G4Ofdm54M", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4OFDM_54Mbps)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT20Bpsk"), "TxPwr2G4HT20Bpsk", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT20_BPSK)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT20Qpsk"), "TxPwr2G4HT20Qpsk", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT20_QPSK)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT2016Qam"), "TxPwr2G4HT2016Qam", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT20_16QAM)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT20MCS5"), "TxPwr2G4HT20MCS5", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT20_MCS5)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT20MCS6"), "TxPwr2G4HT20MCS6", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT20_MCS6)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT20MCS7"), "TxPwr2G4HT20MCS7", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT20_MCS7)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT40Bpsk"), "TxPwr2G4HT40Bpsk", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT40_BPSK)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT40Qpsk"), "TxPwr2G4HT40Qpsk", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT40_QPSK)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT4016Qam"), "TxPwr2G4HT4016Qam", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT40_16QAM)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT40MCS5"), "TxPwr2G4HT40MCS5", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT40_MCS5)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT40MCS6"), "TxPwr2G4HT40MCS6", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT40_MCS6)}
	,
	{NDIS_STRING_CONST("TxPwr2G4HT40MCS7"), "TxPwr2G4HT40MCS7", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr2G4HT40_MCS7)}
	,
	{NDIS_STRING_CONST("TxPwr5GOfdmBpsk"), "TxPwr5GOfdmBpsk", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GOFDM_BPSK)}
	,
	{NDIS_STRING_CONST("TxPwr5GOfdmQpsk"), "TxPwr5GOfdmQpsk", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GOFDM_QPSK)}
	,
	{NDIS_STRING_CONST("TxPwr5GOfdm16Qam"), "TxPwr5GOfdm16Qam", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GOFDM_16QAM)}
	,
	{NDIS_STRING_CONST("TxPwr5GOfdm48M"), "TxPwr5GOfdm48M", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GOFDM_48Mbps)}
	,
	{NDIS_STRING_CONST("TxPwr5GOfdm54M"), "TxPwr5GOfdm54M", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GOFDM_54Mbps)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT20Bpsk"), "TxPwr5GHT20Bpsk", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT20_BPSK)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT20Qpsk"), "TxPwr5GHT20Qpsk", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT20_QPSK)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT2016Qam"), "TxPwr5GHT2016Qam", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT20_16QAM)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT20MCS5"), "TxPwr5GHT20MCS5", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT20_MCS5)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT20MCS6"), "TxPwr5GHT20MCS6", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT20_MCS6)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT20MCS7"), "TxPwr5GHT20MCS7", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT20_MCS7)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT40Bpsk"), "TxPwr5GHT40Bpsk", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT40_BPSK)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT40Qpsk"), "TxPwr5GHT40Qpsk", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT40_QPSK)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT4016Qam"), "TxPwr5GHT4016Qam", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT40_16QAM)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT40MCS5"), "TxPwr5GHT40MCS5", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT40_MCS5)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT40MCS6"), "TxPwr5GHT40MCS6", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT40_MCS6)}
	,
	{NDIS_STRING_CONST("TxPwr5GHT40MCS7"), "TxPwr5GHT40MCS7", 0, 1, NdisParameterInteger, 0, 0,
	 0xFF, GLUE_GET_REG_OFFSET(rTxPwr.cTxPwr5GHT40_MCS7)}
	,
	{NDIS_STRING_CONST("BandEdge2G4PwrUsed"), "BandEdge2G4PwrUsed", 0, 1, NdisParameterInteger,
	 0, 0, 1, GLUE_GET_REG_OFFSET(fg2G4BandEdgePwrUsed)}
	,
	{NDIS_STRING_CONST("BandEdgeMaxPwrCCK"), "BandEdgeMaxPwrCCK", 0, 1, NdisParameterInteger, 0,
	 0, 0xFF, GLUE_GET_REG_OFFSET(cBandEdgeMaxPwrCCK)}
	,
	{NDIS_STRING_CONST("BandEdgeMaxPwrOFDM20"), "BandEdgeMaxPwrOFDM20", 0, 1,
	 NdisParameterInteger, 0, 0, 0xFF, GLUE_GET_REG_OFFSET(cBandEdgeMaxPwrOFDM20)}
	,
	{NDIS_STRING_CONST("BandEdgeMaxPwrOFDM40"), "BandEdgeMaxPwrOFDM40", 0, 1,
	 NdisParameterInteger, 0, 0, 0xFF, GLUE_GET_REG_OFFSET(cBandEdgeMaxPwrOFDM40)}
	,
	{NDIS_STRING_CONST("HiRssiThresh"), "HiRssiThresh", 0, 4, NdisParameterInteger, 46, 0, 128,
	 GLUE_GET_REG_OFFSET(i4HighRssiThreshold)}
	,
	{NDIS_STRING_CONST("MedRssiThresh"), "MedRssiThresh", 0, 4, NdisParameterInteger, 56, 0,
	 128, GLUE_GET_REG_OFFSET(i4MediumRssiThreshold)}
	,
	{NDIS_STRING_CONST("LoRssiThresh"), "LoRssiThresh", 0, 4, NdisParameterInteger, 70, 0, 128,
	 GLUE_GET_REG_OFFSET(i4LowRssiThreshold)}
	,
	{NDIS_STRING_CONST("TxTagAc0"), "TxTagAc0", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4TxPriorityTag[ENUM_AC_TYPE_AC0])}
	,
	{NDIS_STRING_CONST("TxTagAc1"), "TxTagAc1", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4TxPriorityTag[ENUM_AC_TYPE_AC1])}
	,
	{NDIS_STRING_CONST("TxTagAc2"), "TxTagAc2", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4TxPriorityTag[ENUM_AC_TYPE_AC2])}
	,
	{NDIS_STRING_CONST("TxTagAc3"), "TxTagAc3", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4TxPriorityTag[ENUM_AC_TYPE_AC3])}
	,
	{NDIS_STRING_CONST("TxTagAc4"), "TxTagAc4", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4TxPriorityTag[ENUM_AC_TYPE_AC4])}
	,
	{NDIS_STRING_CONST("TxTagAc5"), "TxTagAc5", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4TxPriorityTag[ENUM_AC_TYPE_AC5])}
	,
	{NDIS_STRING_CONST("TxTagAc6"), "TxTagAc6", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4TxPriorityTag[ENUM_AC_TYPE_AC6])}
	,
	{NDIS_STRING_CONST("TxTagBmc"), "TxTagBmc", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4TxPriorityTag[ENUM_AC_TYPE_BMC])}
	,
	{NDIS_STRING_CONST("RxTagAc0"), "RxTagAc0", 0, 4, NdisParameterInteger, 5, 0, 15,
	 GLUE_GET_REG_OFFSET(au4RxPriorityTag[ENUM_AC_TYPE_AC0])}
	,
	{NDIS_STRING_CONST("RxTagAc1"), "RxTagAc1", 0, 4, NdisParameterInteger, 5, 0, 15,
	 GLUE_GET_REG_OFFSET(au4RxPriorityTag[ENUM_AC_TYPE_AC1])}
	,
	{NDIS_STRING_CONST("RxTagAc2"), "RxTagAc2", 0, 4, NdisParameterInteger, 5, 0, 15,
	 GLUE_GET_REG_OFFSET(au4RxPriorityTag[ENUM_AC_TYPE_AC2])}
	,
	{NDIS_STRING_CONST("RxTagAc3"), "RxTagAc3", 0, 4, NdisParameterInteger, 5, 0, 15,
	 GLUE_GET_REG_OFFSET(au4RxPriorityTag[ENUM_AC_TYPE_AC3])}
	,
	{NDIS_STRING_CONST("RxTagAc4"), "RxTagAc4", 0, 4, NdisParameterInteger, 5, 0, 15,
	 GLUE_GET_REG_OFFSET(au4RxPriorityTag[ENUM_AC_TYPE_AC4])}
	,
	{NDIS_STRING_CONST("RxTagAc5"), "RxTagAc5", 0, 4, NdisParameterInteger, 5, 0, 15,
	 GLUE_GET_REG_OFFSET(au4RxPriorityTag[ENUM_AC_TYPE_AC5])}
	,
	{NDIS_STRING_CONST("RxTagAc6"), "RxTagAc6", 0, 4, NdisParameterInteger, 5, 0, 15,
	 GLUE_GET_REG_OFFSET(au4RxPriorityTag[ENUM_AC_TYPE_AC6])}
	,
	{NDIS_STRING_CONST("RxNswTag"), "RxNswTag", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4AdvPriorityTag[ENUM_ADV_AC_TYPE_RX_NSW])}
	,
	{NDIS_STRING_CONST("RxPtaTag"), "RxPtaTag", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4AdvPriorityTag[ENUM_ADV_AC_TYPE_RX_PTA])}
	,
	{NDIS_STRING_CONST("RxSpTag"), "RxSpTag", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4AdvPriorityTag[ENUM_ADV_AC_TYPE_RX_SP])}
	,
	{NDIS_STRING_CONST("TxPtaTag"), "TxPtaTag", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4AdvPriorityTag[ENUM_ADV_AC_TYPE_TX_PTA])}
	,
	{NDIS_STRING_CONST("TxRspTag"), "TxRspTag", 0, 4, NdisParameterInteger, 2, 0, 15,
	 GLUE_GET_REG_OFFSET(au4AdvPriorityTag[ENUM_ADV_AC_TYPE_TX_RSP])}
	,
	{NDIS_STRING_CONST("FastPsPoll"), "FastPsPoll", 0, 4, NdisParameterInteger, 2, 0, 2,
	 GLUE_GET_REG_OFFSET(u4FastPSPoll)}
	,
	{NDIS_STRING_CONST("PTA"), "PTA", 0, 4, NdisParameterInteger, 0, 0, 1,
	 GLUE_GET_REG_OFFSET(u4PTA)}
	,
	{NDIS_STRING_CONST("TXLimit"), "TXLimit", 0, 4, NdisParameterInteger, 0, 0, 1,
	 GLUE_GET_REG_OFFSET(u4TXLimit)}
	,
	{NDIS_STRING_CONST("SilenceWindow"), "SilenceWindow", 0, 4, NdisParameterInteger, 100, 100,
	 625, GLUE_GET_REG_OFFSET(u4SilenceWindow)}
	,
	{NDIS_STRING_CONST("TXLimitThreshold"), "TXLimitThreshold", 0, 4, NdisParameterInteger,
	 1250, 250, 1250, GLUE_GET_REG_OFFSET(u4TXLimitThreshold)}
	,
	{NDIS_STRING_CONST("PowerMode"), "PowerMode", 0, 4, NdisParameterInteger,
	 CFG_INIT_POWER_SAVE_PROF, 0, ENUM_PSP_NUM - 1, GLUE_GET_REG_OFFSET(u4PowerMode)}
	,
	{NDIS_STRING_CONST("ArpFilterEn"), "ArpFilterEn", 0, 4, NdisParameterInteger,
	 CFG_INIT_ENABLE_PATTERN_FILTER_ARP, 0, 1, GLUE_GET_REG_OFFSET(fgEnArpFilter)}
	,
	{NDIS_STRING_CONST("UAPSD_AC"), "UAPSD_AC", 0, 4, NdisParameterInteger,
	 CFG_INIT_UAPSD_AC_BMP, 0, 0xf, GLUE_GET_REG_OFFSET(u4UapsdAcBmp)}
	,
	{NDIS_STRING_CONST("PsCurrentMeasureEn"), "PsCurrentMeasureEn", 0, 4, NdisParameterInteger,
	 0, 0, 1, GLUE_GET_REG_OFFSET(u4PsCurrentMeasureEn)}
	,
	{NDIS_STRING_CONST("MAX_SP_LEN"), "MAX_SP_LEN", 0, 4, NdisParameterInteger, 2, 0, 6,
	 GLUE_GET_REG_OFFSET(u4MaxSpLen)}
	,
	{NDIS_STRING_CONST("DisableOnlineScan"), "DisableOnlineScan", 0, 4, NdisParameterInteger, 0,
	 0, 1, GLUE_GET_REG_OFFSET(fgDisOnlineScan)}
	,
	{NDIS_STRING_CONST("DisableBcnLostDetection"), "DisableBcnLostDetection", 0, 4,
	 NdisParameterInteger, 0, 0, 1, GLUE_GET_REG_OFFSET(fgDisBcnLostDetection)}
	,
	{NDIS_STRING_CONST("Rate"), "Rate", 0, 4, NdisParameterInteger, 0, 0, 46,
	 GLUE_GET_REG_OFFSET(u4FixedRate)}
	,
	{NDIS_STRING_CONST("ArSysParam0"), "ArSysParam0", 0, 4, NdisParameterInteger, 0, 0,
	 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4ArSysParam0)}
	,
	{NDIS_STRING_CONST("ArSysParam1"), "ArSysParam1", 0, 4, NdisParameterInteger, 0, 0,
	 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4ArSysParam1)}
	,
	{NDIS_STRING_CONST("ArSysParam2"), "ArSysParam2", 0, 4, NdisParameterInteger, 0, 0,
	 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4ArSysParam2)}
	,
	{NDIS_STRING_CONST("ArSysParam3"), "ArSysParam3", 0, 4, NdisParameterInteger, 0, 0,
	 0xFFFFFFFF, GLUE_GET_REG_OFFSET(u4ArSysParam3)}
	,
	{NDIS_STRING_CONST("DisableRoaming"), "DisableRoaming", 0, 4, NdisParameterInteger, 0, 0, 1,
	 GLUE_GET_REG_OFFSET(fgDisRoaming)}
	,
	{NDIS_STRING_CONST("TxPwrValid"), "TxPwrValid", 0, 4, NdisParameterInteger, 0, 0, 1,
	 GLUE_GET_REG_OFFSET(ucTxPwrValid)}
	,
	{NDIS_STRING_CONST("Support5GBand"), "Support5GBand", 0, 4, NdisParameterInteger,
	 CFG_SUPPORT_AIS_5GHZ, 0, 1, GLUE_GET_REG_OFFSET(ucSupport5GBand)}
	,
	{NDIS_STRING_CONST("2G4BwFixed20M"), "2G4BwFixed20M", 0, 4, NdisParameterInteger, 0, 0, 1,
	 GLUE_GET_REG_OFFSET(uc2G4BwFixed20M)}
	,
	{NDIS_STRING_CONST("5GBwFixed20M"), "5GBwFixed20M", 0, 4, NdisParameterInteger, 0, 0, 1,
	 GLUE_GET_REG_OFFSET(uc5GBwFixed20M)}
	,
	{NDIS_STRING_CONST("Enable5GBand"), "Enable5GBand", 0, 4, NdisParameterInteger,
	 CFG_SUPPORT_AIS_5GHZ, 0, 1, GLUE_GET_REG_OFFSET(ucEnable5GBand)}
	,
};

/******************************************************************************
*                                 M A C R O S
*******************************************************************************
*/

/******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
*******************************************************************************
*/
WLAN_STATUS
windowsQueryInformation(IN P_GLUE_INFO_T prGlueInfo,
			IN P_WLAN_REQ_ENTRY prWlanReqEntry,
			IN PVOID pvInfoBuf,
			IN UINT_32 u4InfoBufLen,
			OUT PUINT_32 pu4QryInfoLen,
			IN PUINT_32 pu4ByteWritten,
			IN PUINT_32 pu4ByteNeeded, IN BOOLEAN fgIsGlueExtension);

WLAN_STATUS
windowsSetInformation(IN P_GLUE_INFO_T prGlueInfo,
		      IN P_WLAN_REQ_ENTRY prWlanReqEntry,
		      IN PVOID pvInfoBuf,
		      IN UINT_32 u4InfoBufLen,
		      OUT PUINT_32 pu4SetInfoLen,
		      IN PUINT_32 pu4ByteRead,
		      IN PUINT_32 pu4ByteNeeded, IN BOOLEAN fgIsGlueExtension);

/******************************************************************************
*                              F U N C T I O N S
*******************************************************************************
*/

#if DBG
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to check the order of the supported OIDs.
*
* \param pvAdapter[in]      Pointer of Adapter Data Structure
*
* \retval TRUE: The order is correct.
* \retval FALSE: The order is not correct.
*/
/*----------------------------------------------------------------------------*/
BOOLEAN reqCheckOrderOfSupportedOids(IN PVOID pvAdapter)
{
	UINT i;
	NDIS_OID previousOid;

	DEBUGFUNC("reqCheckOrderOfSupportedOids");

	DBGLOG(REQ, TRACE, ("\n"));

	previousOid = arWlanOidReqTable[0].rOid;

	for (i = 1; i < NUM_SUPPORTED_OIDS; i++) {
		if (previousOid > arWlanOidReqTable[i].rOid) {
			DBGLOG(REQ, ERROR, ("Incorrect order in nicReqTable: [%d] %s vs [%d] %s\n",
					    i - 1, arWlanOidReqTable[i - 1].pucOidName, i,
					    arWlanOidReqTable[i].pucOidName));

			ASSERT(0);

			return FALSE;
		}
	}

	return TRUE;
}				/* reqCheckOrderOfSupportedOids */
#endif


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to search desired OID.
*
* \param rOid[in]               Desired NDIS_OID
* \param ppWlanReqEntry[out]    Found registered OID entry
*
* \retval TRUE: Matched OID is found
* \retval FALSE: No matched OID is found
*/
/*----------------------------------------------------------------------------*/
BOOLEAN reqSearchSupportedOidEntry(IN NDIS_OID rOid, OUT P_WLAN_REQ_ENTRY * ppWlanReqEntry)
{
	INT i, j, k;

	i = 0;
	j = NUM_SUPPORTED_OIDS - 1;

	while (i <= j) {
		k = (i + j) / 2;

		if (rOid == arWlanOidReqTable[k].rOid) {
			*ppWlanReqEntry = &arWlanOidReqTable[k];
			return TRUE;
		} else if (rOid < arWlanOidReqTable[k].rOid) {
			j = k - 1;
		} else {
			i = k + 1;
		}
	}

	return FALSE;
}				/* reqSearchSupportedOidEntry */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function that returns information about
*        the capabilities and status of the driver and/or its network adapter.
*
* \param miniportAdapterContext[in]     Follow MSDN definition.
* \param oid[in]                        Follow MSDN definition.
* \param pvInfomationBuffer[in]         Follow MSDN definition.
* \param u4InformationBufferLength[in]  Follow MSDN definition.
* \param pu4ByteWritten[out]            Follow MSDN definition.
* \param pu4ByteNeeded[out]             Follow MSDN definition.
*
* \retval Follow MSDN definition.
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
mpQueryInformation(IN NDIS_HANDLE miniportAdapterContext,
		   IN NDIS_OID oid,
		   IN PVOID pvInfomationBuffer,
		   IN UINT_32 u4InformationBufferLength,
		   OUT PUINT_32 pu4ByteWritten, OUT PUINT_32 pu4ByteNeeded)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;
	P_WLAN_REQ_ENTRY prWlanReqEntry;
	NDIS_STATUS status;
	UINT_32 u4QueryInfoLen = u4InformationBufferLength;

	DEBUGFUNC("mpQueryInformation");

	*pu4ByteWritten = 0;
	*pu4ByteNeeded = 0;

	do {
		if (prGlueInfo->fgIsCardRemoved) {
			DBGLOG(REQ, WARN, ("Query OID: 0x%08x (card removed)\n", oid));
			status = NDIS_STATUS_NOT_ACCEPTED;
			break;
		} else if (!reqSearchSupportedOidEntry(oid, &prWlanReqEntry)) {
			DBGLOG(REQ, WARN, ("Query OID: 0x%08x (unknown)\n", oid));
			status = NDIS_STATUS_NOT_SUPPORTED;
			break;
		} else {
			DBGLOG(REQ, TRACE,
			       ("Query OID: 0x%08x , %s\n", oid, prWlanReqEntry->pucOidName));
		}

		ASSERT(prWlanReqEntry);
		if (prWlanReqEntry->pfOidQueryHandler == NULL) {
			DBGLOG(REQ, WARN, ("Query %s: Null query handler\n",
					   prWlanReqEntry->pucOidName));
			status = NDIS_STATUS_NOT_SUPPORTED;
			break;
		}

		if (prWlanReqEntry->fgQryBufLenChecking) {
			if (u4InformationBufferLength < prWlanReqEntry->u4InfoBufLen) {
				/* Not enough room in InformationBuffer. Punt */
				*pu4ByteNeeded = prWlanReqEntry->u4InfoBufLen;

				DBGLOG(REQ, WARN,
				       ("Query %s: Buffer too short (current=%d, needed=%d)\n",
					prWlanReqEntry->pucOidName, u4InformationBufferLength,
					*pu4ByteNeeded));
				status = NDIS_STATUS_INVALID_LENGTH;
				break;
			}
		}

		if (wlanQueryTestMode(prGlueInfo->prAdapter) == TRUE &&
		    prWlanReqEntry->eOidMethod == ENUM_OID_DRIVER_CORE &&
		    wlanIsHandlerAllowedInRFTest(prWlanReqEntry->pfOidQueryHandler,
						 FALSE) == FALSE) {
			DBGLOG(REQ, WARN,
			       ("Query %s: prohibited in RF test mode\n",
				prWlanReqEntry->pucOidName));
			status = NDIS_STATUS_NOT_SUPPORTED;
			break;
		}

		/* Check what kind of OID query method is used */
		if (prWlanReqEntry->eOidMethod == ENUM_OID_GLUE_ONLY) {
			status = prWlanReqEntry->pfOidQueryHandler(prGlueInfo,
								   pvInfomationBuffer,
								   u4InformationBufferLength,
								   &u4QueryInfoLen);
		} else if (prWlanReqEntry->eOidMethod == ENUM_OID_DRIVER_CORE) {
			status =
			    windowsQueryInformation(prGlueInfo,
						    prWlanReqEntry,
						    pvInfomationBuffer,
						    u4InformationBufferLength,
						    &u4QueryInfoLen,
						    pu4ByteWritten, pu4ByteNeeded, FALSE);
		} else if (prWlanReqEntry->eOidMethod == ENUM_OID_GLUE_EXTENSION) {
			status =
			    windowsQueryInformation(prGlueInfo,
						    prWlanReqEntry,
						    pvInfomationBuffer,
						    u4InformationBufferLength,
						    &u4QueryInfoLen,
						    pu4ByteWritten, pu4ByteNeeded, TRUE);
		} else {
			ASSERT(0);
		}

		/* In order to pass the WHQL test, we set the variable BytesNeeded no
		 * matter which status is returned.
		 */
		*pu4ByteNeeded = u4QueryInfoLen;

		switch (status) {
		case NDIS_STATUS_SUCCESS:
			if (u4QueryInfoLen > u4InformationBufferLength) {
				/* This case is added to solve the problem of both 32-bit
				   and 64-bit counters supported for the general
				   statistics OIDs. */
				*pu4ByteWritten = u4InformationBufferLength;
			} else {
				*pu4ByteWritten = u4QueryInfoLen;
			}
			break;
#if DBG
		case NDIS_STATUS_INVALID_LENGTH:
			DBGLOG(REQ, WARN, ("Query %s: Invalid length (current=%d, needed=%d)\n",
					   prWlanReqEntry->pucOidName, u4InformationBufferLength,
					   *pu4ByteNeeded));
			break;

		case NDIS_STATUS_INVALID_DATA:
			DBGLOG(REQ, WARN, ("Query %s: Invalid data\n", prWlanReqEntry->pucOidName));
			break;

		case NDIS_STATUS_ADAPTER_NOT_READY:
			DBGLOG(REQ, WARN, ("Query %s: Adapter not ready\n",
					   prWlanReqEntry->pucOidName));
			break;

		case NDIS_STATUS_NOT_SUPPORTED:
			DBGLOG(REQ, WARN, ("Query %s: OID not supported\n",
					   prWlanReqEntry->pucOidName));
			break;
		case NDIS_STATUS_PENDING:
			DBGLOG(REQ, LOUD, ("Query %s: OID return Pending.\n",
					   prWlanReqEntry->pucOidName));
			break;

		default:
			DBGLOG(REQ, WARN, ("Query %s: Failed (status=0x%08x)\n",
					   prWlanReqEntry->pucOidName, status));
#endif				/* DBG */
		}
	} while (FALSE);

	return status;
}				/* mpQueryInformation */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function that allows bound protocol
*        drivers, or NDIS, to request changes in the state information that
*        the miniport maintains for particular object identifiers, such as
*        changes in multicast addresses.
*
* \param miniportAdapterContext[in]     Follow MSDN definition.
* \param oid[in]                        Follow MSDN definition.
* \param pvInfomationBuffer[in]         Follow MSDN definition.
* \param u4InformationBufferLength[in]  Follow MSDN definition.
* \param pu4ByteRead[out]               Follow MSDN definition.
* \param pu4ByteNeeded[out]             Follow MSDN definition.
*
* \retval Follow MSDN definition.
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
mpSetInformation(IN NDIS_HANDLE miniportAdapterContext,
		 IN NDIS_OID oid,
		 IN PVOID pvInfomationBuffer,
		 IN UINT_32 u4InformationBufferLength,
		 OUT PUINT_32 pu4ByteRead, OUT PUINT_32 pu4ByteNeeded)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;
	P_WLAN_REQ_ENTRY prWlanReqEntry;
	NDIS_STATUS status;
	UINT_32 u4SetInfoLen;

	DEBUGFUNC("mpSetInformation");

	*pu4ByteRead = 0;
	*pu4ByteNeeded = 0;

	do {
		if (prGlueInfo->fgIsCardRemoved) {
			DBGLOG(REQ, WARN, ("Set OID: 0x%08x (card removed)\n", oid));
			status = NDIS_STATUS_NOT_ACCEPTED;
			break;
		} else if (!reqSearchSupportedOidEntry(oid, &prWlanReqEntry)) {
			DBGLOG(REQ, WARN, ("Set OID: 0x%08x (unknown)\n", oid));
			status = NDIS_STATUS_INVALID_OID;
			break;
		}
#if DBG
		else if (prWlanReqEntry->pucOidName[0]) {
			DBGLOG(REQ, INFO,
			       ("Set OID: 0x%08x , %s\n", oid, prWlanReqEntry->pucOidName));
		}
#endif

		if (prWlanReqEntry->pfOidSetHandler == NULL) {
			DBGLOG(REQ, WARN,
			       ("Set %s: Null set handler\n", prWlanReqEntry->pucOidName));
			status = NDIS_STATUS_INVALID_OID;
			break;
		}

		if (prWlanReqEntry->fgSetBufLenChecking) {
			if (u4InformationBufferLength != prWlanReqEntry->u4InfoBufLen) {
				*pu4ByteNeeded = prWlanReqEntry->u4InfoBufLen;
				DBGLOG(REQ, WARN,
				       ("Set %s: Invalid length (current=%d, needed=%d)\n",
					prWlanReqEntry->pucOidName, u4InformationBufferLength,
					*pu4ByteNeeded));
				status = NDIS_STATUS_INVALID_LENGTH;
				break;
			}
		}

		if (wlanQueryTestMode(prGlueInfo->prAdapter) == TRUE &&
		    prWlanReqEntry->eOidMethod == ENUM_OID_DRIVER_CORE &&
		    wlanIsHandlerAllowedInRFTest(prWlanReqEntry->pfOidSetHandler, TRUE) == FALSE) {
			DBGLOG(REQ, WARN,
			       ("Set %s: prohibited in RF test mode\n",
				prWlanReqEntry->pucOidName));
			status = NDIS_STATUS_NOT_SUPPORTED;
			break;
		}

		/* Check what kind of OID query method is used */
		if (prWlanReqEntry->eOidMethod == ENUM_OID_GLUE_ONLY) {
			status = prWlanReqEntry->pfOidSetHandler(prGlueInfo,
								 pvInfomationBuffer,
								 u4InformationBufferLength,
								 &u4SetInfoLen);
		} else if (prWlanReqEntry->eOidMethod == ENUM_OID_DRIVER_CORE) {
			status = windowsSetInformation(prGlueInfo,
						       prWlanReqEntry,
						       pvInfomationBuffer,
						       u4InformationBufferLength,
						       &u4SetInfoLen,
						       pu4ByteRead, pu4ByteNeeded, FALSE);

		} else if (prWlanReqEntry->eOidMethod == ENUM_OID_GLUE_EXTENSION) {
			status = windowsSetInformation(prGlueInfo,
						       prWlanReqEntry,
						       pvInfomationBuffer,
						       u4InformationBufferLength,
						       &u4SetInfoLen,
						       pu4ByteRead, pu4ByteNeeded, TRUE);
		} else {
			ASSERT(0);
		}

		switch (status) {
		case NDIS_STATUS_SUCCESS:
			*pu4ByteRead = u4SetInfoLen;
			break;

		case NDIS_STATUS_INVALID_LENGTH:
			*pu4ByteNeeded = u4SetInfoLen;

			DBGLOG(REQ, WARN, ("Set %s: Invalid length (current=%d, needed=%d)\n",
					   prWlanReqEntry->pucOidName, u4InformationBufferLength,
					   *pu4ByteNeeded));
			break;
#if DBG
		case NDIS_STATUS_INVALID_DATA:
			DBGLOG(REQ, WARN, ("Set %s: Invalid data\n", prWlanReqEntry->pucOidName));
			break;

		case NDIS_STATUS_NOT_SUPPORTED:
			DBGLOG(REQ, WARN, ("Set %s: OID not supported\n",
					   prWlanReqEntry->pucOidName));
			break;

		case NDIS_STATUS_PENDING:
			DBGLOG(REQ, LOUD, ("Set %s: OID return Pending.\n",
					   prWlanReqEntry->pucOidName));
			break;

		default:
			DBGLOG(REQ, WARN, ("Set %s: Failed (status=0x%08x)\n",
					   prWlanReqEntry->pucOidName, status));
#endif
		}
	} while (FALSE);

	return status;
}				/* mpSetInformation */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called to read Windows registry parameters
*
* \param prGlueInfo[in]                     Pointer to GLUE structure
* \param wrapperConfigurationContext[in]    Specifies a handle used only in
*               MiniportInitialize() for calls to NdisXXX configuration and
*               initialization functions.
*
* \retval
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
windowsReadRegistryParameters(IN P_GLUE_INFO_T prGlueInfo,
			      IN NDIS_HANDLE wrapperConfigurationContext)
{
#define REG_NAME_MAX_LEN    32

	NDIS_STATUS rStatus = NDIS_STATUS_SUCCESS;
	NDIS_HANDLE configurationHandle;
	P_WLAN_REG_ENTRY_T prRegEntry;
	UINT i;
	PNDIS_CONFIGURATION_PARAMETER returnedValue_p;
	UINT j;
	UINT_8 ansiRegName[REG_NAME_MAX_LEN];
	UINT_16 u2StrLen;
	PUINT_8 pucNetworkAddress;
	UINT_32 u4NetworkAddrLeng;
	PUINT_32 pc;

	DEBUGFUNC("windowsReadRegistryParameters");

	DBGLOG(INIT, TRACE, ("\n"));

	/* Open the registry for this adapter */
	NdisOpenConfiguration(&rStatus, &configurationHandle, wrapperConfigurationContext);

	if (rStatus != NDIS_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, ("NdisOpenConfiguration failed (status = 0x%x)\n", rStatus));

		return rStatus;
	}

	/* Read NetworkAddress registry value */
	/* Use it as the current address if any */
	NdisReadNetworkAddress(&rStatus,
			       (PVOID) & pucNetworkAddress,
			       &u4NetworkAddrLeng, configurationHandle);

	/* If there is a NetworkAddress override in registry, use it */
	if (rStatus == NDIS_STATUS_SUCCESS && u4NetworkAddrLeng == PARAM_MAC_ADDR_LEN) {
		DBGLOG(REQ, INFO, ("NetworkAddress            %02x-%02x-%02x-%02x-%02x-%02x\n",
				   pucNetworkAddress[0], pucNetworkAddress[1], pucNetworkAddress[2],
				   pucNetworkAddress[3], pucNetworkAddress[4],
				   pucNetworkAddress[5]));

		if (pucNetworkAddress[0] & BIT(0)) {
			DBGLOG(REQ, WARN,
			       ("Overriding NetworkAddress is invalid - %02x-%02x-%02x-%02x-%02x-%02x\n",
				pucNetworkAddress[0], pucNetworkAddress[1], pucNetworkAddress[2],
				pucNetworkAddress[3], pucNetworkAddress[4], pucNetworkAddress[5]));
			kalMemZero((PVOID) ((PUINT_8) prGlueInfo + GLUE_GET_REG_OFFSET(aucMacAddr)),
				   PARAM_MAC_ADDR_LEN);
		} else {
			kalMemCopy((PVOID) ((PUINT_8) prGlueInfo + GLUE_GET_REG_OFFSET(aucMacAddr)),
				   (PVOID) pucNetworkAddress, PARAM_MAC_ADDR_LEN);
		}
	} else {
		/* set LSB to indicate it's a invalid address */
		prGlueInfo->rRegInfo.aucMacAddr[0] = BIT(0);
	}

	rStatus = NDIS_STATUS_SUCCESS;

	/* Read all the registry values. */
	prRegEntry = arWlanRegTable;
	for (i = 0; i < NUM_REG_PARAMS; i++, prRegEntry++) {
		for (j = 0; j < (UINT) (prRegEntry->regName.Length / 2) &&
		     j < REG_NAME_MAX_LEN - 1; j++) {
			ansiRegName[j] = (UINT_8) prRegEntry->regName.Buffer[j];
		}
		ansiRegName[j] = '\0';

		/* Get the configuration value for a specific parameter.  Under NT the
		 * parameters are all read in as DWORDs.
		 */
		NdisReadConfiguration(&rStatus, &returnedValue_p, configurationHandle,
				      &prRegEntry->regName, prRegEntry->ParameterType);

		/* If the parameter was present, then check its value for validity. */
		if (rStatus == NDIS_STATUS_SUCCESS) {
			/* Check that param value is not too small or too large */
			switch (prRegEntry->ParameterType) {
			case NdisParameterBinary:
				if (returnedValue_p->ParameterData.BinaryData.Length == 4) {
					if (prRegEntry->fieldSize == 4) {
						pc = (PUINT_32) returnedValue_p->ParameterData.
						    BinaryData.Buffer;
						*(PUINT_32) ((PUINT_8) prGlueInfo +
							     prRegEntry->u4GlueOffset) =
						    (UINT_32) pc[0];
						DBGLOG(INIT, TRACE,
						       ("%-25s 0x%08x\n", ansiRegName,
							(UINT_32) *
							(PUINT_32) ((PUINT_8) prGlueInfo +
								    prRegEntry->u4GlueOffset)));
					} else {
						DBGLOG(INIT, ERROR,
						       ("Driver Support only 4 bytes binary to formart as DWORD Hex value,"
							"Add extra binary format here!!\n"));
					}
				} else {
					if (prRegEntry->fieldSize == 4) {
						/* Here to read the field size of 4 for already released inf file
						   with DWORD value now using binary format, for BWCS-related registry */
						NdisReadConfiguration(&rStatus, &returnedValue_p,
								      configurationHandle,
								      &prRegEntry->regName,
								      NdisParameterHexInteger);
						if (rStatus == NDIS_STATUS_SUCCESS) {
							if ((UINT) returnedValue_p->ParameterData.
							    IntegerData < prRegEntry->min
							    || (UINT) returnedValue_p->
							    ParameterData.IntegerData >
							    prRegEntry->max) {
								*(PUINT_32) ((PUINT_8) prGlueInfo +
									     prRegEntry->
									     u4GlueOffset) =
								    prRegEntry->rDefaultValue.
								    u4IntData;
							} else {	/* Use the value if it is within range */
								*(PUINT_32) ((PUINT_8) prGlueInfo +
									     prRegEntry->
									     u4GlueOffset) =
								    returnedValue_p->ParameterData.
								    IntegerData;
							}
							DBGLOG(INIT, TRACE,
							       ("%-25s 0x%08x\n", ansiRegName,
								*(PUINT_32) ((PUINT_8) prGlueInfo +
									     prRegEntry->
									     u4GlueOffset)));
						}
					} else if (prRegEntry->fieldSize == 1) {
						*(PINT_8) ((PUINT_8) prGlueInfo +
							   prRegEntry->u4GlueOffset) =
						    (*(PINT_8)
						     (returnedValue_p->ParameterData.BinaryData.
						      Buffer));

						DBGLOG(INIT, TRACE, ("%-25s 0x%08x\n", ansiRegName,
								     *(PINT_8) ((PUINT_8) prGlueInfo
										+
										prRegEntry->
										u4GlueOffset)));
					} else {
						DBGLOG(INIT, ERROR,
						       ("Driver Support only 1/4 bytes binary to formart as DWORD Hex value,"
							"Add extra binary format here!!\n"));
					}
				}
				break;

			case NdisParameterHexInteger:
			case NdisParameterInteger:
				if ((UINT) returnedValue_p->ParameterData.IntegerData <
				    prRegEntry->min ||
				    (UINT) returnedValue_p->ParameterData.IntegerData >
				    prRegEntry->max) {
					*(PUINT_32) ((PUINT_8) prGlueInfo +
						     prRegEntry->u4GlueOffset) =
					    prRegEntry->rDefaultValue.u4IntData;
				} else {	/* Use the value if it is within range */
					*(PUINT_32) ((PUINT_8) prGlueInfo +
						     prRegEntry->u4GlueOffset) =
					    returnedValue_p->ParameterData.IntegerData;
				}
				DBGLOG(INIT, TRACE, ("%-25s 0x%08x\n", ansiRegName,
						     *(PUINT_32) ((PUINT_8) prGlueInfo +
								  prRegEntry->u4GlueOffset)));
				break;

			case NdisParameterString:
			case NdisParameterMultiString:
				u2StrLen = returnedValue_p->ParameterData.StringData.Length;
				if (u2StrLen < prRegEntry->min || u2StrLen > prRegEntry->max) {
					DBGLOG(INIT, TRACE, ("Out of range: %d, min: %d, max:%d\n",
							     u2StrLen,
							     prRegEntry->min, prRegEntry->max));
					kalMemCopy((PUINT_8) prGlueInfo + prRegEntry->u4GlueOffset,
						   prRegEntry->rDefaultValue.pucStrData,
						   prRegEntry->fieldSize);
				} else {	/* Use the value if it is within range */
					PUINT_8 pucTerm;

					kalMemCopy((PUINT_8) prGlueInfo + prRegEntry->u4GlueOffset,
						   (PUINT_8) &returnedValue_p->ParameterData.
						   StringData.Buffer[0], u2StrLen);

					pucTerm =
					    (PUINT_8) prGlueInfo + prRegEntry->u4GlueOffset +
					    u2StrLen;

					*(pucTerm) = '\0';
					*(pucTerm + 1) = '\0';
				}
				DBGLOG(INIT, TRACE, ("%-25s :%ws\n", ansiRegName,
						     ((PUINT_8) prGlueInfo +
						      prRegEntry->u4GlueOffset)));
				break;

			default:
				DBGLOG(INIT, ERROR,
				       ("parameter type error %d\n", prRegEntry->ParameterType));
				break;
			}

		} else if (prRegEntry->mandatory) {
			/* If a mandatory parameter wasn't present then error out. */
			DBGLOG(REQ, ERROR, ("Could not find mandantory '%s' in registry\n\n",
					    ansiRegName));
			rStatus = NDIS_STATUS_FAILURE;
			break;
		} else {
			/* If a non-mandatory parameter wasn't present, then set it to its
			   default value. */
			switch (prRegEntry->ParameterType) {
			case NdisParameterBinary:
			case NdisParameterHexInteger:
			case NdisParameterInteger:
				if (prRegEntry->fieldSize == 4) {
					*(PUINT_32) ((PUINT_8) prGlueInfo +
						     prRegEntry->u4GlueOffset) =
					    prRegEntry->rDefaultValue.u4IntData;

					DBGLOG(INIT, TRACE, ("%-25s 0x%08x\n", ansiRegName,
							     *(PUINT_32) ((PUINT_8) prGlueInfo +
									  prRegEntry->
									  u4GlueOffset)));
				} else if (prRegEntry->fieldSize == 1) {
					*(PINT_8) ((PUINT_8) prGlueInfo +
						   prRegEntry->u4GlueOffset) =
					    (INT_8) (prRegEntry->rDefaultValue.u4IntData);

					DBGLOG(INIT, TRACE, ("%-25s 0x%08x\n", ansiRegName,
							     *(PINT_8) ((PUINT_8) prGlueInfo +
									prRegEntry->u4GlueOffset)));
				}
				break;
			case NdisParameterString:
			case NdisParameterMultiString:
				if (prRegEntry->rDefaultValue.pucStrData) {
					memcpy((PUINT_8) prGlueInfo + prRegEntry->u4GlueOffset,
					       prRegEntry->rDefaultValue.pucStrData,
					       prRegEntry->fieldSize * sizeof(WCHAR));
					DBGLOG(INIT, TRACE, ("%-25s \"%ws\"\n", ansiRegName,
							     ((PUINT_8) prGlueInfo +
							      prRegEntry->u4GlueOffset)));
				}
				break;
			default:
				DBGLOG(INIT, ERROR, ("parameter type error %d\n",
						     prRegEntry->ParameterType));
				break;
			}

			rStatus = NDIS_STATUS_SUCCESS;
		}
	}

	/* Close the registry */
	NdisCloseConfiguration(configurationHandle);

	return rStatus;
}				/* nicReadRegistryParameters */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to return supported NDIS_OIDs.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO structure
* \param[out] pvQryBuf      A pointer to the buffer that holds the result of
*                           the query
* \param[in] u4QryBufLen    The length of the query buffer
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQuerySupportedList(IN P_GLUE_INFO_T prGlueInfo,
		      OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	UINT i;

	DEBUGFUNC("reqQuerySupportedList");

	*pu4QryInfoLen = NUM_SUPPORTED_OIDS * sizeof(NDIS_OID);

	/* Check if the query buffer is large enough to hold all the query
	   information. */
	if (u4QryBufLen < *pu4QryInfoLen) {
		/* Not enough room for the query information. */
		return WLAN_STATUS_INVALID_LENGTH;
	}

	for (i = 0; i < NUM_SUPPORTED_OIDS; i++) {
		*((PNDIS_OID) pvQryBuf + i) = arWlanOidReqTable[i].rOid;
	}

	DBGLOG(REQ, LOUD, ("Supported OIDs:\n"));
	DBGLOG_MEM32(REQ, LOUD, pvQryBuf, *pu4QryInfoLen);

	return WLAN_STATUS_SUCCESS;
}				/* reqQuerySupportedList */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query specific OID information
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO structure
* \param[in] prWlanReqEntry Target registry entry to process
* \param[in] pvInfoBuf      Points to a buffer in which we should return
*                           queried information
* \param[in] u4InfoBufLen   Specifies the number of bytes at pvInfoBuf
* \param[out]pu4QryInfoLen  Points to a variable that number of bytes it is
*                           returning at pvInfoBuf.
* \param[in] pu4ByteWritten For XP, as arguments for OID Thread
* \param[in] pu4ByteNeeded  For XP, as arguments for OID Thread
* \parma[in] fgIsGlueExtension TRUE: glue layer can provide all inform
*                              FALSE: need core library to provide inform
*
* \retval WLAN_STATUS_SUCCESS Query inform is filled in buf successfully
* \retval NDIS_STATUS_PENDING Query process is pending for OID thread (XP only)
* \retval Others              Fail for some reason
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
windowsQueryInformation(P_GLUE_INFO_T prGlueInfo,
			P_WLAN_REQ_ENTRY prWlanReqEntry,
			IN PVOID pvInfoBuf,
			IN UINT_32 u4InfoBufLen,
			OUT PUINT_32 pu4QryInfoLen,
			IN PUINT_32 pu4ByteWritten,
			IN PUINT_32 pu4ByteNeeded, IN BOOLEAN fgIsGlueExtension)
{
	ASSERT(prGlueInfo);
	ASSERT(prWlanReqEntry);

	if (wlanIsHandlerNeedHwAccess(prWlanReqEntry->pfOidQueryHandler, FALSE) == FALSE) {
		return wlanQueryInformation(prGlueInfo->prAdapter,
					    prWlanReqEntry->pfOidQueryHandler,
					    pvInfoBuf, u4InfoBufLen, pu4QryInfoLen);
	}

	ASSERT(!(prGlueInfo->rHifInfo.u4ReqFlag & REQ_FLAG_OID));

	GLUE_INC_REF_CNT(prGlueInfo->i4OidPendingCount);
	ASSERT(prGlueInfo->i4OidPendingCount <= 1);

	prGlueInfo->fgSetOid = FALSE;
	prGlueInfo->pvInformationBuffer = pvInfoBuf;
	prGlueInfo->u4InformationBufferLength = u4InfoBufLen;
	prGlueInfo->pu4BytesReadOrWritten = pu4ByteWritten;
	prGlueInfo->pu4BytesNeeded = pu4ByteNeeded;
	prGlueInfo->pvOidEntry = prWlanReqEntry;
	prGlueInfo->fgIsGlueExtension = fgIsGlueExtension;

	_InterlockedOr(&prGlueInfo->rHifInfo.u4ReqFlag, REQ_FLAG_OID);
#if 0
	KeSetEvent(&prGlueInfo->rHifInfo.rOidReqEvent, EVENT_INCREMENT, FALSE);
#else
/* DBGLOG(INIT, TRACE, ("NdisSetEvent\n")); */
	NdisSetEvent(&prGlueInfo->rTxReqEvent);
#endif

	return NDIS_STATUS_PENDING;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set specific OID information
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO structure
* \param[in] prWlanReqEntry Target registry entry to process
* \param[in] pvInfoBuf      Points to a buffer in which we should return
*                           queried information
* \param[in] u4InfoBufLen   Specifies the number of bytes at pvInfoBuf
* \param[out]pu4SetInfoLen  Points to a variable that number of bytes it is
*                           returning at pvInfoBuf.
* \param[in] pu4ByteRead    For XP, as arguments for OID Thread
* \param[in] pu4ByteNeeded  For XP, as arguments for OID Thread
* \parma[in] fgIsGlueExtension TRUE: glue layer can provide all inform
*                              FALSE: need core library to provide inform
*
* \retval WLAN_STATUS_SUCCESS Set inform is filled in buf successfully
* \retval NDIS_STATUS_PENDING Set process is pending for OID thread (XP only)
* \retval Others              Fail for some reason
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
windowsSetInformation(IN P_GLUE_INFO_T prGlueInfo,
		      IN P_WLAN_REQ_ENTRY prWlanReqEntry,
		      IN PVOID pvInfoBuf,
		      IN UINT_32 u4InfoBufLen,
		      OUT PUINT_32 pu4SetInfoLen,
		      OUT PUINT_32 pu4ByteRead,
		      OUT PUINT_32 pu4ByteNeeded, IN BOOLEAN fgIsGlueExtension)
{
	if (wlanIsHandlerNeedHwAccess(prWlanReqEntry->pfOidSetHandler, TRUE) == FALSE) {
		return wlanSetInformation(prGlueInfo->prAdapter,
					  prWlanReqEntry->pfOidSetHandler,
					  pvInfoBuf, u4InfoBufLen, pu4SetInfoLen);
	}

	ASSERT(!(prGlueInfo->rHifInfo.u4ReqFlag & REQ_FLAG_OID));

	GLUE_INC_REF_CNT(prGlueInfo->i4OidPendingCount);
	ASSERT(prGlueInfo->i4OidPendingCount <= 1);

	prGlueInfo->fgSetOid = TRUE;
	prGlueInfo->pvInformationBuffer = pvInfoBuf;
	prGlueInfo->u4InformationBufferLength = u4InfoBufLen;
	prGlueInfo->pu4BytesReadOrWritten = pu4ByteRead;
	prGlueInfo->pu4BytesNeeded = pu4ByteNeeded;
	prGlueInfo->pvOidEntry = prWlanReqEntry;
	prGlueInfo->fgIsGlueExtension = fgIsGlueExtension;

	_InterlockedOr(&prGlueInfo->rHifInfo.u4ReqFlag, REQ_FLAG_OID);
#if 0
	KeSetEvent(&prGlueInfo->rHifInfo.rOidReqEvent, EVENT_INCREMENT, FALSE);
#else
/* DBGLOG(INIT, TRACE, ("NdisSetEvent\n")); */
	NdisSetEvent(&prGlueInfo->rTxReqEvent);
#endif

	return NDIS_STATUS_PENDING;
}
