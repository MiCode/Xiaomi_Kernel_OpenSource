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

#define CFG_STAT_DBG_PEER_NUM	10

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

#if CFG_SUPPORT_WPS2
	{
		OID_802_11_WSC_ASSOC_INFO,
		DISP_STRING("OID_802_11_WSC_ASSOC_INFO"),
		FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
		NULL,
		(PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWSCAssocInfo
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
};

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
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
#if CFG_MTK_ENGINEER_MODE_SUPPORT
	case IOCTL_SET_STRUCT_FOR_EM:
#endif
		return priv_set_struct(prNetDev, &rIwReqInfo, &prIwReq->u,
				       (char *) &(prIwReq->u));

	case IOCTL_GET_STRUCT:
		return priv_get_struct(prNetDev, &rIwReqInfo, &prIwReq->u,
				       (char *) &(prIwReq->u));

#if (CFG_SUPPORT_QA_TOOL)
	case IOCTL_QA_TOOL_DAEMON:
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
		return priv_set_ap(prNetDev, &rIwReqInfo, &(prIwReq->u),
				     (char *) &(prIwReq->u));
#if CFG_SUPPORT_WAC
	case IOCTL_SET_DRIVER:
		return priv_set_struct(prNetDev, &rIwReqInfo, &prIwReq->u,
				       (char *) &(prIwReq->u));
#endif

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

void
parseNoiseHistogramReport(uint32_t *i4BytesWritten, char *pcCommand,
		int *i4TotalLen, IN struct CMD_NOISE_HISTOGRAM_REPORT *cmd)
{
	if (cmd->ucAction == CMD_NOISE_HISTOGRAM_GET) {
		*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
					*i4TotalLen - *i4BytesWritten,
					"\nWF0 Noise IPI");
#if CFG_IPI_2CHAIN_SUPPORT
	} else if (cmd->ucAction == CMD_NOISE_HISTOGRAM_GET2) {
		*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
					*i4TotalLen - *i4BytesWritten,
					"\nWF1 Noise IPI");
#endif /* CFG_IPI_2CHAIN_SUPPORT */
	}

	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n       Power > -55: %10d", cmd->u4IPI10);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-55 >= Power > -60: %10d", cmd->u4IPI9);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-60 >= Power > -65: %10d", cmd->u4IPI8);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-65 >= Power > -70: %10d", cmd->u4IPI7);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-70 >= Power > -75: %10d", cmd->u4IPI6);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-75 >= Power > -80: %10d", cmd->u4IPI5);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-80 >= Power > -83: %10d", cmd->u4IPI4);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-83 >= Power > -86: %10d", cmd->u4IPI3);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-86 >= Power > -89: %10d", cmd->u4IPI2);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-89 >= Power > -92: %10d", cmd->u4IPI1);
	*i4BytesWritten += snprintf(pcCommand + *i4BytesWritten,
			*i4TotalLen - *i4BytesWritten,
			"\n-92 >= Power      : %10d", cmd->u4IPI0);
}

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
priv_set_int(IN struct net_device *prNetDev,
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
		struct BSS_INFO *prBssInfo =
				prGlueInfo->prAdapter->prAisBssInfo;

		if (!prBssInfo)
			break;

		rPowerMode.ePowerMode = (enum PARAM_POWER_MODE)
					pu4IntBuf[1];
		rPowerMode.ucBssIdx = prBssInfo->ucBssIndex;

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
	     IN struct iw_request_info *prIwReqInfo,
	     IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	uint32_t u4SubCmd;
	uint32_t *pu4IntBuf;
	struct GLUE_INFO *prGlueInfo;
	uint32_t u4BufLen = 0;
	int status = 0;
	struct NDIS_TRANSPORT_STRUCT *prNdisReq;
	int32_t ch[MAX_CHN_NUM];

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
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_MTK_WIFI_TEST;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0) {
			prIwReqData->mode = *(uint32_t *)
					    &prNdisReq->ndisOidContent[4];

		}
		return status;

#if CFG_SUPPORT_PRIV_MCR_RW
	case PRIV_CMD_ACCESS_MCR:
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
			prIwReqData->mode = *(uint32_t *)
					    &prNdisReq->ndisOidContent[4];
		}
		return status;
#endif

	case PRIV_CMD_DUMP_MEM:
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

		if (!prGlueInfo->fgMcrAccessAllowed
		    || !capable(CAP_NET_ADMIN)) {
			DBGLOG(REQ, WARN, "Access Denied\n");
			status = 0;
			return status;
		}

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
		prNdisReq = (struct NDIS_TRANSPORT_STRUCT *) &aucOidBuf[0];

		kalMemCopy(&prNdisReq->ndisOidContent[0], &pu4IntBuf[1], 8);

		prNdisReq->ndisOidCmd = OID_CUSTOM_SW_CTRL;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		status = priv_get_ndis(prNetDev, prNdisReq, &u4BufLen);
		if (status == 0) {
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
		uint8_t NumOfChannel = MAX_CHN_NUM;
		uint8_t ucMaxChannelNum = MAX_CHN_NUM;
		struct RF_CHANNEL_INFO *aucChannelList;

		DBGLOG(RLM, INFO, "Domain: Query Channel List.\n");
		aucChannelList = (struct RF_CHANNEL_INFO *)
			kalMemAlloc(sizeof(struct RF_CHANNEL_INFO)*MAX_CHN_NUM,
				VIR_MEM_TYPE);
		if (!aucChannelList) {
			DBGLOG(REQ, ERROR,
				"Can not alloc memory for rf channel info\n");
			return -ENOMEM;
		}
		kalMemZero(aucChannelList,
			sizeof(struct RF_CHANNEL_INFO)*MAX_CHN_NUM);

		kalGetChannelList(prGlueInfo, BAND_NULL, ucMaxChannelNum,
				  &NumOfChannel, aucChannelList);
		if (NumOfChannel > MAX_CHN_NUM)
			NumOfChannel = MAX_CHN_NUM;

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
			sizeof(struct RF_CHANNEL_INFO)*MAX_CHN_NUM);

		prIwReqData->data.length = j;
		if (copy_to_user(prIwReqData->data.pointer, ch,
				 NumOfChannel * sizeof(int32_t)))
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
priv_set_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo,
	      IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	uint16_t i = 0;
	uint32_t u4SubCmd, u4BufLen, u4CmdLen;
	struct GLUE_INFO *prGlueInfo;
	int32_t  setting[4] = {0};
	int status = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct SET_TXPWR_CTRL *prTxpwr;

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);

	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	u4SubCmd = (uint32_t) prIwReqData->data.flags;
	u4CmdLen = (uint32_t) prIwReqData->data.length;

	switch (u4SubCmd) {
	case PRIV_CMD_SET_TX_POWER: {
		if (u4CmdLen > 4)
			return -EINVAL;
		if (copy_from_user(setting, prIwReqData->data.pointer,
				   u4CmdLen))
			return -EFAULT;

#if 0
		DBGLOG(INIT, INFO, "Tx power num = %d\n",
		       prIwReqData->data.length);

		DBGLOG(INIT, INFO,
		       "Tx power setting = %d %d %d %d\n", setting[0],
		       setting[1], setting[2], setting[3]);
#endif
		prTxpwr = &prGlueInfo->rTxPwr;
		if (setting[0] == 0
		    && prIwReqData->data.length == 4 /* argc num */) {
			/* 0 (All networks), 1 (legacy STA), 2 (Hotspot AP),
			 * 3 (P2P), 4 (BT over Wi-Fi)
			 */
			if (setting[1] == 1 || setting[1] == 0) {
				if (setting[2] == 0 || setting[2] == 1)
					prTxpwr->c2GLegacyStaPwrOffset =
								setting[3];
				if (setting[2] == 0 || setting[2] == 2)
					prTxpwr->c5GLegacyStaPwrOffset =
								setting[3];
			}
			if (setting[1] == 2 || setting[1] == 0) {
				if (setting[2] == 0 || setting[2] == 1)
					prTxpwr->c2GHotspotPwrOffset =
								setting[3];
				if (setting[2] == 0 || setting[2] == 2)
					prTxpwr->c5GHotspotPwrOffset =
								setting[3];
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
		} else if (setting[0] == 1
			   && prIwReqData->data.length == 2) {
			prTxpwr->ucConcurrencePolicy = setting[1];
		} else if (setting[0] == 2
			   && prIwReqData->data.length == 3) {
			if (setting[1] == 0) {
				for (i = 0; i < 14; i++)
					prTxpwr->acTxPwrLimit2G[i] = setting[2];
			} else if (setting[1] <= 14)
				prTxpwr->acTxPwrLimit2G[setting[1] - 1] =
								setting[2];
		} else if (setting[0] == 3
			   && prIwReqData->data.length == 3) {
			if (setting[1] == 0) {
				for (i = 0; i < 4; i++)
					prTxpwr->acTxPwrLimit5G[i] = setting[2];
			} else if (setting[1] <= 4)
				prTxpwr->acTxPwrLimit5G[setting[1] - 1] =
								setting[2];
		} else if (setting[0] == 4
			   && prIwReqData->data.length == 2) {
			if (setting[1] == 0)
				wlanDefTxPowerCfg(prGlueInfo->prAdapter);
			rStatus = kalIoctl(prGlueInfo, wlanoidSetTxPower,
					   prTxpwr,
					   sizeof(struct SET_TXPWR_CTRL),
					   FALSE, FALSE, TRUE, &u4BufLen);
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
	      IN struct iw_request_info *prIwReqInfo,
	      IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	uint32_t u4SubCmd;
	struct GLUE_INFO *prGlueInfo;
	int status = 0;
	int32_t ch[MAX_CHN_NUM];

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
		uint8_t NumOfChannel = MAX_CHN_NUM;
		uint8_t ucMaxChannelNum = MAX_CHN_NUM;
		struct RF_CHANNEL_INFO aucChannelList[MAX_CHN_NUM];

		kalGetChannelList(prGlueInfo, BAND_NULL, ucMaxChannelNum,
				  &NumOfChannel, aucChannelList);
		if (NumOfChannel > MAX_CHN_NUM)
			NumOfChannel = MAX_CHN_NUM;

		for (i = 0; i < NumOfChannel; i++)
			ch[i] = (int32_t) aucChannelList[i].ucChannelNum;

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
		/* retrieve IE for Probe Request */
		u4CmdLen = prIwReqData->data.length;
		if (u4CmdLen > GLUE_INFO_WSCIE_LENGTH) {
			DBGLOG(REQ, ERROR, "Input data length is invalid %u\n",
			       u4CmdLen);
			return -EINVAL;
		}

		if (prIwReqData->data.length > 0) {
			if (copy_from_user(prGlueInfo->aucWSCIE,
					   prIwReqData->data.pointer,
					   u4CmdLen)) {
				status = -EFAULT;
				break;
			}
			prGlueInfo->u2WSCIELen = u4CmdLen;
		} else {
			prGlueInfo->u2WSCIELen = 0;
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
		if (u4CmdLen > CMD_OID_BUF_LENGTH) {
			DBGLOG(REQ, ERROR, "Input data length is invalid %u\n",
			       u4CmdLen);
			return -EINVAL;
		}
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

#if CFG_SUPPORT_WAC
	case PRIV_CMD_WAC_IE:
		/* Set WAC IE */
		if (prIwReqData->data.length > 0) {
			if (prIwReqData->data.length > ELEM_MAX_LEN_WAC_INFO) {
				DBGLOG(REQ, ERROR, "exceed len(%ld),ignore\n",
					prIwReqData->data.length);
				break;
			}
			kalMemZero(prGlueInfo->prAdapter->
					rWifiVar.aucWACIECache,
			sizeof(prGlueInfo->prAdapter->rWifiVar.aucWACIECache));
			if (copy_from_user(prGlueInfo->prAdapter->
						rWifiVar.aucWACIECache,
						prIwReqData->data.pointer,
						prIwReqData->data.length)) {
				status = -EFAULT;
				DBGLOG(REQ, ERROR, "cp_f_us WACIE failed!\n");
				break;
			}

			prGlueInfo->prAdapter->rWifiVar.u2WACIELen =
					prIwReqData->data.length;
			DBGLOG(REQ, INFO, "Set WAC IE:\n");
			dumpMemory8(prGlueInfo->prAdapter->
						rWifiVar.aucWACIECache,
				prGlueInfo->prAdapter->rWifiVar.u2WACIELen);
		} else {
			prGlueInfo->prAdapter->rWifiVar.u2WACIELen = 0;
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
			kalMemCopy(pcExtra, prNdisReq,
				u4BufLen + sizeof(struct NDIS_TRANSPORT_STRUCT)
				- sizeof(prNdisReq->ndisOidContent));
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
	} else {
		if (prNdisReq->inNdisOidlength >
		    (sizeof(aucOidBuf) -
		    OFFSET_OF(struct NDIS_TRANSPORT_STRUCT, ndisOidContent))) {
			DBGLOG(REQ, INFO, "exceeds length limit\n");
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
	} else {
		if (prNdisReq->inNdisOidlength >
		    (sizeof(aucOidBuf) -
		    OFFSET_OF(struct NDIS_TRANSPORT_STRUCT, ndisOidContent))) {
			DBGLOG(REQ, INFO, "exceeds length limit\n");
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
priv_ate_set(IN struct net_device *prNetDev,
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
	DBGLOG(REQ, INFO, "MT6632: %s, u4SubCmd=%d\n", __func__,
	       u4SubCmd);

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
		if (!access_ok(VERIFY_READ, prIwReqData->data.pointer,
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
		pcExtra[prIwReqData->data.length - 1] = 0;
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
	    PARAM_MEDIA_STATE_CONNECTED)
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
#define CMD_GET_AP_CHANNELS		"GET_AP_CHANNELS"
#define CMD_P2P_SET_NOA			"P2P_SET_NOA"
#define CMD_P2P_GET_NOA			"P2P_GET_NOA"
#define CMD_P2P_SET_PS			"P2P_SET_PS"
#define CMD_SET_AP_WPS_P2P_IE		"SET_AP_WPS_P2P_IE"
#define CMD_SETROAMMODE			"SETROAMMODE"
#define CMD_MIRACAST			"MIRACAST"

#ifdef CFG_SUPPORT_ADJUST_MCC_STAY_TIME
#define CMD_MCCTIME		"MCCTIME"
#endif


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

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
#define CMD_GET_CH_RANK_LIST	"GET_CH_RANK_LIST"
#define CMD_GET_CH_DIRTINESS	"GET_CH_DIRTINESS"
#endif

#if CFG_SUPPORT_ANT_DIV
#define CMD_SET_ANT_DIV                 "ANT_DIV_SET"
#define CMD_GET_ANT_DIV                 "ANT_DIV_GET"
#define CMD_DETC_ANT_DIV                "ANT_DIV_DETC"
#define CMD_SWH_ANT_DIV                 "ANT_DIV_SWH"

#define CMD_SET_ANT_DIV_ARG_NUM		2
#define CMD_GET_ANT_DIV_ARG_NUM		1
#define CMD_DETC_ANT_DIV_ARG_NUM	1
#define CMD_SWH_ANT_DIV_ARG_NUM		1
#endif

#if CFG_CHIP_RESET_HANG
#define CMD_SET_RST_HANG                 "RST_HANG_SET"

#define CMD_SET_RST_HANG_ARG_NUM		2
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
#define CMD_SET_MCR		"SET_MCR"
#define CMD_GET_MCR		"GET_MCR"
#define CMD_SET_DRV_MCR		"SET_DRV_MCR"
#define CMD_GET_DRV_MCR		"GET_DRV_MCR"
#define CMD_SET_SW_CTRL	        "SET_SW_CTRL"
#define CMD_GET_SW_CTRL         "GET_SW_CTRL"
#define CMD_SET_CFG             "SET_CFG"
#define CMD_GET_CFG             "GET_CFG"
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
#ifdef CFG_ALPS_ANDROID_AOSP_PRIV_CMD
#define PRIV_CMD_SIZE 512
#else
#define PRIV_CMD_SIZE 2000
#endif
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
#define CMD_GET_IPI		"GET_IPI"

#if CFG_SUPPORT_ADJUST_MCC_MODE_SET
#define CMD_SET_MCC_MODE   "SET_MCHAN_SCHED_MODE"
#endif

#if CFG_WOW_SUPPORT
#define CMD_WOW_START		"WOW_START"
#define CMD_SET_WOW_ENABLE	"SET_WOW_ENABLE"
#define CMD_SET_WOW_PAR		"SET_WOW_PAR"
#define CMD_SET_WOW_UDP		"SET_WOW_UDP"
#define CMD_SET_WOW_TCP		"SET_WOW_TCP"
#define CMD_GET_WOW_PORT	"GET_WOW_PORT"
#define CMD_GET_WOW_REASON	"GET_WOW_REASON"
#define CMD_SET_SUSP_CMD	"sET_SUSP_CMD"
#define CMD_SET_MDNS_OFFLOAD_ENABLE	    "ENABLE_MDNS_OFFLOADING"
#define CMD_SET_SHOW_CACHE    "SHOW_MDNS_CACHE"


#endif
#define CMD_SET_ADV_PWS		"SET_ADV_PWS"
#define CMD_SET_MDTIM		"SET_MDTIM"
#define CMD_GET_DSLP_CNT	"GET_DSLEEP_CNT"
#define CMD_ENFORCE_POWER_MODE  "ENFORCE_POWER_MODE"
#define CMD_GET_POWER_MODE  "GET_POWER_MODE"

#define CMD_SET_DBDC		"SET_DBDC"

#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
#define CMD_SET_CALBACKUP_TEST_DRV_FW		"SET_CALBACKUP_TEST_DRV_FW"
#endif

#define CMD_GET_CNM		"GET_CNM"

#define CMD_SET_SW_AMSDU_NUM      "SET_SW_AMSDU_NUM"
#define CMD_SET_SW_AMSDU_SIZE      "SET_SW_AMSDU_SIZE"

#define CMD_SET_DRV_SER           "SET_DRV_SER"

/* Debug for consys */
#define CMD_DBG_SHOW_TR_INFO			"show-tr"
#define CMD_DBG_SHOW_PLE_INFO			"show-ple"
#define CMD_DBG_SHOW_PSE_INFO			"show-pse"
#define CMD_DBG_SHOW_CSR_INFO			"show-csr"
#define CMD_DBG_SHOW_DMASCH_INFO		"show-dmasch"

#if CFG_SUPPORT_EASY_DEBUG
#define CMD_FW_PARAM				"set_fw_param"
#endif /* CFG_SUPPORT_EASY_DEBUG */

#if CFG_SUPPORT_802_11K
#define CMD_NEIGHBOR_REQ			"neighbor-request"
#endif

#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
#define CMD_BTM_QUERY				"bss-transition-query"
#endif

#define CMD_GET_BSS_TABLE           "BSSTABLE_RSSI"

#if CFG_SUPPORT_GET_MCS_INFO
#define CMD_GET_MCS_INFO	"GET_MCS_INFO"
#endif

#ifdef CFG_SUPPORT_TIME_MEASURE
#define CMD_START_FTM "START_FTM"
#define CMD_GET_TMR_DISTANCE "GET_TMR_DISTANCE"
#define CMD_GET_TMR_AUDIOSYNC "GET_TMR_AUDIOSYNC"
#define CMD_START_FTM_NON_BLOCK "START_FTM_NON_BLOCK"
#define CMD_ENABLE_TMR "ENABLE_TMR"
#endif

#if CFG_SUPPORT_WAC
#define	CMD_SET_WAC_IE_ENABLE	"SET_WAC_IE_ENABLE"
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

void CoexGetArbMode(char *pcCommand,
		uint32_t *pu4Offset,
		uint8_t **ppucData)
{
	uint8_t *pucRegValue = *ppucData;
	uint32_t u4RegValue = *pucRegValue |
			*(pucRegValue+1) << 8 |
			*(pucRegValue+2) << 16 |
			*(pucRegValue+3) << 24;
	pucRegValue += 4;
	*ppucData = pucRegValue;
	/* In-Band ARB Mode */
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset, "%-20s : ",
	"InBand ARB mode");
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset,
	"WF Rx BT Tx %s common action\n",
	((u4RegValue & BIT(12))) ? "all" : "no");
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset,
	"%-20s : WF Wx BT Rx %s common action\n",
	" ",
	((u4RegValue & BIT(13))) ? "all" : "no");
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset,
	"%-20s : WF Rx BT Rx %s common action\n",
	" ",
	((u4RegValue & BIT(14))) ? "all" : "no");
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset,
	"%-20s : WF Tx BT Tx %s common action\n",
	" ",
	((u4RegValue & BIT(15))) ? "all" : "no");

	/* Out-Band ARB Mode */
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset, "%-20s : ",
	"OutBand ARB mode");
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset,
	"WF Rx BT Tx %s common action\n",
	((u4RegValue & BIT(28))) ? "all" : "no");
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset,
	"%-20s : WF Tx BT Rx %s common action\n",
	" ",
	((u4RegValue & BIT(29))) ? "all" : "no");
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset,
	"%-20s : WF Rx BT Rx %s common action\n",
	" ",
	((u4RegValue & BIT(30))) ? "all" : "no");
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset,
	"%-20s : WF Tx BT Tx %s common action\n",
	" ",
	((u4RegValue & BIT(31))) ? "all" : "no");
}

void CoexGetBtProfile(char *pcCommand,
		uint32_t *pu4Offset,
		uint8_t **ppucData)
{
	uint8_t *pucRegValue = *ppucData;
	uint32_t u4RegValue = *pucRegValue |
			*(pucRegValue+1) << 8 |
			*(pucRegValue+2) << 16 |
			*(pucRegValue+3) << 24;
	pucRegValue += 4;
	*ppucData = pucRegValue;
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset, "%-20s : ",
		"BT Profile");

	if (COEX_BCM_IS_BT_NONE(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"none\n");
		return;
	}

	if (COEX_BCM_IS_BT_SCO(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"SCO\n");
	}

	if (COEX_BCM_IS_BT_A2DP(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"A2DP\n");
	}

	if (COEX_BCM_IS_BT_LINK_CONNECTED(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"LINK CONNECTED\n");
	}

	if (COEX_BCM_IS_BT_HID(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"HID\n");
	}

	if (COEX_BCM_IS_BT_PAGE(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"PAGE\n");
	}

	if (COEX_BCM_IS_BT_INQUIRY(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"INQUIRY\n");
	}

	if (COEX_BCM_IS_BT_ESCO(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"ESCO\n");
	}

	if (COEX_BCM_IS_BT_MULTI_HID(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"MULTI HID\n");
	}

	if (COEX_BCM_IS_BT_BLE_VOBLE(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"BLE VOBLE\n");
	}

	if (COEX_BCM_IS_BT_A2DP_SINK(u4RegValue)) {
		*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset,
		"A2DP SINK\n");
	}
}

void CoexGetProtFrmType(char *pcCommand,
		uint32_t *pu4Offset,
		uint8_t **ppucData)
{
	uint8_t *pucRegValue = *ppucData;
	uint32_t u4RegValue = *pucRegValue |
			*(pucRegValue+1) << 8 |
			*(pucRegValue+2) << 16 |
			*(pucRegValue+3) << 24;
	pucRegValue += 4;
	*ppucData = pucRegValue;
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset, "%-20s : %s\n",
	"Protection Type",
	u4RegValue ? "PS-NULL" : "CTS");
}

void CoexGetRxBaSize(char *pcCommand,
		uint32_t *pu4Offset,
		uint8_t **ppucData)
{
	uint8_t *pucRegValue = *ppucData;
	/* BA Size */
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset, "%-20s : %d\n",
	"Rx BA Size",
	*pucRegValue);
	*ppucData = ++pucRegValue;
}

void CoexGetCfgCoexIsoCtrl(char *pcCommand,
		uint32_t *pu4Offset,
		uint8_t **ppucData)
{
	uint8_t *pucRegValue = *ppucData;
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset, "%-20s : %d\n",
	"CFG CoexIsoCtrl",
	*pucRegValue);
	*ppucData = ++pucRegValue;
}

void CoexGetCfgCoexModeCtrl(char *pcCommand,
		uint32_t *pu4Offset,
		uint8_t **ppucData)
{
	uint8_t *pucRegValue = *ppucData;
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset, "%-20s : %d\n",
	"CFG CoexModeCtrl",
	*pucRegValue);
	*ppucData = ++pucRegValue;
}

void CoexGetCfgFddPerPkt(char *pcCommand,
		uint32_t *pu4Offset,
		uint8_t **ppucData)
{
	uint8_t *pucRegValue = *ppucData;
	*pu4Offset += snprintf(pcCommand + *pu4Offset,
	PRIV_CMD_SIZE - *pu4Offset, "%-20s : %d\n",
	"CFG FddPerPkt",
	*pucRegValue);
	*ppucData = ++pucRegValue;
}

/* Coex Ctrl Cmd - Coex Info Content */
struct COEX_REF_TABLE {
	uint16_t ucCoexInfoId;
	char *cContent;
	void (*pCoexRefHandle)(char *pcCommand,
			uint32_t *pu4Offset,
			uint8_t **ppucData);
};

const struct COEX_REF_TABLE coex_ref_table[] = {
	{COEX_REF_TABLE_ID_ISO_DETECTION_VALUE,
		"Isolation Detection Value", NULL},
	{COEX_REF_TABLE_ID_COEX_BT_PROFILE,
		"Coex BT Profile", CoexGetBtProfile},
	{COEX_REF_TABLE_ID_RX_BASIZE,
		"Rx Ba Size", CoexGetRxBaSize},
	{COEX_REF_TABLE_ID_ARB_MODE,
		"ARB Mode", CoexGetArbMode},
	{COEX_REF_TABLE_ID_PROT_FRM_TYPE,
		"Prot Frame Type", CoexGetProtFrmType},
	{COEX_REF_TABLE_ID_CFG_COEXISOCTRL,
		"CFG CoexIsoCtrl", CoexGetCfgCoexIsoCtrl},
	{COEX_REF_TABLE_ID_CFG_COEXMODECTRL,
		"CFG CoexModeCtrl", CoexGetCfgCoexModeCtrl},
	{COEX_REF_TABLE_ID_CFG_FDDPERPKT,
		"CFG FddPerPkt", CoexGetCfgFddPerPkt},
	{COEX_REF_TABLE_ID_CFG_BT_FIX_POWER,
		"CFG BT FIX POWER", NULL},
	{COEX_REF_TABLE_ID_CFG_BT_FDD_GAIN,
		"CFG BT FDD GAIN", NULL},
	{COEX_REF_TABLE_ID_CFG_BT_FDD_POWER,
		"CFG BT FDD POWER", NULL},
	{COEX_REF_TABLE_ID_CFG_WF_FDD_POWER,
		"CFG WF FDD POWER", NULL}
};

int priv_driver_get_dbg_level(IN struct net_device *prNetDev,
		IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t u4DbgIdx, u4DbgMask;
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
				snprintf(pcCommand, i4TotalLen,
					 "Get DBG module[%u] log level => [0x%02x]!",
					 u4DbgIdx,
					 (uint8_t) u4DbgMask);
		}
	}

	if (!fgIsCmdAccept)
		i4BytesWritten = snprintf(pcCommand, i4TotalLen,
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
		snprintf(pcCommand, i4TotalLen, "set buffer mode %s",
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
		/* Get AIS AP address for no argument */
		if (prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP) {
			COPY_MAC_ADDR(rQueryStaStatistics.aucMacAddr,
				prGlueInfo->prAdapter->prAisBssInfo
				->prStaRecOfAP->aucMacAddr);
			DBGLOG(RSN, INFO, "use ais ap "MACSTR"\n",
			       MAC2STR(prGlueInfo->prAdapter->prAisBssInfo
			       ->prStaRecOfAP->aucMacAddr));
		} else {
			DBGLOG(RSN, INFO, "not connect to ais ap %lx\n",
			       prGlueInfo->prAdapter->prAisBssInfo
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
	int32_t i4Rssi;
	struct PARAM_GET_BSS_STATISTICS rQueryBssStatistics;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) NULL;
	uint8_t ucBssIndex;
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

	/* 2. fill RSSI */
	if (prGlueInfo->eParamMediaStateIndicated !=
	    PARAM_MEDIA_STATE_CONNECTED) {
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
		prNetDevPrivate = (struct NETDEV_PRIVATE_GLUE_INFO *)
				  netdev_priv(prNetDev);
		ASSERT(prNetDevPrivate->prGlueInfo == prGlueInfo);
		ucBssIndex = prNetDevPrivate->ucBssIdx;

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

#if CFG_SUPPORT_GET_MCS_INFO
#define GET_TX_MCS_BW(_x)	(((_x) & (0x3 << 12)) >> 12)
#define GET_TX_MCS_SGI(_x)	(((_x) & (0x1 << 14)) >> 14)
#define GET_TX_MCS_LDPC(_x)	(((_x) & (0x1 << 15)) >> 15)
#endif

char *HW_TX_MODE_STR[] = {"CCK", "OFDM", "MM", "GF", "VHT", "N/A"};
char *HW_TX_RATE_CCK_STR[] = {"1M", "2M", "5.5M", "11M", "N/A"};
char *HW_TX_RATE_OFDM_STR[] = {"6M", "9M", "12M", "18M", "24M", "36M",
				      "48M", "54M", "N/A"};
char *HW_TX_RATE_BW[] = {"BW20", "BW40", "BW80", "BW160/BW8080", "N/A"};

#if (CFG_SUPPORT_RA_GEN == 0)
static char *RATE_TBLE[] = {"B", "G", "N", "N_2SS", "AC", "AC_2SS", "N/A"};
#else
static char *RATE_TBLE[] = {"B", "G", "N", "N_2SS", "AC", "AC_2SS", "BG",
			    "N/A"};
static char *RA_STATUS_TBLE[] = {"INVALID", "POWER_SAVING", "SLEEP", "STANDBY",
				 "RUNNING", "N/A"};
#if 0
static char *LT_MODE_TBLE[] = {"RSSI", "LAST_RATE", "TRACKING", "N/A"};
static char *SGI_UNSP_STATE_TBLE[] = {"INITIAL", "PROBING", "SUCCESS",
				      "FAILURE", "N/A"};
static char *BW_STATE_TBLE[] = {"UNCHANGED", "DOWN", "N/A"};
#endif
#endif

#if 0
static char *AR_STATE[] = {"NULL", "STEADY", "PROBE", "N/A"};
static char *AR_ACTION[] = {"NULL", "INDEX", "RATE_UP", "RATE_DOWN", "RATE_GRP",
			    "RATE_BACK", "GI", "SGI_EN", "SGI_DIS", "PWR",
			    "PWR_UP", "PWR_DOWN", "PWR_RESET_UP", "BF", "BF_EN",
			    "BF_DIS", "N/A"};
#endif
#define BW_20		0
#define BW_40		1
#define BW_80		2
#define BW_160		3
#define BW_10		4
#define BW_5		6
#define BW_8080		7
#define BW_ALL		0xFF

char *hw_rate_ofdm_str(uint16_t ofdm_idx)
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
		if (txmode >= MAX_TX_MODE)
			txmode = MAX_TX_MODE;
		rate = HW_TX_RATE_TO_MCS(
			       prHwWlanInfo->rWtblRateInfo.au2RateCode[i],
			       txmode);
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
				hw_rate_ofdm_str(rate));
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

	ASSERT(pcCommand);

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

static int priv_driver_get_wtbl_info(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int32_t u4Ret = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	struct PARAM_HW_WLAN_INFO *prHwWlanInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	/* DBGLOG(RSN, INFO, "MT6632 : priv_driver_get_wtbl_info\n"); */

	prHwWlanInfo = (struct PARAM_HW_WLAN_INFO *)kalMemAlloc(
			sizeof(struct PARAM_HW_WLAN_INFO), VIR_MEM_TYPE);
	if (!prHwWlanInfo)
		return -1;

	if (i4Argc >= 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &prHwWlanInfo->u4Index);

		DBGLOG(REQ, INFO, "MT6632 : index = %d\n",
		       prHwWlanInfo->u4Index);

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryWlanInfo,
				   prHwWlanInfo,
				   sizeof(struct PARAM_HW_WLAN_INFO),
				   TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, INFO, "rStatus %u u4BufLen = %d\n", rStatus,
		       u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			kalMemFree(prHwWlanInfo, VIR_MEM_TYPE,
				   sizeof(struct PARAM_HW_WLAN_INFO));
			return -1;
		}
		i4BytesWritten = priv_driver_dump_helper_wtbl_info(pcCommand,
						i4TotalLen, prHwWlanInfo);
	}

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

	kalMemFree(prHwWlanInfo, VIR_MEM_TYPE,
		   sizeof(struct PARAM_HW_WLAN_INFO));

	return i4BytesWritten;
}

/* Private Coex Ctrl Subcmd for Getting Coex Info */
static int priv_driver_get_coex_info(IN struct GLUE_INFO *prGlueInfo,
				IN struct CMD_COEX_HANDLER *prCmdCoexHandler,
				IN signed char *argv[])
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	uint32_t u4Ret = 0;

	struct CMD_COEX_GET_INFO rCmdCoexGetInfo;

	kalMemSet(&rCmdCoexGetInfo,
			0,
			sizeof(struct CMD_COEX_GET_INFO));

	/* Copy Memory */
	kalMemCopy(prCmdCoexHandler->aucBuffer,
			&rCmdCoexGetInfo,
			sizeof(struct CMD_COEX_GET_INFO));
	DBGLOG(INIT, INFO, "priv_driver_get_coex_info end\n");

	/* Ioctl Get Coex Info */
	rStatus = kalIoctl(prGlueInfo,
			wlanoidQueryCoexGetInfo,
			prCmdCoexHandler,
			sizeof(struct CMD_COEX_HANDLER),
			TRUE,
			TRUE,
			TRUE,
			&u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	/* If all pass, return u4Ret to 0 */
	return u4Ret;
}

static void priv_coex_get_info(IN char *pcCommand,
			IN int i4TotalLen,
			IN uint32_t *pu4Offset,
			IN struct CMD_COEX_HANDLER *prCmdCoexHandler)
{
#define CMD_DEFAULT_LEN 2
#define DEFAULT_LEN_BYTE 2
	struct CMD_COEX_GET_INFO *prCmdCoexGetInfo;
	uint8_t ucIdx = 0;
	uint16_t u2TotLen;
	uint8_t ucCoexId;
	uint8_t ucCoexLen;
	uint8_t *pucCoexInfo;

	prCmdCoexGetInfo =
		(struct CMD_COEX_GET_INFO *) prCmdCoexHandler->aucBuffer;

	*pu4Offset += snprintf(pcCommand + *pu4Offset,
		PRIV_CMD_SIZE - *pu4Offset, "\n");

	pucCoexInfo = &prCmdCoexGetInfo->ucCoexInfo[0];
	u2TotLen = *pucCoexInfo | (*(pucCoexInfo+1) << 8);
	pucCoexInfo += DEFAULT_LEN_BYTE;

	while (ucIdx < u2TotLen) {
		ucCoexId = *pucCoexInfo++;
		ucCoexLen = *pucCoexInfo++;
		ucIdx += ucCoexLen + CMD_DEFAULT_LEN;

		if (ucCoexId >= COEX_REF_TABLE_ID_NUM) {
			*pu4Offset += snprintf(pcCommand + *pu4Offset,
			PRIV_CMD_SIZE - *pu4Offset, "Wrong coex info id...\n");
			return;
		}

		if (coex_ref_table[ucCoexId].pCoexRefHandle) {
			coex_ref_table[ucCoexId].pCoexRefHandle(pcCommand,
					pu4Offset,
					&pucCoexInfo);
		} else{
			pucCoexInfo += ucCoexLen;
		}
	}
}

/* Private Coex Ctrl Subcmd for Isolation Detection */
static int priv_driver_iso_detect(IN struct GLUE_INFO *prGlueInfo,
				IN struct CMD_COEX_HANDLER *prCmdCoexHandler,
				IN signed char *argv[])
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	uint32_t u4Ret = 0;

	struct CMD_COEX_ISO_DETECT rCmdCoexIsoDetect;

	rCmdCoexIsoDetect.u4Isolation = 0;

	u4Ret = kalkStrtou32(argv[2], 0, &(rCmdCoexIsoDetect.u4IsoPath));
	if (u4Ret) {
		DBGLOG(REQ, LOUD,
		"Parse Iso Path failed u4Ret=%d\n", u4Ret);
		return -1;
	}

	u4Ret = kalkStrtou32(argv[3], 0, &(rCmdCoexIsoDetect.u4Channel));
	if (u4Ret) {
		DBGLOG(REQ, LOUD,
		"Parse channel failed u4Ret = %d\n", u4Ret);
		return -1;
	}

	/* Copy Memory */
	kalMemCopy(prCmdCoexHandler->aucBuffer,
			&rCmdCoexIsoDetect,
			sizeof(struct CMD_COEX_ISO_DETECT));

	DBGLOG(REQ, INFO, "priv_driver_get_coex_info end\n");

	/* Ioctl Isolation Detect */
	rStatus = kalIoctl(prGlueInfo,
			wlanoidQueryCoexIso,
			prCmdCoexHandler,
			sizeof(struct CMD_COEX_HANDLER),
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
static int priv_driver_coex_ctrl(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int32_t i4ArgNum = 2;
	signed char *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t u4Ret = 0;
	uint32_t u4Offset = 0;
	int32_t i4SubArgNum;
	enum ENUM_COEX_CMD_CTRL CoexCmdCtrl;
	struct CMD_COEX_HANDLER rCmdCoexHandler;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	/* Prevent Kernel Panic, set default i4ArgNum to 2 */
	if (i4Argc >= i4ArgNum) {

		/* Parse Coex SubCmd */
		u4Ret = kalkStrtou32(apcArgv[1], 0, &rCmdCoexHandler.u4SubCmd);
		if (u4Ret)
			return -1;

		CoexCmdCtrl =
			(enum ENUM_COEX_CMD_CTRL)rCmdCoexHandler.u4SubCmd;

		switch (CoexCmdCtrl) {
		case COEX_CMD_GET_INFO:
		{
			u4Ret = priv_driver_get_coex_info(prGlueInfo,
							&rCmdCoexHandler,
							apcArgv);
			if (u4Ret)
				return -1;
			priv_coex_get_info(pcCommand,
					i4TotalLen,
					&u4Offset,
					&rCmdCoexHandler);
			break;
		}
		case COEX_CMD_GET_ISO_DETECT:
		{
			struct CMD_COEX_ISO_DETECT *prCmdCoexIsoDetect;

			i4SubArgNum = 3;
			/* Safely dereference "argv[3]".*/
			if (i4Argc < i4SubArgNum)
				break;
			/* Isolation Detection Method */
			u4Ret = priv_driver_iso_detect(prGlueInfo,
							&rCmdCoexHandler,
							apcArgv);
			if (u4Ret)
				return -1;

			/* Get Isolation value */
			prCmdCoexIsoDetect =
		(struct CMD_COEX_ISO_DETECT *)rCmdCoexHandler.aucBuffer;

			/* Set Return i4BytesWritten Value */
			u4Offset = snprintf(pcCommand, i4TotalLen, "%d",
				(prCmdCoexIsoDetect->u4Isolation/2));
			DBGLOG(REQ, INFO, "Isolation: %d\n",
				(prCmdCoexIsoDetect->u4Isolation/2));
			break;
		}
		/* Default Coex Cmd */
		default:
			break;
		}

	 /* Set Return i4BytesWritten Value */
	i4BytesWritten = (int32_t)u4Offset;
	}
	return i4BytesWritten;
}

static int priv_driver_get_sta_info(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t ucWlanIndex;
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
		/* Get AIS AP address for no argument */
		if (prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP)
			ucWlanIndex = prGlueInfo->prAdapter->prAisBssInfo
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
			"\tRx data indicate total=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_INDICATION_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx data retain total=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_RETAINED_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx drop by SW total=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_DROP_TOTAL_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx reorder miss=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_MISS_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx reorder within=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_WITHIN_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx reorder ahead=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_AHEAD_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx reorder behind=%ld\n", RX_GET_CNT(prRxCtrl,
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
			"\tRx total MSDU in AMSDU=%ld\n", RX_GET_CNT(prRxCtrl,
			RX_DATA_MSDU_IN_AMSDU_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx AMSDU=%ld\n", RX_GET_CNT(prRxCtrl,
			RX_DATA_AMSDU_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx AMSDU miss=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_DATA_AMSDU_MISS_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx no StaRec drop=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_NO_STA_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx inactive BSS drop=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_INACTIVE_BSS_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx HS20 drop=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_HS20_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx low SwRfb drop=%ld\n", RX_GET_CNT(prRxCtrl,
			RX_LESS_SW_RFB_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx dupicate drop=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_DUPICATE_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx MIC err drop=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_MIC_ERROR_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx BAR handle=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_BAR_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx non-interest drop=%ld\n", RX_GET_CNT(prRxCtrl,
			RX_NO_INTEREST_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\tRx type err drop=%ld\n",
			RX_GET_CNT(prRxCtrl, RX_TYPE_ERR_DROP_COUNT));
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\tRx class err drop=%ld\n", RX_GET_CNT(prRxCtrl,
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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

		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "0x%08x",
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t u4Ret;
	int32_t i4ArgNum = 2, u4MagicKey;

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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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

		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rRfATInfo.u4FuncIndex));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "Parse Test CMD Index error u4Ret=%d\n", u4Ret);

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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
		u4Ret = kalkStrtou32(apcArgv[1], 0, &(rRfATInfo.u4FuncIndex));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "Parse Test CMD Index error u4Ret=%d\n", u4Ret);

		u4Ret = kalkStrtou32(apcArgv[2], 0, &(rRfATInfo.u4FuncData));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
			       "Parse Test CMD Data error u4Ret=%d\n",
			       u4Ret);

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
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%d[0x%08x]",
					  u4Data, u4Data);
		DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__,
		       pcCommand);
	}

	return i4BytesWritten;

}				/* priv_driver_get_test_result */

int32_t priv_driver_tx_rate_info(IN char *pcCommand, IN int i4TotalLen,
			u_int8_t fgDumpAll,
			struct PARAM_HW_WLAN_INFO *prHwWlanInfo,
			struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics)
{
	uint8_t i, txmode, rate, stbc;
	uint8_t nsts;
	int32_t i4BytesWritten = 0;

	for (i = 0; i < AUTO_RATE_NUM; i++) {
		txmode = HW_TX_RATE_TO_MODE(
				prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);
		if (txmode >= MAX_TX_MODE)
			txmode = MAX_TX_MODE;
		rate = HW_TX_RATE_TO_MCS(
			prHwWlanInfo->rWtblRateInfo.au2RateCode[i], txmode);
		nsts = HW_TX_RATE_TO_NSS(
			prHwWlanInfo->rWtblRateInfo.au2RateCode[i]) + 1;
		stbc = HW_TX_RATE_TO_STBC(
			prHwWlanInfo->rWtblRateInfo.au2RateCode[i]);

		if (fgDumpAll) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Rate index - %d    ", i);

			if (prHwWlanInfo->rWtblRateInfo.ucRateIdx == i) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "--> ");
			} else {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "    ");
			}
		}

		if (!fgDumpAll) {
			if (prHwWlanInfo->rWtblRateInfo.ucRateIdx != i)
				continue;
			else
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s", "Auto TX Rate", " = ");
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", HW_TX_RATE_CCK_STR[rate & 0x3]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", hw_rate_ofdm_str(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) ||
			 (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"NSS%d_MCS%d, ", nsts, rate);

		if (prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(
			    pcCommand + i4BytesWritten,
			    i4TotalLen - i4BytesWritten, "%s, ",
			    prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability
			      < 4 ? HW_TX_RATE_BW[
			      prHwWlanInfo->rWtblPeerCap.ucFrequencyCapability]
			      : HW_TX_RATE_BW[4]);
		} else {
			if ((txmode == TX_RATE_MODE_CCK) ||
			    (txmode == TX_RATE_MODE_OFDM))
				i4BytesWritten += kalScnprintf(
				    pcCommand + i4BytesWritten,
				    i4TotalLen - i4BytesWritten,
				    "%s, ", HW_TX_RATE_BW[0]);
			else
			if (i > prHwWlanInfo->rWtblPeerCap
				.ucChangeBWAfterRateN)
				i4BytesWritten += kalScnprintf(
				    pcCommand + i4BytesWritten,
				    i4TotalLen - i4BytesWritten, "%s, ",
				    prHwWlanInfo->rWtblPeerCap.
					ucFrequencyCapability < 4 ?
				    (prHwWlanInfo->rWtblPeerCap.
					ucFrequencyCapability > BW_20 ?
					HW_TX_RATE_BW[prHwWlanInfo->
					    rWtblPeerCap
					    .ucFrequencyCapability - 1] :
					HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap
					    .ucFrequencyCapability]) :
				    HW_TX_RATE_BW[4]);
			else
				i4BytesWritten += kalScnprintf(
				    pcCommand + i4BytesWritten,
				    i4TotalLen - i4BytesWritten,
				    "%s, ",
				    prHwWlanInfo->rWtblPeerCap.
					ucFrequencyCapability < 4 ?
				    HW_TX_RATE_BW[
					prHwWlanInfo->rWtblPeerCap
					.ucFrequencyCapability] :
				    HW_TX_RATE_BW[4]);
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ",
				priv_driver_get_sgi_info(
				    &prHwWlanInfo->rWtblPeerCap) == 0 ?
				    "LGI" : "SGI");

		if (prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s%s%s\n",
				txmode < 5 ?
				    HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
				stbc ? ", STBC, " : ", ",
				priv_driver_get_ldpc_info(
				    &prHwWlanInfo->rWtblTxConfig) == 0 ?
				    "BCC" : "LDPC");
		} else {
#if (CFG_SUPPORT_RA_GEN == 0)
			if (prQueryStaStatistics->aucArRatePer[
			    prQueryStaStatistics->aucRateEntryIndex[i]] == 0xFF)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s%s%s   (--)\n",
					txmode < 5 ?
					    HW_TX_MODE_STR[txmode] :
					    HW_TX_MODE_STR[5],
					stbc ? ", STBC, " : ", ",
					((priv_driver_get_ldpc_info(
					    &prHwWlanInfo->rWtblTxConfig) == 0)
					    || (txmode == TX_RATE_MODE_CCK)
					    || (txmode == TX_RATE_MODE_OFDM)) ?
						"BCC" : "LDPC");
			else
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s%s%s   (%d)\n",
					txmode < 5 ?
					    HW_TX_MODE_STR[txmode] :
					    HW_TX_MODE_STR[5],
					stbc ? ", STBC, " : ", ",
					((priv_driver_get_ldpc_info(
					    &prHwWlanInfo->rWtblTxConfig) == 0)
					    || (txmode == TX_RATE_MODE_CCK)
					    || (txmode == TX_RATE_MODE_OFDM))
						? "BCC" : "LDPC",
					prQueryStaStatistics->aucArRatePer[
					    prQueryStaStatistics
					    ->aucRateEntryIndex[i]]);
#else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s%s%s\n",
				txmode < 5 ?
				    HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
				stbc ? ", STBC, " : ", ",
				((priv_driver_get_ldpc_info(
				    &prHwWlanInfo->rWtblTxConfig) == 0) ||
				    (txmode == TX_RATE_MODE_CCK) ||
				    (txmode == TX_RATE_MODE_OFDM)) ?
				    "BCC" : "LDPC");
#endif
		}

		if (!fgDumpAll)
			break;
	}

	return i4BytesWritten;
}

int32_t priv_driver_last_rx_rssi(struct ADAPTER *prAdapter, IN char *pcCommand,
				 IN int i4TotalLen, IN uint8_t ucWlanIdx)
{
	int32_t i4RSSI0 = 0, i4RSSI1 = 0, i4RSSI2 = 0, i4RSSI3 = 0;
	int32_t i4BytesWritten = 0;
	uint32_t u4RxVector3 = 0;
	uint8_t ucStaIdx;

	if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIdx, &ucStaIdx) ==
	    WLAN_STATUS_SUCCESS) {
		u4RxVector3 = prAdapter->arStaRec[ucStaIdx].u4RxVector3;
		DBGLOG(REQ, LOUD, "****** RX Vector3 = 0x%08x ******\n",
		       u4RxVector3);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s", "Last RX RSSI", " = NOT SUPPORT");
		return i4BytesWritten;
	}

	i4RSSI0 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI0_MASK) >>
			      RX_VT_RCPI0_OFFSET);
	i4RSSI1 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI1_MASK) >>
			      RX_VT_RCPI1_OFFSET);

	if (prAdapter->rWifiVar.ucNSS > 2) {
		i4RSSI2 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI2_MASK) >>
				      RX_VT_RCPI2_OFFSET);
		i4RSSI3 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI3_MASK) >>
				      RX_VT_RCPI3_OFFSET);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%-20s%s%d %d %d %d\n",
			"Last RX Data RSSI", " = ",
			i4RSSI0, i4RSSI1, i4RSSI2, i4RSSI3);
	} else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%-20s%s%d %d\n",
			"Last RX Data RSSI", " = ", i4RSSI0, i4RSSI1);

	return i4BytesWritten;
}

#if CFG_SUPPORT_ADVANCE_CONTROL
static int32_t priv_driver_get_snr(IN struct GLUE_INFO *prGlueInfo,
		IN char *pcCommand, IN int i4TotalLen)
{
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	int32_t i4BytesWritten = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int8_t iSnrR0Phase1 = 0, iSnrR1Phase1 = 0;
	int8_t iSnrR0Phase2 = 0, iSnrR1Phase2 = 0;

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_SNR_ID;

	rStatus = kalIoctl(prGlueInfo, wlanoidQuerySwCtrlRead,
			&rSwCtrlInfo, sizeof(rSwCtrlInfo),
			TRUE, TRUE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(HAL, ERROR, "CMD_ADVCTL_SNR_ID rStatus %u\n", rStatus);
		return -1;
	}

	iSnrR1Phase2 = rSwCtrlInfo.u4Data & 0xFF;
	iSnrR0Phase2 = (rSwCtrlInfo.u4Data >> 8) & 0xFF;
	iSnrR1Phase1 = (rSwCtrlInfo.u4Data >> 16) & 0xFF;
	iSnrR0Phase1 = (rSwCtrlInfo.u4Data >> 24) & 0xFF;

	i4BytesWritten +=
			kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s %-d %-d\n", "SNR", " = ",
			iSnrR0Phase2, iSnrR1Phase2);

	DBGLOG(HAL, INFO, "SNR: R0P1:0x%02x R1P1:0x%02x\n",
			iSnrR0Phase1, iSnrR1Phase1);

	DBGLOG(HAL, INFO, "SNR: R0P2:0x%02x R1P2:0x%02x u4Data:0x%08x\n",
			iSnrR0Phase2, iSnrR1Phase2, rSwCtrlInfo.u4Data);

	return i4BytesWritten;
}
#endif

int32_t priv_driver_rx_rate_info(struct ADAPTER *prAdapter, IN char *pcCommand,
				 IN int i4TotalLen, IN uint8_t ucWlanIdx)
{
	uint32_t txmode, rate, frmode, sgi, nsts, ldpc, stbc, groupid, mu;
	int32_t i4BytesWritten = 0;
	uint32_t u4RxVector0 = 0, u4RxVector1 = 0;
	uint8_t ucStaIdx;

	if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIdx, &ucStaIdx) ==
	    WLAN_STATUS_SUCCESS) {
		u4RxVector0 = prAdapter->arStaRec[ucStaIdx].u4RxVector0;
		u4RxVector1 = prAdapter->arStaRec[ucStaIdx].u4RxVector1;
		DBGLOG(REQ, LOUD, "****** RX Vector0 = 0x%08x ******\n",
		       u4RxVector0);
		DBGLOG(REQ, LOUD, "****** RX Vector1 = 0x%08x ******\n",
		       u4RxVector1);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
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

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%-20s%s", "Last RX Rate", " = ");

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			rate < 4 ? HW_TX_RATE_CCK_STR[rate] :
				HW_TX_RATE_CCK_STR[4]);
	else if (txmode == TX_RATE_MODE_OFDM)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			hw_rate_ofdm_str(rate));
	else if ((txmode == TX_RATE_MODE_HTMIX) ||
		 (txmode == TX_RATE_MODE_HTGF))
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "MCS%d, ", rate);
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "NSS%d_MCS%d, ",
			nsts, rate);

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s, ",
		frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			rate < 4 ? "LP" : "SP");
	else if (txmode == TX_RATE_MODE_OFDM)
		;
	else
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			sgi == 0 ? "LGI" : "SGI");

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "%s", stbc == 0 ? "" : "STBC, ");

	if (mu) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, %s, %s (%d)\n",
			txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			ldpc == 0 ? "BCC" : "LDPC", "MU", groupid);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, %s\n",
			txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			ldpc == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}

int32_t priv_driver_tx_vector_info(IN char *pcCommand, IN int i4TotalLen,
				   IN struct TX_VECTOR_BBP_LATCH *prTxV)
{
	uint8_t rate, txmode, frmode, sgi, ldpc, nsts, stbc;
	int8_t txpwr;
	int32_t i4BytesWritten = 0;

	rate = TX_VECTOR_GET_TX_RATE(prTxV);
	txmode = TX_VECTOR_GET_TX_MODE(prTxV);
	frmode = TX_VECTOR_GET_TX_FRMODE(prTxV);
	nsts = TX_VECTOR_GET_TX_NSTS(prTxV) + 1;
	sgi = TX_VECTOR_GET_TX_SGI(prTxV);
	ldpc = TX_VECTOR_GET_TX_LDPC(prTxV);
	stbc = TX_VECTOR_GET_TX_STBC(prTxV);
	txpwr = TX_VECTOR_GET_TX_PWR(prTxV);

	if (prTxV->u4TxVector1 == 0xFFFFFFFF) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "Last TX Rate", " = ", "N/A");
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s", "Last TX Rate", " = ");

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? HW_TX_RATE_CCK_STR[rate] :
					HW_TX_RATE_CCK_STR[4]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", hw_rate_ofdm_str(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) ||
			 (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"NSS%d_MCS%d, ", nsts, rate);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", sgi == 0 ? "LGI" : "SGI");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s%s%s\n",
			txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			stbc ? ", STBC, " : ", ", ldpc == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}

uint16_t priv_driver_get_idx_info(IN struct ADAPTER *prAdapter,
				  IN uint8_t ucWlanIdx)
{
	static uint8_t aucWlanIdxArray[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint16_t u2ValidBitMask;	/* support max 16 peers */
	uint8_t ucIdx, ucStaIdx, ucCnt = 0;
	uint8_t ucWlanIdxExist;

	/* check every wlanIdx and unmask no longer used wlanIdx */
	for (ucIdx = 0; ucIdx < CFG_STAT_DBG_PEER_NUM; ucIdx++) {
		if (u2ValidBitMask & BIT(ucIdx)) {
			ucWlanIdxExist = aucWlanIdxArray[ucIdx];

			if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIdxExist,
				&ucStaIdx) != WLAN_STATUS_SUCCESS)
				u2ValidBitMask &= ~BIT(ucIdx);
		}
	}

	/* Search matched WlanIdx */
	for (ucIdx = 0; ucIdx < CFG_STAT_DBG_PEER_NUM; ucIdx++) {
		if (u2ValidBitMask & BIT(ucIdx)) {
			ucCnt++;
			ucWlanIdxExist = aucWlanIdxArray[ucIdx];

			if (ucWlanIdxExist == ucWlanIdx) {
				DBGLOG(REQ, INFO,
				    "=== Matched, Mask=0x%x, ucIdx=%d ===\n",
				    u2ValidBitMask, ucIdx);
				return ucIdx;
			}
		}
	}

	/* No matched WlanIdx, add new one */
	if (ucCnt < CFG_STAT_DBG_PEER_NUM) {
		for (ucIdx = 0; ucIdx < CFG_STAT_DBG_PEER_NUM; ucIdx++)	{
			if (~u2ValidBitMask & BIT(ucIdx)) {
				u2ValidBitMask |= BIT(ucIdx);
				aucWlanIdxArray[ucIdx] = ucWlanIdx;
				DBGLOG(REQ, INFO,
				    "=== New Add, Mask=0x%x, ucIdx=%d ===\n",
				    u2ValidBitMask, ucIdx);
				return ucIdx;
			}
		}
	}

	return 0xFFFF;
}

#if (CFG_SUPPORT_RA_GEN == 0)
static int32_t priv_driver_dump_stat_info(struct ADAPTER *prAdapter,
			IN char *pcCommand, IN int i4TotalLen,
			struct PARAM_HW_WLAN_INFO *prHwWlanInfo,
			struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics,
			u_int8_t fgResetCnt, uint32_t u4StatGroup)
{
	int32_t i4BytesWritten = 0;
	int32_t rRssi;
	uint16_t u2LinkSpeed;
	uint32_t u4Per, u4RxPer[ENUM_BAND_NUM], u4AmpduPer[ENUM_BAND_NUM],
		 u4InstantPer;
	uint8_t ucDbdcIdx, ucStaIdx, ucNss;
	uint8_t ucSkipAr;
	static uint32_t u4TotalTxCnt, u4TotalFailCnt;
	static uint32_t u4Rate1TxCnt, u4Rate1FailCnt;
	static uint32_t au4RxMpduCnt[ENUM_BAND_NUM] = {0};
	static uint32_t au4FcsError[ENUM_BAND_NUM] = {0};
	static uint32_t au4RxFifoCnt[ENUM_BAND_NUM] = {0};
	static uint32_t au4AmpduTxSfCnt[ENUM_BAND_NUM] = {0};
	static uint32_t au4AmpduTxAckSfCnt[ENUM_BAND_NUM] = {0};
	struct RX_CTRL *prRxCtrl;
	uint32_t u4InstantRxPer[ENUM_BAND_NUM];
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int16_t i2Wf0AvgPwr;
	int16_t i2Wf1AvgPwr;
	uint32_t u4BufLen = 0;

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

		u4InstantPer =
			(prHwWlanInfo->rWtblTxCounter.u2Rate1TxCnt == 0) ? (0) :
			(1000 * (prHwWlanInfo->rWtblTxCounter.u2Rate1FailCnt) /
			(prHwWlanInfo->rWtblTxCounter.u2Rate1TxCnt));
	} else {
		u4Per = (prQueryStaStatistics->u4Rate1TxCnt == 0) ?
			(0) : (1000 * (prQueryStaStatistics->u4Rate1FailCnt) /
			(prQueryStaStatistics->u4Rate1TxCnt));

		u4InstantPer = (prQueryStaStatistics->ucPer == 0) ?
					(0) : (prQueryStaStatistics->ucPer);
	}

	for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
		au4RxMpduCnt[ucDbdcIdx] +=
		    prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxMpduCnt;
		au4FcsError[ucDbdcIdx] +=
		    prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError;
		au4RxFifoCnt[ucDbdcIdx] +=
		    prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxFifoFull;
		au4AmpduTxSfCnt[ucDbdcIdx] +=
		    prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4AmpduTxSfCnt;
		au4AmpduTxAckSfCnt[ucDbdcIdx] +=
		    prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4AmpduTxAckSfCnt;

		u4RxPer[ucDbdcIdx] = ((au4RxMpduCnt[ucDbdcIdx] +
		    au4FcsError[ucDbdcIdx]) == 0) ? (0) :
		    (1000 * au4FcsError[ucDbdcIdx] /
		    (au4RxMpduCnt[ucDbdcIdx] + au4FcsError[ucDbdcIdx]));

		u4AmpduPer[ucDbdcIdx] =
		    (au4AmpduTxSfCnt[ucDbdcIdx] == 0) ? (0) :
		    (1000 * (au4AmpduTxSfCnt[ucDbdcIdx] -
		    au4AmpduTxAckSfCnt[ucDbdcIdx]) /
		    au4AmpduTxSfCnt[ucDbdcIdx]);

		u4InstantRxPer[ucDbdcIdx] =
		    ((prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxMpduCnt +
		    prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError) == 0)
		    ? (0) :
		    (1000 * prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError
		    / (prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxMpduCnt +
		    prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError));
	}

	/* get Beacon RSSI */
	rStatus = kalIoctl(prAdapter->prGlueInfo, wlanoidQueryRssi, &rRssi,
			   sizeof(rRssi), TRUE, FALSE, FALSE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, WARN, "unable to retrieve rssi\n");

	u2LinkSpeed = (prQueryStaStatistics->u2LinkSpeed == 0) ?
				0 : prQueryStaStatistics->u2LinkSpeed / 2;

	/* =========== Group 0x0001 =========== */
	if (u4StatGroup & 0x0001) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n----- STA Stat (Group 0x01) -----\n");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CurrTemperature", " = ",
			prQueryStaStatistics->ucTemperature);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Tx Total cnt", " = ",
			ucSkipAr ? (u4TotalTxCnt) :
				(prQueryStaStatistics->u4TransmitCount));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Tx Fail Cnt", " = ",
			ucSkipAr ? (u4TotalFailCnt) :
				(prQueryStaStatistics->u4TransmitFailCount));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Rate1 Tx Cnt", " = ",
			ucSkipAr ? (u4Rate1TxCnt) :
				(prQueryStaStatistics->u4Rate1TxCnt));

		if (ucSkipAr)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n",
				"Rate1 Fail Cnt", " = ",
				u4Rate1FailCnt, u4Per/10, u4Per%10,
				u4InstantPer/10, u4InstantPer%10);
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d%%\n",
				"Rate1 Fail Cnt", " = ",
				prQueryStaStatistics->u4Rate1FailCnt,
				u4Per/10, u4Per%10, u4InstantPer);

		if ((ucSkipAr) && (fgResetCnt)) {
			u4TotalTxCnt = 0;
			u4TotalFailCnt = 0;
			u4Rate1TxCnt = 0;
			u4Rate1FailCnt = 0;
		}
	}

	/* =========== Group 0x0002 =========== */
	if (u4StatGroup & 0x0002) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- MIB Info (Group 0x02) -----\n");

		for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
			if (prAdapter->rWifiVar.fgDbDcModeEn)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"[DBDC_%d] :\n", ucDbdcIdx);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "RX Success", " = ",
				au4RxMpduCnt[ucDbdcIdx]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n",
				"RX with CRC", " = ",
				au4FcsError[ucDbdcIdx], u4RxPer[ucDbdcIdx]/10,
				u4RxPer[ucDbdcIdx] % 10,
				u4InstantRxPer[ucDbdcIdx] / 10,
				u4InstantRxPer[ucDbdcIdx]%10);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "RX drop FIFO full", " = ",
				au4RxFifoCnt[ucDbdcIdx]);

			if (!prAdapter->rWifiVar.fgDbDcModeEn)
				break;
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
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- Last Rx Info (Group 0x04) -----\n");

		rSwCtrlInfo.u4Data = 0;
		rSwCtrlInfo.u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + 1;

		rStatus = kalIoctl(prAdapter->prGlueInfo,
			wlanoidQuerySwCtrlRead, &rSwCtrlInfo,
			sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE, &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u, rSwCtrlInfo.u4Data 0x%x\n",
		       rStatus, rSwCtrlInfo.u4Data);
		if (rStatus == WLAN_STATUS_SUCCESS) {
			i2Wf0AvgPwr = rSwCtrlInfo.u4Data & 0xFFFF;
			i2Wf1AvgPwr = (rSwCtrlInfo.u4Data >> 16) & 0xFFFF;

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d %d\n", "NOISE", " = ",
				i2Wf0AvgPwr, i2Wf1AvgPwr);
		}

		/* Last RX Rate */
		i4BytesWritten += priv_driver_rx_rate_info(prAdapter,
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			(uint8_t)(prHwWlanInfo->u4Index));

#if CFG_SUPPORT_ADVANCE_CONTROL
		/* Last RX SNR */
		i4BytesWritten += priv_driver_get_snr(prAdapter->prGlueInfo,
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten);
#endif

		/* Last RX RSSI */
		i4BytesWritten += priv_driver_last_rx_rssi(prAdapter,
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			(uint8_t)(prHwWlanInfo->u4Index));

		/* Last TX Resp RSSI */
		if (ucNss > 2)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d %d %d %d\n", "Tx Response RSSI",
				" = ",
				RCPI_TO_dBm(prHwWlanInfo->
					rWtblRxCounter.ucRxRcpi0),
				RCPI_TO_dBm(prHwWlanInfo->
					rWtblRxCounter.ucRxRcpi1),
				RCPI_TO_dBm(prHwWlanInfo->
					rWtblRxCounter.ucRxRcpi2),
				RCPI_TO_dBm(prHwWlanInfo->
					rWtblRxCounter.ucRxRcpi3));
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d %d\n", "Tx Response RSSI", " = ",
				RCPI_TO_dBm(
				    prHwWlanInfo->rWtblRxCounter.ucRxRcpi0),
				RCPI_TO_dBm(
				    prHwWlanInfo->rWtblRxCounter.ucRxRcpi1));

		/* Last Beacon RSSI */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "Beacon RSSI", " = ", rRssi);
	}

	/* =========== Group 0x0008 =========== */
	if (u4StatGroup & 0x0008) {
		/* TxV */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- Last TX Info (Group 0x08) -----\n");

		for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
			int8_t txpwr;

			txpwr = TX_VECTOR_GET_TX_PWR(
				&prQueryStaStatistics->rTxVector[ucDbdcIdx]);

			if (prAdapter->rWifiVar.fgDbDcModeEn)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"[DBDC_%d] :\n", ucDbdcIdx);

			i4BytesWritten += priv_driver_tx_vector_info(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				&prQueryStaStatistics->rTxVector[ucDbdcIdx]);

			if (prQueryStaStatistics->
			    rTxVector[ucDbdcIdx].u4TxVector1 == 0xFFFFFFFF)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Chip Out TX Power",
					" = ", "N/A");
			else
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%c%d.%1d dBm\n",
					"Chip Out TX Power", " = ",
					(txpwr < 0) ? '-' : '+',
					abs(txpwr / 2),
					5 * abs(txpwr % 2));

			if (!prAdapter->rWifiVar.fgDbDcModeEn)
				break;
		}
	}

	/* =========== Group 0x0010 =========== */
	if (u4StatGroup & 0x0010) {
		/* RX Reorder */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- RX Reorder (Group 0x10) -----\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder miss", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_MISS_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder within", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_WITHIN_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder ahead", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_AHEAD_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder behind", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_BEHIND_COUNT));
#if CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Rx reorder STA", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_WITHIN_COUNT_STA));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Rx reorder P2P", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_WITHIN_COUNT_P2P));
#endif
	}

	/* =========== Group 0x0020 =========== */
	if (u4StatGroup & 0x0020) {
		/* AR info */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s", "----- AR Info (Group 0x20) -----\n");

		/* Last TX Rate */
		i4BytesWritten += priv_driver_tx_rate_info(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, FALSE,
				prHwWlanInfo, prQueryStaStatistics);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "LinkSpeed", " = ", u2LinkSpeed);

		if (!prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "RateTable", " = ",
				prQueryStaStatistics->ucArTableIdx < 6 ?
					RATE_TBLE[
					prQueryStaStatistics->ucArTableIdx] :
					RATE_TBLE[6]);

			if (wlanGetStaIdxByWlanIdx(prAdapter,
			    (uint8_t)(prHwWlanInfo->u4Index), &ucStaIdx) ==
				WLAN_STATUS_SUCCESS){
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "2G Support 256QAM TX",
					" = ",
					(prAdapter->arStaRec[ucStaIdx].u4Flags &
					    MTK_SYNERGY_CAP_SUPPORT_24G_MCS89) ?
					    1 : 0);
			}

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d%%\n", "Rate1 instantPer", " = ",
				u4InstantPer);

			if (prQueryStaStatistics->ucAvePer == 0xFF) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Train Down", " = ",
					"N/A");

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Train Up", " = ",
					"N/A");
			} else {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d -> %hd\n", "Train Down",
					" = ",
					(uint16_t)((prQueryStaStatistics
					    ->u2TrainDown) & BITS(0, 7)),
					(uint16_t)(((prQueryStaStatistics
					    ->u2TrainDown) >> 8) & BITS(0, 7)));

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d -> %hd\n", "Train Up", " = ",
					(uint16_t)((prQueryStaStatistics->
						u2TrainUp) & BITS(0, 7)),
					(uint16_t)(((prQueryStaStatistics->
						u2TrainUp) >> 8) & BITS(0, 7)));
			}

			if (prQueryStaStatistics->fgIsForceTxStream == 0)
				i4BytesWritten += kalScnprintf(
				    pcCommand + i4BytesWritten,
				    i4TotalLen - i4BytesWritten,
				    "%-20s%s%s\n", "Force Tx Stream", " = ",
				    "N/A");
			else
				i4BytesWritten += kalScnprintf(
				    pcCommand + i4BytesWritten,
				    i4TotalLen - i4BytesWritten,
				    "%-20s%s%d\n", "Force Tx Stream", " = ",
				    prQueryStaStatistics->fgIsForceTxStream);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "Force SE off", " = ",
				prQueryStaStatistics->fgIsForceSeOff);
		}

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CBRN", " = ",
			prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN);

		/* Rate1~Rate8 */
		i4BytesWritten += priv_driver_tx_rate_info(
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			TRUE, prHwWlanInfo, prQueryStaStatistics);
	}

	/* =========== Group 0x0040 =========== */
	if (u4StatGroup & 0x0040) {
		/* Tx Agg */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "------ TX AGG (Group 0x40) -----\n");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-12s%s", "Range:",
			"1     2~5     6~15    16~22    23~33    34~49    50~57    58~64\n");

		for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"DBDC%d:", ucDbdcIdx);
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%7d%8d%9d%9d%9d%9d%9d%9d\n",
				prQueryStaStatistics->
					rMibInfo[ucDbdcIdx].u2TxRange1AmpduCnt,
				prQueryStaStatistics->
					rMibInfo[ucDbdcIdx].u2TxRange2AmpduCnt,
				prQueryStaStatistics->
					rMibInfo[ucDbdcIdx].u2TxRange3AmpduCnt,
				prQueryStaStatistics->
					rMibInfo[ucDbdcIdx].u2TxRange4AmpduCnt,
				prQueryStaStatistics->
					rMibInfo[ucDbdcIdx].u2TxRange5AmpduCnt,
				prQueryStaStatistics->
					rMibInfo[ucDbdcIdx].u2TxRange6AmpduCnt,
				prQueryStaStatistics->
					rMibInfo[ucDbdcIdx].u2TxRange7AmpduCnt,
				prQueryStaStatistics->
					rMibInfo[ucDbdcIdx].u2TxRange8AmpduCnt);

			if (!prAdapter->rWifiVar.fgDbDcModeEn)
				break;
		}
	}

	return i4BytesWritten;
}
#else
static int32_t priv_driver_dump_stat_info(struct ADAPTER *prAdapter,
			IN char *pcCommand, IN int i4TotalLen,
			struct PARAM_HW_WLAN_INFO *prHwWlanInfo,
			struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics,
			u_int8_t fgResetCnt, uint32_t u4StatGroup)
{
	int32_t i4BytesWritten = 0;
	int32_t rRssi;
	uint16_t u2LinkSpeed;
	uint32_t u4Per, u4RxPer[ENUM_BAND_NUM], u4AmpduPer[ENUM_BAND_NUM],
		 u4InstantPer;
	uint8_t ucDbdcIdx, ucSkipAr, ucStaIdx, ucNss;
	static uint32_t u4TotalTxCnt[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint32_t	u4TotalFailCnt[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint32_t u4Rate1TxCnt[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint32_t	u4Rate1FailCnt[CFG_STAT_DBG_PEER_NUM] = {0};
	static uint32_t au4RxMpduCnt[ENUM_BAND_NUM] = {0};
	static uint32_t au4FcsError[ENUM_BAND_NUM] = {0};
	static uint32_t au4RxFifoCnt[ENUM_BAND_NUM] = {0};
	static uint32_t au4AmpduTxSfCnt[ENUM_BAND_NUM] = {0};
	static uint32_t au4AmpduTxAckSfCnt[ENUM_BAND_NUM] = {0};
	struct RX_CTRL *prRxCtrl;
	uint32_t u4InstantRxPer[ENUM_BAND_NUM];
#if CFG_SUPPORT_ADVANCE_CONTROL
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
#endif
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int16_t i2Wf0AvgPwr = 0, i2Wf1AvgPwr = 0;
	uint32_t u4BufLen = 0;
	uint8_t ucRaTableNum = sizeof(RATE_TBLE) / sizeof(char *);
	uint8_t ucRaStatusNum = sizeof(RA_STATUS_TBLE) / sizeof(char *);
#if 0
	uint8_t ucRaLtModeNum = sizeof(LT_MODE_TBLE) / sizeof(char *);
	uint8_t ucRaSgiUnSpStateNum = sizeof(SGI_UNSP_STATE_TBLE) /
								sizeof(char *);
	uint8_t ucRaBwStateNum = sizeof(BW_STATE_TBLE) / sizeof(char *);
#endif
	uint8_t ucAggRange[AGG_RANGE_SEL_NUM] = {0};
	uint32_t u4RangeCtrl_0, u4RangeCtrl_1;
	enum AGG_RANGE_TYPE_T eRangeType = ENUM_AGG_RANGE_TYPE_TX;
	uint16_t u2Idx = 0;
	uint8_t ucRcpi0 = 0, ucRcpi1 = 0, ucRcpi2 = 0, ucRcpi3 = 0;

	ucSkipAr = prQueryStaStatistics->ucSkipAr;
	prRxCtrl = &prAdapter->rRxCtrl;
	ucNss = prAdapter->rWifiVar.ucNSS;

	if (ucSkipAr) {
		u2Idx = priv_driver_get_idx_info(prAdapter,
			(uint8_t)(prHwWlanInfo->u4Index));

		if (u2Idx == 0xFFFF)
			return i4BytesWritten;
	}

	if (ucSkipAr) {
		u4TotalTxCnt[u2Idx] += prQueryStaStatistics->u4TransmitCount;
		u4TotalFailCnt[u2Idx] += prQueryStaStatistics->
							u4TransmitFailCount;
		u4Rate1TxCnt[u2Idx] += prQueryStaStatistics->u4Rate1TxCnt;
		u4Rate1FailCnt[u2Idx] += prQueryStaStatistics->u4Rate1FailCnt;
	}

	if (ucSkipAr) {
		u4Per = (u4Rate1TxCnt[u2Idx] == 0) ?
			(0) : (1000 * (u4Rate1FailCnt[u2Idx]) /
			(u4Rate1TxCnt[u2Idx]));

		u4InstantPer = (prQueryStaStatistics->u4Rate1TxCnt == 0) ?
			(0) : (1000 * (prQueryStaStatistics->u4Rate1FailCnt) /
				(prQueryStaStatistics->u4Rate1TxCnt));
	} else {
		u4Per = (prQueryStaStatistics->u4Rate1TxCnt == 0) ?
			(0) : (1000 * (prQueryStaStatistics->u4Rate1FailCnt) /
				(prQueryStaStatistics->u4Rate1TxCnt));

		u4InstantPer = (prQueryStaStatistics->ucPer == 0) ?
			(0) : (prQueryStaStatistics->ucPer);
	}

	for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
		au4RxMpduCnt[ucDbdcIdx] += g_arMibInfo[ucDbdcIdx].u4RxMpduCnt;
		au4FcsError[ucDbdcIdx] += g_arMibInfo[ucDbdcIdx].u4FcsError;
		au4RxFifoCnt[ucDbdcIdx] += g_arMibInfo[ucDbdcIdx].u4RxFifoFull;
		au4AmpduTxSfCnt[ucDbdcIdx] +=
			g_arMibInfo[ucDbdcIdx].u4AmpduTxSfCnt;
		au4AmpduTxAckSfCnt[ucDbdcIdx] +=
			g_arMibInfo[ucDbdcIdx].u4AmpduTxAckSfCnt;

		u4RxPer[ucDbdcIdx] =
		    ((au4RxMpduCnt[ucDbdcIdx] + au4FcsError[ucDbdcIdx]) == 0) ?
			(0) : (1000 * au4FcsError[ucDbdcIdx] /
				(au4RxMpduCnt[ucDbdcIdx] +
				au4FcsError[ucDbdcIdx]));

		u4AmpduPer[ucDbdcIdx] =
		    (au4AmpduTxSfCnt[ucDbdcIdx] == 0) ?
			(0) : (1000 * (au4AmpduTxSfCnt[ucDbdcIdx] -
				au4AmpduTxAckSfCnt[ucDbdcIdx]) /
				au4AmpduTxSfCnt[ucDbdcIdx]);

		u4InstantRxPer[ucDbdcIdx] =
			((prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4RxMpduCnt
			+ prQueryStaStatistics->rMibInfo[ucDbdcIdx].u4FcsError)
			== 0) ?
				(0) : (1000 * prQueryStaStatistics->
				rMibInfo[ucDbdcIdx].u4FcsError /
				(prQueryStaStatistics->rMibInfo[ucDbdcIdx].
				u4RxMpduCnt +
				prQueryStaStatistics->rMibInfo[ucDbdcIdx].
				u4FcsError));
	}

	rRssi = RCPI_TO_dBm(prQueryStaStatistics->ucRcpi);
	u2LinkSpeed = (prQueryStaStatistics->u2LinkSpeed == 0) ? 0 :
					prQueryStaStatistics->u2LinkSpeed / 2;

	if (ucSkipAr) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d(%d)\n", "\nWlanIdx(BackupIdx)", "  = ",
			prHwWlanInfo->u4Index, u2Idx);
	} else {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "\nWlanIdx", "  = ",
			prHwWlanInfo->u4Index);
	}

	/* =========== Group 0x0001 =========== */
	if (u4StatGroup & 0x0001) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- STA Stat (Group 0x01) -----\n");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CurrTemperature", " = ",
			prQueryStaStatistics->ucTemperature);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Tx Total cnt", " = ",
			ucSkipAr ? (u4TotalTxCnt[u2Idx]) :
				(prQueryStaStatistics->u4TransmitCount));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Tx Fail Cnt", " = ",
			ucSkipAr ? (u4TotalFailCnt[u2Idx]) :
				(prQueryStaStatistics->u4TransmitFailCount));

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "Rate1 Tx Cnt", " = ",
			ucSkipAr ? (u4Rate1TxCnt[u2Idx]) :
				(prQueryStaStatistics->u4Rate1TxCnt));

		if (ucSkipAr)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n",
				"Rate1 Fail Cnt", " = ",
				u4Rate1FailCnt[u2Idx], u4Per/10, u4Per%10,
				u4InstantPer/10, u4InstantPer%10);
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d%%\n",
				"Rate1 Fail Cnt", " = ",
				prQueryStaStatistics->u4Rate1FailCnt,
				u4Per/10, u4Per%10, u4InstantPer);

		if ((ucSkipAr) && (fgResetCnt)) {
			u4TotalTxCnt[u2Idx] = 0;
			u4TotalFailCnt[u2Idx] = 0;
			u4Rate1TxCnt[u2Idx] = 0;
			u4Rate1FailCnt[u2Idx] = 0;
		}
	}

	/* =========== Group 0x0002 =========== */
	if (u4StatGroup & 0x0002) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					       i4TotalLen - i4BytesWritten,
			"%s", "----- MIB Info (Group 0x02) -----\n");

		for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
			if (prAdapter->rWifiVar.fgDbDcModeEn)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"[DBDC_%d] :\n", ucDbdcIdx);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "RX Success", " = ",
				au4RxMpduCnt[ucDbdcIdx]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d, PER = %d.%1d%%, instant PER = %d.%1d%%\n",
				"RX with CRC", " = ", au4FcsError[ucDbdcIdx],
				u4RxPer[ucDbdcIdx]/10, u4RxPer[ucDbdcIdx]%10,
				u4InstantRxPer[ucDbdcIdx]/10,
				u4InstantRxPer[ucDbdcIdx]%10);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "RX drop FIFO full", " = ",
				au4RxFifoCnt[ucDbdcIdx]);

			if (!prAdapter->rWifiVar.fgDbDcModeEn)
				break;
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
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					       i4TotalLen - i4BytesWritten,
			"%s", "----- Last Rx Info (Group 0x04) -----\n");

		/* get Beacon RSSI */
		rStatus = kalIoctl(prAdapter->prGlueInfo,
				   wlanoidQueryRssi, &rRssi,
				   sizeof(rRssi), TRUE, TRUE, TRUE,
				   &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, WARN, "unable to retrieve rssi\n");

#if CFG_SUPPORT_ADVANCE_CONTROL
		rSwCtrlInfo.u4Data = 0;
		rSwCtrlInfo.u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID
				   + CMD_ADVCTL_NOISE_ID;

		rStatus = kalIoctl(prAdapter->prGlueInfo,
				   wlanoidQuerySwCtrlRead, &rSwCtrlInfo,
				   sizeof(rSwCtrlInfo), TRUE, TRUE, TRUE,
				   &u4BufLen);

		DBGLOG(REQ, LOUD, "rStatus %u, rSwCtrlInfo.u4Data 0x%x\n",
		       rStatus, rSwCtrlInfo.u4Data);
		if (rStatus == WLAN_STATUS_SUCCESS) {
			i2Wf0AvgPwr = rSwCtrlInfo.u4Data & 0xFFFF;
			i2Wf1AvgPwr = (rSwCtrlInfo.u4Data >> 16) & 0xFFFF;

		}
#endif /* CFG_SUPPORT_ADVANCE_CONTROL */
		i4BytesWritten += kalScnprintf(
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d %d\n", "NOISE", " = ",
			i2Wf0AvgPwr, i2Wf1AvgPwr);

		/* Last RX Rate */
		i4BytesWritten += priv_driver_rx_rate_info(prAdapter,
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			(uint8_t)(prHwWlanInfo->u4Index));

#if CFG_SUPPORT_ADVANCE_CONTROL
		/* Last RX SNR */
		i4BytesWritten += priv_driver_get_snr(prAdapter->prGlueInfo,
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten);
#endif
		/* Last RX RSSI */
		i4BytesWritten += priv_driver_last_rx_rssi(prAdapter,
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			(uint8_t)(prHwWlanInfo->u4Index));

	ucRcpi0 = prHwWlanInfo->rWtblRxCounter.ucRxRcpi0;
	ucRcpi1 = prHwWlanInfo->rWtblRxCounter.ucRxRcpi1;
	if (ucNss > 2) {
		ucRcpi2 = prHwWlanInfo->rWtblRxCounter.ucRxRcpi2;
		ucRcpi3 = prHwWlanInfo->rWtblRxCounter.ucRxRcpi3;
	}

#if CFG_RCPI_COMPENSATION
		if (wlanGetStaIdxByWlanIdx(prAdapter,
		(uint8_t)(prHwWlanInfo->u4Index), &ucStaIdx) ==
		WLAN_STATUS_SUCCESS){
			ucRcpi0 = (ucRcpi0 >= RCPI_MEASUREMENT_NOT_AVAILABLE) ?
				ucRcpi0 : (ucRcpi0 +
				wlanGetCurrChRxFELoss(prAdapter, ucStaIdx, 0));
			ucRcpi1 = (ucRcpi1 >= RCPI_MEASUREMENT_NOT_AVAILABLE) ?
				ucRcpi1 : (ucRcpi1 +
				wlanGetCurrChRxFELoss(prAdapter, ucStaIdx, 1));

			if (ucNss > 2) {
				ucRcpi2 = (ucRcpi2 >=
					RCPI_MEASUREMENT_NOT_AVAILABLE) ?
					ucRcpi2 : (ucRcpi2 +
					wlanGetCurrChRxFELoss(prAdapter,
					ucStaIdx, 2));
				ucRcpi3 = (ucRcpi3 >=
					RCPI_MEASUREMENT_NOT_AVAILABLE) ?
					ucRcpi3 : (ucRcpi3 +
					wlanGetCurrChRxFELoss(prAdapter,
					ucStaIdx, 3));

			}
		}
		DBGLOG(REQ, LOUD, "ucRcpi0:%d ucRcpi1:%d\n", ucRcpi0, ucRcpi1);
#endif /* CFG_RCPI_COMPENSATION */

		/* Last TX Resp RSSI */
		i4BytesWritten += kalScnprintf(
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d %d", "Tx Response RSSI", " = ",
			RCPI_TO_dBm(ucRcpi0), RCPI_TO_dBm(ucRcpi1));
		if (ucNss > 2)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				" %d %d\n",
				RCPI_TO_dBm(ucRcpi2), RCPI_TO_dBm(ucRcpi3));
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "\n");

		/* Last Beacon RSSI */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "Beacon RSSI", " = ", rRssi);
	}

	/* =========== Group 0x0008 =========== */
	if (u4StatGroup & 0x0008) {
		/* TxV */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- Last TX Info (Group 0x08) -----\n");

		for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
			int8_t txpwr;

			txpwr = TX_VECTOR_GET_TX_PWR(
				&prQueryStaStatistics->rTxVector[ucDbdcIdx]);

			if (prAdapter->rWifiVar.fgDbDcModeEn)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"[DBDC_%d] :\n", ucDbdcIdx);

			i4BytesWritten += priv_driver_tx_vector_info(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				&prQueryStaStatistics->rTxVector[ucDbdcIdx]);

			if (prQueryStaStatistics->rTxVector[ucDbdcIdx]
			    .u4TxVector1 == 0xFFFFFFFF)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Chip Out TX Power",
					" = ", "N/A");
			else
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%c%d.%1d dBm\n",
					"Chip Out TX Power", " = ",
					(txpwr < 0) ? '-' : '+',
					abs(txpwr / 2),
					5 * abs(txpwr % 2));

			if (!prAdapter->rWifiVar.fgDbDcModeEn)
				break;
		}
	}

	/* =========== Group 0x0010 =========== */
	if (u4StatGroup & 0x0010) {
		/* RX Reorder */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- RX Reorder (Group 0x10) -----\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder miss", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_MISS_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder within", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_WITHIN_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder ahead", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_AHEAD_COUNT));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%ld\n", "Rx reorder behind", " = ",
			RX_GET_CNT(prRxCtrl, RX_DATA_REORDER_BEHIND_COUNT));
	}

	/* =========== Group 0x0020 =========== */
	if (u4StatGroup & 0x0020) {
		/* RA info */
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "----- RA Info (Group 0x20) -----\n");
#if 0
		/* Last TX Rate */
		i4BytesWritten += priv_driver_tx_rate_info(
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			FALSE, prHwWlanInfo, prQueryStaStatistics);
#endif
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%-20s%s%d\n", "LinkSpeed",
			" = ", u2LinkSpeed);

		if (!prQueryStaStatistics->ucSkipAr) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "RateTable", " = ",
				prQueryStaStatistics->ucArTableIdx <
				    (ucRaTableNum - 1) ?
				    RATE_TBLE[
					prQueryStaStatistics->ucArTableIdx] :
					RATE_TBLE[ucRaTableNum - 1]);

			if (wlanGetStaIdxByWlanIdx(prAdapter,
			    (uint8_t)(prHwWlanInfo->u4Index), &ucStaIdx) ==
			    WLAN_STATUS_SUCCESS){
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "2G Support 256QAM TX",
					" = ",
					((prAdapter->arStaRec[ucStaIdx].u4Flags
					 & MTK_SYNERGY_CAP_SUPPORT_24G_MCS89) ||
					(prQueryStaStatistics->
					 ucDynamicGband256QAMState == 2)) ?
					1 : 0);
			}
#if 0
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d%%\n", "Rate1 instantPer", " = ",
				u4InstantPer);
#endif
			if (prQueryStaStatistics->ucAvePer == 0xFF) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Train Down", " = ",
					"N/A");

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Train Up", " = ",
					"N/A");
			} else {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d -> %d\n", "Train Down",
					" = ",
					(uint16_t)
					(prQueryStaStatistics->u2TrainDown
						& BITS(0, 7)),
					(uint16_t)
					((prQueryStaStatistics->u2TrainDown >>
						8) & BITS(0, 7)));

				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d -> %d\n", "Train Up", " = ",
					(uint16_t)
					(prQueryStaStatistics->u2TrainUp
						& BITS(0, 7)),
					(uint16_t)
					((prQueryStaStatistics->u2TrainUp >> 8)
						& BITS(0, 7)));
			}

			if (prQueryStaStatistics->fgIsForceTxStream == 0)
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%s\n", "Force Tx Stream",
					" = ", "N/A");
			else
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%-20s%s%d\n", "Force Tx Stream", " = ",
					prQueryStaStatistics->
						fgIsForceTxStream);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "Force SE off", " = ",
				prQueryStaStatistics->fgIsForceSeOff);
#if 0
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s%d%s%d%s%d%s%d\n", "TxQuality", " = ",
				"KEEP_", prQueryStaStatistics->aucTxQuality[0],
				", UP_", prQueryStaStatistics->aucTxQuality[1],
				", DOWN_", prQueryStaStatistics->
					aucTxQuality[2],
				", BWUP_", prQueryStaStatistics->
					aucTxQuality[3]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "UpPenalty", " = ",
				prQueryStaStatistics->ucTxRateUpPenalty);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "LtMode", " = ",
				prQueryStaStatistics->ucLowTrafficMode <
				(ucRaLtModeNum - 1) ?
				LT_MODE_TBLE[prQueryStaStatistics->
				ucLowTrafficMode] :
				LT_MODE_TBLE[ucRaLtModeNum - 1]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "LtCnt", " = ",
				prQueryStaStatistics->ucLowTrafficCount);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "LtDashBoard", " = ",
				prQueryStaStatistics->ucLowTrafficDashBoard);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "SgiState", " = ",
				prQueryStaStatistics->ucDynamicSGIState <
				(ucRaSgiUnSpStateNum - 1) ?
				SGI_UNSP_STATE_TBLE[prQueryStaStatistics->
				ucDynamicSGIState] :
				SGI_UNSP_STATE_TBLE[ucRaSgiUnSpStateNum - 1]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%d\n", "SgiScore", " = ",
				prQueryStaStatistics->ucDynamicSGIScore);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "BwState", " = ",
				prQueryStaStatistics->ucDynamicBWState <
				(ucRaBwStateNum - 1) ?
				BW_STATE_TBLE[prQueryStaStatistics->
				ucDynamicBWState] :
				BW_STATE_TBLE[ucRaBwStateNum - 1]);

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-20s%s%s\n", "NonSpState", " = ",
				prQueryStaStatistics->ucDynamicSGIState <
				(ucRaSgiUnSpStateNum - 1) ?
				SGI_UNSP_STATE_TBLE[prQueryStaStatistics->
				ucVhtNonSpRateState] :
				SGI_UNSP_STATE_TBLE[ucRaSgiUnSpStateNum - 1]);
#endif
		}

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RunningCnt", " = ",
			prQueryStaStatistics->u2RaRunningCnt);

		prQueryStaStatistics->ucRaStatus &= ~0x80;
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "Status", " = ",
			prQueryStaStatistics->ucRaStatus < (ucRaStatusNum - 1) ?
			RA_STATUS_TBLE[prQueryStaStatistics->ucRaStatus] :
			RA_STATUS_TBLE[ucRaStatusNum - 1]);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "MaxAF", " = ",
			prHwWlanInfo->rWtblPeerCap.ucAmpduFactor);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s0x%x\n", "SpeIdx", " = ",
			prHwWlanInfo->rWtblPeerCap.ucSpatialExtensionIndex);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "CBRN", " = ",
			prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN);

		/* Rate1~Rate8 */
		i4BytesWritten += priv_driver_tx_rate_info(
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			TRUE, prHwWlanInfo, prQueryStaStatistics);
	}

	/* =========== Group 0x0040 =========== */
	if (u4StatGroup & 0x0040) {
		u4RangeCtrl_0 = prQueryStaStatistics->u4AggRangeCtrl_0;
		u4RangeCtrl_1 = prQueryStaStatistics->u4AggRangeCtrl_1;
		eRangeType = (enum AGG_RANGE_TYPE_T)
					prQueryStaStatistics->ucRangeType;

		ucAggRange[0] = (((u4RangeCtrl_0) & AGG_RANGE_SEL_0_MASK) >>
						AGG_RANGE_SEL_0_OFFSET);
		ucAggRange[1] = (((u4RangeCtrl_0) & AGG_RANGE_SEL_1_MASK) >>
						AGG_RANGE_SEL_1_OFFSET);
		ucAggRange[2] = (((u4RangeCtrl_0) & AGG_RANGE_SEL_2_MASK) >>
						AGG_RANGE_SEL_2_OFFSET);
		ucAggRange[3] = (((u4RangeCtrl_0) & AGG_RANGE_SEL_3_MASK) >>
						AGG_RANGE_SEL_3_OFFSET);
		ucAggRange[4] = (((u4RangeCtrl_1) & AGG_RANGE_SEL_4_MASK) >>
						AGG_RANGE_SEL_4_OFFSET);
		ucAggRange[5] = (((u4RangeCtrl_1) & AGG_RANGE_SEL_5_MASK) >>
						AGG_RANGE_SEL_5_OFFSET);
		ucAggRange[6] = (((u4RangeCtrl_1) & AGG_RANGE_SEL_6_MASK) >>
						AGG_RANGE_SEL_6_OFFSET);

		/* Tx Agg */
		i4BytesWritten += kalScnprintf(
			pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s%s%s", "------ ",
			(eRangeType > ENUM_AGG_RANGE_TYPE_TX) ? (
				(eRangeType == ENUM_AGG_RANGE_TYPE_TRX) ?
				("TRX") : ("RX")) : ("TX"),
				" AGG (Group 0x40) -----\n");

		if (eRangeType == ENUM_AGG_RANGE_TYPE_TRX) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-6s%8d%5d%1s%2d%5d%1s%2d%5d%3s",
				" TX  :", ucAggRange[0] + 1,
				ucAggRange[0] + 2, "~", ucAggRange[1] + 1,
				ucAggRange[1] + 2, "~", ucAggRange[2] + 1,
				ucAggRange[2] + 2, "~64\n");

			for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM;
			     ucDbdcIdx++) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"DBDC%d:", ucDbdcIdx);
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%8d%8d%8d%8d\n",
					g_arMibInfo[ucDbdcIdx].
					u2TxRange1AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange2AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange3AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange4AmpduCnt);

				if (!prAdapter->rWifiVar.fgDbDcModeEn)
					break;
			}

			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%-6s%8d%5d%1s%2d%5d%1s%2d%5d%3s",
				" RX  :", ucAggRange[3] + 1,
				ucAggRange[3] + 2, "~", ucAggRange[4] + 1,
				ucAggRange[4] + 2, "~", ucAggRange[5] + 1,
				ucAggRange[5] + 2, "~64\n");

			for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM;
			     ucDbdcIdx++) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"DBDC%d:", ucDbdcIdx);
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%8d%8d%8d%8d\n",
					g_arMibInfo[ucDbdcIdx].
					u2TxRange5AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange6AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange7AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange8AmpduCnt);

				if (!prAdapter->rWifiVar.fgDbDcModeEn)
					break;
			}
		} else {
			i4BytesWritten += kalScnprintf(
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
				"%-6s%8d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%1s%2d%5d%3s",
				"Range:", ucAggRange[0] + 1,
				ucAggRange[0] + 2, "~", ucAggRange[1] + 1,
				ucAggRange[1] + 2, "~", ucAggRange[2] + 1,
				ucAggRange[2] + 2, "~", ucAggRange[3] + 1,
				ucAggRange[3] + 2, "~", ucAggRange[4] + 1,
				ucAggRange[4] + 2, "~", ucAggRange[5] + 1,
				ucAggRange[5] + 2, "~", ucAggRange[6] + 1,
				ucAggRange[6] + 2, "~64\n");

			for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM;
			     ucDbdcIdx++) {
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"DBDC%d:", ucDbdcIdx);
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%8d%8d%8d%8d%8d%8d%8d%8d\n",
					g_arMibInfo[ucDbdcIdx].
					u2TxRange1AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange2AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange3AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange4AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange5AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange6AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange7AmpduCnt,
					g_arMibInfo[ucDbdcIdx].
					u2TxRange8AmpduCnt);

				if (!prAdapter->rWifiVar.fgDbDcModeEn)
					break;
			}
		}
	}

	kalMemZero(g_arMibInfo, sizeof(g_arMibInfo));

	return i4BytesWritten;
}

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
		i4BytesWritten = snprintf(pcCommand, i4TotalLen,
			"Update fgIsUseWCID %d\n", fgIsUseWCID);
	} else if (i4Recv == 11) {
		rSwCtrlInfo.u4Id = u4Id;
		rSwCtrlInfo.u4Data = u4Data;

		if (fgIsUseWCID && u4WCID < WTBL_SIZE &&
			prAdapter->rWifiVar.arWtbl[u4WCID].ucUsed) {
			rSwCtrlInfo.u4Id |= u4WCID;
			rSwCtrlInfo.u4Id |= BIT(8);
			i4BytesWritten = snprintf(
				pcCommand, i4TotalLen,
				"Apply WCID %d\n", u4WCID);
		} else {
			i4BytesWritten = snprintf(
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
#define CCK_IDX 0
#define OFDM_IDX 1
#define HT20_IDX 2
#define HT40_IDX 3
#define VHT20_IDX 4
#define VHT40_IDX 5
#define VHT80_IDX 6
#define VHT160_IDX 7

		uint8_t ucTxPwrIdx = 0, ucTxPwrType = 0, ucIdx = 0,
			ucIdxOffset = 0;
		u_int8_t fgIsHt = TRUE;
		uint8_t *pucTxPwrRate = NULL;
		struct FRAME_POWER_CONFIG_INFO_T rRatePowerInfo;
		uint8_t ucTxPwrCckRate[MODULATION_SYSTEM_CCK_NUM] = {
						1, 2, 5, 11};
		uint8_t ucTxPwrOfdmRate[MODULATION_SYSTEM_OFDM_NUM] = {
						6, 9, 12, 18, 24, 36, 48, 54 };
		uint8_t ucTxPwrHt20Rate[MODULATION_SYSTEM_HT20_NUM] = {
						0, 1, 2, 3, 4, 5, 6, 7 };
		uint8_t ucTxPwrHt40Rate[MODULATION_SYSTEM_HT40_NUM] = {
						0, 1, 2, 3, 4, 5, 6, 7, 32 };
		uint8_t *POWER_TYPE_STR[] = {
			[CCK_IDX] = "CCK",
			[OFDM_IDX] = "OFDM",
			[HT20_IDX] = "HT20",
			[HT40_IDX] = "HT40",
			[VHT20_IDX] = "VHT20",
			[VHT40_IDX] = "VHT40",
			[VHT80_IDX] = "VHT80",
			[VHT160_IDX] = "VHT160"};
		uint8_t ucPwrIdxLen[] = {
		    MODULATION_SYSTEM_CCK_NUM, MODULATION_SYSTEM_OFDM_NUM,
		    MODULATION_SYSTEM_HT20_NUM, MODULATION_SYSTEM_HT40_NUM,
		    MODULATION_SYSTEM_VHT20_NUM, MODULATION_SYSTEM_VHT40_NUM,
		    MODULATION_SYSTEM_VHT80_NUM, MODULATION_SYSTEM_VHT160_NUM };

		if ((sizeof(POWER_TYPE_STR)/sizeof(uint8_t *)) !=
		    (sizeof(ucPwrIdxLen)/sizeof(uint8_t)))
			return i4BytesWritten;

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%s",
					"\n====== TX POWER INFO ======\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%s",
					"------\n");
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"DBDC Index: %d, Channel Band: %s\n",
					prTxPowerInfo->ucBandIdx,
					(prTxPowerInfo->ucChBand) ?
						("5G") : ("2G"));
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "------\n");

		for (ucTxPwrType = 0;
		     ucTxPwrType < sizeof(POWER_TYPE_STR)/sizeof(uint8_t *);
		     ucTxPwrType++) {
			for (ucTxPwrIdx = 0;
			     ucTxPwrIdx < ucPwrIdxLen[ucTxPwrType];
			     ucTxPwrIdx++) {
				if ((ucTxPwrType == CCK_IDX) ||
				    (ucTxPwrType == OFDM_IDX)) {
					pucTxPwrRate =
						(ucTxPwrType == CCK_IDX) ?
							(ucTxPwrCckRate) :
							(ucTxPwrOfdmRate);

					ucIdx = ucTxPwrIdx + ucIdxOffset;
					rRatePowerInfo =
						prTxPowerInfo->rRatePowerInfo;

					i4BytesWritten += kalScnprintf(
						pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
						"[%s_%02dM]: 0x%02x (%03d)\n",
						POWER_TYPE_STR[ucTxPwrType],
						pucTxPwrRate[ucTxPwrIdx],
						rRatePowerInfo.
						aicFramePowerConfig[ucIdx].
						icFramePowerDbm,
						rRatePowerInfo.
						aicFramePowerConfig[ucIdx].
						icFramePowerDbm);
				} else {
					if (ucTxPwrType == HT20_IDX) {
						pucTxPwrRate = ucTxPwrHt20Rate;
						fgIsHt = TRUE;
					} else if (ucTxPwrType == HT40_IDX) {
						pucTxPwrRate = ucTxPwrHt40Rate;
						fgIsHt = TRUE;
					} else {
						fgIsHt = FALSE;
					}

					rRatePowerInfo =
						prTxPowerInfo->rRatePowerInfo;

					i4BytesWritten += kalScnprintf(
						pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
						"[%s_M%02d]: 0x%02x (%03d)\n",
						POWER_TYPE_STR[ucTxPwrType],
						((fgIsHt) ?
						    (pucTxPwrRate[ucTxPwrIdx]) :
						    (ucTxPwrIdx)),
						rRatePowerInfo.
						aicFramePowerConfig[ucIdx]
						.icFramePowerDbm,
						rRatePowerInfo.
						aicFramePowerConfig[ucIdx]
						.icFramePowerDbm);
				}
			}

			if ((ucTxPwrType == OFDM_IDX) ||
			    (ucTxPwrType == HT40_IDX) ||
			    (ucTxPwrType == VHT160_IDX))
				i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"%s", "------\n");

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
	uint32_t u4Ret = 0;
	uint8_t ucParam = 0;
	struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T *prTxPowerInfo = NULL;

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
	u4Ret = kalkStrtou8(this_char, 0, &ucParam);
	DBGLOG(REQ, LOUD, "u4Ret = %d, ucParam = %d\n", u4Ret, ucParam);

	u4Size = sizeof(struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T);

	prTxPowerInfo =
		(struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T *)kalMemAlloc(
							u4Size, VIR_MEM_TYPE);
	if (!prTxPowerInfo)
		return -1;

	if (u4Ret == 0) {
		prTxPowerInfo->ucTxPowerCategory = ucParam;

		/*
		 * FIX ME: Mobile driver can't get correct band,
		 * so assigned Band_0 as temp solution.
		 * Remember to fix it when needed to use this command on Band_1.
		 */
		prTxPowerInfo->ucBandIdx = ENUM_BAND_0;
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
		       "iwpriv wlanXX driver TxPowerInfo=[Param]\n");
	}

	kalMemFree(prTxPowerInfo, VIR_MEM_TYPE,
		   sizeof(struct PARAM_TXPOWER_ALL_RATE_POWER_INFO_T));

	return i4BytesWritten;
}
#endif

static int priv_driver_get_sta_stat(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0, u4Ret, u4StatGroup = 0xFFFFFFFF;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint8_t aucMacAddr[MAC_ADDR_LEN];
	uint8_t ucWlanIndex;
	uint8_t *pucMacAddr = NULL;
	struct PARAM_HW_WLAN_INFO *prHwWlanInfo = NULL;
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics = NULL;
	u_int8_t fgResetCnt = FALSE;
	u_int8_t fgRxCCSel = FALSE;
	u_int8_t fgSearchMacAddr = FALSE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

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
					"wlan index of %pM is not found!\n",
					aucMacAddr);
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

			if (prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP) {
				ucWlanIndex = prGlueInfo->prAdapter
						->prAisBssInfo->prStaRecOfAP
						->ucWlanIndex;
			} else if (!wlanGetWlanIdxByAddress(
				prGlueInfo->prAdapter, NULL,
				&ucWlanIndex)) {
				DBGLOG(REQ, INFO,
					"wlan index of %pM is not found!\n",
					aucMacAddr);
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
					"wlan index of %pM is not found!\n",
					aucMacAddr);
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
			if (prGlueInfo->prAdapter->prAisBssInfo->prStaRecOfAP) {
				ucWlanIndex = prGlueInfo->prAdapter->
					prAisBssInfo->prStaRecOfAP->ucWlanIndex;
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

	DBGLOG(REQ, INFO, "MT6632 : index = %d i4TotalLen = %d\n",
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
		i4BytesWritten = priv_driver_dump_stat_info(prAdapter,
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
					IN char *pcCommand, IN int i4TotalLen,
					IN u_int8_t fgResetCnt)
{
	int32_t i4BytesWritten = 0;
	uint32_t u4RxVector0 = 0, u4RxVector2 = 0, u4RxVector3 = 0,
		 u4RxVector4 = 0;
	uint8_t ucStaIdx, ucWlanIndex, cbw;
	u_int8_t fgWlanIdxFound = TRUE, fgSkipRxV = FALSE;
	uint32_t u4FAGCRssiWBR0, u4FAGCRssiIBR0;
	uint32_t u4Value, u4Foe, foe_const;
	static uint32_t au4MacMdrdy[ENUM_BAND_NUM] = {0};
	static uint32_t au4FcsError[ENUM_BAND_NUM] = {0};
	static uint32_t au4OutOfResource[ENUM_BAND_NUM] = {0};
	static uint32_t au4LengthMismatch[ENUM_BAND_NUM] = {0};

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

	if (fgResetCnt) {
		kalMemZero(au4MacMdrdy, sizeof(au4MacMdrdy));
		kalMemZero(au4FcsError, sizeof(au4FcsError));
		kalMemZero(au4OutOfResource, sizeof(au4OutOfResource));
		kalMemZero(au4LengthMismatch, sizeof(au4LengthMismatch));
	}

	if (prAdapter->prAisBssInfo->prStaRecOfAP)
		ucWlanIndex =
			prAdapter->prAisBssInfo->prStaRecOfAP->ucWlanIndex;
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

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%s", "\n\nRX Stat:\n");
#if 0
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "PER0", " = ",
			g_HqaRxStat.PER0);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "PER1", " = ",
			g_HqaRxStat.PER1);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RX OK0", " = ",
			g_HqaRxStat.RXOK0);

	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "RX OK1", " = ",
			g_HqaRxStat.RXOK1);
#endif
	i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%d\n", "MAC Mdrdy0", " = ",
			au4MacMdrdy[ENUM_BAND_0]);

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
			"%-20s%s%s\n", "FAGC RSSI W", " = ", "N/A");

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"%-20s%s%s\n", "FAGC RSSI I", " = ", "N/A");
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
#if 0
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%s\n", "Driver RX Cnt0",
					      " = ", "N/A");

		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%s\n", "Driver RX Cnt1",
					      " = ", "N/A");
#endif
		i4BytesWritten += kalSnprintf(pcCommand + i4BytesWritten,
					      i4TotalLen - i4BytesWritten,
					      "%-20s%s%s\n",
					      "Freq Offset From RX",
					      " = ", "N/A");

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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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

	DBGLOG(INIT, ERROR, "MT6632 : priv_driver_show_rx_stat\n");

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
					pcCommand, i4TotalLen, fgResetCnt);

		kalMemFree(prRxStatisticsTest, VIR_MEM_TYPE,
			   sizeof(struct PARAM_CUSTOM_ACCESS_RX_STAT));
	}

	return i4BytesWritten;
}

static int priv_driver_get_ipi(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int32_t u4Ret = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	struct PARAM_GET_IPI_INFO_T rCmdGetIpiInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	kalMemZero(&rCmdGetIpiInfo, sizeof(struct PARAM_GET_IPI_INFO_T));

	if (i4Argc >= 3) {
		u4Ret = kalkStrtou32(apcArgv[1], 0,
				&(rCmdGetIpiInfo.u4DurInSec));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
				"parse get_ipi error (DurInSec) u4Ret=%d\n",
				u4Ret);

		u4Ret = kalkStrtou32(apcArgv[2], 0,
				&(rCmdGetIpiInfo.u4Interval));
		if (u4Ret)
			DBGLOG(REQ, LOUD,
				"parse get_ipi error (Interval) u4Ret=%d\n",
				u4Ret);

		rStatus = kalIoctl(prGlueInfo, wlanoidGetIpiInfo,
				   &rCmdGetIpiInfo, sizeof(rCmdGetIpiInfo),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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

		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "0x%08x",
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
		/* rSwCtrlInfo.u4Id = kalStrtoul(apcArgv[1], NULL, 0); */
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

		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "0x%08x",
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
	uint32_t u4Mode = 0, u4Bw = 0, u4Mcs = 0, u4VhtNss = 0;
	uint32_t u4SGI = 0, u4Preamble = 0, u4STBC = 0, u4LDPC = 0, u4SpeEn = 0;
	int32_t i4Recv = 0;
	int8_t *this_char = NULL;
	uint32_t u4Id = 0xa0610000;
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
		i4Recv = sscanf(this_char, "%d-%d-%d-%d-%d-%d-%d-%d-%d-%d",
				&(u4WCID), &(u4Mode), &(u4Bw), &(u4Mcs),
				&(u4VhtNss), &(u4SGI), &(u4Preamble), &(u4STBC),
				&(u4LDPC), &(u4SpeEn));

		DBGLOG(REQ, LOUD, "u4WCID=%d\nu4Mode=%d\nu4Bw=%d\n", u4WCID,
		       u4Mode, u4Bw);
		DBGLOG(REQ, LOUD, "u4Mcs=%d\nu4VhtNss=%d\nu4SGI=%d\n", u4Mcs,
		       u4VhtNss, u4SGI);
		DBGLOG(REQ, LOUD, "u4Preamble=%d\nu4STBC=%d\n", u4Preamble,
		       u4STBC);
		DBGLOG(REQ, LOUD, "u4LDPC=%d\nu4SpeEn=%d\nfgIsUseWCID=%d\n",
			u4LDPC, u4SpeEn, fgIsUseWCID);
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
		i4BytesWritten = snprintf(pcCommand, i4TotalLen,
			"Update fgIsUseWCID %d\n", fgIsUseWCID);
	} else if (i4Recv == 10) {
		rSwCtrlInfo.u4Id = u4Id;
		rSwCtrlInfo.u4Data = u4Data;

		if (fgIsUseWCID && u4WCID < WTBL_SIZE &&
			prAdapter->rWifiVar.arWtbl[u4WCID].ucUsed) {
			rSwCtrlInfo.u4Id |= u4WCID;
			rSwCtrlInfo.u4Id |= BIT(8);
			i4BytesWritten = snprintf(
				pcCommand, i4TotalLen,
				"Apply WCID %d\n", u4WCID);
		} else {
			i4BytesWritten = snprintf(
				pcCommand, i4TotalLen, "Apply All\n");
		}

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
		       "Option:[WCID]-[Mode]-[BW]-[MCS]-[VhtNss]-[SGI]-[Preamble]-[STBC]-[LDPC]-[SPE_EN]\n");
		DBGLOG(INIT, ERROR, "[WCID]Wireless Client ID\n");
		DBGLOG(INIT, ERROR, "[Mode]CCK=0, OFDM=1, HT=2, GF=3, VHT=4\n");
		DBGLOG(INIT, ERROR, "[BW]BW20=0, BW40=1, BW80=2,BW160=3\n");
		DBGLOG(INIT, ERROR,
		       "[MCS]CCK=0~3, OFDM=0~7, HT=0~32, VHT=0~9\n");
		DBGLOG(INIT, ERROR, "[VhtNss]VHT=1~4, Other=ignore\n");
		DBGLOG(INIT, ERROR, "[Preamble]Long=0, Other=Short\n");
	}

	return i4BytesWritten;
}				/* priv_driver_set_fixed_rate */

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
			kalMemCopy(pucCurrBuf + offset, apcArgv[2], u4BufLen);
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
				kalMemCopy(pucCurrBuf + offset, apcArgv[i],
					   u4BufLen);
				offset += u4BufLen;
			}
		}

		DBGLOG(INIT, WARN, "Update to driver temp buffer as [%s]\n",
		       ucTmp);

		/* wlanCfgSet(prAdapter, apcArgv[1], apcArgv[2], 0); */
		/* Call by  wlanoid because the set_cfg will trigger callback */
		kalMemCopy(rKeyCfgInfo.aucKey, apcArgv[1],
			   WLAN_CFG_KEY_LEN_MAX);
		kalStrnCpy(rKeyCfgInfo.aucValue, ucTmp, WLAN_CFG_KEY_LEN_MAX);
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
			    snprintf(pcCommand + i4BytesWritten,
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
			    snprintf(pcCommand + i4BytesWritten,
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
				    snprintf(pcCommand + i4BytesWritten,
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
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	uint32_t u4Rate = 0;
	uint32_t u4LinkSpeed = 0;
	int32_t i4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (!netif_carrier_ok(prNetDev))
		return -1;

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryLinkSpeed, &u4Rate,
			   sizeof(u4Rate), TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4LinkSpeed = u4Rate * 100;
	i4BytesWritten = snprintf(pcCommand, i4TotalLen, "LinkSpeed %u",
				  (unsigned int)u4LinkSpeed);
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
	uint8_t ucBssIndex;
	enum ENUM_BAND eBand = BAND_NULL;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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

		ucBssIndex = wlanGetAisBssIndex(prGlueInfo->prAdapter);
		eBand = BAND_NULL;
		if (ucBand == CMD_BAND_5G)
			eBand = BAND_5G;
		else if (ucBand == CMD_BAND_2G)
			eBand = BAND_2G4;
		prAdapter->aePreferBand[ucBssIndex] = eBand;
		/* XXX call wlanSetPreferBandByNetwork directly in different
		 * thread
		 */
		/* wlanSetPreferBandByNetwork (prAdapter, eBand, ucBssIndex); */
	}

	return 0;
}

int priv_driver_set_txpower(IN struct net_device *prNetDev,
			    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	struct SET_TXPWR_CTRL *prTxpwr;
	uint16_t i;
	int32_t u4Ret = 0;
	int32_t ai4Setting[4];

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prTxpwr = &prGlueInfo->rTxPwr;

	if (i4Argc >= 3 && i4Argc <= 5) {
		for (i = 0; i < (i4Argc - 1); i++) {
			/* ai4Setting[i] = kalStrtol(apcArgv[i + 1], NULL, 0);
			 */
			u4Ret = kalkStrtos32(apcArgv[i + 1], 0,
					     &(ai4Setting[i]));
			if (u4Ret)
				DBGLOG(REQ, LOUD,
				       "parse apcArgv error u4Ret=%d\n", u4Ret);
		}
	} else {
		DBGLOG(REQ, INFO, "set_txpower wrong argc : %d\n", i4Argc);
		return -1;
	}

	/*
	 *  ai4Setting[0]
	 *  0 : Set TX power offset for specific network
	 *  1 : Set TX power offset policy when multiple networks are in the
	 *      same channel
	 *  2 : Set TX power limit for specific channel in 2.4GHz band
	 *  3 : Set TX power limit of specific sub-band in 5GHz band
	 *  4 : Enable or reset setting
	 */
	if (ai4Setting[0] == 0 && (i4Argc - 1) == 4 /* argc num */) {
		/* ai4Setting[1] : 0 (All networks), 1 (legacy STA),
		 *                 2 (Hotspot AP), 3 (P2P), 4 (BT over Wi-Fi)
		 * ai4Setting[2] : 0 (All bands),1 (2.4G), 2 (5G)
		 * ai4Setting[3] : -30 ~ 20 in unit of 0.5dBm (default: 0)
		 */
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
		/* ai4Setting[1] : 0 (highest power is used) (default),
		 *                 1 (lowest power is used)
		 */
		prTxpwr->ucConcurrencePolicy = ai4Setting[1];
	} else if (ai4Setting[0] == 2 && (i4Argc - 1) == 3) {
		/* ai4Setting[1] : 0 (all channels in 2.4G), 1~14 */
		/* ai4Setting[2] : 10 ~ 46 in unit of 0.5dBm (default: 46) */
		if (ai4Setting[1] == 0) {
			for (i = 0; i < 14; i++)
				prTxpwr->acTxPwrLimit2G[i] = ai4Setting[2];
		} else if (ai4Setting[1] <= 14)
			prTxpwr->acTxPwrLimit2G[ai4Setting[1] - 1] =
								ai4Setting[2];
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
			prTxpwr->acTxPwrLimit5G[ai4Setting[1] - 1] =
								ai4Setting[2];
	} else if (ai4Setting[0] == 4 && (i4Argc - 1) == 2) {
		/* ai4Setting[1] : 1 (enable), 0 (reset and disable) */
		if (ai4Setting[1] == 0)
			wlanDefTxPowerCfg(prGlueInfo->prAdapter);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetTxPower, prTxpwr,
				   sizeof(struct SET_TXPWR_CTRL),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -1;
	} else
		return -EFAULT;

	return 0;
}

int priv_driver_set_country(IN struct net_device *prNetDev,
			    IN char *pcCommand, IN int i4TotalLen)
{

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t ch_num = 0;
	uint32_t u4Ret = 0;
	uint8_t ucRoleIdx = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	uint32_t ch_idx, start_idx, end_idx;
	struct channel *pCh;
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

			if (ch_num && (ch_num != pCh->chNum))
				continue; /*show specific channel information*/

			/* Channel number */
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "CH-%d:",
					pCh->chNum);

			/* Active/Passive */
			if (pCh->flags & IEEE80211_CHAN_PASSIVE_FLAG) {
				/* passive channel */
				LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				       " " IEEE80211_CHAN_PASSIVE_STR);
			} else
				LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				       " ACTIVE");

			/* Max BW */
			if ((pCh->flags & IEEE80211_CHAN_NO_160MHZ) ==
			    IEEE80211_CHAN_NO_160MHZ)
				maxbw = 80;
			if ((pCh->flags & IEEE80211_CHAN_NO_80MHZ) ==
			    IEEE80211_CHAN_NO_80MHZ)
				maxbw = 40;
			if ((pCh->flags & IEEE80211_CHAN_NO_HT40) ==
			    IEEE80211_CHAN_NO_HT40)
				maxbw = 20;
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			       " BW_%dMHz", maxbw);
			/* Channel flags */
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			       "  (flags=0x%x)\n", pCh->flags);
		}
	}
#endif

	return i4BytesWritten;
}

int priv_driver_get_ap_channels(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
#if (CFG_SUPPORT_SINGLE_SKU == 1)
	uint32_t ch_idx, start_idx, end_idx;
	struct channel *pCh;
	uint32_t ch_num = 0;
	uint8_t maxbw = 160;
	uint32_t u4Ret = 0;
#endif
	struct ADAPTER *prAdapter = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct WIFI_VAR *prWifiVar = NULL;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	prWifiVar = &prAdapter->rWifiVar;

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
	 * Usage: iwpriv wlan0 driver "get_ap_channels [2g |5g |ch_num]"
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
			uint8_t ApMaxbw = 0;

			pCh = (rlmDomainGetActiveChannels() + ch_idx);

			if (ch_num && (ch_num != pCh->chNum))
				continue; /*show specific channel information*/

			/* Channel number */
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten, "CH-%d:",
					pCh->chNum);
			/* Active/Passive */
			if (pCh->flags & IEEE80211_CHAN_PASSIVE_FLAG) {
				LOGBUF(pcCommand, i4TotalLen,
					i4BytesWritten,
					" " IEEE80211_CHAN_PASSIVE_STR);
			} else {
				LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
					" ACTIVE");
			}
			/* Max BW */
			if ((pCh->flags & IEEE80211_CHAN_NO_160MHZ) ==
				IEEE80211_CHAN_NO_160MHZ)
				maxbw = 80;
			if ((pCh->flags & IEEE80211_CHAN_NO_80MHZ) ==
				IEEE80211_CHAN_NO_80MHZ)
				maxbw = 40;
			if ((pCh->flags & IEEE80211_CHAN_NO_HT40) ==
				IEEE80211_CHAN_NO_HT40)
				maxbw = 20;

			/* Checking 2G BW setting */
			if (pCh->chNum < MAX_2G_BAND_CHN_NUM) {
				switch (prWifiVar->ucAp2gBandwidth) {
				case MAX_BW_40MHZ:
					ApMaxbw = 40;
					break;
				case MAX_BW_20MHZ:
					ApMaxbw = 20;
					break;
				default:
					ApMaxbw = 40;
					break;
				}
			} else {
				switch (prWifiVar->ucAp5gBandwidth) {
				case MAX_BW_80MHZ:
					ApMaxbw = 80;
					break;
				case MAX_BW_40MHZ:
					ApMaxbw = 40;
					break;
				case MAX_BW_20MHZ:
					ApMaxbw = 20;
					break;
				default:
					ApMaxbw = 80;
					break;
				}
			}
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				" BW_%dMHz",
				((maxbw <= ApMaxbw)?(maxbw):(ApMaxbw)));
			/* Channel flags */
			LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
				"  (flags=0x%x)\n",
				pCh->flags);
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4BytesWritten = 0;
	uint8_t ucCnt = 0;
	struct P2P_RADAR_INFO *prP2pRadarInfo = (struct P2P_RADAR_INFO *) NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	prP2pRadarInfo = (struct P2P_RADAR_INFO *) cnmMemAlloc(
		prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(*prP2pRadarInfo));

	if (prP2pRadarInfo == NULL) {
		DBGLOG(REQ, ERROR,
			   "NCHO no memory for P2pRadarInfo req\n");
		return -1;
	}

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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
#ifdef CFG_SUPPORT_ADJUST_MCC_STAY_TIME
#define CFG_MCC_AIS_QUOTA_TYPE                  0
#define CFG_MCC_P2PGC_QUOTA_TYPE                1
#define CFG_MCC_P2PGO_QUOTA_TYPE                2

int priv_driver_set_mcc_time(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct ADAPTER *prAdapter = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	uint32_t i4BytesWritten = 0;
	uint32_t ucLinkType = 0; /* 0 for AIS and 1 for P2P */
	uint32_t ucStayTime = 0; /* In unit of us */
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = 0;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prAdapter = prGlueInfo->prAdapter;
	if (i4Argc != 3)
		goto out;

	i4Ret = kalkStrtou32(apcArgv[1], 0, &ucLinkType);
	if (i4Ret) {
		DBGLOG(REQ, LOUD, "parse ucLinkType error i4Ret=%d\n", i4Ret);
		goto out;
	}
	i4Ret = kalkStrtou32(apcArgv[2], 0, &ucStayTime);
	if (i4Ret) {
		DBGLOG(REQ, LOUD, "parse ucStayTime error i4Ret=%d\n", i4Ret);
		goto out;
	}
	if (ucLinkType == CFG_MCC_AIS_QUOTA_TYPE) {
		snprintf(pcCommand, i4TotalLen, CMD_SET_CHIP " mccTime 0 %d",
					ucStayTime);
	} else if (ucLinkType == CFG_MCC_P2PGC_QUOTA_TYPE) {
		snprintf(pcCommand, i4TotalLen, CMD_SET_CHIP " mccTime 1 %d",
					ucStayTime);
	} else if (ucLinkType == CFG_MCC_P2PGO_QUOTA_TYPE) {
		snprintf(pcCommand, i4TotalLen, CMD_SET_CHIP " mccTime 2 %d",
					ucStayTime);
	} else {
		DBGLOG(REQ, LOUD, "Wrong network type %i\n", ucLinkType);
		i4BytesWritten = -1;
		goto out;
	}
	priv_driver_set_chip_config(prNetDev, pcCommand, i4TotalLen);
out:
	return i4BytesWritten;
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
				snprintf(pcCommand, i4TotalLen,
					 CMD_SET_CHIP " mira 0");
			} else if (ucMode == MIRACAST_MODE_SOURCE) {
				prWfdCfgSettings->ucWfdEnable = 1;
				snprintf(pcCommand, i4TotalLen,
					 CMD_SET_CHIP " mira 1");
			} else if (ucMode == MIRACAST_MODE_SINK) {
				prWfdCfgSettings->ucWfdEnable = 2;
				snprintf(pcCommand, i4TotalLen,
					 CMD_SET_CHIP " mira 2");
			} else {
				prWfdCfgSettings->ucWfdEnable = 0;
				snprintf(pcCommand, i4TotalLen,
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
	uint8_t aucValue[WLAN_CFG_ARGV_MAX];
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
	uint8_t aucValue[WLAN_CFG_ARGV_MAX];
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
			"MAC[%d]=" MACSTR "\n",
			i++,
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
	uint8_t aucValue[WLAN_CFG_ARGV_MAX];
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

int
priv_set_ap(IN struct net_device *prNetDev,
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
		if (!access_ok(VERIFY_READ, prIwReqData->data.pointer,
			prIwReqData->data.length)) {
			DBGLOG(REQ, INFO,
				"%s access_ok Read fail written = %d\n",
				__func__, i4BytesWritten);
			return -EFAULT;
		}
		if (copy_from_user(pcExtra,
			prIwReqData->data.pointer,
			prIwReqData->data.length)) {
			DBGLOG(REQ, INFO,
				"%s copy_form_user fail written = %d\n",
				__func__,
				prIwReqData->data.length);
				return -EFAULT;
		}
		/* prIwReqData->data.length include the terminate '\0' */
		pcExtra[prIwReqData->data.length - 1] = 0;
	}

	if (!pcExtra)
		goto exit;

	DBGLOG(REQ, INFO, "%s pcExtra %s\n", __func__, pcExtra);

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
		pcExtra,
		i4TotalFixLen);
	  break;
	case IOC_AP_SET_CFG:
	i4BytesWritten =
		priv_driver_set_ap_set_cfg(
		prNetDev,
		pcExtra,
		i4TotalFixLen);
	  break;
	case IOC_AP_STA_DISASSOC:
	i4BytesWritten =
		priv_driver_set_ap_sta_disassoc(
		prNetDev,
		pcExtra,
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
		}
		if (i4BytesWritten >= i4TotalFixLen) {
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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

#if CFG_SUPPORT_ADJUST_MCC_MODE_SET
static int priv_driver_set_mcc_mode(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct ADAPTER *prAdapter = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = 0;
	/* MCC mode 0: fair (5:5), 1: favor STA (7:3), 2: favor P2P (3:7) */
	uint32_t ucMccMode = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = 0;
	uint8_t ucRole;
	uint32_t u4MccTimeToSet = 0;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prAdapter = prGlueInfo->prAdapter;
	if (i4Argc != 2)
		goto out;

	i4Ret = kalkStrtou32(apcArgv[1], 0, &ucMccMode);
	if (i4Ret) {
		DBGLOG(REQ, LOUD, "Parse ucMccMode error i4Ret=%d\n", i4Ret);
		goto out;
	}

	if (ucMccMode == MCC_FAIR) {
		prAdapter->ucModeMCC = MCC_FAIR;
		DBGLOG(REQ, TRACE,
			"Current dynamic MCC priority : Fair\n");
#if CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY
		prAdapter->ucModeMCC_STA_time = MCC_TIME_5;
		prAdapter->ucModeMCC_P2P_time = MCC_TIME_5;
#endif
	} else if (ucMccMode == MCC_FAV_STA) {
		prAdapter->ucModeMCC = MCC_FAV_STA;
		DBGLOG(REQ, TRACE,
		    "Current dynamic MCC priority : STA favor\n");
#if CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY
		prAdapter->ucModeMCC_STA_time = MCC_TIME_7;
		prAdapter->ucModeMCC_P2P_time = MCC_TIME_3;
#endif
	} else if (ucMccMode == MCC_FAV_P2P) {
		prAdapter->ucModeMCC = MCC_FAV_P2P;
		DBGLOG(REQ, TRACE,
		   "Current dynamic MCC priority : P2P Favor\n");
#if CFG_SUPPORT_RX_DYNAMIC_MCC_PRIORITY
		prAdapter->ucModeMCC_STA_time = MCC_TIME_3;
		prAdapter->ucModeMCC_P2P_time = MCC_TIME_7;
#endif
	} else {
		DBGLOG(REQ, LOUD, "Wrong MCC mode %i\n", ucMccMode);
		i4BytesWritten = -1;
		goto out;
	}

	/*0:CFG_MCC_AIS_QUOTA_TYPE,
	**1:CFG_MCC_P2PGC_QUOTA_TYPE, 2:CFG_MCC_P2PGO_QUOTA_TYPE
	*/
	for (ucRole = CFG_MCC_AIS_QUOTA_TYPE;
		ucRole <= CFG_MCC_P2PGO_QUOTA_TYPE; ucRole++) {
		if (ucRole == CFG_MCC_AIS_QUOTA_TYPE)
			u4MccTimeToSet = prAdapter->ucModeMCC_STA_time;
		else
			u4MccTimeToSet = prAdapter->ucModeMCC_P2P_time;

		snprintf(pcCommand, i4TotalLen,
			CMD_SET_CHIP " mccTime %d %d",
			ucRole, u4MccTimeToSet);
		priv_driver_set_chip_config(prNetDev, pcCommand,
			i4TotalLen);
	}

out:
	return i4BytesWritten;
}
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
	uint8_t	ucWakeupHif = 0, GpioPin = 0, ucGpioLevel = 0, ucBlockCount,
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
	uint8_t	ucVer, ucCount;
	uint16_t u2Port = 0;
	uint16_t *pausPortArry;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgumentLong(pcCommand, &i4Argc, apcPortArgv);
	DBGLOG(REQ, WARN, "argc is %i\n", i4Argc);

	/* example ipv4: set_wow_udp 0 5353,8080 (set) */
	/* example ipv4: set_wow_udp 0 (clear) */
	/* example ipv6: set_wow_udp 1 8000 (set) */
	/* example ipv6: set_wow_udp 1 (clear) */

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
	uint8_t	ucVer, ucCount;
	uint16_t u2Port = 0;
	uint16_t *pausPortArry;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgumentLong(pcCommand, &i4Argc, apcPortArgv);
	DBGLOG(REQ, WARN, "argc is %i\n", i4Argc);

	/* example ipv4: set_wow_tcp 0 5353,8080 (set) */
	/* example ipv4: set_wow_tcp 0 (clear) */
	/* example ipv6: set_wow_tcp 1 8000 (set) */
	/* example ipv6: set_wow_tcp 1 (clear) */

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
	uint8_t	ucVer, ucProto;
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

static int priv_driver_get_wow_reason(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int32_t i4BytesWritten = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct WOW_CTRL *pWOW_CTRL = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	pWOW_CTRL = &prGlueInfo->prAdapter->rWowCtrl;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (pWOW_CTRL->ucReason != INVALID_WOW_WAKE_UP_REASON)
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"\nwakeup_reason:%d", pWOW_CTRL->ucReason);

	return	i4BytesWritten;
}

static int priv_driver_set_suspend_cmd(IN struct net_device *prNetDev,
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

	wlanSetSuspendMode(prGlueInfo, Enable);

	return 0;
}

static int priv_driver_set_mdns_offload_enable(IN struct net_device *prNetDev,
		IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret = 0;
	uint8_t ucEnable = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	u4Ret = kalkStrtou8(apcArgv[1], 0, &ucEnable);

	if (u4Ret)
		DBGLOG(REQ, LOUD, "parse bEnable error u4Ret=%d\n", u4Ret);

	if (prGlueInfo->prAdapter->mdns_offload_enable == FALSE
				&& ucEnable != 0) {
		DBGLOG(PF, STATE, "Mdns offload enable.\n");
		prGlueInfo->prAdapter->mdns_offload_enable = TRUE;
		kalSendMdnsEnableToFw(prGlueInfo);
	} else if (prGlueInfo->prAdapter->mdns_offload_enable == TRUE
				&& ucEnable == 0) {
		DBGLOG(PF, STATE, "Mdns offload disable.\n");
		prGlueInfo->prAdapter->mdns_offload_enable = FALSE;
		kalInitMdnsCache();
		kalSendMdnsDisableToFw(prGlueInfo);
	} else
		DBGLOG(PF, STATE, "Mdns offload already %s\n",
			ucEnable ? "enable" : "disable");

	return 0;
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
	uint8_t ucMultiDtim = 0, ucVer;

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

static int priv_driver_get_deep_sleep_cnt(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rWlanStatus;
	int32_t i4BytesWritten = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = 0xb0100000;
	rWlanStatus = kalIoctl(prGlueInfo, wlanoidQuerySwCtrlRead,
				&rSwCtrlInfo, sizeof(rSwCtrlInfo),
				TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, STATE, "Deep sleep cnt=%d\n", rSwCtrlInfo.u4Data);
	if (rWlanStatus == WLAN_STATUS_SUCCESS) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n Deep sleep cnt: %d\n", rSwCtrlInfo.u4Data);
	}

	return i4BytesWritten;
}

static int priv_driver_enforce_power_mode(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rWlanStatus;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Ret = 0;
	uint16_t ucEnforcePowerMode;
	struct PARAM_POWER_MODE_ rPowerMode;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc != 2) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       "\n format:enforce_power_mode [0|1|2|3]\n");
		return i4BytesWritten;
	}

	i4Ret = kalkStrtou16(apcArgv[1], 0, &ucEnforcePowerMode);
	if (i4Ret)
		DBGLOG(REQ, ERROR, "parse enforce_power_mode error i4Ret=%d\n",
			i4Ret);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo)
		return -EFAULT;

	if (!prGlueInfo->prAdapter->prAisBssInfo)
		return -EFAULT;

	rPowerMode.ucBssIdx = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;
	rPowerMode.ePowerMode = (enum PARAM_POWER_MODE) ucEnforcePowerMode;
	prGlueInfo->prAdapter->rWifiVar.ucEnforcePSMode = rPowerMode.ePowerMode;

	/* Restore Android's power mode setting
	*  if we do not enforce power mode.
	*/
	if (rPowerMode.ePowerMode >= Param_PowerModeMax &&
		prGlueInfo->prAdapter->prAisBssInfo->ePowerModeFromUser
		< Param_PowerModeMax) {
		rPowerMode.ePowerMode =
			prGlueInfo->prAdapter->prAisBssInfo->ePowerModeFromUser;
	}

	rWlanStatus = kalIoctl(prGlueInfo,
				wlanoidSet802dot11PowerSaveProfile,
				&rPowerMode,
				sizeof(struct PARAM_POWER_MODE_),
				FALSE, FALSE, TRUE, &u4BufLen);

	if (rWlanStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rWlanStatus);
		return -1;
	}
	return i4BytesWritten;
}

static int priv_driver_get_power_mode(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	uint16_t ucEnforcePowerMode;
	uint16_t ePowerModeFromUser;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo)
		return -EFAULT;

	if (!prGlueInfo->prAdapter->prAisBssInfo)
		return -EFAULT;

	ucEnforcePowerMode =
		prGlueInfo->prAdapter->rWifiVar.ucEnforcePSMode;
	ePowerModeFromUser =
		prGlueInfo->prAdapter->prAisBssInfo->ePowerModeFromUser;
	if (ucEnforcePowerMode < Param_PowerModeMax) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"\n enforce power mode %d\n", ucEnforcePowerMode);
	} else {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"\n enforce power mode: disabled\n");
	}
	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
	       " system power mode: %d\n", ePowerModeFromUser);

	return i4BytesWritten;
}

#if CFG_CHIP_RESET_HANG
static int priv_driver_set_rst_hang(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
			glGetRstReason(RST_CMD_TRIGGER);
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

#if CFG_SUPPORT_ANT_DIV
static int priv_driver_ant_diversity_config(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t u4Ret;
	uint8_t fgRead = FALSE;
	uint8_t fgWaitResp = FALSE;
	struct CMD_ANT_DIV_CTRL rAntDivInfo;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -EFAULT;

	rAntDivInfo.ucAction = 0;
	rAntDivInfo.ucAntId = 0;
	rAntDivInfo.ucRcpi = 0;
	rAntDivInfo.ucState = 0;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc == 0) {
		DBGLOG(REQ, INFO, "ANT_DIV Argc = %d\n", i4Argc);
		return -EFAULT;
	}

	if (strnicmp(apcArgv[0],
		   CMD_GET_ANT_DIV,
		   strlen(CMD_GET_ANT_DIV)) == 0) {
		if (i4Argc < CMD_GET_ANT_DIV_ARG_NUM) {
			DBGLOG(REQ, INFO,
				"GET_ANT_DIV arg num=%d, must be %d\n",
				i4Argc, CMD_GET_ANT_DIV_ARG_NUM);
			return -EFAULT;
		}
		rAntDivInfo.ucAction = ANT_DIV_CMD_GET_ANT;
		fgRead = TRUE;
		fgWaitResp = TRUE;
	} else if (strnicmp(apcArgv[0],
		   CMD_DETC_ANT_DIV,
		   strlen(CMD_DETC_ANT_DIV)) == 0) {
		if (i4Argc < CMD_DETC_ANT_DIV_ARG_NUM) {
			DBGLOG(REQ, INFO,
				"DECT_ANT_DIV arg num=%d, must be %d\n",
				i4Argc, CMD_DETC_ANT_DIV_ARG_NUM);
			return -EFAULT;
		}

		rAntDivInfo.ucAction = ANT_DIV_CMD_DETC;
		fgRead = TRUE;
		fgWaitResp = TRUE;
	} else if (strnicmp(apcArgv[0],
		   CMD_SWH_ANT_DIV,
		   strlen(CMD_SWH_ANT_DIV)) == 0) {
		if (i4Argc < CMD_SWH_ANT_DIV_ARG_NUM) {
			DBGLOG(REQ, INFO,
				"SWH_ANT_DIV arg num=%d, must be %d\n",
				i4Argc, CMD_SWH_ANT_DIV_ARG_NUM);
			return -EFAULT;
		}

		rAntDivInfo.ucAction = ANT_DIV_CMD_SWH;
		fgRead = TRUE;
		fgWaitResp = TRUE;
	} else if (strnicmp(apcArgv[0],
			CMD_SET_ANT_DIV,
			strlen(CMD_SET_ANT_DIV)) == 0) {
		if (i4Argc < CMD_SET_ANT_DIV_ARG_NUM) {
			DBGLOG(REQ, INFO,
			       "SET_ANT_DIV arg num=%d, must be %d\n",
			       i4Argc, CMD_SET_ANT_DIV_ARG_NUM);
			return -EFAULT;
		}

		rAntDivInfo.ucAction = ANT_DIV_CMD_SET_ANT;
		fgRead = FALSE;
		fgWaitResp = FALSE;
		u4Ret = kalkStrtou8(apcArgv[1], 0, &(rAntDivInfo.ucAntId));
		if (u4Ret ||
		   ((rAntDivInfo.ucAntId != 1) &&
		   (rAntDivInfo.ucAntId != 2))) {
			DBGLOG(REQ, INFO,
				"Parse ANT Index error Ret=%d, AntId =%d\n",
				u4Ret, rAntDivInfo.ucAntId);
			return -EFAULT;
		}
	} else {
		DBGLOG(REQ, INFO, "ANT DIV subcmd(%s) error !\n", apcArgv[0]);
		return -EFAULT;
	}

	rStatus = kalIoctl(prGlueInfo,
			wlanoidAntDivCfg,
			&rAntDivInfo,
			sizeof(rAntDivInfo),
			fgRead,
			fgWaitResp,
			TRUE,
			&u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;


	if (fgWaitResp) {
		DBGLOG(REQ, LOUD,
			"[ANT-DIV] priv_driver_ant_diversity_config\n");
		DBGLOG(REQ, LOUD, "[ANT-DIV] action=%d\n",
			rAntDivInfo.ucAction);
		DBGLOG(REQ, LOUD, "[ANT-DIV] ucState=%d\n",
			rAntDivInfo.ucState);

		switch (rAntDivInfo.ucAction) {
		case ANT_DIV_CMD_GET_ANT:
			i4BytesWritten = scnprintf(pcCommand,
						i4TotalLen,
						"%d",
						rAntDivInfo.ucState);
		break;
		case ANT_DIV_CMD_DETC:
			if (rAntDivInfo.ucState == ANT_DIV_SUCCESS) {
				/* rcpi to rssi */
				DBGLOG(REQ, LOUD,
					"[ANT-DIV] ucRcpi=%d rssi=%d\n",
					rAntDivInfo.ucRcpi,
					RCPI_TO_dBm(rAntDivInfo.ucRcpi));
				i4BytesWritten = scnprintf(pcCommand,
							i4TotalLen,
							"%d",
					RCPI_TO_dBm(rAntDivInfo.ucRcpi));
			} else {
				i4BytesWritten = scnprintf(pcCommand,
							i4TotalLen,
							"%d",
							rAntDivInfo.ucState);
			}
		break;
		case ANT_DIV_CMD_SWH:
			i4BytesWritten = scnprintf(pcCommand,
							i4TotalLen,
							"%d",
							rAntDivInfo.ucState);
		break;
		default:
			DBGLOG(REQ, ERROR,
				"[ANT-DIV][WARN] ucAction=%d\n",
				rAntDivInfo.ucAction);
			i4BytesWritten = scnprintf(pcCommand,
						i4TotalLen, "fail");
		break;
		}

		DBGLOG(REQ, INFO, "%s: command result is %s\n",
					__func__, pcCommand);
	}

	return i4BytesWritten;
}
#endif



int priv_driver_set_suspend_mode(IN struct net_device *prNetDev,
				 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	u_int8_t fgEnable;
	uint32_t u4Enable;
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
			i4BytesWritten = snprintf(pcCommand, i4TotalLen,
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
			    snprintf(pcCommand, i4TotalLen,
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
		    snprintf(pcCommand, i4TotalLen, "set monitor config %s",
			     (rStatus == WLAN_STATUS_SUCCESS) ?
			     "success" : "fail");

		return i4BytesWritten;
	}

	i4BytesWritten = snprintf(pcCommand, i4TotalLen,
				  "monitor [Enable][PriChannel][ChannelWidth][Sco]");

	return i4BytesWritten;
}
#endif

static int priv_driver_get_sta_index(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter;
	int32_t i4BytesWritten = 0, i4Argc = 0;
	uint8_t ucStaIdx, ucWlanIndex;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
		"WiFi Driver Version %u.%u.%u\n",
		NIC_DRIVER_MAJOR_VERSION,
		NIC_DRIVER_MINOR_VERSION,
		NIC_DRIVER_SERIAL_VERSION);

	i4BytesWritten = (int32_t)u4Offset;

	return i4BytesWritten;
}

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
	uint32_t u4Ret, u4Parse;
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

static int priv_driver_get_cnm(IN struct net_device *prNetDev,
			       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	struct PARAM_GET_CNM_T *prCnmInfo = NULL;

	enum ENUM_DBDC_BN	eDbdcIdx, eDbdcIdxMax;
	uint8_t			ucBssIdx;
	struct BSS_INFO *prBssInfo;
	enum ENUM_CNM_NETWORK_TYPE_T eNetworkType;
	uint8_t ucNss;

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
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
				   i4TotalLen - i4BytesWritten,
				   "\n[CNM Info]\n");
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
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

		/* backward compatible for 7668 format */
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					   i4TotalLen - i4BytesWritten,
					   "BAND%u channels : %u %u %u\n",
					   eDbdcIdx,
					   prCnmInfo->ucChList[eDbdcIdx][0],
					   prCnmInfo->ucChList[eDbdcIdx][1],
					   prCnmInfo->ucChList[eDbdcIdx][2]);

		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					   i4TotalLen - i4BytesWritten,
					   "Band %u OPCH %d [%u, %u, %u]\n",
					   eDbdcIdx,
					   prCnmInfo->ucOpChNum[eDbdcIdx],
					   prCnmInfo->ucChList[eDbdcIdx][0],
					   prCnmInfo->ucChList[eDbdcIdx][1],
					   prCnmInfo->ucChList[eDbdcIdx][2]);

	}
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
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
		       PARAM_MEDIA_STATE_CONNECTED)))) {
			if (eNetworkType == ENUM_CNM_NETWORK_TYPE_P2P_GO) {
				struct STA_RECORD *prCurrStaRec =
						(struct STA_RECORD *) NULL;

				prCurrStaRec = LINK_PEEK_HEAD(
						&prBssInfo->rStaRecOfClientList,
						struct STA_RECORD, rLinkEntry);

				if (prCurrStaRec != NULL &&
				    IS_CONNECTION_NSS2(prBssInfo,
				    prCurrStaRec)) {
					ucNss = 2;
				} else
					ucNss = 1;
			} else if (prBssInfo->prStaRecOfAP != NULL &&
				   IS_CONNECTION_NSS2(prBssInfo,
				   prBssInfo->prStaRecOfAP)) {
				ucNss = 2;
			} else
				ucNss = 1;

		} else {
			eNetworkType = ENUM_CNM_NETWORK_TYPE_OTHER;
			ucNss = prBssInfo->ucNss;
			/* Do not show history information */
			/* if argc is 1 */
			if (i4Argc < 2)
				continue;
		}

		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"BSS%u Inuse%u Act%u ConnStat%u [NetType%u][CH%3u][DBDC b%u][WMM%u b%u][OMAC%u b%u][BW%3u][NSS%u]\n",
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
			ucNss);
	}

	kalMemFree(prCnmInfo, VIR_MEM_TYPE, sizeof(struct PARAM_GET_CNM_T));
	return i4BytesWritten;
}				/* priv_driver_get_sw_ctrl */

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
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
				u4Offset = kalSnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
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
#endif

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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t u4Ret;
	int32_t i4Parameter;
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
	ASSERT(prGlueInfo);
	if (prGlueInfo->prAdapter &&
	    prGlueInfo->prAdapter->chip_info &&
	    !prGlueInfo->prAdapter->chip_info->is_support_efuse) {
		u4Offset += snprintf(pcCommand + u4Offset,
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
			u4Offset += snprintf(pcCommand + u4Offset,
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
			u4Offset += snprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Write success 0x%X = 0x%X\n",
					     u4Efuse_addr, ucEfuse_value);
		}
	} else if (ucOpMode == EFUSE_FREE) {
		struct PARAM_CUSTOM_EFUSE_FREE_BLOCK rEfuseFreeBlock = {};

		if (prGlueInfo->prAdapter->fgIsSupportGetFreeEfuseBlockCount
		    == FALSE) {
			u4Offset += snprintf(pcCommand + u4Offset,
					     i4TotalLen - u4Offset,
					     "Cannot read free block size\n");
			return (int32_t)u4Offset;
		}
		rStatus = kalIoctl(prGlueInfo, wlanoidQueryEfuseFreeBlock,
				   &rEfuseFreeBlock,
				   sizeof(struct PARAM_CUSTOM_EFUSE_FREE_BLOCK),
				   TRUE, TRUE, TRUE, &u4BufLen);
		if (rStatus == WLAN_STATUS_SUCCESS) {
			u4Offset += snprintf(pcCommand + u4Offset,
				     i4TotalLen - u4Offset,
				     "Free block size 0x%X\n",
				     prGlueInfo->prAdapter->u4FreeBlockNum);
		}
	}
#else
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
					"efuse ops is invalid\n");
#endif

	return (int32_t)u4Offset;

efuse_op_invalid:

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\nHelp menu\n");
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tRead:\t\"efuse read addr_hex\"\n");
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"\tWrite:\t\"efuse write addr_hex val_hex\"\n");
	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
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
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
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

	if (!IS_SDIO_INF(prGlueInfo)) {
		u4Offset += snprintf(pcCommand + u4Offset,
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
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_NOISE_ID;
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

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
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
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_POP_ID;
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

static int priv_driver_get_traffic_report(IN struct net_device
			*prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	struct CMD_GET_TRAFFIC_REPORT *cmd = NULL;
	uint8_t ucBand = ENUM_BAND_0;
	uint16_t u2Val = 0;
	uint8_t ucVal = 0;
	int32_t u4Ret = 0;
	u_int8_t fgWaitResp = FALSE;
	u_int8_t fgRead = FALSE;
	u_int8_t fgGetDbg = FALSE;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (!prGlueInfo)
		goto get_report_invalid;

	cmd = (struct CMD_GET_TRAFFIC_REPORT *)
				kalMemAlloc(sizeof(*cmd), VIR_MEM_TYPE);
	if (!cmd)
		goto get_report_invalid;

	if ((i4Argc > 4) || (i4Argc < 2))
		goto get_report_invalid;

	memset(cmd, 0, sizeof(*cmd));

	cmd->u2Type = CMD_GET_REPORT_TYPE;
	cmd->u2Len = sizeof(*cmd);
	cmd->ucBand = ucBand;

	if (strnicmp(apcArgv[1], "ENABLE", strlen("ENABLE")) == 0) {
		prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap
				|= KEEP_FULL_PWR_TRAFFIC_REPORT_BIT;
		cmd->ucAction = CMD_GET_REPORT_ENABLE;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
	} else if (strnicmp(apcArgv[1], "DISABLE", strlen("DISABLE")) == 0) {
		prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap
				&= ~KEEP_FULL_PWR_TRAFFIC_REPORT_BIT;
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
		if ((i4Argc == 4) && (strnicmp(apcArgv[2],
					"BAND", strlen("BAND")) == 0)) {
			u4Ret = kalkStrtou8(apcArgv[3], 0, &ucVal);
			cmd->ucBand = ucVal;
		}
		if (strnicmp(apcArgv[1], "GETDBG", strlen("GETDBG")) == 0)
			fgGetDbg = TRUE;
	} else if ((strnicmp(apcArgv[1], "SAMPLEPOINTS",
			strlen("SAMPLEPOINTS")) == 0) && (i4Argc == 3)) {
		u4Ret = kalkStrtou16(apcArgv[2], 0, &u2Val);
		cmd->u2SamplePoints = u2Val;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
		cmd->ucAction = CMD_SET_REPORT_SAMPLE_POINT;
	} else if ((strnicmp(apcArgv[1], "TXTHRES",
				strlen("TXTHRES")) == 0) && (i4Argc == 3)) {
		u4Ret = kalkStrtou8(apcArgv[2], 0, &ucVal);
		/* valid val range is from 0 - 100% */
		if (ucVal > 100)
			ucVal = 100;
		cmd->ucTxThres = ucVal;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
		cmd->ucAction = CMD_SET_REPORT_TXTHRES;
	} else if ((strnicmp(apcArgv[1], "RXTHRES",
				strlen("RXTHRES")) == 0) && (i4Argc == 3)) {
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

	rStatus = kalIoctl(prGlueInfo, wlanoidAdvCtrl, cmd,
				sizeof(*cmd), TRUE, TRUE, TRUE, &u4BufLen);

	if ((rStatus != WLAN_STATUS_SUCCESS)
					&& (rStatus != WLAN_STATUS_PENDING))
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\ncommand failed %x", rStatus);
	else if (cmd->ucAction == CMD_GET_REPORT_GET) {
		int persentage = 0;
		int sample_dur = cmd->u4FetchEd - cmd->u4FetchSt;

		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\nCCK false detect cnt: %d"
				, (cmd->u4FalseCCA >> EVENT_REPORT_CCK_FCCA)
						& EVENT_REPORT_CCK_FCCA_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\nOFDM false detect cnt: %d"
				, (cmd->u4FalseCCA >> EVENT_REPORT_OFDM_FCCA)
					& EVENT_REPORT_OFDM_FCCA_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
				"\nCCK Sig CRC cnt: %d"
				, (cmd->u4HdrCRC >> EVENT_REPORT_CCK_SIGERR)
					& EVENT_REPORT_CCK_SIGERR_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\nOFDM Sig CRC cnt: %d"
				, (cmd->u4HdrCRC >> EVENT_REPORT_OFDM_SIGERR)
					& EVENT_REPORT_OFDM_SIGERR_FEILD);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\nBand%d Info:", cmd->ucBand);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\n\tSample every %u ms with %u points",
					cmd->u4TimerDur, cmd->u2SamplePoints);
		if (fgGetDbg) {
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
		 "from systime%u-%u total_dur %u us f_cost %u us t_drift %d ms"
				, cmd->u4FetchSt, cmd->u4FetchEd
				, sample_dur
				, cmd->u4FetchCost, cmd->TimerDrift);
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
			"\n\tbusy-RMAC %u us, idle-TMAC %u us, t_total %u"
					, cmd->u4ChBusy, cmd->u4ChIdle,
						cmd->u4ChBusy + cmd->u4ChIdle);
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
				"\n\theavy tx threshold %u%% rx threshold %u%%"
					, cmd->ucTxThres, cmd->ucRxThres);
		}
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
			"\n\tch_busy %u us, ch_idle %u us, total_period %u us"
				, sample_dur - cmd->u4ChIdle
				, cmd->u4ChIdle, sample_dur);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\n\tmy_tx_time: %u us"
				, cmd->u4TxAirTime);
		if (cmd->u4FetchEd - cmd->u4FetchSt) {
			persentage = cmd->u4TxAirTime / (sample_dur / 1000);
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				", tx utility: %d.%1d%%"
				, persentage / 10
				, persentage % 10);
		}
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\n\tmy_data_rx_time (no BMC data): %u us"
				, cmd->u4RxAirTime);
		if (cmd->u4FetchEd - cmd->u4FetchSt) {
			persentage = cmd->u4RxAirTime / (sample_dur / 1000);
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				", rx utility: %d.%1d%%"
				, persentage / 10
				, persentage % 10);
		}
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\n\tTotal packet transmitted: %u",
							cmd->u4PktSent);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\n\tTotal tx ok packet: %u",
					cmd->u4PktSent - cmd->u4PktTxfailed);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\n\tTotal tx failed packet: %u",
						cmd->u4PktTxfailed);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\n\tTotal tx retried packet: %u",
							cmd->u4PktRetried);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\n\tTotal rx mpdu: %u", cmd->u4RxMPDU);
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\n\tTotal rx fcs: %u", cmd->u4RxFcs);
	} else
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
				"\ncommand sent %x", rStatus);

	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));

	return i4BytesWritten;
get_report_invalid:
	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						i4TotalLen - i4BytesWritten,
			"\nformat:get_report [enable|disable|get|reset]");
	return i4BytesWritten;
}

static int priv_driver_get_pop(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_POP_ID;
	uint32_t u4Offset = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	int32_t u4CckTh = 0, u4OfdmTh = 0;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
				wlanoidQuerySwCtrlRead,
				&rSwCtrlInfo, sizeof(rSwCtrlInfo),
				TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4CckTh = rSwCtrlInfo.u4Data & 0xFF;
	u4OfdmTh = (rSwCtrlInfo.u4Data >> 8) & 0xFF;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"PoP: CckTh:%ddB OfdmTh:%ddB\n",
				u4CckTh, u4OfdmTh);

	i4BytesWritten = (int32_t)u4Offset;

	return i4BytesWritten;

}


static int priv_driver_set_ed(IN struct net_device *prNetDev,
			      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t i4Ret = 0;
	int32_t i4EdVal[2] = { 0 };
	uint32_t u4Sel = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return WLAN_STATUS_FAILURE;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(REQ, LOUD, "Adapter is NULL!\n");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc <= 3) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR! Parameters for SET_ED:\n",
			i4Argc);
		DBGLOG(REQ, ERROR,
			"<Sel> <2.4G ED(-49~-81dBm)> <5G ED(-49~-81dBm)>\n");

		return WLAN_STATUS_FAILURE;
	}

	i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);
	if (i4Ret)
		DBGLOG(REQ, ERROR, "parse  error i4Ret=%d\n", i4Ret);
	i4Ret = kalkStrtos32(apcArgv[2], 0, &i4EdVal[0]);
	if (i4Ret)
		DBGLOG(REQ, ERROR,
			"parse i4EdVal(2.4G) error i4Ret=%d\n", i4Ret);
	i4Ret = kalkStrtos32(apcArgv[3], 0, &i4EdVal[1]);
	if (i4Ret)
		DBGLOG(REQ, ERROR,
			"parse i4EdVal(5G) error u4Ret=%d\n", i4Ret);

	u4Status = wlanSetEd(prAdapter, i4EdVal[0], i4EdVal[1], u4Sel);

	if (u4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", u4Status);
		return WLAN_STATUS_FAILURE;
	}

	return i4BytesWritten;

}

static int priv_driver_get_ed(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_ED_ID;
	uint32_t u4Offset = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	int8_t iEdVal[2] = { 0 };

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return WLAN_STATUS_FAILURE;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	u4Status = kalIoctl(prGlueInfo,
			wlanoidQuerySwCtrlRead,
			&rSwCtrlInfo, sizeof(rSwCtrlInfo),
			TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "Status %u\n", u4Status);
	if (u4Status != WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;

	iEdVal[0] = rSwCtrlInfo.u4Data & 0xFF;
	iEdVal[1] = (rSwCtrlInfo.u4Data >> 16) & 0xFF;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
			"ED: 2.4G(%ddB), 5G(%ddB)\n", iEdVal[0], iEdVal[1]);

	i4BytesWritten = (int32_t)u4Offset;

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
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_PD_ID;
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

static int priv_driver_get_pd(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_PD_ID;
	uint32_t u4Offset = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	int8_t u4CckTh = 0, u4OfdmTh = 0;

	 ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
	return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
				wlanoidQuerySwCtrlRead,
				&rSwCtrlInfo, sizeof(rSwCtrlInfo),
				TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4CckTh = rSwCtrlInfo.u4Data & 0xFF;
	u4OfdmTh = (rSwCtrlInfo.u4Data >> 8) & 0xFF;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
			"PD: CckTh:%ddB OfdmTh:%ddB\n", u4CckTh, u4OfdmTh);

	i4BytesWritten = (int32_t)u4Offset;

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
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_MAX_RFGAIN_ID;
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

static int priv_driver_get_maxrfgain(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_MAX_RFGAIN_ID;
	uint32_t u4Offset = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint8_t u4Wf0Gain = 0, u4Wf1Gain = 0;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
				wlanoidQuerySwCtrlRead,
				&rSwCtrlInfo, sizeof(rSwCtrlInfo),
				TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4Wf0Gain = rSwCtrlInfo.u4Data & 0xFF;
	u4Wf1Gain = (rSwCtrlInfo.u4Data >> 8) & 0xFF;

	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
				"Max RFGain: WF0:%ddB WF1:%ddB\n",
				u4Wf0Gain, u4Wf1Gain);

	i4BytesWritten = (int32_t)u4Offset;
	return i4BytesWritten;

}

static int priv_driver_noise_histogram(IN struct net_device *prNetDev,
	IN char *pcCommand, IN int i4TotalLen)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo;
	int32_t  i4BytesWritten = 0;
	uint32_t u4BufLen = 0;
	int32_t  i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	struct CMD_NOISE_HISTOGRAM_REPORT *cmd = NULL;

	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo)
		goto noise_histogram_invalid;

	cmd = (struct CMD_NOISE_HISTOGRAM_REPORT *)kalMemAlloc(sizeof(*cmd),
		VIR_MEM_TYPE);
	if (!cmd)
		goto noise_histogram_invalid;

	if ((i4Argc > 4) || (i4Argc < 2))
		goto noise_histogram_invalid;

	memset(cmd, 0, sizeof(*cmd));

	cmd->u2Type = CMD_NOISE_HISTOGRAM_TYPE;
	cmd->u2Len = sizeof(*cmd);

	if (strnicmp(apcArgv[1], "ENABLE", strlen("ENABLE")) == 0) {
		prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap |=
			KEEP_FULL_PWR_NOISE_HISTOGRAM_BIT;
		cmd->ucAction = CMD_NOISE_HISTOGRAM_ENABLE;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
	} else if (strnicmp(apcArgv[1], "DISABLE", strlen("DISABLE")) == 0) {
		prGlueInfo->prAdapter->u4IsKeepFullPwrBitmap &=
			~KEEP_FULL_PWR_NOISE_HISTOGRAM_BIT;
		cmd->ucAction = CMD_NOISE_HISTOGRAM_DISABLE;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
	} else if (strnicmp(apcArgv[1], "RESET", strlen("RESET")) == 0) {
		cmd->ucAction = CMD_NOISE_HISTOGRAM_RESET;
		cmd->u2Type |= CMD_ADV_CONTROL_SET;
#if CFG_IPI_2CHAIN_SUPPORT
	} else if (strnicmp(apcArgv[1], "GET2", strlen("GET2")) == 0) {
		/*
		 * For getting backward compatibility
		 * to original format for default chain
		 */
		cmd->u2Type = CMD_NOISE_HISTOGRAM_TYPE2;
		cmd->ucAction = CMD_NOISE_HISTOGRAM_GET;
		u4Status = kalIoctl(prGlueInfo, wlanoidAdvCtrl,
				cmd, sizeof(*cmd), TRUE, TRUE, TRUE, &u4BufLen);
		if (u4Status == WLAN_STATUS_SUCCESS)
			parseNoiseHistogramReport(&i4BytesWritten,
				pcCommand, &i4TotalLen, cmd);

		cmd->ucAction = CMD_NOISE_HISTOGRAM_GET2;
#endif /* CFG_IPI_2CHAIN_SUPPORT */
	} else if (strnicmp(apcArgv[1], "GET", strlen("GET")) == 0) {
		cmd->ucAction = CMD_NOISE_HISTOGRAM_GET;
	} else
		goto noise_histogram_invalid;

	DBGLOG(REQ, LOUD, "%s(%s) action %x\n"
		, __func__, pcCommand, cmd->ucAction);

	u4Status = kalIoctl(prGlueInfo, wlanoidAdvCtrl, cmd, sizeof(*cmd),
		TRUE, TRUE, TRUE, &u4BufLen);

	if (u4Status == WLAN_STATUS_NOT_SUPPORTED) {
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\nThis fw version does not support priv command: %s\n",
			pcCommand);
	} else if ((u4Status != WLAN_STATUS_SUCCESS)
			&& (u4Status != WLAN_STATUS_PENDING)) {
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\ncommand failed %x", u4Status);
	} else if (cmd->ucAction == CMD_NOISE_HISTOGRAM_GET
#if CFG_IPI_2CHAIN_SUPPORT
			|| cmd->ucAction == CMD_NOISE_HISTOGRAM_GET2
#endif /* CFG_IPI_2CHAIN_SUPPORT */
		) {
		parseNoiseHistogramReport(&i4BytesWritten,
			pcCommand, &i4TotalLen, cmd);
	} else
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten, "\ncommand sent %x", u4Status);

	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));

	return i4BytesWritten;
noise_histogram_invalid:
	if (cmd)
		kalMemFree(cmd, VIR_MEM_TYPE, sizeof(*cmd));
	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\nformat:get_report [enable|disable|get|reset]");
	return i4BytesWritten;
}

#if CFG_RX_SINGLE_CHAIN_SUPPORT
static int priv_driver_set_rxchain(IN struct net_device *prNetDev,
			IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_RXC_ID;
	uint32_t u4Sel = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint32_t u4Offset = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 1) {
		u4Offset += snprintf(pcCommand + u4Offset,
			i4TotalLen - u4Offset,
			"SET RX CHAIN:Fail!(%d) SET_RXC<Sel:0/1/2>\n", i4Argc);
		goto out;
	}

	/* not connected */
	/* if (prGlueInfo->eParamMediaStateIndicated !=
		* PARAM_MEDIA_STATE_CONNECTED) {
			* u4Offset += snprintf(pcCommand + u4Offset,
				* i4TotalLen - u4Offset,
				* "SET RX CHAIN: Fail! not connected\n");
			* goto out;
	* }
	*/

	/* if (prAdapter->rWifiVar.ucNSS > 1 ||
		* prAdapter->rWifiVar.ucSpeIdxCtrl == 2) {
			* u4Offset += snprintf(pcCommand + u4Offset,
				* i4TotalLen - u4Offset,
				* "SET RX CHAIN: Fail!NSS =%d &
				* SpeIdxCtrl =%d wrong setting\n",
				* prAdapter->rWifiVar.ucNSS,
				* prAdapter->rWifiVar.ucSpeIdxCtrl);
				* goto out;
	 * }
	*/

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);
	if (u4Ret) {
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error u4Ret=%d\n", u4Ret);
		return -1;
	}
	if ((u4Sel < 0 || u4Sel > 2)  && (u4Sel != 5)) {
		u4Offset += snprintf(pcCommand + u4Offset,
			i4TotalLen - u4Offset,
			"SET RX CHAIN:Fail!(%d) not equal 0/1/2/5\n", u4Sel);
		goto out;
	}
	/* else if (u4Sel != prAdapter->rWifiVar.ucSpeIdxCtrl) {
		* u4Offset += snprintf(pcCommand + u4Offset,
			* i4TotalLen - u4Offset,
			* "SET RX CHAIN:Fail!(%d)
			* not equal to SpeIdxCtrl(%d)\n",
			* u4Sel, prAdapter->rWifiVar.ucSpeIdxCtrl);
		* goto out;
	* }
	*/

	rSwCtrlInfo.u4Data = u4Sel;
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		u4Offset += snprintf(pcCommand + u4Offset,
				i4TotalLen - u4Offset,
				"SET RX CHAIN: kalIoctl fail (%d)\n", rStatus);

		goto out;
	} else {
		u4Offset += snprintf(pcCommand + u4Offset,
		  i4TotalLen - u4Offset,
		  "SET RX CHAIN: %d (0: WF0 1:WF1 2:WF0+1 5:Disable)Success\n"
			, u4Sel);
	}
out:
	i4BytesWritten = (int32_t)u4Offset;

	return i4BytesWritten;

}

static int priv_driver_get_rxchain(IN struct net_device *prNetDev,
	IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_RXC_ID;
	uint32_t u4Offset = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint8_t ucRxChain;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	/*not connected*/
	/*if (prGlueInfo->eParamMediaStateIndicated !=
		* PARAM_MEDIA_STATE_CONNECTED) {
			* u4Offset += snprintf(pcCommand + u4Offset,
			* i4TotalLen - u4Offset,
			* "GET RX CHAIN: Fail! not yet connected\n");
			* goto out;
	* }
	*/


	/*if (prAdapter->rWifiVar.ucNSS > 1 ||
		* prAdapter->rWifiVar.ucSpeIdxCtrl == 2) {
			* u4Offset += snprintf(pcCommand + u4Offset,
			* i4TotalLen - u4Offset,
				* "GET RX CHAIN: Fail! NSS=%d (Need <2)
				* SpeIdxCtrl=%d (Need <2) \n",
				* prAdapter->rWifiVar.ucNSS,
				* prAdapter->rWifiVar.ucSpeIdxCtrl);
			* goto out;
	* }
	*/
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
			   TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	ucRxChain = (rSwCtrlInfo.u4Data) & 0xFF;


	u4Offset += snprintf(pcCommand + u4Offset, i4TotalLen - u4Offset,
			"GET RX CHAIN: %d (0: WF0 1:WF1 2:WF0+WF1)\n",
			ucRxChain);

  /*out:*/
	i4BytesWritten = (int32_t)u4Offset;

	return i4BytesWritten;

}
#endif
static int priv_driver_set_adm_ctrl(IN struct net_device *prNetDev,
	IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_ADM_CTRL_ID;
	uint32_t u4Enable = 0, u4TimeRatio = 100, u4AdmBase = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	rSwCtrlInfo.u4Id = u4Id;

	if ((i4Argc > 4) || (i4Argc < 2)) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: set_adm_ctrl\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Enable);
	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse u4Enable error u4Ret=%d\n", u4Ret);

	if (i4Argc > 2) {
		/* u4AdmTime default is 100% */
		u4Ret = kalkStrtos32(apcArgv[2], 0, &u4TimeRatio);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "u4AdmTime err u4Ret=%d\n", u4Ret);
	}

	if (i4Argc > 3) {
		u4Ret = kalkStrtos32(apcArgv[3], 0, &u4AdmBase);
		if (u4Ret)
			DBGLOG(REQ, ERROR, "u4AdmBase err u4Ret=%d\n", u4Ret);
	}

	rSwCtrlInfo.u4Data = ((u4AdmBase & 0xFFFF) |
			     ((u4TimeRatio & 0xFF) << 16) |
			     ((u4Enable & 0xFF) << 24));
	DBGLOG(REQ, LOUD,
		"u4Enable=%d u4AdmTime=%d, u4AdmBase=%d, u4Data=0x%x,\n",
		u4Enable, u4TimeRatio, u4AdmBase, rSwCtrlInfo.u4Data);

	rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite, &rSwCtrlInfo,
			   sizeof(rSwCtrlInfo), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

static int priv_driver_get_adm_ctrl(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_ADM_CTRL_ID;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint32_t u4Enable = 0, u4TimeRatio = 100, u4AdmBase = 0;

	 ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
	return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
				wlanoidQuerySwCtrlRead,
				&rSwCtrlInfo, sizeof(rSwCtrlInfo),
				TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	u4Enable = (rSwCtrlInfo.u4Data >> 24) & 0xFF;
	u4TimeRatio = (rSwCtrlInfo.u4Data >> 16) & 0xFF;
	u4AdmBase = rSwCtrlInfo.u4Data & 0xFFFF;

	i4BytesWritten += scnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tAdminCtrl: %s",
		(u4Enable) ?
		"Enabled" : "Disabled\n");

	if (u4Enable == 1)
		i4BytesWritten += scnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\nLimited %% from thermal: %u%%\n",
		u4TimeRatio);

	return i4BytesWritten;
}

#if CFG_ENABLE_1RPD_MMPS_CTRL
static int priv_driver_set_1rpd(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID +
		CMD_ADVCTL_1RPD;
	uint32_t u4Enable = FALSE;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_1RPD <Sel>\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Enable);

	if (u4Ret) {
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
				"\nCommand failed, invalid parameter");
		return i4BytesWritten;
	}

	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
			"\nSet 1RPD %d", u4Enable);

	rSwCtrlInfo.u4Data = u4Enable;
	u4Status = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
			   FALSE, FALSE, g_fgIsOid, &u4BufLen);

	if (u4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", u4Status);
		return -1;
	}

	return i4BytesWritten;
}

static int priv_driver_get_1rpd(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID +
		CMD_ADVCTL_1RPD;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint32_t u4Sel = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE,
			   TRUE, g_fgIsOid, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	u4Sel = rSwCtrlInfo.u4Data & 0xFF;

	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
				"\n1RPD status: %d", u4Sel);

	return i4BytesWritten;

}
static int priv_driver_set_mmps(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID +
		CMD_ADVCTL_MMPS;
	uint32_t u4Enable = FALSE;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_NOISE <Sel>\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Enable);

	if (u4Ret) {
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
				"\nCommand failed, invalid parameter");
		return i4BytesWritten;
	}

	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
			"\nSet MMPS %d", u4Enable);

	rSwCtrlInfo.u4Data = u4Enable;
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
			   FALSE, FALSE, g_fgIsOid, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

static int priv_driver_get_mmps(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID +
		CMD_ADVCTL_MMPS;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint32_t u4Sel = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE,
			   TRUE, g_fgIsOid, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	u4Sel = rSwCtrlInfo.u4Data  & 0xFF;

	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
				"\nMMPS status: %d", u4Sel);

	return i4BytesWritten;
}
#endif
#if CFG_ENABLE_DEWEIGHTING_CTRL
static int priv_driver_set_deweighting_th(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Ret = 0;
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID +
			CMD_ADVCTL_DEWEIGHTING_TH_ID;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint32_t u4Sel = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Id = u4Id;

	if (i4Argc <= 1) {
		DBGLOG(REQ, ERROR, "Argc(%d) ERR: SET_NOISE <Sel>\n", i4Argc);
		return -1;
	}

	u4Ret = kalkStrtou32(apcArgv[1], 0, &u4Sel);

	if (u4Ret) {
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
				"\nCommand failed, threshold range is 0 to 15");
		return i4BytesWritten;
	}

	if ((u4Sel < 0) || (u4Sel > 15)) {
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
				"\nCommand failed, threshold range is 0 to 15");
		return i4BytesWritten;
	}

	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
			"\nSetting noise difference threshold %d", u4Sel);

	rSwCtrlInfo.u4Data = u4Sel << 16;
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

static int priv_driver_get_deweighting_th(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID +
			CMD_ADVCTL_DEWEIGHTING_TH_ID;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint32_t u4Sel = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo), TRUE,
			   TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	u4Sel = rSwCtrlInfo.u4Data & 0xFF;

	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
				"\nNoise difference threshold is %d", u4Sel);

	return i4BytesWritten;

}

static int priv_driver_get_deweighting_noise(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID +
			CMD_ADVCTL_DEWEIGHTING_NOISE_ID;
	uint8_t u2Wf0AvgNoise = 0, u2Wf1AvgNoise = 0;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	u2Wf1AvgNoise = rSwCtrlInfo.u4Data & 0xFF;
	u2Wf0AvgNoise = (rSwCtrlInfo.u4Data >> 16) & 0xFF;

	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
				"\nDeweighting Average noise, WF0 = %d, WF1 = %d",
				u2Wf0AvgNoise, u2Wf1AvgNoise);

	return i4BytesWritten;
}

static int priv_driver_get_deweighting_weight(
	IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID +
			CMD_ADVCTL_DEWEIGHTING_WEIGHT_ID;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	uint8_t ucWF0OfdmSetpt, ucWF0CckSetpt, ucWF1OfdmSetpt, ucWF1CckSetpt;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	ucWF1CckSetpt = rSwCtrlInfo.u4Data & 0xFF;
	ucWF1OfdmSetpt = (rSwCtrlInfo.u4Data >> 8) & 0xFF;
	ucWF0CckSetpt = (rSwCtrlInfo.u4Data >> 16) & 0xFF;
	ucWF0OfdmSetpt = (rSwCtrlInfo.u4Data >> 24) & 0xFF;

	i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
				"\nDeweighting weight, WF0 ofdm=%02X cck=%02X, WF1 ofdm=%02X cck=%02X",
				ucWF0OfdmSetpt, ucWF0CckSetpt,
				ucWF1OfdmSetpt, ucWF1CckSetpt);

	return i4BytesWritten;
}
#endif /* CFG_ENABLE_DEWEIGHTING_CTRL */


static int priv_driver_set_bcn_th(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_BCN_TH_ID;
	int32_t i4Ret = 0;
	int8_t ucP2P, ucInfra, ucWithbt;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc != 4) {
		i4BytesWritten += scnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\nformat:set_bcn_th <p2p> <infra> <withbt>");
		return i4BytesWritten;
	}

	i4Ret = kalkStrtou8(apcArgv[1], 0, &ucP2P);
	if (i4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error i4Ret=%d\n", i4Ret);
	i4Ret = kalkStrtou8(apcArgv[2], 0, &ucInfra);
	if (i4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error i4Ret=%d\n", i4Ret);
	i4Ret = kalkStrtou8(apcArgv[3], 0, &ucWithbt);
	if (i4Ret)
		DBGLOG(REQ, ERROR, "parse rSwCtrlInfo error i4Ret=%d\n", i4Ret);

	rSwCtrlInfo.u4Data = (ucP2P | (ucInfra << 8) | (ucWithbt << 16));
	rSwCtrlInfo.u4Id   = u4Id;

	DBGLOG(REQ, LOUD, "ucP2P=%d ucInfra=%d, ucWithbt=%d u4Data=0x%x,\n",
		ucP2P, ucInfra, ucWithbt, rSwCtrlInfo.u4Data);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetSwCtrlWrite,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
			   FALSE, FALSE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	return i4BytesWritten;
}


static int priv_driver_get_bcn_th(IN struct net_device *prNetDev,
	IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID + CMD_ADVCTL_BCN_TH_ID;
	int8_t ucAdhoc, ucInfra, ucWithbt;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc != 1) {
		i4BytesWritten += scnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\nformat:get_bcn_th");
		return i4BytesWritten;
	}

	rSwCtrlInfo.u4Data = 0;
	rSwCtrlInfo.u4Id   = u4Id;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySwCtrlRead,
			   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
			   TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	ucAdhoc  = rSwCtrlInfo.u4Data & 0xFF;
	ucInfra  = (rSwCtrlInfo.u4Data >> 8) & 0xFF;
	ucWithbt = (rSwCtrlInfo.u4Data >> 16) & 0xFF;

	i4BytesWritten += scnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\nBeacon loss threshold: P2P:%d, INFRA:%d, With BT:%d\n",
				ucAdhoc, ucInfra, ucWithbt);

	return i4BytesWritten;
}

static int priv_driver_get_bcntimeout_cnt(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t u4Id = CMD_SW_DBGCTL_ADVCTL_GET_ID +
					CMD_ADVCTL_BCNTIMOUT_NUM_ID;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	 ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
	return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (!prGlueInfo->prAdapter->prAisBssInfo)
		return -EFAULT;
	rSwCtrlInfo.u4Data = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;
	rSwCtrlInfo.u4Id = u4Id;

	rStatus = kalIoctl(prGlueInfo,
				wlanoidQuerySwCtrlRead,
				&rSwCtrlInfo, sizeof(rSwCtrlInfo),
				TRUE, TRUE, TRUE, &u4BufLen);

	DBGLOG(REQ, LOUD, "rStatus %u\n", rStatus);
	if (rStatus != WLAN_STATUS_SUCCESS)
		return -1;

	i4BytesWritten += scnprintf(pcCommand + i4BytesWritten,
		i4TotalLen - i4BytesWritten,
		"\tbeacon timeout count: %d",
		rSwCtrlInfo.u4Data);

	return i4BytesWritten;
}


#endif /* CFG_SUPPORT_ADVANCE_CONTROL */

#if CFG_SUPPORT_GET_MCS_INFO
static int32_t priv_driver_last_sec_mcs_info(struct ADAPTER *prAdapter,
			IN char *pcCommand, IN int i4TotalLen,
			struct PARAM_TX_MCS_INFO *prTxMcsInfo)
{
	int32_t i4BytesWritten = 0;
	uint8_t ucStaIdx = 0, i, j, ucCnt = 0, ucPerSum = 0;
	uint16_t u2RateCode;
	struct STA_RECORD *prStaRec = NULL;
	uint32_t au4RxV0[MCS_INFO_SAMPLE_CNT], au4RxV1[MCS_INFO_SAMPLE_CNT];
	uint32_t txmode, rate, frmode, sgi, nsts, ldpc, stbc, groupid, mu, bw;
	uint32_t u4RxV0, u4RxV1;


	if (prAdapter->prAisBssInfo->prStaRecOfAP != NULL) {
		ucStaIdx = prAdapter->prAisBssInfo->prStaRecOfAP->ucIndex;
		prStaRec = cnmGetStaRecByIndex(prAdapter, ucStaIdx);
	}

	if (prStaRec != NULL && prStaRec->fgIsValid && prStaRec->fgIsInUse) {
		kalMemCopy(au4RxV0, prStaRec->au4RxV0, sizeof(au4RxV0));
		kalMemCopy(au4RxV1, prStaRec->au4RxV1, sizeof(au4RxV1));
	} else {
		i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"Not Connect to AP\n");
		return i4BytesWritten;
	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "\nTx MCS:\n");

	for (i = 0; i < MCS_INFO_SAMPLE_CNT; i++) {
		if (prTxMcsInfo->au2TxRateCode[i] == 0xFFFF)
			continue;

		txmode = HW_TX_RATE_TO_MODE(prTxMcsInfo->au2TxRateCode[i]);
		if (txmode >= MAX_TX_MODE)
			txmode = MAX_TX_MODE;

		rate = HW_TX_RATE_TO_MCS(prTxMcsInfo->au2TxRateCode[i], txmode);
		nsts = HW_TX_RATE_TO_NSS(prTxMcsInfo->au2TxRateCode[i]) + 1;
		stbc = HW_TX_RATE_TO_STBC(prTxMcsInfo->au2TxRateCode[i]);
		bw = GET_TX_MCS_BW(prTxMcsInfo->au2TxRateCode[i]);
		sgi = GET_TX_MCS_SGI(prTxMcsInfo->au2TxRateCode[i]);
		ldpc = GET_TX_MCS_LDPC(prTxMcsInfo->au2TxRateCode[i]);

		ucCnt = 0;
		ucPerSum = 0;
		u2RateCode = prTxMcsInfo->au2TxRateCode[i];
		for (j = 0; j < MCS_INFO_SAMPLE_CNT; j++) {
			if (u2RateCode == prTxMcsInfo->au2TxRateCode[j]) {
				ucPerSum += prTxMcsInfo->aucTxRatePer[j];
				ucCnt++;
				prTxMcsInfo->au2TxRateCode[j] = 0xFFFF;
			}
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", HW_TX_RATE_CCK_STR[rate & 0x3]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, ", hw_rate_ofdm_str(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) ||
			 (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"NSS%d_MCS%d, ", nsts, rate);
#if 0
		if ((txmode == TX_RATE_MODE_CCK) ||
		    (txmode == TX_RATE_MODE_OFDM))
			i4BytesWritten += kalScnprintf(
			    pcCommand + i4BytesWritten,
			    i4TotalLen - i4BytesWritten,
			    "%s, ", HW_TX_RATE_BW[0]);
		else if (i > prHwWlanInfo->rWtblPeerCap.ucChangeBWAfterRateN)
			i4BytesWritten += kalScnprintf(
			    pcCommand + i4BytesWritten,
			    i4TotalLen - i4BytesWritten, "%s, ",
			    prHwWlanInfo->rWtblPeerCap.
				ucFrequencyCapability < 4 ?
			    (prHwWlanInfo->rWtblPeerCap.
				ucFrequencyCapability > BW_20 ?
				HW_TX_RATE_BW[prHwWlanInfo->
				    rWtblPeerCap
				    .ucFrequencyCapability - 1] :
				HW_TX_RATE_BW[prHwWlanInfo->rWtblPeerCap
				    .ucFrequencyCapability]) :
			    HW_TX_RATE_BW[4]);
		else
			i4BytesWritten += kalScnprintf(
			    pcCommand + i4BytesWritten,
			    i4TotalLen - i4BytesWritten,
			    "%s, ",
			    prHwWlanInfo->rWtblPeerCap.
				ucFrequencyCapability < 4 ?
			    HW_TX_RATE_BW[
				prHwWlanInfo->rWtblPeerCap
				.ucFrequencyCapability] :
			    HW_TX_RATE_BW[4]);
#endif
		i4BytesWritten += kalScnprintf(
		    pcCommand + i4BytesWritten,
		    i4TotalLen - i4BytesWritten,
		    "%s, ", HW_TX_RATE_BW[bw]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
					    pcCommand + i4BytesWritten,
					    i4TotalLen - i4BytesWritten,
					    "%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(
					    pcCommand + i4BytesWritten,
					    i4TotalLen - i4BytesWritten,
					    "%s, ", sgi == 0 ?
						"LGI" : "SGI");

		i4BytesWritten += kalScnprintf(
			pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten,
			"%s%s%s [PER: %02d%%]\t", txmode < 5 ?
			HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			stbc ? ", STBC, " : ", ",
			((ldpc == 0) ||
			(txmode == TX_RATE_MODE_CCK) ||
			(txmode == TX_RATE_MODE_OFDM)) ?
			"BCC" : "LDPC", ucPerSum/ucCnt);

		for (j = 0; j < ucCnt; j++)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "*");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "\n");

	}

	i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "\nRx MCS:\n");

	for (i = 0; i < MCS_INFO_SAMPLE_CNT; i++) {
		if (au4RxV0[i] == 0xFFFFFFFF)
			continue;

		u4RxV0 = au4RxV0[i];
		u4RxV1 = au4RxV1[i];

		txmode = (u4RxV0 & RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET;
		rate = (u4RxV0 & RX_VT_RX_RATE_MASK) >> RX_VT_RX_RATE_OFFSET;
		frmode = (u4RxV0 & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET;
		nsts = ((u4RxV1 & RX_VT_NSTS_MASK) >> RX_VT_NSTS_OFFSET);
		stbc = (u4RxV0 & RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET;
		sgi = u4RxV0 & RX_VT_SHORT_GI;
		ldpc = u4RxV0 & RX_VT_LDPC;
		groupid = (u4RxV1 & RX_VT_GROUP_ID_MASK)
			   >> RX_VT_GROUP_ID_OFFSET;

		if (groupid && groupid != 63) {
			mu = 1;
		} else {
			mu = 0;
			nsts += 1;
		}

		/* Distribution Calculation clear the same sample content */
		ucCnt = 0;
		for (j = 0; j < MCS_INFO_SAMPLE_CNT; j++) {
			if ((u4RxV0 & RX_MCS_INFO_MASK) ==
			    (au4RxV0[j] & RX_MCS_INFO_MASK)) {
				au4RxV0[j] = 0xFFFFFFFF;
				ucCnt++;
			}
		}

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				rate < 4 ? HW_TX_RATE_CCK_STR[rate] :
					HW_TX_RATE_CCK_STR[4]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				hw_rate_ofdm_str(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) ||
			 (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "NSS%d_MCS%d, ",
				nsts, rate);

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten, "%s, ",
			frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, ",
				sgi == 0 ? "LGI" : "SGI");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "%s",
					stbc == 0 ? "" : "STBC, ");

		if (mu) {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"%s, %s, %s (%d)\n",
				txmode < 5 ?
				HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
				ldpc == 0 ? "BCC" : "LDPC", "MU", groupid);
		} else {
			i4BytesWritten += kalScnprintf(
				pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten, "%s, %s\n",
				txmode < 5 ?
				HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
				ldpc == 0 ? "BCC" : "LDPC");
		}

		for (j = 0; j < ucCnt; j++)
			i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "*");

		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten, "\n");
	}


	return i4BytesWritten;
}

static int32_t priv_driver_get_mcs_info(IN struct net_device *prNetDev,
	IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	int32_t i4BytesWritten = 0, i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct PARAM_TX_MCS_INFO *prTxMcsInfo = NULL;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (prAdapter->rRxMcsInfoTimer.pfMgmtTimeOutFunc == NULL) {
	cnmTimerInitTimer(prAdapter,
		&prAdapter->rRxMcsInfoTimer,
		(PFN_MGMT_TIMEOUT_FUNC) wlanRxMcsInfoMonitor,
		(unsigned long) NULL);
	}

	if (i4Argc >= 2) {
		if (strnicmp(apcArgv[1], "START", strlen("START")) == 0) {
			cnmTimerStartTimer(prAdapter,
			&prAdapter->rRxMcsInfoTimer, MCS_INFO_SAMPLE_PERIOD);
			prAdapter->fgIsMcsInfoValid = TRUE;

			i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"\nStart the MCS Info Function\n");
			return i4BytesWritten;
		} else if (strnicmp(apcArgv[1], "STOP", strlen("STOP")) == 0) {
			cnmTimerStopTimer(prAdapter,
				&prAdapter->rRxMcsInfoTimer);
			prAdapter->fgIsMcsInfoValid = FALSE;

			i4BytesWritten += kalScnprintf(
					pcCommand + i4BytesWritten,
				i4TotalLen - i4BytesWritten,
				"\nStop the MCS Info Function\n");
			return i4BytesWritten;
		}
	}

	if (prGlueInfo->prAdapter->fgIsMcsInfoValid != TRUE) {
		i4BytesWritten += kalScnprintf(pcCommand + i4BytesWritten,
			i4TotalLen - i4BytesWritten,
			"\nWARNING: Use GET_MCS_INFO [START/STOP]\n");
		return i4BytesWritten;
	}


	if (rStatus != WLAN_STATUS_SUCCESS)
		goto out;

	prTxMcsInfo = (struct PARAM_TX_MCS_INFO *)kalMemAlloc(
			sizeof(struct PARAM_TX_MCS_INFO), VIR_MEM_TYPE);
	if (!prTxMcsInfo) {
		DBGLOG(REQ, ERROR, "Allocate prTxMcsInfo failed!\n");
		i4BytesWritten = -1;
		goto out;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidTxQueryMcsInfo, prTxMcsInfo,
			   sizeof(struct PARAM_TX_MCS_INFO),
			   TRUE, TRUE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		goto out;

	i4BytesWritten = priv_driver_last_sec_mcs_info(prGlueInfo->prAdapter,
			pcCommand, i4TotalLen, prTxMcsInfo);

	DBGLOG(REQ, INFO, "%s: command result is %s\n", __func__, pcCommand);

out:

	if (prTxMcsInfo)
		kalMemFree(prTxMcsInfo, VIR_MEM_TYPE,
			sizeof(struct PARAM_TX_MCS_INFO));

	return i4BytesWritten;
}
#endif /* CFG_SUPPORT_GET_MCS_INFO */

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

	i4BytesWritten += snprintf(pcCommand, i4TotalLen,
				   "trigger driver SER\n");
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}

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

	i4BytesWritten += snprintf(pcCommand, i4TotalLen,
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

	i4BytesWritten += snprintf(pcCommand, i4TotalLen,
				   "Set Sw Amsdu Max Size:%u\n", u4Size);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\n", rStatus);
		return -1;
	}

	return i4BytesWritten;

}

#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
static int priv_driver_bss_transition_query(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	uint8_t *pucQueryReason = NULL;

	if (strnicmp(pcCommand, "BSS-TRANSITION-QUERY", 20) == 0) {
		if (strnicmp(pcCommand+20, " reason=", 8) == 0) {
			pucQueryReason = pcCommand + 28;
			DBGLOG(REQ, INFO,
				"BSS-TRANSITION-QUERY, pucQueryReason=%s\r\n",
				pucQueryReason);
		}
	}
	if ((pucQueryReason != NULL) && (strlen(pucQueryReason) > 3)) {
		DBGLOG(REQ, ERROR, "ERR: BTM query wrong reason!\r\n");
		return -EFAULT;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo)
		return -EFAULT;

	if (!prGlueInfo->prAdapter->prAisBssInfo)
		return -EFAULT;

	rStatus = kalIoctl(prGlueInfo,
				wlanoidSendBTMQuery,
				(void *)pucQueryReason, strlen(pucQueryReason),
				FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\r\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}
#endif
#if CFG_SUPPORT_802_11K
static int priv_driver_neighbor_request(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = 0;
	uint8_t *pucSSID = NULL;

	if (strnicmp(pcCommand, "NEIGHBOR-REQUEST", 16) == 0) {
		if (strnicmp(pcCommand+16, " SSID=", 6) == 0) {
			pucSSID = pcCommand + 22;
			DBGLOG(REQ, INFO,
				"NEIGHBOR-REQUEST, ssid=%s\r\n", pucSSID);
		}
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	if (!prGlueInfo)
		return -EFAULT;

	if (!prGlueInfo->prAdapter->prAisBssInfo)
		return -EFAULT;

	if (pucSSID == NULL)
		rStatus = kalIoctl(prGlueInfo,
				wlanoidSendNeighborRequest,
				NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);
	else
		rStatus = kalIoctl(prGlueInfo,
				wlanoidSendNeighborRequest,
				pucSSID, kalStrLen(pucSSID),
				FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "ERR: kalIoctl fail (%d)\r\n", rStatus);
		return -1;
	}

	return i4BytesWritten;
}
#endif

int priv_driver_get_bsstable(IN struct net_device *prNetDev, IN char *pcCommand,
	IN int i4TotalLen)
{
	int32_t i4BytesWritten = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct SCAN_INFO *prScanInfo;
	struct LINK *prBSSDescList;
	struct BSS_DESC *prBssDesc;
	struct ADAPTER *prAdapter = NULL;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
		"\nBSSID, rssi0, rssi1\n------\n");

	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry,
			struct BSS_DESC) {
		LOGBUF(pcCommand, i4TotalLen, i4BytesWritten,
			"%02x:%02x:%02x:%02x:%02x:%02x, %d, %d\n",
			prBssDesc->aucBSSID[0], prBssDesc->aucBSSID[1],
			prBssDesc->aucBSSID[2], prBssDesc->aucBSSID[3],
			prBssDesc->aucBSSID[4], prBssDesc->aucBSSID[5],
			RCPI_TO_dBm(prBssDesc->ucRCPI0),
			RCPI_TO_dBm(prBssDesc->ucRCPI1));
	}

	return i4BytesWritten;
}

#ifdef CFG_SUPPORT_TIME_MEASURE
int priv_driver_start_ftm(IN struct net_device *prNetDev,
			IN char *pcCommand, IN int i4TotalLen)
{
	struct PARAM_TM_T *prTmParam = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t u4Ret = 0;
	int8_t *pucMac;
	uint32_t u4BufLen;
	uint8_t fgWaitResp = TRUE;

	if (prNetDev == NULL)
		return -EFAULT;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prTmParam =
		(struct PARAM_TM_T *)kalMemAlloc(sizeof(struct PARAM_TM_T),
								VIR_MEM_TYPE);
	if (!prTmParam) {
		DBGLOG(REQ, ERROR,
				"Can not alloc memory for prTmParam\n");
		return -ENOMEM;
	}
	kalMemZero(prTmParam, sizeof(struct PARAM_TM_T));

	if (i4Argc >= 5) {
		pucMac = apcArgv[1];
		pucMac[2] = '\0';
		u4Ret = kalkStrtou8(pucMac, 16, &prTmParam->aucRttPeerAddr[0]);
		pucMac += 3;
		pucMac[2] = '\0';
		u4Ret = kalkStrtou8(pucMac, 16, &prTmParam->aucRttPeerAddr[1]);
		pucMac += 3;
		pucMac[2] = '\0';
		u4Ret = kalkStrtou8(pucMac, 16, &prTmParam->aucRttPeerAddr[2]);
		pucMac += 3;
		pucMac[2] = '\0';
		u4Ret = kalkStrtou8(pucMac, 16, &prTmParam->aucRttPeerAddr[3]);
		pucMac += 3;
		pucMac[2] = '\0';
		u4Ret = kalkStrtou8(pucMac, 16, &prTmParam->aucRttPeerAddr[4]);
		pucMac += 3;
		pucMac[2] = '\0';
		u4Ret = kalkStrtou8(pucMac, 16, &prTmParam->aucRttPeerAddr[5]);

		u4Ret = kalkStrtou8(apcArgv[2], 0, &prTmParam->ucFTMBandwidth);
		u4Ret = kalkStrtou8(apcArgv[3], 0, &prTmParam->ucFTMNum);
		u4Ret = kalkStrtou8(apcArgv[4], 0,
						&prTmParam->ucMinDeltaIn100US);

		prTmParam->u4DistanceCm = 0;
		prTmParam->u2NumOfValidResult = 0;
		prTmParam->u8DistStdevSq = 0;

		DBGLOG(REQ, INFO,
		"TMR PeerMac["MACSTR"] BW[%d] ucFTMNum[%d] ucMinDeltaFtm[%d]\n",
				MAC2STR(prTmParam->aucRttPeerAddr),
				prTmParam->ucFTMBandwidth,
				prTmParam->ucFTMNum,
				prTmParam->ucMinDeltaIn100US);

		if (strnicmp(apcArgv[0], CMD_START_FTM_NON_BLOCK,
					strlen(CMD_START_FTM_NON_BLOCK)) == 0) {
			fgWaitResp = FALSE;
			prTmParam->u4DistanceCm = 1;
		} else
			fgWaitResp = TRUE;

		u4Ret = kalIoctl(prGlueInfo, wlanoidQueryStartFtm, prTmParam,
					sizeof(struct PARAM_TM_T), fgWaitResp,
					fgWaitResp, TRUE, &u4BufLen);
	}

	if (prTmParam) {
		i4BytesWritten = snprintf(pcCommand, i4TotalLen,
		"["MACSTR"] Success = %d, Distance = %d (cm), STDEV = %lld",
					MAC2STR(prTmParam->aucRttPeerAddr),
					prTmParam->u2NumOfValidResult,
					prTmParam->u4DistanceCm,
					prTmParam->u8DistStdevSq);

	}
	kalMemFree(prTmParam, VIR_MEM_TYPE, sizeof(struct PARAM_TM_T));
	return i4BytesWritten;
}

int priv_driver_get_ftm(IN struct net_device *prNetDev,
			IN char *pcCommand, IN int i4TotalLen)
{
	struct PARAM_TM_T *prTmParam = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t u4Ret = 0;
	uint32_t u4BufLen;
	struct timespec Ftmtv_raw;
	uint64_t u8SysClkns;

	if (prNetDev == NULL)
		return -EFAULT;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prTmParam = (struct PARAM_TM_T *)kalMemAlloc(
				sizeof(struct PARAM_TM_T), VIR_MEM_TYPE);
	if (!prTmParam) {
		DBGLOG(REQ, ERROR, "Can not alloc memory for prTmParam\n");
		return -ENOMEM;
	}
	kalMemZero(prTmParam, sizeof(struct PARAM_TM_T));
	if (strnicmp(pcCommand, CMD_GET_TMR_AUDIOSYNC,
			strlen(CMD_GET_TMR_AUDIOSYNC)) == 0) {
		prTmParam->ucTmCategory = TM_ACTION_TMSYNC_QUERY;
	} else if (strnicmp(pcCommand, CMD_GET_TMR_DISTANCE,
			strlen(CMD_GET_TMR_DISTANCE)) == 0) {
		prTmParam->ucTmCategory = TM_ACTION_DISTANCE_QUERY;
	}
	u4Ret = kalIoctl(prGlueInfo, wlanoidQueryFtm, prTmParam,
						sizeof(struct PARAM_TM_T),
						TRUE, TRUE, TRUE, &u4BufLen);

	if (prTmParam) {
		if (prTmParam->ucTmCategory == TM_ACTION_TMSYNC_QUERY) {
			i4BytesWritten = snprintf(pcCommand, i4TotalLen,
					"["MACSTR"] Success = %d\n\n",
					MAC2STR(prTmParam->aucRttPeerAddr),
					prTmParam->u2NumOfValidResult);

			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"Clk Value  = %lld (ps)\n",
					prTmParam->u8Tsf);

			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"Clk Offset = %lld (ps)\n",
					prTmParam->i8ClockOffset);

			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"CODPer10ms = %lld (ps)\n",
					prTmParam->i8ClkRateDiffRatioIn10ms);

			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"Last ToA   = %lld (ps)\n",
					prTmParam->u8LastToA);

			getrawmonotonic(&Ftmtv_raw);
			u8SysClkns = (uint64_t)(Ftmtv_raw.tv_sec * 1000000000LL
							+ Ftmtv_raw.tv_nsec);

			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"Cur SysClk  = %lld (ns)\n",
					u8SysClkns);

			u8SysClkns = nicFtmRAWASCtrl(u8SysClkns);
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					i4TotalLen - i4BytesWritten,
					"Cur GlbCnt  = %lld (ps)\n",
					u8SysClkns);

		} else if (prTmParam->ucTmCategory ==
						TM_ACTION_DISTANCE_QUERY) {
			i4BytesWritten = snprintf(pcCommand, i4TotalLen,
		"["MACSTR"] Success = %d, Distance = %d (cm), STDEV = %lld",
					MAC2STR(prTmParam->aucRttPeerAddr),
					prTmParam->u2NumOfValidResult,
					prTmParam->u4DistanceCm,
					prTmParam->u8DistStdevSq);
		}
	}
	kalMemFree(prTmParam, VIR_MEM_TYPE, sizeof(struct PARAM_TM_T));
	return i4BytesWritten;
}

int priv_driver_enable_tmr(IN struct net_device *prNetDev,
	IN char *pcCommand,
	IN int i4TotalLen)
{
	struct PARAM_TM_T *prTmParam = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t u4Ret = 0;
	uint32_t u4BufLen;

	if (prNetDev == NULL)
		return -EFAULT;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prTmParam = (struct PARAM_TM_T *)kalMemAlloc(
				sizeof(struct PARAM_TM_T), VIR_MEM_TYPE);
	if (!prTmParam) {
		DBGLOG(REQ, ERROR,
				"Can not alloc memory for prTmParam\n");
		return -ENOMEM;
	}
	kalMemZero(prTmParam, sizeof(struct PARAM_TM_T));

	if (i4Argc >= 2) {
		u4Ret = kalkStrtou8(apcArgv[1], 0, &prTmParam->fgFtmEnable);

		u4Ret = kalIoctl(prGlueInfo, wlanoidSetEnableTmr, prTmParam,
						sizeof(struct PARAM_TM_T),
						FALSE, FALSE, TRUE, &u4BufLen);
	}
	kalMemFree(prTmParam, VIR_MEM_TYPE, sizeof(struct PARAM_TM_T));
	return i4BytesWritten;

}
#endif

#if CFG_SUPPORT_WAC
static int priv_driver_set_wac_ie_enable(IN struct net_device *prNetDev,
				IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	int32_t u4Ret = 0;
	int32_t Enable = -1;
	uint8_t ucRoleIdx = 0, ucBssIdx = 0;

	ASSERT(prNetDev);

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	u4Ret = kalkStrtos32(apcArgv[1], 0, &Enable);

	if (u4Ret)
		DBGLOG(REQ, ERROR, "parse bEnable error u4Ret=%d\n", u4Ret);

	if (Enable == 0) {
		DBGLOG(REQ, INFO, "disable WAC IE!\n");
		prAdapter->rWifiVar.fgEnableWACIE = FALSE;
	} else if (Enable == 1) {
		DBGLOG(REQ, INFO, "enable WAC IE!\n");
		prAdapter->rWifiVar.fgEnableWACIE = TRUE;
	} else {
		DBGLOG(REQ, INFO, "invalid WAC IE enable flag!\n");
		return -1;
	}

	/* get Bss Index from ndev */
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, prNetDev, &ucRoleIdx) != 0) {
		DBGLOG(REQ, ERROR, "Failed to get role index!\n");
		return -1;
	}
	if (p2pFuncRoleToBssIdx(prAdapter, ucRoleIdx, &ucBssIdx) !=
		WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Failed to get Bss index!\n");
		return -1;
	}

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];
	bssUpdateBeaconContent(prAdapter, prBssInfo->ucBssIndex);

	return 0;
}
#endif

int32_t priv_driver_cmds(IN struct net_device *prNetDev, IN int8_t *pcCommand,
			 IN int32_t i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = 0;

	if (g_u4HaltFlag) {
		DBGLOG(REQ, WARN, "wlan is halt, skip priv_driver_cmds\n");
		return -1;
	}

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

		if (strnicmp(pcCommand, CMD_RSSI, strlen(CMD_RSSI)) == 0) {
			/* i4BytesWritten =
			 *  wl_android_get_rssi(net, command, i4TotalLen);
			 */
		} else if (strnicmp(pcCommand, CMD_AP_START,
			   strlen(CMD_AP_START)) == 0) {
			i4BytesWritten = priv_driver_set_ap_start(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_LINKSPEED,
			   strlen(CMD_LINKSPEED)) == 0) {
			i4BytesWritten = priv_driver_get_linkspeed(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_PNOSSIDCLR_SET,
			   strlen(CMD_PNOSSIDCLR_SET)) == 0) {
			/* ToDo:: Nothing */
		} else if (strnicmp(pcCommand, CMD_PNOSETUP_SET,
			   strlen(CMD_PNOSETUP_SET)) == 0) {
			/* ToDo:: Nothing */
		} else if (strnicmp(pcCommand, CMD_PNOENABLE_SET,
			   strlen(CMD_PNOENABLE_SET)) == 0) {
			/* ToDo:: Nothing */
		} else if (strnicmp(pcCommand, CMD_SETSUSPENDOPT,
			   strlen(CMD_SETSUSPENDOPT)) == 0) {
			/* i4BytesWritten = wl_android_set_suspendopt(net,
			 *				pcCommand, i4TotalLen);
			 */
		} else if (strnicmp(pcCommand, CMD_SETSUSPENDMODE,
			   strlen(CMD_SETSUSPENDMODE)) == 0) {
			i4BytesWritten = priv_driver_set_suspend_mode(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SETBAND,
			   strlen(CMD_SETBAND)) == 0) {
			i4BytesWritten = priv_driver_set_band(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GETBAND,
			   strlen(CMD_GETBAND)) == 0) {
			/* i4BytesWritten = wl_android_get_band(net, pcCommand,
			 *				i4TotalLen);
			 */
		} else if (strnicmp(pcCommand, CMD_SET_TXPOWER,
			   strlen(CMD_SET_TXPOWER)) == 0) {
			i4BytesWritten = priv_driver_set_txpower(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_COUNTRY,
			   strlen(CMD_COUNTRY)) == 0) {
			i4BytesWritten = priv_driver_set_country(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_CSA,
				strlen(CMD_CSA)) == 0) {
			i4BytesWritten = priv_driver_set_csa(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_COUNTRY,
			   strlen(CMD_GET_COUNTRY)) == 0) {
			i4BytesWritten = priv_driver_get_country(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_CHANNELS,
			   strlen(CMD_GET_CHANNELS)) == 0) {
			i4BytesWritten = priv_driver_get_channels(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_AP_CHANNELS,
			   strlen(CMD_GET_AP_CHANNELS)) == 0) {
			i4BytesWritten = priv_driver_get_ap_channels(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_MIRACAST,
			   strlen(CMD_MIRACAST)) == 0) {
			i4BytesWritten = priv_driver_set_miracast(prNetDev,
							pcCommand, i4TotalLen);
		}
		/* Mediatek private command */
		else if (strnicmp(pcCommand, CMD_SET_SW_CTRL,
			 strlen(CMD_SET_SW_CTRL)) == 0) {
			i4BytesWritten = priv_driver_set_sw_ctrl(prNetDev,
							pcCommand, i4TotalLen);
#if (CFG_SUPPORT_RA_GEN == 1)
		} else if (strnicmp(pcCommand, CMD_SET_FIXED_FALLBACK,
			   strlen(CMD_SET_FIXED_FALLBACK)) == 0) {
			i4BytesWritten = priv_driver_set_fixed_fallback(
							prNetDev, pcCommand,
							i4TotalLen);
		} else if (strnicmp(pcCommand,  CMD_SET_RA_DBG,
			   strlen(CMD_SET_RA_DBG)) == 0) {
			i4BytesWritten = priv_driver_set_ra_debug_proc(prNetDev,
							pcCommand, i4TotalLen);
#endif
#if (CFG_SUPPORT_TXPOWER_INFO == 1)
		} else if (strnicmp(pcCommand,  CMD_GET_TX_POWER_INFO,
			   strlen(CMD_GET_TX_POWER_INFO)) == 0) {
			i4BytesWritten = priv_driver_get_txpower_info(prNetDev,
							pcCommand, i4TotalLen);
#endif
		} else if (strnicmp(pcCommand, CMD_SET_FIXED_RATE,
			   strlen(CMD_SET_FIXED_RATE)) == 0) {
			i4BytesWritten = priv_driver_set_fixed_rate(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_IPI,
			   strlen(CMD_GET_IPI)) == 0) {
			i4BytesWritten = priv_driver_get_ipi(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_SW_CTRL,
			   strlen(CMD_GET_SW_CTRL)) == 0) {
			i4BytesWritten = priv_driver_get_sw_ctrl(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_MCR,
			   strlen(CMD_SET_MCR)) == 0) {
			i4BytesWritten = priv_driver_set_mcr(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_MCR,
			   strlen(CMD_GET_MCR)) == 0) {
			i4BytesWritten = priv_driver_get_mcr(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_DRV_MCR,
			   strlen(CMD_SET_DRV_MCR)) == 0) {
			i4BytesWritten = priv_driver_set_drv_mcr(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_DRV_MCR,
			   strlen(CMD_GET_DRV_MCR)) == 0) {
			i4BytesWritten = priv_driver_get_drv_mcr(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_TEST_MODE,
			   strlen(CMD_SET_TEST_MODE)) == 0) {
			i4BytesWritten = priv_driver_set_test_mode(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_TEST_CMD,
			   strlen(CMD_SET_TEST_CMD)) == 0) {
			i4BytesWritten = priv_driver_set_test_cmd(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_TEST_RESULT,
			   strlen(CMD_GET_TEST_RESULT)) == 0) {
			i4BytesWritten = priv_driver_get_test_result(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_STA_STAT2,
			   strlen(CMD_GET_STA_STAT2)) == 0) {
			i4BytesWritten = priv_driver_get_sta_stat2(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_STA_STAT,
			   strlen(CMD_GET_STA_STAT)) == 0) {
			i4BytesWritten = priv_driver_get_sta_stat(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_STA_RX_STAT,
			   strlen(CMD_GET_STA_RX_STAT)) == 0) {
			i4BytesWritten = priv_driver_show_rx_stat(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_ACL_POLICY,
			   strlen(CMD_SET_ACL_POLICY)) == 0) {
			i4BytesWritten = priv_driver_set_acl_policy(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_ADD_ACL_ENTRY,
			   strlen(CMD_ADD_ACL_ENTRY)) == 0) {
			i4BytesWritten = priv_driver_add_acl_entry(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_DEL_ACL_ENTRY,
			   strlen(CMD_DEL_ACL_ENTRY)) == 0) {
			i4BytesWritten = priv_driver_del_acl_entry(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SHOW_ACL_ENTRY,
			   strlen(CMD_SHOW_ACL_ENTRY)) == 0) {
			i4BytesWritten = priv_driver_show_acl_entry(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_CLEAR_ACL_ENTRY,
			   strlen(CMD_CLEAR_ACL_ENTRY)) == 0) {
			i4BytesWritten = priv_driver_clear_acl_entry(prNetDev,
							pcCommand, i4TotalLen);
		}
#if (CFG_SUPPORT_DFS_MASTER == 1)
		else if (strnicmp(pcCommand, CMD_SHOW_DFS_STATE,
			 strlen(CMD_SHOW_DFS_STATE)) == 0) {
			i4BytesWritten = priv_driver_show_dfs_state(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SHOW_DFS_RADAR_PARAM,
			   strlen(CMD_SHOW_DFS_RADAR_PARAM)) == 0) {
			i4BytesWritten = priv_driver_show_dfs_radar_param(
							prNetDev, pcCommand,
							i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SHOW_DFS_HELP,
			   strlen(CMD_SHOW_DFS_HELP)) == 0) {
			i4BytesWritten = priv_driver_show_dfs_help(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SHOW_DFS_CAC_TIME,
			   strlen(CMD_SHOW_DFS_CAC_TIME)) == 0) {
			i4BytesWritten = priv_driver_show_dfs_cac_time(prNetDev,
							pcCommand, i4TotalLen);
		}
#endif
#if CFG_SUPPORT_CAL_RESULT_BACKUP_TO_HOST
		else if (strnicmp(pcCommand,
			 CMD_SET_CALBACKUP_TEST_DRV_FW,
			 strlen(CMD_SET_CALBACKUP_TEST_DRV_FW)) == 0)
			i4BytesWritten = priv_driver_set_calbackup_test_drv_fw(
							prNetDev, pcCommand,
							i4TotalLen);
#endif
#if CFG_WOW_SUPPORT
		else if (strnicmp(pcCommand, CMD_WOW_START,
			 strlen(CMD_WOW_START)) == 0)
			i4BytesWritten = priv_driver_set_wow(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_WOW_ENABLE,
			 strlen(CMD_SET_WOW_ENABLE)) == 0)
			i4BytesWritten = priv_driver_set_wow_enable(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_WOW_PAR,
			 strlen(CMD_SET_WOW_PAR)) == 0)
			i4BytesWritten = priv_driver_set_wow_par(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_WOW_UDP,
			 strlen(CMD_SET_WOW_UDP)) == 0)
			i4BytesWritten = priv_driver_set_wow_udpport(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_WOW_TCP,
			 strlen(CMD_SET_WOW_TCP)) == 0)
			i4BytesWritten = priv_driver_set_wow_tcpport(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_WOW_PORT,
			 strlen(CMD_GET_WOW_PORT)) == 0)
			i4BytesWritten = priv_driver_get_wow_port(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_WOW_REASON,
			 strlen(CMD_GET_WOW_PORT)) == 0)
			i4BytesWritten = priv_driver_get_wow_reason(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_SUSP_CMD,
			 strlen(CMD_SET_SUSP_CMD)) == 0)
			i4BytesWritten = priv_driver_set_suspend_cmd(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_MDNS_OFFLOAD_ENABLE,
				strlen(CMD_SET_MDNS_OFFLOAD_ENABLE)) == 0)
			i4BytesWritten = priv_driver_set_mdns_offload_enable(
					prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_SHOW_CACHE,
				strlen(CMD_SET_SHOW_CACHE)) == 0)
			kalShowMdnsCache();

#endif
		else if (strnicmp(pcCommand, CMD_SET_ADV_PWS,
			 strlen(CMD_SET_ADV_PWS)) == 0)
			i4BytesWritten = priv_driver_set_adv_pws(
							prNetDev, pcCommand,
							i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_MDTIM,
			 strlen(CMD_SET_MDTIM)) == 0)
			i4BytesWritten = priv_driver_set_mdtim(
							prNetDev, pcCommand,
							i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_DSLP_CNT,
			 strlen(CMD_GET_DSLP_CNT)) == 0)
			i4BytesWritten = priv_driver_get_deep_sleep_cnt(
							prNetDev, pcCommand,
							i4TotalLen);
		else if (strnicmp(pcCommand,
			CMD_ENFORCE_POWER_MODE,
			strlen(CMD_ENFORCE_POWER_MODE)) == 0)
			i4BytesWritten =
				priv_driver_enforce_power_mode(
					prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_POWER_MODE,
			strlen(CMD_GET_POWER_MODE)) == 0)
			i4BytesWritten =
				priv_driver_get_power_mode(
					prNetDev, pcCommand, i4TotalLen);
#ifdef CFG_SUPPORT_ADJUST_MCC_STAY_TIME
		else if (strnicmp(pcCommand, CMD_MCCTIME,
			strlen(CMD_MCCTIME)) == 0)
			i4BytesWritten = priv_driver_set_mcc_time(prNetDev,
						pcCommand, i4TotalLen);
#endif

#if CFG_SUPPORT_ADJUST_MCC_MODE_SET
		else if (strnicmp(pcCommand, CMD_SET_MCC_MODE,
				strlen(CMD_SET_MCC_MODE)) == 0)
			i4BytesWritten = priv_driver_set_mcc_mode(prNetDev,
						pcCommand, i4TotalLen);
#endif

#if CFG_SUPPORT_ANT_DIV
		else if (strnicmp(pcCommand, CMD_GET_ANT_DIV,
				strlen(CMD_GET_ANT_DIV)) == 0)
			i4BytesWritten = priv_driver_ant_diversity_config(
					prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_DETC_ANT_DIV,
				strlen(CMD_DETC_ANT_DIV)) == 0)
			i4BytesWritten = priv_driver_ant_diversity_config(
					prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SWH_ANT_DIV,
				strlen(CMD_SWH_ANT_DIV)) == 0)
			i4BytesWritten = priv_driver_ant_diversity_config(
					prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_ANT_DIV,
				strlen(CMD_SET_ANT_DIV)) == 0)
			i4BytesWritten = priv_driver_ant_diversity_config(
					prNetDev, pcCommand, i4TotalLen);
#endif

#if CFG_CHIP_RESET_HANG
		else if (strnicmp(pcCommand, CMD_SET_RST_HANG,
				strlen(CMD_SET_RST_HANG)) == 0)
			i4BytesWritten = priv_driver_set_rst_hang(
				prNetDev, pcCommand, i4TotalLen);
#endif

#if CFG_SUPPORT_QA_TOOL
		else if (strnicmp(pcCommand, CMD_GET_RX_STATISTICS,
			 strlen(CMD_GET_RX_STATISTICS)) == 0)
			i4BytesWritten = priv_driver_get_rx_statistics(
							prNetDev, pcCommand,
							i4TotalLen);
#if CFG_SUPPORT_BUFFER_MODE
		else if (strnicmp(pcCommand, CMD_SETBUFMODE,
			 strlen(CMD_SETBUFMODE)) == 0)
			i4BytesWritten = priv_driver_set_efuse_buffer_mode(
							prNetDev, pcCommand,
							i4TotalLen);
#endif
#endif
#if CFG_SUPPORT_MSP
#if 0
		else if (strnicmp(pcCommand, CMD_GET_STAT,
			 strlen(CMD_GET_STAT)) == 0)
			i4BytesWritten = priv_driver_get_stat(prNetDev,
							pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_GET_STA_STATISTICS,
			 strlen(CMD_GET_STA_STATISTICS)) == 0)
			i4BytesWritten = priv_driver_get_sta_statistics(
							prNetDev, pcCommand,
							i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_BSS_STATISTICS,
			 strlen(CMD_GET_BSS_STATISTICS)) == 0)
			i4BytesWritten = priv_driver_get_bss_statistics(
							prNetDev, pcCommand,
							i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_STA_IDX,
			 strlen(CMD_GET_STA_IDX)) == 0)
			i4BytesWritten = priv_driver_get_sta_index(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_STA_INFO,
			 strlen(CMD_GET_STA_INFO)) == 0)
			i4BytesWritten = priv_driver_get_sta_info(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_WTBL_INFO,
			 strlen(CMD_GET_WTBL_INFO)) == 0)
			i4BytesWritten = priv_driver_get_wtbl_info(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_MIB_INFO,
			 strlen(CMD_GET_MIB_INFO)) == 0)
			i4BytesWritten = priv_driver_get_mib_info(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_FW_LOG,
			 strlen(CMD_SET_FW_LOG)) == 0)
			i4BytesWritten = priv_driver_set_fw_log(prNetDev,
							pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_SET_CFG,
			 strlen(CMD_SET_CFG)) == 0) {
			i4BytesWritten = priv_driver_set_cfg(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_CFG,
			   strlen(CMD_GET_CFG)) == 0) {
			i4BytesWritten = priv_driver_get_cfg(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_COEX_CONTROL,
			strlen(CMD_COEX_CONTROL)) == 0) {
			i4BytesWritten = priv_driver_coex_ctrl(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_SET_CHIP,
			   strlen(CMD_SET_CHIP)) == 0) {
			i4BytesWritten = priv_driver_set_chip_config(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_CHIP,
			   strlen(CMD_GET_CHIP)) == 0) {
			i4BytesWritten = priv_driver_get_chip_config(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_VERSION,
			   strlen(CMD_GET_VERSION)) == 0) {
			i4BytesWritten = priv_driver_get_version(prNetDev,
							pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_CNM,
			   strlen(CMD_GET_CNM)) == 0) {
			i4BytesWritten = priv_driver_get_cnm(prNetDev,
							pcCommand, i4TotalLen);
#if CFG_SUPPORT_DBDC

		} else if (strnicmp(pcCommand, CMD_SET_DBDC,
			   strlen(CMD_SET_DBDC)) == 0) {
			i4BytesWritten = priv_driver_set_dbdc(prNetDev,
							pcCommand, i4TotalLen);
#endif /*CFG_SUPPORT_DBDC*/
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
#if CFG_SUPPORT_SNIFFER
		else if (strnicmp(pcCommand, CMD_SETMONITOR,
			 strlen(CMD_SETMONITOR)) == 0)
			i4BytesWritten = priv_driver_set_monitor(prNetDev,
							pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_GET_QUE_INFO,
			 strlen(CMD_GET_QUE_INFO)) == 0)
			i4BytesWritten = priv_driver_get_que_info(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_MEM_INFO,
			 strlen(CMD_GET_MEM_INFO)) == 0)
			i4BytesWritten = priv_driver_get_mem_info(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_HIF_INFO,
			 strlen(CMD_GET_HIF_INFO)) == 0)
			i4BytesWritten = priv_driver_get_hif_info(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_TP_INFO,
			 strlen(CMD_GET_TP_INFO)) == 0)
			i4BytesWritten = priv_driver_get_tp_info(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_CNM,
			 strlen(CMD_GET_CNM)) == 0)
			i4BytesWritten = priv_driver_get_cnm(prNetDev,
							pcCommand, i4TotalLen);
#if CFG_AUTO_CHANNEL_SEL_SUPPORT
		else if (strnicmp(pcCommand, CMD_GET_CH_RANK_LIST,
			 strlen(CMD_GET_CH_RANK_LIST)) == 0)
			i4BytesWritten = priv_driver_get_ch_rank_list(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_CH_DIRTINESS,
			 strlen(CMD_GET_CH_DIRTINESS)) == 0)
			i4BytesWritten = priv_driver_get_ch_dirtiness(prNetDev,
							pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_EFUSE,
			 sizeof(CMD_EFUSE)-1) == 0)
			i4BytesWritten = priv_driver_efuse_ops(prNetDev,
							pcCommand, i4TotalLen);
#if defined(_HIF_SDIO) && (MTK_WCN_HIF_SDIO == 0)
		else if (strnicmp(pcCommand, CMD_CCCR,
			 strlen(CMD_CCCR)) == 0)
			i4BytesWritten = priv_driver_cccr_ops(prNetDev,
							pcCommand, i4TotalLen);
#endif /* _HIF_SDIO && (MTK_WCN_HIF_SDIO == 0) */
#if CFG_SUPPORT_ADVANCE_CONTROL
		else if (strnicmp(pcCommand, CMD_SET_NOISE,
			 strlen(CMD_SET_NOISE)) == 0)
			i4BytesWritten = priv_driver_set_noise(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_NOISE,
			 strlen(CMD_GET_NOISE)) == 0)
			i4BytesWritten = priv_driver_get_noise(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_TRAFFIC_REPORT,
			strlen(CMD_TRAFFIC_REPORT)) == 0)
			i4BytesWritten = priv_driver_get_traffic_report(
					prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_POP,
			 strlen(CMD_SET_POP)) == 0)
			i4BytesWritten = priv_driver_set_pop(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_POP,
			 strlen(CMD_GET_POP)) == 0)
			i4BytesWritten = priv_driver_get_pop(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_ED,
			 strlen(CMD_SET_ED)) == 0)
			i4BytesWritten = priv_driver_set_ed(prNetDev, pcCommand,
							i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_ED,
			 strlen(CMD_GET_ED)) == 0)
			i4BytesWritten = priv_driver_get_ed(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_PD,
			 strlen(CMD_SET_PD)) == 0)
			i4BytesWritten = priv_driver_set_pd(prNetDev, pcCommand,
							i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_PD,
			 strlen(CMD_GET_PD)) == 0)
			i4BytesWritten = priv_driver_get_pd(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_MAX_RFGAIN,
			 strlen(CMD_SET_MAX_RFGAIN)) == 0)
			i4BytesWritten = priv_driver_set_maxrfgain(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_MAX_RFGAIN,
			 strlen(CMD_GET_MAX_RFGAIN)) == 0)
			i4BytesWritten = priv_driver_get_maxrfgain(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_NOISE_HISTOGRAM,
			strlen(CMD_NOISE_HISTOGRAM)) == 0)
			i4BytesWritten = priv_driver_noise_histogram(prNetDev,
							pcCommand, i4TotalLen);
#if CFG_RX_SINGLE_CHAIN_SUPPORT
		else if (strnicmp(pcCommand, CMD_SET_RXC,
					strlen(CMD_SET_RXC)) == 0)
			i4BytesWritten = priv_driver_set_rxchain(prNetDev,
					pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_RXC,
					strlen(CMD_GET_RXC)) == 0)
			i4BytesWritten = priv_driver_get_rxchain(prNetDev,
					pcCommand, i4TotalLen);
#endif
		else if (strnicmp(pcCommand, CMD_SET_ADM_CTRL,
					strlen(CMD_SET_ADM_CTRL)) == 0)
			i4BytesWritten = priv_driver_set_adm_ctrl(prNetDev,
					pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_ADM_CTRL,
					strlen(CMD_GET_ADM_CTRL)) == 0)
			i4BytesWritten = priv_driver_get_adm_ctrl(prNetDev,
					pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_BCN_TH,
					strlen(CMD_SET_BCN_TH)) == 0)
			i4BytesWritten = priv_driver_set_bcn_th(prNetDev,
					pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_BCN_TH,
					strlen(CMD_GET_BCN_TH)) == 0)
			i4BytesWritten = priv_driver_get_bcn_th(prNetDev,
					pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_BCNTIMEOUT_NUM,
					strlen(CMD_GET_BCNTIMEOUT_NUM)) == 0)
			i4BytesWritten = priv_driver_get_bcntimeout_cnt(
					prNetDev, pcCommand, i4TotalLen);
#if CFG_ENABLE_1RPD_MMPS_CTRL
		else if (strnicmp(pcCommand, CMD_SET_1RPD,
					strlen(CMD_SET_1RPD)) == 0)
			i4BytesWritten = priv_driver_set_1rpd(prNetDev,
					pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_1RPD,
					strlen(CMD_GET_1RPD)) == 0)
			i4BytesWritten = priv_driver_get_1rpd(prNetDev,
					pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_MMPS,
					strlen(CMD_SET_MMPS)) == 0)
			i4BytesWritten = priv_driver_set_mmps(prNetDev,
					pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_MMPS,
					strlen(CMD_GET_MMPS)) == 0)
			i4BytesWritten = priv_driver_get_mmps(prNetDev,
					pcCommand, i4TotalLen);
#endif /* CFG_ENABLE_1RPD_MMPS_CTRL */
#if CFG_ENABLE_DEWEIGHTING_CTRL
		else if (strnicmp(pcCommand, CMD_SET_DEWEIGHTING_TH,
					strlen(CMD_SET_DEWEIGHTING_TH)) == 0)
			i4BytesWritten = priv_driver_set_deweighting_th(
					prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_DEWEIGHTING_TH,
					strlen(CMD_GET_DEWEIGHTING_TH)) == 0)
			i4BytesWritten = priv_driver_get_deweighting_th(
					prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_DEWEIGHTING_NOISE,
					strlen(CMD_GET_DEWEIGHTING_NOISE)) == 0)
			i4BytesWritten = priv_driver_get_deweighting_noise(
					prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_GET_DEWEIGHTING_WEIGHT,
					strlen(CMD_GET_DEWEIGHTING_WEIGHT))
					== 0)
			i4BytesWritten = priv_driver_get_deweighting_weight(
					prNetDev, pcCommand, i4TotalLen);
#endif /* CFG_ENABLE_DEWEIGHTING_CTRL */


#endif /* CFG_SUPPORT_ADVANCE_CONTROL */
#if CFG_SUPPORT_GET_MCS_INFO
		else if (strnicmp(pcCommand, CMD_GET_MCS_INFO,
					strlen(CMD_GET_MCS_INFO)) == 0)
			i4BytesWritten = priv_driver_get_mcs_info(prNetDev,
			pcCommand, i4TotalLen);
#endif
#if CFG_ENABLE_WIFI_DIRECT
		else if (strnicmp(pcCommand, CMD_P2P_SET_PS,
			 strlen(CMD_P2P_SET_PS)) == 0)
			i4BytesWritten = priv_driver_set_p2p_ps(prNetDev,
							pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_P2P_SET_NOA,
			 strlen(CMD_P2P_SET_NOA)) == 0)
			i4BytesWritten = priv_driver_set_p2p_noa(prNetDev,
							pcCommand, i4TotalLen);
#endif /* CFG_ENABLE_WIFI_DIRECT */
		else if (strnicmp(pcCommand, CMD_SET_DRV_SER,
				strlen(CMD_SET_DRV_SER)) == 0)
			i4BytesWritten = priv_driver_set_drv_ser(
				prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_SW_AMSDU_NUM,
				strlen(CMD_SET_SW_AMSDU_NUM)) == 0)
			i4BytesWritten = priv_driver_set_amsdu_num(
				prNetDev, pcCommand, i4TotalLen);
		else if (strnicmp(pcCommand, CMD_SET_SW_AMSDU_SIZE,
				  strlen(CMD_SET_SW_AMSDU_SIZE)) == 0)
			i4BytesWritten = priv_driver_set_amsdu_size(
				prNetDev, pcCommand, i4TotalLen);
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
		} else if (!strnicmp(pcCommand, CMD_DUMP_TS,
				     strlen(CMD_DUMP_TS)) ||
			   !strnicmp(pcCommand, CMD_ADD_TS,
				     strlen(CMD_ADD_TS)) ||
			   !strnicmp(pcCommand, CMD_DEL_TS,
				     strlen(CMD_DEL_TS))) {
			kalIoctl(prGlueInfo, wlanoidTspecOperation,
				 (void *)pcCommand, i4TotalLen, FALSE, FALSE,
				 FALSE, &i4BytesWritten);
		} else if (kalStrStr(pcCommand, "-IT ")) {
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
#if CFG_SUPPORT_802_11K
		} else if (!strnicmp(pcCommand, CMD_NEIGHBOR_REQ,
					strlen(CMD_NEIGHBOR_REQ))) {
			i4BytesWritten = priv_driver_neighbor_request(
				prNetDev, pcCommand, i4TotalLen);
#endif
#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
		} else if (!strnicmp(pcCommand, CMD_BTM_QUERY,
					strlen(CMD_BTM_QUERY))) {
			i4BytesWritten = priv_driver_bss_transition_query(
				prNetDev, pcCommand, i4TotalLen);
#endif
		} else if (strnicmp(pcCommand, CMD_GET_BSS_TABLE,
					strlen(CMD_GET_BSS_TABLE)) == 0) {
			i4BytesWritten = priv_driver_get_bsstable(prNetDev,
						pcCommand, i4TotalLen);
#ifdef CFG_SUPPORT_TIME_MEASURE
		} else if (strnicmp(pcCommand, CMD_START_FTM_NON_BLOCK,
					strlen(CMD_START_FTM_NON_BLOCK)) == 0) {
			i4BytesWritten = priv_driver_start_ftm(prNetDev,
						pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_START_FTM,
					strlen(CMD_START_FTM)) == 0) {
			i4BytesWritten = priv_driver_start_ftm(prNetDev,
						pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_TMR_DISTANCE,
					strlen(CMD_GET_TMR_DISTANCE)) == 0) {
			i4BytesWritten = priv_driver_get_ftm(prNetDev,
						pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_GET_TMR_AUDIOSYNC,
					strlen(CMD_GET_TMR_AUDIOSYNC)) == 0) {
			i4BytesWritten = priv_driver_get_ftm(prNetDev,
						pcCommand, i4TotalLen);
		} else if (strnicmp(pcCommand, CMD_ENABLE_TMR,
					strlen(CMD_ENABLE_TMR)) == 0) {
			i4BytesWritten = priv_driver_enable_tmr(prNetDev,
						pcCommand, i4TotalLen);
#endif
#if CFG_SUPPORT_WAC
		} else if (strnicmp(pcCommand, CMD_SET_WAC_IE_ENABLE,
					strlen(CMD_SET_WAC_IE_ENABLE)) == 0) {
			i4BytesWritten = priv_driver_set_wac_ie_enable(prNetDev,
						pcCommand, i4TotalLen);
#endif
		} else
				i4BytesWritten = priv_cmd_not_support
				(prNetDev, pcCommand, i4TotalLen);

	if (i4BytesWritten >= 0) {
		if ((i4BytesWritten == 0) && (i4TotalLen > 0)) {
			/* reset the command buffer */
			pcCommand[0] = '\0';
		}

		if (i4BytesWritten >= i4TotalLen) {
			DBGLOG(REQ, INFO,
			       "%s: i4BytesWritten %d > i4TotalLen < %d\n",
			       __func__, i4BytesWritten, i4TotalLen);
			i4BytesWritten = i4TotalLen;
		} else {
			pcCommand[i4BytesWritten] = '\0';
			i4BytesWritten++;
		}
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

	if (i4BytesWritten < 0) {
		DBGLOG(REQ, INFO, "%s: command %s Written is %d\n", __func__,
		       pcCommand, i4BytesWritten);
		if (i4TotalLen >= 3) {
			snprintf(pcCommand, 3, "OK");
			i4BytesWritten = strlen("OK");
		}
	}

exit:
	kfree(priv_cmd);

	return ret;
}				/* priv_support_driver_cmd */

#if CFG_SUPPORT_NCHO
/* NCHO related command definition. Setting by supplicant */
#define CMD_NCHO_ROAM_TRIGGER_GET		"GETROAMTRIGGER"
#define CMD_NCHO_ROAM_TRIGGER_SET		"SETROAMTRIGGER"
#define CMD_NCHO_ROAM_DELTA_GET			"GETROAMDELTA"
#define CMD_NCHO_ROAM_DELTA_SET			"SETROAMDELTA"
#define CMD_NCHO_ROAM_SCAN_PERIOD_GET		"GETROAMSCANPERIOD"
#define CMD_NCHO_ROAM_SCAN_PERIOD_SET		"SETROAMSCANPERIOD"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_GET		"GETROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_SET		"SETROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CONTROL_GET		"GETROAMSCANCONTROL"
#define CMD_NCHO_ROAM_SCAN_CONTROL_SET		"SETROAMSCANCONTROL"
#define CMD_NCHO_SCAN_CHANNEL_TIME_GET		"GETSCANCHANNELTIME"
#define CMD_NCHO_SCAN_CHANNEL_TIME_SET		"SETSCANCHANNELTIME"
#define CMD_NCHO_SCAN_HOME_TIME_GET		"GETSCANHOMETIME"
#define CMD_NCHO_SCAN_HOME_TIME_SET		"SETSCANHOMETIME"
#define CMD_NCHO_SCAN_HOME_AWAY_TIME_GET	"GETSCANHOMEAWAYTIME"
#define CMD_NCHO_SCAN_HOME_AWAY_TIME_SET	"SETSCANHOMEAWAYTIME"
#define CMD_NCHO_SCAN_NPROBES_GET		"GETSCANNPROBES"
#define CMD_NCHO_SCAN_NPROBES_SET		"SETSCANNPROBES"
#define CMD_NCHO_REASSOC_SEND			"REASSOC"
#define CMD_NCHO_ACTION_FRAME_SEND		"SENDACTIONFRAME"
#define CMD_NCHO_WES_MODE_GET			"GETWESMODE"
#define CMD_NCHO_WES_MODE_SET			"SETWESMODE"
#define CMD_NCHO_BAND_GET			"GETBAND"
#define CMD_NCHO_BAND_SET			"SETBAND"
#define CMD_NCHO_DFS_SCAN_MODE_GET		"GETDFSSCANMODE"
#define CMD_NCHO_DFS_SCAN_MODE_SET		"SETDFSSCANMODE"
#define CMD_NCHO_DFS_SCAN_MODE_GET		"GETDFSSCANMODE"
#define CMD_NCHO_DFS_SCAN_MODE_SET		"SETDFSSCANMODE"
#define CMD_NCHO_ENABLE				"NCHOENABLE"
#define CMD_NCHO_DISABLE			"NCHODISABLE"
static int
priv_driver_enable_ncho(IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen);
static int
priv_driver_disable_ncho(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen);

int
priv_driver_set_ncho_roam_trigger(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam trigger cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamTrigger,
				   &i4Param, sizeof(int32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set roam trigger fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE,
			       "NCHO set roam trigger successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}

int
priv_driver_get_ncho_roam_trigger(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamTrigger,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamTrigger fail 0x%x\n", rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &i4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4Param = RCPI_TO_dBm(i4Param);		/* RCPI to DB */
		DBGLOG(INIT, TRACE, "NCHO query RoamTrigger is %d\n", i4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%d", i4Param);
	}

	return i4BytesWritten;
}

int priv_driver_set_ncho_roam_delta(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR,
			       "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam delta cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamDelta,
				   &i4Param, sizeof(int32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "NCHO set roam delta fail 0x%x\n", rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set roam delta successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_roam_delta(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	int32_t i4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamDelta,
			   &i4Param, sizeof(int32_t),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamDelta fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", i4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%d", i4Param);
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_roam_scn_period(IN struct net_device *prNetDev,
					 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam period cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamScnPeriod,
				   &u4Param, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set roam period fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set roam period successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}

int priv_driver_get_ncho_roam_scn_period(IN struct net_device *prNetDev,
					 IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}
	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnPeriod,
			   &u4Param, sizeof(uint32_t),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamScnPeriod fail 0x%x\n",
		       rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", u4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_roam_scn_chnl(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4ChnlInfo = 0;
	uint8_t i = 1;
	uint8_t t = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct _CFG_NCHO_SCAN_CHNL_T rRoamScnChnl;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, cmd is %s\n", i4Argc,
		       apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		rRoamScnChnl.ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, ERROR, "NCHO ChannelListNum is %d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "NCHO param mismatch %d\n",
			       u4ChnlInfo);
			return -1;
		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4ChnlInfo);
			if (i4Ret) {
				while (i != 2) {
					rRoamScnChnl.arChnlInfoList[i]
					.ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR,
				       "NCHO parse chnl num error %d\n", i4Ret);
				return -1;
			}
			if (u4ChnlInfo != 0) {
				DBGLOG(INIT, TRACE,
				       "NCHO t = %d, channel value=%d\n",
				       t, u4ChnlInfo);
				if ((u4ChnlInfo >= 1) && (u4ChnlInfo <= 14))
					rRoamScnChnl.arChnlInfoList[t].eBand =
								BAND_2G4;
				else
					rRoamScnChnl.arChnlInfoList[t].eBand =
								BAND_5G;

				rRoamScnChnl.arChnlInfoList[t].ucChannelNum =
								u4ChnlInfo;
				t++;
			}

		}

		DBGLOG(INIT, TRACE, "NCHO set roam scan channel cmd\n");
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamScnChnl,
				   &rRoamScnChnl,
				   sizeof(struct _CFG_NCHO_SCAN_CHNL_T),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "NCHO set roam scan channel fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE,
			       "NCHO set roam scan channel successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}

int priv_driver_get_ncho_roam_scn_chnl(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t i = 0;
	uint32_t u4BufLen = 0;
	int32_t i4BytesWritten = -1;
	int32_t i4Argc = 0;
	uint32_t u4ChnlInfo = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	struct _CFG_NCHO_SCAN_CHNL_T rRoamScnChnl;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnChnl,
			   &rRoamScnChnl, sizeof(struct _CFG_NCHO_SCAN_CHNL_T),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamScnChnl fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n",
		       rRoamScnChnl.ucChannelListNum);
		u4ChnlInfo = rRoamScnChnl.ucChannelListNum;
		i4BytesWritten = 0;
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					   i4TotalLen - i4BytesWritten, "%u",
					   u4ChnlInfo);
		for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
			u4ChnlInfo =
				rRoamScnChnl.arChnlInfoList[i].ucChannelNum;
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
					       i4TotalLen - i4BytesWritten,
					       " %u", u4ChnlInfo);
		}
	}

	DBGLOG(REQ, TRACE, "NCHO i4BytesWritten is %d and channel list is %s\n",
	       i4BytesWritten, pcCommand);
	return i4BytesWritten;
}

int priv_driver_set_ncho_roam_scn_ctrl(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam scan control cmd %d\n",
		       u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoRoamScnCtrl,
				   &u4Param, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "NCHO set roam scan control fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE,
			       "NCHO set roam scan control successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_roam_scn_ctrl(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoRoamScnCtrl,
			   &u4Param, sizeof(uint32_t),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoRoamScnCtrl fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", u4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_scn_chnl_time(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set scan channel time cmd %d\n",
		       u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoScnChnlTime,
				   &u4Param, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "NCHO set scan channel time fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE,
			       "NCHO set scan channel time successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_scn_chnl_time(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoScnChnlTime,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoScnChnlTime fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n",
		       cmdV1Header.buffer);
		i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u",
						  u4Param);
		}
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_scn_home_time(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc,
		       apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set scan home time cmd %d\n",
		       u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoScnHomeTime,
				   &u4Param, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "NCHO set scan home time fail 0x%x\n", rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE,
			       "NCHO set scan home time successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_scn_home_time(IN struct net_device *prNetDev,
				       IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}
	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoScnHomeTime,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoScnChnlTime fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n",
		       cmdV1Header.buffer);
		i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u",
						  u4Param);
		}
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_scn_home_away_time(IN struct net_device *prNetDev,
					IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set scan home away time cmd %d\n",
		       u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoScnHomeAwayTime,
				   &u4Param, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "NCHO set scan home away time fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE,
			       "NCHO set scan home away time successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_scn_home_away_time(IN struct net_device *prNetDev,
				    IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}
	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoScnHomeAwayTime,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoScnHomeAwayTime fail 0x%x\n",
		       rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}

	return i4BytesWritten;
}

int priv_driver_set_ncho_scn_nprobes(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);

		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set scan nprobes cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoScnNprobes,
				   &u4Param, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set scan nprobes fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE,
			       "NCHO set scan nprobes successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}

int priv_driver_get_ncho_scn_nprobes(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoScnNprobes,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoScnNprobes fail 0x%x\n", rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}

	return i4BytesWritten;
}

/* handle this command as framework roaming */
int priv_driver_send_ncho_reassoc(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Ret = -1;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct _CFG_NCHO_RE_ASSOC_T rReAssoc;
	struct PARAM_CONNECT rParamConn;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc == 3) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s %s\n", i4Argc,
		       apcArgv[1], apcArgv[2]);

		i4Ret = kalkStrtou32(apcArgv[2], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}
		DBGLOG(INIT, TRACE, "NCHO send reassoc cmd %d\n", u4Param);
		kalMemZero(&rReAssoc, sizeof(struct _CFG_NCHO_RE_ASSOC_T));
		rReAssoc.u4CenterFreq = nicChannelNum2Freq(u4Param);
		CmdStringMacParse(apcArgv[1], (uint8_t **)&apcArgv[1],
				  &u4SetInfoLen, rReAssoc.aucBssid);
		DBGLOG(INIT, TRACE, "NCHO Bssid " MACSTR " to roam\n",
		       MAC2STR(rReAssoc.aucBssid));
		rParamConn.pucBssid = (uint8_t *)rReAssoc.aucBssid;
		rParamConn.pucSsid = (uint8_t *)rReAssoc.aucSsid;
		rParamConn.u4SsidLen = rReAssoc.u4SsidLen;
		rParamConn.u4CenterFreq = rReAssoc.u4CenterFreq;

		rStatus = kalIoctl(prGlueInfo, wlanoidGetNchoReassocInfo,
				   &rParamConn, sizeof(struct PARAM_CONNECT),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       "NCHO get reassoc information fail 0x%x\n",
			       rStatus);
			return -1;
		}
		DBGLOG(INIT, TRACE, "NCHO ssid %s to roam\n",
		       rParamConn.pucSsid);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetConnect, &rParamConn,
				   sizeof(struct PARAM_CONNECT),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO send reassoc fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO send reassoc successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}

int
nchoRemainOnChannel(IN struct ADAPTER *prAdapter, IN uint8_t ucChannelNum,
		    IN uint32_t u4DewellTime)
{
	int32_t i4Ret = -1;
	struct MSG_REMAIN_ON_CHANNEL *prMsgChnlReq =
					(struct MSG_REMAIN_ON_CHANNEL *) NULL;

	do {
		if (!prAdapter)
			break;

		prMsgChnlReq = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				   sizeof(struct MSG_REMAIN_ON_CHANNEL));

		if (prMsgChnlReq == NULL) {
			ASSERT(FALSE);
			DBGLOG(REQ, ERROR,
			       "NCHO there is no memory for message channel req\n");
			return i4Ret;
		}
		kalMemZero(prMsgChnlReq, sizeof(struct MSG_REMAIN_ON_CHANNEL));

		prMsgChnlReq->rMsgHdr.eMsgId = MID_MNY_AIS_REMAIN_ON_CHANNEL;
		prMsgChnlReq->u4DurationMs = u4DewellTime;
		prMsgChnlReq->u8Cookie = 0;
		prMsgChnlReq->ucChannelNum = ucChannelNum;

		if ((ucChannelNum >= 1) && (ucChannelNum <= 14))
			prMsgChnlReq->eBand = BAND_2G4;
		else
			prMsgChnlReq->eBand = BAND_5G;

		mboxSendMsg(prAdapter, MBOX_ID_0,
			    (struct MSG_HDR *) prMsgChnlReq,
			    MSG_SEND_METHOD_BUF);

		i4Ret = 0;
	} while (FALSE);

	return i4Ret;
}

int
nchoSendActionFrame(IN struct ADAPTER *prAdapter,
		    struct _NCHO_ACTION_FRAME_PARAMS_T *prParamActionFrame)
{
	int32_t i4Ret = -1;
	struct MSG_MGMT_TX_REQUEST *prMsgTxReq =
					(struct MSG_MGMT_TX_REQUEST *) NULL;

	if (!prAdapter || !prParamActionFrame)
		return i4Ret;

	do {
		/* Channel & Channel Type & Wait time are ignored. */
		prMsgTxReq = cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
					 sizeof(struct MSG_MGMT_TX_REQUEST));

		if (prMsgTxReq == NULL) {
			ASSERT(FALSE);
			DBGLOG(REQ, ERROR,
			       "NCHO there is no memory for message tx req\n");
			return i4Ret;
		}

		prMsgTxReq->fgNoneCckRate = FALSE;
		prMsgTxReq->fgIsWaitRsp = TRUE;

		prMsgTxReq->u8Cookie = 0;
		prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_AIS_NCHO_ACTION_FRAME;
		mboxSendMsg(prAdapter, MBOX_ID_0, (struct MSG_HDR *) prMsgTxReq,
			    MSG_SEND_METHOD_BUF);

		i4Ret = 0;
	} while (FALSE);

	if ((i4Ret != 0) && (prMsgTxReq != NULL)) {
		if (prMsgTxReq->prMgmtMsduInfo != NULL)
			cnmMgtPktFree(prAdapter, prMsgTxReq->prMgmtMsduInfo);

		cnmMemFree(prAdapter, prMsgTxReq);
	}

	return i4Ret;
}

uint32_t nchoParseActionFrame(
	IN struct _NCHO_ACTION_FRAME_PARAMS_T *prParamActionFrame,
	IN char *pcCommand)
{
	uint32_t u4SetInfoLen = 0;
	uint32_t u4Num = 0;
	struct _NCHO_AF_INFO_T *prAfInfo = NULL;

	if (!prParamActionFrame || !pcCommand)
		return WLAN_STATUS_FAILURE;

	prAfInfo = (struct _NCHO_AF_INFO_T *)(pcCommand +
				kalStrLen(CMD_NCHO_ACTION_FRAME_SEND) + 1);
	if (prAfInfo->i4len > CMD_NCHO_AF_DATA_LENGTH) {
		DBGLOG(INIT, ERROR, "NCHO AF data length is %d\n",
		       prAfInfo->i4len);
		return WLAN_STATUS_FAILURE;
	}

	prParamActionFrame->i4len = prAfInfo->i4len;
	prParamActionFrame->i4channel = prAfInfo->i4channel;
	prParamActionFrame->i4DwellTime = prAfInfo->i4DwellTime;
	kalMemZero(prParamActionFrame->aucData, CMD_NCHO_AF_DATA_LENGTH/2);
	u4SetInfoLen = prAfInfo->i4len;
	while (u4SetInfoLen > 0 && u4Num < CMD_NCHO_AF_DATA_LENGTH/2) {
		*(prParamActionFrame->aucData + u4Num) =
				CmdString2HexParse(prAfInfo->pucData,
					   (uint8_t **)&prAfInfo->pucData,
					   (uint8_t *)&u4SetInfoLen);
		u4Num++;
	}
	DBGLOG(INIT, TRACE, "NCHO MAC str is %s\n", prAfInfo->aucBssid);
	CmdStringMacParse(prAfInfo->aucBssid,
			  (uint8_t **)&prAfInfo->aucBssid,
			  &u4SetInfoLen,
			  prParamActionFrame->aucBssid);
	return WLAN_STATUS_SUCCESS;
}

int
priv_driver_send_ncho_action_frame(IN struct net_device *prNetDev,
				   IN char *pcCommand, IN int i4TotalLen)
{
	NCHO_ACTION_FRAME_PARAMS rParamActionFrame;
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Ret = -1;
	uint32_t u4SetInfoLen = 0;
	unsigned long ulTimer = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = nchoParseActionFrame(&rParamActionFrame, pcCommand);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO action frame parse error\n");
		return -1;
	}

	DBGLOG(INIT, TRACE, "NCHO MAC is " MACSTR "\n",
		MAC2STR(rParamActionFrame.aucBssid));
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSendNchoActionFrameStart,
			   &rParamActionFrame,
			   sizeof(NCHO_ACTION_FRAME_PARAMS),
			   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO send action fail 0x%x\n", rStatus);
		return -1;
	}

	reinit_completion(&prGlueInfo->rAisChGrntComp);
	i4Ret = nchoRemainOnChannel(prGlueInfo->prAdapter,
				rParamActionFrame.i4channel,
				rParamActionFrame.i4DwellTime);

	ulTimer = wait_for_completion_timeout(&prGlueInfo->rAisChGrntComp,
				msecs_to_jiffies(CMD_NCHO_COMP_TIMEOUT));
	if (ulTimer) {
		rStatus = kalIoctl(prGlueInfo,
			   wlanoidSendNchoActionFrameEnd,
			   NULL, 0, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO send action fail 0x%x\n",
			       rStatus);
			return -1;
		}
		i4Ret = nchoSendActionFrame(prGlueInfo->prAdapter,
					    &rParamActionFrame);
	} else {
		i4Ret = -1;
		DBGLOG(INIT, ERROR, "NCHO req channel timeout\n");
	}

	return i4Ret;
}

int
priv_driver_set_ncho_wes_mode(IN struct net_device *prNetDev,
			      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint8_t puCommondBuf[WLAN_CFG_ARGV_MAX];


	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}
		/*If WES mode is 1, enable NCHO*/
		/*If WES mode is 0, disable NCHO*/
		if (u4Param == TRUE &&
		    prGlueInfo->prAdapter->rNchoInfo.fgECHOEnabled == FALSE) {
			kalSnprintf(puCommondBuf, WLAN_CFG_ARGV_MAX, "%s %d",
				    CMD_NCHO_ENABLE, 1);
			priv_driver_enable_ncho(prNetDev, puCommondBuf,
						sizeof(puCommondBuf));
		} else if (u4Param == FALSE &&
		    prGlueInfo->prAdapter->rNchoInfo.fgECHOEnabled == TRUE) {
			kalSnprintf(puCommondBuf, WLAN_CFG_ARGV_MAX, "%s",
				    CMD_NCHO_DISABLE);
			priv_driver_disable_ncho(prNetDev, puCommondBuf,
						 sizeof(puCommondBuf));
		}

		DBGLOG(INIT, INFO, "NCHO set WES mode cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoWesMode, &u4Param,
				   sizeof(uint32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set WES mode fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set WES mode successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}

int priv_driver_get_ncho_wes_mode(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -WLAN_STATUS_FAILURE;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoWesMode, &u4Param,
			   sizeof(uint32_t),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoWesMode fail 0x%x\n",
		       rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", u4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}
	DBGLOG(REQ, TRACE, "NCHO get result is %s\n", pcCommand);
	return i4BytesWritten;
}

int priv_driver_set_ncho_band(IN struct net_device *prNetDev,
			      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set band cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoBand, &u4Param,
				   sizeof(uint32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set band fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set band successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_band(IN struct net_device *prNetDev,
			      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoBand, &u4Param,
			   sizeof(uint32_t),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoBand fail 0x%x\n",
		       rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", u4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_dfs_scn_mode(IN struct net_device *prNetDev,
				      IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	uint32_t u4SetInfoLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4Ret = -1;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set DFS scan cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoDfsScnMode,
				   &u4Param, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set DFS scan fail 0x%x\n",
			       rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set DFS scan successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int
priv_driver_get_ncho_dfs_scn_mode(IN struct net_device *prNetDev,
				  IN char *pcCommand, IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO Error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoDfsScnMode, &cmdV1Header,
			   sizeof(struct CMD_HEADER),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidQueryNchoDfsScnMode fail 0x%x\n", rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}

	return i4BytesWritten;
}

int
priv_driver_enable_ncho(IN struct net_device *prNetDev, IN char *pcCommand,
			IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Param = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int32_t i4BytesWritten = -1;
	uint32_t u4SetInfoLen = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc,
		       apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n",
			       i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set enable cmd %d\n",
			       u4Param);
			rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoEnable,
				&u4Param, sizeof(uint32_t),
				FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR,
				       "NCHO set enable fail 0x%x\n", rStatus);
				i4BytesWritten = -1;
			} else {
				DBGLOG(INIT, TRACE,
				       "NCHO set enable successed\n");
				i4BytesWritten = 0;
			}
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4BytesWritten;
}

int
priv_driver_disable_ncho(IN struct net_device *prNetDev, IN char *pcCommand,
			 IN int i4TotalLen)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4BytesWritten = -1;
	uint32_t u4Param = 0;
	uint32_t u4BufLen = 0;
	int32_t i4Argc = 0;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}
	/*<1> Set NCHO Disable to FW*/
	u4Param = FALSE;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetNchoEnable, &u4Param,
			sizeof(uint32_t), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "NCHO wlanoidSetNchoEnable :%d fail 0x%x\n",
		       u4Param, rStatus);
		return i4BytesWritten;
	}

	/*<2> Query NCHOEnable Satus*/
	rStatus = kalIoctl(prGlueInfo, wlanoidQueryNchoEnable,
			   &cmdV1Header, sizeof(cmdV1Header),
			   TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoEnable fail 0x%x\n",
		       rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);


	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n",
		       i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}

	return i4BytesWritten;
}
/*Check NCHO is enable or not.*/
u_int8_t
priv_driver_auto_enable_ncho(IN struct net_device *prNetDev)
{
	uint8_t puCommondBuf[WLAN_CFG_ARGV_MAX];
	struct GLUE_INFO *prGlueInfo = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);

	kalSnprintf(puCommondBuf, WLAN_CFG_ARGV_MAX, "%s %d", CMD_NCHO_ENABLE,
		    1);
#if CFG_SUPPORT_NCHO_AUTO_ENABLE
	if (prGlueInfo->prAdapter->rNchoInfo.fgECHOEnabled == FALSE) {
		DBGLOG(INIT, INFO,
		       "NCHO is unavailable now! Start to NCHO Enable CMD\n");
		priv_driver_enable_ncho(prNetDev, puCommondBuf,
					sizeof(puCommondBuf));

	}
#endif
	return TRUE;
}

#endif
