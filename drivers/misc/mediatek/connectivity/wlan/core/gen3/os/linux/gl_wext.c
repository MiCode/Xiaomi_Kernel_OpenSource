/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/gl_wext.c#5
*/

/*
 * ! \file gl_wext.c
 * \brief  ioctl() (mostly Linux Wireless Extensions) routines for STA driver.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 ********************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ********************************************************************************
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
 ********************************************************************************
 */
const long channel_freq[] = {
	2412, 2417, 2422, 2427, 2432, 2437, 2442,
	2447, 2452, 2457, 2462, 2467, 2472, 2484
};

#define NUM_CHANNELS (ARRAY_SIZE(channel_freq))

#define MAX_SSID_LEN		32
#define COUNTRY_CODE_LEN	10	/* country code length */

/*******************************************************************************
 *                             D A T A   T Y P E S
 ********************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 ********************************************************************************
 */
/* NOTE: name in iwpriv_args only have 16 bytes */
static const struct iw_priv_args rIwPrivTable[] = {
	{IOCTL_SET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, ""},
	{IOCTL_GET_INT, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, ""},
	{IOCTL_SET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, ""},
	{IOCTL_GET_INT, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, ""},
	{IOCTL_SET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, ""},

	{IOCTL_GET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, ""},
	{IOCTL_GET_INT, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, ""},

	{IOCTL_SET_INTS, IW_PRIV_TYPE_INT | 4, 0, ""},
	{IOCTL_GET_INT, 0, IW_PRIV_TYPE_INT | 50, ""},

	/* added for set_oid and get_oid */
	{IOCTL_SET_STRUCT, 256, 0, ""},
	{IOCTL_GET_STRUCT, 0, 256, ""},

	{IOCTL_GET_DRIVER, IW_PRIV_TYPE_CHAR | 2000, IW_PRIV_TYPE_CHAR | 2000, "driver"},

#if CFG_SUPPORT_QA_TOOL
	/* added for ATE iwpriv Command */
	{IOCTL_IWPRIV_ATE, IW_PRIV_TYPE_CHAR | 2000, 0, ""},
#endif

	/* sub-ioctl definitions */
#if 0
	{PRIV_CMD_REG_DOMAIN, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_reg_domain"},
	{PRIV_CMD_REG_DOMAIN, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_reg_domain"},
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	{PRIV_CMD_CSUM_OFFLOAD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_tcp_csum"},
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

	{PRIV_CMD_POWER_MODE, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_power_mode"},
	{PRIV_CMD_POWER_MODE, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_power_mode"},

	{PRIV_CMD_WMM_PS, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "set_wmm_ps"},

	{PRIV_CMD_TEST_MODE, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_test_mode"},
	{PRIV_CMD_TEST_CMD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_test_cmd"},
	{PRIV_CMD_TEST_CMD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_test_result"},
#if CFG_SUPPORT_PRIV_MCR_RW
	{PRIV_CMD_ACCESS_MCR, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_mcr"},
	{PRIV_CMD_ACCESS_MCR, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mcr"},
#endif
#if CFG_SUPPORT_QA_TOOL
	{PRIV_QACMD_SET, IW_PRIV_TYPE_CHAR | 2000, 0, "set"},
#endif
	{PRIV_CMD_SW_CTRL, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_sw_ctrl"},
	{PRIV_CMD_SW_CTRL, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_sw_ctrl"},

#if CFG_SUPPORT_BCM && CFG_SUPPORT_BCM_BWCS
	{PRIV_CUSTOM_BWCS_CMD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_bwcs"},
	/* GET STRUCT sub-ioctls commands */
	{PRIV_CUSTOM_BWCS_CMD, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_bwcs"},
#endif

	/* SET STRUCT sub-ioctls commands */
	{PRIV_CMD_OID, 256, 0, "set_oid"},
	/* GET STRUCT sub-ioctls commands */
	{PRIV_CMD_OID, 0, 256, "get_oid"},

	{PRIV_CMD_BAND_CONFIG, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_band"},
	{PRIV_CMD_BAND_CONFIG, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_band"},

	{PRIV_CMD_SET_TX_POWER, IW_PRIV_TYPE_INT | 4, 0, "set_txpower"},
	{PRIV_CMD_GET_CH_LIST, 0, IW_PRIV_TYPE_INT | 50, "get_ch_list"},
	{PRIV_CMD_DUMP_MEM, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
	 IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_mem"},

#if CFG_ENABLE_WIFI_DIRECT
	{PRIV_CMD_P2P_MODE, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_p2p_mode"},
#endif
	{PRIV_CMD_MET_PROFILING, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "set_met_prof"},

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
#if CFG_SUPPORT_QA_TOOL
	[IOCTL_IWPRIV_ATE - SIOCIWFIRSTPRIV] = priv_ate_set
#endif
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
static const iw_handler rIwStdHandler[] = {
	IW_HANDLER(SIOCGIWNAME, std_wext_get_name),
	IW_HANDLER(SIOCSIWFREQ, std_wext_set_freq),
	IW_HANDLER(SIOCGIWFREQ, std_wext_get_freq),
	IW_HANDLER(SIOCSIWMODE, std_wext_set_mode),
	IW_HANDLER(SIOCGIWMODE, std_wext_get_mode),
	IW_HANDLER(SIOCGIWRANGE, std_wext_get_range),
	IW_HANDLER(SIOCSIWPRIV, std_wext_set_country),
	IW_HANDLER(SIOCGIWPRIV, std_wext_get_priv),
	IW_HANDLER(SIOCSIWAP, std_wext_set_ap),
	IW_HANDLER(SIOCGIWAP, std_wext_get_ap),
	IW_HANDLER(SIOCSIWMLME, std_wext_set_mlme),
	IW_HANDLER(SIOCSIWSCAN, std_wext_set_scan),
	IW_HANDLER(SIOCGIWSCAN, std_wext_get_scan),
	IW_HANDLER(SIOCSIWESSID, std_wext_set_essid),
	IW_HANDLER(SIOCGIWESSID, std_wext_get_essid),
	IW_HANDLER(SIOCGIWRATE, std_wext_get_rate),
	IW_HANDLER(SIOCSIWRTS, std_wext_set_rts),
	IW_HANDLER(SIOCGIWRTS, std_wext_get_rts),
	IW_HANDLER(SIOCGIWFRAG, std_wext_set_rts),
	IW_HANDLER(SIOCSIWTXPOW, std_wext_set_txpow),
	IW_HANDLER(SIOCGIWTXPOW, std_wext_get_txpow),
	IW_HANDLER(SIOCSIWENCODE, std_wext_set_encode),
	IW_HANDLER(SIOCGIWENCODE, std_wext_get_encode),
	IW_HANDLER(SIOCSIWPOWER, std_wext_set_power),
	IW_HANDLER(SIOCGIWPOWER, std_wext_get_power),
#if WIRELESS_EXT > 17
	IW_HANDLER(SIOCSIWGENIE, std_wext_SIOCSIWGENIE_Action),
#endif
	IW_HANDLER(SIOCSIWAUTH, std_wext_set_auth),
	IW_HANDLER(SIOCSIWENCODEEXT, std_wext_set_encode_ext),
	IW_HANDLER(SIOCSIWPMKSA, std_wext_SIOCSIWPMKSA_Action),
};
#endif

const struct iw_handler_def wext_handler_def = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
	.num_standard = (__u16) sizeof(rIwStdHandler) / sizeof(iw_handler),
#else
	.num_standard = 0,
#endif
#if defined(CONFIG_WEXT_PRIV)

	.num_private = (__u16) sizeof(rIwPrivHandler) / sizeof(iw_handler),
	.num_private_args = (__u16) sizeof(rIwPrivTable) / sizeof(struct iw_priv_args),
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
	.standard = rIwStdHandler,
#else
	.standard = (iw_handler *)NULL,
#endif
#if defined(CONFIG_WEXT_PRIV)

	.private = rIwPrivHandler,
	.private_args = rIwPrivTable,
#endif
	.get_wireless_stats = wext_get_wireless_stats,
};

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

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
static void wext_support_ioctl_SIOCSIWGENIE(IN P_GLUE_INFO_T prGlueInfo, IN char *prExtraBuf, IN UINT_32 u4ExtraSize);

static void
wext_support_ioctl_SIOCSIWPMKSA_Action(IN struct net_device *prDev, IN char *prExtraBuf, IN int ioMode, OUT int *ret);

/*----------------------------------------------------------------------------*/
/*!
* \brief Find the desired WPA/RSN Information Element according to desiredElemID.
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
BOOLEAN
wextSrchDesiredWPAIE(IN PUINT_8 pucIEStart,
		     IN INT_32 i4TotalIeLen, IN UINT_8 ucDesiredElemId, OUT PUINT_8 *ppucDesiredIE)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (INT_32) pucIEStart[1] + 2;

		if (pucIEStart[0] == ucDesiredElemId && i4InfoElemLen <= i4TotalIeLen) {
			if (ucDesiredElemId != 0xDD) {
				/* Non 0xDD, OK! */
				*ppucDesiredIE = &pucIEStart[0];
				return TRUE;
			}
			/* EID == 0xDD, check WPA IE */
			if (pucIEStart[1] >= 4) {
				if (memcmp(&pucIEStart[2], "\x00\x50\xf2\x01", 4) == 0) {
					*ppucDesiredIE = &pucIEStart[0];
					return TRUE;
				}
			}	/* check WPA IE length */
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
BOOLEAN wextSrchDesiredWAPIIE(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PUINT_8 *ppucDesiredIE)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (INT_32) pucIEStart[1] + 2;

		if (pucIEStart[0] == ELEM_ID_WAPI && i4InfoElemLen <= i4TotalIeLen) {
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
* \brief Check if exist the desired HS2.0 Information Element according to desiredElemID.
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
BOOLEAN wextIsDesiredHS20IE(IN PUINT_8 pucCurIE, IN INT_32 i4TotalIeLen)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucCurIE);

	i4InfoElemLen = (INT_32) pucCurIE[1] + 2;

	if (pucCurIE[0] == ELEM_ID_VENDOR && i4InfoElemLen <= i4TotalIeLen) {
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
* \brief Check if exist the desired interworking Information Element according to desiredElemID.
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
BOOLEAN wextIsDesiredInterworkingIE(IN PUINT_8 pucCurIE, IN INT_32 i4TotalIeLen)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucCurIE);

	i4InfoElemLen = (INT_32) pucCurIE[1] + 2;

	if (pucCurIE[0] == ELEM_ID_INTERWORKING && i4InfoElemLen <= i4TotalIeLen) {
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
* \brief Check if exist the desired Adv Protocol Information Element according to desiredElemID.
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
BOOLEAN wextIsDesiredAdvProtocolIE(IN PUINT_8 pucCurIE, IN INT_32 i4TotalIeLen)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucCurIE);

	i4InfoElemLen = (INT_32) pucCurIE[1] + 2;

	if (pucCurIE[0] == ELEM_ID_ADVERTISEMENT_PROTOCOL && i4InfoElemLen <= i4TotalIeLen)
		return TRUE;
	/* check desired EID */
	return FALSE;
}				/* wextIsDesiredAdvProtocolIE */

/*----------------------------------------------------------------------------*/
/*!
* \brief Check if exist the desired Roaming Consortium Information Element according to desiredElemID.
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
BOOLEAN wextIsDesiredRoamingConsortiumIE(IN PUINT_8 pucCurIE, IN INT_32 i4TotalIeLen)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucCurIE);

	i4InfoElemLen = (INT_32) pucCurIE[1] + 2;

	if (pucCurIE[0] == ELEM_ID_ROAMING_CONSORTIUM && i4InfoElemLen <= i4TotalIeLen)
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
BOOLEAN wextSrchDesiredHS20IE(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PUINT_8 *ppucDesiredIE)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (INT_32) pucIEStart[1] + 2;

		if (pucIEStart[0] == ELEM_ID_VENDOR && i4InfoElemLen <= i4TotalIeLen) {
			if (pucIEStart[1] >= ELEM_MIN_LEN_HS20_INDICATION) {
				if (memcmp(&pucIEStart[2], "\x50\x6f\x9a\x10", 4) == 0) {
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
* \brief Find the desired interworking Information Element according to desiredElemID.
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
BOOLEAN wextSrchDesiredInterworkingIE(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PUINT_8 *ppucDesiredIE)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (INT_32) pucIEStart[1] + 2;

		if (pucIEStart[0] == ELEM_ID_INTERWORKING && i4InfoElemLen <= i4TotalIeLen) {
			*ppucDesiredIE = &pucIEStart[0];
			return TRUE;
		}

		/* check desired EID */
		/* Select next information element. */
		i4TotalIeLen -= i4InfoElemLen;
		pucIEStart += i4InfoElemLen;
	}

	return FALSE;
}				/* wextSrchDesiredInterworkingIE */

/*----------------------------------------------------------------------------*/
/*!
* \brief Find the desired Adv Protocol Information Element according to desiredElemID.
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
BOOLEAN wextSrchDesiredAdvProtocolIE(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PUINT_8 *ppucDesiredIE)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (INT_32) pucIEStart[1] + 2;

		if (pucIEStart[0] == ELEM_ID_ADVERTISEMENT_PROTOCOL && i4InfoElemLen <= i4TotalIeLen) {
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

/*----------------------------------------------------------------------------*/
/*!
* \brief Find the desired Roaming Consortium Information Element according to desiredElemID.
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
BOOLEAN wextSrchDesiredRoamingConsortiumIE(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PUINT_8 *ppucDesiredIE)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (INT_32) pucIEStart[1] + 2;

		if (pucIEStart[0] == ELEM_ID_ROAMING_CONSORTIUM && i4InfoElemLen <= i4TotalIeLen) {
			*ppucDesiredIE = &pucIEStart[0];
			return TRUE;
		}

		/* check desired EID */
		/* Select next information element. */
		i4TotalIeLen -= i4InfoElemLen;
		pucIEStart += i4InfoElemLen;
	}

	return FALSE;
}				/* wextSrchDesiredRoamingConsortiumIE */

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
BOOLEAN
wextSrchDesiredWPSIE(IN PUINT_8 pucIEStart,
		     IN INT_32 i4TotalIeLen, IN UINT_8 ucDesiredElemId, OUT PUINT_8 *ppucDesiredIE)
{
	INT_32 i4InfoElemLen;

	ASSERT(pucIEStart);
	ASSERT(ppucDesiredIE);

	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (INT_32) pucIEStart[1] + 2;

		if (pucIEStart[0] == ucDesiredElemId && i4InfoElemLen <= i4TotalIeLen) {
			if (ucDesiredElemId != 0xDD) {
				/* Non 0xDD, OK! */
				*ppucDesiredIE = &pucIEStart[0];
				return TRUE;
			}

			/* EID == 0xDD, check WPS IE */
			if (pucIEStart[1] >= 4) {
				if (memcmp(&pucIEStart[2], "\x00\x50\xf2\x04", 4) == 0) {
					*ppucDesiredIE = &pucIEStart[0];
					return TRUE;
				}
			}	/* check WPS IE length */
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
wext_get_name(IN struct net_device *prNetDev, IN struct iw_request_info *prIwrInfo, OUT char *pcName,
	      IN size_t szNameSize, IN char *pcExtra)
{
	ENUM_PARAM_NETWORK_TYPE_T eNetWorkType;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(pcName);
	if (GLUE_CHK_PR2(prNetDev, pcName) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (netif_carrier_ok(prNetDev)) {

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryNetworkTypeInUse,
				   &eNetWorkType, sizeof(eNetWorkType), TRUE, FALSE, FALSE, &u4BufLen);

		switch (eNetWorkType) {
		case PARAM_NETWORK_TYPE_DS:
			strncpy(pcName, "IEEE 802.11b", szNameSize);
			break;
		case PARAM_NETWORK_TYPE_OFDM24:
			strncpy(pcName, "IEEE 802.11bgn", szNameSize);
			break;
		case PARAM_NETWORK_TYPE_AUTOMODE:
		case PARAM_NETWORK_TYPE_OFDM5:
			strncpy(pcName, "IEEE 802.11abgn", szNameSize);
			break;
		case PARAM_NETWORK_TYPE_FH:
		default:
			strncpy(pcName, "IEEE 802.11", szNameSize);
			break;
		}
	} else {
		strncpy(pcName, "Disconnected", szNameSize);
	}

	pcName[szNameSize - 1] = '\0';

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
	      IN struct iw_request_info *prIwReqInfo, IN struct iw_freq *prIwFreq, IN char *pcExtra)
{

#if 0
	UINT_32 u4ChnlFreq;	/* Store channel or frequency information */

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prIwFreq);
	if (GLUE_CHK_PR2(prNetDev, prIwFreq) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* If setting by frequency, convert to a channel */
	if ((prIwFreq->e == 1) && (prIwFreq->m >= (int)2.412e8) && (prIwFreq->m <= (int)2.484e8)) {

		/* Change to KHz format */
		u4ChnlFreq = (UINT_32) (prIwFreq->m / (KILO / 10));

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetFrequency,
				   &u4ChnlFreq, sizeof(u4ChnlFreq), FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EINVAL;
	}
	/* Setting by channel number */
	else if ((prIwFreq->m > KILO) || (prIwFreq->e > 0))
		return -EOPNOTSUPP;

	/* Change to channel number format */
	u4ChnlFreq = (UINT_32) prIwFreq->m;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetChannel, &u4ChnlFreq, sizeof(u4ChnlFreq), FALSE, FALSE, FALSE, &u4BufLen);

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
	      IN struct iw_request_info *prIwrInfo, OUT struct iw_freq *prIwFreq, IN char *pcExtra)
{
	UINT_32 u4Channel = 0;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prIwFreq);
	if (GLUE_CHK_PR2(prNetDev, prIwFreq) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* GeorgeKuo: TODO skip checking in IBSS mode */
	if (!netif_carrier_ok(prNetDev))
		return -ENOTCONN;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryFrequency, &u4Channel, sizeof(u4Channel), TRUE, FALSE, FALSE, &u4BufLen);

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
	      IN struct iw_request_info *prIwReqInfo, IN unsigned int *pu4Mode, IN char *pcExtra)
{
	ENUM_PARAM_OP_MODE_T eOpMode;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(pu4Mode);
	if (GLUE_CHK_PR2(prNetDev, pu4Mode) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	switch (*pu4Mode) {
	case IW_MODE_AUTO:
		eOpMode = NET_TYPE_AUTO_SWITCH;
		break;

	case IW_MODE_ADHOC:
		eOpMode = NET_TYPE_IBSS;
		break;

	case IW_MODE_INFRA:
		eOpMode = NET_TYPE_INFRA;
		break;

	default:
		DBGLOG(INIT, INFO, "%s(): Set UNSUPPORTED Mode = %d.\n", __func__, *pu4Mode);
		return -EOPNOTSUPP;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetInfrastructureMode, &eOpMode, sizeof(eOpMode), FALSE, FALSE, TRUE, &u4BufLen);

	/* after set operation mode, key table are cleared */

	/* reset wpa info */
	prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prGlueInfo->rWpaInfo.u4KeyMgmt = 0;
	prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
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
	      IN struct iw_request_info *prIwReqInfo, OUT unsigned int *pu4Mode, IN char *pcExtra)
{
	ENUM_PARAM_OP_MODE_T eOpMode;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(pu4Mode);
	if (GLUE_CHK_PR2(prNetDev, pu4Mode) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryInfrastructureMode, &eOpMode, sizeof(eOpMode), TRUE, FALSE, FALSE, &u4BufLen);

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
*                     function, wext_support_ioctl() or ioctl_standard_call() in
*                     wireless.c.
*
* \retval 0 If data is valid.
*
* \note The extra buffer (pcExtra) is filled with information from driver.
*/
/*----------------------------------------------------------------------------*/
static int
wext_get_range(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo, IN struct iw_point *prData, OUT char *pcExtra)
{
	struct iw_range *prRange = NULL;
	PARAM_RATES_EX aucSuppRate = { 0 };	/* data buffers */
	int i = 0;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR2(prNetDev, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	prRange = (struct iw_range *)pcExtra;

	memset(prRange, 0, sizeof(*prRange));
	prRange->throughput = 20000000;	/* 20Mbps */
	prRange->min_nwid = 0;	/* not used */
	prRange->max_nwid = 0;	/* not used */

	/* scan_capa not implemented */

	/* event_capa[6]: kernel + driver capabilities */
	prRange->event_capa[0] = (IW_EVENT_CAPA_K_0 | IW_EVENT_CAPA_MASK(SIOCGIWAP)
				  | IW_EVENT_CAPA_MASK(SIOCGIWSCAN)
				  /* can't display meaningful string in iwlist
				   * | IW_EVENT_CAPA_MASK(SIOCGIWTXPOW)
				   * | IW_EVENT_CAPA_MASK(IWEVMICHAELMICFAILURE)
				   * | IW_EVENT_CAPA_MASK(IWEVASSOCREQIE)
				   * | IW_EVENT_CAPA_MASK(IWEVPMKIDCAND)
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
			   &aucSuppRate, sizeof(aucSuppRate), TRUE, FALSE, FALSE, &u4BufLen);

	for (i = 0; i < IW_MAX_BITRATES && i < PARAM_MAX_LEN_RATES_EX; i++) {
		if (aucSuppRate[i] == 0)
			break;
		prRange->bitrate[i] = (aucSuppRate[i] & 0x7F) * 500000;	/* 0.5Mbps */
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
	prRange->pmp_flags = IW_POWER_RELATIVE;	/* pm default flag */
	prRange->pmt_flags = IW_POWER_ON;	/* pm timeout flag */
	prRange->pm_capa = IW_POWER_ON;	/* power management by driver */

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
	prRange->max_qual.level = (__u8) (0x100 - 0);	/* max 0 dbm */
	prRange->max_qual.noise = (__u8) (0x100 - 0);	/* max 0 dbm */

	/* enc_capa */
#if WIRELESS_EXT > 17
	prRange->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 | IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;
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
	    IN struct iw_request_info *prIwrInfo, IN struct sockaddr *prAddr, IN char *pcExtra)
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
	    IN struct iw_request_info *prIwrInfo, OUT struct sockaddr *prAddr, IN char *pcExtra)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prAddr);
	if (GLUE_CHK_PR2(prNetDev, prAddr) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* if (!netif_carrier_ok(prNetDev)) { */
	/* return -ENOTCONN; */
	/* } */

	if (prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_DISCONNECTED) {
		memset(prAddr, 0, sizeof(*prAddr));
		return 0;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryBssid, prAddr->sa_data, ETH_ALEN, TRUE, FALSE, FALSE, &u4BufLen);

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
	      IN struct iw_request_info *prIwrInfo, IN struct iw_point *prData, IN char *pcExtra)
{
	struct iw_mlme *prMlme = NULL;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR2(prNetDev, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	prMlme = (struct iw_mlme *)pcExtra;
	if (prMlme->cmd == IW_MLME_DEAUTH || prMlme->cmd == IW_MLME_DISASSOC) {
		if (!netif_carrier_ok(prNetDev)) {
			DBGLOG(INIT, INFO, "[wifi] Set MLME Deauth/Disassoc, but netif_carrier_off\n");
			return 0;
		}

		rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);
		return 0;
	}

	DBGLOG(INIT, INFO, "[wifi] unsupported IW_MLME_ command :%d\n", prMlme->cmd);
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
	      IN struct iw_request_info *prIwrInfo, IN struct iw_scan_req *prIwScanReq, IN char *pcExtra)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	int essid_len = 0;

	ASSERT(prNetDev);
	if (GLUE_CHK_DEV(prNetDev) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

#if WIRELESS_EXT > 17
	/* retrieve SSID */
	if (prIwScanReq)
		essid_len = prIwScanReq->essid_len;
#endif

	init_completion(&prGlueInfo->rScanComp);

	/* TODO:  parse flags and issue different scan requests? */

	rStatus = kalIoctl(prGlueInfo, wlanoidSetBssidListScan, pcExtra, essid_len, FALSE, FALSE, FALSE, &u4BufLen);

	/* wait_for_completion_interruptible_timeout(&prGlueInfo->rScanComp, 2 * KAL_HZ); */
	/* kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_SCAN_COMPLETE, NULL, 0); */

	return 0;
}				/* wext_set_scan */

/*----------------------------------------------------------------------------*/
/*!
* \brief To write the ie to buffer
*
*/
/*----------------------------------------------------------------------------*/
static inline int snprintf_hex(char *buf, size_t buf_size, const u8 *data, size_t len)
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
* \param[out] prData Pointer to iw_point structure, pData->length is the size of
*               pcExtra buffer before used, and is updated after filling scan
*               results.
* \param[out] pcExtra Pointer to buffer which is allocated by caller of this
*                     function, wext_support_ioctl() or ioctl_standard_call() in
*                     wireless.c.
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
	      IN struct iw_request_info *prIwrInfo, IN OUT struct iw_point *prData, IN char *pcExtra)
{
	UINT_32 i = 0;
	UINT_32 j = 0;
	P_PARAM_BSSID_LIST_EX_T prList = NULL;
	P_PARAM_BSSID_EX_T prBss = NULL;
	P_PARAM_VARIABLE_IE_T prDesiredIE = NULL;
	struct iw_event iwEvent;	/* local iw_event buffer */

	/* write pointer of extra buffer */
	char *pcCur = NULL;
	/* pointer to the end of  last full entry in extra buffer */
	char *pcValidEntryEnd = NULL;
	char *pcEnd = NULL;	/* end of extra buffer */

	UINT_32 u4AllocBufLen = 0;

	/* arrange rate information */
	UINT_32 u4HighestRate = 0;
	char aucRatesBuf[64];
	UINT_32 u4BufIndex;

	/* return value */
	int ret = 0;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prData);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prData, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* Initialize local variables */
	pcCur = pcExtra;
	pcValidEntryEnd = pcExtra;
	pcEnd = pcExtra + prData->length;	/* end of extra buffer */

	/* Allocate another query buffer with the same size of extra buffer */
	u4AllocBufLen = prData->length;
	prList = kalMemAlloc(u4AllocBufLen, VIR_MEM_TYPE);
	if (prList == NULL) {
		DBGLOG(INIT, INFO, "[wifi] no memory for scan list:%d\n", prData->length);
		ret = -ENOMEM;
		goto error;
	}
	prList->u4NumberOfItems = 0;

	/* wait scan done */
	/* wait_for_completion_interruptible_timeout(&prGlueInfo->rScanComp, 4 * KAL_HZ); */

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryBssidList, prList, u4AllocBufLen, TRUE, FALSE, FALSE, &u4BufLen);

	if (rStatus == WLAN_STATUS_INVALID_LENGTH) {

#if WIRELESS_EXT >= 17
		/* This feature is supported in WE-17 or above, limited by iwlist.
		 ** Return -E2BIG and iwlist will request again with a larger buffer.
		 */
		ret = -E2BIG;
		/* Update length to give application a hint on result length */
		prData->length = (__u16) u4BufLen;
		goto error;
#else
		/* Realloc a larger query buffer here, but don't write too much to extra
		 ** buffer when filling it later.
		 */
		kalMemFree(prList, VIR_MEM_TYPE, u4AllocBufLen);

		u4AllocBufLen = u4BufLen;
		prList = kalMemAlloc(u4AllocBufLen, VIR_MEM_TYPE);
		if (prList == NULL) {
			DBGLOG(INIT, INFO, "[wifi] no memory for larger scan list :%ld\n", u4BufLen);
			ret = -ENOMEM;
			goto error;
		}
		prList->NumberOfItems = 0;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryBssidList, prList, u4AllocBufLen, TRUE, FALSE, FALSE, &u4BufLen);

		if (rStatus == WLAN_STATUS_INVALID_LENGTH) {
			DBGLOG(INIT, INFO, "[wifi] larger buf:%d result:%ld\n", u4AllocBufLen, u4BufLen);
			ret = -E2BIG;
			prData->length = (__u16) u4BufLen;
			goto error;
		}
#endif /* WIRELESS_EXT >= 17 */

	}

	if (prList->u4NumberOfItems > CFG_MAX_NUM_BSS_LIST) {
		DBGLOG(INIT, INFO, "[wifi] strange scan result count:%u\n", prList->u4NumberOfItems);
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
		ether_addr_copy(iwEvent.u.ap_addr.sa_data, prBss->arMacAddress);
		memcpy(pcCur, &iwEvent, IW_EV_ADDR_LEN);
		pcCur += IW_EV_ADDR_LEN;

		/* SSID */
		iwEvent.cmd = SIOCGIWESSID;
		/* Modification to user space pointer(essid.pointer) is not needed. */
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
		memcpy(pcCur + IW_EV_LCP_LEN, &iwEvent.u.data.length, sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
		memcpy(pcCur + IW_EV_POINT_LEN, prBss->rSsid.aucSsid, iwEvent.u.essid.length);
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
		iwEvent.u.qual.qual = 0;	/* Quality not available now */
		/* -100 < Rssi < -10, normalized by adding 0x100 */
		iwEvent.u.qual.level = 0x100 + prBss->rRssi;
		iwEvent.u.qual.noise = 0;	/* Noise not available now */
		iwEvent.u.qual.updated = IW_QUAL_QUAL_INVALID | IW_QUAL_LEVEL_UPDATED | IW_QUAL_NOISE_INVALID;
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
		memcpy(pcCur + IW_EV_LCP_LEN, &iwEvent.u.data.length, sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
		pcCur += IW_EV_POINT_LEN;

		/* rearrange rate information */
		u4BufIndex = sprintf(aucRatesBuf, "Rates (Mb/s):");
		u4HighestRate = 0;
		for (j = 0; j < PARAM_MAX_LEN_RATES_EX; ++j) {
			UINT_8 curRate = prBss->rSupportedRates[j] & 0x7F;

			if (curRate == 0)
				break;

			if (curRate > u4HighestRate)
				u4HighestRate = curRate;

			if (curRate == RATE_5_5M)
				u4BufIndex += sprintf(aucRatesBuf + u4BufIndex, " 5.5");
			else
				u4BufIndex += sprintf(aucRatesBuf + u4BufIndex, " %d", curRate / 2);
#if DBG
			if (u4BufIndex > sizeof(aucRatesBuf))
				break;
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
		memcpy(pcCur + IW_EV_LCP_LEN, &iwEvent.u.data.length, sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
		memcpy(pcCur + IW_EV_POINT_LEN, aucRatesBuf, u4BufIndex);
		pcCur += iwEvent.len;
#endif /* WIRELESS_EXT >= 15 */

		if (wextSrchDesiredWPAIE(&prBss->aucIEs[sizeof(PARAM_FIXED_IEs)],
					 prBss->u4IELength - sizeof(PARAM_FIXED_IEs),
					 0xDD, (PUINT_8 *) &prDesiredIE)) {
			iwEvent.cmd = IWEVGENIE;
			iwEvent.u.data.flags = 1;
			iwEvent.u.data.length = 2 + (__u16) prDesiredIE->ucLength;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
#if WIRELESS_EXT <= 18
			memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length, sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
			memcpy(pcCur + IW_EV_POINT_LEN, prDesiredIE, 2 + prDesiredIE->ucLength);
			pcCur += iwEvent.len;
		}
#if CFG_SUPPORT_WPS		/* search WPS IE (0xDD, 221, OUI: 0x0050f204 ) */
		if (wextSrchDesiredWPSIE(&prBss->aucIEs[sizeof(PARAM_FIXED_IEs)],
					 prBss->u4IELength - sizeof(PARAM_FIXED_IEs),
					 0xDD, (PUINT_8 *) &prDesiredIE)) {
			iwEvent.cmd = IWEVGENIE;
			iwEvent.u.data.flags = 1;
			iwEvent.u.data.length = 2 + (__u16) prDesiredIE->ucLength;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
#if WIRELESS_EXT <= 18
			memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length, sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
			memcpy(pcCur + IW_EV_POINT_LEN, prDesiredIE, 2 + prDesiredIE->ucLength);
			pcCur += iwEvent.len;
		}
#endif

		/* Search RSN IE (0x30, 48). pBss->IEs starts from timestamp. */
		/* pBss->IEs starts from timestamp */
		if (wextSrchDesiredWPAIE(&prBss->aucIEs[sizeof(PARAM_FIXED_IEs)],
					 prBss->u4IELength - sizeof(PARAM_FIXED_IEs),
					 0x30, (PUINT_8 *) &prDesiredIE)) {

			iwEvent.cmd = IWEVGENIE;
			iwEvent.u.data.flags = 1;
			iwEvent.u.data.length = 2 + (__u16) prDesiredIE->ucLength;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
#if WIRELESS_EXT <= 18
			memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length, sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
			memcpy(pcCur + IW_EV_POINT_LEN, prDesiredIE, 2 + prDesiredIE->ucLength);
			pcCur += iwEvent.len;
		}
#if CFG_SUPPORT_WAPI		/* Android+ */
		if (wextSrchDesiredWAPIIE(&prBss->aucIEs[sizeof(PARAM_FIXED_IEs)],
					  prBss->u4IELength - sizeof(PARAM_FIXED_IEs), (PUINT_8 *) &prDesiredIE)) {

#if 0
			iwEvent.cmd = IWEVGENIE;
			iwEvent.u.data.flags = 1;
			iwEvent.u.data.length = 2 + (__u16) prDesiredIE->ucLength;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
#if WIRELESS_EXT <= 18
			memcpy(pcCur, &iwEvent, IW_EV_POINT_LEN);
#else
			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length, sizeof(struct iw_point) - IW_EV_POINT_OFF);
#endif
			memcpy(pcCur + IW_EV_POINT_LEN, prDesiredIE, 2 + prDesiredIE->ucLength);
			pcCur += iwEvent.len;
#else
			iwEvent.cmd = IWEVCUSTOM;
			iwEvent.u.data.length = (2 + prDesiredIE->ucLength) * 2 + 8 /* wapi_ie= */;
			iwEvent.len = IW_EV_POINT_LEN + iwEvent.u.data.length;
			if ((pcCur + iwEvent.len) > pcEnd)
				break;
			iwEvent.u.data.flags = 1;

			memcpy(pcCur, &iwEvent, IW_EV_LCP_LEN);
			memcpy(pcCur + IW_EV_LCP_LEN,
			       &iwEvent.u.data.length, sizeof(struct iw_point) - IW_EV_POINT_OFF);

			pcCur += (IW_EV_POINT_LEN);

			pcCur += sprintf(pcCur, "wapi_ie=");

			snprintf_hex(pcCur, pcEnd - pcCur, (UINT_8 *) prDesiredIE, prDesiredIE->ucLength + 2);

			pcCur += (2 + prDesiredIE->ucLength) * 2 /* iwEvent.len */;
#endif
		}
#endif
		/* Complete an entry. Update end of valid entry */
		pcValidEntryEnd = pcCur;
		/* Extract next bss */
		prBss = (P_PARAM_BSSID_EX_T) ((char *)prBss + prBss->u4Length);
	}

	/* Update valid data length for caller function and upper layer
	 * applications.
	 */
	prData->length = (pcValidEntryEnd - pcExtra);
	/* kalIndicateStatusAndComplete(prGlueInfo, WLAN_STATUS_SCAN_COMPLETE, NULL, 0); */

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
	       IN struct iw_request_info *prIwrInfo, IN struct iw_point *prEssid, IN char *pcExtra)
{
	PARAM_SSID_T rNewSsid;
	UINT_32 cipher;
	ENUM_PARAM_ENCRYPTION_STATUS_T eEncStatus;
	ENUM_PARAM_AUTH_MODE_T eAuthMode;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prEssid);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prEssid, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (prEssid->length > IW_ESSID_MAX_SIZE)
		return -E2BIG;

	/* set auth mode */
	if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_DISABLED) {
		eAuthMode = (prGlueInfo->rWpaInfo.u4AuthAlg == IW_AUTH_ALG_OPEN_SYSTEM) ?
		    AUTH_MODE_OPEN : AUTH_MODE_AUTO_SWITCH;
	} else {
		/* set auth mode */
		switch (prGlueInfo->rWpaInfo.u4KeyMgmt) {
		case IW_AUTH_KEY_MGMT_802_1X:
			eAuthMode =
			    (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_WPA) ?
			    AUTH_MODE_WPA : AUTH_MODE_WPA2;
			break;
		case IW_AUTH_KEY_MGMT_PSK:
			eAuthMode =
			    (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_WPA) ?
			    AUTH_MODE_WPA_PSK : AUTH_MODE_WPA2_PSK;
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
			eAuthMode = AUTH_MODE_AUTO_SWITCH;
			break;
		}
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetAuthMode, &eAuthMode, sizeof(eAuthMode), FALSE, FALSE, FALSE, &u4BufLen);

	/* set encryption status */
	cipher = prGlueInfo->rWpaInfo.u4CipherGroup | prGlueInfo->rWpaInfo.u4CipherPairwise;
	if (cipher & IW_AUTH_CIPHER_CCMP) {
		eEncStatus = ENUM_ENCRYPTION3_ENABLED;
	} else if (cipher & IW_AUTH_CIPHER_TKIP) {
		eEncStatus = ENUM_ENCRYPTION2_ENABLED;
	} else if (cipher & (IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40)) {
		eEncStatus = ENUM_ENCRYPTION1_ENABLED;
	} else if (cipher & IW_AUTH_CIPHER_NONE) {
		if (prGlueInfo->rWpaInfo.fgPrivacyInvoke)
			eEncStatus = ENUM_ENCRYPTION1_ENABLED;
		else
			eEncStatus = ENUM_ENCRYPTION_DISABLED;
	} else {
		eEncStatus = ENUM_ENCRYPTION_DISABLED;
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetEncryptionStatus, &eEncStatus, sizeof(eEncStatus), FALSE, FALSE, FALSE, &u4BufLen);

#if WIRELESS_EXT < 21
	/*
	 * GeorgeKuo: a length error bug exists in (WE < 21) cases, kernel before
	 * 2.6.19. Cut the trailing '\0'.
	 */
	rNewSsid.u4SsidLen = (prEssid->length) ? prEssid->length - 1 : 0;
#else
	rNewSsid.u4SsidLen = prEssid->length;
#endif
	kalMemCopy(rNewSsid.aucSsid, pcExtra, rNewSsid.u4SsidLen);

	/*
	 * rNewSsid.aucSsid[rNewSsid.u4SsidLen] = '\0';
	 */

	if (kalIoctl(prGlueInfo,
		     wlanoidSetSsid,
		     (PVOID)&rNewSsid, sizeof(PARAM_SSID_T), FALSE, FALSE, TRUE, &u4BufLen) != WLAN_STATUS_SUCCESS) {
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
* \param[in] prEssid Pointer to iw_point structure containing essid information.
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
	       IN struct iw_request_info *prIwrInfo, IN struct iw_point *prEssid, OUT char *pcExtra)
{
	/* PARAM_SSID_T ssid; */

	P_PARAM_SSID_T prSsid;
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prEssid);
	ASSERT(pcExtra);

	if (GLUE_CHK_PR3(prNetDev, prEssid, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* if (!netif_carrier_ok(prNetDev)) { */
	/* return -ENOTCONN; */
	/* } */

	prSsid = kalMemAlloc(sizeof(PARAM_SSID_T), VIR_MEM_TYPE);

	if (!prSsid)
		return -ENOMEM;

	rStatus = kalIoctl(prGlueInfo, wlanoidQuerySsid, prSsid, sizeof(PARAM_SSID_T), TRUE, FALSE, FALSE, &u4BufLen);

	if ((rStatus == WLAN_STATUS_SUCCESS) && (prSsid->u4SsidLen <= MAX_SSID_LEN)) {
		kalMemCopy(pcExtra, prSsid->aucSsid, prSsid->u4SsidLen);
		prEssid->length = prSsid->u4SsidLen;
		prEssid->flags = 1;
	}

	kalMemFree(prSsid, VIR_MEM_TYPE, sizeof(PARAM_SSID_T));

	return rStatus;
}				/* wext_get_essid */

#if 0

/*----------------------------------------------------------------------------*/
/*!
* \brief To set tx desired bit rate. Three cases here
*        iwconfig wlan0 auto -> Set to origianl supported rate set.
*        iwconfig wlan0 18M -> Imply "fixed" case, set to 18Mbps as desired rate.
*        iwconfig wlan0 18M auto -> Set to auto rate lower and equal to 18Mbps
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
	      IN struct iw_request_info *prIwReqInfo, IN struct iw_param *prRate, IN char *pcExtra)
{
	PARAM_RATES_EX aucSuppRate = { 0 };
	PARAM_RATES_EX aucNewRate = { 0 };
	UINT_32 u4NewRateLen = 0;
	UINT_32 i;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prRate);
	if (GLUE_CHK_PR2(prNetDev, prRate) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQuerySupportedRates, &aucSuppRate, sizeof(aucSuppRate), &u4BufLen);

	/* Case: AUTO */
	if (prRate->value < 0) {
		if (prRate->fixed == 0) {
			/* iwconfig wlan0 rate auto */

			/* set full supported rate to device */
			rStatus = wlanSetInformation(prGlueInfo->prAdapter,
						     wlanoidSetDesiredRates,
						     &aucSuppRate, sizeof(aucSuppRate), &u4BufLen);
		} else {
			/* iwconfig wlan0 rate fixed */

			/* fix rate to what? DO NOTHING */
			return -EINVAL;
		}
		return 0;
	}

	aucNewRate[0] = prRate->value / 500000;	/* In unit of 500k */

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
				     wlanoidSetDesiredRates, &aucNewRate, sizeof(aucNewRate), &u4BufLen);
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
	      IN struct iw_request_info *prIwrInfo, OUT struct iw_param *prRate, IN char *pcExtra)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	UINT_32 u4Rate = 0;

	ASSERT(prNetDev);
	ASSERT(prRate);
	if (GLUE_CHK_PR2(prNetDev, prRate) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (!netif_carrier_ok(prNetDev))
		return -ENOTCONN;

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryLinkSpeed, &u4Rate, sizeof(u4Rate), TRUE, FALSE, FALSE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	prRate->value = u4Rate * 100;	/* u4Rate is in unit of 100bps */
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
	     IN struct iw_request_info *prIwrInfo, IN struct iw_param *prRts, IN char *pcExtra)
{
	PARAM_RTS_THRESHOLD u4RtsThresh;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prRts);
	if (GLUE_CHK_PR2(prNetDev, prRts) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (prRts->disabled == 1)
		u4RtsThresh = 2347;
	else if (prRts->value >= 0 || prRts->value <= 2347)
		u4RtsThresh = (PARAM_RTS_THRESHOLD) prRts->value;
	else
		return -EINVAL;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetRtsThreshold, &u4RtsThresh, sizeof(u4RtsThresh), FALSE, FALSE, FALSE, &u4BufLen);

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
	     IN struct iw_request_info *prIwrInfo, OUT struct iw_param *prRts, IN char *pcExtra)
{
	PARAM_RTS_THRESHOLD u4RtsThresh;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prRts);
	if (GLUE_CHK_PR2(prNetDev, prRts) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryRtsThreshold, &u4RtsThresh, sizeof(u4RtsThresh), TRUE, FALSE, FALSE, &u4BufLen);

	prRts->value = (typeof(prRts->value)) u4RtsThresh;
	prRts->disabled = (prRts->value > 2347 || prRts->value < 0) ? 1 : 0;
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
	      IN struct iw_request_info *prIwrInfo, OUT struct iw_param *prFrag, IN char *pcExtra)
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
	       IN struct iw_request_info *prIwrInfo, IN struct iw_param *prTxPow, IN char *pcExtra)
{
	int ret = 0;
	/* PARAM_DEVICE_POWER_STATE ePowerState; */
	ENUM_ACPI_STATE_T ePowerState;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prTxPow);
	if (GLUE_CHK_PR2(prNetDev, prTxPow) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (prTxPow->disabled) {
		/* <1> disconnect */
		rStatus = kalIoctl(prGlueInfo, wlanoidSetDisassociate, NULL, 0, FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			/* ToDo:: DBGLOG */
			DBGLOG(INIT, INFO, "######set disassoc failed\n");
		} else {
			DBGLOG(INIT, INFO, "######set assoc ok\n");
		}
		/* <2> mark to power state flag */
		ePowerState = ACPI_STATE_D0;
		DBGLOG(INIT, INFO, "set to acpi d3(0)\n");
		wlanSetAcpiState(prGlueInfo->prAdapter, ePowerState);

	} else {
		ePowerState = ACPI_STATE_D0;
		DBGLOG(INIT, INFO, "set to acpi d0\n");
		wlanSetAcpiState(prGlueInfo->prAdapter, ePowerState);
	}

	prGlueInfo->ePowerState = ePowerState;

	return ret;
}				/* wext_set_txpow */

#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief To get TX power.
*
* \param[in] prDev Net device requested.
* \param[in] prIwrInfo NULL.
* \param[out] prTxPow Pointer to iw_param structure containing tx power setting.
* \param[in] pcExtra NULL.
*
* \retval 0 Success.
*
* \note Tx power is stored in pTxPow.
*/
/*----------------------------------------------------------------------------*/
static int
wext_get_txpow(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo, OUT struct iw_param *prTxPow, IN char *pcExtra)
{
	/* PARAM_DEVICE_POWER_STATE ePowerState; */

	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(prNetDev);
	ASSERT(prTxPow);
	if (GLUE_CHK_PR2(prNetDev, prTxPow) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* GeorgeKuo: wlanoidQueryAcpiDevicePowerState() reports capability, not
	 * current state. Use GLUE_INFO_T to store state.
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
	/* prTxPow->disabled = (ePowerState != ParamDeviceStateD3) ? FALSE : TRUE; */
	prTxPow->disabled = TRUE;

	return 0;
}				/* wext_get_txpow */

/*----------------------------------------------------------------------------*/
/*!
* \brief To get encryption cipher and key.
*
* \param[in] prDev Net device requested.
* \param[in] prIwrInfo NULL.
* \param[out] prEnc Pointer to iw_point structure containing securiry information.
* \param[in] pcExtra Buffer to store key content.
*
* \retval 0 Success.
*
* \note Securiry information is stored in pEnc except key content.
*/
/*----------------------------------------------------------------------------*/
static int
wext_get_encode(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwrInfo, OUT struct iw_point *prEnc, IN char *pcExtra)
{
#if 1
	/* ENUM_ENCRYPTION_STATUS_T eEncMode; */
	ENUM_PARAM_ENCRYPTION_STATUS_T eEncMode;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prEnc);
	if (GLUE_CHK_PR2(prNetDev, prEnc) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryEncryptionStatus, &eEncMode, sizeof(eEncMode), TRUE, FALSE, FALSE, &u4BufLen);

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
* \param[in] prEnc Pointer to iw_point structure containing securiry information.
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
static UINT_8 wepBuf[48];

static int
wext_set_encode(IN struct net_device *prNetDev,
		IN struct iw_request_info *prIwrInfo, IN struct iw_point *prEnc, IN char *pcExtra)
{
#if 1
	ENUM_PARAM_ENCRYPTION_STATUS_T eEncStatus;
	ENUM_PARAM_AUTH_MODE_T eAuthMode;
	/* UINT_8 wepBuf[48]; */
	P_PARAM_WEP_T prWepKey = (P_PARAM_WEP_T) wepBuf;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prEnc);
	ASSERT(pcExtra);
	if (GLUE_CHK_PR3(prNetDev, prEnc, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* reset to default mode */
	prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
	prGlueInfo->rWpaInfo.u4KeyMgmt = 0;
	prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_NONE;
	prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
#if CFG_SUPPORT_802_11W
	prGlueInfo->rWpaInfo.u4Mfp = IW_AUTH_MFP_DISABLED;
#endif

	/* iwconfig wlan0 key off */
	if ((prEnc->flags & IW_ENCODE_MODE) == IW_ENCODE_DISABLED) {
		eAuthMode = AUTH_MODE_OPEN;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetAuthMode, &eAuthMode, sizeof(eAuthMode), FALSE, FALSE, FALSE, &u4BufLen);

		eEncStatus = ENUM_ENCRYPTION_DISABLED;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetEncryptionStatus,
				   &eEncStatus, sizeof(eEncStatus), FALSE, FALSE, FALSE, &u4BufLen);

		return 0;
	}

	/* iwconfig wlan0 key 0123456789 */
	/* iwconfig wlan0 key s:abcde */
	/* iwconfig wlan0 key 0123456789 [1] */
	/* iwconfig wlan0 key 01234567890123456789012345 [1] */
	/* check key size for WEP */
	if (prEnc->length == 5 || prEnc->length == 13 || prEnc->length == 16) {
		/* prepare PARAM_WEP key structure */
		prWepKey->u4KeyIndex = (prEnc->flags & IW_ENCODE_INDEX) ? (prEnc->flags & IW_ENCODE_INDEX) - 1 : 0;
		if (prWepKey->u4KeyIndex > 3) {
			/* key id is out of range */
			return -EINVAL;
		}
		prWepKey->u4KeyIndex |= 0x80000000;
		prWepKey->u4Length = 12 + prEnc->length;
		prWepKey->u4KeyLength = prEnc->length;
		kalMemCopy(prWepKey->aucKeyMaterial, pcExtra, prEnc->length);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetAddWep, prWepKey, prWepKey->u4Length, FALSE, FALSE, TRUE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, INFO, "wlanoidSetAddWep fail 0x%x\n", rStatus);
			return -EFAULT;
		}

		/* change to auto switch */
		prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_SHARED_KEY | IW_AUTH_ALG_OPEN_SYSTEM;
		eAuthMode = AUTH_MODE_AUTO_SWITCH;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetAuthMode, &eAuthMode, sizeof(eAuthMode), FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;

		prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40;
		prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40;

		eEncStatus = ENUM_WEP_ENABLED;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetEncryptionStatus,
				   &eEncStatus, sizeof(ENUM_PARAM_ENCRYPTION_STATUS_T), FALSE, FALSE, FALSE, &u4BufLen);

		if (rStatus != WLAN_STATUS_SUCCESS)
			return -EFAULT;

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
	       IN struct iw_request_info *prIwrInfo, IN struct iw_param *prPower, IN char *pcExtra)
{
	PARAM_POWER_MODE ePowerMode;
	INT_32 i4PowerValue;

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	PARAM_POWER_MODE_T rPowerMode;

	ASSERT(prNetDev);
	ASSERT(prPower);
	if (GLUE_CHK_PR2(prNetDev, prPower) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (!prGlueInfo)
		return -EFAULT;

	if (!prGlueInfo->prAdapter->prAisBssInfo)
		return -EFAULT;

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
			DBGLOG(INIT, INFO, "%s(): unsupported power management mode value = %d.\n",
					    __func__, prPower->value);

			return -EINVAL;
		}
	}

	rPowerMode.ePowerMode = ePowerMode;
	rPowerMode.ucBssIdx = prGlueInfo->prAdapter->prAisBssInfo->ucBssIndex;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSet802dot11PowerSaveProfile,
			   &rPowerMode, sizeof(PARAM_POWER_MODE_T), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return 0;
}				/* wext_set_power */

/*----------------------------------------------------------------------------*/
/*!
* \brief To get power management.
*
* \param[in] prDev Net device requested.
* \param[in] prIwrInfo NULL.
* \param[out] prPower Pointer to iw_param structure containing tx power setting.
* \param[in] pcExtra NULL.
*
* \retval 0 Success.
*
* \note Power management mode is stored in pTxPow->value.
*/
/*----------------------------------------------------------------------------*/
static int
wext_get_power(IN struct net_device *prNetDev,
	       IN struct iw_request_info *prIwrInfo, OUT struct iw_param *prPower, IN char *pcExtra)
{

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	PARAM_POWER_MODE ePowerMode = Param_PowerModeCAM;

	ASSERT(prNetDev);
	ASSERT(prPower);
	if (GLUE_CHK_PR2(prNetDev, prPower) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

#if 0
#if defined(_HIF_SDIO)
	rStatus = sdio_io_ctrl(prGlueInfo,
			       wlanoidQuery802dot11PowerSaveProfile,
			       &ePowerMode, sizeof(ePowerMode), TRUE, TRUE, &u4BufLen);
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQuery802dot11PowerSaveProfile,
				       &ePowerMode, sizeof(ePowerMode), &u4BufLen);
#endif
#else
	rStatus = wlanQueryInformation(prGlueInfo->prAdapter,
				       wlanoidQuery802dot11PowerSaveProfile,
				       &ePowerMode, sizeof(ePowerMode), &u4BufLen);
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

	return 0;
}				/* wext_get_power */

/*----------------------------------------------------------------------------*/
/*!
* \brief To set authentication parameters.
*
* \param[in] prDev Net device requested.
* \param[in] prIwrInfo NULL.
* \param[in] rpAuth Pointer to iw_param structure containing authentication information.
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
	      IN struct iw_request_info *prIwrInfo, IN struct iw_param *prAuth, IN char *pcExtra)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(prNetDev);
	ASSERT(prAuth);
	if (GLUE_CHK_PR2(prNetDev, prAuth) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	/* Save information to glue info and process later when ssid is set. */
	switch (prAuth->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
#if CFG_SUPPORT_WAPI
		if (wlanQueryWapiMode(prGlueInfo->prAdapter)) {
			prGlueInfo->rWpaInfo.u4WpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
			prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_OPEN_SYSTEM;
		} else {
			prGlueInfo->rWpaInfo.u4WpaVersion = prAuth->value;
		}
#else
		prGlueInfo->rWpaInfo.u4WpaVersion = prAuth->value;
#endif
		break;

	case IW_AUTH_CIPHER_PAIRWISE:
		prGlueInfo->rWpaInfo.u4CipherPairwise = prAuth->value;
		break;

	case IW_AUTH_CIPHER_GROUP:
		prGlueInfo->rWpaInfo.u4CipherGroup = prAuth->value;
		break;

	case IW_AUTH_KEY_MGMT:
		prGlueInfo->rWpaInfo.u4KeyMgmt = prAuth->value;
#if CFG_SUPPORT_WAPI
		if (prGlueInfo->rWpaInfo.u4KeyMgmt == IW_AUTH_KEY_MGMT_WAPI_PSK ||
		    prGlueInfo->rWpaInfo.u4KeyMgmt == IW_AUTH_KEY_MGMT_WAPI_CERT) {
			UINT_32 u4BufLen;
			WLAN_STATUS rStatus;

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetWapiMode,
					   &prAuth->value, sizeof(UINT_32), FALSE, FALSE, TRUE, &u4BufLen);
			DBGLOG(INIT, INFO, "IW_AUTH_WAPI_ENABLED :%d\n", prAuth->value);
		}
#endif
		if (prGlueInfo->rWpaInfo.u4KeyMgmt == IW_AUTH_KEY_MGMT_WPS)
			prGlueInfo->fgWpsActive = TRUE;
		else
			prGlueInfo->fgWpsActive = FALSE;
		break;

	case IW_AUTH_80211_AUTH_ALG:
		prGlueInfo->rWpaInfo.u4AuthAlg = prAuth->value;
		break;

	case IW_AUTH_PRIVACY_INVOKED:
		prGlueInfo->rWpaInfo.fgPrivacyInvoke = prAuth->value;
		break;
#if CFG_SUPPORT_802_11W
	case IW_AUTH_MFP:
		prGlueInfo->rWpaInfo.u4Mfp = prAuth->value;
		break;
#endif
#if CFG_SUPPORT_WAPI
	case IW_AUTH_WAPI_ENABLED:
		{
			UINT_32 u4BufLen;
			WLAN_STATUS rStatus;

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetWapiMode,
					   &prAuth->value, sizeof(UINT_32), FALSE, FALSE, TRUE, &u4BufLen);
		}
		DBGLOG(INIT, INFO, "IW_AUTH_WAPI_ENABLED :%d\n", prAuth->value);
		break;
#endif
	default:
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
* \param[in] prEnc Pointer to iw_point structure containing securiry information.
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
UINT_8 keyStructBuf[1024];	/* add/remove key shared buffer */
#else
UINT_8 keyStructBuf[100];	/* add/remove key shared buffer */
#endif

static int
wext_set_encode_ext(IN struct net_device *prNetDev,
		    IN struct iw_request_info *prIwrInfo, IN struct iw_point *prEnc, IN char *pcExtra)
{
	P_PARAM_REMOVE_KEY_T prRemoveKey = (P_PARAM_REMOVE_KEY_T) keyStructBuf;
	P_PARAM_KEY_T prKey = (P_PARAM_KEY_T) keyStructBuf;

	P_PARAM_WEP_T prWepKey = (P_PARAM_WEP_T) wepBuf;

	struct iw_encode_ext *prIWEncExt = (struct iw_encode_ext *)pcExtra;

	ENUM_PARAM_ENCRYPTION_STATUS_T eEncStatus;
	ENUM_PARAM_AUTH_MODE_T eAuthMode;
	/* ENUM_PARAM_OP_MODE_T eOpMode = NET_TYPE_AUTO_SWITCH; */

#if CFG_SUPPORT_WAPI
	P_PARAM_WPI_KEY_T prWpiKey = (P_PARAM_WPI_KEY_T) keyStructBuf;
#endif

	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(prNetDev);
	ASSERT(prEnc);
	if (GLUE_CHK_PR3(prNetDev, prEnc, pcExtra) == FALSE)
		return -EINVAL;
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	memset(keyStructBuf, 0, sizeof(keyStructBuf));

#if CFG_SUPPORT_WAPI
	if (prIWEncExt->alg == IW_ENCODE_ALG_SMS4) {
		if (prEnc->flags & IW_ENCODE_DISABLED)
			return 0;

		/* KeyID */
		prWpiKey->ucKeyID = (prEnc->flags & IW_ENCODE_INDEX);
		prWpiKey->ucKeyID--;
		if (prWpiKey->ucKeyID > 1) {
			/* key id is out of range */
			return -EINVAL;
		}

		if (prIWEncExt->key_len != 32) {
			/* key length not valid */
			return -EINVAL;
		}

		if (prIWEncExt->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
			prWpiKey->eKeyType = ENUM_WPI_GROUP_KEY;
			prWpiKey->eDirection = ENUM_WPI_RX;
		} else if (prIWEncExt->ext_flags & IW_ENCODE_EXT_SET_TX_KEY) {
			prWpiKey->eKeyType = ENUM_WPI_PAIRWISE_KEY;
			prWpiKey->eDirection = ENUM_WPI_RX_TX;
		}

		/* PN */
		/*memcpy(&prWpiKey->aucPN[0], &prIWEncExt->tx_seq[0], IW_ENCODE_SEQ_MAX_SIZE * 2);*/
		memcpy(&prWpiKey->aucPN[0], &prIWEncExt->tx_seq[0], IW_ENCODE_SEQ_MAX_SIZE);
		memcpy(&prWpiKey->aucPN[8], &prIWEncExt->rx_seq[0], IW_ENCODE_SEQ_MAX_SIZE);

		/* BSSID */
		memcpy(prWpiKey->aucAddrIndex, prIWEncExt->addr.sa_data, 6);

		memcpy(prWpiKey->aucWPIEK, prIWEncExt->key, 16);
		prWpiKey->u4LenWPIEK = 16;

		memcpy(prWpiKey->aucWPICK, &prIWEncExt->key[16], 16);
		prWpiKey->u4LenWPICK = 16;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetWapiKey, prWpiKey, sizeof(PARAM_WPI_KEY_T), FALSE, FALSE, TRUE, &u4BufLen);

	} else
#endif
	{

		if ((prEnc->flags & IW_ENCODE_MODE) == IW_ENCODE_DISABLED) {
			prRemoveKey->u4Length = sizeof(*prRemoveKey);
			memcpy(prRemoveKey->arBSSID, prIWEncExt->addr.sa_data, 6);

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetRemoveKey,
					   prRemoveKey, prRemoveKey->u4Length, FALSE, FALSE, TRUE, &u4BufLen);

			if (rStatus != WLAN_STATUS_SUCCESS)
				DBGLOG(INIT, INFO, "remove key error:%x\n", rStatus);
			return 0;
		}

		if (prIWEncExt->key_len > sizeof(prKey->aucKeyMaterial)) {
			DBGLOG(INIT, ERROR, "Invalid key length %d\n", prIWEncExt->key_len);
			return -EINVAL;
		}

		switch (prIWEncExt->alg) {
		case IW_ENCODE_ALG_NONE:
			break;
		case IW_ENCODE_ALG_WEP:
			/* iwconfig wlan0 key 0123456789 */
			/* iwconfig wlan0 key s:abcde */
			/* iwconfig wlan0 key 0123456789 [1] */
			/* iwconfig wlan0 key 01234567890123456789012345 [1] */
			/* check key size for WEP */
			if (prIWEncExt->key_len == 5 || prIWEncExt->key_len == 13 || prIWEncExt->key_len == 16) {
				/* prepare PARAM_WEP key structure */
				prWepKey->u4KeyIndex = (prEnc->flags & IW_ENCODE_INDEX) ?
				    (prEnc->flags & IW_ENCODE_INDEX) - 1 : 0;
				if (prWepKey->u4KeyIndex > 3) {
					/* key id is out of range */
					return -EINVAL;
				}
				prWepKey->u4KeyIndex |= 0x80000000;
				prWepKey->u4Length = 12 + prIWEncExt->key_len;
				prWepKey->u4KeyLength = prIWEncExt->key_len;
				/* kalMemCopy(prWepKey->aucKeyMaterial, pcExtra, prIWEncExt->key_len); */
				kalMemCopy(prWepKey->aucKeyMaterial, prIWEncExt->key, prIWEncExt->key_len);

				rStatus = kalIoctl(prGlueInfo,
						   wlanoidSetAddWep,
						   prWepKey, prWepKey->u4Length, FALSE, FALSE, TRUE, &u4BufLen);

				if (rStatus != WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, INFO, "wlanoidSetAddWep fail 0x%x\n", rStatus);
					return -EFAULT;
				}

				/* change to auto switch */
				prGlueInfo->rWpaInfo.u4AuthAlg = IW_AUTH_ALG_SHARED_KEY | IW_AUTH_ALG_OPEN_SYSTEM;
				eAuthMode = AUTH_MODE_AUTO_SWITCH;

				rStatus = kalIoctl(prGlueInfo,
						   wlanoidSetAuthMode,
						   &eAuthMode, sizeof(eAuthMode), FALSE, FALSE, FALSE, &u4BufLen);

				if (rStatus != WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, INFO, "wlanoidSetAuthMode fail 0x%x\n", rStatus);
					return -EFAULT;
				}

				prGlueInfo->rWpaInfo.u4CipherPairwise = IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40;
				prGlueInfo->rWpaInfo.u4CipherGroup = IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40;

				eEncStatus = ENUM_WEP_ENABLED;

				rStatus = kalIoctl(prGlueInfo,
						   wlanoidSetEncryptionStatus,
						   &eEncStatus,
						   sizeof(ENUM_PARAM_ENCRYPTION_STATUS_T),
						   FALSE, FALSE, FALSE, &u4BufLen);

				if (rStatus != WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, INFO, "wlanoidSetEncryptionStatus fail 0x%x\n", rStatus);
					return -EFAULT;
				}

			} else {
				DBGLOG(INIT, INFO, "key length %x\n", prIWEncExt->key_len);
				DBGLOG(INIT, INFO, "key error\n");
			}

			break;
		case IW_ENCODE_ALG_TKIP:
		case IW_ENCODE_ALG_CCMP:
#if CFG_SUPPORT_802_11W
		case IW_ENCODE_ALG_AES_CMAC:
#endif
			/* KeyID */
			prKey->u4KeyIndex = (prEnc->flags & IW_ENCODE_INDEX) ?
				(prEnc->flags & IW_ENCODE_INDEX) - 1 : 0;
#if CFG_SUPPORT_802_11W
			if (prKey->u4KeyIndex > 5) {
#else
			if (prKey->u4KeyIndex > 3) {
#endif
				DBGLOG(INIT, INFO, "key index error:0x%x\n", prKey->u4KeyIndex);
				/* key id is out of range */
				return -EINVAL;
			}

			/* bit(31) and bit(30) are shared by pKey and pRemoveKey */
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

			/* Rx SC Bit(29) */
			if (prIWEncExt->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID) {
				prKey->u4KeyIndex |= 0x1UL << 29;
				memcpy(&prKey->rKeyRSC, prIWEncExt->rx_seq, IW_ENCODE_SEQ_MAX_SIZE);
			}

			/* BSSID */
			memcpy(prKey->arBSSID, prIWEncExt->addr.sa_data, 6);

			/* switch tx/rx MIC key for sta */
			if (prIWEncExt->alg == IW_ENCODE_ALG_TKIP && prIWEncExt->key_len == 32) {
				memcpy(prKey->aucKeyMaterial, prIWEncExt->key, 16);
				memcpy(((PUINT_8) prKey->aucKeyMaterial) + 16, prIWEncExt->key + 24, 8);
				memcpy((prKey->aucKeyMaterial) + 24, prIWEncExt->key + 16, 8);
			} else {
				memcpy(prKey->aucKeyMaterial, prIWEncExt->key, prIWEncExt->key_len);
			}

			prKey->u4KeyLength = prIWEncExt->key_len;
			prKey->u4Length = ((ULONG)&(((P_PARAM_KEY_T) 0)->aucKeyMaterial)) + prKey->u4KeyLength;

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetAddKey, prKey, prKey->u4Length, FALSE, FALSE, TRUE, &u4BufLen);

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
* \note Country code is stored and channel list is updated based on current country domain.
*/
/*----------------------------------------------------------------------------*/
static int wext_set_country(IN struct net_device *prNetDev, IN struct iw_point *prData)
{
	P_GLUE_INFO_T prGlueInfo;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	UINT_8 aucCountry[COUNTRY_CODE_LEN];

	ASSERT(prNetDev);

	/* prData->pointer should be like "COUNTRY US", "COUNTRY EU"
	 * and "COUNTRY JP"
	 */
	if (GLUE_CHK_PR2(prNetDev, prData) == FALSE || !prData->pointer || prData->length < COUNTRY_CODE_LEN)
		return -EINVAL;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (copy_from_user(aucCountry, prData->pointer, COUNTRY_CODE_LEN))
		return -EFAULT;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetCountryCode,
			   &aucCountry[COUNTRY_CODE_LEN-2], 2, FALSE, FALSE, TRUE, &u4BufLen);
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
static int wext_get_priv(IN struct net_device *prNetDev, OUT struct iw_point *prData)
{
	UINT_16 u2BufferSize = prData->length;

	/* Update our private args table size */
	prData->length = (__u16)sizeof(rIwPrivTable);
	if (u2BufferSize < prData->length)
		return -E2BIG;

	if (prData->length) {
		if (copy_to_user(prData->pointer, rIwPrivTable, sizeof(rIwPrivTable)))
			return -EFAULT;
	}

	return 0;
}				/* wext_get_priv */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))

int std_wext_get_name(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_name(prDev, prIwrInfo, (char *)&wru->name, sizeof(wru->name), NULL);

}

int std_wext_set_freq(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wrqu, char *pcExtra)
{
	return wext_set_freq(prDev, NULL, &wrqu->freq, NULL);
}

int std_wext_get_freq(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_freq(prDev, NULL, &wru->freq, NULL);

}

int std_wext_set_mode(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_set_mode(prDev, NULL, &wru->mode, NULL);
}

int std_wext_get_mode(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_mode(prDev, NULL, &wru->mode, NULL);
}

int std_wext_get_range(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;

	if (!pcExtra) {
		ret = -ENOMEM;
		return ret;
	}
	if (wru->data.pointer != NULL) {
		/* Buffer size should be large enough */
		if (wru->data.length < sizeof(struct iw_range)) {
			ret = -E2BIG;
			return ret;
		}

		ret = wext_get_range(prDev, NULL, &wru->data, pcExtra);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

int std_wext_set_ap(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;

	if (wru->ap_addr.sa_data[0] == 0 &&
		wru->ap_addr.sa_data[1] == 0 &&
		wru->ap_addr.sa_data[2] == 0 &&
		wru->ap_addr.sa_data[3] == 0 &&
		wru->ap_addr.sa_data[4] == 0 &&
		wru->ap_addr.sa_data[5] == 0) {
		/* WPA Supplicant will set 000000000000 in
		* wpa_driver_wext_deinit(), do nothing here or disassoc again?
		*/
		ret = 0;
	} else {
		ret = wext_set_ap(prDev, NULL, &wru->ap_addr, NULL);
	}

	return ret;
}

int std_wext_get_ap(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_ap(prDev, NULL, &wru->ap_addr, NULL);
}

int std_wext_set_mlme(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;

	if (!pcExtra) {
		ret = -ENOMEM;
		return ret;
	}

	if (!wru->data.pointer) {
		ret = -EINVAL;
		return ret;
	}

	if (wru->data.length < sizeof(struct iw_mlme)) {
		DBGLOG(INIT, INFO, "MLME buffer strange:%d\n", wru->data.length);
		ret = -EINVAL;
		return ret;
	}

	ret = wext_set_mlme(prDev, NULL, &(wru->data), pcExtra);

	return ret;
}

int std_wext_set_scan(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;
	struct iw_scan_req *prIwScanReq = NULL;

	if (!pcExtra) {
		ret = -EINVAL;
		return ret;
	}

	if (wru->data.pointer == NULL)
		ret = wext_set_scan(prDev, NULL, NULL, NULL);
#if WIRELESS_EXT > 17
	else if (wru->data.length == sizeof(struct iw_scan_req)) {
		prIwScanReq = (struct iw_scan_req *)pcExtra;

		if (prIwScanReq->essid_len > IW_ESSID_MAX_SIZE)
			prIwScanReq->essid_len = IW_ESSID_MAX_SIZE;
		ret = wext_set_scan(prDev, NULL, prIwScanReq, &(prIwScanReq->essid[0]));
	}
#endif
	else
		ret = -EINVAL;

	return ret;
}

int std_wext_get_scan(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;
	UINT_32 u4ExtraSize = 0;

	if (!pcExtra) {
		ret = -ENOMEM;
		return ret;
	}

	if (!wru->data.pointer) {
		ret = -EINVAL;
		return ret;
	}

	u4ExtraSize = wru->data.length;
	/* iwr->u.data.length may be updated by wext_get_scan() */
	ret = wext_get_scan(prDev, NULL, &wru->data, pcExtra);
	if (ret != 0) {
		if (ret == -E2BIG)
			DBGLOG(INIT, INFO, "[wifi] wext_get_scan -E2BIG\n");
	} else {
		/* check updated length is valid */
		ASSERT(wru->data.length <= u4ExtraSize);
		if (wru->data.length > u4ExtraSize) {
			DBGLOG(INIT, INFO,
				"Updated result length is larger than allocated (%u > %u)\n",
					wru->data.length, u4ExtraSize);
					wru->data.length = u4ExtraSize;
		}
	}

	return ret;
}

int std_wext_set_essid(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;
	UINT_32 u4ExtraSize = 0;

	u4ExtraSize = wru->essid.length;

	if (!pcExtra) {
		ret = -ENOMEM;
		return ret;
	}
	if (u4ExtraSize > IW_ESSID_MAX_SIZE) {
		ret = -E2BIG;
		return ret;
	}
	if (!wru->essid.pointer) {
		ret = -EINVAL;
		return ret;
	}

	ret = wext_set_essid(prDev, NULL, &wru->essid, pcExtra);
	return ret;
}

int std_wext_get_essid(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;
	UINT_32 u4ExtraSize = 0;

	u4ExtraSize = wru->essid.length;

	if (!pcExtra) {
		ret = -ENOMEM;
		return ret;
	}
	if (!wru->essid.pointer) {
		ret = -EINVAL;
		return ret;
	}

	if (u4ExtraSize != IW_ESSID_MAX_SIZE && u4ExtraSize != IW_ESSID_MAX_SIZE + 1) {
		DBGLOG(INIT, INFO, "[wifi] iwr->u.essid.length:%d error\n", wru->essid.length);
		ret = -E2BIG;	/* let caller try larger buffer */
		return ret;
	}

	/* iwr->u.essid.length is updated by wext_get_essid() */
	ret = wext_get_essid(prDev, NULL, &wru->essid, pcExtra);
	return ret;
}

int std_wext_get_rate(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_rate(prDev, NULL, &wru->bitrate, NULL);
}

int std_wext_set_rts(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_set_rts(prDev, NULL, &wru->rts, NULL);
}

int std_wext_get_rts(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_rts(prDev, NULL, &wru->rts, NULL);
}

int std_wext_get_frag(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_frag(prDev, NULL, &wru->frag, NULL);
}

int std_wext_set_txpow(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_set_txpow(prDev, NULL, &wru->txpower, NULL);
}

int std_wext_get_txpow(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_txpow(prDev, NULL, &wru->txpower, NULL);
}

int std_wext_set_encode(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;
	UINT_32 u4ExtraSize = 0;

	if (!pcExtra) {
		ret = -ENOMEM;
		return ret;
	}

	u4ExtraSize = wru->encoding.length;
	if (wru->encoding.pointer) {
		if (u4ExtraSize > IW_ENCODING_TOKEN_MAX) {
			ret = -E2BIG;
			return ret;
		}
	} else
		return -EINVAL;

	ret = wext_set_encode(prDev, NULL, &wru->encoding, pcExtra);
	return ret;
}

int std_wext_get_encode(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_encode(prDev, NULL, &wru->encoding, NULL);
}

int std_wext_set_power(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_set_power(prDev, NULL, &wru->power, NULL);
}

int std_wext_get_power(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_get_power(prDev, NULL, &wru->power, NULL);
}

int std_wext_set_auth(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_set_auth(prDev, NULL, &wru->param, NULL);
}

int std_wext_set_encode_ext(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;
	UINT_32 u4ExtraSize = 0;

	if (!pcExtra) {
		ret = -ENOMEM;
		return ret;
	}

	if (wru->encoding.pointer) {
		u4ExtraSize = wru->encoding.length;
		if (!u4ExtraSize || u4ExtraSize > (sizeof(struct iw_encode_ext) + 32)) {
			ret = -EINVAL;
			return ret;
		}
		ret = wext_set_encode_ext(prDev, NULL, &wru->encoding, pcExtra);
	} else if (wru->encoding.length != 0)
		ret = -EINVAL;

	return ret;
}

int std_wext_set_country(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	return wext_set_country(prDev, &wru->data);
}

int std_wext_get_priv(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	UINT_16 u2TblSize = 0;

	/* Update our private args table size */
	u2TblSize = (__u16)sizeof(rIwPrivTable)/sizeof(struct iw_priv_args);
	if (!pcExtra)
		return -ENOMEM;

	if (wru->data.length < u2TblSize)
		return -E2BIG;

	if (memcpy(pcExtra, rIwPrivTable, sizeof(rIwPrivTable)))
		return -EFAULT;

	return 0;
}

int std_wext_SIOCSIWPMKSA_Action(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;
	UINT_32 u4ExtraSize = 0;

	if (!pcExtra) {
		ret = -ENOMEM;
		return ret;
	}

	if (wru->data.pointer) {
			/* Fixed length structure */
		if (wru->data.length != sizeof(struct iw_pmksa)) {
			ret = -EINVAL;
			return ret;
		}

		u4ExtraSize = sizeof(struct iw_pmksa);
		switch (((struct iw_pmksa *)pcExtra)->cmd) {
		case IW_PMKSA_ADD:
			wext_support_ioctl_SIOCSIWPMKSA_Action(prDev, pcExtra, IW_PMKSA_ADD,
									&ret);
			break;
		case IW_PMKSA_REMOVE:
			break;
		case IW_PMKSA_FLUSH:
				wext_support_ioctl_SIOCSIWPMKSA_Action(prDev, pcExtra,
									IW_PMKSA_FLUSH, &ret);
			break;
		default:
			DBGLOG(INIT, INFO, "UNKNOWN iw_pmksa command:%d\n",
						((struct iw_pmksa *)pcExtra)->cmd);
			ret = -EFAULT;
			break;
		}
	} else if (wru->data.length != 0) {
		ret = -EINVAL;
		return ret;
	}

	return ret;
}

int std_wext_SIOCSIWGENIE_Action(struct net_device *prDev,
		struct iw_request_info *prIwrInfo,
		union iwreq_data *wru, char *pcExtra)
{
	int ret = 0;
#if WIRELESS_EXT > 17
	UINT_32 u4ExtraSize = 0;

	if (!pcExtra) {
		ret = -ENOMEM;
		return ret;
	}

	if (wru->data.pointer) {
		P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));

		u4ExtraSize = wru->data.length;
#if CFG_SUPPORT_WAPI
		if (u4ExtraSize > 42 /* The max wapi ie buffer */) {
			ret = -EINVAL;
			return ret;
		}
#endif
		if (u4ExtraSize)
			wext_support_ioctl_SIOCSIWGENIE(prGlueInfo, pcExtra, u4ExtraSize);
	}
#endif

	return ret;
}

#endif

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
int wext_support_ioctl(IN struct net_device *prDev, IN struct ifreq *prIfReq, IN int i4Cmd)
{
	struct iwreq *iwr = (struct iwreq *)prIfReq;
	struct iw_request_info rIwReqInfo;
	int ret = 0;
	char *prExtraBuf = NULL;
	UINT_32 u4ExtraSize = 0;
	struct iw_scan_req *prIwScanReq = NULL;

	rIwReqInfo.cmd = (__u16) i4Cmd;
	rIwReqInfo.flags = 0;

	switch (i4Cmd) {
	case SIOCGIWNAME:	/* 0x8B01, get wireless protocol name */
		ret = wext_get_name(prDev, &rIwReqInfo, (char *)&iwr->u.name, sizeof(iwr->u.name), NULL);
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

			prExtraBuf = kalMemAlloc(sizeof(struct iw_range), VIR_MEM_TYPE);
			if (!prExtraBuf) {
				ret = -ENOMEM;
				break;
			}

			/* reset all fields */
			memset(prExtraBuf, 0, sizeof(struct iw_range));
			iwr->u.data.length = sizeof(struct iw_range);

			ret = wext_get_range(prDev, NULL, &iwr->u.data, prExtraBuf);
			/* Push up to the caller */
			if (copy_to_user(iwr->u.data.pointer, prExtraBuf, iwr->u.data.length))
				ret = -EFAULT;

			kalMemFree(prExtraBuf, VIR_MEM_TYPE, sizeof(struct iw_range));
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
		/*
		 * case SIOCGIWSTATS:
		 * get statistics, intercepted by wireless_process_ioctl() in wireless.c,
		 * redirected to dev_iwstats(), dev->get_wireless_stats().
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
		    iwr->u.ap_addr.sa_data[4] == 0 && iwr->u.ap_addr.sa_data[5] == 0) {
			/* WPA Supplicant will set 000000000000 in
			 ** wpa_driver_wext_deinit(), do nothing here or disassoc again?
			 */
			ret = 0;
		} else {
			ret = wext_set_ap(prDev, NULL, &iwr->u.ap_addr, NULL);
		}
		break;

	case SIOCGIWAP:	/* 0x8B15, get access point MAC addresses (BSSID) */
		ret = wext_get_ap(prDev, NULL, &iwr->u.ap_addr, NULL);
		break;

	case SIOCSIWMLME:	/* 0x8B16, request MLME operation */
		/* Fixed length structure */
		if (iwr->u.data.length != sizeof(struct iw_mlme)) {
			DBGLOG(INIT, INFO, "MLME buffer strange:%d\n", iwr->u.data.length);
			ret = -EINVAL;
			break;
		}

		if (!iwr->u.data.pointer) {
			ret = -EINVAL;
			break;
		}

		prExtraBuf = kalMemAlloc(sizeof(struct iw_mlme), VIR_MEM_TYPE);
		if (!prExtraBuf) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(prExtraBuf, iwr->u.data.pointer, sizeof(struct iw_mlme)))
			ret = -EFAULT;
		else
			ret = wext_set_mlme(prDev, NULL, &(iwr->u.data), prExtraBuf);

		kalMemFree(prExtraBuf, VIR_MEM_TYPE, sizeof(struct iw_mlme));
		prExtraBuf = NULL;
		break;

		/* case SIOCGIWAPLIST: 0x8B17, deprecated */
	case SIOCSIWSCAN:	/* 0x8B18, scan request */
		if (iwr->u.data.pointer == NULL)
			ret = wext_set_scan(prDev, NULL, NULL, NULL);
#if WIRELESS_EXT > 17
		else if (iwr->u.data.length == sizeof(struct iw_scan_req)) {
			prIwScanReq = kalMemAlloc(iwr->u.data.length, VIR_MEM_TYPE);
			if (!prIwScanReq) {
				ret = -ENOMEM;
				break;
			}
			if (copy_from_user(prIwScanReq, iwr->u.data.pointer, iwr->u.data.length))
				ret = -EFAULT;
			else {
				if (prIwScanReq->essid_len > IW_ESSID_MAX_SIZE)
					prIwScanReq->essid_len = IW_ESSID_MAX_SIZE;
				ret = wext_set_scan(prDev, NULL, prIwScanReq, &(prIwScanReq->essid[0]));
			}

			kalMemFree(prIwScanReq, VIR_MEM_TYPE, iwr->u.data.length);
			prIwScanReq = NULL;
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
		/* allocate the same size of kernel buffer to store scan results. */
		prExtraBuf = kalMemAlloc(u4ExtraSize, VIR_MEM_TYPE);
		if (!prExtraBuf) {
			ret = -ENOMEM;
			break;
		}

		/* iwr->u.data.length may be updated by wext_get_scan() */
		ret = wext_get_scan(prDev, NULL, &iwr->u.data, prExtraBuf);
		if (ret != 0) {
			if (ret == -E2BIG)
				DBGLOG(INIT, INFO, "[wifi] wext_get_scan -E2BIG\n");
		} else {
			/* check updated length is valid */
			ASSERT(iwr->u.data.length <= u4ExtraSize);
			if (iwr->u.data.length > u4ExtraSize) {
				DBGLOG(INIT, INFO,
				       "Updated result length is larger than allocated (%u > %u)\n",
					iwr->u.data.length, u4ExtraSize);
				iwr->u.data.length = u4ExtraSize;
			}

			if (copy_to_user(iwr->u.data.pointer, prExtraBuf, iwr->u.data.length))
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

		prExtraBuf = kalMemAlloc(IW_ESSID_MAX_SIZE + 4, VIR_MEM_TYPE);
		if (!prExtraBuf) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(prExtraBuf, iwr->u.essid.pointer, u4ExtraSize))
			ret = -EFAULT;
		else
			ret = wext_set_essid(prDev, NULL, &iwr->u.essid, prExtraBuf);

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

		if (u4ExtraSize != IW_ESSID_MAX_SIZE && u4ExtraSize != IW_ESSID_MAX_SIZE + 1) {
			DBGLOG(INIT, INFO, "[wifi] iwr->u.essid.length:%d error\n", iwr->u.essid.length);
			ret = -E2BIG;	/* let caller try larger buffer */
			break;
		}

		prExtraBuf = kalMemAlloc(IW_ESSID_MAX_SIZE + 1, VIR_MEM_TYPE);
		if (!prExtraBuf) {
			ret = -ENOMEM;
			break;
		}

		/* iwr->u.essid.length is updated by wext_get_essid() */

		ret = wext_get_essid(prDev, NULL, &iwr->u.essid, prExtraBuf);
		if (ret == 0) {
			if (copy_to_user(iwr->u.essid.pointer, prExtraBuf, iwr->u.essid.length))
				ret = -EFAULT;
		}

		kalMemFree(prExtraBuf, VIR_MEM_TYPE, IW_ESSID_MAX_SIZE + 1);
		prExtraBuf = NULL;

		break;

		/* case SIOCSIWNICKN: 0x8B1C, not supported */
		/* case SIOCGIWNICKN: 0x8B1D, not supported */

	case SIOCSIWRATE:	/* 0x8B20, set default bit rate (bps) */
		/* ret = wext_set_rate(prDev, &rIwReqInfo, &iwr->u.bitrate, NULL); */
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

			if (copy_from_user(prExtraBuf, iwr->u.encoding.pointer, u4ExtraSize))
				ret = -EFAULT;
		} else if (u4ExtraSize != 0) {
			ret = -EINVAL;
			break;
		}

		if (ret == 0)
			ret = wext_set_encode(prDev, NULL, &iwr->u.encoding, prExtraBuf);

		if (prExtraBuf) {
			kalMemFree(prExtraBuf, VIR_MEM_TYPE, u4ExtraSize);
			prExtraBuf = NULL;
		}
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
			P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));

			u4ExtraSize = iwr->u.data.length;
			if (1 /* wlanQueryWapiMode(prGlueInfo->prAdapter) */) {
				/* Fixed length structure */
#if CFG_SUPPORT_WAPI
				if (u4ExtraSize > 42 /* The max wapi ie buffer */) {
					ret = -EINVAL;
					break;
				}
#endif
				if (u4ExtraSize) {
					prExtraBuf = kalMemAlloc(u4ExtraSize, VIR_MEM_TYPE);
					if (!prExtraBuf) {
						ret = -ENOMEM;
						break;
					}
					if (copy_from_user(prExtraBuf, iwr->u.data.pointer, u4ExtraSize))
						ret = -EFAULT;
					else
						wext_support_ioctl_SIOCSIWGENIE(prGlueInfo, prExtraBuf, u4ExtraSize);
					kalMemFree(prExtraBuf, VIR_MEM_TYPE, u4ExtraSize);
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
			if (!u4ExtraSize || u4ExtraSize > (sizeof(struct iw_encode_ext) + 32)) {
				ret = -EINVAL;
				break;
			}

			prExtraBuf = kalMemAlloc(u4ExtraSize, VIR_MEM_TYPE);
			if (!prExtraBuf) {
				ret = -ENOMEM;
				break;
			}
			kalMemZero(prExtraBuf, u4ExtraSize);
			if (!copy_from_user(prExtraBuf, iwr->u.encoding.pointer, u4ExtraSize))
				ret = wext_set_encode_ext(prDev, NULL, &iwr->u.encoding, prExtraBuf);
			else
				ret = -EFAULT;
			kalMemFree(prExtraBuf, VIR_MEM_TYPE, u4ExtraSize);
			prExtraBuf = NULL;
		} else if (iwr->u.encoding.length != 0)
			ret = -EINVAL;
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

			if (copy_from_user(prExtraBuf, iwr->u.data.pointer, sizeof(struct iw_pmksa))) {
				ret = -EFAULT;
			} else {
				switch (((struct iw_pmksa *)prExtraBuf)->cmd) {
				case IW_PMKSA_ADD:
					{
						wext_support_ioctl_SIOCSIWPMKSA_Action(prDev, prExtraBuf, IW_PMKSA_ADD,
										       &ret);
					}
					break;
				case IW_PMKSA_REMOVE:
					break;
				case IW_PMKSA_FLUSH:
					{
						wext_support_ioctl_SIOCSIWPMKSA_Action(prDev, prExtraBuf,
										       IW_PMKSA_FLUSH, &ret);
					}
					break;
				default:
					DBGLOG(INIT, INFO, "UNKNOWN iw_pmksa command:%d\n",
							    ((struct iw_pmksa *)prExtraBuf)->cmd);
					ret = -EFAULT;
					break;
				}
			}

			if (prExtraBuf) {
				kalMemFree(prExtraBuf, VIR_MEM_TYPE, u4ExtraSize);
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
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}				/* wext_support_ioctl */

static void wext_support_ioctl_SIOCSIWGENIE(IN P_GLUE_INFO_T prGlueInfo, IN char *prExtraBuf, IN UINT_32 u4ExtraSize)
{
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

#if CFG_SUPPORT_WAPI
	rStatus = kalIoctl(prGlueInfo, wlanoidSetWapiAssocInfo, prExtraBuf, u4ExtraSize, FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
	}
#endif

}

static void
wext_support_ioctl_SIOCSIWPMKSA_Action(IN struct net_device *prDev, IN char *prExtraBuf, IN int ioMode, OUT int *ret)
{
	P_GLUE_INFO_T prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;
	P_PARAM_PMKID_T prPmkid;

	switch (ioMode) {
	case IW_PMKSA_ADD:
		prPmkid = (P_PARAM_PMKID_T) kalMemAlloc(8 + sizeof(PARAM_BSSID_INFO_T), VIR_MEM_TYPE);
		if (!prPmkid) {
			DBGLOG(INIT, INFO, "Can not alloc memory for IW_PMKSA_ADD\n");
			*ret = -ENOMEM;
			break;
		}

		prPmkid->u4Length = 8 + sizeof(PARAM_BSSID_INFO_T);
		prPmkid->u4BSSIDInfoCount = 1;
		kalMemCopy(prPmkid->arBSSIDInfo->arBSSID, ((struct iw_pmksa *)prExtraBuf)->bssid.sa_data, 6);
		kalMemCopy(prPmkid->arBSSIDInfo->arPMKID, ((struct iw_pmksa *)prExtraBuf)->pmkid, IW_PMKID_LEN);

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetPmkid, prPmkid, sizeof(PARAM_PMKID_T), FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, INFO, "add pmkid error:%x\n", rStatus);

		kalMemFree(prPmkid, VIR_MEM_TYPE, 8 + sizeof(PARAM_BSSID_INFO_T));
		break;
	case IW_PMKSA_FLUSH:
		prPmkid = (P_PARAM_PMKID_T) kalMemAlloc(8, VIR_MEM_TYPE);
		if (!prPmkid) {
			DBGLOG(INIT, INFO, "Can not alloc memory for IW_PMKSA_FLUSH\n");
			*ret = -ENOMEM;
			break;
		}

		prPmkid->u4Length = 8;
		prPmkid->u4BSSIDInfoCount = 0;

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidSetPmkid, prPmkid, sizeof(PARAM_PMKID_T), FALSE, FALSE, TRUE, &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, INFO, "flush pmkid error:%x\n", rStatus);

		kalMemFree(prPmkid, VIR_MEM_TYPE, 8);
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
* \note Event is indicated to upper layer if cmd is supported and data is valid.
*       Using of kernel symbol wireless_send_event(), which is defined in
*      <net/iw_handler.h> after WE-14 (2.4.20).
*/
/*----------------------------------------------------------------------------*/
void
wext_indicate_wext_event(IN P_GLUE_INFO_T prGlueInfo,
			 IN unsigned int u4Cmd, IN unsigned char *pucData, IN unsigned int u4dataLen)
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

	memset(&wrqu, 0, sizeof(wrqu));

	switch (u4Cmd) {
	case SIOCGIWTXPOW:
		memcpy(&wrqu.power, pucData, u4dataLen);
		break;
	case SIOCGIWSCAN:
		complete_all(&prGlueInfo->rScanComp);
		break;

	case SIOCGIWAP:
		if (pucData)
			ether_addr_copy(wrqu.ap_addr.sa_data, pucData);
		else
			/* memset(&wrqu.ap_addr.sa_data, 0, ETH_ALEN); */
			eth_zero_addr((u8 *)&wrqu.ap_addr.sa_data);
		break;

	case IWEVASSOCREQIE:
#if WIRELESS_EXT < 15
		/* under WE-15, no suitable Event can be used */
		goto skip_indicate_event;
#else
		/* do supplicant a favor, parse to the start of WPA/RSN IE */
		if (wextSrchDesiredWPAIE(pucData, u4dataLen, 0x30, &pucDesiredIE)) {
			/* RSN IE found */
			/* RSN IE found */
		}
#if 0
		else if (wextSrchDesiredWPSIE(pucData, u4dataLen, 0xDD, &pucDesiredIE)) {
			/* WPS IE found */
			/* WPS IE found */
		}
#endif
		else if (wextSrchDesiredWPAIE(pucData, u4dataLen, 0xDD, &pucDesiredIE)) {
			/* WPA IE found */
			/* WPA IE found */
		}
#if CFG_SUPPORT_WAPI		/* Android+ */
		else if (wextSrchDesiredWAPIIE(pucData, u4dataLen, &pucDesiredIE)) {
			/* WAPI IE found */
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
		/* translate binary string to hex string, requirement of IWEVCUSTOM */
		for (i = 0; i < pucDesiredIE[1] + 2; ++i)
			pucExtraInfo += sprintf(pucExtraInfo, "%02x", pucDesiredIE[i]);
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
			P_PARAM_AUTH_REQUEST_T pAuthReq = (P_PARAM_AUTH_REQUEST_T) pucData;
			/* under WE-18, only IWEVCUSTOM can be used */
			u4Cmd = IWEVCUSTOM;
			pucExtraInfo = aucExtraInfoBuf;
			pucExtraInfo += snprintf(pucExtraInfo, sizeof(aucExtraInfoBuf),
				"MLME-MICHAELMICFAILURE.indication %s",
				(pAuthReq->u4Flags == PARAM_AUTH_REQUEST_GROUP_ERROR) ?
				"groupcast " : "unicast ");

			wrqu.data.length = pucExtraInfo - aucExtraInfoBuf;
			pucExtraInfo = aucExtraInfoBuf;
		}
#endif /* WIRELESS_EXT < 15 */
		break;

	case IWEVPMKIDCAND:
		if (prGlueInfo->rWpaInfo.u4WpaVersion == IW_AUTH_WPA_VERSION_WPA2 &&
		    prGlueInfo->rWpaInfo.u4KeyMgmt == IW_AUTH_KEY_MGMT_802_1X) {

			/* only used in WPA2 */
#if WIRELESS_EXT >= 18
			P_PARAM_PMKID_CANDIDATE_T prPmkidCand = (P_PARAM_PMKID_CANDIDATE_T) pucData;

			struct iw_pmkid_cand rPmkidCand;

			pucExtraInfo = aucExtraInfoBuf;

			rPmkidCand.flags = prPmkidCand->u4Flags;
			rPmkidCand.index = 0;
			rPmkidCand.bssid.sa_family = 0;
			kalMemCopy(rPmkidCand.bssid.sa_data, prPmkidCand->arBSSID, 6);

			kalMemCopy(pucExtraInfo, (PUINT_8) &rPmkidCand, sizeof(struct iw_pmkid_cand));
			wrqu.data.length = sizeof(struct iw_pmkid_cand);

			/* pmkid canadidate list is supported after WE-18 */
			/* indicate struct iw_pmkid_cand */
#else
			goto skip_indicate_event;
#endif
		} else {
			goto skip_indicate_event;
		}
		break;

	case IWEVCUSTOM:
		u4Cmd = IWEVCUSTOM;
		pucExtraInfo = aucExtraInfoBuf;
		kalMemCopy(pucExtraInfo, pucData, sizeof(PTA_IPC_T));
		wrqu.data.length = sizeof(PTA_IPC_T);
		break;

	default:
		goto skip_indicate_event;
	}

	/* Send event to user space */
	wireless_send_event(prGlueInfo->prDevHandler, u4Cmd, &wrqu, pucExtraInfo);

skip_indicate_event:
	return;
}				/* wext_indicate_wext_event */

/*----------------------------------------------------------------------------*/
/*!
* \brief A method of struct net_device, to get the network interface statistical
*        information.
*
* Whenever an application needs to get statistics for the interface, this method
* is called. This happens, for example, when ifconfig or netstat -i is run.
*
* \param[in] pDev Pointer to struct net_device.
*
* \return net_device_stats buffer pointer.
*
*/
/*----------------------------------------------------------------------------*/
struct iw_statistics *wext_get_wireless_stats(struct net_device *prDev)
{

	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct iw_statistics *pStats = NULL;
	INT_32 i4Rssi;
	UINT_32 bufLen = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prDev));
	ASSERT(prGlueInfo);
	if (!prGlueInfo)
		goto stat_out;

	pStats = (struct iw_statistics *)(&(prGlueInfo->rIwStats));

	if (!prDev || !netif_carrier_ok(prDev)) {
		/* network not connected */
		goto stat_out;
	}

	rStatus = kalIoctl(prGlueInfo, wlanoidQueryRssi, &i4Rssi, sizeof(i4Rssi), TRUE, TRUE, TRUE, &bufLen);

stat_out:
	return pStats;
}				/* wlan_get_wireless_stats */

BOOLEAN wextSrchOkcAndPMKID(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PUINT_8 *ppucPMKID, OUT PUINT_8 okc)
{
	INT_32 i4InfoElemLen;
	UINT_8 ucDone = 0;

	ASSERT(pucIEStart);
	ASSERT(ppucPMKID);
	ASSERT(okc);
	*okc = 0;
	*ppucPMKID = NULL;
	while (i4TotalIeLen >= 2) {
		i4InfoElemLen = (INT_32) pucIEStart[1] + 2;
		if (i4InfoElemLen > i4TotalIeLen)
			break;
		if (pucIEStart[0] == ELEM_ID_VENDOR) {
			if (pucIEStart[1] != 4 || pucIEStart[2] != 0 || pucIEStart[3] != 0x8 || pucIEStart[4] != 0x22)
				goto check_next;
			*okc = pucIEStart[5];
			ucDone |= 1;
		} else if (pucIEStart[0] == ELEM_ID_RSN) {
			/* RSN IE: EID(1), Len(1), Version(2), GrpCipher(4), PairCipherCnt(2),
			** PairCipherList(PairCipherCnt * 4), AKMCnt(2), AkmList(4*AkmCnt),
			** RSNCap(2), PMKIDCnt(2), PMKIDList(16*PMKIDCnt), GrpMgtCipher(4)
			*/
			UINT_16 u2CipherCnt = 0;
			UINT_16 u2AkmCnt = 0;
			INT_32 i4LenToCheck = 8;

			/* if no Pairwise Cipher Count field, bypass */
			if (i4InfoElemLen < i4LenToCheck + 2)
				goto check_next;
			u2CipherCnt = *(PUINT_16)&pucIEStart[i4LenToCheck];
			i4LenToCheck += 2; /* include length of Pairwise Cipher Count field */
			i4LenToCheck += u2CipherCnt * 4; /* include cipher list field */
			/* if no AKM Count, bypass */
			if (i4InfoElemLen < i4LenToCheck + 2)
				goto check_next;
			u2AkmCnt = *(PUINT_16)&pucIEStart[i4LenToCheck];
			i4LenToCheck += 2; /* include length of AKM Count */
			i4LenToCheck += u2AkmCnt * 4 + 2; /* include akm list field and RSN Cap field */
			/* if IE length is 10 + u2CipherCnt * 4 + 2 + u2AkmCnt * 4 + 2 + 6,
			** means PMKID count field is zero, and Group Mgmt Cipher may be exist
			*/
			if (i4InfoElemLen <= i4LenToCheck + 6)
				goto check_next;
			*ppucPMKID = pucIEStart + i4LenToCheck; /* return PMKID field and started at PMKID count */
			ucDone |= 2;
		}
		if (ucDone == 3)
			return TRUE;
		/* check desired EID */
		/* Select next information element. */
check_next:
		i4TotalIeLen -= i4InfoElemLen;
		pucIEStart += i4InfoElemLen;
	}
	return FALSE;
}

#endif /* WIRELESS_EXT */
