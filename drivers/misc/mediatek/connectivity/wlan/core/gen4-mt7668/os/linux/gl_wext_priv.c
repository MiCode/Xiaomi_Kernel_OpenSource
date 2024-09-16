/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/gl_wext_priv.c#8
*/

/*! \file gl_wext_priv.c
*    \brief This file includes private ioctl support.
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
#include "gl_os.h"
#include "gl_wext_priv.h"

#if CFG_SUPPORT_QA_TOOL
#include "gl_ate_agent.h"
#include "gl_qa_agent.h"
#endif

#if CFG_SUPPORT_WAPI
#include "gl_sec.h"
#endif
#if CFG_ENABLE_WIFI_DIRECT
#include "gl_p2p_os.h"
#endif

/*
* #if CFG_SUPPORT_QA_TOOL
* extern UINT_16 g_u2DumpIndex;
* #endif
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define NUM_SUPPORTED_OIDS      (sizeof(arWlanOidReqTable) / sizeof(WLAN_REQ_ENTRY))

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

static int
priv_get_ndis(IN struct net_device *prNetDev, IN NDIS_TRANSPORT_STRUCT * prNdisReq, OUT PUINT_32 pu4OutputLen);

static int
priv_set_ndis(IN struct net_device *prNetDev, IN NDIS_TRANSPORT_STRUCT * prNdisReq, OUT PUINT_32 pu4OutputLen);

#if 0				/* CFG_SUPPORT_WPS */
static int
priv_set_appie(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, OUT char *pcExtra);

static int
priv_set_filter(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, OUT char *pcExtra);
#endif /* CFG_SUPPORT_WPS */

static BOOLEAN reqSearchSupportedOidEntry(IN UINT_32 rOid, OUT P_WLAN_REQ_ENTRY * ppWlanReqEntry);

#if 0
static WLAN_STATUS
reqExtQueryConfiguration(IN P_GLUE_INFO_T prGlueInfo,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

static WLAN_STATUS
reqExtSetConfiguration(IN P_GLUE_INFO_T prGlueInfo,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif

static WLAN_STATUS
reqExtSetAcpiDevicePowerState(IN P_GLUE_INFO_T prGlueInfo,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

/*******************************************************************************
*                       P R I V A T E   D A T A
********************************************************************************
*/
static UINT_8 aucOidBuf[4096] = { 0 };

/* OID processing table */
/* Order is important here because the OIDs should be in order of
 *  increasing value for binary searching.
 */
static WLAN_REQ_ENTRY arWlanOidReqTable[] = {
#if 0
	   {(NDIS_OID)rOid,
	   (PUINT_8)pucOidName,
	   fgQryBufLenChecking, fgSetBufLenChecking, fgIsHandleInGlueLayerOnly, u4InfoBufLen,
	   pfOidQueryHandler,
	   pfOidSetHandler}
#endif
	/* General Operational Characteristics */

	/* Ethernet Operational Characteristics */
	{OID_802_3_CURRENT_ADDRESS,
	 DISP_STRING("OID_802_3_CURRENT_ADDRESS"),
	 TRUE, TRUE, ENUM_OID_DRIVER_CORE, 6,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryCurrentAddr,
	 NULL},

	/* OID_802_3_MULTICAST_LIST */
	/* OID_802_3_MAXIMUM_LIST_SIZE */
	/* Ethernet Statistics */

	/* NDIS 802.11 Wireless LAN OIDs */
	{OID_802_11_SUPPORTED_RATES,
	 DISP_STRING("OID_802_11_SUPPORTED_RATES"),
	 TRUE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_RATES_EX),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQuerySupportedRates,
	 NULL}
	,
	/*
	 *  {OID_802_11_CONFIGURATION,
	 *  DISP_STRING("OID_802_11_CONFIGURATION"),
	 *  TRUE, TRUE, ENUM_OID_GLUE_EXTENSION, sizeof(PARAM_802_11_CONFIG_T),
	 *  (PFN_OID_HANDLER_FUNC_REQ)reqExtQueryConfiguration,
	 *  (PFN_OID_HANDLER_FUNC_REQ)reqExtSetConfiguration},
	 */
	{OID_PNP_SET_POWER,
	 DISP_STRING("OID_PNP_SET_POWER"),
	 TRUE, FALSE, ENUM_OID_GLUE_EXTENSION, sizeof(PARAM_DEVICE_POWER_STATE),
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) reqExtSetAcpiDevicePowerState}
	,

	/* Custom OIDs */
	{OID_CUSTOM_OID_INTERFACE_VERSION,
	 DISP_STRING("OID_CUSTOM_OID_INTERFACE_VERSION"),
	 TRUE, FALSE, ENUM_OID_DRIVER_CORE, 4,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryOidInterfaceVersion,
	 NULL}
	,
#if 0
	   #if PTA_ENABLED
	   {OID_CUSTOM_BT_COEXIST_CTRL,
	   DISP_STRING("OID_CUSTOM_BT_COEXIST_CTRL"),
	   FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_BT_COEXIST_T),
	   NULL,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetBtCoexistCtrl},
	   #endif

	   {OID_CUSTOM_POWER_MANAGEMENT_PROFILE,
	   DISP_STRING("OID_CUSTOM_POWER_MANAGEMENT_PROFILE"),
	   FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryPwrMgmtProfParam,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetPwrMgmtProfParam},
	   {OID_CUSTOM_PATTERN_CONFIG,
	   DISP_STRING("OID_CUSTOM_PATTERN_CONFIG"),
	   TRUE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_PATTERN_SEARCH_CONFIG_STRUCT_T),
	   NULL,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetPatternConfig},
	   {OID_CUSTOM_BG_SSID_SEARCH_CONFIG,
	   DISP_STRING("OID_CUSTOM_BG_SSID_SEARCH_CONFIG"),
	   FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	   NULL,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetBgSsidParam},
	   {OID_CUSTOM_VOIP_SETUP,
	   DISP_STRING("OID_CUSTOM_VOIP_SETUP"),
	   TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryVoipConnectionStatus,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetVoipConnectionStatus},
	   {OID_CUSTOM_ADD_TS,
	   DISP_STRING("OID_CUSTOM_ADD_TS"),
	   TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	   NULL,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidAddTS},
	   {OID_CUSTOM_DEL_TS,
	   DISP_STRING("OID_CUSTOM_DEL_TS"),
	   TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	   NULL,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidDelTS},

	   #if CFG_LP_PATTERN_SEARCH_SLT
	   {OID_CUSTOM_SLT,
	   DISP_STRING("OID_CUSTOM_SLT"),
	   FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQuerySltResult,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetSltMode},
	   #endif

	   {OID_CUSTOM_ROAMING_EN,
	   DISP_STRING("OID_CUSTOM_ROAMING_EN"),
	   TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryRoamingFunction,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetRoamingFunction},
	   {OID_CUSTOM_WMM_PS_TEST,
	   DISP_STRING("OID_CUSTOM_WMM_PS_TEST"),
	   TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	   NULL,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetWiFiWmmPsTest},
	   {OID_CUSTOM_COUNTRY_STRING,
	   DISP_STRING("OID_CUSTOM_COUNTRY_STRING"),
	   FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryCurrentCountry,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetCurrentCountry},

	   #if CFG_SUPPORT_802_11D
	   {OID_CUSTOM_MULTI_DOMAIN_CAPABILITY,
	   DISP_STRING("OID_CUSTOM_MULTI_DOMAIN_CAPABILITY"),
	   FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryMultiDomainCap,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetMultiDomainCap},
	   #endif

	   {OID_CUSTOM_GPIO2_MODE,
	   DISP_STRING("OID_CUSTOM_GPIO2_MODE"),
	   FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(ENUM_PARAM_GPIO2_MODE_T),
	   NULL,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetGPIO2Mode},
	   {OID_CUSTOM_CONTINUOUS_POLL,
	   DISP_STRING("OID_CUSTOM_CONTINUOUS_POLL"),
	   FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CONTINUOUS_POLL_T),
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryContinuousPollInterval,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetContinuousPollProfile},
	   {OID_CUSTOM_DISABLE_BEACON_DETECTION,
	   DISP_STRING("OID_CUSTOM_DISABLE_BEACON_DETECTION"),
	   FALSE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryDisableBeaconDetectionFunc,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetDisableBeaconDetectionFunc},

	/* WPS */
	   {OID_CUSTOM_DISABLE_PRIVACY_CHECK,
	   DISP_STRING("OID_CUSTOM_DISABLE_PRIVACY_CHECK"),
	   FALSE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	   NULL,
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetDisablePriavcyCheck},
#endif

	{OID_CUSTOM_MCR_RW,
	 DISP_STRING("OID_CUSTOM_MCR_RW"),
	 TRUE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_MCR_RW_STRUCT_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryMcrRead,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetMcrWrite}
	,

	{OID_CUSTOM_EEPROM_RW,
	 DISP_STRING("OID_CUSTOM_EEPROM_RW"),
	 TRUE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_EEPROM_RW_STRUCT_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryEepromRead,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetEepromWrite}
	,

	{OID_CUSTOM_SW_CTRL,
	 DISP_STRING("OID_CUSTOM_SW_CTRL"),
	 TRUE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_SW_CTRL_STRUCT_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQuerySwCtrlRead,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetSwCtrlWrite}
	,

	{OID_CUSTOM_MEM_DUMP,
	 DISP_STRING("OID_CUSTOM_MEM_DUMP"),
	 TRUE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_MEM_DUMP_STRUCT_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryMemDump,
	 NULL}
	,

	{OID_CUSTOM_TEST_MODE,
	 DISP_STRING("OID_CUSTOM_TEST_MODE"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetTestMode}
	,

#if 0
	   {OID_CUSTOM_TEST_RX_STATUS,
	   DISP_STRING("OID_CUSTOM_TEST_RX_STATUS"),
	   FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_RFTEST_RX_STATUS_STRUCT_T),
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryRfTestRxStatus,
	   NULL},
	   {OID_CUSTOM_TEST_TX_STATUS,
	   DISP_STRING("OID_CUSTOM_TEST_TX_STATUS"),
	   FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_RFTEST_TX_STATUS_STRUCT_T),
	   (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryRfTestTxStatus,
	   NULL},
#endif
	{OID_CUSTOM_ABORT_TEST_MODE,
	 DISP_STRING("OID_CUSTOM_ABORT_TEST_MODE"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetAbortTestMode}
	,
	{OID_CUSTOM_MTK_WIFI_TEST,
	 DISP_STRING("OID_CUSTOM_MTK_WIFI_TEST"),
	 TRUE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_MTK_WIFI_TEST_STRUCT_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestQueryAutoTest,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetAutoTest}
	,
	{OID_CUSTOM_TEST_ICAP_MODE,
	 DISP_STRING("OID_CUSTOM_TEST_ICAP_MODE"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetTestIcapMode}
	,

	/* OID_CUSTOM_EMULATION_VERSION_CONTROL */

	/* BWCS */
#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	{OID_CUSTOM_BWCS_CMD,
	 DISP_STRING("OID_CUSTOM_BWCS_CMD"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(PTA_IPC_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryBT,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetBT}
	,
#endif
#if 0
	{OID_CUSTOM_SINGLE_ANTENNA,
	DISP_STRING("OID_CUSTOM_SINGLE_ANTENNA"),
	FALSE, FALSE, ENUM_OID_DRIVER_CORE, 4,
	(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryBtSingleAntenna,
	(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetBtSingleAntenna},
	{OID_CUSTOM_SET_PTA,
	DISP_STRING("OID_CUSTOM_SET_PTA"),
	FALSE, FALSE, ENUM_OID_DRIVER_CORE, 4,
	(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryPta,
	(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetPta},
#endif

	{OID_CUSTOM_MTK_NVRAM_RW,
	 DISP_STRING("OID_CUSTOM_MTK_NVRAM_RW"),
	 TRUE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_NVRAM_RW_STRUCT_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryNvramRead,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetNvramWrite}
	,

	{OID_CUSTOM_CFG_SRC_TYPE,
	 DISP_STRING("OID_CUSTOM_CFG_SRC_TYPE"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(ENUM_CFG_SRC_TYPE_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryCfgSrcType,
	 NULL}
	,

	{OID_CUSTOM_EEPROM_TYPE,
	 DISP_STRING("OID_CUSTOM_EEPROM_TYPE"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(ENUM_EEPROM_TYPE_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryEepromType,
	 NULL}
	,

#if CFG_SUPPORT_WAPI
	{OID_802_11_WAPI_MODE,
	 DISP_STRING("OID_802_11_WAPI_MODE"),
	 FALSE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWapiMode}
	,
	{OID_802_11_WAPI_ASSOC_INFO,
	 DISP_STRING("OID_802_11_WAPI_ASSOC_INFO"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWapiAssocInfo}
	,
	{OID_802_11_SET_WAPI_KEY,
	 DISP_STRING("OID_802_11_SET_WAPI_KEY"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_WPI_KEY_T),
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWapiKey}
	,
#endif

#if CFG_SUPPORT_WPS2
	{OID_802_11_WSC_ASSOC_INFO,
	 DISP_STRING("OID_802_11_WSC_ASSOC_INFO"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWSCAssocInfo}
	,
#endif
};

#ifdef CONFIG_COMPAT
#ifdef in_compat_syscall
	#define mtk_is_compat_task in_compat_syscall
#else
	#define mtk_is_compat_task is_compat_task
#endif
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief Dispatching function for private ioctl region (SIOCIWFIRSTPRIV ~
*   SIOCIWLASTPRIV).
*
* \param[in] prNetDev Net device requested.
* \param[in] prIfReq Pointer to ifreq structure.
* \param[in] i4Cmd Command ID between SIOCIWFIRSTPRIV and SIOCIWLASTPRIV.
*
* \retval 0 for success.
* \retval -EOPNOTSUPP If cmd is not supported.
* \retval -EFAULT For fail.
*
*/
/*----------------------------------------------------------------------------*/
int priv_support_ioctl(IN struct net_device *prNetDev, IN OUT struct ifreq *prIfReq, IN int i4Cmd)
{
	/* prIfReq is verified in the caller function wlanDoIOCTL() */
	struct iwreq *prIwReq = (struct iwreq *)prIfReq;
	struct iw_request_info rIwReqInfo;
	int err = 0;
	/* prNetDev is verified in the caller function wlanDoIOCTL() */

#ifdef CONFIG_COMPAT
	struct compat_iw_point *iwp_compat;
	union iwreq_data wrq_data;

	iwp_compat = (struct compat_iw_point *)&prIwReq->u.data;
	wrq_data.data.pointer = compat_ptr(iwp_compat->pointer);
	wrq_data.data.length = iwp_compat->length;
	wrq_data.data.flags = iwp_compat->flags;
#endif /* CONFIG_COMPAT */

	/* Prepare the call */
	rIwReqInfo.cmd = (__u16) i4Cmd;
	rIwReqInfo.flags = 0;

	switch (i4Cmd) {
	case IOCTL_SET_INT:
		/* NOTE(Kevin): 1/3 INT Type <= IFNAMSIZ, so we don't need copy_from/to_user() */
#ifdef CONFIG_COMPAT
		if (mtk_is_compat_task())
		/* User space is 32-bit, use compat ioctl */
		err = priv_set_int(prNetDev, &rIwReqInfo, &(wrq_data),
						(char *)&(wrq_data));
#else
		err = priv_set_int(prNetDev, &rIwReqInfo, &(prIwReq->u),
						(char *)&(prIwReq->u));
#endif
		break;

	case IOCTL_GET_INT:
		/* NOTE(Kevin): 1/3 INT Type <= IFNAMSIZ, so we don't need copy_from/to_user() */
#ifdef CONFIG_COMPAT
		if (mtk_is_compat_task())
		/* User space is 32-bit, use compat ioctl */
		err = priv_get_int(prNetDev, &rIwReqInfo, &wrq_data,
						(char *)&(wrq_data));
#else
		err = priv_get_int(prNetDev, &rIwReqInfo, &(prIwReq->u),
						(char *)&(prIwReq->u));
#endif
		break;

	case IOCTL_SET_STRUCT:
	case IOCTL_SET_STRUCT_FOR_EM:
#ifdef CONFIG_COMPAT
		if (mtk_is_compat_task())
		/* User space is 32-bit, use compat ioctl */
		err = priv_set_struct(prNetDev, &rIwReqInfo, &wrq_data,
						(char *)&(wrq_data));
#else
		err = priv_set_struct(prNetDev, &rIwReqInfo, &(prIwReq->u),
						(char *)&(prIwReq->u));
#endif
		break;

	case IOCTL_GET_STRUCT:
#ifdef CONFIG_COMPAT
		if (mtk_is_compat_task())
		/* User space is 32-bit, use compat ioctl */
		err = priv_get_struct(prNetDev, &rIwReqInfo, &wrq_data,
						(char *)&(wrq_data));
#else
		err = priv_get_struct(prNetDev, &rIwReqInfo, &(prIwReq->u),
						(char *)&(prIwReq->u));
#endif
		break;

#if (CFG_SUPPORT_QA_TOOL)
	case IOCTL_QA_TOOL_DAEMON:
#ifdef CONFIG_COMPAT
		if (mtk_is_compat_task())
		/* User space is 32-bit, use compat ioctl */
		err = priv_qa_agent(prNetDev, &rIwReqInfo, &wrq_data,
						(char *)&(wrq_data));
#else
		err = priv_qa_agent(prNetDev, &rIwReqInfo, &(prIwReq->u),
						(char *)&(prIwReq->u));
#endif
		break;
#endif

	case IOCTL_GET_STR:

	default:
#ifdef CONFIG_COMPAT
		iwp_compat->pointer = ptr_to_compat(wrq_data.data.pointer);
		iwp_compat->length = wrq_data.data.length;
		iwp_compat->flags = wrq_data.data.flags;
#endif
		return -EOPNOTSUPP;

	}			/* end of switch */
#ifdef CONFIG_COMPAT
	iwp_compat->pointer = ptr_to_compat(wrq_data.data.pointer);
	iwp_compat->length = wrq_data.data.length;
	iwp_compat->flags = wrq_data.data.flags;
#endif
	return err;
}				/* priv_support_ioctl */

#if CFG_SUPPORT_BATCH_SCAN

EVENT_BATCH_RESULT_T g_rEventBatchResult[CFG_BATCH_MAX_MSCAN];

UINT_32 batchChannelNum2Freq(UINT_32 u4ChannelNum)
{
	UINT_32 u4ChannelInMHz;

	if (u4ChannelNum >= 1 && u4ChannelNum <= 13)
		u4ChannelInMHz = 2412 + (u4ChannelNum - 1) * 5;
	else if (u4ChannelNum == 14)
		u4ChannelInMHz = 2484;
	else if (u4ChannelNum == 133)
		u4ChannelInMHz = 3665;	/* 802.11y */
	else if (u4ChannelNum == 137)
		u4ChannelInMHz = 3685;	/* 802.11y */
	else if (u4ChannelNum >= 34 && u4ChannelNum <= 165)
		u4ChannelInMHz = 5000 + u4ChannelNum * 5;
	else if (u4ChannelNum >= 183 && u4ChannelNum <= 196)
		u4ChannelInMHz = 4000 + u4ChannelNum * 5;
	else
		u4ChannelInMHz = 0;

	return u4ChannelInMHz;
}

#define TMP_TEXT_LEN_S 40
#define TMP_TEXT_LEN_L 60
static UCHAR text1[TMP_TEXT_LEN_S], text2[TMP_TEXT_LEN_L], text3[TMP_TEXT_LEN_L];	/* A safe len */

WLAN_STATUS
batchConvertResult(IN P_EVENT_BATCH_RESULT_T prEventBatchResult,
		   OUT PVOID pvBuffer, IN UINT_32 u4MaxBufferLen, OUT PUINT_32 pu4RetLen)
{
	CHAR *p = pvBuffer;
	CHAR ssid[ELEM_MAX_LEN_SSID + 1];
	INT_32 nsize, nsize1, nsize2, nsize3, scancount;
	INT_32 i, j, nleft;
	UINT_32 freq;

	P_EVENT_BATCH_RESULT_ENTRY_T prEntry;
	P_EVENT_BATCH_RESULT_T pBr;

	nsize = 0;
	nleft = u4MaxBufferLen;

	pBr = prEventBatchResult;
	scancount = 0;
	for (j = 0; j < CFG_BATCH_MAX_MSCAN; j++) {
		scancount += pBr->ucScanCount;
		pBr++;
	}

	nsize1 = scnprintf(text1, TMP_TEXT_LEN_S,
			   "scancount=%d\nnextcount=%d\n",
			   scancount, scancount);
	if (nsize1 < nleft) {
		nsize1 = scnprintf(p, nleft, "%s", text1);
		p += nsize1;
		nleft -= nsize1;
	} else
		goto short_buf;

	pBr = prEventBatchResult;
	for (j = 0; j < CFG_BATCH_MAX_MSCAN; j++) {
		DBGLOG(SCN, TRACE, "convert mscan = %d, apcount=%d, nleft=%d\n", j, pBr->ucScanCount, nleft);

		if (pBr->ucScanCount == 0) {
			pBr++;
			continue;
		}

		nleft -= 5;	/* -5 for "####\n" */

		/* We only support one round scan result now. */
		nsize1 = scnprintf(text1, TMP_TEXT_LEN_S, "apcount=%d\n", pBr->ucScanCount);
		if (nsize1 < nleft) {
			nsize1 = scnprintf(p, nleft, "%s", text1);
			p += nsize1;
			nleft -= nsize1;
		} else
			goto short_buf;

		for (i = 0; i < pBr->ucScanCount; i++) {
			prEntry = &pBr->arBatchResult[i];

			nsize1 = scnprintf(text1, TMP_TEXT_LEN_S, "bssid=" MACSTR "\n",
						MAC2STR(prEntry->aucBssid));
			kalMemCopy(ssid,
				   prEntry->aucSSID,
				   (prEntry->ucSSIDLen < ELEM_MAX_LEN_SSID ? prEntry->ucSSIDLen : ELEM_MAX_LEN_SSID));
			ssid[(prEntry->ucSSIDLen <
			      (ELEM_MAX_LEN_SSID - 1) ? prEntry->ucSSIDLen : (ELEM_MAX_LEN_SSID - 1))] = '\0';
			nsize2 = scnprintf(text2, TMP_TEXT_LEN_L, "ssid=%s\n", ssid);

			freq = batchChannelNum2Freq(prEntry->ucFreq);
			nsize3 =
			    scnprintf(text3, TMP_TEXT_LEN_L,
			    "freq=%u\nlevel=%d\ndist=%u\ndistSd=%u\n===\n",
			    freq, prEntry->cRssi, prEntry->u4Dist,
			    prEntry->u4Distsd);

			nsize = nsize1 + nsize2 + nsize3;
			if (nsize < nleft) {

				kalStrnCpy(p, text1, TMP_TEXT_LEN_S);
				p += nsize1;

				kalStrnCpy(p, text2, TMP_TEXT_LEN_L);
				p += nsize2;

				kalStrnCpy(p, text3, TMP_TEXT_LEN_L);
				p += nsize3;

				nleft -= nsize;
			} else {
				DBGLOG(SCN, TRACE, "Warning: Early break! (%d)\n", i);
				break;	/* discard following entries, TODO: apcount? */
			}
		}

		nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S, "%s", "####\n");
		if (nsize1 < nleft) {
			nsize1 = scnprintf(p, nleft, "%s", text1);
			p += nsize1;
			nleft -= nsize1;
		} else
			goto short_buf;

		pBr++;
	}

	nsize1 = scnprintf(text1, TMP_TEXT_LEN_S, "%s", "----\n");
	if (nsize1 < nleft)
		scnprintf(p, nleft, "%s", text1);
	else
		goto short_buf;

	*pu4RetLen = u4MaxBufferLen - nleft;
	DBGLOG(SCN, TRACE, "total len = %d (max len = %d)\n", *pu4RetLen, u4MaxBufferLen);

	return WLAN_STATUS_SUCCESS;

short_buf:
	DBGLOG(SCN, TRACE,
	       "Short buffer issue! %d > %d, %s\n",
		u4MaxBufferLen + (nsize - nleft),
		u4MaxBufferLen, (char *)pvBuffer);
	return WLAN_STATUS_INVALID_LENGTH;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief Private ioctl set int handler.
*
* \param[in] prNetDev Net device requested.
* \param[in] prIwReqInfo Pointer to iwreq structure.
* \param[in] prIwReqData The ioctl data structure, use the field of sub-command.
* \param[in] pcExtra The buffer with input value
*
* \retval 0 For success.
* \retval -EOPNOTSUPP If cmd is not supported.
* \retval -EINVAL If a value is out of range.
*
*/
/*----------------------------------------------------------------------------*/
int
priv_set_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	UINT_32 u4SubCmd;
	PUINT_32 pu4IntBuf;
	P_NDIS_TRANSPORT_STRUCT prNdisReq;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4BufLen = 0;
	int status = 0;
	P_PTA_IPC_T prPtaIpc;

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);

	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (prGlueInfo == NULL || prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, INFO, "adapter is not ready\n");
		return -EIO;
	}

	u4SubCmd = (UINT_32) prIwReqData->mode;
	pu4IntBuf = (PUINT_32) pcExtra;

	switch (u4SubCmd) {
	case PRIV_CMD_TEST_MODE:
		/* printk("TestMode=%ld\n", pu4IntBuf[1]); */
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		if (pu4IntBuf[1] == PRIV_CMD_TEST_MAGIC_KEY) {
			prNdisReq->ndisOidCmd = OID_CUSTOM_TEST_MODE;
		} else if (pu4IntBuf[1] == 0) {
			prNdisReq->ndisOidCmd = OID_CUSTOM_ABORT_TEST_MODE;
		} else if (pu4IntBuf[1] == PRIV_CMD_TEST_MAGIC_KEY_ICAP) {
			prNdisReq->ndisOidCmd = OID_CUSTOM_TEST_ICAP_MODE;
		} else {
			status = 0;
			break;
		}
		prNdisReq->inNdisOidlength = 0;
		prNdisReq->outNdisOidLength = 0;

		/* Execute this OID */
		status = priv_set_ndis(prNetDev, prNdisReq, &u4BufLen);
		break;

	case PRIV_CMD_TEST_CMD:
		/* printk("CMD=0x%08lx, data=0x%08lx\n", pu4IntBuf[1], pu4IntBuf[2]); */
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_MTK_WIFI_TEST;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		/* Execute this OID */
		status = priv_set_ndis(prNetDev, prNdisReq, &u4BufLen);
		break;

#if CFG_SUPPORT_PRIV_MCR_RW
	case PRIV_CMD_ACCESS_MCR:
		/* printk("addr=0x%08lx, data=0x%08lx\n", pu4IntBuf[1], pu4IntBuf[2]); */
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		if (!prGlueInfo->fgMcrAccessAllowed) {
			if (pu4IntBuf[1] == PRIV_CMD_TEST_MAGIC_KEY && pu4IntBuf[2] == PRIV_CMD_TEST_MAGIC_KEY)
				prGlueInfo->fgMcrAccessAllowed = TRUE;
			status = 0;
			break;
		}

		if (pu4IntBuf[1] == PRIV_CMD_TEST_MAGIC_KEY && pu4IntBuf[2] == PRIV_CMD_TEST_MAGIC_KEY) {
			status = 0;
			break;
		}

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_MCR_RW;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		/* Execute this OID */
		status = priv_set_ndis(prNetDev, prNdisReq, &u4BufLen);
		break;
#endif

	case PRIV_CMD_SW_CTRL:
		/* printk("addr=0x%08lx, data=0x%08lx\n", pu4IntBuf[1], pu4IntBuf[2]); */
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_SW_CTRL;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		/* Execute this OID */
		status = priv_set_ndis(prNetDev, prNdisReq, &u4BufLen);
		break;

#if 0
	case PRIV_CMD_BEACON_PERIOD:
		/* pu4IntBuf[0] is used as input SubCmd */
		rStatus = wlanSetInformation(prGlueInfo->prAdapter, wlanoidSetBeaconInterval, (PVOID)&pu4IntBuf[1],
					     sizeof(UINT_32), &u4BufLen);
		break;
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	case PRIV_CMD_CSUM_OFFLOAD:
		{
			UINT_32 u4CSUMFlags;

			if (pu4IntBuf[1] == 1)
				u4CSUMFlags = CSUM_OFFLOAD_EN_ALL;
			else if (pu4IntBuf[1] == 0)
				u4CSUMFlags = 0;
			else
				return -EINVAL;

			if (kalIoctl(prGlueInfo,
				     wlanoidSetCSUMOffload,
				     (PVOID)&u4CSUMFlags,
				     sizeof(UINT_32), FALSE, FALSE, TRUE, &u4BufLen) == WLAN_STATUS_SUCCESS) {
				if (pu4IntBuf[1] == 1)
					prNetDev->features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | NETIF_F_RXCSUM;
				else if (pu4IntBuf[1] == 0)
					prNetDev->features &= ~(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | NETIF_F_RXCSUM);
			}
		}
		break;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	case PRIV_CMD_POWER_MODE:
		{
			PARAM_POWER_MODE_T rPowerMode;

			P_BSS_INFO_T prBssInfo = prGlueInfo->prAdapter->prAisBssInfo;

			if (!prBssInfo)
				break;

			rPowerMode.ePowerMode = (PARAM_POWER_MODE) pu4IntBuf[1];
			rPowerMode.ucBssIdx = prBssInfo->ucBssIndex;

			/* pu4IntBuf[0] is used as input SubCmd */
			kalIoctl(prGlueInfo, wlanoidSet802dot11PowerSaveProfile, &rPowerMode,
				 sizeof(PARAM_POWER_MODE_T), FALSE, FALSE, TRUE, &u4BufLen);
		}
		break;

	case PRIV_CMD_WMM_PS:
		{
			PARAM_CUSTOM_WMM_PS_TEST_STRUCT_T rWmmPsTest;

			rWmmPsTest.bmfgApsdEnAc = (UINT_8) pu4IntBuf[1];
			rWmmPsTest.ucIsEnterPsAtOnce = (UINT_8) pu4IntBuf[2];
			rWmmPsTest.ucIsDisableUcTrigger = (UINT_8) pu4IntBuf[3];
			rWmmPsTest.reserved = 0;

			kalIoctl(prGlueInfo,
				 wlanoidSetWiFiWmmPsTest,
				 (PVOID)&rWmmPsTest,
				 sizeof(PARAM_CUSTOM_WMM_PS_TEST_STRUCT_T), FALSE, FALSE, TRUE, &u4BufLen);
		}
		break;

#if 0
	case PRIV_CMD_ADHOC_MODE:
		/* pu4IntBuf[0] is used as input SubCmd */
		rStatus = wlanSetInformation(prGlueInfo->prAdapter, wlanoidSetAdHocMode, (PVOID)&pu4IntBuf[1],
					     sizeof(UINT_32), &u4BufLen);
		break;
#endif

	case PRIV_CUSTOM_BWCS_CMD:

		DBGLOG(REQ, INFO,
		       "pu4IntBuf[1] = %x, size of PTA_IPC_T = %zu.\n",
			pu4IntBuf[1], sizeof(PARAM_PTA_IPC_T));

		prPtaIpc = (P_PTA_IPC_T) aucOidBuf;
		prPtaIpc->u.aucBTPParams[0] = (UINT_8) (pu4IntBuf[1] >> 24);
		prPtaIpc->u.aucBTPParams[1] = (UINT_8) (pu4IntBuf[1] >> 16);
		prPtaIpc->u.aucBTPParams[2] = (UINT_8) (pu4IntBuf[1] >> 8);
		prPtaIpc->u.aucBTPParams[3] = (UINT_8) (pu4IntBuf[1]);

		DBGLOG(REQ, INFO,
		       "BCM BWCS CMD : PRIV_CUSTOM_BWCS_CMD : aucBTPParams[0] = %02x, aucBTPParams[1] = %02x.\n",
		       prPtaIpc->u.aucBTPParams[0], prPtaIpc->u.aucBTPParams[1]);
		DBGLOG(REQ, INFO,
		       "BCM BWCS CMD : PRIV_CUSTOM_BWCS_CMD : aucBTPParams[2] = %02x, aucBTPParams[3] = %02x.\n",
		       prPtaIpc->u.aucBTPParams[2], prPtaIpc->u.aucBTPParams[3]);

#if 0
		status = wlanSetInformation(prGlueInfo->prAdapter,
					    wlanoidSetBT, (PVOID)&aucOidBuf[0], u4CmdLen, &u4BufLen);
#endif

		status = wlanoidSetBT(prGlueInfo->prAdapter,
				      (PVOID)&aucOidBuf[0], sizeof(PARAM_PTA_IPC_T), &u4BufLen);

		if (status != WLAN_STATUS_SUCCESS)
			status = -EFAULT;

		break;

	case PRIV_CMD_BAND_CONFIG:
		{
			DBGLOG(INIT, INFO, "CMD set_band = %u\n",
			       (UINT_32) pu4IntBuf[1]);
		}
		break;

#if CFG_ENABLE_WIFI_DIRECT
	case PRIV_CMD_P2P_MODE:
		{
			PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P;
			WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;

			rSetP2P.u4Enable = pu4IntBuf[1];
			rSetP2P.u4Mode = pu4IntBuf[2];
#if 1
			if (!rSetP2P.u4Enable)
				p2pNetUnregister(prGlueInfo, TRUE);

			/* pu4IntBuf[0] is used as input SubCmd */
			rWlanStatus = kalIoctl(prGlueInfo, wlanoidSetP2pMode, (PVOID)&rSetP2P,
					       sizeof(PARAM_CUSTOM_P2P_SET_STRUCT_T), FALSE, FALSE, TRUE, &u4BufLen);

			if ((rSetP2P.u4Enable) && (rWlanStatus == WLAN_STATUS_SUCCESS))
				p2pNetRegister(prGlueInfo, TRUE);
#endif

		}
		break;
#endif

#if (CFG_MET_PACKET_TRACE_SUPPORT == 1)
	case PRIV_CMD_MET_PROFILING:
		{
			/* PARAM_CUSTOM_WFD_DEBUG_STRUCT_T rWfdDebugModeInfo; */
			/* rWfdDebugModeInfo.ucWFDDebugMode=(UINT_8)pu4IntBuf[1]; */
			/* rWfdDebugModeInfo.u2SNPeriod=(UINT_16)pu4IntBuf[2]; */
			/* DBGLOG(REQ, INFO, ("WFD Debug Mode:%d Period:%d\n",
			 *  rWfdDebugModeInfo.ucWFDDebugMode, rWfdDebugModeInfo.u2SNPeriod));
			 */
			prGlueInfo->fgMetProfilingEn = (UINT_8) pu4IntBuf[1];
			prGlueInfo->u2MetUdpPort = (UINT_16) pu4IntBuf[2];
			/* DBGLOG(INIT, INFO, ("MET_PROF: Enable=%d UDP_PORT=%d\n",
			 *  prGlueInfo->fgMetProfilingEn, prGlueInfo->u2MetUdpPort);
			 */

		}
		break;

#endif

	default:
		return -EOPNOTSUPP;
	}

	return status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Private ioctl get int handler.
*
* \param[in] pDev Net device requested.
* \param[out] pIwReq Pointer to iwreq structure.
* \param[in] prIwReqData The ioctl req structure, use the field of sub-command.
* \param[out] pcExtra The buffer with put the return value
*
* \retval 0 For success.
* \retval -EOPNOTSUPP If cmd is not supported.
* \retval -EFAULT For fail.
*
*/
/*----------------------------------------------------------------------------*/
int
priv_get_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	UINT_32 u4SubCmd;
	PUINT_32 pu4IntBuf;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4BufLen = 0;
	int status = 0;
	P_NDIS_TRANSPORT_STRUCT prNdisReq;
	INT_32 ch[MAX_CHN_NUM];

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (prGlueInfo == NULL || prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, INFO, "adapter is not ready\n");
		return -EIO;
	}

	u4SubCmd = (UINT_32) prIwReqData->mode;
	pu4IntBuf = (PUINT_32) pcExtra;

	switch (u4SubCmd) {
	case PRIV_CMD_TEST_CMD:
		/* printk("CMD=0x%08lx, data=0x%08lx\n", pu4IntBuf[1], pu4IntBuf[2]); */
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_MTK_WIFI_TEST;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0) {
			/* printk("Result=%ld\n", *(PUINT_32)&prNdisReq->ndisOidContent[4]); */
			prIwReqData->mode = *(PUINT_32) &prNdisReq->ndisOidContent[4];
			/*
			 *  if (copy_to_user(prIwReqData->data.pointer,
			 *  &prNdisReq->ndisOidContent[4], 4)) {
			 *  printk(KERN_NOTICE "priv_get_int() copy_to_user oidBuf fail(3)\n");
			 *  return -EFAULT;
			 *  }
			 */
		}
		return status;

#if CFG_SUPPORT_PRIV_MCR_RW
	case PRIV_CMD_ACCESS_MCR:
		/* printk("addr=0x%08lx\n", pu4IntBuf[1]); */
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		if (!prGlueInfo->fgMcrAccessAllowed) {
			status = 0;
			return status;
		}

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_MCR_RW;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0) {
			/* printk("Result=%ld\n", *(PUINT_32)&prNdisReq->ndisOidContent[4]); */
			prIwReqData->mode = *(PUINT_32) &prNdisReq->ndisOidContent[4];
		}
		return status;
#endif

	case PRIV_CMD_DUMP_MEM:
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

#if 1
		if (!prGlueInfo->fgMcrAccessAllowed) {
			status = 0;
			return status;
		}
#endif
		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_MEM_DUMP;
		prNdisReq->inNdisOidlength = sizeof(PARAM_CUSTOM_MEM_DUMP_STRUCT_T);
		prNdisReq->outNdisOidLength = sizeof(PARAM_CUSTOM_MEM_DUMP_STRUCT_T);

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0)
			prIwReqData->mode = *(PUINT_32) &prNdisReq->ndisOidContent[0];
		return status;

	case PRIV_CMD_SW_CTRL:
		/* printk(" addr=0x%08lx\n", pu4IntBuf[1]); */

		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_SW_CTRL;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0) {
			/* printk("Result=%ld\n", *(PUINT_32)&prNdisReq->ndisOidContent[4]); */
			prIwReqData->mode = *(PUINT_32) &prNdisReq->ndisOidContent[4];
		}
		return status;

#if 0
	case PRIV_CMD_BEACON_PERIOD:
		status = wlanQueryInformation(prGlueInfo->prAdapter,
					      wlanoidQueryBeaconInterval,
					      (PVOID) pu4IntBuf, sizeof(UINT_32), &u4BufLen);
		return status;

	case PRIV_CMD_POWER_MODE:
		status = wlanQueryInformation(prGlueInfo->prAdapter,
					      wlanoidQuery802dot11PowerSaveProfile,
					      (PVOID) pu4IntBuf, sizeof(UINT_32), &u4BufLen);
		return status;

	case PRIV_CMD_ADHOC_MODE:
		status = wlanQueryInformation(prGlueInfo->prAdapter,
					      wlanoidQueryAdHocMode, (PVOID) pu4IntBuf, sizeof(UINT_32), &u4BufLen);
		return status;
#endif

	case PRIV_CMD_BAND_CONFIG:
		DBGLOG(INIT, INFO, "CMD get_band=\n");
		prIwReqData->mode = 0;
		return status;

	default:
		break;
	}

	u4SubCmd = (UINT_32) prIwReqData->data.flags;

	switch (u4SubCmd) {
	case PRIV_CMD_GET_CH_LIST:
		{
			UINT_16 i, j = 0;
			UINT_8 NumOfChannel = MAX_CHN_NUM;
			UINT_8 ucMaxChannelNum = MAX_CHN_NUM;
			RF_CHANNEL_INFO_T aucChannelList[MAX_CHN_NUM];

			DBGLOG(RLM, INFO, "Domain: Query Channel List.\n");
			kalGetChannelList(prGlueInfo, BAND_NULL, ucMaxChannelNum, &NumOfChannel, aucChannelList);
			if (NumOfChannel > MAX_CHN_NUM)
				NumOfChannel = MAX_CHN_NUM;

			if (kalIsAPmode(prGlueInfo)) {
				for (i = 0; i < NumOfChannel; i++) {
					if ((aucChannelList[i].ucChannelNum <= 13) ||
					    (aucChannelList[i].ucChannelNum == 36
					     || aucChannelList[i].ucChannelNum == 40
					     || aucChannelList[i].ucChannelNum == 44
					     || aucChannelList[i].ucChannelNum == 48)) {
						ch[j] = (INT_32) aucChannelList[i].ucChannelNum;
						j++;
					}
				}
			} else {
				for (j = 0; j < NumOfChannel; j++)
					ch[j] = (INT_32) aucChannelList[j].ucChannelNum;
			}

			prIwReqData->data.length = j;
			if (copy_to_user(prIwReqData->data.pointer, ch, NumOfChannel * sizeof(INT_32)))
				return -EFAULT;
			else
				return status;
		}
	default:
		return -EOPNOTSUPP;
	}

	return status;
}				/* priv_get_int */

/*----------------------------------------------------------------------------*/
/*!
* \brief Private ioctl set int array handler.
*
* \param[in] prNetDev Net device requested.
* \param[in] prIwReqInfo Pointer to iwreq structure.
* \param[in] prIwReqData The ioctl data structure, use the field of sub-command.
* \param[in] pcExtra The buffer with input value
*
* \retval 0 For success.
* \retval -EOPNOTSUPP If cmd is not supported.
* \retval -EINVAL If a value is out of range.
*
*/
/*----------------------------------------------------------------------------*/
int
priv_set_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	UINT_32 u4SubCmd, u4BufLen, u4CmdLen;
	P_GLUE_INFO_T prGlueInfo;
	int status = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_SET_TXPWR_CTRL_T prTxpwr;
	UINT_16 i = 0;
	INT_32 setting[4] = {0};

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);

	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	u4SubCmd = (UINT_32) prIwReqData->data.flags;
	u4CmdLen = prIwReqData->data.length;

	switch (u4SubCmd) {
	case PRIV_CMD_SET_TX_POWER:
		{
			if (u4CmdLen > 4)
				return -EINVAL;
			if (copy_from_user(setting,
					prIwReqData->data.pointer, u4CmdLen))
				return -EFAULT;

			prTxpwr = &prGlueInfo->rTxPwr;
			if (setting[0] == 0 && prIwReqData->data.length == 4 /* argc num */) {
				/* 0 (All networks), 1 (legacy STA), 2 (Hotspot AP), 3 (P2P), 4 (BT over Wi-Fi) */
				if (setting[1] == 1 || setting[1] == 0) {
					if (setting[2] == 0 || setting[2] == 1)
						prTxpwr->c2GLegacyStaPwrOffset = setting[3];
					if (setting[2] == 0 || setting[2] == 2)
						prTxpwr->c5GLegacyStaPwrOffset = setting[3];
				}
				if (setting[1] == 2 || setting[1] == 0) {
					if (setting[2] == 0 || setting[2] == 1)
						prTxpwr->c2GHotspotPwrOffset = setting[3];
					if (setting[2] == 0 || setting[2] == 2)
						prTxpwr->c5GHotspotPwrOffset = setting[3];
				}
				if (setting[1] == 3 || setting[1] == 0) {
					if (setting[2] == 0 || setting[2] == 1)
						prTxpwr->c2GP2pPwrOffset = setting[3];
					if (setting[2] == 0 || setting[2] == 2)
						prTxpwr->c5GP2pPwrOffset = setting[3];
				}
				if (setting[1] == 4 || setting[1] == 0) {
					if (setting[2] == 0 || setting[2] == 1)
						prTxpwr->c2GBowPwrOffset = setting[3];
					if (setting[2] == 0 || setting[2] == 2)
						prTxpwr->c5GBowPwrOffset = setting[3];
				}
			} else if (setting[0] == 1 && prIwReqData->data.length == 2) {
				prTxpwr->ucConcurrencePolicy = setting[1];
			} else if (setting[0] == 2 && prIwReqData->data.length == 3) {
				if (setting[1] == 0) {
					for (i = 0; i < 14; i++)
						prTxpwr->acTxPwrLimit2G[i] = setting[2];
				} else if (setting[1] <= 14)
					prTxpwr->acTxPwrLimit2G[setting[1] - 1] = setting[2];
			} else if (setting[0] == 3 && prIwReqData->data.length == 3) {
				if (setting[1] == 0) {
					for (i = 0; i < 4; i++)
						prTxpwr->acTxPwrLimit5G[i] = setting[2];
				} else if (setting[1] <= 4)
					prTxpwr->acTxPwrLimit5G[setting[1] - 1] = setting[2];
			} else if (setting[0] == 4 && prIwReqData->data.length == 2) {
				if (setting[1] == 0)
					wlanDefTxPowerCfg(prGlueInfo->prAdapter);
				rStatus = kalIoctl(prGlueInfo,
						   wlanoidSetTxPower,
						   prTxpwr, sizeof(SET_TXPWR_CTRL_T), FALSE, FALSE, TRUE, &u4BufLen);
			} else
				return -EFAULT;
		}
		return status;
	default:
		break;
	}

	return status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Private ioctl get int array handler.
*
* \param[in] pDev Net device requested.
* \param[out] pIwReq Pointer to iwreq structure.
* \param[in] prIwReqData The ioctl req structure, use the field of sub-command.
* \param[out] pcExtra The buffer with put the return value
*
* \retval 0 For success.
* \retval -EOPNOTSUPP If cmd is not supported.
* \retval -EFAULT For fail.
*
*/
/*----------------------------------------------------------------------------*/
int
priv_get_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	UINT_32 u4SubCmd;
	P_GLUE_INFO_T prGlueInfo;
	int status = 0;
	INT_32 ch[MAX_CHN_NUM];

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (prGlueInfo == NULL || prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, INFO, "adapter is not ready\n");
		return -EIO;
	}

	u4SubCmd = (UINT_32) prIwReqData->data.flags;

	switch (u4SubCmd) {
	case PRIV_CMD_GET_CH_LIST:
		{
			UINT_16 i;
			UINT_8 NumOfChannel = MAX_CHN_NUM;
			UINT_8 ucMaxChannelNum = MAX_CHN_NUM;
			RF_CHANNEL_INFO_T aucChannelList[MAX_CHN_NUM];

			kalGetChannelList(prGlueInfo, BAND_NULL, ucMaxChannelNum, &NumOfChannel, aucChannelList);
			if (NumOfChannel > MAX_CHN_NUM)
				NumOfChannel = MAX_CHN_NUM;

			for (i = 0; i < NumOfChannel; i++)
				ch[i] = (INT_32) aucChannelList[i].ucChannelNum;

			prIwReqData->data.length = NumOfChannel;
			if (copy_to_user(prIwReqData->data.pointer, ch, NumOfChannel * sizeof(INT_32)))
				return -EFAULT;
			else
				return status;
		}
	default:
		break;
	}

	return status;
}				/* priv_get_int */

/*----------------------------------------------------------------------------*/
/*!
* \brief Private ioctl set structure handler.
*
* \param[in] pDev Net device requested.
* \param[in] prIwReqData Pointer to iwreq_data structure.
*
* \retval 0 For success.
* \retval -EOPNOTSUPP If cmd is not supported.
* \retval -EINVAL If a value is out of range.
*
*/
/*----------------------------------------------------------------------------*/
int
priv_set_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	UINT_32 u4SubCmd = 0;
	int status = 0;
	/* WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS; */
	UINT_32 u4CmdLen = 0;
	P_NDIS_TRANSPORT_STRUCT prNdisReq;
	PUINT_32 pu4IntBuf = NULL;

	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	/* ASSERT(prIwReqInfo); */
	ASSERT(prIwReqData);
	/* ASSERT(pcExtra); */

	kalMemZero(&aucOidBuf[0], sizeof(aucOidBuf));

	if (GLUE_CHK_PR2(prNetDev, prIwReqData) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (prGlueInfo == NULL || prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, INFO, "adapter is not ready\n");
		return -EIO;
	}

	u4SubCmd = (UINT_32) prIwReqData->data.flags;

#if 0
	DBGLOG(INIT, INFO, "priv_set_struct(): prIwReqInfo->cmd(0x%X), u4SubCmd(%ld)\n", prIwReqInfo->cmd, u4SubCmd);
#endif

	switch (u4SubCmd) {
#if 0				/* PTA_ENABLED */
	case PRIV_CMD_BT_COEXIST:
		u4CmdLen = prIwReqData->data.length * sizeof(UINT_32);
		ASSERT(sizeof(PARAM_CUSTOM_BT_COEXIST_T) >= u4CmdLen);
		if (sizeof(PARAM_CUSTOM_BT_COEXIST_T) < u4CmdLen)
			return -EFAULT;

		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer, u4CmdLen)) {
			status = -EFAULT;	/* return -EFAULT; */
			break;
		}

		rStatus = wlanSetInformation(prGlueInfo->prAdapter,
					     wlanoidSetBtCoexistCtrl, (PVOID)&aucOidBuf[0], u4CmdLen, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			status = -EFAULT;
		break;
#endif

	case PRIV_CUSTOM_BWCS_CMD:
		u4CmdLen = prIwReqData->data.length * sizeof(UINT_32);
		ASSERT(sizeof(PARAM_PTA_IPC_T) >= u4CmdLen);
		if (sizeof(PARAM_PTA_IPC_T) < u4CmdLen)
			return -EFAULT;
#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS && CFG_SUPPORT_BCM_BWCS_DEBUG
		DBGLOG(REQ, INFO,
		       "ucCmdLen = %d, size of PTA_IPC_T = %d, prIwReqData->data = 0x%x.\n",
		       u4CmdLen, sizeof(PARAM_PTA_IPC_T), prIwReqData->data);

		DBGLOG(REQ, INFO, "priv_set_struct(): prIwReqInfo->cmd(0x%X), u4SubCmd(%ld)\n", prIwReqInfo->cmd,
		       u4SubCmd);
		DBGLOG(REQ, INFO, "*pcExtra = 0x%x\n", *pcExtra);
#endif

		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer, u4CmdLen)) {
			status = -EFAULT;	/* return -EFAULT; */
			break;
		}
#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS && CFG_SUPPORT_BCM_BWCS_DEBUG
		DBGLOG(REQ, INFO, "priv_set_struct(): BWCS CMD = %02x%02x%02x%02x\n", aucOidBuf[2], aucOidBuf[3],
		       aucOidBuf[4], aucOidBuf[5]);
#endif

#if 0
		status = wlanSetInformation(prGlueInfo->prAdapter,
					    wlanoidSetBT, (PVOID)&aucOidBuf[0], u4CmdLen, &u4BufLen);
#endif

#if 1
		status = wlanoidSetBT(prGlueInfo->prAdapter, (PVOID)&aucOidBuf[0], u4CmdLen, &u4BufLen);
#endif

		if (status != WLAN_STATUS_SUCCESS)
			status = -EFAULT;

		break;

#if CFG_SUPPORT_WPS2
	case PRIV_CMD_WSC_PROBE_REQ:
		{
			if (prIwReqData->data.length >
			    (sizeof(prGlueInfo->aucWSCIE)/sizeof(UINT_8))) {
				status = -EINVAL;
				break;
			}
			/* retrieve IE for Probe Request */
			if (prIwReqData->data.length > 0) {
				if (copy_from_user(prGlueInfo->aucWSCIE, prIwReqData->data.pointer,
						   prIwReqData->data.length)) {
					status = -EFAULT;
					break;
				}
				prGlueInfo->u2WSCIELen = prIwReqData->data.length;
			} else {
				prGlueInfo->u2WSCIELen = 0;
			}
		}
		break;
#endif
	case PRIV_CMD_OID:
		if (prIwReqData->data.length >
		    (sizeof(aucOidBuf)/sizeof(UINT_8))) {
			status = -EINVAL;
			break;
		}
		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer, prIwReqData->data.length)) {
			status = -EFAULT;
			break;
		}
		if (!kalMemCmp(&aucOidBuf[0], pcExtra, prIwReqData->data.length)) {
			/* ToDo:: DBGLOG */
			DBGLOG(REQ, INFO, "pcExtra buffer is valid\n");
		} else {
			DBGLOG(REQ, INFO, "pcExtra 0x%p\n", pcExtra);
		}
		/* Execute this OID */
		status = priv_set_ndis(prNetDev, (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0], &u4BufLen);
		/* Copy result to user space */
		((P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0])->outNdisOidLength = u4BufLen;

		if (copy_to_user(prIwReqData->data.pointer,
				 &aucOidBuf[0], OFFSET_OF(NDIS_TRANSPORT_STRUCT, ndisOidContent))) {
			DBGLOG(REQ, INFO, "copy_to_user oidBuf fail\n");
			status = -EFAULT;
		}

		break;

	case PRIV_CMD_SW_CTRL:
		pu4IntBuf = (PUINT_32) prIwReqData->data.pointer;
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		/* kalMemCopy(&prNdisReq->ndisOidContent[0], prIwReqData->data.pointer, 8); */
		if (copy_from_user(&prNdisReq->ndisOidContent[0],
			prIwReqData->data.pointer, prIwReqData->data.length)) {
			status = -EFAULT;
			break;
		}
		prNdisReq->ndisOidCmd = OID_CUSTOM_SW_CTRL;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		/* Execute this OID */
		status = priv_set_ndis(prNetDev, prNdisReq, &u4BufLen);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Private ioctl get struct handler.
*
* \param[in] pDev Net device requested.
* \param[out] pIwReq Pointer to iwreq structure.
* \param[in] cmd Private sub-command.
*
* \retval 0 For success.
* \retval -EFAULT If copy from user space buffer fail.
* \retval -EOPNOTSUPP Parameter "cmd" not recognized.
*
*/
/*----------------------------------------------------------------------------*/
int
priv_get_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	UINT_32 u4SubCmd = 0;
	P_NDIS_TRANSPORT_STRUCT prNdisReq = NULL;

	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4BufLen = 0;
	PUINT_32 pu4IntBuf = NULL;
	PUINT_8 prDest = NULL;
	int status = 0;

	kalMemZero(&aucOidBuf[0], sizeof(aucOidBuf));

	ASSERT(prNetDev);
	ASSERT(prIwReqData);
	if (!prNetDev || !prIwReqData) {
		DBGLOG(REQ, INFO, "priv_get_struct(): invalid param(0x%p, 0x%p)\n", prNetDev, prIwReqData);
		return -EINVAL;
	}

	u4SubCmd = (UINT_32) prIwReqData->data.flags;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO, "priv_get_struct(): invalid prGlueInfo(0x%p, 0x%p)\n",
		       prNetDev, *((P_GLUE_INFO_T *) netdev_priv(prNetDev)));
		return -EINVAL;
	}

	if (prGlueInfo == NULL || prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, INFO, "adapter is not ready\n");
		return -EIO;
	}

#if 0
	DBGLOG(INIT, INFO, "priv_get_struct(): prIwReqInfo->cmd(0x%X), u4SubCmd(%ld)\n", prIwReqInfo->cmd, u4SubCmd);
#endif
	memset(aucOidBuf, 0, sizeof(aucOidBuf));

	switch (u4SubCmd) {
	case PRIV_CMD_OID:
		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer, sizeof(NDIS_TRANSPORT_STRUCT))) {
			DBGLOG(REQ, INFO, "priv_get_struct() copy_from_user oidBuf fail\n");
			return -EFAULT;
		}

		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];
#if 0
		DBGLOG(INIT, INFO, "\n priv_get_struct cmd 0x%02x len:%d OID:0x%08x OID Len:%d\n", cmd,
		       pIwReq->u.data.length, ndisReq->ndisOidCmd, ndisReq->inNdisOidlength);
#endif
		if (priv_get_ndis(prNetDev, prNdisReq, &u4BufLen) == 0) {
			prNdisReq->outNdisOidLength = u4BufLen;
			kalMemCopy(pcExtra, prNdisReq,
				u4BufLen + sizeof(NDIS_TRANSPORT_STRUCT) -
				sizeof(prNdisReq->ndisOidContent));
			if (copy_to_user(prIwReqData->data.pointer,
					 &aucOidBuf[0],
					 u4BufLen + sizeof(NDIS_TRANSPORT_STRUCT) -
					 sizeof(prNdisReq->ndisOidContent))) {
				DBGLOG(REQ, INFO, "priv_get_struct() copy_to_user oidBuf fail(1)\n");
				return -EFAULT;
			}
			return 0;
		}
		prNdisReq->outNdisOidLength = u4BufLen;
		if (copy_to_user(prIwReqData->data.pointer,
				 &aucOidBuf[0], OFFSET_OF(NDIS_TRANSPORT_STRUCT, ndisOidContent))) {
			DBGLOG(REQ, INFO, "priv_get_struct() copy_to_user oidBuf fail(2)\n");
		}
		return -EFAULT;

	case PRIV_CMD_SW_CTRL:
		pu4IntBuf = (PUINT_32) prIwReqData->data.pointer;
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];
		prDest = (PUINT_8) &aucOidBuf[OFFSET_OF(NDIS_TRANSPORT_STRUCT, ndisOidContent)];

		if (prIwReqData->data.length > (sizeof(aucOidBuf) - OFFSET_OF(NDIS_TRANSPORT_STRUCT, ndisOidContent))) {
			DBGLOG(REQ, INFO, "priv_get_struct() exceeds length limit\n");
			return -EFAULT;
		}

		if (copy_from_user(prDest, prIwReqData->data.pointer, prIwReqData->data.length)) {
			DBGLOG(REQ, INFO, "priv_get_struct() copy_from_user oidBuf fail\n");
			return -EFAULT;
		}

		prNdisReq->ndisOidCmd = OID_CUSTOM_SW_CTRL;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0) {
			prNdisReq->outNdisOidLength = u4BufLen;
			/* printk("len=%d Result=%08lx\n", u4BufLen, *(PUINT_32)&prNdisReq->ndisOidContent[4]); */

			if (copy_to_user(prIwReqData->data.pointer, &prNdisReq->ndisOidContent[4], 4))
				DBGLOG(REQ, INFO, "priv_get_struct() copy_to_user oidBuf fail(2)\n");
		}
		return 0;
	default:
		DBGLOG(REQ, WARN, "get struct cmd:0x%x\n", u4SubCmd);
		return -EOPNOTSUPP;
	}
}				/* priv_get_struct */

/*----------------------------------------------------------------------------*/
/*!
* \brief The routine handles a set operation for a single OID.
*
* \param[in] pDev Net device requested.
* \param[in] ndisReq Ndis request OID information copy from user.
* \param[out] outputLen_p If the call is successful, returns the number of
*                         bytes written into the query buffer. If the
*                         call failed due to invalid length of the query
*                         buffer, returns the amount of storage needed..
*
* \retval 0 On success.
* \retval -EOPNOTSUPP If cmd is not supported.
*
*/
/*----------------------------------------------------------------------------*/
static int
priv_set_ndis(IN struct net_device *prNetDev, IN NDIS_TRANSPORT_STRUCT * prNdisReq, OUT PUINT_32 pu4OutputLen)
{
	P_WLAN_REQ_ENTRY prWlanReqEntry = NULL;
	WLAN_STATUS status = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4SetInfoLen = 0;

	ASSERT(prNetDev);
	ASSERT(prNdisReq);
	ASSERT(pu4OutputLen);

	if (!prNetDev || !prNdisReq || !pu4OutputLen) {
		DBGLOG(REQ, INFO, "priv_set_ndis(): invalid param(0x%p, 0x%p, 0x%p)\n",
		       prNetDev, prNdisReq, pu4OutputLen);
		return -EINVAL;
	}

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO, "priv_set_ndis(): invalid prGlueInfo(0x%p, 0x%p)\n",
		       prNetDev, *((P_GLUE_INFO_T *) netdev_priv(prNetDev)));
		return -EINVAL;
	}
#if 0
	DBGLOG(INIT, INFO, "priv_set_ndis(): prNdisReq->ndisOidCmd(0x%lX)\n", prNdisReq->ndisOidCmd);
#endif

	if (reqSearchSupportedOidEntry(prNdisReq->ndisOidCmd, &prWlanReqEntry) == FALSE) {
		/* WARNLOG(("Set OID: 0x%08lx (unknown)\n", prNdisReq->ndisOidCmd)); */
		return -EOPNOTSUPP;
	}

	if (prWlanReqEntry->pfOidSetHandler == NULL) {
		/* WARNLOG(("Set %s: Null set handler\n", prWlanReqEntry->pucOidName)); */
		return -EOPNOTSUPP;
	}
#if 0
	DBGLOG(INIT, INFO, "priv_set_ndis(): %s\n", prWlanReqEntry->pucOidName);
#endif

	if (prWlanReqEntry->fgSetBufLenChecking) {
		if (prNdisReq->inNdisOidlength != prWlanReqEntry->u4InfoBufLen) {
			DBGLOG(REQ, WARN,
			"Set %s: Invalid length (current=%d, needed=%d)\n",
			       prWlanReqEntry->pucOidName, prNdisReq->inNdisOidlength, prWlanReqEntry->u4InfoBufLen);

			*pu4OutputLen = prWlanReqEntry->u4InfoBufLen;
			return -EINVAL;
		}
	}

	if (prWlanReqEntry->eOidMethod == ENUM_OID_GLUE_ONLY) {
		/* GLUE sw info only */
		status = prWlanReqEntry->pfOidSetHandler(prGlueInfo,
							 prNdisReq->ndisOidContent,
							 prNdisReq->inNdisOidlength, &u4SetInfoLen);
	} else if (prWlanReqEntry->eOidMethod == ENUM_OID_GLUE_EXTENSION) {
		/* multiple sw operations */
		status = prWlanReqEntry->pfOidSetHandler(prGlueInfo,
							 prNdisReq->ndisOidContent,
							 prNdisReq->inNdisOidlength, &u4SetInfoLen);
	} else if (prWlanReqEntry->eOidMethod == ENUM_OID_DRIVER_CORE) {
		/* driver core */

		status = kalIoctl(prGlueInfo,
				  (PFN_OID_HANDLER_FUNC) prWlanReqEntry->pfOidSetHandler,
				  prNdisReq->ndisOidContent,
				  prNdisReq->inNdisOidlength, FALSE, FALSE, TRUE, &u4SetInfoLen);
	} else {
		DBGLOG(REQ, INFO, "priv_set_ndis(): unsupported OID method:0x%x\n", prWlanReqEntry->eOidMethod);
		return -EOPNOTSUPP;
	}

	*pu4OutputLen = u4SetInfoLen;

	switch (status) {
	case WLAN_STATUS_SUCCESS:
		break;

	case WLAN_STATUS_INVALID_LENGTH:
		/* WARNLOG(("Set %s: Invalid length (current=%ld, needed=%ld)\n", */
		/* prWlanReqEntry->pucOidName, */
		/* prNdisReq->inNdisOidlength, */
		/* u4SetInfoLen)); */
		break;
	}

	if (status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return 0;
}				/* priv_set_ndis */

/*----------------------------------------------------------------------------*/
/*!
* \brief The routine handles a query operation for a single OID. Basically we
*   return information about the current state of the OID in question.
*
* \param[in] pDev Net device requested.
* \param[in] ndisReq Ndis request OID information copy from user.
* \param[out] outputLen_p If the call is successful, returns the number of
*                        bytes written into the query buffer. If the
*                        call failed due to invalid length of the query
*                        buffer, returns the amount of storage needed..
*
* \retval 0 On success.
* \retval -EOPNOTSUPP If cmd is not supported.
* \retval -EINVAL invalid input parameters
*
*/
/*----------------------------------------------------------------------------*/
static int
priv_get_ndis(IN struct net_device *prNetDev, IN NDIS_TRANSPORT_STRUCT * prNdisReq, OUT PUINT_32 pu4OutputLen)
{
	P_WLAN_REQ_ENTRY prWlanReqEntry = NULL;
	UINT_32 u4BufLen = 0;
	WLAN_STATUS status = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(prNetDev);
	ASSERT(prNdisReq);
	ASSERT(pu4OutputLen);

	if (!prNetDev || !prNdisReq || !pu4OutputLen) {
		DBGLOG(REQ, INFO, "priv_get_ndis(): invalid param(0x%p, 0x%p, 0x%p)\n",
		       prNetDev, prNdisReq, pu4OutputLen);
		return -EINVAL;
	}

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO, "priv_get_ndis(): invalid prGlueInfo(0x%p, 0x%p)\n",
		       prNetDev, *((P_GLUE_INFO_T *) netdev_priv(prNetDev)));
		return -EINVAL;
	}
#if 0
	DBGLOG(INIT, INFO, "priv_get_ndis(): prNdisReq->ndisOidCmd(0x%lX)\n", prNdisReq->ndisOidCmd);
#endif

	if (reqSearchSupportedOidEntry(prNdisReq->ndisOidCmd, &prWlanReqEntry) == FALSE) {
		/* WARNLOG(("Query OID: 0x%08lx (unknown)\n", prNdisReq->ndisOidCmd)); */
		return -EOPNOTSUPP;
	}

	if (prWlanReqEntry->pfOidQueryHandler == NULL) {
		/* WARNLOG(("Query %s: Null query handler\n", prWlanReqEntry->pucOidName)); */
		return -EOPNOTSUPP;
	}
#if 0
	DBGLOG(INIT, INFO, "priv_get_ndis(): %s\n", prWlanReqEntry->pucOidName);
#endif

	if (prWlanReqEntry->fgQryBufLenChecking) {
		if (prNdisReq->inNdisOidlength < prWlanReqEntry->u4InfoBufLen) {
			/* Not enough room in InformationBuffer. Punt */
			/* WARNLOG(("Query %s: Buffer too short (current=%ld, needed=%ld)\n", */
			/* prWlanReqEntry->pucOidName, */
			/* prNdisReq->inNdisOidlength, */
			/* prWlanReqEntry->u4InfoBufLen)); */

			*pu4OutputLen = prWlanReqEntry->u4InfoBufLen;

			status = WLAN_STATUS_INVALID_LENGTH;
			return -EINVAL;
		}
	}

	if (prWlanReqEntry->eOidMethod == ENUM_OID_GLUE_ONLY) {
		/* GLUE sw info only */
		status = prWlanReqEntry->pfOidQueryHandler(prGlueInfo,
							   prNdisReq->ndisOidContent,
							   prNdisReq->inNdisOidlength, &u4BufLen);
	} else if (prWlanReqEntry->eOidMethod == ENUM_OID_GLUE_EXTENSION) {
		/* multiple sw operations */
		status = prWlanReqEntry->pfOidQueryHandler(prGlueInfo,
							   prNdisReq->ndisOidContent,
							   prNdisReq->inNdisOidlength, &u4BufLen);
	} else if (prWlanReqEntry->eOidMethod == ENUM_OID_DRIVER_CORE) {
		/* driver core */

		status = kalIoctl(prGlueInfo,
				  (PFN_OID_HANDLER_FUNC) prWlanReqEntry->pfOidQueryHandler,
				  prNdisReq->ndisOidContent, prNdisReq->inNdisOidlength, TRUE, TRUE, TRUE, &u4BufLen);
	} else {
		DBGLOG(REQ, INFO, "priv_set_ndis(): unsupported OID method:0x%x\n", prWlanReqEntry->eOidMethod);
		return -EOPNOTSUPP;
	}

	*pu4OutputLen = u4BufLen;

	switch (status) {
	case WLAN_STATUS_SUCCESS:
		break;

	case WLAN_STATUS_INVALID_LENGTH:
		/* WARNLOG(("Set %s: Invalid length (current=%ld, needed=%ld)\n", */
		/* prWlanReqEntry->pucOidName, */
		/* prNdisReq->inNdisOidlength, */
		/* u4BufLen)); */
		break;
	}

	if (status != WLAN_STATUS_SUCCESS)
		return -EOPNOTSUPP;

	return 0;
}				/* priv_get_ndis */

#if CFG_SUPPORT_QA_TOOL
/*----------------------------------------------------------------------------*/
/*!
* \brief The routine handles ATE set operation.
*
* \param[in] pDev Net device requested.
* \param[in] ndisReq Ndis request OID information copy from user.
* \param[out] outputLen_p If the call is successful, returns the number of
*                         bytes written into the query buffer. If the
*                         call failed due to invalid length of the query
*                         buffer, returns the amount of storage needed..
*
* \retval 0 On success.
* \retval -EOPNOTSUPP If cmd is not supported.
* \retval -EFAULT If copy from user space buffer fail.
*
*/
/*----------------------------------------------------------------------------*/
int
priv_ate_set(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	P_GLUE_INFO_T GlueInfo;
	INT_32 i4Status;
	UINT_8 *InBuf;
	/* UINT_8 *addr_str, *value_str; */
	UINT_32 InBufLen;
	UINT_32 u4SubCmd;
	/* BOOLEAN isWrite = 0;
	*UINT_32 u4BufLen = 0;
	*P_NDIS_TRANSPORT_STRUCT prNdisReq;
	*UINT_32 pu4IntBuf[2];
	*/

	/* sanity check */
	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);

	/* init */
	DBGLOG(REQ, INFO, "priv_set_string (%s)(%d)\n",
	       (UINT_8 *) prIwReqData->data.pointer, (INT_32) prIwReqData->data.length);

	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;

	GlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (GlueInfo == NULL || GlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, INFO, "adapter is not ready\n");
		return -EIO;
	}

	u4SubCmd = (UINT_32) prIwReqData->data.flags;

	DBGLOG(REQ, INFO, "MT6632 : priv_ate_set u4SubCmd = %d\n", u4SubCmd);

	switch (u4SubCmd) {
	case PRIV_QACMD_SET:
		DBGLOG(REQ, INFO, "MT6632 : priv_ate_set PRIV_QACMD_SET\n");
		InBuf = aucOidBuf;
		InBufLen = prIwReqData->data.length;
		i4Status = 0;

		if (copy_from_user(InBuf, prIwReqData->data.pointer, prIwReqData->data.length))
			return -EFAULT;
		i4Status = AteCmdSetHandle(prNetDev, InBuf, InBufLen);
		break;

	default:
		return -EOPNOTSUPP;
	}
	return 0;
}
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
static BOOLEAN reqSearchSupportedOidEntry(IN UINT_32 rOid, OUT P_WLAN_REQ_ENTRY *ppWlanReqEntry)
{
	INT_32 i, j, k;

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
* \brief Private ioctl driver handler.
*
* \param[in] pDev Net device requested.
* \param[out] pIwReq Pointer to iwreq structure.
* \param[in] cmd Private sub-command.
*
* \retval 0 For success.
* \retval -EFAULT If copy from user space buffer fail.
* \retval -EOPNOTSUPP Parameter "cmd" not recognized.
*
*/
/*----------------------------------------------------------------------------*/
int
priv_set_driver(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	UINT_32 u4SubCmd = 0;
	UINT_16 u2Cmd = 0;

	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = 0;

	ASSERT(prNetDev);
	ASSERT(prIwReqData);
	if (!prNetDev || !prIwReqData) {
		DBGLOG(REQ, INFO, "priv_set_driver(): invalid param(0x%p, 0x%p)\n", prNetDev, prIwReqData);
		return -EINVAL;
	}

	u2Cmd = prIwReqInfo->cmd;
	DBGLOG(REQ, INFO, "prIwReqInfo->cmd %u\n", u2Cmd);

	u4SubCmd = (UINT_32) prIwReqData->data.flags;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO, "priv_set_driver(): invalid prGlueInfo(0x%p, 0x%p)\n",
		       prNetDev, *((P_GLUE_INFO_T *) netdev_priv(prNetDev)));
		return -EINVAL;
	}

	if (prGlueInfo == NULL || prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(REQ, INFO, "adapter is not ready\n");
		return -EIO;
	}

	/* trick,hack in ./net/wireless/wext-priv.c ioctl_private_iw_point */
	/* because the cmd number is odd (get), the input string will not be copy_to_user */

	DBGLOG(REQ, INFO, "prIwReqData->data.length %u\n", prIwReqData->data.length);

	/* Use GET type becauase large data by iwpriv. */

	ASSERT(IW_IS_GET(u2Cmd));
	if (prIwReqData->data.length != 0) {
		if (!access_ok(VERIFY_READ, prIwReqData->data.pointer, prIwReqData->data.length)) {
			DBGLOG(REQ, INFO, "%s access_ok Read fail written = %d\n", __func__, i4BytesWritten);
			return -EFAULT;
		}
		if (copy_from_user(pcExtra, prIwReqData->data.pointer, prIwReqData->data.length)) {
			DBGLOG(REQ, INFO, "%s copy_form_user fail written = %d\n", __func__, prIwReqData->data.length);
			return -EFAULT;
		}
	}

	if (pcExtra) {
		DBGLOG(REQ, INFO, "pcExtra %s\n", pcExtra);
		/* Please check max length in rIwPrivTable */
		DBGLOG(REQ, INFO, "%s prIwReqData->data.length = %d\n", __func__, prIwReqData->data.length);
		i4BytesWritten = priv_driver_cmds(prNetDev, pcExtra, 2000 /*prIwReqData->data.length */);
		DBGLOG(REQ, INFO, "%s i4BytesWritten = %d\n", __func__, i4BytesWritten);
	}

	DBGLOG(REQ, INFO, "pcExtra done\n");

	if (i4BytesWritten > 0) {

		if (i4BytesWritten > 2000)
			i4BytesWritten = 2000;
		prIwReqData->data.length = i4BytesWritten;	/* the iwpriv will use the length */

	} else if (i4BytesWritten == 0) {
		prIwReqData->data.length = i4BytesWritten;
	}
#if 0
	/* trick,hack in ./net/wireless/wext-priv.c ioctl_private_iw_point */
	/* because the cmd number is even (set), the return string will not be copy_to_user */
	ASSERT(IW_IS_SET(u2Cmd));
	if (!access_ok(VERIFY_WRITE, prIwReqData->data.pointer, i4BytesWritten)) {
		DBGLOG(REQ, INFO, "%s access_ok Write fail written = %d\n", __func__, i4BytesWritten);
		return -EFAULT;
	}
	if (copy_to_user(prIwReqData->data.pointer, pcExtra, i4BytesWritten)) {
		DBGLOG(REQ, INFO, "%s copy_to_user fail written = %d\n", __func__, i4BytesWritten);
		return -EFAULT;
	}
	DBGLOG(RSN, INFO, "%s copy_to_user written = %d\n", __func__, i4BytesWritten);
#endif
	return 0;

}				/* priv_set_driver */
#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the radio configuration used in IBSS
*        mode and RF test mode.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuffer     Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
static WLAN_STATUS
reqExtQueryConfiguration(IN P_GLUE_INFO_T prGlueInfo,
			 OUT PVOID pvQueryBuffer, IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_802_11_CONFIG_T prQueryConfig = (P_PARAM_802_11_CONFIG_T) pvQueryBuffer;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4QueryInfoLen = 0;

	DEBUGFUNC("wlanoidQueryConfiguration");

	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(PARAM_802_11_CONFIG_T);
	if (u4QueryBufferLen < sizeof(PARAM_802_11_CONFIG_T))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvQueryBuffer);

	kalMemZero(prQueryConfig, sizeof(PARAM_802_11_CONFIG_T));

	/* Update the current radio configuration. */
	prQueryConfig->u4Length = sizeof(PARAM_802_11_CONFIG_T);

#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidSetBeaconInterval,
			       &prQueryConfig->u4BeaconPeriod, sizeof(UINT_32), TRUE, TRUE, &u4QueryInfoLen);
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQueryBeaconInterval,
				       &prQueryConfig->u4BeaconPeriod, sizeof(UINT_32), &u4QueryInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidQueryAtimWindow,
			       &prQueryConfig->u4ATIMWindow, sizeof(UINT_32), TRUE, TRUE, &u4QueryInfoLen);
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQueryAtimWindow,
				       &prQueryConfig->u4ATIMWindow, sizeof(UINT_32), &u4QueryInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidQueryFrequency,
			       &prQueryConfig->u4DSConfig, sizeof(UINT_32), TRUE, TRUE, &u4QueryInfoLen);
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQueryFrequency,
				       &prQueryConfig->u4DSConfig, sizeof(UINT_32), &u4QueryInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;

	prQueryConfig->rFHConfig.u4Length = sizeof(PARAM_802_11_CONFIG_FH_T);

	return rStatus;

}				/* end of reqExtQueryConfiguration() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the radio configuration used in IBSS
*        mode.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] pvSetBuffer    A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_NOT_ACCEPTED
*/
/*----------------------------------------------------------------------------*/
static WLAN_STATUS
reqExtSetConfiguration(IN P_GLUE_INFO_T prGlueInfo,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_802_11_CONFIG_T prNewConfig = (P_PARAM_802_11_CONFIG_T) pvSetBuffer;
	UINT_32 u4SetInfoLen = 0;

	DEBUGFUNC("wlanoidSetConfiguration");

	ASSERT(prGlueInfo);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_802_11_CONFIG_T);

	if (u4SetBufferLen < *pu4SetInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	/* OID_802_11_CONFIGURATION. If associated, NOT_ACCEPTED shall be returned. */
	if (prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED)
		return WLAN_STATUS_NOT_ACCEPTED;

	ASSERT(pvSetBuffer);

#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidSetBeaconInterval,
			       &prNewConfig->u4BeaconPeriod, sizeof(UINT_32), FALSE, TRUE, &u4SetInfoLen);
#else
	rStatus = wlanSetInformation(prGlueInfo->prAdapter,
				     wlanoidSetBeaconInterval,
				     &prNewConfig->u4BeaconPeriod, sizeof(UINT_32), &u4SetInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidSetAtimWindow,
			       &prNewConfig->u4ATIMWindow, sizeof(UINT_32), FALSE, TRUE, &u4SetInfoLen);
#else
	rStatus = wlanSetInformation(prGlueInfo->prAdapter,
				     wlanoidSetAtimWindow, &prNewConfig->u4ATIMWindow, sizeof(UINT_32), &u4SetInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidSetFrequency,
			       &prNewConfig->u4DSConfig, sizeof(UINT_32), FALSE, TRUE, &u4SetInfoLen);
#else
	rStatus = wlanSetInformation(prGlueInfo->prAdapter,
				     wlanoidSetFrequency, &prNewConfig->u4DSConfig, sizeof(UINT_32), &u4SetInfoLen);
#endif

	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;

	return rStatus;

}				/* end of reqExtSetConfiguration() */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set beacon detection function enable/disable state
*        This is mainly designed for usage under BT inquiry state (disable function).
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen The length of the set buffer
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*   bytes read from the set buffer. If the call failed due to invalid length of
*   the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA If new setting value is wrong.
* \retval WLAN_STATUS_INVALID_LENGTH
*
*/
/*----------------------------------------------------------------------------*/
static WLAN_STATUS
reqExtSetAcpiDevicePowerState(IN P_GLUE_INFO_T prGlueInfo,
			      IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prGlueInfo);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	/* WIFI is enabled, when ACPI is D0 (ParamDeviceStateD0 = 1). And vice versa */

	/* rStatus = wlanSetInformation(prGlueInfo->prAdapter, */
	/* wlanoidSetAcpiDevicePowerState, */
	/* pvSetBuffer, */
	/* u4SetBufferLen, */
	/* pu4SetInfoLen); */
	return rStatus;
}

#define CMD_START				"START"
#define CMD_STOP				"STOP"
#define CMD_SCAN_ACTIVE			"SCAN-ACTIVE"
#define CMD_SCAN_PASSIVE		"SCAN-PASSIVE"
#define CMD_RSSI				"RSSI"
#define CMD_LINKSPEED			"LINKSPEED"
#define CMD_RXFILTER_START		"RXFILTER-START"
#define CMD_RXFILTER_STOP		"RXFILTER-STOP"
#define CMD_RXFILTER_ADD		"RXFILTER-ADD"
#define CMD_RXFILTER_REMOVE		"RXFILTER-REMOVE"
#define CMD_BTCOEXSCAN_START	"BTCOEXSCAN-START"
#define CMD_BTCOEXSCAN_STOP		"BTCOEXSCAN-STOP"
#define CMD_BTCOEXMODE			"BTCOEXMODE"
#define CMD_SETSUSPENDOPT		"SETSUSPENDOPT"
#define CMD_SETSUSPENDMODE		"SETSUSPENDMODE"
#define CMD_P2P_DEV_ADDR		"P2P_DEV_ADDR"
#define CMD_SETFWPATH			"SETFWPATH"
#define CMD_SETBAND				"SETBAND"
#define CMD_GETBAND				"GETBAND"
#define CMD_AP_START			"AP_START"

#if CFG_SUPPORT_QA_TOOL
#define CMD_GET_RX_STATISTICS	"GET_RX_STATISTICS"
#endif
#define CMD_GET_STAT		"GET_STAT"
#define CMD_GET_BSS_STATISTICS	"GET_BSS_STATISTICS"
#define CMD_GET_STA_STATISTICS	"GET_STA_STATISTICS"
#define CMD_GET_WTBL_INFO	"GET_WTBL"
#define CMD_GET_MIB_INFO	"GET_MIB"
#if CFG_SUPPORT_LAST_SEC_MCS_INFO
#define CMD_GET_MCS_INFO	"GET_MCS_INFO"
#endif
#define CMD_GET_STA_INFO	"GET_STA"
#define CMD_SET_FW_LOG		"SET_FWLOG"
#define CMD_GET_QUE_INFO	"GET_QUE"
#define CMD_GET_MEM_INFO	"GET_MEM"
#define CMD_GET_HIF_INFO	"GET_HIF"
#define CMD_GET_STA_KEEP_CNT    "KEEPCOUNTER"
#define CMD_STAT_RESET_CNT      "RESETCOUNTER"
#define CMD_STAT_NOISE_SEL      "NOISESELECT"
#define CMD_STAT_GROUP_SEL      "GROUP"


#define CMD_SET_TXPOWER			"SET_TXPOWER"
#define CMD_COUNTRY				"COUNTRY"
#define CMD_GET_COUNTRY			"GET_COUNTRY"
#define CMD_GET_CHANNELS		"GET_CHANNELS"
#define CMD_P2P_SET_NOA			"P2P_SET_NOA"
#define CMD_P2P_GET_NOA			"P2P_GET_NOA"
#define CMD_P2P_SET_PS			"P2P_SET_PS"
#define CMD_SET_AP_WPS_P2P_IE	"SET_AP_WPS_P2P_IE"
#define CMD_SETROAMMODE	"SETROAMMODE"
#define CMD_MIRACAST		"MIRACAST"

#if (CFG_SUPPORT_DFS_MASTER == 1)
#define CMD_SHOW_DFS_STATE			"SHOW_DFS_STATE"
#define CMD_SHOW_DFS_RADAR_PARAM	"SHOW_DFS_RADAR_PARAM"
#define CMD_SHOW_DFS_HELP			"SHOW_DFS_HELP"
#define CMD_SHOW_DFS_CAC_TIME		"SHOW_DFS_CAC_TIME"
#endif

#define CMD_PNOSSIDCLR_SET	"PNOSSIDCLR"
#define CMD_PNOSETUP_SET	"PNOSETUP "
#define CMD_PNOENABLE_SET	"PNOFORCE"
#define CMD_PNODEBUG_SET	"PNODEBUG"
#define CMD_WLS_BATCHING	"WLS_BATCHING"

#define CMD_OKC_SET_PMK		"SET_PMK"
#define CMD_OKC_ENABLE		"OKC_ENABLE"

#define CMD_SETMONITOR		"MONITOR"
#define CMD_SETBUFMODE		"BUFFER_MODE"
#define CMD_SETEEPROM_MODE	"EEPROM_MODE"

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
#define CMD_GET_CH_RANK_LIST "GET_CH_RANK_LIST"
#define CMD_GET_CH_DIRTINESS "GET_CH_DIRTINESS"
#endif

#define CMD_EFUSE		"EFUSE"
#define CMD_CCCR		"CCCR"


/* miracast related definition */
#define MIRACAST_MODE_OFF	0
#define MIRACAST_MODE_SOURCE	1
#define MIRACAST_MODE_SINK	2

#ifndef MIRACAST_AMPDU_SIZE
#define MIRACAST_AMPDU_SIZE	8
#endif

#ifndef MIRACAST_MCHAN_ALGO
#define MIRACAST_MCHAN_ALGO     1
#endif

#ifndef MIRACAST_MCHAN_BW
#define MIRACAST_MCHAN_BW       25
#endif

#define	CMD_BAND_AUTO	0
#define	CMD_BAND_5G		1
#define	CMD_BAND_2G		2
#define	CMD_BAND_ALL	3

/* Mediatek private command */
#define CMD_SET_MCR				"SET_MCR"
#define CMD_GET_MCR	            "GET_MCR"
#define CMD_SET_DRV_MCR			"SET_DRV_MCR"
#define CMD_GET_DRV_MCR			"GET_DRV_MCR"
#define CMD_SET_SW_CTRL	        "SET_SW_CTRL"
#define CMD_GET_SW_CTRL         "GET_SW_CTRL"
#define CMD_SET_CFG             "SET_CFG"
#define CMD_GET_CFG             "GET_CFG"
#define CMD_SET_CHIP            "SET_CHIP"
#define CMD_GET_CHIP            "GET_CHIP"
#define CMD_SET_DBG_LEVEL       "SET_DBG_LEVEL"
#define CMD_GET_DBG_LEVEL       "GET_DBG_LEVEL"
#ifdef CFG_ALPS_ANDROID_AOSP_PRIV_CMD
#define PRIV_CMD_SIZE 512
#else
#define PRIV_CMD_SIZE 2000
#endif /* CFG_ALPS_ANDROID_AOSP_PRIV_CMD */
#define CMD_SET_FIXED_RATE      "FixedRate"
#define CMD_GET_VERSION         "VER"
#define CMD_SET_TEST_MODE		"SET_TEST_MODE"
#define CMD_SET_TEST_CMD		"SET_TEST_CMD"
#define CMD_GET_TEST_RESULT		"GET_TEST_RESULT"
#define CMD_GET_STA_STAT        "STAT"
#define CMD_GET_STA_STAT2       "STAT2"
#define CMD_GET_STA_RX_STAT		"RX_STAT"
#define CMD_SET_ACL_POLICY      "SET_ACL_POLICY"
#define CMD_ADD_ACL_ENTRY       "ADD_ACL_ENTRY"
#define CMD_DEL_ACL_ENTRY       "DEL_ACL_ENTRY"
#define CMD_SHOW_ACL_ENTRY      "SHOW_ACL_ENTRY"
#define CMD_CLEAR_ACL_ENTRY     "CLEAR_ACL_ENTRY"
#define CMD_GET_CURR_AR_RATE	"GET_CURR_AR_RATE"
#define CMD_COEX_CONTROL        "COEX_CONTROL"
#if CFG_SUPPORT_CSI
#define CMD_SET_CSI             "SET_CSI"
#endif

#if CFG_WOW_SUPPORT
#define CMD_WOW_START			"WOW_START"
#define CMD_SET_WOW_ENABLE		"SET_WOW_ENABLE"
#define CMD_SET_WOW_PAR			"SET_WOW_PAR"
#define CMD_SET_WOW_UDP			"SET_WOW_UDP"
#define CMD_SET_WOW_TCP			"SET_WOW_TCP"
#define CMD_GET_WOW_PORT		"GET_WOW_PORT"
#define CMD_GET_WOW_REASON		"GET_WOW_REASON"
#endif
#define CMD_SET_ADV_PWS			"SET_ADV_PWS"
#define CMD_SET_MDTIM			"SET_MDTIM"
#define CMD_SET_LISTEN_DTIM_INTERVAL	"SET_LISTEN_DTIM_INTERVAL"

#define CMD_SET_DBDC			"SET_DBDC"

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
#define CMD_SET_CALBACKUP_TEST_DRV_FW		"SET_CALBACKUP_TEST_DRV_FW"
#endif

#define CMD_SET_P2P_PS			"SET_P2P_PS"
#define CMD_SET_P2P_NOA			"SET_P2P_NOA"

#define CMD_GET_CNM_INFO		"GET_CNM"
#define CMD_GET_DSLP_CNT		"GET_DSLEEP_CNT"

#define CMD_GET_BSS_TABLE "BSSTABLE_RSSI"

static UINT_8 g_ucMiracastMode = MIRACAST_MODE_OFF;

typedef struct cmd_tlv {
	char prefix;
	char version;
	char subver;
	char reserved;
} cmd_tlv_t;

typedef struct priv_driver_cmd_s {
	char buf[PRIV_CMD_SIZE];
	int used_len;
	int total_len;
} priv_driver_cmd_t;

#ifdef CFG_ANDROID_AOSP_PRIV_CMD
struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
};
#endif /* CFG_ANDROID_AOSP_PRIV_CMD */

#ifdef CFG_ALPS_ANDROID_AOSP_PRIV_CMD
struct android_wifi_priv_cmd {
	char buf[PRIV_CMD_SIZE];
	int used_len;
	int total_len;
};
#endif /* CFG_ALPS_ANDROID_AOSP_PRIV_CMD */

/* Coex Ctrl Cmd - Coex Info Content */
struct COEX_REF_TABLE {
	UINT_16 ucCoexInfoId;
	char *cContent;
};

const struct COEX_REF_TABLE coex_ref_table[] = {
	{1, "Isolation Detection Value"},
	{2, "Coex FDD Parameter"},
	{3, "Coex WMT Config"},
	{4, "Active BSSID"},
	{5, "Coex Mode"},
	{6, "Hybrid Mode"},
	{7, "Coex Iso"},
	{8, "Coex Channel Info"},
	{9, "Coex BT Profile"},
	{10, "BT Long Rx Disable WF Tx"},
	{11, "BT Port"},
	{12, "BT Tx Power"},
	{13, "TDD Band"},
	{30, "Perpkt Stat"},
	{31, "Perpkt BTHitCnt"},
	{32, "Perpkt RxProtectCtl"},
	{33, "Perpkt WfProtTime"},
	{34, "Perpkt BTDuration"},
	{35, "BtRxGainInfo"},
	{36, "BtTxPwrDist"},
	{37, "WfRxGainDist"}
};


int priv_driver_get_dbg_level(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4DbgIdx, u4DbgMask;
	BOOLEAN fgIsCmdAccept = FALSE;
	INT_32 u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		/* u4DbgIdx = kalStrtoul(apcArgv[1], NULL, 0); */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4DbgIdx);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n", u4Ret);

		if (wlanGetDebugLevel(u4DbgIdx, &u4DbgMask) == WLAN_STATUS_SUCCESS) {
			fgIsCmdAccept = TRUE;
			i4BytesWritten =
			    snprintf(pcCommand, i4TotalLen,
			    "Get DBG module[%u] log level => [0x%02x]!",
			    u4DbgIdx, (UINT_8) u4DbgMask);
		}
	}

	if (!fgIsCmdAccept)
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "Get DBG module log level failed!");

	return i4BytesWritten;

}				/* priv_driver_get_sw_ctrl */


#if CFG_SUPPORT_QA_TOOL
#if CFG_SUPPORT_BUFFER_MODE
static int priv_driver_set_eeprom_mode(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Argc = 0;
	INT_32 i4BytesWritten = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 arg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 2) {
		rStatus = kalkStrtou32(apcArgv[1], 0, &arg);
		if (rStatus) {
			DBGLOG(REQ, LOUD, "parse apcArgv error rStatus=%d\n", rStatus);
			return -1;
		}

		rStatus = priv_set_eeprom_mode(arg);
		if (rStatus) {
			DBGLOG(REQ, LOUD, "priv_set_eeprom_mode rStatus=%d\n", rStatus);
			return -1;
		}

		i4BytesWritten =
			snprintf(pcCommand, i4TotalLen, "Switch eeprom source as %s",
				(arg == EFUSE_MODE) ? "Efuse" : "Buffer Bin");
	}

	return i4BytesWritten;
}
static int priv_driver_set_efuse_buffer_mode(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	INT_32 i4BytesWritten = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	PARAM_CUSTOM_EFUSE_BUFFER_MODE_T *prSetEfuseBufModeInfo = NULL;
#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 0)
	BIN_CONTENT_T *pBinContent;
	int i = 0;
#endif
	PUINT_8 pucConfigBuf = NULL;
	UINT_32 u4ConfigReadLen;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	pucConfigBuf = (PUINT_8) kalMemAlloc(2048, VIR_MEM_TYPE);

	if (!pucConfigBuf) {
		DBGLOG(INIT, INFO, "allocate memory for pucConfigBuf failed\n");
		i4BytesWritten = -1;
		goto out;
	}
	kalMemZero(pucConfigBuf, 2048);
	u4ConfigReadLen = 0;

	if (kalReadToFile("/MT6632_eFuse_usage_table.xlsm.bin", pucConfigBuf, 2048, &u4ConfigReadLen) == 0) {
		/* ToDo:: Nothing */
	} else {
		DBGLOG(INIT, INFO, "can't find file\n");
		i4BytesWritten = -1;
		goto out;
	}

	/* pucConfigBuf */
	prSetEfuseBufModeInfo =
	(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T *) kalMemAlloc(sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T), VIR_MEM_TYPE);
	if (prSetEfuseBufModeInfo == NULL) {
		DBGLOG(INIT, INFO, "allocate memory for prSetEfuseBufModeInfo failed\n");
		i4BytesWritten = -1;
		goto out;
	}
	kalMemZero(prSetEfuseBufModeInfo, sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T));

	prSetEfuseBufModeInfo->ucSourceMode = 1;
	prSetEfuseBufModeInfo->ucCount = (UINT_8)EFUSE_CONTENT_SIZE;

#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 0)
	pBinContent = (BIN_CONTENT_T *)prSetEfuseBufModeInfo->aBinContent;
	for (i = 0; i < EFUSE_CONTENT_SIZE; i++) {
		pBinContent->u2Addr  = i;
		pBinContent->ucValue = *(pucConfigBuf + i);

		pBinContent++;
	}

	for (i = 0; i < 20; i++)
		DBGLOG(INIT, INFO, "%x\n", prSetEfuseBufModeInfo->aBinContent[i].ucValue);
#endif

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetEfusBufferMode,
			   prSetEfuseBufModeInfo, sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T), FALSE, FALSE, TRUE,
			   &u4BufLen);

	i4BytesWritten =
		snprintf(pcCommand, i4TotalLen, "set buffer mode %s",
			 (rStatus == WLAN_STATUS_SUCCESS) ? "success" : "fail");
out:
	if (pucConfigBuf)
		kalMemFree(pucConfigBuf, VIR_MEM_TYPE, 2048);

	if (prSetEfuseBufModeInfo)
		kalMemFree(prSetEfuseBufModeInfo, VIR_MEM_TYPE,
			sizeof(PARAM_CUSTOM_EFUSE_BUFFER_MODE_T));

	return i4BytesWritten;
}
#endif /* CFG_SUPPORT_BUFFER_MODE */

static int priv_driver_get_rx_statistics(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 u4Ret = 0;
	PARAM_CUSTOM_ACCESS_RX_STAT rRxStatisticsTest;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	DBGLOG(INIT, ERROR, "MT6632 : priv_driver_get_rx_statistics\n");

	if (i4Argc >= 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rRxStatisticsTest.u4SeqNum));
		rRxStatisticsTest.u4TotalNum = sizeof(PARAM_RX_STAT_T) / 4;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryRxStatistics,
				   &rRxStatisticsTest, sizeof(rRxStatisticsTest), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}

	return i4BytesWritten;
}
#endif /* CFG_SUPPORT_QA_TOOL */

#if CFG_SUPPORT_MSP
#if 0
static int priv_driver_get_stat(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 i4ArgNum = 2;
	PARAM_GET_STA_STA_STATISTICS rQueryStaStatistics;
	PARAM_RSSI rRssi;
	UINT_16 u2LinkSpeed;
	UINT_32 u4Per;
	UINTT_8 i;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));

	if (i4Argc >= i4ArgNum) {
		wlanHwAddrToBin(apcArgv[1], &rQueryStaStatistics.aucMacAddr[0]);

		rQueryStaStatistics.ucReadClear = TRUE;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryStaStatistics,
				   &rQueryStaStatistics,
				   sizeof(rQueryStaStatistics), TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			rRssi = RCPI_TO_dBm(rQueryStaStatistics.ucRcpi);
			u2LinkSpeed = rQueryStaStatistics.u2LinkSpeed == 0 ? 0 : rQueryStaStatistics.u2LinkSpeed/2;

			i4BytesWritten = kalScnprintf(pcCommand, i4TotalLen, "%s", "\n\nSTA Stat:\n");

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"CurrentTemperature            = %d\n", 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Tx success                    = %lu\n", 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Tx fail count                 = %ld, PER=%ld.%1ld%%\n", 0, 0, 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rx success                    = %lu\n", 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rx with CRC                   = %ld, PER=%ld.%1ld%%\n", 0, 0, 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rx with PhyErr                = %lu\n", 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rx with PlcpErr               = %lu\n", 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rx drop due to out of resource= %lu\n", 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rx duplicate frame            = %lu\n", 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"False CCA                     = %lu\n", 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"RSSI                          = %d %d %d %d\n", 0, 0, 0, 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Last TX Rate	       = %s, %s, %s, %s, %s\n", "NA", "NA", "NA", "NA", "NA");
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Last RX Rate	       = %s, %s, %s, %s, %s\n", "NA", "NA", "NA", "NA", "NA");

			for (i = 0; i < 2 /* band num */; i++) {
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"BandIdx:	       = %d\n", i);
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%s", "\tRange:  1   2~5   6~15   16~22   23~33   34~49   50~57   58~64\n");
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"\t\t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d\n",
					0, 0, 0, 0, 0, 0, 0, 0);
			}
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Tx success	       = %ld\n",
				rQueryStaStatistics.u4TransmitCount - rQueryStaStatistics.u4TransmitFailCount);

			u4Per = rQueryStaStatistics.u4TransmitFailCount == 0 ? 0 :
				(1000 * (rQueryStaStatistics.u4TransmitFailCount)) /
				rQueryStaStatistics.u4TransmitCount;
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Tx fail count	       = %ld, PER=%ld.%1ld%%\n",
				rQueryStaStatistics.u4TransmitFailCount, u4Per/10, u4Per%10);

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"RSSI		       = %d\n", rRssi);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"LinkSpeed	       = %d\n", u2LinkSpeed);
		}
	} else
		i4BytesWritten = kalScnprintf(pcCommand, i4TotalLen, "%s", "\n\nNo STA Stat:\n");

	return i4BytesWritten;
}
#endif


static int priv_driver_get_sta_statistics(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 i4ArgNum = 3;
	PARAM_GET_STA_STA_STATISTICS rQueryStaStatistics;
	PARAM_RSSI rRssi;
	UINT_16 u2LinkSpeed;
	UINT_32 u4Per;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));
	rQueryStaStatistics.ucReadClear = TRUE;

	if (i4Argc >= i4ArgNum) {
		if (strnicmp(apcArgv[1], CMD_GET_STA_KEEP_CNT,
			strlen(CMD_GET_STA_KEEP_CNT)) == 0) {
			wlanHwAddrToBin(apcArgv[2],
			&rQueryStaStatistics.aucMacAddr[0]);
			rQueryStaStatistics.ucReadClear = FALSE;
		} else if (strnicmp(apcArgv[2], CMD_GET_STA_KEEP_CNT,
			strlen(CMD_GET_STA_KEEP_CNT)) == 0) {
			wlanHwAddrToBin(apcArgv[1],
			&rQueryStaStatistics.aucMacAddr[0]);
			rQueryStaStatistics.ucReadClear = FALSE;
		}
	} else {
		/* Get AIS AP address for no argument */
		if (prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP) {
			COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr,
				prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP->aucMacAddr);
			DBGLOG(RSN, INFO, "use ais ap "MACSTR"\n",
				MAC2STR(prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP->aucMacAddr));
		} else {
			DBGLOG(RSN, INFO, "not connect to ais ap %lx\n",
				prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP);
			i4BytesWritten = kalScnprintf(pcCommand, i4TotalLen, "%s", "\n\nNo STA Stat:\n");
			return i4BytesWritten;
		}

		if (i4Argc == 2) {
			if (strnicmp(apcArgv[1], CMD_GET_STA_KEEP_CNT, strlen(CMD_GET_STA_KEEP_CNT)) == 0)
				rQueryStaStatistics.ucReadClear = FALSE;
		}
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStaStatistics,
			   &rQueryStaStatistics,
			   sizeof(rQueryStaStatistics), TRUE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_SUCCESS) {
		rRssi = RCPI_TO_dBm(rQueryStaStatistics.ucRcpi);
		u2LinkSpeed = rQueryStaStatistics.u2LinkSpeed == 0 ? 0 : rQueryStaStatistics.u2LinkSpeed/2;

		i4BytesWritten = kalScnprintf(pcCommand, i4TotalLen, "%s", "\n\nSTA Stat:\n");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"Tx total cnt           = %d\n",
			rQueryStaStatistics.u4TransmitCount);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"Tx success	       = %d\n",
			rQueryStaStatistics.u4TransmitCount - rQueryStaStatistics.u4TransmitFailCount);

		u4Per = rQueryStaStatistics.u4TransmitCount == 0 ? 0 :
			(1000 * (rQueryStaStatistics.u4TransmitFailCount)) /
			rQueryStaStatistics.u4TransmitCount;
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"Tx fail count	       = %d, PER=%d.%1d%%\n",
			rQueryStaStatistics.u4TransmitFailCount, u4Per/10, u4Per%10);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"RSSI		       = %d\n", rRssi);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"LinkSpeed	       = %d\n", u2LinkSpeed);

	} else
		i4BytesWritten = kalScnprintf(pcCommand, i4TotalLen, "%s", "\n\nNo STA Stat:\n");

	return i4BytesWritten;

}


static int priv_driver_get_bss_statistics(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus;
	PARAM_MAC_ADDRESS arBssid;
	UINT_32 u4BufLen;
	INT_32 i4Rssi;
	PARAM_GET_BSS_STATISTICS rQueryBssStatistics;
	P_NETDEV_PRIVATE_GLUE_INFO prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) NULL;
	UINT_8 ucBssIndex;
	INT_32 i4BytesWritten = 0;
#if 0
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Argc = 0;
	UINT_32	u4Index;
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid, &arBssid[0], sizeof(arBssid), &u4BufLen);

#if 0 /* Todo:: Get the none-AIS statistics */
	if (i4Argc >= 2)
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Index);
#endif

	/* 2. fill RSSI */
	if (prGlueInfo->eParamMediaStateIndicated != PARAM_MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
		return WLAN_STATUS_SUCCESS;
	}
	rStatus = kalIoctl(prGlueInfo, wlanoidQueryRssi, &i4Rssi,
		sizeof(i4Rssi), TRUE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "unable to retrieve rssi\n");


	/* 3 get per-BSS link statistics */
	if (rStatus == WLAN_STATUS_SUCCESS) {
		/* get Bss Index from ndev */
		prNetDevPrivate = (P_NETDEV_PRIVATE_GLUE_INFO) netdev_priv(prNetDev);
		ASSERT(prNetDevPrivate->prGlueInfo == prGlueInfo);
		ucBssIndex = prNetDevPrivate->ucBssIdx;

		kalMemZero(&rQueryBssStatistics, sizeof(rQueryBssStatistics));
		rQueryBssStatistics.ucBssIndex = ucBssIndex;

		rQueryBssStatistics.ucReadClear = TRUE;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryBssStatistics,
				   &rQueryBssStatistics,
				   sizeof(rQueryBssStatistics), TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			i4BytesWritten = kalScnprintf(pcCommand, i4TotalLen, "%s", "\n\nStat:\n");
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s", "CurrentTemperature    = -\n");
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Tx success	       = %d\n",
				rQueryBssStatistics.u4TransmitCount - rQueryBssStatistics.u4TransmitFailCount);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Tx fail count	       = %d\n",
				rQueryBssStatistics.u4TransmitFailCount);
#if 0
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rx success	       = %ld\n", 0);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rx with CRC	       = %ld\n", prStatistics->rFCSErrorCount.QuadPart);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s", "Rx with PhyErr	     = 0\n");
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s", "Rx with PlcpErr	     = 0\n");
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s", "Rx drop due to out of resource	= 0\n");
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rx duplicate frame    = %ld\n", prStatistics->rFrameDuplicateCount.QuadPart);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s", "False CCA	     = 0\n");
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"RSSI		       = %d\n", i4Rssi);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Last TX Rate	       = %s, %s, %s, %s, %s\n", "NA", "NA", "NA", "NA", "NA");
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Last RX Rate	       = %s, %s, %s, %s, %s\n", "NA", "NA", "NA", "NA", "NA");
#endif

		}

	} else {
		DBGLOG(REQ, WARN, "unable to retrieve per-BSS link statistics\n");
	}


	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	return i4BytesWritten;

}

char *HW_TX_MODE_STR[] = {"CCK", "OFDM", "MM", "GF", "VHT", "N/A"};
char *HW_TX_RATE_CCK_STR[] = {"1M", "2M", "5.5M", "11M", "N/A"};
char *HW_TX_RATE_OFDM_STR[] = {"6M", "9M", "12M", "18M", "24M",
				"36M", "48M", "54M", "N/A"};
char *HW_TX_RATE_BW[] = {"BW20", "BW40", "BW80", "BW160/BW8080", "N/A"};
enum {
	RATE_TBL_B = 0,
	RATE_TBL_G,
	RATE_TBL_N,
	RATE_TBL_N_2SS,
	RATE_TBL_AC,
	RATE_TBL_AC_2SS,
	RATE_TBL_MAX
};

static char *RATE_TBLE[] = {
	[RATE_TBL_B] = "B",
	[RATE_TBL_G] = "G",
	[RATE_TBL_N] = "N",
	[RATE_TBL_N_2SS] = "N_2SS",
	[RATE_TBL_AC] = "AC",
	[RATE_TBL_AC_2SS] = "AC_2SS",
	[RATE_TBL_MAX] = "N/A"
};
#if 0
static char *AR_STATE[] = {"NULL", "STEADY", "PROBE", "N/A"};
static char *AR_ACTION[] = {"NULL", "INDEX", "RATE_UP", "RATE_DOWN", "RATE_GRP", "RATE_BACK",
							"GI", "SGI_EN", "SGI_DIS", "PWR", "PWR_UP", "PWR_DOWN",
							"PWR_RESET_UP", "BF", "BF_EN", "BF_DIS", "N/A"};
#endif
#define BW_20		0
#define BW_40		1
#define BW_80		2
#define BW_160		3
#define BW_10		4
#define BW_5		6
#define BW_8080		7
#define BW_ALL		0xFF

char *hw_rate_ofdm_str(UINT_16 ofdm_idx)
{
	switch (ofdm_idx) {
	case 11: /* 6M */
		return HW_TX_RATE_OFDM_STR[0];
	case 15: /* 9M */
		return HW_TX_RATE_OFDM_STR[1];
	case 10: /* 12M */
		return HW_TX_RATE_OFDM_STR[2];
	case 14: /* 18M */
		return HW_TX_RATE_OFDM_STR[3];
	case 9: /* 24M */
		return HW_TX_RATE_OFDM_STR[4];
	case 13: /* 36M */
		return HW_TX_RATE_OFDM_STR[5];
	case 8: /* 48M */
		return HW_TX_RATE_OFDM_STR[6];
	case 12: /* 54M */
		return HW_TX_RATE_OFDM_STR[7];
	default:
		return HW_TX_RATE_OFDM_STR[8];
	}
}

static BOOL priv_driver_get_sgi_info(IN P_PARAM_PEER_CAP_T prWtblPeerCap)
{
	if (!prWtblPeerCap)
		return FALSE;

	switch (prWtblPeerCap->ucFrequencyCapability) {
	case BW_20:
		return prWtblPeerCap->fgG2;
	case BW_40:
		return prWtblPeerCap->fgG4;
	case BW_80:
		return prWtblPeerCap->fgG8;
	case BW_160:
		return prWtblPeerCap->fgG16;
	default:
		return FALSE;
	}
}

static BOOL priv_driver_get_ldpc_info(IN P_PARAM_TX_CONFIG_T prWtblTxConfig)
{
	if (!prWtblTxConfig)
		return FALSE;

	if (prWtblTxConfig->fgIsVHT)
		return prWtblTxConfig->fgVhtLDPC;
	else
		return prWtblTxConfig->fgLDPC;
}

INT_32 priv_driver_rate_to_string(IN char *pcCommand, IN int i4TotalLen, UINT_8 TxRx,
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo)
{
	UINT_8 i, txmode, rate, stbc;
	UINT_8 nss;
	INT_32 i4BytesWritten = 0;

	for (i = 0; i < AUTO_RATE_NUM; i++) {

		txmode = HW_TX_RATE_TO_MODE(prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);
		if (txmode >= MAX_TX_MODE)
			txmode = MAX_TX_MODE;
		rate = HW_TX_RATE_TO_MCS(prHwWlanInfo->rWtblRateInfo.au2RateCode[i], txmode);
		nss = HW_TX_RATE_TO_NSS(prHwWlanInfo->rWtblRateInfo.au2RateCode[i]) + 1;
		stbc = HW_TX_RATE_TO_STBC(prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRate index[%d] ", i);

		if (prHwWlanInfo->rWtblRateInfo.ucRateIdx == i) {
			if (TxRx == 0)
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%s", "[Last RX Rate] ");
			else
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%s", "[Last TX Rate] ");
		} else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s", "               ");

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? HW_TX_RATE_CCK_STR[rate] : HW_TX_RATE_CCK_STR[4]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", hw_rate_ofdm_str(rate));
		else {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"NSS%d_MCS%d, ", nss, rate);
		}

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s, ", HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", priv_driver_get_sgi_info(&prHwWlanInfo->rWtblPeerCap) == 0 ? "LGI" : "SGI");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s%s %s\n", HW_TX_MODE_STR[txmode],
			stbc ? "STBC" : " ",
			priv_driver_get_ldpc_info(&prHwWlanInfo->rWtblTxConfig) == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}

static INT_32 priv_driver_dump_helper_wtbl_info(IN char *pcCommand, IN int i4TotalLen,
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo)
{
	UINT_8 i;
	INT_32 i4BytesWritten = 0;

	ASSERT(pcCommand);

	i4BytesWritten = kalScnprintf(pcCommand, i4TotalLen, "%s", "\n\nwtbl:\n");
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"Dump WTBL info of WLAN_IDX	    = %d\n", prHwWlanInfo->u4Index);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tAddr="MACSTR"\n", MAC2STR(prHwWlanInfo->rWtblTxConfig.aucPA));
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tMUAR_Idx	 = %d\n", prHwWlanInfo->rWtblSecConfig.ucMUARIdx);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\trc_a1/rc_a2:%d/%d\n", prHwWlanInfo->rWtblSecConfig.fgRCA1,  prHwWlanInfo->rWtblSecConfig.fgRCA2);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tKID:%d/RCID:%d/RKV:%d/RV:%d/IKV:%d/WPI_FLAG:%d\n", prHwWlanInfo->rWtblSecConfig.ucKeyID,
		prHwWlanInfo->rWtblSecConfig.fgRCID, prHwWlanInfo->rWtblSecConfig.fgRKV,
		prHwWlanInfo->rWtblSecConfig.fgRV, prHwWlanInfo->rWtblSecConfig.fgIKV,
		prHwWlanInfo->rWtblSecConfig.fgEvenPN);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\tGID_SU:NA");
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tsw/DIS_RHTR:%d/%d\n", prHwWlanInfo->rWtblTxConfig.fgSW,  prHwWlanInfo->rWtblTxConfig.fgDisRxHdrTran);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tHT/VHT/HT-LDPC/VHT-LDPC/DYN_BW/MMSS:%d/%d/%d/%d/%d/%d\n",
		prHwWlanInfo->rWtblTxConfig.fgIsHT,  prHwWlanInfo->rWtblTxConfig.fgIsVHT,
		prHwWlanInfo->rWtblTxConfig.fgLDPC, prHwWlanInfo->rWtblTxConfig.fgVhtLDPC,
		prHwWlanInfo->rWtblTxConfig.fgDynBw, prHwWlanInfo->rWtblPeerCap.ucMMSS);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tFCAP/G2/G4/G8/G16/CBRN:%d/%d/%d/%d/%d/%d\n",
		prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability, prHwWlanInfo->rWtblPeerCap.fgG2,
		prHwWlanInfo->rWtblPeerCap.fgG4, prHwWlanInfo->rWtblPeerCap.fgG8, prHwWlanInfo->rWtblPeerCap.fgG16,
		prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tHT-TxBF(tibf/tebf):%d/%d, VHT-TxBF(tibf/tebf):%d/%d, PFMU_IDX=%d\n",
		prHwWlanInfo->rWtblTxConfig.fgTIBF, prHwWlanInfo->rWtblTxConfig.fgTEBF,
		prHwWlanInfo->rWtblTxConfig.fgVhtTIBF, prHwWlanInfo->rWtblTxConfig.fgVhtTEBF,
		prHwWlanInfo->rWtblTxConfig.ucPFMUIdx);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\tSPE_IDX=NA\n");
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tBA Enable:0x%x, BAFail Enable:%d\n", prHwWlanInfo->rWtblBaConfig.ucBaEn,
		prHwWlanInfo->rWtblTxConfig.fgBAFEn);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tQoS Enable:%d\n", prHwWlanInfo->rWtblTxConfig.fgIsQoS);
	if (prHwWlanInfo->rWtblTxConfig.fgIsQoS) {
		for (i = 0; i < 8; i += 2) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\t\tBA WinSize: TID 0 - %d, TID 1 - %d\n",
				(UINT_32)
				((prHwWlanInfo->rWtblBaConfig.u4BaWinSize >>
				(i * 3)) & BITS(0, 2)),
				(UINT_32)
				((prHwWlanInfo->rWtblBaConfig.u4BaWinSize >>
				((i + 1) * 3)) & BITS(0, 2)));
		}
	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tpartial_aid:%d\n", prHwWlanInfo->rWtblTxConfig.u2PartialAID);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\twpi_even:%d\n", prHwWlanInfo->rWtblSecConfig.fgEvenPN);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tAAD_OM/CipherSuit:%d/%d\n", prHwWlanInfo->rWtblTxConfig.fgAADOM,
		prHwWlanInfo->rWtblSecConfig.ucCipherSuit);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\taf:%d\n", prHwWlanInfo->rWtblPeerCap.ucAmpduFactor);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\trdg_ba:%d/rdg capability:%d\n", prHwWlanInfo->rWtblTxConfig.fgRdgBA,
		prHwWlanInfo->rWtblTxConfig.fgRDG);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tcipher_suit:%d\n", prHwWlanInfo->rWtblSecConfig.ucCipherSuit);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tFromDS:%d\n", prHwWlanInfo->rWtblTxConfig.fgIsFromDS);
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tToDS:%d\n", prHwWlanInfo->rWtblTxConfig.fgIsToDS);
#if 0
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tRCPI = %d %d %d %d\n", prHwWlanInfo->rWtblRxCounter.ucRxRcpi0,
		prHwWlanInfo->rWtblRxCounter.ucRxRcpi1,
		prHwWlanInfo->rWtblRxCounter.ucRxRcpi2,
		prHwWlanInfo->rWtblRxCounter.ucRxRcpi3);
#endif
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"\tRSSI = %d %d %d %d\n", RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi0),
		RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi1),
		RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi2),
		RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi3));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\tRate Info\n");

	i4BytesWritten += priv_driver_rate_to_string(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten, 1,
		prHwWlanInfo);

#if 0
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\t===Key======\n");
	for (i = 0; i < 32; i += 8) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\t0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 0], prHwWlanInfo->rWtblKeyConfig.aucKey[i + 1],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 2], prHwWlanInfo->rWtblKeyConfig.aucKey[i + 3],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 4], prHwWlanInfo->rWtblKeyConfig.aucKey[i + 5],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 6], prHwWlanInfo->rWtblKeyConfig.aucKey[i + 7]);
	}
#endif

	return i4BytesWritten;
}

static int priv_driver_get_wtbl_info(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	INT_32 u4Ret = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	/* DBGLOG(RSN, INFO, "MT6632 : priv_driver_get_wtbl_info\n"); */

	prHwWlanInfo = (P_PARAM_HW_WLAN_INFO_T)kalMemAlloc(sizeof(PARAM_HW_WLAN_INFO_T), VIR_MEM_TYPE);
	if (!prHwWlanInfo)
		return -1;

	if (i4Argc >= 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &prHwWlanInfo->u4Index);

		DBGLOG(REQ, INFO, "MT6632 : index = %d\n", prHwWlanInfo->u4Index);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryWlanInfo,
				   prHwWlanInfo, sizeof(PARAM_HW_WLAN_INFO_T), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, INFO, "rStatus %u u4BufLen = %d\n", rStatus, u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			kalMemFree(prHwWlanInfo, VIR_MEM_TYPE, sizeof(PARAM_HW_WLAN_INFO_T));
			return -1;
		}
		i4BytesWritten = priv_driver_dump_helper_wtbl_info(pcCommand, i4TotalLen, prHwWlanInfo);
	}

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	kalMemFree(prHwWlanInfo, VIR_MEM_TYPE, sizeof(PARAM_HW_WLAN_INFO_T));

	return i4BytesWritten;
}

static int priv_driver_get_sta_info(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_8 aucMacAddr[MAC_ADDR_LEN];
	UINT_8 ucWlanIndex;
	PUINT_8 pucMacAddr = NULL;
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo = NULL;
	PARAM_GET_STA_STA_STATISTICS rQueryStaStatistics;
	PARAM_RSSI rRssi;
	UINT_16 u2LinkSpeed;
	UINT_32 u4Per;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));
	rQueryStaStatistics.ucReadClear = TRUE;

	/* DBGLOG(RSN, INFO, "MT6632 : priv_driver_get_sta_info\n"); */
	if (i4Argc >= 3) {
		if (strnicmp(apcArgv[1], CMD_GET_STA_KEEP_CNT, strlen(CMD_GET_STA_KEEP_CNT)) == 0) {
			wlanHwAddrToBin(apcArgv[2], &aucMacAddr[0]);
			rQueryStaStatistics.ucReadClear = FALSE;
		} else if (strnicmp(apcArgv[2], CMD_GET_STA_KEEP_CNT, strlen(CMD_GET_STA_KEEP_CNT)) == 0) {
			wlanHwAddrToBin(apcArgv[1], &aucMacAddr[0]);
			rQueryStaStatistics.ucReadClear = FALSE;
		}

		if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter, &aucMacAddr[0], &ucWlanIndex))
			return i4BytesWritten;
	} else {
		/* Get AIS AP address for no argument */
		if (prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP)
			ucWlanIndex = prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP->ucWlanIndex;
		else if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter, NULL, &ucWlanIndex)) /* try get a peer */
			return i4BytesWritten;

		if (i4Argc == 2) {
			if (strnicmp(apcArgv[1], CMD_GET_STA_KEEP_CNT, strlen(CMD_GET_STA_KEEP_CNT)) == 0)
				rQueryStaStatistics.ucReadClear = FALSE;
		}
	}

	prHwWlanInfo = (P_PARAM_HW_WLAN_INFO_T)kalMemAlloc(sizeof(PARAM_HW_WLAN_INFO_T), VIR_MEM_TYPE);

	if (!prHwWlanInfo) {
		DBGLOG(REQ, ERROR, "alloc memory for prHwWlanInfo failed!\n");
		i4BytesWritten = -1;
		goto out;
	}
	prHwWlanInfo->u4Index = ucWlanIndex;

	DBGLOG(REQ, INFO, "MT6632 : index = %d i4TotalLen = %d\n", prHwWlanInfo->u4Index, i4TotalLen);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryWlanInfo,
			   prHwWlanInfo, sizeof(PARAM_HW_WLAN_INFO_T), TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "query prHwWlanInfo failed!\n");
		goto out;
	}

	i4BytesWritten = priv_driver_dump_helper_wtbl_info(pcCommand, i4TotalLen, prHwWlanInfo);

	pucMacAddr = wlanGetStaAddrByWlanIdx(prGlueInfo->prAdapter, ucWlanIndex);
	if (pucMacAddr) {
		COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, pucMacAddr);
		/* i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		*	"\tAddr="MACSTR"\n", MAC2STR(rQueryStaStatistics.aucMacAddr));
		*/

		rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryStaStatistics,
			   &rQueryStaStatistics,
			   sizeof(rQueryStaStatistics), TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			rRssi = RCPI_TO_dBm(rQueryStaStatistics.ucRcpi);
			u2LinkSpeed = rQueryStaStatistics.u2LinkSpeed == 0 ? 0 : rQueryStaStatistics.u2LinkSpeed/2;

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s", "\n\nSTA Stat:\n");

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Tx total cnt           = %d\n",
				rQueryStaStatistics.u4TransmitCount);

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Tx success	       = %d\n",
				rQueryStaStatistics.u4TransmitCount - rQueryStaStatistics.u4TransmitFailCount);

			u4Per = rQueryStaStatistics.u4TransmitCount == 0 ? 0 :
				(1000 * (rQueryStaStatistics.u4TransmitFailCount)) /
				rQueryStaStatistics.u4TransmitCount;
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Tx fail count	       = %d, PER=%d.%1d%%\n",
				rQueryStaStatistics.u4TransmitFailCount, u4Per/10, u4Per%10);

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"RSSI		       = %d\n", rRssi);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"LinkSpeed	       = %d\n", u2LinkSpeed);
		}
	}
	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

out:
	if (prHwWlanInfo)
		kalMemFree(prHwWlanInfo, VIR_MEM_TYPE, sizeof(PARAM_HW_WLAN_INFO_T));

	return i4BytesWritten;
}


static int priv_driver_get_mib_info(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	UINT_8 i;
	UINT_32 u4Per;
	INT_32 u4Ret = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	P_PARAM_HW_MIB_INFO_T prHwMibInfo;
	P_RX_CTRL_T prRxCtrl;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	prRxCtrl = &prGlueInfo->prAdapter->rRxCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	DBGLOG(REQ, INFO, "MT6632 : priv_driver_get_mib_info\n");

	prHwMibInfo = (P_PARAM_HW_MIB_INFO_T)kalMemAlloc(sizeof(PARAM_HW_MIB_INFO_T), VIR_MEM_TYPE);
	if (!prHwMibInfo)
		return -1;

	if (i4Argc == 1)
		prHwMibInfo->u4Index = 0;

	if (i4Argc >= 2)
		u4Ret = kalkStrtou32(apcArgv[1], 0, &prHwMibInfo->u4Index);

	DBGLOG(REQ, INFO, "MT6632 : index = %d\n", prHwMibInfo->u4Index);

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryMibInfo,
			prHwMibInfo, sizeof(PARAM_HW_MIB_INFO_T), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		kalMemFree(prHwMibInfo, VIR_MEM_TYPE, sizeof(PARAM_HW_MIB_INFO_T));
		return -1;
	}

	if (prHwMibInfo->u4Index < 2) {
		i4BytesWritten = kalScnprintf(pcCommand, i4TotalLen, "%s", "\n\nmib state:\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"Dump MIB info of IDX         = %d\n", prHwMibInfo->u4Index);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "===Rx Related Counters===\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx with CRC=%d\n",
			prHwMibInfo->rHwMibCnt.u4RxFcsErrCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx drop due to out of resource=%d\n",
			prHwMibInfo->rHwMibCnt.u4RxFifoFullCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx Mpdu=%d\n", prHwMibInfo->rHwMibCnt.u4RxMpduCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx AMpdu=%d\n", prHwMibInfo->rHwMibCnt.u4RxAMPDUCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx PF Drop=%d\n",
			prHwMibInfo->rHwMibCnt.u4PFDropCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx Len Mismatch=%d\n",
			prHwMibInfo->rHwMibCnt.u4RxLenMismatchCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx data indicate total=%ld\n", RX_GET_CNT(prRxCtrl, RX_DATA_INDICATION_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx data retain total=%ld\n", RX_GET_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx drop by SW total=%ld\n", RX_GET_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx reorder miss=%ld\n", RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_MISS_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx reorder within=%ld\n", RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_WITHIN_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx reorder ahead=%ld\n", RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_AHEAD_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx reorder behind=%ld\n", RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_BEHIND_COUNT));

		do {
			UINT_32 u4AmsduCntx100 = 0;

			if (RX_GET_CNT(prRxCtrl, RX_DATA_AMSDU_COUNT))
				u4AmsduCntx100 = (UINT_32)div64_u64(RX_GET_CNT(prRxCtrl,
					RX_DATA_MSDU_IN_AMSDU_COUNT) * 100,
					RX_GET_CNT(prRxCtrl, RX_DATA_AMSDU_COUNT));

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\tRx avg MSDU in AMSDU=%1d.%02d\n",
				u4AmsduCntx100 / 100, u4AmsduCntx100 % 100);
		} while (FALSE);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx total MSDU in AMSDU=%ld\n", RX_GET_CNT(prRxCtrl, RX_DATA_MSDU_IN_AMSDU_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx AMSDU=%ld\n", RX_GET_CNT(prRxCtrl, RX_DATA_AMSDU_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx AMSDU miss=%ld\n", RX_GET_CNT(prRxCtrl, RX_DATA_AMSDU_MISS_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx no StaRec drop=%ld\n", RX_GET_CNT(prRxCtrl, RX_NO_STA_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx inactive BSS drop=%ld\n", RX_GET_CNT(prRxCtrl, RX_INACTIVE_BSS_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx HS20 drop=%ld\n", RX_GET_CNT(prRxCtrl, RX_HS20_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx low SwRfb drop=%ld\n", RX_GET_CNT(prRxCtrl, RX_LESS_SW_RFB_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx dupicate drop=%ld\n", RX_GET_CNT(prRxCtrl, RX_DUPICATE_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx MIC err drop=%ld\n", RX_GET_CNT(prRxCtrl, RX_MIC_ERROR_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx BAR handle=%ld\n", RX_GET_CNT(prRxCtrl, RX_BAR_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx non-interest drop=%ld\n", RX_GET_CNT(prRxCtrl, RX_NO_INTEREST_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx type err drop=%ld\n", RX_GET_CNT(prRxCtrl, RX_TYPE_ERR_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx class err drop=%ld\n", RX_GET_CNT(prRxCtrl, RX_CLASS_ERR_DROP_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "===Phy/Timing Related Counters===\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tChannelIdleCnt=%d\n",
			prHwMibInfo->rHwMibCnt.u4ChannelIdleCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tCCA_NAV_Tx_Time=%d\n",
			prHwMibInfo->rHwMibCnt.u4CcaNavTx);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tRx_MDRDY_CNT=%d\n",
			prHwMibInfo->rHwMibCnt.u4MdrdyCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tCCK_MDRDY=%d, OFDM_MDRDY=0x%x, OFDM_GREEN_MDRDY=0x%x\n",
			prHwMibInfo->rHwMibCnt.u4CCKMdrdyCnt, prHwMibInfo->rHwMibCnt.u4OFDMLGMixMdrdy,
			prHwMibInfo->rHwMibCnt.u4OFDMGreenMdrdy);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tPrim CCA Time=%d\n",
			prHwMibInfo->rHwMibCnt.u4PCcaTime);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tSec CCA Time=%d\n",
			prHwMibInfo->rHwMibCnt.u4SCcaTime);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tPrim ED Time=%d\n",
			prHwMibInfo->rHwMibCnt.u4PEDTime);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "===Tx Related Counters(Generic)===\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tBeaconTxCnt=%d\n",
			prHwMibInfo->rHwMibCnt.u4BeaconTxCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tTx 40MHz Cnt=%d\n",
			prHwMibInfo->rHwMib2Cnt.u4Tx40MHzCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tTx 80MHz Cnt=%d\n",
			prHwMibInfo->rHwMib2Cnt.u4Tx80MHzCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tTx 160MHz Cnt=%d\n",
			prHwMibInfo->rHwMib2Cnt.u4Tx160MHzCnt);
		for (i = 0; i < BSSID_NUM; i++) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\t===BSSID[%d] Related Counters===\n", i);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\tBA Miss Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4BaMissedCnt[i]);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\tRTS Tx Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4RtsTxCnt[i]);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\tFrame Retry Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4FrameRetryCnt[i]);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\tFrame Retry 2 Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4FrameRetry2Cnt[i]);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\tRTS Retry Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4RtsRetryCnt[i]);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\tAck Failed Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4AckFailedCnt[i]);
		}

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "===AMPDU Related Counters===\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tTx AMPDU_Pkt_Cnt=%d\n",
			prHwMibInfo->rHwTxAmpduMts.u2TxAmpduCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tTx AMPDU_MPDU_Pkt_Cnt=%d\n",
			prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tAMPDU SuccessCnt=%d\n",
			prHwMibInfo->rHwTxAmpduMts.u4TxAckSfCnt);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tAMPDU Tx success      = %d\n",
			prHwMibInfo->rHwTxAmpduMts.u4TxAckSfCnt);

		u4Per = prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt == 0 ? 0 :
			(1000 * (prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt - prHwMibInfo->rHwTxAmpduMts.u4TxAckSfCnt)) /
			prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt;
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\tAMPDU Tx fail count   = %d, PER=%d.%1d%%\n",
			prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt - prHwMibInfo->rHwTxAmpduMts.u4TxAckSfCnt,
			u4Per/10, u4Per%10);
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "\tTx Agg\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "\tRange:  1    2~5   6~15    16~22   23~33    34~49    50~57    58~64\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\t\t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d\n",
			prHwMibInfo->rHwTxAmpduMts.u2TxRange1AmpduCnt, prHwMibInfo->rHwTxAmpduMts.u2TxRange2AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange3AmpduCnt, prHwMibInfo->rHwTxAmpduMts.u2TxRange4AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange5AmpduCnt, prHwMibInfo->rHwTxAmpduMts.u2TxRange6AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange7AmpduCnt, prHwMibInfo->rHwTxAmpduMts.u2TxRange8AmpduCnt);
	} else
		i4BytesWritten = kalScnprintf(pcCommand, i4TotalLen, "%s", "\nClear All Statistics\n");

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	kalMemFree(prHwMibInfo, VIR_MEM_TYPE, sizeof(PARAM_HW_MIB_INFO_T));

	nicRxClearStatistics(prGlueInfo->prAdapter);

	return i4BytesWritten;
}

/* Private Coex Ctrl Subcmd for Isolation Detection */
static int priv_driver_iso_detect(IN P_GLUE_INFO_T prGlueInfo,
				IN struct CMD_COEX_CTRL *prCmdCoexCtrl, IN signed char *argv[])
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	UINT_32 u4Ret = 0;

	struct CMD_COEX_ISO_DETECT rCmdCoexIsoDetect;

	rCmdCoexIsoDetect.u4Isolation = 0;

	u4Ret = kalkStrtou32(argv[2], 0, &(rCmdCoexIsoDetect.u4IsoPath));
	if (u4Ret) {
		DBGLOG(REQ, LOUD, " -priv_driver_coex_iso_detect - Parse Iso Path failed u4Ret=%d\n", u4Ret);
		return -1;
	}

	u4Ret = kalkStrtou32(argv[3], 0, &(rCmdCoexIsoDetect.u4Channel));
	if (u4Ret) {
		DBGLOG(REQ, LOUD, " -priv_driver_coex_iso_detect - Parse channel failed u4Ret = %d\n", u4Ret);
		return -1;
	}

	/* Copy Memory */
	kalMemCopy(prCmdCoexCtrl->aucBuffer, &rCmdCoexIsoDetect, sizeof(struct CMD_COEX_ISO_DETECT));

	/* Ioctl Isolation Detect */
	rStatus = kalIoctl(prGlueInfo,
			wlanoidQueryCoexIso,
			prCmdCoexCtrl,
			sizeof(struct CMD_COEX_CTRL),
			TRUE,
			TRUE,
			TRUE,
			&u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	/* If all pass, return u4Ret to 0 */
	return u4Ret;
}

/* Private Coex Ctrl Subcmd for Getting Coex Info */
static int priv_driver_get_coex_info(IN P_GLUE_INFO_T prGlueInfo,
				IN struct CMD_COEX_CTRL *prCmdCoexCtrl, IN signed char *argv[])
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	UINT_32 u4Ret = 0;

	struct CMD_COEX_GET_INFO rCmdCoexGetInfo;

	kalMemSet(&rCmdCoexGetInfo, 0, sizeof(struct CMD_COEX_GET_INFO));

	/* Copy Memory */
	kalMemCopy(prCmdCoexCtrl->aucBuffer, &rCmdCoexGetInfo, sizeof(struct CMD_COEX_GET_INFO));
	DBGLOG(REQ, INFO, "priv_driver_get_coex_info end\n");

	/* Ioctl Get Coex Info */
	rStatus = kalIoctl(prGlueInfo,
			wlanoidQueryCoexGetInfo,
			prCmdCoexCtrl,
			sizeof(struct CMD_COEX_CTRL),
			TRUE,
			TRUE,
			TRUE,
			&u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	/* If all pass, return u4Ret to 0 */
	return u4Ret;
}

/* Private Command for Coex Ctrl */
static int priv_driver_coex_ctrl(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	INT_32 i4ArgNum = 2;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4Ret = 0;
	UINT_32 u4Offset = 0;
	INT_32 i4SubArgNum;
	enum ENUM_COEX_CTRL_CMD CoexCtrlCmd;
	struct CMD_COEX_CTRL rCmdCoexCtrl;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	/* Prevent Kernel Panic, set default i4ArgNum to 2 */
	if (i4Argc >= i4ArgNum) {

		/* Parse Coex SubCmd */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &rCmdCoexCtrl.u4SubCmd);
		if (u4Ret)
			return -1;

		CoexCtrlCmd = (enum ENUM_COEX_CTRL_CMD) rCmdCoexCtrl.u4SubCmd;

		switch (CoexCtrlCmd) {
		/* Isolation Detection */
		case ENUM_COEX_CTRL_ISO_DETECT:
		{
			struct CMD_COEX_ISO_DETECT *prCmdCoexIsoDetect;

			i4SubArgNum = 3;
			/* Safely dereference "argv[3]".*/
			if (i4Argc < i4SubArgNum)
				break;
			/* Isolation Detection Method */
			u4Ret = priv_driver_iso_detect(prGlueInfo, &rCmdCoexCtrl, apcArgv);
			if (u4Ret)
				return -1;

			/* Get Isolation value */
			prCmdCoexIsoDetect = (struct CMD_COEX_ISO_DETECT *) rCmdCoexCtrl.aucBuffer;

			/* Set Return i4BytesWritten Value */
			u4Offset = snprintf(pcCommand, i4TotalLen, "%d",
				(prCmdCoexIsoDetect->u4Isolation/2));
			DBGLOG(REQ, INFO, "Isolation: %d\n",
				(prCmdCoexIsoDetect->u4Isolation/2));
			break;
		}
		case ENUM_COEX_CTRL_GET_INFO:
		{
			struct CMD_COEX_GET_INFO *prCmdCoexGetInfo;
			UINT_8 ucCoexTblSize = sizeof(coex_ref_table) / sizeof(struct COEX_REF_TABLE);
			UINT_8 ucCoexInfoNum, ucIdx = 0, ucTblIdx;
			UINT_16 u2CoexInfoId, u2CoexInfoLen;
			PUINT_32 pu4CoexInfo;

			u4Ret = priv_driver_get_coex_info(prGlueInfo, &rCmdCoexCtrl, apcArgv);
			if (u4Ret)
				return -1;

			prCmdCoexGetInfo = (struct CMD_COEX_GET_INFO *) rCmdCoexCtrl.aucBuffer;

			pu4CoexInfo = &prCmdCoexGetInfo->u4CoexInfo[0];
			ucCoexInfoNum = *pu4CoexInfo++ & 0xff;
			while (ucIdx < ucCoexInfoNum) {
				u2CoexInfoId = (*pu4CoexInfo >> 16) & 0xffff;
				u2CoexInfoLen = *pu4CoexInfo++ & 0xffff;

				/* Find Coex Info Cmd Id from priv_driver_coex_info_table[] */
				for (ucTblIdx = 0; ucTblIdx < ucCoexTblSize; ucTblIdx++) {
					if (u2CoexInfoId == coex_ref_table[ucTblIdx].ucCoexInfoId) {
						u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
							"\n%-25s:", coex_ref_table[ucTblIdx].cContent);
						break;
					}
				}

				/* If not found in priv_driver_coex_info_table[], show Unknown Id */
				if (ucTblIdx == ucCoexTblSize) {
					u4Offset += snprintf(pcCommand + u4Offset,
						i4TotalLen - u4Offset, "\nUnknown Cmd ID :%-9d:", u2CoexInfoId);
				}

				switch (u2CoexInfoLen) {
				case 0x0001:
				{
					u4Offset += snprintf(pcCommand + u4Offset,
					i4TotalLen - u4Offset, "0x%02x",
					*pu4CoexInfo++);
					break;
				}
				case 0x0002:
				{
					u4Offset += snprintf(pcCommand + u4Offset,
					i4TotalLen - u4Offset, "0x%04x",
					*pu4CoexInfo++);
					break;
				}
				case 0x0003:
				{
					u4Offset += snprintf(pcCommand + u4Offset,
					i4TotalLen - u4Offset, "0x%06x",
					*pu4CoexInfo++);
					break;
				}
				case 0x0004:
				{
					u4Offset += snprintf(pcCommand + u4Offset,
					i4TotalLen - u4Offset, "0x%08x",
					*pu4CoexInfo++);
					break;
				}
				default:
					break;
				}
				ucIdx++;
			}
			break;
		}
		/* Default Coex Cmd */
		default:
			break;
		}

	 /* Set Return i4BytesWritten Value */
	i4BytesWritten = (INT_32)u4Offset;
	}
	return i4BytesWritten;
}

static int priv_driver_set_fw_log(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4McuDest = 0;
	UINT_32 u4LogType = 0;
#if CFG_SUPPORT_FW_DBG_LEVEL_CTRL
	UINT_32 ucFwLogLevel = FW_DBG_LEVEL_DONT_SET;
#endif
	P_CMD_FW_LOG_2_HOST_CTRL_T prFwLog2HostCtrl = NULL;
	UINT_32 u4Ret = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RSN, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	DBGLOG(RSN, INFO, "MT6632 : priv_driver_set_fw_log\n");

	prFwLog2HostCtrl = (P_CMD_FW_LOG_2_HOST_CTRL_T)kalMemAlloc(sizeof(CMD_FW_LOG_2_HOST_CTRL_T), VIR_MEM_TYPE);
	if (!prFwLog2HostCtrl) {
		DBGLOG(REQ, ERROR, "allocate memory for prFwLog2HostCtrl failed\n");
		i4BytesWritten = -1;
		goto out;
	}

#if CFG_SUPPORT_FW_DBG_LEVEL_CTRL
	if ((i4Argc != 3) && (i4Argc != 4)) {
		DBGLOG(REQ, ERROR, "argc %i  must be 3 or 4\n", i4Argc);
		i4BytesWritten = -1;
		goto out;
	}
#else
	if (i4Argc != 3) {
		DBGLOG(REQ, ERROR, "argc %i is not equal to 3\n", i4Argc);
		i4BytesWritten = -1;
		goto out;
	}
#endif

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4McuDest);
	if (u4Ret)
		DBGLOG(REQ, LOUD, "parse u4McuDest error u4Ret=%d\n", u4Ret);

	u4Ret = kalkStrtou32(apcArgv[2], 0, &u4LogType);
	if (u4Ret)
		DBGLOG(REQ, LOUD, "parse u4LogType error u4Ret=%d\n", u4Ret);

#if CFG_SUPPORT_FW_DBG_LEVEL_CTRL
	if (i4Argc == 4) {
		u4Ret = kalkStrtou32(apcArgv[3], 0, &ucFwLogLevel);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucFwLogLevel error u4Ret=%d\n", u4Ret);
	}
	prFwLog2HostCtrl->ucFwLogLevel = (UINT_8)ucFwLogLevel;
#endif

	prFwLog2HostCtrl->ucMcuDest = (UINT_8)u4McuDest;
	prFwLog2HostCtrl->ucFwLog2HostCtrl = (UINT_8)u4LogType;

	if (prFwLog2HostCtrl->ucMcuDest == 0)
		prGlueInfo->prAdapter->rWifiVar.ucN9Log2HostCtrl = prFwLog2HostCtrl->ucFwLog2HostCtrl;
	else if (prFwLog2HostCtrl->ucMcuDest == 1)
		prGlueInfo->prAdapter->rWifiVar.ucCR4Log2HostCtrl = prFwLog2HostCtrl->ucFwLog2HostCtrl;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetFwLog2Host,
			   prFwLog2HostCtrl, sizeof(CMD_FW_LOG_2_HOST_CTRL_T), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, INFO, "%s: command result is %s (%d %d)\n", __func__, pcCommand, u4McuDest, u4LogType);
	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "send fw log to host cmd failed\n");
		i4BytesWritten = -1;
		goto out;
	}

out:
	if (prFwLog2HostCtrl)
		kalMemFree(prFwLog2HostCtrl, VIR_MEM_TYPE,
			sizeof(CMD_FW_LOG_2_HOST_CTRL_T));

	return i4BytesWritten;
}
#endif

static int priv_driver_get_mcr(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4Ret;
	INT_32 i4ArgNum = 2;
	CMD_ACCESS_REG rCmdAccessReg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rCmdAccessReg.u4Address));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse get_mcr error (Address) u4Ret=%d\n", u4Ret);

		/* rCmdAccessReg.u4Address = kalStrtoul(apcArgv[1], NULL, 0); */
		rCmdAccessReg.u4Data = 0;

		DBGLOG(REQ, LOUD, "address is %x\n", rCmdAccessReg.u4Address);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryMcrRead,
				   &rCmdAccessReg, sizeof(rCmdAccessReg), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "0x%08x", (unsigned int)rCmdAccessReg.u4Data);
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);
	}

	return i4BytesWritten;

}				/* priv_driver_get_mcr */

int priv_driver_set_mcr(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Ret;
	INT_32 i4ArgNum = 3;
	CMD_ACCESS_REG rCmdAccessReg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rCmdAccessReg.u4Address));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse get_mcr error (Address) u4Ret=%d\n", u4Ret);

		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rCmdAccessReg.u4Data));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse get_mcr error (Data) u4Ret=%d\n", u4Ret);

		/* rCmdAccessReg.u4Address = kalStrtoul(apcArgv[1], NULL, 0); */
		/* rCmdAccessReg.u4Data = kalStrtoul(apcArgv[2], NULL, 0); */

		rStatus = kalIoctl(prGlueInfo,
					wlanoidSetMcrWrite,
					&rCmdAccessReg, sizeof(rCmdAccessReg), FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	}

	return i4BytesWritten;

}

static int priv_driver_set_test_mode(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4Ret;
	INT_32 i4ArgNum = 2, u4MagicKey;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &(u4MagicKey));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse Magic Key error u4Ret=%d\n", u4Ret);

		DBGLOG(REQ, LOUD, "The Set Test Mode Magic Key is %d\n", u4MagicKey);

		if (u4MagicKey == PRIV_CMD_TEST_MAGIC_KEY) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidRftestSetTestMode,
					   NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);
		} else if (u4MagicKey == 0) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidRftestSetAbortTestMode,
					   NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);
		}

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}

	return i4BytesWritten;

}				/* priv_driver_set_test_mode */

static int priv_driver_set_test_cmd(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4Ret;
	INT_32 i4ArgNum = 3;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rRfATInfo.u4FuncIndex));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "Parse Test CMD Index error u4Ret=%d\n", u4Ret);

		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rRfATInfo.u4FuncData));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "Parse Test CMD Data error u4Ret=%d\n", u4Ret);

		DBGLOG(REQ, LOUD, "Set Test CMD FuncIndex = %d, FuncData = %d\n",
			rRfATInfo.u4FuncIndex, rRfATInfo.u4FuncData);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidRftestSetAutoTest,
				   &rRfATInfo, sizeof(rRfATInfo), FALSE, FALSE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}

	return i4BytesWritten;

}				/* priv_driver_set_test_cmd */

static int priv_driver_get_test_result(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4Ret;
	UINT_32 u4Data = 0;
	INT_32 i4ArgNum = 3;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rRfATInfo.u4FuncIndex));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "Parse Test CMD Index error u4Ret=%d\n", u4Ret);

		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rRfATInfo.u4FuncData));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "Parse Test CMD Data error u4Ret=%d\n", u4Ret);

		DBGLOG(REQ, LOUD, "Get Test CMD FuncIndex = %d, FuncData = %d\n",
			rRfATInfo.u4FuncIndex, rRfATInfo.u4FuncData);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidRftestQueryAutoTest,
				   &rRfATInfo, sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
		u4Data = (unsigned int)rRfATInfo.u4FuncData;
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%d[0x%08x]", u4Data, u4Data);
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);
	}

	return i4BytesWritten;

}				/* priv_driver_get_test_result */

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
INT_32 priv_driver_last_sec_mcs_info(IN P_ADAPTER_T prAdapter, IN char *pcCommand, IN int i4TotalLen,
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo, struct PARAM_TX_MCS_INFO *prTxMcsInfo)
{
	UINT_8 i, j, txmode, rate, stbc;
	UINT_8 nsts;
	INT_32 i4BytesWritten = 0;
	UINT_32 au4RxVect0Que[MCS_INFO_SAMPLE_CNT], au4RxVect1Que[MCS_INFO_SAMPLE_CNT];
	UINT_8 ucStaIdx = prAdapter->prAisBssInfo->prStaRecOfAP->ucIndex;

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten, "\nTx MCS:\n");

	for (i = 0; i < MCS_INFO_SAMPLE_CNT; i++) {
		UINT_8 tmpPerSum = 0, cnt = 0;
		UINT_16 tmpRateCode = 0xFFFF;

		if (prTxMcsInfo->au2TxRateCode[i] == 0xFFFF)
			continue;

		if (tmpRateCode == 0xFFFF)
			tmpRateCode = prTxMcsInfo->au2TxRateCode[i];

		txmode = HW_TX_RATE_TO_MODE(prTxMcsInfo->au2TxRateCode[i]);
		if (txmode >= MAX_TX_MODE)
			txmode = MAX_TX_MODE;
		rate = HW_TX_RATE_TO_MCS(prTxMcsInfo->au2TxRateCode[i], txmode);
		nsts = HW_TX_RATE_TO_NSS(prTxMcsInfo->au2TxRateCode[i]) + 1;
		stbc = HW_TX_RATE_TO_STBC(prTxMcsInfo->au2TxRateCode[i]);

		for (j = 0; j < MCS_INFO_SAMPLE_CNT; j++) {
			if (tmpRateCode == prTxMcsInfo->au2TxRateCode[j]) {
				tmpPerSum += prTxMcsInfo->aucTxRatePer[j];
				cnt++;
				prTxMcsInfo->au2TxRateCode[j] = 0xFFFF;
			}
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"	%s, ", HW_TX_RATE_CCK_STR[rate & 0x3]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"	%s, ", hw_rate_ofdm_str(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) || (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"	MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"	NSS%d_MCS%d, ", nsts, rate);

		if ((txmode == TX_RATE_MODE_CCK) || (txmode == TX_RATE_MODE_OFDM))
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", HW_TX_RATE_BW[0]);
		else
			if (i > prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN)
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					 i4TotalLen - i4BytesWritten, "%s, ",
					 prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability < 4 ?
						(prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability > BW_20 ?
						HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability - 1] :
						HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability]) :
						HW_TX_RATE_BW[4]);
			else
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					  i4TotalLen - i4BytesWritten, "%s, ",
					  prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability < 4 ?
						HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability] :
						HW_TX_RATE_BW[4]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", priv_driver_get_sgi_info(&prHwWlanInfo->rWtblPeerCap) == 0 ? "LGI" : "SGI");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s%s%s [PER: %02d%]\t", txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			stbc ? ", STBC, " : ", ",
			((priv_driver_get_ldpc_info(&prHwWlanInfo->rWtblTxConfig) == 0) ||
			(txmode == TX_RATE_MODE_CCK) || (txmode == TX_RATE_MODE_OFDM)) ? "BCC" : "LDPC",
			tmpPerSum/cnt);

		for (j = 0; j < cnt; j++)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten, "*");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten, "\n");
	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten, "\nRx MCS:\n");

	kalMemCopy(au4RxVect0Que, prAdapter->arStaRec[ucStaIdx].au4RxVect0Que, sizeof(au4RxVect0Que));
	kalMemCopy(au4RxVect1Que, prAdapter->arStaRec[ucStaIdx].au4RxVect1Que, sizeof(au4RxVect1Que));

	for (i = 0; i < MCS_INFO_SAMPLE_CNT; i++) {
		UINT_8 cnt = 0;
		UINT_32 u4RxVector0 = 0xFFFFFFFF;
		UINT_32 txmode, rate, frmode, sgi, nsts, ldpc, stbc, groupid, mu;
		#define RX_MCS_INFO_MASK BITS(0, 17)

		if (au4RxVect0Que[i] == 0xFFFFFFFF)
			continue;

		if (u4RxVector0 == 0xFFFFFFFF)
			u4RxVector0 = au4RxVect0Que[i];

		txmode = (au4RxVect0Que[i] & RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET;
		rate = (au4RxVect0Que[i] & RX_VT_RX_RATE_MASK) >> RX_VT_RX_RATE_OFFSET;
		frmode = (au4RxVect0Que[i] & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET;
		nsts = ((au4RxVect1Que[i] & RX_VT_NSTS_MASK) >> RX_VT_NSTS_OFFSET);
		stbc = (au4RxVect0Que[i] & RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET;
		sgi = au4RxVect0Que[i] & RX_VT_SHORT_GI;
		ldpc = au4RxVect0Que[i] & RX_VT_LDPC;
		groupid = (au4RxVect1Que[i] & RX_VT_GROUP_ID_MASK) >> RX_VT_GROUP_ID_OFFSET;

		for (j = 0; j < MCS_INFO_SAMPLE_CNT; j++) {
			if ((u4RxVector0 & RX_MCS_INFO_MASK) == (au4RxVect0Que[j] & RX_MCS_INFO_MASK)) {
				au4RxVect0Que[j] = 0xFFFFFFFF;
				cnt++;
			}
		}

		if (groupid && groupid != 63) {
			mu = 1;
		} else {
			mu = 0;
			nsts += 1;
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"	%s, ", rate < 4 ? HW_TX_RATE_CCK_STR[rate] : HW_TX_RATE_CCK_STR[4]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"	%s, ", hw_rate_ofdm_str(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) || (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"	MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"	NSS%d_MCS%d, ", nsts, rate);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s, ", frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s,", sgi == 0 ? "LGI" : "SGI");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", stbc == 0 ? " " : " STBC, ");

		if (mu) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, %s, %s (%d)\t", txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
				ldpc == 0 ? "BCC" : "LDPC", "MU", groupid);
		} else {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, %s\t", txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
				ldpc == 0 ? "BCC" : "LDPC");
		}

		for (j = 0; j < cnt; j++)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten, "*");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten, "\n");
	}

	return i4BytesWritten;
}
#endif

INT_32 priv_driver_tx_rate_info(IN char *pcCommand, IN int i4TotalLen, BOOLEAN fgDumpAll,
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo, P_PARAM_GET_STA_STATISTICS prQueryStaStatistics)
{
	UINT_8 i, txmode, rate, stbc;
	UINT_8 nsts;
	INT_32 i4BytesWritten = 0;

	for (i = 0; i < AUTO_RATE_NUM; i++) {

		txmode = HW_TX_RATE_TO_MODE(prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);
		if (txmode >= MAX_TX_MODE)
			txmode = MAX_TX_MODE;
		rate = HW_TX_RATE_TO_MCS(prHwWlanInfo->rWtblRateInfo.au2RateCode[i], txmode);
		nsts = HW_TX_RATE_TO_NSS(prHwWlanInfo->rWtblRateInfo.au2RateCode[i]) + 1;
		stbc = HW_TX_RATE_TO_STBC(prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);

		if (fgDumpAll) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"Rate index - %d    ", i);

			if (prHwWlanInfo->rWtblRateInfo.ucRateIdx == i) {
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%s", "--> ");
			} else {
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%s", "    ");
			}
		}

		if (!fgDumpAll) {
			if (prHwWlanInfo->rWtblRateInfo.ucRateIdx != i)
				continue;
			else
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s", "AR TX Rate", " = ");
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", HW_TX_RATE_CCK_STR[rate & 0x3]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", hw_rate_ofdm_str(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) || (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"NSS%d_MCS%d, ", nsts, rate);

		if (prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability < 4 ?
				HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability] : HW_TX_RATE_BW[4]);
		} else {
			if ((txmode == TX_RATE_MODE_CCK) || (txmode == TX_RATE_MODE_OFDM))
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%s, ", HW_TX_RATE_BW[0]);
			else
				if (i > prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN)
					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					  i4TotalLen - i4BytesWritten, "%s, ",
					  prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability < 4 ?
						(prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability > BW_20 ?
						HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability - 1] :
						HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability]) :
						HW_TX_RATE_BW[4]);
				else
					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					  i4TotalLen - i4BytesWritten, "%s, ",
					  prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability < 4 ?
						HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability] :
						HW_TX_RATE_BW[4]);
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", priv_driver_get_sgi_info(&prHwWlanInfo->rWtblPeerCap) == 0 ? "LGI" : "SGI");

		if (prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s%s%s\n", txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
				stbc ? ", STBC, " : ", ",
				priv_driver_get_ldpc_info(&prHwWlanInfo->rWtblTxConfig) == 0 ? "BCC" : "LDPC");
		} else if (prQueryStaStatistics->aucArRatePer[prQueryStaStatistics->aucRateEntryIndex[i]] == 0xFF) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s%s%s   (--)\n", txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
				stbc ? ", STBC, " : ", ",
				((priv_driver_get_ldpc_info(&prHwWlanInfo->rWtblTxConfig) == 0) ||
				(txmode == TX_RATE_MODE_CCK) || (txmode == TX_RATE_MODE_OFDM)) ? "BCC" : "LDPC");
		} else {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s%s%s   (%d)\n", txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
				stbc ? ", STBC, " : ", ",
				((priv_driver_get_ldpc_info(&prHwWlanInfo->rWtblTxConfig) == 0) ||
				(txmode == TX_RATE_MODE_CCK) || (txmode == TX_RATE_MODE_OFDM)) ? "BCC" : "LDPC",
				prQueryStaStatistics->aucArRatePer[prQueryStaStatistics->aucRateEntryIndex[i]]);
		}

		if (!fgDumpAll)
			break;
	}

	return i4BytesWritten;
}


INT_32 priv_driver_last_rx_rssi(P_ADAPTER_T prAdapter, IN char *pcCommand, IN int i4TotalLen,
	IN UINT_8 ucWlanIdx)
{
	INT_32 i4RSSI0 = 0, i4RSSI1 = 0, i4RSSI2 = 0, i4RSSI3;
	INT_32 i4BytesWritten = 0;
	UINT_32 u4RxVector3 = 0;
	UINT_8 ucStaIdx;

	if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIdx, &ucStaIdx) == WLAN_STATUS_SUCCESS) {
		u4RxVector3 = prAdapter->arStaRec[ucStaIdx].u4RxVector3;
		DBGLOG(REQ, LOUD, "****** RX Vector3 = 0x%08x ******\n", u4RxVector3);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s", "Last RX RSSI", " = NOT SUPPORT");
		return i4BytesWritten;
	}

	i4RSSI0 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI0_MASK) >> RX_VT_RCPI0_OFFSET);
	i4RSSI1 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI1_MASK) >> RX_VT_RCPI1_OFFSET);

	if (prAdapter->rWifiVar.ucNSS > 2) {
		i4RSSI2 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI2_MASK) >> RX_VT_RCPI2_OFFSET);
		i4RSSI3 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI3_MASK) >> RX_VT_RCPI3_OFFSET);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d %d %d %d\n", "Last RX Data RSSI", " = ", i4RSSI0, i4RSSI1, i4RSSI2, i4RSSI3);
	} else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d %d\n", "Last RX Data RSSI", " = ", i4RSSI0, i4RSSI1);

	return i4BytesWritten;
}


INT_32 priv_driver_rx_rate_info(P_ADAPTER_T prAdapter, IN char *pcCommand, IN int i4TotalLen,
	IN UINT_8 ucWlanIdx)
{
	UINT_32 txmode, rate, frmode, sgi, nsts, ldpc, stbc, groupid, mu;
	INT_32 i4BytesWritten = 0;
	UINT_32 u4RxVector0 = 0, u4RxVector1 = 0;
	UINT_8 ucStaIdx;

	if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIdx, &ucStaIdx) == WLAN_STATUS_SUCCESS) {
		u4RxVector0 = prAdapter->arStaRec[ucStaIdx].u4RxVector0;
		u4RxVector1 = prAdapter->arStaRec[ucStaIdx].u4RxVector1;
		DBGLOG(REQ, LOUD, "****** RX Vector0 = 0x%08x ******\n", u4RxVector0);
		DBGLOG(REQ, LOUD, "****** RX Vector1 = 0x%08x ******\n", u4RxVector1);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s", "Last RX Rate", " = NOT SUPPORT");
		return i4BytesWritten;
	}

	txmode = (u4RxVector0 & RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET;
	rate = (u4RxVector0 & RX_VT_RX_RATE_MASK) >> RX_VT_RX_RATE_OFFSET;
	frmode = (u4RxVector0 & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET;
	nsts = ((u4RxVector1 & RX_VT_NSTS_MASK) >> RX_VT_NSTS_OFFSET);
	stbc = (u4RxVector0 & RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET;
	sgi = u4RxVector0 & RX_VT_SHORT_GI;
	ldpc = u4RxVector0 & RX_VT_LDPC;
	groupid = (u4RxVector1 & RX_VT_GROUP_ID_MASK) >> RX_VT_GROUP_ID_OFFSET;

	if (groupid && groupid != 63) {
		mu = 1;
	} else {
		mu = 0;
		nsts += 1;
	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s", "Last RX Rate", " = ");

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s, ", rate < 4 ? HW_TX_RATE_CCK_STR[rate] : HW_TX_RATE_CCK_STR[4]);
	else if (txmode == TX_RATE_MODE_OFDM)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s, ", hw_rate_ofdm_str(rate));
	else if ((txmode == TX_RATE_MODE_HTMIX) || (txmode == TX_RATE_MODE_HTGF))
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"MCS%d, ", rate);
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"NSS%d_MCS%d, ", nsts, rate);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s, ", frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s, ", rate < 4 ? "LP" : "SP");
	else if (txmode == TX_RATE_MODE_OFDM)
		;
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s,", sgi == 0 ? "LGI" : "SGI");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", stbc == 0 ? " " : " STBC, ");

	if (mu) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s, %s, %s (%d)\n", txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			ldpc == 0 ? "BCC" : "LDPC", "MU", groupid);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s, %s\n", txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			ldpc == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}

INT_32 priv_driver_tx_vector_info(IN char *pcCommand, IN int i4TotalLen,
	IN P_TX_VECTOR_BBP_LATCH_T prTxV)
{
	UINT_8 rate, txmode, frmode, sgi, ldpc, nsts, stbc, txpwr;
	INT_32 i4BytesWritten = 0;

	rate = TX_VECTOR_GET_TX_RATE(prTxV);
	txmode = TX_VECTOR_GET_TX_MODE(prTxV);
	frmode = TX_VECTOR_GET_TX_FRMODE(prTxV);
	nsts = TX_VECTOR_GET_TX_NSTS(prTxV) + 1;
	sgi = TX_VECTOR_GET_TX_SGI(prTxV);
	ldpc = TX_VECTOR_GET_TX_LDPC(prTxV);
	stbc = TX_VECTOR_GET_TX_STBC(prTxV);
	txpwr = TX_VECTOR_GET_TX_PWR(prTxV);

	if (prTxV->u4TxVector1 == 0xFFFFFFFF) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "Last TX Rate", " = ", "N/A");
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s", "Last TX Rate", " = ");

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? HW_TX_RATE_CCK_STR[rate] : HW_TX_RATE_CCK_STR[4]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", hw_rate_ofdm_str(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) || (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"NSS%d_MCS%d, ", nsts, rate);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s, ", frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%s, ", sgi == 0 ? "LGI" : "SGI");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s%s%s\n", txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			stbc ? ", STBC, " : ", ", ldpc == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}

static INT_32 priv_driver_dump_stat_info(P_ADAPTER_T prAdapter, IN char *pcCommand, IN int i4TotalLen,
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo, P_PARAM_GET_STA_STATISTICS prQueryStaStatistics, BOOLEAN fgResetCnt,
	UINT_32 u4StatGroup)
{
	INT_32 i4BytesWritten = 0;
	PARAM_RSSI rRssi = 0;
	UINT_16 u2LinkSpeed;
	UINT_32 u4Per, u4RxPer[ENUM_BAND_NUM], u4AmpduPer[ENUM_BAND_NUM], u4InstantPer;
	UINT_8 ucDbdcIdx, ucStaIdx, ucNss;
	UINT_8 ucSkipAr;
	static UINT_32 u4TotalTxCnt, u4TotalFailCnt;
	static UINT_32 u4Rate1TxCnt, u4Rate1FailCnt;
	static UINT_32 au4RxMpduCnt[ENUM_BAND_NUM] = {0};
	static UINT_32 au4FcsError[ENUM_BAND_NUM] = {0};
	static UINT_32 au4RxFifoCnt[ENUM_BAND_NUM] = {0};
	static UINT_32 au4AmpduTxSfCnt[ENUM_BAND_NUM] = {0};
	static UINT_32 au4AmpduTxAckSfCnt[ENUM_BAND_NUM] = {0};
	P_RX_CTRL_T prRxCtrl;
	UINT_32 u4InstantRxPer[ENUM_BAND_NUM];
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_16 i2Wf0AvgPwr;
	INT_16 i2Wf1AvgPwr;
	UINT_32 u4BufLen = 0;

	ucSkipAr = prQueryStaStatistics->ucSkipAr;
	prRxCtrl = &prAdapter->rRxCtrl;
	ucNss = prAdapter->rWifiVar.ucNSS;

	if (ucSkipAr) {
		u4TotalTxCnt += prHwWlanInfo->rWtblTxCounter.u2CurBwTxCnt +
							prHwWlanInfo->rWtblTxCounter.u2OtherBwTxCnt;
		u4TotalFailCnt += prHwWlanInfo->rWtblTxCounter.u2CurBwFailCnt +
							prHwWlanInfo->rWtblTxCounter.u2OtherBwFailCnt;
		u4Rate1TxCnt += prHwWlanInfo->rWtblTxCounter.u2Rate1TxCnt;
		u4Rate1FailCnt += prHwWlanInfo->rWtblTxCounter.u2Rate1FailCnt;
	}

	if (ucSkipAr) {
		u4Per = (prHwWlanInfo->rWtblTxCounter.u2Rate1TxCnt == 0) ?
					(0) : (1000 * u4Rate1FailCnt / u4Rate1TxCnt);

		u4InstantPer = (prHwWlanInfo->rWtblTxCounter.u2Rate1TxCnt == 0) ?
					(0) : (1000 * (prHwWlanInfo->rWtblTxCounter.u2Rate1FailCnt) /
					(prHwWlanInfo->rWtblTxCounter.u2Rate1TxCnt));
	} else {
		u4Per = (prQueryStaStatistics->u4Rate1TxCnt == 0) ?
					(0) : (1000 * (prQueryStaStatistics->u4Rate1FailCnt) /
					(prQueryStaStatistics->u4Rate1TxCnt));

		u4InstantPer = (prQueryStaStatistics->ucPer == 0) ?
					(0) : (prQueryStaStatistics->ucPer);
	}

	for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
		au4RxMpduCnt[ucDbdcIdx] += prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxMpduCnt;
		au4FcsError[ucDbdcIdx] += prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError;
		au4RxFifoCnt[ucDbdcIdx] += prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxFifoFull;
		au4AmpduTxSfCnt[ucDbdcIdx] += prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4AmpduTxSfCnt;
		au4AmpduTxAckSfCnt[ucDbdcIdx] += prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4AmpduTxAckSfCnt;

		u4RxPer[ucDbdcIdx] = ((au4RxMpduCnt[ucDbdcIdx] + au4FcsError[ucDbdcIdx]) == 0) ?
							(0) : (1000 * au4FcsError[ucDbdcIdx] /
							(au4RxMpduCnt[ucDbdcIdx] + au4FcsError[ucDbdcIdx]));

		u4AmpduPer[ucDbdcIdx] = (au4AmpduTxSfCnt[ucDbdcIdx] == 0) ?
					(0) : (1000 * (au4AmpduTxSfCnt[ucDbdcIdx] - au4AmpduTxAckSfCnt[ucDbdcIdx]) /
					au4AmpduTxSfCnt[ucDbdcIdx]);

		u4InstantRxPer[ucDbdcIdx] = ((prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxMpduCnt +
					prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError) == 0) ?
					(0) : (1000 * prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError /
					(prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxMpduCnt +
					prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError));
	}

	/* get Beacon RSSI */
	rStatus = kalIoctl(prAdapter->prGlueInfo,
			   wlanoidQueryRssi, &rRssi, sizeof(rRssi), TRUE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "unable to retrieve rssi\n");

	u2LinkSpeed = (prQueryStaStatistics->u2LinkSpeed == 0) ? 0 : prQueryStaStatistics->u2LinkSpeed / 2;

	/* =========== Group 0x0001 =========== */
	if (u4StatGroup & 0x0001) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "\n----- STA Stat (Group 0x01) -----\n");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CurrTemperature", " = ",
			prQueryStaStatistics->ucTemperature);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Tx Total cnt", " = ", ucSkipAr ?
			(u4TotalTxCnt) : (prQueryStaStatistics->u4TransmitCount));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Tx Fail Cnt", " = ", ucSkipAr ?
			(u4TotalFailCnt) : (prQueryStaStatistics->u4TransmitFailCount));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Rate1 Tx Cnt", " = ", ucSkipAr ?
			(u4Rate1TxCnt) : (prQueryStaStatistics->u4Rate1TxCnt));

		if (ucSkipAr)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n", "Rate1 Fail Cnt", " = ",
				u4Rate1FailCnt, u4Per/10, u4Per%10, u4InstantPer/10, u4InstantPer%10);
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d%%\n", "Rate1 Fail Cnt", " = ",
				prQueryStaStatistics->u4Rate1FailCnt, u4Per/10, u4Per%10, u4InstantPer);

		if ((ucSkipAr) && (fgResetCnt)) {
			u4TotalTxCnt = 0;
			u4TotalFailCnt = 0;
			u4Rate1TxCnt = 0;
			u4Rate1FailCnt = 0;
		}
	}

	/* =========== Group 0x0002 =========== */
	if (u4StatGroup & 0x0002) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "----- MIB Info (Group 0x02) -----\n");

		if (!prAdapter->rWifiVar.fgDbDcModeEn) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "RX Success", " = ", au4RxMpduCnt[ENUM_BAND_0]);

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n", "RX with CRC", " = ",
				au4FcsError[ENUM_BAND_0], u4RxPer[ENUM_BAND_0]/10, u4RxPer[ENUM_BAND_0]%10,
				u4InstantRxPer[ENUM_BAND_0]/10, u4InstantRxPer[ENUM_BAND_0]%10);

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "RX drop FIFO full", " = ", au4RxFifoCnt[ENUM_BAND_0]);
		} else {
			for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"[DBDC_%d] :\n", ucDbdcIdx);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "RX Success", " = ", au4RxMpduCnt[ucDbdcIdx]);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n", "RX with CRC", " = ",
					au4FcsError[ucDbdcIdx], u4RxPer[ucDbdcIdx]/10, u4RxPer[ucDbdcIdx]%10,
					u4InstantRxPer[ucDbdcIdx]/10, u4InstantRxPer[ucDbdcIdx]%10);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "RX drop FIFO full", " = ", au4RxFifoCnt[ucDbdcIdx]);
#if 0
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "AMPDU Tx success", " = ", au4AmpduTxSfCnt[ucDbdcIdx]);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d, PER = %d.%1d%%\n", "AMPDU Tx fail count", " = ",
					(au4AmpduTxSfCnt[ucDbdcIdx] - au4AmpduTxAckSfCnt[ucDbdcIdx]),
					u4AmpduPer[ucDbdcIdx]/10, u4AmpduPer[ucDbdcIdx]%10);
#endif
			}
		}

		if (fgResetCnt) {
			kalMemZero(au4RxMpduCnt, sizeof(au4RxMpduCnt));
			kalMemZero(au4FcsError, sizeof(au4RxMpduCnt));
			kalMemZero(au4RxFifoCnt, sizeof(au4RxMpduCnt));
			kalMemZero(au4AmpduTxSfCnt, sizeof(au4RxMpduCnt));
			kalMemZero(au4AmpduTxAckSfCnt, sizeof(au4RxMpduCnt));
		}
	}

	/* =========== Group 0x0004 =========== */
	if (u4StatGroup & 0x0004) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "----- Last Rx Info (Group 0x04) -----\n");

		rSwCtrlInfo.u4Data = 0;
		rSwCtrlInfo.u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + 1;

		rStatus = kalIoctl(prAdapter->prGlueInfo,
				   wlanoidQuerySwCtrlRead,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u, rSwCtrlInfo.u4Data 0x%x\n", rStatus, rSwCtrlInfo.u4Data);
		if (rStatus == WLAN_STATUS_SUCCESS) {
			i2Wf0AvgPwr = rSwCtrlInfo.u4Data & 0xFFFF;
			i2Wf1AvgPwr = (rSwCtrlInfo.u4Data >> 16) & 0xFFFF;

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
						"%-20s%s%d %d\n", "NOISE", " = ", i2Wf0AvgPwr, i2Wf1AvgPwr);
		}

		/* Last RX Rate */
		i4BytesWritten += priv_driver_rx_rate_info(prAdapter, pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, (UINT_8)(prHwWlanInfo->u4Index));

		/* Last RX RSSI */
		i4BytesWritten += priv_driver_last_rx_rssi(prAdapter, pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, (UINT_8)(prHwWlanInfo->u4Index));

		/* Last RX Resp RSSI */
		if (ucNss > 2)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%d %d %d %d\n", "Tx Response RSSI", " = ",
				RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi0),
				RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi1),
				RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi2),
				RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi3));
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%d %d\n", "Tx Response RSSI", " = ",
				RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi0),
				RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi1));

		/* Last Beacon RSSI */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "Beacon RSSI", " = ", rRssi);
	}

	/* =========== Group 0x0008 =========== */
	if (u4StatGroup & 0x0008) {
		/* TxV */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "----- Last TX Info (Group 0x08) -----\n");

		if (!prAdapter->rWifiVar.fgDbDcModeEn) {
			i4BytesWritten += priv_driver_tx_vector_info(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, &prQueryStaStatistics->rTxVector[ENUM_BAND_0]);

			if (prQueryStaStatistics->rTxVector[ENUM_BAND_0].u4TxVector1 == 0xFFFFFFFF)
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Chip Out TX Power", " = ", "N/A");
			else
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%ld.%1ld dBm\n", "Chip Out TX Power", " = ",
					TX_VECTOR_GET_TX_PWR(&prQueryStaStatistics->rTxVector[ENUM_BAND_0]) >> 1,
					5 * (TX_VECTOR_GET_TX_PWR(&prQueryStaStatistics->rTxVector[ENUM_BAND_0]) % 2));
		} else {
			for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"[DBDC_%d] :\n", ucDbdcIdx);

				i4BytesWritten += priv_driver_tx_vector_info(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, &prQueryStaStatistics->rTxVector[ucDbdcIdx]);

				if (prQueryStaStatistics->rTxVector[ucDbdcIdx].u4TxVector1 == 0xFFFFFFFF)
					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%-20s%s%s\n", "Chip Out TX Power", " = ", "N/A");
				else
					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%-20s%s%ld.%1ld dBm\n", "ChipOut TX Power", " = ",
					TX_VECTOR_GET_TX_PWR(&prQueryStaStatistics->rTxVector[ucDbdcIdx]) >> 1,
					5 * (TX_VECTOR_GET_TX_PWR(&prQueryStaStatistics->rTxVector[ucDbdcIdx]) % 2));
#if 0
				if (prQueryStaStatistics->rTxVector[ucDbdcIdx].u4TxVector2 == 0xFFFFFFFF)
					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%-20s%s%s\n", "Beamform Enable", " = ", "N/A");
				else
					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%-20s%s%d\n", "Beamform Enable", " = ",
					TX_VECTOR_GET_BF_EN(&prQueryStaStatistics->rTxVector[ucDbdcIdx]) ? 1 : 0);

				if (prQueryStaStatistics->rTxVector[ucDbdcIdx].u4TxVector4 == 0xFFFFFFFF) {
					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					"%-20s%s%s\n", "Dynamic BW", " = ", "N/A");

					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%-20s%s%s\n", "Sounding Pkt", " = ", "N/A");
				} else {
					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%-20s%s%d\n", "Dynamic BW", " = ",
					TX_VECTOR_GET_DYN_BW(&prQueryStaStatistics->rTxVector[ucDbdcIdx]) ? 1 : 0);

					i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%-20s%s%d\n", "Sounding Pkt", " = ",
					TX_VECTOR_GET_NO_SOUNDING(&prQueryStaStatistics->rTxVector[ucDbdcIdx]) ? 1 : 0);
				}
#endif
			}
		}
	}

	/* =========== Group 0x0010 =========== */
	if (u4StatGroup & 0x0010) {
		/* RX Reorder */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "------ RX Reorder (Group 0x10) -----\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder miss", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_MISS_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder within", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_WITHIN_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder ahead", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_AHEAD_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder behind", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_BEHIND_COUNT));
	}

	/* =========== Group 0x0020 =========== */
	if (u4StatGroup & 0x0020) {
		/* AR info */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "------ AR Info (Group 0x20) -----\n");

		/* Last TX Rate */
		i4BytesWritten += priv_driver_tx_rate_info(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, FALSE, prHwWlanInfo, prQueryStaStatistics);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "LinkSpeed", " = ", u2LinkSpeed);

		if (!prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "RateTable", " = ", prQueryStaStatistics->ucArTableIdx < RATE_TBL_MAX ?
				RATE_TBLE[prQueryStaStatistics->ucArTableIdx] : RATE_TBLE[RATE_TBL_MAX]);

			if (wlanGetStaIdxByWlanIdx(prAdapter, (UINT_8)(prHwWlanInfo->u4Index), &ucStaIdx) ==
				WLAN_STATUS_SUCCESS){
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "2G Support 256QAM TX", " = ",
					(prAdapter->arStaRec[ucStaIdx].u4Flags & MTK_SYNERGY_CAP_SUPPORT_24G_MCS89) ?
					1 : 0);
			}

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%d%%\n", "Rate1 instantPer", " = ", u4InstantPer);

			if (prQueryStaStatistics->ucAvePer == 0xFF) {
#if 0
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "average RSSI", " = ", "N/A");

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Rate1 AvePer", " = ", "N/A");
#endif
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Train Down", " = ", "N/A");

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Train Up", " = ", "N/A");
#if 0
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Highest Rate Cnt", " = ", "N/A");

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Lowest Rate Cnt", " = ", "N/A");

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "SGI Pass Cnt", " = ", "N/A");

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "AR State Prev", " = ", "N/A");

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "AR State Curr", " = ", "N/A");

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "AR Action Type", " = ", "N/A");

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Rate Entry Idx", " = ", "N/A");
#endif
			} else {
#if 0
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "average RSSI", " = ", rRssi);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d%%\n", "Rate1 AvePer", " = ", prQueryStaStatistics->ucAvePer);
#endif
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d -> %d\n", "Train Down", " = ",
					(UINT_16)(prQueryStaStatistics
					->u2TrainDown & BITS(0, 7)),
					(UINT_16)((prQueryStaStatistics
					->u2TrainDown >> 8) & BITS(0, 7)));

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d -> %d\n", "Train Up", " = ",
					(UINT_16)(prQueryStaStatistics
					->u2TrainUp & BITS(0, 7)),
					(UINT_16)((prQueryStaStatistics
					->u2TrainUp >> 8) & BITS(0, 7)));
#if 0
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "Highest Rate Cnt", " = ",
					prQueryStaStatistics->ucHighestRateCnt);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "Lowest Rate Cnt", " = ", prQueryStaStatistics->ucLowestRateCnt);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "SGI Pass Cnt", " = ",
					prQueryStaStatistics->ucTxSgiDetectPassCnt);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "AR State Prev", " = ",
					prQueryStaStatistics->ucArStatePrev < 3 ?
					AR_STATE[prQueryStaStatistics->ucArStatePrev] : AR_STATE[3]);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "AR State Curr", " = ",
					prQueryStaStatistics->ucArStateCurr < 3 ?
					AR_STATE[prQueryStaStatistics->ucArStateCurr] : AR_STATE[3]);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "AR Action Type", " = ",
					prQueryStaStatistics->ucArActionType < 16 ?
					AR_ACTION[prQueryStaStatistics->ucArActionType] : AR_ACTION[16]);

				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d -> %d\n", "Rate Entry Idx", " = ",
					(prQueryStaStatistics->ucRateEntryIdxPrev == AR_RATE_ENTRY_INDEX_NULL) ?
					(prQueryStaStatistics->ucRateEntryIdx) :
					(prQueryStaStatistics->ucRateEntryIdxPrev),
					prQueryStaStatistics->ucRateEntryIdx);
#endif
			}

			if (prQueryStaStatistics->fgIsForceTxStream == 0)
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Force Tx Stream", " = ", "N/A");
			else
				i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "Force Tx Stream", " = ",
					prQueryStaStatistics->fgIsForceTxStream);

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "Force SE off", " = ", prQueryStaStatistics->fgIsForceSeOff);
		}

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CBRN", " = ", prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN);

		/* Rate1~Rate8 */
		i4BytesWritten += priv_driver_tx_rate_info(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, TRUE, prHwWlanInfo, prQueryStaStatistics);
	}

	/* =========== Group 0x0040 =========== */
	if (u4StatGroup & 0x0040) {
		/* Tx Agg */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", "------ TX AGG (Group 0x40) -----\n");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-12s%s", "Range:", "1     2~5     6~15    16~22    23~33    34~49    50~57    58~64\n");

		for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"DBDC%d:", ucDbdcIdx);
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%7d%8d%9d%9d%9d%9d%9d%9d\n",
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].u2TxRange1AmpduCnt,
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].u2TxRange2AmpduCnt,
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].u2TxRange3AmpduCnt,
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].u2TxRange4AmpduCnt,
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].u2TxRange5AmpduCnt,
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].u2TxRange6AmpduCnt,
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].u2TxRange7AmpduCnt,
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].u2TxRange8AmpduCnt);
		}
	}

	return i4BytesWritten;
}

static int priv_driver_get_sta_stat(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0, u4Ret, u4StatGroup = 0xFFFFFFFF;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_8 aucMacAddr[MAC_ADDR_LEN];
	UINT_8 ucWlanIndex;
	PUINT_8 pucMacAddr = NULL;
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo = NULL;
	P_PARAM_GET_STA_STATISTICS prQueryStaStatistics = NULL;
	BOOLEAN fgResetCnt = FALSE;
	BOOLEAN fgRxCCSel = FALSE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 3) {
		if (strnicmp(apcArgv[1], CMD_STAT_GROUP_SEL, strlen(CMD_STAT_GROUP_SEL)) == 0) {
			u4Ret = kalkStrtou32(apcArgv[2], 0, &(u4StatGroup));
			if (u4Ret)
				DBGLOG(REQ, LOUD, "parse get_mcr error (Address) u4Ret=%d\n", u4Ret);
			if (u4StatGroup == 0)
				u4StatGroup = 0xFFFFFFFF;

			if (prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP) {
				ucWlanIndex = prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP->ucWlanIndex;
			} else if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter, NULL, &ucWlanIndex)) {
				DBGLOG(REQ, INFO, "Can't find the wlan index of MAC addr %pM!\n", aucMacAddr);
				goto out;
			}
		} else {
			if (strnicmp(apcArgv[1], CMD_STAT_RESET_CNT, strlen(CMD_STAT_RESET_CNT)) == 0) {
				wlanHwAddrToBin(apcArgv[2], &aucMacAddr[0]);
				fgResetCnt = TRUE;
			} else if (strnicmp(apcArgv[2], CMD_STAT_RESET_CNT, strlen(CMD_STAT_RESET_CNT)) == 0) {
				wlanHwAddrToBin(apcArgv[1], &aucMacAddr[0]);
				fgResetCnt = TRUE;
			} else {
				wlanHwAddrToBin(apcArgv[1], &aucMacAddr[0]);
				fgResetCnt = FALSE;
			}

			if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter, &aucMacAddr[0], &ucWlanIndex)) {
				DBGLOG(REQ, INFO, "Can't find the wlan index of MAC addr %pM!\n", aucMacAddr);
				goto out;
			}
		}

	} else {
		/* Get AIS AP address for no argument */
		if (prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP) {
			ucWlanIndex = prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP->ucWlanIndex;
		} else if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter, NULL, &ucWlanIndex)) {
			DBGLOG(REQ, INFO, "No connected peer found!\n");
			goto out;
		}

		if (i4Argc == 2) {
			if (strnicmp(apcArgv[1], CMD_STAT_RESET_CNT, strlen(CMD_STAT_RESET_CNT)) == 0)
				fgResetCnt = TRUE;
			else if (strnicmp(apcArgv[1], CMD_STAT_NOISE_SEL, strlen(CMD_STAT_NOISE_SEL)) == 0)
				fgRxCCSel = TRUE;
		}
	}

	prHwWlanInfo = (P_PARAM_HW_WLAN_INFO_T)kalMemAlloc(sizeof(PARAM_HW_WLAN_INFO_T), VIR_MEM_TYPE);
	if (!prHwWlanInfo) {
		DBGLOG(REQ, ERROR, "Allocate memory for prHwWlanInfo failed!\n");
		i4BytesWritten = -1;
		goto out;
	}

	prHwWlanInfo->u4Index = ucWlanIndex;
	if (fgRxCCSel == TRUE)
		prHwWlanInfo->rWtblRxCounter.fgRxCCSel = TRUE;
	else
		prHwWlanInfo->rWtblRxCounter.fgRxCCSel = FALSE;

	DBGLOG(REQ, INFO, "MT6632 : index = %d i4TotalLen = %d\n", prHwWlanInfo->u4Index, i4TotalLen);

	/* Get WTBL info */
	rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryWlanInfo,
					prHwWlanInfo, sizeof(PARAM_HW_WLAN_INFO_T), TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Query prHwWlanInfo failed!\n");
		i4BytesWritten = -1;
		goto out;
	}

	/* Get Statistics info */
	prQueryStaStatistics =
		(P_PARAM_GET_STA_STATISTICS)kalMemAlloc(sizeof(PARAM_GET_STA_STA_STATISTICS), VIR_MEM_TYPE);
	if (!prQueryStaStatistics) {
		DBGLOG(REQ, ERROR, "Allocate memory for prQueryStaStatistics failed!\n");
		i4BytesWritten = -1;
		goto out;
	}

	prQueryStaStatistics->ucResetCounter = fgResetCnt;

	pucMacAddr = wlanGetStaAddrByWlanIdx(prGlueInfo->prAdapter, ucWlanIndex);

	if (!pucMacAddr) {
		DBGLOG(REQ, ERROR, "Couldn't find the MAC addr of WlanIndex %d!\n",
			ucWlanIndex);
		i4BytesWritten = -1;
		goto out;
	}

	COPY_MAC_ADDR(prQueryStaStatistics->aucMacAddr, pucMacAddr);

	rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryStaStatistics,
					prQueryStaStatistics,
					sizeof(PARAM_GET_STA_STA_STATISTICS), TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Query prQueryStaStatistics failed!\n");
		i4BytesWritten = -1;
		goto out;
	}

	if (pucMacAddr) {
		i4BytesWritten = priv_driver_dump_stat_info(prAdapter, pcCommand, i4TotalLen,
			prHwWlanInfo, prQueryStaStatistics, fgResetCnt, u4StatGroup);
	}
	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

out:
	if (prHwWlanInfo)
		kalMemFree(prHwWlanInfo, VIR_MEM_TYPE, sizeof(PARAM_HW_WLAN_INFO_T));

	if (prQueryStaStatistics)
		kalMemFree(prQueryStaStatistics, VIR_MEM_TYPE, sizeof(PARAM_GET_STA_STA_STATISTICS));

	if (fgResetCnt)
		nicRxClearStatistics(prGlueInfo->prAdapter);

	return i4BytesWritten;
}

static INT_32 priv_driver_dump_stat2_info(P_ADAPTER_T prAdapter, IN char *pcCommand, IN int i4TotalLen,
	P_UMAC_STAT2_GET_T prUmacStat2GetInfo, P_PARAM_GET_DRV_STATISTICS prQueryDrvStatistics)
{
	INT_32 i4BytesWritten = 0;
	UINT_16 u2PleTotalRevPage = 0;
	UINT_16 u2PleTotalSrcPage = 0;
	UINT_16 u2PseTotalRevPage = 0;
	UINT_16 u2PseTotalSrcPage = 0;

	u2PleTotalRevPage = prUmacStat2GetInfo->u2PleRevPgHif0Group0 +
		prUmacStat2GetInfo->u2PleRevPgCpuGroup2;

	u2PleTotalSrcPage = prUmacStat2GetInfo->u2PleSrvPgHif0Group0 +
		prUmacStat2GetInfo->u2PleSrvPgCpuGroup2;

	u2PseTotalRevPage = prUmacStat2GetInfo->u2PseRevPgHif0Group0 +
		prUmacStat2GetInfo->u2PseRevPgHif1Group1 +
		prUmacStat2GetInfo->u2PseRevPgCpuGroup2 +
		prUmacStat2GetInfo->u2PseRevPgLmac0Group3 +
		prUmacStat2GetInfo->u2PseRevPgLmac1Group4 +
		prUmacStat2GetInfo->u2PseRevPgLmac2Group5 +
		prUmacStat2GetInfo->u2PseRevPgPleGroup6;

	u2PseTotalSrcPage = prUmacStat2GetInfo->u2PseSrvPgHif0Group0 +
		prUmacStat2GetInfo->u2PseSrvPgHif1Group1 +
		prUmacStat2GetInfo->u2PseSrvPgCpuGroup2 +
		prUmacStat2GetInfo->u2PseSrvPgLmac0Group3 +
		prUmacStat2GetInfo->u2PseSrvPgLmac1Group4 +
		prUmacStat2GetInfo->u2PseSrvPgLmac2Group5 +
		prUmacStat2GetInfo->u2PseSrvPgPleGroup6;

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\n----- Stat2 Info -----\n");


	/* Rev Page number Info. */
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\n----- PLE Reservation Page Info. -----\n");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Ple Hif0 Group0 RevPage", " = ",
		prUmacStat2GetInfo->u2PleRevPgHif0Group0);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Ple Cpu Group2 RevPage", " = ",
		prUmacStat2GetInfo->u2PleRevPgCpuGroup2);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Ple Total RevPage", " = ",
		u2PleTotalRevPage);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\n----- PLE Source Page Info. ----------\n");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Ple Hif0 Group0 SrcPage", " = ",
		prUmacStat2GetInfo->u2PleSrvPgHif0Group0);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Ple Cpu Group2 SrcPage", " = ",
		prUmacStat2GetInfo->u2PleSrvPgCpuGroup2);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Ple Total SrcPage", " = ",
		u2PleTotalSrcPage);

	/* umac MISC Info. */
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\n----- PLE Misc Info. -----------------\n");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "ple Total Page Number", " = ",
		prUmacStat2GetInfo->u2PleTotalPageNum);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "ple Free Page Number", " = ",
		prUmacStat2GetInfo->u2PleFreePageNum);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "ple FFA Page Number", " = ",
		prUmacStat2GetInfo->u2PleFfaNum);

	/* PSE Info. */
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\n----- PSE Reservation Page Info. -----\n");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Hif0 Group0 RevPage", " = ",
		prUmacStat2GetInfo->u2PseRevPgHif0Group0);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Hif1 Group1 RevPage", " = ",
		prUmacStat2GetInfo->u2PseRevPgHif1Group1);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Cpu Group2 RevPage", " = ",
		prUmacStat2GetInfo->u2PseRevPgCpuGroup2);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Lmac0 Group3 RevPage", " = ",
		prUmacStat2GetInfo->u2PseRevPgLmac0Group3);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Lmac1 Group4 RevPage", " = ",
		prUmacStat2GetInfo->u2PseRevPgLmac1Group4);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Lmac2 Group5 RevPage", " = ",
		prUmacStat2GetInfo->u2PseRevPgLmac2Group5);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Ple Group6 RevPage", " = ",
		prUmacStat2GetInfo->u2PseRevPgPleGroup6);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Total RevPage", " = ",
		u2PseTotalRevPage);

	/* PSE Info. */
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\n----- PSE Source Page Info. ----------\n");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Hif0 Group0 SrcPage", " = ",
		prUmacStat2GetInfo->u2PseSrvPgHif0Group0);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Hif1 Group1 SrcPage", " = ",
		prUmacStat2GetInfo->u2PseSrvPgHif1Group1);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Cpu Group2 SrcPage", " = ",
		prUmacStat2GetInfo->u2PseSrvPgCpuGroup2);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Lmac0 Group3 SrcPage", " = ",
		prUmacStat2GetInfo->u2PseSrvPgLmac0Group3);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Lmac1 Group4 SrcPage", " = ",
		prUmacStat2GetInfo->u2PseSrvPgLmac1Group4);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Lmac2 Group5 SrcPage", " = ",
		prUmacStat2GetInfo->u2PseSrvPgLmac2Group5);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Ple Group6 SrcPage", " = ",
		prUmacStat2GetInfo->u2PseSrvPgPleGroup6);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "Pse Total SrcPage", " = ",
		u2PseTotalSrcPage);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\n----- PSE Misc Info. -----------------\n");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "pse Total Page Number", " = ",
		prUmacStat2GetInfo->u2PseTotalPageNum);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "pse Free Page Number", " = ",
		prUmacStat2GetInfo->u2PseFreePageNum);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-26s%s%d\n", "pse FFA Page Number", " = ",
		prUmacStat2GetInfo->u2PseFfaNum);


	/* driver info */
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\n\n----- DRV Stat -----------------------\n\n");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-26s%s%d\n", "Pending Data", " = ",
				prQueryDrvStatistics->i4TxPendingFrameNum);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-26s%s%d\n", "Pending Sec", " = ",
				prQueryDrvStatistics->i4TxPendingSecurityFrameNum);
#if 0
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-26s%s%s\n", "Tx Pending Cmd Number", " = ",
				prQueryDrvStatistics->i4TxPendingCmdNum);
#endif

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-26s%s%d\n", "Tx Pending For-pkt Number", " = ",
				prQueryDrvStatistics->i4PendingFwdFrameCount);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-26s%s%d\n", "MsduInfo Available Number", " = ",
				prQueryDrvStatistics->u4MsduNumElem);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-26s%s%d\n", "MgmtTxRing Pending Number", " = ",
				prQueryDrvStatistics->u4TxMgmtTxringQueueNumElem);

	/* Driver Rx Info. */
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-26s%s%d\n", "Rx Free Sw Rfb Number", " = ",
				prQueryDrvStatistics->u4RxFreeSwRfbMsduNumElem);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-26s%s%d\n", "Rx Received Sw Rfb Number", " = ",
				prQueryDrvStatistics->u4RxReceivedRfbNumElem);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-26s%s%d\n", "Rx Indicated Sw Rfb Number", " = ",
				prQueryDrvStatistics->u4RxIndicatedNumElem);

	return i4BytesWritten;
}

static int priv_driver_get_sta_stat2(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	INT_32 i4BytesWritten = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4ArgNum = 1;
	P_UMAC_STAT2_GET_T prUmacStat2GetInfo = NULL;
	P_PARAM_GET_DRV_STATISTICS prQueryDrvStatistics = NULL;
	P_QUE_T prQueList, prTxMgmtTxRingQueList;
	P_RX_CTRL_T prRxCtrl;

	KAL_SPIN_LOCK_DECLARATION();

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc < i4ArgNum)
		return -1;

	prQueList = &prAdapter->rTxCtrl.rFreeMsduInfoList;

	prTxMgmtTxRingQueList = &prAdapter->rTxCtrl.rTxMgmtTxingQueue;

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	/* to do for UMAC Dump */
	prUmacStat2GetInfo = (P_UMAC_STAT2_GET_T)kalMemAlloc(sizeof(UMAC_STAT2_GET_T), VIR_MEM_TYPE);
	if (!prUmacStat2GetInfo) {
		DBGLOG(REQ, ERROR, "allocate memory for prUmacStat2GetInfo failed\n");
		i4BytesWritten = -1;
		goto out;
	}

	halUmacInfoGetMiscStatus(prAdapter, prUmacStat2GetInfo);


	/* Get Driver stat info */
	prQueryDrvStatistics =
		(P_PARAM_GET_DRV_STATISTICS)kalMemAlloc(sizeof(PARAM_GET_DRV_STATISTICS), VIR_MEM_TYPE);
	if (!prQueryDrvStatistics) {
		DBGLOG(REQ, ERROR, "allocate memory for prQueryDrvStatistics failed\n");
		i4BytesWritten = -1;
		goto out;
	}

	prQueryDrvStatistics->i4TxPendingFrameNum =
		(UINT_32) GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum);
	prQueryDrvStatistics->i4TxPendingSecurityFrameNum =
		(UINT_32) GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum);
#if 0
	prQueryDrvStatistics->i4TxPendingCmdNum =
		(UINT_32) GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingCmdNum);
#endif

	prQueryDrvStatistics->i4PendingFwdFrameCount = prAdapter->rTxCtrl.i4PendingFwdFrameCount;

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
	prQueryDrvStatistics->u4MsduNumElem = prQueList->u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
	prQueryDrvStatistics->u4TxMgmtTxringQueueNumElem = prTxMgmtTxRingQueList->u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);


	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	prQueryDrvStatistics->u4RxFreeSwRfbMsduNumElem = prRxCtrl->rFreeSwRfbList.u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
	prQueryDrvStatistics->u4RxReceivedRfbNumElem = prRxCtrl->rReceivedRfbList.u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	prQueryDrvStatistics->u4RxIndicatedNumElem = prRxCtrl->rIndicatedRfbList.u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);


	i4BytesWritten = priv_driver_dump_stat2_info(prAdapter, pcCommand, i4TotalLen,
		prUmacStat2GetInfo, prQueryDrvStatistics);

out:
	if (prUmacStat2GetInfo)
		kalMemFree(prUmacStat2GetInfo, VIR_MEM_TYPE, sizeof(UMAC_STAT2_GET_T));
	if (prQueryDrvStatistics)
		kalMemFree(prQueryDrvStatistics, VIR_MEM_TYPE, sizeof(PARAM_GET_DRV_STATISTICS));

	return i4BytesWritten;
}


static INT_32 priv_driver_dump_rx_stat_info(P_ADAPTER_T prAdapter, IN char *pcCommand, IN int i4TotalLen,
	IN BOOLEAN fgResetCnt)
{
	INT_32 i4BytesWritten = 0;
	UINT_32 u4RxVector0 = 0, u4RxVector2 = 0, u4RxVector3 = 0, u4RxVector4 = 0;
	UINT_8 ucStaIdx, ucWlanIndex, cbw;
	BOOLEAN fgWlanIdxFound = TRUE, fgSkipRxV = FALSE;
	UINT_32 u4FAGCRssiWBR0, u4FAGCRssiIBR0;
	UINT_32 u4Value, u4Foe, foe_const;
	static UINT_32 au4MacMdrdy[ENUM_BAND_NUM] = {0};
	static UINT_32 au4FcsError[ENUM_BAND_NUM] = {0};
	static UINT_32 au4OutOfResource[ENUM_BAND_NUM] = {0};
	static UINT_32 au4LengthMismatch[ENUM_BAND_NUM] = {0};

	au4MacMdrdy[ENUM_BAND_0] += htonl(g_HqaRxStat.MAC_Mdrdy);
	au4MacMdrdy[ENUM_BAND_1] += htonl(g_HqaRxStat.MAC_Mdrdy1);
	au4FcsError[ENUM_BAND_0] += htonl(g_HqaRxStat.MAC_FCS_Err);
	au4FcsError[ENUM_BAND_1] += htonl(g_HqaRxStat.MAC_FCS_Err1);
	au4OutOfResource[ENUM_BAND_0] += htonl(g_HqaRxStat.OutOfResource);
	au4OutOfResource[ENUM_BAND_1] += htonl(g_HqaRxStat.OutOfResource1);
	au4LengthMismatch[ENUM_BAND_0] += htonl(g_HqaRxStat.LengthMismatchCount_B0);
	au4LengthMismatch[ENUM_BAND_1] += htonl(g_HqaRxStat.LengthMismatchCount_B1);

	if (fgResetCnt) {
		kalMemZero(au4MacMdrdy, sizeof(au4MacMdrdy));
		kalMemZero(au4FcsError, sizeof(au4FcsError));
		kalMemZero(au4OutOfResource, sizeof(au4OutOfResource));
		kalMemZero(au4LengthMismatch, sizeof(au4LengthMismatch));
	}

	if (prAdapter->prAisBssInfo->prStaRecOfAP)
		ucWlanIndex = prAdapter->prAisBssInfo->prStaRecOfAP->ucWlanIndex;
	else if (!wlanGetWlanIdxByAddress(prAdapter, NULL, &ucWlanIndex))
		fgWlanIdxFound = FALSE;

	if (fgWlanIdxFound) {
		if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIndex, &ucStaIdx) == WLAN_STATUS_SUCCESS) {
			u4RxVector0 = prAdapter->arStaRec[ucStaIdx].u4RxVector0;
			u4RxVector2 = prAdapter->arStaRec[ucStaIdx].u4RxVector2;
			u4RxVector3 = prAdapter->arStaRec[ucStaIdx].u4RxVector3;
			u4RxVector4 = prAdapter->arStaRec[ucStaIdx].u4RxVector4;
		} else{
			fgSkipRxV = TRUE;
		}
	} else{
		fgSkipRxV = TRUE;
	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%s", "\n\nRX Stat:\n");
#if 0
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "PER0", " = ",
		g_HqaRxStat.PER0);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "PER1", " = ",
		g_HqaRxStat.PER1);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "RX OK0", " = ",
		g_HqaRxStat.RXOK0);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "RX OK1", " = ",
		g_HqaRxStat.RXOK1);
#endif
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "MAC Mdrdy0", " = ",
		au4MacMdrdy[ENUM_BAND_0]);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "MAC Mdrdy1", " = ",
		au4MacMdrdy[ENUM_BAND_1]);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "FCS Err0", " = ",
		au4FcsError[ENUM_BAND_0]);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "FCS Err1", " = ",
		au4FcsError[ENUM_BAND_1]);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK PD Cnt B0", " = ",
		htonl(g_HqaRxStat.CCK_PD));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK PD Cnt B1", " = ",
		htonl(g_HqaRxStat.CCK_PD_Band1));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK SIG Err B0", " = ",
		htonl(g_HqaRxStat.CCK_SIG_Err));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK SIG Err B1", " = ",
		htonl(g_HqaRxStat.CCK_SIG_Err_Band1));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM PD Cnt B0", " = ",
		htonl(g_HqaRxStat.OFDM_PD));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM PD Cnt B1", " = ",
		htonl(g_HqaRxStat.OFDM_PD_Band1));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM TAG Error", " = ",
		htonl(g_HqaRxStat.OFDM_TAG_Err));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK SFD Err B0", " = ",
		htonl(g_HqaRxStat.CCK_SFD_Err));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK SFD Err B1", " = ",
		htonl(g_HqaRxStat.CCK_SFD_Err_Band1));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM SIG Err B0", " = ",
		htonl(g_HqaRxStat.OFDM_SIG_Err));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM SIG Err B1", " = ",
		htonl(g_HqaRxStat.OFDM_SIG_Err_Band1));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK FCS Err B0", " = ",
		htonl(g_HqaRxStat.FCSErr_CCK));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK FCS Err B1", " = ",
		htonl(g_HqaRxStat.CCK_FCS_Err_Band1));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM FCS Err B0", " = ",
		htonl(g_HqaRxStat.FCSErr_OFDM));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM FCS Err B1", " = ",
		htonl(g_HqaRxStat.OFDM_FCS_Err_Band1));

	if (!fgSkipRxV) {
		u4FAGCRssiIBR0 = (u4RxVector2 & BITS(16, 23)) >> 16;
		u4FAGCRssiWBR0 = (u4RxVector2 & BITS(24, 31)) >> 24;

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "FAGC RSSI W", " = ",
			(u4FAGCRssiWBR0 >= 128) ? (u4FAGCRssiWBR0 - 256) : (u4FAGCRssiWBR0));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "FAGC RSSI I", " = ",
			(u4FAGCRssiIBR0 >= 128) ? (u4FAGCRssiIBR0 - 256) : (u4FAGCRssiIBR0));
	} else{
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "FAGC RSSI W", " = ", "N/A");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "FAGC RSSI I", " = ", "N/A");
	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK MDRDY B0", " = ",
		htonl(g_HqaRxStat.PhyMdrdyCCK));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "CCK MDRDY B1", " = ",
		htonl(g_HqaRxStat.PHY_CCK_MDRDY_Band1));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM MDRDY B0", " = ",
		htonl(g_HqaRxStat.PhyMdrdyOFDM));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM MDRDY B1", " = ",
		htonl(g_HqaRxStat.PHY_OFDM_MDRDY_Band1));

	if (!fgSkipRxV) {
#if 0
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Driver RX Cnt0", " = ",
			htonl(g_HqaRxStat.DriverRxCount));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Driver RX Cnt1", " = ",
			htonl(g_HqaRxStat.DriverRxCount1));
#endif
		u4Value = (u4RxVector0 & BITS(12, 14)) >> 12;
		if (u4Value == 0) {
			u4Foe = (((u4RxVector4 & BITS(7, 31)) >> 7) & 0x7ff);
			u4Foe = (u4Foe * 1000)>>11;
		} else{
			cbw = ((u4RxVector0 & BITS(15, 16)) >> 15);
			foe_const = ((1 << (cbw + 1)) & 0xf) * 10000;
			u4Foe = (((u4RxVector4 & BITS(7, 31)) >> 7) & 0xfff);
			u4Foe = (u4Foe * foe_const) >> 15;
		}

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Freq Offset From RX", " = ", u4Foe);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RX SNR (dB)", " = ",
			(UINT_32)(((u4RxVector4 & BITS(26, 31)) >> 26) - 16));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RCPI RX0", " = ",
			(UINT_32)(u4RxVector3 & BITS(0, 7)));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RCPI RX1", " = ",
			(UINT_32)((u4RxVector3 & BITS(8, 15)) >> 8));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RCPI RX2", " = ",
			((u4RxVector3 & BITS(16, 23)) >> 16) == 0xFF ? (0) :
			(UINT_32)((u4RxVector3 & BITS(16, 23)) >> 16));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RCPI RX3", " = ",
			((u4RxVector3 & BITS(24, 31)) >> 24) == 0xFF ? (0) :
			(UINT_32)((u4RxVector3 & BITS(24, 31)) >> 24));
	} else{
#if 0
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "Driver RX Cnt0", " = ", "N/A");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "Driver RX Cnt1", " = ", "N/A");
#endif
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "Freq Offset From RX", " = ", "N/A");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "RX SNR (dB)", " = ", "N/A");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "RCPI RX0", " = ", "N/A");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "RCPI RX1", " = ", "N/A");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "RCPI RX2", " = ", "N/A");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "RCPI RX3", " = ", "N/A");
	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Inst RSSI IB R0", " = ",
		htonl(g_HqaRxStat.InstRssiIBR0));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Inst RSSI WB R0", " = ",
		htonl(g_HqaRxStat.InstRssiWBR0));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Inst RSSI IB R1", " = ",
		htonl(g_HqaRxStat.InstRssiIBR1));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Inst RSSI WB R1", " = ",
		htonl(g_HqaRxStat.InstRssiWBR1));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Inst RSSI IB R2", " = ",
		htonl(g_HqaRxStat.InstRssiIBR2));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Inst RSSI WB R2", " = ",
		htonl(g_HqaRxStat.InstRssiWBR2));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Inst RSSI IB R3", " = ",
		htonl(g_HqaRxStat.InstRssiIBR3));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Inst RSSI WB R3", " = ",
		htonl(g_HqaRxStat.InstRssiWBR3));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "ACI Hit Lower", " = ",
		htonl(g_HqaRxStat.ACIHitLower));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "ACI Hit Higher", " = ",
		htonl(g_HqaRxStat.ACIHitUpper));

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OutOf Resource Pkt0", " = ",
		au4OutOfResource[ENUM_BAND_0]);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OutOf Resource Pkt1", " = ",
		au4OutOfResource[ENUM_BAND_1]);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Len Mismatch Cnt B0", " = ",
		au4LengthMismatch[ENUM_BAND_0]);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "Len Mismatch Cnt B1", " = ",
		au4LengthMismatch[ENUM_BAND_1]);
#if 0
	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "MU RX Cnt", " = ",
		htonl(g_HqaRxStat.MRURxCount));
#endif
	return i4BytesWritten;
}


static int priv_driver_show_rx_stat(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	P_PARAM_CUSTOM_ACCESS_RX_STAT prRxStatisticsTest;
	BOOLEAN fgResetCnt = FALSE;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;
	UINT_32 u4Id = 0x99980000;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	DBGLOG(INIT, ERROR, "MT6632 : priv_driver_show_rx_stat\n");

	if (i4Argc >= 2) {
		if (strnicmp(apcArgv[1], CMD_STAT_RESET_CNT, strlen(CMD_STAT_RESET_CNT)) == 0)
			fgResetCnt = TRUE;
	}

	if (i4Argc >= 1) {
		if (fgResetCnt) {
			rSwCtrlInfo.u4Id = u4Id;
			rSwCtrlInfo.u4Data = 0;

			rStatus = kalIoctl(prGlueInfo,
					wlanoidSetSwCtrlWrite,
					&rSwCtrlInfo, sizeof(rSwCtrlInfo),
					FALSE, FALSE, TRUE, &u4BufLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				return -1;
		}

		prRxStatisticsTest =
			(P_PARAM_CUSTOM_ACCESS_RX_STAT)kalMemAlloc(sizeof(PARAM_CUSTOM_ACCESS_RX_STAT), VIR_MEM_TYPE);
		if (!prRxStatisticsTest)
			return -1;

		prRxStatisticsTest->u4SeqNum = u4RxStatSeqNum;
		prRxStatisticsTest->u4TotalNum = sizeof(PARAM_RX_STAT_T) / 4;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryRxStatistics,
				   prRxStatisticsTest, sizeof(PARAM_CUSTOM_ACCESS_RX_STAT),
				   TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			kalMemFree(prRxStatisticsTest, VIR_MEM_TYPE, sizeof(PARAM_CUSTOM_ACCESS_RX_STAT));
			return -1;
		}

		i4BytesWritten = priv_driver_dump_rx_stat_info(prAdapter, pcCommand, i4TotalLen, fgResetCnt);

		kalMemFree(prRxStatisticsTest, VIR_MEM_TYPE, sizeof(PARAM_CUSTOM_ACCESS_RX_STAT));
	}

	return i4BytesWritten;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Handle command to get current Tx rate from rate table and respond
*        string buffer. Example: VHT-2SS-BW80-SGI-MCS7
*
* \param[in] net_device Pointer to the Adapter structure.
* \param[out] pcCommand Pointer to the command buffer to respond.
* \param[in] i4TotalLen The length of  buffer.
*
* \retval Length of response buffer
*/
/*----------------------------------------------------------------------------*/
static int priv_driver_get_sta_curr_ar_rate(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	UINT_8 ucWlanIndex = 0;
	PUINT_8 pucMacAddr = NULL;
	UINT_8 idx, txmode, rate;
	P_PARAM_GET_STA_STATISTICS prQueryStaStatistics = NULL;
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -EINVAL;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* Get AIS AP address for no argument */
	if (prAdapter->prAisBssInfo->prStaRecOfAP)
		ucWlanIndex = prAdapter->prAisBssInfo->prStaRecOfAP->ucWlanIndex;
	else if (!wlanGetWlanIdxByAddress(prAdapter, NULL, &ucWlanIndex))
		return i4BytesWritten;
	pucMacAddr = wlanGetStaAddrByWlanIdx(prAdapter, ucWlanIndex);
	if (!pucMacAddr) {
		DBGLOG(REQ, WARN, "%s: MAC address is invalid!\n", __func__);
		return -EFAULT;
	}
	/* Get WTBL info */
	prHwWlanInfo = (P_PARAM_HW_WLAN_INFO_T)kalMemAlloc(sizeof(PARAM_HW_WLAN_INFO_T), VIR_MEM_TYPE);
	if (!prHwWlanInfo)
		return -ENOMEM;
	prHwWlanInfo->u4Index = ucWlanIndex;
	rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryWlanInfo,
					prHwWlanInfo, sizeof(PARAM_HW_WLAN_INFO_T), TRUE, TRUE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		i4BytesWritten = -EFAULT;
		goto out_get_curr_ar_rate;
	}

	prQueryStaStatistics =
		(P_PARAM_GET_STA_STATISTICS)kalMemAlloc(sizeof(PARAM_GET_STA_STA_STATISTICS), VIR_MEM_TYPE);
	if (!prQueryStaStatistics) {
		i4BytesWritten = -ENOMEM;
		goto out_get_curr_ar_rate;
	}

	/* Get Statistics info */
	COPY_MAC_ADDR(prQueryStaStatistics->aucMacAddr, pucMacAddr);
	rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryStaStatistics,
					prQueryStaStatistics,
					sizeof(PARAM_GET_STA_STA_STATISTICS), TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		i4BytesWritten = -EFAULT;
		goto out_get_curr_ar_rate;
	}

	idx = prHwWlanInfo->rWtblRateInfo.ucRateIdx;
	if (idx >= AUTO_RATE_NUM) {
		DBGLOG(REQ, WARN, "%s: Rate index is incorrect (%d)\n", __func__, idx);
		i4BytesWritten = -EFAULT;
		goto out_get_curr_ar_rate;
	}
	txmode = HW_TX_RATE_TO_MODE(prHwWlanInfo->rWtblRateInfo.au2RateCode[idx]);
	if (txmode >= MAX_TX_MODE)
		txmode = MAX_TX_MODE;
	rate = HW_TX_RATE_TO_MCS(prHwWlanInfo->rWtblRateInfo.au2RateCode[idx], txmode);

	/* Mode: [CCK | OFDM | HT | VHT] */
	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"CCK-");
	else if (txmode == TX_RATE_MODE_OFDM)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"OFDM-");
	else if (txmode == TX_RATE_MODE_HTGF || txmode == TX_RATE_MODE_HTMIX)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"HT-");
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"VHT-");
	/* Spatial Streams: [1SS | 2SS | N/A] */
	if (prQueryStaStatistics->ucArTableIdx == RATE_TBL_N_2SS ||
		prQueryStaStatistics->ucArTableIdx == RATE_TBL_AC_2SS)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"2SS-");
	else if (prQueryStaStatistics->ucArTableIdx == RATE_TBL_N ||
		prQueryStaStatistics->ucArTableIdx == RATE_TBL_AC)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"1SS-");
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"N/A-");

	/* BW mode: [BW20 | BW40 | BW80 | BW160/BW8080] */
	if ((txmode == TX_RATE_MODE_CCK) || (txmode == TX_RATE_MODE_OFDM))
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s-", HW_TX_RATE_BW[0]);
	else
		if (idx > prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN)
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			  i4TotalLen - i4BytesWritten, "%s-",
			  prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability < 4 ?
				(prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability > BW_20 ?
				HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability - 1] :
				HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability]) :
				HW_TX_RATE_BW[4]);
		else
			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			  i4TotalLen - i4BytesWritten, "%s-",
			  prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability < 4 ?
				HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability] :
				HW_TX_RATE_BW[4]);
	/* GI mode: [LGI | SGI | X] */
	if (txmode == TX_RATE_MODE_CCK || txmode == TX_RATE_MODE_OFDM)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"N/A-");
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s-", priv_driver_get_sgi_info(&prHwWlanInfo->rWtblPeerCap) == 0 ?
			"LGI" : "SGI");
	/* Rate index: [1M | 2M | 5.5M | 11M |
	 *				6M | 9M | 12M | 18M | 24M | 36M | 48M | 54M |
	 *				MCS# ]
	 */
	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", rate < 4 ? HW_TX_RATE_CCK_STR[rate] : HW_TX_RATE_CCK_STR[4]);
	else if (txmode == TX_RATE_MODE_OFDM)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s", hw_rate_ofdm_str(rate));
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"MCS%d", rate);

out_get_curr_ar_rate:
	if (prHwWlanInfo)
		kalMemFree(prHwWlanInfo, VIR_MEM_TYPE, sizeof(PARAM_HW_WLAN_INFO_T));
	if (prQueryStaStatistics)
		kalMemFree(prQueryStaStatistics, VIR_MEM_TYPE, sizeof(PARAM_GET_STA_STA_STATISTICS));

	return i4BytesWritten;

}

/*----------------------------------------------------------------------------*/
/*
* @ The function will set policy of ACL.
*  0: disable ACL
*  1: enable accept list
*  2: enable deny list
* example: iwpriv p2p0 driver "set_acl_policy 1"
*/
/*----------------------------------------------------------------------------*/
static int priv_driver_set_acl_policy(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Argc = 0, i4BytesWritten = 0, i4Ret = 0, i4Policy = 0;
	UINT_8 ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	DBGLOG(REQ, LOUD, "ucRoleIdx %hhu ucBssIdx %hhu\n", ucRoleIdx, ucBssIdx);
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc < 2)
		return -1;

	i4Ret = kalkStrtou32(apcArgv[1], 0, &i4Policy);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "integer format error i4Ret=%d\n", i4Ret);
		return -1;
	}

	switch (i4Policy) {
	case PARAM_CUSTOM_ACL_POLICY_DISABLE:
	case PARAM_CUSTOM_ACL_POLICY_ACCEPT:
	case PARAM_CUSTOM_ACL_POLICY_DENY:
		prBssInfo->rACL.ePolicy = i4Policy;
		break;
	default: /*Invalid argument */
		DBGLOG(REQ, ERROR, "Invalid ACL Policy=%d\n", i4Policy);
		return -1;
	}

	DBGLOG(REQ, TRACE, "ucBssIdx[%hhu] ACL Policy=%d\n", ucBssIdx, prBssInfo->rACL.ePolicy);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"ucBssIdx[%hhu] ACL Policy=%d\n", ucBssIdx, prBssInfo->rACL.ePolicy);

	/* check if the change in ACL affects any existent association */
	if (prBssInfo->rACL.ePolicy != PARAM_CUSTOM_ACL_POLICY_DISABLE)
		p2pRoleUpdateACLEntry(prAdapter, ucBssIdx);

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	return i4BytesWritten;
} /* priv_driver_set_acl_policy */

static INT_32 priv_driver_inspect_mac_addr(IN char *pcMacAddr)
{
	INT_32 i = 0;

	if (pcMacAddr == NULL)
		return -1;

	for (i = 0; i < 17; i++) {
		if ((i % 3 != 2) && (!kalIsXdigit(pcMacAddr[i]))) {
			DBGLOG(REQ, ERROR, "[%c] is not hex digit\n", pcMacAddr[i]);
			return -1;
		}
		if ((i % 3 == 2) && (pcMacAddr[i] != ':')) {
			DBGLOG(REQ, ERROR, "[%c]separate symbol is error\n", pcMacAddr[i]);
			return -1;
		}
	}

	if (pcMacAddr[17] != '\0') {
		DBGLOG(REQ, ERROR, "no null-terminated character\n");
		return -1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*
* @ The function will add entry to ACL for accept or deny list.
*  example: iwpriv p2p0 driver "add_acl_entry 01:02:03:04:05:06"
*/
/*----------------------------------------------------------------------------*/
static int priv_driver_add_acl_entry(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_8 aucMacAddr[MAC_ADDR_LEN] = {0};
	INT_32 i = 0, i4Argc = 0, i4BytesWritten = 0, i4Ret = 0;
	UINT_8 ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	DBGLOG(REQ, LOUD, "ucRoleIdx %hhu ucBssIdx %hhu\n", ucRoleIdx, ucBssIdx);
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc < 2)
		return -1;

	i4Ret = priv_driver_inspect_mac_addr(apcArgv[1]);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "inspect mac format error u4Ret=%d\n", i4Ret);
		return -1;
	}

	i4Ret = sscanf(apcArgv[1], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		&aucMacAddr[0], &aucMacAddr[1], &aucMacAddr[2],
		&aucMacAddr[3], &aucMacAddr[4], &aucMacAddr[5]);

	if (i4Ret != MAC_ADDR_LEN) {
		DBGLOG(REQ, ERROR, "sscanf mac format fail u4Ret=%d\n", i4Ret);
		return -1;
	}

	for (i = 0; i <= prBssInfo->rACL.u4Num; i++) {
		if (memcmp(prBssInfo->rACL.rEntry[i].aucAddr, &aucMacAddr, MAC_ADDR_LEN) == 0) {
			DBGLOG(REQ, ERROR, "add this mac [" MACSTR "] is duplicate.\n", MAC2STR(aucMacAddr));
			return -1;
		}
	}

	if ((i < 1) || (i > MAX_NUMBER_OF_ACL)) {
		DBGLOG(REQ, ERROR, "idx[%d] error or ACL is full.\n", i);
		return -1;
	}

	memcpy(prBssInfo->rACL.rEntry[i-1].aucAddr, &aucMacAddr, MAC_ADDR_LEN);
	prBssInfo->rACL.u4Num = i;
	DBGLOG(REQ, TRACE, "add mac addr [" MACSTR "] to ACL(%d).\n",
		MAC2STR(prBssInfo->rACL.rEntry[i-1].aucAddr), i);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"add mac addr [" MACSTR "] to ACL(%d)\n", MAC2STR(prBssInfo->rACL.rEntry[i-1].aucAddr), i);

	/* Check if the change in ACL affects any existent association. */
	if (prBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_DENY)
		p2pRoleUpdateACLEntry(prAdapter, ucBssIdx);

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	return i4BytesWritten;
} /* priv_driver_add_acl_entry */

/*----------------------------------------------------------------------------*/
/*
* @ The function will delete entry to ACL for accept or deny list.
*  example: iwpriv p2p0 driver "add_del_entry 01:02:03:04:05:06"
*/
/*----------------------------------------------------------------------------*/
static int priv_driver_del_acl_entry(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_8 aucMacAddr[MAC_ADDR_LEN] = {0};
	INT_32 i = 0, j = 0, i4Argc = 0, i4BytesWritten = 0, i4Ret = 0;
	UINT_8 ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	DBGLOG(REQ, LOUD, "ucRoleIdx %hhu ucBssIdx %hhu\n", ucRoleIdx, ucBssIdx);
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc < 2)
		return -1;

	i4Ret = priv_driver_inspect_mac_addr(apcArgv[1]);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "inspect mac format error u4Ret=%d\n", i4Ret);
		return -1;
	}

	i4Ret = sscanf(apcArgv[1], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		&aucMacAddr[0], &aucMacAddr[1], &aucMacAddr[2],
		&aucMacAddr[3], &aucMacAddr[4], &aucMacAddr[5]);

	if (i4Ret != MAC_ADDR_LEN) {
		DBGLOG(REQ, ERROR, "sscanf mac format fail u4Ret=%d\n", i4Ret);
		return -1;
	}

	for (i = 0; i < prBssInfo->rACL.u4Num; i++) {
		if (memcmp(prBssInfo->rACL.rEntry[i].aucAddr, &aucMacAddr, MAC_ADDR_LEN) == 0) {
			memset(&prBssInfo->rACL.rEntry[i], 0x00, sizeof(PARAM_CUSTOM_ACL_ENTRY));
			DBGLOG(REQ, TRACE, "delete this mac [" MACSTR "]\n", MAC2STR(aucMacAddr));

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"delete this mac [" MACSTR "] from ACL(%d)\n", MAC2STR(aucMacAddr), i+1);
			break;
		}
	}

	if ((prBssInfo->rACL.u4Num == 0) || (i == MAX_NUMBER_OF_ACL)) {
		DBGLOG(REQ, ERROR, "delete entry fail, num of entries=%d\n", i);
		return -1;
	}

	for (j = i+1; j < prBssInfo->rACL.u4Num; j++)
		memcpy(prBssInfo->rACL.rEntry[j-1].aucAddr, prBssInfo->rACL.rEntry[j].aucAddr, MAC_ADDR_LEN);

	prBssInfo->rACL.u4Num = j-1;
	memset(prBssInfo->rACL.rEntry[j-1].aucAddr, 0x00, MAC_ADDR_LEN);

	/* check if the change in ACL affects any existent association */
	if (prBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_ACCEPT)
		p2pRoleUpdateACLEntry(prAdapter, ucBssIdx);

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	return i4BytesWritten;
} /* priv_driver_del_acl_entry */

/*----------------------------------------------------------------------------*/
/*
* @ The function will show all entries to ACL for accept or deny list.
*  example: iwpriv p2p0 driver "show_acl_entry"
*/
/*----------------------------------------------------------------------------*/
static int priv_driver_show_acl_entry(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i = 0, i4Argc = 0, i4BytesWritten = 0;
	UINT_8 ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);
	DBGLOG(REQ, TRACE, "ACL Policy = %d\n", prBssInfo->rACL.ePolicy);
	DBGLOG(REQ, TRACE, "Total ACLs = %d\n", prBssInfo->rACL.u4Num);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"ACL Policy = %d, Total ACLs = %d\n", prBssInfo->rACL.ePolicy, prBssInfo->rACL.u4Num);

	for (i = 0; i < prBssInfo->rACL.u4Num; i++) {
		DBGLOG(REQ, TRACE, "ACL(%d): [" MACSTR "]\n", i+1, MAC2STR(prBssInfo->rACL.rEntry[i].aucAddr));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"ACL(%d): [" MACSTR "]\n", i+1, MAC2STR(prBssInfo->rACL.rEntry[i].aucAddr));
	}

	return i4BytesWritten;
} /* priv_driver_show_acl_entry */

/*----------------------------------------------------------------------------*/
/*
* @ The function will clear all entries to ACL for accept or deny list.
*  example: iwpriv p2p0 driver "clear_acl_entry"
*/
/*----------------------------------------------------------------------------*/
static int priv_driver_clear_acl_entry(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	P_BSS_INFO_T prBssInfo = NULL;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Argc = 0, i4BytesWritten = 0;
	UINT_8 ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (prBssInfo->rACL.u4Num) {
		memset(&prBssInfo->rACL.rEntry[0], 0x00, sizeof(PARAM_CUSTOM_ACL_ENTRY)*MAC_ADDR_LEN);
		prBssInfo->rACL.u4Num = 0;
	}

	DBGLOG(REQ, TRACE, "ACL Policy = %d\n", prBssInfo->rACL.ePolicy);
	DBGLOG(REQ, TRACE, "Total ACLs = %d\n", prBssInfo->rACL.u4Num);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
		"ACL Policy = %d, Total ACLs = %d\n", prBssInfo->rACL.ePolicy, prBssInfo->rACL.u4Num);

	/* check if the change in ACL affects any existent association */
	if (prBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_ACCEPT)
		p2pRoleUpdateACLEntry(prAdapter, ucBssIdx);

	return i4BytesWritten;
} /* priv_driver_clear_acl_entry */

static int priv_driver_get_drv_mcr(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4Ret;
	/* INT_32 i4ArgNum_with_ant_sel = 3; */	/* Add Antenna Selection Input */
	INT_32 i4ArgNum = 2;

	CMD_ACCESS_REG rCmdAccessReg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rCmdAccessReg.u4Address));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse get_drv_mcr error (Address) u4Ret=%d\n", u4Ret);

		/* rCmdAccessReg.u4Address = kalStrtoul(apcArgv[1], NULL, 0); */
		rCmdAccessReg.u4Data = 0;

		DBGLOG(REQ, LOUD, "address is %x\n", rCmdAccessReg.u4Address);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryDrvMcrRead,
				   &rCmdAccessReg, sizeof(rCmdAccessReg), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "0x%08x", (unsigned int)rCmdAccessReg.u4Data);
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);
	}

	return i4BytesWritten;

}				/* priv_driver_get_drv_mcr */

int priv_driver_set_drv_mcr(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Ret;
	/* INT_32 i4ArgNum_with_ant_sel = 4; */	/* Add Antenna Selection Input */
	INT_32 i4ArgNum = 3;

	CMD_ACCESS_REG rCmdAccessReg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rCmdAccessReg.u4Address));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse get_drv_mcr error (Address) u4Ret=%d\n", u4Ret);

		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rCmdAccessReg.u4Data));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse get_drv_mcr error (Data) u4Ret=%d\n", u4Ret);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetDrvMcrWrite,
				   &rCmdAccessReg, sizeof(rCmdAccessReg), FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	}

	return i4BytesWritten;

}

static int priv_driver_get_sw_ctrl(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 u4Ret = 0;

	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		/* rSwCtrlInfo.u4Id = kalStrtoul(apcArgv[1], NULL, 0); */
		rSwCtrlInfo.u4Data = 0;
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rSwCtrlInfo.u4Id));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);

		DBGLOG(REQ, LOUD, "id is %x\n", rSwCtrlInfo.u4Id);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQuerySwCtrlRead,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "0x%08x", (unsigned int)rSwCtrlInfo.u4Data);
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);
	}

	return i4BytesWritten;

}				/* priv_driver_get_sw_ctrl */


int priv_driver_set_sw_ctrl(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret = 0;

	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 3) {
		/* rSwCtrlInfo.u4Id = kalStrtoul(apcArgv[1], NULL, 0);
		 *  rSwCtrlInfo.u4Data = kalStrtoul(apcArgv[2], NULL, 0);
		 */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rSwCtrlInfo.u4Id));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);
		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rSwCtrlInfo.u4Data));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetSwCtrlWrite,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	}

	return i4BytesWritten;

}				/* priv_driver_set_sw_ctrl */



int priv_driver_set_fixed_rate(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	/* INT_32 u4Ret = 0; */
	UINT_32 u4WCID = 0;
	UINT_32 u4Mode = 0, u4Bw = 0, u4Mcs = 0, u4VhtNss = 0;
	UINT_32 u4SGI = 0, u4Preamble = 0, u4STBC = 0, u4LDPC = 0, u4SpeEn = 0;
	INT_32 i4Recv = 0;
	CHAR *this_char = NULL;
	UINT_32 u4Id = 0xa0610000;
	UINT_32 u4Data = 0x80000000;
	UINT_32 u4Id2 = 0xa0600000;
	UINT_8 u4Nsts = 1;
	BOOLEAN fgStatus = TRUE;

	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %d, apcArgv[0] = %s\n\n", i4Argc, *apcArgv);

	this_char = kalStrStr(*apcArgv, "=");

	if (!this_char)
		return -1;

	this_char++;

	DBGLOG(REQ, LOUD, "string = %s\n", this_char);

	if (strnicmp(this_char, "auto", strlen("auto")) == 0) {
		i4Recv = 1;
	} else {
		i4Recv = sscanf(this_char, "%d-%d-%d-%d-%d-%d-%d-%d-%d-%d", &(u4WCID),
					&(u4Mode), &(u4Bw), &(u4Mcs), &(u4VhtNss),
					&(u4SGI), &(u4Preamble), &(u4STBC), &(u4LDPC), &(u4SpeEn));

		DBGLOG(REQ, LOUD, "u4WCID=%d\nu4Mode=%d\nu4Bw=%d\n", u4WCID, u4Mode, u4Bw);
	    DBGLOG(REQ, LOUD, "u4Mcs=%d\nu4VhtNss=%d\nu4SGI=%d\n", u4Mcs, u4VhtNss, u4SGI);
	    DBGLOG(REQ, LOUD, "u4Preamble=%d\nu4STBC=%d\n", u4Preamble, u4STBC);
	    DBGLOG(REQ, LOUD, "u4LDPC=%d\nu4SpeEn=%d\n", u4LDPC, u4SpeEn);
	}

	if (i4Recv == 1) {
		rSwCtrlInfo.u4Id = u4Id2;
		rSwCtrlInfo.u4Data = 0;

		rStatus = kalIoctl(prGlueInfo,
					wlanoidSetSwCtrlWrite,
					&rSwCtrlInfo, sizeof(rSwCtrlInfo),
					FALSE, FALSE, TRUE, &u4BufLen);
	} else if (i4Recv == 10) {
		rSwCtrlInfo.u4Id = u4Id;
		rSwCtrlInfo.u4Data = u4Data;

		if (u4SGI)
			rSwCtrlInfo.u4Data |= BIT(30);
		if (u4LDPC)
			rSwCtrlInfo.u4Data |= BIT(29);
		if (u4SpeEn)
			rSwCtrlInfo.u4Data |= BIT(28);
		if (u4STBC)
			rSwCtrlInfo.u4Data |= BIT(11);

		if (u4Bw <= 3)
			rSwCtrlInfo.u4Data |= ((u4Bw << 26) & BITS(26, 27));
		else {
			fgStatus = FALSE;
			DBGLOG(INIT, ERROR, "Wrong BW! BW20=0, BW40=1, BW80=2,BW160=3\n");
		}
		if (u4Mode <= 4) {
			rSwCtrlInfo.u4Data |= ((u4Mode << 6) & BITS(6, 8));

			switch (u4Mode) {
			case 0:
				if (u4Mcs <= 3)
					rSwCtrlInfo.u4Data |= u4Mcs;
				else {
					fgStatus = FALSE;
					DBGLOG(INIT, ERROR, "CCK mode but wrong MCS!\n");
				}

			if (u4Preamble)
				rSwCtrlInfo.u4Data |= BIT(2);
			else
				rSwCtrlInfo.u4Data &= ~BIT(2);

			break;
			case 1:
				switch (u4Mcs) {
				case 0:
					/* 6'b001011 */
					rSwCtrlInfo.u4Data |= 11;
					break;
				case 1:
					/* 6'b001111 */
					rSwCtrlInfo.u4Data |= 15;
					break;
				case 2:
					/* 6'b001010 */
					rSwCtrlInfo.u4Data |= 10;
					break;
				case 3:
					/* 6'b001110 */
					rSwCtrlInfo.u4Data |= 14;
					break;
				case 4:
					/* 6'b001001 */
					rSwCtrlInfo.u4Data |= 9;
					break;
				case 5:
					/* 6'b001101 */
					rSwCtrlInfo.u4Data |= 13;
					break;
				case 6:
					/* 6'b001000 */
					rSwCtrlInfo.u4Data |= 8;
					break;
				case 7:
					/* 6'b001100 */
					rSwCtrlInfo.u4Data |= 12;
					break;
				default:
					fgStatus = FALSE;
					DBGLOG(INIT, ERROR, "OFDM mode but wrong MCS!\n");
				break;
				}
			break;
			case 2:
			case 3:
				if (u4Mcs <= 32)
					rSwCtrlInfo.u4Data |= u4Mcs;
				else {
					fgStatus = FALSE;
					DBGLOG(INIT, ERROR, "HT mode but wrong MCS!\n");
				}

				if (u4Mcs != 32) {
					u4Nsts += (u4Mcs >> 3);
					if (u4STBC && (u4Nsts == 1))
						u4Nsts++;
				}
				break;
			case 4:
				if (u4Mcs <= 9)
					rSwCtrlInfo.u4Data |= u4Mcs;
				else {
				fgStatus = FALSE;
				DBGLOG(INIT, ERROR, "VHT mode but wrong MCS!\n");
				}
				if (u4STBC && (u4VhtNss == 1))
					u4Nsts++;
				else
					u4Nsts = u4VhtNss;
			break;
			default:
				break;
			}
		} else {
			fgStatus = FALSE;
			DBGLOG(INIT, ERROR, "Wrong TxMode! CCK=0, OFDM=1, HT=2, GF=3, VHT=4\n");
		}

		rSwCtrlInfo.u4Data |= (((u4Nsts - 1) << 9) & BITS(9, 10));

		if (fgStatus) {
			rStatus = kalIoctl(prGlueInfo,
								wlanoidSetSwCtrlWrite,
								&rSwCtrlInfo, sizeof(rSwCtrlInfo),
								FALSE, FALSE, TRUE, &u4BufLen);
		}

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver FixedRate=Option\n");
		DBGLOG(INIT, ERROR,
			"Option:[WCID]-[Mode]-[BW]-[MCS]-[VhtNss]-[SGI]-[Preamble]-[STBC]-[LDPC]-[SPE_EN]\n");
		DBGLOG(INIT, ERROR, "[WCID]Wireless Client ID\n");
		DBGLOG(INIT, ERROR, "[Mode]CCK=0, OFDM=1, HT=2, GF=3, VHT=4\n");
		DBGLOG(INIT, ERROR, "[BW]BW20=0, BW40=1, BW80=2,BW160=3\n");
		DBGLOG(INIT, ERROR, "[MCS]CCK=0~3, OFDM=0~7, HT=0~32, VHT=0~9\n");
		DBGLOG(INIT, ERROR, "[VhtNss]VHT=1~4, Other=ignore\n");
		DBGLOG(INIT, ERROR, "[Preamble]Long=0, Other=Short\n");
	}

	return i4BytesWritten;
}				/* priv_driver_set_fixed_rate */

int priv_driver_set_cfg(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };

	PARAM_CUSTOM_KEY_CFG_STRUCT_T rKeyCfgInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rKeyCfgInfo, sizeof(rKeyCfgInfo));

	if (i4Argc >= 3) {

		CHAR ucTmp[WLAN_CFG_VALUE_LEN_MAX];
		PUINT_8 pucCurrBuf = ucTmp;
		UINT_8	i = 0;
		INT_32	i4TmpBufLen = 0;

		kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);

		if (i4Argc == 3) {
			/*no space for it, driver can't accept space in the end of the line*/
			/*ToDo: skip the space when parsing*/
			scnprintf(pucCurrBuf, sizeof(ucTmp), "%s", apcArgv[2]);
		} else {
			for (i = 2; i < i4Argc; i++)
				i4TmpBufLen += scnprintf((pucCurrBuf + i4TmpBufLen),
								(sizeof(ucTmp) - i4TmpBufLen), "%s", apcArgv[i]);
		}

		DBGLOG(INIT, WARN, "Update to driver temp buffer as [%s]\n", ucTmp);

		/* wlanCfgSet(prAdapter, apcArgv[1], apcArgv[2], 0); */
		/* Call by  wlanoid because the set_cfg will trigger callback */
		kalStrnCpy(rKeyCfgInfo.aucKey, apcArgv[1], WLAN_CFG_KEY_LEN_MAX);
		kalStrnCpy(rKeyCfgInfo.aucValue, ucTmp, WLAN_CFG_KEY_LEN_MAX);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetKeyCfg, &rKeyCfgInfo, sizeof(rKeyCfgInfo), FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}

	return i4BytesWritten;

}				/* priv_driver_set_cfg  */

int priv_driver_get_cfg(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	CHAR aucValue[WLAN_CFG_VALUE_LEN_MAX];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);
	prAdapter = prGlueInfo->prAdapter;

	if (i4Argc >= 2) {
		/* by wlanoid ? */
		if (wlanCfgGet(prAdapter, apcArgv[1], aucValue, "", 0) == WLAN_STATUS_SUCCESS) {
			kalStrnCpy(pcCommand, aucValue, WLAN_CFG_VALUE_LEN_MAX);
			i4BytesWritten = kalStrnLen(pcCommand, WLAN_CFG_VALUE_LEN_MAX);
		}
	}

	return i4BytesWritten;

}				/* priv_driver_get_cfg  */

int priv_driver_set_chip_config(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	UINT_32 u4CmdLen = 0;
	UINT_32 u4PrefixLen = 0;
	/* INT_32 i4Argc = 0; */
	/* PCHAR  apcArgv[WLAN_CFG_ARGV_MAX] = {0}; */

	PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T rChipConfigInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	/* wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv); */
	/* DBGLOG(REQ, LOUD,("argc is %i\n",i4Argc)); */
	/*  */
	u4CmdLen = kalStrnLen(pcCommand, i4TotalLen);
	u4PrefixLen = kalStrLen(CMD_SET_CHIP) + 1 /*space */;

	kalMemZero(&rChipConfigInfo, sizeof(rChipConfigInfo));

	/* if(i4Argc >= 2) { */
	if (u4CmdLen > u4PrefixLen) {

		rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
		/* rChipConfigInfo.u2MsgSize = kalStrnLen(apcArgv[1],CHIP_CONFIG_RESP_SIZE); */
		rChipConfigInfo.u2MsgSize = u4CmdLen - u4PrefixLen;
		/* kalStrnCpy(rChipConfigInfo.aucCmd,apcArgv[1],CHIP_CONFIG_RESP_SIZE); */
		kalStrnCpy(rChipConfigInfo.aucCmd, pcCommand + u4PrefixLen, CHIP_CONFIG_RESP_SIZE - 1);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetChipConfig,
				   &rChipConfigInfo, sizeof(rChipConfigInfo), FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, INFO, "%s: kalIoctl ret=%d\n", __func__, rStatus);
			i4BytesWritten = -1;
		}
	}

	return i4BytesWritten;

}				/* priv_driver_set_chip_config  */

void
priv_driver_get_chip_config_16(PUINT_8 pucStartAddr, UINT_32 u4Length, UINT_32 u4Line, int i4TotalLen,
				INT_32 i4BytesWritten, char *pcCommand)
{

	while (u4Length >= 16) {
		if (i4TotalLen > i4BytesWritten) {
			i4BytesWritten +=
			    snprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%04x %02x %02x %02x %02x  %02x %02x %02x %02x - %02x %02x %02x %02x  %02x %02x %02x %02x\n",
					u4Line, pucStartAddr[0],
					pucStartAddr[1],
					pucStartAddr[2],
					pucStartAddr[3],
					pucStartAddr[4],
					pucStartAddr[5],
					pucStartAddr[6],
					pucStartAddr[7],
					pucStartAddr[8],
					pucStartAddr[9],
					pucStartAddr[10],
					pucStartAddr[11],
					pucStartAddr[12], pucStartAddr[13], pucStartAddr[14], pucStartAddr[15]);
		}

		pucStartAddr += 16;
		u4Length -= 16;
		u4Line += 16;
	}			/* u4Length */
}


void
priv_driver_get_chip_config_4(PUINT_32 pu4StartAddr, UINT_32 u4Length, UINT_32 u4Line, int i4TotalLen,
			      INT_32 i4BytesWritten, char *pcCommand)
{
	while (u4Length >= 16) {
		if (i4TotalLen > i4BytesWritten) {
			i4BytesWritten +=
			    snprintf(pcCommand +
				     i4BytesWritten,
				     i4TotalLen -
				     i4BytesWritten,
				     "%04x %08x %08x %08x %08x\n",
				     u4Line, pu4StartAddr[0], pu4StartAddr[1], pu4StartAddr[2], pu4StartAddr[3]);
		}

		pu4StartAddr += 4;
		u4Length -= 16;
		u4Line += 4;
	}			/* u4Length */
}

int priv_driver_get_chip_config(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	INT_32 i4BytesWritten = 0;
	UINT_32 u4BufLen = 0;
	UINT_32 u2MsgSize = 0;
	UINT_32 u4CmdLen = 0;
	UINT_32 u4PrefixLen = 0;
	/* INT_32 i4Argc = 0; */
	/* PCHAR  apcArgv[WLAN_CFG_ARGV_MAX]; */

	PARAM_CUSTOM_CHIP_CONFIG_STRUCT_T rChipConfigInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	/* wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv); */
	/* DBGLOG(REQ, LOUD,("argc is %i\n",i4Argc)); */

	u4CmdLen = kalStrnLen(pcCommand, i4TotalLen);
	u4PrefixLen = kalStrLen(CMD_GET_CHIP) + 1 /*space */;

	/* if(i4Argc >= 2) { */
	if (u4CmdLen > u4PrefixLen) {
		rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_ASCII;
		/* rChipConfigInfo.u2MsgSize = kalStrnLen(apcArgv[1],CHIP_CONFIG_RESP_SIZE); */
		rChipConfigInfo.u2MsgSize = u4CmdLen - u4PrefixLen;
		/* kalStrnCpy(rChipConfigInfo.aucCmd,apcArgv[1],CHIP_CONFIG_RESP_SIZE); */
		kalStrnCpy(rChipConfigInfo.aucCmd, pcCommand + u4PrefixLen, CHIP_CONFIG_RESP_SIZE - 1);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryChipConfig,
				   &rChipConfigInfo, sizeof(rChipConfigInfo), TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, INFO, "%s: kalIoctl ret=%d\n", __func__, rStatus);
			return -1;
		}

		/* Check respType */
		u2MsgSize = rChipConfigInfo.u2MsgSize;
		DBGLOG(REQ, INFO, "%s: RespTyep  %u\n", __func__, rChipConfigInfo.ucRespType);
		DBGLOG(REQ, INFO, "%s: u2MsgSize %u\n", __func__, rChipConfigInfo.u2MsgSize);

		if (u2MsgSize > sizeof(rChipConfigInfo.aucCmd)) {
			DBGLOG(REQ, INFO, "%s: u2MsgSize error ret=%u\n", __func__, rChipConfigInfo.u2MsgSize);
			return -1;
		}

		if (u2MsgSize > 0) {

			if (rChipConfigInfo.ucRespType == CHIP_CONFIG_TYPE_ASCII) {
				i4BytesWritten =
				    snprintf(pcCommand + i4BytesWritten, i4TotalLen, "%s", rChipConfigInfo.aucCmd);
			} else {
				UINT_32 u4Length;
				UINT_32 u4Line;

				if (rChipConfigInfo.ucRespType == CHIP_CONFIG_TYPE_MEM8) {
					PUINT_8 pucStartAddr = NULL;

					pucStartAddr = (PUINT_8) rChipConfigInfo.aucCmd;
					/* align 16 bytes because one print line is 16 bytes */
					u4Length = (((u2MsgSize + 15) >> 4)) << 4;
					u4Line = 0;
					priv_driver_get_chip_config_16(pucStartAddr, u4Length, u4Line, i4TotalLen,
									i4BytesWritten, pcCommand);
				} else {
					PUINT_32 pu4StartAddr = NULL;

					pu4StartAddr = (PUINT_32) rChipConfigInfo.aucCmd;
					/* align 16 bytes because one print line is 16 bytes */
					u4Length = (((u2MsgSize + 15) >> 4)) << 4;
					u4Line = 0;

					if (IS_ALIGN_4((ULONG) pu4StartAddr)) {
						priv_driver_get_chip_config_4(pu4StartAddr, u4Length, u4Line,
									      i4TotalLen, i4BytesWritten, pcCommand);
					} else {
						DBGLOG(REQ, INFO,
							"%s: rChipConfigInfo.aucCmd is not 4 bytes alignment %p\n",
							__func__, rChipConfigInfo.aucCmd);
					}
				}	/* ChipConfigInfo.ucRespType */
			}
		}
		/* u2MsgSize > 0 */
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);
	}
	/* i4Argc */
	return i4BytesWritten;

}				/* priv_driver_get_chip_config  */



int priv_driver_set_ap_start(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{

	PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Ret;
	INT_32 i4ArgNum = 2;


	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rSetP2P.u4Mode));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ap-start error (u4Enable) u4Ret=%d\n", u4Ret);

		if (rSetP2P.u4Mode >= RUNNING_P2P_MODE_NUM) {
			rSetP2P.u4Mode = 0;
			rSetP2P.u4Enable = 0;
		} else
			rSetP2P.u4Enable = 1;

		set_p2p_mode_handler(prNetDev, rSetP2P);
	}

	return 0;
}

int priv_driver_get_linkspeed(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	UINT_32 u4Rate = 0;
	UINT_32 u4LinkSpeed = 0;
	INT_32 i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (!netif_carrier_ok(prNetDev))
		return -1;

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryLinkSpeed, &u4Rate, sizeof(u4Rate), TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4LinkSpeed = u4Rate * 100;
	i4BytesWritten = snprintf(pcCommand, i4TotalLen, "LinkSpeed %u", (unsigned int)u4LinkSpeed);
	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);
	return i4BytesWritten;

}				/* priv_driver_get_linkspeed */

int priv_driver_set_band(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_ADAPTER_T prAdapter = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Argc = 0;
	UINT_32 ucBand = 0;
	UINT_8 ucBssIndex;
	ENUM_BAND_T eBand = BAND_NULL;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prAdapter = prGlueInfo->prAdapter;
	if (i4Argc >= 2) {
		/* ucBand = kalStrtoul(apcArgv[1], NULL, 0); */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &ucBand);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucBand error u4Ret=%d\n", u4Ret);

		ucBssIndex = wlanGetAisBssIndex(prGlueInfo->prAdapter);
		eBand = BAND_NULL;
		if (ucBand == CMD_BAND_5G)
			eBand = BAND_5G;
		else if (ucBand == CMD_BAND_2G)
			eBand = BAND_2G4;
		prAdapter->aePreferBand[ucBssIndex] = eBand;
		/* XXX call wlanSetPreferBandByNetwork directly in different thread */
		/* wlanSetPreferBandByNetwork (prAdapter, eBand, ucBssIndex); */
	}

	return 0;
}

int priv_driver_set_txpower(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	P_SET_TXPWR_CTRL_T prTxpwr;
	UINT_16 i;
	INT_32 u4Ret = 0;
	INT_32 ai4Setting[4];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prTxpwr = &prGlueInfo->rTxPwr;

	if (i4Argc >= 3 && i4Argc <= 5) {
		for (i = 0; i < (i4Argc - 1); i++) {
			/* ai4Setting[i] = kalStrtol(apcArgv[i + 1], NULL, 0); */
			u4Ret = kalkStrtos32(apcArgv[i + 1], 0, &(ai4Setting[i]));
			if (u4Ret)
				DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n", u4Ret);
			/* printk("PeiHsuan setting[%d] = %d\n", i, setting[i]); */
		}
	} else {
		DBGLOG(REQ, INFO, "set_txpower wrong argc : %d\n", i4Argc);
		return -1;
	}

	/*
	 *  ai4Setting[0]
	 *  0 : Set TX power offset for specific network
	 *  1 : Set TX power offset policy when multiple networks are in the same channel
	 *  2 : Set TX power limit for specific channel in 2.4GHz band
	 *  3 : Set TX power limit of specific sub-band in 5GHz band
	 *  4 : Enable or reset setting
	 */
	if (ai4Setting[0] == 0 && (i4Argc - 1) == 4 /* argc num */) {
		/* ai4Setting[1] : 0 (All networks), 1 (legacy STA), 2 (Hotspot AP), 3 (P2P), 4 (BT over Wi-Fi) */
		/* ai4Setting[2] : 0 (All bands),1 (2.4G), 2 (5G) */
		/* ai4Setting[3] : -30 ~ 20 in unit of 0.5dBm (default: 0) */
		if (ai4Setting[1] == 1 || ai4Setting[1] == 0) {
			if (ai4Setting[2] == 0 || ai4Setting[2] == 1)
				prTxpwr->c2GLegacyStaPwrOffset = ai4Setting[3];
			if (ai4Setting[2] == 0 || ai4Setting[2] == 2)
				prTxpwr->c5GLegacyStaPwrOffset = ai4Setting[3];
		}
		if (ai4Setting[1] == 2 || ai4Setting[1] == 0) {
			if (ai4Setting[2] == 0 || ai4Setting[2] == 1)
				prTxpwr->c2GHotspotPwrOffset = ai4Setting[3];
			if (ai4Setting[2] == 0 || ai4Setting[2] == 2)
				prTxpwr->c5GHotspotPwrOffset = ai4Setting[3];
		}
		if (ai4Setting[1] == 3 || ai4Setting[1] == 0) {
			if (ai4Setting[2] == 0 || ai4Setting[2] == 1)
				prTxpwr->c2GP2pPwrOffset = ai4Setting[3];
			if (ai4Setting[2] == 0 || ai4Setting[2] == 2)
				prTxpwr->c5GP2pPwrOffset = ai4Setting[3];
		}
		if (ai4Setting[1] == 4 || ai4Setting[1] == 0) {
			if (ai4Setting[2] == 0 || ai4Setting[2] == 1)
				prTxpwr->c2GBowPwrOffset = ai4Setting[3];
			if (ai4Setting[2] == 0 || ai4Setting[2] == 2)
				prTxpwr->c5GBowPwrOffset = ai4Setting[3];
		}
	} else if (ai4Setting[0] == 1 && (i4Argc - 1) == 2) {
		/* ai4Setting[1] : 0 (highest power is used) (default), 1 (lowest power is used) */
		prTxpwr->ucConcurrencePolicy = ai4Setting[1];
	} else if (ai4Setting[0] == 2 && (i4Argc - 1) == 3) {
		/* ai4Setting[1] : 0 (all channels in 2.4G), 1~14 */
		/* ai4Setting[2] : 10 ~ 46 in unit of 0.5dBm (default: 46) */
		if (ai4Setting[1] == 0) {
			for (i = 0; i < 14; i++)
				prTxpwr->acTxPwrLimit2G[i] = ai4Setting[2];
		} else if (ai4Setting[1] <= 14)
			prTxpwr->acTxPwrLimit2G[ai4Setting[1] - 1] = ai4Setting[2];
	} else if (ai4Setting[0] == 3 && (i4Argc - 1) == 3) {
		/* ai4Setting[1] : 0 (all sub-bands in 5G),
		 *  1 (5000 ~ 5250MHz),
		 *  2 (5255 ~ 5350MHz),
		 *  3 (5355 ~ 5725MHz),
		 *  4 (5730 ~ 5825MHz)
		 */
		/* ai4Setting[2] : 10 ~ 46 in unit of 0.5dBm (default: 46) */
		if (ai4Setting[1] == 0) {
			for (i = 0; i < 4; i++)
				prTxpwr->acTxPwrLimit5G[i] = ai4Setting[2];
		} else if (ai4Setting[1] <= 4)
			prTxpwr->acTxPwrLimit5G[ai4Setting[1] - 1] = ai4Setting[2];
	} else if (ai4Setting[0] == 4 && (i4Argc - 1) == 2) {
		/* ai4Setting[1] : 1 (enable), 0 (reset and disable) */
		if (ai4Setting[1] == 0)
			wlanDefTxPowerCfg(prGlueInfo->prAdapter);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetTxPower, prTxpwr, sizeof(SET_TXPWR_CTRL_T), FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	} else
		return -EFAULT;

	return 0;
}

int priv_driver_set_country(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_8 aucCountry[2];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (regd_is_single_sku_en()) {
		UINT_8 aucCountry_code[4] = {0, 0, 0, 0};
		UINT_8 i, count;

		/* command like "COUNTRY US", "COUNTRY US1" and "COUNTRY US01" */
		count = kalStrnLen(apcArgv[1], sizeof(aucCountry_code));
		for (i = 0; i < count; i++)
			aucCountry_code[i] = apcArgv[1][i];


		rStatus = kalIoctl(prGlueInfo, wlanoidSetCountryCode,
							&aucCountry_code[0], count, FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		return 0;
	}


	if (i4Argc >= 2) {
		/* command like "COUNTRY US", "COUNTRY EU" and "COUNTRY JP" */
		aucCountry[0] = apcArgv[1][0];
		aucCountry[1] = apcArgv[1][1];

		rStatus = kalIoctl(prGlueInfo, wlanoidSetCountryCode, &aucCountry[0], 2, FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}
	return 0;
}

int priv_driver_get_country(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{

	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 i4BytesWritten = 0;
	UINT_32 country = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (!regd_is_single_sku_en()) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "Not Supported.");
		return i4BytesWritten;
	}

	country = rlmDomainGetCountryCode();

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nCountry Code: (0x%x)",
	       country);

	return	i4BytesWritten;
}

int priv_driver_get_channels(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	UINT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	UINT_32 ch_idx, start_idx, end_idx;
	struct channel *pCh;
	UINT_32 ch_num = 0;
	UINT_8 maxbw = 160;
	UINT_32 u4Ret = 0;
#endif

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (!regd_is_single_sku_en()) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "Not Supported.");
		return i4BytesWritten;
	}

#if (CFG_SUPPORT_SINGLE_SKU == 1)
	/**
	 * Usage: iwpriv wlan0 driver "get_channels [2g |5g |ch_num]"
	 **/
	if (i4Argc >= 2 && (apcArgv[1][0] == '2') && (apcArgv[1][1] == 'g')) {
		start_idx = 0;
		end_idx = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ);
	} else if (i4Argc >= 2 && (apcArgv[1][0] == '5') && (apcArgv[1][1] == 'g')) {
		start_idx = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ);
		end_idx = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ)
				+ rlmDomainGetActiveChannelCount(KAL_BAND_5GHZ);
	} else {
		start_idx = 0;
		end_idx = rlmDomainGetActiveChannelCount(KAL_BAND_2GHZ)
				+ rlmDomainGetActiveChannelCount(KAL_BAND_5GHZ);
		if (i4Argc >= 2)
			/* Dump only specified channel */
			u4Ret = kalkStrtou32(apcArgv[1], 0, &ch_num);
	}

	if (regd_is_single_sku_en()) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n");

		for (ch_idx = start_idx; ch_idx < end_idx; ch_idx++) {

			pCh = (rlmDomainGetActiveChannels() + ch_idx);

			if (ch_num && (ch_num != pCh->chNum))
				continue; /*show specific channel information*/

			/* Channel number */
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "CH-%d:",
					pCh->chNum);
			/* Active/Passive */
			if (pCh->flags & IEEE80211_CHAN_PASSIVE_FLAG)
				LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, " " IEEE80211_CHAN_PASSIVE_STR);
			else
				LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, " ACTIVE");
			/* Max BW */
			if ((pCh->flags & IEEE80211_CHAN_NO_160MHZ) == IEEE80211_CHAN_NO_160MHZ)
				maxbw = 80;
			if ((pCh->flags & IEEE80211_CHAN_NO_80MHZ) == IEEE80211_CHAN_NO_80MHZ)
				maxbw = 40;
			if ((pCh->flags & IEEE80211_CHAN_NO_HT40) == IEEE80211_CHAN_NO_HT40)
				maxbw = 20;
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, " BW_%dMHz", maxbw);
			/* Channel flags */
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "  (flags=0x%x)\n", pCh->flags);
		}
	}
#endif

	return i4BytesWritten;
}

#if (CFG_SUPPORT_DFS_MASTER == 1)
int priv_driver_show_dfs_state(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nDFS State: \"%s\"",
			p2pFuncShowDfsState());

	return	i4BytesWritten;
}

int priv_driver_show_dfs_radar_param(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4BytesWritten = 0;
	UINT_8 ucCnt = 0;
	struct P2P_RADAR_INFO *prP2pRadarInfo = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	prP2pRadarInfo = (struct P2P_RADAR_INFO *) cnmMemAlloc(prGlueInfo->prAdapter,
		RAM_TYPE_MSG, sizeof(*prP2pRadarInfo));

	if (!prP2pRadarInfo)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	p2pFuncGetRadarInfo(prP2pRadarInfo);

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nRDD idx: %d\n",
			prP2pRadarInfo->ucRddIdx);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nLong Pulse detected: %d\n",
			prP2pRadarInfo->ucLongDetected);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nPeriodic Pulse detected: %d\n",
			prP2pRadarInfo->ucPeriodicDetected);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nLPB Num: %d\n",
			prP2pRadarInfo->ucLPBNum);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nPPB Num: %d\n",
			prP2pRadarInfo->ucPPBNum);

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n===========================");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nLong Pulse Buffer Contents:\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\npulse_time    pulse_width    PRI\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n%-10d    %-11d    -\n"
		, prP2pRadarInfo->arLpbContent[ucCnt].u4LongStartTime
		, prP2pRadarInfo->arLpbContent[ucCnt].u2LongPulseWidth);
	for (ucCnt = 1; ucCnt < prP2pRadarInfo->ucLPBNum; ucCnt++) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n%-10d    %-11d    %d\n"
			, prP2pRadarInfo->arLpbContent[ucCnt].u4LongStartTime
			, prP2pRadarInfo->arLpbContent[ucCnt].u2LongPulseWidth
			, (prP2pRadarInfo->arLpbContent[ucCnt].u4LongStartTime
				- prP2pRadarInfo->arLpbContent[ucCnt-1].u4LongStartTime) * 2 / 5);
	}
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nLPB Period Valid: %d",
			prP2pRadarInfo->ucLPBPeriodValid);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nLPB Period Valid: %d\n",
			prP2pRadarInfo->ucLPBWidthValid);

	ucCnt = 0;
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n===========================");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nPeriod Pulse Buffer Contents:\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\npulse_time    pulse_width    PRI\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n%-10d    %-11d    -\n"
		, prP2pRadarInfo->arPpbContent[ucCnt].u4PeriodicStartTime
		, prP2pRadarInfo->arPpbContent[ucCnt].u2PeriodicPulseWidth);
	for (ucCnt = 1; ucCnt < prP2pRadarInfo->ucPPBNum; ucCnt++) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n%-10d    %-11d    %d\n"
			, prP2pRadarInfo->arPpbContent[ucCnt].u4PeriodicStartTime
			, prP2pRadarInfo->arPpbContent[ucCnt].u2PeriodicPulseWidth
			, (prP2pRadarInfo->arPpbContent[ucCnt].u4PeriodicStartTime
				- prP2pRadarInfo->arPpbContent[ucCnt-1].u4PeriodicStartTime) * 2 / 5);
	}
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nPRI Count M1 TH: %d; PRI Count M1: %d",
			prP2pRadarInfo->ucPRICountM1TH, prP2pRadarInfo->ucPRICountM1);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nPRI Count M2 TH: %d; PRI Count M2: %d",
			prP2pRadarInfo->ucPRICountM2TH, prP2pRadarInfo->ucPRICountM2);


	cnmMemFree(prGlueInfo->prAdapter, prP2pRadarInfo);

	return	i4BytesWritten;
}

int priv_driver_show_dfs_help(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);


	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n--iwpriv wlanX driver \"show_dfs_state\"\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nINACTIVE: RDD disable or temporary RDD disable");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nCHECKING: During CAC time");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nACTIVE  : In-serive monitoring");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		"\nDETECTED: Has detected radar but hasn't moved to new channel\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n--iwpriv wlanX driver \"show_dfs_radar_param\"\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nShow the latest pulse information\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n--iwpriv wlanX driver \"show_dfs_cac_time\"\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nShow the remaining time of CAC\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n--iwpriv wlanX set ByPassCac=yy\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nValue yy: set the time of CAC\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n--iwpriv wlanX set RDDReport=yy\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nValue yy is \"0\" or \"1\"");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n\"0\": Emulate RDD0 manual radar event");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n\"1\": Emulate RDD1 manual radar event\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n--iwpriv wlanX set RadarDetectMode=yy\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nValue yy is \"0\" or \"1\"");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n\"0\": Switch channel when radar detected (default)");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n\"1\": Do not switch channel when radar detected");

	return	i4BytesWritten;
}

int priv_driver_show_dfs_cac_time(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (p2pFuncGetDfsState() != DFS_STATE_CHECKING) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nNot in CAC period");
		return i4BytesWritten;
	}

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nRemaining time of CAC: %dsec", p2pFuncGetCacRemainingTime());

	return	i4BytesWritten;
}

#endif
int priv_driver_set_miracast(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{

	P_ADAPTER_T prAdapter = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 i4BytesWritten = 0;
	/* WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS; */
	/* UINT_32 u4BufLen = 0; */
	INT_32 i4Argc = 0;
	UINT_32 ucMode = 0;
	P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;
	P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T prMsgWfdCfgUpdate = (P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T) NULL;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prAdapter = prGlueInfo->prAdapter;
	if (i4Argc >= 2) {
		/* ucMode = kalStrtoul(apcArgv[1], NULL, 0); */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &ucMode);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucMode error u4Ret=%d\n", u4Ret);

		if (g_ucMiracastMode == (UINT_8) ucMode) {
			/* XXX: continue or skip */
			/* XXX: continue or skip */
		}

		g_ucMiracastMode = (UINT_8) ucMode;
		prMsgWfdCfgUpdate = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_WFD_CONFIG_SETTINGS_CHANGED_T));

		if (prMsgWfdCfgUpdate != NULL) {

			prWfdCfgSettings = &(prAdapter->rWifiVar.rWfdConfigureSettings);
			prMsgWfdCfgUpdate->rMsgHdr.eMsgId = MID_MNY_P2P_WFD_CFG_UPDATE;
			prMsgWfdCfgUpdate->prWfdCfgSettings = prWfdCfgSettings;

			if (ucMode == MIRACAST_MODE_OFF) {
				prWfdCfgSettings->ucWfdEnable = 0;
				snprintf(pcCommand, i4TotalLen, CMD_SET_CHIP " mira 0");
			} else if (ucMode == MIRACAST_MODE_SOURCE) {
				prWfdCfgSettings->ucWfdEnable = 1;
				snprintf(pcCommand, i4TotalLen, CMD_SET_CHIP " mira 1");
			} else if (ucMode == MIRACAST_MODE_SINK) {
				prWfdCfgSettings->ucWfdEnable = 2;
				snprintf(pcCommand, i4TotalLen, CMD_SET_CHIP " mira 2");
			} else {
				prWfdCfgSettings->ucWfdEnable = 0;
				snprintf(pcCommand, i4TotalLen, CMD_SET_CHIP " mira 0");
			}

			mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgWfdCfgUpdate, MSG_SEND_METHOD_BUF);

			priv_driver_set_chip_config(prNetDev, pcCommand, i4TotalLen);
		} /* prMsgWfdCfgUpdate */
		else {
			ASSERT(FALSE);
			i4BytesWritten = -1;
		}
	}

	/* i4Argc */
	return i4BytesWritten;
}

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
/*
 * Memo
 * 00 : Reset All Cal Data in Driver
 * 01 : Trigger All Cal Function
 * 02 : Get Thermal Temp from FW
 * 03 : Get Cal Data Size from FW
 * 04 : Get Cal Data from FW (Rom)
 * 05 : Get Cal Data from FW (Ram)
 * 06 : Print Cal Data in Driver (Rom)
 * 07 : Print Cal Data in Driver (Ram)
 * 08 : Print Cal Data in FW (Rom)
 * 09 : Print Cal Data in FW (Ram)
 * 10 : Send Cal Data to FW (Rom)
 * 11 : Send Cal Data to FW (Ram)
 */
static int priv_driver_set_calbackup_test_drv_fw(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4Ret, u4GetInput;
	INT_32 i4ArgNum = 2;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "%s\r\n", __func__);

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4GetInput);
		if (u4Ret)
			DBGLOG(RFTEST, INFO, "priv_driver_set_calbackup_test_drv_fw Parsing Fail\n");

		if (u4GetInput == 0) {
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#0 : Reset All Cal Data in Driver.\n");
			/* (New Flow 20160720) Step 0 : Reset All Cal Data Structure */
			memset(&g_rBackupCalDataAllV2, 1, sizeof(RLM_CAL_RESULT_ALL_V2_T));
			g_rBackupCalDataAllV2.u4MagicNum1 = 6632;
			g_rBackupCalDataAllV2.u4MagicNum2 = 6632;
		} else if (u4GetInput == 1) {
			DBGLOG(RFTEST, INFO, "CMD#1 : Trigger FW Do All Cal.\n");
			/* Step 1 : Trigger All Cal Function */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 1, 2, 0);
			DBGLOG(RFTEST, INFO, "Trigger FW Do All Cal, rStatus = 0x%08x\n", rStatus);
		} else if (u4GetInput == 2) {
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#2 : Get Thermal Temp from FW.\n");
			/* (New Flow 20160720) Step 2 : Get Thermal Temp from FW */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 0, 0, 0);
			DBGLOG(RFTEST, INFO, "Get Thermal Temp from FW, rStatus = 0x%08x\n", rStatus);

		} else if (u4GetInput == 3) {
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#3 : Get Cal Data Size from FW.\n");
			/* (New Flow 20160720) Step 3 : Get Cal Data Size from FW */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 0, 1, 0);
			DBGLOG(RFTEST, INFO, "Get Rom Cal Data Size, rStatus = 0x%08x\n", rStatus);

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 0, 1, 1);
			DBGLOG(RFTEST, INFO, "Get Ram Cal Data Size, rStatus = 0x%08x\n", rStatus);

		} else if (u4GetInput == 4) {
#if 1
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#4 : Print Cal Data in FW (Ram) (Part 1 - [0]~[3327]).\n");
			/* Debug Use : Print Cal Data in FW (Ram) */
			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 4, 6, 1);
			DBGLOG(RFTEST, INFO, "Print Cal Data in FW (Ram), rStatus = 0x%08x\n", rStatus);
#else		/* For Temp Use this Index */
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#4 : Get Cal Data from FW (Rom). Start!!!!!!!!!!!\n");
			DBGLOG(RFTEST, INFO, "Thermal Temp = %d\n", g_rBackupCalDataAllV2.u4ThermalInfo);
			DBGLOG(RFTEST, INFO, "Total Length (Rom) = %d\n",
				g_rBackupCalDataAllV2.u4ValidRomCalDataLength);
			/* (New Flow 20160720) Step 3 : Get Cal Data from FW */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 2, 4, 0);
			DBGLOG(RFTEST, INFO, "Get Cal Data from FW (Rom), rStatus = 0x%08x\n", rStatus);
#endif
		} else if (u4GetInput == 5) {
#if 1
			DBGLOG(RFTEST, INFO,
				"(New Flow) CMD#5 : Print RAM Cal Data in Driver (Part 1 - [0]~[3327]).\n");
			DBGLOG(RFTEST, INFO, "==================================================================\n");
			/* RFTEST_INFO_LOGDUMP32(&(g_rBackupCalDataAllV2.au4RamCalData[0]), 3328*sizeof(UINT_32)); */
			DBGLOG(RFTEST, INFO, "==================================================================\n");
			DBGLOG(RFTEST, INFO, "Dumped Ram Cal Data Szie : %d bytes\n", 3328*sizeof(UINT_32));
			DBGLOG(RFTEST, INFO, "Total Ram Cal Data Szie : %d bytes\n",
				g_rBackupCalDataAllV2.u4ValidRamCalDataLength);
			DBGLOG(RFTEST, INFO, "==================================================================\n");
#else		/* For Temp Use this Index */
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#5 : Get Cal Data from FW (Ram). Start!!!!!!!!!!!\n");
			DBGLOG(RFTEST, INFO, "Thermal Temp = %d\n", g_rBackupCalDataAllV2.u4ThermalInfo);
			DBGLOG(RFTEST, INFO, "Total Length (Ram) = %d\n",
				g_rBackupCalDataAllV2.u4ValidRamCalDataLength);
			/* (New Flow 20160720) Step 3 : Get Cal Data from FW */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 2, 4, 1);
			DBGLOG(RFTEST, INFO, "Get Cal Data from FW (Ram), rStatus = 0x%08x\n", rStatus);
#endif
		} else if (u4GetInput == 6) {
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#6 : Print ROM Cal Data in Driver.\n");
			DBGLOG(RFTEST, INFO, "==================================================================\n");
			/* RFTEST_INFO_LOGDUMP32(&(g_rBackupCalDataAllV2.au4RomCalData[0]), */
			/* g_rBackupCalDataAllV2.u4ValidRomCalDataLength); */
			DBGLOG(RFTEST, INFO, "==================================================================\n");
			DBGLOG(RFTEST, INFO, "Total Rom Cal Data Szie : %d bytes\n",
				g_rBackupCalDataAllV2.u4ValidRomCalDataLength);
			DBGLOG(RFTEST, INFO, "==================================================================\n");
		} else if (u4GetInput == 7) {
			DBGLOG(RFTEST, INFO,
				"(New Flow) CMD#7 : Print RAM Cal Data in Driver (Part 2 - [3328]~[6662]).\n");
			DBGLOG(RFTEST, INFO, "==================================================================\n");
			/* RFTEST_INFO_LOGDUMP32(&(g_rBackupCalDataAllV2.au4RamCalData[3328]), */
			/*(g_rBackupCalDataAllV2.u4ValidRamCalDataLength - 3328*sizeof(UINT_32))); */
			DBGLOG(RFTEST, INFO, "==================================================================\n");
			DBGLOG(RFTEST, INFO, "Dumped Ram Cal Data Szie : %d bytes\n",
				(g_rBackupCalDataAllV2.u4ValidRamCalDataLength - 3328*sizeof(UINT_32)));
			DBGLOG(RFTEST, INFO, "Total Ram Cal Data Szie : %d bytes\n",
				g_rBackupCalDataAllV2.u4ValidRamCalDataLength);
			DBGLOG(RFTEST, INFO, "==================================================================\n");
		} else if (u4GetInput == 8) {
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#8 : Print Cal Data in FW (Rom).\n");
			/* Debug Use : Print Cal Data in FW (Rom) */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 4, 6, 0);
			DBGLOG(RFTEST, INFO, "Print Cal Data in FW (Rom), rStatus = 0x%08x\n", rStatus);

		} else if (u4GetInput == 9) {
			DBGLOG(RFTEST, INFO,
				"(New Flow) CMD#9 : Print Cal Data in FW (Ram) (Part 2 - [3328]~[6662]).\n");
			/* Debug Use : Print Cal Data in FW (Ram) */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 4, 6, 2);
			DBGLOG(RFTEST, INFO, "Print Cal Data in FW (Ram), rStatus = 0x%08x\n", rStatus);

		} else if (u4GetInput == 10) {
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#10 : Send Cal Data to FW (Rom).\n");
			/* Send Cal Data to FW (Rom) */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 3, 5, 0);
			DBGLOG(RFTEST, INFO, "Send Cal Data to FW (Rom), rStatus = 0x%08x\n", rStatus);

		} else if (u4GetInput == 11) {
			DBGLOG(RFTEST, INFO, "(New Flow) CMD#11 : Send Cal Data to FW (Ram).\n");
			/* Send Cal Data to FW (Ram) */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 3, 5, 1);
			DBGLOG(RFTEST, INFO, "Send Cal Data to FW (Ram), rStatus = 0x%08x\n", rStatus);

		}
	}

	return i4BytesWritten;
}				/* priv_driver_set_calbackup_test_drv_fw */
#endif

#if CFG_WOW_SUPPORT
static int priv_driver_set_wow(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_WOW_CTRL_T pWOW_CTRL = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Ret = 0;
	UINT_32 Enable = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	u4Ret = kalkStrtou32(apcArgv[1], 0, &Enable);

	if (u4Ret)
		DBGLOG(REQ, LOUD, "parse bEnable error u4Ret=%d\n", u4Ret);

	DBGLOG(INIT, INFO, "CMD set_wow_enable = %d\n", Enable);
	DBGLOG(INIT, INFO, "Scenario ID %d\n", pWOW_CTRL->ucScenarioId);
	DBGLOG(INIT, INFO, "ucBlockCount %d\n", pWOW_CTRL->ucBlockCount);
	DBGLOG(INIT, INFO, "interface %d\n", pWOW_CTRL->astWakeHif[0].ucWakeupHif);
	DBGLOG(INIT, INFO, "gpio_pin %d\n", pWOW_CTRL->astWakeHif[0].ucGpioPin);
	DBGLOG(INIT, INFO, "gpio_level 0x%x\n", pWOW_CTRL->astWakeHif[0].ucTriggerLvl);
	DBGLOG(INIT, INFO, "gpio_timer %d\n", pWOW_CTRL->astWakeHif[0].u4GpioInterval);
	kalWowProcess(prGlueInfo, Enable);

#if defined(_HIF_USB)
	if (Enable)
		glUsbSetState(&prGlueInfo->rHifInfo, USB_STATE_SUSPEND);
	else
		glUsbSetState(&prGlueInfo->rHifInfo, USB_STATE_LINK_UP);
#endif

	return 0;
}

static int priv_driver_set_wow_enable(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_WOW_CTRL_T pWOW_CTRL = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Ret = 0;
	UINT_8 ucEnable = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	u4Ret = kalkStrtou8(apcArgv[1], 0, &ucEnable);

	if (u4Ret)
		DBGLOG(REQ, LOUD, "parse bEnable error u4Ret=%d\n", u4Ret);

	pWOW_CTRL->fgWowEnable = ucEnable;

	DBGLOG(PF, INFO, "WOW enable %d\n", pWOW_CTRL->fgWowEnable);

	return 0;
}

static int priv_driver_set_wow_par(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_WOW_CTRL_T pWOW_CTRL = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret = 0;
	UINT_8	ucWakeupHif = 0, GpioPin = 0, ucGpioLevel = 0, ucBlockCount, ucScenario = 0;
	UINT_32 u4GpioTimer = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc > 3) {

		u4Ret = kalkStrtou8(apcArgv[1], 0, &ucWakeupHif);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n", u4Ret);
		pWOW_CTRL->astWakeHif[0].ucWakeupHif = ucWakeupHif;

		u4Ret = kalkStrtou8(apcArgv[2], 0, &GpioPin);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse GpioPin error u4Ret=%d\n", u4Ret);
		pWOW_CTRL->astWakeHif[0].ucGpioPin = GpioPin;

		u4Ret = kalkStrtou8(apcArgv[3], 0, &ucGpioLevel);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse Gpio level error u4Ret=%d\n", u4Ret);
		pWOW_CTRL->astWakeHif[0].ucTriggerLvl = ucGpioLevel;

		u4Ret = kalkStrtou32(apcArgv[4], 0, &u4GpioTimer);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse u4GpioTimer error u4Ret=%d\n", u4Ret);
		pWOW_CTRL->astWakeHif[0].u4GpioInterval = u4GpioTimer;

		u4Ret = kalkStrtou8(apcArgv[5], 0, &ucScenario);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucScenario error u4Ret=%d\n", u4Ret);
		pWOW_CTRL->ucScenarioId = ucScenario;

		u4Ret = kalkStrtou8(apcArgv[6], 0, &ucBlockCount);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucBlockCnt error u4Ret=%d\n", u4Ret);
		pWOW_CTRL->ucBlockCount = ucBlockCount;

		DBGLOG(INIT, INFO, "gpio_scenario%d\n", pWOW_CTRL->ucScenarioId);
		DBGLOG(INIT, INFO, "interface %d\n", pWOW_CTRL->astWakeHif[0].ucWakeupHif);
		DBGLOG(INIT, INFO, "gpio_pin %d\n", pWOW_CTRL->astWakeHif[0].ucGpioPin);
		DBGLOG(INIT, INFO, "gpio_level %d\n", pWOW_CTRL->astWakeHif[0].ucTriggerLvl);
		DBGLOG(INIT, INFO, "gpio_timer %d\n", pWOW_CTRL->astWakeHif[0].u4GpioInterval);

		return 0;
	} else
		return -1;


}

static int priv_driver_set_wow_udpport(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_WOW_CTRL_T pWOW_CTRL = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcPortArgv[WLAN_CFG_ARGV_MAX_LONG] = { 0 }; /* to input 20 port */
	INT_32 u4Ret = 0, ii;
	UINT_8	ucVer, ucCount;
	UINT_16 u2Port = 0;
	PUINT_16 pausPortArry;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgumentLong(pcCommand, &i4Argc, apcPortArgv);
	DBGLOG(REQ, WARN, "argc is %i\n", i4Argc);

	/* example: set_wow_udp 0 5353,8080 (set) */
	/* example: set_wow_udp 1 (clear) */

	if (i4Argc >= 3) {

		/* Pick Max */
		ucCount = ((i4Argc - 2) > MAX_TCP_UDP_PORT) ? MAX_TCP_UDP_PORT : (i4Argc - 2);
		DBGLOG(PF, INFO, "UDP ucCount=%d\n", ucCount);

		u4Ret = kalkStrtou8(apcPortArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n", u4Ret);
			return -1;
		}

		/* IPv4/IPv6 */
		DBGLOG(PF, INFO, "ucVer=%d\n", ucVer);
		if (ucVer == 0) {
			pWOW_CTRL->stWowPort.ucIPv4UdpPortCnt = ucCount;
			pausPortArry = pWOW_CTRL->stWowPort.ausIPv4UdpPort;
		} else {
			pWOW_CTRL->stWowPort.ucIPv6UdpPortCnt = ucCount;
			pausPortArry = pWOW_CTRL->stWowPort.ausIPv6UdpPort;
		}

		/* Port */
		for (ii = 0; ii < ucCount; ii++) {
			u4Ret = kalkStrtou16(apcPortArgv[ii+2], 0, &u2Port);
			if (u4Ret) {
				DBGLOG(PF, ERROR, "parse u2Port error u4Ret=%d\n", u4Ret);
				return -1;
			}

			pausPortArry[ii] = u2Port;
			DBGLOG(PF, INFO, "ucPort=%d, idx=%d\n", u2Port, ii);
		}

		return 0;
	} else if (i4Argc == 2) {

		u4Ret = kalkStrtou8(apcPortArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n", u4Ret);
			return -1;
		}

		if (ucVer == 0) {
			kalMemZero(prGlueInfo->prAdapter->rWowCtrl.stWowPort.ausIPv4UdpPort,
				sizeof(UINT_16) * MAX_TCP_UDP_PORT);
			prGlueInfo->prAdapter->rWowCtrl.stWowPort.ucIPv4UdpPortCnt = 0;
		} else {
			kalMemZero(prGlueInfo->prAdapter->rWowCtrl.stWowPort.ausIPv6UdpPort,
				sizeof(UINT_16) * MAX_TCP_UDP_PORT);
			prGlueInfo->prAdapter->rWowCtrl.stWowPort.ucIPv6UdpPortCnt = 0;
		}

		return 0;
	} else
		return -1;

}

static int priv_driver_set_wow_tcpport(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_WOW_CTRL_T pWOW_CTRL = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcPortArgv[WLAN_CFG_ARGV_MAX_LONG] = { 0 }; /* to input 20 port */
	INT_32 u4Ret = 0, ii;
	UINT_8	ucVer, ucCount;
	UINT_16 u2Port = 0;
	PUINT_16 pausPortArry;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgumentLong(pcCommand, &i4Argc, apcPortArgv);
	DBGLOG(REQ, WARN, "argc is %i\n", i4Argc);

	/* example: set_wow_tcp 0 5353,8080 (Set) */
	/* example: set_wow_tcp 1 (clear) */

	if (i4Argc >= 3) {

		/* Pick Max */
		ucCount = ((i4Argc - 2) > MAX_TCP_UDP_PORT) ? MAX_TCP_UDP_PORT : (i4Argc - 2);
		DBGLOG(PF, INFO, "TCP ucCount=%d\n", ucCount);

		u4Ret = kalkStrtou8(apcPortArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n", u4Ret);
			return -1;
		}

		/* IPv4/IPv6 */
		DBGLOG(PF, INFO, "Ver=%d\n", ucVer);
		if (ucVer == 0) {
			pWOW_CTRL->stWowPort.ucIPv4TcpPortCnt = ucCount;
			pausPortArry = pWOW_CTRL->stWowPort.ausIPv4TcpPort;
		} else {
			pWOW_CTRL->stWowPort.ucIPv6TcpPortCnt = ucCount;
			pausPortArry = pWOW_CTRL->stWowPort.ausIPv6TcpPort;
		}

		/* Port */
		for (ii = 0; ii < ucCount; ii++) {
			u4Ret = kalkStrtou16(apcPortArgv[ii+2], 0, &u2Port);
			if (u4Ret) {
				DBGLOG(PF, ERROR, "parse u2Port error u4Ret=%d\n", u4Ret);
				return -1;
			}

			pausPortArry[ii] = u2Port;
			DBGLOG(PF, INFO, "ucPort=%d, idx=%d\n", u2Port, ii);
		}

		return 0;
	} else if (i4Argc == 2) {

		u4Ret = kalkStrtou8(apcPortArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n", u4Ret);
			return -1;
		}

		if (ucVer == 0) {
			kalMemZero(prGlueInfo->prAdapter->
				rWowCtrl.stWowPort.ausIPv4TcpPort,
					sizeof(UINT_16) * MAX_TCP_UDP_PORT);
			prGlueInfo->prAdapter->
				rWowCtrl.stWowPort.ucIPv4TcpPortCnt = 0;
		} else {
			kalMemZero(prGlueInfo->prAdapter->
				rWowCtrl.stWowPort.ausIPv6TcpPort,
					sizeof(UINT_16) * MAX_TCP_UDP_PORT);
			prGlueInfo->prAdapter->
				rWowCtrl.stWowPort.ucIPv6TcpPortCnt = 0;
		}

		return 0;
	} else
		return -1;

}

static int priv_driver_get_wow_port(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_WOW_CTRL_T pWOW_CTRL = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret = 0, ii;
	UINT_8	ucVer, ucProto;
	UINT_16 ucCount;
	PUINT_16 pausPortArry;
	PCHAR aucIp[2] = {"IPv4", "IPv6"};
	PCHAR aucProto[2] = {"UDP", "TCP"};

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	/* example: get_wow_port 0 0 (ipv4-udp) */
	/* example: get_wow_port 0 1 (ipv4-tcp) */
	/* example: get_wow_port 1 0 (ipv6-udp) */
	/* example: get_wow_port 1 1 (ipv6-tcp) */

	if (i4Argc >= 3) {

		/* 0=IPv4, 1=IPv6 */
		u4Ret = kalkStrtou8(apcArgv[1], 0, &ucVer);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse argc[1] error u4Ret=%d\n", u4Ret);

		/* 0=UDP, 1=TCP */
		u4Ret = kalkStrtou8(apcArgv[2], 0, &ucProto);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse argc[2] error u4Ret=%d\n", u4Ret);

		if (ucVer > 1)
			ucVer = 0;

		if (ucProto > 1)
			ucProto = 0;

		if (ucVer == 0) {
			if (ucProto == 0) {
				/* IPv4/UDP */
				ucCount = pWOW_CTRL->stWowPort.ucIPv4UdpPortCnt;
				pausPortArry = pWOW_CTRL->stWowPort.ausIPv4UdpPort;
			} else {
				/* IPv4/TCP */
				ucCount = pWOW_CTRL->stWowPort.ucIPv4TcpPortCnt;
				pausPortArry = pWOW_CTRL->stWowPort.ausIPv4TcpPort;
			}
		} else {
			if (ucProto == 0) {
				/* IPv6/UDP */
				ucCount = pWOW_CTRL->stWowPort.ucIPv6UdpPortCnt;
				pausPortArry = pWOW_CTRL->stWowPort.ausIPv6UdpPort;
			} else {
				/* IPv6/TCP */
				ucCount = pWOW_CTRL->stWowPort.ucIPv6TcpPortCnt;
				pausPortArry = pWOW_CTRL->stWowPort.ausIPv6TcpPort;
			}
		}

		/* Dunp Port */
		for (ii = 0; ii < ucCount; ii++)
			DBGLOG(PF, INFO, "ucPort=%d, idx=%d\n", pausPortArry[ii], ii);


		DBGLOG(PF, INFO, "[%s/%s] count:%d\n", aucIp[ucVer], aucProto[ucProto], ucCount);

		return 0;
	} else
		return -1;

}

static int priv_driver_get_wow_reason(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Argc = 0;
	INT_32 i4BytesWritten = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	P_WOW_CTRL_T pWOW_CTRL = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (pWOW_CTRL->ucReason != INVALID_WOW_WAKE_UP_REASON)
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nwakeup_reason:%d", pWOW_CTRL->ucReason);

	return	i4BytesWritten;
}
#endif

static int priv_driver_set_adv_pws(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Ret = 0;
	UINT_8 ucAdvPws = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	u4Ret = kalkStrtou8(apcArgv[1], 0, &ucAdvPws);

	if (u4Ret)
		DBGLOG(REQ, LOUD, "parse bEnable error u4Ret=%d\n", u4Ret);

	prGlueInfo->prAdapter->rWifiVar.ucAdvPws = ucAdvPws;

	DBGLOG(INIT, INFO, "AdvPws:%d\n",
	       prGlueInfo->prAdapter->rWifiVar.ucAdvPws);

	return 0;

}

static int priv_driver_set_mdtim(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Ret = 0;
	UINT_8 ucMultiDtim = 0, ucVer;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	/* iwpriv wlan0 driver "set_mdtim 1 3 */
	if (i4Argc >= 3) {

		u4Ret = kalkStrtou8(apcArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, ERROR, "parse apcArgv1 error u4Ret=%d\n", u4Ret);
			return -1;
		}

		u4Ret = kalkStrtou8(apcArgv[2], 0, &ucMultiDtim);
		if (u4Ret) {
			DBGLOG(REQ, ERROR, "parse apcArgv2 error u4Ret=%d\n", u4Ret);
			return -1;
		}

		if (ucVer == 0) {
			prGlueInfo->prAdapter->rWifiVar.ucWowOnMdtim = ucMultiDtim;
			DBGLOG(REQ, INFO, "WOW On MDTIM:%d\n",
			       prGlueInfo->prAdapter->rWifiVar.ucWowOnMdtim);
		} else {
			prGlueInfo->prAdapter->rWifiVar.ucWowOffMdtim = ucMultiDtim;
			DBGLOG(REQ, INFO, "WOW Off MDTIM:%d\n",
			       prGlueInfo->prAdapter->rWifiVar.ucWowOffMdtim);
		}
	}

	return 0;

}

static int priv_driver_set_listen_dtim_interval(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Ret = 0;
	UINT_8 ucInterval = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	/* iwpriv wlan0 driver "set_listen_dtim_interval x */
	if (i4Argc >= 2) {

		u4Ret = kalkStrtou8(apcArgv[1], 0, &ucInterval);
		if (u4Ret) {
			DBGLOG(REQ, ERROR, "parse apcArgv1 error u4Ret=%d\n", u4Ret);
			return -1;
		}

		prGlueInfo->prAdapter->rWifiVar.ucListenDtimInterval = ucInterval;
		DBGLOG(REQ, INFO, "Listen Interval(DTIM) :%d\n",
		       prGlueInfo->prAdapter->rWifiVar.ucListenDtimInterval);
	}

	return 0;
}

int priv_driver_set_suspend_mode(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	BOOLEAN fgEnable;
	UINT_32 u4Enable;
	INT_32 u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		/* fgEnable = (kalStrtoul(apcArgv[1], NULL, 0) == 1) ? TRUE : FALSE; */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Enable);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse u4Enable error u4Ret=%d\n", u4Ret);
		if (u4Enable == 1)
			fgEnable = TRUE;
		else
			fgEnable = FALSE;

		DBGLOG(REQ, INFO, "%s: Set suspend mode [%u]\n", __func__, fgEnable);

		if (prGlueInfo->fgIsInSuspendMode == fgEnable) {
			DBGLOG(REQ, INFO, "%s: Already in suspend mode, SKIP!\n", __func__);
			return 0;
		}

		prGlueInfo->fgIsInSuspendMode = fgEnable;

		wlanSetSuspendMode(prGlueInfo, fgEnable);
		p2pSetSuspendMode(prGlueInfo, fgEnable);
	}

	return 0;
}

#if CFG_SUPPORT_SNIFFER
int priv_driver_set_monitor(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	INT_32 i4BytesWritten = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	PARAM_CUSTOM_MONITOR_SET_STRUCT_T rMonitorSetInfo;
	UINT_8 ucEnable = 0;
	UINT_8 ucPriChannel = 0;
	UINT_8 ucChannelWidth = 0;
	UINT_8 ucExt = 0;
	UINT_8 ucSco = 0;
	UINT_8 ucChannelS1 = 0;
	UINT_8 ucChannelS2 = 0;
	BOOLEAN fgIsLegalChannel = FALSE;
	BOOLEAN fgError = FALSE;
	BOOLEAN fgEnable = FALSE;
	ENUM_BAND_T eBand = BAND_NULL;
	UINT_32 u4Parse = 0;
	INT_32 u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 5) {
		/* ucEnable = (UINT_8) (kalStrtoul(apcArgv[1], NULL, 0));
		 *  ucPriChannel = (UINT_8) (kalStrtoul(apcArgv[2], NULL, 0));
		 *  ucChannelWidth = (UINT_8) (kalStrtoul(apcArgv[3], NULL, 0));
		 *  ucExt = (UINT_8) (kalStrtoul(apcArgv[4], NULL, 0));
		 */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n", u4Ret);
		ucEnable = (UINT_8) u4Parse;
		u4Ret = kalkStrtou32(apcArgv[2], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n", u4Ret);
		ucPriChannel = (UINT_8) u4Parse;
		u4Ret = kalkStrtou32(apcArgv[3], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n", u4Ret);
		ucChannelWidth = (UINT_8) u4Parse;
		u4Ret = kalkStrtou32(apcArgv[4], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n", u4Ret);
		ucExt = (UINT_8) u4Parse;

		eBand = (ucPriChannel <= 14) ? BAND_2G4 : BAND_5G;
		fgIsLegalChannel = rlmDomainIsLegalChannel(prAdapter, eBand, ucPriChannel);

		if (fgIsLegalChannel == FALSE) {
			i4BytesWritten = snprintf(pcCommand, i4TotalLen, "Illegal primary channel %d", ucPriChannel);
			return i4BytesWritten;
		}

		switch (ucChannelWidth) {
		case 160:
			ucChannelWidth = (UINT_8) CW_160MHZ;
			ucSco = (UINT_8) CHNL_EXT_SCN;

			if (ucPriChannel >= 36 && ucPriChannel <= 64)
				ucChannelS2 = 50;
			else if (ucPriChannel >= 100 && ucPriChannel <= 128)
				ucChannelS2 = 114;
			else
				fgError = TRUE;
			break;

		case 80:
			ucChannelWidth = (UINT_8) CW_80MHZ;
			ucSco = (UINT_8) CHNL_EXT_SCN;

			if (ucPriChannel >= 36 && ucPriChannel <= 48)
				ucChannelS1 = 42;
			else if (ucPriChannel >= 52 && ucPriChannel <= 64)
				ucChannelS1 = 58;
			else if (ucPriChannel >= 100 && ucPriChannel <= 112)
				ucChannelS1 = 106;
			else if (ucPriChannel >= 116 && ucPriChannel <= 128)
				ucChannelS1 = 122;
			else if (ucPriChannel >= 132 && ucPriChannel <= 144)
				ucChannelS1 = 138;
			else if (ucPriChannel >= 149 && ucPriChannel <= 161)
				ucChannelS1 = 155;
			else
				fgError = TRUE;
			break;

		case 40:
			ucChannelWidth = (UINT_8) CW_20_40MHZ;
			ucSco = (ucExt) ? (UINT_8) CHNL_EXT_SCA : (UINT_8) CHNL_EXT_SCB;
			break;

		case 20:
			ucChannelWidth = (UINT_8) CW_20_40MHZ;
			ucSco = (UINT_8) CHNL_EXT_SCN;
			break;

		default:
			fgError = TRUE;
			break;
		}

		if (fgError) {
			i4BytesWritten =
			    snprintf(pcCommand, i4TotalLen, "Invalid primary channel %d with bandwidth %d",
				     ucPriChannel, ucChannelWidth);
			return i4BytesWritten;
		}

		fgEnable = (ucEnable) ? TRUE : FALSE;

		if (prGlueInfo->fgIsEnableMon != fgEnable) {
			prGlueInfo->fgIsEnableMon = fgEnable;
			schedule_work(&prGlueInfo->monWork);
		}

		kalMemZero(&rMonitorSetInfo, sizeof(rMonitorSetInfo));

		rMonitorSetInfo.ucEnable = ucEnable;
		rMonitorSetInfo.ucPriChannel = ucPriChannel;
		rMonitorSetInfo.ucSco = ucSco;
		rMonitorSetInfo.ucChannelWidth = ucChannelWidth;
		rMonitorSetInfo.ucChannelS1 = ucChannelS1;
		rMonitorSetInfo.ucChannelS2 = ucChannelS2;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetMonitor,
				   &rMonitorSetInfo, sizeof(rMonitorSetInfo), FALSE, FALSE, TRUE, &u4BufLen);

		i4BytesWritten =
		    snprintf(pcCommand, i4TotalLen, "set monitor config %s",
			     (rStatus == WLAN_STATUS_SUCCESS) ? "success" : "fail");

		return i4BytesWritten;
	}

	i4BytesWritten = snprintf(pcCommand, i4TotalLen, "monitor [Enable][PriChannel][ChannelWidth][Sco]");

	return i4BytesWritten;
}
#endif

static int priv_driver_get_version(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter;
	INT_32 i4BytesWritten = 0;
	UINT_32 u4Offset = 0;
	P_WIFI_VER_INFO_T prVerInfo;
	tailer_format_t *prTailer;
	UINT_8 aucBuf[32], aucDate[32];

	ASSERT(prNetDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	prVerInfo = &prAdapter->rVerInfo;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
	"\nChip ROM ver [%u]\n", prAdapter->chip_info->eco_ver);

	wlanPrintVersion(prAdapter);

	kalStrnCpy(aucBuf, prVerInfo->aucFwBranchInfo, 4);
	aucBuf[4] = '\0';
	kalStrnCpy(aucDate, prVerInfo->aucFwDateCode, 16);
	aucDate[16] = '\0';
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
	"\nN9 FW version %s-%u.%u.%u[DEC] (%s)\n",
	aucBuf, (UINT_32)(prVerInfo->u2FwOwnVersion >> 8),
	(UINT_32)(prVerInfo->u2FwOwnVersion & BITS(0, 7)),
	prVerInfo->ucFwBuildNumber, aucDate);
#if CFG_SUPPORT_COMPRESSION_FW_OPTION
	if (prVerInfo->fgIsN9CompressedFW) {
		tailer_format_t_2 *prTailer;

		prTailer = &prVerInfo->rN9Compressedtailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"N9  tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);
	} else {
		prTailer = &prVerInfo->rN9tailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"N9  tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);
	}
	if (prVerInfo->fgIsCR4CompressedFW) {
		tailer_format_t_2 *prTailer;

		prTailer = &prVerInfo->rCR4Compressedtailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"CR4 tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);
	} else {
		prTailer = &prVerInfo->rCR4tailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"CR4 tailer version %s (%s) info %u:E%u\n",
		aucBuf, prTailer->ram_built_date, prTailer->chip_info,
		prTailer->eco_code + 1);
	}
#else
		prTailer = &prVerInfo->rN9tailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		kalMemCopy(aucDate, prTailer->ram_built_date, sizeof(prTailer->ram_built_date));
		aucDate[sizeof(prTailer->ram_built_date)] = '\0';
		u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"N9  tailer version %s (%s) info %u:E%u\n",
		aucBuf, aucDate, prTailer->chip_info,
		prTailer->eco_code + 1);

		prTailer = &prVerInfo->rCR4tailer;
		kalMemCopy(aucBuf, prTailer->ram_version, 10);
		aucBuf[10] = '\0';
		kalMemCopy(aucDate, prTailer->ram_built_date, sizeof(prTailer->ram_built_date));
		aucDate[sizeof(prTailer->ram_built_date)] = '\0';
		u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"CR4 tailer version %s (%s) info %u:E%u\n",
		aucBuf, aucDate, prTailer->chip_info,
		prTailer->eco_code + 1);
#endif
	if (!prVerInfo->fgPatchIsDlByDrv) {
		u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
			"Patch is not downloaded by driver, read patch binary\n");
		wlanGetPatchInfo(prAdapter);
	}

	kalStrnCpy(aucBuf, prVerInfo->rPatchHeader.aucPlatform, 4);
	aucBuf[4] = '\0';
	kalStrnCpy(aucDate, prVerInfo->rPatchHeader.aucBuildDate, 16);
	aucDate[16] = '\0';
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"Patch platform %s version 0x%04X %s\n",
		aucBuf, prVerInfo->rPatchHeader.u4PatchVersion, aucDate);

#if 0
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"Drv version %u.%u[DEC]", (prVerInfo->u2FwPeerVersion >> 8),
		(prVerInfo->u2FwPeerVersion & BITS(0, 7)));
#endif

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"WiFi Driver Version " NIC_DRIVER_VERSION_STRING "\n");

	i4BytesWritten = (INT_32)u4Offset;

	return i4BytesWritten;
}

#if CFG_SUPPORT_DBDC
int priv_driver_set_dbdc(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = {0};

	UINT_32 u4Ret, u4Parse;

	UINT_8 ucDBDCEnable;
	/*UINT_8 ucBssIndex;*/
	/*P_BSS_INFO_T prBssInfo;*/


	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
#if 0
	for (ucBssIndex = 0; ucBssIndex < (HW_BSSID_NUM+1); ucBssIndex++) {
		prBssInfo = prGlueInfo->prAdapter->aprBssInfo[ucBssIndex];
		pr_info("****BSS %u inUse %u active %u Mode %u priCh %u state %u rfBand %u\n",
			ucBssIndex,
			prBssInfo->fgIsInUse,
			prBssInfo->fgIsNetActive,
			prBssInfo->eCurrentOPMode,
			prBssInfo->ucPrimaryChannel,
			prBssInfo->eConnectionState,
			prBssInfo->eBand);
	}
#endif
	if (prGlueInfo->prAdapter->rWifiVar.ucDbdcMode != DBDC_MODE_DYNAMIC) {
		DBGLOG(REQ, LOUD, "Current DBDC mode %u cannot enable/disable DBDC!!\n",
			prGlueInfo->prAdapter->rWifiVar.ucDbdcMode);
		return -1;
	}

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n", u4Ret);

		ucDBDCEnable = (UINT_8) u4Parse;
		if ((!prGlueInfo->prAdapter->rWifiVar.fgDbDcModeEn && !ucDBDCEnable) ||
			(prGlueInfo->prAdapter->rWifiVar.fgDbDcModeEn && ucDBDCEnable))
			return i4BytesWritten;

		rStatus = kalIoctl(prGlueInfo,
						wlanoidSetDbdcEnable,
						&ucDBDCEnable, 1,
						FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver SET_DBDC <enable>\n");
		DBGLOG(INIT, ERROR, "<enable> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}
#endif /*CFG_SUPPORT_DBDC*/

#if CFG_SUPPORT_BATCH_SCAN
#define CMD_BATCH_SET           "WLS_BATCHING SET"
#define CMD_BATCH_GET           "WLS_BATCHING GET"
#define CMD_BATCH_STOP          "WLS_BATCHING STOP"
#endif

static int priv_driver_get_que_info(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	return qmDumpQueueStatus(prGlueInfo->prAdapter, pcCommand, i4TotalLen);
}

static int priv_driver_get_mem_info(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	return cnmDumpMemoryStatus(prGlueInfo->prAdapter, pcCommand, i4TotalLen);
}

static int priv_driver_get_hif_info(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	return halDumpHifStatus(prGlueInfo->prAdapter, pcCommand, i4TotalLen);
}

int priv_driver_set_p2p_ps(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	UINT_32 u4Ret;
	INT_32 i4Parameter;
	PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T rOpppsParamInfo;
	UINT_8 ucRoleIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 3) {
		/* get Bss Index from ndev */
		if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
			return -1;
		if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &rOpppsParamInfo.ucBssIdx) !=
			WLAN_STATUS_SUCCESS)
			return -1;

		DBGLOG(REQ, LOUD, "priv_driver_set_p2p_ps bss Idx %u\n", rOpppsParamInfo.ucBssIdx);

		u4Ret = kalkStrtos32(apcArgv[1], 0, &i4Parameter);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv[1] error u4Ret=%d\n", u4Ret);

		if (i4Parameter >= 1)
			rOpppsParamInfo.ucLegcyPS = 1;
		else
			rOpppsParamInfo.ucLegcyPS = 0;

		u4Ret = kalkStrtos32(apcArgv[2], 0, &i4Parameter);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv[2] error u4Ret=%d\n", u4Ret);

		if (i4Parameter >= 1)
			rOpppsParamInfo.ucOppPs = 1;
		else
			rOpppsParamInfo.ucOppPs = 0;

		u4Ret = kalkStrtos32(apcArgv[3], 0, &i4Parameter);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv[2] error u4Ret=%d\n", u4Ret);

		if (rOpppsParamInfo.ucOppPs)
			rOpppsParamInfo.u4CTwindowMs = (UINT_32)i4Parameter;

		rStatus = kalIoctl(prGlueInfo,
						wlanoidSetOppPsParam,
						&rOpppsParamInfo, sizeof(rOpppsParamInfo),
						FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	} else {
		DBGLOG(INIT, ERROR, "ERR, iwpriv wlanXX driver SET_P2P_PS <legacy ps> <Oppps> <CTW>\n");
	}

	return i4BytesWritten;
}

#if CFG_SUPPORT_LAST_SEC_MCS_INFO
static int priv_driver_get_mcs_info(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0, i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	P_PARAM_HW_WLAN_INFO_T prHwWlanInfo = NULL;
	struct PARAM_TX_MCS_INFO *prTxMcsInfo = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (prGlueInfo->prAdapter->rRxMcsInfoTimer.pfMgmtTimeOutFunc == NULL) {
		cnmTimerInitTimer(prGlueInfo->prAdapter,
			&prGlueInfo->prAdapter->rRxMcsInfoTimer,
			(PFN_MGMT_TIMEOUT_FUNC) aisRxMcsCollectionTimeout, (ULONG) NULL);
	}

	if (i4Argc >= 2) {
		if (strnicmp(apcArgv[1], "START", strlen("START")) == 0) {
			cnmTimerStartTimer(prGlueInfo->prAdapter, &prGlueInfo->prAdapter->rRxMcsInfoTimer, 100);
			prGlueInfo->prAdapter->fgIsMcsInfoValid = TRUE;

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"\nStart the MCS Info Function\n");
			return i4BytesWritten;
		} else if (strnicmp(apcArgv[1], "STOP", strlen("STOP")) == 0) {
			cnmTimerStopTimer(prGlueInfo->prAdapter, &prGlueInfo->prAdapter->rRxMcsInfoTimer);
			prGlueInfo->prAdapter->fgIsMcsInfoValid = FALSE;

			i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\nStop the MCS Info Function\n");
			return i4BytesWritten;
		}
	}

	if (prGlueInfo->prAdapter->fgIsMcsInfoValid != TRUE) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\nUse GET_MCS_INFO [START/STOP] to control the MCS Info Function\n");
		return i4BytesWritten;
	}

	if (prGlueInfo->prAdapter->prAisBssInfo->eConnectionState != PARAM_MEDIA_STATE_CONNECTED)
		return -1;

	prHwWlanInfo = (P_PARAM_HW_WLAN_INFO_T)kalMemAlloc(sizeof(PARAM_HW_WLAN_INFO_T), VIR_MEM_TYPE);
	if (!prHwWlanInfo)
		return -1;

	prHwWlanInfo->u4Index = prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP->ucWlanIndex;
	rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryWlanInfo,
				   prHwWlanInfo, sizeof(PARAM_HW_WLAN_INFO_T), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, INFO, "rStatus %u u4BufLen = %d\n", rStatus, u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		goto out;

	prTxMcsInfo = (struct PARAM_TX_MCS_INFO *)kalMemAlloc(sizeof(struct PARAM_TX_MCS_INFO), VIR_MEM_TYPE);
	if (!prTxMcsInfo)
		goto out;

	prTxMcsInfo->ucStaIndex = prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP->ucIndex;
	rStatus = kalIoctl(prGlueInfo,
				   wlanoidTxMcsInfo,
				   prTxMcsInfo, sizeof(struct PARAM_TX_MCS_INFO), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, INFO, "rStatus %u u4BufLen = %d\n", rStatus, u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		goto out;

	i4BytesWritten = priv_driver_last_sec_mcs_info(prGlueInfo->prAdapter,
							pcCommand, i4TotalLen, prHwWlanInfo, prTxMcsInfo);

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

out:
	if (prHwWlanInfo)
		kalMemFree(prHwWlanInfo, VIR_MEM_TYPE, sizeof(PARAM_HW_WLAN_INFO_T));
	if (prTxMcsInfo)
		kalMemFree(prTxMcsInfo, VIR_MEM_TYPE, sizeof(struct PARAM_TX_MCS_INFO));

	return i4BytesWritten;
}
#endif

static int priv_driver_get_deep_sleep_cnt(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	CNM_STATUS_T rCnmStatus;
	PUINT_32 pu4Ptr;
	UINT_32 u4Offset = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = 0xb0100000;

	rStatus = kalIoctl(prGlueInfo,
						 wlanoidQuerySwCtrlRead,
						 &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	pu4Ptr = (PUINT_32)&rCnmStatus;
	*pu4Ptr = rSwCtrlInfo.u4Data;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
							"Deep Sleep Cnt %d\n", rSwCtrlInfo.u4Data);

	i4BytesWritten = (INT_32)u4Offset;

	return i4BytesWritten;
}

static int priv_driver_get_cnm_info(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	CNM_STATUS_T rCnmStatus;
	PUINT_32 pu4Ptr;
	P_CNM_CH_LIST_T prChList;
	UINT_32 u4Offset = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = 0xb0000000;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	pu4Ptr = (PUINT_32)&rCnmStatus;
	*pu4Ptr = rSwCtrlInfo.u4Data;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"DBDC is %s, %u CHs in BAND0, %u CHs in BAND1\n",
				rCnmStatus.fgDbDcModeEn ? "ON" : "OFF", rCnmStatus.ucChNumB0, rCnmStatus.ucChNumB1);

	if (rCnmStatus.ucChNumB0 > 0) {
		rSwCtrlInfo.u4Id = 0xb0010000;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQuerySwCtrlRead,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		prChList = (P_CNM_CH_LIST_T)&rSwCtrlInfo.u4Data;

		u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
					"BAND0 channels : %u %u %u\n",
					prChList->ucChNum[0], prChList->ucChNum[1], prChList->ucChNum[2]);
	}
	if (rCnmStatus.ucChNumB1 > 0) {
		rSwCtrlInfo.u4Id = 0xb0010001;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQuerySwCtrlRead,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		prChList = (P_CNM_CH_LIST_T)&rSwCtrlInfo.u4Data;

		u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
					"BAND1 channels : %u %u %u\n",
					prChList->ucChNum[0], prChList->ucChNum[1], prChList->ucChNum[2]);
	}

	i4BytesWritten = (INT_32)u4Offset;

	return i4BytesWritten;

}				/* priv_driver_get_sw_ctrl */

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
static int priv_driver_get_ch_rank_list(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 i4BytesWritten = 0;
	INT_8 ucIdx = 0, ucIdx2 = 0, ucChannelNum = 0,
		ucNumOf2gChannel = 0, ucNumOf5gChannel = 0;
	P_PARAM_GET_CHN_INFO prChnLoadInfo = NULL;
	RF_CHANNEL_INFO_T *prChannelList = NULL,
		auc2gChannelList[MAX_2G_BAND_CHN_NUM],
		auc5gChannelList[MAX_5G_BAND_CHN_NUM];

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prChnLoadInfo = &(prGlueInfo->prAdapter->rWifiVar.rChnLoadInfo);
	kalMemZero(pcCommand, i4TotalLen);

	rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_2G4, TRUE,
			     MAX_2G_BAND_CHN_NUM, &ucNumOf2gChannel, auc2gChannelList);
	rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_5G, TRUE,
			     MAX_5G_BAND_CHN_NUM, &ucNumOf5gChannel, auc5gChannelList);

	for (ucIdx = 0; ucIdx < MAX_CHN_NUM; ucIdx++) {

		if (prChnLoadInfo->rChnRankList[ucIdx].ucChannel > 14) {
			prChannelList = auc5gChannelList;
			ucChannelNum = ucNumOf5gChannel;
		} else {
			prChannelList = auc2gChannelList;
			ucChannelNum = ucNumOf2gChannel;
		}

		for (ucIdx2 = 0; ucIdx2 < ucChannelNum; ucIdx2++) {
			if (prChnLoadInfo->rChnRankList[ucIdx].ucChannel ==
				prChannelList[ucIdx2].ucChannelNum) {
				pcCommand[i4BytesWritten++] =
					prChnLoadInfo->rChnRankList[ucIdx].ucChannel;
				DBGLOG(SCN, TRACE, "ch %u, dirtiness %d\n",
					prChnLoadInfo->rChnRankList[ucIdx].ucChannel,
					prChnLoadInfo->rChnRankList[ucIdx].u4Dirtiness);
				break;
			}
		}
	}

	return i4BytesWritten;
}

static int priv_driver_get_ch_dirtiness(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_8 cIdx = 0;
	UINT_8 ucNumOf2gChannel = 0;
	UINT_8 ucNumOf5gChannel = 0;
	UINT_32 i4BytesWritten = 0;
	P_PARAM_GET_CHN_INFO prChnLoadInfo = NULL;
	RF_CHANNEL_INFO_T ar2gChannelList[MAX_2G_BAND_CHN_NUM];
	RF_CHANNEL_INFO_T ar5gChannelList[MAX_5G_BAND_CHN_NUM];

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prChnLoadInfo = &(prGlueInfo->prAdapter->rWifiVar.rChnLoadInfo);
	kalMemZero(pcCommand, i4TotalLen);

	rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_2G4, TRUE,
			     MAX_2G_BAND_CHN_NUM, &ucNumOf2gChannel, ar2gChannelList);
	rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_5G, TRUE,
			     MAX_5G_BAND_CHN_NUM, &ucNumOf5gChannel, ar5gChannelList);

	for (cIdx = 0; cIdx < MAX_CHN_NUM; cIdx++) {
		INT_8 cIdx2 = 0;
		UINT_8 ucChannelNum = 0;
		UINT_32 u4Offset = 0;
		RF_CHANNEL_INFO_T *prChannelList = NULL;

		if (prChnLoadInfo->rChnRankList[cIdx].ucChannel > 14) {
			prChannelList = ar5gChannelList;
			ucChannelNum = ucNumOf5gChannel;
		} else {
			prChannelList = ar2gChannelList;
			ucChannelNum = ucNumOf2gChannel;
		}

		for (cIdx2 = 0; cIdx2 < ucChannelNum; cIdx2++) {
			if (prChnLoadInfo->rChnRankList[cIdx].ucChannel ==
				prChannelList[cIdx2].ucChannelNum) {
				u4Offset = kalSnprintf(pcCommand + i4BytesWritten,
							i4TotalLen - i4BytesWritten,
							"\nch %03u -> dirtiness %u",
							prChnLoadInfo->rChnRankList[cIdx].ucChannel,
							prChnLoadInfo->rChnRankList[cIdx].u4Dirtiness);
				i4BytesWritten += u4Offset;
				break;
			}

			if (i4BytesWritten >= i4TotalLen)
				return i4BytesWritten;
		}
	}

	return i4BytesWritten;
}
#endif

static int priv_driver_efuse_ops(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	enum EFUSE_OP_MODE {
		EFUSE_READ,
		EFUSE_WRITE,
		EFUSE_FREE,
		EFUSE_INVALID,
	};
	UINT_8 ucOpMode = EFUSE_INVALID;
	UCHAR ucOpChar;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	UINT_32 u4Ret;
	INT_32 i4Parameter;
	UINT_32 u4Efuse_addr = 0;
	UINT_8 ucEfuse_value = 0;

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4Offset = 0;
	UINT_32 u4BufLen = 0;
	UINT_8  u4Index = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_CUSTOM_ACCESS_EFUSE_T rAccessEfuseInfo;
#endif
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	/* Sanity check */
	if (i4Argc < 2)
		goto efuse_op_invalid;

	ucOpChar = (UCHAR)apcArgv[1][0];
	if ((i4Argc == 3) && (ucOpChar == 'r' || ucOpChar == 'R'))
		ucOpMode = EFUSE_READ;
	else if ((i4Argc == 4) && (ucOpChar == 'w' || ucOpChar == 'W'))
		ucOpMode = EFUSE_WRITE;
	else if ((ucOpChar == 'f' || ucOpChar == 'F'))
		ucOpMode = EFUSE_FREE;

	/* Print out help if input format is wrong */
	if (ucOpMode == EFUSE_INVALID)
		goto efuse_op_invalid;

	/* convert address */
	if (ucOpMode == EFUSE_READ || ucOpMode == EFUSE_WRITE) {
		u4Ret = kalkStrtos32(apcArgv[2], 16, &i4Parameter);
		u4Efuse_addr = (UINT_32)i4Parameter;
	}

	/* convert value */
	if (ucOpMode == EFUSE_WRITE) {
		u4Ret = kalkStrtos32(apcArgv[3], 16, &i4Parameter);
		ucEfuse_value = (UINT_8)i4Parameter;
	}

	/* Start operation */
#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	kalMemSet(&rAccessEfuseInfo, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));
	rAccessEfuseInfo.u4Address = (u4Efuse_addr / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
	u4Index = u4Efuse_addr % EFUSE_BLOCK_SIZE;

	if (ucOpMode == EFUSE_READ) {
		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryProcessAccessEfuseRead,
					&rAccessEfuseInfo,
					sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
						"Read success 0x%X = 0x%X\n", u4Efuse_addr
						, prGlueInfo->prAdapter->aucEepromVaule[u4Index]);
		}
	} else if (ucOpMode == EFUSE_WRITE) {

		prGlueInfo->prAdapter->aucEepromVaule[u4Index] = ucEfuse_value;

		kalMemCopy(rAccessEfuseInfo.aucData, prGlueInfo->prAdapter->aucEepromVaule, 16);

		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryProcessAccessEfuseWrite,
					&rAccessEfuseInfo,
					sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus == WLAN_STATUS_SUCCESS) {
			u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
						"Write success 0x%X = 0x%X\n"
						, u4Efuse_addr
						, ucEfuse_value);
		}
	} else if (ucOpMode == EFUSE_FREE) {
		PARAM_CUSTOM_EFUSE_FREE_BLOCK_T rEfuseFreeBlock = {};

		if (prGlueInfo->prAdapter->fgIsSupportGetFreeEfuseBlockCount == FALSE) {
			u4Offset += snprintf(pcCommand + u4Offset,
						i4TotalLen - u4Offset, "Cannot read free block size\n");
			return (INT_32)u4Offset;
		}
		rStatus = kalIoctl(prGlueInfo,
					wlanoidQueryEfuseFreeBlock,
					&rEfuseFreeBlock,
					sizeof(PARAM_CUSTOM_EFUSE_FREE_BLOCK_T), TRUE, TRUE, TRUE, &u4BufLen);
		if (rStatus == WLAN_STATUS_SUCCESS) {
			u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
						"Free block size 0x%X\n", prGlueInfo->prAdapter->u4FreeBlockNum);
		}
	}
#else
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
					"efuse ops is invalid\n");
#endif

	return (INT_32)u4Offset;

efuse_op_invalid:

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\nHelp menu\n");
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tRead:\t\"efuse read addr_hex\"\n");
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tWrite:\t\"efuse write addr_hex val_hex\"\n");
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tFree Blocks:\t\"efuse free\"\n");
	return (INT_32)u4Offset;
}

static int priv_driver_cccr_ops(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t u4Offset = 0;
#if defined(_HIF_SDIO) && (MTK_WCN_HIF_SDIO == 0)
	enum CCCR_OP_MODE {
		CCCR_READ,
		CCCR_WRITE,
		CCCR_INVALID,
	};
	uint8_t ucOpMode = CCCR_INVALID;
	uint8_t ucOpChar;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret;
	int32_t i4Parameter;
	uint32_t u4CCCR_addr = 0;
	uint8_t ucCCCR_value = 0;

	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = NULL;

	struct sdio_func *func;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	/* Sanity check */
	if (i4Argc < 2)
		goto cccr_op_invalid;

	ucOpChar = (uint8_t)apcArgv[1][0];
	if ((i4Argc == 3) && (ucOpChar == 'r' || ucOpChar == 'R'))
		ucOpMode = CCCR_READ;
	else if ((i4Argc == 4) && (ucOpChar == 'w' || ucOpChar == 'W'))
		ucOpMode = CCCR_WRITE;

	/* Print out help if input format is wrong */
	if (ucOpMode == CCCR_INVALID)
		goto cccr_op_invalid;

	/* convert address */
	if (ucOpMode == CCCR_READ || ucOpMode == CCCR_WRITE) {
		i4Ret = kalkStrtos32(apcArgv[2], 16, &i4Parameter);
		/* Valid address 0x0~0xFF */
		u4CCCR_addr = (uint32_t)(i4Parameter & 0xFF);
	}

	/* convert value */
	if (ucOpMode == CCCR_WRITE) {
		i4Ret = kalkStrtos32(apcArgv[3], 16, &i4Parameter);
		ucCCCR_value = (uint8_t)i4Parameter;
	}

	/* Set SDIO host reference */
	func = prGlueInfo->rHifInfo.func;

	/* Start operation */
	if (ucOpMode == CCCR_READ) {
		sdio_claim_host(func);
		ucCCCR_value = sdio_f0_readb(func, u4CCCR_addr, &rStatus);
		sdio_release_host(func);

		if (rStatus) /* Fail case */
			u4Offset += snprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Read Fail 0x%X (ret=%d)\n",
					     u4CCCR_addr, rStatus);
		else
			u4Offset += snprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Read success 0x%X = 0x%X\n",
					     u4CCCR_addr, ucCCCR_value);
	} else if (ucOpMode == CCCR_WRITE) {
		uint32_t quirks_bak;

		sdio_claim_host(func);
		/* Enable capability to write CCCR */
		quirks_bak = func->card->quirks;
		func->card->quirks |= MMC_QUIRK_LENIENT_FN0;
		/* Write CCCR into card */
		sdio_f0_writeb(func, ucCCCR_value, u4CCCR_addr, &rStatus);
		func->card->quirks = quirks_bak;
		sdio_release_host(func);

		if (rStatus) /* Fail case */
			u4Offset += snprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Write Fail 0x%X (ret=%d)\n",
					     u4CCCR_addr, rStatus);
		else
			u4Offset += snprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Write success 0x%X = 0x%X\n",
					     u4CCCR_addr, ucCCCR_value);
	}

	return (int32_t)u4Offset;

cccr_op_invalid:

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\nHelp menu\n");
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tRead:\t\"cccr read addr_hex\"\n");
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tWrite:\t\"cccr write addr_hex val_hex\"\n");
#else
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tSupport Linux standard SDIO only\n");
#endif /* _HIF_SDIO && (MTK_WCN_HIF_SDIO == 0) */
	return (int32_t)u4Offset;
}



#if CFG_SUPPORT_ADVANCE_CONTROL
static int priv_driver_set_noise(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret = 0;
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_NOISE_ID;
	UINT_32 u4Sel = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_NOISE <Sel>\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);

	rSwCtrlInfo.u4Data = u4Sel << 30;
	DBGLOG(REQ, LOUD, "u4Sel=%d u4Data=0x%x,\n", u4Sel, rSwCtrlInfo.u4Data);
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;

}

static int priv_driver_get_noise(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_NOISE_ID;
	UINT_32 u4Offset = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;
	INT_16 u2Wf0AvgPwr, u2Wf1AvgPwr;

	ASSERT(prNetDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u2Wf0AvgPwr = rSwCtrlInfo.u4Data & 0xFFFF;
	u2Wf1AvgPwr = (rSwCtrlInfo.u4Data >> 16) & 0xFFFF;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"Noise Idle Avg. Power: WF0:%ddB WF1:%ddB\n", u2Wf0AvgPwr, u2Wf1AvgPwr);

	i4BytesWritten = (INT_32)u4Offset;

	return i4BytesWritten;

}				/* priv_driver_get_sw_ctrl */

static int priv_driver_get_traffic_report(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo;
	INT_32 i4BytesWritten = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	struct CMD_GET_TRAFFIC_REPORT *cmd = NULL;
	UINT_8 ucBand = ENUM_BAND_0;
	UINT_16 u2Val = 0;
	UINT_8 ucVal = 0;
	INT_32 u4Ret = 0;
	BOOL fgWaitResp = FALSE;
	BOOL fgRead = FALSE;
	BOOL fgGetDbg = FALSE;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	if (!prGlueInfo)
		goto get_report_invalid;

	cmd = (struct CMD_GET_TRAFFIC_REPORT *)kalMemAlloc(sizeof(*cmd), VIR_MEM_TYPE);
	if (!cmd)
		goto get_report_invalid;

	if ((i4Argc > 4) || (i4Argc < 2))
		goto get_report_invalid;

	memset(cmd, 0, sizeof(*cmd));

	cmd->u2Type = CMD_GET_REPORT_TYPE;
	cmd->u2Len = sizeof(*cmd);
	cmd->ucBand = ucBand;

	if (strnicmp(apcArgv[1], "ENABLE", strlen("ENABLE")) == 0) {
		prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap |= KEEP_FULL_PWR_TRAFFIC_REPORT_BIT;
		cmd->ucAction = CMD_GET_REPORT_ENABLE;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
	} else if (strnicmp(apcArgv[1], "DISABLE", strlen("DISABLE")) == 0) {
		prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap &= ~KEEP_FULL_PWR_TRAFFIC_REPORT_BIT;
		cmd->ucAction = CMD_GET_REPORT_DISABLE;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
	} else if (strnicmp(apcArgv[1], "RESET", strlen("RESET")) == 0) {
		cmd->ucAction = CMD_GET_REPORT_RESET;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
	} else if ((strnicmp(apcArgv[1], "GET", strlen("GET")) == 0) ||
		(strnicmp(apcArgv[1], "GETDBG", strlen("GETDBG")) == 0)) {
		cmd->ucAction = CMD_GET_REPORT_GET;
		fgWaitResp = TRUE;
		fgRead = TRUE;
		if ((i4Argc == 4) && (strnicmp(apcArgv[2], "BAND", strlen("BAND")) == 0)) {
			u4Ret = kalkStrtou8(apcArgv[3], 0, &ucVal);
			cmd->ucBand = ucVal;
		}
		if (strnicmp(apcArgv[1], "GETDBG", strlen("GETDBG")) == 0)
			fgGetDbg = TRUE;
	} else if ((strnicmp(apcArgv[1], "SAMPLEPOINTS", strlen("SAMPLEPOINTS")) == 0) && (i4Argc == 3)) {
		u4Ret = kalkStrtou16(apcArgv[2], 0, &u2Val);
		cmd->u2SamplePoints = u2Val;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
		cmd->ucAction = CMD_SET_REPORT_SAMPLE_POINT;
	} else if ((strnicmp(apcArgv[1], "TXTHRES", strlen("TXTHRES")) == 0) && (i4Argc == 3)) {
		u4Ret = kalkStrtou8(apcArgv[2], 0, &ucVal);
		/* valid val range is from 0 - 100% */
		if (ucVal > 100)
			ucVal = 100;
		cmd->ucTxThres = ucVal;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
		cmd->ucAction = CMD_SET_REPORT_TXTHRES;
	} else if ((strnicmp(apcArgv[1], "RXTHRES", strlen("RXTHRES")) == 0) && (i4Argc == 3)) {
		u4Ret = kalkStrtou8(apcArgv[2], 0, &ucVal);
		/* valid val range is from 0 - 100% */
		if (ucVal > 100)
			ucVal = 100;
		cmd->ucRxThres = ucVal;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
		cmd->ucAction = CMD_SET_REPORT_RXTHRES;
	} else
		goto get_report_invalid;

	DBGLOG(REQ, LOUD, "%s(%s) action %x band %x wait_resp %x\n"
		, __func__, pcCommand, cmd->ucAction, ucBand, fgWaitResp);

	rStatus = kalIoctl(prGlueInfo, wlanoidAdvCtrl, cmd, sizeof(*cmd), TRUE, TRUE, TRUE, &u4BufLen);

	if ((rStatus != WLAN_STATUS_SUCCESS) && (rStatus != WLAN_STATUS_PENDING))
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\ncommand failed %x", rStatus);
	else if (cmd->ucAction == CMD_GET_REPORT_GET) {
		int persentage = 0;
		int sample_dur = cmd->u4FetchEd - cmd->u4FetchSt;

		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nCCK false detect cnt: %d"
				, (cmd->u4FalseCCA >> EVENT_REPORT_CCK_FCCA) & EVENT_REPORT_CCK_FCCA_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nOFDM false detect cnt: %d"
				, (cmd->u4FalseCCA >> EVENT_REPORT_OFDM_FCCA) & EVENT_REPORT_OFDM_FCCA_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nCCK Sig CRC cnt: %d"
				, (cmd->u4HdrCRC >> EVENT_REPORT_CCK_SIGERR) & EVENT_REPORT_CCK_SIGERR_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nOFDM Sig CRC cnt: %d"
				, (cmd->u4HdrCRC >> EVENT_REPORT_OFDM_SIGERR) & EVENT_REPORT_OFDM_SIGERR_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nBand%d Info:", cmd->ucBand);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tSample every %u ms with %u points", cmd->u4TimerDur, cmd->u2SamplePoints);
		if (fgGetDbg) {
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				" from systime %u - %u total_dur %u us f_cost %u us t_drift %d ms"
				, cmd->u4FetchSt, cmd->u4FetchEd
				, sample_dur
				, cmd->u4FetchCost, cmd->TimerDrift);
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"\n\tbusy-RMAC %u us, idle-TMAC %u us, t_total %u"
					, cmd->u4ChBusy, cmd->u4ChIdle, cmd->u4ChBusy + cmd->u4ChIdle);
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"\n\theavy tx threshold %u%% rx threshold %u%%"
					, cmd->ucTxThres, cmd->ucRxThres);
		}
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tch_busy %u us, ch_idle %u us, total_period %u us"
				, sample_dur - cmd->u4ChIdle
				, cmd->u4ChIdle, sample_dur);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tmy_tx_time: %u us"
				, cmd->u4TxAirTime);
		if (cmd->u4FetchEd - cmd->u4FetchSt) {
			persentage = cmd->u4TxAirTime / (sample_dur / 1000);
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				", tx utility: %d.%1d%%"
				, persentage / 10
				, persentage % 10);
		}
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tmy_data_rx_time (no BMC data): %u us"
				, cmd->u4RxAirTime);
		if (cmd->u4FetchEd - cmd->u4FetchSt) {
			persentage = cmd->u4RxAirTime / (sample_dur / 1000);
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				", rx utility: %d.%1d%%"
				, persentage / 10
				, persentage % 10);
		}
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tTotal packet transmitted: %u", cmd->u4PktSent);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tTotal tx ok packet: %u", cmd->u4PktSent - cmd->u4PktTxfailed);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tTotal tx failed packet: %u", cmd->u4PktTxfailed);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tTotal tx retried packet: %u", cmd->u4PktRetried);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tTotal rx mpdu: %u", cmd->u4RxMPDU);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\tTotal rx fcs: %u", cmd->u4RxFcs);
	} else
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\ncommand sent %x", rStatus);

	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));

	return i4BytesWritten;
get_report_invalid:
	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nformat:get_report [enable|disable|get|reset]");
	return i4BytesWritten;
}


static int priv_driver_pta_config(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo;
	INT_32 i4BytesWritten = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	UINT_32 u4Val = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	P_CMD_PTA_CONFIG_T cmd = NULL;
	INT_32 u4Ret = 0;
	INT_32 i = 0;

	DBGLOG(REQ, LOUD, "%s(%s)>\n", __func__, pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc < 2)
		goto set_pta_invalid;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	if (!prGlueInfo)
		goto set_pta_invalid;

	cmd = (P_CMD_PTA_CONFIG_T)kalMemAlloc(sizeof(*cmd), VIR_MEM_TYPE);
	if (!cmd)
		goto set_pta_invalid;

	memset(cmd, 0, sizeof(*cmd));
	cmd->u2Type = CMD_PTA_CONFIG_TYPE;
	cmd->u2Len = sizeof(*cmd);
	/* set command + parameter must be even number */
	if ((strnicmp(apcArgv[1], "SET", strlen("SET")) == 0) && (i4Argc >= 4) && !(i4Argc & 1)) {
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
		for (i = 2; i < i4Argc; i += 2) {
			u4Ret = kalkStrtou32(apcArgv[i+1], 0, &u4Val);
			if (u4Ret) {
				i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"\nparsing err(%d) %s %s", u4Ret, apcArgv[i], apcArgv[i+1]);
				goto set_pta_invalid;
			}

			DBGLOG(REQ, LOUD, "arg[%d] %s %s (0x%x)\n", i, apcArgv[i], apcArgv[i+1], u4Val);
			if (strnicmp(apcArgv[i], "ENABLE", strlen("ENABLE")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_PTA;
				if (u4Val)
					cmd->u4PtaConfig |= CMD_PTA_CONFIG_PTA_EN;
			} else if (strnicmp(apcArgv[i], "TXDATA", strlen("TXDATA")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_TXDATA_TAG;
				cmd->u4TxDataTag = u4Val;
			} else if (strnicmp(apcArgv[i], "RXDATAACK", strlen("RXDATAACK")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_RXDATAACK_TAG;
				cmd->u4RxDataAckTag = u4Val;
			} else if (strnicmp(apcArgv[i], "RXDATA", strlen("RXDATA")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_RX_NSW_TAG;
				cmd->u4RxNswTag = u4Val;
			}  else if (strnicmp(apcArgv[i], "TXACK", strlen("TXACK")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_TXACK_TAG;
				cmd->u4TxAckTag = u4Val;
			} else if (strnicmp(apcArgv[i], "PROTTAG", strlen("PROTTAG")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_TXPROTFRAME_TAG;
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_RXPROTFRAMEACK_TAG;
				cmd->u4TxProtFrameTag = u4Val;
				cmd->u4RxProtFrameAckTag = u4Val;
			} else if (strnicmp(apcArgv[i], "TXBMC", strlen("TXBMC")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_TX_BMC_TAG;
				cmd->u4TxBMCTag = u4Val;
			} else if (strnicmp(apcArgv[i], "TXBCN", strlen("TXBCN")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_TX_BCN_TAG;
				cmd->u4TxBCNTag = u4Val;
			} else if (strnicmp(apcArgv[i], "RXBCN", strlen("RXBCN")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_RX_SP_TAG;
				cmd->u4RxSPTag = u4Val;
			} else if (strnicmp(apcArgv[i], "TXMGMT", strlen("TXMGMT")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_TX_MGMT_TAG;
				cmd->u4TxMgmtTag = u4Val;
			} else if (strnicmp(apcArgv[i], "RXMGMTACK", strlen("RXMGMTACK")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_RXMGMTACK_TAG;
				cmd->u4RxMgmtAckTag = u4Val;
			} else if (strnicmp(apcArgv[i], "STATENABLE", strlen("STATENABLE")) == 0) {
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_PTA_STAT;
				if (!u4Val)
					cmd->u4PtaConfig &= ~CMD_PTA_CONFIG_PTA_STAT_EN;
				else
					cmd->u4PtaConfig |= CMD_PTA_CONFIG_PTA_STAT_EN;
			} else if (strnicmp(apcArgv[i], "STATRESET", strlen("STATRESET")) == 0)
				cmd->u4ConfigMask |= CMD_PTA_CONFIG_PTA_STAT_RESET;
			else {
				i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"\nunknown parameter %s %s", apcArgv[i], apcArgv[i+1]);
				goto set_pta_invalid;
			}
		}

		rStatus	= wlanSendSetQueryCmd(prGlueInfo->prAdapter,
			CMD_ID_ADV_CONTROL,
			TRUE,
			FALSE,
			FALSE,
			NULL, NULL, sizeof(*cmd), (PUINT_8) cmd, NULL, 0);
	} else if (strnicmp(apcArgv[1], "GET", strlen("GET")) == 0) {
		rStatus = kalIoctl(prGlueInfo, wlanoidAdvCtrl, cmd, sizeof(*cmd), TRUE, TRUE, TRUE, &u4BufLen);
	} else
		goto set_pta_invalid;

	if ((rStatus != WLAN_STATUS_SUCCESS) && (rStatus != WLAN_STATUS_PENDING))
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\ncommand failed %x", rStatus);
	else if (!(cmd->u2Type & CMD_ADV_CONTROL_SET)) {
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nCoex mode: %s", (cmd->u4CoexMode) ? "FDD" : "TDD");
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nPTA status:");
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n module enable %x wifi %x bt %x wifi arb %x"
				, (cmd->u4PtaConfig>>EVENT_CONFIG_PTA_OFFSET)&EVENT_CONFIG_PTA_FEILD
				, (cmd->u4PtaConfig>>EVENT_CONFIG_PTA_WIFI_OFFSET)&EVENT_CONFIG_PTA_WIFI_FEILD
				, (cmd->u4PtaConfig>>EVENT_CONFIG_PTA_BT_OFFSET)&EVENT_CONFIG_PTA_BT_FEILD
				, (cmd->u4PtaConfig>>EVENT_CONFIG_PTA_ARB_OFFSET)&EVENT_CONFIG_PTA_ARB_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nPriority stat:");
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n txData %d", cmd->u4TxDataTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n rxDataAck %d", cmd->u4RxDataAckTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n rxData %d", cmd->u4RxNswTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n txAck %d", cmd->u4TxAckTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n txBmc %d", cmd->u4TxBMCTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n txBcn %d", cmd->u4TxBCNTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n txMgmt %d", cmd->u4TxMgmtTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n rxMgmtAck %d", cmd->u4RxMgmtAckTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n rxBcn %d", cmd->u4RxSPTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n prottag %d", cmd->u4TxProtFrameTag);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nLast fetched grant stat:");
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n wifi grant %x txreq %x rxreq %x pritag %x"
				, (cmd->u4GrantStat>>EVENT_CONFIG_WIFI_GRANT_OFFSET)&EVENT_CONFIG_WIFI_GRANT_FEILD
				, (cmd->u4GrantStat>>EVENT_CONFIG_WIFI_TXREQ_OFFSET)&EVENT_CONFIG_WIFI_TXREQ_FEILD
				, (cmd->u4GrantStat>>EVENT_CONFIG_WIFI_RXREQ_OFFSET)&EVENT_CONFIG_WIFI_RXREQ_FEILD
				, (cmd->u4GrantStat>>EVENT_CONFIG_WIFI_PRI_OFFSET)&EVENT_CONFIG_WIFI_PRI_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n bt grant %x txreq %x rxreq %x pritag %x"
				, (cmd->u4GrantStat>>EVENT_CONFIG_BT_GRANT_OFFSET)&EVENT_CONFIG_BT_GRANT_FEILD
				, (cmd->u4GrantStat>>EVENT_CONFIG_BT_TXREQ_OFFSET)&EVENT_CONFIG_BT_TXREQ_FEILD
				, (cmd->u4GrantStat>>EVENT_CONFIG_BT_RXREQ_OFFSET)&EVENT_CONFIG_BT_RXREQ_FEILD
				, (cmd->u4GrantStat>>EVENT_CONFIG_BT_PRI_OFFSET)&EVENT_CONFIG_BT_PRI_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nPTA stat:");
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n wf0 txreq_cnt %d txgrant_cnt %d txabort_cnt %d"
				, (cmd->u4PtaWF0TxCnt>>EVENT_PTA_WFTRX_CNT_OFFSET)&EVENT_PTA_WFTRX_CNT_FEILD
				, (cmd->u4PtaWF0TxCnt>>EVENT_PTA_WFTRX_GRANT_CNT_OFFSET)&EVENT_PTA_WFTRX_GRANT_CNT_FEILD
				, (cmd->u4PtaWF0AbtCnt>>EVENT_PTA_TX_ABT_CNT_OFFSET)&EVENT_PTA_TX_ABT_CNT_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n wf0 rxreq_cnt %d rxgrant_cnt %d rxabort_cnt %d"
				, (cmd->u4PtaWF0RxCnt>>EVENT_PTA_WFTRX_CNT_OFFSET)&EVENT_PTA_WFTRX_CNT_FEILD
				, (cmd->u4PtaWF0RxCnt>>EVENT_PTA_WFTRX_GRANT_CNT_OFFSET)&EVENT_PTA_WFTRX_GRANT_CNT_FEILD
				, (cmd->u4PtaWF0AbtCnt>>EVENT_PTA_RX_ABT_CNT_OFFSET)&EVENT_PTA_RX_ABT_CNT_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n wf1 txreq_cnt %d txgrant_cnt %d txabort_cnt %d"
				, (cmd->u4PtaWF1TxCnt>>EVENT_PTA_WFTRX_CNT_OFFSET)&EVENT_PTA_WFTRX_CNT_FEILD
				, (cmd->u4PtaWF1TxCnt>>EVENT_PTA_WFTRX_GRANT_CNT_OFFSET)&EVENT_PTA_WFTRX_GRANT_CNT_FEILD
				, (cmd->u4PtaWF1AbtCnt>>EVENT_PTA_TX_ABT_CNT_OFFSET)&EVENT_PTA_TX_ABT_CNT_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n wf1 rxreq_cnt %d rxgrant_cnt %d rxabort_cnt %d"
				, (cmd->u4PtaWF1RxCnt>>EVENT_PTA_WFTRX_CNT_OFFSET)&EVENT_PTA_WFTRX_CNT_FEILD
				, (cmd->u4PtaWF1RxCnt>>EVENT_PTA_WFTRX_GRANT_CNT_OFFSET)&EVENT_PTA_WFTRX_GRANT_CNT_FEILD
				, (cmd->u4PtaWF1AbtCnt>>EVENT_PTA_RX_ABT_CNT_OFFSET)&EVENT_PTA_RX_ABT_CNT_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n bt txreq_cnt %d txgrant_cnt %d txabort_cnt %d"
				, (cmd->u4PtaBTTxCnt>>EVENT_PTA_BTTRX_CNT_OFFSET)&EVENT_PTA_BTTRX_CNT_FEILD
				, (cmd->u4PtaBTTxCnt>>EVENT_PTA_BTTRX_GRANT_CNT_OFFSET)&EVENT_PTA_BTTRX_GRANT_CNT_FEILD
				, (cmd->u4PtaBTAbtCnt>>EVENT_PTA_TX_ABT_CNT_OFFSET)&EVENT_PTA_TX_ABT_CNT_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n bt rxreq_cnt %d rxgrant_cnt %d rxabort_cnt %d"
				, (cmd->u4PtaBTRxCnt>>EVENT_PTA_BTTRX_CNT_OFFSET)&EVENT_PTA_BTTRX_CNT_FEILD
				, (cmd->u4PtaBTRxCnt>>EVENT_PTA_BTTRX_GRANT_CNT_OFFSET)&EVENT_PTA_BTTRX_GRANT_CNT_FEILD
				, (cmd->u4PtaBTAbtCnt>>EVENT_PTA_RX_ABT_CNT_OFFSET)&EVENT_PTA_RX_ABT_CNT_FEILD);
	} else
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\ncommand sent %x", rStatus);

	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));

	return i4BytesWritten;
set_pta_invalid:
	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nformat:pta_config set [enable 1|0][txdata val][rxdataack val]");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\t[rxdata val][txack val][txbmc val][txbcn val][rxbcn val]");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\t[txmgmt val][rxmgmtack val][prottag val]");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n\t[statenable 1|0][statreset 1]");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n [enable val]: enable PTA(1) or not(0)");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n [txtag val<0~15>]: priority tag for tx ac0-ac3");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n [rxdataack val<0~15>]: priority tag for rx ac0-ac3 ack");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n [rxdata val<0~15>]: priority tag for rx data");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n [txack val<0~15>]: priority tag for tx ack");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n [txbmc val<0~15>]: priority tag for tx bmc packet");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n [txbcn val<0~15>]: priority tag for tx beacon");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n [rxbcn val<0~15>]: priority tag for rx beacon");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n [prottag val<0~15>]: priority tag for Protection frame");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nformat(%d):pta_config get", i4Argc);
	return i4BytesWritten;
}

static int priv_driver_set_pop(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret = 0;
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_POP_ID;
	UINT_32 u4Sel = 0, u4CckTh = 0, u4OfdmTh = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 3) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_POP <Sel> <CCK TH> <OFDM TH>\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);
	u4Ret = kalkStrtou32(apcArgv[2], 0, &u4CckTh);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);
	u4Ret = kalkStrtou32(apcArgv[3], 0, &u4OfdmTh);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);

	rSwCtrlInfo.u4Data = (u4CckTh | (u4OfdmTh<<8) | (u4Sel<<30));
	DBGLOG(REQ, LOUD, "u4Sel=%d u4CckTh=%d u4OfdmTh=%d, u4Data=0x%x,\n",
		u4Sel, u4CckTh, u4OfdmTh, rSwCtrlInfo.u4Data);
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;

}

static int priv_driver_get_pop(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_POP_ID;
	UINT_32 u4Offset = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;
	UINT_32 u4CckTh = 0, u4OfdmTh = 0;

	ASSERT(prNetDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4CckTh = rSwCtrlInfo.u4Data & 0xFF;
	u4OfdmTh = (rSwCtrlInfo.u4Data >> 8) & 0xFF;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"PoP: CckTh:%ddB OfdmTh:%ddB\n", u4CckTh, u4OfdmTh);

	i4BytesWritten = (INT_32)u4Offset;

	return i4BytesWritten;

}

static int priv_driver_set_ed(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret = 0, u4EdVal = 0;
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_ED_ID;
	UINT_32 u4Sel = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 2) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_ED <Sel> <EDCCA(-49~-81dBm)>\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);
	u4Ret = kalkStrtos32(apcArgv[2], 0, &u4EdVal);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);

	rSwCtrlInfo.u4Data = ((u4EdVal & 0xFF) | (u4Sel << 31));
	DBGLOG(REQ, LOUD, "u4Sel=%d u4EdCcaVal=%d, u4Data=0x%x,\n",
		u4Sel, u4EdVal, rSwCtrlInfo.u4Data);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;

}

static int priv_driver_get_ed(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_ED_ID;
	UINT_32 u4Offset = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;
	INT_8 u4EdVal = 0;

	ASSERT(prNetDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4EdVal = rSwCtrlInfo.u4Data & 0xFF;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"ED: %ddB\n", u4EdVal);

	i4BytesWritten = (INT_32)u4Offset;

	return i4BytesWritten;

}

static int priv_driver_set_pd(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret = 0;
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_PD_ID;
	UINT_32 u4Sel = 0;
	INT_32 u4CckTh = 0, u4OfdmTh = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_PD <Sel> [CCK TH] [OFDM TH]\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);

	if (u4Sel == 1) {
		if (i4Argc <= 3) {
			DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_PD 1 <CCK TH> <OFDM CH>\n", i4Argc);
			return -1;
		}
		u4Ret = kalkStrtos32(apcArgv[2], 0, &u4CckTh);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);
		u4Ret = kalkStrtos32(apcArgv[3], 0, &u4OfdmTh);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);
	}

	rSwCtrlInfo.u4Data = ((u4OfdmTh & 0xFFFF) | ((u4CckTh & 0xFF) << 16) | (u4Sel << 30));
	DBGLOG(REQ, LOUD, "u4Sel=%d u4OfdmTh=%d, u4CckTh=%d, u4Data=0x%x,\n",
		u4Sel, u4OfdmTh, u4CckTh, rSwCtrlInfo.u4Data);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

static int priv_driver_get_pd(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_PD_ID;
	UINT_32 u4Offset = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;
	INT_8 u4CckTh = 0, u4OfdmTh = 0;

	ASSERT(prNetDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4CckTh = rSwCtrlInfo.u4Data & 0xFF;
	u4OfdmTh = (rSwCtrlInfo.u4Data >> 8) & 0xFF;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"PD: CckTh:%ddB OfdmTh:%ddB\n", u4CckTh, u4OfdmTh);

	i4BytesWritten = (INT_32)u4Offset;

	return i4BytesWritten;
}

static int priv_cmd_not_support(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	DBGLOG(REQ, WARN, "not support priv command: %s\n", pcCommand);

	return -EOPNOTSUPP;
}

static int priv_driver_set_maxrfgain(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret = 0;
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_MAX_RFGAIN_ID;
	UINT_32 u4Sel = 0;
	INT_32 u4Wf0Gain = 0, u4Wf1Gain = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_RFGAIN <Sel> <WF0 Gain> <WF1 Gain>\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);

	if (u4Sel == 1) {
		if (i4Argc <= 3) {
			DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_RFGAIN 1 <WF0 Gain> <WF1 Gain>\n", i4Argc);
			return -1;
		}
		u4Ret = kalkStrtos32(apcArgv[2], 0, &u4Wf0Gain);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);
		u4Ret = kalkStrtos32(apcArgv[3], 0, &u4Wf1Gain);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);
	}

	rSwCtrlInfo.u4Data = ((u4Wf0Gain & 0xFF) | ((u4Wf1Gain & 0xFF) << 8) | (u4Sel << 31));
	DBGLOG(REQ, LOUD, "u4Sel=%d u4Wf0Gain=%d, u4Wf1Gain=%d, u4Data=0x%x,\n",
		u4Sel, u4Wf0Gain, u4Wf1Gain, rSwCtrlInfo.u4Data);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

static int priv_driver_get_maxrfgain(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_MAX_RFGAIN_ID;
	UINT_32 u4Offset = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;
	UINT_8 u4Wf0Gain = 0, u4Wf1Gain = 0;

	ASSERT(prNetDev);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4Wf0Gain = rSwCtrlInfo.u4Data & 0xFF;
	u4Wf1Gain = (rSwCtrlInfo.u4Data >> 8) & 0xFF;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"Max RFGain: WF0:%ddB WF1:%ddB\n", u4Wf0Gain, u4Wf1Gain);

	i4BytesWritten = (INT_32)u4Offset;

	return i4BytesWritten;

}

static int priv_driver_noise_histogram(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo;
	INT_32 i4BytesWritten = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	struct CMD_NOISE_HISTOGRAM_REPORT *cmd = NULL;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	if (!prGlueInfo)
		goto noise_histogram_invalid;

	cmd = (struct CMD_NOISE_HISTOGRAM_REPORT *)kalMemAlloc(sizeof(*cmd), VIR_MEM_TYPE);
	if (!cmd)
		goto noise_histogram_invalid;

	if ((i4Argc > 4) || (i4Argc < 2))
		goto noise_histogram_invalid;

	memset(cmd, 0, sizeof(*cmd));

	cmd->u2Type = CMD_NOISE_HISTOGRAM_TYPE;
	cmd->u2Len = sizeof(*cmd);

	if (strnicmp(apcArgv[1], "ENABLE", strlen("ENABLE")) == 0) {
		prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap |= KEEP_FULL_PWR_NOISE_HISTOGRAM_BIT;
		cmd->ucAction = CMD_NOISE_HISTOGRAM_ENABLE;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
	} else if (strnicmp(apcArgv[1], "DISABLE", strlen("DISABLE")) == 0) {
		prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap &= ~KEEP_FULL_PWR_NOISE_HISTOGRAM_BIT;
		cmd->ucAction = CMD_NOISE_HISTOGRAM_DISABLE;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
	} else if (strnicmp(apcArgv[1], "RESET", strlen("RESET")) == 0) {
		cmd->ucAction = CMD_NOISE_HISTOGRAM_RESET;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
	} else if (strnicmp(apcArgv[1], "GET", strlen("GET")) == 0) {
		cmd->ucAction = CMD_NOISE_HISTOGRAM_GET;
	} else
		goto noise_histogram_invalid;

	DBGLOG(REQ, LOUD, "%s(%s) action %x\n"
		, __func__, pcCommand, cmd->ucAction);

	rStatus = kalIoctl(prGlueInfo, wlanoidAdvCtrl, cmd, sizeof(*cmd), TRUE, TRUE, TRUE, &u4BufLen);

	if ((rStatus != WLAN_STATUS_SUCCESS) && (rStatus != WLAN_STATUS_PENDING))
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\ncommand failed %x", rStatus);
	else if (cmd->ucAction == CMD_NOISE_HISTOGRAM_GET) {

		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n       Power > -55: %10d"
				, cmd->u4IPI10);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-55 >= Power > -60: %10d"
				, cmd->u4IPI9);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-60 >= Power > -65: %10d"
				, cmd->u4IPI8);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-65 >= Power > -70: %10d"
				, cmd->u4IPI7);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-70 >= Power > -75: %10d"
				, cmd->u4IPI6);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-75 >= Power > -80: %10d"
				, cmd->u4IPI5);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-80 >= Power > -83: %10d"
				, cmd->u4IPI4);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-83 >= Power > -86: %10d"
				, cmd->u4IPI3);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-86 >= Power > -89: %10d"
				, cmd->u4IPI2);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-89 >= Power > -92: %10d"
				, cmd->u4IPI1);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\n-92 >= Power      : %10d"
				, cmd->u4IPI0);

	} else
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\ncommand sent %x", rStatus);

	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));

	return i4BytesWritten;
noise_histogram_invalid:
	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"\nformat:get_report [enable|disable|get|reset]");
	return i4BytesWritten;
}

static int priv_driver_set_adm_ctrl(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret = 0;
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_ADM_CTRL_ID;
	UINT_32 u4Enable = 0, u4TimeRatio = 100, u4AdmBase = 0;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if ((i4Argc > 4) || (i4Argc < 2)) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: set_admission_ctrl\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Enable);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse u4Enable error u4Ret=%d\n", u4Ret);

	if (i4Argc > 2) {
		/* u4AdmTime default is 100% */
		u4Ret = kalkStrtos32(apcArgv[2], 0, &u4TimeRatio);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse u4AdmTime error u4Ret=%d\n", u4Ret);
	}

	if (i4Argc > 3) {
		u4Ret = kalkStrtos32(apcArgv[3], 0, &u4AdmBase);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse u4AdmBase error u4Ret=%d\n", u4Ret);
	}

	rSwCtrlInfo.u4Data = ((u4AdmBase & 0xFFFF) | ((u4TimeRatio & 0xFF) << 16) |
				((u4Enable & 0xFF) << 24));
	DBGLOG(REQ, LOUD, "u4Enable=%d u4AdmTime=%d, u4AdmBase=%d, u4Data=0x%x,\n",
		u4Enable, u4TimeRatio, u4AdmBase, rSwCtrlInfo.u4Data);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

static int priv_driver_set_bcn_th(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 i4Ret = 0;
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_BCN_TH_ID;
	UINT_8 ucAdhoc, ucInfra, ucWithbt;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc != 4) {
		i4BytesWritten += scnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"\nformat:set_bcn_th <adhoc> <infra> <withbt>");
		return i4BytesWritten;
	}

	i4Ret = kalkStrtou8(apcArgv[1], 0, &ucAdhoc);
	if (i4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error i4Ret=%d\n", i4Ret);
	i4Ret = kalkStrtou8(apcArgv[2], 0, &ucInfra);
	if (i4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error i4Ret=%d\n", i4Ret);
	i4Ret = kalkStrtou8(apcArgv[3], 0, &ucWithbt);
	if (i4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error i4Ret=%d\n", i4Ret);

	rSwCtrlInfo.u4Data = (ucAdhoc | (ucInfra << 8) | (ucWithbt << 16));
	DBGLOG(REQ, LOUD, "u4Withbt=%d u4Infra=%d, u4Withbt=%d u4Data=0x%x,\n",
		ucAdhoc, ucInfra, ucWithbt, rSwCtrlInfo.u4Data);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

static int priv_driver_get_bcn_th(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_32 u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_BCN_TH_ID;
	UINT_8 ucAdhoc, ucInfra, ucWithbt;
	PARAM_CUSTOM_SW_CTRL_STRUCT_T rSwCtrlInfo;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc != 1) {
		i4BytesWritten += scnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
					"\nformat:get_bcn_th");
		return i4BytesWritten;
	}

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id   = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	ucAdhoc  = rSwCtrlInfo.u4Data & 0xFF;
	ucInfra  = (rSwCtrlInfo.u4Data >> 8) & 0xFF;
	ucWithbt = (rSwCtrlInfo.u4Data >> 16) & 0xFF;

	i4BytesWritten += scnprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"\nBeacon loss threshold: ADHOC:%d, INFRA:%d, With BT:%d\n",
			ucAdhoc, ucInfra, ucWithbt);

	return i4BytesWritten;
}
#endif

#if CFG_SUPPORT_CSI
static int priv_driver_set_csi(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	struct CMD_CSI_CONTROL_T *prCSICtrl = NULL;
	UINT_32 u4Ret = 0;
	struct CSI_INFO_T *prCSIInfo = NULL;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RSN, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	DBGLOG(RSN, INFO, "priv_driver_csi_control\n");

	prCSIInfo = &(prGlueInfo->prAdapter->rCSIInfo);

	prCSICtrl = (struct CMD_CSI_CONTROL_T *)kalMemAlloc(sizeof(struct CMD_CSI_CONTROL_T), VIR_MEM_TYPE);
	if (!prCSICtrl) {
		DBGLOG(REQ, ERROR, "allocate memory for prCSICtrl failed\n");
		i4BytesWritten = -1;
		goto out;
	}

	if (i4Argc < 2 || i4Argc > 5) {
		DBGLOG(REQ, ERROR, "argc %i is invalid\n", i4Argc);
		i4BytesWritten = -1;
		goto out;
	}

	u4Ret = kalkStrtou8(apcArgv[1], 0, &(prCSICtrl->ucMode));
	if (u4Ret) {
		DBGLOG(REQ, LOUD, "parse ucMode error u4Ret=%d\n", u4Ret);
		goto out;
	}

	if (prCSICtrl->ucMode >= CSI_CONTROL_MODE_NUM) {
		DBGLOG(REQ, LOUD, "Invalid ucMode %d, should be 0 or 1\n",
			prCSICtrl->ucMode);
		goto out;
	}

	prCSIInfo->ucMode = prCSICtrl->ucMode;

	if (prCSICtrl->ucMode == CSI_CONTROL_MODE_STOP ||
		prCSICtrl->ucMode == CSI_CONTROL_MODE_START) {
		prCSIInfo->bIncomplete = FALSE;
		prCSIInfo->u4CopiedDataSize = 0;
		prCSIInfo->u4RemainingDataSize = 0;
		prCSIInfo->u4CSIBufferHead = 0;
		prCSIInfo->u4CSIBufferTail = 0;
		prCSIInfo->u4CSIBufferUsed = 0;
		goto send_cmd;
	}

	u4Ret = kalkStrtou8(apcArgv[2], 0, &(prCSICtrl->ucCfgItem));
	if (u4Ret) {
		DBGLOG(REQ, LOUD, "parse cfg item error u4Ret=%d\n", u4Ret);
		goto out;
	}

	if (prCSICtrl->ucCfgItem >= CSI_CONFIG_ITEM_NUM) {
		DBGLOG(REQ, LOUD, "Invalid csi cfg_item %u\n",
			prCSICtrl->ucCfgItem);
		goto out;
	}

	u4Ret = kalkStrtou8(apcArgv[3], 0, &(prCSICtrl->ucValue1));
	if (u4Ret) {
		DBGLOG(REQ, LOUD,
			"parse csi cfg value1 error u4Ret=%d\n", u4Ret);
		goto out;
	}
	prCSIInfo->ucValue1[prCSICtrl->ucCfgItem] = prCSICtrl->ucValue1;

	if (i4Argc == 5) {
		u4Ret = kalkStrtou8(apcArgv[4], 0, &(prCSICtrl->ucValue2));
		if (u4Ret) {
			DBGLOG(REQ, LOUD,
				"parse csi cfg value2 error u4Ret=%d\n", u4Ret);
			goto out;
		}
		prCSIInfo->ucValue2[prCSICtrl->ucCfgItem] = prCSICtrl->ucValue2;
	}

send_cmd:
	rStatus = kalIoctl(prGlueInfo, wlanoidSetCSIControl, prCSICtrl,
		sizeof(struct CMD_CSI_CONTROL_T), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);
	DBGLOG(REQ, INFO,
	   "mode %d, csi cfg item %d, value1 %d, value2 %d",
		prCSICtrl->ucMode, prCSICtrl->ucCfgItem,
		prCSICtrl->ucValue1, prCSICtrl->ucValue2);
	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "send CSI control cmd failed\n");
		i4BytesWritten = -1;
	}

out:
	if (prCSICtrl)
		kalMemFree(prCSICtrl, VIR_MEM_TYPE, sizeof(struct CMD_CSI_CONTROL_T));

	return i4BytesWritten;
}
#endif

int priv_driver_get_bsstable(IN struct net_device *prNetDev, IN char *pcCommand,
	IN int i4TotalLen)
{
	UINT_32 i4BytesWritten = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc;
	P_ADAPTER_T prAdapter = NULL;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		"\nBSSID, rssi0, rssi1\n------\n");

	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"%02x:%02x:%02x:%02x:%02x:%02x, %d, %d\n",
			prBssDesc->aucBSSID[0], prBssDesc->aucBSSID[1],
			prBssDesc->aucBSSID[2], prBssDesc->aucBSSID[3],
			prBssDesc->aucBSSID[4], prBssDesc->aucBSSID[5],
			RCPI_TO_dBm(prBssDesc->ucRCPI),
			RCPI_TO_dBm(prBssDesc->ucRCPI1));
	}

	return i4BytesWritten;
}

INT_32 priv_driver_cmds(IN struct net_device *prNetDev, IN PCHAR pcCommand, IN INT_32 i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = 0;

	if (g_u4HaltFlag) {
		DBGLOG(REQ, WARN, "wlan is halt, skip priv_driver_cmds\n");
		return -1;
	}

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

		if (strnicmp(pcCommand, CMD_RSSI, strlen(CMD_RSSI)) == 0) {
			/* i4BytesWritten =
			 *  wl_android_get_rssi(net, command, i4TotalLen);
			 */
		} else if (strnicmp(pcCommand, CMD_AP_START, strlen(CMD_AP_START)) == 0) {
			i4BytesWritten = priv_driver_set_ap_start(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_LINKSPEED, strlen(CMD_LINKSPEED)) == 0) {
			i4BytesWritten = priv_driver_get_linkspeed(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_PNOSSIDCLR_SET, strlen(CMD_PNOSSIDCLR_SET)) == 0) {
			/* ToDo:: Nothing */
		} else if (strnicmp(pcCommand, CMD_PNOSETUP_SET, strlen(CMD_PNOSETUP_SET)) == 0) {
			/* ToDo:: Nothing */
		} else if (strnicmp(pcCommand, CMD_PNOENABLE_SET, strlen(CMD_PNOENABLE_SET)) == 0) {
			/* ToDo:: Nothing */
		} else if (strnicmp(pcCommand, CMD_SETSUSPENDOPT, strlen(CMD_SETSUSPENDOPT)) == 0) {
			/* i4BytesWritten = wl_android_set_suspendopt(net, pcCommand, i4TotalLen); */
		} else if (strnicmp(pcCommand, CMD_SETSUSPENDMODE, strlen(CMD_SETSUSPENDMODE)) == 0) {
			i4BytesWritten = priv_driver_set_suspend_mode(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SETBAND, strlen(CMD_SETBAND)) == 0) {
			i4BytesWritten = priv_driver_set_band(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GETBAND, strlen(CMD_GETBAND)) == 0) {
			/* i4BytesWritten = wl_android_get_band(net, pcCommand, i4TotalLen); */
		} else if (strnicmp(pcCommand, CMD_SET_TXPOWER, strlen(CMD_SET_TXPOWER)) == 0) {
			i4BytesWritten = priv_driver_set_txpower(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_COUNTRY, strlen(CMD_COUNTRY)) == 0) {
			i4BytesWritten = priv_driver_set_country(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_COUNTRY, strlen(CMD_GET_COUNTRY)) == 0) {
			i4BytesWritten = priv_driver_get_country(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_CHANNELS, strlen(CMD_GET_CHANNELS)) == 0) {
			i4BytesWritten = priv_driver_get_channels(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_MIRACAST, strlen(CMD_MIRACAST)) == 0) {
			i4BytesWritten = priv_driver_set_miracast(prNetDev, pcCommand, i4TotalLen);
		}
		/* Mediatek private command  */
		else if (strnicmp(pcCommand, CMD_SET_SW_CTRL, strlen(CMD_SET_SW_CTRL)) == 0) {
			i4BytesWritten = priv_driver_set_sw_ctrl(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_FIXED_RATE, strlen(CMD_SET_FIXED_RATE)) == 0) {
			i4BytesWritten = priv_driver_set_fixed_rate(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_SW_CTRL, strlen(CMD_GET_SW_CTRL)) == 0) {
			i4BytesWritten = priv_driver_get_sw_ctrl(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_MCR, strlen(CMD_SET_MCR)) == 0) {
			i4BytesWritten = priv_driver_set_mcr(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_MCR, strlen(CMD_GET_MCR)) == 0) {
			i4BytesWritten = priv_driver_get_mcr(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_DRV_MCR, strlen(CMD_SET_DRV_MCR)) == 0) {
			i4BytesWritten = priv_driver_set_drv_mcr(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_DRV_MCR, strlen(CMD_GET_DRV_MCR)) == 0) {
			i4BytesWritten = priv_driver_get_drv_mcr(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_TEST_MODE, strlen(CMD_SET_TEST_MODE)) == 0) {
			i4BytesWritten = priv_driver_set_test_mode(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_TEST_CMD, strlen(CMD_SET_TEST_CMD)) == 0) {
			i4BytesWritten = priv_driver_set_test_cmd(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_TEST_RESULT, strlen(CMD_GET_TEST_RESULT)) == 0) {
			i4BytesWritten = priv_driver_get_test_result(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_STA_STAT2, strlen(CMD_GET_STA_STAT2)) == 0) {
			i4BytesWritten = priv_driver_get_sta_stat2(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_STA_STAT, strlen(CMD_GET_STA_STAT)) == 0) {
			i4BytesWritten = priv_driver_get_sta_stat(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_CURR_AR_RATE, strlen(CMD_GET_CURR_AR_RATE)) == 0) {
			i4BytesWritten = priv_driver_get_sta_curr_ar_rate(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_STA_RX_STAT, strlen(CMD_GET_STA_RX_STAT)) == 0) {
			i4BytesWritten = priv_driver_show_rx_stat(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_ACL_POLICY, strlen(CMD_SET_ACL_POLICY)) == 0) {
			i4BytesWritten = priv_driver_set_acl_policy(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_ADD_ACL_ENTRY, strlen(CMD_ADD_ACL_ENTRY)) == 0) {
			i4BytesWritten = priv_driver_add_acl_entry(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_DEL_ACL_ENTRY, strlen(CMD_DEL_ACL_ENTRY)) == 0) {
			i4BytesWritten = priv_driver_del_acl_entry(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SHOW_ACL_ENTRY, strlen(CMD_SHOW_ACL_ENTRY)) == 0) {
			i4BytesWritten = priv_driver_show_acl_entry(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_CLEAR_ACL_ENTRY, strlen(CMD_CLEAR_ACL_ENTRY)) == 0) {
			i4BytesWritten = priv_driver_clear_acl_entry(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_COEX_CONTROL, strlen(CMD_COEX_CONTROL)) == 0) {
			i4BytesWritten = priv_driver_coex_ctrl(prNetDev, pcCommand, i4TotalLen);
		}
#if CFG_SUPPORT_CSI
		else if (strnicmp(pcCommand,
			CMD_SET_CSI, strlen(CMD_SET_CSI)) == 0) {
			i4BytesWritten = priv_driver_set_csi(prNetDev, pcCommand, i4TotalLen);
		}
#endif
#if (CFG_SUPPORT_DFS_MASTER == 1)
		else if (strnicmp(pcCommand, CMD_SHOW_DFS_STATE, strlen(CMD_SHOW_DFS_STATE)) == 0) {
			i4BytesWritten = priv_driver_show_dfs_state(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SHOW_DFS_RADAR_PARAM, strlen(CMD_SHOW_DFS_RADAR_PARAM)) == 0) {
			i4BytesWritten = priv_driver_show_dfs_radar_param(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SHOW_DFS_HELP, strlen(CMD_SHOW_DFS_HELP)) == 0) {
			i4BytesWritten = priv_driver_show_dfs_help(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SHOW_DFS_CAC_TIME, strlen(CMD_SHOW_DFS_CAC_TIME)) == 0) {
			i4BytesWritten = priv_driver_show_dfs_cac_time(prNetDev, pcCommand, i4TotalLen);
		}
#endif
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
		else if (strnicmp(pcCommand,
			CMD_SET_CALBACKUP_TEST_DRV_FW, strlen(CMD_SET_CALBACKUP_TEST_DRV_FW)) == 0)
			i4BytesWritten = priv_driver_set_calbackup_test_drv_fw(prNetDev, pcCommand, i4TotalLen);
#endif
#if CFG_WOW_SUPPORT
		else if (strnicmp(pcCommand, CMD_WOW_START, strlen(CMD_WOW_START)) == 0)
			i4BytesWritten = priv_driver_set_wow(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_WOW_ENABLE, strlen(CMD_SET_WOW_ENABLE)) == 0)
			i4BytesWritten = priv_driver_set_wow_enable(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_WOW_PAR, strlen(CMD_SET_WOW_PAR)) == 0)
			i4BytesWritten = priv_driver_set_wow_par(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_WOW_UDP, strlen(CMD_SET_WOW_UDP)) == 0)
			i4BytesWritten = priv_driver_set_wow_udpport(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_WOW_TCP, strlen(CMD_SET_WOW_TCP)) == 0)
			i4BytesWritten = priv_driver_set_wow_tcpport(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_WOW_PORT, strlen(CMD_GET_WOW_PORT)) == 0)
			i4BytesWritten = priv_driver_get_wow_port(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_WOW_REASON, strlen(CMD_GET_WOW_REASON)) == 0)
			i4BytesWritten = priv_driver_get_wow_reason(prNetDev, pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_SET_ADV_PWS, strlen(CMD_SET_ADV_PWS)) == 0)
			i4BytesWritten = priv_driver_set_adv_pws(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_MDTIM, strlen(CMD_SET_MDTIM)) == 0)
			i4BytesWritten = priv_driver_set_mdtim(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_LISTEN_DTIM_INTERVAL, strlen(CMD_SET_LISTEN_DTIM_INTERVAL)) == 0)
			i4BytesWritten = priv_driver_set_listen_dtim_interval(prNetDev, pcCommand, i4TotalLen);
#if CFG_SUPPORT_QA_TOOL
		else if (strnicmp(pcCommand, CMD_GET_RX_STATISTICS, strlen(CMD_GET_RX_STATISTICS)) == 0)
			i4BytesWritten = priv_driver_get_rx_statistics(prNetDev, pcCommand, i4TotalLen);
#if CFG_SUPPORT_BUFFER_MODE
		else if (strnicmp(pcCommand, CMD_SETBUFMODE, strlen(CMD_SETBUFMODE)) == 0)
			i4BytesWritten = priv_driver_set_efuse_buffer_mode(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SETEEPROM_MODE, strlen(CMD_SETEEPROM_MODE)) == 0)
			i4BytesWritten = priv_driver_set_eeprom_mode(prNetDev, pcCommand, i4TotalLen);
#endif
#endif
#if CFG_SUPPORT_MSP
#if 0
		else if (strnicmp(pcCommand, CMD_GET_STAT, strlen(CMD_GET_STAT)) == 0)
			i4BytesWritten = priv_driver_get_stat(prNetDev, pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_GET_STA_STATISTICS, strlen(CMD_GET_STA_STATISTICS)) == 0)
			i4BytesWritten = priv_driver_get_sta_statistics(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_BSS_STATISTICS, strlen(CMD_GET_BSS_STATISTICS)) == 0)
			i4BytesWritten = priv_driver_get_bss_statistics(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_STA_INFO, strlen(CMD_GET_STA_INFO)) == 0)
			i4BytesWritten = priv_driver_get_sta_info(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_WTBL_INFO, strlen(CMD_GET_WTBL_INFO)) == 0)
			i4BytesWritten = priv_driver_get_wtbl_info(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_MIB_INFO, strlen(CMD_GET_MIB_INFO)) == 0)
			i4BytesWritten = priv_driver_get_mib_info(prNetDev, pcCommand, i4TotalLen);
#if CFG_SUPPORT_LAST_SEC_MCS_INFO
		else if (strnicmp(pcCommand, CMD_GET_MCS_INFO, strlen(CMD_GET_MCS_INFO)) == 0)
			i4BytesWritten = priv_driver_get_mcs_info(prNetDev, pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_SET_FW_LOG, strlen(CMD_SET_FW_LOG)) == 0)
			i4BytesWritten = priv_driver_set_fw_log(prNetDev, pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_SET_CFG, strlen(CMD_SET_CFG)) == 0) {
			i4BytesWritten = priv_driver_set_cfg(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_CFG, strlen(CMD_GET_CFG)) == 0) {
			i4BytesWritten = priv_driver_get_cfg(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_CHIP, strlen(CMD_SET_CHIP)) == 0) {
			i4BytesWritten = priv_driver_set_chip_config(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_CHIP, strlen(CMD_GET_CHIP)) == 0) {
			i4BytesWritten = priv_driver_get_chip_config(prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_VERSION, strlen(CMD_GET_VERSION)) == 0) {
			i4BytesWritten = priv_driver_get_version(prNetDev, pcCommand, i4TotalLen);
#if CFG_SUPPORT_DBDC

		} else if (strnicmp(pcCommand, CMD_SET_DBDC, strlen(CMD_SET_DBDC)) == 0) {
			i4BytesWritten = priv_driver_set_dbdc(prNetDev, pcCommand, i4TotalLen);
#endif /*CFG_SUPPORT_DBDC*/
#if CFG_SUPPORT_BATCH_SCAN
		} else if (strnicmp(pcCommand, CMD_BATCH_SET, strlen(CMD_BATCH_SET)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidSetBatchScanReq,
				 (PVOID) pcCommand, i4TotalLen, FALSE, FALSE, TRUE, &i4BytesWritten);
		} else if (strnicmp(pcCommand, CMD_BATCH_GET, strlen(CMD_BATCH_GET)) == 0) {
			/* strcpy(pcCommand, "BATCH SCAN DATA FROM FIRMWARE"); */
			/* i4BytesWritten = strlen("BATCH SCAN DATA FROM FIRMWARE") + 1; */
			/* i4BytesWritten = priv_driver_get_linkspeed (prNetDev, pcCommand, i4TotalLen); */

			UINT_32 u4BufLen;
			int i;
			/* int rlen=0; */

			for (i = 0; i < CFG_BATCH_MAX_MSCAN; i++) {
				g_rEventBatchResult[i].ucScanCount = i + 1;	/* for get which mscan */
				kalIoctl(prGlueInfo,
					 wlanoidQueryBatchScanResult,
					 (PVOID)&g_rEventBatchResult[i],
					 sizeof(EVENT_BATCH_RESULT_T), TRUE, TRUE, TRUE, &u4BufLen);
			}

#if 0
			DBGLOG(SCN, INFO, "Batch Scan Results, scan count = %u\n", g_rEventBatchResult.ucScanCount);
			for (i = 0; i < g_rEventBatchResult.ucScanCount; i++) {
				prEntry = &g_rEventBatchResult.arBatchResult[i];
				DBGLOG(SCN, INFO, "Entry %u\n", i);
				DBGLOG(SCN, INFO, "	 BSSID = " MACSTR "\n", MAC2STR(prEntry->aucBssid));
				DBGLOG(SCN, INFO, "	 SSID = %s\n", prEntry->aucSSID);
				DBGLOG(SCN, INFO, "	 SSID len = %u\n", prEntry->ucSSIDLen);
				DBGLOG(SCN, INFO, "	 RSSI = %d\n", prEntry->cRssi);
				DBGLOG(SCN, INFO, "	 Freq = %u\n", prEntry->ucFreq);
			}
#endif

			batchConvertResult(&g_rEventBatchResult[0], pcCommand, i4TotalLen, &i4BytesWritten);

			/* Dump for debug */
			/* print_hex_dump(KERN_INFO,
			 *  "BATCH", DUMP_PREFIX_ADDRESS, 16, 1, pcCommand, i4BytesWritten, TRUE);
			 */

		} else if (strnicmp(pcCommand, CMD_BATCH_STOP, strlen(CMD_BATCH_STOP)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidSetBatchScanReq,
				 (PVOID) pcCommand, i4TotalLen, FALSE, FALSE, TRUE, &i4BytesWritten);
#endif
		}
#if CFG_SUPPORT_SNIFFER
		else if (strnicmp(pcCommand, CMD_SETMONITOR, strlen(CMD_SETMONITOR)) == 0)
			i4BytesWritten = priv_driver_set_monitor(prNetDev, pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_GET_QUE_INFO, strlen(CMD_GET_QUE_INFO)) == 0)
			i4BytesWritten = priv_driver_get_que_info(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_MEM_INFO, strlen(CMD_GET_MEM_INFO)) == 0)
			i4BytesWritten = priv_driver_get_mem_info(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_HIF_INFO, strlen(CMD_GET_HIF_INFO)) == 0)
			i4BytesWritten = priv_driver_get_hif_info(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_P2P_PS, strlen(CMD_SET_P2P_PS)) == 0)
			i4BytesWritten = priv_driver_set_p2p_ps(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_CNM_INFO, strlen(CMD_GET_CNM_INFO)) == 0)
			i4BytesWritten = priv_driver_get_cnm_info(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_DSLP_CNT, strlen(CMD_GET_DSLP_CNT)) == 0)
			i4BytesWritten = priv_driver_get_deep_sleep_cnt(prNetDev, pcCommand, i4TotalLen);
#if CFG_AUTO_CHANNEL_SEL_SUPPORT
		else if (strnicmp(pcCommand, CMD_GET_CH_RANK_LIST, strlen(CMD_GET_CH_RANK_LIST)) == 0)
			i4BytesWritten = priv_driver_get_ch_rank_list(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_CH_DIRTINESS, strlen(CMD_GET_CH_DIRTINESS)) == 0)
			i4BytesWritten = priv_driver_get_ch_dirtiness(prNetDev, pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_EFUSE, sizeof(CMD_EFUSE)-1) == 0)
			i4BytesWritten = priv_driver_efuse_ops(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_CCCR, strlen(CMD_CCCR)) == 0)
			i4BytesWritten = priv_driver_cccr_ops(prNetDev,
							pcCommand,
							i4TotalLen);
#if CFG_SUPPORT_ADVANCE_CONTROL
		else if (strnicmp(pcCommand, CMD_SET_NOISE, strlen(CMD_SET_NOISE)) == 0)
			i4BytesWritten = priv_driver_set_noise(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_NOISE, strlen(CMD_GET_NOISE)) == 0)
			i4BytesWritten = priv_driver_get_noise(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_PTA_CONFIG, strlen(CMD_PTA_CONFIG)) == 0)
			i4BytesWritten = priv_driver_pta_config(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_TRAFFIC_REPORT, strlen(CMD_TRAFFIC_REPORT)) == 0)
			i4BytesWritten = priv_driver_get_traffic_report(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_POP, strlen(CMD_SET_POP)) == 0)
			i4BytesWritten = priv_driver_set_pop(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_POP, strlen(CMD_GET_POP)) == 0)
			i4BytesWritten = priv_driver_get_pop(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_ED, strlen(CMD_SET_ED)) == 0)
			i4BytesWritten = priv_driver_set_ed(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_ED, strlen(CMD_GET_ED)) == 0)
			i4BytesWritten = priv_driver_get_ed(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_PD, strlen(CMD_SET_PD)) == 0)
			i4BytesWritten = priv_driver_set_pd(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_PD, strlen(CMD_GET_PD)) == 0)
			i4BytesWritten = priv_driver_get_pd(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_MAX_RFGAIN, strlen(CMD_SET_MAX_RFGAIN)) == 0)
			i4BytesWritten = priv_driver_set_maxrfgain(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_MAX_RFGAIN, strlen(CMD_GET_MAX_RFGAIN)) == 0)
			i4BytesWritten = priv_driver_get_maxrfgain(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_NOISE_HISTOGRAM, strlen(CMD_NOISE_HISTOGRAM)) == 0)
			i4BytesWritten = priv_driver_noise_histogram(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_ADM_CTRL, strlen(CMD_SET_ADM_CTRL)) == 0)
			i4BytesWritten = priv_driver_set_adm_ctrl(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_BCN_TH, strlen(CMD_SET_BCN_TH)) == 0)
			i4BytesWritten = priv_driver_set_bcn_th(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_BCN_TH, strlen(CMD_GET_BCN_TH)) == 0)
			i4BytesWritten = priv_driver_get_bcn_th(prNetDev, pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_GET_BSS_TABLE,
						strlen(CMD_GET_BSS_TABLE)) == 0)
			i4BytesWritten = priv_driver_get_bsstable(prNetDev,
						pcCommand, i4TotalLen);
		else
		i4BytesWritten = priv_cmd_not_support(prNetDev, pcCommand, i4TotalLen);

	if (i4BytesWritten >= 0) {
		if ((i4BytesWritten == 0) && (i4TotalLen > 0)) {
			/* reset the command buffer */
			pcCommand[0] = '\0';
		}

		if (i4BytesWritten >= i4TotalLen) {
			DBGLOG(REQ, INFO,
			       "%s: i4BytesWritten %d > i4TotalLen < %d\n", __func__, i4BytesWritten, i4TotalLen);
			i4BytesWritten = i4TotalLen;
		} else {
			pcCommand[i4BytesWritten] = '\0';
			i4BytesWritten++;
		}
	}

	return i4BytesWritten;

}				/* priv_driver_cmds */

int priv_support_driver_cmd(IN struct net_device *prNetDev, IN OUT struct ifreq *prReq, IN int i4Cmd)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	int ret = 0;
	char *pcCommand = NULL;
	priv_driver_cmd_t *priv_cmd = NULL;
	int i4BytesWritten = 0;
	int i4TotalLen = 0;
	struct iwreq	*wrqin = (struct iwreq *) prReq;
	struct iw_point iwp;
#ifdef CONFIG_COMPAT
	struct compat_iw_point *iwp_compat = NULL;
#endif
	iwp.pointer = wrqin->u.data.pointer;
	iwp.length = wrqin->u.data.length;
	iwp.flags = wrqin->u.data.flags;

#ifdef CONFIG_COMPAT
#if KERNEL_VERSION(4, 6, 0) <= LINUX_VERSION_CODE
	if (in_compat_syscall()) {
#else
	if (is_compat_task()) {
#endif
		iwp_compat = (struct compat_iw_point *) &wrqin->u.data;
		iwp.pointer = compat_ptr(iwp_compat->pointer);
		iwp.length = iwp_compat->length;
		iwp.flags = iwp_compat->flags;
	}
#endif

	if (!prReq->ifr_data) {
		ret = -EINVAL;
		goto exit;
	}

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	if (!prGlueInfo) {
		DBGLOG(REQ, WARN, "No glue info\n");
		ret = -EFAULT;
		goto exit;
	}
	if (prGlueInfo->u4ReadyFlag == 0) {
		ret = -EINVAL;
		goto exit;
	}

	priv_cmd = kzalloc(sizeof(priv_driver_cmd_t), GFP_KERNEL);
	if (!priv_cmd) {
		DBGLOG(REQ, WARN, "%s, alloc mem failed\n", __func__);
		return -ENOMEM;
	}

	if (iwp.length > PRIV_CMD_SIZE)
		iwp.length = PRIV_CMD_SIZE;

	if (copy_from_user(priv_cmd->buf, iwp.pointer, iwp.length)) {
		DBGLOG(REQ, ERROR, "%s: copy_from_user fail with len [%d]\n",
			__func__, iwp.length);
		ret = -EFAULT;
		goto exit;
	}

	priv_cmd->total_len = iwp.length;

	i4TotalLen = priv_cmd->total_len;

	if (i4TotalLen <= 0) {
		ret = -EINVAL;
		DBGLOG(REQ, ERROR, "%s: Invalid Len %x %x %s\n", __func__,
			i4TotalLen, priv_cmd->used_len, priv_cmd->buf);
		goto exit;
	}

	pcCommand = priv_cmd->buf;

	DBGLOG(REQ, STATE, "%s: driver cmd \"%s\" on %s\n",
		__func__, pcCommand, prReq->ifr_name);

	i4BytesWritten = priv_driver_cmds(prNetDev, pcCommand, PRIV_CMD_SIZE);

	if (i4BytesWritten < 0) {
		DBGLOG(REQ, ERROR, "%s: command %s Written is %d\n",
			__func__, pcCommand, i4BytesWritten);
		if (i4TotalLen >= 3) {
			snprintf(pcCommand, 3, "OK");
			i4BytesWritten = strlen("OK");
		} else {
			ret = -EFAULT;
			goto exit;
		}
	}

	if (copy_to_user(iwp.pointer, priv_cmd->buf, i4BytesWritten)) {
		DBGLOG(REQ, ERROR, "%s: copy_to_user fail with Len : %d\n",
			__func__, i4BytesWritten);
		ret = -EFAULT;
		goto exit;
	}

	if (i4BytesWritten > PRIV_CMD_SIZE)
		i4BytesWritten = PRIV_CMD_SIZE;

	wrqin->u.data.length = i4BytesWritten;
#ifdef CONFIG_COMPAT
#if KERNEL_VERSION(4, 6, 0) <= CFG80211_VERSION_CODE
	if (in_compat_syscall()) {
#else
	if (is_compat_task()) {
#endif
		iwp_compat->pointer = ptr_to_compat(iwp.pointer);
		iwp_compat->length = i4BytesWritten;
	}
#endif

exit:
	kfree(priv_cmd);

	return ret;
}				/* priv_support_driver_cmd */

int priv_support_mdns_offload(IN struct net_device *prNetDev,
	IN OUT struct ifreq *prReq, IN int i4Cmd)
{
	P_GLUE_INFO_T    prGlueInfo = NULL;
	struct MDNS_PARAM_T *prMdnsParam = NULL;

	int ret = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (!prReq->ifr_data) {
		DBGLOG(REQ, ERROR, "%s: prReq->ifr_data is NULL.\n", __func__);
		return -EINVAL;
	}

	prMdnsParam = kzalloc(sizeof(struct MDNS_PARAM_T), GFP_KERNEL);
	if (!prMdnsParam) {
		DBGLOG(REQ, WARN, "%s, alloc mem failed\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(prMdnsParam,
		prReq->ifr_data, sizeof(struct MDNS_PARAM_T))) {
		DBGLOG(REQ, ERROR, "%s: copy_from_user fail\n", __func__);
		ret = -EFAULT;
		goto exit;
	}

	kalMdnsProcess(prGlueInfo, prMdnsParam);

exit:
	kfree(prMdnsParam);
	return ret;

}

#ifdef CFG_ANDROID_AOSP_PRIV_CMD
int android_private_support_driver_cmd(IN struct net_device *prNetDev,
									IN OUT struct ifreq *prReq, IN int i4Cmd)
{
	struct android_wifi_priv_cmd priv_cmd;
	char *command = NULL;
	int ret = 0, bytes_written = 0;
#ifdef CONFIG_COMPAT
	struct compat_android_wifi_priv_cmd compat_priv_cmd;
#endif

	if (!prReq->ifr_data)
		return -EINVAL;

#ifdef CONFIG_COMPAT
	if (mtk_is_compat_task()) {
		/* User space is 32-bit, use compat ioctl */
		if (copy_from_user(&compat_priv_cmd, prReq->ifr_data,
			sizeof(compat_priv_cmd))) {
			return -EFAULT;
		}
		priv_cmd.buf = compat_ptr(compat_priv_cmd.buf);
		priv_cmd.used_len = compat_priv_cmd.used_len;
		priv_cmd.total_len = compat_priv_cmd.total_len;
	} else
#endif
	if (copy_from_user(&priv_cmd, prReq->ifr_data, sizeof(priv_cmd)))
		return -EFAULT;

	if (priv_cmd.total_len <= 0)
		return -EINVAL;

	command = kzalloc(priv_cmd.total_len, GFP_KERNEL);
	if (!command) {
		DBGLOG(REQ, WARN, "%s, alloc mem failed\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(command, priv_cmd.buf, priv_cmd.total_len)) {
		ret = -EFAULT;
		goto FREE;
	}

	bytes_written = priv_driver_cmds(prNetDev, command, priv_cmd.total_len);

	if (bytes_written == -EOPNOTSUPP) {
		/* Report positive status */
		INT_32 i4BytesWritten = 0;

		i4BytesWritten += kalSnprintf(command + i4BytesWritten,
					priv_cmd.total_len - i4BytesWritten,
					"%s", "NotSupport");
		bytes_written = i4BytesWritten;
	}

	if (bytes_written >= 0) {
		/* priv_cmd in but no response */
		if ((bytes_written == 0) && (priv_cmd.total_len > 0))
			command[0] = '\0';

		if (bytes_written >= priv_cmd.total_len)
			bytes_written = priv_cmd.total_len;
		else
			bytes_written++;

		priv_cmd.used_len = bytes_written;

		if (copy_to_user(priv_cmd.buf, command, bytes_written))
			ret = -EFAULT;
	} else
		ret = bytes_written;

FREE:
		kfree(command);

	return ret;
}
#endif /* CFG_ANDROID_AOSP_PRIV_CMD */

#ifdef CFG_ALPS_ANDROID_AOSP_PRIV_CMD
int alps_android_private_support_driver_cmd(IN struct net_device *prNetDev,
				IN OUT struct ifreq *prReq, IN int i4Cmd)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	int ret = 0;
	char *pcCommand = NULL;
	struct android_wifi_priv_cmd *priv_cmd = NULL;
	int i4BytesWritten = 0;
	int i4TotalLen = 0;

	if (!prReq->ifr_data) {
		ret = -EINVAL;
		goto exit;
	}

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (!prGlueInfo) {
		DBGLOG(REQ, WARN, "No glue info\n");
		ret = -EFAULT;
		goto exit;
	}
	if (prGlueInfo->u4ReadyFlag == 0) {
		ret = -EINVAL;
		goto exit;
	}

	priv_cmd = kzalloc(sizeof(struct android_wifi_priv_cmd), GFP_KERNEL);
	if (!priv_cmd) {
		DBGLOG(REQ, WARN, "%s, alloc mem failed\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(priv_cmd, prReq->ifr_data, sizeof(struct
	android_wifi_priv_cmd))) {
		DBGLOG(REQ, INFO, "%s: copy_from_user fail\n", __func__);
		ret = -EFAULT;
		goto exit;
	}

	i4TotalLen = priv_cmd->total_len;
	if (i4TotalLen <= 0) {
		ret = -EINVAL;
		DBGLOG(REQ, INFO, "%s: i4TotalLen invalid\n", __func__);
		goto exit;
	}

	pcCommand = priv_cmd->buf;

	DBGLOG(REQ, INFO, "%s: driver cmd \"%s\" on %s\n",
		__func__, pcCommand, prReq->ifr_name);

	i4BytesWritten = priv_driver_cmds(prNetDev, pcCommand, i4TotalLen);

	if (i4BytesWritten < 0) {
		DBGLOG(REQ, INFO, "%s: command %s failed; Written is %d\n",
		__func__, pcCommand, i4BytesWritten);
		ret = -EFAULT;
	}

exit:
		kfree(priv_cmd);

	return ret;

}
#endif /* CFG_ALPS_ANDROID_AOSP_PRIV_CMD */
