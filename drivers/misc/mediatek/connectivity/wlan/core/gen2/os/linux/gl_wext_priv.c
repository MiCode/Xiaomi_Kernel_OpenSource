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
#if CFG_SUPPORT_WAPI
#include "gl_sec.h"
#endif
#if CFG_ENABLE_WIFI_DIRECT
#include "gl_p2p_os.h"
#endif
#ifdef FW_CFG_SUPPORT
#include "fwcfg.h"
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define NUM_SUPPORTED_OIDS		(sizeof(arWlanOidReqTable) / sizeof(WLAN_REQ_ENTRY))
#define CMD_START			"START"
#define CMD_STOP			"STOP"
#define CMD_SCAN_ACTIVE			"SCAN-ACTIVE"
#define CMD_SCAN_PASSIVE		"SCAN-PASSIVE"
#define CMD_RSSI			"RSSI"
#define CMD_LINKSPEED			"LINKSPEED"
#define CMD_RXFILTER_START		"RXFILTER-START"
#define CMD_RXFILTER_STOP		"RXFILTER-STOP"
#define CMD_RXFILTER_ADD		"RXFILTER-ADD"
#define CMD_RXFILTER_REMOVE		"RXFILTER-REMOVE"
#define CMD_BTCOEXSCAN_START		"BTCOEXSCAN-START"
#define CMD_BTCOEXSCAN_STOP		"BTCOEXSCAN-STOP"
#define CMD_BTCOEXMODE			"BTCOEXMODE"
#define CMD_SETSUSPENDOPT		"SETSUSPENDOPT"
#define CMD_SETSUSPENDMODE		"SETSUSPENDMODE"
#define CMD_P2P_DEV_ADDR		"P2P_DEV_ADDR"
#define CMD_SETFWPATH			"SETFWPATH"
#define CMD_SETBAND			"SETBAND"
#define CMD_GETBAND			"GETBAND"
#define CMD_COUNTRY			"COUNTRY"
#define CMD_P2P_SET_NOA			"P2P_SET_NOA"
#define CMD_P2P_GET_NOA			"P2P_GET_NOA"
#define CMD_P2P_SET_PS			"P2P_SET_PS"
#define CMD_SET_AP_WPS_P2P_IE		"SET_AP_WPS_P2P_IE"
#define CMD_SETROAMMODE			"SETROAMMODE"
#define CMD_MIRACAST			"MIRACAST"

#define CMD_ECSA			"P2P_ECSA"
#define CMD_PNOSSIDCLR_SET	"PNOSSIDCLR"
#define CMD_PNOSETUP_SET	"PNOSETUP "
#define CMD_PNOENABLE_SET	"PNOFORCE"
#define CMD_PNODEBUG_SET	"PNODEBUG"
#define CMD_WLS_BATCHING	"WLS_BATCHING"

#define CMD_OKC_SET_PMK		"SET_PMK"
#define CMD_OKC_ENABLE		"OKC_ENABLE"

#define CMD_ADD_TS          "addts"
#define CMD_DELETE_TS		"delts"
#define CMD_FW_PARAM            "set_fw_param "

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

#define	CMD_BAND_AUTO		0
#define	CMD_BAND_5G		1
#define	CMD_BAND_2G		2
#define	CMD_BAND_ALL		3
#define	CMD_OID_BUF_LENGTH	4096

/* Mediatek private command */

#define CMD_SET_SW_CTRL	        "SET_SW_CTRL"
#define CMD_GET_SW_CTRL         "GET_SW_CTRL"
#define CMD_SET_CFG             "SET_CFG"
#define CMD_GET_CFG             "GET_CFG"
#define CMD_SET_CHIP            "SET_CHIP"
#define CMD_GET_CHIP            "GET_CHIP"
#define CMD_SET_DBG_LEVEL       "SET_DBG_LEVEL"
#define CMD_GET_DBG_LEVEL       "GET_DBG_LEVEL"
#define CMD_SET_FCC_CERT        "SET_FCC_CHANNEL"
#define CMD_GET_WIFI_TYPE		"GET_WIFI_TYPE"
#define PRIV_CMD_SIZE			512

static UINT_32 g_ucMiracastMode = MIRACAST_MODE_OFF;

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

#if CFG_SUPPORT_BATCH_SCAN
#define CMD_BATCH_SET           "WLS_BATCHING SET"
#define CMD_BATCH_GET           "WLS_BATCHING GET"
#define CMD_BATCH_STOP          "WLS_BATCHING STOP"
#endif

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
priv_driver_enable_ncho(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen);
static int
priv_driver_disable_ncho(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen);

int
priv_driver_set_ncho_roam_trigger(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Param = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	UINT_32 u4SetInfoLen = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam trigger cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoRoamTrigger,
				   &i4Param, sizeof(INT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set roam trigger fail 0x%x\n", rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set roam trigger successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}

int
priv_driver_get_ncho_roam_trigger(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	INT_32 i4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoRoamTrigger,
			   &cmdV1Header,
			   sizeof(cmdV1Header), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoRoamTrigger fail 0x%x\n", rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &i4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n", i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4Param = RCPI_TO_dBm(i4Param);		/* RCPI to DB */
		DBGLOG(INIT, TRACE, "NCHO query RoamTrigger is %d\n", i4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%d", i4Param);
	}

	return i4BytesWritten;
}

int priv_driver_set_ncho_roam_delta(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtos32(apcArgv[1], 0, &i4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam delta cmd %d\n", i4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoRoamDelta,
				   &i4Param, sizeof(INT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set roam delta fail 0x%x\n", rStatus);
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
int priv_driver_get_ncho_roam_delta(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	INT_32 i4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoRoamDelta,
			   &i4Param,
			   sizeof(INT_32), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoRoamDelta fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", i4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%d", i4Param);
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_roam_scn_period(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam period cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoRoamScnPeriod,
				   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set roam period fail 0x%x\n", rStatus);
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
int priv_driver_get_ncho_roam_scn_period(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoRoamScnPeriod,
			   &u4Param,
			   sizeof(UINT_32), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoRoamScnPeriod fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", u4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}
	return i4BytesWritten;
}
int priv_driver_set_ncho_roam_scn_chnl(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4ChnlInfo = 0;
	UINT_8 i = 1;
	UINT_8 t = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	CFG_NCHO_SCAN_CHNL_T rRoamScnChnl;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, cmd is %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4ChnlInfo);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		rRoamScnChnl.ucChannelListNum = u4ChnlInfo;
		DBGLOG(REQ, ERROR, "NCHO ChannelListNum is %d\n", u4ChnlInfo);
		if (i4Argc != u4ChnlInfo + 2) {
			DBGLOG(REQ, ERROR, "NCHO param mismatch %d\n", u4ChnlInfo);
			return -1;
		}
		for (i = 2; i < i4Argc; i++) {
			i4Ret = kalkStrtou32(apcArgv[i], 0, &u4ChnlInfo);
			if (i4Ret) {
				while (i != 2) {
					rRoamScnChnl.arChnlInfoList[i].ucChannelNum = 0;
					i--;
				}
				DBGLOG(REQ, ERROR, "NCHO parse chnl num error %d\n", i4Ret);
				return -1;
			}
			if (u4ChnlInfo != 0) {
				DBGLOG(INIT, TRACE, "NCHO t = %d, channel value=%d\n", t, u4ChnlInfo);
				if ((u4ChnlInfo >= 1) && (u4ChnlInfo <= 14))
					rRoamScnChnl.arChnlInfoList[t].eBand = BAND_2G4;
				else
					rRoamScnChnl.arChnlInfoList[t].eBand = BAND_5G;

				rRoamScnChnl.arChnlInfoList[t].ucChannelNum = u4ChnlInfo;
				t++;
			}

		}

		DBGLOG(INIT, TRACE, "NCHO set roam scan channel cmd\n");
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoRoamScnChnl,
				   &rRoamScnChnl,
				   sizeof(CFG_NCHO_SCAN_CHNL_T),
				   FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set roam scan channel fail 0x%x\n", rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set roam scan channel successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}

int priv_driver_get_ncho_roam_scn_chnl(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_8 i = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4BytesWritten = -1;
	INT_32 i4Argc = 0;
	UINT_32 u4ChnlInfo = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	CFG_NCHO_SCAN_CHNL_T rRoamScnChnl;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoRoamScnChnl,
			   &rRoamScnChnl,
			   sizeof(CFG_NCHO_SCAN_CHNL_T), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoRoamScnChnl fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", rRoamScnChnl.ucChannelListNum);
		u4ChnlInfo = rRoamScnChnl.ucChannelListNum;
		i4BytesWritten = 0;
		i4BytesWritten += snprintf(pcCommand + i4BytesWritten, i4TotalLen - i4BytesWritten, "%u", u4ChnlInfo);
		for (i = 0; i < rRoamScnChnl.ucChannelListNum; i++) {
			u4ChnlInfo = rRoamScnChnl.arChnlInfoList[i].ucChannelNum;
			i4BytesWritten += snprintf(pcCommand + i4BytesWritten,
						   i4TotalLen - i4BytesWritten, " %u", u4ChnlInfo);
		}
	}

	DBGLOG(REQ, TRACE, "NCHO i4BytesWritten is %d and channel list is %s\n", i4BytesWritten, pcCommand);
	return i4BytesWritten;
}

int priv_driver_set_ncho_roam_scn_ctrl(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set roam scan control cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoRoamScnCtrl,
				   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set roam scan control fail 0x%x\n", rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set roam scan control successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_roam_scn_ctrl(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoRoamScnCtrl,
			   &u4Param,
			   sizeof(UINT_32), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoRoamScnCtrl fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", u4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_scn_chnl_time(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set scan channel time cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoScnChnlTime,
				   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set scan channel time fail 0x%x\n", rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set scan channel time successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_scn_chnl_time(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoScnChnlTime,
			   &cmdV1Header,
			   sizeof(cmdV1Header), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoScnChnlTime fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
		i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n", i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
		}
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_scn_home_time(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set scan home time cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoScnHomeTime,
				   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set scan home time fail 0x%x\n", rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set scan home time successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_scn_home_time(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoScnHomeTime,
			   &cmdV1Header,
			   sizeof(cmdV1Header), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoScnChnlTime fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
		i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n", i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
		}
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_scn_home_away_time(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set scan home away time cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoScnHomeAwayTime,
				   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set scan home away time fail 0x%x\n", rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set scan home away time successed\n");
			i4Ret = 0;
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}
int priv_driver_get_ncho_scn_home_away_time(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoScnHomeAwayTime,
			   &cmdV1Header,
			   sizeof(cmdV1Header), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoScnHomeAwayTime fail 0x%x\n", rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n", i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}

	return i4BytesWritten;
}

int priv_driver_set_ncho_scn_nprobes(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);

		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set scan nprobes cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoScnNprobes,
				   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set scan nprobes fail 0x%x\n", rStatus);
			i4Ret = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set scan nprobes successed\n");
			i4Ret = 0;
		}

	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4Ret;
}

int priv_driver_get_ncho_scn_nprobes(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoScnNprobes,
			   &cmdV1Header,
			   sizeof(cmdV1Header), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoScnNprobes fail 0x%x\n", rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n", i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}

	return i4BytesWritten;
}

/* handle this command as framework roaming */
int priv_driver_send_ncho_reassoc(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Ret = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	CFG_NCHO_RE_ASSOC_T rReAssoc;
	PARAM_CONNECT_T rParamConn;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc == 3) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s %s\n", i4Argc, apcArgv[1], apcArgv[2]);

		i4Ret = kalkStrtou32(apcArgv[2], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}
		DBGLOG(INIT, TRACE, "NCHO send reassoc cmd %d\n", u4Param);
		kalMemZero(&rReAssoc, sizeof(CFG_NCHO_RE_ASSOC_T));
		rReAssoc.u4CenterFreq = nicChannelNum2Freq(u4Param);
		CmdStringMacParse(apcArgv[1], (UINT_8 **)&apcArgv[1], &u4SetInfoLen, rReAssoc.aucBssid);
		DBGLOG(INIT, TRACE, "NCHO Bssid %pM to roam\n", rReAssoc.aucBssid);
		rParamConn.pucBssid = (UINT_8 *)rReAssoc.aucBssid;
		rParamConn.pucSsid = (UINT_8 *)rReAssoc.aucSsid;
		rParamConn.u4SsidLen = rReAssoc.u4SsidLen;
		rParamConn.u4CenterFreq = rReAssoc.u4CenterFreq;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidGetNchoReassocInfo,
				   &rParamConn,
				   sizeof(PARAM_CONNECT_T), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO get reassoc information fail 0x%x\n", rStatus);
			return -1;
		}
		DBGLOG(INIT, TRACE, "NCHO ssid %s to roam\n", HIDE(rParamConn.pucSsid));
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetConnect,
				   &rParamConn,
				   sizeof(PARAM_CONNECT_T), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO send reassoc fail 0x%x\n", rStatus);
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
nchoRemainOnChannel(IN P_ADAPTER_T prAdapter, IN UINT_8 ucChannelNum, IN UINT_32 u4DewellTime)
{
	INT_32 i4Ret = -1;
	P_MSG_REMAIN_ON_CHANNEL_T prMsgChnlReq = (P_MSG_REMAIN_ON_CHANNEL_T) NULL;

	do {
		if (!prAdapter)
			break;

		prMsgChnlReq = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_REMAIN_ON_CHANNEL_T));

		if (prMsgChnlReq == NULL) {
			ASSERT(FALSE);
			DBGLOG(REQ, ERROR, "NCHO there is no memory for message channel req\n");
			return i4Ret;
		}
		kalMemZero(prMsgChnlReq, sizeof(MSG_REMAIN_ON_CHANNEL_T));

		prMsgChnlReq->rMsgHdr.eMsgId = MID_MNY_AIS_REMAIN_ON_CHANNEL;
		prMsgChnlReq->u4DurationMs = u4DewellTime;
		prMsgChnlReq->u8Cookie = 0;
		prMsgChnlReq->ucChannelNum = ucChannelNum;

		if ((ucChannelNum >= 1) && (ucChannelNum <= 14))
			prMsgChnlReq->eBand = BAND_2G4;
		else
			prMsgChnlReq->eBand = BAND_5G;

		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChnlReq, MSG_SEND_METHOD_BUF);

		i4Ret = 0;
	} while (FALSE);

	return i4Ret;
}

int
nchoSendActionFrame(IN P_ADAPTER_T prAdapter, P_NCHO_ACTION_FRAME_PARAMS prParamActionFrame)
{
	INT_32 i4Ret = -1;
	P_MSG_MGMT_TX_REQUEST_T prMsgTxReq = (P_MSG_MGMT_TX_REQUEST_T) NULL;

	if (!prAdapter || !prParamActionFrame)
		return i4Ret;

	do {
		/* Channel & Channel Type & Wait time are ignored. */
		prMsgTxReq = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_MGMT_TX_REQUEST_T));

		if (prMsgTxReq == NULL) {
			ASSERT(FALSE);
			DBGLOG(REQ, ERROR, "NCHO there is no memory for message tx req\n");
			return i4Ret;
		}

		prMsgTxReq->fgNoneCckRate = FALSE;
		prMsgTxReq->fgIsWaitRsp = TRUE;

		prMsgTxReq->u8Cookie = 0;
		prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_AIS_NCHO_ACTION_FRAME;
		mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgTxReq, MSG_SEND_METHOD_BUF);

		i4Ret = 0;
	} while (FALSE);

	if ((i4Ret != 0) && (prMsgTxReq != NULL)) {
		if (prMsgTxReq->prMgmtMsduInfo != NULL)
			cnmMgtPktFree(prAdapter, prMsgTxReq->prMgmtMsduInfo);

		cnmMemFree(prAdapter, prMsgTxReq);
	}

	return i4Ret;
}

WLAN_STATUS nchoParseActionFrame(IN P_NCHO_ACTION_FRAME_PARAMS prParamActionFrame, IN char *pcCommand)
{
	UINT_32 u4SetInfoLen = 0;
	UINT_32 u4Num = 0;
	P_NCHO_AF_INFO prAfInfo = NULL;

	if (!prParamActionFrame || !pcCommand)
		return WLAN_STATUS_FAILURE;

	prAfInfo = (P_NCHO_AF_INFO)(pcCommand + kalStrLen(CMD_NCHO_ACTION_FRAME_SEND) + 1);
	if (prAfInfo->i4len > CMD_NCHO_AF_DATA_LENGTH) {
		DBGLOG(INIT, ERROR, "NCHO AF data length is %d\n", prAfInfo->i4len);
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
						   (UINT_8 **)&prAfInfo->pucData,
						   (UINT_8 *)&u4SetInfoLen);
		u4Num++;
	}
	DBGLOG(INIT, TRACE, "NCHO MAC str is %s\n", prAfInfo->aucBssid);
	CmdStringMacParse(prAfInfo->aucBssid,
			  (UINT_8 **)&prAfInfo->aucBssid,
			  &u4SetInfoLen,
			  prParamActionFrame->aucBssid);
	return WLAN_STATUS_SUCCESS;
}

int
priv_driver_send_ncho_action_frame(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	NCHO_ACTION_FRAME_PARAMS rParamActionFrame;
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Ret = -1;
	UINT_32 u4SetInfoLen = 0;
	ULONG ulTimer = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = nchoParseActionFrame(&rParamActionFrame, pcCommand);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO action frame parse error\n");
		return -1;
	}

	DBGLOG(INIT, TRACE, "NCHO MAC is %pM\n", rParamActionFrame.aucBssid);
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
			DBGLOG(INIT, ERROR, "NCHO send action fail 0x%x\n", rStatus);
			return -1;
		}
		i4Ret = nchoSendActionFrame(prGlueInfo->prAdapter, &rParamActionFrame);
	} else {
		i4Ret = -1;
		DBGLOG(INIT, ERROR, "NCHO req channel timeout\n");
	}

	return i4Ret;
}

int
priv_driver_set_ncho_wes_mode(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	UINT_8 puCommondBuf[WLAN_CFG_ARGV_MAX];


	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}
		/*If WES mode is 1, enable NCHO*/
		/*If WES mode is 0, disable NCHO*/
		if (u4Param == TRUE && prGlueInfo->prAdapter->rNchoInfo.fgECHOEnabled == FALSE) {
			kalSnprintf(puCommondBuf, WLAN_CFG_ARGV_MAX, "%s %d", CMD_NCHO_ENABLE, 1);
			priv_driver_enable_ncho(prNetDev, puCommondBuf, sizeof(puCommondBuf));
		} else if (u4Param == FALSE && prGlueInfo->prAdapter->rNchoInfo.fgECHOEnabled == TRUE) {
			kalSnprintf(puCommondBuf, WLAN_CFG_ARGV_MAX, "%s", CMD_NCHO_DISABLE);
			priv_driver_disable_ncho(prNetDev, puCommondBuf, sizeof(puCommondBuf));
		}

		DBGLOG(INIT, INFO, "NCHO set WES mode cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoWesMode,
				   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set WES mode fail 0x%x\n", rStatus);
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

int priv_driver_get_ncho_wes_mode(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -WLAN_STATUS_FAILURE;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoWesMode,
			   &u4Param,
			   sizeof(UINT_32), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoWesMode fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", u4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}
	DBGLOG(REQ, TRACE, "NCHO get result is %s\n", pcCommand);
	return i4BytesWritten;
}

int priv_driver_set_ncho_band(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set band cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoBand,
				   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set band fail 0x%x\n", rStatus);
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
int priv_driver_get_ncho_band(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoBand,
			   &u4Param,
			   sizeof(UINT_32), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoBand fail 0x%x\n", rStatus);
	} else {
		DBGLOG(REQ, TRACE, "NCHO query ok and ret is %d\n", u4Param);
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}
	return i4BytesWritten;
}

int priv_driver_set_ncho_dfs_scn_mode(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	UINT_32 u4SetInfoLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4Ret = -1;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4Ret;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4Ret = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4Ret) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4Ret);
			return -1;
		}

		DBGLOG(INIT, TRACE, "NCHO set DFS scan cmd %d\n", u4Param);
		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetNchoDfsScnMode,
				   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "NCHO set DFS scan fail 0x%x\n", rStatus);
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
priv_driver_get_ncho_dfs_scn_mode(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO Error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoDfsScnMode,
			   &cmdV1Header,
			   sizeof(struct _CMD_HEADER_T), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoDfsScnMode fail 0x%x\n", rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);
	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n", i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}

	return i4BytesWritten;
}

int
priv_driver_enable_ncho(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4Param = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	INT_32 i4BytesWritten = -1;
	UINT_32 u4SetInfoLen = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (rStatus == WLAN_STATUS_SUCCESS && i4Argc >= 2) {
		DBGLOG(REQ, TRACE, "NCHO argc is %i, %s\n", i4Argc, apcArgv[1]);
		i4BytesWritten = kalkStrtou32(apcArgv[1], 0, &u4Param);
		if (i4BytesWritten) {
			DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d\n", i4BytesWritten);
			i4BytesWritten = -1;
		} else {
			DBGLOG(INIT, TRACE, "NCHO set enable cmd %d\n", u4Param);
			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetNchoEnable,
					   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);

			if (rStatus != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR, "NCHO set enable fail 0x%x\n", rStatus);
				i4BytesWritten = -1;
			} else {
				DBGLOG(INIT, TRACE, "NCHO set enable successed\n");
				i4BytesWritten = 0;
			}
		}
	} else {
		DBGLOG(REQ, ERROR, "NCHO set failed\n");
	}
	return i4BytesWritten;
}

int
priv_driver_disable_ncho(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = -1;
	UINT_32 u4Param = 0;
	UINT_32 u4BufLen = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	struct _CMD_HEADER_T cmdV1Header;

	DBGLOG(INIT, TRACE, "NCHO command is %s\n", pcCommand);
	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return i4BytesWritten;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	if (rStatus != WLAN_STATUS_SUCCESS || i4Argc >= 2) {
		DBGLOG(REQ, ERROR, "NCHO error input parameter %d\n", i4Argc);
		return i4BytesWritten;
	}
	/*<1> Set NCHO Disable to FW*/
	u4Param = FALSE;
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetNchoEnable,
			   &u4Param, sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidSetNchoEnable :%d fail 0x%x\n", u4Param, rStatus);
		return i4BytesWritten;
	}

	/*<2> Query NCHOEnable Satus*/
	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryNchoEnable,
			   &cmdV1Header,
			   sizeof(cmdV1Header), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, "NCHO wlanoidQueryNchoEnable fail 0x%x\n", rStatus);
		return i4BytesWritten;
	}

	DBGLOG(REQ, TRACE, "NCHO query ok and ret is %s\n", cmdV1Header.buffer);


	i4BytesWritten = kalkStrtou32(cmdV1Header.buffer, 0, &u4Param);
	if (i4BytesWritten) {
		DBGLOG(REQ, ERROR, "NCHO parse u4Param error %d!\n", i4BytesWritten);
		i4BytesWritten = -1;
	} else {
		i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%u", u4Param);
	}

	return i4BytesWritten;
}
/*Check NCHO is enable or not.*/
BOOLEAN
priv_driver_auto_enable_ncho(IN struct net_device *prNetDev)
{
	UINT_8 puCommondBuf[WLAN_CFG_ARGV_MAX];
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	ASSERT(prGlueInfo);

	kalSnprintf(puCommondBuf, WLAN_CFG_ARGV_MAX, "%s %d", CMD_NCHO_ENABLE, 1);
#if CFG_SUPPORT_NCHO_AUTO_ENABLE
	if (prGlueInfo->prAdapter->rNchoInfo.fgECHOEnabled == FALSE) {
		DBGLOG(INIT, INFO, "NCHO is unavailable now! Start to NCHO Enable CMD\n");
		priv_driver_enable_ncho(prNetDev, puCommondBuf, sizeof(puCommondBuf));

	}
#endif
	return TRUE;
}

#endif

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
static UINT_8 aucOidBuf[CMD_OID_BUF_LENGTH] = { 0 };

/* OID processing table */
/*
 * Order is important here because the OIDs should be in order of
 * increasing value for binary searching.
 */
static WLAN_REQ_ENTRY arWlanOidReqTable[] = {
	/*
	 * {(NDIS_OID)rOid,
	 * (PUINT_8)pucOidName,
	 * fgQryBufLenChecking, fgSetBufLenChecking, fgIsHandleInGlueLayerOnly, u4InfoBufLen,
	 * pfOidQueryHandler,
	 * pfOidSetHandler}
	 */
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
	 * {OID_802_11_CONFIGURATION,
	 * DISP_STRING("OID_802_11_CONFIGURATION"),
	 * TRUE, TRUE, ENUM_OID_GLUE_EXTENSION, sizeof(PARAM_802_11_CONFIG_T),
	 * (PFN_OID_HANDLER_FUNC_REQ)reqExtQueryConfiguration,
	 * (PFN_OID_HANDLER_FUNC_REQ)reqExtSetConfiguration},
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

	/*
	 * #if PTA_ENABLED
	 * {OID_CUSTOM_BT_COEXIST_CTRL,
	 * DISP_STRING("OID_CUSTOM_BT_COEXIST_CTRL"),
	 * FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_BT_COEXIST_T),
	 * NULL,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetBtCoexistCtrl},
	 * #endif
	 */

	/*
	 * {OID_CUSTOM_POWER_MANAGEMENT_PROFILE,
	 * DISP_STRING("OID_CUSTOM_POWER_MANAGEMENT_PROFILE"),
	 * FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryPwrMgmtProfParam,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetPwrMgmtProfParam},
	 * {OID_CUSTOM_PATTERN_CONFIG,
	 * DISP_STRING("OID_CUSTOM_PATTERN_CONFIG"),
	 * TRUE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_PATTERN_SEARCH_CONFIG_STRUCT_T),
	 * NULL,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetPatternConfig},
	 * {OID_CUSTOM_BG_SSID_SEARCH_CONFIG,
	 * DISP_STRING("OID_CUSTOM_BG_SSID_SEARCH_CONFIG"),
	 * FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 * NULL,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetBgSsidParam},
	 * {OID_CUSTOM_VOIP_SETUP,
	 * DISP_STRING("OID_CUSTOM_VOIP_SETUP"),
	 * TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryVoipConnectionStatus,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetVoipConnectionStatus},
	 * {OID_CUSTOM_ADD_TS,
	 * DISP_STRING("OID_CUSTOM_ADD_TS"),
	 * TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	 * NULL,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidAddTS},
	 * {OID_CUSTOM_DEL_TS,
	 * DISP_STRING("OID_CUSTOM_DEL_TS"),
	 * TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	 * NULL,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidDelTS},
	 */

	/*
	 * #if CFG_LP_PATTERN_SEARCH_SLT
	 * {OID_CUSTOM_SLT,
	 * DISP_STRING("OID_CUSTOM_SLT"),
	 * FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQuerySltResult,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetSltMode},
	 * #endif
	 *
	 * {OID_CUSTOM_ROAMING_EN,
	 * DISP_STRING("OID_CUSTOM_ROAMING_EN"),
	 * TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryRoamingFunction,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetRoamingFunction},
	 * {OID_CUSTOM_WMM_PS_TEST,
	 * DISP_STRING("OID_CUSTOM_WMM_PS_TEST"),
	 * TRUE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	 * NULL,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetWiFiWmmPsTest},
	 * {OID_CUSTOM_COUNTRY_STRING,
	 * DISP_STRING("OID_CUSTOM_COUNTRY_STRING"),
	 * FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryCurrentCountry,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetCurrentCountry},
	 *
	 * #if CFG_SUPPORT_802_11D
	 * {OID_CUSTOM_MULTI_DOMAIN_CAPABILITY,
	 * DISP_STRING("OID_CUSTOM_MULTI_DOMAIN_CAPABILITY"),
	 * FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryMultiDomainCap,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetMultiDomainCap},
	 * #endif
	 *
	 * {OID_CUSTOM_GPIO2_MODE,
	 * DISP_STRING("OID_CUSTOM_GPIO2_MODE"),
	 * FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(ENUM_PARAM_GPIO2_MODE_T),
	 * NULL,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetGPIO2Mode},
	 * {OID_CUSTOM_CONTINUOUS_POLL,
	 * DISP_STRING("OID_CUSTOM_CONTINUOUS_POLL"),
	 * FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CONTINUOUS_POLL_T),
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryContinuousPollInterval,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetContinuousPollProfile},
	 * {OID_CUSTOM_DISABLE_BEACON_DETECTION,
	 * DISP_STRING("OID_CUSTOM_DISABLE_BEACON_DETECTION"),
	 * FALSE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryDisableBeaconDetectionFunc,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetDisableBeaconDetectionFunc},
	 */

	/* WPS */
	/*
	 * {OID_CUSTOM_DISABLE_PRIVACY_CHECK,
	 * DISP_STRING("OID_CUSTOM_DISABLE_PRIVACY_CHECK"),
	 * FALSE, TRUE, ENUM_OID_DRIVER_CORE, 4,
	 * NULL,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetDisablePriavcyCheck},
	 */

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

	/*
	 * {OID_CUSTOM_TEST_RX_STATUS,
	 * DISP_STRING("OID_CUSTOM_TEST_RX_STATUS"),
	 * FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_RFTEST_RX_STATUS_STRUCT_T),
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryRfTestRxStatus,
	 * NULL},
	 * {OID_CUSTOM_TEST_TX_STATUS,
	 * DISP_STRING("OID_CUSTOM_TEST_TX_STATUS"),
	 * FALSE, TRUE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_CUSTOM_RFTEST_TX_STATUS_STRUCT_T),
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryRfTestTxStatus,
	 * NULL},
	 */
	{OID_CUSTOM_ABORT_TEST_MODE,
	 DISP_STRING("OID_CUSTOM_ABORT_TEST_MODE"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, 0,
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetAbortTestMode}
	,
	{OID_CUSTOM_MTK_WIFI_TEST,
	 DISP_STRING("OID_CUSTOM_MTK_WIFI_TEST"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(PARAM_MTK_WIFI_TEST_STRUCT_T),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestQueryAutoTest,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidRftestSetAutoTest}
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

	/*
	 * {OID_CUSTOM_SINGLE_ANTENNA,
	 * DISP_STRING("OID_CUSTOM_SINGLE_ANTENNA"),
	 * FALSE, FALSE, ENUM_OID_DRIVER_CORE, 4,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryBtSingleAntenna,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetBtSingleAntenna},
	 * {OID_CUSTOM_SET_PTA,
	 * DISP_STRING("OID_CUSTOM_SET_PTA"),
	 * FALSE, FALSE, ENUM_OID_DRIVER_CORE, 4,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidQueryPta,
	 * (PFN_OID_HANDLER_FUNC_REQ)wlanoidSetPta},
	 */

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

#if CFG_SUPPORT_GAMING_MODE
	{OID_CUSTOM_GAMING_MODE,
	 DISP_STRING("OID_CUSTOM_GAMING_MODE"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(UINT_32),
	 NULL,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetGamingMode}
	,
#endif

	{OID_IPC_WIFI_LOG_UI,
	 DISP_STRING("OID_IPC_WIFI_LOG_UI"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(struct PARAM_WIFI_LOG_LEVEL_UI),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryWifiLogLevelSupport,
	 NULL}
	,

	{OID_IPC_WIFI_LOG_LEVEL,
	 DISP_STRING("OID_IPC_WIFI_LOG_LEVEL"),
	 FALSE, FALSE, ENUM_OID_DRIVER_CORE, sizeof(struct PARAM_WIFI_LOG_LEVEL),
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidQueryWifiLogLevel,
	 (PFN_OID_HANDLER_FUNC_REQ) wlanoidSetWifiLogLevel}
	,

};

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

	/* prDev is verified in the caller function wlanDoIOCTL() */

	/* Prepare the call */
	rIwReqInfo.cmd = (__u16) i4Cmd;
	rIwReqInfo.flags = 0;

	if ((i4Cmd == IOCTL_SET_STRUCT_FOR_EM) && !capable(CAP_NET_ADMIN))
		return -EPERM;

	switch (i4Cmd) {
	case IOCTL_SET_INT:
		/* NOTE(Kevin): 1/3 INT Type <= IFNAMSIZ, so we don't need copy_from/to_user() */
		return priv_set_int(prNetDev, &rIwReqInfo, &(prIwReq->u), (char *)&(prIwReq->u));

	case IOCTL_GET_INT:
		/* NOTE(Kevin): 1/3 INT Type <= IFNAMSIZ, so we don't need copy_from/to_user() */
		return priv_get_int(prNetDev, &rIwReqInfo, &(prIwReq->u), (char *)&(prIwReq->u));

	case IOCTL_SET_STRUCT:
	case IOCTL_SET_STRUCT_FOR_EM:
		return priv_set_struct(prNetDev, &rIwReqInfo, &prIwReq->u, (char *)&(prIwReq->u));

	case IOCTL_GET_STRUCT:
		return priv_get_struct(prNetDev, &rIwReqInfo, &prIwReq->u, (char *)&(prIwReq->u));

	default:
		return -EOPNOTSUPP;

	}			/* end of switch */

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
	INT_32 nsize = 0, nsize1, nsize2, nsize3, scancount;
	INT_32 i, j, nleft;
	UINT_32 freq;

	P_EVENT_BATCH_RESULT_ENTRY_T prEntry;
	P_EVENT_BATCH_RESULT_T pBr;

	nleft = u4MaxBufferLen - 5;	/* -5 for "----\n" */

	pBr = prEventBatchResult;
	scancount = 0;
	for (j = 0; j < CFG_BATCH_MAX_MSCAN; j++) {
		scancount += pBr->ucScanCount;
		pBr++;
	}

	nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S, "scancount=%x\nnextcount=%x\n", scancount, scancount);
	if (nsize1 < nleft) {
		p += nsize1 = kalSprintf(p, "%s", text1);
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
		nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S, "apcount=%d\n", pBr->ucScanCount);
		if (nsize1 < nleft) {
			p += nsize1 = kalSprintf(p, "%s", text1);
			nleft -= nsize1;
		} else
			goto short_buf;

		for (i = 0; i < pBr->ucScanCount; i++) {
			prEntry = &pBr->arBatchResult[i];

			nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S, "bssid=" MACSTR "\n",
					     prEntry->aucBssid[0],
					     prEntry->aucBssid[1],
					     prEntry->aucBssid[2],
					     prEntry->aucBssid[3],
					     prEntry->aucBssid[4], prEntry->aucBssid[5]);

			kalMemCopy(ssid,
				   prEntry->aucSSID,
				   (prEntry->ucSSIDLen < ELEM_MAX_LEN_SSID ? prEntry->ucSSIDLen : ELEM_MAX_LEN_SSID));
			ssid[(prEntry->ucSSIDLen <
			      (ELEM_MAX_LEN_SSID - 1) ? prEntry->ucSSIDLen : (ELEM_MAX_LEN_SSID - 1))] = '\0';
			nsize2 = kalSnprintf(text2, TMP_TEXT_LEN_L, "ssid=%s\n", ssid);

			freq = batchChannelNum2Freq(prEntry->ucFreq);
			nsize3 =
			    kalSnprintf(text3, TMP_TEXT_LEN_L,
					"freq=%u\nlevel=%d\ndist=%u\ndistSd=%u\n====\n", freq,
					prEntry->cRssi, prEntry->u4Dist, prEntry->u4Distsd);

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
		p += kalSprintf(p, "%s", text1);

		pBr++;
	}

	nsize1 = kalSnprintf(text1, TMP_TEXT_LEN_S, "%s", "----\n");
	kalSprintf(p, "%s", text1);

	*pu4RetLen = u4MaxBufferLen - nleft;
	DBGLOG(SCN, TRACE, "total len = %d (max len = %d)\n", *pu4RetLen, u4MaxBufferLen);

	return WLAN_STATUS_SUCCESS;

short_buf:
	DBGLOG(SCN, TRACE,
	       "Short buffer issue! %d > %d, %s\n", u4MaxBufferLen + (nsize - nleft),
		u4MaxBufferLen, (char *)pvBuffer);
	return WLAN_STATUS_INVALID_LENGTH;
}
#endif

#if CFG_SUPPORT_GET_CH_ENV
WLAN_STATUS
scanEnvResult(P_GLUE_INFO_T prGlueInfo, OUT PVOID pvBuffer, IN UINT_32 u4MaxBufferLen, OUT PUINT_32 pu4RetLen)
{
	P_ADAPTER_T prAdapter = NULL;
	CHAR *p = pvBuffer;
	INT_32 nsize;
	INT_32 i, nleft;
	P_SCAN_INFO_T prScanInfo;
	P_LINK_T prBSSDescList;
	P_BSS_DESC_T prBssDesc;
	CH_ENV_T chEnvInfo[54];	/* 54: from FW define; TODO: sync MAXIMUM_OPERATION_CHANNEL_LIST */
	UINT_32 i4GetCh = 0;
	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	UINT_8 ucTextLen = 40;
	UCHAR text[ucTextLen];
	INT_32 u4Ret;

	prAdapter = prGlueInfo->prAdapter;
	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);
	prBSSDescList = &prScanInfo->rBSSDescList;

	kalMemZero(chEnvInfo, sizeof(chEnvInfo));

	DBGLOG(SCN, TRACE, "pvBuffer:%s, pu4RetLen:%d\n", (char *)pvBuffer, *pu4RetLen);

	wlanCfgParseArgument(pvBuffer, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	if (i4Argc >= 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &i4GetCh);
		if (u4Ret)
			DBGLOG(SCN, TRACE, "parse pvBuffer error u4Ret=%d\n", u4Ret);
		/* i4GetCh = kalStrtoul(apcArgv[1], NULL, 0); */
	}

	nleft = u4MaxBufferLen - 5;	/* -5 for "----\n" */

	nsize = kalSnprintf(text, ucTextLen, "%s", "scanEnvResult\nResult:1\n");/* Always return 1 for alpha version. */

	if (nsize < nleft) {
		p += nsize = kalSnprintf(p, ucTextLen, "%s", text);
		nleft -= nsize;
	} else
		goto short_buf;

	/* Search BSS Desc from current SCAN result list. */
	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry, BSS_DESC_T) {
		if (prBssDesc->ucChannelNum > 0) {
			if (prBssDesc->ucChannelNum <= 14) {	/* 1~14 */
				chEnvInfo[prBssDesc->ucChannelNum - 1].ucChNum = prBssDesc->ucChannelNum;
				chEnvInfo[prBssDesc->ucChannelNum - 1].ucApNum++;
			} else if (prBssDesc->ucChannelNum <= 64) {	/* 15~22 */
				chEnvInfo[prBssDesc->ucChannelNum / 4 + 5].ucChNum = prBssDesc->ucChannelNum;
				chEnvInfo[prBssDesc->ucChannelNum / 4 + 5].ucApNum++;
			} else if (prBssDesc->ucChannelNum <= 116) {	/* 23~27 */
				chEnvInfo[prBssDesc->ucChannelNum / 4 - 3].ucChNum = prBssDesc->ucChannelNum;
				chEnvInfo[prBssDesc->ucChannelNum / 4 - 3].ucApNum++;
			} else if (prBssDesc->ucChannelNum <= 140) {	/* 28~30 */
				chEnvInfo[prBssDesc->ucChannelNum / 4 - 6].ucChNum = prBssDesc->ucChannelNum;
				chEnvInfo[prBssDesc->ucChannelNum / 4 - 6].ucApNum++;
			} else if (prBssDesc->ucChannelNum <= 165) {	/* 31~35 */
				chEnvInfo[(prBssDesc->ucChannelNum - 1) / 4 - 7].ucChNum = prBssDesc->ucChannelNum;
				chEnvInfo[(prBssDesc->ucChannelNum - 1) / 4 - 7].ucApNum++;
			}
		}
	}

	for (i = 0; i < 54; i++) {
		if (chEnvInfo[i].ucChNum != 0) {
			if (i4GetCh == 0 || (chEnvInfo[i].ucChNum == (UINT_8)i4GetCh)) {
				DBGLOG(SCN, TRACE, "chNum=%d,apNum=%d\n", chEnvInfo[i].ucChNum, chEnvInfo[i].ucApNum);
				p += nsize =
				    kalSnprintf(p, ucTextLen, "chNum=%d,apNum=%d\n", chEnvInfo[i].ucChNum,
						chEnvInfo[i].ucApNum);
				nleft -= nsize;
			}
		}
	}

	p += nsize = kalSnprintf(p, ucTextLen, "%s", "----\n");
	nleft -= nsize;

	*pu4RetLen = u4MaxBufferLen - nleft;
	DBGLOG(SCN, TRACE, "total len = %d (max len = %d)\n", *pu4RetLen, u4MaxBufferLen);

	return WLAN_STATUS_SUCCESS;

short_buf:
	DBGLOG(SCN, TRACE, "Short buffer issue! %d > %d, %s\n", u4MaxBufferLen + (nsize - nleft), u4MaxBufferLen, p);
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
static int
_priv_set_int(IN struct net_device *prNetDev,
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
		rStatus = wlanSetInformation(prGlueInfo->prAdapter, wlanoidSetBeaconInterval,
					    (PVOID)&pu4IntBuf[1],/* pu4IntBuf[0] is used as input SubCmd */
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
				     sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4BufLen) == WLAN_STATUS_SUCCESS) {
				if (pu4IntBuf[1] == 1)
					prNetDev->features |= NETIF_F_HW_CSUM;
				else if (pu4IntBuf[1] == 0)
					prNetDev->features &= ~NETIF_F_HW_CSUM;
			}
		}
		break;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	case PRIV_CMD_POWER_MODE:
		kalIoctl(prGlueInfo, wlanoidSet802dot11PowerSaveProfile,
			(PVOID)&pu4IntBuf[1],	/* pu4IntBuf[0] is used as input SubCmd */
			 sizeof(UINT_32), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
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
				 sizeof(PARAM_CUSTOM_WMM_PS_TEST_STRUCT_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
		}
		break;

#if 0
	case PRIV_CMD_ADHOC_MODE:
		rStatus = wlanSetInformation(prGlueInfo->prAdapter, wlanoidSetAdHocMode,
					    (PVOID)&pu4IntBuf[1],	/* pu4IntBuf[0] is used as input SubCmd */
					     sizeof(UINT_32), &u4BufLen);
		break;
#endif

	case PRIV_CUSTOM_BWCS_CMD:

		DBGLOG(REQ, INFO, "pu4IntBuf[1] = %x, size of PTA_IPC_T = %zu.\n",
				   pu4IntBuf[1], sizeof(PARAM_PTA_IPC_T));

		prPtaIpc = (P_PTA_IPC_T) aucOidBuf;
		prPtaIpc->u.aucBTPParams[0] = (UINT_8) (pu4IntBuf[1] >> 24);
		prPtaIpc->u.aucBTPParams[1] = (UINT_8) (pu4IntBuf[1] >> 16);
		prPtaIpc->u.aucBTPParams[2] = (UINT_8) (pu4IntBuf[1] >> 8);
		prPtaIpc->u.aucBTPParams[3] = (UINT_8) (pu4IntBuf[1]);

		DBGLOG(REQ, INFO,
		       "BCM BWCS CMD : BTPParams[0]=%02x, BTPParams[1]=%02x, BTPParams[2]=%02x, BTPParams[3]=%02x.\n",
			prPtaIpc->u.aucBTPParams[0], prPtaIpc->u.aucBTPParams[1], prPtaIpc->u.aucBTPParams[2],
			prPtaIpc->u.aucBTPParams[3]);

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
			DBGLOG(REQ, INFO, "CMD set_band=%u\n", (UINT_32) pu4IntBuf[1]);
		}
		break;

#if CFG_ENABLE_WIFI_DIRECT
	case PRIV_CMD_P2P_MODE:
		{
			/* no use, move to set_p2p_mode_handler() */
			PARAM_CUSTOM_P2P_SET_STRUCT_T p2pmode;

			p2pmode.u4Enable = pu4IntBuf[1];
			p2pmode.u4Mode = pu4IntBuf[2];
			set_p2p_mode_handler(prNetDev, p2pmode);
#if 0
			PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P;
			WLAN_STATUS rWlanStatus = WLAN_STATUS_SUCCESS;
			BOOLEAN fgIsP2PEnding;

			GLUE_SPIN_LOCK_DECLARATION();

			/* avoid remove & p2p off command simultaneously */
			GLUE_ACQUIRE_THE_SPIN_LOCK(&g_p2p_lock);
			fgIsP2PEnding = g_u4P2PEnding;
			g_u4P2POnOffing = 1;
			GLUE_RELEASE_THE_SPIN_LOCK(&g_p2p_lock);

			if (fgIsP2PEnding == 1) {
				/* skip the command if we are removing */
				GLUE_ACQUIRE_THE_SPIN_LOCK(&g_p2p_lock);
				g_u4P2POnOffing = 0;
				GLUE_RELEASE_THE_SPIN_LOCK(&g_p2p_lock);
				break;
			}
			rSetP2P.u4Enable = pu4IntBuf[1];
			rSetP2P.u4Mode = pu4IntBuf[2];

			if (!rSetP2P.u4Enable)
				p2pNetUnregister(prGlueInfo, TRUE);

			/* move out to caller to avoid kalIoctrl & suspend/resume deadlock problem ALPS00844864 */
			/*
			 * Scenario:
			 * 1. System enters suspend/resume but not yet enter wlanearlysuspend()
			 * or wlanlateresume();
			 *
			 * 2. System switches to do PRIV_CMD_P2P_MODE and execute kalIoctl()
			 * and get g_halt_sem then do glRegisterEarlySuspend() or
			 * glUnregisterEarlySuspend();
			 *
			 * But system suspend/resume procedure is not yet finished so we
			 * suspend;
			 *
			 * 3. System switches back to do suspend/resume procedure and execute
			 * kalIoctl(). But driver does not yet release g_halt_sem so system
			 * suspend in wlanearlysuspend() or wlanlateresume();
			 *
			 * ==> deadlock occurs.
			 */
			if ((!rSetP2P.u4Enable) && (g_u4HaltFlag == 0) && (fgIsResetting == FALSE)) {
				/* fgIsP2PRegistered == TRUE means P2P is enabled */
				DBGLOG(P2P, INFO, "p2pEalySuspendReg\n");
				p2pEalySuspendReg(prGlueInfo, rSetP2P.u4Enable);	/* p2p remove */
			}

			DBGLOG(P2P, INFO,
			       "wlanoidSetP2pMode 0x%p %d %d\n", &rSetP2P, rSetP2P.u4Enable, rSetP2P.u4Mode);
			rWlanStatus = kalIoctl(prGlueInfo, wlanoidSetP2pMode,
					      (PVOID)&rSetP2P,	/* pu4IntBuf[0] is used as input SubCmd */
					       sizeof(PARAM_CUSTOM_P2P_SET_STRUCT_T),
					       FALSE, FALSE, TRUE, FALSE, &u4BufLen);
			DBGLOG(P2P, INFO, "wlanoidSetP2pMode ok\n");

			/* move out to caller to avoid kalIoctrl & suspend/resume deadlock problem ALPS00844864 */
			if ((rSetP2P.u4Enable) && (g_u4HaltFlag == 0) && (fgIsResetting == FALSE)) {
				/* fgIsP2PRegistered == TRUE means P2P on successfully */
				p2pEalySuspendReg(prGlueInfo, rSetP2P.u4Enable);	/* p2p on */
			}

			if (rSetP2P.u4Enable)
				p2pNetRegister(prGlueInfo, TRUE);

			GLUE_ACQUIRE_THE_SPIN_LOCK(&g_p2p_lock);
			g_u4P2POnOffing = 0;
			GLUE_RELEASE_THE_SPIN_LOCK(&g_p2p_lock);
#endif
		}
		break;
#endif

#if (CFG_SUPPORT_MET_PROFILING == 1)
	case PRIV_CMD_MET_PROFILING:
		{
			/* PARAM_CUSTOM_WFD_DEBUG_STRUCT_T rWfdDebugModeInfo; */
			/* rWfdDebugModeInfo.ucWFDDebugMode=(UINT_8)pu4IntBuf[1]; */
			/* rWfdDebugModeInfo.u2SNPeriod=(UINT_16)pu4IntBuf[2]; */
			/* DBGLOG(REQ, INFO,("WFD Debug Mode:%d Period:%d\n", */
			/* rWfdDebugModeInfo.ucWFDDebugMode,rWfdDebugModeInfo.u2SNPeriod)); */
			prGlueInfo->u8MetProfEnable = (UINT_8) pu4IntBuf[1];
			prGlueInfo->u16MetUdpPort = (UINT_16) pu4IntBuf[2];
			DBGLOG(REQ, INFO, "MET_PROF: Enable=%d UDP_PORT=%d\n", prGlueInfo->u8MetProfEnable,
			       prGlueInfo->u16MetUdpPort);

		}
		break;

#endif
	case PRIV_CMD_WFD_DEBUG_CODE:
		{
			PARAM_CUSTOM_WFD_DEBUG_STRUCT_T rWfdDebugModeInfo;

			rWfdDebugModeInfo.ucWFDDebugMode = (UINT_8) pu4IntBuf[1];
			rWfdDebugModeInfo.u2SNPeriod = (UINT_16) pu4IntBuf[2];
			DBGLOG(REQ, INFO, "WFD Debug Mode:%d Period:%d\n", rWfdDebugModeInfo.ucWFDDebugMode,
			       rWfdDebugModeInfo.u2SNPeriod);
			kalIoctl(prGlueInfo, wlanoidSetWfdDebugMode, (PVOID)&rWfdDebugModeInfo,
				 sizeof(PARAM_CUSTOM_WFD_DEBUG_STRUCT_T), FALSE, FALSE, TRUE, FALSE, &u4BufLen);

		}
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
UINT_8 gucBufDbgCode[1000];

static int
_priv_get_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	UINT_32 u4SubCmd;
	PUINT_32 pu4IntBuf;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4BufLen = 0;
	int status = 0;
	P_NDIS_TRANSPORT_STRUCT prNdisReq;

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

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
			 * if (copy_to_user(prIwReqData->data.pointer,
			 * &prNdisReq->ndisOidContent[4], 4)) {
			 * printk(KERN_NOTICE "priv_get_int() copy_to_user oidBuf fail(3)\n");
			 * return -EFAULT;
			 * }
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
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

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
		DBGLOG(REQ, INFO, "CMD get_band=\n");
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
			UINT_8 NumOfChannel = 50;
			UINT_8 ucMaxChannelNum = 50;
			INT_32 ch[50];
			/* RF_CHANNEL_INFO_T aucChannelList[50]; */
			P_RF_CHANNEL_INFO_T paucChannelList;
			P_RF_CHANNEL_INFO_T ChannelList_t;

			paucChannelList = kalMemAlloc(sizeof(RF_CHANNEL_INFO_T) * 50, VIR_MEM_TYPE);
			if (paucChannelList == NULL) {
				DBGLOG(REQ, INFO, "alloc ChannelList fail\n");
				return -EFAULT;
			}
			kalMemZero(paucChannelList, sizeof(RF_CHANNEL_INFO_T) * 50);
			kalGetChannelList(prGlueInfo, BAND_NULL, ucMaxChannelNum, &NumOfChannel, paucChannelList);
			if (NumOfChannel > 50) {
				ASSERT(0);
				NumOfChannel = 50;
			}

			ChannelList_t = paucChannelList;
			if (kalIsAPmode(prGlueInfo)) {
				for (i = 0; i < NumOfChannel; i++) {
					if ((ChannelList_t->ucChannelNum <= 13)
						|| (ChannelList_t->ucChannelNum == 36
						|| ChannelList_t->ucChannelNum == 40
						|| ChannelList_t->ucChannelNum == 44
						|| ChannelList_t->ucChannelNum == 48)) {
						ch[j] = (INT_32) ChannelList_t->ucChannelNum;
						ChannelList_t++;
						j++;
					}
				}
			} else {
				for (j = 0; j < NumOfChannel; j++) {
					ch[j] = (INT_32) ChannelList_t->ucChannelNum;
					ChannelList_t++;
				}
			}

			kalMemFree(paucChannelList, VIR_MEM_TYPE, sizeof(RF_CHANNEL_INFO_T) * 50);

			prIwReqData->data.length = j;
			if (copy_to_user(prIwReqData->data.pointer, ch, NumOfChannel * sizeof(INT_32)))
				return -EFAULT;
			else
				return status;
		}

	case PRIV_CMD_GET_BUILD_DATE_CODE:
		{
			UINT_8 aucBuffer[16];

			if (kalIoctl(prGlueInfo,
				     wlanoidQueryBuildDateCode,
				     (PVOID) aucBuffer,
				     sizeof(UINT_8) * 16, TRUE, TRUE, TRUE, FALSE, &u4BufLen) == WLAN_STATUS_SUCCESS) {
				prIwReqData->data.length = sizeof(UINT_8) * 16;

				if (copy_to_user(prIwReqData->data.pointer, aucBuffer, prIwReqData->data.length))
					return -EFAULT;
				else
					return status;
			} else {
				return -EFAULT;
			}
		}

	case PRIV_CMD_GET_DEBUG_CODE:
		{
			wlanQueryDebugCode(prGlueInfo->prAdapter);

			kalMemSet(gucBufDbgCode, '.', sizeof(gucBufDbgCode));
			u4BufLen = prIwReqData->data.length;
			if (u4BufLen > sizeof(gucBufDbgCode))
				u4BufLen = sizeof(gucBufDbgCode);
			if (copy_to_user(prIwReqData->data.pointer, gucBufDbgCode, u4BufLen))
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
static int
_priv_set_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	UINT_32 u4SubCmd, u4BufLen, u4CmdLen;
	P_GLUE_INFO_T prGlueInfo;
	int status = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_SET_TXPWR_CTRL_T prTxpwr;

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);

	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	u4SubCmd = (UINT_32) prIwReqData->data.flags;
	u4CmdLen = (UINT_32) prIwReqData->data.length;

	switch (u4SubCmd) {
	case PRIV_CMD_SET_TX_POWER:
	{
		UINT_16 i, j;
		INT_32  setting[4] = {0};

		if (u4CmdLen > 4)
			return -EINVAL;
		if (copy_from_user(setting, prIwReqData->data.pointer, u4CmdLen))
			return -EFAULT;
#if !(CFG_SUPPORT_TX_POWER_BACK_OFF)
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
					   prTxpwr,
					   sizeof(SET_TXPWR_CTRL_T), TRUE, FALSE, FALSE, FALSE, &u4BufLen);
		} else
			return -EFAULT;
#else

#if 0
		DBGLOG(REQ, INFO, "Tx power num = %d\n", prIwReqData->data.length);

		DBGLOG(REQ, INFO, "Tx power setting = %d %d %d %d\n",
			setting[0], setting[1], setting[2], setting[3]);
#endif
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
				prTxpwr,
				sizeof(SET_TXPWR_CTRL_T), TRUE, FALSE, FALSE, FALSE, &u4BufLen);
		} else if (setting[0] == 5 && prIwReqData->data.length == 4) {
			UINT_8 ch = setting[1];
			UINT_8 modulation = setting[2];
			INT_8 offset = setting[3];
			P_MITIGATED_PWR_BY_CH_BY_MODE pOffsetEntry;

			j = 0;
			do {
				pOffsetEntry = &(prTxpwr->arRlmMitigatedPwrByChByMode[j++]);
				if (ch == 0)
					break;

				if (ch == pOffsetEntry->channel) {
					switch (modulation) {
					case 0:
						pOffsetEntry->mitigatedCckDsss = offset;
						pOffsetEntry->mitigatedOfdm = offset;
						pOffsetEntry->mitigatedHt20 = offset;
						pOffsetEntry->mitigatedHt40 = offset;
					break;
					case 1:
						pOffsetEntry->mitigatedCckDsss = offset;
					break;
					case 2:
						pOffsetEntry->mitigatedOfdm = offset;
					break;
					case 3:
						pOffsetEntry->mitigatedHt20 = offset;
					break;
					case 4:
						pOffsetEntry->mitigatedHt40 = offset;
					break;
					default:
						return -EFAULT;
					}
				}
			} while (j < 40);
		} else
			return -EFAULT;
#endif
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
static int
_priv_get_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	UINT_32 u4SubCmd;
	P_GLUE_INFO_T prGlueInfo;
	int status = 0;
	INT_32 ch[50];

	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	u4SubCmd = (UINT_32) prIwReqData->data.flags;

	switch (u4SubCmd) {
	case PRIV_CMD_GET_CH_LIST:
		{
			UINT_16 i;
			UINT_8 NumOfChannel = 50;
			UINT_8 ucMaxChannelNum = 50;
			/*RF_CHANNEL_INFO_T aucChannelList[50];*/
			P_RF_CHANNEL_INFO_T paucChannelList;
			P_RF_CHANNEL_INFO_T ChannelList_t;

			paucChannelList = kalMemAlloc(sizeof(RF_CHANNEL_INFO_T) * 50, VIR_MEM_TYPE);
			if (paucChannelList == NULL) {
				DBGLOG(REQ, INFO, "alloc fail\n");
				return -EINVAL;
			}
			kalMemZero(paucChannelList, sizeof(RF_CHANNEL_INFO_T) * 50);

			kalGetChannelList(prGlueInfo, BAND_NULL, ucMaxChannelNum, &NumOfChannel, paucChannelList);
			if (NumOfChannel > 50)
				NumOfChannel = 50;

			ChannelList_t = paucChannelList;
			for (i = 0; i < NumOfChannel; i++) {
				ch[i] = (INT_32) ChannelList_t->ucChannelNum;
				ChannelList_t++;
			}

			kalMemFree(paucChannelList, VIR_MEM_TYPE, sizeof(RF_CHANNEL_INFO_T) * 50);
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
static int
_priv_set_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	UINT_32 u4SubCmd = 0;
	int status = 0;
	/* WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS; */
	UINT_32 u4CmdLen = 0;
	P_NDIS_TRANSPORT_STRUCT prNdisReq;
#if CFG_SUPPORT_TX_POWER_BACK_OFF
	P_PARAM_MTK_WIFI_TEST_STRUCT_T prTestStruct;
#endif

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

	u4SubCmd = (UINT_32) prIwReqData->data.flags;

#if 0
	DBGLOG(REQ, INFO, "priv_set_struct(): prIwReqInfo->cmd(0x%X), u4SubCmd(%ld)\n",
		prIwReqInfo->cmd, u4SubCmd);
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
		       "ucCmdLen = %d, size of PTA_IPC_T = %d, prIwReqData->data = 0x%x.\n", u4CmdLen,
			sizeof(PARAM_PTA_IPC_T), prIwReqData->data);

		DBGLOG(REQ, INFO, "priv_set_struct(): prIwReqInfo->cmd(0x%X), u4SubCmd(%u)\n",
				   prIwReqInfo->cmd, u4SubCmd);

		DBGLOG(REQ, INFO, "*pcExtra = 0x%x\n", *pcExtra);
#endif

		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer, u4CmdLen)) {
			status = -EFAULT;	/* return -EFAULT; */
			break;
		}
#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS && CFG_SUPPORT_BCM_BWCS_DEBUG
		DBGLOG(REQ, INFO, "priv_set_struct(): BWCS CMD = %02x%02x%02x%02x\n",
				   aucOidBuf[2], aucOidBuf[3], aucOidBuf[4], aucOidBuf[5]);
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
			/* retrieve IE for Probe Request */
			u4CmdLen = prIwReqData->data.length;
			if (u4CmdLen > GLUE_INFO_WSCIE_LENGTH) {
				DBGLOG(REQ, ERROR, "Input data length is invalid %u\n", u4CmdLen);
				return -EINVAL;
			}

			if (u4CmdLen > 0) {
				if (copy_from_user(prGlueInfo->aucWSCIE, prIwReqData->data.pointer, u4CmdLen)) {
					DBGLOG(REQ, ERROR, "Copy from user failed\n");
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
			DBGLOG(REQ, ERROR, "Input data length is invalid %u\n", u4CmdLen);
			return -EINVAL;
		}

		if (copy_from_user(&aucOidBuf[0], prIwReqData->data.pointer, u4CmdLen)) {
			status = -EFAULT;
			break;
		}
		if (!kalMemCmp(&aucOidBuf[0], pcExtra, u4CmdLen))
			DBGLOG(REQ, INFO, "pcExtra buffer is valid\n");
		else
			DBGLOG(REQ, INFO, "pcExtra 0x%p\n", pcExtra);
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
		u4CmdLen = prIwReqData->data.length;
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		if (u4CmdLen > sizeof(prNdisReq->ndisOidContent)) {
			DBGLOG(REQ, ERROR, "Input data length is invalid %u\n", u4CmdLen);
			return -EINVAL;
		}

		if (copy_from_user(&prNdisReq->ndisOidContent[0], prIwReqData->data.pointer, u4CmdLen)) {
			status = -EFAULT;
			break;
		}
		prNdisReq->ndisOidCmd = OID_CUSTOM_SW_CTRL;
		prNdisReq->inNdisOidlength = 8;
		prNdisReq->outNdisOidLength = 8;

		/* Execute this OID */
		status = priv_set_ndis(prNetDev, prNdisReq, &u4BufLen);
		break;

#if CFG_SUPPORT_TX_POWER_BACK_OFF
	case PRIV_CMD_SET_TX_POWER:
		{
			P_REG_INFO_T prRegInfo = &prGlueInfo->rRegInfo;
			WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
			UINT8 cStartTxBackOff = 0;
			BOOLEAN bFgForceExecution = FALSE;
			/* TxPwrBackOffParam's 0th byte contains enable/disable TxPowerBackOff for 2G */
			/* TxPwrBackOffParam's 1st byte contains default TxPowerBackOff value for 2G */
			/* TxPwrBackOffParam's 2nd byte contains enable/disable TxPowerBackOff for 5G */
			/* TxPwrBackOffParam's 3rd byte contains default TxPowerBackOff value for 5G */

			ULONG TxPwrBackOffParam = 0;

			u4CmdLen = prIwReqData->data.length;
			prTestStruct = (P_PARAM_MTK_WIFI_TEST_STRUCT_T)&aucOidBuf[0];

			if (u4CmdLen > sizeof(aucOidBuf)) {
				DBGLOG(REQ, ERROR, "SET_TX_POWER: Input data length is invalid %u\n", u4CmdLen);
				return -EINVAL;
			}
			if (copy_from_user(prTestStruct, prIwReqData->data.pointer, u4CmdLen)) {
				DBGLOG(REQ, INFO, "SET_TX_POWER: copy from user failed\n");
				return -EFAULT;
			}

			DBGLOG(REQ, INFO, "%s: SET_TX_POWER FuncIndex:%u\n",
				__func__, prTestStruct->u4FuncIndex);
			DBGLOG(REQ, INFO, "FuncData:%u[0x%x], FuncData2:%u[0x%x]\n",
				prTestStruct->u4FuncData,
				prTestStruct->u4FuncData,
				prTestStruct->u4FuncData2,
				prTestStruct->u4FuncData2);
			DBGLOG(REQ, INFO, "Enable2G:%d, MaxPower2G:%u\n",
				prRegInfo->bTxPowerLimitEnable2G,
				prRegInfo->cTxBackOffMaxPower2G);
			DBGLOG(REQ, INFO, "Enable5G:%d, MaxPower5G:%u\n",
				prRegInfo->bTxPowerLimitEnable5G,
				prRegInfo->cTxBackOffMaxPower5G);

			/* u4FuncData: start or stop */
			cStartTxBackOff = prTestStruct->u4FuncData;

			/*
			 * u4FuncData2: used in dynamiclly set back off from ioctl
			 * with a specific power value.
			 * if u4FuncData2 is not 0, driver will force to send cmd to firmware.
			 */
			if (prTestStruct->u4FuncData2 != 0)
				bFgForceExecution = TRUE;

			if ((prRegInfo->bTxPowerLimitEnable2G == TRUE)
				|| (prRegInfo->bTxPowerLimitEnable5G == TRUE)
				|| bFgForceExecution) {
				if (cStartTxBackOff == TRUE) {
					UINT_8 ucTxBackOffMaxPower = prTestStruct->u4FuncData2 * 2;

					if (ucTxBackOffMaxPower != 0) {
						prRegInfo->cTxBackOffMaxPower2G = ucTxBackOffMaxPower;
						prRegInfo->bTxPowerLimitEnable2G = 1;
					}
					TxPwrBackOffParam |= prRegInfo->bTxPowerLimitEnable2G;
					TxPwrBackOffParam |= (prRegInfo->cTxBackOffMaxPower2G) << 8;
					if (ucTxBackOffMaxPower != 0) {
						prRegInfo->cTxBackOffMaxPower5G = ucTxBackOffMaxPower;
						prRegInfo->bTxPowerLimitEnable5G = 1;
					}
					TxPwrBackOffParam |= prRegInfo->bTxPowerLimitEnable5G << 16;
					TxPwrBackOffParam |= (ULONG)(prRegInfo->cTxBackOffMaxPower5G) << 24;
					DBGLOG(REQ, INFO,
					       "Start BackOff: TxPwrBackOffParam=0x%lx\n",
					       TxPwrBackOffParam);
				} else {
					TxPwrBackOffParam = 0; /* First byte is start/stop */
					DBGLOG(REQ, INFO,
					       "Stop BackOff: TxPwrBackOffParam=0x%lx\n",
					       TxPwrBackOffParam);
				}
				rStatus = nicTxPowerBackOff(prGlueInfo->prAdapter, TxPwrBackOffParam);
				if (rStatus == WLAN_STATUS_PENDING)
					status = 0;
				else
					status = -EINVAL;
			}
		}
		break;
#endif

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
static int
_priv_get_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	UINT_32 u4SubCmd = 0;
	P_NDIS_TRANSPORT_STRUCT prNdisReq = NULL;

	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4BufLen = 0;
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
#if 0
	DBGLOG(REQ, INFO, "priv_get_struct(): prIwReqInfo->cmd(0x%X), u4SubCmd(%ld)\n",
	       prIwReqInfo->cmd, u4SubCmd);
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
		DBGLOG(REQ, INFO, "\n priv_get_struct cmd 0x%02x len:%d OID:0x%08x OID Len:%d\n",
			cmd, pIwReq->u.data.length, ndisReq->ndisOidCmd, ndisReq->inNdisOidlength);
#endif
		if (priv_get_ndis(prNetDev, prNdisReq, &u4BufLen) == 0) {
			prNdisReq->outNdisOidLength = u4BufLen;
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
		prNdisReq = (P_NDIS_TRANSPORT_STRUCT) &aucOidBuf[0];

		if (prIwReqData->data.length > sizeof(prNdisReq->ndisOidContent)) {
			DBGLOG(REQ, INFO, "priv_get_struct() exceeds length limit\n");
			return -EFAULT;
		}

		if (copy_from_user(&prNdisReq->ndisOidContent[0],
				   prIwReqData->data.pointer,
				   prIwReqData->data.length)) {
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

			if (copy_to_user(prIwReqData->data.pointer,
					 &prNdisReq->ndisOidContent[4],
					 4 /* OFFSET_OF(NDIS_TRANSPORT_STRUCT, ndisOidContent) */)) {
				DBGLOG(REQ, INFO, "priv_get_struct() copy_to_user oidBuf fail(2)\n");
			}
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
	DBGLOG(REQ, INFO, "priv_set_ndis(): prNdisReq->ndisOidCmd(0x%lX)\n", prNdisReq->ndisOidCmd);
#endif

	if (reqSearchSupportedOidEntry(prNdisReq->ndisOidCmd, &prWlanReqEntry) == FALSE) {
		DBGLOG(REQ, WARN, "Unknown oid: %d\n", prNdisReq->ndisOidCmd);
		/* WARNLOG(("Set OID: 0x%08lx (unknown)\n", prNdisReq->ndisOidCmd)); */
		return -EOPNOTSUPP;
	}

	if (prWlanReqEntry->pfOidSetHandler == NULL) {
		DBGLOG(REQ, WARN, "No oid handler for %d\n", prNdisReq->ndisOidCmd);
		/* WARNLOG(("Set %s: Null set handler\n", prWlanReqEntry->pucOidName)); */
		return -EOPNOTSUPP;
	}
#if 0
	DBGLOG(REQ, INFO, "priv_set_ndis(): %s\n", prWlanReqEntry->pucOidName);
#endif

	if (prWlanReqEntry->fgSetBufLenChecking) {
		if (prNdisReq->inNdisOidlength != prWlanReqEntry->u4InfoBufLen) {
			DBGLOG(REQ, WARN, "Set %s: Invalid length (current=%u, needed=%u)\n",
					   prWlanReqEntry->pucOidName,
					   prNdisReq->inNdisOidlength, prWlanReqEntry->u4InfoBufLen);

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
				  prNdisReq->inNdisOidlength, FALSE, FALSE, TRUE, FALSE, &u4SetInfoLen);
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
	DBGLOG(REQ, INFO, "priv_get_ndis(): prNdisReq->ndisOidCmd(0x%lX)\n", prNdisReq->ndisOidCmd);
#endif

	if (reqSearchSupportedOidEntry(prNdisReq->ndisOidCmd, &prWlanReqEntry) == FALSE) {
		DBGLOG(REQ, WARN, "Unknown oid: %d\n", prNdisReq->ndisOidCmd);
		/* WARNLOG(("Query OID: 0x%08lx (unknown)\n", prNdisReq->ndisOidCmd)); */
		return -EOPNOTSUPP;
	}

	if (prWlanReqEntry->pfOidQueryHandler == NULL) {
		DBGLOG(REQ, WARN, "No oid handler for %d\n", prNdisReq->ndisOidCmd);
		/* WARNLOG(("Query %s: Null query handler\n", prWlanReqEntry->pucOidName)); */
		return -EOPNOTSUPP;
	}
#if 0
	DBGLOG(REQ, INFO, "priv_get_ndis(): %s\n", prWlanReqEntry->pucOidName);
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
				  prNdisReq->ndisOidContent,
				  prNdisReq->inNdisOidlength, TRUE, TRUE, TRUE, FALSE, &u4BufLen);
	} else {
		DBGLOG(REQ, INFO, "priv_get_ndis(): unsupported OID method:0x%x\n", prWlanReqEntry->eOidMethod);
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

/*----------------------------------------------------------------------------*/
/*!
* @brief Parse command value in a string.
*
* @param InStr  Pointer to the string buffer.
* @param OutStr  Pointer to the next command value.
* @param OutLen  Record the resident buffer length.
*
* @retval Command value.
*/
/*----------------------------------------------------------------------------*/
UINT_32 CmdStringDecParse(IN UINT_8 *InStr, OUT UINT_8 **OutStr, OUT UINT_32 *OutLen)
{
	unsigned char Charc, *Buf;
	unsigned int Num;
	int Maxloop;
	int ReadId;
	int TotalLen;

	/* init */
	Num = 0;
	Maxloop = 0;
	ReadId = 0;
	Buf = (unsigned char *)InStr;
	TotalLen = *OutLen;
	*OutStr = Buf;

	/* sanity check */
	if (Buf[0] == 0x00)
		return 0;

	/* check the value is decimal or hex */
	if ((Buf[ReadId] == 'x') || ((Buf[ReadId] == '0') && (Buf[ReadId + 1] == 'x'))) {
		/* skip x or 0x */
		if (Buf[ReadId] == 'x')
			ReadId++;
		else
			ReadId += 2;

		/* translate the hex number */
		while (Maxloop++ < 10) {
			Charc = Buf[ReadId];
			if ((Charc >= 0x30) && (Charc <= 0x39))
				Charc -= 0x30;
			else if ((Charc >= 'a') && (Charc <= 'f'))
				Charc -= 'a';
			else if ((Charc >= 'A') && (Charc <= 'F'))
				Charc -= 'A';
			else
				break;	/* exit the parsing */
			Num = Num * 16 + Charc + 10;
			ReadId++;
			TotalLen--;
		}
	} else {
		/* translate the decimal number */
		while (Maxloop++ < 10) {
			Charc = Buf[ReadId];
			if ((Charc < 0x30) || (Charc > 0x39))
				break;	/* exit the parsing */
			Charc -= 0x30;
			Num = Num * 10 + Charc;
			ReadId++;
			TotalLen--;
		}
	}

	if (Buf[ReadId] == 0x00)
		*OutStr = &Buf[ReadId];
	else
		*OutStr = &Buf[ReadId + 1];	/* skip the character: _ */

	*OutLen = TotalLen - 1;	/* skip the character: _ */
	return Num;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief Parse command MAC address in a string.
*
* @param InStr  Pointer to the string buffer.
* @param OutStr  Pointer to the next command value.
* @param OutLen  Record the resident buffer length.
*
* @retval Command value.
*/
/*----------------------------------------------------------------------------*/
UINT_32 CmdStringMacParse(IN UINT_8 *InStr, OUT UINT_8 **OutStr, OUT UINT_32 *OutLen, OUT UINT_8 *OutMac)
{
	unsigned char Charc, *Buf;
	unsigned int Num;
	int Maxloop;
	int ReadId;
	int TotalLen;

	/* init */
	Num = 0;
	Maxloop = 0;
	ReadId = 0;
	Buf = (unsigned char *)InStr;
	TotalLen = *OutLen;
	*OutStr = Buf;

	/* sanity check */
	if (Buf[0] == 0x00)
		return 0;

	/* parse MAC */
	while (Maxloop < 6) {
		Charc = Buf[ReadId];
		if ((Charc >= 0x30) && (Charc <= 0x39))
			Charc -= 0x30;
		else if ((Charc >= 'a') && (Charc <= 'f'))
			Charc = Charc - 'a' + 10;
		else if ((Charc >= 'A') && (Charc <= 'F'))
			Charc = Charc - 'A' + 10;
		else
			return -1;	/* error, exit the parsing */

		Num = Charc;
		ReadId++;
		TotalLen--;

		Charc = Buf[ReadId];
		if ((Charc >= 0x30) && (Charc <= 0x39))
			Charc -= 0x30;
		else if ((Charc >= 'a') && (Charc <= 'f'))
			Charc = Charc - 'a' + 10;
		else if ((Charc >= 'A') && (Charc <= 'F'))
			Charc = Charc - 'A' + 10;
		else
			return -1;	/* error, exit the parsing */

		Num = Num * 16 + Charc;
		ReadId += 2;	/* skip the character and ':' */
		TotalLen -= 2;

		OutMac[Maxloop] = Num;
		Maxloop++;
	}

	*OutStr = &Buf[ReadId];	/* skip the character: _ */
	*OutLen = TotalLen;	/* skip the character: _ */
	return Num;
}
#if CFG_SUPPORT_NCHO
/* do not include 0x or x, string to Hexadecimal */
UINT_8 CmdString2HexParse(IN UINT_8 *InStr, OUT UINT_8 **OutStr, OUT UINT_8 *OutLen)
{
	unsigned char Charc, *Buf;
	unsigned char ucNum;
	int Maxloop;
	int ReadId;
	int TotalLen;

	/* init */
	ucNum = 0;
	Maxloop = 0;
	ReadId = 0;
	Buf = (unsigned char *)InStr;
	TotalLen = *OutLen;
	*OutStr = Buf;

	/* sanity check */
	if (TotalLen <= 0)
		return 0;
	if (Buf[0] == 0x00)
		return 0;
	{
		while (Maxloop++ < 2) {
			Charc = Buf[ReadId];
			if ((Charc >= 0x30) && (Charc <= 0x39)) {
				Charc -= 0x30;
			} else if ((Charc >= 'a') && (Charc <= 'f')) {
				Charc -= 'a';
				Charc += 10;
			} else if ((Charc >= 'A') && (Charc <= 'F')) {
				Charc -= 'A';
				Charc += 10;
			} else {
				break;	/* exit the parsing */
			}
			ucNum = ucNum * 16 + Charc;
			ReadId++;
			TotalLen--;
		}
	}

	*OutStr = &Buf[ReadId];
	*OutLen = TotalLen;
	return ucNum;
}
#endif

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
_priv_set_string(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	P_GLUE_INFO_T GlueInfo;
	INT_32 status = 0;
	UINT_32 subcmd;
	UINT_8 *pucInBuf = aucOidBuf;
	UINT_32 u4BufLen;

	/* sanity check */
	ASSERT(prNetDev);
	ASSERT(prIwReqInfo);
	ASSERT(prIwReqData);
	ASSERT(pcExtra);


	if (GLUE_CHK_PR3(prNetDev, prIwReqData, pcExtra) == FALSE)
		return -EINVAL;

	u4BufLen = prIwReqData->data.length;
	GlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (u4BufLen > CMD_OID_BUF_LENGTH) {
		DBGLOG(REQ, ERROR, "Input data length is invalid %u\n", u4BufLen);
		return -EINVAL;
	}

	if (copy_from_user(pucInBuf, prIwReqData->data.pointer, u4BufLen))
		return -EFAULT;

	DBGLOG(REQ, INFO, "orig str cmd %s, %u\n", pucInBuf, u4BufLen);

	subcmd = CmdStringDecParse(pucInBuf, &pucInBuf, &u4BufLen);
	DBGLOG(REQ, INFO, "priv_set_string> command = %u\n", (UINT32) subcmd);

	/* handle the command */
	switch (subcmd) {
#if (CFG_SUPPORT_TDLS == 1)
	case PRIV_CMD_OTHER_TDLS:
		TdlsexCmd(GlueInfo, pucInBuf, u4BufLen);
		break;
#endif /* CFG_SUPPORT_TDLS */

#if (CFG_SUPPORT_TXR_ENC == 1)
	case PRIV_CMD_OTHER_TAR:
	{
		rlmCmd(GlueInfo, pucInBuf, u4BufLen);
		break;
	}
#endif /* CFG_SUPPORT_TXR_ENC */
	case PRIV_CMD_OTHER:
	{
		INT_32 i4BytesWritten;

		if (!kalStrniCmp(pucInBuf, "addts", 5) || !kalStrniCmp(pucInBuf, "delts", 5))
			kalIoctl(GlueInfo, wlanoidTspecOperation, (PVOID)pucInBuf,
					 u4BufLen, FALSE, FALSE, FALSE, FALSE, &i4BytesWritten);
		else if (!kalStrniCmp(pucInBuf, "RM-IT", 5))
			kalIoctl(GlueInfo, wlanoidRadioMeasurementIT, (PVOID)(pucInBuf+6),
					 u4BufLen, FALSE, FALSE, FALSE, FALSE, &i4BytesWritten);
		break;
	}
	default:
		break;
	}

	return status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief The routine handles a get operation for a single OID.
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
_priv_get_string(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	UINT_32 u4SubCmd = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	int status = 0;

	if (!prNetDev || !prIwReqData || !pcExtra) {
		DBGLOG(REQ, INFO, "priv_get_struct(): invalid param(0x%p, 0x%p)\n", prNetDev, prIwReqData);
		return -EINVAL;
	}

	u4SubCmd = (UINT_32) prIwReqData->data.flags;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	if (!prGlueInfo) {
		DBGLOG(REQ, INFO, "priv_get_struct(): invalid prGlueInfo(0x%p, 0x%p)\n",
				   prNetDev, *((P_GLUE_INFO_T *) netdev_priv(prNetDev)));
		return -EINVAL;
	}
	if (copy_from_user(pcExtra, prIwReqData->data.pointer, prIwReqData->data.length)) {
		DBGLOG(REQ, INFO, "copy from user failed\n");
		return -EFAULT;
	}
	switch (u4SubCmd) {
	case PRIV_CMD_DUMP_DRIVER:
	{
		INT_32 i4BytesWritten = 0;

		if (!kalStrniCmp(pcExtra, "dumpts", 6))
			kalIoctl(prGlueInfo, wlanoidTspecOperation, (PVOID)pcExtra,
					 512, FALSE, FALSE, FALSE, FALSE, &i4BytesWritten);
		else if (!kalStrniCmp(pcExtra, "dumpuapsd", 9))
			kalIoctl(prGlueInfo, wlanoidDumpUapsdSetting, (PVOID)pcExtra,
					 512, FALSE, FALSE, FALSE, FALSE, &i4BytesWritten);
		prIwReqData->data.length = i4BytesWritten;
		DBGLOG(REQ, INFO, "returned Bytes %d\n", i4BytesWritten);
		break;
	}
	default:
		DBGLOG(REQ, WARN, "Unknown SubCmd %u with param %s\n", u4SubCmd, pcExtra);
		break;
	}
	return status;
}

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

} /* priv_set_driver */

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
	DBGLOG(REQ, INFO, "priv_driver_set_chip_config command is %s\n", pcCommand);
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
		if (u4PrefixLen <= CHIP_CONFIG_RESP_SIZE) {
			kalStrnCpy(rChipConfigInfo.aucCmd, pcCommand + u4PrefixLen,
				   CHIP_CONFIG_RESP_SIZE - u4PrefixLen);

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetChipConfig,
					   &rChipConfigInfo,
					   sizeof(rChipConfigInfo), FALSE, FALSE, TRUE, TRUE, &u4BufLen);
		} else {

			DBGLOG(REQ, INFO, "%s: kalIoctl Command Len > %d\n", __func__, CHIP_CONFIG_RESP_SIZE);
			rStatus = WLAN_STATUS_FAILURE;
		}

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(REQ, INFO, "%s: kalIoctl ret=%d\n", __func__, rStatus);
			i4BytesWritten = -1;
		}
	}

	return i4BytesWritten;

}

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
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX] = { 0 };
	INT_32 u4Ret;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);
	DBGLOG(REQ, LOUD, "argc is %i\n", i4Argc);

	prAdapter = prGlueInfo->prAdapter;
	if (i4Argc >= 2) {
		u4Ret = kalkStrtou32(apcArgv[1], 0, &ucMode); /* ucMode = kalStrtoul(apcArgv[1], NULL, 0); */
		if (u4Ret)
			DBGLOG(REQ, LOUD, "parse pcCommand error u4Ret=%d\n", u4Ret);

		if (g_ucMiracastMode == ucMode)
			;
			/* XXX: continue or skip */

		g_ucMiracastMode = ucMode;
		prMsgWfdCfgUpdate = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_WFD_CONFIG_SETTINGS_CHANGED_T));

		if (prMsgWfdCfgUpdate != NULL) {

			prWfdCfgSettings = &(prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rWfdConfigureSettings);
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
#if CFG_SUPPORT_P2P_ECSA
int priv_driver_set_cs_config(IN struct net_device *prNetDev,
				IN UINT_8 mode,
				IN UINT_8 channel,
				IN UINT_8 op_class,
				IN UINT_8 count,
				IN UINT_8 sco)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_ECSA_CONFIG_STRUCT_T rECSAConfig;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	kalMemZero(&rECSAConfig, sizeof(rECSAConfig));

	rECSAConfig.channel = channel;
	rECSAConfig.count = count;
	rECSAConfig.mode = mode;
	rECSAConfig.op_class = op_class;
	rECSAConfig.sco = sco;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetECSAConfig,
			   &rECSAConfig,
			   sizeof(rECSAConfig), FALSE, FALSE, TRUE, TRUE, &u4BufLen);
	DBGLOG(REQ, INFO, "%s status: %d\n", __func__, rStatus);
	rStatus = p2pUpdateBeaconEcsaIE(prGlueInfo->prAdapter, NETWORK_TYPE_P2P_INDEX);
	DBGLOG(REQ, INFO, "%s update beacon status: %d\n", __func__, rStatus);
	return rStatus;
}

int priv_driver_ecsa(IN struct net_device *prNetDev, IN char *pcCommand, IN int i4TotalLen)
{
	UINT_32 channel;
	UINT_8 op_class;
	UINT_32 bandwidth;
	UINT_32 u4Freq;
	INT_32 sec_channel = 0;
	UINT_8 ucPreferedChnl;
	ENUM_BAND_T eBand;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;

	P_MSG_P2P_ECSA_T prMsgECSA = NULL;
	P_MSG_P2P_ECSA_T prMsgCSA = NULL;

	INT_32 i4Argc = 0;
	PCHAR apcArgv[WLAN_CFG_ARGV_MAX];
	ENUM_CHNL_EXT_T eSco = CHNL_EXT_SCN;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(REQ, INFO, "command is %s\n", pcCommand);
	wlanCfgParseArgument(pcCommand, &i4Argc, apcArgv);

	if (i4Argc != 3) {
		/*
		 * cmd format: P2P_ECSA channel bandwidth
		 * argc should be 3
		 */
		DBGLOG(REQ, WARN, "cmd format invalid. argc: %d\n", i4Argc);
		return -1;
	}
	/*
	 * apcArgv[0] = "P2P_ECSA
	 * apcArgv[1] = channel
	 * apcArgv[2] = bandwidth
	 */
	if (kalkStrtou32(apcArgv[1], 0, &channel) ||
		kalkStrtou32(apcArgv[2], 0, &bandwidth)) {
		DBGLOG(REQ, INFO, "kalkstrtou32 failed\n");
		return -1;
	}
	DBGLOG(REQ, INFO, "ECSA: channel:bandwidth %d:%d\n", channel, bandwidth);
	u4Freq = nicChannelNum2Freq(channel);
	prMsgCSA = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_ECSA_T));

	prMsgECSA = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_ECSA_T));
	if (!prMsgECSA || !prMsgCSA) {
		if (prMsgECSA)
			cnmMemFree(prAdapter, prMsgECSA);
		if (prMsgCSA)
			cnmMemFree(prAdapter, prMsgCSA);

		DBGLOG(REQ, ERROR, "Msg alloc failed\n");
		return -1;
	}

	if (bandwidth == 20) {
		/* no need to get sco */
	} else if (bandwidth == 40) {
		/* need get sco */
		if (cnmPreferredChannel(prAdapter,
					&eBand,
					&ucPreferedChnl,
					&eSco) == FALSE) {
			eSco = rlmDecideSco(prAdapter, channel, channel > 14 ? BAND_2G4 : BAND_5G);
		}

	} else {
		/* failed, we not support 80/160 yet */
		DBGLOG(REQ, ERROR, "band width %d not support\n", bandwidth);
		return -2;
	}

	if (eSco == CHNL_EXT_SCN) {
		DBGLOG(REQ, INFO, "SCO: No Sco\n");
		sec_channel = 0;
	} else if (eSco == CHNL_EXT_SCA) {
		DBGLOG(REQ, INFO, "SCO: above Sco\n");
		sec_channel = 1;
	} else if (eSco == CHNL_EXT_SCB) {
		DBGLOG(REQ, INFO, "SCO: above Sco\n");
		sec_channel = -1;
	}
	rlmFreqToChannelExt(u4Freq / 1000, sec_channel, &op_class, (PUINT_8)&channel);

	prMsgCSA->rMsgHdr.eMsgId = MID_MNY_P2P_CSA;
	prMsgCSA->rP2pECSA.channel = channel;
	prMsgCSA->rP2pECSA.count = 50; /* 50 TBTTs */
	prMsgCSA->rP2pECSA.mode = 0; /* not reserve transimit */
	prMsgCSA->rP2pECSA.op_class = op_class;
	prMsgCSA->rP2pECSA.sco = eSco;

	DBGLOG(REQ, INFO, "freq:channel:mode:count:op_class:sco %d:%d:%d:%d:%d:%d",
			u4Freq,
			channel, prMsgCSA->rP2pECSA.mode,
			prMsgCSA->rP2pECSA.count,
			prMsgCSA->rP2pECSA.op_class,
			prMsgCSA->rP2pECSA.sco);

	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgCSA, MSG_SEND_METHOD_BUF);

	prMsgECSA->rMsgHdr.eMsgId = MID_MNY_P2P_ECSA;
	prMsgECSA->rP2pECSA.channel = channel;
	prMsgECSA->rP2pECSA.count = 50; /* 50 TBTTs */
	prMsgECSA->rP2pECSA.mode = 0; /* not reserve transimit */
	prMsgECSA->rP2pECSA.op_class = op_class;
	prMsgECSA->rP2pECSA.sco = eSco;
	mboxSendMsg(prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgECSA, MSG_SEND_METHOD_BUF);

	priv_driver_set_cs_config(prNetDev, 0, channel, op_class, 50, eSco);
	return 0;
}
#endif

int priv_support_driver_cmd(IN struct net_device *prNetDev, IN OUT struct ifreq *prReq, IN int i4Cmd)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	int ret = 0;
	char *pcCommand = NULL;
	priv_driver_cmd_t *priv_cmd = NULL;
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

	priv_cmd = kzalloc(sizeof(priv_driver_cmd_t), GFP_KERNEL);
	if (!priv_cmd) {
		DBGLOG(REQ, WARN, "%s, alloc mem failed\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(priv_cmd, prReq->ifr_data, sizeof(priv_driver_cmd_t))) {
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

	pcCommand = priv_cmd->buf;

	DBGLOG(REQ, INFO, "%s: driver cmd \"%s\" on %s\n", __func__, pcCommand, prReq->ifr_name);

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

static int priv_driver_get_wifi_type(IN struct net_device *prNetDev,
				     IN char *pcCommand, IN int i4TotalLen)
{
	struct PARAM_GET_WIFI_TYPE rParamGetWifiType;
	P_GLUE_INFO_T prGlueInfo = NULL;
	uint32_t rStatus;
	uint32_t u4BytesWritten = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE) {
		DBGLOG(REQ, ERROR, "GLUE_CHK_PR2 fail\n");
		return -1;
	}

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rParamGetWifiType.prNetDev = prNetDev;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidGetWifiType,
			   (void *)&rParamGetWifiType,
			   sizeof(void *),
			   FALSE,
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

#if CFG_SUPPORT_BATCH_SCAN
#define CMD_BATCH_SET           "WLS_BATCHING SET"
#define CMD_BATCH_GET           "WLS_BATCHING GET"
#define CMD_BATCH_STOP          "WLS_BATCHING STOP"
#endif

#if CFG_SUPPORT_GET_CH_ENV
#define CMD_CH_ENV_GET			"CH_ENV_GET"
#endif

INT_32 priv_driver_cmds(IN struct net_device *prNetDev, IN PCHAR pcCommand, IN INT_32 i4TotalLen)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4BytesWritten = 0;
	INT_32 i4CmdFound = 0;

	if (GLUE_CHK_PR2(prNetDev, pcCommand) == FALSE)
		return -1;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (i4CmdFound == 0) {
		i4CmdFound = 1;

		if (strncasecmp(pcCommand, CMD_MIRACAST, strlen(CMD_MIRACAST)) == 0)
			i4BytesWritten = priv_driver_set_miracast(prNetDev, pcCommand, i4TotalLen);
#if CFG_SUPPORT_P2P_ECSA
		else if (kalStrniCmp(pcCommand, CMD_ECSA, strlen(CMD_ECSA)) == 0)
			i4BytesWritten = priv_driver_ecsa(prNetDev, pcCommand, i4TotalLen);
#endif
#if CFG_SUPPORT_BATCH_SCAN
		else if (strncasecmp(pcCommand, CMD_BATCH_SET, strlen(CMD_BATCH_SET)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidSetBatchScanReq,
				 (PVOID) pcCommand, i4TotalLen, FALSE, FALSE, TRUE, FALSE, &i4BytesWritten);
		} else if (strncasecmp(pcCommand, CMD_BATCH_GET, strlen(CMD_BATCH_GET)) == 0) {
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
					 sizeof(EVENT_BATCH_RESULT_T), TRUE, TRUE, TRUE, FALSE, &u4BufLen);
			}

#if 0
			DBGLOG(SCN, INFO, "Batch Scan Results, scan count = %u\n", g_rEventBatchResult.ucScanCount);
			for (i = 0; i < g_rEventBatchResult.ucScanCount; i++) {
				prEntry = &g_rEventBatchResult.arBatchResult[i];
				DBGLOG(SCN, INFO, "Entry %u\n", i);
				DBGLOG(SCN, INFO, "	 BSSID = %pM\n", prEntry->aucBssid);
				DBGLOG(SCN, INFO, "	 SSID = %s\n", prEntry->aucSSID);
				DBGLOG(SCN, INFO, "	 SSID len = %u\n", prEntry->ucSSIDLen);
				DBGLOG(SCN, INFO, "	 RSSI = %d\n", prEntry->cRssi);
				DBGLOG(SCN, INFO, "	 Freq = %u\n", prEntry->ucFreq);
			}
#endif

			batchConvertResult(&g_rEventBatchResult[0], pcCommand, i4TotalLen, &i4BytesWritten);

			/* Dump for debug */
			/*
			 * print_hex_dump(KERN_INFO, "BATCH", DUMP_PREFIX_ADDRESS, 16, 1, pcCommand,
			 * i4BytesWritten, TRUE);
			 */

		} else if (strncasecmp(pcCommand, CMD_BATCH_STOP, strlen(CMD_BATCH_STOP)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidSetBatchScanReq,
				 (PVOID) pcCommand, i4TotalLen, FALSE, FALSE, TRUE, FALSE, &i4BytesWritten);
		}
#endif
#if CFG_SUPPORT_GET_CH_ENV
		else if (strncasecmp(pcCommand, CMD_CH_ENV_GET, strlen(CMD_CH_ENV_GET)) == 0)
			scanEnvResult(prGlueInfo, pcCommand, i4TotalLen, &i4BytesWritten);
#endif
		else if (kalStrniCmp(pcCommand, CMD_ADD_TS, strlen(CMD_ADD_TS)) == 0 ||
			kalStrniCmp(pcCommand, CMD_DELETE_TS, strlen(CMD_DELETE_TS)) == 0) {
			kalIoctl(prGlueInfo, wlanoidTspecOperation, (PVOID)pcCommand,
					 i4TotalLen, FALSE, FALSE, FALSE, FALSE, &i4BytesWritten);
		}
#if CFG_SUPPORT_FCC_POWER_BACK_OFF
		else if (kalStrniCmp(pcCommand, CMD_SET_FCC_CERT, strlen(CMD_SET_FCC_CERT)) == 0) {
			CMD_FCC_TX_PWR_ADJUST rFccTxPwrAdjust;
			P_FCC_TX_PWR_ADJUST pFccTxPwrAdjust = &prGlueInfo->rRegInfo.rFccTxPwrAdjust;
			WLAN_STATUS rWlanStatus = WLAN_STATUS_FAILURE;

			if (pFccTxPwrAdjust->fgFccTxPwrAdjust == 0)
				DBGLOG(RLM, WARN,
				       "FCC cert control(%d) is disabled in NVRAM\n",
				       pFccTxPwrAdjust->fgFccTxPwrAdjust);
			else {
				pcCommand += (strlen(CMD_SET_FCC_CERT) + 1);
				if (kalStrniCmp(pcCommand, "-1", strlen("-1")) != 0 && *pcCommand != '0')
					DBGLOG(RLM, WARN, "control parameter(%s) is not correct(0 or -1)\n",
					       pcCommand);
				else {
					kalMemSet(&rFccTxPwrAdjust, 0, sizeof(rFccTxPwrAdjust));
#if 0
					rFccTxPwrAdjust.Offset_CCK = 14;	/* drop 7dB */
					rFccTxPwrAdjust.Offset_HT20 = 16;	/* drop 8dB */
					rFccTxPwrAdjust.Offset_HT40 = 14;	/* drop 7dB */
					rFccTxPwrAdjust.Channel_CCK[0] = 12;	/* start channel */
					rFccTxPwrAdjust.Channel_CCK[1] = 13;	/* end channel */
					rFccTxPwrAdjust.Channel_HT20[0] = 12;	/* start channel */
					rFccTxPwrAdjust.Channel_HT20[1] = 13;	/* end channel */
					/* start channel, primiary channel 12, HT40, center channel (10) -2 */
					rFccTxPwrAdjust.Channel_HT40[0] = 8;
					/* end channel, primiary channel 12, HT40,  center channel (11) -2 */
					rFccTxPwrAdjust.Channel_HT40[1] = 9;
					/* set special bandedge*/
					rFccTxPwrAdjust.Channel_Bandedge[0] = 11;
					rFccTxPwrAdjust.Channel_Bandedge[1] = 13;
#else
					kalMemCopy(&rFccTxPwrAdjust, pFccTxPwrAdjust, sizeof(FCC_TX_PWR_ADJUST));
					/* set special channel band edge */
					kalMemCopy(&rFccTxPwrAdjust.Channel_Bandedge,
						&prGlueInfo->rRegInfo.aucChannelBandEdge, sizeof(UINT_8)*2);
#endif
					rFccTxPwrAdjust.fgFccTxPwrAdjust = *pcCommand == '0' ? 1 : 0;

					DBGLOG(RLM, INFO, "FCC Cert Control (%d)\n", rFccTxPwrAdjust.fgFccTxPwrAdjust);

					rWlanStatus = kalIoctl(prGlueInfo,
							       wlanoidSetFccCert,
							       (PVOID)&rFccTxPwrAdjust,
							       sizeof(CMD_FCC_TX_PWR_ADJUST),
							       FALSE,
							       FALSE,
							       TRUE,
							       FALSE,
							       NULL);
					if (rWlanStatus == WLAN_STATUS_SUCCESS)
						i4BytesWritten = i4TotalLen;
				}
			}
		}
#endif
#if 0

		else if (strncasecmp(pcCommand, CMD_RSSI, strlen(CMD_RSSI)) == 0) {
			/* i4BytesWritten = wl_android_get_rssi(net, command, i4TotalLen); */
		} else if (strncasecmp(pcCommand, CMD_LINKSPEED, strlen(CMD_LINKSPEED)) == 0) {
			i4BytesWritten = priv_driver_get_linkspeed(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_PNOSSIDCLR_SET, strlen(CMD_PNOSSIDCLR_SET)) == 0) {
			/* Do nothing */
		} else if (strncasecmp(pcCommand, CMD_PNOSETUP_SET, strlen(CMD_PNOSETUP_SET)) == 0) {
			/* Do nothing */
		} else if (strncasecmp(pcCommand, CMD_PNOENABLE_SET, strlen(CMD_PNOENABLE_SET)) == 0) {
			/* Do nothing */
		} else if (strncasecmp(pcCommand, CMD_SETSUSPENDOPT, strlen(CMD_SETSUSPENDOPT)) == 0) {
			/* i4BytesWritten = wl_android_set_suspendopt(net, pcCommand, i4TotalLen); */
		} else if (strncasecmp(pcCommand, CMD_SETSUSPENDMODE, strlen(CMD_SETSUSPENDMODE)) == 0) {
			i4BytesWritten = priv_driver_set_suspend_mode(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_SETBAND, strlen(CMD_SETBAND)) == 0) {
			i4BytesWritten = priv_driver_set_band(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_GETBAND, strlen(CMD_GETBAND)) == 0) {
			/* i4BytesWritten = wl_android_get_band(net, pcCommand, i4TotalLen); */
		} else if (strncasecmp(pcCommand, CMD_COUNTRY, strlen(CMD_COUNTRY)) == 0) {
			i4BytesWritten = priv_driver_set_country(prNetDev, pcCommand, i4TotalLen);
		}
		/* Mediatek private command  */
		else if (strncasecmp(pcCommand, CMD_SET_SW_CTRL, strlen(CMD_SET_SW_CTRL)) == 0) {
			i4BytesWritten = priv_driver_set_sw_ctrl(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_GET_SW_CTRL, strlen(CMD_GET_SW_CTRL)) == 0) {
			i4BytesWritten = priv_driver_get_sw_ctrl(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_SET_CFG, strlen(CMD_SET_CFG)) == 0) {
			i4BytesWritten = priv_driver_set_cfg(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_GET_CFG, strlen(CMD_GET_CFG)) == 0) {
			i4BytesWritten = priv_driver_get_cfg(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_SET_CHIP, strlen(CMD_SET_CHIP)) == 0) {
			i4BytesWritten = priv_driver_set_chip_config(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_GET_CHIP, strlen(CMD_GET_CHIP)) == 0) {
			i4BytesWritten = priv_driver_get_chip_config(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_SET_DBG_LEVEL, strlen(CMD_SET_DBG_LEVEL)) == 0) {
			i4BytesWritten = priv_driver_set_dbg_level(prNetDev, pcCommand, i4TotalLen);
		} else if (strncasecmp(pcCommand, CMD_GET_DBG_LEVEL, strlen(CMD_GET_DBG_LEVEL)) == 0) {
			i4BytesWritten = priv_driver_get_dbg_level(prNetDev, pcCommand, i4TotalLen);
		}
#if CFG_SUPPORT_BATCH_SCAN
		else if (strncasecmp(pcCommand, CMD_BATCH_SET, strlen(CMD_BATCH_SET)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidSetBatchScanReq,
				 (PVOID) pcCommand, i4TotalLen, FALSE, FALSE, TRUE, &i4BytesWritten);
		} else if (strncasecmp(pcCommand, CMD_BATCH_GET, strlen(CMD_BATCH_GET)) == 0) {
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
				DBGLOG(SCN, INFO, "	 BSSID = %pM\n", prEntry->aucBssid);
				DBGLOG(SCN, INFO, "	 SSID = %s\n", prEntry->aucSSID);
				DBGLOG(SCN, INFO, "	 SSID len = %u\n", prEntry->ucSSIDLen);
				DBGLOG(SCN, INFO, "	 RSSI = %d\n", prEntry->cRssi);
				DBGLOG(SCN, INFO, "	 Freq = %u\n", prEntry->ucFreq);
			}
#endif

			batchConvertResult(&g_rEventBatchResult[0], pcCommand, i4TotalLen, &i4BytesWritten);

			/* Dump for debug */
			/*
			 * print_hex_dump(KERN_INFO, "BATCH", DUMP_PREFIX_ADDRESS, 16, 1, pcCommand, i4BytesWritten,
			 * TRUE);
			 */

		} else if (strncasecmp(pcCommand, CMD_BATCH_STOP, strlen(CMD_BATCH_STOP)) == 0) {
			kalIoctl(prGlueInfo,
				 wlanoidSetBatchScanReq,
				 (PVOID) pcCommand, i4TotalLen, FALSE, FALSE, TRUE, &i4BytesWritten);
		}
#endif

#endif

#if CFG_SUPPORT_NCHO
		else if (kalStrniCmp(pcCommand,
				  CMD_NCHO_ROAM_TRIGGER_SET,
				  strlen(CMD_NCHO_ROAM_TRIGGER_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_roam_trigger(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ROAM_TRIGGER_GET,
				    strlen(CMD_NCHO_ROAM_TRIGGER_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_roam_trigger(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ROAM_DELTA_SET,
				    strlen(CMD_NCHO_ROAM_DELTA_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_roam_delta(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ROAM_DELTA_GET,
				    strlen(CMD_NCHO_ROAM_DELTA_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_roam_delta(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ROAM_SCAN_PERIOD_SET,
				    strlen(CMD_NCHO_ROAM_SCAN_PERIOD_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_roam_scn_period(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ROAM_SCAN_PERIOD_GET,
				    strlen(CMD_NCHO_ROAM_SCAN_PERIOD_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_roam_scn_period(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ROAM_SCAN_CHANNELS_SET,
				    strlen(CMD_NCHO_ROAM_SCAN_CHANNELS_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_roam_scn_chnl(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ROAM_SCAN_CHANNELS_GET,
				    strlen(CMD_NCHO_ROAM_SCAN_CHANNELS_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_roam_scn_chnl(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ROAM_SCAN_CONTROL_SET,
				    strlen(CMD_NCHO_ROAM_SCAN_CONTROL_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_roam_scn_ctrl(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ROAM_SCAN_CONTROL_GET,
				    strlen(CMD_NCHO_ROAM_SCAN_CONTROL_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_roam_scn_ctrl(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_SCAN_CHANNEL_TIME_SET,
				    strlen(CMD_NCHO_SCAN_CHANNEL_TIME_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_scn_chnl_time(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_SCAN_CHANNEL_TIME_GET,
				    strlen(CMD_NCHO_SCAN_CHANNEL_TIME_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_scn_chnl_time(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_SCAN_HOME_TIME_SET,
				    strlen(CMD_NCHO_SCAN_HOME_TIME_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_scn_home_time(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_SCAN_HOME_TIME_GET,
				    strlen(CMD_NCHO_SCAN_HOME_TIME_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_scn_home_time(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_SCAN_HOME_AWAY_TIME_SET,
				    strlen(CMD_NCHO_SCAN_HOME_AWAY_TIME_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_scn_home_away_time(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_SCAN_HOME_AWAY_TIME_GET,
				    strlen(CMD_NCHO_SCAN_HOME_AWAY_TIME_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_scn_home_away_time(prNetDev, pcCommand, i4TotalLen);
		}  else if (kalStrniCmp(pcCommand,
				     CMD_NCHO_SCAN_NPROBES_SET,
				     strlen(CMD_NCHO_SCAN_NPROBES_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_scn_nprobes(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_SCAN_NPROBES_GET,
				    strlen(CMD_NCHO_SCAN_NPROBES_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_scn_nprobes(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_REASSOC_SEND,
				    strlen(CMD_NCHO_REASSOC_SEND)) == 0) {
			i4BytesWritten = priv_driver_send_ncho_reassoc(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ACTION_FRAME_SEND,
				    strlen(CMD_NCHO_ACTION_FRAME_SEND)) == 0) {
			i4BytesWritten = priv_driver_send_ncho_action_frame(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_WES_MODE_SET,
				    strlen(CMD_NCHO_WES_MODE_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_wes_mode(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_WES_MODE_GET, strlen(CMD_NCHO_WES_MODE_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_wes_mode(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_BAND_SET,
				    strlen(CMD_NCHO_BAND_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_band(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_BAND_GET,
				    strlen(CMD_NCHO_BAND_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_band(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_DFS_SCAN_MODE_SET,
				    strlen(CMD_NCHO_DFS_SCAN_MODE_SET)) == 0) {
			i4BytesWritten = priv_driver_set_ncho_dfs_scn_mode(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_DFS_SCAN_MODE_GET,
				    strlen(CMD_NCHO_DFS_SCAN_MODE_GET)) == 0) {
			i4BytesWritten = priv_driver_get_ncho_dfs_scn_mode(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_ENABLE,
				    strlen(CMD_NCHO_ENABLE)) == 0) {
			i4BytesWritten = priv_driver_enable_ncho(prNetDev, pcCommand, i4TotalLen);
		} else if (kalStrniCmp(pcCommand,
				    CMD_NCHO_DISABLE,
				    strlen(CMD_NCHO_DISABLE)) == 0) {
			i4BytesWritten = priv_driver_disable_ncho(prNetDev, pcCommand, i4TotalLen);
		}
#endif
		else if (!strncasecmp(pcCommand, CMD_FW_PARAM, strlen(CMD_FW_PARAM)))
			kalIoctl(prGlueInfo, wlanoidSetFwParam, (PVOID)(pcCommand + 13),
				 i4TotalLen - 13, FALSE, FALSE, FALSE, FALSE, &i4BytesWritten);
		else if (!strncasecmp(pcCommand, CMD_GET_WIFI_TYPE, strlen(CMD_GET_WIFI_TYPE)))
			i4BytesWritten = priv_driver_get_wifi_type(prNetDev, pcCommand, i4TotalLen);
		else
			i4CmdFound = 0;
	}

	/* i4CmdFound */
	if (i4CmdFound == 0)
		DBGLOG(REQ, INFO, "Unknown driver command %s - ignored\n", pcCommand);

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

}

static int compat_priv(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra,
	     int (*priv_func)(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra))
{
	struct iw_point *prIwp;
	int ret = 0;
#ifdef CONFIG_COMPAT
	struct compat_iw_point *iwp_compat = NULL;
	struct iw_point iwp;
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


	ret = priv_func(prNetDev, prIwReqInfo, (union iwreq_data *)prIwp, pcExtra);

#ifdef CONFIG_COMPAT
	if (prIwReqInfo->flags & IW_REQUEST_FLAG_COMPAT) {
		iwp_compat->pointer = ptr_to_compat(iwp.pointer);
		iwp_compat->length = iwp.length;
		iwp_compat->flags = iwp.flags;
	}
#endif
	return ret;
}

int
priv_set_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	return compat_priv(prNetDev, prIwReqInfo, prIwReqData, pcExtra, _priv_set_int);
}

int
priv_get_int(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	return compat_priv(prNetDev, prIwReqInfo, prIwReqData, pcExtra, _priv_get_int);
}

int
priv_set_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	return compat_priv(prNetDev, prIwReqInfo, prIwReqData, pcExtra, _priv_set_ints);
}

int
priv_get_ints(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	return compat_priv(prNetDev, prIwReqInfo, prIwReqData, pcExtra, _priv_get_ints);
}

int
priv_set_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	return compat_priv(prNetDev, prIwReqInfo, prIwReqData, pcExtra, _priv_set_struct);
}

int
priv_get_struct(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN OUT char *pcExtra)
{
	return compat_priv(prNetDev, prIwReqInfo, prIwReqData, pcExtra, _priv_get_struct);
}

int
priv_set_string(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	return _priv_set_string(prNetDev, prIwReqInfo, prIwReqData, pcExtra);
	/*return compat_priv(prNetDev, prIwReqInfo, prIwReqData, pcExtra, _priv_set_string);*/
}

int
priv_get_string(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra)
{
	return _priv_get_string(prNetDev, prIwReqInfo, prIwReqData, pcExtra);
	/*return compat_priv(prNetDev, prIwReqInfo, prIwReqData, pcExtra, _priv_get_string);*/
}

