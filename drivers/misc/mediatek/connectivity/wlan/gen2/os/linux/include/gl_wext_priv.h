/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _GL_WEXT_PRIV_H
#define _GL_WEXT_PRIV_H
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/
/* If it is set to 1, iwpriv will support register read/write */
#define CFG_SUPPORT_PRIV_MCR_RW         1

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#if CFG_ENABLE_WIFI_DIRECT
extern int set_p2p_mode_handler(struct net_device *netdev, PARAM_CUSTOM_P2P_SET_STRUCT_T p2pmode);
#if 0
extern BOOLEAN fgIsResetting;
extern BOOLEAN g_u4HaltFlag;
extern spinlock_t g_p2p_lock;
extern int g_u4P2PEnding;
extern int g_u4P2POnOffing;
#endif
#endif


#if (CFG_SUPPORT_TXR_ENC == 1)
extern VOID rlmCmd(P_GLUE_INFO_T prGlueInfo, UINT_8 *prInBuf, UINT_32 u4InBufLen);
#endif		/* CFG_SUPPORT_TXR_ENC */

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* New wireless extensions API - SET/GET convention (even ioctl numbers are
 * root only)
 */
#define IOCTL_SET_INT                   (SIOCIWFIRSTPRIV + 0)
#define IOCTL_GET_INT                   (SIOCIWFIRSTPRIV + 1)

#define IOCTL_SET_ADDRESS               (SIOCIWFIRSTPRIV + 2)
#define IOCTL_GET_ADDRESS               (SIOCIWFIRSTPRIV + 3)
#define IOCTL_SET_STR                   (SIOCIWFIRSTPRIV + 4)
#define IOCTL_GET_STR                   (SIOCIWFIRSTPRIV + 5)
#define IOCTL_SET_KEY                   (SIOCIWFIRSTPRIV + 6)
#define IOCTL_GET_KEY                   (SIOCIWFIRSTPRIV + 7)
#define IOCTL_SET_STRUCT                (SIOCIWFIRSTPRIV + 8)
#define IOCTL_GET_STRUCT                (SIOCIWFIRSTPRIV + 9)
#define IOCTL_SET_STRUCT_FOR_EM         (SIOCIWFIRSTPRIV + 11)
#define IOCTL_SET_INTS                  (SIOCIWFIRSTPRIV + 12)
#define IOCTL_GET_INTS                  (SIOCIWFIRSTPRIV + 13)
#define IOCTL_SET_STRING				(SIOCIWFIRSTPRIV + 14)

#define PRIV_CMD_REG_DOMAIN             0
#define PRIV_CMD_BEACON_PERIOD          1
#define PRIV_CMD_ADHOC_MODE             2

#if CFG_TCP_IP_CHKSUM_OFFLOAD
#define PRIV_CMD_CSUM_OFFLOAD       3
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

#define PRIV_CMD_ROAMING                4
#define PRIV_CMD_VOIP_DELAY             5
#define PRIV_CMD_POWER_MODE             6

#define PRIV_CMD_WMM_PS                 7
#define PRIV_CMD_BT_COEXIST             8
#define PRIV_GPIO2_MODE                 9

#define PRIV_CUSTOM_SET_PTA			10
#define PRIV_CUSTOM_CONTINUOUS_POLL     11
#define PRIV_CUSTOM_SINGLE_ANTENNA		12
#define PRIV_CUSTOM_BWCS_CMD			13
#define PRIV_CUSTOM_DISABLE_BEACON_DETECTION	14	/* later */
#define PRIV_CMD_OID                    15
#define PRIV_SEC_MSG_OID                16

#define PRIV_CMD_TEST_MODE              17
#define PRIV_CMD_TEST_CMD               18
#define PRIV_CMD_ACCESS_MCR             19
#define PRIV_CMD_SW_CTRL                20

#if 1				/* ANTI_PRIVCY */
#define PRIV_SEC_CHECK_OID              21
#endif

#define PRIV_CMD_WSC_PROBE_REQ          22

#define PRIV_CMD_P2P_VERSION                   23

#define PRIV_CMD_GET_CH_LIST            24

#define PRIV_CMD_SET_TX_POWER           25

#define PRIV_CMD_BAND_CONFIG            26

#define PRIV_CMD_DUMP_MEM               27

#define PRIV_CMD_P2P_MODE               28

#define PRIV_CMD_GET_BUILD_DATE_CODE    29

#define PRIV_CMD_GET_DEBUG_CODE			30

#define PRIV_CMD_OTHER					31

#define PRIV_CMD_WFD_DEBUG_CODE			32

#define PRIV_CMD_MET_PROFILING			33

/* other string command ID */
#define PRIV_CMD_OTHER_TDLS				0x00
#define PRIV_CMD_OTHER_TAR				0x01 /* TX auto rate */

/* 802.3 Objects (Ethernet) */
#define OID_802_3_CURRENT_ADDRESS           0x01010102

/* IEEE 802.11 OIDs */
#define OID_802_11_SUPPORTED_RATES              0x0D01020E
#define OID_802_11_CONFIGURATION                0x0D010211

/* PnP and PM OIDs, NDIS default OIDS */
#define OID_PNP_SET_POWER                               0xFD010101

#define OID_CUSTOM_OID_INTERFACE_VERSION                0xFFA0C000

/* MT5921 specific OIDs */
#define OID_CUSTOM_BT_COEXIST_CTRL                      0xFFA0C580
#define OID_CUSTOM_POWER_MANAGEMENT_PROFILE             0xFFA0C581
#define OID_CUSTOM_PATTERN_CONFIG                       0xFFA0C582
#define OID_CUSTOM_BG_SSID_SEARCH_CONFIG                0xFFA0C583
#define OID_CUSTOM_VOIP_SETUP                           0xFFA0C584
#define OID_CUSTOM_ADD_TS                               0xFFA0C585
#define OID_CUSTOM_DEL_TS                               0xFFA0C586
#define OID_CUSTOM_SLT                               0xFFA0C587
#define OID_CUSTOM_ROAMING_EN                           0xFFA0C588
#define OID_CUSTOM_WMM_PS_TEST                          0xFFA0C589
#define OID_CUSTOM_COUNTRY_STRING                       0xFFA0C58A
#define OID_CUSTOM_MULTI_DOMAIN_CAPABILITY              0xFFA0C58B
#define OID_CUSTOM_GPIO2_MODE                           0xFFA0C58C
#define OID_CUSTOM_CONTINUOUS_POLL                      0xFFA0C58D
#define OID_CUSTOM_DISABLE_BEACON_DETECTION             0xFFA0C58E

/* CR1460, WPS privacy bit check disable */
#define OID_CUSTOM_DISABLE_PRIVACY_CHECK                0xFFA0C600

/* Precedent OIDs */
#define OID_CUSTOM_MCR_RW                               0xFFA0C801
#define OID_CUSTOM_EEPROM_RW                            0xFFA0C803
#define OID_CUSTOM_SW_CTRL                              0xFFA0C805
#define OID_CUSTOM_MEM_DUMP                             0xFFA0C807

/* RF Test specific OIDs */
#define OID_CUSTOM_TEST_MODE                            0xFFA0C901
#define OID_CUSTOM_TEST_RX_STATUS                       0xFFA0C903
#define OID_CUSTOM_TEST_TX_STATUS                       0xFFA0C905
#define OID_CUSTOM_ABORT_TEST_MODE                      0xFFA0C906
#define OID_CUSTOM_MTK_WIFI_TEST                        0xFFA0C911

/* BWCS */
#define OID_CUSTOM_BWCS_CMD                             0xFFA0C931
#define OID_CUSTOM_SINGLE_ANTENNA                       0xFFA0C932
#define OID_CUSTOM_SET_PTA                              0xFFA0C933

/* NVRAM */
#define OID_CUSTOM_MTK_NVRAM_RW                         0xFFA0C941
#define OID_CUSTOM_CFG_SRC_TYPE                         0xFFA0C942
#define OID_CUSTOM_EEPROM_TYPE                          0xFFA0C943

#if CFG_SUPPORT_WAPI
#define OID_802_11_WAPI_MODE                            0xFFA0CA00
#define OID_802_11_WAPI_ASSOC_INFO                      0xFFA0CA01
#define OID_802_11_SET_WAPI_KEY                         0xFFA0CA02
#endif

#if CFG_SUPPORT_WPS2
#define OID_802_11_WSC_ASSOC_INFO                       0xFFA0CB00
#endif

/* Define magic key of test mode (Don't change it for future compatibity) */
#define PRIV_CMD_TEST_MAGIC_KEY                         2011

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/* NIC BBCR configuration entry structure */
typedef struct _PRIV_CONFIG_ENTRY {
	UINT_8 ucOffset;
	UINT_8 ucValue;
} PRIV_CONFIG_ENTRY, *PPRIV_CONFIG_ENTRY;

typedef WLAN_STATUS(*PFN_OID_HANDLER_FUNC_REQ) (IN PVOID prAdapter,
						IN OUT PVOID pvBuf, IN UINT_32 u4BufLen, OUT PUINT_32 pu4OutInfoLen);

typedef enum _ENUM_OID_METHOD_T {
	ENUM_OID_GLUE_ONLY,
	ENUM_OID_GLUE_EXTENSION,
	ENUM_OID_DRIVER_CORE
} ENUM_OID_METHOD_T, *P_ENUM_OID_METHOD_T;

/* OID set/query processing entry */
typedef struct _WLAN_REQ_ENTRY {
	UINT_32 rOid;		/* OID */
	PUINT_8 pucOidName;	/* OID name text */
	BOOLEAN fgQryBufLenChecking;
	BOOLEAN fgSetBufLenChecking;
	ENUM_OID_METHOD_T eOidMethod;
	UINT_32 u4InfoBufLen;
	PFN_OID_HANDLER_FUNC_REQ pfOidQueryHandler;	/*  PFN_OID_HANDLER_FUNC */
	PFN_OID_HANDLER_FUNC_REQ pfOidSetHandler;	/* PFN_OID_HANDLER_FUNC */
} WLAN_REQ_ENTRY, *P_WLAN_REQ_ENTRY;

typedef struct _NDIS_TRANSPORT_STRUCT {
	UINT_32 ndisOidCmd;
	UINT_32 inNdisOidlength;
	UINT_32 outNdisOidLength;
	UINT_8 ndisOidContent[0];
} NDIS_TRANSPORT_STRUCT, *P_NDIS_TRANSPORT_STRUCT;

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

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

int
priv_set_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra);

int
priv_get_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra);

int
priv_set_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra);

int
priv_get_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra);

int
priv_set_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra);

int
priv_get_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra);

UINT_32 CmdStringDecParse(IN UINT_8 *InStr, OUT UINT_8 **OutStr, OUT UINT_32 *OutLen);

UINT_32 CmdStringMacParse(IN UINT_8 *InStr, OUT UINT_8 **OutStr, OUT UINT_32 *OutLen, OUT UINT_8 *OutMac);

int
priv_set_string(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra);

int priv_support_ioctl(IN struct net_device *prDev, IN OUT struct ifreq *prReq, IN int i4Cmd);

int priv_support_driver_cmd(IN struct net_device *prDev, IN OUT struct ifreq *prReq, IN int i4Cmd);

INT_32 priv_driver_cmds(IN struct net_device *prNetDev, IN PCHAR pcCommand, IN INT_32 i4TotalLen);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _GL_WEXT_PRIV_H */
