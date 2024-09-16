/*******************************************************************************
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
 ******************************************************************************/
/*
 ** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux
 *      /gl_wext_priv.c#8
 */

/*! \file gl_wext_priv.c
 *    \brief This file includes private ioctl support.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
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
 *******************************************************************************
 */
#define	NUM_SUPPORTED_OIDS      (sizeof(arWlanOidReqTable) / \
				sizeof(struct WLAN_REQ_ENTRY))
#define	CMD_OID_BUF_LENGTH	4096

#if (CFG_SUPPORT_TWT == 1)
#define CMD_TWT_ACTION_TEN_PARAMS        10
#define CMD_TWT_ACTION_THREE_PARAMS      3
#define CMD_TWT_MAX_PARAMS CMD_TWT_ACTION_TEN_PARAMS
#endif

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

static int
priv_get_ndis(IN struct net_device *prNetDev,
	      IN struct NDIS_TRANSPORT_STRUCT *prNdisReq,
	      OUT uint32_t *pu4OutputLen);

static int
priv_set_ndis(IN struct net_device *prNetDev,
	      IN struct NDIS_TRANSPORT_STRUCT *prNdisReq,
	      OUT uint32_t *pu4OutputLen);

#if 0				/* CFG_SUPPORT_WPS */
static int
priv_set_appie(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwReqInfo,
	       IN union iwreq_data *prIwReqData, OUT char *pcExtra);

static int
priv_set_filter(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo,
		IN union iwreq_data *prIwReqData, OUT char *pcExtra);
#endif /* CFG_SUPPORT_WPS */

static u_int8_t reqSearchSupportedOidEntry(IN uint32_t rOid,
		OUT struct WLAN_REQ_ENTRY **ppWlanReqEntry);

#if 0
static uint32_t
reqExtQueryConfiguration(IN struct GLUE_INFO *prGlueInfo,
			 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen);

static uint32_t
reqExtSetConfiguration(IN struct GLUE_INFO *prGlueInfo,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen);
#endif

static uint32_t
reqExtSetAcpiDevicePowerState(IN struct GLUE_INFO
			      *prGlueInfo,
			      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen);

#if CFG_SUPPORT_DYNAMIC_PWR_LIMIT
/* dynamic tx power control */
static int priv_driver_set_power_control(IN struct net_device *prNetDev,
			      IN char *pcCommand,
			      IN int i4TotalLen);
#endif
#if CFG_SUPPORT_ANT_SWAP
/* set ant swap */
static int priv_driver_set_ant_swap(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen);
#endif
#if CFG_SUPPORT_SCAN_EXT_FLAG
static int priv_driver_set_scan_ext_flag(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen);
#endif

/*******************************************************************************
 *                       P R I V A T E   D A T A
 *******************************************************************************
 */
static uint8_t aucOidBuf[CMD_OID_BUF_LENGTH] = { 0 };

/* OID processing table */
/* Order is important here because the OIDs should be in order of
 *  increasing value for binary searching.
 */
static struct WLAN_REQ_ENTRY arWlanOidReqTable[] = {
#if 0
	{
		(NDIS_OID)rOid,
		(uint8_t *)pucOidName,
		fgQryBufLenChecking, fgSetBufLenChecking,
		fgIsHandleInGlueLayerOnly, u4InfoBufLen,
		pfOidQueryHandler,
		pfOidSetHandler
	}
#endif
	/* General Operational Characteristics */

	/* Ethernet Operational Characteristics */
	{
		OID_802_3_CURRENT_ADDRESS,
		DISP_STRING("OID_802_3_CURRENT_ADDRESS"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE, 6,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryCurrentAddr,
		NULL
	},

	/* OID_802_3_MULTICAST_LIST */
	/* OID_802_3_MAXIMUM_LIST_SIZE */
	/* Ethernet Statistics */

	/* NDIS 802.11 Wireless LAN OIDs */
	{
		OID_802_11_SUPPORTED_RATES,
		DISP_STRING("OID_802_11_SUPPORTED_RATES"),
		TRUE, FALSE, ENUM_OID_DRIVER_CORE,
		(sizeof(uint8_t) * PARAM_MAX_LEN_RATES_EX),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQuerySupportedRates,
		NULL
	}
	,
	/*
	 *  {OID_802_11_CONFIGURATION,
	 *  DISP_STRING("OID_802_11_CONFIGURATION"),
	 *  TRUE, TRUE, ENUM_OID_GLUE_EXTENSION,
	 *  sizeof(struct PARAM_802_11_CONFIG),
	 *  (PFN_OID_HANDLER_FUNC_REQ)reqExtQueryConfiguration,
	 *  (PFN_OID_HANDLER_FUNC_REQ)reqExtSetConfiguration},
	 */
	{
		OID_PNP_SET_POWER,
		DISP_STRING("OID_PNP_SET_POWER"),
		TRUE, FALSE, ENUM_OID_GLUE_EXTENSION,
		sizeof(enum PARAM_DEVICE_POWER_STATE),
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ) reqExtSetAcpiDevicePowerState
	}
	,

	/* Custom OIDs */
	{
		OID_CUSTOM_OID_INTERFACE_VERSION,
		DISP_STRING("OID_CUSTOM_OID_INTERFACE_VERSION"),
		TRUE, FALSE, ENUM_OID_DRIVER_CORE, 4,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryOidInterfaceVersion,
		NULL
	}
	,
#if 0
#if PTA_ENABLED
	{
		OID_CUSTOM_BT_COEXIST_CTRL,
		DISP_STRING("OID_CUSTOM_BT_COEXIST_CTRL"),
		FALSE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(PARAM_CUSTOM_BT_COEXIST_T),
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetBtCoexistCtrl
	},
#endif

	{
		OID_CUSTOM_POWER_MANAGEMENT_PROFILE,
		DISP_STRING("OID_CUSTOM_POWER_MANAGEMENT_PROFILE"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryPwrMgmtProfParam,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetPwrMgmtProfParam},
	{
		OID_CUSTOM_PATTERN_CONFIG,
		DISP_STRING("OID_CUSTOM_PATTERN_CONFIG"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(PARAM_CUSTOM_PATTERN_SEARCH_CONFIG_STRUCT_T),
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetPatternConfig
	},
	{
		OID_CUSTOM_BG_SSID_SEARCH_CONFIG,
		DISP_STRING("OID_CUSTOM_BG_SSID_SEARCH_CONFIG"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetBgSsidParam
	},
	{
		OID_CUSTOM_VOIP_SETUP,
		DISP_STRING("OID_CUSTOM_VOIP_SETUP"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryVoipConnectionStatus,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetVoipConnectionStatus
	},
	{
		OID_CUSTOM_ADD_TS,
		DISP_STRING("OID_CUSTOM_ADD_TS"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidAddTS
	},
	{
		OID_CUSTOM_DEL_TS,
		DISP_STRING("OID_CUSTOM_DEL_TS"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidDelTS
	},

#if CFG_LP_PATTERN_SEARCH_SLT
	{
		OID_CUSTOM_SLT,
		DISP_STRING("OID_CUSTOM_SLT"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQuerySltResult,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetSltMode
	},
#endif

	{
		OID_CUSTOM_ROAMING_EN,
		DISP_STRING("OID_CUSTOM_ROAMING_EN"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryRoamingFunction,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetRoamingFunction},
	{
		OID_CUSTOM_WMM_PS_TEST,
		DISP_STRING("OID_CUSTOM_WMM_PS_TEST"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetWiFiWmmPsTest
	},
	{
		OID_CUSTOM_COUNTRY_STRING,
		DISP_STRING("OID_CUSTOM_COUNTRY_STRING"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryCurrentCountry,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetCurrentCountry
	},

#if CFG_SUPPORT_802_11D
	{
		OID_CUSTOM_MULTI_DOMAIN_CAPABILITY,
		DISP_STRING("OID_CUSTOM_MULTI_DOMAIN_CAPABILITY"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryMultiDomainCap,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetMultiDomainCap
	},
#endif

	{
		OID_CUSTOM_GPIO2_MODE,
		DISP_STRING("OID_CUSTOM_GPIO2_MODE"),
		FALSE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(ENUM_PARAM_GPIO2_MODE_T),
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetGPIO2Mode},
	{
		OID_CUSTOM_CONTINUOUS_POLL,
		DISP_STRING("OID_CUSTOM_CONTINUOUS_POLL"),
		FALSE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(PARAM_CONTINUOUS_POLL_T),
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryContinuousPollInterval,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetContinuousPollProfile
	},
	{
		OID_CUSTOM_DISABLE_BEACON_DETECTION,
		DISP_STRING("OID_CUSTOM_DISABLE_BEACON_DETECTION"),
		FALSE, TRUE, ENUM_OID_DRIVER_CORE, 4,
		(PFN_OID_HANDLER_FUNC_REQ)
			wlanoidQueryDisableBeaconDetectionFunc,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetDisableBeaconDetectionFunc
	},

	/* WPS */
	{
		OID_CUSTOM_DISABLE_PRIVACY_CHECK,
		DISP_STRING("OID_CUSTOM_DISABLE_PRIVACY_CHECK"),
		FALSE, TRUE, ENUM_OID_DRIVER_CORE, 4,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetDisablePriavcyCheck
	},
#endif

	{
		OID_CUSTOM_MCR_RW,
		DISP_STRING("OID_CUSTOM_MCR_RW"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_CUSTOM_MCR_RW_STRUCT),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryMcrRead,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetMcrWrite}
	,

	{
		OID_CUSTOM_EEPROM_RW,
		DISP_STRING("OID_CUSTOM_EEPROM_RW"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_CUSTOM_EEPROM_RW_STRUCT),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryEepromRead,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetEepromWrite
	}
	,

	{
		OID_CUSTOM_SW_CTRL,
		DISP_STRING("OID_CUSTOM_SW_CTRL"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_CUSTOM_SW_CTRL_STRUCT),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQuerySwCtrlRead,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetSwCtrlWrite
	}
	,

	{
		OID_CUSTOM_MEM_DUMP,
		DISP_STRING("OID_CUSTOM_MEM_DUMP"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_CUSTOM_MEM_DUMP_STRUCT),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryMemDump,
		NULL
	}
	,

	{
		OID_CUSTOM_TEST_MODE,
		DISP_STRING("OID_CUSTOM_TEST_MODE"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetTestMode
	}
	,

#if 0
	{
		OID_CUSTOM_TEST_RX_STATUS,
		DISP_STRING("OID_CUSTOM_TEST_RX_STATUS"),
		FALSE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_CUSTOM_RFTEST_RX_STATUS_STRUCT),
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryRfTestRxStatus,
		NULL
	},
	{
		OID_CUSTOM_TEST_TX_STATUS,
		DISP_STRING("OID_CUSTOM_TEST_TX_STATUS"),
		FALSE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_CUSTOM_RFTEST_TX_STATUS_STRUCT),
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryRfTestTxStatus,
		NULL
	},
#endif
	{
		OID_CUSTOM_ABORT_TEST_MODE,
		DISP_STRING("OID_CUSTOM_ABORT_TEST_MODE"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetAbortTestMode
	}
	,
	{
		OID_CUSTOM_MTK_WIFI_TEST,
		DISP_STRING("OID_CUSTOM_MTK_WIFI_TEST"),
		/* PeiHsuan Temp Remove this check for workaround Gen2/Gen3 EM
		 * Mode Modification
		 */
		/* TRUE, TRUE, ENUM_OID_DRIVER_CORE,
		 * sizeof(PARAM_MTK_WIFI_TEST_STRUCT_T),
		 */
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestQueryAutoTest,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetAutoTest
	}
	,
	{
		OID_CUSTOM_TEST_ICAP_MODE,
		DISP_STRING("OID_CUSTOM_TEST_ICAP_MODE"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetTestIcapMode
	}
	,

	/* OID_CUSTOM_EMULATION_VERSION_CONTROL */

	/* BWCS */
#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	{
		OID_CUSTOM_BWCS_CMD,
		DISP_STRING("OID_CUSTOM_BWCS_CMD"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(struct PTA_IPC),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryBT,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetBT
	}
	,
#endif
#if 0
	{
		OID_CUSTOM_SINGLE_ANTENNA,
		DISP_STRING("OID_CUSTOM_SINGLE_ANTENNA"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 4,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryBtSingleAntenna,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetBtSingleAntenna
	},
	{
		OID_CUSTOM_SET_PTA,
		DISP_STRING("OID_CUSTOM_SET_PTA"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 4,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryPta,
		(PFN_OID_HANDLER_FUNC_REQ)wlanoidSetPta
	},
#endif

	{
		OID_CUSTOM_MTK_NVRAM_RW,
		DISP_STRING("OID_CUSTOM_MTK_NVRAM_RW"),
		TRUE, TRUE, ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_CUSTOM_EEPROM_RW_STRUCT),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryNvramRead,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetNvramWrite}
	,

	{
		OID_CUSTOM_CFG_SRC_TYPE,
		DISP_STRING("OID_CUSTOM_CFG_SRC_TYPE"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE,
		sizeof(enum ENUM_CFG_SRC_TYPE),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryCfgSrcType,
		NULL
	}
	,

	{
		OID_CUSTOM_EEPROM_TYPE,
		DISP_STRING("OID_CUSTOM_EEPROM_TYPE"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE,
		sizeof(enum ENUM_EEPROM_TYPE),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryEepromType,
		NULL
	}
	,

#if CFG_SUPPORT_WAPI
	{
		OID_802_11_WAPI_MODE,
		DISP_STRING("OID_802_11_WAPI_MODE"),
		FALSE, TRUE, ENUM_OID_DRIVER_CORE, 4,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWapiMode
	}
	,
	{
		OID_802_11_WAPI_ASSOC_INFO,
		DISP_STRING("OID_802_11_WAPI_ASSOC_INFO"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWapiAssocInfo
	}
	,
	{
		OID_802_11_SET_WAPI_KEY,
		DISP_STRING("OID_802_11_SET_WAPI_KEY"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_WPI_KEY),
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWapiKey
	}
	,
#endif

#if CFG_SUPPORT_LOWLATENCY_MODE
	/* Note: we should put following code in order */
	{
		OID_CUSTOM_LOWLATENCY_MODE,	/* 0xFFA0CC00 */
		DISP_STRING("OID_CUSTOM_LOWLATENCY_MODE"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(uint32_t),
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetLowLatencyMode
	}
	,
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

	{
		OID_IPC_WIFI_LOG_UI,
		DISP_STRING("OID_IPC_WIFI_LOG_UI"),
		FALSE,
		FALSE,
		ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_WIFI_LOG_LEVEL_UI),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryWifiLogLevelSupport,
		NULL
	}
	,

	{
		OID_IPC_WIFI_LOG_LEVEL,
		DISP_STRING("OID_IPC_WIFI_LOG_LEVEL"),
		FALSE,
		FALSE,
		ENUM_OID_DRIVER_CORE,
		sizeof(struct PARAM_WIFI_LOG_LEVEL),
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryWifiLogLevel,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWifiLogLevel
	}
	,
#if CFG_SUPPORT_ANT_SWAP
	{
	OID_CUSTOM_QUERY_ANT_SWAP_CAPABILITY,	/* 0xFFA0CD00 */
	DISP_STRING("OID_CUSTOM_QUERY_ANT_SWAP_CAPABILITY"),
	FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(uint32_t),
	(PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryAntennaSwap,
	NULL
	}
	,
#endif
};

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

static int compat_priv(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
		 IN union iwreq_data *prIwReqData, IN char *pcExtra,
	     int (*priv_func)(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
		 IN union iwreq_data *prIwReqData, IN char *pcExtra))
{
	struct iw_point *prIwp;
	int ret = 0;
#ifdef CONFIG_COMPAT
	struct compat_iw_point *iwp_compat = NULL;
	struct iw_point iwp = {0};
#endif

	if (!prIwReqData)
		return -EINVAL;

#ifdef CONFIG_COMPAT
	if (prIwReqInfo->flags & IW_REQUEST_FLAG_COMPAT) {
		iwp_compat = (struct compat_iw_point *) &prIwReqData->data;
		iwp.pointer = compat_ptr(iwp_compat->pointer);
		iwp.length = iwp_compat->length;
		iwp.flags = iwp_compat->flags;
		prIwp = &iwp;
	} else
#endif
	prIwp = &prIwReqData->data;


	ret = priv_func(prNetDev, prIwReqInfo,
				(union iwreq_data *)prIwp, pcExtra);

#ifdef CONFIG_COMPAT
	if (prIwReqInfo->flags & IW_REQUEST_FLAG_COMPAT) {
		iwp_compat->pointer = ptr_to_compat(iwp.pointer);
		iwp_compat->length = iwp.length;
		iwp_compat->flags = iwp.flags;
	}
#endif
	return ret;
}

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
int priv_support_ioctl(IN struct net_device *prNetDev,
		       IN OUT struct ifreq *prIfReq, IN int i4Cmd)
{
	/* prIfReq is verified in the caller function wlanDoIOCTL() */
	struct iwreq *prIwReq = (struct iwreq *)prIfReq;
	struct iw_request_info rIwReqInfo;

	/* prNetDev is verified in the caller function wlanDoIOCTL() */

	/* Prepare the call */
	rIwReqInfo.cmd = (__u16) i4Cmd;
	rIwReqInfo.flags = 0;

	switch (i4Cmd) {
	case IOCTL_SET_INT:
		/* NOTE(Kevin): 1/3 INT Type <= IFNAMSIZ, so we don't need
		 *              copy_from/to_user()
		 */
		return priv_set_int(prNetDev, &rIwReqInfo, &(prIwReq->u),
				    (char *) &(prIwReq->u));

	case IOCTL_GET_INT:
		/* NOTE(Kevin): 1/3 INT Type <= IFNAMSIZ, so we don't need
		 *              copy_from/to_user()
		 */
		return priv_get_int(prNetDev, &rIwReqInfo, &(prIwReq->u),
				    (char *) &(prIwReq->u));

	case IOCTL_SET_STRUCT:
	case IOCTL_SET_STRUCT_FOR_EM:
		return priv_set_struct(prNetDev, &rIwReqInfo, &prIwReq->u,
				       (char *) &(prIwReq->u));

	case IOCTL_GET_STRUCT:
		return priv_get_struct(prNetDev, &rIwReqInfo, &prIwReq->u,
				       (char *) &(prIwReq->u));

#if (CFG_SUPPORT_QA_TOOL)
	case IOCTL_QA_TOOL_DAEMON:
	case SIOCDEVPRIVATE+2:
		return priv_qa_agent(prNetDev, &rIwReqInfo, &(prIwReq->u),
				     (char *) &(prIwReq->u));
#endif

	/* This case need to fall through */
	case IOC_AP_GET_STA_LIST:
	/* This case need to fall through */
	case IOC_AP_SET_MAC_FLTR:
	/* This case need to fall through */
	case IOC_AP_SET_CFG:
	/* This case need to fall through */
	case IOC_AP_STA_DISASSOC:
	/* This case need to fall through */
	case IOC_AP_SET_NSS:
		return priv_set_ap(prNetDev, &rIwReqInfo, &(prIwReq->u),
				     (char *) &(prIwReq->u));

	case IOCTL_GET_STR:

	default:
		return -EOPNOTSUPP;

	}			/* end of switch */

}				/* priv_support_ioctl */

#if CFG_SUPPORT_BATCH_SCAN

struct EVENT_BATCH_RESULT
	g_rEventBatchResult[CFG_BATCH_MAX_MSCAN];

uint32_t batchChannelNum2Freq(uint32_t u4ChannelNum)
{
	uint32_t u4ChannelInMHz;

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
static uint8_t text1[TMP_TEXT_LEN_S], text2[TMP_TEXT_LEN_L],
	       text3[TMP_TEXT_LEN_L]; /* A safe len */

uint32_t
batchConvertResult(IN struct EVENT_BATCH_RESULT
		   *prEventBatchResult,
		   OUT void *pvBuffer, IN uint32_t u4MaxBufferLen,
		   OUT uint32_t *pu4RetLen)
{
	int8_t *p = pvBuffer;
	int8_t ssid[ELEM_MAX_LEN_SSID + 1];
	int32_t nsize, nsize1, nsize2, nsize3, scancount;
	int32_t i, j, nleft;
	uint32_t freq;

	struct EVENT_BATCH_RESULT_ENTRY *prEntry;
	struct EVENT_BATCH_RESULT *pBr;

	nsize = 0;
	nleft = u4MaxBufferLen - 5;	/* -5 for "----\n" */

	pBr = prEventBatchResult;
	scancount = 0;
	for (j = 0; j < CFG_BATCH_MAX_MSCAN; j++) {
		scancount += pBr->ucScanCount;
		pBr++;
	}

	nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S,
			     "scancount=%d\nnextcount=%d\n", scancount,
			     scancount);
	if (nsize1 < nleft) {
		kalStrnCpy(p, text1, nsize1);
		p += nsize1;
		nleft -= nsize1;
	} else
		goto short_buf;

	pBr = prEventBatchResult;
	for (j = 0; j < CFG_BATCH_MAX_MSCAN; j++) {
		DBGLOG(SCN, TRACE,
		       "convert mscan = %d, apcount=%d, nleft=%d\n", j,
		       pBr->ucScanCount, nleft);

		if (pBr->ucScanCount == 0) {
			pBr++;
			continue;
		}

		nleft -= 5;	/* -5 for "####\n" */

		/* We only support one round scan result now. */
		nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S, "apcount=%d\n",
				     pBr->ucScanCount);
		if (nsize1 < nleft) {
			kalStrnCpy(p, text1, nsize1);
			p += nsize1;
			nleft -= nsize1;
		} else
			goto short_buf;

		for (i = 0; i < pBr->ucScanCount; i++) {
			prEntry = &pBr->arBatchResult[i];

			nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S,
					     "bssid=" MACSTR "\n",
					     MAC2STR(prEntry->aucBssid));
			kalMemCopy(ssid,
				   prEntry->aucSSID,
				   (prEntry->ucSSIDLen < ELEM_MAX_LEN_SSID ?
				    prEntry->ucSSIDLen : ELEM_MAX_LEN_SSID));
			ssid[(prEntry->ucSSIDLen <
			      (ELEM_MAX_LEN_SSID - 1) ? prEntry->ucSSIDLen :
			      (ELEM_MAX_LEN_SSID - 1))] = '\0';
			nsize2 = kalSnprintf(text2, TMP_TEXT_LEN_L, "ssid=%s\n",
					     ssid);

			freq = batchChannelNum2Freq(prEntry->ucFreq);
			nsize3 =
				kalSnprintf(text3, TMP_TEXT_LEN_L,
					"freq=%u\nlevel=%d\ndist=%u\ndistSd=%u\n====\n",
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
				DBGLOG(SCN, TRACE,
				       "Warning: Early break! (%d)\n", i);
				break;	/* discard following entries,
					 * TODO: apcount?
					 */
			}
		}

		nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S, "%s", "####\n");
		if (nsize1 < nleft) {
			kalStrnCpy(p, text1, nsize1);
			p += nsize1;
			nleft -= nsize1;
		}

		pBr++;
	}

	nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S, "%s", "----\n");
	if (nsize1 < nleft) {
		kalStrnCpy(p, text1, nsize1);
		p += nsize1;
		nleft -= nsize1;
	}

	*pu4RetLen = u4MaxBufferLen - nleft;
	DBGLOG(SCN, TRACE, "total len = %d (max len = %d)\n",
	       *pu4RetLen, u4MaxBufferLen);

	return WLAN_STATUS_SUCCESS;

short_buf:
	DBGLOG(SCN, TRACE,
	       "Short buffer issue! %d > %d, %s\n",
	       u4MaxBufferLen + (nsize - nleft), u4MaxBufferLen,
	       (char *)pvBuffer);
	return WLAN_STATUS_INVALID_LENGTH;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief Private ioctl set int handler.
 *
 * \param[in] prNetDev Net device requested.
 * \param[in] prIwReqInfo Pointer to iwreq structure.
 * \param[in] prIwReqData The ioctl data structure, use the field of
 *            sub-command.
 * \param[in] pcExtra The buffer with input value
 *
 * \retval 0 For success.
 * \retval -EOPNOTSUPP If cmd is not supported.
 * \retval -EINVAL If a value is out of range.
 *
 */
/*----------------------------------------------------------------------------*/
int
__priv_set_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	uint32_t u4SubCmd;
	uint32_t *pu4IntBuf;
	struct NDIS_TRANSPORT_STRUCT *prNdisReq;
	struct GLUE_INFO *prGlueInfo;
	uint32_t u4BufLen = 0;
	int status = 0;
	struct PTA_IPC *prPtaIpc;

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);

	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	u4SubCmd = (uint32_t) prIwReqData->mode;
	pu4IntBuf = (uint32_t *) pcExtra;

	switch (u4SubCmd) {
	case PRIV_CMD_TEST_MODE:
		/* printk("TestMode=%ld\n", pu4IntBuf[1]); */
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

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
		/* printk("CMD=0x%08lx, data=0x%08lx\n", pu4IntBuf[1],
		 *        pu4IntBuf[2]);
		 */
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_MTK_WIFI_TEST;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		/* Execute this OID */
		status = priv_set_ndis(prNetDev, prNdisReq, &u4BufLen);
		break;

#if CFG_SUPPORT_PRIV_MCR_RW
	case PRIV_CMD_ACCESS_MCR:
		/* printk("addr=0x%08lx, data=0x%08lx\n", pu4IntBuf[1],
		 *        pu4IntBuf[2]);
		 */
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

		if (!prGlueInfo->fgMcrAccessAllowed) {
			if (pu4IntBuf[1] == PRIV_CMD_TEST_MAGIC_KEY
			    && pu4IntBuf[2] == PRIV_CMD_TEST_MAGIC_KEY)
				prGlueInfo->fgMcrAccessAllowed = TRUE;
			status = 0;
			break;
		}

		if (pu4IntBuf[1] == PRIV_CMD_TEST_MAGIC_KEY
		    && pu4IntBuf[2] == PRIV_CMD_TEST_MAGIC_KEY) {
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
		/* printk("addr=0x%08lx, data=0x%08lx\n", pu4IntBuf[1],
		 *        pu4IntBuf[2]);
		 */
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

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
		rStatus = wlanSetInformation(prGlueInfo->prAdapter,
				wlanoidSetBeaconInterval, (void *)&pu4IntBuf[1],
				sizeof(uint32_t), &u4BufLen);
		break;
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	case PRIV_CMD_CSUM_OFFLOAD: {
		uint32_t u4CSUMFlags;

		if (pu4IntBuf[1] == 1)
			u4CSUMFlags = CSUM_OFFLOAD_EN_ALL;
		else if (pu4IntBuf[1] == 0)
			u4CSUMFlags = 0;
		else
			return -EINVAL;

		if (kalIoctl(prGlueInfo, wlanoidSetCSUMOffload,
		    (void *)&u4CSUMFlags, sizeof(uint32_t), FALSE, FALSE, TRUE,
		    &u4BufLen) == WLAN_STATUS_SUCCESS) {
			if (pu4IntBuf[1] == 1)
				prNetDev->features |= NETIF_F_IP_CSUM |
						      NETIF_F_IPV6_CSUM |
						      NETIF_F_RXCSUM;
			else if (pu4IntBuf[1] == 0)
				prNetDev->features &= ~(NETIF_F_IP_CSUM |
							NETIF_F_IPV6_CSUM |
							NETIF_F_RXCSUM);
		}
	}
	break;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	case PRIV_CMD_POWER_MODE: {
		struct PARAM_POWER_MODE_ rPowerMode;

		rPowerMode.ePowerMode = (enum PARAM_POWER_MODE)
					pu4IntBuf[1];
		rPowerMode.ucBssIdx = wlanGetBssIdx(prNetDev);

		/* pu4IntBuf[0] is used as input SubCmd */
		kalIoctl(prGlueInfo, wlanoidSet802dot11PowerSaveProfile,
			 &rPowerMode, sizeof(struct PARAM_POWER_MODE_),
			 FALSE, FALSE, TRUE, &u4BufLen);
	}
	break;

	case PRIV_CMD_WMM_PS: {
		struct PARAM_CUSTOM_WMM_PS_TEST_STRUCT rWmmPsTest;

		rWmmPsTest.bmfgApsdEnAc = (uint8_t) pu4IntBuf[1];
		rWmmPsTest.ucIsEnterPsAtOnce = (uint8_t) pu4IntBuf[2];
		rWmmPsTest.ucIsDisableUcTrigger = (uint8_t) pu4IntBuf[3];
		rWmmPsTest.reserved = 0;
		rWmmPsTest.ucBssIdx = wlanGetBssIdx(prNetDev);
		kalIoctl(prGlueInfo, wlanoidSetWiFiWmmPsTest,
			 (void *)&rWmmPsTest,
			 sizeof(struct PARAM_CUSTOM_WMM_PS_TEST_STRUCT),
			 FALSE, FALSE, TRUE, &u4BufLen);
	}
	break;

#if 0
	case PRIV_CMD_ADHOC_MODE:
		/* pu4IntBuf[0] is used as input SubCmd */
		rStatus = wlanSetInformation(prGlueInfo->prAdapter,
				wlanoidSetAdHocMode, (void *)&pu4IntBuf[1],
				sizeof(uint32_t), &u4BufLen);
		break;
#endif

	case PRIV_CUSTOM_BWCS_CMD:

		DBGLOG(REQ, INFO,
		       "pu4IntBuf[1] = %x, size of struct PTA_IPC = %d.\n",
		       pu4IntBuf[1], (uint32_t) sizeof(struct PTA_IPC));

		prPtaIpc = (struct PTA_IPC *) aucOidBuf;
		prPtaIpc->u.aucBTPParams[0] = (uint8_t) (pu4IntBuf[1] >>
					      24);
		prPtaIpc->u.aucBTPParams[1] = (uint8_t) (pu4IntBuf[1] >>
					      16);
		prPtaIpc->u.aucBTPParams[2] = (uint8_t) (pu4IntBuf[1] >> 8);
		prPtaIpc->u.aucBTPParams[3] = (uint8_t) (pu4IntBuf[1]);

		DBGLOG(REQ, INFO,
		       "BCM BWCS CMD : PRIV_CUSTOM_BWCS_CMD : aucBTPParams[0] = %02x, aucBTPParams[1] = %02x.\n",
		       prPtaIpc->u.aucBTPParams[0],
		       prPtaIpc->u.aucBTPParams[1]);
		DBGLOG(REQ, INFO,
		       "BCM BWCS CMD : PRIV_CUSTOM_BWCS_CMD : aucBTPParams[2] = %02x, aucBTPParams[3] = %02x.\n",
		       prPtaIpc->u.aucBTPParams[2],
		       prPtaIpc->u.aucBTPParams[3]);

#if 0
		status = wlanSetInformation(prGlueInfo->prAdapter, wlanoidSetBT,
				(void *)&aucOidBuf[0], u4CmdLen, &u4BufLen);
#endif

		status = wlanoidSetBT(prGlueInfo->prAdapter,
				(void *)&aucOidBuf[0], sizeof(struct PTA_IPC),
				&u4BufLen);

		if (status != WLAN_STATUS_SUCCESS)
			status = -EFAULT;

		break;

	case PRIV_CMD_BAND_CONFIG: {
		DBGLOG(INIT, INFO, "CMD set_band = %u\n",
		       (uint32_t) pu4IntBuf[1]);
	}
	break;

#if CFG_ENABLE_WIFI_DIRECT
	case PRIV_CMD_P2P_MODE: {
		struct PARAM_CUSTOM_P2P_SET_STRUCT rSetP2P;
		uint32_t rWlanStatus = WLAN_STATUS_SUCCESS;

		rSetP2P.u4Enable = pu4IntBuf[1];
		rSetP2P.u4Mode = pu4IntBuf[2];
#if 1
		if (!rSetP2P.u4Enable)
			p2pNetUnregister(prGlueInfo, TRUE);

		/* pu4IntBuf[0] is used as input SubCmd */
		rWlanStatus = kalIoctl(prGlueInfo, wlanoidSetP2pMode,
				(void *)&rSetP2P,
				sizeof(struct PARAM_CUSTOM_P2P_SET_STRUCT),
				FALSE, FALSE, TRUE, &u4BufLen);

		if ((rSetP2P.u4Enable)
		    && (rWlanStatus == WLAN_STATUS_SUCCESS))
			p2pNetRegister(prGlueInfo, TRUE);
#endif

	}
	break;
#endif

#if (CFG_MET_PACKET_TRACE_SUPPORT == 1)
	case PRIV_CMD_MET_PROFILING: {
		/* PARAM_CUSTOM_WFD_DEBUG_STRUCT_T rWfdDebugModeInfo; */
		/* rWfdDebugModeInfo.ucWFDDebugMode=(UINT_8)pu4IntBuf[1]; */
		/* rWfdDebugModeInfo.u2SNPeriod=(UINT_16)pu4IntBuf[2]; */
		/* DBGLOG(REQ, INFO, ("WFD Debug Mode:%d Period:%d\n",
		 *  rWfdDebugModeInfo.ucWFDDebugMode,
		 *  rWfdDebugModeInfo.u2SNPeriod));
		 */
		prGlueInfo->fgMetProfilingEn = (uint8_t) pu4IntBuf[1];
		prGlueInfo->u2MetUdpPort = (uint16_t) pu4IntBuf[2];
		/* DBGLOG(INIT, INFO, ("MET_PROF: Enable=%d UDP_PORT=%d\n",
		 *  prGlueInfo->fgMetProfilingEn, prGlueInfo->u2MetUdpPort);
		 */

	}
	break;

#endif
	case PRIV_CMD_SET_SER:
		kalIoctl(prGlueInfo, wlanoidSetSer, (void *)&pu4IntBuf[1],
			 sizeof(uint32_t), FALSE, FALSE, TRUE, &u4BufLen);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return status;
}				/* __priv_set_int */

int
priv_set_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	DBGLOG(REQ, LOUD, "cmd=%x, flags=%x\n",
	     prIwReqInfo->cmd, prIwReqInfo->flags);
	DBGLOG(REQ, LOUD, "mode=%x, flags=%x\n",
	     prIwReqData->mode, prIwReqData->data.flags);

	return compat_priv(prNetDev, prIwReqInfo,
	     prIwReqData, pcExtra, __priv_set_int);
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
__priv_get_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	uint32_t u4SubCmd;
	uint32_t *pu4IntBuf;
	struct GLUE_INFO *prGlueInfo;
	uint32_t u4BufLen = 0;
	int status = 0;
	struct NDIS_TRANSPORT_STRUCT *prNdisReq;
	int32_t ch[50] = {0};

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	u4SubCmd = (uint32_t) prIwReqData->mode;
	pu4IntBuf = (uint32_t *) pcExtra;

	switch (u4SubCmd) {
	case PRIV_CMD_TEST_CMD:
		/* printk("CMD=0x%08lx, data=0x%08lx\n", pu4IntBuf[1],
		 *        pu4IntBuf[2]);
		 */
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_MTK_WIFI_TEST;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0) {
			/* printk("Result=%ld\n",
			 *        *(uint32_t *)&prNdisReq->ndisOidContent[4]);
			 */
			prIwReqData->mode = *(uint32_t *)
					    &prNdisReq->ndisOidContent[4];
			/*
			 *  if (copy_to_user(prIwReqData->data.pointer,
			 *      &prNdisReq->ndisOidContent[4], 4)) {
			 *      printk(KERN_NOTICE
			 *      "priv_get_int() copy_to_user oidBuf fail(3)\n");
			 *      return -EFAULT;
			 *  }
			 */
		}
		return status;

#if CFG_SUPPORT_PRIV_MCR_RW
	case PRIV_CMD_ACCESS_MCR:
		/* printk("addr=0x%08lx\n", pu4IntBuf[1]); */
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

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
			/* printk("Result=%ld\n",
			 *        *(uint32_t *)&prNdisReq->ndisOidContent[4]);
			 */
			prIwReqData->mode = *(uint32_t *)
					    &prNdisReq->ndisOidContent[4];
		}
		return status;
#endif

	case PRIV_CMD_DUMP_MEM:
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

#if 1
		if (!prGlueInfo->fgMcrAccessAllowed) {
			status = 0;
			return status;
		}
#endif
		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_MEM_DUMP;
		prNdisReq->inNdisOidlength = sizeof(struct
						PARAM_CUSTOM_MEM_DUMP_STRUCT);
		prNdisReq->outNdisOidLength = sizeof(struct
						PARAM_CUSTOM_MEM_DUMP_STRUCT);

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0)
			prIwReqData->mode = *(uint32_t *)
					    &prNdisReq->ndisOidContent[0];
		return status;

	case PRIV_CMD_SW_CTRL:
		/* printk(" addr=0x%08lx\n", pu4IntBuf[1]); */

		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_SW_CTRL;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0) {
			/* printk("Result=%ld\n",
			 *        *(uint32_t *)&prNdisReq->ndisOidContent[4]);
			 */
			prIwReqData->mode = *(uint32_t *)
					    &prNdisReq->ndisOidContent[4];
		}
		return status;

#if 0
	case PRIV_CMD_BEACON_PERIOD:
		status = wlanQueryInformation(prGlueInfo->prAdapter,
				wlanoidQueryBeaconInterval, (void *) pu4IntBuf,
				sizeof(uint32_t), &u4BufLen);
		return status;

	case PRIV_CMD_POWER_MODE:
		status = wlanQueryInformation(prGlueInfo->prAdapter,
				wlanoidQuery802dot11PowerSaveProfile,
				(void *)pu4IntBuf, sizeof(uint32_t), &u4BufLen);
		return status;

	case PRIV_CMD_ADHOC_MODE:
		status = wlanQueryInformation(prGlueInfo->prAdapter,
				wlanoidQueryAdHocMode, (void *) pu4IntBuf,
				sizeof(uint32_t), &u4BufLen);
		return status;
#endif

	case PRIV_CMD_BAND_CONFIG:
		DBGLOG(INIT, INFO, "CMD get_band=\n");
		prIwReqData->mode = 0;
		return status;

	default:
		break;
	}

	u4SubCmd = (uint32_t) prIwReqData->data.flags;

	switch (u4SubCmd) {
	case PRIV_CMD_GET_CH_LIST: {
		uint16_t i, j = 0;
		uint8_t NumOfChannel = 50;
		uint8_t ucMaxChannelNum = 50;
		struct RF_CHANNEL_INFO *aucChannelList;

		DBGLOG(RLM, INFO, "Domain: Query Channel List.\n");
		aucChannelList = (struct RF_CHANNEL_INFO *)
			kalMemAlloc(sizeof(struct RF_CHANNEL_INFO)
				*ucMaxChannelNum, VIR_MEM_TYPE);
		if (!aucChannelList) {
			DBGLOG(REQ, ERROR,
				"Can not alloc memory for rf channel info\n");
			return -ENOMEM;
		}
		kalMemZero(aucChannelList,
			sizeof(struct RF_CHANNEL_INFO)*ucMaxChannelNum);

		kalGetChannelList(prGlueInfo, BAND_NULL, ucMaxChannelNum,
				  &NumOfChannel, aucChannelList);
		if (NumOfChannel > ucMaxChannelNum)
			NumOfChannel = ucMaxChannelNum;

		if (kalIsAPmode(prGlueInfo)) {
			for (i = 0; i < NumOfChannel; i++) {
				if ((aucChannelList[i].ucChannelNum <= 13) ||
				    (aucChannelList[i].ucChannelNum == 36
				     || aucChannelList[i].ucChannelNum == 40
				     || aucChannelList[i].ucChannelNum == 44
				     || aucChannelList[i].ucChannelNum == 48)) {
					ch[j] = (int32_t) aucChannelList[i]
								.ucChannelNum;
					j++;
				}
			}
		} else {
			for (j = 0; j < NumOfChannel; j++)
				ch[j] = (int32_t)aucChannelList[j].ucChannelNum;
		}
		kalMemFree(aucChannelList, VIR_MEM_TYPE,
			sizeof(struct RF_CHANNEL_INFO)*ucMaxChannelNum);

		prIwReqData->data.length = j;
		if (copy_to_user(prIwReqData->data.pointer, ch,
				 NumOfChannel * sizeof(int32_t)))
			return -EFAULT;
		else
			return status;
	}
	case PRIV_CMD_GET_FW_VERSION: {
			uint16_t u2Len = 0;
			struct ADAPTER *prAdapter;

			prAdapter = prGlueInfo->prAdapter;
			if (prAdapter) {
				u2Len = kalStrLen(
					prAdapter->rVerInfo.aucReleaseManifest);
				DBGLOG(REQ, INFO,
					"Get FW manifest version: %d\n", u2Len);
				prIwReqData->data.length = u2Len;
				if (copy_to_user(prIwReqData->data.pointer,
					prAdapter->rVerInfo.aucReleaseManifest,
					u2Len * sizeof(uint8_t)))
					return -EFAULT;
				else
					return status;
			}
			DBGLOG(REQ, WARN, "Fail to get FW manifest version\n");
			return status;
		}
	default:
		return -EOPNOTSUPP;
	}

	return status;
}				/* __priv_get_int */

int
priv_get_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	DBGLOG(REQ, LOUD, "cmd=%x, flags=%x\n",
	     prIwReqInfo->cmd, prIwReqInfo->flags);
	DBGLOG(REQ, LOUD, "mode=%x, flags=%x\n",
	     prIwReqData->mode, prIwReqData->data.flags);

	return compat_priv(prNetDev, prIwReqInfo,
	     prIwReqData, pcExtra, __priv_get_int);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Private ioctl set int array handler.
 *
 * \param[in] prNetDev Net device requested.
 * \param[in] prIwReqInfo Pointer to iwreq structure.
 * \param[in] prIwReqData The ioctl data structure, use the field of
 *            sub-command.
 * \param[in] pcExtra The buffer with input value
 *
 * \retval 0 For success.
 * \retval -EOPNOTSUPP If cmd is not supported.
 * \retval -EINVAL If a value is out of range.
 *
 */
/*----------------------------------------------------------------------------*/
int
__priv_set_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo,
	      IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	DBGLOG(REQ, LOUD, "not support now");
	return -EINVAL;
}				/* __priv_set_ints */

int
priv_set_ints(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	DBGLOG(REQ, LOUD, "cmd=%x, flags=%x\n",
	     prIwReqInfo->cmd, prIwReqInfo->flags);
	DBGLOG(REQ, LOUD, "mode=%x, flags=%x\n",
	     prIwReqData->mode, prIwReqData->data.flags);

	return compat_priv(prNetDev, prIwReqInfo,
	     prIwReqData, pcExtra, __priv_set_ints);
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
__priv_get_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo,
	      IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	uint32_t u4SubCmd;
	struct GLUE_INFO *prGlueInfo;
	int status = 0;
	int32_t ch[50];

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	u4SubCmd = (uint32_t) prIwReqData->data.flags;

	switch (u4SubCmd) {
	case PRIV_CMD_GET_CH_LIST: {
		uint16_t i;
		uint8_t NumOfChannel = 50;
		uint8_t ucMaxChannelNum = 50;
		struct RF_CHANNEL_INFO *aucChannelList;

		aucChannelList = (struct RF_CHANNEL_INFO *)
			kalMemAlloc(sizeof(struct RF_CHANNEL_INFO)
				*ucMaxChannelNum, VIR_MEM_TYPE);
		if (!aucChannelList) {
			DBGLOG(REQ, ERROR,
				"Can not alloc memory for rf channel info\n");
			return -ENOMEM;
		}
		kalMemZero(aucChannelList,
			sizeof(struct RF_CHANNEL_INFO)*ucMaxChannelNum);

		kalGetChannelList(prGlueInfo, BAND_NULL, ucMaxChannelNum,
				  &NumOfChannel, aucChannelList);
		if (NumOfChannel > ucMaxChannelNum)
			NumOfChannel = ucMaxChannelNum;

		for (i = 0; i < NumOfChannel; i++)
			ch[i] = (int32_t) aucChannelList[i].ucChannelNum;

		kalMemFree(aucChannelList, VIR_MEM_TYPE,
			sizeof(struct RF_CHANNEL_INFO)*ucMaxChannelNum);

		prIwReqData->data.length = NumOfChannel;
		if (copy_to_user(prIwReqData->data.pointer, ch,
				 NumOfChannel * sizeof(int32_t)))
			return -EFAULT;
		else
			return status;
	}
	default:
		break;
	}

	return status;
}				/* __priv_get_ints */

int
priv_get_ints(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	DBGLOG(REQ, LOUD, "cmd=%x, flags=%x\n",
	     prIwReqInfo->cmd, prIwReqInfo->flags);
	DBGLOG(REQ, LOUD, "mode=%x, flags=%x\n",
	     prIwReqData->mode, prIwReqData->data.flags);

	return compat_priv(prNetDev, prIwReqInfo,
	    prIwReqData, pcExtra, __priv_get_ints);
}

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
__priv_set_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo,
		IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	uint32_t u4SubCmd = 0;
	int status = 0;
	/* WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS; */
	uint32_t u4CmdLen = 0;
	struct NDIS_TRANSPORT_STRUCT *prNdisReq;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen = 0;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	ASSERT(prNetDev);
	/* ASSERT(prIwReqInfo); */
	ASSERT(prIwReqData);
	/* ASSERT(pcExtra); */

	kalMemZero(&aucOidBuf[0], sizeof(aucOidBuf));

	if (GLUE_CHK_PR2(prNetDev, prIwReqData) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	u4SubCmd = (uint32_t) prIwReqData->data.flags;

#if 0
	DBGLOG(INIT, INFO,
	       "priv_set_struct(): prIwReqInfo->cmd(0x%X), u4SubCmd(%ld)\n",
	       prIwReqInfo->cmd, u4SubCmd);
#endif

	switch (u4SubCmd) {
#if 0				/* PTA_ENABLED */
	case PRIV_CMD_BT_COEXIST:
		u4CmdLen = prIwReqData->data.length * sizeof(uint32_t);
		ASSERT(sizeof(PARAM_CUSTOM_BT_COEXIST_T) >= u4CmdLen);
		if (sizeof(PARAM_CUSTOM_BT_COEXIST_T) < u4CmdLen)
			return -EFAULT;

		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer,
		    u4CmdLen)) {
			status = -EFAULT;	/* return -EFAULT; */
			break;
		}

		rStatus = wlanSetInformation(prGlueInfo->prAdapter,
				wlanoidSetBtCoexistCtrl, (void *)&aucOidBuf[0],
				u4CmdLen, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			status = -EFAULT;
		break;
#endif

	case PRIV_CUSTOM_BWCS_CMD:
		u4CmdLen = prIwReqData->data.length * sizeof(uint32_t);
		ASSERT(sizeof(struct PTA_IPC) >= u4CmdLen);
		if (sizeof(struct PTA_IPC) < u4CmdLen)
			return -EFAULT;
#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS && CFG_SUPPORT_BCM_BWCS_DEBUG
		DBGLOG(REQ, INFO,
		       "ucCmdLen = %d, size of struct PTA_IPC = %d, prIwReqData->data = 0x%x.\n",
		       u4CmdLen, sizeof(struct PTA_IPC), prIwReqData->data);

		DBGLOG(REQ, INFO,
		       "priv_set_struct(): prIwReqInfo->cmd(0x%X), u4SubCmd(%ld)\n",
		       prIwReqInfo->cmd,
		       u4SubCmd);
		DBGLOG(REQ, INFO, "*pcExtra = 0x%x\n", *pcExtra);
#endif

		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer,
				   u4CmdLen)) {
			status = -EFAULT;	/* return -EFAULT; */
			break;
		}
#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS && CFG_SUPPORT_BCM_BWCS_DEBUG
		DBGLOG(REQ, INFO,
		       "priv_set_struct(): BWCS CMD = %02x%02x%02x%02x\n",
		       aucOidBuf[2], aucOidBuf[3],
		       aucOidBuf[4], aucOidBuf[5]);
#endif

#if 0
		status = wlanSetInformation(prGlueInfo->prAdapter, wlanoidSetBT,
				(void *)&aucOidBuf[0], u4CmdLen, &u4BufLen);
#endif

#if 1
		status = wlanoidSetBT(prGlueInfo->prAdapter,
				(void *)&aucOidBuf[0], u4CmdLen, &u4BufLen);
#endif

		if (status != WLAN_STATUS_SUCCESS)
			status = -EFAULT;

		break;

#if CFG_SUPPORT_WPS2
	case PRIV_CMD_WSC_PROBE_REQ: {
		struct CONNECTION_SETTINGS *prConnSettings =
			aisGetConnSettings(prGlueInfo->prAdapter,
			ucBssIndex);

		/* retrieve IE for Probe Request */
		u4CmdLen = prIwReqData->data.length;
		if (u4CmdLen > GLUE_INFO_WSCIE_LENGTH) {
			DBGLOG(REQ, ERROR, "Input data length is invalid %u\n",
			       u4CmdLen);
			return -EINVAL;
		}

		if (prIwReqData->data.length > 0) {
			if (copy_from_user(prConnSettings->aucWSCIE,
					   prIwReqData->data.pointer,
					   u4CmdLen)) {
				status = -EFAULT;
				break;
			}
			prConnSettings->u2WSCIELen = u4CmdLen;
		} else {
			prConnSettings->u2WSCIELen = 0;
		}
	}
	break;
#endif
	case PRIV_CMD_OID:
		u4CmdLen = prIwReqData->data.length;
		if (u4CmdLen > CMD_OID_BUF_LENGTH) {
			DBGLOG(REQ, ERROR, "Input data length is invalid %u\n",
			       u4CmdLen);
			return -EINVAL;
		}
		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer,
				   u4CmdLen)) {
			status = -EFAULT;
			break;
		}
		if (!kalMemCmp(&aucOidBuf[0], pcExtra, u4CmdLen)) {
			/* ToDo:: DBGLOG */
			DBGLOG(REQ, INFO, "pcExtra buffer is valid\n");
		} else {
			DBGLOG(REQ, INFO, "pcExtra 0x%p\n", pcExtra);
		}
		/* Execute this OID */
		status = priv_set_ndis(prNetDev,
				(struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0],
				&u4BufLen);
		/* Copy result to user space */
		((struct NDIS_TRANSPORT_STRUCT *)
		 &aucOidBuf[0])->outNdisOidLength = u4BufLen;

		if (copy_to_user(prIwReqData->data.pointer, &aucOidBuf[0],
		    OFFSET_OF(struct NDIS_TRANSPORT_STRUCT, ndisOidContent))) {
			DBGLOG(REQ, INFO, "copy_to_user oidBuf fail\n");
			status = -EFAULT;
		}

		break;

	case PRIV_CMD_SW_CTRL:
		u4CmdLen = prIwReqData->data.length;
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

		if (u4CmdLen > sizeof(prNdisReq->ndisOidContent)) {
			DBGLOG(REQ, ERROR, "Input data length is invalid %u\n",
			       u4CmdLen);
			return -EINVAL;
		}

		if (copy_from_user(&prNdisReq->ndisOidContent[0],
				   prIwReqData->data.pointer, u4CmdLen)) {
			status = -EFAULT;
			break;
		}
		prNdisReq->ndisOidCmd = OID_CUSTOM_SW_CTRL;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		/* Execute this OID */
		status = priv_set_ndis(prNetDev, prNdisReq, &u4BufLen);
		break;

	case PRIV_CMD_GET_WIFI_TYPE:
		{
			int32_t i4ResultLen;

			u4CmdLen = prIwReqData->data.length;
			if (u4CmdLen >= CMD_OID_BUF_LENGTH) {
				DBGLOG(REQ, ERROR,
				       "u4CmdLen:%u >= CMD_OID_BUF_LENGTH:%d\n",
				       u4CmdLen, CMD_OID_BUF_LENGTH);
				return -EINVAL;
			}
			if (copy_from_user(&aucOidBuf[0],
					   prIwReqData->data.pointer,
					   u4CmdLen)) {
				DBGLOG(REQ, ERROR, "copy_from_user fail\n");
				return -EFAULT;
			}
			aucOidBuf[u4CmdLen] = 0;
			i4ResultLen = priv_driver_cmds(prNetDev, aucOidBuf,
						       u4CmdLen);
			if (i4ResultLen > 1) {
				if (copy_to_user(prIwReqData->data.pointer,
						 &aucOidBuf[0], i4ResultLen)) {
					DBGLOG(REQ, ERROR,
					       "copy_to_user fail\n");
					return -EFAULT;
				}
				prIwReqData->data.length = i4ResultLen;
			} else {
				DBGLOG(REQ, ERROR,
				       "i4ResultLen:%d <= 1\n", i4ResultLen);
				return -EFAULT;
			}
		}
		break; /* case PRIV_CMD_GET_WIFI_TYPE: */

	/* dynamic tx power control */
	case PRIV_CMD_SET_PWR_CTRL:
		{
			char *pCommand = NULL;

			u4CmdLen = prIwReqData->data.length;
			if (u4CmdLen >= CMD_OID_BUF_LENGTH)
				return -EINVAL;
			if (copy_from_user(&aucOidBuf[0],
					   prIwReqData->data.pointer,
					   u4CmdLen)) {
				status = -EFAULT;
				break;
			}
			aucOidBuf[u4CmdLen] = 0;
			if (strlen(aucOidBuf) <= 0) {
				status = -EFAULT;
				break;
			}
			pCommand = kalMemAlloc(u4CmdLen + 1, VIR_MEM_TYPE);
			if (pCommand == NULL) {
				DBGLOG(REQ, INFO, "alloc fail\n");
				return -EINVAL;
			}
			kalMemZero(pCommand, u4CmdLen + 1);
			kalMemCopy(pCommand, aucOidBuf, u4CmdLen);
			priv_driver_cmds(prNetDev, pCommand, u4CmdLen);
			kalMemFree(pCommand, VIR_MEM_TYPE, i4TotalLen);
		}
		break;

        case PRIV_CMD_SET_ANT_SWAP_CTRL:
		{
			char *pCommand = NULL;

			u4CmdLen = prIwReqData->data.length;
			if (u4CmdLen >= CMD_OID_BUF_LENGTH)
				return -EINVAL;
			if (copy_from_user(&aucOidBuf[0],
					   prIwReqData->data.pointer,
					   u4CmdLen)) {
				status = -EFAULT;
				break;
			}
			aucOidBuf[u4CmdLen] = 0;
			if (strlen(aucOidBuf) <= 0) {
				status = -EFAULT;
				break;
			}
			pCommand = kalMemAlloc(u4CmdLen + 1, VIR_MEM_TYPE);
			if (pCommand == NULL) {
				DBGLOG(REQ, INFO, "alloc fail\n");
				return -EINVAL;
			}
			kalMemZero(pCommand, u4CmdLen + 1);
			kalMemCopy(pCommand, aucOidBuf, u4CmdLen);
			priv_driver_cmds(prNetDev, pCommand, u4CmdLen);
			kalMemFree(pCommand, VIR_MEM_TYPE, i4TotalLen);
		}
		break;

	default:
		return -EOPNOTSUPP;
	}

	return status;
}				/* __priv_set_struct */

int
priv_set_struct(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	DBGLOG(REQ, LOUD, "cmd=%x, flags=%x\n",
	     prIwReqInfo->cmd, prIwReqInfo->flags);
	DBGLOG(REQ, LOUD, "mode=%x, flags=%x\n",
	     prIwReqData->mode, prIwReqData->data.flags);

	return compat_priv(prNetDev, prIwReqInfo,
	     prIwReqData, pcExtra, __priv_set_struct);
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
__priv_get_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo,
		IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	uint32_t u4SubCmd = 0;
	struct NDIS_TRANSPORT_STRUCT *prNdisReq = NULL;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4BufLen = 0;
	/* uint32_t *pu4IntBuf = NULL; */
	int status = 0;

	kalMemZero(&aucOidBuf[0], sizeof(aucOidBuf));

	ASSERT(prNetDev);
	ASSERT(prIwReqData);
	if (!prNetDev || !prIwReqData) {
		DBGLOG(REQ, INFO,
		       "priv_get_struct(): invalid param(0x%p, 0x%p)\n",
		       prNetDev, prIwReqData);
		return -EINVAL;
	}

	u4SubCmd = (uint32_t) prIwReqData->data.flags;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO,
		       "priv_get_struct(): invalid prGlueInfo(0x%p, 0x%p)\n",
		       prNetDev,
		       *((struct GLUE_INFO **) netdev_priv(prNetDev)));
		return -EINVAL;
	}
#if 0
	DBGLOG(INIT, INFO,
	       "priv_get_struct(): prIwReqInfo->cmd(0x%X), u4SubCmd(%ld)\n",
	       prIwReqInfo->cmd, u4SubCmd);
#endif
	memset(aucOidBuf, 0, sizeof(aucOidBuf));

	switch (u4SubCmd) {
	case PRIV_CMD_OID:
		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer,
				   sizeof(struct NDIS_TRANSPORT_STRUCT))) {
			DBGLOG(REQ, INFO,
			       "priv_get_struct() copy_from_user oidBuf fail\n");
			return -EFAULT;
		}

		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];
#if 0
		DBGLOG(INIT, INFO,
		       "\n priv_get_struct cmd 0x%02x len:%d OID:0x%08x OID Len:%d\n",
		       cmd,
		       pIwReq->u.data.length, ndisReq->ndisOidCmd,
		       ndisReq->inNdisOidlength);
#endif
		if (priv_get_ndis(prNetDev, prNdisReq, &u4BufLen) == 0) {
			prNdisReq->outNdisOidLength = u4BufLen;
			if (copy_to_user(prIwReqData->data.pointer,
			    &aucOidBuf[0], u4BufLen +
			    sizeof(struct NDIS_TRANSPORT_STRUCT) -
			    sizeof(prNdisReq->ndisOidContent))) {
				DBGLOG(REQ, INFO,
				       "priv_get_struct() copy_to_user oidBuf fail(1)\n"
				       );
				return -EFAULT;
			}
			return 0;
		}
		prNdisReq->outNdisOidLength = u4BufLen;
		if (copy_to_user(prIwReqData->data.pointer,
		    &aucOidBuf[0], OFFSET_OF(struct NDIS_TRANSPORT_STRUCT,
						 ndisOidContent))) {
			DBGLOG(REQ, INFO,
			       "priv_get_struct() copy_to_user oidBuf fail(2)\n"
			       );
		}
		return -EFAULT;

	case PRIV_CMD_SW_CTRL:
		/* pu4IntBuf = (uint32_t *) prIwReqData->data.pointer; */
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

		if (prIwReqData->data.length > (sizeof(aucOidBuf) -
		    OFFSET_OF(struct NDIS_TRANSPORT_STRUCT, ndisOidContent))) {
			DBGLOG(REQ, INFO,
			       "priv_get_struct() exceeds length limit\n");
			return -EFAULT;
		}

		/* if (copy_from_user(&prNdisReq->ndisOidContent[0],
		 *     prIwReqData->data.pointer,
		 */
		/* Coverity uanble to detect real size of ndisOidContent,
		 * it's 4084 bytes instead of 16 bytes
		 */
		if (copy_from_user(&aucOidBuf[OFFSET_OF(struct
		    NDIS_TRANSPORT_STRUCT, ndisOidContent)],
				   prIwReqData->data.pointer,
				   prIwReqData->data.length)) {
			DBGLOG(REQ, INFO,
			       "priv_get_struct() copy_from_user oidBuf fail\n");
			return -EFAULT;
		}

		prNdisReq->ndisOidCmd = OID_CUSTOM_SW_CTRL;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0) {
			prNdisReq->outNdisOidLength = u4BufLen;
			/* printk("len=%d Result=%08lx\n", u4BufLen,
			 * *(uint32_t *)&prNdisReq->ndisOidContent[4]);
			 */

			if (copy_to_user(prIwReqData->data.pointer,
					 &prNdisReq->ndisOidContent[4], 4))
				DBGLOG(REQ, INFO,
				       "priv_get_struct() copy_to_user oidBuf fail(2)\n"
				       );
		}
		return 0;
	default:
		DBGLOG(REQ, WARN, "get struct cmd:0x%x\n", u4SubCmd);
		return -EOPNOTSUPP;
	}
}				/* __priv_get_struct */

int
priv_get_struct(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	DBGLOG(REQ, LOUD, "cmd=%x, flags=%x\n",
	     prIwReqInfo->cmd, prIwReqInfo->flags);
	DBGLOG(REQ, LOUD, "mode=%x, flags=%x\n",
	     prIwReqData->mode, prIwReqData->data.flags);

	return compat_priv(prNetDev, prIwReqInfo,
	     prIwReqData, pcExtra, __priv_get_struct);
}

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
priv_set_ndis(IN struct net_device *prNetDev,
	      IN struct NDIS_TRANSPORT_STRUCT *prNdisReq,
	      OUT uint32_t *pu4OutputLen)
{
	struct WLAN_REQ_ENTRY *prWlanReqEntry = NULL;
	uint32_t status = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4SetInfoLen = 0;

	ASSERT(prNetDev);
	ASSERT(prNdisReq);
	ASSERT(pu4OutputLen);

	if (!prNetDev || !prNdisReq || !pu4OutputLen) {
		DBGLOG(REQ, INFO,
		       "priv_set_ndis(): invalid param(0x%p, 0x%p, 0x%p)\n",
		       prNetDev, prNdisReq, pu4OutputLen);
		return -EINVAL;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO,
		       "priv_set_ndis(): invalid prGlueInfo(0x%p, 0x%p)\n",
		       prNetDev,
		       *((struct GLUE_INFO **) netdev_priv(prNetDev)));
		return -EINVAL;
	}
#if 0
	DBGLOG(INIT, INFO,
	       "priv_set_ndis(): prNdisReq->ndisOidCmd(0x%lX)\n",
	       prNdisReq->ndisOidCmd);
#endif

	if (reqSearchSupportedOidEntry(prNdisReq->ndisOidCmd,
				       &prWlanReqEntry) == FALSE) {
		/* WARNLOG(
		 *         ("Set OID: 0x%08lx (unknown)\n",
		 *         prNdisReq->ndisOidCmd));
		 */
		return -EOPNOTSUPP;
	}

	if (prWlanReqEntry->pfOidSetHandler == NULL) {
		/* WARNLOG(
		 *         ("Set %s: Null set handler\n",
		 *         prWlanReqEntry->pucOidName));
		 */
		return -EOPNOTSUPP;
	}
#if 0
	DBGLOG(INIT, INFO, "priv_set_ndis(): %s\n",
	       prWlanReqEntry->pucOidName);
#endif

	if (prWlanReqEntry->fgSetBufLenChecking) {
		if (prNdisReq->inNdisOidlength !=
		    prWlanReqEntry->u4InfoBufLen) {
			DBGLOG(REQ, WARN,
			       "Set %s: Invalid length (current=%d, needed=%d)\n",
			       prWlanReqEntry->pucOidName,
			       prNdisReq->inNdisOidlength,
			       prWlanReqEntry->u4InfoBufLen);

			*pu4OutputLen = prWlanReqEntry->u4InfoBufLen;
			return -EINVAL;
		}
	}

	if (prWlanReqEntry->eOidMethod == ENUM_OID_GLUE_ONLY) {
		/* GLUE sw info only */
		status = prWlanReqEntry->pfOidSetHandler(prGlueInfo,
				prNdisReq->ndisOidContent,
				prNdisReq->inNdisOidlength, &u4SetInfoLen);
	} else if (prWlanReqEntry->eOidMethod ==
		   ENUM_OID_GLUE_EXTENSION) {
		/* multiple sw operations */
		status = prWlanReqEntry->pfOidSetHandler(prGlueInfo,
				prNdisReq->ndisOidContent,
				prNdisReq->inNdisOidlength, &u4SetInfoLen);
	} else if (prWlanReqEntry->eOidMethod ==
		   ENUM_OID_DRIVER_CORE) {
		/* driver core */

		status = kalIoctl(prGlueInfo,
			(PFN_OID_HANDLER_FUNC) prWlanReqEntry->pfOidSetHandler,
			prNdisReq->ndisOidContent,
			prNdisReq->inNdisOidlength,
			FALSE, FALSE, TRUE, &u4SetInfoLen);
	} else {
		DBGLOG(REQ, INFO,
		       "priv_set_ndis(): unsupported OID method:0x%x\n",
		       prWlanReqEntry->eOidMethod);
		return -EOPNOTSUPP;
	}

	*pu4OutputLen = u4SetInfoLen;

	switch (status) {
	case WLAN_STATUS_SUCCESS:
		break;

	case WLAN_STATUS_INVALID_LENGTH:
		/* WARNLOG(
		 * ("Set %s: Invalid length (current=%ld, needed=%ld)\n",
		 * prWlanReqEntry->pucOidName,
		 * prNdisReq->inNdisOidlength,
		 * u4SetInfoLen));
		 */
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
priv_get_ndis(IN struct net_device *prNetDev,
	      IN struct NDIS_TRANSPORT_STRUCT *prNdisReq,
	      OUT uint32_t *pu4OutputLen)
{
	struct WLAN_REQ_ENTRY *prWlanReqEntry = NULL;
	uint32_t u4BufLen = 0;
	uint32_t status = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = NULL;

	ASSERT(prNetDev);
	ASSERT(prNdisReq);
	ASSERT(pu4OutputLen);

	if (!prNetDev || !prNdisReq || !pu4OutputLen) {
		DBGLOG(REQ, INFO,
		       "priv_get_ndis(): invalid param(0x%p, 0x%p, 0x%p)\n",
		       prNetDev, prNdisReq, pu4OutputLen);
		return -EINVAL;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO,
		       "priv_get_ndis(): invalid prGlueInfo(0x%p, 0x%p)\n",
		       prNetDev,
		       *((struct GLUE_INFO **) netdev_priv(prNetDev)));
		return -EINVAL;
	}
#if 0
	DBGLOG(INIT, INFO,
	       "priv_get_ndis(): prNdisReq->ndisOidCmd(0x%lX)\n",
	       prNdisReq->ndisOidCmd);
#endif

	if (reqSearchSupportedOidEntry(prNdisReq->ndisOidCmd,
				       &prWlanReqEntry) == FALSE) {
		/* WARNLOG(
		 *         ("Query OID: 0x%08lx (unknown)\n",
		 *         prNdisReq->ndisOidCmd));
		 */
		return -EOPNOTSUPP;
	}

	if (prWlanReqEntry->pfOidQueryHandler == NULL) {
		/* WARNLOG(
		 *         ("Query %s: Null query handler\n",
		 *         prWlanReqEntry->pucOidName));
		 */
		return -EOPNOTSUPP;
	}
#if 0
	DBGLOG(INIT, INFO, "priv_get_ndis(): %s\n",
	       prWlanReqEntry->pucOidName);
#endif

	if (prWlanReqEntry->fgQryBufLenChecking) {
		if (prNdisReq->inNdisOidlength <
		    prWlanReqEntry->u4InfoBufLen) {
			/* Not enough room in InformationBuffer. Punt */
			/* WARNLOG(
			 * ("Query %s: Buffer too short (current=%ld,
			 * needed=%ld)\n",
			 * prWlanReqEntry->pucOidName,
			 * prNdisReq->inNdisOidlength,
			 * prWlanReqEntry->u4InfoBufLen));
			 */

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
	} else if (prWlanReqEntry->eOidMethod ==
		   ENUM_OID_GLUE_EXTENSION) {
		/* multiple sw operations */
		status = prWlanReqEntry->pfOidQueryHandler(prGlueInfo,
				prNdisReq->ndisOidContent,
				prNdisReq->inNdisOidlength, &u4BufLen);
	} else if (prWlanReqEntry->eOidMethod ==
		   ENUM_OID_DRIVER_CORE) {
		/* driver core */

		status = kalIoctl(prGlueInfo,
		    (PFN_OID_HANDLER_FUNC)prWlanReqEntry->pfOidQueryHandler,
		    prNdisReq->ndisOidContent, prNdisReq->inNdisOidlength,
		    TRUE, TRUE, TRUE, &u4BufLen);
	} else {
		DBGLOG(REQ, INFO,
		       "priv_set_ndis(): unsupported OID method:0x%x\n",
		       prWlanReqEntry->eOidMethod);
		return -EOPNOTSUPP;
	}

	*pu4OutputLen = u4BufLen;

	switch (status) {
	case WLAN_STATUS_SUCCESS:
		break;

	case WLAN_STATUS_INVALID_LENGTH:
		/* WARNLOG(
		 * ("Set %s: Invalid length (current=%ld, needed=%ld)\n",
		 *  prWlanReqEntry->pucOidName,
		 *  prNdisReq->inNdisOidlength,
		 *  u4BufLen));
		 */
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
__priv_ate_set(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData,
	     IN char *pcExtra)
{
	int32_t i4Status;
	/* uint8_t *InBuf;
	 * uint8_t *addr_str, *value_str;
	 * uint32_t InBufLen;
	 */
	uint32_t u4SubCmd;
	/* u_int8_t isWrite = 0;
	 * uint32_t u4BufLen = 0;
	 * struct NDIS_TRANSPORT_STRUCT *prNdisReq;
	 * uint32_t pu4IntBuf[2];
	 */
	uint32_t u4CopySize = sizeof(aucOidBuf);

	/* sanity check */
	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);

	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;

	u4SubCmd = (uint32_t) prIwReqData->data.flags;
	DBGLOG(REQ, INFO, "MT6632: %s, u4SubCmd=%d mode=%d\n", __func__,
	       u4SubCmd, (uint32_t) prIwReqData->mode);

	switch (u4SubCmd) {
	case PRIV_QACMD_SET:
		u4CopySize = (prIwReqData->data.length < u4CopySize)
			     ? prIwReqData->data.length : (u4CopySize - 1);
		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer,
				   u4CopySize))
			return -EFAULT;
		aucOidBuf[u4CopySize] = '\0';
		DBGLOG(REQ, INFO,
		       "PRIV_QACMD_SET: priv_set_string=(%s)(%u,%d)\n",
		       aucOidBuf, u4CopySize,
		       (int32_t)prIwReqData->data.length);
		i4Status = AteCmdSetHandle(prNetDev, &aucOidBuf[0],
					   u4CopySize);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

int
priv_ate_set(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	DBGLOG(REQ, LOUD, "cmd=%x, flags=%x\n",
	     prIwReqInfo->cmd, prIwReqInfo->flags);
	DBGLOG(REQ, LOUD, "mode=%x, flags=%x\n",
	     prIwReqData->mode, prIwReqData->data.flags);

	return compat_priv(prNetDev, prIwReqInfo,
	     prIwReqData, pcExtra, __priv_ate_set);
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
static u_int8_t reqSearchSupportedOidEntry(IN uint32_t rOid,
		OUT struct WLAN_REQ_ENTRY **ppWlanReqEntry)
{
	int32_t i, j, k;

	i = 0;
	j = NUM_SUPPORTED_OIDS - 1;

	while (i <= j) {
		k = i + (j - i) / 2;

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
		IN struct iw_request_info *prIwReqInfo,
		IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	uint32_t u4SubCmd = 0;
	uint16_t u2Cmd = 0;

	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;

	ASSERT(prNetDev);
	ASSERT(prIwReqData);
	if (!prNetDev || !prIwReqData) {
		DBGLOG(REQ, INFO,
		       "priv_set_driver(): invalid param(0x%p, 0x%p)\n",
		       prNetDev, prIwReqData);
		return -EINVAL;
	}

	u2Cmd = prIwReqInfo->cmd;
	DBGLOG(REQ, INFO, "prIwReqInfo->cmd %u\n", u2Cmd);

	u4SubCmd = (uint32_t) prIwReqData->data.flags;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO,
		       "priv_set_driver(): invalid prGlueInfo(0x%p, 0x%p)\n",
		       prNetDev,
		       *((struct GLUE_INFO **) netdev_priv(prNetDev)));
		return -EINVAL;
	}

	/* trick,hack in ./net/wireless/wext-priv.c ioctl_private_iw_point */
	/* because the cmd number is odd (get), the input string will not be
	 * copy_to_user
	 */

	DBGLOG(REQ, INFO, "prIwReqData->data.length %u\n",
	       prIwReqData->data.length);

	/* Use GET type becauase large data by iwpriv. */

	ASSERT(IW_IS_GET(u2Cmd));
	if (prIwReqData->data.length != 0) {
		if (!kal_access_ok(VERIFY_READ, prIwReqData->data.pointer,
			       prIwReqData->data.length)) {
			DBGLOG(REQ, INFO,
			       "%s access_ok Read fail written = %d\n",
			       __func__, i4BytesWritten);
			return -EFAULT;
		}
		if (copy_from_user(pcExtra, prIwReqData->data.pointer,
				   prIwReqData->data.length)) {
			DBGLOG(REQ, INFO,
			       "%s copy_form_user fail written = %d\n",
			       __func__, prIwReqData->data.length);
			return -EFAULT;
		}
		/* prIwReqData->data.length include the terminate '\0' */
		pcExtra[prIwReqData->data.length] = 0;
	}

	if (pcExtra) {
		DBGLOG(REQ, INFO, "pcExtra %s\n", pcExtra);
		/* Please check max length in rIwPrivTable */
		DBGLOG(REQ, INFO, "%s prIwReqData->data.length = %d\n",
		       __func__, prIwReqData->data.length);
		i4BytesWritten = priv_driver_cmds(prNetDev, pcExtra,
					  2000 /*prIwReqData->data.length */);
		DBGLOG(REQ, INFO, "%s i4BytesWritten = %d\n", __func__,
		       i4BytesWritten);
	}

	DBGLOG(REQ, INFO, "pcExtra done\n");

	if (i4BytesWritten > 0) {

		if (i4BytesWritten > 2000)
			i4BytesWritten = 2000;
		prIwReqData->data.length =
			i4BytesWritten;	/* the iwpriv will use the length */

	} else if (i4BytesWritten == 0) {
		prIwReqData->data.length = i4BytesWritten;
	}
#if 0
	/* trick,hack in ./net/wireless/wext-priv.c ioctl_private_iw_point */
	/* because the cmd number is even (set), the return string will not be
	 * copy_to_user
	 */
	ASSERT(IW_IS_SET(u2Cmd));
	if (!access_ok(VERIFY_WRITE, prIwReqData->data.pointer,
		       i4BytesWritten)) {
		DBGLOG(REQ, INFO, "%s access_ok Write fail written = %d\n",
		       __func__, i4BytesWritten);
		return -EFAULT;
	}
	if (copy_to_user(prIwReqData->data.pointer, pcExtra,
			 i4BytesWritten)) {
		DBGLOG(REQ, INFO, "%s copy_to_user fail written = %d\n",
		       __func__, i4BytesWritten);
		return -EFAULT;
	}
	DBGLOG(RSN, INFO, "%s copy_to_user written = %d\n",
	       __func__, i4BytesWritten);
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
 * \param[out] pvQueryBuffer     Pointer to the buffer that holds the result of
 *                               the query.
 * \param[in] u4QueryBufferLen   The length of the query buffer.
 * \param[out] pu4QueryInfoLen   If the call is successful, returns the number
 *                               of bytes written into the query buffer. If the
 *                               call failed due to invalid length of the query
 *                               buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_SUCCESS
 * \retval WLAN_STATUS_INVALID_LENGTH
 */
/*----------------------------------------------------------------------------*/
static uint32_t
reqExtQueryConfiguration(IN struct GLUE_INFO *prGlueInfo,
			 OUT void *pvQueryBuffer, IN uint32_t u4QueryBufferLen,
			 OUT uint32_t *pu4QueryInfoLen)
{
	struct PARAM_802_11_CONFIG *prQueryConfig =
		(struct PARAM_802_11_CONFIG *) pvQueryBuffer;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4QueryInfoLen = 0;

	DEBUGFUNC("wlanoidQueryConfiguration");

	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(struct PARAM_802_11_CONFIG);
	if (u4QueryBufferLen < sizeof(struct PARAM_802_11_CONFIG))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvQueryBuffer);

	kalMemZero(prQueryConfig,
		   sizeof(struct PARAM_802_11_CONFIG));

	/* Update the current radio configuration. */
	prQueryConfig->u4Length = sizeof(struct PARAM_802_11_CONFIG);

#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidSetBeaconInterval,
			       &prQueryConfig->u4BeaconPeriod, sizeof(uint32_t),
			       TRUE, TRUE, &u4QueryInfoLen);
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQueryBeaconInterval,
				       &prQueryConfig->u4BeaconPeriod,
				       sizeof(uint32_t), &u4QueryInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidQueryAtimWindow,
			       &prQueryConfig->u4ATIMWindow, sizeof(uint32_t),
			       TRUE, TRUE, &u4QueryInfoLen);
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQueryAtimWindow,
				       &prQueryConfig->u4ATIMWindow,
				       sizeof(uint32_t),
				       &u4QueryInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidQueryFrequency,
			       &prQueryConfig->u4DSConfig, sizeof(uint32_t),
			       TRUE, TRUE, &u4QueryInfoLen);
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQueryFrequency,
				       &prQueryConfig->u4DSConfig,
				       sizeof(uint32_t),
				       &u4QueryInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;

	prQueryConfig->rFHConfig.u4Length = sizeof(
			struct PARAM_802_11_CONFIG_FH);

	return rStatus;

}				/* end of reqExtQueryConfiguration() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set the radio configuration used in IBSS
 *        mode.
 *
 * \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
 * \param[in] pvSetBuffer    A pointer to the buffer that holds the data to be
 *                           set.
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
static uint32_t
reqExtSetConfiguration(IN struct GLUE_INFO *prGlueInfo,
		       IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
		       OUT uint32_t *pu4SetInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct PARAM_802_11_CONFIG *prNewConfig =
		(struct PARAM_802_11_CONFIG *) pvSetBuffer;
	uint32_t u4SetInfoLen = 0;

	DEBUGFUNC("wlanoidSetConfiguration");

	ASSERT(prGlueInfo);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(struct PARAM_802_11_CONFIG);

	if (u4SetBufferLen < *pu4SetInfoLen)
		return WLAN_STATUS_INVALID_LENGTH;

	/* OID_802_11_CONFIGURATION. If associated, NOT_ACCEPTED shall be
	 * returned.
	 */
	if (prGlueInfo->eParamMediaStateIndicated ==
	    MEDIA_STATE_CONNECTED)
		return WLAN_STATUS_NOT_ACCEPTED;

	ASSERT(pvSetBuffer);

#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidSetBeaconInterval,
			       &prNewConfig->u4BeaconPeriod, sizeof(uint32_t),
			       FALSE, TRUE, &u4SetInfoLen);
#else
	rStatus = wlanSetInformation(prGlueInfo->prAdapter,
				     wlanoidSetBeaconInterval,
				     &prNewConfig->u4BeaconPeriod,
				     sizeof(uint32_t),
				     &u4SetInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidSetAtimWindow,
			       &prNewConfig->u4ATIMWindow, sizeof(uint32_t),
			       FALSE, TRUE, &u4SetInfoLen);
#else
	rStatus = wlanSetInformation(prGlueInfo->prAdapter,
				     wlanoidSetAtimWindow,
				     &prNewConfig->u4ATIMWindow,
				     sizeof(uint32_t), &u4SetInfoLen);
#endif
	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo, wlanoidSetFrequency,
			       &prNewConfig->u4DSConfig, sizeof(uint32_t),
			       FALSE, TRUE, &u4SetInfoLen);
#else
	rStatus = wlanSetInformation(prGlueInfo->prAdapter, wlanoidSetFrequency,
				     &prNewConfig->u4DSConfig,
				     sizeof(uint32_t), &u4SetInfoLen);
#endif

	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;

	return rStatus;

}				/* end of reqExtSetConfiguration() */
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set beacon detection function enable/disable
 *        state.
 *        This is mainly designed for usage under BT inquiry state
 *        (disable function).
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
static uint32_t
reqExtSetAcpiDevicePowerState(IN struct GLUE_INFO
			      *prGlueInfo,
			      IN void *pvSetBuffer, IN uint32_t u4SetBufferLen,
			      OUT uint32_t *pu4SetInfoLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prGlueInfo);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	/* WIFI is enabled, when ACPI is
	 * D0 (ParamDeviceStateD0 = 1). And vice versa
	 */

	/* rStatus = wlanSetInformation(prGlueInfo->prAdapter, */
	/* wlanoidSetAcpiDevicePowerState, */
	/* pvSetBuffer, */
	/* u4SetBufferLen, */
	/* pu4SetInfoLen); */
	return rStatus;
}

#define CMD_START		"START"
#define CMD_STOP		"STOP"
#define CMD_SCAN_ACTIVE		"SCAN-ACTIVE"
#define CMD_SCAN_PASSIVE	"SCAN-PASSIVE"
#define CMD_RSSI		"RSSI"
#define CMD_LINKSPEED		"LINKSPEED"
#define CMD_RXFILTER_START	"RXFILTER-START"
#define CMD_RXFILTER_STOP	"RXFILTER-STOP"
#define CMD_RXFILTER_ADD	"RXFILTER-ADD"
#define CMD_RXFILTER_REMOVE	"RXFILTER-REMOVE"
#define CMD_BTCOEXSCAN_START	"BTCOEXSCAN-START"
#define CMD_BTCOEXSCAN_STOP	"BTCOEXSCAN-STOP"
#define CMD_BTCOEXMODE		"BTCOEXMODE"
#define CMD_SETSUSPENDOPT	"SETSUSPENDOPT"
#define CMD_SETSUSPENDMODE	"SETSUSPENDMODE"
#define CMD_P2P_DEV_ADDR	"P2P_DEV_ADDR"
#define CMD_SETFWPATH		"SETFWPATH"
#define CMD_SETBAND		"SETBAND"
#define CMD_GETBAND		"GETBAND"
#define CMD_AP_START		"AP_START"

#if CFG_SUPPORT_QA_TOOL
#define CMD_GET_RX_STATISTICS	"GET_RX_STATISTICS"
#endif
#define CMD_GET_STAT		"GET_STAT"
#define CMD_GET_BSS_STATISTICS	"GET_BSS_STATISTICS"
#define CMD_GET_STA_STATISTICS	"GET_STA_STATISTICS"
#define CMD_GET_WTBL_INFO	"GET_WTBL"
#define CMD_GET_MIB_INFO	"GET_MIB"
#define CMD_GET_STA_INFO	"GET_STA"
#define CMD_SET_FW_LOG		"SET_FWLOG"
#define CMD_GET_QUE_INFO	"GET_QUE"
#define CMD_GET_MEM_INFO	"GET_MEM"
#define CMD_GET_HIF_INFO	"GET_HIF"
#define CMD_GET_TP_INFO		"GET_TP"
#define CMD_GET_STA_KEEP_CNT    "KEEPCOUNTER"
#define CMD_STAT_RESET_CNT      "RESETCOUNTER"
#define CMD_STAT_NOISE_SEL      "NOISESELECT"
#define CMD_STAT_GROUP_SEL      "GROUP"

#define CMD_SET_TXPOWER			"SET_TXPOWER"
#define CMD_COUNTRY			"COUNTRY"
#define CMD_CSA				"CSA"
#define CMD_GET_COUNTRY			"GET_COUNTRY"
#define CMD_GET_CHANNELS		"GET_CHANNELS"
#define CMD_P2P_SET_NOA			"P2P_SET_NOA"
#define CMD_P2P_GET_NOA			"P2P_GET_NOA"
#define CMD_P2P_SET_PS			"P2P_SET_PS"
#define CMD_SET_AP_WPS_P2P_IE		"SET_AP_WPS_P2P_IE"
#define CMD_SETROAMMODE			"SETROAMMODE"
#define CMD_MIRACAST			"MIRACAST"

#if (CFG_SUPPORT_DFS_MASTER == 1)
#define CMD_SHOW_DFS_STATE		"SHOW_DFS_STATE"
#define CMD_SHOW_DFS_RADAR_PARAM	"SHOW_DFS_RADAR_PARAM"
#define CMD_SHOW_DFS_HELP		"SHOW_DFS_HELP"
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

#define CMD_GET_CH_RANK_LIST	"GET_CH_RANK_LIST"
#define CMD_GET_CH_DIRTINESS	"GET_CH_DIRTINESS"

#if CFG_CHIP_RESET_HANG
#define CMD_SET_RST_HANG                 "RST_HANG_SET"

#define CMD_SET_RST_HANG_ARG_NUM		2
#endif


#define CMD_EFUSE		"EFUSE"

#if (CFG_SUPPORT_TWT == 1)
#define CMD_SET_TWT_PARAMS	"SET_TWT_PARAMS"
#endif

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

#define	CMD_BAND_TYPE_AUTO	0
#define	CMD_BAND_TYPE_5G	1
#define	CMD_BAND_TYPE_2G	2
#define	CMD_BAND_TYPE_ALL	3

/* Mediatek private command */
#define CMD_SET_MCR		"SET_MCR"
#define CMD_GET_MCR		"GET_MCR"
#define CMD_SET_NVRAM	"SET_NVRAM"
#define CMD_GET_NVRAM	"GET_NVRAM"
#define CMD_SET_DRV_MCR		"SET_DRV_MCR"
#define CMD_GET_DRV_MCR		"GET_DRV_MCR"
#define CMD_SET_SW_CTRL	        "SET_SW_CTRL"
#define CMD_GET_SW_CTRL         "GET_SW_CTRL"
#define CMD_SET_CFG             "SET_CFG"
#define CMD_GET_CFG             "GET_CFG"
#define CMD_SET_EM_CFG          "SET_EM_CFG"
#define CMD_GET_EM_CFG          "GET_EM_CFG"
#define CMD_SET_CHIP            "SET_CHIP"
#define CMD_GET_CHIP            "GET_CHIP"
#define CMD_SET_DBG_LEVEL       "SET_DBG_LEVEL"
#define CMD_GET_DBG_LEVEL       "GET_DBG_LEVEL"
#define CMD_ADD_TS		"addts"
#define CMD_DEL_TS		"delts"
#define CMD_DUMP_TS		"dumpts"
#define CMD_RM_IT		"RM-IT"
#define CMD_DUMP_UAPSD		"dumpuapsd"
#define CMD_FW_EVENT		"FW-EVENT "
#define CMD_GET_WIFI_TYPE	"GET_WIFI_TYPE"
#define CMD_SET_PWR_CTRL        "SET_PWR_CTRL"
#define CMD_SET_ANT_SWAP        "SET_ANT_SWAP"
#define PRIV_CMD_SIZE 512
#define CMD_SET_FIXED_RATE      "FixedRate"
#define CMD_GET_VERSION         "VER"
#define CMD_SET_TEST_MODE	"SET_TEST_MODE"
#define CMD_SET_TEST_CMD	"SET_TEST_CMD"
#define CMD_GET_TEST_RESULT	"GET_TEST_RESULT"
#define CMD_GET_STA_STAT        "STAT"
#define CMD_GET_STA_STAT2       "STAT2"
#define CMD_GET_STA_RX_STAT	"RX_STAT"
#define CMD_SET_ACL_POLICY      "SET_ACL_POLICY"
#define CMD_ADD_ACL_ENTRY       "ADD_ACL_ENTRY"
#define CMD_DEL_ACL_ENTRY       "DEL_ACL_ENTRY"
#define CMD_SHOW_ACL_ENTRY      "SHOW_ACL_ENTRY"
#define CMD_CLEAR_ACL_ENTRY     "CLEAR_ACL_ENTRY"
#define CMD_SET_RA_DBG		"RADEBUG"
#define CMD_SET_FIXED_FALLBACK	"FIXEDRATEFALLBACK"
#define CMD_GET_STA_IDX         "GET_STA_IDX"
#define CMD_GET_TX_POWER_INFO   "TxPowerInfo"
#define CMD_TX_POWER_MANUAL_SET "TxPwrManualSet"
#if CFG_SUPPORT_SCAN_EXT_FLAG
#define CMD_SET_SCAN_EXT_FLAG   "SetScanExtFlag"
#endif


/* neptune doens't support "show" entry, use "driver" to handle
 * MU GET request, and MURX_PKTCNT comes from RX_STATS,
 * so this command will reuse RX_STAT's flow
 */
#define CMD_GET_MU_RX_PKTCNT	"hqa_get_murx_pktcnt"
#define CMD_RUN_HQA	"hqa"
#define CMD_CALIBRATION	"cal"

#if CFG_WOW_SUPPORT
#define CMD_WOW_START		"WOW_START"
#define CMD_SET_WOW_ENABLE	"SET_WOW_ENABLE"
#define CMD_SET_WOW_PAR		"SET_WOW_PAR"
#define CMD_SET_WOW_UDP		"SET_WOW_UDP"
#define CMD_SET_WOW_TCP		"SET_WOW_TCP"
#define CMD_GET_WOW_PORT	"GET_WOW_PORT"
#endif
#define CMD_SET_ADV_PWS		"SET_ADV_PWS"
#define CMD_SET_MDTIM		"SET_MDTIM"

#define CMD_SET_DBDC		"SET_DBDC"
#define CMD_SET_STA1NSS		"SET_STA1NSS"

#define CMD_SET_AMPDU_TX        "SET_AMPDU_TX"
#define CMD_SET_AMPDU_RX        "SET_AMPDU_RX"
#define CMD_SET_BF              "SET_BF"
#define CMD_SET_NSS             "SET_NSS"
#define CMD_SET_AMSDU_TX        "SET_AMSDU_TX"
#define CMD_SET_AMSDU_RX        "SET_AMSDU_RX"
#define CMD_SET_QOS             "SET_QOS"
#if (CFG_SUPPORT_802_11AX == 1)
#define CMD_SET_BA_SIZE         "SET_BA_SIZE"
#define CMD_SET_TP_TEST_MODE    "SET_TP_TEST_MODE"
#define CMD_SET_MUEDCA_OVERRIDE "MUEDCA_OVERRIDE"
#define CMD_SET_TX_MCSMAP       "SET_MCS_MAP"
#define CMD_SET_TX_PPDU         "TX_PPDU"
#define CMD_SET_LDPC            "SET_LDPC"
#define CMD_FORCE_AMSDU_TX		"FORCE_AMSDU_TX"
#define CMD_SET_OM_CH_BW        "SET_OM_CHBW"
#define CMD_SET_OM_RX_NSS       "SET_OM_RXNSS"
#define CMD_SET_OM_TX_NSS       "SET_OM_TXNSTS"
#define CMD_SET_OM_MU_DISABLE   "SET_OM_MU_DISABLE"
#define CMD_SET_TX_OM_PACKET    "TX_OM_PACKET"
#define CMD_SET_TX_CCK_1M_PWR   "TX_CCK_1M_PWR"
#define CMD_SET_PAD_DUR	        "SET_PAD_DUR"
#endif /* CFG_SUPPORT_802_11AX == 1 */

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
#define CMD_SET_CALBACKUP_TEST_DRV_FW		"SET_CALBACKUP_TEST_DRV_FW"
#endif

#define CMD_GET_CNM		"GET_CNM"
#define CMD_GET_CAPAB_RSDB "GET_CAPAB_RSDB"

#ifdef UT_TEST_MODE
#define CMD_RUN_UT		"UT"
#endif

#if CFG_SUPPORT_ADVANCE_CONTROL
#define CMD_SW_DBGCTL_ADVCTL_SET_ID 0xa1260000
#define CMD_SW_DBGCTL_ADVCTL_GET_ID 0xb1260000
#define CMD_SET_NOISE		"SET_NOISE"
#define CMD_GET_NOISE           "GET_NOISE"
#define CMD_SET_POP		"SET_POP"
#define CMD_SET_ED		"SET_ED"
#define CMD_SET_PD		"SET_PD"
#define CMD_SET_MAX_RFGAIN	"SET_MAX_RFGAIN"
#endif

#if CFG_SUPPORT_WIFI_SYSDVT
#define CMD_WIFI_SYSDVT         "DVT"
#define CMD_SET_TXS_TEST        "TXS_TEST"
#define CMD_SET_TXS_TEST_RESULT "TXS_RESULT"
#define CMD_SET_RXV_TEST        "RXV_TEST"
#define CMD_SET_RXV_TEST_RESULT        "RXV_RESULT"
#if CFG_TCP_IP_CHKSUM_OFFLOAD
#define CMD_SET_CSO_TEST        "CSO_TEST"
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */
#define CMD_SET_TX_TEST          "TX_TEST"
#define CMD_SET_TX_AC_TEST       "TX_AC_TEST"
#define CMD_SET_SKIP_CH_CHECK   "SKIP_CH_CHECK"

#if (CFG_SUPPORT_DMASHDL_SYSDVT)
#define CMD_SET_DMASHDL_DUMP    "DMASHDL_DUMP_MEM"
#define CMD_SET_DMASHDL_DVT_ITEM "DMASHDL_DVT_ITEM"
#endif /* CFG_SUPPORT_DMASHDL_SYSDVT */
#endif /* CFG_SUPPORT_WIFI_SYSDVT */

#define CMD_SET_SW_AMSDU_NUM      "SET_SW_AMSDU_NUM"
#define CMD_SET_SW_AMSDU_SIZE      "SET_SW_AMSDU_SIZE"

#define CMD_SET_DRV_SER           "SET_DRV_SER"

#define CMD_GET_WFDMA_INFO      "GET_WFDMA_INFO"
#define CMD_GET_PLE_INFO        "GET_PLE_INFO"
#define CMD_GET_PSE_INFO        "GET_PSE_INFO"
#define CMD_GET_DMASCH_INFO     "GET_DMASCH_INFO"
#define CMD_SHOW_TXD_INFO       "SHOW_TXD_INFO"

#if (CFG_SUPPORT_CONNAC2X == 1)
#define CMD_GET_FWTBL_UMAC      "GET_UMAC_FWTBL"
#endif /* CFG_SUPPORT_CONNAC2X == 1 */

/* Debug for consys */
#define CMD_DBG_SHOW_TR_INFO			"show-tr"
#define CMD_DBG_SHOW_PLE_INFO			"show-ple"
#define CMD_DBG_SHOW_PSE_INFO			"show-pse"
#define CMD_DBG_SHOW_CSR_INFO			"show-csr"
#define CMD_DBG_SHOW_DMASCH_INFO		"show-dmasch"

#if CFG_SUPPORT_EASY_DEBUG
#define CMD_FW_PARAM				"set_fw_param"
#endif /* CFG_SUPPORT_EASY_DEBUG */

#if (CFG_SUPPORT_CONNINFRA == 1)
#define CMD_SET_WHOLE_CHIP_RESET "SET_WHOLE_CHIP_RESET"
#define CMD_SET_WFSYS_RESET      "SET_WFSYS_RESET"
#endif
static uint8_t g_ucMiracastMode = MIRACAST_MODE_OFF;

struct cmd_tlv {
	char prefix;
	char version;
	char subver;
	char reserved;
};

struct priv_driver_cmd_s {
	char buf[PRIV_CMD_SIZE];
	int used_len;
	int total_len;
};

#ifdef CFG_ANDROID_AOSP_PRIV_CMD
struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
};
#endif /* CFG_ANDROID_AOSP_PRIV_CMD */

int priv_driver_get_dbg_level(IN struct net_device
			      *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4DbgIdx = DBG_MODULE_NUM, u4DbgMask = 0;
	u_int8_t fgIsCmdAccept = FALSE;
	int32_t u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		/* u4DbgIdx = kalStrtoul(apcArgv[1], NULL, 0); */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4DbgIdx);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		if (wlanGetDriverDbgLevel(u4DbgIdx, &u4DbgMask) ==
		    WLAN_STATUS_SUCCESS) {
			fgIsCmdAccept = TRUE;
			i4BytesWritten =
				kalSnprintf(pcCommand, i4TotalLen,
					 "Get DBG module[%u] log level => [0x%02x]!",
					 u4DbgIdx,
					 (uint8_t) u4DbgMask);
		}
	}

	if (!fgIsCmdAccept)
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
					  "Get DBG module log level failed!");

	return i4BytesWritten;

}				/* priv_driver_get_sw_ctrl */

static int priv_cmd_not_support(IN struct net_device *prNetDev,
	IN char *pcCommand, IN int i4TotalLen)
{
	DBGLOG(REQ, WARN, "not support priv command: %s\n", pcCommand);

	return -EOPNOTSUPP;
}

#if CFG_SUPPORT_QA_TOOL
#if CFG_SUPPORT_BUFFER_MODE
static int priv_driver_set_efuse_buffer_mode(
	IN struct net_device *prNetDev, IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct PARAM_CUSTOM_EFUSE_BUFFER_MODE
		*prSetEfuseBufModeInfo = NULL;
#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 0)
	struct BIN_CONTENT *pBinContent;
	int i = 0;
#endif
	uint8_t *pucConfigBuf = NULL;
	uint32_t u4ConfigReadLen;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	pucConfigBuf = (uint8_t *) kalMemAlloc(2048, VIR_MEM_TYPE);

	if (!pucConfigBuf) {
		DBGLOG(INIT, INFO, "allocate pucConfigBuf failed\n");
		i4BytesWritten = -1;
		goto out;
	}

	kalMemZero(pucConfigBuf, 2048);
	u4ConfigReadLen = 0;

	if (kalReadToFile("/MT6632_eFuse_usage_table.xlsm.bin",
			  pucConfigBuf, 2048, &u4ConfigReadLen) == 0) {
		/* ToDo:: Nothing */
	} else {
		DBGLOG(INIT, INFO, "can't find file\n");
		i4BytesWritten = -1;
		goto out;
	}

	/* pucConfigBuf */
	prSetEfuseBufModeInfo =
		(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE *) kalMemAlloc(
			sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE),
			VIR_MEM_TYPE);

	if (prSetEfuseBufModeInfo == NULL) {
		DBGLOG(INIT, INFO,
			"allocate prSetEfuseBufModeInfo failed\n");
		i4BytesWritten = -1;
		goto out;
	}

	kalMemZero(prSetEfuseBufModeInfo,
		   sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE));

	prSetEfuseBufModeInfo->ucSourceMode = 1;
	prSetEfuseBufModeInfo->ucCount = (uint8_t)
					 EFUSE_CONTENT_SIZE;

#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 0)
	pBinContent = (struct BIN_CONTENT *)
		      prSetEfuseBufModeInfo->aBinContent;
	for (i = 0; i < EFUSE_CONTENT_SIZE; i++) {
		pBinContent->u2Addr  = i;
		pBinContent->ucValue = *(pucConfigBuf + i);

		pBinContent++;
	}

	for (i = 0; i < 20; i++)
		DBGLOG(INIT, INFO, "%x\n",
		       prSetEfuseBufModeInfo->aBinContent[i].ucValue);
#endif

	rStatus = kalIoctl(prGlueInfo, wlanoidSetEfusBufferMode,
			   prSetEfuseBufModeInfo,
			   sizeof(struct PARAM_CUSTOM_EFUSE_BUFFER_MODE),
			   FALSE, FALSE, TRUE, &u4BufLen);

	i4BytesWritten =
		kalSnprintf(pcCommand, i4TotalLen, "set buffer mode %s",
			 (rStatus == WLAN_STATUS_SUCCESS) ? "success" : "fail");

out:
	if (pucConfigBuf)
		kalMemFree(pucConfigBuf, VIR_MEM_TYPE, 2048);

	if (prSetEfuseBufModeInfo)
		kalMemFree(prSetEfuseBufModeInfo, VIR_MEM_TYPE,
			sizeof(truct PARAM_CUSTOM_EFUSE_BUFFER_MODE));

	return i4BytesWritten;
}
#endif /* CFG_SUPPORT_BUFFER_MODE */

static int priv_driver_get_rx_statistics(IN struct net_device *prNetDev,
					 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t u4Ret = 0;
	struct PARAM_CUSTOM_ACCESS_RX_STAT rRxStatisticsTest;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	DBGLOG(INIT, ERROR,
	       "MT6632 : priv_driver_get_rx_statistics\n");

	if (i4Argc >= 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0,
				     &(rRxStatisticsTest.u4SeqNum));
		rRxStatisticsTest.u4TotalNum = sizeof(struct
						      PARAM_RX_STAT) / 4;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryRxStatistics,
				&rRxStatisticsTest, sizeof(rRxStatisticsTest),
				TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}

	return i4BytesWritten;
}
#endif /* CFG_SUPPORT_QA_TOOL */

#if CFG_SUPPORT_MSP
#if 0
static int priv_driver_get_stat(IN struct net_device
			*prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4ArgNum = 2;
	struct PARAM_GET_STA_STATISTICS rQueryStaStatistics;
	int32_t rRssi;
	uint16_t u2LinkSpeed;
	uint32_t u4Per;
	UINTT_8 i;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	kalMemZero(&rQueryStaStatistics,
		   sizeof(rQueryStaStatistics));

	if (i4Argc >= i4ArgNum) {
		wlanHwAddrToBin(apcArgv[1],
				&rQueryStaStatistics.aucMacAddr[0]);

		rQueryStaStatistics.ucReadClear = TRUE;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryStaStatistics,
				   &rQueryStaStatistics,
				   sizeof(rQueryStaStatistics),
				   TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			rRssi = RCPI_TO_dBm(rQueryStaStatistics.ucRcpi);
			u2LinkSpeed = rQueryStaStatistics.u2LinkSpeed == 0 ? 0 :
				      rQueryStaStatistics.u2LinkSpeed / 2;

			i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
				"%s", "\n\nSTA Stat:\n");

			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"CurrentTemperature            = %d\n", 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Tx success                    = %lu\n", 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Tx fail count                 = %ld, PER=%ld.%1ld%%\n",
				0, 0, 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rx success                    = %lu\n", 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rx with CRC                   = %ld, PER=%ld.%1ld%%\n",
				0, 0, 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rx with PhyErr                = %lu\n", 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rx with PlcpErr               = %lu\n", 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rx drop due to out of resource= %lu\n", 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rx duplicate frame            = %lu\n", 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"False CCA                     = %lu\n", 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"RSSI                          = %d %d %d %d\n",
				0, 0, 0, 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Last TX Rate	       = %s, %s, %s, %s, %s\n",
				"NA", "NA", "NA", "NA", "NA");
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Last RX Rate	       = %s, %s, %s, %s, %s\n",
				"NA", "NA", "NA", "NA", "NA");

			for (i = 0; i < 2 /* band num */; i++) {
				i4BytesWritten += kalSnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"BandIdx:	       = %d\n", i);
				i4BytesWritten += kalSnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%s",
					"\tRange:  1   2~5   6~15   16~22   23~33   34~49   50~57   58~64\n"
					);
				i4BytesWritten += kalSnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"\t\t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d\n",
					0, 0, 0, 0, 0, 0, 0, 0);
			}
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Tx success	       = %ld\n",
				rQueryStaStatistics.u4TransmitCount -
				    rQueryStaStatistics.u4TransmitFailCount);

			u4Per = rQueryStaStatistics.u4TransmitFailCount == 0 ?
			    0 :
			    (1000 * (rQueryStaStatistics.u4TransmitFailCount))
				/ rQueryStaStatistics.u4TransmitCount;
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Tx fail count	       = %ld, PER=%ld.%1ld%%\n",
				rQueryStaStatistics.u4TransmitFailCount,
				u4Per / 10, u4Per % 10);

			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"RSSI		       = %d\n", rRssi);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"LinkSpeed	       = %d\n", u2LinkSpeed);
		}
	} else
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "%s",
					     "\n\nNo STA Stat:\n");

	return i4BytesWritten;
}
#endif


static int priv_driver_get_sta_statistics(
	IN struct net_device *prNetDev, IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4ArgNum = 3;
	struct PARAM_GET_STA_STATISTICS rQueryStaStatistics;
	int32_t rRssi;
	uint16_t u2LinkSpeed;
	uint32_t u4Per;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	kalMemZero(&rQueryStaStatistics,
		   sizeof(rQueryStaStatistics));
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
		struct BSS_INFO *prAisBssInfo =   aisGetAisBssInfo(
			prGlueInfo->prAdapter, wlanGetBssIdx(prNetDev));

		/* Get AIS AP address for no argument */
		if (prAisBssInfo->prStaRecOfAP) {
			COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr,
				prAisBssInfo
				->prStaRecOfAP->aucMacAddr);
			DBGLOG(RSN, INFO, "use ais ap "MACSTR"\n",
			       MAC2STR(prAisBssInfo
			       ->prStaRecOfAP->aucMacAddr));
		} else {
			DBGLOG(RSN, INFO, "not connect to ais ap %lx\n",
			       prAisBssInfo
			       ->prStaRecOfAP);
			i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
				"%s", "\n\nNo STA Stat:\n");
			return i4BytesWritten;
		}

		if (i4Argc == 2) {
			if (strnicmp(apcArgv[1], CMD_GET_STA_KEEP_CNT,
				     strlen(CMD_GET_STA_KEEP_CNT)) == 0)
				rQueryStaStatistics.ucReadClear = FALSE;
		}
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryStaStatistics,
			   &rQueryStaStatistics, sizeof(rQueryStaStatistics),
			   TRUE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_SUCCESS) {
		rRssi = RCPI_TO_dBm(rQueryStaStatistics.ucRcpi);
		u2LinkSpeed = rQueryStaStatistics.u2LinkSpeed == 0 ? 0 :
			      rQueryStaStatistics.u2LinkSpeed / 2;

		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "%s",
					     "\n\nSTA Stat:\n");

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"Tx total cnt           = %d\n",
			rQueryStaStatistics.u4TransmitCount);

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"Tx success	       = %d\n",
			rQueryStaStatistics.u4TransmitCount -
			rQueryStaStatistics.u4TransmitFailCount);

		u4Per = rQueryStaStatistics.u4TransmitCount == 0 ? 0 :
			(1000 * (rQueryStaStatistics.u4TransmitFailCount)) /
			rQueryStaStatistics.u4TransmitCount;
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"Tx fail count	       = %d, PER=%d.%d%%\n",
			rQueryStaStatistics.u4TransmitFailCount, u4Per / 10,
			u4Per % 10);

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"RSSI		       = %d\n", rRssi);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"LinkSpeed	       = %d\n", u2LinkSpeed);

	} else
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "%s",
					     "\n\nNo STA Stat:\n");

	return i4BytesWritten;

}


static int priv_driver_get_bss_statistics(
	IN struct net_device *prNetDev, IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	uint32_t u4BufLen;
	struct PARAM_LINK_SPEED_EX *prLinkSpeed;
	struct PARAM_GET_BSS_STATISTICS rQueryBssStatistics;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;
	int32_t i4BytesWritten = 0;
#if 0
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Argc = 0;
	uint32_t	u4Index;
#endif

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	wlanQueryInformation(prGlueInfo->prAdapter, wlanoidQueryBssid,
			     &arBssid[0], sizeof(arBssid), &u4BufLen);

#if 0 /* Todo:: Get the none-AIS statistics */
	if (i4Argc >= 2)
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Index);
#endif
	ucBssIndex = wlanGetBssIdx(prNetDev);
	if (!IS_BSS_INDEX_AIS(prGlueInfo->prAdapter, ucBssIndex))
		return WLAN_STATUS_FAILURE;

	/* 2. fill RSSI */
	if (kalGetMediaStateIndicated(prGlueInfo,
		ucBssIndex) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(REQ, WARN, "not yet connected\n");
		return WLAN_STATUS_SUCCESS;
	}
	rStatus = kalIoctlByBssIdx(prGlueInfo,
				   wlanoidQueryRssi,
				   &prLinkSpeed, sizeof(prLinkSpeed),
				   TRUE, FALSE, FALSE,
				   &u4BufLen, ucBssIndex);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "unable to retrieve rssi\n");


	/* 3 get per-BSS link statistics */
	if (rStatus == WLAN_STATUS_SUCCESS) {
		kalMemZero(&rQueryBssStatistics,
			   sizeof(rQueryBssStatistics));
		rQueryBssStatistics.ucBssIndex = ucBssIndex;

		rQueryBssStatistics.ucReadClear = TRUE;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryBssStatistics,
				   &rQueryBssStatistics,
				   sizeof(rQueryBssStatistics),
				   TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
				"%s", "\n\nStat:\n");
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s", "CurrentTemperature    = -\n");
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Tx success	       = %d\n",
				rQueryBssStatistics.u4TransmitCount -
				rQueryBssStatistics.u4TransmitFailCount);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Tx fail count	       = %d\n",
				rQueryBssStatistics.u4TransmitFailCount);
#if 0
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rx success	       = %ld\n", 0);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rx with CRC	       = %ld\n",
				prStatistics->rFCSErrorCount.QuadPart);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s", "Rx with PhyErr	     = 0\n");
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				"%s", "Rx with PlcpErr	     = 0\n");
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s", "Rx drop due to out of resource	= 0\n");
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rx duplicate frame    = %ld\n",
				prStatistics->rFrameDuplicateCount.QuadPart);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s", "False CCA	     = 0\n");
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"RSSI		       = %d\n", i4Rssi);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Last TX Rate	       = %s, %s, %s, %s, %s\n",
				"NA", "NA", "NA", "NA", "NA");
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Last RX Rate	       = %s, %s, %s, %s, %s\n",
				"NA", "NA", "NA", "NA", "NA");
#endif

		}

	} else {
		DBGLOG(REQ, WARN,
		       "unable to retrieve per-BSS link statistics\n");
	}


	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__,
	       pcCommand);

	return i4BytesWritten;

}

static u_int8_t priv_driver_get_sgi_info(
					IN struct PARAM_PEER_CAP *prWtblPeerCap)
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

static u_int8_t priv_driver_get_ldpc_info(
	IN struct PARAM_TX_CONFIG *prWtblTxConfig)
{
	if (!prWtblTxConfig)
		return FALSE;

	if (prWtblTxConfig->fgIsVHT)
		return prWtblTxConfig->fgVhtLDPC;
	else
		return prWtblTxConfig->fgLDPC;
}

int32_t priv_driver_rate_to_string(IN char *pcCommand,
				   IN int i4TotalLen, uint8_t TxRx,
				   struct PARAM_HW_WLAN_INFO *prHwWlanInfo)
{
	uint8_t i, txmode, rate, stbc;
	uint8_t nss;
	int32_t i4BytesWritten = 0;

	for (i = 0; i < AUTO_RATE_NUM; i++) {

		txmode = HW_TX_RATE_TO_MODE(
				 prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);
		if (txmode >= ENUM_TX_MODE_NUM)
			txmode = ENUM_TX_MODE_NUM - 1;
		rate = HW_TX_RATE_TO_MCS(
			       prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);
		nss = HW_TX_RATE_TO_NSS(
			      prHwWlanInfo->rWtblRateInfo.au2RateCode[i]) + 1;
		stbc = HW_TX_RATE_TO_STBC(
			       prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "\tRate index[%d] ", i);

		if (prHwWlanInfo->rWtblRateInfo.ucRateIdx == i) {
			if (TxRx == 0)
				i4BytesWritten += kalSnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "[Last RX Rate] ");
			else
				i4BytesWritten += kalSnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "[Last TX Rate] ");
		} else
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s", "               ");

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				rate < 4 ? HW_TX_RATE_CCK_STR[rate] :
					   HW_TX_RATE_CCK_STR[4]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				nicHwRateOfdmStr(rate));
		else {
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"NSS%d_MCS%d, ", nss, rate);
		}

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ", HW_TX_RATE_BW[
			prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				priv_driver_get_sgi_info(
					&prHwWlanInfo->rWtblPeerCap) == 0 ?
					"LGI" : "SGI");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s%s %s\n",
			HW_TX_MODE_STR[txmode], stbc ? "STBC" : " ",
			priv_driver_get_ldpc_info(&prHwWlanInfo->rWtblTxConfig)
			== 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}

static int32_t priv_driver_dump_helper_wtbl_info(IN char *pcCommand,
		IN int i4TotalLen, struct PARAM_HW_WLAN_INFO *prHwWlanInfo)
{
	uint8_t i;
	int32_t i4BytesWritten = 0;

	if (!pcCommand) {
		DBGLOG(HAL, ERROR, "%s: pcCommand is NULL.\n",
			__func__);
		return i4BytesWritten;
	}

	i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "%s",
		"\n\nwtbl:\n");
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"Dump WTBL info of WLAN_IDX	    = %d\n",
		prHwWlanInfo->u4Index);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\tAddr="MACSTR"\n",
		MAC2STR(prHwWlanInfo->rWtblTxConfig.aucPA));
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\tMUAR_Idx	 = %d\n",
		prHwWlanInfo->rWtblSecConfig.ucMUARIdx);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\trc_a1/rc_a2:%d/%d\n",
		prHwWlanInfo->rWtblSecConfig.fgRCA1,
		prHwWlanInfo->rWtblSecConfig.fgRCA2);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tKID:%d/RCID:%d/RKV:%d/RV:%d/IKV:%d/WPI_FLAG:%d\n",
		prHwWlanInfo->rWtblSecConfig.ucKeyID,
		prHwWlanInfo->rWtblSecConfig.fgRCID,
		prHwWlanInfo->rWtblSecConfig.fgRKV,
		prHwWlanInfo->rWtblSecConfig.fgRV,
		prHwWlanInfo->rWtblSecConfig.fgIKV,
		prHwWlanInfo->rWtblSecConfig.fgEvenPN);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s", "\tGID_SU:NA");
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\tsw/DIS_RHTR:%d/%d\n",
		prHwWlanInfo->rWtblTxConfig.fgSW,
		prHwWlanInfo->rWtblTxConfig.fgDisRxHdrTran);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tHT/VHT/HT-LDPC/VHT-LDPC/DYN_BW/MMSS:%d/%d/%d/%d/%d/%d\n",
		prHwWlanInfo->rWtblTxConfig.fgIsHT,
		prHwWlanInfo->rWtblTxConfig.fgIsVHT,
		prHwWlanInfo->rWtblTxConfig.fgLDPC,
		prHwWlanInfo->rWtblTxConfig.fgVhtLDPC,
		prHwWlanInfo->rWtblTxConfig.fgDynBw,
		prHwWlanInfo->rWtblPeerCap.ucMMSS);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tFCAP/G2/G4/G8/G16/CBRN:%d/%d/%d/%d/%d/%d\n",
		prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability,
		prHwWlanInfo->rWtblPeerCap.fgG2,
		prHwWlanInfo->rWtblPeerCap.fgG4,
		prHwWlanInfo->rWtblPeerCap.fgG8,
		prHwWlanInfo->rWtblPeerCap.fgG16,
		prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tHT-TxBF(tibf/tebf):%d/%d, VHT-TxBF(tibf/tebf):%d/%d, PFMU_IDX=%d\n",
		prHwWlanInfo->rWtblTxConfig.fgTIBF,
		prHwWlanInfo->rWtblTxConfig.fgTEBF,
		prHwWlanInfo->rWtblTxConfig.fgVhtTIBF,
		prHwWlanInfo->rWtblTxConfig.fgVhtTEBF,
		prHwWlanInfo->rWtblTxConfig.ucPFMUIdx);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s", "\tSPE_IDX=NA\n");
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tBA Enable:0x%x, BAFail Enable:%d\n",
		prHwWlanInfo->rWtblBaConfig.ucBaEn,
		prHwWlanInfo->rWtblTxConfig.fgBAFEn);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tQoS Enable:%d\n", prHwWlanInfo->rWtblTxConfig.fgIsQoS);
	if (prHwWlanInfo->rWtblTxConfig.fgIsQoS) {
		for (i = 0; i < 8; i += 2) {
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\t\tBA WinSize: TID 0 - %d, TID 1 - %d\n",
				(uint32_t)
				    ((prHwWlanInfo->rWtblBaConfig.u4BaWinSize >>
				    (i * 3)) & BITS(0, 2)),
				(uint32_t)
				    ((prHwWlanInfo->rWtblBaConfig.u4BaWinSize >>
				    ((i + 1) * 3)) & BITS(0, 2)));
		}
	}

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\tpartial_aid:%d\n",
		prHwWlanInfo->rWtblTxConfig.u2PartialAID);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\twpi_even:%d\n",
		prHwWlanInfo->rWtblSecConfig.fgEvenPN);
	i4BytesWritten += scnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\tAAD_OM/CipherSuit:%d/%d\n",
		prHwWlanInfo->rWtblTxConfig.fgAADOM,
		prHwWlanInfo->rWtblSecConfig.ucCipherSuit);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\taf:%d\n",
		prHwWlanInfo->rWtblPeerCap.ucAmpduFactor);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\trdg_ba:%d/rdg capability:%d\n",
		prHwWlanInfo->rWtblTxConfig.fgRdgBA,
		prHwWlanInfo->rWtblTxConfig.fgRDG);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\tcipher_suit:%d\n",
		prHwWlanInfo->rWtblSecConfig.ucCipherSuit);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\tFromDS:%d\n",
		prHwWlanInfo->rWtblTxConfig.fgIsFromDS);
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\tToDS:%d\n",
		prHwWlanInfo->rWtblTxConfig.fgIsToDS);
#if 0
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tRCPI = %d %d %d %d\n",
		prHwWlanInfo->rWtblRxCounter.ucRxRcpi0,
		prHwWlanInfo->rWtblRxCounter.ucRxRcpi1,
		prHwWlanInfo->rWtblRxCounter.ucRxRcpi2,
		prHwWlanInfo->rWtblRxCounter.ucRxRcpi3);
#endif
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tRSSI = %d %d %d %d\n",
		RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi0),
		RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi1),
		RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi2),
		RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi3));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s", "\tRate Info\n");

	i4BytesWritten += priv_driver_rate_to_string(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, 1, prHwWlanInfo);

#if 0
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten, i4TotalLen -
				      i4BytesWritten,
		"%s", "\t===Key======\n");
	for (i = 0; i < 32; i += 8) {
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\t0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 0],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 1],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 2],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 3],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 4],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 5],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 6],
			prHwWlanInfo->rWtblKeyConfig.aucKey[i + 7]);
	}
#endif

	return i4BytesWritten;
}

static int priv_driver_get_wtbl_info_default(
	IN struct GLUE_INFO *prGlueInfo,
	IN uint32_t u4Index,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	struct PARAM_HW_WLAN_INFO *prHwWlanInfo;

	prHwWlanInfo = (struct PARAM_HW_WLAN_INFO *)kalMemAlloc(
			sizeof(struct PARAM_HW_WLAN_INFO), VIR_MEM_TYPE);
	if (!prHwWlanInfo)
		return i4BytesWritten;

	prHwWlanInfo->u4Index = u4Index;
	DBGLOG(REQ, INFO, "%s : index = %d\n",
		__func__,
		prHwWlanInfo->u4Index);

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryWlanInfo,
		prHwWlanInfo,
		sizeof(struct PARAM_HW_WLAN_INFO),
		TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, INFO, "rStatus %u u4BufLen = %d\n", rStatus, u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		kalMemFree(prHwWlanInfo, VIR_MEM_TYPE,
		sizeof(struct PARAM_HW_WLAN_INFO));
		return i4BytesWritten;
	}
	i4BytesWritten = priv_driver_dump_helper_wtbl_info(
		pcCommand,
		i4TotalLen,
		prHwWlanInfo);

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	kalMemFree(prHwWlanInfo, VIR_MEM_TYPE,
		   sizeof(struct PARAM_HW_WLAN_INFO));

	return i4BytesWritten;
}

static int priv_driver_get_wtbl_info(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int32_t u4Ret = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Index = 0;
	struct CHIP_DBG_OPS *prDbgOps;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2)
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Index);

	if (u4Ret)
		return -1;

	prDbgOps = prGlueInfo->prAdapter->chip_info->prDebugOps;

	if (prDbgOps->showWtblInfo)
		return prDbgOps->showWtblInfo(
			prGlueInfo->prAdapter, u4Index, pcCommand, i4TotalLen);
	else
		return priv_driver_get_wtbl_info_default(
			prGlueInfo, u4Index, pcCommand, i4TotalLen);
}

static int priv_driver_get_sta_info(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t ucWlanIndex = 0;
	uint8_t *pucMacAddr = NULL;
	struct PARAM_HW_WLAN_INFO *prHwWlanInfo;
	struct PARAM_GET_STA_STATISTICS rQueryStaStatistics;
	int32_t rRssi;
	uint16_t u2LinkSpeed;
	uint32_t u4Per;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	kalMemZero(&rQueryStaStatistics, sizeof(rQueryStaStatistics));
	rQueryStaStatistics.ucReadClear = TRUE;

	/* DBGLOG(RSN, INFO, "MT6632 : priv_driver_get_sta_info\n"); */
	if (i4Argc >= 3) {
		if (strnicmp(apcArgv[1], CMD_GET_STA_KEEP_CNT,
		    strlen(CMD_GET_STA_KEEP_CNT)) == 0) {
			wlanHwAddrToBin(apcArgv[2], &aucMacAddr[0]);
			rQueryStaStatistics.ucReadClear = FALSE;
		} else if (strnicmp(apcArgv[2], CMD_GET_STA_KEEP_CNT,
		    strlen(CMD_GET_STA_KEEP_CNT)) == 0) {
			wlanHwAddrToBin(apcArgv[1], &aucMacAddr[0]);
			rQueryStaStatistics.ucReadClear = FALSE;
		}

		if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter,
		    &aucMacAddr[0], &ucWlanIndex))
			return i4BytesWritten;
	} else {
		struct BSS_INFO *prAisBssInfo =   aisGetAisBssInfo(
			prGlueInfo->prAdapter, wlanGetBssIdx(prNetDev));

		/* Get AIS AP address for no argument */
		if (prAisBssInfo->prStaRecOfAP)
			ucWlanIndex = prAisBssInfo
					->prStaRecOfAP->ucWlanIndex;
		else if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter, NULL,
		    &ucWlanIndex)) /* try get a peer */
			return i4BytesWritten;

		if (i4Argc == 2) {
			if (strnicmp(apcArgv[1], CMD_GET_STA_KEEP_CNT,
			    strlen(CMD_GET_STA_KEEP_CNT)) == 0)
				rQueryStaStatistics.ucReadClear = FALSE;
		}
	}

	prHwWlanInfo = (struct PARAM_HW_WLAN_INFO *)kalMemAlloc(
			sizeof(struct PARAM_HW_WLAN_INFO), VIR_MEM_TYPE);
	if (prHwWlanInfo == NULL) {
		DBGLOG(REQ, INFO, "prHwWlanInfo is null\n");
		return -1;
	}

	prHwWlanInfo->u4Index = ucWlanIndex;

	DBGLOG(REQ, INFO, "MT6632 : index = %d i4TotalLen = %d\n",
	       prHwWlanInfo->u4Index, i4TotalLen);

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryWlanInfo, prHwWlanInfo,
			   sizeof(struct PARAM_HW_WLAN_INFO),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		kalMemFree(prHwWlanInfo, VIR_MEM_TYPE,
			   sizeof(struct PARAM_HW_WLAN_INFO));
		return -1;
	}

	i4BytesWritten = priv_driver_dump_helper_wtbl_info(pcCommand,
						i4TotalLen, prHwWlanInfo);

	pucMacAddr = wlanGetStaAddrByWlanIdx(prGlueInfo->prAdapter,
					     ucWlanIndex);
	if (pucMacAddr) {
		COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr, pucMacAddr);
		/* i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
		 *      i4TotalLen - i4BytesWritten,
		 *      "\tAddr="MACSTR"\n",
		 *      MAC2STR(rQueryStaStatistics.aucMacAddr));
		 */

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryStaStatistics,
				   &rQueryStaStatistics,
				   sizeof(rQueryStaStatistics),
				   TRUE, FALSE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			rRssi = RCPI_TO_dBm(rQueryStaStatistics.ucRcpi);
			u2LinkSpeed = rQueryStaStatistics.u2LinkSpeed == 0 ?
				0 : rQueryStaStatistics.u2LinkSpeed/2;

			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s", "\n\nSTA Stat:\n");

			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Tx total cnt           = %d\n",
				rQueryStaStatistics.u4TransmitCount);

			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Tx success	       = %d\n",
				rQueryStaStatistics.u4TransmitCount -
				rQueryStaStatistics.u4TransmitFailCount);

			u4Per = rQueryStaStatistics.u4TransmitCount == 0 ? 0 :
				(1000 *
				(rQueryStaStatistics.u4TransmitFailCount)) /
				rQueryStaStatistics.u4TransmitCount;
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Tx fail count	       = %d, PER=%d.%1d%%\n",
				rQueryStaStatistics.u4TransmitFailCount,
				u4Per/10, u4Per%10);

			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"RSSI		       = %d\n", rRssi);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"LinkSpeed	       = %d\n", u2LinkSpeed);
		}
	}
	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	kalMemFree(prHwWlanInfo, VIR_MEM_TYPE,
		   sizeof(struct PARAM_HW_WLAN_INFO));

	return i4BytesWritten;
}

static int priv_driver_get_mib_info(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	uint8_t i;
	uint32_t u4Per;
	int32_t u4Ret = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct PARAM_HW_MIB_INFO *prHwMibInfo;
	struct RX_CTRL *prRxCtrl;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	prRxCtrl = &prGlueInfo->prAdapter->rRxCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	DBGLOG(REQ, INFO, "MT6632 : priv_driver_get_mib_info\n");

	prHwMibInfo = (struct PARAM_HW_MIB_INFO *)kalMemAlloc(
				sizeof(struct PARAM_HW_MIB_INFO), VIR_MEM_TYPE);
	if (!prHwMibInfo)
		return -1;

	if (i4Argc == 1)
		prHwMibInfo->u4Index = 0;

	if (i4Argc >= 2)
		u4Ret = kalkStrtou32(apcArgv[1], 0, &prHwMibInfo->u4Index);

	DBGLOG(REQ, INFO, "MT6632 : index = %d\n", prHwMibInfo->u4Index);

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryMibInfo, prHwMibInfo,
			   sizeof(struct PARAM_HW_MIB_INFO),
			   TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		kalMemFree(prHwMibInfo, VIR_MEM_TYPE,
			   sizeof(struct PARAM_HW_MIB_INFO));
		return -1;
	}

	if (prHwMibInfo->u4Index < 2) {
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "%s",
			"\n\nmib state:\n");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"Dump MIB info of IDX         = %d\n",
			prHwMibInfo->u4Index);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "===Rx Related Counters===\n");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx with CRC=%d\n",
			prHwMibInfo->rHwMibCnt.u4RxFcsErrCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx drop due to out of resource=%d\n",
			prHwMibInfo->rHwMibCnt.u4RxFifoFullCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx Mpdu=%d\n",
			prHwMibInfo->rHwMibCnt.u4RxMpduCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx AMpdu=%d\n",
			prHwMibInfo->rHwMibCnt.u4RxAMPDUCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx PF Drop=%d\n",
			prHwMibInfo->rHwMibCnt.u4PFDropCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx Len Mismatch=%d\n",
			prHwMibInfo->rHwMibCnt.u4RxLenMismatchCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx data indicate total=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_INDICATION_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx data retain total=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx drop by SW total=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx reorder miss=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_MISS_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx reorder within=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_WITHIN_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx reorder ahead=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_AHEAD_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx reorder behind=%llu\n", RX_GET_CNT(prRxCtrl,
			RX_DATA_REORDER_BEHIND_COUNT));

		do {
			uint32_t u4AmsduCntx100 = 0;

			if (RX_GET_CNT(prRxCtrl, RX_DATA_AMSDU_COUNT))
				u4AmsduCntx100 =
					(uint32_t)div64_u64(RX_GET_CNT(prRxCtrl,
					    RX_DATA_MSDU_IN_AMSDU_COUNT) * 100,
					    RX_GET_CNT(prRxCtrl,
					    RX_DATA_AMSDU_COUNT));

			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\tRx avg MSDU in AMSDU=%1d.%02d\n",
				u4AmsduCntx100 / 100, u4AmsduCntx100 % 100);
		} while (FALSE);

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx total MSDU in AMSDU=%llu\n", RX_GET_CNT(prRxCtrl,
			RX_DATA_MSDU_IN_AMSDU_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx AMSDU=%llu\n", RX_GET_CNT(prRxCtrl,
			RX_DATA_AMSDU_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx AMSDU miss=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_AMSDU_MISS_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx no StaRec drop=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_NO_STA_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx inactive BSS drop=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_INACTIVE_BSS_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx HS20 drop=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_HS20_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx low SwRfb drop=%llu\n", RX_GET_CNT(prRxCtrl,
			RX_LESS_SW_RFB_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx dupicate drop=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_DUPICATE_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx MIC err drop=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_MIC_ERROR_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx BAR handle=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_BAR_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx non-interest drop=%llu\n", RX_GET_CNT(prRxCtrl,
			RX_NO_INTEREST_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx type err drop=%llu\n",
			RX_GET_CNT(prRxCtrl, RX_TYPE_ERR_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx class err drop=%llu\n", RX_GET_CNT(prRxCtrl,
			RX_CLASS_ERR_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "===Phy/Timing Related Counters===\n");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tChannelIdleCnt=%d\n",
			prHwMibInfo->rHwMibCnt.u4ChannelIdleCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tCCA_NAV_Tx_Time=%d\n",
			prHwMibInfo->rHwMibCnt.u4CcaNavTx);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx_MDRDY_CNT=%d\n",
			prHwMibInfo->rHwMibCnt.u4MdrdyCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tCCK_MDRDY=%d, OFDM_MDRDY=0x%x, OFDM_GREEN_MDRDY=0x%x\n",
			prHwMibInfo->rHwMibCnt.u4CCKMdrdyCnt,
			prHwMibInfo->rHwMibCnt.u4OFDMLGMixMdrdy,
			prHwMibInfo->rHwMibCnt.u4OFDMGreenMdrdy);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tPrim CCA Time=%d\n",
			prHwMibInfo->rHwMibCnt.u4PCcaTime);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tSec CCA Time=%d\n",
			prHwMibInfo->rHwMibCnt.u4SCcaTime);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tPrim ED Time=%d\n",
			prHwMibInfo->rHwMibCnt.u4PEDTime);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s",
			"===Tx Related Counters(Generic)===\n");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tBeaconTxCnt=%d\n",
			prHwMibInfo->rHwMibCnt.u4BeaconTxCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tTx 40MHz Cnt=%d\n",
			prHwMibInfo->rHwMib2Cnt.u4Tx40MHzCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tTx 80MHz Cnt=%d\n",
			prHwMibInfo->rHwMib2Cnt.u4Tx80MHzCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tTx 160MHz Cnt=%d\n",
			prHwMibInfo->rHwMib2Cnt.u4Tx160MHzCnt);
		for (i = 0; i < BSSID_NUM; i++) {
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\t===BSSID[%d] Related Counters===\n", i);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\tBA Miss Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4BaMissedCnt[i]);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\tRTS Tx Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4RtsTxCnt[i]);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\tFrame Retry Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4FrameRetryCnt[i]);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\tFrame Retry 2 Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4FrameRetry2Cnt[i]);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\tRTS Retry Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4RtsRetryCnt[i]);
			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\tAck Failed Cnt=%d\n",
				prHwMibInfo->rHwMibCnt.au4AckFailedCnt[i]);
		}

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "===AMPDU Related Counters===\n");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tTx AMPDU_Pkt_Cnt=%d\n",
			prHwMibInfo->rHwTxAmpduMts.u2TxAmpduCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tTx AMPDU_MPDU_Pkt_Cnt=%d\n",
			prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tAMPDU SuccessCnt=%d\n",
			prHwMibInfo->rHwTxAmpduMts.u4TxAckSfCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tAMPDU Tx success      = %d\n",
			prHwMibInfo->rHwTxAmpduMts.u4TxAckSfCnt);

		u4Per = prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt == 0 ? 0 :
			(1000 * (prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt -
			prHwMibInfo->rHwTxAmpduMts.u4TxAckSfCnt)) /
			prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt;
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tAMPDU Tx fail count   = %d, PER=%d.%1d%%\n",
			prHwMibInfo->rHwTxAmpduMts.u4TxSfCnt -
			prHwMibInfo->rHwTxAmpduMts.u4TxAckSfCnt,
			u4Per/10, u4Per%10);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s", "\tTx Agg\n");
#if (CFG_SUPPORT_802_11AX == 1)
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\tRange:  1    2~9   10~18    19~27   ");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "28~36    37~45    46~54    55~78\n");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\t\t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d\n",
			prHwMibInfo->rHwTxAmpduMts.u2TxRange1AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange2AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange3AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange4AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange5AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange6AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange7AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange8AmpduCnt);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\tRange: 79~102 103~126 127~150 151~174 ");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "174~198 199~222 223~246 247~255\n");
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\t\t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d\n",
			prHwMibInfo->rHwTxAmpduMts.u2TxRange9AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange10AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange11AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange12AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange13AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange14AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange15AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange16AmpduCnt);
#else
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s",
			"\tRange:  1    2~5   6~15    16~22   23~33    34~49    50~57    58~64\n"
			);
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\t\t%d \t%d \t%d \t%d \t%d \t%d \t%d \t%d\n",
			prHwMibInfo->rHwTxAmpduMts.u2TxRange1AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange2AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange3AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange4AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange5AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange6AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange7AmpduCnt,
			prHwMibInfo->rHwTxAmpduMts.u2TxRange8AmpduCnt);
#endif
	} else
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "%s",
					     "\nClear All Statistics\n");

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	kalMemFree(prHwMibInfo, VIR_MEM_TYPE, sizeof(struct PARAM_HW_MIB_INFO));

	nicRxClearStatistics(prGlueInfo->prAdapter);

	return i4BytesWritten;
}

static int priv_driver_set_fw_log(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4McuDest = 0;
	uint32_t u4LogType = 0;
	struct CMD_FW_LOG_2_HOST_CTRL *prFwLog2HostCtrl;
	uint32_t u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(RSN, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	DBGLOG(RSN, INFO, "MT6632 : priv_driver_set_fw_log\n");

	prFwLog2HostCtrl = (struct CMD_FW_LOG_2_HOST_CTRL *)kalMemAlloc(
			sizeof(struct CMD_FW_LOG_2_HOST_CTRL), VIR_MEM_TYPE);
	if (!prFwLog2HostCtrl)
		return -1;

	if (i4Argc == 3) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4McuDest);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse u4McuDest error u4Ret=%d\n",
			       u4Ret);

		u4Ret = kalkStrtou32(apcArgv[2], 0, &u4LogType);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse u4LogType error u4Ret=%d\n",
			       u4Ret);

		prFwLog2HostCtrl->ucMcuDest = (uint8_t)u4McuDest;
		prFwLog2HostCtrl->ucFwLog2HostCtrl = (uint8_t)u4LogType;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetFwLog2Host,
				   prFwLog2HostCtrl,
				   sizeof(struct CMD_FW_LOG_2_HOST_CTRL),
				   TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, INFO, "%s: command result is %s (%d %d)\n",
		       __func__, pcCommand, u4McuDest, u4LogType);
		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			kalMemFree(prFwLog2HostCtrl, VIR_MEM_TYPE,
				   sizeof(struct CMD_FW_LOG_2_HOST_CTRL));
			return -1;
		}
	} else {
		DBGLOG(REQ, ERROR, "argc %i is not equal to 3\n", i4Argc);
		i4BytesWritten = -1;
	}

	kalMemFree(prFwLog2HostCtrl, VIR_MEM_TYPE,
		   sizeof(struct CMD_FW_LOG_2_HOST_CTRL));
	return i4BytesWritten;
}
#endif

static int priv_driver_get_mcr(IN struct net_device *prNetDev,
			       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret;
	int32_t i4ArgNum = 2;
	struct CMD_ACCESS_REG rCmdAccessReg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		rCmdAccessReg.u4Address = 0;
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rCmdAccessReg.u4Address));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "parse get_mcr error (Address) u4Ret=%d\n",
			       u4Ret);

		/* rCmdAccessReg.u4Address = kalStrtoul(apcArgv[1], NULL, 0); */
		rCmdAccessReg.u4Data = 0;

		DBGLOG(REQ, LOUD, "address is %x\n", rCmdAccessReg.u4Address);

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryMcrRead,
				   &rCmdAccessReg, sizeof(rCmdAccessReg),
				   TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "0x%08x",
					  (unsigned int)rCmdAccessReg.u4Data);
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__,
		       pcCommand);
	}

	return i4BytesWritten;

}				/* priv_driver_get_mcr */

int priv_driver_set_mcr(IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret;
	int32_t i4ArgNum = 3;
	struct CMD_ACCESS_REG rCmdAccessReg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rCmdAccessReg.u4Address));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "parse get_mcr error (Address) u4Ret=%d\n",
			       u4Ret);

		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rCmdAccessReg.u4Data));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "parse get_mcr error (Data) u4Ret=%d\n", u4Ret);

		/* rCmdAccessReg.u4Address = kalStrtoul(apcArgv[1], NULL, 0); */
		/* rCmdAccessReg.u4Data = kalStrtoul(apcArgv[2], NULL, 0); */

		rStatus = kalIoctl(prGlueInfo, wlanoidSetMcrWrite,
				   &rCmdAccessReg, sizeof(rCmdAccessReg),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	}

	return i4BytesWritten;

}

static int priv_driver_set_test_mode(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret;
	int32_t i4ArgNum = 2, u4MagicKey = -1;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &(u4MagicKey));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse Magic Key error u4Ret=%d\n",
			       u4Ret);

		DBGLOG(REQ, LOUD, "The Set Test Mode Magic Key is %d\n",
		       u4MagicKey);

		if (u4MagicKey == PRIV_CMD_TEST_MAGIC_KEY) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidRftestSetTestMode,
					   NULL, 0, FALSE, FALSE, TRUE,
					   &u4BufLen);
		} else if (u4MagicKey == 0) {
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidRftestSetAbortTestMode,
					   NULL, 0, FALSE, FALSE, TRUE,
					   &u4BufLen);
		}

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}

	return i4BytesWritten;

}				/* priv_driver_set_test_mode */

static int priv_driver_set_test_cmd(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret;
	int32_t i4ArgNum = 3;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		rRfATInfo.u4FuncIndex = 0;
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rRfATInfo.u4FuncIndex));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "Parse Test CMD Index error u4Ret=%d\n", u4Ret);

		rRfATInfo.u4FuncData = 0;
		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rRfATInfo.u4FuncData));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "Parse Test CMD Data error u4Ret=%d\n", u4Ret);

		DBGLOG(REQ, LOUD,
		       "Set Test CMD FuncIndex = %d, FuncData = %d\n",
		       rRfATInfo.u4FuncIndex, rRfATInfo.u4FuncData);

		rStatus = kalIoctl(prGlueInfo, wlanoidRftestSetAutoTest,
				   &rRfATInfo, sizeof(rRfATInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}

	return i4BytesWritten;

}				/* priv_driver_set_test_cmd */

static int priv_driver_get_test_result(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret;
	uint32_t u4Data = 0;
	int32_t i4ArgNum = 3;
	struct PARAM_MTK_WIFI_TEST_STRUCT rRfATInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		rRfATInfo.u4FuncIndex = 0;
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rRfATInfo.u4FuncIndex));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "Parse Test CMD Index error u4Ret=%d\n", u4Ret);

		rRfATInfo.u4FuncData = 0;
		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rRfATInfo.u4FuncData));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "Parse Test CMD Data error u4Ret=%d\n", u4Ret);

		DBGLOG(REQ, LOUD,
		       "Get Test CMD FuncIndex = %d, FuncData = %d\n",
		       rRfATInfo.u4FuncIndex, rRfATInfo.u4FuncData);

		rStatus = kalIoctl(prGlueInfo, wlanoidRftestQueryAutoTest,
				   &rRfATInfo, sizeof(rRfATInfo),
				   TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
		u4Data = (unsigned int)rRfATInfo.u4FuncData;
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
						"%d[0x%08x]", u4Data, u4Data);
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__,
		       pcCommand);
	}

	return i4BytesWritten;

}				/* priv_driver_get_test_result */

#if (CFG_SUPPORT_RA_GEN == 1)

static int32_t priv_driver_set_ra_debug_proc(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int8_t *this_char = NULL;
	int32_t i4Recv = 0;
	uint32_t u4WCID = 0, u4DebugType = 0;
	uint32_t u4Id = 0xa0650000;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT *prSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %d, apcArgv[0] = %s\n\n", i4Argc, *apcArgv);

	this_char = kalStrStr(*apcArgv, "=");
	if (!this_char)
		return -1;
	this_char++;

	DBGLOG(REQ, LOUD, "string = %s\n", this_char);

	i4Recv = sscanf(this_char, "%d:%d", &(u4WCID), &(u4DebugType));
	if (i4Recv < 0)
		return -1;

	prSwCtrlInfo =
		(struct PARAM_CUSTOM_SW_CTRL_STRUCT *)kalMemAlloc(
				sizeof(struct PARAM_CUSTOM_SW_CTRL_STRUCT),
				VIR_MEM_TYPE);
	if (!prSwCtrlInfo)
		return -1;

	if (i4Recv == 2) {
		prSwCtrlInfo->u4Id = u4Id;
		prSwCtrlInfo->u4Data = u4WCID |
					((u4DebugType & BITS(0, 15)) << 16);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite,
				   prSwCtrlInfo,
				   sizeof(struct PARAM_CUSTOM_SW_CTRL_STRUCT),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			kalMemFree(prSwCtrlInfo, VIR_MEM_TYPE,
				   sizeof(struct PARAM_CUSTOM_SW_CTRL_STRUCT));
			return -1;
		}

		DBGLOG(REQ, LOUD, "WlanIdx=%d\nDebugType=%d\nu4Data=0x%08x\n",
			u4WCID, u4DebugType, prSwCtrlInfo->u4Data);
	} else {
		DBGLOG(INIT, ERROR,
		       "iwpriv wlanXX driver RaDebug=[wlanIdx]:[debugType]\n");
	}

	kalMemFree(prSwCtrlInfo, VIR_MEM_TYPE,
		   sizeof(struct PARAM_CUSTOM_SW_CTRL_STRUCT));

	return i4BytesWritten;
}

int priv_driver_set_fixed_fallback(IN struct net_device *prNetDev,
				   IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	/* INT_32 u4Ret = 0; */
	uint32_t u4WCID = 0;
	uint32_t u4Mode = 0, u4Bw = 0, u4Mcs = 0, u4VhtNss = 0, u4Band = 0;
	uint32_t u4SGI = 0, u4Preamble = 0, u4STBC = 0, u4LDPC = 0, u4SpeEn = 0;
	int32_t i4Recv = 0;
	int8_t *this_char = NULL;
	uint32_t u4Id = 0xa0660000;
	uint32_t u4Data = 0x80000000;
	uint32_t u4Id2 = 0xa0600000;
	uint8_t u4Nsts = 1;
	u_int8_t fgStatus = TRUE;
	static uint8_t fgIsUseWCID = FALSE;

	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

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
	} else if (strnicmp(this_char, "UseWCID", strlen("UseWCID")) == 0) {
		i4Recv = 2;
		fgIsUseWCID = TRUE;
	} else if (strnicmp(this_char, "ApplyAll", strlen("ApplyAll")) == 0) {
		i4Recv = 3;
		fgIsUseWCID = FALSE;
	} else {
		i4Recv = sscanf(this_char, "%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d",
				&(u4WCID), &(u4Mode), &(u4Bw), &(u4Mcs),
				&(u4VhtNss), &(u4SGI), &(u4Preamble), &(u4STBC),
				&(u4LDPC), &(u4SpeEn), &(u4Band));

		DBGLOG(REQ, LOUD, "u4WCID=%d\nu4Mode=%d\nu4Bw=%d\n",
		       u4WCID, u4Mode, u4Bw);
		DBGLOG(REQ, LOUD, "u4Mcs=%d\nu4VhtNss=%d\nu4SGI=%d\n",
		       u4Mcs, u4VhtNss, u4SGI);
		DBGLOG(REQ, LOUD, "u4Preamble=%d\nu4STBC=%d\n",
		       u4Preamble, u4STBC);
		DBGLOG(REQ, LOUD, "u4LDPC=%d\nu4SpeEn=%d\nu4Band=%d\n",
		       u4LDPC, u4SpeEn, u4Band);
		DBGLOG(REQ, LOUD, "fgIsUseWCID=%d\n\n",
		       fgIsUseWCID);
	}

	if (i4Recv == 1) {
		rSwCtrlInfo.u4Id = u4Id2;
		rSwCtrlInfo.u4Data = 0;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	} else if (i4Recv == 2 || i4Recv == 3) {
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
			"Update fgIsUseWCID %d\n", fgIsUseWCID);
	} else if (i4Recv == 11) {
		rSwCtrlInfo.u4Id = u4Id;
		rSwCtrlInfo.u4Data = u4Data;
		if (fgIsUseWCID && u4WCID < WTBL_SIZE &&
			prAdapter->rWifiVar.arWtbl[u4WCID].ucUsed) {
			rSwCtrlInfo.u4Id |= u4WCID;
			rSwCtrlInfo.u4Id |= BIT(8);
			i4BytesWritten = kalSnprintf(
				pcCommand, i4TotalLen,
				"Apply WCID %d\n", u4WCID);
		} else {
			i4BytesWritten = kalSnprintf(
				pcCommand, i4TotalLen, "Apply All\n");
		}

		if (u4SGI)
			rSwCtrlInfo.u4Data |= BIT(30);
		if (u4LDPC)
			rSwCtrlInfo.u4Data |= BIT(29);
		if (u4SpeEn)
			rSwCtrlInfo.u4Data |= BIT(28);
		if (u4Band)
			rSwCtrlInfo.u4Data |= BIT(25);
		if (u4STBC)
			rSwCtrlInfo.u4Data |= BIT(11);

		if (u4Bw <= 3)
			rSwCtrlInfo.u4Data |= ((u4Bw << 26) & BITS(26, 27));
		else {
			fgStatus = FALSE;
			DBGLOG(INIT, ERROR,
			       "Wrong BW! BW20=0, BW40=1, BW80=2,BW160=3\n");
		}
		if (u4Mode <= 4) {
			rSwCtrlInfo.u4Data |= ((u4Mode << 6) & BITS(6, 8));

			switch (u4Mode) {
			case 0:
				if (u4Mcs <= 3)
					rSwCtrlInfo.u4Data |= u4Mcs;
				else {
					fgStatus = FALSE;
					DBGLOG(INIT, ERROR,
					       "CCK mode but wrong MCS!\n");
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
					DBGLOG(INIT, ERROR,
					       "OFDM mode but wrong MCS!\n");
				break;
				}
			break;
			case 2:
			case 3:
				if (u4Mcs <= 32)
					rSwCtrlInfo.u4Data |= u4Mcs;
				else {
					fgStatus = FALSE;
					DBGLOG(INIT, ERROR,
					       "HT mode but wrong MCS!\n");
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
				    DBGLOG(INIT, ERROR,
					"VHT mode but wrong MCS!\n");
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
			DBGLOG(INIT, ERROR,
			       "Wrong TxMode! CCK=0, OFDM=1, HT=2, GF=3, VHT=4\n");
		}

		rSwCtrlInfo.u4Data |= (((u4Nsts - 1) << 9) & BITS(9, 10));

		if (fgStatus) {
			rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite,
					   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
					   FALSE, FALSE, TRUE, &u4BufLen);
		}

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver FixedRate=Option\n");
		DBGLOG(INIT, ERROR,
		       "Option:[WCID]-[Mode]-[BW]-[MCS]-[VhtNss]-[SGI]-[Preamble]-[STBC]-[LDPC]-[SPE_EN]-[BAND]\n");
		DBGLOG(INIT, ERROR, "[WCID]Wireless Client ID\n");
		DBGLOG(INIT, ERROR, "[Mode]CCK=0, OFDM=1, HT=2, GF=3, VHT=4\n");
		DBGLOG(INIT, ERROR, "[BW]BW20=0, BW40=1, BW80=2,BW160=3\n");
		DBGLOG(INIT, ERROR,
		       "[MCS]CCK=0~3, OFDM=0~7, HT=0~32, VHT=0~9\n");
		DBGLOG(INIT, ERROR, "[VhtNss]VHT=1~4, Other=ignore\n");
		DBGLOG(INIT, ERROR, "[Preamble]Long=0, Other=Short\n");
		DBGLOG(INIT, ERROR, "[BAND]2G=0, Other=5G\n");
	}

	return i4BytesWritten;
}
#endif

#if (CFG_SUPPORT_TXPOWER_INFO == 1)
static int32_t priv_driver_dump_txpower_info(struct ADAPTER *prAdapter,
		IN char *pcCommand, IN int i4TotalLen,
		struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T *prTxPowerInfo)
{
	int32_t i4BytesWritten = 0;

	if (prTxPowerInfo->ucTxPowerCategory ==
	    TXPOWER_EVENT_SHOW_ALL_RATE_TXPOWER_INFO) {
		uint8_t ucTxPwrIdx = 0, ucTxPwrType = 0, ucIdx = 0,
			ucIdxOffset = 0;

		char *rateStr = NULL;
		struct FRAME_POWER_CONFIG_INFO_T rRatePowerInfo;
		char *cckRateStr[MODULATION_SYSTEM_CCK_NUM] = {
			"1M", "2M", "5M", "11M"};
		char *ofdmRateStr[MODULATION_SYSTEM_OFDM_NUM] = {
			"6M", "9M", "12M", "18M", "24M", "36M", "48M", "54M" };
		char *ht20RateStr[MODULATION_SYSTEM_HT20_NUM] = {
			"M0", "M1", "M2", "M3", "M4", "M5", "M6", "M7" };
		char *ht40RateStr[MODULATION_SYSTEM_HT40_NUM] = {
			"M0", "M1", "M2", "M3", "M4", "M5", "M6",
			"M7", "M32" };
		char *vhtRateStr[MODULATION_SYSTEM_VHT20_NUM] = {
			"M0", "M1", "M2", "M3", "M4", "M5", "M6",
			"M7", "M8", "M9" };
		char *ruRateStr[MODULATION_SYSTEM_HE_26_MCS_NUM] = {
			"M0", "M1", "M2", "M3", "M4", "M5", "M6", "M7",
			"M8", "M9", "M10", "M11" };

		uint8_t *pucStr = NULL;
		uint8_t *POWER_TYPE_STR[] = {
			"CCK", "OFDM", "HT20", "HT40",
			"VHT20", "VHT40", "VHT80",
			"VHT160", "RU26", "RU52", "RU106",
			"RU242", "RU484", "RU996"};
		uint8_t ucPwrIdxLen[] = {
		    MODULATION_SYSTEM_CCK_NUM,
			MODULATION_SYSTEM_OFDM_NUM,
		    MODULATION_SYSTEM_HT20_NUM,
		    MODULATION_SYSTEM_HT40_NUM,
		    MODULATION_SYSTEM_VHT20_NUM,
		    MODULATION_SYSTEM_VHT40_NUM,
		    MODULATION_SYSTEM_VHT80_NUM,
		    MODULATION_SYSTEM_VHT160_NUM,
		    MODULATION_SYSTEM_HE_26_MCS_NUM,
		    MODULATION_SYSTEM_HE_52_MCS_NUM,
		    MODULATION_SYSTEM_HE_106_MCS_NUM,
		    MODULATION_SYSTEM_HE_242_MCS_NUM,
		    MODULATION_SYSTEM_HE_484_MCS_NUM,
		    MODULATION_SYSTEM_HE_996_MCS_NUM};

		uint8_t ucBandIdx;
		uint8_t ucFormat;

		if ((sizeof(POWER_TYPE_STR)/sizeof(uint8_t *)) !=
		    (sizeof(ucPwrIdxLen)/sizeof(uint8_t)))
			return i4BytesWritten;

		ucBandIdx = prTxPowerInfo->ucBandIdx;
		ucFormat = prTxPowerInfo->u1Format;

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%s",
					"\n====== TX POWER INFO ======\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"DBDC Index: %d, Channel Band: %s\n",
					ucBandIdx,
					(prTxPowerInfo->ucChBand) ?
						("5G") : ("2G"));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "===========================\n");

		rRatePowerInfo =
			prTxPowerInfo->rRatePowerInfo;


		for (ucTxPwrType = 0;
		     ucTxPwrType < sizeof(POWER_TYPE_STR)/sizeof(uint8_t *);
		     ucTxPwrType++) {

			pucStr = POWER_TYPE_STR[ucTxPwrType];

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"[%6s] > ", pucStr);

			for (ucTxPwrIdx = 0;
			     ucTxPwrIdx < ucPwrIdxLen[ucTxPwrType];
			     ucTxPwrIdx++) {
				/*Legcay format*/
				if (kalStrCmp(pucStr, "CCK") == 0)
					rateStr = cckRateStr[ucTxPwrIdx];
				else if (kalStrCmp(pucStr, "OFDM") == 0)
					rateStr = ofdmRateStr[ucTxPwrIdx];
				else if (kalStrCmp(pucStr, "HT20") == 0)
					rateStr = ht20RateStr[ucTxPwrIdx];
				else if (kalStrCmp(pucStr, "HT40") == 0)
					rateStr = ht40RateStr[ucTxPwrIdx];
				else if (kalStrnCmp(pucStr, "VHT", 3) == 0)
					rateStr = vhtRateStr[ucTxPwrIdx];
				/*HE format*/
				else if ((kalStrnCmp(pucStr, "RU", 2) == 0)
					&& (ucFormat == TXPOWER_FORMAT_HE))
					rateStr = ruRateStr[ucTxPwrIdx];
				else
					continue;

				ucIdx = ucTxPwrIdx + ucIdxOffset;

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%3s:%03d, ",
					rateStr,
					rRatePowerInfo.
					aicFramePowerConfig[ucIdx][ucBandIdx].
					icFramePowerDbm);
			}
			 i4BytesWritten += kalScnprintf(
				 pcCommand + i4BytesWritten,
				 i4TotalLen - i4BytesWritten,
				 "\n");

			ucIdxOffset += ucPwrIdxLen[ucTxPwrType];
		}

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"[MAX][Bound]: 0x%02x (%03d)\n",
				prTxPowerInfo->icPwrMaxBnd,
				prTxPowerInfo->icPwrMaxBnd);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"[MIN][Bound]: 0x%02x (%03d)\n",
				prTxPowerInfo->icPwrMinBnd,
				prTxPowerInfo->icPwrMinBnd);
	}

	return i4BytesWritten;
}

static int32_t priv_driver_get_txpower_info(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0, u4Size = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int8_t *this_char = NULL;
	uint32_t u4ParamNum = 0;
	uint8_t ucParam = 0;
	uint8_t ucBandIdx = 0;
	struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T *prTxPowerInfo = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %d, apcArgv[0] = %s\n\n", i4Argc, *apcArgv);

	this_char = kalStrStr(*apcArgv, "=");
	if (!this_char)
		return -1;
	this_char++;

	DBGLOG(REQ, INFO, "string = %s\n", this_char);

	u4ParamNum = sscanf(this_char, "%d:%d", &ucParam, &ucBandIdx);
	if (u4ParamNum < 0)
		return -1;
	DBGLOG(REQ, INFO, "ParamNum=%d,Param=%d,Band=%d\n",
		u4ParamNum, ucParam, ucBandIdx);

	u4Size = sizeof(struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T);

	prTxPowerInfo =
		(struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T *)kalMemAlloc(
							u4Size, VIR_MEM_TYPE);
	if (!prTxPowerInfo)
		return -1;

	if (u4ParamNum == TXPOWER_INFO_INPUT_ARG_NUM) {
		prTxPowerInfo->ucTxPowerCategory = ucParam;
		prTxPowerInfo->ucBandIdx = ucBandIdx;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryTxPowerInfo,
			prTxPowerInfo,
			sizeof(struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T),
			TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			kalMemFree(prTxPowerInfo, VIR_MEM_TYPE,
			    sizeof(struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T));
			return -1;
		}

		i4BytesWritten = priv_driver_dump_txpower_info(prAdapter,
					pcCommand, i4TotalLen, prTxPowerInfo);
	} else {
		DBGLOG(INIT, ERROR,
		       "[Error]iwpriv wlanXX driver TxPowerInfo=[Param]:[BandIdx]\n");
	}

	kalMemFree(prTxPowerInfo, VIR_MEM_TYPE,
		   sizeof(struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T));

	return i4BytesWritten;
}
#endif
static int32_t priv_driver_txpower_man_set(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0, u4Size = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int8_t *this_char = NULL;
	uint32_t u4ParamNum = 0;
	uint8_t ucPhyMode = 0;
	uint8_t ucTxRate = 0;
	uint8_t ucBw = 0;
	int8_t iTargetPwr = 0;
	struct PARAM_TXPOWER_BY_RATE_SET_T *prPwrCtl = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %d, apcArgv[0] = %s\n\n", i4Argc, *apcArgv);

	this_char = kalStrStr(*apcArgv, "=");
	if (!this_char)
		return -1;
	this_char++;

	DBGLOG(REQ, INFO, "string = %s\n", this_char);

	u4ParamNum = sscanf(this_char, "%d:%d:%d:%d", &ucPhyMode, &ucTxRate,
		&ucBw, &iTargetPwr);

	if (u4ParamNum < 0) {
		DBGLOG(REQ, WARN, "sscanf input fail\n");
		return -1;
	}

	DBGLOG(REQ, INFO, "ParamNum=%d,PhyMod=%d,Rate=%d,Bw=%d,Pwr=%d\n",
		u4ParamNum, ucPhyMode, ucTxRate, ucBw, iTargetPwr);

	u4Size = sizeof(struct PARAM_TXPOWER_BY_RATE_SET_T);

	prPwrCtl =
		(struct PARAM_TXPOWER_BY_RATE_SET_T *)kalMemAlloc(
							u4Size, VIR_MEM_TYPE);
	if (!prPwrCtl)
		return -1;

	if (u4ParamNum == TXPOWER_MAN_SET_INPUT_ARG_NUM) {
		prPwrCtl->u1PhyMode = ucPhyMode;
		prPwrCtl->u1TxRate = ucTxRate;
		prPwrCtl->u1BW = ucBw;
		prPwrCtl->i1TxPower = iTargetPwr;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetTxPowerByRateManual,
			prPwrCtl,
			sizeof(struct PARAM_TXPOWER_BY_RATE_SET_T),
			FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			kalMemFree(prPwrCtl, VIR_MEM_TYPE,
			    sizeof(struct PARAM_TXPOWER_BY_RATE_SET_T));
			return -1;
		}

	} else {
		DBGLOG(INIT, ERROR,
			"[Error]iwpriv wlanXX driver TxPwrManualSet=PhyMode:Rate:Bw:Pwr\n");
	}

	kalMemFree(prPwrCtl, VIR_MEM_TYPE,
		   sizeof(struct PARAM_TXPOWER_BY_RATE_SET_T));

	return i4BytesWritten;
}

static int priv_driver_get_sta_stat(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0, u4Ret, u4StatGroup = 0xFFFFFFFF;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t ucWlanIndex = 0;
	uint8_t *pucMacAddr = NULL;
	struct PARAM_HW_WLAN_INFO *prHwWlanInfo = NULL;
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics = NULL;
	u_int8_t fgResetCnt = FALSE;
	u_int8_t fgRxCCSel = FALSE;
	u_int8_t fgSearchMacAddr = FALSE;
	struct BSS_INFO *prAisBssInfo = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	prAisBssInfo = aisGetAisBssInfo(
		prGlueInfo->prAdapter, wlanGetBssIdx(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 4) {
		if (strnicmp(apcArgv[2], CMD_STAT_GROUP_SEL,
		    strlen(CMD_STAT_GROUP_SEL)) == 0) {
			u4Ret = kalkStrtou32(apcArgv[3], 0, &(u4StatGroup));

			if (u4Ret)
				DBGLOG(REQ, LOUD,
				       "parse get_sta_stat error (Group) u4Ret=%d\n",
				       u4Ret);
			if (u4StatGroup == 0)
				u4StatGroup = 0xFFFFFFFF;

			wlanHwAddrToBin(apcArgv[1], &aucMacAddr[0]);

			if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter,
			    &aucMacAddr[0], &ucWlanIndex)) {
				DBGLOG(REQ, INFO,
				      "wlan index of "MACSTR" is not found!\n",
				      MAC2STR(aucMacAddr));
				goto out;
			}
		} else {
			goto out;
		}
	} else if (i4Argc >= 3) {
		if (strnicmp(apcArgv[1], CMD_STAT_GROUP_SEL,
		    strlen(CMD_STAT_GROUP_SEL)) == 0) {
			u4Ret = kalkStrtou32(apcArgv[2], 0, &(u4StatGroup));

			if (u4Ret)
				DBGLOG(REQ, LOUD,
				       "parse get_sta_stat error (Group) u4Ret=%d\n",
				       u4Ret);
			if (u4StatGroup == 0)
				u4StatGroup = 0xFFFFFFFF;

			if (prAisBssInfo->prStaRecOfAP) {
				ucWlanIndex = prAisBssInfo->prStaRecOfAP
						->ucWlanIndex;
			} else if (!wlanGetWlanIdxByAddress(
				prGlueInfo->prAdapter, NULL,
				&ucWlanIndex)) {
				DBGLOG(REQ, INFO,
				      "wlan index of "MACSTR" is not found!\n",
				      MAC2STR(aucMacAddr));
				goto out;
			}
		} else {
			if (strnicmp(apcArgv[1], CMD_STAT_RESET_CNT,
			    strlen(CMD_STAT_RESET_CNT)) == 0) {
				wlanHwAddrToBin(apcArgv[2], &aucMacAddr[0]);
				fgResetCnt = TRUE;
			} else if (strnicmp(apcArgv[2], CMD_STAT_RESET_CNT,
			    strlen(CMD_STAT_RESET_CNT)) == 0) {
				wlanHwAddrToBin(apcArgv[1], &aucMacAddr[0]);
				fgResetCnt = TRUE;
			} else {
				wlanHwAddrToBin(apcArgv[1], &aucMacAddr[0]);
				fgResetCnt = FALSE;
			}

			if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter,
			    &aucMacAddr[0], &ucWlanIndex)) {
				DBGLOG(REQ, INFO,
				      "wlan index of "MACSTR" is not found!\n",
				      MAC2STR(aucMacAddr));
				goto out;
			}
		}
	} else {
		if (i4Argc == 1) {
			fgSearchMacAddr = TRUE;
		} else if (i4Argc == 2) {
			if (strnicmp(apcArgv[1], CMD_STAT_RESET_CNT,
			    strlen(CMD_STAT_RESET_CNT)) == 0) {
				fgResetCnt = TRUE;
				fgSearchMacAddr = TRUE;
			} else if (strnicmp(apcArgv[1], CMD_STAT_NOISE_SEL,
			    strlen(CMD_STAT_NOISE_SEL)) == 0) {
				fgRxCCSel = TRUE;
				fgSearchMacAddr = TRUE;
			} else {
				wlanHwAddrToBin(apcArgv[1], &aucMacAddr[0]);

				if (!wlanGetWlanIdxByAddress(prGlueInfo->
				    prAdapter, &aucMacAddr[0], &ucWlanIndex)) {
					DBGLOG(REQ, INFO,
						"No connected peer found!\n");
					goto out;
				}
			}
		}

		if (fgSearchMacAddr) {
			/* Get AIS AP address for no argument */
			if (prAisBssInfo->prStaRecOfAP) {
				ucWlanIndex = prAisBssInfo->prStaRecOfAP
					->ucWlanIndex;
			} else if (!wlanGetWlanIdxByAddress(prGlueInfo->
				prAdapter, NULL, &ucWlanIndex)) {
				DBGLOG(REQ, INFO, "No connected peer found!\n");
				goto out;
			}
		}
	}

	prHwWlanInfo = (struct PARAM_HW_WLAN_INFO *)kalMemAlloc(
			sizeof(struct PARAM_HW_WLAN_INFO), VIR_MEM_TYPE);
	if (!prHwWlanInfo) {
		DBGLOG(REQ, ERROR,
			"Allocate prHwWlanInfo failed!\n");
		i4BytesWritten = -1;
		goto out;
	}

	prHwWlanInfo->u4Index = ucWlanIndex;
	if (fgRxCCSel == TRUE)
		prHwWlanInfo->rWtblRxCounter.fgRxCCSel = TRUE;
	else
		prHwWlanInfo->rWtblRxCounter.fgRxCCSel = FALSE;

	DBGLOG(REQ, INFO, "index = %d i4TotalLen = %d\n",
	       prHwWlanInfo->u4Index, i4TotalLen);

	/* Get WTBL info */
	rStatus = kalIoctl(prGlueInfo, wlanoidQueryWlanInfo, prHwWlanInfo,
			   sizeof(struct PARAM_HW_WLAN_INFO),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Query prHwWlanInfo failed!\n");
		i4BytesWritten = -1;
		goto out;
	}

	/* Get Statistics info */
	prQueryStaStatistics =
		(struct PARAM_GET_STA_STATISTICS *)kalMemAlloc(
			sizeof(struct PARAM_GET_STA_STATISTICS), VIR_MEM_TYPE);
	if (!prQueryStaStatistics) {
		DBGLOG(REQ, ERROR,
			"Allocate prQueryStaStatistics failed!\n");
		i4BytesWritten = -1;
		goto out;
	}

	prQueryStaStatistics->ucResetCounter = fgResetCnt;

	pucMacAddr = wlanGetStaAddrByWlanIdx(prGlueInfo->prAdapter,
					     ucWlanIndex);

	if (!pucMacAddr) {
		DBGLOG(REQ, ERROR, "Addr of WlanIndex %d is not found!\n",
			ucWlanIndex);
		i4BytesWritten = -1;
		goto out;
	}
	COPY_MAC_ADDR(prQueryStaStatistics->aucMacAddr, pucMacAddr);

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryStaStatistics,
			   prQueryStaStatistics,
			   sizeof(struct PARAM_GET_STA_STATISTICS),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Query prQueryStaStatistics failed!\n");
		i4BytesWritten = -1;
		goto out;
	}

	if (pucMacAddr) {
		struct CHIP_DBG_OPS *prChipDbg;

		prChipDbg = prAdapter->chip_info->prDebugOps;

		if (prChipDbg && prChipDbg->show_stat_info)
			i4BytesWritten = prChipDbg->show_stat_info(prAdapter,
				pcCommand, i4TotalLen, prHwWlanInfo,
				prQueryStaStatistics, fgResetCnt, u4StatGroup);
	}
	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

out:
	if (prHwWlanInfo)
		kalMemFree(prHwWlanInfo, VIR_MEM_TYPE,
			sizeof(struct PARAM_HW_WLAN_INFO));

	if (prQueryStaStatistics)
		kalMemFree(prQueryStaStatistics, VIR_MEM_TYPE,
			sizeof(struct PARAM_GET_STA_STATISTICS));


	if (fgResetCnt)
		nicRxClearStatistics(prGlueInfo->prAdapter);

	return i4BytesWritten;
}

static int32_t priv_driver_dump_stat2_info(struct ADAPTER *prAdapter,
			IN char *pcCommand, IN int i4TotalLen,
			struct UMAC_STAT2_GET *prUmacStat2GetInfo,
			struct PARAM_GET_DRV_STATISTICS *prQueryDrvStatistics)
{
	int32_t i4BytesWritten = 0;
	uint16_t u2PleTotalRevPage = 0;
	uint16_t u2PleTotalSrcPage = 0;
	uint16_t u2PseTotalRevPage = 0;
	uint16_t u2PseTotalSrcPage = 0;

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

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n----- Stat2 Info -----\n");


	/* Rev Page number Info. */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n----- PLE Reservation Page Info. -----\n");

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Ple Hif0 Group0 RevPage", " = ",
			prUmacStat2GetInfo->u2PleRevPgHif0Group0);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Ple Cpu Group2 RevPage", " = ",
			prUmacStat2GetInfo->u2PleRevPgCpuGroup2);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Ple Total RevPage", " = ",
			u2PleTotalRevPage);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n----- PLE Source Page Info. ----------\n");

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Ple Hif0 Group0 SrcPage", " = ",
			prUmacStat2GetInfo->u2PleSrvPgHif0Group0);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Ple Cpu Group2 SrcPage", " = ",
			prUmacStat2GetInfo->u2PleSrvPgCpuGroup2);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Ple Total SrcPage", " = ",
			u2PleTotalSrcPage);

	/* umac MISC Info. */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n----- PLE Misc Info. -----------------\n");

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "ple Total Page Number", " = ",
			prUmacStat2GetInfo->u2PleTotalPageNum);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "ple Free Page Number", " = ",
			prUmacStat2GetInfo->u2PleFreePageNum);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "ple FFA Page Number", " = ",
			prUmacStat2GetInfo->u2PleFfaNum);

	/* PSE Info. */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n----- PSE Reservation Page Info. -----\n");

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Hif0 Group0 RevPage", " = ",
			prUmacStat2GetInfo->u2PseRevPgHif0Group0);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Hif1 Group1 RevPage", " = ",
			prUmacStat2GetInfo->u2PseRevPgHif1Group1);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Cpu Group2 RevPage", " = ",
			prUmacStat2GetInfo->u2PseRevPgCpuGroup2);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Lmac0 Group3 RevPage", " = ",
			prUmacStat2GetInfo->u2PseRevPgLmac0Group3);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Lmac1 Group4 RevPage", " = ",
			prUmacStat2GetInfo->u2PseRevPgLmac1Group4);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Lmac2 Group5 RevPage", " = ",
			prUmacStat2GetInfo->u2PseRevPgLmac2Group5);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Ple Group6 RevPage", " = ",
			prUmacStat2GetInfo->u2PseRevPgPleGroup6);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Total RevPage", " = ",
			u2PseTotalRevPage);

	/* PSE Info. */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n----- PSE Source Page Info. ----------\n");

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Hif0 Group0 SrcPage", " = ",
			prUmacStat2GetInfo->u2PseSrvPgHif0Group0);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Hif1 Group1 SrcPage", " = ",
			prUmacStat2GetInfo->u2PseSrvPgHif1Group1);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Cpu Group2 SrcPage", " = ",
			prUmacStat2GetInfo->u2PseSrvPgCpuGroup2);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Lmac0 Group3 SrcPage", " = ",
			prUmacStat2GetInfo->u2PseSrvPgLmac0Group3);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Lmac1 Group4 SrcPage", " = ",
			prUmacStat2GetInfo->u2PseSrvPgLmac1Group4);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Lmac2 Group5 SrcPage", " = ",
			prUmacStat2GetInfo->u2PseSrvPgLmac2Group5);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Ple Group6 SrcPage", " = ",
			prUmacStat2GetInfo->u2PseSrvPgPleGroup6);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pse Total SrcPage", " = ",
			u2PseTotalSrcPage);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n----- PSE Misc Info. -----------------\n");

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "pse Total Page Number", " = ",
			prUmacStat2GetInfo->u2PseTotalPageNum);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "pse Free Page Number", " = ",
			prUmacStat2GetInfo->u2PseFreePageNum);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "pse FFA Page Number", " = ",
			prUmacStat2GetInfo->u2PseFfaNum);


	/* driver info */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n\n----- DRV Stat -----------------------\n\n");

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pending Data", " = ",
			prQueryDrvStatistics->i4TxPendingFrameNum);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Pending Sec", " = ",
			prQueryDrvStatistics->i4TxPendingSecurityFrameNum);
#if 0
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%s\n", "Tx Pending Cmd Number", " = ",
			prQueryDrvStatistics->i4TxPendingCmdNum);
#endif

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Tx Pending For-pkt Number", " = ",
			prQueryDrvStatistics->i4PendingFwdFrameCount);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "MsduInfo Available Number", " = ",
			prQueryDrvStatistics->u4MsduNumElem);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "MgmtTxRing Pending Number", " = ",
			prQueryDrvStatistics->u4TxMgmtTxringQueueNumElem);

	/* Driver Rx Info. */
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Rx Free Sw Rfb Number", " = ",
			prQueryDrvStatistics->u4RxFreeSwRfbMsduNumElem);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Rx Received Sw Rfb Number", " = ",
			prQueryDrvStatistics->u4RxReceivedRfbNumElem);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-26s%s%d\n", "Rx Indicated Sw Rfb Number", " = ",
			prQueryDrvStatistics->u4RxIndicatedNumElem);

	return i4BytesWritten;
}

static int priv_driver_get_sta_stat2(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4BytesWritten = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4ArgNum = 1;
	struct UMAC_STAT2_GET *prUmacStat2GetInfo;
	struct PARAM_GET_DRV_STATISTICS *prQueryDrvStatistics;
	struct QUE *prQueList, *prTxMgmtTxRingQueList;
	struct RX_CTRL *prRxCtrl;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
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
	prUmacStat2GetInfo = (struct UMAC_STAT2_GET *)kalMemAlloc(
				sizeof(struct UMAC_STAT2_GET), VIR_MEM_TYPE);
	if (prUmacStat2GetInfo == NULL)
		return -1;

	halUmacInfoGetMiscStatus(prAdapter, prUmacStat2GetInfo);


	/* Get Driver stat info */
	prQueryDrvStatistics =
		(struct PARAM_GET_DRV_STATISTICS *)kalMemAlloc(sizeof(
				struct PARAM_GET_DRV_STATISTICS), VIR_MEM_TYPE);
	if (prQueryDrvStatistics == NULL) {
		kalMemFree(prUmacStat2GetInfo, VIR_MEM_TYPE,
			   sizeof(struct UMAC_STAT2_GET));
		return -1;
	}

	prQueryDrvStatistics->i4TxPendingFrameNum =
		(uint32_t) GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingFrameNum);
	prQueryDrvStatistics->i4TxPendingSecurityFrameNum =
		(uint32_t) GLUE_GET_REF_CNT(
				prGlueInfo->i4TxPendingSecurityFrameNum);
#if 0
	prQueryDrvStatistics->i4TxPendingCmdNum =
		(uint32_t) GLUE_GET_REF_CNT(prGlueInfo->i4TxPendingCmdNum);
#endif

	prQueryDrvStatistics->i4PendingFwdFrameCount =
				prAdapter->rTxCtrl.i4PendingFwdFrameCount;

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);
	prQueryDrvStatistics->u4MsduNumElem = prQueList->u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_MSDU_INFO_LIST);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);
	prQueryDrvStatistics->u4TxMgmtTxringQueueNumElem =
					prTxMgmtTxRingQueList->u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TXING_MGMT_LIST);


	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	prQueryDrvStatistics->u4RxFreeSwRfbMsduNumElem =
					prRxCtrl->rFreeSwRfbList.u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);
	prQueryDrvStatistics->u4RxReceivedRfbNumElem =
					prRxCtrl->rReceivedRfbList.u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_QUE);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	prQueryDrvStatistics->u4RxIndicatedNumElem =
					prRxCtrl->rIndicatedRfbList.u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

	i4BytesWritten = priv_driver_dump_stat2_info(prAdapter, pcCommand,
			i4TotalLen, prUmacStat2GetInfo, prQueryDrvStatistics);

	kalMemFree(prUmacStat2GetInfo, VIR_MEM_TYPE,
		   sizeof(struct UMAC_STAT2_GET));
	kalMemFree(prQueryDrvStatistics, VIR_MEM_TYPE,
		   sizeof(struct PARAM_GET_DRV_STATISTICS));

	return i4BytesWritten;
}


static int32_t priv_driver_dump_rx_stat_info(struct ADAPTER *prAdapter,
					IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen,
					IN u_int8_t fgResetCnt)
{
	int32_t i4BytesWritten = 0;
	uint32_t u4RxVector0 = 0, u4RxVector2 = 0, u4RxVector3 = 0,
		 u4RxVector4 = 0;
	uint8_t ucStaIdx, ucWlanIndex = 0, cbw;
	u_int8_t fgWlanIdxFound = TRUE, fgSkipRxV = FALSE;
	uint32_t u4FAGCRssiWBR0, u4FAGCRssiIBR0;
	uint32_t u4Value, u4Foe, foe_const;
	static uint32_t au4MacMdrdy[ENUM_BAND_NUM] = {0};
	static uint32_t au4FcsError[ENUM_BAND_NUM] = {0};
	static uint32_t au4OutOfResource[ENUM_BAND_NUM] = {0};
	static uint32_t au4LengthMismatch[ENUM_BAND_NUM] = {0};
	struct STA_RECORD *prStaRecOfAP;

	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	prStaRecOfAP =
		aisGetStaRecOfAP(prAdapter, ucBssIndex);

	au4MacMdrdy[ENUM_BAND_0] += htonl(g_HqaRxStat.MAC_Mdrdy);
	au4MacMdrdy[ENUM_BAND_1] += htonl(g_HqaRxStat.MAC_Mdrdy1);
	au4FcsError[ENUM_BAND_0] += htonl(g_HqaRxStat.MAC_FCS_Err);
	au4FcsError[ENUM_BAND_1] += htonl(g_HqaRxStat.MAC_FCS_Err1);
	au4OutOfResource[ENUM_BAND_0] += htonl(g_HqaRxStat.OutOfResource);
	au4OutOfResource[ENUM_BAND_1] += htonl(g_HqaRxStat.OutOfResource1);
	au4LengthMismatch[ENUM_BAND_0] += htonl(
					g_HqaRxStat.LengthMismatchCount_B0);
	au4LengthMismatch[ENUM_BAND_1] += htonl(
					g_HqaRxStat.LengthMismatchCount_B1);

	DBGLOG(INIT, INFO, "fgResetCnt = %d\n", fgResetCnt);
	if (fgResetCnt) {
		kalMemZero(au4MacMdrdy, sizeof(au4MacMdrdy));
		kalMemZero(au4FcsError, sizeof(au4FcsError));
		kalMemZero(au4OutOfResource, sizeof(au4OutOfResource));
		kalMemZero(au4LengthMismatch, sizeof(au4LengthMismatch));
	}

	if (prStaRecOfAP)
		ucWlanIndex =
			prStaRecOfAP->ucWlanIndex;
	else if (!wlanGetWlanIdxByAddress(prAdapter, NULL, &ucWlanIndex))
		fgWlanIdxFound = FALSE;

	if (fgWlanIdxFound) {
		if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIndex, &ucStaIdx)
		    == WLAN_STATUS_SUCCESS) {
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
	DBGLOG(INIT, INFO, "g_HqaRxStat.MAC_Mdrdy = %d\n",
		htonl(g_HqaRxStat.MAC_Mdrdy));
	DBGLOG(INIT, INFO, "au4MacMdrdy[ENUM_BAND_0] = %d\n",
		au4MacMdrdy[ENUM_BAND_0]);
	DBGLOG(INIT, INFO, "g_HqaRxStat.PhyMdrdyOFDM = %d\n",
		htonl(g_HqaRxStat.PhyMdrdyOFDM));
	DBGLOG(INIT, INFO, "g_HqaRxStat.MAC_FCS_Err= %d\n",
		htonl(g_HqaRxStat.MAC_FCS_Err));
	DBGLOG(INIT, INFO, "au4FcsError[ENUM_BAND_0]= %d\n",
		au4FcsError[ENUM_BAND_0]);
	DBGLOG(INIT, INFO, "g_HqaRxStat.FCSErr_OFDM = %d\n",
		htonl(g_HqaRxStat.FCSErr_OFDM));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n\nRX Stat:\n");
#if 1
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "PER0", " = ",
			htonl(g_HqaRxStat.PER0));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "PER1", " = ",
			htonl(g_HqaRxStat.PER1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RX OK0", " = ",
			au4MacMdrdy[ENUM_BAND_0] - au4FcsError[ENUM_BAND_0]);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RX OK1", " = ",
			au4MacMdrdy[ENUM_BAND_1] - au4FcsError[ENUM_BAND_1]);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RCPI0", " = ",
			htonl(g_HqaRxStat.RCPI0));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RCPI1", " = ",
			htonl(g_HqaRxStat.RCPI1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "MuRxCnt", " = ",
			htonl(g_HqaRxStat.MRURxCount));
#endif
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "MAC Mdrdy0 diff", " = ",
			htonl(g_HqaRxStat.MAC_Mdrdy));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "MAC Mdrdy0", " = ",
			au4MacMdrdy[ENUM_BAND_0]);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "MAC Mdrdy1 diff", " = ",
			htonl(g_HqaRxStat.MAC_Mdrdy1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "MAC Mdrdy1", " = ",
			au4MacMdrdy[ENUM_BAND_1]);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "FCS Err0", " = ",
			au4FcsError[ENUM_BAND_0]);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "FCS Err1", " = ",
			au4FcsError[ENUM_BAND_1]);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK PD Cnt B0", " = ",
			htonl(g_HqaRxStat.CCK_PD));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK PD Cnt B1", " = ",
			htonl(g_HqaRxStat.CCK_PD_Band1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK SIG Err B0", " = ",
			htonl(g_HqaRxStat.CCK_SIG_Err));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK SIG Err B1", " = ",
			htonl(g_HqaRxStat.CCK_SIG_Err_Band1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
	i4TotalLen - i4BytesWritten,
		"%-20s%s%d\n", "OFDM PD Cnt B0", " = ",
		htonl(g_HqaRxStat.OFDM_PD));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "OFDM PD Cnt B1", " = ",
			htonl(g_HqaRxStat.OFDM_PD_Band1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "OFDM TAG Error", " = ",
			htonl(g_HqaRxStat.OFDM_TAG_Err));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK SFD Err B0", " = ",
			htonl(g_HqaRxStat.CCK_SFD_Err));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK SFD Err B1", " = ",
			htonl(g_HqaRxStat.CCK_SFD_Err_Band1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "OFDM SIG Err B0", " = ",
			htonl(g_HqaRxStat.OFDM_SIG_Err));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "OFDM SIG Err B1", " = ",
			htonl(g_HqaRxStat.OFDM_SIG_Err_Band1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK FCS Err B0", " = ",
			htonl(g_HqaRxStat.FCSErr_CCK));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK FCS Err B1", " = ",
			htonl(g_HqaRxStat.CCK_FCS_Err_Band1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "OFDM FCS Err B0", " = ",
			htonl(g_HqaRxStat.FCSErr_OFDM));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "OFDM FCS Err B1", " = ",
			htonl(g_HqaRxStat.OFDM_FCS_Err_Band1));

	if (!fgSkipRxV) {
		u4FAGCRssiIBR0 = (u4RxVector2 & BITS(16, 23)) >> 16;
		u4FAGCRssiWBR0 = (u4RxVector2 & BITS(24, 31)) >> 24;

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "FAGC RSSI W", " = ",
			(u4FAGCRssiWBR0 >= 128) ? (u4FAGCRssiWBR0 - 256) :
				(u4FAGCRssiWBR0));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "FAGC RSSI I", " = ",
			(u4FAGCRssiIBR0 >= 128) ? (u4FAGCRssiIBR0 - 256) :
				(u4FAGCRssiIBR0));
	} else{
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "FAGC RSSI W", " = ",
			htonl(g_HqaRxStat.FAGCRssiWBR0));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "FAGC RSSI I", " = ",
			htonl(g_HqaRxStat.FAGCRssiIBR0));
	}

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK MDRDY B0", " = ",
			htonl(g_HqaRxStat.PhyMdrdyCCK));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CCK MDRDY B1", " = ",
			htonl(g_HqaRxStat.PHY_CCK_MDRDY_Band1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "OFDM MDRDY B0", " = ",
			htonl(g_HqaRxStat.PhyMdrdyOFDM));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "OFDM MDRDY B1", " = ",
			htonl(g_HqaRxStat.PHY_OFDM_MDRDY_Band1));

	if (!fgSkipRxV) {
#if 0
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Driver RX Cnt0", " = ",
			htonl(g_HqaRxStat.DriverRxCount));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
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

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Freq Offset From RX", " = ", u4Foe);

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RX SNR (dB)", " = ",
			(uint32_t)(((u4RxVector4 & BITS(26, 31)) >> 26) - 16));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "uint8_t RX0", " = ",
			(uint32_t)(u4RxVector3 & BITS(0, 7)));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "uint8_t RX1", " = ",
			(uint32_t)((u4RxVector3 & BITS(8, 15)) >> 8));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "uint8_t RX2", " = ",
			((u4RxVector3 & BITS(16, 23)) >> 16) == 0xFF ?
			(0) : ((uint32_t)(u4RxVector3 & BITS(16, 23)) >> 16));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "uint8_t RX3", " = ",
			((u4RxVector3 & BITS(24, 31)) >> 24) == 0xFF ?
			(0) : ((uint32_t)(u4RxVector3 & BITS(24, 31)) >> 24));
	} else{
#if 1
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%d\n", "Driver RX Cnt0",
					      " = ",
					      htonl(g_HqaRxStat.DriverRxCount));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%d\n", "Driver RX Cnt1",
					      " = ",
					     htonl(g_HqaRxStat.DriverRxCount1));
#endif
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%d\n",
					      "Freq Offset From RX",
					      " = ",
					   htonl(g_HqaRxStat.FreqOffsetFromRX));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%s\n", "RX SNR (dB)",
					      " = ", "N/A");

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%s\n", "uint8_t RX0",
					      " = ", "N/A");

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%s\n", "uint8_t RX1",
					      " = ", "N/A");

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%s\n", "uint8_t RX2",
					      " = ", "N/A");

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%s\n", "uint8_t RX3",
					      " = ", "N/A");
	}

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Inst RSSI IB R0", " = ",
				      htonl(g_HqaRxStat.InstRssiIBR0));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Inst RSSI WB R0", " = ",
				      htonl(g_HqaRxStat.InstRssiWBR0));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Inst RSSI IB R1", " = ",
				      htonl(g_HqaRxStat.InstRssiIBR1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Inst RSSI WB R1", " = ",
				      htonl(g_HqaRxStat.InstRssiWBR1));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Inst RSSI IB R2", " = ",
				      htonl(g_HqaRxStat.InstRssiIBR2));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Inst RSSI WB R2", " = ",
				      htonl(g_HqaRxStat.InstRssiWBR2));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Inst RSSI IB R3", " = ",
				      htonl(g_HqaRxStat.InstRssiIBR3));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Inst RSSI WB R3", " = ",
				      htonl(g_HqaRxStat.InstRssiWBR3));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "ACI Hit Lower", " = ",
				      htonl(g_HqaRxStat.ACIHitLower));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "ACI Hit Higher",
				      " = ", htonl(g_HqaRxStat.ACIHitUpper));

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "OutOf Resource Pkt0",
				      " = ", au4OutOfResource[ENUM_BAND_0]);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "OutOf Resource Pkt1",
				      " = ", au4OutOfResource[ENUM_BAND_1]);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Len Mismatch Cnt B0",
				      " = ", au4LengthMismatch[ENUM_BAND_0]);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "Len Mismatch Cnt B1",
				      " = ", au4LengthMismatch[ENUM_BAND_1]);
#if 0
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "%-20s%s%d\n", "MU RX Cnt", " = ",
		htonl(g_HqaRxStat.MRURxCount));
#endif
	return i4BytesWritten;
}


static int priv_driver_show_rx_stat(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct PARAM_CUSTOM_ACCESS_RX_STAT *prRxStatisticsTest;
	u_int8_t fgResetCnt = FALSE;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint32_t u4Id = 0x99980000;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	DBGLOG(INIT, ERROR,
		"MT6632 : priv_driver_show_rx_stat %s, i4Argc[%d]\n",
		pcCommand, i4Argc);

	if (i4Argc >= 2) {
		if (strnicmp(apcArgv[1], CMD_STAT_RESET_CNT,
		    strlen(CMD_STAT_RESET_CNT)) == 0)
			fgResetCnt = TRUE;
	}

	if (i4Argc >= 1) {
		if (fgResetCnt) {
			rSwCtrlInfo.u4Id = u4Id;
			rSwCtrlInfo.u4Data = 0;

			rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite,
					   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
					   FALSE, FALSE, TRUE, &u4BufLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				return -1;
		}

		prRxStatisticsTest =
			(struct PARAM_CUSTOM_ACCESS_RX_STAT *)kalMemAlloc(
			sizeof(struct PARAM_CUSTOM_ACCESS_RX_STAT),
			VIR_MEM_TYPE);
		if (!prRxStatisticsTest)
			return -1;

		prRxStatisticsTest->u4SeqNum = u4RxStatSeqNum;
		prRxStatisticsTest->u4TotalNum =
					sizeof(struct PARAM_RX_STAT) / 4;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryRxStatistics,
				   prRxStatisticsTest,
				   sizeof(struct PARAM_CUSTOM_ACCESS_RX_STAT),
				   TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			kalMemFree(prRxStatisticsTest, VIR_MEM_TYPE,
				   sizeof(struct PARAM_CUSTOM_ACCESS_RX_STAT));
			return -1;
		}

		i4BytesWritten = priv_driver_dump_rx_stat_info(prAdapter,
			prNetDev, pcCommand, i4TotalLen, fgResetCnt);

		kalMemFree(prRxStatisticsTest, VIR_MEM_TYPE,
			   sizeof(struct PARAM_CUSTOM_ACCESS_RX_STAT));
	}

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
static int priv_driver_set_acl_policy(IN struct net_device *prNetDev,
				      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0, i4BytesWritten = 0, i4Ret = 0, i4Policy = 0;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	DBGLOG(REQ, LOUD, "ucRoleIdx %hhu ucBssIdx %hhu\n", ucRoleIdx,
	       ucBssIdx);
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

	DBGLOG(REQ, TRACE, "ucBssIdx[%hhu] ACL Policy=%d\n", ucBssIdx,
	       prBssInfo->rACL.ePolicy);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "ucBssIdx[%hhu] ACL Policy=%d\n",
				      ucBssIdx, prBssInfo->rACL.ePolicy);

	/* check if the change in ACL affects any existent association */
	if (prBssInfo->rACL.ePolicy != PARAM_CUSTOM_ACL_POLICY_DISABLE)
		p2pRoleUpdateACLEntry(prAdapter, ucBssIdx);

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	return i4BytesWritten;
} /* priv_driver_set_acl_policy */

static int32_t priv_driver_inspect_mac_addr(IN char *pcMacAddr)
{
	int32_t i = 0;

	if (pcMacAddr == NULL)
		return -1;

	for (i = 0; i < 17; i++) {
		if ((i % 3 != 2) && (!kalIsXdigit(pcMacAddr[i]))) {
			DBGLOG(REQ, ERROR, "[%c] is not hex digit\n",
			       pcMacAddr[i]);
			return -1;
		}
		if ((i % 3 == 2) && (pcMacAddr[i] != ':')) {
			DBGLOG(REQ, ERROR, "[%c]separate symbol is error\n",
			       pcMacAddr[i]);
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
static int priv_driver_add_acl_entry(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t aucMacAddr[MAC_ADDR_LEN] = {0};
	int32_t i = 0, i4Argc = 0, i4BytesWritten = 0, i4Ret = 0;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	DBGLOG(REQ, LOUD, "ucRoleIdx %hhu ucBssIdx %hhu\n", ucRoleIdx,
	       ucBssIdx);
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc < 2)
		return -1;

	i4Ret = priv_driver_inspect_mac_addr(apcArgv[1]);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "inspect mac format error u4Ret=%d\n",
		       i4Ret);
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
		if (memcmp(prBssInfo->rACL.rEntry[i].aucAddr, &aucMacAddr,
		    MAC_ADDR_LEN) == 0) {
			DBGLOG(REQ, ERROR, "add this mac [" MACSTR
			       "] is duplicate.\n", MAC2STR(aucMacAddr));
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

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"add mac addr [" MACSTR "] to ACL(%d)\n",
				MAC2STR(prBssInfo->rACL.rEntry[i-1].aucAddr),
				i);

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
static int priv_driver_del_acl_entry(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t aucMacAddr[MAC_ADDR_LEN] = {0};
	int32_t i = 0, j = 0, i4Argc = 0, i4BytesWritten = 0, i4Ret = 0;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		return -1;

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	DBGLOG(REQ, LOUD, "ucRoleIdx %hhu ucBssIdx %hhu\n", ucRoleIdx,
	       ucBssIdx);
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc < 2)
		return -1;

	i4Ret = priv_driver_inspect_mac_addr(apcArgv[1]);
	if (i4Ret) {
		DBGLOG(REQ, ERROR, "inspect mac format error u4Ret=%d\n",
		       i4Ret);
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
		if (memcmp(prBssInfo->rACL.rEntry[i].aucAddr, &aucMacAddr,
		    MAC_ADDR_LEN) == 0) {
			memset(&prBssInfo->rACL.rEntry[i], 0x00,
			       sizeof(struct PARAM_CUSTOM_ACL_ENTRY));
			DBGLOG(REQ, TRACE, "delete this mac [" MACSTR "]\n",
			       MAC2STR(aucMacAddr));

			i4BytesWritten += kalSnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"delete this mac [" MACSTR "] from ACL(%d)\n",
				MAC2STR(aucMacAddr), i+1);
			break;
		}
	}

	if ((prBssInfo->rACL.u4Num == 0) || (i == MAX_NUMBER_OF_ACL)) {
		DBGLOG(REQ, ERROR, "delete entry fail, num of entries=%d\n", i);
		return -1;
	}

	for (j = i+1; j < prBssInfo->rACL.u4Num; j++)
		memcpy(prBssInfo->rACL.rEntry[j-1].aucAddr,
		       prBssInfo->rACL.rEntry[j].aucAddr, MAC_ADDR_LEN);

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
static int priv_driver_show_acl_entry(IN struct net_device *prNetDev,
				      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i = 0, i4Argc = 0, i4BytesWritten = 0;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
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

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "ACL Policy = %d, Total ACLs = %d\n",
				      prBssInfo->rACL.ePolicy,
				      prBssInfo->rACL.u4Num);

	for (i = 0; i < prBssInfo->rACL.u4Num; i++) {
		DBGLOG(REQ, TRACE, "ACL(%d): [" MACSTR "]\n", i+1,
		       MAC2STR(prBssInfo->rACL.rEntry[i].aucAddr));

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "ACL(%d): [" MACSTR
			"]\n", i+1, MAC2STR(prBssInfo->rACL.rEntry[i].aucAddr));
	}

	return i4BytesWritten;
} /* priv_driver_show_acl_entry */

/*----------------------------------------------------------------------------*/
/*
 * @ The function will clear all entries to ACL for accept or deny list.
 *  example: iwpriv p2p0 driver "clear_acl_entry"
 */
/*----------------------------------------------------------------------------*/
static int priv_driver_clear_acl_entry(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Argc = 0, i4BytesWritten = 0;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
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
		memset(&prBssInfo->rACL.rEntry[0], 0x00,
		       sizeof(struct PARAM_CUSTOM_ACL_ENTRY) * MAC_ADDR_LEN);
		prBssInfo->rACL.u4Num = 0;
	}

	DBGLOG(REQ, TRACE, "ACL Policy = %d\n", prBssInfo->rACL.ePolicy);
	DBGLOG(REQ, TRACE, "Total ACLs = %d\n", prBssInfo->rACL.u4Num);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				      i4TotalLen - i4BytesWritten,
				      "ACL Policy = %d, Total ACLs = %d\n",
				      prBssInfo->rACL.ePolicy,
				      prBssInfo->rACL.u4Num);

	/* check if the change in ACL affects any existent association */
	if (prBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_ACCEPT)
		p2pRoleUpdateACLEntry(prAdapter, ucBssIdx);

	return i4BytesWritten;
} /* priv_driver_clear_acl_entry */

static int priv_driver_get_drv_mcr(IN struct net_device *prNetDev,
				   IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret;

	/* Add Antenna Selection Input */
	/* INT_32 i4ArgNum_with_ant_sel = 3; */

	int32_t i4ArgNum = 2;

	struct CMD_ACCESS_REG rCmdAccessReg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		rCmdAccessReg.u4Address = 0;
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rCmdAccessReg.u4Address));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "parse get_drv_mcr error (Address) u4Ret=%d\n",
			       u4Ret);

		/* rCmdAccessReg.u4Address = kalStrtoul(apcArgv[1], NULL, 0); */
		rCmdAccessReg.u4Data = 0;

		DBGLOG(REQ, LOUD, "address is %x\n", rCmdAccessReg.u4Address);

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryDrvMcrRead,
				   &rCmdAccessReg, sizeof(rCmdAccessReg),
				   TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "0x%08x",
					  (unsigned int)rCmdAccessReg.u4Data);
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__,
		       pcCommand);
	}

	return i4BytesWritten;

}				/* priv_driver_get_drv_mcr */

int priv_driver_set_drv_mcr(IN struct net_device *prNetDev, IN char *pcCommand,
			    IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret;

	/* Add Antenna Selection Input */
	/* INT_32 i4ArgNum_with_ant_sel = 4; */

	int32_t i4ArgNum = 3;

	struct CMD_ACCESS_REG rCmdAccessReg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rCmdAccessReg.u4Address));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "parse get_drv_mcr error (Address) u4Ret=%d\n",
			       u4Ret);

		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rCmdAccessReg.u4Data));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "parse get_drv_mcr error (Data) u4Ret=%d\n",
			       u4Ret);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetDrvMcrWrite,
				   &rCmdAccessReg, sizeof(rCmdAccessReg),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	}

	return i4BytesWritten;

}

static int priv_driver_get_sw_ctrl(IN struct net_device *prNetDev,
				   IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t u4Ret = 0;

	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		rSwCtrlInfo.u4Id = 0;
		rSwCtrlInfo.u4Data = 0;
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rSwCtrlInfo.u4Id));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse rSwCtrlInfo error u4Ret=%d\n",
			       u4Ret);

		DBGLOG(REQ, LOUD, "id is %x\n", rSwCtrlInfo.u4Id);

		rStatus = kalIoctl(prGlueInfo, wlanoidQuerySwCtrlRead,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
				   TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "0x%08x",
					  (unsigned int)rSwCtrlInfo.u4Data);
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__,
		       pcCommand);
	}

	return i4BytesWritten;

}				/* priv_driver_get_sw_ctrl */


int priv_driver_set_sw_ctrl(IN struct net_device *prNetDev, IN char *pcCommand,
			    IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;

	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 3) {
		/* rSwCtrlInfo.u4Id = kalStrtoul(apcArgv[1], NULL, 0);
		 *  rSwCtrlInfo.u4Data = kalStrtoul(apcArgv[2], NULL, 0);
		 */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rSwCtrlInfo.u4Id));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse rSwCtrlInfo error u4Ret=%d\n",
			       u4Ret);
		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rSwCtrlInfo.u4Data));
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse rSwCtrlInfo error u4Ret=%d\n",
			       u4Ret);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	}

	return i4BytesWritten;

}				/* priv_driver_set_sw_ctrl */



int priv_driver_set_fixed_rate(IN struct net_device *prNetDev,
			       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	/* INT_32 u4Ret = 0; */
	uint32_t u4WCID = 0;
	int32_t i4Recv = 0;
	int8_t *this_char = NULL;
	uint32_t u4Id = 0xa0610000;
	uint32_t u4Id2 = 0xa0600000;
	static uint8_t fgIsUseWCID = FALSE;
	struct FIXED_RATE_INFO rFixedRate;

	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %d, apcArgv[0] = %s\n\n", i4Argc, *apcArgv);

	this_char = kalStrStr(*apcArgv, "=");
	if (!this_char)
		return -1;
	this_char++;
	kalMemZero(&rFixedRate, sizeof(struct FIXED_RATE_INFO));
	DBGLOG(REQ, LOUD, "string = %s\n", this_char);

	if (strnicmp(this_char, "auto", strlen("auto")) == 0) {
		i4Recv = 1;
	} else if (strnicmp(this_char, "UseWCID", strlen("UseWCID")) == 0) {
		i4Recv = 2;
		fgIsUseWCID = TRUE;
	} else if (strnicmp(this_char, "ApplyAll", strlen("ApplyAll")) == 0) {
		i4Recv = 3;
		fgIsUseWCID = FALSE;
	} else {
		i4Recv = sscanf(this_char,
			"%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d",
			&(u4WCID),
			&(rFixedRate.u4Mode),
			&(rFixedRate.u4Bw),
			&(rFixedRate.u4Mcs),
			&(rFixedRate.u4VhtNss),
			&(rFixedRate.u4SGI),
			&(rFixedRate.u4Preamble),
			&(rFixedRate.u4STBC),
			&(rFixedRate.u4LDPC),
			&(rFixedRate.u4SpeEn),
			&(rFixedRate.u4HeLTF),
			&(rFixedRate.u4HeErDCM),
			&(rFixedRate.u4HeEr106t));


		DBGLOG(REQ, LOUD, "u4WCID=%d\nu4Mode=%d\nu4Bw=%d\n", u4WCID,
			rFixedRate.u4Mode, rFixedRate.u4Bw);
		DBGLOG(REQ, LOUD, "u4Mcs=%d\nu4VhtNss=%d\nu4GI=%d\n",
			rFixedRate.u4Mcs,
			rFixedRate.u4VhtNss, rFixedRate.u4SGI);
		DBGLOG(REQ, LOUD, "u4Preamble=%d\nu4STBC=%d\n",
			rFixedRate.u4Preamble,
			rFixedRate.u4STBC);
		DBGLOG(REQ, LOUD, "u4LDPC=%d\nu4SpeEn=%d\nu4HeLTF=%d\n",
			rFixedRate.u4LDPC, rFixedRate.u4SpeEn,
			rFixedRate.u4HeLTF);
		DBGLOG(REQ, LOUD, "u4HeErDCM=%d\nu4HeEr106t=%d\n",
			rFixedRate.u4HeErDCM, rFixedRate.u4HeEr106t);
		DBGLOG(REQ, LOUD, "fgIsUseWCID=%d\n", fgIsUseWCID);
	}

	if (i4Recv == 1) {
		rSwCtrlInfo.u4Id = u4Id2;
		rSwCtrlInfo.u4Data = 0;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	} else if (i4Recv == 2 || i4Recv == 3) {
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
			"Update fgIsUseWCID %d\n", fgIsUseWCID);
	} else if (i4Recv == 10 || i4Recv == 13) {
		rSwCtrlInfo.u4Id = u4Id;
		if (fgIsUseWCID && u4WCID < prAdapter->ucWtblEntryNum &&
			prAdapter->rWifiVar.arWtbl[u4WCID].ucUsed) {
			rSwCtrlInfo.u4Id |= u4WCID;
			rSwCtrlInfo.u4Id |= BIT(8);
			i4BytesWritten = kalSnprintf(
				pcCommand, i4TotalLen,
				"Apply WCID %d\n", u4WCID);
		} else {
			i4BytesWritten = kalSnprintf(
				pcCommand, i4TotalLen, "Apply All\n");
		}

		if (rFixedRate.u4Mode >= TX_RATE_MODE_HE_SU) {
			if (i4Recv == 10) {
				/* Give default value */
				rFixedRate.u4HeLTF = HE_LTF_4X;
				rFixedRate.u4HeErDCM = 0;
				rFixedRate.u4HeEr106t = 0;
			}
			/* check HE-LTF and HE GI combinations */
			rStatus = nicRateHeLtfCheckGi(&rFixedRate);

			if (rStatus != WLAN_STATUS_SUCCESS)
				return -1;
		}

		rStatus = nicSetFixedRateData(&rFixedRate, &rSwCtrlInfo.u4Data);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite,
					   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
					   FALSE, FALSE, TRUE, &u4BufLen);
		}

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	} else {
		DBGLOG(REQ, ERROR, "iwpriv wlanXX driver FixedRate=Option\n");
		DBGLOG(REQ, ERROR, "Option support 10 or 13 parameters\n");
		DBGLOG(REQ, ERROR,
		       "Option:[WCID]-[Mode]-[BW]-[MCS]-[VhtHeNss]-[GI]-[Preamble]-[STBC]-[LDPC]-[SPE_EN]\n");
		DBGLOG(REQ, ERROR,
			"13 param support another [HE-LTF]-[HE-ER-DCM]-[HE-ER-106]\n");
		DBGLOG(REQ, ERROR, "[WCID]Wireless Client ID\n");
		DBGLOG(REQ, ERROR, "[Mode]CCK=0, OFDM=1, HT=2, GF=3, VHT=4");
		DBGLOG(REQ, ERROR,
			"PLR=5, HE_SU=8, HE_ER_SU=9, HE_TRIG=10, HE_MU=11\n");
		DBGLOG(REQ, ERROR, "[BW]BW20=0, BW40=1, BW80=2,BW160=3\n");
		DBGLOG(REQ, ERROR,
		       "[MCS]CCK=0~3, OFDM=0~7, HT=0~32, VHT=0~9, HE=0~11\n");
		DBGLOG(REQ, ERROR, "[VhtHeNss]1~8, Other=ignore\n");
		DBGLOG(REQ, ERROR, "[GI]HT/VHT: 0:Long, 1:Short, ");
		DBGLOG(REQ, ERROR,
			"HE: SGI=0(0.8us), MGI=1(1.6us), LGI=2(3.2us)\n");
		DBGLOG(REQ, ERROR, "[Preamble]Long=0, Other=Short\n");
		DBGLOG(REQ, ERROR, "[STBC]Enable=1, Disable=0\n");
		DBGLOG(REQ, ERROR, "[LDPC]BCC=0, LDPC=1\n");
		DBGLOG(REQ, ERROR, "[HE-LTF]1X=0, 2X=1, 4X=2\n");
		DBGLOG(REQ, ERROR, "[HE-ER-DCM]Enable=1, Disable=0\n");
		DBGLOG(REQ, ERROR, "[HE-ER-106]106-tone=1\n");
	}

	return i4BytesWritten;
}	/* priv_driver_set_fixed_rate */

int priv_driver_set_em_cfg(IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t u4CfgSetNum = 0, u4Ret = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4BufLen;

	struct PARAM_CUSTOM_KEY_CFG_STRUCT rKeyCfgInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);
	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -1; /* WLAN_STATUS_ADAPTER_NOT_READY */

	kalMemZero(&rKeyCfgInfo, sizeof(rKeyCfgInfo));

	wlanCleanAllEmCfgSetting(prAdapter);


	if (i4Argc >= 3) {

		uint8_t	i = 0;

		u4Ret = kalkStrtou32(apcArgv[1], 10, &u4CfgSetNum);

		if (u4Ret != 0) {
			DBGLOG(REQ, ERROR,
			       "apcArgv[2] format fail erro code:%d\n",
			       u4Ret);
			return -1;
		}

		if (u4CfgSetNum*2 > i4Argc) {
			DBGLOG(REQ, ERROR,
			       "Set Num(%d) over input arg num(%d)\n",
			       u4CfgSetNum, i4Argc);
			return -1;
		}

		DBGLOG(REQ, INFO, "Total Cfg Num=%d\n", u4CfgSetNum);

		for (i = 0; i < (u4CfgSetNum*2); i += 2) {

			kalStrnCpy(rKeyCfgInfo.aucKey, apcArgv[2+i],
				   WLAN_CFG_KEY_LEN_MAX - 1);
			kalStrnCpy(rKeyCfgInfo.aucValue, apcArgv[2+(i+1)],
				   WLAN_CFG_VALUE_LEN_MAX - 1);
			rKeyCfgInfo.u4Flag = WLAN_CFG_EM;

			DBGLOG(REQ, INFO,
				"Update to driver EM CFG [%s]:[%s],OP:%d\n",
				rKeyCfgInfo.aucKey,
				rKeyCfgInfo.aucValue,
				rKeyCfgInfo.u4Flag);

			rStatus = kalIoctl(prGlueInfo,
				wlanoidSetKeyCfg,
				&rKeyCfgInfo,
				sizeof(rKeyCfgInfo),
				FALSE,
				FALSE,
				TRUE,
				&u4BufLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				return -1;


		}

	}

	return i4BytesWritten;

}				/* priv_driver_set_cfg_em  */

int priv_driver_get_em_cfg(IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	int32_t i = 0;
	uint32_t u4Offset = 0;
	uint32_t u4CfgNum = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, INFO,
		"command is %s, i4TotalLen=%d\n",
		pcCommand,
		i4TotalLen);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %i\n", i4Argc);
	prAdapter = prGlueInfo->prAdapter;

	if (i4Argc >= 1) {

		u4CfgNum = wlanCfgGetTotalCfgNum(prAdapter, WLAN_CFG_EM);

		DBGLOG(REQ, INFO,
			   "Total cfg Num:%d\n", u4CfgNum);

		u4Offset += snprintf(pcCommand + u4Offset,
			(i4TotalLen - u4Offset),
			"%d,",
			u4CfgNum);

		for (i = 0; i < u4CfgNum; i++) {
			prWlanCfgEntry = wlanCfgGetEntryByIndex(
				prAdapter,
				i,
				WLAN_CFG_EM);

			if ((!prWlanCfgEntry)
				|| (prWlanCfgEntry->aucKey[0] == '\0'))
				break;

			DBGLOG(REQ, INFO,
				"cfg dump:(%s,%s)\n",
				prWlanCfgEntry->aucKey,
				prWlanCfgEntry->aucValue);

			if (u4Offset >= i4TotalLen) {
				DBGLOG(REQ, ERROR,
					"out of bound\n");
				break;
			}

			u4Offset += snprintf(pcCommand + u4Offset,
				(i4TotalLen - u4Offset),
				"%s,%s,",
				prWlanCfgEntry->aucKey,
				prWlanCfgEntry->aucValue);

		}

		pcCommand[u4Offset-1] = '\n';
		pcCommand[u4Offset] = '\0';

	}

	return (int32_t)u4Offset;

}				/* priv_driver_get_cfg_em  */


int priv_driver_set_cfg(IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };

	struct PARAM_CUSTOM_KEY_CFG_STRUCT rKeyCfgInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);
	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL)
		return -1; /* WLAN_STATUS_ADAPTER_NOT_READY */

	kalMemZero(&rKeyCfgInfo, sizeof(rKeyCfgInfo));

	if (i4Argc >= 3) {

		int8_t ucTmp[WLAN_CFG_VALUE_LEN_MAX];
		uint8_t *pucCurrBuf = ucTmp;
		uint8_t	i = 0;
		uint32_t offset = 0;

		pucCurrBuf = ucTmp;
		kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);

		if (i4Argc == 3) {
			/* no space for it, driver can't accept space in the end
			 * of the line
			 */
			/* ToDo: skip the space when parsing */
			u4BufLen = kalStrLen(apcArgv[2]);
			if (offset + u4BufLen > WLAN_CFG_VALUE_LEN_MAX - 1) {
				DBGLOG(INIT, ERROR,
				       "apcArgv[2] length [%d] overrun\n",
				       u4BufLen);
				return -1;
			}
			kalStrnCpy(pucCurrBuf + offset, apcArgv[2], u4BufLen);
			offset += u4BufLen;
		} else {
			for (i = 2; i < i4Argc; i++) {
				u4BufLen = kalStrLen(apcArgv[i]);
				if (offset + u4BufLen >
				    WLAN_CFG_VALUE_LEN_MAX - 1) {
					DBGLOG(INIT, ERROR,
					       "apcArgv[%d] length [%d] overrun\n",
					       i, u4BufLen);
					return -1;
				}
				kalStrnCpy(pucCurrBuf + offset, apcArgv[i],
					   u4BufLen);
				offset += u4BufLen;
			}
		}

		DBGLOG(INIT, WARN, "Update to driver temp buffer as [%s]\n",
		       ucTmp);

		/* wlanCfgSet(prAdapter, apcArgv[1], apcArgv[2], 0); */
		/* Call by  wlanoid because the set_cfg will trigger callback */
		kalStrnCpy(rKeyCfgInfo.aucKey, apcArgv[1],
			   WLAN_CFG_KEY_LEN_MAX - 1);
		kalStrnCpy(rKeyCfgInfo.aucValue, ucTmp,
			   WLAN_CFG_VALUE_LEN_MAX - 1);

		rKeyCfgInfo.u4Flag = WLAN_CFG_DEFAULT;
		rStatus = kalIoctl(prGlueInfo, wlanoidSetKeyCfg, &rKeyCfgInfo,
				   sizeof(rKeyCfgInfo), FALSE, FALSE, TRUE,
				   &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}

	return i4BytesWritten;

}				/* priv_driver_set_cfg  */

int priv_driver_get_cfg(IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);
	prAdapter = prGlueInfo->prAdapter;

	if (i4Argc >= 2) {
		/* by wlanoid ? */
		if (wlanCfgGet(prAdapter, apcArgv[1], aucValue, "", 0) ==
		    WLAN_STATUS_SUCCESS) {
			kalStrnCpy(pcCommand, aucValue, WLAN_CFG_VALUE_LEN_MAX);
			i4BytesWritten = kalStrnLen(pcCommand,
						    WLAN_CFG_VALUE_LEN_MAX);
		}
	}

	return i4BytesWritten;

}				/* priv_driver_get_cfg  */

int priv_driver_set_chip_config(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	uint32_t u4CmdLen = 0;
	uint32_t u4PrefixLen = 0;
	/* INT_32 i4Argc = 0; */
	/* PCHAR  apcArgv[WLAN_CFG_ARGV_MAX] = {0}; */

	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo = {0};

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if ((!prGlueInfo) ||
	    (prGlueInfo->u4ReadyFlag == 0) ||
	    kalIsResetting()) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -1;
	}

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
		/* rChipConfigInfo.u2MsgSize = kalStrnLen(apcArgv[1],
		 *					CHIP_CONFIG_RESP_SIZE);
		 */
		rChipConfigInfo.u2MsgSize = u4CmdLen - u4PrefixLen;
		/* kalStrnCpy(rChipConfigInfo.aucCmd, apcArgv[1],
		 *	      CHIP_CONFIG_RESP_SIZE);
		 */
		kalStrnCpy(rChipConfigInfo.aucCmd, pcCommand + u4PrefixLen,
			   CHIP_CONFIG_RESP_SIZE - 1);
		rChipConfigInfo.aucCmd[CHIP_CONFIG_RESP_SIZE - 1] = '\0';

#if (CFG_SUPPORT_802_11AX == 1)
		if (kalStrnCmp("FrdHeTrig2Host",
			pcCommand, kalStrLen("FrdHeTrig2Host"))) {
			uint32_t idx = kalStrLen("set_chip FrdHeTrig2Host ");

			prAdapter->fgEnShowHETrigger = pcCommand[idx] - 0x30;
			DBGLOG(REQ, STATE, "set flag fgEnShowHETrigger:%x\n",
			prAdapter->fgEnShowHETrigger);
		}
#endif /* CFG_SUPPORT_802_11AX  == 1 */

		rStatus = kalIoctl(prGlueInfo, wlanoidSetChipConfig,
				   &rChipConfigInfo, sizeof(rChipConfigInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, INFO, "%s: kalIoctl ret=%d\n", __func__,
			       rStatus);
			i4BytesWritten = -1;
		}
	}

	return i4BytesWritten;

}				/* priv_driver_set_chip_config  */

void
priv_driver_get_chip_config_16(uint8_t *pucStartAddr, uint32_t u4Length,
			       uint32_t u4Line, int i4TotalLen,
			       int32_t i4BytesWritten, char *pcCommand)
{

	while (u4Length >= 16) {
		if (i4TotalLen > i4BytesWritten) {
			i4BytesWritten +=
			    kalSnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%04x %02x %02x %02x %02x  %02x %02x %02x %02x - %02x %02x %02x %02x  %02x %02x %02x %02x\n",
					u4Line, pucStartAddr[0],
					pucStartAddr[1], pucStartAddr[2],
					pucStartAddr[3], pucStartAddr[4],
					pucStartAddr[5], pucStartAddr[6],
					pucStartAddr[7], pucStartAddr[8],
					pucStartAddr[9], pucStartAddr[10],
					pucStartAddr[11], pucStartAddr[12],
					pucStartAddr[13], pucStartAddr[14],
					pucStartAddr[15]);
		}

		pucStartAddr += 16;
		u4Length -= 16;
		u4Line += 16;
	}			/* u4Length */
}


void
priv_driver_get_chip_config_4(uint32_t *pu4StartAddr, uint32_t u4Length,
			      uint32_t u4Line, int i4TotalLen,
			      int32_t i4BytesWritten, char *pcCommand)
{
	while (u4Length >= 16) {
		if (i4TotalLen > i4BytesWritten) {
			i4BytesWritten +=
			    kalSnprintf(pcCommand + i4BytesWritten,
				     i4TotalLen - i4BytesWritten,
				     "%04x %08x %08x %08x %08x\n", u4Line,
				     pu4StartAddr[0], pu4StartAddr[1],
				     pu4StartAddr[2], pu4StartAddr[3]);
		}

		pu4StartAddr += 4;
		u4Length -= 16;
		u4Line += 4;
	}			/* u4Length */
}

int priv_driver_get_chip_config(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0;
	uint32_t u4BufLen = 0;
	uint32_t u2MsgSize = 0;
	uint32_t u4CmdLen = 0;
	uint32_t u4PrefixLen = 0;
	/* INT_32 i4Argc = 0; */
	/* PCHAR  apcArgv[WLAN_CFG_ARGV_MAX]; */

	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo = {0};

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	/* wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv); */
	/* DBGLOG(REQ, LOUD,("argc is %i\n",i4Argc)); */

	u4CmdLen = kalStrnLen(pcCommand, i4TotalLen);
	u4PrefixLen = kalStrLen(CMD_GET_CHIP) + 1 /*space */;

	/* if(i4Argc >= 2) { */
	if (u4CmdLen > u4PrefixLen) {
		rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_ASCII;
		/* rChipConfigInfo.u2MsgSize = kalStrnLen(apcArgv[1],
		 *                             CHIP_CONFIG_RESP_SIZE);
		 */
		rChipConfigInfo.u2MsgSize = u4CmdLen - u4PrefixLen;
		/* kalStrnCpy(rChipConfigInfo.aucCmd, apcArgv[1],
		 *            CHIP_CONFIG_RESP_SIZE);
		 */
		kalStrnCpy(rChipConfigInfo.aucCmd, pcCommand + u4PrefixLen,
			   CHIP_CONFIG_RESP_SIZE - 1);
		rChipConfigInfo.aucCmd[CHIP_CONFIG_RESP_SIZE - 1] = '\0';
		rStatus = kalIoctl(prGlueInfo, wlanoidQueryChipConfig,
				   &rChipConfigInfo, sizeof(rChipConfigInfo),
				   TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, INFO, "%s: kalIoctl ret=%d\n", __func__,
			       rStatus);
			return -1;
		}

		/* Check respType */
		u2MsgSize = rChipConfigInfo.u2MsgSize;
		DBGLOG(REQ, INFO, "%s: RespTyep  %u\n", __func__,
		       rChipConfigInfo.ucRespType);
		DBGLOG(REQ, INFO, "%s: u2MsgSize %u\n", __func__,
		       rChipConfigInfo.u2MsgSize);

		if (u2MsgSize > sizeof(rChipConfigInfo.aucCmd)) {
			DBGLOG(REQ, INFO, "%s: u2MsgSize error ret=%u\n",
			       __func__, rChipConfigInfo.u2MsgSize);
			return -1;
		}

		if (u2MsgSize > 0) {

			if (rChipConfigInfo.ucRespType ==
			    CHIP_CONFIG_TYPE_ASCII) {
				i4BytesWritten =
				    kalSnprintf(pcCommand + i4BytesWritten,
					     i4TotalLen, "%s",
					     rChipConfigInfo.aucCmd);
			} else {
				uint32_t u4Length;
				uint32_t u4Line;

				if (rChipConfigInfo.ucRespType ==
				    CHIP_CONFIG_TYPE_MEM8) {
					uint8_t *pucStartAddr = NULL;

					pucStartAddr = (uint8_t *)
							rChipConfigInfo.aucCmd;
					/* align 16 bytes because one print line
					 * is 16 bytes
					 */
					u4Length = (((u2MsgSize + 15) >> 4))
									<< 4;
					u4Line = 0;
					priv_driver_get_chip_config_16(
						pucStartAddr, u4Length, u4Line,
						i4TotalLen, i4BytesWritten,
						pcCommand);
				} else {
					uint32_t *pu4StartAddr = NULL;

					pu4StartAddr = (uint32_t *)
							rChipConfigInfo.aucCmd;
					/* align 16 bytes because one print line
					 * is 16 bytes
					 */
					u4Length = (((u2MsgSize + 15) >> 4))
									<< 4;
					u4Line = 0;

					if (IS_ALIGN_4(
					    (unsigned long) pu4StartAddr)) {
						priv_driver_get_chip_config_4(
							pu4StartAddr, u4Length,
							u4Line, i4TotalLen,
							i4BytesWritten,
							pcCommand);
					} else {
						DBGLOG(REQ, INFO,
							"%s: rChipConfigInfo.aucCmd is not 4 bytes alignment %p\n",
							__func__,
							rChipConfigInfo.aucCmd);
					}
				}	/* ChipConfigInfo.ucRespType */
			}
		}
		/* u2MsgSize > 0 */
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__,
		       pcCommand);
	}
	/* i4Argc */
	return i4BytesWritten;

}				/* priv_driver_get_chip_config  */



int priv_driver_set_ap_start(IN struct net_device *prNetDev, IN char *pcCommand,
			     IN int i4TotalLen)
{

	struct PARAM_CUSTOM_P2P_SET_STRUCT rSetP2P;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret;
	int32_t i4ArgNum = 2;


	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rSetP2P.u4Mode));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "parse ap-start error (u4Enable) u4Ret=%d\n",
			       u4Ret);

		if (rSetP2P.u4Mode >= RUNNING_P2P_MODE_NUM) {
			rSetP2P.u4Mode = 0;
			rSetP2P.u4Enable = 0;
		} else
			rSetP2P.u4Enable = 1;

		set_p2p_mode_handler(prNetDev, rSetP2P);
	}

	return 0;
}

int priv_driver_get_linkspeed(IN struct net_device *prNetDev,
			      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_LINK_SPEED_EX rLinkSpeed;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	uint32_t u4Rate = 0;
	int32_t i4BytesWritten = 0;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ucBssIndex = wlanGetBssIdx(prNetDev);

	if (!netif_carrier_ok(prNetDev))
		return -1;

	if (ucBssIndex >= BSSID_NUM)
		return -EFAULT;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryLinkSpeedEx,
			   &rLinkSpeed, sizeof(rLinkSpeed),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;
	u4Rate = rLinkSpeed.rLq[ucBssIndex].u2LinkSpeed;
	i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen, "LinkSpeed %u",
				  (unsigned int)(u4Rate * 100));
	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);
	return i4BytesWritten;

}				/* priv_driver_get_linkspeed */

int priv_driver_set_band(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct ADAPTER *prAdapter = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	uint32_t ucBand = 0;
	enum ENUM_BAND eBand = BAND_NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prAdapter = prGlueInfo->prAdapter;
	if (i4Argc >= 2) {
		/* ucBand = kalStrtoul(apcArgv[1], NULL, 0); */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &ucBand);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucBand error u4Ret=%d\n",
			       u4Ret);

		eBand = BAND_NULL;
		if (ucBand == CMD_BAND_TYPE_5G)
			eBand = BAND_5G;
		else if (ucBand == CMD_BAND_TYPE_2G)
			eBand = BAND_2G4;
		prAdapter->aePreferBand[KAL_NETWORK_TYPE_AIS_INDEX] = eBand;
		/* XXX call wlanSetPreferBandByNetwork directly in different
		 * thread
		 */
		/* wlanSetPreferBandByNetwork (prAdapter, eBand, ucBssIndex); */
	}

	return 0;
}


int priv_driver_set_country(IN struct net_device *prNetDev,
			    IN char *pcCommand, IN int i4TotalLen)
{

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t aucCountry[2];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (regd_is_single_sku_en()) {
		uint8_t aucCountry_code[4] = {0, 0, 0, 0};
		uint8_t i, count;

		/* command like "COUNTRY US", "COUNTRY US1" and
		 * "COUNTRY US01"
		 */
		count = kalStrnLen(apcArgv[1], sizeof(aucCountry_code));
		for (i = 0; i < count; i++)
			aucCountry_code[i] = apcArgv[1][i];


		rStatus = kalIoctl(prGlueInfo, wlanoidSetCountryCode,
				   &aucCountry_code[0], count,
				   FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		return 0;
	}


	if (i4Argc >= 2) {
		/* command like "COUNTRY US", "COUNTRY EU" and "COUNTRY JP" */
		aucCountry[0] = apcArgv[1][0];
		aucCountry[1] = apcArgv[1][1];

		rStatus = kalIoctl(prGlueInfo, wlanoidSetCountryCode,
				   &aucCountry[0], 2, FALSE, FALSE, TRUE,
				   &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	}
	return 0;
}

int priv_driver_set_csa(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t ch_num = 0;
	uint32_t u4Ret = 0;
	uint8_t ucRoleIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		return -1;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &ch_num);
		u4Ret = cnmIdcCsaReq(prGlueInfo->prAdapter, ch_num, ucRoleIdx);
		DBGLOG(REQ, INFO, "u4Ret is %d\n", u4Ret);
	} else {
		DBGLOG(REQ, INFO, "Input insufficent\n");
	}

	return 0;
}

int priv_driver_get_country(IN struct net_device *prNetDev,
			    IN char *pcCommand, IN int i4TotalLen)
{

	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t i4BytesWritten = 0;
	uint32_t country = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

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

int priv_driver_get_channels(IN struct net_device *prNetDev,
			     IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	uint32_t ch_idx, start_idx, end_idx;
	struct CMD_DOMAIN_CHANNEL *pCh;
	uint32_t ch_num = 0;
	uint8_t maxbw = 160;
	uint32_t u4Ret = 0;
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
	} else if (i4Argc >= 2 && (apcArgv[1][0] == '5') &&
	    (apcArgv[1][1] == 'g')) {
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

			if (ch_num && (ch_num != pCh->u2ChNum))
				continue; /*show specific channel information*/

			/* Channel number */
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "CH-%d:",
					pCh->u2ChNum);

			/* Active/Passive */
			if (pCh->eFlags & IEEE80211_CHAN_PASSIVE_FLAG) {
				/* passive channel */
				LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				       " " IEEE80211_CHAN_PASSIVE_STR);
			} else
				LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				       " ACTIVE");

			/* Max BW */
			if ((pCh->eFlags & IEEE80211_CHAN_NO_160MHZ) ==
			    IEEE80211_CHAN_NO_160MHZ)
				maxbw = 80;
			if ((pCh->eFlags & IEEE80211_CHAN_NO_80MHZ) ==
			    IEEE80211_CHAN_NO_80MHZ)
				maxbw = 40;
			if ((pCh->eFlags & IEEE80211_CHAN_NO_HT40) ==
			    IEEE80211_CHAN_NO_HT40)
				maxbw = 20;
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			       " BW_%dMHz", maxbw);
			/* Channel flags */
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			       "  (flags=0x%x)\n", pCh->eFlags);
		}
	}
#endif

	return i4BytesWritten;
}

#if (CFG_SUPPORT_DFS_MASTER == 1)
int priv_driver_show_dfs_state(IN struct net_device *prNetDev,
			       IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = 0;

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

int priv_driver_show_dfs_radar_param(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = 0;
	uint8_t ucCnt = 0;
	struct P2P_RADAR_INFO *prP2pRadarInfo = (struct P2P_RADAR_INFO *) NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	prP2pRadarInfo = (struct P2P_RADAR_INFO *) cnmMemAlloc(
		prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(*prP2pRadarInfo));
	if (prP2pRadarInfo == NULL)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	p2pFuncGetRadarInfo(prP2pRadarInfo);

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nRDD idx: %d\n",
	       prP2pRadarInfo->ucRddIdx);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nLong Pulse detected: %d\n", prP2pRadarInfo->ucLongDetected);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nPeriodic Pulse detected: %d\n",
	       prP2pRadarInfo->ucPeriodicDetected);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nLPB Num: %d\n",
	       prP2pRadarInfo->ucLPBNum);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\nPPB Num: %d\n",
	       prP2pRadarInfo->ucPPBNum);

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n===========================");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nLong Pulse Buffer Contents:\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\npulse_time    pulse_width    PRI\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n%-10d    %-11d    -\n"
		, prP2pRadarInfo->arLpbContent[ucCnt].u4LongStartTime
		, prP2pRadarInfo->arLpbContent[ucCnt].u2LongPulseWidth);
	for (ucCnt = 1; ucCnt < prP2pRadarInfo->ucLPBNum; ucCnt++) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		       "\n%-10d    %-11d    %d\n",
		       prP2pRadarInfo->arLpbContent[ucCnt].u4LongStartTime,
		       prP2pRadarInfo->arLpbContent[ucCnt].u2LongPulseWidth,
		       (prP2pRadarInfo->arLpbContent[ucCnt].u4LongStartTime
		       - prP2pRadarInfo->arLpbContent[ucCnt-1]
						.u4LongStartTime) * 2 / 5);
	}
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nLPB Period Valid: %d", prP2pRadarInfo->ucLPBPeriodValid);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nLPB Period Valid: %d\n", prP2pRadarInfo->ucLPBWidthValid);

	ucCnt = 0;
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n===========================");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nPeriod Pulse Buffer Contents:\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\npulse_time    pulse_width    PRI\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "\n%-10d    %-11d    -\n"
		, prP2pRadarInfo->arPpbContent[ucCnt].u4PeriodicStartTime
		, prP2pRadarInfo->arPpbContent[ucCnt].u2PeriodicPulseWidth);
	for (ucCnt = 1; ucCnt < prP2pRadarInfo->ucPPBNum; ucCnt++) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		       "\n%-10d    %-11d    %d\n"
		       , prP2pRadarInfo->arPpbContent[ucCnt].u4PeriodicStartTime
		       , prP2pRadarInfo->arPpbContent[ucCnt]
						.u2PeriodicPulseWidth
		       , (prP2pRadarInfo->arPpbContent[ucCnt]
						.u4PeriodicStartTime
		       - prP2pRadarInfo->arPpbContent[ucCnt-1]
						.u4PeriodicStartTime) * 2 / 5);
	}
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nPRI Count M1 TH: %d; PRI Count M1: %d",
	       prP2pRadarInfo->ucPRICountM1TH, prP2pRadarInfo->ucPRICountM1);
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nPRI Count M2 TH: %d; PRI Count M2: %d",
	       prP2pRadarInfo->ucPRICountM2TH,
	       prP2pRadarInfo->ucPRICountM2);


	cnmMemFree(prGlueInfo->prAdapter, prP2pRadarInfo);

	return	i4BytesWritten;
}

int priv_driver_show_dfs_help(IN struct net_device *prNetDev,
			      IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);


	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n--iwpriv wlanX driver \"show_dfs_state\"\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nINACTIVE: RDD disable or temporary RDD disable");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nCHECKING: During CAC time");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nACTIVE  : In-serive monitoring");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nDETECTED: Has detected radar but hasn't moved to new channel\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n--iwpriv wlanX driver \"show_dfs_radar_param\"\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nShow the latest pulse information\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n--iwpriv wlanX driver \"show_dfs_cac_time\"\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nShow the remaining time of CAC\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n--iwpriv wlanX set ByPassCac=yy\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nValue yy: set the time of CAC\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n--iwpriv wlanX set RDDReport=yy\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nValue yy is \"0\" or \"1\"");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n\"0\": Emulate RDD0 manual radar event");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n\"1\": Emulate RDD1 manual radar event\n");

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n--iwpriv wlanX set RadarDetectMode=yy\n");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nValue yy is \"0\" or \"1\"");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n\"0\": Switch channel when radar detected (default)");
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n\"1\": Do not switch channel when radar detected");

	return	i4BytesWritten;
}

int priv_driver_show_dfs_cac_time(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (p2pFuncGetDfsState() != DFS_STATE_CHECKING) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		       "\nNot in CAC period");
		return i4BytesWritten;
	}

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\nRemaining time of CAC: %dsec", p2pFuncGetCacRemainingTime());

	return	i4BytesWritten;
}

#endif
int priv_driver_set_miracast(IN struct net_device *prNetDev,
			     IN char *pcCommand, IN int i4TotalLen)
{

	struct ADAPTER *prAdapter = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t i4BytesWritten = 0;
	/* WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS; */
	/* UINT_32 u4BufLen = 0; */
	int32_t i4Argc = 0;
	uint32_t ucMode = 0;
	struct WFD_CFG_SETTINGS *prWfdCfgSettings =
				(struct WFD_CFG_SETTINGS *) NULL;
	struct MSG_WFD_CONFIG_SETTINGS_CHANGED *prMsgWfdCfgUpdate =
				(struct MSG_WFD_CONFIG_SETTINGS_CHANGED *) NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prAdapter = prGlueInfo->prAdapter;
	if (i4Argc >= 2) {
		/* ucMode = kalStrtoul(apcArgv[1], NULL, 0); */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &ucMode);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucMode error u4Ret=%d\n",
			       u4Ret);

		if (g_ucMiracastMode == (uint8_t) ucMode) {
			/* XXX: continue or skip */
			/* XXX: continue or skip */
		}

		g_ucMiracastMode = (uint8_t) ucMode;
		prMsgWfdCfgUpdate = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				sizeof(struct MSG_WFD_CONFIG_SETTINGS_CHANGED));

		if (prMsgWfdCfgUpdate != NULL) {

			prWfdCfgSettings =
				&(prAdapter->rWifiVar.rWfdConfigureSettings);
			prMsgWfdCfgUpdate->rMsgHdr.eMsgId =
						MID_MNY_P2P_WFD_CFG_UPDATE;
			prMsgWfdCfgUpdate->prWfdCfgSettings = prWfdCfgSettings;

			if (ucMode == MIRACAST_MODE_OFF) {
				prWfdCfgSettings->ucWfdEnable = 0;
				kalSnprintf(pcCommand, i4TotalLen,
					 CMD_SET_CHIP " mira 0");
			} else if (ucMode == MIRACAST_MODE_SOURCE) {
				prWfdCfgSettings->ucWfdEnable = 1;
				kalSnprintf(pcCommand, i4TotalLen,
					 CMD_SET_CHIP " mira 1");
			} else if (ucMode == MIRACAST_MODE_SINK) {
				prWfdCfgSettings->ucWfdEnable = 2;
				kalSnprintf(pcCommand, i4TotalLen,
					 CMD_SET_CHIP " mira 2");
			} else {
				prWfdCfgSettings->ucWfdEnable = 0;
				kalSnprintf(pcCommand, i4TotalLen,
					 CMD_SET_CHIP " mira 0");
			}

			mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *)
				    prMsgWfdCfgUpdate, MSG_SEND_METHOD_BUF);

			priv_driver_set_chip_config(prNetDev, pcCommand,
						    i4TotalLen);
		} /* prMsgWfdCfgUpdate */
		else {
			ASSERT(FALSE);
			i4BytesWritten = -1;
		}
	}

	/* i4Argc */
	return i4BytesWritten;
}

int parseValueInString(
	IN char **pcCommand,
	IN const char *acDelim,
	IN void *aucValue,
	IN int u4MaxLen)
{
	uint8_t *pcPtr;
	uint32_t u4Len;
	uint8_t *pucValueHead = NULL;
	uint8_t *pucValueTail = NULL;

	if (*pcCommand
		&& !kalStrnCmp(*pcCommand, acDelim, kalStrLen(acDelim))) {
		pcPtr = kalStrSep(pcCommand, "=,");
		pucValueHead = *pcCommand;
		pcPtr = kalStrSep(pcCommand, "=,");
		DBGLOG(REQ, TRACE, "pucValueHead = %s\n", pucValueHead);
		if (pucValueHead) {
			u4Len = kalStrLen(pucValueHead);
			if (*pcCommand) {
				pucValueTail = *pcCommand - 1;
				u4Len = pucValueTail - pucValueHead;
			}
			if (u4Len > u4MaxLen)
				u4Len = u4MaxLen;

			/* MAC */
			if (!kalStrnCmp(acDelim, "MAC=", kalStrLen(acDelim))) {
				u8 *addr = aucValue;

				wlanHwAddrToBin(pucValueHead, addr);
				DBGLOG(REQ, TRACE, "MAC type");
			} else {
				u8 *addr = aucValue;

				kalStrnCpy(addr, pucValueHead, u4Len);
				*((char *)aucValue + u4Len) = '\0';
				DBGLOG(REQ, TRACE,
					"STR type = %s\n", (char *)aucValue);
			}
			return 0;
		}
	}

	return -1;
}

int priv_driver_set_ap_set_mac_acl(IN struct net_device *prNetDev,
		IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	uint8_t aucValue[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Count = 0, i4Mode = 0;
	int i = 0;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		goto error;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		goto error;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		goto error;

	DBGLOG(REQ, INFO, "ucRoleIdx = %d\n", ucRoleIdx);
	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];
	if (!prBssInfo) {
		DBGLOG(REQ, WARN, "bss is not active\n");
		goto error;
	}

	/* Mode */
	if (parseValueInString(&pcCommand,
		"MAC_MODE=", &aucValue, WLAN_CFG_ARGV_MAX)) {
		DBGLOG(REQ, ERROR, "[MODE] parse error\n");
		goto error;
	}
	if (kalkStrtou32(aucValue, 0, &i4Mode)) {
		DBGLOG(REQ, ERROR, "[MODE] convert to int error\n");
		goto error;
	}
	if (i4Mode == 0)
		prBssInfo->rACL.ePolicy = PARAM_CUSTOM_ACL_POLICY_DISABLE;
	else if (i4Mode == 1)
		prBssInfo->rACL.ePolicy = PARAM_CUSTOM_ACL_POLICY_DENY;
	else if (i4Mode == 2)
		prBssInfo->rACL.ePolicy = PARAM_CUSTOM_ACL_POLICY_ACCEPT;
	else {
		DBGLOG(REQ, ERROR, "[MODE] invalid ACL policy= %d\n", i4Mode);
		goto error;
	}

	/* Count */
	/* No need to parse count for diabled mode */
	if (prBssInfo->rACL.ePolicy != PARAM_CUSTOM_ACL_POLICY_DISABLE) {
		if (parseValueInString(&pcCommand,
			"MAC_CNT=", &aucValue, WLAN_CFG_ARGV_MAX)) {
			DBGLOG(REQ, ERROR, "[CNT] parse count error\n");
			goto error;
		}
		if (kalkStrtou32(aucValue, 0, &i4Count)) {
			DBGLOG(REQ, ERROR, "[CNT] convert to int error\n");
			goto error;
		}
		if (i4Count > MAX_NUMBER_OF_ACL) {
			DBGLOG(REQ, ERROR, "[CNT] invalid count > max ACL\n");
			goto error;
		}
	}

	/* MAC */
	if (prBssInfo->rACL.u4Num) {
		/* Clear */
		kalMemZero(&prBssInfo->rACL.rEntry[0],
			sizeof(struct PARAM_CUSTOM_ACL_ENTRY) * MAC_ADDR_LEN);
		prBssInfo->rACL.u4Num = 0;
	}

	if (prBssInfo->rACL.ePolicy != PARAM_CUSTOM_ACL_POLICY_DISABLE) {
		for (i = 0; i < i4Count; i++) {
			/* Add */
			if (parseValueInString(&pcCommand,
				"MAC=", &aucValue, WLAN_CFG_ARGV_MAX))
				break;
			kalMemCopy(prBssInfo->rACL.rEntry[i].aucAddr,
				&aucValue, MAC_ADDR_LEN);
			DBGLOG(REQ, INFO,
				"[MAC] add mac addr " MACSTR " to ACL(%d).\n",
				MAC2STR(prBssInfo->rACL.rEntry[i].aucAddr), i);
		}

		prBssInfo->rACL.u4Num = i;
		/* check ACL affects any existent association */
		p2pRoleUpdateACLEntry(prAdapter, ucBssIdx);
		DBGLOG(REQ, INFO,
			"[MAC] Mode = %d, #ACL = %d, count = %d\n",
			i4Mode, i, i4Count);
	}

	return i4BytesWritten;

error:
	return -1;
}

int priv_driver_set_ap_set_cfg(IN struct net_device *prNetDev,
	IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct WIFI_VAR *prWifiVar = NULL;
	uint8_t aucValue[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t ucRoleIdx = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4MaxCount = 0, i4Channel = 0;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		goto error;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	prWifiVar = &prAdapter->rWifiVar;

	/* get role index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		goto error;

	DBGLOG(REQ, INFO, "ucRoleIdx = %d\n", ucRoleIdx);

	/* Cfg */
	if (parseValueInString(&pcCommand, "ASCII_CMD=",
		&aucValue, WLAN_CFG_ARGV_MAX)) {
		DBGLOG(REQ, ERROR, "[CFG] cmd parse error\n");
		goto error;
	}
	if (kalStrnCmp(aucValue, "AP_CFG", 6)) {
		DBGLOG(REQ, ERROR, "[CFG] sub cmd parse error\n");
		goto error;
	}

	/* Channel */
	if (parseValueInString(&pcCommand, "CHANNEL=",
		&aucValue, WLAN_CFG_ARGV_MAX)) {
		DBGLOG(REQ, ERROR, "[CH] parse error\n");
		goto error;
	}
	if (kalkStrtou32(aucValue, 0, &i4Channel)) {
		DBGLOG(REQ, ERROR, "[CH] convert to int error\n");
		goto error;
	}

	/* Max SCB */
	if (parseValueInString(&pcCommand, "MAX_SCB=",
		&aucValue, WLAN_CFG_ARGV_MAX)) {
		DBGLOG(REQ, ERROR, "[MAX_SCB] parse error\n");
		goto error;
	}
	if (kalkStrtou32(aucValue, 0, &i4MaxCount)) {
		DBGLOG(REQ, ERROR, "[MAX_SCB] convert to int error\n");
		goto error;
	}

	/* Overwrite AP channel */
	prWifiVar->ucApChannel = i4Channel;

	/* Set max clients of Hotspot */
	kalP2PSetMaxClients(prGlueInfo, i4MaxCount, ucRoleIdx);

	DBGLOG(REQ, INFO,
		"[CFG] CH = %d, MAX_SCB = %d\n",
		i4Channel, i4MaxCount);

	/* Stop ap */
#if 0
	{
		struct PARAM_CUSTOM_P2P_SET_STRUCT rSetP2P;

		rSetP2P.u4Mode = 0;
		rSetP2P.u4Enable = 0;
		set_p2p_mode_handler(prNetDev, rSetP2P);
	}
#endif

	return i4BytesWritten;

error:
	return -1;
}

int priv_driver_set_ap_get_sta_list(IN struct net_device *prNetDev,
		IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	struct LINK *prClientList;
	struct STA_RECORD *prCurrStaRec, *prNextStaRec;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;
	int32_t i4BytesWritten = 0;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		goto error;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		goto error;
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS)
		goto error;

	DBGLOG(REQ, INFO, "ucRoleIdx = %d\n", ucRoleIdx);
	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];
	if (!prBssInfo) {
		DBGLOG(REQ, WARN, "bss is not active\n");
		goto error;
	}

	prClientList = &prBssInfo->rStaRecOfClientList;
	LINK_FOR_EACH_ENTRY_SAFE(prCurrStaRec,
		prNextStaRec, prClientList, rLinkEntry, struct STA_RECORD) {
		if (!prCurrStaRec) {
			DBGLOG(REQ, WARN, "NULL STA_REC\n");
			break;
		}
		DBGLOG(SW4, INFO, "STA[%u] [" MACSTR "]\n",
			prCurrStaRec->ucIndex,
			MAC2STR(prCurrStaRec->aucMacAddr));
		i4BytesWritten += kalSnprintf(
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			""MACSTR"\n",
			MAC2STR(prCurrStaRec->aucMacAddr));
	}

	return i4BytesWritten;

error:
	return -1;
}

int priv_driver_set_ap_sta_disassoc(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint8_t aucValue[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t ucRoleIdx = 0;
	int32_t i4BytesWritten = 0;
	struct MSG_P2P_CONNECTION_ABORT *prDisconnectMsg =
		(struct MSG_P2P_CONNECTION_ABORT *) NULL;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		goto error;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	/* get role index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0)
		goto error;

	DBGLOG(REQ, INFO, "ucRoleIdx = %d\n", ucRoleIdx);

	if (parseValueInString(&pcCommand, "MAC=",
		&aucValue, WLAN_CFG_ARGV_MAX)) {
		DBGLOG(REQ, ERROR, "[MAC] parse error\n");
		goto error;
	}

	prDisconnectMsg =
		(struct MSG_P2P_CONNECTION_ABORT *)
		cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
			sizeof(struct MSG_P2P_CONNECTION_ABORT));
	if (prDisconnectMsg == NULL) {
		ASSERT(FALSE);
		goto error;
	}

	prDisconnectMsg->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_ABORT;
	prDisconnectMsg->ucRoleIdx = ucRoleIdx;
	COPY_MAC_ADDR(prDisconnectMsg->aucTargetID, aucValue);
	prDisconnectMsg->u2ReasonCode = REASON_CODE_UNSPECIFIED;
	prDisconnectMsg->fgSendDeauth = TRUE;

	mboxSendMsg(prGlueInfo->prAdapter,
		MBOX_ID_0,
		(struct MSG_HDR *) prDisconnectMsg,
		MSG_SEND_METHOD_BUF);

	return i4BytesWritten;

error:
	return -1;
}

int priv_driver_set_ap_nss(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucNSS;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret) {
			DBGLOG(REQ, WARN, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);
			goto error;
		}
		ucNSS = (uint8_t) u4Parse;

		if ((ucNSS == 1) || (ucNSS == 2)) {
			prGlueInfo->prAdapter->rWifiVar.ucAp5gNSS = ucNSS;
			prGlueInfo->prAdapter->rWifiVar.ucAp2gNSS = ucNSS;

			DBGLOG(REQ, STATE,
				"Ap2gNSS = %d\n",
				prGlueInfo->prAdapter->rWifiVar.ucAp2gNSS);
		} else
			DBGLOG(REQ, WARN, "Invalid nss=%d\n",
				ucNSS);
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX SET_AP_NSS <value>\n");
	}

	return i4BytesWritten;

error:
	return -1;
}

int
__priv_set_ap(IN struct net_device *prNetDev,
	IN struct iw_request_info *prIwReqInfo,
	IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	uint32_t u4SubCmd = 0;
	uint16_t u2Cmd = 0;
	int32_t i4TotalFixLen = 1024;
	int32_t i4CmdFound = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;

	ASSERT(prNetDev);
	ASSERT(prIwReqData);
	if (!prNetDev || !prIwReqData) {
		DBGLOG(REQ, INFO,
			"invalid param(0x%p, 0x%p)\n",
		prNetDev, prIwReqData);
		return -EINVAL;
	}

	u2Cmd = prIwReqInfo->cmd;
	DBGLOG(REQ, INFO, "prIwReqInfo->cmd %x\n", u2Cmd);

	u4SubCmd = (uint32_t) prIwReqData->data.flags;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO,
			"invalid prGlueInfo(0x%p, 0x%p)\n",
			prNetDev,
			*((struct GLUE_INFO **) netdev_priv(prNetDev)));
		return -EINVAL;
	}

	DBGLOG(REQ, INFO, "prIwReqData->data.length %u\n",
		prIwReqData->data.length);

	ASSERT(IW_IS_GET(u2Cmd));
	if (prIwReqData->data.length != 0) {
		if (!kal_access_ok(VERIFY_READ, prIwReqData->data.pointer,
			prIwReqData->data.length)) {
			DBGLOG(REQ, INFO,
				"%s access_ok Read fail written = %d\n",
				__func__, i4BytesWritten);
			return -EFAULT;
		}
		if (copy_from_user(aucOidBuf,
			prIwReqData->data.pointer,
			prIwReqData->data.length)) {
			DBGLOG(REQ, INFO,
				"%s copy_form_user fail written = %d\n",
				__func__,
				prIwReqData->data.length);
				return -EFAULT;
		}
		/* prIwReqData->data.length include the terminate '\0' */
		aucOidBuf[prIwReqData->data.length - 1] = 0;
	}

	DBGLOG(REQ, INFO, "%s aucBuf %s\n", __func__, aucOidBuf);

	if (!pcExtra)
		goto exit;

	i4CmdFound = 1;
	switch (u2Cmd) {
	case IOC_AP_GET_STA_LIST:
	i4BytesWritten =
		priv_driver_set_ap_get_sta_list(
		prNetDev,
		pcExtra,
		i4TotalFixLen);
		break;
	case IOC_AP_SET_MAC_FLTR:
	i4BytesWritten =
		priv_driver_set_ap_set_mac_acl(
		prNetDev,
		aucOidBuf,
		i4TotalFixLen);
	  break;
	case IOC_AP_SET_CFG:
	i4BytesWritten =
		priv_driver_set_ap_set_cfg(
		prNetDev,
		aucOidBuf,
		i4TotalFixLen);
	  break;
	case IOC_AP_STA_DISASSOC:
	i4BytesWritten =
		priv_driver_set_ap_sta_disassoc(
		prNetDev,
		aucOidBuf,
		i4TotalFixLen);
	  break;
	case IOC_AP_SET_NSS:
	i4BytesWritten =
		priv_driver_set_ap_nss(
		prNetDev,
		aucOidBuf,
		i4TotalFixLen);
	  break;
	default:
		i4CmdFound = 0;
		break;
	}

	if (i4CmdFound == 0)
		DBGLOG(REQ, INFO,
			"Unknown driver command\n");

	if (i4BytesWritten >= 0) {
		if ((i4BytesWritten == 0) && (i4TotalFixLen > 0)) {
			/* reset the command buffer */
			pcExtra[0] = '\0';
		} else if (i4BytesWritten >= i4TotalFixLen) {
			DBGLOG(REQ, INFO,
				"%s: i4BytesWritten %d > i4TotalFixLen < %d\n",
				__func__, i4BytesWritten, i4TotalFixLen);
			i4BytesWritten = i4TotalFixLen;
		} else {
			pcExtra[i4BytesWritten] = '\0';
			i4BytesWritten++;
		}
	}

	DBGLOG(REQ, INFO, "%s i4BytesWritten = %d\n", __func__,
		i4BytesWritten);

exit:

	DBGLOG(REQ, INFO, "pcExtra done\n");

	if (i4BytesWritten >= 0)
		prIwReqData->data.length = i4BytesWritten;

	return 0;
}

int
priv_set_ap(IN struct net_device *prNetDev,
	IN struct iw_request_info *prIwReqInfo,
	IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
#if 0
	return compat_priv(prNetDev, prIwReqInfo,
		prIwReqData, pcExtra, __priv_set_ap);
#else
	return __priv_set_ap(prNetDev, prIwReqInfo,
		prIwReqData, pcExtra);
#endif
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
static int priv_driver_set_calbackup_test_drv_fw(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4GetInput;
	int32_t i4ArgNum = 2;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "%s\r\n", __func__);

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4GetInput);
		if (u4Ret)
			DBGLOG(RFTEST, INFO,
			       "priv_driver_set_calbackup_test_drv_fw Parsing Fail\n");

		if (u4GetInput == 0) {
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#0 : Reset All Cal Data in Driver.\n");
			/* (New Flow 20160720) Step 0 : Reset All Cal Data
			 *                              Structure
			 */
			memset(&g_rBackupCalDataAllV2, 1,
			       sizeof(struct RLM_CAL_RESULT_ALL_V2));
			g_rBackupCalDataAllV2.u4MagicNum1 = 6632;
			g_rBackupCalDataAllV2.u4MagicNum2 = 6632;
		} else if (u4GetInput == 1) {
			DBGLOG(RFTEST, INFO,
			       "CMD#1 : Trigger FW Do All Cal.\n");
			/* Step 1 : Trigger All Cal Function */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 1, 2, 0);
			DBGLOG(RFTEST, INFO,
			       "Trigger FW Do All Cal, rStatus = 0x%08x\n",
			       rStatus);
		} else if (u4GetInput == 2) {
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#2 : Get Thermal Temp from FW.\n");
			/* (New Flow 20160720) Step 2 : Get Thermal Temp from
			 *                              FW
			 */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 0, 0, 0);
			DBGLOG(RFTEST, INFO,
			       "Get Thermal Temp from FW, rStatus = 0x%08x\n",
			       rStatus);

		} else if (u4GetInput == 3) {
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#3 : Get Cal Data Size from FW.\n");
			/* (New Flow 20160720) Step 3 : Get Cal Data Size from
			 *                              FW
			 */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 0, 1, 0);
			DBGLOG(RFTEST, INFO,
			       "Get Rom Cal Data Size, rStatus = 0x%08x\n",
			       rStatus);

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 0, 1, 1);
			DBGLOG(RFTEST, INFO,
			       "Get Ram Cal Data Size, rStatus = 0x%08x\n",
			       rStatus);

		} else if (u4GetInput == 4) {
#if 1
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#4 : Print Cal Data in FW (Ram) (Part 1 - [0]~[3327]).\n");
			/* Debug Use : Print Cal Data in FW (Ram) */
			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 4, 6, 1);
			DBGLOG(RFTEST, INFO,
			       "Print Cal Data in FW (Ram), rStatus = 0x%08x\n",
			       rStatus);
#else		/* For Temp Use this Index */
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#4 : Get Cal Data from FW (Rom). Start!!!!!!!!!!!\n");
			DBGLOG(RFTEST, INFO, "Thermal Temp = %d\n",
			       g_rBackupCalDataAllV2.u4ThermalInfo);
			DBGLOG(RFTEST, INFO, "Total Length (Rom) = %d\n",
				g_rBackupCalDataAllV2.u4ValidRomCalDataLength);
			/* (New Flow 20160720) Step 3 : Get Cal Data from FW */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 2, 4, 0);
			DBGLOG(RFTEST, INFO,
			       "Get Cal Data from FW (Rom), rStatus = 0x%08x\n",
			       rStatus);
#endif
		} else if (u4GetInput == 5) {
#if 1
			DBGLOG(RFTEST, INFO,
				"(New Flow) CMD#5 : Print RAM Cal Data in Driver (Part 1 - [0]~[3327]).\n");
			DBGLOG(RFTEST, INFO,
			       "==================================================================\n");
			/* RFTEST_INFO_LOGDUMP32(
			 *     &(g_rBackupCalDataAllV2.au4RamCalData[0]),
			 *     3328*sizeof(uint32_t));
			 */
			DBGLOG(RFTEST, INFO,
			       "==================================================================\n");
			DBGLOG(RFTEST, INFO,
			       "Dumped Ram Cal Data Szie : %d bytes\n",
			       3328*sizeof(uint32_t));
			DBGLOG(RFTEST, INFO,
			       "Total Ram Cal Data Szie : %d bytes\n",
				g_rBackupCalDataAllV2.u4ValidRamCalDataLength);
			DBGLOG(RFTEST, INFO,
			       "==================================================================\n");
#else		/* For Temp Use this Index */
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#5 : Get Cal Data from FW (Ram). Start!!!!!!!!!!!\n");
			DBGLOG(RFTEST, INFO, "Thermal Temp = %d\n",
			       g_rBackupCalDataAllV2.u4ThermalInfo);
			DBGLOG(RFTEST, INFO, "Total Length (Ram) = %d\n",
				g_rBackupCalDataAllV2.u4ValidRamCalDataLength);
			/* (New Flow 20160720) Step 3 : Get Cal Data from FW */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 2, 4, 1);
			DBGLOG(RFTEST, INFO,
			       "Get Cal Data from FW (Ram), rStatus = 0x%08x\n",
			       rStatus);
#endif
		} else if (u4GetInput == 6) {
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#6 : Print ROM Cal Data in Driver.\n");
			DBGLOG(RFTEST, INFO,
			       "==================================================================\n");
			/* RFTEST_INFO_LOGDUMP32(
			 *     &(g_rBackupCalDataAllV2.au4RomCalData[0]),
			 *     g_rBackupCalDataAllV2.u4ValidRomCalDataLength);
			 */
			DBGLOG(RFTEST, INFO,
			       "==================================================================\n");
			DBGLOG(RFTEST, INFO,
			       "Total Rom Cal Data Szie : %d bytes\n",
			       g_rBackupCalDataAllV2.u4ValidRomCalDataLength);
			DBGLOG(RFTEST, INFO,
			       "==================================================================\n");
		} else if (u4GetInput == 7) {
			DBGLOG(RFTEST, INFO,
				"(New Flow) CMD#7 : Print RAM Cal Data in Driver (Part 2 - [3328]~[6662]).\n");
			DBGLOG(RFTEST, INFO,
			       "==================================================================\n");
			/* RFTEST_INFO_LOGDUMP32(
			 *     &(g_rBackupCalDataAllV2.au4RamCalData[3328]),
			 *     (g_rBackupCalDataAllV2.u4ValidRamCalDataLength -
			 *     3328*sizeof(uint32_t)));
			 */
			DBGLOG(RFTEST, INFO,
			       "==================================================================\n");
			DBGLOG(RFTEST, INFO,
			       "Dumped Ram Cal Data Szie : %d bytes\n",
			       (g_rBackupCalDataAllV2.u4ValidRamCalDataLength -
			       3328*sizeof(uint32_t)));
			DBGLOG(RFTEST, INFO,
			       "Total Ram Cal Data Szie : %d bytes\n",
			       g_rBackupCalDataAllV2.u4ValidRamCalDataLength);
			DBGLOG(RFTEST, INFO,
			       "==================================================================\n");
		} else if (u4GetInput == 8) {
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#8 : Print Cal Data in FW (Rom).\n");
			/* Debug Use : Print Cal Data in FW (Rom) */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 4, 6, 0);
			DBGLOG(RFTEST, INFO,
			       "Print Cal Data in FW (Rom), rStatus = 0x%08x\n",
			       rStatus);

		} else if (u4GetInput == 9) {
			DBGLOG(RFTEST, INFO,
				"(New Flow) CMD#9 : Print Cal Data in FW (Ram) (Part 2 - [3328]~[6662]).\n");
			/* Debug Use : Print Cal Data in FW (Ram) */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 4, 6, 2);
			DBGLOG(RFTEST, INFO,
			       "Print Cal Data in FW (Ram), rStatus = 0x%08x\n",
			       rStatus);

		} else if (u4GetInput == 10) {
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#10 : Send Cal Data to FW (Rom).\n");
			/* Send Cal Data to FW (Rom) */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 3, 5, 0);
			DBGLOG(RFTEST, INFO,
			       "Send Cal Data to FW (Rom), rStatus = 0x%08x\n",
			       rStatus);

		} else if (u4GetInput == 11) {
			DBGLOG(RFTEST, INFO,
			       "(New Flow) CMD#11 : Send Cal Data to FW (Ram).\n");
			/* Send Cal Data to FW (Ram) */

			rStatus = rlmCalBackup(prGlueInfo->prAdapter, 3, 5, 1);
			DBGLOG(RFTEST, INFO,
			       "Send Cal Data to FW (Ram), rStatus = 0x%08x\n",
			       rStatus);

		}
	}

	return i4BytesWritten;
}				/* priv_driver_set_calbackup_test_drv_fw */
#endif

#if CFG_WOW_SUPPORT
static int priv_driver_set_wow(IN struct net_device *prNetDev,
			       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct WOW_CTRL *pWOW_CTRL = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret = 0;
	uint32_t Enable = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
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
	DBGLOG(INIT, INFO, "interface %d\n",
	       pWOW_CTRL->astWakeHif[0].ucWakeupHif);
	DBGLOG(INIT, INFO, "gpio_pin %d\n",
	       pWOW_CTRL->astWakeHif[0].ucGpioPin);
	DBGLOG(INIT, INFO, "gpio_level 0x%x\n",
	       pWOW_CTRL->astWakeHif[0].ucTriggerLvl);
	DBGLOG(INIT, INFO, "gpio_timer %d\n",
	       pWOW_CTRL->astWakeHif[0].u4GpioInterval);
	kalWowProcess(prGlueInfo, Enable);

	return 0;
}

static int priv_driver_set_wow_enable(IN struct net_device *prNetDev,
				      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct WOW_CTRL *pWOW_CTRL = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret = 0;
	uint8_t ucEnable = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
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

static int priv_driver_set_wow_par(IN struct net_device *prNetDev,
				   IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct WOW_CTRL *pWOW_CTRL = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	uint8_t	ucWakeupHif = 0, GpioPin = 0, ucGpioLevel = 0, ucBlockCount = 0,
		ucScenario = 0;
	uint32_t u4GpioTimer = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc > 3) {

		u4Ret = kalkStrtou8(apcArgv[1], 0, &ucWakeupHif);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n",
			       u4Ret);
		pWOW_CTRL->astWakeHif[0].ucWakeupHif = ucWakeupHif;

		u4Ret = kalkStrtou8(apcArgv[2], 0, &GpioPin);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse GpioPin error u4Ret=%d\n",
			       u4Ret);
		pWOW_CTRL->astWakeHif[0].ucGpioPin = GpioPin;

		u4Ret = kalkStrtou8(apcArgv[3], 0, &ucGpioLevel);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse Gpio level error u4Ret=%d\n",
			       u4Ret);
		pWOW_CTRL->astWakeHif[0].ucTriggerLvl = ucGpioLevel;

		u4Ret = kalkStrtou32(apcArgv[4], 0, &u4GpioTimer);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse u4GpioTimer error u4Ret=%d\n",
			       u4Ret);
		pWOW_CTRL->astWakeHif[0].u4GpioInterval = u4GpioTimer;

		u4Ret = kalkStrtou8(apcArgv[5], 0, &ucScenario);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucScenario error u4Ret=%d\n",
			       u4Ret);
		pWOW_CTRL->ucScenarioId = ucScenario;

		u4Ret = kalkStrtou8(apcArgv[6], 0, &ucBlockCount);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse ucBlockCnt error u4Ret=%d\n",
			       u4Ret);
		pWOW_CTRL->ucBlockCount = ucBlockCount;

		DBGLOG(INIT, INFO, "gpio_scenario%d\n",
		       pWOW_CTRL->ucScenarioId);
		DBGLOG(INIT, INFO, "interface %d\n",
		       pWOW_CTRL->astWakeHif[0].ucWakeupHif);
		DBGLOG(INIT, INFO, "gpio_pin %d\n",
		       pWOW_CTRL->astWakeHif[0].ucGpioPin);
		DBGLOG(INIT, INFO, "gpio_level %d\n",
		       pWOW_CTRL->astWakeHif[0].ucTriggerLvl);
		DBGLOG(INIT, INFO, "gpio_timer %d\n",
		       pWOW_CTRL->astWakeHif[0].u4GpioInterval);

		return 0;
	} else
		return -1;


}

static int priv_driver_set_wow_udpport(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct WOW_CTRL *pWOW_CTRL = NULL;
	int32_t i4Argc = 0;
	int8_t *apcPortArgv[WLAN_CFG_ARGV_MAX_LONG] = { 0 }; /* to input 20 port
							      */
	int32_t u4Ret = 0, ii;
	uint8_t	ucVer = 0, ucCount;
	uint16_t u2Port = 0;
	uint16_t *pausPortArry;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgumentLong(pcCommand, &i4Argc, apcPortArgv);
	DBGLOG(REQ, WARN, "argc is %i\n", i4Argc);

	/* example: set_wow_udp 0 5353,8080 (set) */
	/* example: set_wow_udp 1 (clear) */

	if (i4Argc >= 3) {

		/* Pick Max */
		ucCount = ((i4Argc - 2) > MAX_TCP_UDP_PORT) ? MAX_TCP_UDP_PORT :
			  (i4Argc - 2);
		DBGLOG(PF, INFO, "UDP ucCount=%d\n", ucCount);

		u4Ret = kalkStrtou8(apcPortArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n",
			       u4Ret);
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
				DBGLOG(PF, ERROR,
				       "parse u2Port error u4Ret=%d\n", u4Ret);
				return -1;
			}

			pausPortArry[ii] = u2Port;
			DBGLOG(PF, INFO, "ucPort=%d, idx=%d\n", u2Port, ii);
		}

		return 0;
	} else if (i4Argc == 2) {

		u4Ret = kalkStrtou8(apcPortArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n",
			       u4Ret);
			return -1;
		}

		if (ucVer == 0) {
			kalMemZero(prGlueInfo->prAdapter->rWowCtrl.stWowPort
				.ausIPv4UdpPort,
				sizeof(uint16_t) * MAX_TCP_UDP_PORT);
			prGlueInfo->prAdapter->rWowCtrl.stWowPort
				.ucIPv4UdpPortCnt = 0;
		} else {
			kalMemZero(prGlueInfo->prAdapter->rWowCtrl.stWowPort
				.ausIPv6UdpPort,
				sizeof(uint16_t) * MAX_TCP_UDP_PORT);
			prGlueInfo->prAdapter->rWowCtrl.stWowPort
				.ucIPv6UdpPortCnt = 0;
		}

		return 0;
	} else
		return -1;

}

static int priv_driver_set_wow_tcpport(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct WOW_CTRL *pWOW_CTRL = NULL;
	int32_t i4Argc = 0;
	int8_t *apcPortArgv[WLAN_CFG_ARGV_MAX_LONG] = { 0 }; /* to input 20 port
							      */
	int32_t u4Ret = 0, ii;
	uint8_t	ucVer = 0, ucCount;
	uint16_t u2Port = 0;
	uint16_t *pausPortArry;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgumentLong(pcCommand, &i4Argc, apcPortArgv);
	DBGLOG(REQ, WARN, "argc is %i\n", i4Argc);

	/* example: set_wow_tcp 0 5353,8080 (Set) */
	/* example: set_wow_tcp 1 (clear) */

	if (i4Argc >= 3) {

		/* Pick Max */
		ucCount = ((i4Argc - 2) > MAX_TCP_UDP_PORT) ? MAX_TCP_UDP_PORT :
			  (i4Argc - 2);
		DBGLOG(PF, INFO, "TCP ucCount=%d\n", ucCount);

		u4Ret = kalkStrtou8(apcPortArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n",
			       u4Ret);
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
				DBGLOG(PF, ERROR,
				       "parse u2Port error u4Ret=%d\n", u4Ret);
				return -1;
			}

			pausPortArry[ii] = u2Port;
			DBGLOG(PF, INFO, "ucPort=%d, idx=%d\n", u2Port, ii);
		}

		return 0;
	} else if (i4Argc == 2) {

		u4Ret = kalkStrtou8(apcPortArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, LOUD, "parse ucWakeupHif error u4Ret=%d\n",
			       u4Ret);
			return -1;
		}

		if (ucVer == 0) {
			kalMemZero(
				prGlueInfo->prAdapter->rWowCtrl.stWowPort
				.ausIPv4UdpPort,
				sizeof(uint16_t) * MAX_TCP_UDP_PORT);
			prGlueInfo->prAdapter->rWowCtrl.stWowPort
				.ucIPv4UdpPortCnt = 0;
		} else {
			kalMemZero(
				prGlueInfo->prAdapter->rWowCtrl.stWowPort
				.ausIPv6UdpPort,
				sizeof(uint16_t) * MAX_TCP_UDP_PORT);
			prGlueInfo->prAdapter->rWowCtrl.stWowPort
				.ucIPv6UdpPortCnt = 0;
		}

		return 0;
	} else
		return -1;

}

static int priv_driver_get_wow_port(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct WOW_CTRL *pWOW_CTRL = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0, ii;
	uint8_t	ucVer = 0, ucProto = 0;
	uint16_t ucCount;
	uint16_t *pausPortArry;
	int8_t *aucIp[2] = {"IPv4", "IPv6"};
	int8_t *aucProto[2] = {"UDP", "TCP"};

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
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
			DBGLOG(REQ, LOUD, "parse argc[1] error u4Ret=%d\n",
			       u4Ret);

		/* 0=UDP, 1=TCP */
		u4Ret = kalkStrtou8(apcArgv[2], 0, &ucProto);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse argc[2] error u4Ret=%d\n",
			       u4Ret);

		if (ucVer > 1)
			ucVer = 0;

		if (ucProto > 1)
			ucProto = 0;

		if (ucVer == 0) {
			if (ucProto == 0) {
				/* IPv4/UDP */
				ucCount = pWOW_CTRL->stWowPort.ucIPv4UdpPortCnt;
				pausPortArry =
					pWOW_CTRL->stWowPort.ausIPv4UdpPort;
			} else {
				/* IPv4/TCP */
				ucCount = pWOW_CTRL->stWowPort.ucIPv4TcpPortCnt;
				pausPortArry =
					pWOW_CTRL->stWowPort.ausIPv4TcpPort;
			}
		} else {
			if (ucProto == 0) {
				/* IPv6/UDP */
				ucCount = pWOW_CTRL->stWowPort.ucIPv6UdpPortCnt;
				pausPortArry =
					pWOW_CTRL->stWowPort.ausIPv6UdpPort;
			} else {
				/* IPv6/TCP */
				ucCount = pWOW_CTRL->stWowPort.ucIPv6TcpPortCnt;
				pausPortArry =
					pWOW_CTRL->stWowPort.ausIPv6TcpPort;
			}
		}

		/* Dunp Port */
		for (ii = 0; ii < ucCount; ii++)
			DBGLOG(PF, INFO, "ucPort=%d, idx=%d\n",
			       pausPortArry[ii], ii);


		DBGLOG(PF, INFO, "[%s/%s] count:%d\n", aucIp[ucVer],
		       aucProto[ucProto], ucCount);

		return 0;
	} else
		return -1;

}
#endif

static int priv_driver_set_adv_pws(IN struct net_device *prNetDev,
				   IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret = 0;
	uint8_t ucAdvPws = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	u4Ret = kalkStrtou8(apcArgv[1], 0, &ucAdvPws);

	if (u4Ret)
		DBGLOG(REQ, LOUD, "parse bEnable error u4Ret=%d\n",
		       u4Ret);

	prGlueInfo->prAdapter->rWifiVar.ucAdvPws = ucAdvPws;

	DBGLOG(INIT, INFO, "AdvPws:%d\n",
	       prGlueInfo->prAdapter->rWifiVar.ucAdvPws);

	return 0;

}

static int priv_driver_set_mdtim(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret = 0;
	uint8_t ucMultiDtim = 0, ucVer = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	/* iwpriv wlan0 driver "set_mdtim 1 3 */
	if (i4Argc >= 3) {

		u4Ret = kalkStrtou8(apcArgv[1], 0, &ucVer);
		if (u4Ret) {
			DBGLOG(REQ, ERROR, "parse apcArgv1 error u4Ret=%d\n",
			       u4Ret);
			return -1;
		}

		u4Ret = kalkStrtou8(apcArgv[2], 0, &ucMultiDtim);
		if (u4Ret) {
			DBGLOG(REQ, ERROR, "parse apcArgv2 error u4Ret=%d\n",
			       u4Ret);
			return -1;
		}

		if (ucVer == 0) {
			prGlueInfo->prAdapter->rWifiVar.ucWowOnMdtim =
								ucMultiDtim;
			DBGLOG(REQ, INFO, "WOW On MDTIM:%d\n",
			       prGlueInfo->prAdapter->rWifiVar.ucWowOnMdtim);
		} else {
			prGlueInfo->prAdapter->rWifiVar.ucWowOffMdtim =
								ucMultiDtim;
			DBGLOG(REQ, INFO, "WOW Off MDTIM:%d\n",
			       prGlueInfo->prAdapter->rWifiVar.ucWowOffMdtim);
		}
	}

	return 0;

}

int priv_driver_set_suspend_mode(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	u_int8_t fgEnable;
	uint32_t u4Enable = 0;
	int32_t u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		/* fgEnable = (kalStrtoul(apcArgv[1], NULL, 0) == 1) ? TRUE :
		 *            FALSE;
		 */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Enable);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse u4Enable error u4Ret=%d\n",
			       u4Ret);
		if (u4Enable == 1)
			fgEnable = TRUE;
		else
			fgEnable = FALSE;

		if (prGlueInfo->fgIsInSuspendMode == fgEnable) {
			DBGLOG(REQ, INFO,
			       "%s: Already in suspend mode [%u], SKIP!\n",
			       __func__, fgEnable);
			return 0;
		}

		DBGLOG(REQ, INFO, "%s: Set suspend mode [%u]\n", __func__,
		       fgEnable);

		prGlueInfo->fgIsInSuspendMode = fgEnable;

		wlanSetSuspendMode(prGlueInfo, fgEnable);
		p2pSetSuspendMode(prGlueInfo, fgEnable);
	}

	return 0;
}

#if CFG_SUPPORT_SNIFFER
int priv_driver_set_monitor(IN struct net_device *prNetDev, IN char *pcCommand,
			    IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct PARAM_CUSTOM_MONITOR_SET_STRUCT rMonitorSetInfo;
	uint8_t ucEnable = 0;
	uint8_t ucPriChannel = 0;
	uint8_t ucChannelWidth = 0;
	uint8_t ucExt = 0;
	uint8_t ucSco = 0;
	uint8_t ucChannelS1 = 0;
	uint8_t ucChannelS2 = 0;
	u_int8_t fgIsLegalChannel = FALSE;
	u_int8_t fgError = FALSE;
	u_int8_t fgEnable = FALSE;
	enum ENUM_BAND eBand = BAND_NULL;
	uint32_t u4Parse = 0;
	int32_t u4Ret = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc >= 5) {
		/* ucEnable = (uint8_t) (kalStrtoul(apcArgv[1], NULL, 0));
		 * ucPriChannel = (uint8_t) (kalStrtoul(apcArgv[2], NULL, 0));
		 * ucChannelWidth = (uint8_t) (kalStrtoul(apcArgv[3], NULL, 0));
		 * ucExt = (uint8_t) (kalStrtoul(apcArgv[4], NULL, 0));
		 */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);
		ucEnable = (uint8_t) u4Parse;
		u4Ret = kalkStrtou32(apcArgv[2], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);
		ucPriChannel = (uint8_t) u4Parse;
		u4Ret = kalkStrtou32(apcArgv[3], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);
		ucChannelWidth = (uint8_t) u4Parse;
		u4Ret = kalkStrtou32(apcArgv[4], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);
		ucExt = (uint8_t) u4Parse;

		eBand = (ucPriChannel <= 14) ? BAND_2G4 : BAND_5G;
		fgIsLegalChannel = rlmDomainIsLegalChannel(prAdapter, eBand,
							   ucPriChannel);

		if (fgIsLegalChannel == FALSE) {
			i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
						  "Illegal primary channel %d",
						  ucPriChannel);
			return i4BytesWritten;
		}

		switch (ucChannelWidth) {
		case 160:
			ucChannelWidth = (uint8_t) CW_160MHZ;
			ucSco = (uint8_t) CHNL_EXT_SCN;

			if (ucPriChannel >= 36 && ucPriChannel <= 64)
				ucChannelS2 = 50;
			else if (ucPriChannel >= 100 && ucPriChannel <= 128)
				ucChannelS2 = 114;
			else
				fgError = TRUE;
			break;

		case 80:
			ucChannelWidth = (uint8_t) CW_80MHZ;
			ucSco = (uint8_t) CHNL_EXT_SCN;

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
			ucChannelWidth = (uint8_t) CW_20_40MHZ;
			ucSco = (ucExt) ? (uint8_t) CHNL_EXT_SCA :
				(uint8_t) CHNL_EXT_SCB;
			break;

		case 20:
			ucChannelWidth = (uint8_t) CW_20_40MHZ;
			ucSco = (uint8_t) CHNL_EXT_SCN;
			break;

		default:
			fgError = TRUE;
			break;
		}

		if (fgError) {
			i4BytesWritten =
			    kalSnprintf(pcCommand, i4TotalLen,
				     "Invalid primary channel %d with bandwidth %d",
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

		rStatus = kalIoctl(prGlueInfo, wlanoidSetMonitor,
				   &rMonitorSetInfo, sizeof(rMonitorSetInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);

		i4BytesWritten =
		    kalSnprintf(pcCommand, i4TotalLen, "set monitor config %s",
			     (rStatus == WLAN_STATUS_SUCCESS) ?
			     "success" : "fail");

		return i4BytesWritten;
	}

	i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
				  "monitor [Enable][PriChannel][ChannelWidth][Sco]");

	return i4BytesWritten;
}
#endif

int priv_driver_set_bf(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucBfEnable;
	/*UINT_8 ucBssIndex;*/
	/*P_BSS_INFO_T prBssInfo;*/


	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucBfEnable = (uint8_t) u4Parse;
		prGlueInfo->prAdapter->rWifiVar.ucStaHtBfee = ucBfEnable;
		prGlueInfo->prAdapter->rWifiVar.ucStaVhtBfee = ucBfEnable;
#if (CFG_SUPPORT_802_11AX == 1)
		prGlueInfo->prAdapter->rWifiVar.ucStaHeBfee = ucBfEnable;
#endif /* CFG_SUPPORT_802_11AX == 1 */
		prGlueInfo->prAdapter->rWifiVar.ucStaVhtMuBfee = ucBfEnable;
		DBGLOG(REQ, ERROR, "ucBfEnable = %d\n", ucBfEnable);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver SET_NSS <nss>\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_nss(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucNSS;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucNSS = (uint8_t) u4Parse;
		prGlueInfo->prAdapter->rWifiVar.ucNSS = ucNSS;
		DBGLOG(REQ, LOUD, "ucNSS = %d\n", ucNSS);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver SET_NSS <nss>\n");
	}

	return i4BytesWritten;
}


int priv_driver_set_amsdu_tx(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucAmsduTx;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucAmsduTx = (uint8_t) u4Parse;
		prGlueInfo->prAdapter->rWifiVar.ucAmsduInAmpduTx = ucAmsduTx;
		prGlueInfo->prAdapter->rWifiVar.ucHtAmsduInAmpduTx = ucAmsduTx;
		prGlueInfo->prAdapter->rWifiVar.ucVhtAmsduInAmpduTx = ucAmsduTx;
#if (CFG_SUPPORT_802_11AX == 1)
		prGlueInfo->prAdapter->rWifiVar.ucHeAmsduInAmpduTx = ucAmsduTx;
#endif
		DBGLOG(REQ, LOUD, "ucAmsduTx = %d\n", ucAmsduTx);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver SET_NSS <nss>\n");
	}

	return i4BytesWritten;
}


int priv_driver_set_amsdu_rx(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucAmsduRx;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucAmsduRx = (uint8_t) u4Parse;
		prGlueInfo->prAdapter->rWifiVar.ucAmsduInAmpduRx = ucAmsduRx;
		prGlueInfo->prAdapter->rWifiVar.ucHtAmsduInAmpduRx = ucAmsduRx;
		prGlueInfo->prAdapter->rWifiVar.ucVhtAmsduInAmpduRx = ucAmsduRx;
#if (CFG_SUPPORT_802_11AX == 1)
		prGlueInfo->prAdapter->rWifiVar.ucHeAmsduInAmpduRx = ucAmsduRx;
#endif
		DBGLOG(REQ, LOUD, "ucAmsduRx = %d\n", ucAmsduRx);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver SET_NSS <nss>\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_ampdu_tx(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucAmpduEnable;
	/*UINT_8 ucBssIndex;*/
	/*P_BSS_INFO_T prBssInfo;*/


	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));



	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucAmpduEnable = (uint8_t) u4Parse;
		prGlueInfo->prAdapter->rWifiVar.ucAmpduTx = ucAmpduEnable;
		DBGLOG(REQ, ERROR, "ucAmpduTx = %d\n", ucAmpduEnable);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlan0 driver SET_AMPDU_TX <en>\n");
		DBGLOG(INIT, ERROR, "<en> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_ampdu_rx(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucAmpduEnable;
	/*UINT_8 ucBssIndex;*/
	/*P_BSS_INFO_T prBssInfo;*/


	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));



	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucAmpduEnable = (uint8_t) u4Parse;
		prGlueInfo->prAdapter->rWifiVar.ucAmpduRx = ucAmpduEnable;
		DBGLOG(REQ, ERROR, "ucAmpduRx = %d\n",
			prGlueInfo->prAdapter->rWifiVar.ucAmpduRx);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlan0 driver SET_AMPDU_RX <en>\n");
		DBGLOG(INIT, ERROR, "<en> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_qos(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucQoSEnable;
	/*UINT_8 ucBssIndex;*/
	/*P_BSS_INFO_T prBssInfo;*/


	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));



	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
				u4Ret);

		ucQoSEnable = (uint8_t) u4Parse;
		prGlueInfo->prAdapter->rWifiVar.ucQoS = ucQoSEnable;
		DBGLOG(REQ, ERROR, "ucQoS = %d\n",
			prGlueInfo->prAdapter->rWifiVar.ucQoS);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver SET_QOS <enable>\n");
		DBGLOG(INIT, ERROR, "<enable> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

#if (CFG_SUPPORT_802_11AX == 1)
int priv_driver_muedca_override(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucMuEdcaOverride;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucMuEdcaOverride = (uint8_t) u4Parse;
#if (CFG_SUPPORT_802_11AX == 1)
		prGlueInfo->prAdapter->fgMuEdcaOverride = ucMuEdcaOverride;
#endif
		DBGLOG(REQ, LOUD, "ucMuEdcaOverride = %d\n", ucMuEdcaOverride);
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX driver MUEDCA_OVERRIDE <val>\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_mcsmap(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucTxMcsMap;
	struct ADAPTER *prAdapter = NULL;
	/*UINT_8 ucBssIndex;*/
	/*P_BSS_INFO_T prBssInfo;*/


	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));



	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucTxMcsMap = (uint8_t) u4Parse;
#if (CFG_SUPPORT_802_11AX == 1)
		if (ucTxMcsMap >= 0 && ucTxMcsMap <= 2) {
			prAdapter = prGlueInfo->prAdapter;
			prAdapter->ucMcsMapSetFromSigma = ucTxMcsMap;

			DBGLOG(REQ, ERROR, "ucMcsMapSetFromSigma = %d\n",
				prGlueInfo->prAdapter->ucMcsMapSetFromSigma);

			prGlueInfo->prAdapter->fgMcsMapBeenSet = TRUE;
		} else {
			prGlueInfo->prAdapter->fgMcsMapBeenSet = FALSE;
		}
#endif
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlan0 driver SET_TX_MCSMAP <en>\n");
		DBGLOG(INIT, ERROR, "<en> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_ba_size(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint16_t u2HeBaSize;
	/*UINT_8 ucBssIndex;*/
	/*P_BSS_INFO_T prBssInfo;*/


	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));



	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		u2HeBaSize = (uint16_t) u4Parse;

		prGlueInfo->prAdapter->rWifiVar.u2TxHeBaSize = u2HeBaSize;
		prGlueInfo->prAdapter->rWifiVar.u2RxHeBaSize = u2HeBaSize;
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver SET_BA_SIZE\n");
		DBGLOG(INIT, ERROR, "<enable> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

/* This command is for sigma to disable TpTestMode. */
int priv_driver_set_tp_test_mode(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucTpTestMode;
	/*UINT_8 ucBssIndex;*/
	/*P_BSS_INFO_T prBssInfo;*/


	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));



	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucTpTestMode = (uint8_t) u4Parse;

		prGlueInfo->prAdapter->rWifiVar.ucTpTestMode = ucTpTestMode;
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver SET_TP_TEST_MODE\n");
		DBGLOG(INIT, ERROR, "<enable> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

/* This command is for sigma to disable TxPPDU. */
int priv_driver_set_tx_ppdu(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	struct ADAPTER *prAdapter = NULL;
	struct STA_RECORD *prStaRec;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	prStaRec = cnmGetStaRecByIndex(prAdapter, 0);

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		if (u4Parse) {
			/* HE_SU is allowed. */
			prAdapter->fgTxPPDU = TRUE;
			NIC_TX_PPDU_ENABLE(prAdapter);
		} else {
			/* HE_SU is not allowed. */
			prAdapter->fgTxPPDU = FALSE;
			if (prStaRec && prStaRec->fgIsTxAllowed)
				NIC_TX_PPDU_DISABLE(prAdapter);
		}

		DBGLOG(REQ, STATE, "fgTxPPDU is %d\n", prAdapter->fgTxPPDU);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver TX_PPDU\n");
		DBGLOG(INIT, ERROR, "<enable> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

/* This command is for sigma to disable LDPC capability. */
int priv_driver_set_ldpc(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);
		if (u4Parse) {
			/* LDPC is enabled. */
			prAdapter->rWifiVar.ucTxLdpc = TRUE;
			prAdapter->rWifiVar.ucRxLdpc = TRUE;
		} else {
			/* LDPC is disabled. */
			prAdapter->rWifiVar.ucTxLdpc = FALSE;
			prAdapter->rWifiVar.ucRxLdpc = FALSE;
		}

		DBGLOG(REQ, STATE, "prAdapter->rWifiVar.ucTxLdpc is %d\n",
			prAdapter->rWifiVar.ucTxLdpc);
		DBGLOG(REQ, STATE, "prAdapter->rWifiVar.ucRxLdpc is %d\n",
			prAdapter->rWifiVar.ucRxLdpc);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver TX_PPDU\n");
		DBGLOG(INIT, ERROR, "<enable> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

/* This command is for sigma to force tx amsdu. */
int priv_driver_set_tx_force_amsdu(IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);
		if (u4Parse) {
			/* forceAmsdu is enabled. */
			prAdapter->rWifiVar.ucHeCertForceAmsdu = TRUE;
		} else {
			/* forceAmsdu is disabled. */
			prAdapter->rWifiVar.ucHeCertForceAmsdu = FALSE;
		}

		DBGLOG(REQ, STATE,
			"prAdapter->rWifiVar.ucHeCertForceAmsdu is %d\n",
			prAdapter->rWifiVar.ucHeCertForceAmsdu);
	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver FORCE_AMSDU_TX %d\n");
		DBGLOG(INIT, ERROR, "<enable> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}




/* This command is for sigma to change OM CH BW. */
int priv_driver_set_om_ch_bw(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		DBGLOG(REQ, STATE,
			"priv_driver_set_om_ch_bw:: ch bw = %d\n", u4Parse);
		if (u4Parse <= CH_BW_160)
			HE_SET_HTC_HE_OM_CH_WIDTH(
				prAdapter->u4HeHtcOM, u4Parse);
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX driver TX_ACTION <number>\n");
		DBGLOG(INIT, ERROR, "<number> action frame count.\n");
	}

	return i4BytesWritten;
}

/* This command is for sigma to change OM RX NSS. */
int priv_driver_set_om_rx_nss(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		DBGLOG(REQ, STATE,
			"priv_driver_set_om_rx_nss:: rx nss = %d\n", u4Parse);
		HE_SET_HTC_HE_OM_RX_NSS(prAdapter->u4HeHtcOM, u4Parse);
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX driver TX_ACTION <number>\n");
		DBGLOG(INIT, ERROR, "<number> action frame count.\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_om_tx_nss(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		DBGLOG(REQ, STATE,
			"priv_driver_set_om_tx_nss:: tx nss = %d\n", u4Parse);
		HE_SET_HTC_HE_OM_TX_NSTS(prAdapter->u4HeHtcOM, u4Parse);
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX driver SET_OM_TXNSTS <number>\n");
		DBGLOG(INIT, ERROR, "<number> action frame count.\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_om_mu_dis(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	struct ADAPTER *prAdapter = NULL;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		DBGLOG(REQ, STATE,
			"priv_driver_set_om_mu_dis:: disable = %d\n", u4Parse);
		HE_SET_HTC_HE_OM_UL_MU_DISABLE(prAdapter->u4HeHtcOM, u4Parse);
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX driver SET_OM_MU_DISABLE <number>\n");
		DBGLOG(INIT, ERROR, "<number> action frame count.\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_tx_om_packet(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	struct ADAPTER *prAdapter = NULL;
	int32_t index;
	struct STA_RECORD *prStaRec = NULL;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		DBGLOG(REQ, STATE,
			"tx om packet:: Send %d htc null frame\n",
			u4Parse);
		if (u4Parse) {
			prStaRec = cnmGetStaRecByIndex(prAdapter, 0);
			if (prStaRec != NULL) {
				for (index = 0; index < u4Parse; index++)
					heRlmSendHtcNullFrame(prAdapter,
						prStaRec, 7, NULL);
			}
		}
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX driver TX_ACTION <number>\n");
		DBGLOG(INIT, ERROR, "<number> action frame count.\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_tx_cck_1m_pwr(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	struct ADAPTER *prAdapter = NULL;
	uint8_t pwr;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;


	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		DBGLOG(REQ, STATE,
			"priv_driver_set_tx_cck_1m_pwr:: set cck pwr %d\n",
			u4Parse);

		if (u4Parse) {
			pwr = u4Parse;

			wlanSendSetQueryCmd(prAdapter,
			CMD_ID_SET_CCK_1M_PWR,
			TRUE,
			FALSE,
			FALSE,
			NULL,
			NULL,
			sizeof(uint8_t),
			(uint8_t *)&pwr, NULL, 0);
		}
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX driver TX_CCK_1M_PWR <pwr>\n");
		DBGLOG(INIT, ERROR, "<pwr> power of CCK 1M.\n");
	}

	return i4BytesWritten;
}

#endif /* CFG_SUPPORT_802_11AX == 1 */


static int priv_driver_get_sta_index(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	int32_t i4BytesWritten = 0, i4Argc = 0;
	uint8_t ucStaIdx, ucWlanIndex = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint8_t aucMacAddr[MAC_ADDR_LEN];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		wlanHwAddrToBin(apcArgv[1], &aucMacAddr[0]);

		if (!wlanGetWlanIdxByAddress(prGlueInfo->prAdapter,
		    &aucMacAddr[0], &ucWlanIndex))
			return i4BytesWritten;

		if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIndex, &ucStaIdx)
		    != WLAN_STATUS_SUCCESS)
			return i4BytesWritten;

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"StaIdx = %d, WlanIdx = %d\n", ucStaIdx,
				ucWlanIndex);
	}

	return i4BytesWritten;
}

static int priv_driver_get_version(IN struct net_device *prNetDev,
				   IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	int32_t i4BytesWritten = 0;
	uint32_t u4Offset = 0;

	ASSERT(prNetDev);

	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	u4Offset += fwDlGetFwdlInfo(prAdapter, pcCommand, i4TotalLen);
	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"WiFi Driver Version %u.%u.%u\n",
		NIC_DRIVER_MAJOR_VERSION,
		NIC_DRIVER_MINOR_VERSION,
		NIC_DRIVER_SERIAL_VERSION);

	i4BytesWritten = (int32_t)u4Offset;

	return i4BytesWritten;
}

#if CFG_CHIP_RESET_HANG
static int priv_driver_set_rst_hang(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret;


	ASSERT(prNetDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc == 0) {
		DBGLOG(REQ, INFO, "set_rst_hang Argc = %d\n", i4Argc);
		return -EFAULT;
	}

	if (strnicmp(apcArgv[0], CMD_SET_RST_HANG,
				strlen(CMD_SET_RST_HANG)) == 0) {
		if (i4Argc < CMD_SET_RST_HANG_ARG_NUM) {
			DBGLOG(REQ, STATE,
				"[SER][L0] RST_HANG_SET arg num=%d,must be %d\n",
				i4Argc, CMD_SET_RST_HANG_ARG_NUM);
			return -EFAULT;
		}
		u4Ret = kalkStrtou8(apcArgv[1], 0, &fgIsResetHangState);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "u4Ret=%d\n", u4Ret);

		DBGLOG(REQ, STATE, "[SER][L0] set fgIsResetHangState=%d\n",
							fgIsResetHangState);

		if (fgIsResetHangState == SER_L0_HANG_RST_CMD_TRG) {
			DBGLOG(REQ, STATE, "[SER][L0] cmd trigger\n");
			glSetRstReason(RST_CMD_TRIGGER);
			GL_RESET_TRIGGER(NULL, RST_FLAG_CHIP_RESET);
		}

	} else {
		DBGLOG(REQ, STATE, "[SER][L0] get fgIsResetSqcState=%d\n",
							fgIsResetHangState);
		DBGLOG(REQ, ERROR, "[SER][L0] RST HANG subcmd(%s) error !\n",
								apcArgv[0]);

		return -EFAULT;
	}

	return 0;

}
#endif

#if CFG_SUPPORT_DBDC
int priv_driver_set_dbdc(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t ucDBDCEnable;
	/*UINT_8 ucBssIndex;*/
	/*P_BSS_INFO_T prBssInfo;*/


	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
#if 0
	for (ucBssIndex = 0; ucBssIndex < (prAdapter->ucHwBssIdNum + 1);
	     ucBssIndex++) {
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
	if (prGlueInfo->prAdapter->rWifiVar.eDbdcMode !=
	    ENUM_DBDC_MODE_DYNAMIC) {
		DBGLOG(REQ, LOUD,
		       "Current DBDC mode %u cannot enable/disable DBDC!!\n",
		       prGlueInfo->prAdapter->rWifiVar.eDbdcMode);
		return -1;
	}

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		ucDBDCEnable = (uint8_t) u4Parse;
		if ((!prGlueInfo->prAdapter->rWifiVar.fgDbDcModeEn &&
		     !ucDBDCEnable) ||
		    (prGlueInfo->prAdapter->rWifiVar.fgDbDcModeEn &&
		     ucDBDCEnable))
			return i4BytesWritten;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetDbdcEnable,
				   &ucDBDCEnable, 1, FALSE, FALSE, TRUE,
				   &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	} else {
		DBGLOG(INIT, ERROR, "iwpriv wlanXX driver SET_DBDC <enable>\n");
		DBGLOG(INIT, ERROR, "<enable> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

int priv_driver_set_sta1ss(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		prGlueInfo->prAdapter->rWifiVar.fgSta1NSS =
			(uint8_t) u4Parse;

	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX driver SET_STA1NSS <enable>\n");
		DBGLOG(INIT, ERROR,
			"<enable> 1: enable. 0: disable.\n");
	}

	return i4BytesWritten;
}

#endif /*CFG_SUPPORT_DBDC*/

#if CFG_SUPPORT_BATCH_SCAN
#define CMD_BATCH_SET           "WLS_BATCHING SET"
#define CMD_BATCH_GET           "WLS_BATCHING GET"
#define CMD_BATCH_STOP          "WLS_BATCHING STOP"
#endif

static int priv_driver_get_que_info(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	return qmDumpQueueStatus(prGlueInfo->prAdapter, pcCommand, i4TotalLen);
}

static int priv_driver_get_mem_info(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	return cnmDumpMemoryStatus(prGlueInfo->prAdapter, pcCommand,
				   i4TotalLen);
}

static int priv_driver_get_hif_info(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	return halDumpHifStatus(prGlueInfo->prAdapter, pcCommand, i4TotalLen);
}

static int priv_driver_get_capab_rsdb(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	uint32_t u4Offset = 0;
	u_int8_t fgDbDcModeEn = FALSE;

	prGlueInfo = wlanGetGlueInfo();
	if (!prGlueInfo) {
		DBGLOG(REQ, WARN, "prGlueInfo is NULL\n");
		return -EFAULT;
	}
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

#if CFG_SUPPORT_DBDC
	if (prGlueInfo->prAdapter->rWifiVar.eDbdcMode !=
	    ENUM_DBDC_MODE_DISABLED)
		fgDbDcModeEn = TRUE;

	if (prGlueInfo->prAdapter->rWifiFemCfg.u2WifiPath ==
	    (WLAN_FLAG_2G4_WF0 | WLAN_FLAG_5G_WF1))
		fgDbDcModeEn = FALSE;
#endif

	DBGLOG(REQ, INFO, "RSDB:%d\n", fgDbDcModeEn);

	u4Offset += kalScnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
			     "RSDB:%d",
			     fgDbDcModeEn);

	i4BytesWritten = (int32_t)u4Offset;

	return i4BytesWritten;

}

static int priv_driver_get_cnm(IN struct net_device *prNetDev,
			       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct PARAM_GET_CNM_T *prCnmInfo = NULL;

	enum ENUM_DBDC_BN	eDbdcIdx, eDbdcIdxMax;
	uint8_t			ucBssIdx;
	struct BSS_INFO *prBssInfo;
	enum ENUM_CNM_NETWORK_TYPE_T eNetworkType;
	uint8_t ucOpRxNss, ucOpTxNss;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	prCnmInfo = (struct PARAM_GET_CNM_T *)kalMemAlloc(
				sizeof(struct PARAM_GET_CNM_T), VIR_MEM_TYPE);
	if (prCnmInfo == NULL)
		return -1;

	kalMemZero(prCnmInfo, sizeof(struct PARAM_GET_CNM_T));

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryCnm, prCnmInfo,
			   sizeof(struct PARAM_GET_CNM_T),
			   TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		kalMemFree(prCnmInfo,
			VIR_MEM_TYPE, sizeof(struct PARAM_GET_CNM_T));
		return -1;
	}

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				   i4TotalLen - i4BytesWritten,
				   "\n[CNM Info]\n");
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				   i4TotalLen - i4BytesWritten,
				   "DBDC Mode : %s\n\n",
				   (prCnmInfo->fgIsDbdcEnable) ?
				   "Enable" : "Disable");

	eDbdcIdxMax = (prCnmInfo->fgIsDbdcEnable)?ENUM_BAND_NUM:ENUM_BAND_1;
	for (eDbdcIdx = ENUM_BAND_0; eDbdcIdx < eDbdcIdxMax; eDbdcIdx++) {
		/* Do not clean history information */
		/* if argc is bigger than 1 */
		if (i4Argc < 2) {
			if (prCnmInfo->ucOpChNum[eDbdcIdx] < 3)
				prCnmInfo->ucChList[eDbdcIdx][2] = 0;
			if (prCnmInfo->ucOpChNum[eDbdcIdx] < 2)
				prCnmInfo->ucChList[eDbdcIdx][1] = 0;
			if (prCnmInfo->ucOpChNum[eDbdcIdx] < 1)
				prCnmInfo->ucChList[eDbdcIdx][0] = 0;
		}
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					   i4TotalLen - i4BytesWritten,
					   "Band %u OPCH %d [%u, %u, %u]\n",
					   eDbdcIdx,
					   prCnmInfo->ucOpChNum[eDbdcIdx],
					   prCnmInfo->ucChList[eDbdcIdx][0],
					   prCnmInfo->ucChList[eDbdcIdx][1],
					   prCnmInfo->ucChList[eDbdcIdx][2]);
	}
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
				   i4TotalLen - i4BytesWritten, "\n");

	for (ucBssIdx = BSSID_0; ucBssIdx < (BSSID_NUM+1); ucBssIdx++) {

		prBssInfo = prGlueInfo->prAdapter->aprBssInfo[ucBssIdx];
		if (!prBssInfo)
			continue;

		eNetworkType = cnmGetBssNetworkType(prBssInfo);
		if (prCnmInfo->ucBssInuse[ucBssIdx] &&
		    prCnmInfo->ucBssActive[ucBssIdx] &&
		    ((eNetworkType == ENUM_CNM_NETWORK_TYPE_P2P_GO) ||
		     ((eNetworkType == ENUM_CNM_NETWORK_TYPE_AIS ||
		       eNetworkType == ENUM_CNM_NETWORK_TYPE_P2P_GC) &&
		      (prCnmInfo->ucBssConnectState[ucBssIdx] ==
		       MEDIA_STATE_CONNECTED)))) {
			ucOpRxNss = prBssInfo->ucOpRxNss;
			if (eNetworkType == ENUM_CNM_NETWORK_TYPE_P2P_GO) {
				struct STA_RECORD *prCurrStaRec =
						(struct STA_RECORD *) NULL;

				prCurrStaRec = LINK_PEEK_HEAD(
						&prBssInfo->rStaRecOfClientList,
						struct STA_RECORD, rLinkEntry);

				if (prCurrStaRec != NULL &&
				    IS_CONNECTION_NSS2(prBssInfo,
				    prCurrStaRec)) {
					ucOpTxNss = 2;
				} else
					ucOpTxNss = 1;

				ucOpRxNss = prBssInfo->ucOpRxNss;
			} else if (prBssInfo->prStaRecOfAP != NULL &&
				   IS_CONNECTION_NSS2(prBssInfo,
				   prBssInfo->prStaRecOfAP)) {
				ucOpTxNss = 2;
			} else
				ucOpTxNss = 1;
		} else {
			eNetworkType = ENUM_CNM_NETWORK_TYPE_OTHER;
			ucOpTxNss = prBssInfo->ucOpTxNss;
			ucOpRxNss = prBssInfo->ucOpRxNss;
			/* Do not show history information */
			/* if argc is 1 */
			if (i4Argc < 2)
				continue;
		}

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"BSS%u Inuse%u Act%u ConnStat%u [NetType%u][CH%3u][DBDC b%u][WMM%u b%u][OMAC%u b%u][BW%3u][TxNSS%u][RxNss%u]\n",
			ucBssIdx,
			prCnmInfo->ucBssInuse[ucBssIdx],
			prCnmInfo->ucBssActive[ucBssIdx],
			prCnmInfo->ucBssConnectState[ucBssIdx],
			eNetworkType,
			prCnmInfo->ucBssCh[ucBssIdx],
			prCnmInfo->ucBssDBDCBand[ucBssIdx],
			prCnmInfo->ucBssWmmSet[ucBssIdx],
			prCnmInfo->ucBssWmmDBDCBand[ucBssIdx],
			prCnmInfo->ucBssOMACSet[ucBssIdx],
			prCnmInfo->ucBssOMACDBDCBand[ucBssIdx],
			20 * (0x01 << rlmGetBssOpBwByVhtAndHtOpInfo(prBssInfo)),
			ucOpTxNss,
			ucOpRxNss);
#ifdef CONFIG_SUPPORT_OPENWRT
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "BW=BW%u\n",
			20 * (0x01 << rlmGetBssOpBwByVhtAndHtOpInfo(prBssInfo))
			);

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "[TxNSS%u][RxNss%u]\n",
			ucOpTxNss, ucOpRxNss);

#if (CFG_SUPPORT_802_11AX == 1)
	{

		struct STA_RECORD *prStaRec;

		prStaRec = cnmGetStaRecByAddress(prGlueInfo->prAdapter,
			ucBssIdx, prBssInfo->aucBSSID);

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "Mcs1=%u\n",
			(prStaRec->u2HeRxMcsMapBW80) & 0x3);

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "Mcs2=%u\n",
			((prStaRec->u2HeRxMcsMapBW80) >> 2) & 0x3);
	}
#endif /* CFG_SUPPORT_802_11AX */
#endif

	}

	kalMemFree(prCnmInfo, VIR_MEM_TYPE, sizeof(struct PARAM_GET_CNM_T));
	return i4BytesWritten;
}				/* priv_driver_get_sw_ctrl */

static int priv_driver_get_ch_rank_list(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t i4BytesWritten = 0;
	int8_t ucIdx = 0, ucIdx2 = 0, ucChannelNum = 0,
		ucNumOf2gChannel = 0, ucNumOf5gChannel = 0;
	struct PARAM_GET_CHN_INFO *prChnLoadInfo = NULL;
	struct RF_CHANNEL_INFO *prChannelList = NULL,
		auc2gChannelList[MAX_2G_BAND_CHN_NUM],
		auc5gChannelList[MAX_5G_BAND_CHN_NUM];

	ASSERT(prNetDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prChnLoadInfo = &(prGlueInfo->prAdapter->rWifiVar.rChnLoadInfo);
	kalMemZero(pcCommand, i4TotalLen);

	rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_2G4, TRUE,
			     MAX_2G_BAND_CHN_NUM, &ucNumOf2gChannel,
			     auc2gChannelList);
	rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_5G, TRUE,
			     MAX_5G_BAND_CHN_NUM, &ucNumOf5gChannel,
			     auc5gChannelList);

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
					prChnLoadInfo->rChnRankList[ucIdx]
								.ucChannel;
				DBGLOG(SCN, TRACE, "ch %u, dirtiness %d\n",
					prChnLoadInfo->rChnRankList[ucIdx]
								.ucChannel,
					prChnLoadInfo->rChnRankList[ucIdx]
								.u4Dirtiness);
				break;
			}
		}
	}

	return i4BytesWritten;
}

static int priv_driver_get_ch_dirtiness(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int8_t cIdx = 0;
	uint8_t ucNumOf2gChannel = 0;
	uint8_t ucNumOf5gChannel = 0;
	uint32_t i4BytesWritten = 0;
	struct PARAM_GET_CHN_INFO *prChnLoadInfo = NULL;
	struct RF_CHANNEL_INFO ar2gChannelList[MAX_2G_BAND_CHN_NUM];
	struct RF_CHANNEL_INFO ar5gChannelList[MAX_5G_BAND_CHN_NUM];

	ASSERT(prNetDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prChnLoadInfo = &(prGlueInfo->prAdapter->rWifiVar.rChnLoadInfo);
	kalMemZero(pcCommand, i4TotalLen);

	rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_2G4, TRUE,
			     MAX_2G_BAND_CHN_NUM, &ucNumOf2gChannel,
			     ar2gChannelList);
	rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_5G, TRUE,
			     MAX_5G_BAND_CHN_NUM, &ucNumOf5gChannel,
			     ar5gChannelList);

	for (cIdx = 0; cIdx < MAX_CHN_NUM; cIdx++) {
		int8_t cIdx2 = 0;
		uint8_t ucChannelNum = 0;
		uint32_t u4Offset = 0;
		struct RF_CHANNEL_INFO *prChannelList = NULL;

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
				u4Offset = kalSprintf(
					pcCommand + i4BytesWritten,
					"\nch %03u -> dirtiness %u",
					prChnLoadInfo->rChnRankList[cIdx]
								.ucChannel,
					prChnLoadInfo->rChnRankList[cIdx]
								.u4Dirtiness);
				i4BytesWritten += u4Offset;
				break;
			}
		}
	}

	return i4BytesWritten;
}

static int priv_driver_efuse_ops(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	enum EFUSE_OP_MODE {
		EFUSE_READ,
		EFUSE_WRITE,
		EFUSE_FREE,
		EFUSE_INVALID,
	};
	uint8_t ucOpMode = EFUSE_INVALID;
	uint8_t ucOpChar;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret;
	int32_t i4Parameter = 0;
	uint32_t u4Efuse_addr = 0;
	uint8_t ucEfuse_value = 0;

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4Offset = 0;
	uint32_t u4BufLen = 0;
	uint8_t  u4Index = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_CUSTOM_ACCESS_EFUSE rAccessEfuseInfo;
#endif
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	/* Sanity check */
	if (i4Argc < 2)
		goto efuse_op_invalid;

	ucOpChar = (uint8_t)apcArgv[1][0];
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
		u4Efuse_addr = (uint32_t)i4Parameter;
	}

	/* convert value */
	if (ucOpMode == EFUSE_WRITE) {
		u4Ret = kalkStrtos32(apcArgv[3], 16, &i4Parameter);
		ucEfuse_value = (uint8_t)i4Parameter;
	}

	/* Start operation */
#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (prGlueInfo == NULL) {
		DBGLOG(REQ, ERROR, "prGlueInfo is null\n");
		goto efuse_op_invalid;
	}

	if (prGlueInfo &&
	    prGlueInfo->prAdapter &&
	    prGlueInfo->prAdapter->chip_info &&
	    !prGlueInfo->prAdapter->chip_info->is_support_efuse) {
		u4Offset += kalSnprintf(pcCommand + u4Offset,
				     i4TotalLen - u4Offset,
				     "efuse ops is invalid\n");
		return (int32_t)u4Offset;
	}

	kalMemSet(&rAccessEfuseInfo, 0,
		  sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE));
	rAccessEfuseInfo.u4Address = (u4Efuse_addr / EFUSE_BLOCK_SIZE)
				     * EFUSE_BLOCK_SIZE;

	u4Index = u4Efuse_addr % EFUSE_BLOCK_SIZE;

	if (ucOpMode == EFUSE_READ) {
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryProcessAccessEfuseRead,
				   &rAccessEfuseInfo,
				   sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
				   TRUE, TRUE, TRUE, &u4BufLen);

		if (rStatus == WLAN_STATUS_SUCCESS) {
			u4Offset += kalSnprintf(pcCommand + u4Offset,
			     i4TotalLen - u4Offset,
			     "Read success 0x%X = 0x%X\n", u4Efuse_addr,
			     prGlueInfo->prAdapter->aucEepromVaule[u4Index]);
		}
	} else if (ucOpMode == EFUSE_WRITE) {

		prGlueInfo->prAdapter->aucEepromVaule[u4Index] = ucEfuse_value;

		kalMemCopy(rAccessEfuseInfo.aucData,
			   prGlueInfo->prAdapter->aucEepromVaule, 16);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryProcessAccessEfuseWrite,
				   &rAccessEfuseInfo,
				   sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE),
				   FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus == WLAN_STATUS_SUCCESS) {
			u4Offset += kalSnprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Write success 0x%X = 0x%X\n",
					     u4Efuse_addr, ucEfuse_value);
		}
	} else if (ucOpMode == EFUSE_FREE) {
		struct PARAM_CUSTOM_EFUSE_FREE_BLOCK rEfuseFreeBlock = {};

		if (prGlueInfo->prAdapter->fgIsSupportGetFreeEfuseBlockCount
		    == FALSE) {
			u4Offset += kalSnprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Cannot read free block size\n");
			return (int32_t)u4Offset;
		}
		rStatus = kalIoctl(prGlueInfo, wlanoidQueryEfuseFreeBlock,
				   &rEfuseFreeBlock,
				   sizeof(struct PARAM_CUSTOM_EFUSE_FREE_BLOCK),
				   TRUE, TRUE, TRUE, &u4BufLen);
		if (rStatus == WLAN_STATUS_SUCCESS) {
			u4Offset += kalSnprintf(pcCommand + u4Offset,
				     i4TotalLen - u4Offset,
				     "Free block size 0x%X\n",
				     prGlueInfo->prAdapter->u4FreeBlockNum);
		}
	}
#else
	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
					"efuse ops is invalid\n");
#endif

	return (int32_t)u4Offset;

efuse_op_invalid:

	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\nHelp menu\n");
	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tRead:\t\"efuse read addr_hex\"\n");
	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tWrite:\t\"efuse write addr_hex val_hex\"\n");
	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tFree Blocks:\t\"efuse free\"\n");
	return (int32_t)u4Offset;
}

#if defined(_HIF_SDIO) && (MTK_WCN_HIF_SDIO == 0)
static int priv_driver_cccr_ops(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	enum CCCR_OP_MODE {
		CCCR_READ,
		CCCR_WRITE,
		CCCR_FREE,
		CCCR_INVALID,
	};
	uint8_t ucOpMode = CCCR_INVALID;
	uint8_t ucOpChar;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t i4Ret;
	int32_t i4Parameter;
	uint32_t u4CCCR_addr = 0;
	uint8_t ucCCCR_value = 0;

	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4Offset = 0;
	struct GLUE_INFO *prGlueInfo = NULL;

	struct sdio_func *func;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);

	if(!IS_SDIO_INF(prGlueInfo)){
		u4Offset += kalSnprintf(pcCommand + u4Offset,
				     i4TotalLen - u4Offset,
				     "Not SDIO bus(%d)\n",
				     prGlueInfo->u4InfType);
		return (int32_t)u4Offset;
	}

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
			u4Offset += kalSnprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Read Fail 0x%X (ret=%d)\n",
					     u4CCCR_addr, rStatus);
		else
			u4Offset += kalSnprintf(pcCommand + u4Offset,
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
			u4Offset += kalSnprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Write Fail 0x%X (ret=%d)\n",
					     u4CCCR_addr, rStatus);
		else
			u4Offset += kalSnprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Write success 0x%X = 0x%X\n",
					     u4CCCR_addr, ucCCCR_value);
	}

	return (int32_t)u4Offset;

cccr_op_invalid:

	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\nHelp menu\n");
	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tRead:\t\"cccr read addr_hex\"\n");
	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tWrite:\t\"cccr write addr_hex val_hex\"\n");
	return (int32_t)u4Offset;
}
#endif /* _HIF_SDIO && (MTK_WCN_HIF_SDIO == 0) */

#if CFG_SUPPORT_ADVANCE_CONTROL
static int priv_driver_set_noise(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + 1;
	uint32_t u4Sel = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

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
	rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite, &rSwCtrlInfo,
			   sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;

}

static int priv_driver_get_noise(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + 1;
	uint32_t u4Offset = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	int16_t u2Wf0AvgPwr, u2Wf1AvgPwr;

	ASSERT(prNetDev);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo, wlanoidQuerySwCtrlRead, &rSwCtrlInfo,
			   sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u2Wf0AvgPwr = rSwCtrlInfo.u4Data & 0xFFFF;
	u2Wf1AvgPwr = (rSwCtrlInfo.u4Data >> 16) & 0xFFFF;

	u4Offset += kalSnprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
			     "Noise Idle Avg. Power: WF0:%ddB WF1:%ddB\n",
			     u2Wf0AvgPwr, u2Wf1AvgPwr);

	i4BytesWritten = (int32_t)u4Offset;

	return i4BytesWritten;

}				/* priv_driver_get_sw_ctrl */

static int priv_driver_set_pop(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + 2;
	uint32_t u4Sel = 0, u4CckTh = 0, u4OfdmTh = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 3) {
		DBGLOG(REQ, ERROR,
		       "Argc(%d) ERR: SET_POP <Sel> <CCK TH> <OFDM TH>\n",
		       i4Argc);
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
	rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite, &rSwCtrlInfo,
			   sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;

}

static int priv_driver_set_ed(IN struct net_device *prNetDev,
			      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0, u4EdVal = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + 3;
	uint32_t u4Sel = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 2) {
		DBGLOG(REQ, ERROR,
		       "Argc(%d) ERR: SET_ED <Sel> <EDCCA(-49~-81dBm)>\n",
		       i4Argc);
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

	rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite, &rSwCtrlInfo,
			   sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;

}

static int priv_driver_get_tp_info(IN struct net_device *prNetDev,
				   IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	return kalPerMonGetInfo(prGlueInfo->prAdapter, pcCommand, i4TotalLen);
}

static int priv_driver_set_pd(IN struct net_device *prNetDev,
			      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + 4;
	uint32_t u4Sel = 0;
	int32_t u4CckTh = 0, u4OfdmTh = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR,
		       "Argc(%d) ERR: SET_PD <Sel> [CCK TH] [OFDM TH]\n",
		       i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);

	if (u4Sel == 1) {
		if (i4Argc <= 3) {
			DBGLOG(REQ, ERROR,
			       "Argc(%d) ERR: SET_PD 1 <CCK TH> <OFDM CH>\n",
			       i4Argc);
			return -1;
		}
		u4Ret = kalkStrtos32(apcArgv[2], 0, &u4CckTh);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n",
			       u4Ret);
		u4Ret = kalkStrtos32(apcArgv[3], 0, &u4OfdmTh);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n",
			       u4Ret);
	}

	rSwCtrlInfo.u4Data = ((u4OfdmTh & 0xFFFF) | ((u4CckTh & 0xFF) << 16) |
			      (u4Sel << 30));
	DBGLOG(REQ, LOUD, "u4Sel=%d u4OfdmTh=%d, u4CckTh=%d, u4Data=0x%x,\n",
		u4Sel, u4OfdmTh, u4CckTh, rSwCtrlInfo.u4Data);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite, &rSwCtrlInfo,
			   sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

static int priv_driver_set_maxrfgain(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + 5;
	uint32_t u4Sel = 0;
	int32_t u4Wf0Gain = 0, u4Wf1Gain = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR,
		       "Argc(%d) ERR: SET_RFGAIN <Sel> <WF0 Gain> <WF1 Gain>\n",
		       i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);

	if (u4Sel == 1) {
		if (i4Argc <= 3) {
			DBGLOG(REQ, ERROR,
			       "Argc(%d) ERR: SET_RFGAIN 1 <WF0 Gain> <WF1 Gain>\n",
			       i4Argc);
			return -1;
		}
		u4Ret = kalkStrtos32(apcArgv[2], 0, &u4Wf0Gain);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n",
			       u4Ret);
		u4Ret = kalkStrtos32(apcArgv[3], 0, &u4Wf1Gain);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n",
			       u4Ret);
	}

	rSwCtrlInfo.u4Data = ((u4Wf0Gain & 0xFF) | ((u4Wf1Gain & 0xFF) << 8) |
			      (u4Sel << 31));
	DBGLOG(REQ, LOUD, "u4Sel=%d u4Wf0Gain=%d, u4Wf1Gain=%d, u4Data=0x%x,\n",
		u4Sel, u4Wf0Gain, u4Wf1Gain, rSwCtrlInfo.u4Data);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite, &rSwCtrlInfo,
			   sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

#endif


#if (CFG_SUPPORT_TWT == 1)
static int priv_driver_set_twtparams(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct ADAPTER *prAdapter = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX_LONG] = { 0 };
	struct _TWT_CTRL_T rTWTCtrl;
	struct _TWT_PARAMS_T *prTWTParams;
	uint16_t i;
	int32_t u4Ret = 0;
	uint16_t au2Setting[CMD_TWT_MAX_PARAMS];
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate = NULL;
	struct _MSG_TWT_PARAMS_SET_T *prTWTParamSetMsg;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) netdev_priv(prNetDev);

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %i\n", i4Argc);

	prAdapter = prNetDevPrivate->prGlueInfo->prAdapter;

	/* Check param number and convert TWT params to integer type */
	if (i4Argc == CMD_TWT_ACTION_TEN_PARAMS ||
		i4Argc == CMD_TWT_ACTION_THREE_PARAMS) {
		for (i = 0; i < (i4Argc - 1); i++) {

			u4Ret = kalkStrtou16(apcArgv[i + 1],
				0, &(au2Setting[i]));

			if (u4Ret)
				DBGLOG(REQ, INFO, "Argv error ret=%d\n", u4Ret);
		}
	} else {
		DBGLOG(REQ, INFO, "set_twtparams wrong argc : %d\n", i4Argc);
		return -1;
	}

	if ((IS_TWT_PARAM_ACTION_DEL(au2Setting[0]) ||
		IS_TWT_PARAM_ACTION_SUSPEND(au2Setting[0]) ||
		IS_TWT_PARAM_ACTION_RESUME(au2Setting[0])) &&
		i4Argc == CMD_TWT_ACTION_THREE_PARAMS) {

		DBGLOG(REQ, INFO, "Action=%d\n", au2Setting[0]);
		DBGLOG(REQ, INFO, "TWT Flow ID=%d\n", au2Setting[1]);

		if (au2Setting[1] >= TWT_MAX_FLOW_NUM) {
			/* Simple sanity check failure */
			DBGLOG(REQ, INFO, "Invalid TWT Params\n");
			return -1;
		}

		rTWTCtrl.ucBssIdx = prNetDevPrivate->ucBssIdx;
		rTWTCtrl.ucCtrlAction = au2Setting[0];
		rTWTCtrl.ucTWTFlowId = au2Setting[1];

	} else if (i4Argc == CMD_TWT_ACTION_TEN_PARAMS) {
		DBGLOG(REQ, INFO, "Action bitmap=%d\n", au2Setting[0]);
		DBGLOG(REQ, INFO,
			"TWT Flow ID=%d Setup Command=%d Trig enabled=%d\n",
			au2Setting[1], au2Setting[2], au2Setting[3]);
		DBGLOG(REQ, INFO,
			"Unannounced enabled=%d Wake Interval Exponent=%d\n",
			au2Setting[4], au2Setting[5]);
		DBGLOG(REQ, INFO, "Protection enabled=%d Duration=%d\n",
			au2Setting[6], au2Setting[7]);
		DBGLOG(REQ, INFO, "Wake Interval Mantissa=%d\n", au2Setting[8]);
		/*
		 *  au2Setting[0]: Whether bypassing nego or not
		 *  au2Setting[1]: TWT Flow ID
		 *  au2Setting[2]: TWT Setup Command
		 *  au2Setting[3]: Trigger enabled
		 *  au2Setting[4]: Unannounced enabled
		 *  au2Setting[5]: TWT Wake Interval Exponent
		 *  au2Setting[6]: TWT Protection enabled
		 *  au2Setting[7]: Nominal Minimum TWT Wake Duration
		 *  au2Setting[8]: TWT Wake Interval Mantissa
		 */
		if (au2Setting[1] >= TWT_MAX_FLOW_NUM ||
			au2Setting[2] > TWT_SETUP_CMD_DEMAND ||
			au2Setting[5] > TWT_MAX_WAKE_INTVAL_EXP) {
			/* Simple sanity check failure */
			DBGLOG(REQ, INFO, "Invalid TWT Params\n");
			return -1;
		}

		prTWTParams = &(rTWTCtrl.rTWTParams);
		kalMemSet(prTWTParams, 0, sizeof(struct _TWT_PARAMS_T));
		prTWTParams->fgReq = TRUE;
		prTWTParams->ucSetupCmd = (uint8_t) au2Setting[2];
		prTWTParams->fgTrigger = (au2Setting[3]) ? TRUE : FALSE;
		prTWTParams->fgUnannounced = (au2Setting[4]) ? TRUE : FALSE;
		prTWTParams->ucWakeIntvalExponent = (uint8_t) au2Setting[5];
		prTWTParams->fgProtect = (au2Setting[6]) ? TRUE : FALSE;
		prTWTParams->ucMinWakeDur = (uint8_t) au2Setting[7];
		prTWTParams->u2WakeIntvalMantiss = au2Setting[8];

		rTWTCtrl.ucBssIdx = prNetDevPrivate->ucBssIdx;
		rTWTCtrl.ucCtrlAction = au2Setting[0];
		rTWTCtrl.ucTWTFlowId = au2Setting[1];
	} else {
		DBGLOG(REQ, INFO, "wrong argc for update agrt: %d\n", i4Argc);
		return -1;
	}

	prTWTParamSetMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
		sizeof(struct _MSG_TWT_REQFSM_RESUME_T));
	if (prTWTParamSetMsg) {
		prTWTParamSetMsg->rMsgHdr.eMsgId =
			MID_TWT_PARAMS_SET;
		kalMemCopy(&prTWTParamSetMsg->rTWTCtrl,
			&rTWTCtrl, sizeof(rTWTCtrl));

		mboxSendMsg(prAdapter, MBOX_ID_0,
			(struct MSG_HDR *) prTWTParamSetMsg,
			MSG_SEND_METHOD_BUF);
	} else
		return -1;

	return 0;
}
#endif

#if (CFG_SUPPORT_802_11AX == 1)
int priv_driver_set_pad_dur(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4Parse = 0;
	uint8_t u1HePadDur;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));



	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc == 2) {

		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Parse);
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse apcArgv error u4Ret=%d\n",
			       u4Ret);

		u1HePadDur = (uint8_t) u4Parse;

		if (u1HePadDur == 0 || u1HePadDur == 8 ||
			u1HePadDur == 16) {
			prGlueInfo->prAdapter->rWifiVar.ucTrigMacPadDur
				= u1HePadDur/8;
		} else {
			DBGLOG(INIT, ERROR,
				"iwpriv wlanXX driver SET_PAD_DUR <0,1,2>\n");
		}
	} else {
		DBGLOG(INIT, ERROR,
			"iwpriv wlanXX driver SET_PAD_DUR <0,1,2>\n");
	}

	return i4BytesWritten;
}
#endif


static int priv_driver_get_wifi_type(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct PARAM_GET_WIFI_TYPE rParamGetWifiType;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE) {
		DBGLOG(REQ, ERROR, "GLUE_CHK_PR2 fail\n");
		return -1;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	rParamGetWifiType.prNetDev = prNetDev;
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidGetWifiType,
			   (void *)&rParamGetWifiType,
			   sizeof(void *),
			   FALSE,
			   FALSE,
			   FALSE,
			   &u4BytesWritten);
	if (rStatus == WLAN_STATUS_SUCCESS) {
		if (u4BytesWritten > 0) {
			if (u4BytesWritten > i4TotalLen)
				u4BytesWritten = i4TotalLen;
			kalMemCopy(pcCommand, rParamGetWifiType.arWifiTypeName,
				   u4BytesWritten);
		}
	} else {
		DBGLOG(REQ, ERROR, "rStatus=%x\n", rStatus);
		u4BytesWritten = 0;
	}

	return (int)u4BytesWritten;
}

#if CFG_ENABLE_WIFI_DIRECT
static int priv_driver_set_p2p_ps(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t ucRoleIdx;
	uint8_t ucBssIdx;
	uint32_t u4CTwindowMs;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo = NULL;
	struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT *rOppPsParam = NULL;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc < 2) {
		DBGLOG(REQ, ERROR,
		 "Expect param: <role_idx> <CTW>. argc=%d now\n", i4Argc);
		return -1;
	}

	if (kalkStrtou32(apcArgv[1], 0, &ucRoleIdx)) {
		DBGLOG(REQ, ERROR, "parse ucRoleIdx error\n");
		return -1;
	}

	if (kalkStrtou32(apcArgv[2], 0, &u4CTwindowMs)) {
		DBGLOG(REQ, ERROR, "parse u4CTwindowMs error\n");
		return -1;
	}

	/* get Bss Index from ndev */
	if (p2pFuncRoleToBssIdx(prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "can't find ucBssIdx\n");
		return -1;
	}

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	if (!(prBssInfo->fgIsInUse) || (prBssInfo->eIftype != IFTYPE_P2P_GO)) {
		DBGLOG(REQ, ERROR, "wrong bss InUse=%d, iftype=%d\n",
			prBssInfo->fgIsInUse, prBssInfo->eIftype);
		return -1;
	}

	DBGLOG(REQ, INFO, "ucRoleIdx=%d, ucBssIdx=%d, u4CTwindowMs=%d\n",
		ucRoleIdx, ucBssIdx, u4CTwindowMs);

	if (u4CTwindowMs > 0)
		u4CTwindowMs |= BIT(7);	/* FW checks BIT(7) for enable */

	prP2pSpecBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo[ucRoleIdx];
	rOppPsParam = &prP2pSpecBssInfo->rOppPsParam;
	rOppPsParam->u4CTwindowMs = u4CTwindowMs;
	rOppPsParam->ucBssIdx = ucBssIdx;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetOppPsParam, rOppPsParam,
			sizeof(struct PARAM_CUSTOM_OPPPS_PARAM_STRUCT),
			FALSE, FALSE, TRUE, &u4BufLen);

	return !(rStatus == WLAN_STATUS_SUCCESS);
}

static int priv_driver_set_p2p_noa(IN struct net_device *prNetDev,
				   IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint8_t ucBssIdx;
	uint32_t ucRoleIdx;
	uint32_t u4NoaDurationMs;
	uint32_t u4NoaIntervalMs;
	uint32_t u4NoaCount;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecBssInfo = NULL;
	struct PARAM_CUSTOM_NOA_PARAM_STRUCT *rNoaParam = NULL;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc < 4) {
		DBGLOG(REQ, ERROR,
		  "SET_P2P_NOA <role_idx> <count> <interval> <duration>\n");
		return -1;
	}

	if (kalkStrtou32(apcArgv[1], 0, &ucRoleIdx)) {
		DBGLOG(REQ, ERROR, "parse ucRoleIdx error\n");
		return -1;
	}

	if (kalkStrtou32(apcArgv[2], 0, &u4NoaCount)) {
		DBGLOG(REQ, ERROR, "parse u4NoaCount error\n");
		return -1;
	}

	if (kalkStrtou32(apcArgv[3], 0, &u4NoaIntervalMs)) {
		DBGLOG(REQ, ERROR, "parse u4NoaIntervalMs error\n");
		return -1;
	}

	if (kalkStrtou32(apcArgv[4], 0, &u4NoaDurationMs)) {
		DBGLOG(REQ, ERROR, "parse u4NoaDurationMs error\n");
		return -1;
	}

	/* get Bss Index from ndev */
	if (p2pFuncRoleToBssIdx(prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "can't find ucBssIdx\n");
		return -1;
	}

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	if (!(prBssInfo->fgIsInUse) || (prBssInfo->eIftype != IFTYPE_P2P_GO)) {
		DBGLOG(REQ, ERROR, "wrong bss InUse=%d, iftype=%d\n",
			prBssInfo->fgIsInUse, prBssInfo->eIftype);
		return -1;
	}

	DBGLOG(REQ, INFO,
		"RoleIdx=%d, BssIdx=%d, count=%d, interval=%d, duration=%d\n",
		ucRoleIdx, ucBssIdx, u4NoaCount, u4NoaIntervalMs,
		u4NoaDurationMs);

	prP2pSpecBssInfo = prAdapter->rWifiVar.prP2pSpecificBssInfo[ucRoleIdx];
	rNoaParam = &prP2pSpecBssInfo->rNoaParam;
	rNoaParam->u4NoaCount = u4NoaCount;
	rNoaParam->u4NoaIntervalMs = u4NoaIntervalMs;
	rNoaParam->u4NoaDurationMs = u4NoaDurationMs;
	rNoaParam->ucBssIdx = ucBssIdx;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetNoaParam, rNoaParam,
			sizeof(struct PARAM_CUSTOM_NOA_PARAM_STRUCT),
			FALSE, FALSE, TRUE, &u4BufLen);

	return !(rStatus == WLAN_STATUS_SUCCESS);
}
#endif /* CFG_ENABLE_WIFI_DIRECT */

static int priv_driver_set_drv_ser(struct net_device *prNetDev,
				   char *pcCommand, int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = 0;
	uint32_t u4Num = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc <= 0) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: Set driver SER\n", i4Argc);
		return -1;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidSetDrvSer,
			   (void *)&u4Num, sizeof(uint32_t),
			   FALSE, FALSE, FALSE, &u4BufLen);

	i4BytesWritten += kalSnprintf(pcCommand, i4TotalLen,
				   "trigger driver SER\n");
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

#ifdef UT_TEST_MODE
int priv_driver_run_ut(IN struct net_device *prNetDev,
		       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t  *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	int32_t u4Ret = 0;
	uint32_t u4Input = 0;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo || !prGlueInfo->prAdapter) {
		DBGLOG(REQ, ERROR, "prGlueInfo or prAdapter is NULL\n");
		return -1;
	}

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc < 2) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: RUN_UT COMMAND\n", i4Argc);
		return -1;
	}

	if (strlen(apcArgv[1]) == strlen("all") &&
		   strnicmp(apcArgv[1], "all", strlen("all")) == 0)
		testRunAllTestCases(prGlueInfo->prAdapter);
	else if (i4Argc >= 3) {
		if (strlen(apcArgv[1]) == strlen("tc") &&
		    strnicmp(apcArgv[1], "tc", strlen("tc")) == 0) {
			u4Ret = kalkStrtou32(apcArgv[2], 0, &u4Input);
			if (u4Ret) {
				DBGLOG(REQ, ERROR, "parse error u4Ret=%d\n",
				       u4Ret);
				return -1;
			}
			testRunTestCase(prGlueInfo->prAdapter, u4Input);

		} else if (strlen(apcArgv[1]) == strlen("group") &&
			   strnicmp(apcArgv[1], "group",
				    strlen("group")) == 0) {
			testRunGroupTestCases(prGlueInfo->prAdapter,
					      apcArgv[2]);

		} else if (strlen(apcArgv[1]) == strlen("list") &&
			   strnicmp(apcArgv[1], "list", strlen("list")) == 0) {
			if (strlen(apcArgv[2]) == strlen("all") &&
			    strnicmp(apcArgv[2], "all", strlen("all")) == 0) {
				testRunAllTestCaseLists(prGlueInfo->prAdapter);
			} else {
				u4Ret = kalkStrtou32(apcArgv[2], 0, &u4Input);
				if (u4Ret) {
					DBGLOG(REQ, ERROR,
					       "parse error u4Ret=%d\n", u4Ret);
					return -1;
				}
				testRunTestCaseList(prGlueInfo->prAdapter,
						    u4Input);
			}
		}
	}

	return 0;
}
#endif /* UT_TEST_MODE */

static int priv_driver_set_amsdu_num(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = 0;
	int32_t u4Ret = 0;
	uint32_t u4Num = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: Sw Amsdu Num\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Num);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse amsdu num error u4Ret=%d\n", u4Ret);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetAmsduNum,
			   (void *)&u4Num, sizeof(uint32_t),
			   FALSE, FALSE, FALSE, &u4BufLen);

	i4BytesWritten += kalSnprintf(pcCommand, i4TotalLen,
				   "Set Sw Amsdu Num:%u\n", u4Num);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;

}

static int priv_driver_set_amsdu_size(IN struct net_device *prNetDev,
				      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = 0;
	int32_t u4Ret = 0;
	uint32_t u4Size = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: Sw Amsdu Max Size\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Size);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse amsdu size error u4Ret=%d\n", u4Ret);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetAmsduSize,
			   (void *)&u4Size, sizeof(uint32_t),
			   FALSE, FALSE, FALSE, &u4BufLen);

	i4BytesWritten += kalSnprintf(pcCommand, i4TotalLen,
				   "Set Sw Amsdu Max Size:%u\n", u4Size);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;

}

#if (CFG_SUPPORT_CONNINFRA == 1)
static int priv_driver_trigger_whole_chip_reset(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct mt66xx_chip_info *prChipInfo;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if ((!prGlueInfo) ||
	    (prGlueInfo->u4ReadyFlag == 0) ||
	    kalIsResetting()) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -1;
	}
	prChipInfo = prGlueInfo->prAdapter->chip_info;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	glSetRstReason(RST_CMD_TRIGGER);
	glSetRstReasonString(
		"cmd test trigger whole chip reset");
	if (prChipInfo->trigger_wholechiprst)
		prChipInfo->trigger_wholechiprst(g_reason);

	return i4BytesWritten;
}
static int priv_driver_trigger_wfsys_reset(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if ((!prGlueInfo) ||
	    (prGlueInfo->u4ReadyFlag == 0) ||
	    kalIsResetting()) {
		DBGLOG(REQ, WARN, "driver is not ready\n");
		return -1;
	}

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	glSetRstReason(RST_CMD_TRIGGER);
	GL_RESET_TRIGGER(prGlueInfo->prAdapter, RST_FLAG_WF_RESET);

	return i4BytesWritten;
}
#endif
#if (CFG_SUPPORT_CONNAC2X == 1)
static int priv_driver_get_umac_fwtbl(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int32_t u4Ret = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Index = 0;
	struct CHIP_DBG_OPS *prDbgOps;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2)
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Index);

	if (u4Ret)
		return -1;

	prDbgOps = prGlueInfo->prAdapter->chip_info->prDebugOps;

	if (prDbgOps->showUmacFwtblInfo)
		return prDbgOps->showUmacFwtblInfo(
			prGlueInfo->prAdapter, u4Index, pcCommand, i4TotalLen);
	else
		return -1;
}
#endif /* CFG_SUPPORT_CONNAC2X == 1 */

static int priv_driver_show_txd_info(
		IN struct net_device *prNetDev,
		IN char *pcCommand,
		IN int i4TotalLen
)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t u4Ret = -1;
	int32_t idx = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	struct CHIP_DBG_OPS *prDbgOps;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prDbgOps = prGlueInfo->prAdapter->chip_info->prDebugOps;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2)
		u4Ret = kalkStrtos32(apcArgv[1], 16, &idx);

	if (!u4Ret && prDbgOps && prDbgOps->showTxdInfo) {
		DBGLOG(HAL, INFO, "idx = %d 0x%x\n", idx, idx);
		prDbgOps->showTxdInfo(prGlueInfo->prAdapter, idx);
	}
	return i4BytesWritten;
}
#if (CONFIG_WLAN_SERVICE == 1)
int8_t *RxStatCommonUser[] = {
	/* common user stat info */
	"rx_fifo_full	: 0x%08x\n",
	"aci_hit_low	: 0x%08x\n",
	"aci_hit_high	: 0x%08x\n",
	"mu_pkt_cnt	: 0x%08x\n",
	"sig_mcs		: 0x%08x\n",
	"sinr		: 0x%08x\n",
	"driver_rx_count: 0x%08x\n"
};

int8_t *RxStatPerUser[] = {
   /* per user stat info */
	"freq_ofst_from_rx: 0x%08x\n",
	"snr		: 0x%08x\n",
	"fcs_err_cnt	: 0x%08x\n"
};

int8_t *RxStatPerAnt[] = {
	/* per anternna stat info */
	"rcpi		: %d\n",
	"rssi		: %d\n",
	"fagc_ib_rssi	: %d\n",
	"fagc_wb_rssi	: %d\n",
	"inst_ib_rssi	: %d\n",
	"inst_wb_rssi	: %d\n"
};

int8_t *RxStatPerBand[] = {
	/* per band stat info */
	"mac_fcs_err_cnt	: 0x%08x\n",
	"mac_mdy_cnt	: 0x%08x\n",
	"mac_len_mismatch: 0x%08x\n",
	"mac_fcs_ok_cnt	: 0x%08x\n",
	"phy_fcs_err_cnt_cck: 0x%08x\n",
	"phy_fcs_err_cnt_ofdm: 0x%08x\n",
	"phy_pd_cck	: 0x%08x\n",
	"phy_pd_ofdm	: 0x%08x\n",
	"phy_sig_err_cck	: 0x%08x\n",
	"phy_sfd_err_cck	: 0x%08x\n",
	"phy_sig_err_ofdm: 0x%08x\n",
	"phy_tag_err_ofdm: 0x%08x\n",
	"phy_mdy_cnt_cck	: 0x%08x\n",
	"phy_mdy_cnt_ofdm: 0x%08x\n"
};

int32_t priv_driver_rx_stat_parser(
	IN uint8_t *dataptr,
	IN int i4TotalLen,
	OUT char *pcCommand
)
{
	int32_t i4BytesWritten = 0;
	int32_t i4tmpContent = 0;
	int32_t i4TypeNum = 0, i4Type = 0, i4Version = 0;
	int32_t i = 0, j = 0, i4ItemMask = 0, i4TypeLen = 0, i4SubLen = 0;
	/*Get type num*/
	i4TypeNum = NTOHL(dataptr[3] << 24 | dataptr[2] << 16 |
	dataptr[1] << 8 | dataptr[0]);
	dataptr += 4;
	DBGLOG(REQ, LOUD, "i4TypeNum is %x\n", i4TypeNum);
	while (i4TypeNum) {
		i4TypeNum--;
		/*Get type*/
		i4Type = NTOHL(dataptr[3] << 24 | dataptr[2] << 16 |
		dataptr[1] << 8 | dataptr[0]);
		dataptr += 4;
		DBGLOG(REQ, LOUD, "i4Type is %x\n", i4Type);

		/*Get Version*/
		i4Version = NTOHL(dataptr[3] << 24 | dataptr[2] << 16 |
		dataptr[1] << 8 | dataptr[0]);
		dataptr += 4;
		DBGLOG(REQ, LOUD, "i4Version is %x\n", i4Version);

		/*Get Item Mask*/
		i4ItemMask = NTOHL(dataptr[3] << 24 | dataptr[2] << 16 |
		dataptr[1] << 8 | dataptr[0]);
		j = i4ItemMask;
		dataptr += 4;
		DBGLOG(REQ, LOUD, "i4ItemMask is %x\n", i4ItemMask);

		/*Get Len*/
		i4TypeLen = NTOHL(dataptr[3] << 24 | dataptr[2] << 16 |
		dataptr[1] << 8 | dataptr[0]);
		dataptr += 4;
		DBGLOG(REQ, LOUD, "i4TypeLen is %x\n", i4TypeLen);

		/*Get Sub Len*/
		while (j) {
			i++;
			j = j >> 1;
		}

		if (i != 0)
			i4SubLen = i4TypeLen / i;

		i = 0;
		DBGLOG(REQ, LOUD, "i4SubLen is %x\n", i4SubLen);

		while (i4TypeLen) {
			while (i < i4SubLen) {
				/*Get Content*/
				i4tmpContent = NTOHL(dataptr[3] << 24 |
				dataptr[2] << 16 | dataptr[1] << 8 |
				dataptr[0]);
				DBGLOG(REQ, LOUD,
				"i4tmpContent is %x\n", i4tmpContent);

				if (i4Type == 0) {
					i4BytesWritten += kalSnprintf(
						pcCommand + i4BytesWritten,
						i4TotalLen, RxStatPerBand[i/4],
						i4tmpContent);
				} else if (i4Type == 1) {
					i4BytesWritten += kalSnprintf(
						pcCommand + i4BytesWritten,
						i4TotalLen, RxStatPerAnt[i/4],
						i4tmpContent);
				} else if (i4Type == 2) {
					i4BytesWritten += kalSnprintf(
						pcCommand + i4BytesWritten,
						i4TotalLen, RxStatPerUser[i/4],
						i4tmpContent);
				} else {
					i4BytesWritten += kalSnprintf(
						pcCommand + i4BytesWritten,
						i4TotalLen,
						RxStatCommonUser[i/4],
						i4tmpContent);
				}
				i += 4;
				dataptr += 4;
			}
			i = 0;
			i4TypeLen -= i4SubLen;
		}
	}
	return i4BytesWritten;
}
#endif

static int priv_driver_run_hqa(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int8_t *this_char = NULL;
#if (CONFIG_WLAN_SERVICE == 1)
	struct hqa_frame_ctrl local_hqa;
	bool IsShowRxStat = FALSE;
	uint8_t *dataptr = NULL;
	uint8_t *oridataptr = NULL;
	int32_t datalen = 0;
	int32_t oridatalen = 0;
	int32_t ret = WLAN_STATUS_FAILURE;
	int16_t i2tmpVal = 0;
	int32_t i4tmpVal = 0;
#endif
	int32_t i = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	/*Roll over "HQA ", handle xxx=y,y,y,y,y,y....*/
	/* iwpriv wlan0 driver HQA xxx=y,y,y,y,y,y.....*/
	this_char = kalStrStr(pcCommand, "HQA ");
	if (!this_char)
		return -1;
	this_char += strlen("HQA ");

	/*handle white space*/
	i = strspn(this_char, " ");
	this_char += i;

	DBGLOG(REQ, LOUD, "this_char is %s\n", this_char);
#if (CONFIG_WLAN_SERVICE == 1)
	if (this_char) {
		if (strncasecmp(this_char,
		"GetRXStatisticsAllNew",
		strlen("GetRXStatisticsAllNew")) == 0)
		IsShowRxStat = TRUE;

		local_hqa.type = 1;
		local_hqa.hqa_frame_comm.hqa_frame_string = this_char;
		ret = mt_agent_hqa_cmd_handler(&prGlueInfo->rService,
		&local_hqa);
	}

	if (ret != WLAN_STATUS_SUCCESS)
		return -1;

	datalen = NTOHS(local_hqa.hqa_frame_comm.hqa_frame_eth->length);
	dataptr = kalMemAlloc(datalen, VIR_MEM_TYPE);
	if (dataptr == NULL)
		return -1;

	/* Backup Original Ptr /Len for mem Free */
	oridataptr = dataptr;
	oridatalen = datalen;
	kalMemCopy(dataptr,
	local_hqa.hqa_frame_comm.hqa_frame_eth->data, datalen);

	DBGLOG(REQ, LOUD,
	"priv_driver_run_hqa datalen is %d\n", datalen);
	DBGLOG_MEM8(REQ, LOUD, dataptr, datalen);

	/*parsing ret 2 bytes*/
	if ((dataptr) && (datalen)) {
		i2tmpVal = dataptr[1] << 8 | dataptr[0];
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
		"Return : 0x%04x\n", i2tmpVal);

		datalen -= 2;
		dataptr += 2;
	} else {
		DBGLOG(REQ, ERROR,
		"priv_driver_run_hqa not support\n");
		kalMemFree(oridataptr, VIR_MEM_TYPE, oridatalen);
		return -1;
	}

	/*parsing remaining data n bytes ( 4 bytes per parameter)*/
	for (i = 0; i < datalen ; i += 4, dataptr += 4) {
		if ((dataptr) && (datalen)) {
			i4tmpVal = dataptr[3] << 24 | dataptr[2] << 16 |
			dataptr[1] << 8 | dataptr[0];
			if (datalen == 4) {
				i4BytesWritten +=
				kalSnprintf(pcCommand + i4BytesWritten,
				i4TotalLen, "ExtId : 0x%08x\n", i4tmpVal);
			} else if (datalen == 8) {
				i4BytesWritten +=
				kalSnprintf(pcCommand + i4BytesWritten,
				i4TotalLen, "Band%d TX : 0x%08x\n", i/4,
				NTOHL(i4tmpVal));
			} else {
				if (IsShowRxStat) {
				i += datalen;
				i4BytesWritten +=
				priv_driver_rx_stat_parser(dataptr,
				i4TotalLen, pcCommand + i4BytesWritten);
				} else {
				i4BytesWritten +=
				kalSnprintf(pcCommand + i4BytesWritten,
				i4TotalLen, "id%d : 0x%08x\n", i/4,
				NTOHL(i4tmpVal));
				}
			}
		}
	}
	kalMemFree(oridataptr, VIR_MEM_TYPE, oridatalen);
#else
	DBGLOG(REQ, ERROR,
	"wlan_service not support\n");
#endif
	return i4BytesWritten;

}

static int priv_driver_calibration(
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct mt66xx_chip_info *prChipInfo;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret, u4GetInput = 0, u4GetInput2 = 0;
	int32_t i4ArgNum = 2;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prChipInfo = prGlueInfo->prAdapter->chip_info;

	DBGLOG(RFTEST, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(RFTEST, INFO, "argc is %i\n", i4Argc);

	if (i4Argc >= i4ArgNum) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &u4GetInput);
		if (u4Ret) {
			DBGLOG(RFTEST, INFO,
				   "%s: Parsing Fail\n", __func__);
		} else if (u4GetInput == 0) {

			i4BytesWritten = kalSnprintf(pcCommand,
				i4TotalLen,
				"reset driver calibration result\n");

			if (prChipInfo->calDebugCmd)
				prChipInfo->calDebugCmd(u4GetInput, 0);

		} else if (u4GetInput == 1) {

			u4Ret = kalkStrtou32(apcArgv[2], 0, &u4GetInput2);
			if (u4Ret) {
				DBGLOG(RFTEST, INFO,
					   "%s: Parsing 2 Fail\n", __func__);
			} else if (u4GetInput2 == 0) {
				i4BytesWritten = kalSnprintf(pcCommand,
					i4TotalLen,
					"driver result write back EMI\n");
			} else {
				i4BytesWritten = kalSnprintf(pcCommand,
					i4TotalLen,
					"FW use EMI original data\n");
			}

			if (prChipInfo->calDebugCmd)
				prChipInfo->calDebugCmd(u4GetInput,
					u4GetInput2);
		}
	} else {
		i4BytesWritten = kalSnprintf(pcCommand,
			i4TotalLen,
			"support parameter as below:\n");

		i4BytesWritten += kalSnprintf(
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"0: reset driver calibration result\n");

		i4BytesWritten += kalSnprintf(
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"1,0: driver result write back EMI\n");

		i4BytesWritten += kalSnprintf(
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"1,1: FW use EMI original data\n");
	}

	return i4BytesWritten;
}

static int priv_driver_get_nvram(IN struct net_device *prNetDev,
			       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = {0};
	uint32_t u4Ret;
	int32_t i4ArgNum = 2;
	uint32_t u4Offset = 0;
	uint16_t u2Index = 0;
	struct PARAM_CUSTOM_EEPROM_RW_STRUCT rNvrRwInfo;
	struct WIFI_CFG_PARAM_STRUCT *prNvSet;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	kalMemZero(&rNvrRwInfo, sizeof(rNvrRwInfo));

	prNvSet = prGlueInfo->rRegInfo.prNvramSettings;
	ASSERT(prNvSet);


	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %i\n", i4Argc);


	if (i4Argc == i4ArgNum) {
		u4Ret = kalkStrtou16(apcArgv[1], 0, &u2Index);

		if (u4Ret)
			DBGLOG(REQ, INFO,
			       "parse get_nvram error (Index) u4Ret=%d\n",
			       u4Ret);


		DBGLOG(REQ, INFO, "Index is 0x%x\n", u2Index);

		/* NVRAM Check */
		if (prGlueInfo->fgNvramAvailable == TRUE)
			u4Offset += snprintf(pcCommand + u4Offset,
				(i4TotalLen - u4Offset),
				"NVRAM Version is[%d.%d.%d]\n",
				(prNvSet->u2Part1OwnVersion & 0x00FF),
				(prNvSet->u2Part1OwnVersion & 0xFF00) >> 8,
				(prNvSet->u2Part1PeerVersion & 0xFF));
		else {
			u4Offset += snprintf(pcCommand + u4Offset,
				i4TotalLen - u4Offset,
				"[WARNING] NVRAM is unavailable!\n");

			return (int32_t) u4Offset;
		}

		rNvrRwInfo.ucMethod = PARAM_EEPROM_READ_NVRAM;
		rNvrRwInfo.info.rNvram.u2NvIndex = u2Index;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryNvramRead,
				   &rNvrRwInfo, sizeof(rNvrRwInfo),
				   TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, INFO, "rStatus %u\n", rStatus);
		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

		u4Offset += snprintf(pcCommand + u4Offset,
			(i4TotalLen - u4Offset),
			"NVRAM [0x%02X] = %d (0x%02X)\n",
			(unsigned int)rNvrRwInfo.info.rNvram.u2NvIndex,
			(unsigned int)rNvrRwInfo.info.rNvram.u2NvData,
			(unsigned int)rNvrRwInfo.info.rNvram.u2NvData);

		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__,
		       pcCommand);
	}

	return (int32_t)u4Offset;

}				/* priv_driver_get_nvram */

int priv_driver_set_nvram(IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret;
	int32_t i4ArgNum = 3;

	struct PARAM_CUSTOM_EEPROM_RW_STRUCT rNvRwInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, INFO, "argc is %i\n", i4Argc);

	kalMemZero(&rNvRwInfo, sizeof(rNvRwInfo));

	if (i4Argc >= i4ArgNum) {

		rNvRwInfo.ucMethod = PARAM_EEPROM_WRITE_NVRAM;

		u4Ret = kalkStrtou16(apcArgv[1], 0,
			&(rNvRwInfo.info.rNvram.u2NvIndex));
		if (u4Ret)
			DBGLOG(REQ, ERROR,
			       "parse set_nvram error (Add) Ret=%d\n",
			       u4Ret);

		u4Ret = kalkStrtou16(apcArgv[2], 0,
			&(rNvRwInfo.info.rNvram.u2NvData));
		if (u4Ret)
			DBGLOG(REQ, ERROR,
				"parse set_nvram error (Data) Ret=%d\n",
				u4Ret);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetNvramWrite,
				   &rNvRwInfo, sizeof(rNvRwInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;

	} else
		DBGLOG(INIT, ERROR,
					   "[Error]iwpriv wlanXX driver set_nvram [addr] [value]\n");


	return i4BytesWritten;

}

typedef int(*PRIV_CMD_FUNCTION) (
		IN struct net_device *prNetDev,
		IN char *pcCommand,
		IN int i4TotalLen);

struct PRIV_CMD_HANDLER {
	uint8_t *pcCmdStr;
	PRIV_CMD_FUNCTION pfHandler;
};

struct PRIV_CMD_HANDLER priv_cmd_handlers[] = {
	{CMD_RSSI, NULL /*wl_android_get_rssi*/},
	{CMD_AP_START, priv_driver_set_ap_start},
	{CMD_LINKSPEED, priv_driver_get_linkspeed},
	{CMD_PNOSSIDCLR_SET, NULL /*Nothing*/},
	{CMD_PNOSETUP_SET, NULL /*Nothing*/},
	{CMD_PNOENABLE_SET, NULL /*Nothing*/},
	{CMD_SETSUSPENDOPT, NULL /*wl_android_set_suspendopt*/},
	{CMD_SETSUSPENDMODE, priv_driver_set_suspend_mode},
	{CMD_SETBAND, priv_driver_set_band},
	{CMD_GETBAND, NULL /*wl_android_get_band*/},
	{CMD_COUNTRY, priv_driver_set_country},
	{CMD_CSA, priv_driver_set_csa},
	{CMD_GET_COUNTRY, priv_driver_get_country},
	{CMD_GET_CHANNELS, priv_driver_get_channels},
	{CMD_MIRACAST, priv_driver_set_miracast},
	/* Mediatek private command */
	{CMD_SET_SW_CTRL, priv_driver_set_sw_ctrl},
#if (CFG_SUPPORT_RA_GEN == 1)
	{CMD_SET_FIXED_FALLBACK, priv_driver_set_fixed_fallback},
	{CMD_SET_RA_DBG, priv_driver_set_ra_debug_proc},
#endif
#if (CFG_SUPPORT_TXPOWER_INFO == 1)
	{CMD_GET_TX_POWER_INFO, priv_driver_get_txpower_info},
#endif
	{CMD_TX_POWER_MANUAL_SET, priv_driver_txpower_man_set},
	{CMD_SET_FIXED_RATE, priv_driver_set_fixed_rate},
	{CMD_GET_SW_CTRL, priv_driver_get_sw_ctrl},
	{CMD_SET_MCR, priv_driver_set_mcr},
	{CMD_GET_MCR, priv_driver_get_mcr},
	{CMD_SET_DRV_MCR, priv_driver_set_drv_mcr},
	{CMD_GET_DRV_MCR, priv_driver_get_drv_mcr},
	{CMD_SET_TEST_MODE, priv_driver_set_test_mode},
	{CMD_SET_TEST_CMD, priv_driver_set_test_cmd},
	{CMD_GET_TEST_RESULT, priv_driver_get_test_result},
	{CMD_GET_STA_STAT2, priv_driver_get_sta_stat2},
	{CMD_GET_STA_STAT, priv_driver_get_sta_stat},
	{CMD_GET_STA_RX_STAT, priv_driver_show_rx_stat},
	{CMD_SET_ACL_POLICY, priv_driver_set_acl_policy},
	{CMD_ADD_ACL_ENTRY, priv_driver_add_acl_entry},
	{CMD_DEL_ACL_ENTRY, priv_driver_del_acl_entry},
	{CMD_SHOW_ACL_ENTRY, priv_driver_show_acl_entry},
	{CMD_CLEAR_ACL_ENTRY, priv_driver_clear_acl_entry},
#if (CFG_SUPPORT_DFS_MASTER == 1)
	{CMD_SHOW_DFS_STATE, priv_driver_show_dfs_state},
	{CMD_SHOW_DFS_RADAR_PARAM, priv_driver_show_dfs_radar_param},
	{CMD_SHOW_DFS_HELP, priv_driver_show_dfs_help},
	{CMD_SHOW_DFS_CAC_TIME, priv_driver_show_dfs_cac_time},
#endif
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
	{CMD_SET_CALBACKUP_TEST_DRV_FW, priv_driver_set_calbackup_test_drv_fw},
#endif
#if CFG_WOW_SUPPORT
	{CMD_WOW_START, priv_driver_set_wow},
	{CMD_SET_WOW_ENABLE, priv_driver_set_wow_enable},
	{CMD_SET_WOW_PAR, priv_driver_set_wow_par},
	{CMD_SET_WOW_UDP, priv_driver_set_wow_udpport},
	{CMD_SET_WOW_TCP, priv_driver_set_wow_tcpport},
	{CMD_GET_WOW_PORT, priv_driver_get_wow_port},
#endif
	{CMD_SET_ADV_PWS, priv_driver_set_adv_pws},
	{CMD_SET_MDTIM, priv_driver_set_mdtim},
#if CFG_SUPPORT_QA_TOOL
	{CMD_GET_RX_STATISTICS, priv_driver_get_rx_statistics},
#if CFG_SUPPORT_BUFFER_MODE
	{CMD_SETBUFMODE, priv_driver_set_efuse_buffer_mode},
#endif
#endif
#if CFG_SUPPORT_MSP
#if 0
	{CMD_GET_STAT, priv_driver_get_stat},
#endif
	{CMD_GET_STA_STATISTICS, priv_driver_get_sta_statistics},
	{CMD_GET_BSS_STATISTICS, priv_driver_get_bss_statistics},
	{CMD_GET_STA_IDX, priv_driver_get_sta_index},
	{CMD_GET_STA_INFO, priv_driver_get_sta_info},
	{CMD_GET_WTBL_INFO, priv_driver_get_wtbl_info},
	{CMD_GET_MIB_INFO, priv_driver_get_mib_info},
	{CMD_SET_FW_LOG, priv_driver_set_fw_log},
#endif
	{CMD_SET_CFG, priv_driver_set_cfg},
	{CMD_GET_CFG, priv_driver_get_cfg},
	{CMD_SET_EM_CFG, priv_driver_set_em_cfg},
	{CMD_GET_EM_CFG, priv_driver_get_em_cfg},
	{CMD_SET_CHIP, priv_driver_set_chip_config},
	{CMD_GET_CHIP, priv_driver_get_chip_config},
	{CMD_GET_VERSION, priv_driver_get_version},
	{CMD_GET_CNM, priv_driver_get_cnm},
	{CMD_GET_CAPAB_RSDB, priv_driver_get_capab_rsdb},
#if CFG_SUPPORT_DBDC
	{CMD_SET_DBDC, priv_driver_set_dbdc},
#endif /*CFG_SUPPORT_DBDC*/
#if CFG_SUPPORT_SNIFFER
	{CMD_SETMONITOR, priv_driver_set_monitor},
#endif
	{CMD_GET_QUE_INFO, priv_driver_get_que_info},
	{CMD_GET_MEM_INFO, priv_driver_get_mem_info},
	{CMD_GET_HIF_INFO, priv_driver_get_hif_info},
	{CMD_GET_TP_INFO, priv_driver_get_tp_info},
	{CMD_GET_CH_RANK_LIST, priv_driver_get_ch_rank_list},
	{CMD_GET_CH_DIRTINESS, priv_driver_get_ch_dirtiness},
	{CMD_EFUSE, priv_driver_efuse_ops},
#if defined(_HIF_SDIO) && (MTK_WCN_HIF_SDIO == 0)
	{CMD_CCCR, priv_driver_cccr_ops},
#endif /* _HIF_SDIO && (MTK_WCN_HIF_SDIO == 0) */
#if CFG_SUPPORT_ADVANCE_CONTROL
	{CMD_SET_NOISE, priv_driver_set_noise},
	{CMD_GET_NOISE, priv_driver_get_noise},
	{CMD_SET_POP, priv_driver_set_pop},
	{CMD_SET_ED, priv_driver_set_ed},
	{CMD_SET_PD, priv_driver_set_pd},
	{CMD_SET_MAX_RFGAIN, priv_driver_set_maxrfgain},
#endif
	{CMD_SET_DRV_SER, priv_driver_set_drv_ser},
	{CMD_SET_SW_AMSDU_NUM, priv_driver_set_amsdu_num},
	{CMD_SET_SW_AMSDU_SIZE, priv_driver_set_amsdu_size},
#if CFG_ENABLE_WIFI_DIRECT
	{CMD_P2P_SET_PS, priv_driver_set_p2p_ps},
	{CMD_P2P_SET_NOA, priv_driver_set_p2p_noa},
#endif
#ifdef UT_TEST_MODE
	{CMD_RUN_UT, priv_driver_run_ut},
#endif /* UT_TEST_MODE */
	{CMD_GET_WIFI_TYPE, priv_driver_get_wifi_type},
#if CFG_SUPPORT_DYNAMIC_PWR_LIMIT
	{CMD_SET_PWR_CTRL, priv_driver_set_power_control},
#endif
#if CFG_SUPPORT_ANT_SWAP
	{CMD_SET_ANT_SWAP, priv_driver_set_ant_swap},
#endif
#if (CFG_SUPPORT_CONNINFRA == 1)
	{CMD_SET_WHOLE_CHIP_RESET, priv_driver_trigger_whole_chip_reset},
	{CMD_SET_WFSYS_RESET, priv_driver_trigger_wfsys_reset},
#endif
#if (CFG_SUPPORT_CONNAC2X == 1)
	{CMD_GET_FWTBL_UMAC, priv_driver_get_umac_fwtbl},
#endif /* CFG_SUPPORT_CONNAC2X == 1 */
	{CMD_SHOW_TXD_INFO, priv_driver_show_txd_info},
	{CMD_GET_MU_RX_PKTCNT, priv_driver_show_rx_stat},
	{CMD_RUN_HQA, priv_driver_run_hqa},
	{CMD_CALIBRATION, priv_driver_calibration},
	{CMD_SET_STA1NSS, priv_driver_set_sta1ss},
	{CMD_SET_NVRAM, priv_driver_set_nvram},
	{CMD_GET_NVRAM, priv_driver_get_nvram},
#if CFG_SUPPORT_SCAN_EXT_FLAG
	{CMD_SET_SCAN_EXT_FLAG, priv_driver_set_scan_ext_flag},
#endif
};

int32_t priv_driver_cmds(IN struct net_device *prNetDev, IN int8_t *pcCommand,
			 IN int32_t i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4CmdFound = 0;
	int i;

	if (g_u4HaltFlag) {
		DBGLOG(REQ, WARN, "wlan is halt, skip priv_driver_cmds\n");
		return -1;
	}

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	for (i = 0; i < sizeof(priv_cmd_handlers) / sizeof(struct
			PRIV_CMD_HANDLER); i++) {
		if (strnicmp(pcCommand,
				priv_cmd_handlers[i].pcCmdStr,
				strlen(priv_cmd_handlers[i].pcCmdStr)) == 0) {

			if (priv_cmd_handlers[i].pfHandler != NULL) {
				i4BytesWritten =
					priv_cmd_handlers[i].pfHandler(
					prNetDev,
					pcCommand,
					i4TotalLen);
				i4CmdFound = 1;
			}
		}
	}

	if (i4CmdFound == 0) {
		i4CmdFound = 1;
		if (strnicmp(pcCommand, CMD_RSSI, strlen(CMD_RSSI)) == 0) {
			/* i4BytesWritten =
			 *  wl_android_get_rssi(net, command, i4TotalLen);
			 */
#if CFG_SUPPORT_BATCH_SCAN
		} else if (strnicmp(pcCommand, CMD_BATCH_SET,
			   strlen(CMD_BATCH_SET)) == 0) {
			kalIoctl(prGlueInfo, wlanoidSetBatchScanReq,
				 (void *) pcCommand, i4TotalLen,
				 FALSE, FALSE, TRUE, &i4BytesWritten);
		} else if (strnicmp(pcCommand, CMD_BATCH_GET,
			   strlen(CMD_BATCH_GET)) == 0) {
			/* strcpy(pcCommand, "BATCH SCAN DATA FROM FIRMWARE");
			 */
			/* i4BytesWritten =
			 *		strlen("BATCH SCAN DATA FROM FIRMWARE")
			 *		+ 1;
			 */
			/* i4BytesWritten = priv_driver_get_linkspeed (prNetDev,
			 *                  pcCommand, i4TotalLen);
			 */

			uint32_t u4BufLen;
			int i;
			/* int rlen=0; */

			for (i = 0; i < CFG_BATCH_MAX_MSCAN; i++) {
				/* for get which mscan */
				g_rEventBatchResult[i].ucScanCount = i + 1;

				kalIoctl(prGlueInfo,
					 wlanoidQueryBatchScanResult,
					 (void *)&g_rEventBatchResult[i],
					 sizeof(struct EVENT_BATCH_RESULT),
					 TRUE, TRUE, TRUE, &u4BufLen);
			}

#if 0
			DBGLOG(SCN, INFO,
			       "Batch Scan Results, scan count = %u\n",
			       g_rEventBatchResult.ucScanCount);
			for (i = 0; i < g_rEventBatchResult.ucScanCount; i++) {
				prEntry = &g_rEventBatchResult.arBatchResult[i];
				DBGLOG(SCN, INFO, "Entry %u\n", i);
				DBGLOG(SCN, INFO, "	 BSSID = " MACSTR "\n",
				       MAC2STR(prEntry->aucBssid));
				DBGLOG(SCN, INFO, "	 SSID = %s\n",
				       prEntry->aucSSID);
				DBGLOG(SCN, INFO, "	 SSID len = %u\n",
				       prEntry->ucSSIDLen);
				DBGLOG(SCN, INFO, "	 RSSI = %d\n",
				       prEntry->cRssi);
				DBGLOG(SCN, INFO, "	 Freq = %u\n",
				       prEntry->ucFreq);
			}
#endif

			batchConvertResult(&g_rEventBatchResult[0], pcCommand,
					   i4TotalLen, &i4BytesWritten);

			/* Dump for debug */
			/* print_hex_dump(KERN_INFO,
			 *  "BATCH", DUMP_PREFIX_ADDRESS, 16, 1, pcCommand,
			 *  i4BytesWritten, TRUE);
			 */

		} else if (strnicmp(pcCommand, CMD_BATCH_STOP,
			   strlen(CMD_BATCH_STOP)) == 0) {
			kalIoctl(prGlueInfo, wlanoidSetBatchScanReq,
				 (void *) pcCommand, i4TotalLen,
				 FALSE, FALSE, TRUE, &i4BytesWritten);
#endif
		}
#if CFG_SUPPORT_WIFI_SYSDVT
		else if (strnicmp(pcCommand, CMD_SET_TXS_TEST,
			strlen(CMD_SET_TXS_TEST)) == 0)
			i4BytesWritten =
			priv_driver_txs_test(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_TXS_TEST_RESULT,
			strlen(CMD_SET_TXS_TEST_RESULT)) == 0)
			i4BytesWritten =
			priv_driver_txs_test_result(prNetDev,
				pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_RXV_TEST,
			strlen(CMD_SET_RXV_TEST)) == 0)
			i4BytesWritten =
			priv_driver_rxv_test(prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_RXV_TEST_RESULT,
			strlen(CMD_SET_RXV_TEST_RESULT)) == 0)
			i4BytesWritten =
			priv_driver_rxv_test_result(prNetDev,
				pcCommand, i4TotalLen);
#if CFG_TCP_IP_CHKSUM_OFFLOAD
		else if (strnicmp(pcCommand, CMD_SET_CSO_TEST,
			strlen(CMD_SET_CSO_TEST)) == 0)
			i4BytesWritten =
			priv_driver_cso_test(prNetDev, pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_SET_TX_AC_TEST,
			strlen(CMD_SET_TX_AC_TEST)) == 0)
			i4BytesWritten =
			priv_driver_set_tx_test_ac(prNetDev,
				pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_TX_TEST,
			strlen(CMD_SET_TX_TEST)) == 0)
			i4BytesWritten =
			priv_driver_set_tx_test(prNetDev,
				pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_SKIP_CH_CHECK,
			strlen(CMD_SET_SKIP_CH_CHECK)) == 0)
			i4BytesWritten =
			priv_driver_skip_legal_ch_check(prNetDev,
				pcCommand, i4TotalLen);
#if (CFG_SUPPORT_DMASHDL_SYSDVT)
		else if (strnicmp(pcCommand, CMD_SET_DMASHDL_DUMP,
			strlen(CMD_SET_DMASHDL_DUMP)) == 0)
			i4BytesWritten =
			priv_driver_show_dmashdl_allcr(prNetDev,
				pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_DMASHDL_DVT_ITEM,
			strlen(CMD_SET_DMASHDL_DVT_ITEM)) == 0)
			i4BytesWritten =
			priv_driver_dmashdl_dvt_item(prNetDev,
				pcCommand, i4TotalLen);
#endif /* CFG_SUPPORT_DMASHDL_SYSDVT */
#endif /* CFG_SUPPORT_WIFI_SYSDVT */
		else if (strnicmp(pcCommand, CMD_DBG_SHOW_TR_INFO,
				strlen(CMD_DBG_SHOW_TR_INFO)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidShowPdmaInfo,
				 (void *) pcCommand, i4TotalLen,
				 FALSE, FALSE, TRUE, &i4BytesWritten);
		} else if (strnicmp(pcCommand, CMD_DBG_SHOW_PLE_INFO,
				strlen(CMD_DBG_SHOW_PLE_INFO)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidShowPleInfo,
				 (void *) pcCommand, i4TotalLen,
				 FALSE, FALSE, TRUE, &i4BytesWritten);
		} else if (strnicmp(pcCommand, CMD_DBG_SHOW_PSE_INFO,
				strlen(CMD_DBG_SHOW_PSE_INFO)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidShowPseInfo,
				 (void *) pcCommand, i4TotalLen,
				 FALSE, FALSE, TRUE, &i4BytesWritten);
		} else if (strnicmp(pcCommand, CMD_DBG_SHOW_CSR_INFO,
				strlen(CMD_DBG_SHOW_CSR_INFO)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidShowCsrInfo,
				 (void *) pcCommand, i4TotalLen,
				 FALSE, FALSE, TRUE, &i4BytesWritten);
		} else if (strnicmp(pcCommand, CMD_DBG_SHOW_DMASCH_INFO,
				strlen(CMD_DBG_SHOW_DMASCH_INFO)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidShowDmaschInfo,
				 (void *) pcCommand, i4TotalLen,
				 FALSE, FALSE, TRUE, &i4BytesWritten);
#if CFG_SUPPORT_EASY_DEBUG
		} else if (strnicmp(pcCommand, CMD_FW_PARAM,
				strlen(CMD_FW_PARAM)) == 0) {
			kalIoctl(prGlueInfo, wlanoidSetFwParam,
				 (void *)(pcCommand + 13),
				 i4TotalLen - 13, FALSE, FALSE, FALSE,
				 &i4BytesWritten);
#endif /* CFG_SUPPORT_EASY_DEBUG */
			} else if (strnicmp(pcCommand, CMD_GET_WFDMA_INFO,
					strlen(CMD_GET_WFDMA_INFO)) == 0) {
				kalIoctl(prGlueInfo,
					 wlanoidShowPdmaInfo,
					 (void *) pcCommand, i4TotalLen,
					 FALSE, FALSE, TRUE, &i4BytesWritten);
			} else if (strnicmp(pcCommand, CMD_GET_PLE_INFO,
					strlen(CMD_GET_PLE_INFO)) == 0) {
				kalIoctl(prGlueInfo,
					 wlanoidShowPleInfo,
					 (void *) pcCommand, i4TotalLen,
					 FALSE, FALSE, TRUE, &i4BytesWritten);
			} else if (strnicmp(pcCommand, CMD_GET_PSE_INFO,
					strlen(CMD_GET_PSE_INFO)) == 0) {
				kalIoctl(prGlueInfo,
					 wlanoidShowPseInfo,
					 (void *) pcCommand, i4TotalLen,
					 FALSE, FALSE, TRUE, &i4BytesWritten);
			} else if (strnicmp(pcCommand, CMD_GET_DMASCH_INFO,
					strlen(CMD_GET_DMASCH_INFO)) == 0) {
				kalIoctl(prGlueInfo,
					 wlanoidShowDmaschInfo,
					 (void *) pcCommand, i4TotalLen,
					 FALSE, FALSE, TRUE, &i4BytesWritten);
		} else if (!strnicmp(pcCommand, CMD_DUMP_TS,
				     strlen(CMD_DUMP_TS)) ||
			   !strnicmp(pcCommand, CMD_ADD_TS,
				     strlen(CMD_ADD_TS)) ||
			   !strnicmp(pcCommand, CMD_DEL_TS,
				     strlen(CMD_DEL_TS))) {
			kalIoctl(prGlueInfo, wlanoidTspecOperation,
				 (void *)pcCommand, i4TotalLen, FALSE, FALSE,
				 FALSE, &i4BytesWritten);
		} else if (kalStrStr(pcCommand, "-IT")) {
			kalIoctl(prGlueInfo, wlanoidPktProcessIT,
				 (void *)pcCommand, i4TotalLen, FALSE, FALSE,
				 FALSE, &i4BytesWritten);
		} else if (!strnicmp(pcCommand, CMD_FW_EVENT, 9)) {
			kalIoctl(prGlueInfo, wlanoidFwEventIT,
				 (void *)(pcCommand + 9), i4TotalLen, FALSE,
				 FALSE, FALSE, &i4BytesWritten);
		} else if (!strnicmp(pcCommand, CMD_DUMP_UAPSD,
				     strlen(CMD_DUMP_UAPSD))) {
			kalIoctl(prGlueInfo, wlanoidDumpUapsdSetting,
				 (void *)pcCommand, i4TotalLen, FALSE, FALSE,
				 FALSE, &i4BytesWritten);
#if CFG_SUPPORT_DYNAMIC_PWR_LIMIT
		} else if (strnicmp(pcCommand, CMD_SET_PWR_CTRL,
				    strlen(CMD_SET_PWR_CTRL)) == 0) {
			priv_driver_set_power_control(prNetDev,
						      pcCommand, i4TotalLen);
#endif
#if CFG_SUPPORT_ANT_SWAP
                } else if (strnicmp(pcCommand, CMD_SET_ANT_SWAP,
                            strlen(CMD_SET_ANT_SWAP)) == 0) {
                    priv_driver_set_ant_swap(prNetDev,
                                      pcCommand, i4TotalLen);
#endif
		} else if (strnicmp(pcCommand, CMD_SET_BF,
			   strlen(CMD_SET_BF)) == 0) {
			i4BytesWritten = priv_driver_set_bf(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_NSS,
			   strlen(CMD_SET_NSS)) == 0) {
			i4BytesWritten = priv_driver_set_nss(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_AMSDU_TX,
			   strlen(CMD_SET_AMSDU_TX)) == 0) {
			i4BytesWritten = priv_driver_set_amsdu_tx(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_AMSDU_RX,
			   strlen(CMD_SET_AMSDU_RX)) == 0) {
			i4BytesWritten = priv_driver_set_amsdu_rx(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_AMPDU_TX,
			   strlen(CMD_SET_AMPDU_TX)) == 0) {
			i4BytesWritten = priv_driver_set_ampdu_tx(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_AMPDU_RX,
			   strlen(CMD_SET_AMPDU_RX)) == 0) {
			i4BytesWritten = priv_driver_set_ampdu_rx(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_QOS,
			   strlen(CMD_SET_QOS)) == 0) {
			i4BytesWritten = priv_driver_set_qos(prNetDev,
							pcCommand, i4TotalLen);
#if (CFG_SUPPORT_802_11AX == 1)
		} else if (strnicmp(pcCommand, CMD_SET_MUEDCA_OVERRIDE,
			   strlen(CMD_SET_MUEDCA_OVERRIDE)) == 0) {
			i4BytesWritten = priv_driver_muedca_override(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_BA_SIZE,
			   strlen(CMD_SET_BA_SIZE)) == 0) {
			i4BytesWritten = priv_driver_set_ba_size(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_TP_TEST_MODE,
			   strlen(CMD_SET_TP_TEST_MODE)) == 0) {
			i4BytesWritten = priv_driver_set_tp_test_mode(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_TX_MCSMAP,
			   strlen(CMD_SET_TX_MCSMAP)) == 0) {
			i4BytesWritten = priv_driver_set_mcsmap(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_TX_PPDU,
			   strlen(CMD_SET_TX_PPDU)) == 0) {
			i4BytesWritten = priv_driver_set_tx_ppdu(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_LDPC,
			   strlen(CMD_SET_LDPC)) == 0) {
			i4BytesWritten = priv_driver_set_ldpc(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_FORCE_AMSDU_TX,
			   strlen(CMD_FORCE_AMSDU_TX)) == 0) {
			i4BytesWritten = priv_driver_set_tx_force_amsdu(
				prNetDev, pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_OM_CH_BW,
			   strlen(CMD_SET_OM_CH_BW)) == 0) {
			i4BytesWritten = priv_driver_set_om_ch_bw(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_OM_RX_NSS,
			   strlen(CMD_SET_OM_RX_NSS)) == 0) {
			i4BytesWritten = priv_driver_set_om_rx_nss(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_OM_TX_NSS,
			   strlen(CMD_SET_OM_TX_NSS)) == 0) {
			i4BytesWritten = priv_driver_set_om_tx_nss(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_OM_MU_DISABLE,
			   strlen(CMD_SET_OM_MU_DISABLE)) == 0) {
			i4BytesWritten = priv_driver_set_om_mu_dis(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_TX_OM_PACKET,
			   strlen(CMD_SET_TX_OM_PACKET)) == 0) {
			i4BytesWritten = priv_driver_set_tx_om_packet(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_TX_CCK_1M_PWR,
			   strlen(CMD_SET_TX_CCK_1M_PWR)) == 0) {
			i4BytesWritten = priv_driver_set_tx_cck_1m_pwr(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_PAD_DUR,
			strlen(CMD_SET_PAD_DUR)) == 0) {
			i4BytesWritten = priv_driver_set_pad_dur(prNetDev,
				pcCommand, i4TotalLen);
#endif

#if CFG_CHIP_RESET_HANG
		} else if (strnicmp(pcCommand, CMD_SET_RST_HANG,
				strlen(CMD_SET_RST_HANG)) == 0) {
			i4BytesWritten = priv_driver_set_rst_hang(
				prNetDev, pcCommand, i4TotalLen);
#endif

#if (CFG_SUPPORT_TWT == 1)
		} else if (strnicmp(pcCommand, CMD_SET_TWT_PARAMS,
			   strlen(CMD_SET_TWT_PARAMS)) == 0) {
			i4BytesWritten = priv_driver_set_twtparams(prNetDev,
				pcCommand, i4TotalLen);
#endif
		} else
				i4BytesWritten = priv_cmd_not_support
				(prNetDev, pcCommand, i4TotalLen);
	}

	if (i4BytesWritten >= 0) {
		if ((i4BytesWritten == 0) && (i4TotalLen > 0)) {
			/* reset the command buffer */
			pcCommand[0] = '\0';
		}

		if (i4BytesWritten >= i4TotalLen) {
			DBGLOG(REQ, INFO,
			       "%s: i4BytesWritten %d > i4TotalLen < %d\n",
			       __func__, i4BytesWritten, i4TotalLen);
			i4BytesWritten = i4TotalLen - 1;
		}
		pcCommand[i4BytesWritten] = '\0';
		i4BytesWritten++;

	}

	return i4BytesWritten;

} /* priv_driver_cmds */

#ifdef CFG_ANDROID_AOSP_PRIV_CMD
int android_private_support_driver_cmd(IN struct net_device *prNetDev,
	IN OUT struct ifreq *prReq, IN int i4Cmd)
{
	struct android_wifi_priv_cmd priv_cmd;
	char *command = NULL;
	int ret = 0, bytes_written = 0;

	if (!prReq->ifr_data)
		return -EINVAL;

	if (copy_from_user(&priv_cmd, prReq->ifr_data, sizeof(priv_cmd)))
		return -EFAULT;
	/* total_len is controlled by the user. need check length */
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
		bytes_written = kalSnprintf(command, priv_cmd.total_len,
						"%s", "NotSupport");
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

int priv_support_driver_cmd(IN struct net_device *prNetDev,
			    IN OUT struct ifreq *prReq, IN int i4Cmd)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int ret = 0;
	char *pcCommand = NULL;
	struct priv_driver_cmd_s *priv_cmd = NULL;
	int i4BytesWritten = 0;
	int i4TotalLen = 0;

	if (!prReq->ifr_data) {
		ret = -EINVAL;
		goto exit;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo) {
		DBGLOG(REQ, WARN, "No glue info\n");
		ret = -EFAULT;
		goto exit;
	}
	if (prGlueInfo->u4ReadyFlag == 0) {
		ret = -EINVAL;
		goto exit;
	}

	priv_cmd = kzalloc(sizeof(struct priv_driver_cmd_s), GFP_KERNEL);
	if (!priv_cmd) {
		DBGLOG(REQ, WARN, "%s, alloc mem failed\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(priv_cmd, prReq->ifr_data,
	    sizeof(struct priv_driver_cmd_s))) {
		DBGLOG(REQ, INFO, "%s: copy_from_user fail\n", __func__);
		ret = -EFAULT;
		goto exit;
	}

	i4TotalLen = priv_cmd->total_len;

	if (i4TotalLen <= 0 || i4TotalLen > PRIV_CMD_SIZE) {
		ret = -EINVAL;
		DBGLOG(REQ, INFO, "%s: i4TotalLen invalid\n", __func__);
		goto exit;
	}
	priv_cmd->buf[PRIV_CMD_SIZE - 1] = '\0';
	pcCommand = priv_cmd->buf;

	DBGLOG(REQ, INFO, "%s: driver cmd \"%s\" on %s\n", __func__, pcCommand,
	       prReq->ifr_name);

	i4BytesWritten = priv_driver_cmds(prNetDev, pcCommand, i4TotalLen);

	if (i4BytesWritten == -EOPNOTSUPP) {
		/* Report positive status */
		i4BytesWritten = kalSnprintf(pcCommand, i4TotalLen,
					    "%s", "UNSUPPORTED");
		i4BytesWritten++;
	}

	if (i4BytesWritten >= 0) {
		priv_cmd->used_len = i4BytesWritten;
		if ((i4BytesWritten == 0) && (priv_cmd->total_len > 0))
			pcCommand[0] = '\0';
		if (i4BytesWritten >= priv_cmd->total_len)
			i4BytesWritten = priv_cmd->total_len;
		else
			i4BytesWritten++;
		priv_cmd->used_len = i4BytesWritten;
		if (copy_to_user(prReq->ifr_data, priv_cmd,
				sizeof(struct priv_driver_cmd_s))) {
			ret = -EFAULT;
			DBGLOG(REQ, INFO, "copy fail");
		}
	} else
		ret = i4BytesWritten;

exit:
	kfree(priv_cmd);

	return ret;
}				/* priv_support_driver_cmd */

#if CFG_SUPPORT_DYNAMIC_PWR_LIMIT
/* dynamic tx power control */
static int priv_driver_set_power_control(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_TX_PWR_CTRL_IOCTL rPwrCtrlParam = { 0 };
	u_int8_t fgIndex = FALSE;
	char *ptr = pcCommand, *ptr2 = NULL;
	char *str = NULL, *cmd = NULL, *name = NULL, *setting = NULL;
	uint8_t index = 0;
	uint32_t u4SetInfoLen = 0;

	while ((str = strsep(&ptr, " ")) != NULL) {
		if (kalStrLen(str) <= 0)
			continue;
		if (cmd == NULL)
			cmd = str;
		else if (name == NULL)
			name = str;
		else if (fgIndex == FALSE) {
			ptr2 = str;
			if (kalkStrtou8(str, 0, &index) != 0) {
				DBGLOG(REQ, INFO,
				       "index is wrong: %s\n", ptr2);
				return -1;
			}
			fgIndex = TRUE;
		} else if (setting == NULL) {
			setting = str;
			break;
		}
	}

	if ((name == NULL) || (fgIndex == FALSE)) {
		DBGLOG(REQ, INFO, "name(%s) or fgIndex(%d) is wrong\n",
		       name, fgIndex);
		return -1;
	}

	rPwrCtrlParam.fgApplied = (index == 0) ? FALSE : TRUE;
	rPwrCtrlParam.name = name;
	rPwrCtrlParam.index = index;
	rPwrCtrlParam.newSetting = setting;

	DBGLOG(REQ, INFO, "applied=[%d], name=[%s], index=[%u], setting=[%s]\n",
	       rPwrCtrlParam.fgApplied,
	       rPwrCtrlParam.name,
	       rPwrCtrlParam.index,
	       rPwrCtrlParam.newSetting);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	kalIoctl(prGlueInfo,
		 wlanoidTxPowerControl,
		 (void *)&rPwrCtrlParam,
		 sizeof(struct PARAM_TX_PWR_CTRL_IOCTL),
		 FALSE,
		 FALSE,
		 TRUE,
		 &u4SetInfoLen);

	return 0;
}
#endif

#if CFG_SUPPORT_ANT_SWAP
/* set ant swap */
static int priv_driver_set_ant_swap(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
       int ant_swap = 0;
       char *chain0_main = "SET_CHIP AntSwapManualCtrl 2 0";
       char *chain0_aux = "SET_CHIP AntSwapManualCtrl 2 1";

       ant_swap = atoi(pcCommand[13]);
       DBGLOG(REQ, INFO, "%s ant_swap = %d \n", __func__, ant_swap);

       i4TotalLen = kalStrLen(chain0_main) + 1;
       if (ant_swap == 0) {
           priv_driver_set_chip_config(prNetDev, chain0_main, i4TotalLen);
       } else if (ant_swap == 1) {
           priv_driver_set_chip_config(prNetDev, chain0_aux, i4TotalLen);
       } else {
           DBGLOG(REQ, ERROR, "%s ant_swap err, value = %d \n", __func__, ant_swap);
           return -1;
       }

	return 0;
}
#endif
#if CFG_SUPPORT_SCAN_EXT_FLAG
/* Xiaom ADD
 * argv: 1 means that force scan on gaming mode
 */
static int priv_driver_set_scan_ext_flag(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Argc = 0;
	int32_t u4Ret = 0;
	uint32_t u4ScanExtFlag = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: Scan Ext Flag\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4ScanExtFlag);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse scan ext flag error u4Ret=%d\n", u4Ret);
	if (prGlueInfo == NULL) {
		DBGLOG(REQ, ERROR, "prGlueInfo is null\n");
		return -1;
	}
	prGlueInfo->u4ScanExtFlag = u4ScanExtFlag;
	return WLAN_STATUS_SUCCESS;
}
#endif
