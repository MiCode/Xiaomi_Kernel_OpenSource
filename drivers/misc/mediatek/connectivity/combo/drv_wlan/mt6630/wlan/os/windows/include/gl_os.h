/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/include/gl_os.h#1 $
*/

/*! \file   gl_os.h
    \brief  List the external reference to OS for GLUE Layer.

    In this file we define the data structure - GLUE_INFO_T to store those objects
    we acquired from OS - e.g. TIMER, SPINLOCK, NET DEVICE ... . And all the
    external reference (header file, extern func() ..) to OS for GLUE Layer should
    also list down here.
*/



/*
** $Log: gl_os.h $
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
 *
 * 01 05 2012 wh.su
 * [WCXRP00001153] [MT6620 Wi-Fi][Driver] Adding the get_ch_list and set_tx_power proto type function
 * Adding the related ioctl / wlan oid function to set the Tx power cfg.
 *
 * 03 17 2011 chinglan.wang
 * [WCXRP00000570] [MT6620 Wi-Fi][Driver] Add Wi-Fi Protected Setup v2.0 feature
 * .
 *
 * 11 03 2010 wh.su
 * [WCXRP00000124] [MT6620 Wi-Fi] [Driver] Support the dissolve P2P Group
 * Refine the HT rate disallow TKIP pairwise cipher .
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
 * 09 03 2010 kevin.huang
 * NULL
 * Refine #include sequence and solve recursive/nested #include issue
 *
 * 08 31 2010 kevin.huang
 * NULL
 * Use LINK LIST operation to process SCAN result
 *
 * 08 16 2010 cp.wu
 * NULL
 * P2P packets are now marked when being queued into driver, and identified later without checking MAC address
 *
 * 08 11 2010 cp.wu
 * NULL
 * 1) do not use in-stack variable for beacon updating. (for MAUI porting)
 * 2) extending scanning result to 64 instead of 48
 *
 * 07 20 2010 wh.su
 *
 * adding the wapi code.
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 24 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 802.1x and bluetooth-over-Wi-Fi security frames are now delievered to firmware via command path instead of data path.
 *
 * 06 07 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * move bss related data types to wlan_def.h to avoid recursive dependency.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 04 27 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * identify BT Over Wi-Fi Security frame and mark it as 802.1X frame
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * rWlanInfo should be placed at adapter rather than glue due to most operations
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  * are done in adapter layer.
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * ePowerCtrl is not necessary as a glue variable.
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code refine: fgTestMode should be at adapter rather than glue due to the device/fw is also involved
 *
 * 03 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add indication for media stream mode change
 *
 * 03 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) correct OID_802_11_CONFIGURATION with frequency setting behavior.
 *  *  *  *  * the frequency is used for adhoc connection only
 *  *  *  *  * 2) update with SD1 v0.9 CMD/EVENT documentation
 *
 * 03 25 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add Bluetooth-over-Wifi frame header check
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) implement timeout mechanism when OID is pending for longer than 1 second
 *  *  *  *  *  *  *  * 2) allow OID_802_11_CONFIGURATION to be executed when RF test mode is turned on
 *
 * 01 27 2010 wh.su
 * [WPD00003816][MT6620 Wi-Fi] Adding the security support
 * .
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 2. block TX/ordinary OID when RF test mode is engaged
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  * 4. correct some HAL implementation
 *
 * 01 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * Under WinXP with SDIO, use prGlueInfo->rHifInfo.pvInformationBuffer instead of prGlueInfo->pvInformationBuffer
 *
 * 01 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement following 802.11 OIDs:
 *  *  *  *  *  *  * OID_802_11_RSSI,
 *  *  *  *  *  *  * OID_802_11_RSSI_TRIGGER,
 *  *  *  *  *  *  * OID_802_11_STATISTICS,
 *  *  *  *  *  *  * OID_802_11_DISASSOCIATE,
 *  *  *  *  *  *  * OID_802_11_POWER_MODE
 *
 * 01 21 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement OID_802_11_MEDIA_STREAM_MODE
 *
 * 01 21 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement OID_802_11_SUPPORTED_RATES / OID_802_11_DESIRED_RATES
 *
 * 01 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * .implement Set/Query BeaconInterval/AtimWindow
**  \main\maintrunk.MT6620WiFiDriver_Prj\19 2009-12-10 16:44:57 GMT mtk02752
**  code clean
**  \main\maintrunk.MT6620WiFiDriver_Prj\18 2009-11-30 17:32:08 GMT mtk02752
**  add ENUM_PARAM_OP_MODE_T into GL_WLAN_INFO_T
**  \main\maintrunk.MT6620WiFiDriver_Prj\17 2009-11-25 18:20:10 GMT mtk02752
**  GL_WLAN_INFO_T can hold up to 48 entries of scan result
**  \main\maintrunk.MT6620WiFiDriver_Prj\16 2009-11-24 22:43:11 GMT mtk02752
**  modify GL_WLAN_INFO_T
**  \main\maintrunk.MT6620WiFiDriver_Prj\15 2009-11-23 20:32:31 GMT mtk02752
**  add a field to memory connection status to follow MSDN (after 10 second then indicate disconnection)
**  \main\maintrunk.MT6620WiFiDriver_Prj\14 2009-11-13 10:48:57 GMT mtk02752
**  for basic oid request
**  \main\maintrunk.MT6620WiFiDriver_Prj\13 2009-10-29 20:01:56 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\12 2009-10-13 21:59:39 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\11 2009-09-09 17:26:32 GMT mtk01084
**  add DDK related variable
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-05-12 09:42:19 GMT mtk01461
**  Add RESET flag
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-04-21 09:42:57 GMT mtk01461
**  Add OID information in GLUE_INFO_T for NdisMSet/QueryInfomationComplete()
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-03-23 22:02:06 GMT mtk01461
**  Refine the GLUE macro for accessing NDIS_PACKET's reserved fields.
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-03-23 00:35:18 GMT mtk01461
**  Fix Lint warning - GLUE_GET_PKT_DESCRIPTOR
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-03-18 20:27:36 GMT mtk01461
**  Fix LINT warning introduced by SPIN_LOCK macro
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-03-17 10:44:35 GMT mtk01426
**  Move TxServiceThread to Kal layer
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-16 16:48:48 GMT mtk01461
**  Modify the GLUE_SPIN_LOCK macro
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-16 09:12:54 GMT mtk01461
**  Update TX PATH API
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:40:46 GMT mtk01426
**  Init for develop
**
*/


#ifndef _GL_OS_H
#define _GL_OS_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "config.h"

LINT_EXT_HEADER_BEGIN
#include <ndis.h>
    LINT_EXT_HEADER_END
#include "version.h"
#include "gl_typedef.h"
#include "typedef.h"
#include "queue.h"
#include "debug.h"
#include "CFG_Wifi_File.h"
#include "gl_kal.h"
    LINT_EXT_HEADER_BEGIN
#include "hif.h"
    LINT_EXT_HEADER_END
#include "wlan_lib.h"
#include "gl_req.h"
#include "wlan_oid.h"
extern BOOLEAN fgIsBusAccessFailed;

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/* Flags for glue layer status */
#define GLUE_FLAG_WLAN_PROBE                    0x00000001	/* NIC is probed                   */
#define GLUE_FLAG_MAP_REGISTER                  0x00000002	/* Register is ready               */
#define GLUE_FLAG_INTERRUPT_IN_USE              0x00000004	/* Interrupt is workable           */
#define GLUE_FLAG_PAYLOAD_POOL                  0x00000008	/* Payload buffer pool is ready    */
#define GLUE_FLAG_PKT_POOL                      0x00000010	/* Packet descriptor pool is ready */
#define GLUE_FLAG_PKT_DESCR                     0x00000020	/* Packet descriptors are ready    */
#define GLUE_FLAG_BUF_POOL                      0x00000040	/* Buffer descriptor pool is ready */
#define GLUE_FLAG_SPIN_LOCK                     0x00000080	/* Spin lock allocated             */
#define GLUE_FLAG_HALT                          0x00000100	/* NIC is going to halt            */
#define GLUE_FLAG_RESET                         0x00000200	/* NIC is going to reset           */
#define GLUE_FLAG_TIMEOUT                       0x00000400	/* NIC response timeout            */


/* Error log codes */
/* NDIS_ERROR_CODE_ADAPTER_NOT_FOUND (Event ID: 5003) */
#define ERRLOG_READ_PCI_SLOT_FAILED             0x00000101L
#define ERRLOG_WRITE_PCI_SLOT_FAILED            0x00000102L
#define ERRLOG_VENDOR_DEVICE_NOMATCH            0x00000103L

/* NDIS_ERROR_CODE_ADAPTER_DISABLED (Event ID: 5014) */
#define ERRLOG_BUS_MASTER_DISABLED              0x00000201L
#define ERRLOG_SET_LATENCY_TIMER_FAILED         0x00000202L

/* NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION (Event ID: 5009) */
#define ERRLOG_INVALID_SPEED_DUPLEX             0x00000301L
#define ERRLOG_SET_SECONDARY_FAILED             0x00000302L

/* NDIS_ERROR_CODE_OUT_OF_RESOURCES (Event ID: 5001) */
#define ERRLOG_OUT_OF_MEMORY                    0x00000401L
#define ERRLOG_OUT_OF_SHARED_MEMORY             0x00000402L
#define ERRLOG_OUT_OF_MAP_REGISTERS             0x00000403L
#define ERRLOG_OUT_OF_BUFFER_POOL               0x00000404L
#define ERRLOG_OUT_OF_NDIS_BUFFER               0x00000405L
#define ERRLOG_OUT_OF_PACKET_POOL               0x00000406L
#define ERRLOG_OUT_OF_NDIS_PACKET               0x00000407L
#define ERRLOG_OUT_OF_LOOKASIDE_MEMORY          0x00000408L

/* NDIS_ERROR_CODE_HARDWARE_FAILURE (Event ID: 5002) */
#define ERRLOG_UNSUPPORTED_CHIP_ID_REV          0x00000501L
#define ERRLOG_UNSUPPORTED_RF_CTRL_MODE         0x00000502L
#define ERRLOG_SELFTEST_FAILED                  0x00000503L
#define ERRLOG_INITIALIZE_ADAPTER               0x00000504L
#define ERRLOG_REMOVE_MINIPORT                  0x00000505L
#define ERRLOG_TFIFO_UNDERFLOW                  0x00000506L
#define ERRLOG_MMI_FATAL_ERROR                  0x00000507L
#define ERRLOG_BCI_FATAL_ERROR                  0x00000508L

/* NDIS_ERROR_CODE_RESOURCE_CONFLICT (Event ID: 5000) */
#define ERRLOG_MAP_IO_SPACE                     0x00000601L
#define ERRLOG_QUERY_ADAPTER_RESOURCES          0x00000602L
#define ERRLOG_NO_IO_RESOURCE                   0x00000603L
#define ERRLOG_NO_INTERRUPT_RESOURCE            0x00000604L
#define ERRLOG_NO_MEMORY_RESOURCE               0x00000605L
#define ERRLOG_REG_IO_PORT_RANGE                0x00000606L


#define NUM_SUPPORTED_OIDS      (sizeof(arWlanOidReqTable) / sizeof(WLAN_REQ_ENTRY))
#define NUM_REG_PARAMS          (sizeof(arWlanRegTable) / sizeof(WLAN_REG_ENTRY_T))


#define ETHERNET_HEADER_SZ                      14
#define ETHERNET_MAX_PKT_SZ                     1514

#define MAX_ARRAY_SEND_PACKETS                  8

#define SOURCE_PORT_LEN                         2
/* NOTE(Kevin): Without IP Option Length */
#define LOOK_AHEAD_LEN                          (ETHER_HEADER_LEN + IPV4_HDR_LEN + SOURCE_PORT_LEN)

#if 0				/* Avoid multiple definition */
#define ETH_HLEN                                14
#define ETH_TYPE_LEN_LEN                        2
#define ETH_TYPE_LEN_OFFSET                     12
#define ETH_P_IP                                0x0800
#define ETH_P_1X                                0x888E
#define ETH_P_PRE_1X                            0x88C7

#if CFG_SUPPORT_WAPI
#define ETH_WPI_1X                              0x88B4
#endif

#define IPVERSION                               4
#define IP_HEADER_LEN                           20

#define IPVH_VERSION_OFFSET                     4	/* For Little-Endian */
#define IPVH_VERSION_MASK                       0xF0
#define IPTOS_PREC_OFFSET                       5
#define IPTOS_PREC_MASK                         0xE0

#define USER_PRIORITY_DEFAULT                   0


/* 802.2 LLC/SNAP */
#define ETH_LLC_OFFSET                          (ETH_HLEN)
#define ETH_LLC_LEN                             3
#define ETH_LLC_DSAP_SNAP                       0xAA
#define ETH_LLC_SSAP_SNAP                       0xAA
#define ETH_LLC_CONTROL_UNNUMBERED_INFORMATION  0x03

/* Bluetooth SNAP */
#define ETH_SNAP_OFFSET                         (ETH_HLEN + ETH_LLC_LEN)
#define ETH_SNAP_LEN                            5
#define ETH_SNAP_BT_SIG_OUI_0                   0x00
#define ETH_SNAP_BT_SIG_OUI_1                   0x19
#define ETH_SNAP_BT_SIG_OUI_2                   0x58

#define BOW_PROTOCOL_ID_SECURITY_FRAME          0x0003
#endif
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
LINT_EXT_HEADER_BEGIN
    typedef WLAN_STATUS(*PFN_OID_HANDLER_FUNC_REQ) (IN PVOID prAdapter,
						    IN OUT PVOID pvBuf,
						    IN UINT_32 u4BufLen,
						    OUT PUINT_32 pu4OutInfoLen);

typedef enum _ENUM_OID_METHOD_T {
	ENUM_OID_GLUE_ONLY,
	ENUM_OID_GLUE_EXTENSION,
	ENUM_OID_DRIVER_CORE
} ENUM_OID_METHOD_T, *P_ENUM_OID_METHOD_T;

typedef enum _ENUM_NET_PORT_IDX_T {
	NET_PORT_WLAN_IDX = 0,
	NET_PORT_P2P0_IDX,
	NET_PORT_P2P1_IDX,
	NET_PORT_P2P2_IDX,
	NET_PORT_BOW_IDX,
	NET_PORT_NUM
} ENUM_NET_PORT_IDX_T;

typedef enum _ENUM_NET_DEV_IDX_T {
	NET_DEV_WLAN_IDX = 0,
	NET_DEV_P2P_IDX,
	NET_DEV_BOW_IDX,
	NET_DEV_NUM
} ENUM_NET_DEV_IDX_T;

/* OID set/query processing entry */
typedef struct _WLAN_REQ_ENTRY {
	NDIS_OID rOid;		/* OID */
	PUINT_8 pucOidName;	/* OID name text */
	BOOLEAN fgQryBufLenChecking;
	BOOLEAN fgSetBufLenChecking;
	ENUM_OID_METHOD_T eOidMethod;
	UINT_32 u4InfoBufLen;
	PFN_OID_HANDLER_FUNC_REQ pfOidQueryHandler;
	PFN_OID_HANDLER_FUNC_REQ pfOidSetHandler;
} WLAN_REQ_ENTRY, *P_WLAN_REQ_ENTRY;


#if CFG_TCP_IP_CHKSUM_OFFLOAD
/* The offload capabilities of the miniport */
typedef struct _NIC_TASK_OFFLOAD {
	UINT_32 ChecksumOffload:1;
	UINT_32 LargeSendOffload:1;
	UINT_32 IpSecOffload:1;

} NIC_TASK_OFFLOAD;


/* Checksum offload capabilities */
typedef struct _NIC_CHECKSUM_OFFLOAD {
	UINT_32 DoXmitTcpChecksum:1;
	UINT_32 DoRcvTcpChecksum:1;
	UINT_32 DoXmitUdpChecksum:1;
	UINT_32 DoRcvUdpChecksum:1;
	UINT_32 DoXmitIpChecksum:1;
	UINT_32 DoRcvIpChecksum:1;

} NIC_CHECKSUM_OFFLOAD;
#endif				/* CFG_TCP_IP_CHKSUM_OFFLOAD */

typedef enum _ENUM_RSSI_TRIGGER_TYPE {
	ENUM_RSSI_TRIGGER_NONE,
	ENUM_RSSI_TRIGGER_GREATER,
	ENUM_RSSI_TRIGGER_LESS,
	ENUM_RSSI_TRIGGER_TRIGGERED,
	ENUM_RSSI_TRIGGER_NUM
} ENUM_RSSI_TRIGGER_TYPE;

struct _GLUE_INFO_T {
	NDIS_HANDLE rMiniportAdapterHandle;	/* Device handle */

	NDIS_WORK_ITEM rWorkItem;

	UINT_32 ucEmuWorkItemId;

	/* driver description read from registry */
	UINT_8 ucDriverDescLen;
	UINT_8 aucDriverDesc[80];	/* should be moved to os private */

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	/* Add for checksum offloading */
	NIC_TASK_OFFLOAD rNicTaskOffload;
	NIC_CHECKSUM_OFFLOAD rNicChecksumOffload;
	NDIS_ENCAPSULATION_FORMAT rEncapsulationFormat;
#endif

	UINT_16 u2NdisVersion;
	INT_32 exitRefCount;
	UINT_32 u4Flags;

	/* Packet pool and buffer pool */
	NDIS_HANDLE hPktPool;	/* Handle of Rx packet pool */
	NDIS_HANDLE hBufPool;	/* Handle of Rx buffer pool */
	PUINT_8 pucPayloadPool;	/* Starting address of the payload buffer pool */
	UINT_32 u4PayloadPoolSz;	/* Total size of the payload buffer pool, in bytes */
	PVOID pvPktDescrHead;	/* Pointer to the head of packet descriptor list */
	UINT_32 u4PktDescrFreeNum;	/* Currently available packet descriptors in the packet pool */
	UINT_32 u4PktPoolSz;	/* Total acquired packets descriptors in the packet pool */

	/* TxService related info */
	HANDLE hTxService;	/* TxService Thread Handle */
	NDIS_EVENT rTxReqEvent;	/* Event to wake up TxService */
#if defined(WINDOWS_DDK)
	PKTHREAD pvKThread;
#endif

	QUE_T rTxQueue;
	QUE_T rReturnQueue;
	QUE_T rCmdQueue;

	/* spinlock to protect Tx Queue Operation */
	NDIS_SPIN_LOCK arSpinLock[SPIN_LOCK_NUM];


	/* Number of pending frames, also used for debuging if any frame is
	 * missing during the process of unloading Driver.
	 */
	LONG i4TxPendingFrameNum;
	LONG i4TxPendingSecurityFrameNum;

	LONG i4RxPendingFrameNum;

	/* for port to Bss index mapping */
	UINT_8 aucNetInterfaceToBssIdx[NET_PORT_NUM];

	BOOLEAN fgIsCardRemoved;

	/* Host interface related information */
	/* defined in related hif header file */
	GL_HIF_INFO_T rHifInfo;

	NDIS_802_11_ASSOCIATION_INFORMATION rNdisAssocInfo;
	UINT_8 aucNdisAssocInfoIEs[600];

	/* Pointer to ADAPTER_T - main data structure of internal protocol stack */
	P_ADAPTER_T prAdapter;

	/* Indicated media state */
	ENUM_PARAM_MEDIA_STATE_T eParamMediaStateIndicated;

	/* registry info */
	REG_INFO_T rRegInfo;

	/* TX/RX: Interface to BSS Index mapping */
	NET_INTERFACE_INFO_T arNetInterfaceInfo[NET_DEV_NUM];
	P_NET_INTERFACE_INFO_T aprBssIdxToNetInterfaceInfo[HW_BSSID_NUM];

	/* OID related */
	BOOLEAN fgSetOid;
	PVOID pvInformationBuffer;
	UINT_32 u4InformationBufferLength;
	PUINT_32 pu4BytesReadOrWritten;
	PUINT_32 pu4BytesNeeded;
	PVOID pvOidEntry;
	BOOLEAN fgIsGlueExtension;

#if CFG_SUPPORT_WAPI
	/* Should be large than the PARAM_WAPI_ASSOC_INFO_T */
	UINT_8 aucWapiAssocInfoIEs[42];
	UINT_16 u2WapiAssocInfoIESz;
#endif

	LONG i4OidPendingCount;

	/* Timer related */
	PVOID pvTimerFunc;
	NDIS_MINIPORT_TIMER rMasterTimer;

	BOOLEAN fgWpsActive;
	UINT_8 aucWSCIE[500];	/*for probe req */
	UINT_16 u2WSCIELen;
	UINT_8 aucWSCAssocInfoIE[200];	/*for Assoc req */
	UINT_16 u2WSCAssocInfoIELen;

	SET_TXPWR_CTRL_T rTxPwr;

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
	BOOLEAN fgEnSdioTestPattern;
	BOOLEAN fgSdioReadWriteMode;
	BOOLEAN fgIsSdioTestInitialized;
	UINT_8 aucSdioTestBuffer[256];
#endif

};


#define MAX_MINIPORT_NAME_PATH                  256

/*  miniport instance information */
typedef struct _MINIPORT_INSTANCE_INFO {
	WCHAR MiniportName[MAX_MINIPORT_NAME_PATH];
	WCHAR MiniportInstance[MAX_MINIPORT_NAME_PATH];
	WCHAR RegPath[MAX_MINIPORT_NAME_PATH];
	WCHAR ActiveKeyPath[MAX_MINIPORT_NAME_PATH];
	ULONG InstanceNumber;
} MINIPORT_INSTANCE_INFO, *PMINIPORT_INSTANCE_INFO;


/* The packet structure placed in the NDIS_PACKET reserved field */
typedef struct _PKT_INFO_RESERVED_T {
	union {
		/* Reserved for Tx */
		struct {
			PVOID pvPacket;	/* used by MP_GET_PKT_MR(), MP_GET_MR_PKT() */
			UINT_8 ucTid;
			UINT_8 ucTC;
			UINT_16 u2PktLen;
			PUINT_8 pucDA;
		};
		/* Reserved for Rx */
		struct {
			PNDIS_PACKET prNextPkt;	/* next NDIS packet pool */
			PVOID pvBuf;	/* buffer address */
		};
	};
} PKT_INFO_RESERVED, *P_PKT_INFO_RESERVED;

#define PKT_INFO_RESERVED_TID_MASK      BITS(0, 3)
#define PKT_INFO_RESERVED_FLAG_P2P      BIT(4)
#define PKT_INFO_RESERVED_FLAG_PAL      BIT(5)
#define PKT_INFO_RESERVED_FLAG_1X       BIT(6)
#define PKT_INFO_RESERVED_FLAG_802_11   BIT(7)

typedef struct _MEDIA_STREAMING_INDICATIONS_T {
	NDIS_802_11_STATUS_TYPE StatusType;
	NDIS_802_11_MEDIA_STREAM_MODE MediaStreamMode;
} MEDIA_STREAMING_INDICATIONS_T, *P_MEDIA_STREAMING_INDICATIONS_T;

#define PKT_INFO_RESERVED_FLAG_802_3    BIT(0)
#define PKT_INFO_RESERVED_FLAG_VLAN     BIT(1)


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
#define GLUE_SPIN_LOCK_DECLARATION()

#define GLUE_ACQUIRE_SPIN_LOCK(_prGlueInfo, _rLockCategory)  \
	{ \
	    if (_rLockCategory < SPIN_LOCK_NUM) { \
		NdisAcquireSpinLock(&((_prGlueInfo)->arSpinLock[_rLockCategory])); \
	    } \
	}

#define GLUE_RELEASE_SPIN_LOCK(_prGlueInfo, _rLockCategory)  \
	{ \
	    if (_rLockCategory < SPIN_LOCK_NUM) { \
		NdisReleaseSpinLock(&((_prGlueInfo)->arSpinLock[_rLockCategory])); \
	    } \
	}

/*----------------------------------------------------------------------------*/
/* Macros of Waiting/Send EVENT                                               */
/*----------------------------------------------------------------------------*/
#define GLUE_WAIT_EVENT(_prGlueInfo)        NdisWaitEvent(&((_prGlueInfo)->rTxReqEvent), 0)

#define GLUE_RESET_EVENT(_prGlueInfo)       NdisResetEvent(&((_prGlueInfo)->rTxReqEvent))

#define GLUE_SET_EVENT(_prGlueInfo)         NdisSetEvent(&((_prGlueInfo)->rTxReqEvent))


/* Memory tag for this driver */
#define NIC_MEM_TAG                             ((ULONG) 'MCPI')

/* Media type */
#define NIC_MEDIA_TYPE                          NdisMedium802_3

/*Get variables offset of variable in GlueInfo for registry parsing*/
#define GLUE_GET_REG_OFFSET(_f)        (OFFSET_OF(GLUE_INFO_T, rRegInfo) + OFFSET_OF(REG_INFO_T, _f))

/* To utilize reserved buffer of NDIS_PACKET */
#define MP_GET_PKT_MR(_p)               (&(_p)->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, pvPacket)])
#define MP_GET_MR_PKT(_p)               (PNDIS_PACKET) CONTAINING_RECORD(_p, \
					    NDIS_PACKET, MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, pvPacket)])
#define MP_SET_PKT_TID(_p, _pri)        ((_p)->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, ucTid)] = (_pri))
#define MP_GET_PKT_TID(_p)              ((_p)->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, ucTid)])

#define MP_SET_PKT_TC(_p, _tc)          ((_p)->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, ucTC)] = (_tc))
#define MP_GET_PKT_TC(_p)               ((_p)->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, ucTC)])

#define MP_SET_PKT_PKTLEN(_p, _pktLen)  (*((PUINT_16)&((_p)->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, u2PktLen)])) = (_pktLen))
#define MP_GET_PKT_PKTLEN(_p)           (*((PUINT_16)&((_p)->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, u2PktLen)])))

#define MP_SET_PKT_DA_PTR(_p, _DA)      (*((PUINT_16)&((_p)->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, pucDA)])) = (_DA))
#define MP_GET_PKT_DA_PTR(_p)           (*((PUINT_16)&((_p)->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, pucDA)])))



/* Macros for flag operations for the Adapter structure */
#define GLUE_SET_FLAG(_M, _F)           ((_M)->u4Flags |= (_F))
#define GLUE_CLEAR_FLAG(_M, _F)         ((_M)->u4Flags &= ~(_F))
#define GLUE_TEST_FLAG(_M, _F)          ((_M)->u4Flags & (_F))
#define GLUE_TEST_FLAGS(_M, _F)         (((_M)->u4Flags & (_F)) == (_F))

#define GLUE_INC_REF_CNT(_refCount)     NdisInterlockedIncrement(&(_refCount))
#define GLUE_DEC_REF_CNT(_refCount)     NdisInterlockedDecrement(&(_refCount))
#define GLUE_GET_REF_CNT(_refCount)     (_refCount)

/*----------------------------------------------------------------------------*/
/* Macros of SPIN LOCK operations for using in Glue Layer                     */
/*----------------------------------------------------------------------------*/
#define GLUE_SPIN_LOCK_DECLARATION()

/* TODO: */
/*----------------------------------------------------------------------------*/
/* Macros for accessing Reserved Fields of native packet                      */
/*----------------------------------------------------------------------------*/
#define GLUE_CLEAR_PKT_RSVD(_p) \
	    { \
	     *((PUINT_32) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[0])) = 0; \
	     *((PUINT_32) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) = 0; \
	     *((PUINT_32) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[8])) = 0; \
	     *((PUINT_32) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[12])) = 0; \
	    }

#define GLUE_GET_PKT_QUEUE_ENTRY(_p)    \
	    (&(((PNDIS_PACKET)(_p))->MiniportReservedEx[0]))

#define GLUE_GET_PKT_DESCRIPTOR(_prQueueEntry)  \
	    (CONTAINING_RECORD(_prQueueEntry, NDIS_PACKET, MiniportReservedEx[0]))

#define GLUE_SET_PKT_TID(_p, _tid)  \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) |= \
	     ((UINT_8)((_tid) & PKT_INFO_RESERVED_TID_MASK)))

#define GLUE_SET_PKT_FLAG_802_11(_p)  \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) |= \
	     PKT_INFO_RESERVED_FLAG_802_11)

#define GLUE_SET_PKT_FLAG_1X(_p)  \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) |= \
	     PKT_INFO_RESERVED_FLAG_1X)

#define GLUE_SET_PKT_FLAG_PAL(_p)  \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) |= \
	     PKT_INFO_RESERVED_FLAG_PAL)

#define GLUE_SET_PKT_FLAG_P2P(_p)  \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) |= \
	     PKT_INFO_RESERVED_FLAG_P2P)


#define GLUE_GET_PKT_TID(_p)        \
	    ((*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) ) & \
	     PKT_INFO_RESERVED_TID_MASK)

#define GLUE_GET_PKT_IS_802_11(_p)      \
	    ((*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) ) & \
	     PKT_INFO_RESERVED_FLAG_802_11)

#define GLUE_GET_PKT_IS_1X(_p)          \
	    ((*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) ) & \
	     PKT_INFO_RESERVED_FLAG_1X)

#define GLUE_GET_PKT_IS_PAL(_p)         \
	    ((*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) ) & \
	     PKT_INFO_RESERVED_FLAG_PAL)

#define GLUE_GET_PKT_IS_P2P(_p)         \
	    ((*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[4])) ) & \
	     PKT_INFO_RESERVED_FLAG_P2P)


#define GLUE_SET_PKT_HEADER_LEN(_p, _ucMacHeaderLen)    \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[5])) = \
	     (_ucMacHeaderLen))

#define GLUE_GET_PKT_HEADER_LEN(_p) \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[5])) )


#define GLUE_SET_PKT_FRAME_LEN(_p, _u2FrameLen) \
            (*((PUINT_16) & (((PNDIS_PACKET)(_p))->MiniportReservedEx[6])) = \
	     (_u2FrameLen))

#define GLUE_GET_PKT_FRAME_LEN(_p)    \
            (*((PUINT_16) & (((PNDIS_PACKET)(_p))->MiniportReservedEx[6])) )


#define GLUE_SET_PKT_ARRIVAL_TIME(_p, _rSysTime) \
            (*((POS_SYSTIME) & (((PNDIS_PACKET)(_p))->MiniportReservedEx[8])) = \
	     (OS_SYSTIME)(_rSysTime))

#define GLUE_GET_PKT_ARRIVAL_TIME(_p)    \
            (*((POS_SYSTIME) & (((PNDIS_PACKET)(_p))->MiniportReservedEx[8])) )

#define GLUE_SET_PKT_BSS_IDX(_p, _ucBssIndex) \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[12])) = \
	     (UINT_8)(_ucBssIndex))

#define GLUE_GET_PKT_BSS_IDX(_p)    \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[12])) )

#define GLUE_SET_PKT_FLAG_802_3(_p) \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[13])) |= \
	     PKT_INFO_RESERVED_FLAG_802_3)

#define GLUE_GET_PKT_IS_802_3(_p) \
	    ((*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[13])) ) & \
	     PKT_INFO_RESERVED_FLAG_802_3)

#define GLUE_SET_PKT_FLAG_VLAN_EXIST(_p)  \
	    (*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[13])) |= \
	     PKT_INFO_RESERVED_FLAG_VLAN)

#define GLUE_GET_PKT_IS_VLAN_EXIST(_p)  \
	    ((*((PUINT_8) &(((PNDIS_PACKET)(_p))->MiniportReservedEx[13])) ) & \
	     PKT_INFO_RESERVED_FLAG_VLAN)


/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
NDIS_STATUS
windowsFindAdapter(IN P_GLUE_INFO_T prGlueInfo, IN NDIS_HANDLE rWrapperConfigurationContext);

NDIS_STATUS windowsRegisterIsrt(IN P_GLUE_INFO_T prGlueInfo);

PNDIS_PACKET getPoolPacket(IN P_GLUE_INFO_T prGlueInfo);

VOID putPoolPacket(IN P_GLUE_INFO_T prGlueInfo, IN PNDIS_PACKET prPktDscr, IN PVOID pvPayloadBuf);

BOOLEAN
windowsGetPacketInfo(IN P_GLUE_INFO_T prGlueInfo,
		     IN PNDIS_PACKET prPacket,
		     OUT PUINT_32 pu4PktLen,
		     OUT PUINT_8 pucUserPriority, OUT PUINT_8 pucTC, OUT PUINT_8 aucEthDestAddr);

NDIS_STATUS windowsUnregisterIsrt(IN P_GLUE_INFO_T prGlueInfo);

NDIS_STATUS windowsUMapFreeRegister(IN P_GLUE_INFO_T prGlueInfo);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
LINT_EXT_HEADER_END
/* Info 718: Symbol 'NdisReleaseSpinLock' undeclared, assumed to return int [MTK Rule 5.1.2]
 */
#ifdef _lint
VOID NdisAcquireSpinLock(IN PVOID SpinLock);

VOID NdisReleaseSpinLock(IN PVOID SpinLock);

VOID NdisSetEvent(IN PVOID Event);

BOOLEAN NdisWaitEvent(IN PVOID Event, IN UINT_32 MsToWait);

VOID NdisResetEvent(IN PVOID Event);
#endif				/* _lint */


#endif				/* _GL_OS_H */
