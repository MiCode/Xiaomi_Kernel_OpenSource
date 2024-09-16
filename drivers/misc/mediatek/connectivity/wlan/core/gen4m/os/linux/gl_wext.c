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
 *	/gl_wext.c#5
 */

/*! \file gl_wext.c
 *    \brief  ioctl() (mostly Linux Wireless Extensions) routines for STA
 *	      driver.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

#include "gl_os.h"

#include "config.h"
#include "wlan_oid.h"

#include "gl_wext.h"
#include "gl_wext_priv.h"

#include "precomp.h"

#if CFG_SUPPORT_WAPI
#include "gl_sec.h"
#endif

/* compatibility to wireless extensions */
#ifdef WIRELESS_EXT

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
const long channel_freq[] = {
	2412, 2417, 2422, 2427, 2432, 2437, 2442,
	2447, 2452, 2457, 2462, 2467, 2472, 2484
};

#define NUM_CHANNELS (ARRAY_SIZE(channel_freq))

#define MAX_SSID_LEN    32
#define COUNTRY_CODE_LEN	10	/* country code length */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
/* NOTE: name in iwpriv_args only have 16 bytes */
static const struct iw_priv_args rIwPrivTable[] = {
	{IOCTL_SET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, ""},
	{IOCTL_GET_INT, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, ""},
	{IOCTL_SET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, ""},
	{IOCTL_GET_INT, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, ""},
	{IOCTL_SET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, ""},

	{
		IOCTL_GET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, ""
	},
	{
		IOCTL_GET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, ""
	},

	{IOCTL_SET_INTS, IW_PRIV_TYPE_INT | 4, 0, ""},
	{IOCTL_GET_INT, 0, IW_PRIV_TYPE_INT | 50, ""},

	/* added for set_oid and get_oid */
	{IOCTL_SET_STRUCT, 256, 0, ""},
	{IOCTL_GET_STRUCT, 0, 256, ""},

	{IOCTL_GET_DRIVER, IW_PRIV_TYPE_CHAR | 2000, IW_PRIV_TYPE_CHAR |
		2000, "driver"},

#if CFG_SUPPORT_QA_TOOL
	/* added for ATE iwpriv Command */
	{IOCTL_IWPRIV_ATE, IW_PRIV_TYPE_CHAR | 2000, 0, ""},
#endif
	{IOC_AP_SET_CFG, IW_PRIV_TYPE_CHAR | 256,
	 IW_PRIV_TYPE_CHAR | 1024, "AP_SET_CFG"},
	{IOC_AP_GET_STA_LIST, IW_PRIV_TYPE_CHAR | 1024,
	 IW_PRIV_TYPE_CHAR | 1024, "AP_GET_STA_LIST"},
	{IOC_AP_SET_MAC_FLTR, IW_PRIV_TYPE_CHAR | 256,
	 IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_FIXED | 1024, "AP_SET_MAC_FLTR"},
	{IOC_AP_STA_DISASSOC, IW_PRIV_TYPE_CHAR | 256,
	 IW_PRIV_TYPE_CHAR | 1024, "AP_STA_DISASSOC"},
	{IOC_AP_SET_NSS, IW_PRIV_TYPE_CHAR | 256,
	 IW_PRIV_TYPE_CHAR | 1024, "AP_SET_NSS"},

	/* sub-ioctl definitions */
#if 0
	{PRIV_CMD_REG_DOMAIN, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, 0, "set_reg_domain"},
	{PRIV_CMD_REG_DOMAIN, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, "get_reg_domain"},
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	{PRIV_CMD_CSUM_OFFLOAD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, 0, "set_tcp_csum"},
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	{PRIV_CMD_POWER_MODE, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, 0, "set_power_mode"},
	{PRIV_CMD_POWER_MODE, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, "get_power_mode"},

	{PRIV_CMD_WMM_PS, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		3, 0, "set_wmm_ps"},

	{PRIV_CMD_TEST_MODE, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, 0, "set_test_mode"},
	{PRIV_CMD_TEST_CMD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		2, 0, "set_test_cmd"},
	{
		PRIV_CMD_TEST_CMD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_test_result"
	},
#if CFG_SUPPORT_PRIV_MCR_RW
	{PRIV_CMD_ACCESS_MCR, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		2, 0, "set_mcr"},
	{
		PRIV_CMD_ACCESS_MCR, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcr"
	},
#endif

#if CFG_SUPPORT_QA_TOOL
	{PRIV_QACMD_SET, IW_PRIV_TYPE_CHAR | 2000, 0, "set"},
#endif

	{PRIV_CMD_SW_CTRL, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		2, 0, "set_sw_ctrl"},
	{
		PRIV_CMD_SW_CTRL, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_sw_ctrl"
	},

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	{PRIV_CUSTOM_BWCS_CMD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, 0, "set_bwcs"},
	/* GET STRUCT sub-ioctls commands */
	{
		PRIV_CUSTOM_BWCS_CMD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bwcs"
	},
#endif

	/* SET STRUCT sub-ioctls commands */
	{PRIV_CMD_OID, 256, 0, "set_oid"},
	/* GET STRUCT sub-ioctls commands */
	{PRIV_CMD_OID, 0, 256, "get_oid"},

	{PRIV_CMD_BAND_CONFIG, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, 0, "set_band"},
	{PRIV_CMD_BAND_CONFIG, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, "get_band"},
	{PRIV_CMD_GET_CH_LIST, 0, IW_PRIV_TYPE_INT | 50, "get_ch_list"},
	{
		PRIV_CMD_DUMP_MEM, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mem"
	},

#if CFG_ENABLE_WIFI_DIRECT
	{PRIV_CMD_P2P_MODE, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		2, 0, "set_p2p_mode"},
#endif
	{PRIV_CMD_MET_PROFILING, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		2, 0, "set_met_prof"},
	{PRIV_CMD_SET_SER, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED |
		1, 0, "set_ser"},

};

static const iw_handler rIwPrivHandler[] = {
	[IOCTL_SET_INT - SIOCIWFIRSTPRIV] = priv_set_int,
	[IOCTL_GET_INT - SIOCIWFIRSTPRIV] = priv_get_int,
	[IOCTL_SET_ADDRESS - SIOCIWFIRSTPRIV] = NULL,
	[IOCTL_GET_ADDRESS - SIOCIWFIRSTPRIV] = NULL,
	[IOCTL_SET_STR - SIOCIWFIRSTPRIV] = NULL,
	[IOCTL_GET_STR - SIOCIWFIRSTPRIV] = NULL,
	[IOCTL_SET_KEY - SIOCIWFIRSTPRIV] = NULL,
	[IOCTL_GET_KEY - SIOCIWFIRSTPRIV] = NULL,
	[IOCTL_SET_STRUCT - SIOCIWFIRSTPRIV] = priv_set_struct,
	[IOCTL_GET_STRUCT - SIOCIWFIRSTPRIV] = priv_get_struct,
	[IOCTL_SET_STRUCT_FOR_EM - SIOCIWFIRSTPRIV] = priv_set_struct,
	[IOCTL_SET_INTS - SIOCIWFIRSTPRIV] = priv_set_ints,
	[IOCTL_GET_INTS - SIOCIWFIRSTPRIV] = priv_get_ints,
	[IOCTL_GET_DRIVER - SIOCIWFIRSTPRIV] = priv_set_driver,
	[IOC_AP_GET_STA_LIST - SIOCIWFIRSTPRIV] = priv_set_ap,
	[IOC_AP_SET_MAC_FLTR - SIOCIWFIRSTPRIV] = priv_set_ap,
	[IOC_AP_SET_CFG - SIOCIWFIRSTPRIV] = priv_set_ap,
	[IOC_AP_STA_DISASSOC - SIOCIWFIRSTPRIV] = priv_set_ap,
	[IOC_AP_SET_NSS - SIOCIWFIRSTPRIV] = priv_set_ap,
#if CFG_SUPPORT_QA_TOOL
	[IOCTL_QA_TOOL_DAEMON - SIOCIWFIRSTPRIV] = priv_qa_agent,
	[IOCTL_IWPRIV_ATE - SIOCIWFIRSTPRIV] = priv_ate_set
#endif
};

/* standard ioctls */
static int std_get_name(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_freq(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_freq(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_mode(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_mode(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_ap(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_ap(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_rate(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_rts(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_rts(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_frag(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_txpow(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_txpow(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_power(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_power(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_range(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_priv(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_priv(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_mlme(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_scan(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_scan(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_essid(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_essid(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_encode(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_get_encode(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_auth(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

#if (WIRELESS_EXT > 17)
static int std_set_genie(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);
#endif

static int std_set_encode_ext(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static int std_set_pmska(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra);

static const iw_handler mtk_std_handler[] = {
	IW_HANDLER(SIOCGIWNAME, std_get_name),    /* factory mode used */
	IW_HANDLER(SIOCSIWFREQ, std_set_freq),
	IW_HANDLER(SIOCGIWFREQ, std_get_freq),    /* factory mode used */
	IW_HANDLER(SIOCSIWMODE, std_set_mode),    /* factory mode used */
	IW_HANDLER(SIOCGIWMODE, std_get_mode),    /* factory mode used */
	IW_HANDLER(SIOCGIWRANGE, std_get_range),  /* factory mode used */
	IW_HANDLER(SIOCSIWPRIV, std_set_priv),
	IW_HANDLER(SIOCGIWPRIV, std_get_priv),
	IW_HANDLER(SIOCSIWAP, std_set_ap),
	IW_HANDLER(SIOCGIWAP, std_get_ap),        /* factory mode used */
	IW_HANDLER(SIOCSIWMLME, std_set_mlme),
	IW_HANDLER(SIOCSIWSCAN, std_set_scan),    /* factory mode used */
	IW_HANDLER(SIOCGIWSCAN, std_get_scan),    /* factory mode used */
	IW_HANDLER(SIOCSIWESSID, std_set_essid),  /* factory mode used */
	IW_HANDLER(SIOCGIWESSID, std_get_essid),  /* factory mode used */
	IW_HANDLER(SIOCGIWRATE, std_get_rate),    /* factory mode used */
	IW_HANDLER(SIOCSIWRTS, std_set_rts),
	IW_HANDLER(SIOCGIWRTS, std_get_rts),      /* factory mode used */
	IW_HANDLER(SIOCGIWFRAG,  std_get_frag),   /* factory mode used */
	IW_HANDLER(SIOCSIWTXPOW, std_set_txpow),
	IW_HANDLER(SIOCGIWTXPOW, std_get_txpow),  /* factory mode used */
	IW_HANDLER(SIOCSIWENCODE, std_set_encode),
	IW_HANDLER(SIOCGIWENCODE, std_get_encode),/* factory mode used */
	IW_HANDLER(SIOCSIWPOWER, std_set_power),
	IW_HANDLER(SIOCGIWPOWER, std_get_power),  /* factory mode used */
	IW_HANDLER(SIOCSIWAUTH,  std_set_auth),
#if (WIRELESS_EXT > 17)
	IW_HANDLER(SIOCSIWGENIE,  std_set_genie),
#endif
	IW_HANDLER(SIOCSIWENCODEEXT, std_set_encode_ext),
	IW_HANDLER(SIOCSIWPMKSA, std_set_pmska),
};

const struct iw_handler_def wext_handler_def = {
	.num_standard = (__u16) sizeof(mtk_std_handler) / sizeof(iw_handler),
#ifdef CONFIG_WEXT_PRIV
	.num_private = (__u16) sizeof(rIwPrivHandler) / sizeof(iw_handler),
	.num_private_args = (__u16) sizeof(rIwPrivTable) /
						sizeof(struct iw_priv_args),
#endif
	.standard = (iw_handler *) mtk_std_handler,
#ifdef CONFIG_WEXT_PRIV
	.private = rIwPrivHandler,
	.private_args = rIwPrivTable,
#endif
	.get_wireless_stats = wext_get_wireless_stats,
};

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static void wext_support_ioctl_SIOCSIWGENIE(
	IN struct net_device  *prDev, IN char *prExtraBuf,
	IN uint32_t u4ExtraSize);

static void
wext_support_ioctl_SIOCSIWPMKSA_Action(IN struct net_device
	       *prDev, IN char *prExtraBuf, IN int ioMode, OUT int *ret);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if 0 /* not in use */
void MAP_CHANNEL_ID_TO_KHZ(uint32_t ch, uint32_t khz)
{
	switch (ch) {
	case 1:
		khz = 2412000;
		break;
	case 2:
		khz = 2417000;
		break;
	case 3:
		khz = 2422000;
		break;
	case 4:
		khz = 2427000;
		break;
	case 5:
		khz = 2432000;
		break;
	case 6:
		khz = 2437000;
		break;
	case 7:
		khz = 2442000;
		break;
	case 8:
		khz = 2447000;
		break;
	case 9:
		khz = 2452000;
		break;
	case 10:
		khz = 2457000;
		break;
	case 11:
		khz = 2462000;
		break;
	case 12:
		khz = 2467000;
		break;
	case 13:
		khz = 2472000;
		break;
	case 14:
		khz = 2484000;
		break;
	case 36:   /* UNII */
		khz = 5180000;
		break;
	case 40:   /* UNII */
		khz = 5200000;
		break;
	case 44:
		khz = 5220000;
		break;
	case 48:
		khz = 5240000;
		break;
	case 52:
		khz = 5260000;
		break;
	case 56:
		khz = 5280000;
		break;
	case 60:
		khz = 5300000;
		break;
	case 64:
		khz = 5320000;
		break;
	case 149:
		khz = 5745000;
		break;
	case 153:
		khz = 5765000;
		break;
	case 157:
		khz = 5785000;
		break;
	case 161:   /* UNII */
		khz = 5805000;
		break;
	case 165:   /* UNII */
		khz = 5825000;
		break;
	case 100:   /* HiperLAN2 */
		khz = 5500000;
		break;
	case 104:   /* HiperLAN2 */
		khz = 5520000;
		break;
	case 108:   /* HiperLAN2 */
		khz = 5540000;
		break;
	case 112:   /* HiperLAN2 */
		khz = 5560000;
		break;
	case 116:   /* HiperLAN2 */
		khz = 5580000;
		break;
	case 120:   /* HiperLAN2 */
		khz = 5600000;
		break;
	case 124:   /* HiperLAN2 */
		khz = 5620000;
		break;
	case 128:   /* HiperLAN2 */
		khz = 5640000;
		break;
	case 132:   /* HiperLAN2 */
		khz = 5660000;
		break;
	case 136:   /* HiperLAN2 */
		khz = 5680000;
		break;
	case 140:   /* HiperLAN2 */
		khz = 5700000;
		break;
	case 34:   /* Japan MMAC */
		khz = 5170000;
		break;
	case 38:  /* Japan MMAC */
		khz = 5190000;
		break;
	case 42:   /* Japan MMAC */
		khz = 5210000;
		break;
	case 46:   /* Japan MMAC */
		khz = 5230000;
		break;
	case 184:   /* Japan */
		khz = 4920000;
		break;
	case 188:   /* Japan */
		khz = 4940000;
		break;
	case 192:   /* Japan */
		khz = 4960000;
		break;
	case 196:   /* Japan */
		khz = 4980000;
		break;
	case 208:   /* Japan, means J08 */
		khz = 5040000;
		break;
	case 212:   /* Japan, means J12 */
		khz = 5060000;
		break;
	case 216:   /* Japan, means J16 */
		khz = 5080000;
		break;
	default:
		khz = 2412000;
		break;
	}
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * \brief Find the desired WPA/RSN Information Element according to
 *        desiredElemID.
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[in] ucDesiredElemId Desired element ID.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t
wextSrchDesiredWPAIE(IN uint8_t *pucIEStart,
		     IN int32_t i4TotalIeLen, IN uint8_t ucDesiredElemId,
		     OUT uint8_t **ppucDesiredIE)
{
	int32_t i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (int32_t) pucIEStart[1] + 2;

		if (pucIEStart[0] == ucDesiredElemId
		    && i4InfoElemLen <= i4TotalIeLen) {
			if (ucDesiredElemId != 0xDD) {
				/* Non 0xDD, OK! */
				*ppucDesiredIE = &pucIEStart[0];
				return TRUE;
			} /* EID == 0xDD, check WPA IE */
			if (pucIEStart[1] >= 4) {
				if (memcmp(&pucIEStart[2], "\x00\x50\xf2\x01",
				    4) == 0) {
					*ppucDesiredIE = &pucIEStart[0];
					return TRUE;
				}
			}	/* check WPA IE length */
			/* check EID == 0xDD */
		}

		/* check desired EID */
		/* Select next information element. */
		i4TotalIeLen -= i4InfoElemLen;
		pucIEStart += i4InfoElemLen;
	}

	return FALSE;
}				/* parseSearchDesiredWPAIE */

#if CFG_SUPPORT_WAPI
/*----------------------------------------------------------------------------*/
/*!
 * \brief Find the desired WAPI Information Element .
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t wextSrchDesiredWAPIIE(IN uint8_t *pucIEStart,
		       IN int32_t i4TotalIeLen, OUT uint8_t **ppucDesiredIE)
{
	int32_t i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (int32_t) pucIEStart[1] + 2;

		if (pucIEStart[0] == ELEM_ID_WAPI
		    && i4InfoElemLen <= i4TotalIeLen) {
			*ppucDesiredIE = &pucIEStart[0];
			return TRUE;
		}

		/* check desired EID */
		/* Select next information element. */
		i4TotalIeLen -= i4InfoElemLen;
		pucIEStart += i4InfoElemLen;
	}

	return FALSE;
}				/* wextSrchDesiredWAPIIE */
#endif

#if CFG_SUPPORT_PASSPOINT
/*----------------------------------------------------------------------------*/
/*!
 * \brief Check if exist the desired HS2.0 Information Element according to
 *	  desiredElemID.
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[in] ucDesiredElemId Desired element ID.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t wextIsDesiredHS20IE(IN uint8_t *pucCurIE,
			     IN int32_t i4TotalIeLen)
{
	int32_t i4InfoElemLen;

	ASSERT(pucCurIE);

	i4InfoElemLen = (int32_t) pucCurIE[1] + 2;

	if (pucCurIE[0] == ELEM_ID_VENDOR
	    && i4InfoElemLen <= i4TotalIeLen) {
		if (pucCurIE[1] >= ELEM_MIN_LEN_HS20_INDICATION) {
			if (memcmp(&pucCurIE[2], "\x50\x6f\x9a\x10", 4) == 0)
				return TRUE;
		}
	}
	/* check desired EID */
	return FALSE;
}				/* wextIsDesiredHS20IE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Check if exist the desired interworking Information Element according
 *	  to desiredElemID.
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[in] ucDesiredElemId Desired element ID.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t wextIsDesiredInterworkingIE(IN uint8_t *pucCurIE,
				     IN int32_t i4TotalIeLen)
{
	int32_t i4InfoElemLen;

	ASSERT(pucCurIE);

	i4InfoElemLen = (int32_t) pucCurIE[1] + 2;

	if (pucCurIE[0] == ELEM_ID_INTERWORKING
	    && i4InfoElemLen <= i4TotalIeLen) {
		switch (pucCurIE[1]) {
		case IW_IE_LENGTH_ANO:
		case IW_IE_LENGTH_ANO_HESSID:
		case IW_IE_LENGTH_ANO_VENUE:
		case IW_IE_LENGTH_ANO_VENUE_HESSID:
			return TRUE;
		default:
			break;
		}

	}
	/* check desired EID */
	return FALSE;
}				/* wextIsDesiredInterworkingIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Check if exist the desired Adv Protocol Information Element according
 *	  to desiredElemID.
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[in] ucDesiredElemId Desired element ID.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t wextIsDesiredAdvProtocolIE(IN uint8_t *pucCurIE,
				    IN int32_t i4TotalIeLen)
{
	int32_t i4InfoElemLen;

	ASSERT(pucCurIE);

	i4InfoElemLen = (int32_t) pucCurIE[1] + 2;

	if (pucCurIE[0] == ELEM_ID_ADVERTISEMENT_PROTOCOL
	    && i4InfoElemLen <= i4TotalIeLen)
		return TRUE;
	/* check desired EID */
	return FALSE;
}				/* wextIsDesiredAdvProtocolIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Check if exist the desired Roaming Consortium Information Element
 *        according to desiredElemID.
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[in] ucDesiredElemId Desired element ID.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t wextIsDesiredRoamingConsortiumIE(
	IN uint8_t *pucCurIE, IN int32_t i4TotalIeLen)
{
	int32_t i4InfoElemLen;

	ASSERT(pucCurIE);

	i4InfoElemLen = (int32_t) pucCurIE[1] + 2;

	if (pucCurIE[0] == ELEM_ID_ROAMING_CONSORTIUM
	    && i4InfoElemLen <= i4TotalIeLen)
		return TRUE;
	/* check desired EID */
	return FALSE;
}				/* wextIsDesiredRoamingConsortiumIE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Find the desired HS2.0 Information Element according to desiredElemID.
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[in] ucDesiredElemId Desired element ID.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t wextSrchDesiredHS20IE(IN uint8_t *pucIEStart,
		       IN int32_t i4TotalIeLen, OUT uint8_t **ppucDesiredIE)
{
	int32_t i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (int32_t) pucIEStart[1] + 2;

		if (pucIEStart[0] == ELEM_ID_VENDOR
		    && i4InfoElemLen <= i4TotalIeLen) {
			if (pucIEStart[1] >= ELEM_MIN_LEN_HS20_INDICATION) {
				if (memcmp(&pucIEStart[2], "\x50\x6f\x9a\x10",
				    4) == 0) {
					*ppucDesiredIE = &pucIEStart[0];
					return TRUE;
				}
			}
		}

		/* check desired EID */
		/* Select next information element. */
		i4TotalIeLen -= i4InfoElemLen;
		pucIEStart += i4InfoElemLen;
	}

	return FALSE;
}				/* wextSrchDesiredHS20IE */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Find the desired HS2.0 Information Element according to desiredElemID.
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[in] ucDesiredElemId Desired element ID.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t wextSrchDesiredOsenIE(IN uint8_t *pucIEStart,
		       IN int32_t i4TotalIeLen, OUT uint8_t **ppucDesiredIE)
{
	int32_t i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (int32_t) pucIEStart[1] + 2;
		if (pucIEStart[0] == ELEM_ID_VENDOR
		    && i4InfoElemLen <= i4TotalIeLen) {
			if (pucIEStart[1] >= 4) {
				if (memcmp(&pucIEStart[2], "\x50\x6f\x9a\x12",
				    4) == 0) {
					*ppucDesiredIE = &pucIEStart[0];
					return TRUE;
				}
			}
		}

		/* check desired EID */
		/* Select next information element. */
		i4TotalIeLen -= i4InfoElemLen;
		pucIEStart += i4InfoElemLen;
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Find the desired Adv Protocol Information Element according to
 *	  desiredElemID.
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[in] ucDesiredElemId Desired element ID.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t wextSrchDesiredAdvProtocolIE(IN uint8_t
				      *pucIEStart, IN int32_t i4TotalIeLen,
				      OUT uint8_t **ppucDesiredIE)
{
	int32_t i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (int32_t) pucIEStart[1] + 2;

		if (pucIEStart[0] == ELEM_ID_ADVERTISEMENT_PROTOCOL
		    && i4InfoElemLen <= i4TotalIeLen) {
			*ppucDesiredIE = &pucIEStart[0];
			return TRUE;
		}

		/* check desired EID */
		/* Select next information element. */
		i4TotalIeLen -= i4InfoElemLen;
		pucIEStart += i4InfoElemLen;
	}

	return FALSE;
}				/* wextSrchDesiredAdvProtocolIE */

#endif /* CFG_SUPPORT_PASSPOINT */

#if CFG_SUPPORT_WPS
/*----------------------------------------------------------------------------*/
/*!
 * \brief Find the desired WPS Information Element according to desiredElemID.
 *
 * \param[in] pucIEStart IE starting address.
 * \param[in] i4TotalIeLen Total length of all the IE.
 * \param[in] ucDesiredElemId Desired element ID.
 * \param[out] ppucDesiredIE Pointer to the desired IE.
 *
 * \retval TRUE Find the desired IE.
 * \retval FALSE Desired IE not found.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
u_int8_t
wextSrchDesiredWPSIE(IN uint8_t *pucIEStart,
		     IN int32_t i4TotalIeLen, IN uint8_t ucDesiredElemId,
		     OUT uint8_t **ppucDesiredIE)
{
	int32_t i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (int32_t) pucIEStart[1] + 2;

		if (pucIEStart[0] == ucDesiredElemId
		    && i4InfoElemLen <= i4TotalIeLen) {
			if (ucDesiredElemId != 0xDD) {
				/* Non 0xDD, OK! */
				*ppucDesiredIE = &pucIEStart[0];
				return TRUE;
			}
			/* EID == 0xDD, check WPS IE */
			if (pucIEStart[1] >= 4) {
				if (memcmp(&pucIEStart[2], "\x00\x50\xf2\x04",
				    4) == 0) {
					*ppucDesiredIE = &pucIEStart[0];
					return TRUE;
				}
			}	/* check WPS IE length */
			/* check EID == 0xDD */
		}

		/* check desired EID */
		/* Select next information element. */
		i4TotalIeLen -= i4InfoElemLen;
		pucIEStart += i4InfoElemLen;
	}

	return FALSE;
}				/* parseSearchDesiredWPSIE */
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief Get the name of the protocol used on the air.
 *
 * \param[in]  prDev Net device requested.
 * \param[in]  prIwrInfo NULL.
 * \param[out] pcName Buffer to store protocol name string
 * \param[in]  pcExtra NULL.
 *
 * \retval 0 For success.
 *
 * \note If netif_carrier_ok, protocol name is returned;
 *       otherwise, "disconnected" is returned.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_name(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwrInfo,
	      OUT char *pcName, IN uint32_t pcNameSize, IN char *pcExtra)
{
	enum ENUM_PARAM_NETWORK_TYPE eNetWorkType;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(pcName);
	if (GLUE_CHK_PR2(prNetDev, pcName) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (netif_carrier_ok(prNetDev)) {

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryNetworkTypeInUse,
				   &eNetWorkType, sizeof(eNetWorkType),
				   TRUE, FALSE, FALSE, &u4BufLen);

		switch (eNetWorkType) {
		case PARAM_NETWORK_TYPE_DS:
			strncpy(pcName, "IEEE 802.11b", pcNameSize);
			break;
		case PARAM_NETWORK_TYPE_OFDM24:
			strncpy(pcName, "IEEE 802.11bgn", pcNameSize);
			break;
		case PARAM_NETWORK_TYPE_AUTOMODE:
		case PARAM_NETWORK_TYPE_OFDM5:
			strncpy(pcName, "IEEE 802.11abgn", pcNameSize);
			break;
		case PARAM_NETWORK_TYPE_FH:
		default:
			strncpy(pcName, "IEEE 802.11", pcNameSize);
			break;
		}
	} else {
		strncpy(pcName, "Disconnected", pcNameSize);
	}

	pcName[pcNameSize - 1] = '\0';

	return 0;
}				/* wext_get_name */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set the operating channel in the wireless device.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL
 * \param[in] prFreq Buffer to store frequency information
 * \param[in] pcExtra NULL
 *
 * \retval 0 For success.
 * \retval -EOPNOTSUPP If infrastructure mode is not NET NET_TYPE_IBSS.
 * \retval -EINVAL Invalid channel frequency.
 *
 * \note If infrastructure mode is IBSS, new channel frequency is set to device.
 *      The range of channel number depends on different regulatory domain.
 */
/*----------------------------------------------------------------------------*/
static int
wext_set_freq(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo,
	      IN struct iw_freq *prIwFreq, IN char *pcExtra)
{

#if 0
	uint32_t u4ChnlFreq;	/* Store channel or frequency information */

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prIwFreq);
	if (GLUE_CHK_PR2(prNetDev, prIwFreq) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	/*
	 *  printk("set m:%d, e:%d, i:%d, flags:%d\n",
	 *  prIwFreq->m, prIwFreq->e, prIwFreq->i, prIwFreq->flags);
	 */

	/* If setting by frequency, convert to a channel */
	if ((prIwFreq->e == 1) && (prIwFreq->m >= 2.412e8)
	    && (prIwFreq->m <= 2.484e8)) {

		/* Change to KHz format */
		u4ChnlFreq = (uint32_t) (prIwFreq->m / (KILO / 10));

		rStatus = kalIoctl(prGlueInfo, wlanoidSetFrequency, &u4ChnlFreq,
				   sizeof(u4ChnlFreq),
				   FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EINVAL;
	}
	/* Setting by channel number */
	else if ((prIwFreq->m > KILO) || (prIwFreq->e > 0))
		return -EOPNOTSUPP;
	/* Change to channel number format */
	u4ChnlFreq = (uint32_t) prIwFreq->m;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetChannel, &u4ChnlFreq,
			   sizeof(u4ChnlFreq), FALSE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EINVAL;

#endif

	return 0;

}				/* wext_set_freq */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get the operating channel in the wireless device.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[out] prFreq Buffer to store frequency information.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 If netif_carrier_ok.
 * \retval -ENOTCONN Otherwise
 *
 * \note If netif_carrier_ok, channel frequency information is stored in pFreq.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_freq(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwrInfo,
	      OUT struct iw_freq *prIwFreq, IN char *pcExtra)
{
	uint32_t u4Channel = 0;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prIwFreq);
	if (GLUE_CHK_PR2(prNetDev, prIwFreq) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	/* GeorgeKuo: TODO skip checking in IBSS mode */
	if (!netif_carrier_ok(prNetDev))
		return -ENOTCONN;

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryFrequency, &u4Channel,
			   sizeof(u4Channel), TRUE, FALSE, FALSE, &u4BufLen);

	prIwFreq->m = (int)u4Channel;	/* freq in KHz */
	prIwFreq->e = 3;

	return 0;

}				/* wext_get_freq */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set operating mode.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] pu4Mode Pointer to new operation mode.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 For success.
 * \retval -EOPNOTSUPP If new mode is not supported.
 *
 * \note Device will run in new operation mode if it is valid.
 */
/*----------------------------------------------------------------------------*/
static int
wext_set_mode(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo,
	      IN unsigned int *pu4Mode, IN char *pcExtra)
{
	struct PARAM_OP_MODE rOpMode;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct GL_WPA_INFO *prWpaInfo;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	ASSERT(prNetDev);
	ASSERT(pu4Mode);
	if (GLUE_CHK_PR2(prNetDev, pu4Mode) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	switch (*pu4Mode) {
	case IW_MODE_AUTO:
		rOpMode.eOpMode = NET_TYPE_AUTO_SWITCH;
		break;

	case IW_MODE_ADHOC:
		rOpMode.eOpMode = NET_TYPE_IBSS;
		break;

	case IW_MODE_INFRA:
		rOpMode.eOpMode = NET_TYPE_INFRA;
		break;

	default:
		DBGLOG(INIT, INFO, "%s(): Set UNSUPPORTED Mode = %d.\n",
		       __func__, *pu4Mode);
		return -EOPNOTSUPP;
	}

	/* printk("%s(): Set Mode = %d\n", __FUNCTION__, *pu4Mode); */
	rOpMode.ucBssIdx = ucBssIndex;
	rStatus = kalIoctl(prGlueInfo, wlanoidSetInfrastructureMode,
		(void *)&rOpMode, sizeof(struct PARAM_OP_MODE),
		FALSE, FALSE, TRUE, &u4BufLen);

	/* after set operation mode, key table are cleared */

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	/* reset wpa info */
	prWpaInfo->u4WpaVersion =
		IW_AUTH_WPA_VERSION_DISABLED;
	prWpaInfo->u4KeyMgmt = 0;
	prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_802_11W
	prWpaInfo->u4Mfp = IW_AUTH_MFP_DISABLED;
#endif

	return 0;
}				/* wext_set_mode */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get operating mode.
 *
 * \param[in] prNetDev Net device requested.
 * \param[in] prIwReqInfo NULL.
 * \param[out] pu4Mode Buffer to store operating mode information.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 If data is valid.
 * \retval -EINVAL Otherwise.
 *
 * \note If netif_carrier_ok, operating mode information is stored in pu4Mode.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_mode(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo,
	      OUT unsigned int *pu4Mode, IN char *pcExtra)
{
	enum ENUM_PARAM_OP_MODE eOpMode;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(pu4Mode);
	if (GLUE_CHK_PR2(prNetDev, pu4Mode) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryInfrastructureMode, &eOpMode,
			   sizeof(eOpMode), TRUE, FALSE, FALSE, &u4BufLen);

	switch (eOpMode) {
	case NET_TYPE_IBSS:
		*pu4Mode = IW_MODE_ADHOC;
		break;

	case NET_TYPE_INFRA:
		*pu4Mode = IW_MODE_INFRA;
		break;

	case NET_TYPE_AUTO_SWITCH:
		*pu4Mode = IW_MODE_AUTO;
		break;

	default:
		DBGLOG(INIT, INFO, "%s(): Get UNKNOWN Mode.\n", __func__);
		return -EINVAL;
	}

	return 0;
}				/* wext_get_mode */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get the valid range for each configurable STA setting value.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prData Pointer to iw_point structure, not used.
 * \param[out] pcExtra Pointer to buffer which is allocated by caller of this
 *                     function, wext_support_ioctl() or ioctl_standard_call()
 *		       in wireless.c.
 *
 * \retval 0 If data is valid.
 *
 * \note The extra buffer (pcExtra) is filled with information from driver.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_range(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo,
	       IN struct iw_point *prData, OUT char *pcExtra)
{
	struct iw_range *prRange = NULL;
	uint8_t aucSuppRate[PARAM_MAX_LEN_RATES_EX] = { 0 }; /* data buffers */
	int i = 0;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR2(prNetDev, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	prRange = (struct iw_range *)pcExtra;

	memset(prRange, 0, sizeof(*prRange));
	prRange->throughput = 20000000;	/* 20Mbps */
	prRange->min_nwid = 0;	/* not used */
	prRange->max_nwid = 0;	/* not used */

	/* scan_capa not implemented */

	/* event_capa[6]: kernel + driver capabilities */
	prRange->event_capa[0] =
			(IW_EVENT_CAPA_K_0
			| IW_EVENT_CAPA_MASK(SIOCGIWAP)
			| IW_EVENT_CAPA_MASK(SIOCGIWSCAN)
			/* can't display meaningful string in iwlist
			 *  | IW_EVENT_CAPA_MASK(SIOCGIWTXPOW)
			 *  | IW_EVENT_CAPA_MASK(IWEVMICHAELMICFAILURE)
			 *  | IW_EVENT_CAPA_MASK(IWEVASSOCREQIE)
			 *  | IW_EVENT_CAPA_MASK(IWEVPMKIDCAND)
			 */
			 );
	prRange->event_capa[1] = IW_EVENT_CAPA_K_1;

	/* report 2.4G channel and frequency only */
	prRange->num_channels = (__u16) NUM_CHANNELS;
	prRange->num_frequency = (__u8) NUM_CHANNELS;
	for (i = 0; i < NUM_CHANNELS; i++) {
		/* iwlib takes this number as channel number */
		prRange->freq[i].i = i + 1;
		prRange->freq[i].m = channel_freq[i];
		prRange->freq[i].e = 6;	/* Values in table in MHz */
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQuerySupportedRates,
			   &aucSuppRate, sizeof(aucSuppRate),
			   TRUE, FALSE, FALSE, &u4BufLen);

	for (i = 0; i < IW_MAX_BITRATES
	     && i < PARAM_MAX_LEN_RATES_EX; i++) {
		if (aucSuppRate[i] == 0)
			break;
		prRange->bitrate[i] = (aucSuppRate[i] & 0x7F) *
				      500000;	/* 0.5Mbps */
	}
	prRange->num_bitrates = i;

	prRange->min_rts = 0;
	prRange->max_rts = 2347;
	prRange->min_frag = 256;
	prRange->max_frag = 2346;

	prRange->min_pmp = 0;	/* power management by driver */
	prRange->max_pmp = 0;	/* power management by driver */
	prRange->min_pmt = 0;	/* power management by driver */
	prRange->max_pmt = 0;	/* power management by driver */
	prRange->pmp_flags =
		IW_POWER_RELATIVE;	/* pm default flag */
	prRange->pmt_flags = IW_POWER_ON;	/* pm timeout flag */
	prRange->pm_capa =
		IW_POWER_ON;	/* power management by driver */

	prRange->encoding_size[0] = 5;	/* wep40 */
	prRange->encoding_size[1] = 16;	/* tkip */
	prRange->encoding_size[2] = 16;	/* ckip */
	prRange->encoding_size[3] = 16;	/* ccmp */
	prRange->encoding_size[4] = 13;	/* wep104 */
	prRange->encoding_size[5] = 16;	/* wep128 */
	prRange->num_encoding_sizes = 6;
	prRange->max_encoding_tokens = 6;	/* token? */

#if WIRELESS_EXT < 17
	prRange->txpower_capa = 0x0002;	/* IW_TXPOW_RELATIVE */
#else
	prRange->txpower_capa = IW_TXPOW_RELATIVE;
#endif
	prRange->num_txpower = 5;
	prRange->txpower[0] = 0;	/* minimum */
	prRange->txpower[1] = 25;	/* 25% */
	prRange->txpower[2] = 50;	/* 50% */
	prRange->txpower[3] = 100;	/* 100% */

	prRange->we_version_compiled = WIRELESS_EXT;
	prRange->we_version_source = WIRELESS_EXT;

	prRange->retry_capa = IW_RETRY_LIMIT;
	prRange->retry_flags = IW_RETRY_LIMIT;
	prRange->min_retry = 7;
	prRange->max_retry = 7;
	prRange->r_time_flags = IW_RETRY_ON;
	prRange->min_r_time = 0;
	prRange->max_r_time = 0;

	/* signal strength and link quality */
	/* Just define range here, reporting value moved to wext_get_stats() */
	prRange->sensitivity = -83;	/* fixed value */
	prRange->max_qual.qual = 100;	/* max 100% */
	prRange->max_qual.level = (__u8) (0x100 -
					  0);	/* max 0 dbm */
	prRange->max_qual.noise = (__u8) (0x100 -
					  0);	/* max 0 dbm */

	/* enc_capa */
#if WIRELESS_EXT > 17
	prRange->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
			    IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;
#endif

	/* min_pms; Minimal PM saving */
	/* max_pms; Maximal PM saving */
	/* pms_flags; How to decode max/min PM saving */

	/* modul_capa; IW_MODUL_* bit field */
	/* bitrate_capa; Types of bitrates supported */

	return 0;
}				/* wext_get_range */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set BSSID of AP to connect.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prAddr Pointer to struct sockaddr structure containing AP's BSSID.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 For success.
 *
 * \note Desired AP's BSSID is set to driver.
 */
/*----------------------------------------------------------------------------*/
static int
wext_set_ap(IN struct net_device *prDev,
	    IN struct iw_request_info *prIwrInfo,
	    IN struct sockaddr *prAddr, IN char *pcExtra)
{
	return 0;
}				/* wext_set_ap */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get AP MAC address.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[out] prAddr Pointer to struct sockaddr structure storing AP's BSSID.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 If netif_carrier_ok.
 * \retval -ENOTCONN Otherwise.
 *
 * \note If netif_carrier_ok, AP's mac address is stored in pAddr->sa_data.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_ap(IN struct net_device *prNetDev,
	    IN struct iw_request_info *prIwrInfo,
	    OUT struct sockaddr *prAddr, IN char *pcExtra)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	ASSERT(prNetDev);
	ASSERT(prAddr);
	if (GLUE_CHK_PR2(prNetDev, prAddr) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	/* if (!netif_carrier_ok(prNetDev)) { */
	/* return -ENOTCONN; */
	/* } */

	if (kalGetMediaStateIndicated(prGlueInfo,
		ucBssIndex) ==
	    MEDIA_STATE_DISCONNECTED) {
		memset(prAddr, 0, sizeof(struct sockaddr));
		return 0;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryBssid, prAddr->sa_data,
			   ETH_ALEN, TRUE, FALSE, FALSE, &u4BufLen);

	return 0;
}				/* wext_get_ap */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set mlme operation request.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prData Pointer of iw_point header.
 * \param[in] pcExtra Pointer to iw_mlme structure mlme request information.
 *
 * \retval 0 For success.
 * \retval -EOPNOTSUPP unsupported IW_MLME_ command.
 * \retval -EINVAL Set MLME Fail, different bssid.
 *
 * \note Driver will start mlme operation if valid.
 */
/*----------------------------------------------------------------------------*/
static int
wext_set_mlme(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwrInfo,
	      IN struct iw_point *prData, IN char *pcExtra)
{
	struct iw_mlme *prMlme = NULL;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR2(prNetDev, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	prMlme = (struct iw_mlme *)pcExtra;
	if (prMlme->cmd == IW_MLME_DEAUTH
	    || prMlme->cmd == IW_MLME_DISASSOC) {
		if (!netif_carrier_ok(prNetDev)) {
			DBGLOG(INIT, INFO,
			       "[wifi] Set MLME Deauth/Disassoc, but netif_carrier_off\n");
			return 0;
		}

		rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL,
				   0, FALSE, FALSE, TRUE, &u4BufLen);
		return 0;
	}
	DBGLOG(INIT, INFO,
	       "[wifi] unsupported IW_MLME_ command :%d\n", prMlme->cmd);
	return -EOPNOTSUPP;
}				/* wext_set_mlme */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To issue scan request.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prData NULL.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 For success.
 * \retval -EFAULT Tx power is off.
 *
 * \note Device will start scanning.
 */
/*----------------------------------------------------------------------------*/
static int
wext_set_scan(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwrInfo,
	      IN union iwreq_data *prData, IN char *pcExtra)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	int essid_len = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_DEV(prNetDev) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

#if WIRELESS_EXT > 17
	/* retrieve SSID */
	if (prData)
		essid_len = ((struct iw_scan_req *)(((struct iw_point *)
					     prData)->pointer))->essid_len;
#endif

	init_completion(&prGlueInfo->rScanComp);

	/* TODO:  parse flags and issue different scan requests? */

	rStatus = kalIoctl(prGlueInfo, wlanoidSetBssidListScan,
			   pcExtra, essid_len, FALSE, FALSE, FALSE, &u4BufLen);

	/* wait_for_completion_interruptible_timeout(&prGlueInfo->rScanComp,
	 *					     2 * KAL_HZ);
	 */
	/* kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_SCAN_COMPLETE,
	 *				NULL, 0);
	 */

	return 0;
}				/* wext_set_scan */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To write the ie to buffer
 *
 */
/*----------------------------------------------------------------------------*/
static inline int snprintf_hex(char *buf, size_t buf_size,
			       const u8 *data, size_t len)
{
	size_t i;
	char *pos = buf, *end = buf + buf_size;
	int ret;

	if (buf_size == 0)
		return 0;

	for (i = 0; i < len; i++) {
		ret = snprintf(pos, end - pos, "%02x", data[i]);
		if (ret < 0 || ret >= end - pos) {
			end[-1] = '\0';
			return pos - buf;
		}
		pos += ret;
	}
	end[-1] = '\0';
	return pos - buf;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get scan results, transform results from driver's format to WE's.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[out] prData Pointer to iw_point structure, pData->length is the size
 *		 of pcExtra buffer before used, and is updated after filling
 *		 scan results.
 * \param[out] pcExtra Pointer to buffer which is allocated by caller of this
 *                     function, wext_support_ioctl() or ioctl_standard_call()
 *		       in wireless.c.
 *
 * \retval 0 For success.
 * \retval -ENOMEM If dynamic memory allocation fail.
 * \retval -E2BIG Invalid length.
 *
 * \note Scan results is filled into pcExtra buffer, data size is updated in
 *       pData->length.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_scan(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwrInfo,
	      IN OUT struct iw_point *prData, IN char *pcExtra)
{
	uint32_t i = 0;
	uint32_t j = 0;
	struct PARAM_BSSID_LIST_EX *prList = NULL;
	struct PARAM_BSSID_EX *prBss = NULL;
	struct PARAM_VARIABLE_IE *prDesiredIE = NULL;
	struct iw_event iwEvent;	/* local iw_event buffer */

	/* write pointer of extra buffer */
	char *pcCur = NULL;
	/* pointer to the end of  last full entry in extra buffer */
	char *pcValidEntryEnd = NULL;
	char *pcEnd = NULL;	/* end of extra buffer */

	uint32_t u4AllocBufLen = 0;

	/* arrange rate information */
	uint32_t u4HighestRate = 0;
	char aucRatesBuf[64];
	uint32_t u4BufIndex;

	/* return value */
	int ret = 0;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prData);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	/* Initialize local variables */
	pcCur = pcExtra;
	pcValidEntryEnd = pcExtra;
	pcEnd = pcExtra + prData->length;	/* end of extra buffer */

	/* Allocate another query buffer with the same size of extra buffer */
	u4AllocBufLen = prData->length;
	prList = kalMemAlloc(u4AllocBufLen, VIR_MEM_TYPE);
	if (prList == NULL) {
		DBGLOG(INIT, INFO, "[wifi] no memory for scan list:%d\n",
		       prData->length);
		ret = -ENOMEM;
		goto error;
	}
	prList->u4NumberOfItems = 0;

	/* wait scan done */
	/* printk ("wait for scan results\n"); */
	/* wait_for_completion_interruptible_timeout(&prGlueInfo->rScanComp,
	 *					     4 * KAL_HZ);
	 */

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryBssidList, prList,
			   u4AllocBufLen, TRUE, FALSE, FALSE, &u4BufLen);

	if (rStatus == WLAN_STATUS_INVALID_LENGTH) {
		/* Buffer length is not large enough. */
		/* printk(KERN_INFO "[wifi] buf:%d result:%d\n",
		 *        pData->length, u4BufLen);
		 */

#if WIRELESS_EXT >= 17
		/* This feature is supported in WE-17 or above, limited by
		 * iwlist.
		 * Return -E2BIG and iwlist will request again with a larger
		 * buffer.
		 */
		ret = -E2BIG;
		/* Update length to give application a hint on result length */
		prData->length = (__u16) u4BufLen;
		goto error;
#else
		/* Realloc a larger query buffer here, but don't write too
		 * much to extra buffer when filling it later.
		 */
		kalMemFree(prList, VIR_MEM_TYPE, u4AllocBufLen);

		u4AllocBufLen = u4BufLen;
		prList = kalMemAlloc(u4AllocBufLen, VIR_MEM_TYPE);
		if (prList == NULL) {
			DBGLOG(INIT, INFO,
			       "[wifi] no memory for larger scan list :%d\n",
			       u4BufLen);
			ret = -ENOMEM;
			goto error;
		}
		prList->NumberOfItems = 0;

		rStatus = kalIoctl(prGlueInfo, wlanoidQueryBssidList, prList,
				   u4AllocBufLen,
				   TRUE, FALSE, FALSE, &u4BufLen);

		if (rStatus == WLAN_STATUS_INVALID_LENGTH) {
			DBGLOG(INIT, INFO, "[wifi] larger buf:%d result:%d\n",
			       u4AllocBufLen, u4BufLen);
			ret = -E2BIG;
			prData->length = (__u16) u4BufLen;
			goto error;
		}
#endif /* WIRELESS_EXT >= 17 */

	}

	if (prList->u4NumberOfItems > CFG_MAX_NUM_BSS_LIST) {
		DBGLOG(INIT, INFO, "[wifi] strange scan result count:%d\n",
		       prList->u4NumberOfItems);
		goto error;
	}

	/* Copy required data from pList to pcExtra */
	prBss = &prList->arBssid[0];	/* set to the first entry */
	for (i = 0; i < prList->u4NumberOfItems; ++i) {
		/* BSSID */
		iwEvent.cmd = SIOCGIWAP;
		iwEvent.len = IW_EV_ADDR_LEN;
		if ((pcCur + iwEvent.len) > pcEnd)
			break;
		iwEvent.u.ap_addr.sa_family = ARPHRD_ETHER;
		kalMemCopy(iwEvent.u.ap_addr.sa_data, prBss->arMacAddress,
			   ETH_ALEN);
		memcpy(pcCur, &iwEvent, IW_EV_ADDR_LEN);
		pcCur += IW_EV_ADDR_LEN;

		/* SSID */
		iwEvent.cmd = SIOCGIWESSID;
		/* Modification to user space pointer(essid.pointer) is not
		 * needed.
		 */
		iwEvent.u.essid.length = (__u16) prBss->rSsid.u4SsidLen;
		iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.essid.length;

		if ((pcCur + iwEvent.len) > pcEnd)
			break;
		iwEvent.u.essid.flags = 1;
		iwEvent.u.essid.pointer = NULL;

#if WIRELESS_EXT <= 18
		memcpy(pcCur, &iwEvent, iwEvent.len);
#else
		memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
		memcpy(pcCur + IW_EV_LCP_LEN, &iwEvent.u.data.length,
		       sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
		memcpy(pcCur + IW_EV_POINT_LEN, prBss->rSsid.aucSsid,
		       iwEvent.u.essid.length);
		pcCur += iwEvent.len;
		/* Frequency */
		iwEvent.cmd = SIOCGIWFREQ;
		iwEvent.len = IW_EV_FREQ_LEN;
		if ((pcCur + iwEvent.len) > pcEnd)
			break;
		iwEvent.u.freq.m = prBss->rConfiguration.u4DSConfig;
		iwEvent.u.freq.e = 3;	/* (in KHz) */
		iwEvent.u.freq.i = 0;
		memcpy(pcCur, &iwEvent, IW_EV_FREQ_LEN);
		pcCur += IW_EV_FREQ_LEN;

		/* Operation Mode */
		iwEvent.cmd = SIOCGIWMODE;
		iwEvent.len = IW_EV_UINT_LEN;
		if ((pcCur + iwEvent.len) > pcEnd)
			break;
		if (prBss->eOpMode == NET_TYPE_IBSS)
			iwEvent.u.mode = IW_MODE_ADHOC;
		else if (prBss->eOpMode == NET_TYPE_INFRA)
			iwEvent.u.mode = IW_MODE_INFRA;
		else
			iwEvent.u.mode = IW_MODE_AUTO;
		memcpy(pcCur, &iwEvent, IW_EV_UINT_LEN);
		pcCur += IW_EV_UINT_LEN;

		/* Quality */
		iwEvent.cmd = IWEVQUAL;
		iwEvent.len = IW_EV_QUAL_LEN;
		if ((pcCur + iwEvent.len) > pcEnd)
			break;
		iwEvent.u.qual.qual = 0; /* Quality not available now */
		/* -100 < Rssi < -10, normalized by adding 0x100 */
		iwEvent.u.qual.level = 0x100 + prBss->rRssi;
		iwEvent.u.qual.noise = 0; /* Noise not available now */
		iwEvent.u.qual.updated = IW_QUAL_QUAL_INVALID |
					 IW_QUAL_LEVEL_UPDATED |
					 IW_QUAL_NOISE_INVALID;
		memcpy(pcCur, &iwEvent, IW_EV_QUAL_LEN);
		pcCur += IW_EV_QUAL_LEN;

		/* Security Mode */
		iwEvent.cmd = SIOCGIWENCODE;
		iwEvent.len = IW_EV_POINT_LEN;
		if ((pcCur + iwEvent.len) > pcEnd)
			break;
		iwEvent.u.data.pointer = NULL;
		iwEvent.u.data.flags = 0;
		iwEvent.u.data.length = 0;
		if (!prBss->u4Privacy)
			iwEvent.u.data.flags |= IW_ENCODE_DISABLED;
#if WIRELESS_EXT <= 18
		memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
		memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
		memcpy(pcCur + IW_EV_LCP_LEN, &iwEvent.u.data.length,
		       sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
		pcCur += IW_EV_POINT_LEN;

		/* rearrange rate information */
		u4BufIndex = snprintf(aucRatesBuf, sizeof(aucRatesBuf),
				"Rates (Mb/s):");
		u4HighestRate = 0;
		for (j = 0; j < PARAM_MAX_LEN_RATES_EX; ++j) {
			uint8_t curRate = prBss->rSupportedRates[j] & 0x7F;

			if (curRate == 0)
				break;

			if (curRate > u4HighestRate)
				u4HighestRate = curRate;

			if (curRate == RATE_5_5M)
				u4BufIndex += snprintf(aucRatesBuf + u4BufIndex,
						sizeof(aucRatesBuf)
						- u4BufIndex, " 5.5");
			else
				u4BufIndex += snprintf(aucRatesBuf + u4BufIndex,
						sizeof(aucRatesBuf)
						- u4BufIndex, " %d",
						curRate / 2);
#if DBG
			if (u4BufIndex > sizeof(aucRatesBuf)) {
				/* printk("rate info too long\n"); */
				break;
			}
#endif
		}
		/* Report Highest Rates */
		iwEvent.cmd = SIOCGIWRATE;
		iwEvent.len = IW_EV_PARAM_LEN;
		if ((pcCur + iwEvent.len) > pcEnd)
			break;
		iwEvent.u.bitrate.value = u4HighestRate * 500000;
		iwEvent.u.bitrate.fixed = 0;
		iwEvent.u.bitrate.disabled = 0;
		iwEvent.u.bitrate.flags = 0;
		memcpy(pcCur, &iwEvent, iwEvent.len);
		pcCur += iwEvent.len;

#if WIRELESS_EXT >= 15		/* IWEVCUSTOM is available in WE-15 or above */
		/* Report Residual Rates */
		iwEvent.cmd = IWEVCUSTOM;
		iwEvent.u.data.length = u4BufIndex;
		iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
		if ((pcCur + iwEvent.len) > pcEnd)
			break;
		iwEvent.u.data.flags = 0;
#if WIRELESS_EXT <= 18
		memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
		memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
		memcpy(pcCur + IW_EV_LCP_LEN, &iwEvent.u.data.length,
		       sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
		memcpy(pcCur + IW_EV_POINT_LEN, aucRatesBuf, u4BufIndex);
		pcCur += iwEvent.len;
#endif /* WIRELESS_EXT >= 15 */

		if (wextSrchDesiredWPAIE(&prBss->aucIEs[sizeof(
		    struct PARAM_FIXED_IEs)],
		    prBss->u4IELength - sizeof(struct PARAM_FIXED_IEs), 0xDD,
		    (uint8_t **) &prDesiredIE)) {
			iwEvent.cmd = IWEVGENIE;
			iwEvent.u.data.flags = 1;
			iwEvent.u.data.length = 2 +
						(__u16) prDesiredIE->ucLength;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
#if WIRELESS_EXT <= 18
			memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length,
			       sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
			memcpy(pcCur + IW_EV_POINT_LEN, prDesiredIE,
			       2 + prDesiredIE->ucLength);
			pcCur += iwEvent.len;
		}
#if CFG_SUPPORT_WPS		/* search WPS IE (0xDD, 221, OUI: 0x0050f204) */
		if (wextSrchDesiredWPSIE(&prBss->aucIEs[sizeof(
		    struct PARAM_FIXED_IEs)],
		    prBss->u4IELength - sizeof(struct PARAM_FIXED_IEs), 0xDD,
		    (uint8_t **) &prDesiredIE)) {
			iwEvent.cmd = IWEVGENIE;
			iwEvent.u.data.flags = 1;
			iwEvent.u.data.length = 2 +
						(__u16) prDesiredIE->ucLength;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
#if WIRELESS_EXT <= 18
			memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length,
			       sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
			memcpy(pcCur + IW_EV_POINT_LEN, prDesiredIE,
			       2 + prDesiredIE->ucLength);
			pcCur += iwEvent.len;
		}
#endif

		/* Search RSN IE (0x30, 48). pBss->IEs starts from timestamp. */
		/* pBss->IEs starts from timestamp */
		if (wextSrchDesiredWPAIE(&prBss->aucIEs[sizeof(
		    struct PARAM_FIXED_IEs)],
		    prBss->u4IELength - sizeof(struct PARAM_FIXED_IEs), 0x30,
		    (uint8_t **) &prDesiredIE)) {

			iwEvent.cmd = IWEVGENIE;
			iwEvent.u.data.flags = 1;
			iwEvent.u.data.length = 2 +
						(__u16) prDesiredIE->ucLength;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
#if WIRELESS_EXT <= 18
			memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length,
			       sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
			memcpy(pcCur + IW_EV_POINT_LEN, prDesiredIE,
			       2 + prDesiredIE->ucLength);
			pcCur += iwEvent.len;
		}
#if CFG_SUPPORT_WAPI		/* Android+ */
		if (wextSrchDesiredWAPIIE(&prBss->aucIEs[
		    sizeof(struct PARAM_FIXED_IEs)],
		    prBss->u4IELength - sizeof(struct PARAM_FIXED_IEs),
		    (uint8_t **) &prDesiredIE)) {

#if 0
			iwEvent.cmd = IWEVGENIE;
			iwEvent.u.data.flags = 1;
			iwEvent.u.data.length =	2 +
						(__u16) prDesiredIE->ucLength;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
#if WIRELESS_EXT <= 18
			memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length,
			       sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
			memcpy(pcCur + IW_EV_POINT_LEN, prDesiredIE,
			       2 + prDesiredIE->ucLength);
			pcCur += iwEvent.len;
#else
			iwEvent.cmd = IWEVCUSTOM;
			iwEvent.u.data.length = (2 + prDesiredIE->ucLength) * 2
						+ 8 /* wapi_ie= */;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
			iwEvent.u.data.flags = 1;

			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length,
			       sizeof(struct iw_point) - IW_EV_POINT_OFF);

			pcCur += (IW_EV_POINT_LEN);

			pcCur += sprintf(pcCur, "wapi_ie=");

			snprintf_hex(pcCur, pcEnd - pcCur,
				     (uint8_t *) prDesiredIE,
				     prDesiredIE->ucLength + 2);

			pcCur += (2 + prDesiredIE->ucLength)
							* 2 /* iwEvent.len */;
#endif
		}
#endif
		/* Complete an entry. Update end of valid entry */
		pcValidEntryEnd = pcCur;
		/* Extract next bss */
		prBss = (struct PARAM_BSSID_EX *) ((char *)prBss +
						   prBss->u4Length);
	}

	/* Update valid data length for caller function and upper layer
	 * applications.
	 */
	prData->length = (pcValidEntryEnd - pcExtra);
	/* printk(KERN_INFO "[wifi] buf:%d result:%d\n",
	 *	  pData->length, u4BufLen);
	 */

	/* kalIndicateStatusAndComplete(prGlueInfo,
	 *				WLAN_STATUS_SCAN_COMPLETE, NULL, 0);
	 */

error:
	/* free local query buffer */
	if (prList)
		kalMemFree(prList, VIR_MEM_TYPE, u4AllocBufLen);

	return ret;
}				/* wext_get_scan */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set desired network name ESSID.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prEssid Pointer of iw_point header.
 * \param[in] pcExtra Pointer to buffer srtoring essid string.
 *
 * \retval 0 If netif_carrier_ok.
 * \retval -E2BIG Essid string length is too big.
 * \retval -EINVAL pcExtra is null pointer.
 * \retval -EFAULT Driver fail to set new essid.
 *
 * \note If string length is ok, device will try connecting to the new network.
 */
/*----------------------------------------------------------------------------*/
static int
wext_set_essid(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo,
	       IN struct iw_point *prEssid, IN char *pcExtra)
{
	struct PARAM_SSID rNewSsid;
	uint32_t cipher;
	enum ENUM_WEP_STATUS eEncStatus;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct GL_WPA_INFO *prWpaInfo;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	ASSERT(prNetDev);
	ASSERT(prEssid);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prEssid, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (prEssid->length > IW_ESSID_MAX_SIZE)
		return -E2BIG;

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	/* set auth mode */
	if (prWpaInfo->u4WpaVersion ==
	    IW_AUTH_WPA_VERSION_DISABLED) {
		eAuthMode = (prWpaInfo->u4AuthAlg ==
			     IW_AUTH_ALG_OPEN_SYSTEM) ?
			    AUTH_MODE_OPEN : AUTH_MODE_AUTO_SWITCH;
		/* printk(KERN_INFO
		 *        "IW_AUTH_WPA_VERSION_DISABLED->Param_AuthMode%s\n",/
		 *        (eAuthMode == AUTH_MODE_OPEN) ? "Open" : "Shared");
		 */
	} else {
		/* set auth mode */
		switch (prWpaInfo->u4KeyMgmt) {
		case IW_AUTH_KEY_MGMT_802_1X:
			eAuthMode =
				(prWpaInfo->u4WpaVersion ==
				 IW_AUTH_WPA_VERSION_WPA) ?
				AUTH_MODE_WPA : AUTH_MODE_WPA2;
			/* printk("IW_AUTH_KEY_MGMT_802_1X->AUTH_MODE_WPA%s\n",
			 * (eAuthMode == AUTH_MODE_WPA) ? "" : "2");
			 */
			break;
		case IW_AUTH_KEY_MGMT_PSK:
			eAuthMode =
				(prWpaInfo->u4WpaVersion ==
				 IW_AUTH_WPA_VERSION_WPA) ?
				AUTH_MODE_WPA_PSK : AUTH_MODE_WPA2_PSK;
			/* printk("IW_AUTH_KEY_MGMT_PSK->AUTH_MODE_WPA%sPSK\n",
			 * (eAuthMode == AUTH_MODE_WPA_PSK) ? "" : "2");
			 */
			break;
#if CFG_SUPPORT_WAPI		/* Android+ */
		case IW_AUTH_KEY_MGMT_WAPI_PSK:
			break;
		case IW_AUTH_KEY_MGMT_WAPI_CERT:
			break;
#endif

			/* #if defined (IW_AUTH_KEY_MGMT_WPA_NONE) */
			/* case IW_AUTH_KEY_MGMT_WPA_NONE: */
			/* eAuthMode = AUTH_MODE_WPA_NONE; */
			/* //printk(
			 * "IW_AUTH_KEY_MGMT_WPA_NONE->AUTH_MODE_WPA_NONE\n");
			 */
			/* break; */
			/* #endif */
#if CFG_SUPPORT_802_11W
		case IW_AUTH_KEY_MGMT_802_1X_SHA256:
			eAuthMode = AUTH_MODE_WPA2;
			break;
		case IW_AUTH_KEY_MGMT_PSK_SHA256:
			eAuthMode = AUTH_MODE_WPA2_PSK;
			break;
#endif
		default:
			/* printk(KERN_INFO DRV_NAME
			 * "strange IW_AUTH_KEY_MGMT : %d set auto switch\n",
			 * prWpaInfo->u4KeyMgmt);
			 */
			eAuthMode = AUTH_MODE_AUTO_SWITCH;
			break;
		}
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidSetAuthMode, &eAuthMode,
			   sizeof(eAuthMode), FALSE, FALSE, FALSE, &u4BufLen);

	/* set encryption status */
	cipher = prWpaInfo->u4CipherGroup |
		 prWpaInfo->u4CipherPairwise;
	if (cipher & IW_AUTH_CIPHER_CCMP) {
		/* printk("IW_AUTH_CIPHER_CCMP->ENUM_ENCRYPTION3_ENABLED\n"); */
		eEncStatus = ENUM_ENCRYPTION3_ENABLED;
	} else if (cipher & IW_AUTH_CIPHER_TKIP) {
		/* printk("IW_AUTH_CIPHER_TKIP->ENUM_ENCRYPTION2_ENABLED\n"); */
		eEncStatus = ENUM_ENCRYPTION2_ENABLED;
	} else if (cipher & (IW_AUTH_CIPHER_WEP104 |
			     IW_AUTH_CIPHER_WEP40)) {
		/* printk("IW_AUTH_CIPHER_WEPx->ENUM_ENCRYPTION1_ENABLED\n"); */
		eEncStatus = ENUM_ENCRYPTION1_ENABLED;
	} else if (cipher & IW_AUTH_CIPHER_NONE) {
		/* printk("IW_AUTH_CIPHER_NONE->ENUM_ENCRYPTION_DISABLED\n"); */
		if (prWpaInfo->fgPrivacyInvoke)
			eEncStatus = ENUM_ENCRYPTION1_ENABLED;
		else
			eEncStatus = ENUM_ENCRYPTION_DISABLED;
	} else {
		/* printk(
		 *	  "unknown IW_AUTH_CIPHER->Param_EncryptionDisabled\n"
		 *	  );
		 */
		eEncStatus = ENUM_ENCRYPTION_DISABLED;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidSetEncryptionStatus, &eEncStatus,
			   sizeof(eEncStatus),
			   FALSE, FALSE, FALSE, &u4BufLen);

#if WIRELESS_EXT < 21
	/* GeorgeKuo: a length error bug exists in (WE < 21) cases, kernel
	 *            before 2.6.19. Cut the trailing '\0'.
	 */
	rNewSsid.u4SsidLen = (prEssid->length) ? prEssid->length -
			     1 : 0;
#else
	rNewSsid.u4SsidLen = prEssid->length;
#endif
	kalMemCopy(rNewSsid.aucSsid, pcExtra, rNewSsid.u4SsidLen);

	/*
	 *  rNewSsid.aucSsid[rNewSsid.u4SsidLen] = '\0';
	 *  printk("set ssid(%u): %s\n", rNewSsid.u4SsidLen, rNewSsid.aucSsid);
	 */
	if (kalIoctl(prGlueInfo,
		     wlanoidSetSsid,
		     (void *)&rNewSsid, sizeof(struct PARAM_SSID), FALSE, FALSE,
		     TRUE, &u4BufLen) != WLAN_STATUS_SUCCESS) {
		/* printk(KERN_WARNING "Fail to set ssid\n"); */
		return -EFAULT;
	}

	return 0;
}				/* wext_set_essid */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get current network name ESSID.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prEssid Pointer to iw_point structure containing essid
 *            information.
 * \param[out] pcExtra Pointer to buffer srtoring essid string.
 *
 * \retval 0 If netif_carrier_ok.
 * \retval -ENOTCONN Otherwise.
 *
 * \note If netif_carrier_ok, network essid is stored in pcExtra.
 */
/*----------------------------------------------------------------------------*/
/* static PARAM_SSID_T ssid; */
static int
wext_get_essid(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo,
	       IN struct iw_point *prEssid, OUT char *pcExtra)
{
	/* PARAM_SSID_T ssid; */

	struct PARAM_SSID *prSsid;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prEssid);
	ASSERT(pcExtra);

	if (GLUE_CHK_PR3(prNetDev, prEssid, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	/* if (!netif_carrier_ok(prNetDev)) { */
	/* return -ENOTCONN; */
	/* } */

	prSsid = kalMemAlloc(sizeof(struct PARAM_SSID),
			     VIR_MEM_TYPE);

	if (!prSsid)
		return -ENOMEM;
	rStatus = kalIoctl(prGlueInfo, wlanoidQuerySsid, prSsid,
			   sizeof(struct PARAM_SSID),
			   TRUE, FALSE, FALSE, &u4BufLen);

	if ((rStatus == WLAN_STATUS_SUCCESS)
	    && (prSsid->u4SsidLen <= MAX_SSID_LEN)) {
		kalMemCopy(pcExtra, prSsid->aucSsid, prSsid->u4SsidLen);
		prEssid->length = prSsid->u4SsidLen;
		prEssid->flags = 1;
	}

	kalMemFree(prSsid, VIR_MEM_TYPE, sizeof(struct PARAM_SSID));

	return 0;
}				/* wext_get_essid */

#if 0

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set tx desired bit rate. Three cases here
 *      iwconfig wlan0 auto -> Set to origianl supported rate set.
 *      iwconfig wlan0 18M -> Imply "fixed" case, set to 18Mbps as desired rate.
 *      iwconfig wlan0 18M auto -> Set to auto rate lower and equal to 18Mbps
 *
 * \param[in] prNetDev       Pointer to the net_device handler.
 * \param[in] prIwReqInfo    Pointer to the Request Info.
 * \param[in] prRate         Pointer to the Rate Parameter.
 * \param[in] pcExtra        Pointer to the extra buffer.
 *
 * \retval 0         Update desired rate.
 * \retval -EINVAL   Wrong parameter
 */
/*----------------------------------------------------------------------------*/
int
wext_set_rate(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo,
	      IN struct iw_param *prRate, IN char *pcExtra)
{
	uint8_t aucSuppRate[PARAM_MAX_LEN_RATES_EX] = { 0 };
	uint8_t aucNewRate[PARAM_MAX_LEN_RATES_EX] = { 0 };
	uint32_t u4NewRateLen = 0;
	uint32_t i;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prRate);
	if (GLUE_CHK_PR2(prNetDev, prRate) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	/*
	 *  printk("value = %d, fixed = %d, disable = %d, flags = %d\n",
	 *  prRate->value, prRate->fixed, prRate->disabled, prRate->flags);
	 */

	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQuerySupportedRates, &aucSuppRate,
				       sizeof(aucSuppRate), &u4BufLen);

	/* Case: AUTO */
	if (prRate->value < 0) {
		if (prRate->fixed == 0) {
			/* iwconfig wlan0 rate auto */

			/* set full supported rate to device */
			/* printk(
			 *	"wlanoidQuerySupportedRates():u4BufLen = %d\n",
			 *	u4BufLen);
			 */
			rStatus = wlanSetInformation(prGlueInfo->prAdapter,
						     wlanoidSetDesiredRates,
						     &aucSuppRate,
						     sizeof(aucSuppRate),
						     &u4BufLen);
			return 0;
		}
		/* iwconfig wlan0 rate fixed */

		/* fix rate to what? DO NOTHING */
		return -EINVAL;
	}

	aucNewRate[0] = prRate->value /
			500000;	/* In unit of 500k */

	for (i = 0; i < PARAM_MAX_LEN_RATES_EX; i++) {
		/* check the given value is supported */
		if (aucSuppRate[i] == 0)
			break;

		if (aucNewRate[0] == aucSuppRate[i]) {
			u4NewRateLen = 1;
			break;
		}
	}

	if (u4NewRateLen == 0) {
		/* the given value is not supported */
		/* return error or use given rate as upper bound? */
		return -EINVAL;
	}

	if (prRate->fixed == 0) {
		/* add all rates lower than desired rate */
		for (i = 0; i < PARAM_MAX_LEN_RATES_EX; ++i) {
			if (aucSuppRate[i] == 0)
				break;

			if (aucSuppRate[i] < aucNewRate[0])
				aucNewRate[u4NewRateLen++] = aucSuppRate[i];
		}
	}

	rStatus = wlanSetInformation(prGlueInfo->prAdapter,
				     wlanoidSetDesiredRates, &aucNewRate,
				     sizeof(aucNewRate), &u4BufLen);
	return 0;
}				/* wext_set_rate */

#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get current tx bit rate.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[out] prRate Pointer to iw_param structure to store current tx rate.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 If netif_carrier_ok.
 * \retval -ENOTCONN Otherwise.
 *
 * \note If netif_carrier_ok, current tx rate is stored in pRate.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_rate(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwrInfo,
	      OUT struct iw_param *prRate, IN char *pcExtra)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct PARAM_LINK_SPEED_EX rLinkSpeed;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	uint32_t u4Rate = 0;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	ASSERT(prNetDev);
	ASSERT(prRate);
	if (GLUE_CHK_PR2(prNetDev, prRate) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));
	ucBssIndex = wlanGetBssIdx(prNetDev);

	if (!netif_carrier_ok(prNetDev))
		return -ENOTCONN;

	if (ucBssIndex >= BSSID_NUM)
		return -EFAULT;

	rStatus = kalIoctlByBssIdx(prGlueInfo, wlanoidQueryLinkSpeedEx,
			   &rLinkSpeed, sizeof(rLinkSpeed),
			   TRUE, FALSE, FALSE,
			   &u4BufLen, ucBssIndex);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;
	u4Rate = rLinkSpeed.rLq[ucBssIndex].u2LinkSpeed;
	/* u4Rate is in unit of 100bps */
	prRate->value = u4Rate * 100;
	prRate->fixed = 0;

	return 0;
}				/* wext_get_rate */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set RTS/CTS theshold.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prRts Pointer to iw_param structure containing rts threshold.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 For success.
 * \retval -EINVAL Given value is out of range.
 *
 * \note If given value is valid, device will follow the new setting.
 */
/*----------------------------------------------------------------------------*/
static int
wext_set_rts(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwrInfo,
	     IN struct iw_param *prRts, IN char *pcExtra)
{
	uint32_t u4RtsThresh;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prRts);
	if (GLUE_CHK_PR2(prNetDev, prRts) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (prRts->value < 0 || prRts->value > 2347)
		return -EINVAL;

	if (prRts->disabled == 1)
		u4RtsThresh = 2347;
	else
		u4RtsThresh = (uint32_t) prRts->value;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetRtsThreshold, &u4RtsThresh,
			   sizeof(u4RtsThresh),
			   FALSE, FALSE, FALSE, &u4BufLen);

	prRts->value = (typeof(prRts->value)) u4RtsThresh;
	prRts->disabled = (prRts->value > 2347) ? 1 : 0;
	prRts->fixed = 1;

	return 0;
}				/* wext_set_rts */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get RTS/CTS theshold.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[out] prRts Pointer to iw_param structure containing rts threshold.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 Success.
 *
 * \note RTS threshold is stored in pRts.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_rts(IN struct net_device *prNetDev,
	     IN struct iw_request_info *prIwrInfo,
	     OUT struct iw_param *prRts, IN char *pcExtra)
{
	uint32_t u4RtsThresh;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prRts);
	if (GLUE_CHK_PR2(prNetDev, prRts) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryRtsThreshold, &u4RtsThresh,
			   sizeof(u4RtsThresh), TRUE, FALSE, FALSE, &u4BufLen);

	prRts->value = (typeof(prRts->value)) u4RtsThresh;
	prRts->disabled = (prRts->value > 2347
			   || prRts->value < 0) ? 1 : 0;
	prRts->fixed = 1;

	return 0;
}				/* wext_get_rts */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get fragmentation threshold.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[out] prFrag Pointer to iw_param structure containing frag threshold.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 Success.
 *
 * \note RTS threshold is stored in pFrag. Fragmentation is disabled.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_frag(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwrInfo,
	      OUT struct iw_param *prFrag, IN char *pcExtra)
{
	ASSERT(prFrag);

	prFrag->value = 2346;
	prFrag->fixed = 1;
	prFrag->disabled = 1;
	return 0;
}				/* wext_get_frag */

#if 1
/*----------------------------------------------------------------------------*/
/*!
 * \brief To set TX power, or enable/disable the radio.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prTxPow Pointer to iw_param structure containing tx power setting.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 Success.
 *
 * \note Tx power is stored in pTxPow. iwconfig wlan0 txpow on/off are used
 *       to enable/disable the radio.
 */
/*----------------------------------------------------------------------------*/

static int
wext_set_txpow(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo,
	       IN struct iw_param *prTxPow, IN char *pcExtra)
{
	int ret = 0;
	/* PARAM_DEVICE_POWER_STATE ePowerState; */
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prTxPow);
	if (GLUE_CHK_PR2(prNetDev, prTxPow) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (prTxPow->disabled) {
		/* <1> disconnect */
		rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL,
				   0, FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			/* ToDo:: DBGLOG */
			DBGLOG(INIT, INFO, "######set disassoc failed\n");
		} else {
			DBGLOG(INIT, INFO, "######set assoc ok\n");
		}
		/* <2> mark to power state flag */
		DBGLOG(INIT, INFO, "set to acpi d3(0)\n");
		wlanSetAcpiState(prGlueInfo->prAdapter, ACPI_STATE_D0);

	} else {
		DBGLOG(INIT, INFO, "set to acpi d0\n");
		wlanSetAcpiState(prGlueInfo->prAdapter, ACPI_STATE_D0);
	}

	prGlueInfo->ePowerState = ParamDeviceStateD0;
	return ret;
}				/* wext_set_txpow */

#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get TX power.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[out] prTxPow Pointer to iw_param structure containing tx power
 *	      setting.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 Success.
 *
 * \note Tx power is stored in pTxPow.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_txpow(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo,
	       OUT struct iw_param *prTxPow, IN char *pcExtra)
{
	/* PARAM_DEVICE_POWER_STATE ePowerState; */

	struct GLUE_INFO *prGlueInfo = NULL;

	ASSERT(prNetDev);
	ASSERT(prTxPow);
	if (GLUE_CHK_PR2(prNetDev, prTxPow) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	/* GeorgeKuo: wlanoidQueryAcpiDevicePowerState() reports capability, not
	 * current state. Use struct GLUE_INFO to store state.
	 */
	/* ePowerState = prGlueInfo->ePowerState; */

	/* TxPow parameters: Fixed at relative 100% */
#if WIRELESS_EXT < 17
	prTxPow->flags = 0x0002;	/* IW_TXPOW_RELATIVE */
#else
	prTxPow->flags = IW_TXPOW_RELATIVE;
#endif
	prTxPow->value = 100;
	prTxPow->fixed = 1;
	/* prTxPow->disabled = (ePowerState != ParamDeviceStateD3) ?
	 *							FALSE : TRUE;
	 */
	prTxPow->disabled = TRUE;

	return 0;
}				/* wext_get_txpow */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get encryption cipher and key.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[out] prEnc Pointer to iw_point structure containing securiry
 *             information.
 * \param[in] pcExtra Buffer to store key content.
 *
 * \retval 0 Success.
 *
 * \note Securiry information is stored in pEnc except key content.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_encode(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwrInfo,
		OUT struct iw_point *prEnc, IN char *pcExtra)
{
#if 1
	/* ENUM_ENCRYPTION_STATUS_T eEncMode; */
	enum ENUM_WEP_STATUS eEncMode;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prEnc);
	if (GLUE_CHK_PR2(prNetDev, prEnc) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryEncryptionStatus, &eEncMode,
			   sizeof(eEncMode), TRUE, FALSE, FALSE, &u4BufLen);

	switch (eEncMode) {
	case ENUM_WEP_DISABLED:
		prEnc->flags = IW_ENCODE_DISABLED;
		break;
	case ENUM_WEP_ENABLED:
		prEnc->flags = IW_ENCODE_ENABLED;
		break;
	case ENUM_WEP_KEY_ABSENT:
		prEnc->flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
		break;
	default:
		prEnc->flags = IW_ENCODE_ENABLED;
		break;
	}

	/* Cipher, Key Content, Key ID can't be queried */
	prEnc->flags |= IW_ENCODE_NOKEY;
#endif
	return 0;
}				/* wext_get_encode */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set encryption cipher and key.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prEnc Pointer to iw_point structure containing securiry
 *	      information.
 * \param[in] pcExtra Pointer to key string buffer.
 *
 * \retval 0 Success.
 * \retval -EINVAL Key ID error for WEP.
 * \retval -EFAULT Setting parameters to driver fail.
 * \retval -EOPNOTSUPP Key size not supported.
 *
 * \note Securiry information is stored in pEnc.
 */
/*----------------------------------------------------------------------------*/
static uint8_t wepBuf[48];

static int
wext_set_encode(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwrInfo,
		IN struct iw_point *prEnc, IN char *pcExtra)
{
#if 1
	enum ENUM_WEP_STATUS eEncStatus;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	/* UINT_8 wepBuf[48]; */
	struct PARAM_WEP *prWepKey = (struct PARAM_WEP *) wepBuf;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct GL_WPA_INFO *prWpaInfo;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	ASSERT(prNetDev);
	ASSERT(prEnc);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prEnc, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	/* reset to default mode */
	prWpaInfo->u4WpaVersion =
		IW_AUTH_WPA_VERSION_DISABLED;
	prWpaInfo->u4KeyMgmt = 0;
	prWpaInfo->u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prWpaInfo->u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_802_11W
	prWpaInfo->u4Mfp = IW_AUTH_MFP_DISABLED;
#endif

	/* iwconfig wlan0 key off */
	if ((prEnc->flags & IW_ENCODE_MODE) == IW_ENCODE_DISABLED) {
		eAuthMode = AUTH_MODE_OPEN;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetAuthMode, &eAuthMode,
				   sizeof(eAuthMode),
				   FALSE, FALSE, FALSE, &u4BufLen);

		eEncStatus = ENUM_ENCRYPTION_DISABLED;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetEncryptionStatus,
				   &eEncStatus, sizeof(eEncStatus),
				   FALSE, FALSE, FALSE, &u4BufLen);

		return 0;
	}

	/* iwconfig wlan0 key 0123456789 */
	/* iwconfig wlan0 key s:abcde */
	/* iwconfig wlan0 key 0123456789 [1] */
	/* iwconfig wlan0 key 01234567890123456789012345 [1] */
	/* check key size for WEP */
	if (prEnc->length == 5 || prEnc->length == 13
	    || prEnc->length == 16) {
		/* prepare PARAM_WEP key structure */
		prWepKey->u4KeyIndex = (prEnc->flags & IW_ENCODE_INDEX) ?
			       (prEnc->flags & IW_ENCODE_INDEX) - 1 : 0;
		if (prWepKey->u4KeyIndex > 3) {
			/* key id is out of range */
			return -EINVAL;
		}
		prWepKey->u4KeyIndex |= 0x80000000;
		prWepKey->u4Length = 12 + prEnc->length;
		prWepKey->u4KeyLength = prEnc->length;
		kalMemCopy(prWepKey->aucKeyMaterial, pcExtra,
			   prEnc->length);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetAddWep, prWepKey,
				   prWepKey->u4Length,
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, INFO, "wlanoidSetAddWep fail 0x%x\n",
			       rStatus);
			return -EFAULT;
		}

		/* change to auto switch */
		prWpaInfo->u4AuthAlg = IW_AUTH_ALG_SHARED_KEY |
						 IW_AUTH_ALG_OPEN_SYSTEM;
		eAuthMode = AUTH_MODE_AUTO_SWITCH;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetAuthMode, &eAuthMode,
				   sizeof(eAuthMode),
				   FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			/* printk(KERN_INFO DRV_NAME
			 *	  "wlanoidSetAuthMode fail 0x%x\n", rStatus);
			 */
			return -EFAULT;
		}

		prWpaInfo->u4CipherPairwise =
			IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40;
		prWpaInfo->u4CipherGroup = IW_AUTH_CIPHER_WEP104 |
						     IW_AUTH_CIPHER_WEP40;

		eEncStatus = ENUM_WEP_ENABLED;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetEncryptionStatus,
				   &eEncStatus, sizeof(enum ENUM_WEP_STATUS),
				   FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			/* printk(KERN_INFO DRV_NAME
			 *	  "wlanoidSetEncryptionStatus fail 0x%x\n",
			 *	  rStatus);
			 */
			return -EFAULT;
		}

		return 0;
	}
#endif
	return -EOPNOTSUPP;
}				/* wext_set_encode */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set power management.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prPower Pointer to iw_param structure containing tx power setting.
 * \param[in] pcExtra NULL.
 *
 * \retval 0 Success.
 *
 * \note New Power Management Mode is set to driver.
 */
/*----------------------------------------------------------------------------*/
static int
wext_set_power(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo,
	       IN struct iw_param *prPower, IN char *pcExtra)
{
#if 1
	enum PARAM_POWER_MODE ePowerMode;
	int32_t i4PowerValue;

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct PARAM_POWER_MODE_ rPowerMode;

	ASSERT(prNetDev);
	ASSERT(prPower);
	if (GLUE_CHK_PR2(prNetDev, prPower) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (!prGlueInfo)
		return -EFAULT;

	/* printk(KERN_INFO
	 *	  "wext_set_power value(%d) disabled(%d) flag(0x%x)\n",
	 *	  prPower->value, prPower->disabled, prPower->flags);
	 */

	if (prPower->disabled) {
		ePowerMode = Param_PowerModeCAM;
	} else {
		i4PowerValue = prPower->value;
#if WIRELESS_EXT < 21
		i4PowerValue /= 1000000;
#endif
		if (i4PowerValue == 0) {
			ePowerMode = Param_PowerModeCAM;
		} else if (i4PowerValue == 1) {
			ePowerMode = Param_PowerModeMAX_PSP;
		} else if (i4PowerValue == 2) {
			ePowerMode = Param_PowerModeFast_PSP;
		} else {
			DBGLOG(INIT, INFO,
			       "%s(): unsupported power management mode value = %d.\n",
			       __func__, prPower->value);

			return -EINVAL;
		}
	}

	rPowerMode.ePowerMode = ePowerMode;
	rPowerMode.ucBssIdx = wlanGetBssIdx(prNetDev);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSet802dot11PowerSaveProfile,
			   &rPowerMode, sizeof(struct PARAM_POWER_MODE_),
			   FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		/* printk(KERN_INFO DRV_NAME
		 *        "wlanoidSet802dot11PowerSaveProfile fail 0x%x\n",
		 *	  rStatus);
		 */
		return -EFAULT;
	}
#endif
	return 0;
}				/* wext_set_power */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To get power management.
 *
 * \param[in]	prDev Net device requested.
 * \param[in]	prIwrInfo NULL.
 * \param[out]	prPower Pointer to iw_param structure containing tx power
 *		setting.
 * \param[in]	pcExtra NULL.
 *
 * \retval 0	Success.
 *
 * \note	Power management mode is stored in pTxPow->value.
 */
/*----------------------------------------------------------------------------*/
static int
wext_get_power(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo,
	       OUT struct iw_param *prPower, IN char *pcExtra)
{

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	enum PARAM_POWER_MODE ePowerMode = Param_PowerModeCAM;

	ASSERT(prNetDev);
	ASSERT(prPower);
	if (GLUE_CHK_PR2(prNetDev, prPower) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

#if 0
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidQuery802dot11PowerSaveProfile,
			       &ePowerMode, sizeof(ePowerMode), TRUE, TRUE,
			       &u4BufLen);
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQuery802dot11PowerSaveProfile,
				       &ePowerMode, sizeof(ePowerMode),
				       &u4BufLen);
#endif
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQuery802dot11PowerSaveProfile,
				       &ePowerMode, sizeof(ePowerMode),
				       &u4BufLen);
#endif

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	prPower->value = 0;
	prPower->disabled = 1;

	if (Param_PowerModeCAM == ePowerMode) {
		prPower->value = 0;
		prPower->disabled = 1;
	} else if (Param_PowerModeMAX_PSP == ePowerMode) {
		prPower->value = 1;
		prPower->disabled = 0;
	} else if (Param_PowerModeFast_PSP == ePowerMode) {
		prPower->value = 2;
		prPower->disabled = 0;
	}

	prPower->flags = IW_POWER_PERIOD | IW_POWER_RELATIVE;
#if WIRELESS_EXT < 21
	prPower->value *= 1000000;
#endif

	/* printk(KERN_INFO
	 *	  "wext_get_power value(%d) disabled(%d) flag(0x%x)\n",
	 *	  prPower->value, prPower->disabled, prPower->flags);
	 */

	return 0;
}				/* wext_get_power */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set authentication parameters.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] rpAuth Pointer to iw_param structure containing authentication
 *	      information.
 * \param[in] pcExtra Pointer to key string buffer.
 *
 * \retval 0 Success.
 * \retval -EINVAL Key ID error for WEP.
 * \retval -EFAULT Setting parameters to driver fail.
 * \retval -EOPNOTSUPP Key size not supported.
 *
 * \note Securiry information is stored in pEnc.
 */
/*----------------------------------------------------------------------------*/
static int
wext_set_auth(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwrInfo,
	      IN struct iw_param *prAuth, IN char *pcExtra)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct GL_WPA_INFO *prWpaInfo;
	struct CONNECTION_SETTINGS *prConnSettings;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	ASSERT(prNetDev);
	ASSERT(prAuth);
	if (GLUE_CHK_PR2(prNetDev, prAuth) == FALSE)
		return -EINVAL;
	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);
	prConnSettings = aisGetConnSettings(prGlueInfo->prAdapter,
		ucBssIndex);

	/* Save information to glue info and process later when ssid is set. */
	switch (prAuth->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
#if CFG_SUPPORT_WAPI
		if (aisGetWapiMode(prGlueInfo->prAdapter,
			ucBssIndex)) {
			prWpaInfo->u4WpaVersion =
						IW_AUTH_WPA_VERSION_DISABLED;
			prWpaInfo->u4AuthAlg =
						IW_AUTH_ALG_OPEN_SYSTEM;
		} else {
			prWpaInfo->u4WpaVersion = prAuth->value;
		}
#else
		prWpaInfo->u4WpaVersion = prAuth->value;
#endif
		break;

	case IW_AUTH_CIPHER_PAIRWISE:
		prWpaInfo->u4CipherPairwise = prAuth->value;
		break;

	case IW_AUTH_CIPHER_GROUP:
		prWpaInfo->u4CipherGroup = prAuth->value;
		break;

	case IW_AUTH_KEY_MGMT:
		prWpaInfo->u4KeyMgmt = prAuth->value;
#if CFG_SUPPORT_WAPI
		if (prWpaInfo->u4KeyMgmt ==
		    IW_AUTH_KEY_MGMT_WAPI_PSK ||
		    prWpaInfo->u4KeyMgmt ==
		    IW_AUTH_KEY_MGMT_WAPI_CERT) {
			uint32_t u4BufLen;
			uint32_t rStatus;

			rStatus = kalIoctl(prGlueInfo, wlanoidSetWapiMode,
					   &prAuth->value, sizeof(uint32_t),
					   FALSE, FALSE, TRUE, &u4BufLen);
			DBGLOG(INIT, INFO, "IW_AUTH_WAPI_ENABLED :%d\n",
			       prAuth->value);
		}
#endif
		if (prWpaInfo->u4KeyMgmt == IW_AUTH_KEY_MGMT_WPS)
			prConnSettings->fgWpsActive = TRUE;
		else
			prConnSettings->fgWpsActive = FALSE;
		break;

	case IW_AUTH_80211_AUTH_ALG:
		prWpaInfo->u4AuthAlg = prAuth->value;
		break;

	case IW_AUTH_PRIVACY_INVOKED:
		prWpaInfo->fgPrivacyInvoke = prAuth->value;
		break;
#if CFG_SUPPORT_802_11W
	case IW_AUTH_MFP:
		/* printk("wext_set_auth IW_AUTH_MFP=%d\n", prAuth->value); */
		prWpaInfo->u4Mfp = prAuth->value;
		break;
#endif
#if CFG_SUPPORT_WAPI
	case IW_AUTH_WAPI_ENABLED: {
		uint32_t u4BufLen;
		uint32_t rStatus;

		rStatus = kalIoctl(prGlueInfo, wlanoidSetWapiMode,
				   &prAuth->value, sizeof(uint32_t),
				   FALSE, FALSE, TRUE, &u4BufLen);
	}
	DBGLOG(INIT, INFO, "IW_AUTH_WAPI_ENABLED :%d\n",
	       prAuth->value);
	break;
#endif
	default:
		/*
		 *  printk(KERN_INFO "[wifi] unsupported IW_AUTH_INDEX :%d\n",
		 *	   prAuth->flags);
		 */
		break;
	}
	return 0;
}				/* wext_set_auth */

/*----------------------------------------------------------------------------*/
/*!
 * \brief To set encryption cipher and key.
 *
 * \param[in] prDev Net device requested.
 * \param[in] prIwrInfo NULL.
 * \param[in] prEnc Pointer to iw_point structure containing securiry
 *	      information.
 * \param[in] pcExtra Pointer to key string buffer.
 *
 * \retval 0 Success.
 * \retval -EINVAL Key ID error for WEP.
 * \retval -EFAULT Setting parameters to driver fail.
 * \retval -EOPNOTSUPP Key size not supported.
 *
 * \note Securiry information is stored in pEnc.
 */
/*----------------------------------------------------------------------------*/
#if CFG_SUPPORT_WAPI
uint8_t keyStructBuf[1024];	/* add/remove key shared buffer */
#else
uint8_t keyStructBuf[100];	/* add/remove key shared buffer */
#endif

static int
wext_set_encode_ext(IN struct net_device *prNetDev,
		    IN struct iw_request_info *prIwrInfo,
		    IN struct iw_point *prEnc, IN char *pcExtra)
{
	struct PARAM_REMOVE_KEY *prRemoveKey =
				(struct PARAM_REMOVE_KEY *) keyStructBuf;
	struct PARAM_KEY *prKey = (struct PARAM_KEY *) keyStructBuf;

	struct PARAM_WEP *prWepKey = (struct PARAM_WEP *) wepBuf;

	struct iw_encode_ext *prIWEncExt = (struct iw_encode_ext *)
					   pcExtra;

	enum ENUM_WEP_STATUS eEncStatus;
	enum ENUM_PARAM_AUTH_MODE eAuthMode;
	/* ENUM_PARAM_OP_MODE_T eOpMode = NET_TYPE_AUTO_SWITCH; */

#if CFG_SUPPORT_WAPI
	struct PARAM_WPI_KEY *prWpiKey = (struct PARAM_WPI_KEY *)
					 keyStructBuf;
#endif

	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct GL_WPA_INFO *prWpaInfo;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	ASSERT(prNetDev);
	ASSERT(prEnc);
	if (GLUE_CHK_PR3(prNetDev, prEnc, pcExtra) == FALSE)
		return -EINVAL;

	if (prIWEncExt == NULL) {
		DBGLOG(REQ, ERROR, "prIWEncExt is NULL!\n");
		return -EINVAL;
	}

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	memset(keyStructBuf, 0, sizeof(keyStructBuf));

#if CFG_SUPPORT_WAPI
	if (prIWEncExt->alg == IW_ENCODE_ALG_SMS4) {
		if (prEnc->flags & IW_ENCODE_DISABLED) {
			/* printk(KERN_INFO "[wapi] IW_ENCODE_DISABLED\n"); */
			return 0;
		}
		/* KeyID */
		prWpiKey->ucKeyID = (prEnc->flags & IW_ENCODE_INDEX);
		prWpiKey->ucKeyID--;
		if (prWpiKey->ucKeyID > 1) {
			/* key id is out of range */
			/* printk(KERN_INFO
			 *	  "[wapi] add key error: key_id invalid %d\n",
			 *	  prWpiKey->ucKeyID);
			 */
			return -EINVAL;
		}

		if (prIWEncExt->key_len != 32) {
			/* key length not valid */
			/* printk(KERN_INFO
			 *        "[wapi] add key error: key_len invalid %d\n",
			 *	  prIWEncExt->key_len);
			 */
			return -EINVAL;
		}
		/* printk(KERN_INFO "[wapi] %d ext_flags %d\n", prEnc->flags,
		 *        prIWEncExt->ext_flags);
		 */

		if (prIWEncExt->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
			prWpiKey->eKeyType = ENUM_WPI_GROUP_KEY;
			prWpiKey->eDirection = ENUM_WPI_RX;
		} else if (prIWEncExt->ext_flags &
			   IW_ENCODE_EXT_SET_TX_KEY) {
			prWpiKey->eKeyType = ENUM_WPI_PAIRWISE_KEY;
			prWpiKey->eDirection = ENUM_WPI_RX_TX;
		}

		/* PN */
		memcpy(&prWpiKey->aucPN[0], &prIWEncExt->tx_seq[0],
		       IW_ENCODE_SEQ_MAX_SIZE);
		memcpy(&prWpiKey->aucPN[IW_ENCODE_SEQ_MAX_SIZE],
		       &prIWEncExt->rx_seq[0], IW_ENCODE_SEQ_MAX_SIZE);

		/* BSSID */
		memcpy(prWpiKey->aucAddrIndex, prIWEncExt->addr.sa_data, 6);

		memcpy(prWpiKey->aucWPIEK, prIWEncExt->key, 16);
		prWpiKey->u4LenWPIEK = 16;

		memcpy(prWpiKey->aucWPICK, &prIWEncExt->key[16], 16);
		prWpiKey->u4LenWPICK = 16;
		prWpiKey->ucBssIdx = ucBssIndex;
		rStatus = kalIoctl(prGlueInfo, wlanoidSetWapiKey, prWpiKey,
				   sizeof(struct PARAM_WPI_KEY),
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			/* do nothing */
			/* printk(KERN_INFO "[wapi] add key error:%x\n",
			 *        rStatus);
			 */
		}

	} else
#endif
	{

		if ((prEnc->flags & IW_ENCODE_MODE) == IW_ENCODE_DISABLED) {
			prRemoveKey->u4Length = sizeof(*prRemoveKey);
			memcpy(prRemoveKey->arBSSID,
			       prIWEncExt->addr.sa_data, 6);
			/*
			 *  printk("IW_ENCODE_DISABLED: ID:%d, Addr:["
			 *	   MACSTR "]\n", prRemoveKey->KeyIndex,
			 *	   MAC2STR(prRemoveKey->BSSID));
			 */
			prRemoveKey->ucBssIdx = ucBssIndex;
			rStatus = kalIoctl(prGlueInfo, wlanoidSetRemoveKey,
					   prRemoveKey, prRemoveKey->u4Length,
					   FALSE, FALSE, TRUE, &u4BufLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO, "remove key error:%x\n",
				       rStatus);
			return 0;
		}
		/* return 0; */
		/* printk ("alg %x\n", prIWEncExt->alg); */

		switch (prIWEncExt->alg) {
		case IW_ENCODE_ALG_NONE:
			break;
		case IW_ENCODE_ALG_WEP:
			/* iwconfig wlan0 key 0123456789 */
			/* iwconfig wlan0 key s:abcde */
			/* iwconfig wlan0 key 0123456789 [1] */
			/* iwconfig wlan0 key 01234567890123456789012345 [1] */
			/* check key size for WEP */
			if (prIWEncExt->key_len == 5 ||
			    prIWEncExt->key_len == 13 ||
			    prIWEncExt->key_len == 16) {
				/* prepare PARAM_WEP key structure */
				prWepKey->u4KeyIndex =
					(prEnc->flags & IW_ENCODE_INDEX) ?
					(prEnc->flags & IW_ENCODE_INDEX) - 1 :
					0;
				if (prWepKey->u4KeyIndex > 3) {
					/* key id is out of range */
					return -EINVAL;
				}
				prWepKey->u4KeyIndex |= 0x80000000;
				prWepKey->u4Length = 12 + prIWEncExt->key_len;
				prWepKey->u4KeyLength = prIWEncExt->key_len;
				/* kalMemCopy(prWepKey->aucKeyMaterial, pcExtra,
				 *	      prIWEncExt->key_len);
				 */
				kalMemCopy(prWepKey->aucKeyMaterial,
					   prIWEncExt->key,
					   prIWEncExt->key_len);

				rStatus = kalIoctl(prGlueInfo,
						   wlanoidSetAddWep,
						   prWepKey, prWepKey->u4Length,
						   FALSE, FALSE, TRUE,
						   &u4BufLen);

				if (rStatus != WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, INFO,
					       "wlanoidSetAddWep fail 0x%x\n",
					       rStatus);
					return -EFAULT;
				}

				/* change to auto switch */
				prWpaInfo->u4AuthAlg =
							IW_AUTH_ALG_SHARED_KEY |
							IW_AUTH_ALG_OPEN_SYSTEM;
				eAuthMode = AUTH_MODE_AUTO_SWITCH;

				rStatus = kalIoctl(prGlueInfo,
						   wlanoidSetAuthMode,
						   &eAuthMode,
						   sizeof(eAuthMode),
						   FALSE, FALSE, FALSE,
						   &u4BufLen);

				if (rStatus != WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, INFO,
					       "wlanoidSetAuthMode fail 0x%x\n",
					       rStatus);
					return -EFAULT;
				}

				prWpaInfo->u4CipherPairwise =
							IW_AUTH_CIPHER_WEP104 |
							IW_AUTH_CIPHER_WEP40;
				prWpaInfo->u4CipherGroup =
							IW_AUTH_CIPHER_WEP104 |
							IW_AUTH_CIPHER_WEP40;

				eEncStatus = ENUM_WEP_ENABLED;

				rStatus = kalIoctl(prGlueInfo,
						   wlanoidSetEncryptionStatus,
						   &eEncStatus,
						   sizeof(enum ENUM_WEP_STATUS),
						   FALSE, FALSE, FALSE,
						   &u4BufLen);

				if (rStatus != WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, INFO,
					       "wlanoidSetEncryptionStatus fail 0x%x\n",
					       rStatus);
					return -EFAULT;
				}

			} else {
				DBGLOG(INIT, INFO, "key length %x\n",
				       prIWEncExt->key_len);
				DBGLOG(INIT, INFO, "key error\n");
			}

			break;
		case IW_ENCODE_ALG_TKIP:
		case IW_ENCODE_ALG_CCMP:
#if CFG_SUPPORT_802_11W
		case IW_ENCODE_ALG_AES_CMAC:
#endif
		{

			/* KeyID */
			prKey->u4KeyIndex = (prEnc->flags & IW_ENCODE_INDEX) ?
				    (prEnc->flags & IW_ENCODE_INDEX) - 1 : 0;
#if CFG_SUPPORT_802_11W
			if (prKey->u4KeyIndex > 5) {
#else
			if (prKey->u4KeyIndex > 3) {
#endif
				DBGLOG(INIT, INFO, "key index error:0x%x\n",
				       prKey->u4KeyIndex);
				/* key id is out of range */
				return -EINVAL;
			}

			/* bit(31) and bit(30) are shared by pKey and
			 * pRemoveKey
			 */
			/* Tx Key Bit(31) */
			if (prIWEncExt->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
				prKey->u4KeyIndex |= 0x1UL << 31;

			/* Pairwise Key Bit(30) */
			if (prIWEncExt->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
				/* group key */
			} else {
				/* pairwise key */
				prKey->u4KeyIndex |= 0x1UL << 30;
			}

		}
		/* Rx SC Bit(29) */
		if (prIWEncExt->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID) {
			prKey->u4KeyIndex |= 0x1UL << 29;
			memcpy(&prKey->rKeyRSC, prIWEncExt->rx_seq,
			       IW_ENCODE_SEQ_MAX_SIZE);
		}

		/* BSSID */
		memcpy(prKey->arBSSID, prIWEncExt->addr.sa_data, 6);

		/* switch tx/rx MIC key for sta */
		if (prIWEncExt->alg == IW_ENCODE_ALG_TKIP
		    && prIWEncExt->key_len == 32) {
			memcpy(prKey->aucKeyMaterial, prIWEncExt->key, 16);
			memcpy(((uint8_t *) prKey->aucKeyMaterial) + 16,
			       prIWEncExt->key + 24, 8);
			memcpy((prKey->aucKeyMaterial) + 24,
			       prIWEncExt->key + 16, 8);
		} else {
			if (prIWEncExt->key_len >
			    sizeof(prKey->aucKeyMaterial)) {
				DBGLOG(REQ, ERROR,
				       "prIWEncExt->key_len: %u is too long!\n",
				       prIWEncExt->key_len);
				return -EINVAL;
			}
			memcpy(prKey->aucKeyMaterial, prIWEncExt->key,
			       prIWEncExt->key_len);
		}

		prKey->u4KeyLength = prIWEncExt->key_len;
		prKey->u4Length = ((unsigned long) &(((struct PARAM_KEY *)
				0)->aucKeyMaterial)) + prKey->u4KeyLength;
		prKey->ucBssIdx = ucBssIndex;
		rStatus = kalIoctl(prGlueInfo, wlanoidSetAddKey, prKey,
				   prKey->u4Length,
				   FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, INFO, "add key error:%x\n", rStatus);
			return -EFAULT;
		}
		break;
	}
	}

	return 0;
}				/* wext_set_encode_ext */


/*----------------------------------------------------------------------------*/
/*!
 * \brief Set country code
 *
 * \param[in] prNetDev Net device requested.
 * \param[in] prData iwreq.u.data carries country code value.
 *
 * \retval 0 For success.
 * \retval -EEFAULT For fail.
 *
 * \note Country code is stored and channel list is updated based on current
 *	 country domain.
 */
/*----------------------------------------------------------------------------*/
static int wext_set_country(IN struct net_device *prNetDev,
			    IN struct iw_point *prData)
{
	struct GLUE_INFO *prGlueInfo;
	uint32_t rStatus;
	uint32_t u4BufLen;
	uint8_t aucCountry[COUNTRY_CODE_LEN];

	ASSERT(prNetDev);

	/* prData->pointer should be like "COUNTRY US", "COUNTRY EU"
	 * and "COUNTRY JP"
	 */
	if (GLUE_CHK_PR2(prNetDev, prData) == FALSE
	    || !prData->pointer || prData->length < COUNTRY_CODE_LEN)
		return -EINVAL;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prNetDev));

	if (copy_from_user(aucCountry, prData->pointer,
			   COUNTRY_CODE_LEN))
		return -EFAULT;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetCountryCode,
			   &aucCountry[COUNTRY_CODE_LEN - 2], 2,
			   FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "Set country code error: %x\n", rStatus);
		return -EFAULT;
	}

	return 0;
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief To report the iw private args table to user space.
 *
 * \param[in] prNetDev Net device requested.
 * \param[out] prData iwreq.u.data to carry the private args table.
 *
 * \retval 0  For success.
 * \retval -E2BIG For user's buffer size is too small.
 * \retval -EFAULT For fail.
 *
 */
/*----------------------------------------------------------------------------*/
int wext_get_priv(IN struct net_device *prNetDev,
		  OUT struct iw_point *prData)
{
	uint16_t u2BufferSize = prData->length;

	/* Update our private args table size */
	prData->length = (__u16)sizeof(rIwPrivTable);
	if (u2BufferSize < prData->length)
		return -E2BIG;

	if (prData->length) {
		if (copy_to_user(prData->pointer, rIwPrivTable,
				 sizeof(rIwPrivTable)))
			return -EFAULT;
	}

	return 0;
}				/* wext_get_priv */

/*----------------------------------------------------------------------------*/
/*!
 * \brief ioctl() (Linux Wireless Extensions) routines
 *
 * \param[in] prDev Net device requested.
 * \param[in] ifr The ifreq structure for seeting the wireless extension.
 * \param[in] i4Cmd The wireless extension ioctl command.
 *
 * \retval zero On success.
 * \retval -EOPNOTSUPP If the cmd is not supported.
 * \retval -EFAULT If copy_to_user goes wrong.
 * \retval -EINVAL If any value's out of range.
 *
 * \note
 */
/*----------------------------------------------------------------------------*/
int wext_support_ioctl(IN struct net_device *prDev,
		       IN struct ifreq *prIfReq, IN int i4Cmd)
{
	/* prIfReq is verified in the caller function wlanDoIOCTL() */
	struct iwreq *iwr = (struct iwreq *)prIfReq;
	struct iw_request_info rIwReqInfo;
	int ret = 0;
	char *prExtraBuf = NULL;
	uint32_t u4ExtraSize = 0;

	/* prDev is verified in the caller function wlanDoIOCTL() */

	/* printk("%d CMD:0x%x\n", jiffies_to_msecs(jiffies), i4Cmd); */

	/* Prepare the call */
	rIwReqInfo.cmd = (__u16) i4Cmd;
	rIwReqInfo.flags = 0;

	switch (i4Cmd) {
	case SIOCGIWNAME:	/* 0x8B01, get wireless protocol name */
		ret = wext_get_name(prDev, &rIwReqInfo, (char *)&iwr->u.name,
				    sizeof(iwr->u.name), NULL);
		break;

	/* case SIOCSIWNWID: 0x8B02, deprecated */
	/* case SIOCGIWNWID: 0x8B03, deprecated */

	case SIOCSIWFREQ:	/* 0x8B04, set channel */
		ret = wext_set_freq(prDev, NULL, &iwr->u.freq, NULL);
		break;

	case SIOCGIWFREQ:	/* 0x8B05, get channel */
		ret = wext_get_freq(prDev, NULL, &iwr->u.freq, NULL);
		break;

	case SIOCSIWMODE:	/* 0x8B06, set operation mode */
		ret = wext_set_mode(prDev, NULL, &iwr->u.mode, NULL);
		/* ret = 0; */
		break;

	case SIOCGIWMODE:	/* 0x8B07, get operation mode */
		ret = wext_get_mode(prDev, NULL, &iwr->u.mode, NULL);
		break;

	/* case SIOCSIWSENS: 0x8B08, unsupported */
	/* case SIOCGIWSENS: 0x8B09, unsupported */

	/* case SIOCSIWRANGE: 0x8B0A, unused */
	case SIOCGIWRANGE:	/* 0x8B0B, get range of parameters */
		if (iwr->u.data.pointer != NULL) {
			/* Buffer size should be large enough */
			if (iwr->u.data.length < sizeof(struct iw_range)) {
				ret = -E2BIG;
				break;
			}

			prExtraBuf = kalMemAlloc(sizeof(struct iw_range),
						 VIR_MEM_TYPE);
			if (!prExtraBuf) {
				ret = -ENOMEM;
				break;
			}

			/* reset all fields */
			memset(prExtraBuf, 0, sizeof(struct iw_range));
			iwr->u.data.length = sizeof(struct iw_range);

			ret = wext_get_range(prDev, NULL, &iwr->u.data,
					     prExtraBuf);
			/* Push up to the caller */
			if (copy_to_user(iwr->u.data.pointer, prExtraBuf,
					 iwr->u.data.length))
				ret = -EFAULT;

			kalMemFree(prExtraBuf, VIR_MEM_TYPE,
				   sizeof(struct iw_range));
			prExtraBuf = NULL;
		} else {
			ret = -EINVAL;
		}
		break;

	case SIOCSIWPRIV:	/* 0x8B0C, set country code */
		ret = wext_set_country(prDev, &iwr->u.data);
		break;

	case SIOCGIWPRIV:	/* 0x8B0D, get private args table */
		ret = wext_get_priv(prDev, &iwr->u.data);
		break;

	/* caes SIOCSIWSTATS: 0x8B0E, unused */
	/* case SIOCGIWSTATS:
	 *  get statistics, intercepted by wireless_process_ioctl()
	 *  in wireless.c,
	 *  redirected to dev_iwstats(), dev->get_wireless_stats().
	 */
	/* case SIOCSIWSPY: 0x8B10, unsupported */
	/* case SIOCGIWSPY: 0x8B11, unsupported */
	/* case SIOCSIWTHRSPY: 0x8B12, unsupported */
	/* case SIOCGIWTHRSPY: 0x8B13, unsupported */

	case SIOCSIWAP:	/* 0x8B14, set access point MAC addresses (BSSID) */
		if (iwr->u.ap_addr.sa_data[0] == 0 &&
		    iwr->u.ap_addr.sa_data[1] == 0 &&
		    iwr->u.ap_addr.sa_data[2] == 0 &&
		    iwr->u.ap_addr.sa_data[3] == 0 &&
		    iwr->u.ap_addr.sa_data[4] == 0
		    && iwr->u.ap_addr.sa_data[5] == 0) {
			/* WPA Supplicant will set 000000000000 in
			 * wpa_driver_wext_deinit(), do nothing here or
			 * disassoc again?
			 */
			ret = 0;
			break;
		}
		ret = wext_set_ap(prDev, NULL, &iwr->u.ap_addr, NULL);
		break;

	case SIOCGIWAP:	/* 0x8B15, get access point MAC addresses (BSSID) */
		ret = wext_get_ap(prDev, NULL, &iwr->u.ap_addr, NULL);
		break;

	case SIOCSIWMLME:	/* 0x8B16, request MLME operation */
		/* Fixed length structure */
		if (iwr->u.data.length != sizeof(struct iw_mlme)) {
			DBGLOG(INIT, INFO, "MLME buffer strange:%d\n",
			       iwr->u.data.length);
			ret = -EINVAL;
			break;
		}

		if (!iwr->u.data.pointer) {
			ret = -EINVAL;
			break;
		}

		prExtraBuf = kalMemAlloc(sizeof(struct iw_mlme),
					 VIR_MEM_TYPE);
		if (!prExtraBuf) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(prExtraBuf, iwr->u.data.pointer,
				   sizeof(struct iw_mlme)))
			ret = -EFAULT;
		else
			ret = wext_set_mlme(prDev, NULL, &(iwr->u.data),
					    prExtraBuf);

		kalMemFree(prExtraBuf, VIR_MEM_TYPE,
			   sizeof(struct iw_mlme));
		prExtraBuf = NULL;
		break;

	/* case SIOCGIWAPLIST: 0x8B17, deprecated */
	case SIOCSIWSCAN:	/* 0x8B18, scan request */
		if (iwr->u.data.pointer == NULL)
			ret = wext_set_scan(prDev, NULL, NULL, NULL);
#if WIRELESS_EXT > 17
		else if (iwr->u.data.length == sizeof(struct iw_scan_req)) {
			struct iw_scan_req iw;

			if (copy_from_user(&iw,
			    (struct iw_scan_req *)(iwr->u.data.pointer),
			    sizeof(struct iw_scan_req))) {
				ret = -EFAULT;
				break;
			}

			if (iw.essid_len > MAX_SSID_LEN) {
				ret = -EFAULT;
				break;
			}

			prExtraBuf = kalMemAlloc(MAX_SSID_LEN, VIR_MEM_TYPE);
			if (!prExtraBuf) {
				ret = -ENOMEM;
				break;
			}

			if (copy_from_user(prExtraBuf, &iw.essid,
			    iw.essid_len)) {
				ret = -EFAULT;
			} else {
				ret = wext_set_scan(prDev, NULL,
					(union iwreq_data *)&(iwr->u.data),
					prExtraBuf);
			}

			kalMemFree(prExtraBuf, VIR_MEM_TYPE, MAX_SSID_LEN);
			prExtraBuf = NULL;
		}
#endif
		else
			ret = -EINVAL;
		break;
#if 1
	case SIOCGIWSCAN:	/* 0x8B19, get scan results */
		if (!iwr->u.data.pointer || !iwr->u.essid.pointer) {
			ret = -EINVAL;
			break;
		}

		u4ExtraSize = iwr->u.data.length;
		/* allocate the same size of kernel buffer to store scan
		 * results.
		 */
		prExtraBuf = kalMemAlloc(u4ExtraSize, VIR_MEM_TYPE);
		if (!prExtraBuf) {
			ret = -ENOMEM;
			break;
		}

		/* iwr->u.data.length may be updated by wext_get_scan() */
		ret = wext_get_scan(prDev, NULL, &iwr->u.data, prExtraBuf);
		if (ret != 0) {
			if (ret == -E2BIG)
				DBGLOG(INIT, INFO,
				       "[wifi] wext_get_scan -E2BIG\n");
		} else {
			/* check updated length is valid */
			ASSERT(iwr->u.data.length <= u4ExtraSize);
			if (iwr->u.data.length > u4ExtraSize) {
				DBGLOG(INIT, INFO,
				       "Updated result length is larger than allocated (%d > %d)\n",
				       iwr->u.data.length, u4ExtraSize);
				iwr->u.data.length = u4ExtraSize;
			}

			if (copy_to_user(iwr->u.data.pointer, prExtraBuf,
					 iwr->u.data.length))
				ret = -EFAULT;
		}

		kalMemFree(prExtraBuf, VIR_MEM_TYPE, u4ExtraSize);
		prExtraBuf = NULL;

		break;

#endif

#if 1
	case SIOCSIWESSID:	/* 0x8B1A, set SSID (network name) */
		u4ExtraSize = iwr->u.essid.length;
		if (u4ExtraSize > IW_ESSID_MAX_SIZE) {
			ret = -E2BIG;
			break;
		}
		if (!iwr->u.essid.pointer) {
			ret = -EINVAL;
			break;
		}

		prExtraBuf = kalMemAlloc(IW_ESSID_MAX_SIZE + 4,
					 VIR_MEM_TYPE);
		if (!prExtraBuf) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(prExtraBuf, iwr->u.essid.pointer,
				   u4ExtraSize)) {
			ret = -EFAULT;
		} else {
			/* Add trailing '\0' for printk */
			/* prExtraBuf[iwr->u.essid.length] = 0; */
			/* printk(KERN_INFO "wext_set_essid: %s (%d)\n",
			 *	  prExtraBuf, iwr->u.essid.length);
			 */
			ret = wext_set_essid(prDev, NULL, &iwr->u.essid,
					     prExtraBuf);
			/* printk ("set essid %d\n", ret); */
		}

		kalMemFree(prExtraBuf, VIR_MEM_TYPE, IW_ESSID_MAX_SIZE + 4);
		prExtraBuf = NULL;
		break;

#endif

	case SIOCGIWESSID:	/* 0x8B1B, get SSID */
		u4ExtraSize = iwr->u.essid.length;
		if (!iwr->u.essid.pointer) {
			ret = -EINVAL;
			break;
		}

		if (u4ExtraSize != IW_ESSID_MAX_SIZE
		    && u4ExtraSize != IW_ESSID_MAX_SIZE + 1) {
			DBGLOG(INIT, INFO,
			       "[wifi] iwr->u.essid.length:%d too small\n",
			       iwr->u.essid.length);
			ret = -E2BIG;	/* let caller try larger buffer */
			break;
		}

		prExtraBuf = kalMemAlloc(IW_ESSID_MAX_SIZE + 1,
					 VIR_MEM_TYPE);
		if (!prExtraBuf) {
			ret = -ENOMEM;
			break;
		}

		/* iwr->u.essid.length is updated by wext_get_essid() */

		ret = wext_get_essid(prDev, NULL, &iwr->u.essid,
				     prExtraBuf);
		if (ret == 0) {
			if (copy_to_user(iwr->u.essid.pointer, prExtraBuf,
					 iwr->u.essid.length))
				ret = -EFAULT;
		}

		kalMemFree(prExtraBuf, VIR_MEM_TYPE, IW_ESSID_MAX_SIZE + 1);
		prExtraBuf = NULL;

		break;

	/* case SIOCSIWNICKN: 0x8B1C, not supported */
	/* case SIOCGIWNICKN: 0x8B1D, not supported */

	case SIOCSIWRATE:	/* 0x8B20, set default bit rate (bps) */
		/* ret = wext_set_rate(prDev, &rIwReqInfo, &iwr->u.bitrate,
		 *		       NULL);
		 */
		break;

	case SIOCGIWRATE:	/* 0x8B21, get current bit rate (bps) */
		ret = wext_get_rate(prDev, NULL, &iwr->u.bitrate, NULL);
		break;

	case SIOCSIWRTS:	/* 0x8B22, set rts/cts threshold */
		ret = wext_set_rts(prDev, NULL, &iwr->u.rts, NULL);
		break;

	case SIOCGIWRTS:	/* 0x8B23, get rts/cts threshold */
		ret = wext_get_rts(prDev, NULL, &iwr->u.rts, NULL);
		break;

	/* case SIOCSIWFRAG: 0x8B24, unsupported */
	case SIOCGIWFRAG:	/* 0x8B25, get frag threshold */
		ret = wext_get_frag(prDev, NULL, &iwr->u.frag, NULL);
		break;

	case SIOCSIWTXPOW:	/* 0x8B26, set relative tx power (in %) */
		ret = wext_set_txpow(prDev, NULL, &iwr->u.txpower, NULL);
		break;

	case SIOCGIWTXPOW:	/* 0x8B27, get relative tx power (in %) */
		ret = wext_get_txpow(prDev, NULL, &iwr->u.txpower, NULL);
		break;

		/* case SIOCSIWRETRY: 0x8B28, unsupported */
		/* case SIOCGIWRETRY: 0x8B29, unsupported */

#if 1
	case SIOCSIWENCODE:	/* 0x8B2A, set encoding token & mode */
		/* Only DISABLED case has NULL pointer and length == 0 */
		u4ExtraSize = iwr->u.encoding.length;
		if (iwr->u.encoding.pointer) {
			if (u4ExtraSize > 16) {
				ret = -E2BIG;
				break;
			}

			prExtraBuf = kalMemAlloc(u4ExtraSize, VIR_MEM_TYPE);
			if (!prExtraBuf) {
				ret = -ENOMEM;
				break;
			}

			if (copy_from_user(prExtraBuf, iwr->u.encoding.pointer,
					   u4ExtraSize))
				ret = -EFAULT;

			if (ret == 0)
				ret = wext_set_encode(prDev, NULL,
						      &iwr->u.encoding,
						      prExtraBuf);

			kalMemFree(prExtraBuf, VIR_MEM_TYPE, u4ExtraSize);
			prExtraBuf = NULL;

		} else if (u4ExtraSize != 0)
			ret = -EINVAL;
		break;

	case SIOCGIWENCODE:	/* 0x8B2B, get encoding token & mode */
		/* check pointer */
		ret = wext_get_encode(prDev, NULL, &iwr->u.encoding, NULL);
		break;

	case SIOCSIWPOWER:	/* 0x8B2C, set power management */
		ret = wext_set_power(prDev, NULL, &iwr->u.power, NULL);
		break;

	case SIOCGIWPOWER:	/* 0x8B2D, get power management */
		ret = wext_get_power(prDev, NULL, &iwr->u.power, NULL);
		break;

#if WIRELESS_EXT > 17
	case SIOCSIWGENIE:	/* 0x8B30, set gen ie */
		if (iwr->u.data.pointer) {
			u4ExtraSize = iwr->u.data.length;
			if (1 /* wlanQueryWapiMode(prGlueInfo->prAdapter) */) {
				/* Fixed length structure */
#if CFG_SUPPORT_WAPI
				if (u4ExtraSize > 42 /* The max wapi ie buffer
						      */
				    ) {
					ret = -EINVAL;
					break;
				}
#endif
				if (u4ExtraSize) {
					prExtraBuf = kalMemAlloc(u4ExtraSize,
								 VIR_MEM_TYPE);
					if (!prExtraBuf) {
						ret = -ENOMEM;
						break;
					}
					if (copy_from_user(prExtraBuf,
					    iwr->u.data.pointer, u4ExtraSize))
						ret = -EFAULT;
					else
						wext_support_ioctl_SIOCSIWGENIE(
							prDev, prExtraBuf,
							u4ExtraSize);
					kalMemFree(prExtraBuf, VIR_MEM_TYPE,
						   u4ExtraSize);
					prExtraBuf = NULL;
				}
			}
		}
		break;

	case SIOCGIWGENIE:	/* 0x8B31, get gen ie, unused */
		break;

#endif

	case SIOCSIWAUTH:	/* 0x8B32, set auth mode params */
		ret = wext_set_auth(prDev, NULL, &iwr->u.param, NULL);
		break;

	/* case SIOCGIWAUTH: 0x8B33, unused? */
	case SIOCSIWENCODEEXT:	/* 0x8B34, set extended encoding token & mode */
		if (iwr->u.encoding.pointer) {
			u4ExtraSize = iwr->u.encoding.length;
			if (u4ExtraSize > sizeof(struct iw_encode_ext)) {
				ret = -EINVAL;
				break;
			}

			prExtraBuf = kalMemAlloc(u4ExtraSize, VIR_MEM_TYPE);
			if (!prExtraBuf) {
				ret = -ENOMEM;
				break;
			}

			if (copy_from_user(prExtraBuf, iwr->u.encoding.pointer,
					   u4ExtraSize))
				ret = -EFAULT;
		} else if (iwr->u.encoding.length != 0) {
			ret = -EINVAL;
			break;
		}

		if (ret == 0)
			ret = wext_set_encode_ext(prDev, NULL, &iwr->u.encoding,
						  prExtraBuf);

		if (prExtraBuf) {
			kalMemFree(prExtraBuf, VIR_MEM_TYPE, u4ExtraSize);
			prExtraBuf = NULL;
		}
		break;

	/* case SIOCGIWENCODEEXT: 0x8B35, unused? */

	case SIOCSIWPMKSA:	/* 0x8B36, pmksa cache operation */
#if 1
		if (iwr->u.data.pointer) {
			/* Fixed length structure */
			if (iwr->u.data.length != sizeof(struct iw_pmksa)) {
				ret = -EINVAL;
				break;
			}

			u4ExtraSize = sizeof(struct iw_pmksa);
			prExtraBuf = kalMemAlloc(u4ExtraSize, VIR_MEM_TYPE);
			if (!prExtraBuf) {
				ret = -ENOMEM;
				break;
			}

			if (copy_from_user(prExtraBuf, iwr->u.data.pointer,
					   sizeof(struct iw_pmksa))) {
				ret = -EFAULT;
			} else {
				switch (((struct iw_pmksa *)prExtraBuf)->cmd) {
				case IW_PMKSA_ADD:
					/*
					 *  printk(KERN_INFO "IW_PMKSA_ADD ["
					 *	   MACSTR "]\n", MAC2STR((
					 *	   (struct iw_pmksa *)pExtraBuf)
					 *		->bssid.sa_data));
					 */
				{
					wext_support_ioctl_SIOCSIWPMKSA_Action(
						prDev, prExtraBuf,
						IW_PMKSA_ADD, &ret);
				}
				break;
				case IW_PMKSA_REMOVE:
					/*
					 *  printk(KERN_INFO "IW_PMKSA_REMOVE ["
					 *	   MACSTR "]\n", MAC2STR(
					 *	   ((struct iw_pmksa *)buf)
					 *		->bssid.sa_data));
					 */
					break;
				case IW_PMKSA_FLUSH:
					/*
					 *  printk(KERN_INFO
					 *	   "IW_PMKSA_FLUSH\n");
					 */
				{
					wext_support_ioctl_SIOCSIWPMKSA_Action(
							prDev, prExtraBuf,
							IW_PMKSA_FLUSH, &ret);
				}
				break;
				default:
					DBGLOG(INIT, INFO,
					       "UNKNOWN iw_pmksa command:%d\n",
					       ((struct iw_pmksa *)prExtraBuf)
					       ->cmd);
					ret = -EFAULT;
					break;
				}
			}

			if (prExtraBuf) {
				kalMemFree(prExtraBuf, VIR_MEM_TYPE,
					   u4ExtraSize);
				prExtraBuf = NULL;
			}
		} else if (iwr->u.data.length != 0) {
			ret = -EINVAL;
			break;
		}
#endif
		break;

#endif

	default:
		/* printk(KERN_NOTICE "unsupported IOCTL: 0x%x\n", i4Cmd); */
		ret = -EOPNOTSUPP;
		break;
	}

	/* printk("%d CMD:0x%x ret:%d\n", jiffies_to_msecs(jiffies),
	 *	  i4Cmd, ret);
	 */

	return ret;
}				/* wext_support_ioctl */

static void wext_support_ioctl_SIOCSIWGENIE(
	IN struct net_device *prDev, IN char *prExtraBuf,
	IN uint32_t u4ExtraSize)
{
	struct GLUE_INFO *prGlueInfo = *((struct GLUE_INFO **)
		netdev_priv(prDev));
	uint32_t rStatus;
	uint32_t u4BufLen;

#if CFG_SUPPORT_WAPI
	rStatus = kalIoctl(prGlueInfo, wlanoidSetWapiAssocInfo, prExtraBuf,
			   u4ExtraSize, FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		/* do nothing */
		/* printk(KERN_INFO
		 *	  "[WSC] set WSC assoc info error:%x\n",
		 *	  rStatus);
		 */
	}
#endif

}

static void
wext_support_ioctl_SIOCSIWPMKSA_Action(IN struct net_device
		*prDev, IN char *prExtraBuf, IN int ioMode, OUT int *ret)
{
	struct GLUE_INFO *prGlueInfo = *((struct GLUE_INFO **)
					 netdev_priv(prDev));
	uint32_t rStatus;
	uint32_t u4BufLen;
	struct PARAM_PMKID pmkid;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	pmkid.ucBssIdx = ucBssIndex;

	switch (ioMode) {
	case IW_PMKSA_ADD:
		kalMemCopy(pmkid.arBSSID,
			((struct iw_pmksa *)prExtraBuf)->bssid.sa_data,
			PARAM_MAC_ADDR_LEN);
		kalMemCopy(pmkid.arPMKID,
			((struct iw_pmksa *)prExtraBuf)->pmkid, IW_PMKID_LEN);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetPmkid, &pmkid,
				   sizeof(struct PARAM_PMKID),
				   FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, INFO, "add pmkid error:%x\n", rStatus);
		break;
	case IW_PMKSA_FLUSH:
		rStatus = kalIoctl(prGlueInfo, wlanoidFlushPmkid, NULL, 0,
				   FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, INFO, "flush pmkid error:%x\n", rStatus);
		break;
	default:
		break;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief To send an event (RAW socket pacekt) to user process actively.
 *
 * \param[in] prGlueInfo Glue layer info.
 * \param[in] u4cmd Which event command we want to indicate to user process.
 * \param[in] pData Data buffer to be indicated.
 * \param[in] dataLen Available data size in pData.
 *
 * \return (none)
 *
 * \note Event is indicated to upper layer if cmd is supported and data is
 *	 valid. Using of kernel symbol wireless_send_event(), which is defined
 *	 in <net/iw_handler.h> after WE-14 (2.4.20).
 */
/*----------------------------------------------------------------------------*/
void
wext_indicate_wext_event(IN struct GLUE_INFO *prGlueInfo,
			 IN unsigned int u4Cmd, IN unsigned char *pucData,
			 IN unsigned int u4dataLen,
			 IN uint8_t ucBssIndex)
{
	union iwreq_data wrqu;
	unsigned char *pucExtraInfo = NULL;
#if WIRELESS_EXT >= 15
	unsigned char *pucDesiredIE = NULL;
	unsigned char aucExtraInfoBuf[200];
#endif
#if WIRELESS_EXT < 18
	int i;
#endif
	struct GL_WPA_INFO *prWpaInfo;
	struct net_device *prDevHandler;

	memset(&wrqu, 0, sizeof(wrqu));

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		ucBssIndex);

	prDevHandler =
		wlanGetNetDev(prGlueInfo, ucBssIndex);

	switch (u4Cmd) {
	case SIOCGIWTXPOW:
		memcpy(&wrqu.power, pucData, u4dataLen);
		break;
	case SIOCGIWSCAN:
		complete_all(&prGlueInfo->rScanComp);
		break;

	case SIOCGIWAP:
		if (pucData)
			kalMemCopy(&wrqu.ap_addr.sa_data, pucData, ETH_ALEN);
		else
			eth_zero_addr((u8 *)&wrqu.ap_addr.sa_data);
		break;

	case IWEVASSOCREQIE:
#if WIRELESS_EXT < 15
		/* under WE-15, no suitable Event can be used */
		goto skip_indicate_event;
#else
		/* do supplicant a favor, parse to the start of WPA/RSN IE */
		if (wextSrchDesiredWPAIE(pucData, u4dataLen, 0x30,
					 &pucDesiredIE)) {
			/* RSN IE found */
			/* RSN IE found */
		}
#if 0
		else if (wextSrchDesiredWPSIE(pucData, u4dataLen, 0xDD,
					      &pucDesiredIE)) {
			/* WPS IE found */
			/* WPS IE found */
		}
#endif
		else if (wextSrchDesiredWPAIE(pucData, u4dataLen, 0xDD,
					      &pucDesiredIE)) {
			/* WPA IE found */
			/* WPA IE found */
		}
#if CFG_SUPPORT_WAPI		/* Android+ */
		else if (wextSrchDesiredWAPIIE(pucData, u4dataLen,
					       &pucDesiredIE)) {
			/* printk("wextSrchDesiredWAPIIE!!\n"); */
			/* WAPI IE found */
		}
#endif
		else {
			/* no WPA/RSN IE found, skip this event */
			goto skip_indicate_event;
		}

#if WIRELESS_EXT < 18
		/* under WE-18, only IWEVCUSTOM can be used */
		u4Cmd = IWEVCUSTOM;
		pucExtraInfo = aucExtraInfoBuf;
		pucExtraInfo += sprintf(pucExtraInfo, "ASSOCINFO(ReqIEs=");
		/* printk(KERN_DEBUG "assoc info buffer size needed:%d\n",
		 *        infoElemLen * 2 + 17);
		 */
		/* translate binary string to hex string, requirement of
		 * IWEVCUSTOM
		 */
		for (i = 0; i < pucDesiredIE[1] + 2; ++i)
			pucExtraInfo += sprintf(pucExtraInfo, "%02x",
						pucDesiredIE[i]);
		pucExtraInfo = aucExtraInfoBuf;
		wrqu.data.length = 17 + (pucDesiredIE[1] + 2) * 2;
#else
		/* IWEVASSOCREQIE, indicate binary string */
		pucExtraInfo = pucDesiredIE;
		wrqu.data.length = pucDesiredIE[1] + 2;
#endif
#endif /* WIRELESS_EXT < 15 */
		break;

	case IWEVMICHAELMICFAILURE:
#if WIRELESS_EXT < 15
		/* under WE-15, no suitable Event can be used */
		goto skip_indicate_event;
#else
		if (pucData) {
			struct PARAM_AUTH_REQUEST *pAuthReq =
					(struct PARAM_AUTH_REQUEST *) pucData;
			uint32_t nleft = 0, nsize = 0;
			/* under WE-18, only IWEVCUSTOM can be used */
			u4Cmd = IWEVCUSTOM;
			pucExtraInfo = aucExtraInfoBuf;
			nleft = sizeof(aucExtraInfoBuf);
			nsize = snprintf(pucExtraInfo, nleft,
					 "MLME-MICHAELMICFAILURE.indication ");
			if (nsize < nleft) {
				nleft -= nsize;
				pucExtraInfo += nsize;
			}

			nsize = snprintf(pucExtraInfo,
					 nleft, "%s",
					 (pAuthReq->u4Flags ==
					 PARAM_AUTH_REQUEST_GROUP_ERROR) ?
					 "groupcast " : "unicast ");

			if (nsize < nleft) {
				nleft -= nsize;
				pucExtraInfo += nsize;
			}
			wrqu.data.length = sizeof(aucExtraInfoBuf) - nleft;
			pucExtraInfo = aucExtraInfoBuf;
		}
#endif /* WIRELESS_EXT < 15 */
		break;

	case IWEVPMKIDCAND:
		if (prWpaInfo->u4WpaVersion ==
		    IW_AUTH_WPA_VERSION_WPA2 &&
		    prWpaInfo->u4KeyMgmt == IW_AUTH_KEY_MGMT_802_1X) {

			/* only used in WPA2 */
#if WIRELESS_EXT >= 18
			struct PARAM_PMKID_CANDIDATE *prPmkidCand =
				(struct PARAM_PMKID_CANDIDATE *) pucData;

			struct iw_pmkid_cand rPmkidCand;

			pucExtraInfo = aucExtraInfoBuf;

			rPmkidCand.flags = prPmkidCand->u4Flags;
			rPmkidCand.index = 0;
			kalMemCopy(rPmkidCand.bssid.sa_data,
				   prPmkidCand->arBSSID, 6);

			kalMemCopy(pucExtraInfo, (uint8_t *) &rPmkidCand,
				   sizeof(struct iw_pmkid_cand));
			wrqu.data.length = sizeof(struct iw_pmkid_cand);

			/* pmkid canadidate list is supported after WE-18 */
			/* indicate struct iw_pmkid_cand */
#else
			/* printk(KERN_INFO
			 *        "IWEVPMKIDCAND event skipped, WE < 18\n");
			 */
			goto skip_indicate_event;
#endif
		} else {
			/* printk(KERN_INFO
			 *        "IWEVPMKIDCAND event skipped, NOT WPA2\n");
			 */
			goto skip_indicate_event;
		}
		break;

	case IWEVCUSTOM:
		u4Cmd = IWEVCUSTOM;
		pucExtraInfo = aucExtraInfoBuf;
		kalMemCopy(pucExtraInfo, pucData, sizeof(struct PTA_IPC));
		wrqu.data.length = sizeof(struct PTA_IPC);
		break;

	default:
		/* printk(KERN_INFO "Unsupported wext event:%x\n", cmd); */
		goto skip_indicate_event;
	}

	/* Send event to user space */
	wireless_send_event(prDevHandler, u4Cmd, &wrqu,
			    pucExtraInfo);
skip_indicate_event:
	return;
} /* wext_indicate_wext_event */

/*----------------------------------------------------------------------------*/
/*!
 * \brief A method of struct net_device, to get the network interface
 *	  statistical information.
 *
 * Whenever an application needs to get statistics for the interface, this
 * method is called. This happens, for example, when ifconfig or netstat -i is
 * run.
 *
 * \param[in] pDev Pointer to struct net_device.
 *
 * \return net_device_stats buffer pointer.
 *
 */
/*----------------------------------------------------------------------------*/
struct iw_statistics *wext_get_wireless_stats(
	struct net_device *prDev)
{

	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct iw_statistics *pStats = NULL;
	struct PARAM_LINK_SPEED_EX rLinkSpeed;
	uint32_t bufLen = 0;
	uint8_t ucBssIndex = AIS_DEFAULT_INDEX;

	prGlueInfo = *((struct GLUE_INFO **) netdev_priv(prDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo)
		goto stat_out;

	ucBssIndex = wlanGetBssIdx(prDev);
	if (IS_BSS_INDEX_VALID(ucBssIndex))
		pStats = (struct iw_statistics *)
			(&(prGlueInfo->rIwStats[ucBssIndex]));

	if (!prDev || !netif_carrier_ok(prDev)) {
		/* network not connected */
		goto stat_out;
	}

	rStatus = kalIoctlByBssIdx(prGlueInfo,
				   wlanoidQueryRssi,
				   &rLinkSpeed, sizeof(rLinkSpeed),
				   TRUE, TRUE, TRUE,
				   &bufLen, ucBssIndex);

stat_out:
	return pStats;
}				/* wlan_get_wireless_stats */


/* Standard call implementations */
static int std_get_name(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_name(prDev, NULL, (char *)(&(prData->name)),
				    sizeof(prData->name), NULL);
}

static int std_set_freq(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_freq(prDev, NULL, &(prData->freq), NULL);
}

static int std_get_freq(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_freq(prDev, NULL, &(prData->freq), NULL);
}

static int std_set_mode(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_mode(prDev, NULL, &prData->mode, NULL);
}

static int std_get_mode(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_mode(prDev, NULL, &prData->mode, NULL);
}

static int std_set_ap(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	int ret = 0;

	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");

	if (prData->ap_addr.sa_data[0] == 0 &&
		    prData->ap_addr.sa_data[1] == 0 &&
		    prData->ap_addr.sa_data[2] == 0 &&
		    prData->ap_addr.sa_data[3] == 0 &&
		    prData->ap_addr.sa_data[4] == 0
		    && prData->ap_addr.sa_data[5] == 0) {
			/* WPA Supplicant will set 000000000000 in
			 * wpa_driver_wext_deinit(), do nothing here or
			 * disassoc again?
			 */
		ret = 0;
	} else {
		ret = wext_set_ap(prDev, NULL, &(prData->ap_addr), NULL);
	}
	return ret;
}

static int std_get_ap(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_ap(prDev, NULL, &(prData->ap_addr), NULL);
}

static int std_get_rate(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_rate(prDev, NULL, &prData->bitrate, NULL);
}

static int std_set_rts(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_rts(prDev, NULL, &(prData->rts), NULL);
}

static int std_get_rts(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_rts(prDev, NULL, &prData->rts, NULL);
}

static int std_get_frag(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_frag(prDev, NULL, &prData->frag, NULL);
}

static int std_set_txpow(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_txpow(prDev, NULL, &(prData->txpower), NULL);
}

static int std_get_txpow(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_txpow(prDev, NULL, &prData->txpower, NULL);
}

static int std_set_power(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_power(prDev, NULL, &prData->power, NULL);
}

static int std_get_power(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_power(prDev, NULL, &prData->power, NULL);
}

static int std_get_range(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_range(prDev, NULL, &(prData->data),
				pcExtra);
}

static int std_set_priv(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
#ifdef CONFIG_COMPAT
	if (rIwReqInfo->flags & IW_REQUEST_FLAG_COMPAT) {
		int ret = 0;
		struct compat_iw_point *iwp_compat = NULL;
		struct iw_point iwp;

		iwp_compat = (struct compat_iw_point *) &prData->data;
		iwp.pointer = compat_ptr(iwp_compat->pointer);
		iwp.length = iwp_compat->length;
		iwp.flags = iwp_compat->flags;

		ret = wext_set_country(prDev, &iwp);

		iwp_compat->pointer = ptr_to_compat(iwp.pointer);
		iwp_compat->length = iwp.length;
		iwp_compat->flags = iwp.flags;

		return ret;
	}
#endif /* CONFIG_COMPAT */
	return wext_set_country(prDev, &(prData->data));
}

static int std_get_priv(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_priv(prDev, &(prData->data));
}

static int std_set_scan(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_scan(prDev, NULL, NULL, NULL);
}

static int std_set_mlme(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_mlme(prDev, NULL, &(prData->data), pcExtra);
}

static int std_get_scan(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_scan(prDev, NULL, &(prData->data), pcExtra);
}

static int std_set_essid(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_essid(prDev, NULL, &(prData->essid), pcExtra);
}

static int std_get_essid(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_essid(prDev, NULL, &(prData->essid), pcExtra);
}

static int std_set_encode(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_encode(prDev, NULL,
			  &(prData->encoding),
			  pcExtra);
}

static int std_get_encode(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_get_encode(prDev, NULL, &(prData->encoding), NULL);
}

static int std_set_auth(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_auth(prDev, NULL, &(prData->param), NULL);
}

#if WIRELESS_EXT > 17
static int std_set_genie(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	uint32_t u4ExtraSize = prData->data.length;
	struct GLUE_INFO *prGlueInfo = NULL;

	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");

#if CFG_SUPPORT_WAPI
	/* The max wapi ie buffer */
	if (u4ExtraSize > 42)
		return -EINVAL;
#endif

	if (prData->data.pointer) {
		u4ExtraSize = prData->data.length;
		prGlueInfo = *((struct GLUE_INFO **)
						 netdev_priv(prDev));
		wext_support_ioctl_SIOCSIWGENIE(
			prDev, pcExtra,
			u4ExtraSize);
	}

	return 0;
}
#endif /* end of WIRELESS_EXT */

static int std_set_encode_ext(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");
	return wext_set_encode_ext(prDev, NULL, &(prData->encoding),
						  pcExtra);
}

static int std_set_pmska(struct net_device *prDev,
		struct iw_request_info *rIwReqInfo,
		union iwreq_data *prData,
		char *pcExtra)
{
	int ret = 0;

	DBGLOG(INIT, INFO, " mtk std ioctl is called.\n");

	switch (((struct iw_pmksa *)pcExtra)->cmd) {
	case IW_PMKSA_ADD:
		wext_support_ioctl_SIOCSIWPMKSA_Action(
				prDev, pcExtra,
				IW_PMKSA_ADD, &ret);
		break;
	case IW_PMKSA_REMOVE:
		break;
	case IW_PMKSA_FLUSH:
		wext_support_ioctl_SIOCSIWPMKSA_Action(
				prDev, pcExtra,
				IW_PMKSA_FLUSH, &ret);
		break;
	default:
		DBGLOG(INIT, INFO,
		       "UNKNOWN iw_pmksa command:%d\n",
		       ((struct iw_pmksa *)pcExtra)
		       ->cmd);
		ret = -EFAULT;
		break;
	}
	return ret;
}


#endif /* WIRELESS_EXT */
