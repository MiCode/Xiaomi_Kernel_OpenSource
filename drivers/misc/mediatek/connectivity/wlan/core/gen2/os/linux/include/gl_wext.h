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

#ifndef _GL_WEXT_H
#define _GL_WEXT_H

#ifdef WIRELESS_EXT
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define KILO          1000
#define RATE_5_5M     11	/* 5.5M */

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
typedef struct _PARAM_FIXED_IEs {
	UINT_8 aucTimestamp[8];
	UINT_16 u2BeaconInterval;
	UINT_16 u2Capabilities;
} PARAM_FIXED_IEs;

typedef struct _PARAM_VARIABLE_IE_T {
	UINT_8 ucElementID;
	UINT_8 ucLength;
	UINT_8 aucData[1];
} PARAM_VARIABLE_IE_T, *P_PARAM_VARIABLE_IE_T;

#if WIRELESS_EXT < 18

#define SIOCSIWMLME 0x8B16	/* request MLME operation; uses struct iw_mlme */
/* MLME requests (SIOCSIWMLME / struct iw_mlme) */
#define IW_MLME_DEAUTH      0
#define IW_MLME_DISASSOC    1

/*! \brief SIOCSIWMLME data */
struct iw_mlme {
	__u16 cmd;		/*!< IW_MLME_* */
	__u16 reason_code;
	struct sockaddr addr;
};

#define SIOCSIWAUTH 0x8B32	/* set authentication mode params */
#define SIOCGIWAUTH 0x8B33	/* get authentication mode params */
/* SIOCSIWAUTH/SIOCGIWAUTH struct iw_param flags */
#define IW_AUTH_INDEX       0x0FFF
#define IW_AUTH_FLAGS       0xF000
/*
 * SIOCSIWAUTH/SIOCGIWAUTH parameters (0 .. 4095)
 * (IW_AUTH_INDEX mask in struct iw_param flags; this is the index of the
 * parameter that is being set/get to; value will be read/written to
 * struct iw_param value field)
 */
#define IW_AUTH_WPA_VERSION             0
#define IW_AUTH_CIPHER_PAIRWISE         1
#define IW_AUTH_CIPHER_GROUP            2
#define IW_AUTH_KEY_MGMT                3
#define IW_AUTH_TKIP_COUNTERMEASURES    4
#define IW_AUTH_DROP_UNENCRYPTED        5
#define IW_AUTH_80211_AUTH_ALG          6
#define IW_AUTH_WPA_ENABLED             7
#define IW_AUTH_RX_UNENCRYPTED_EAPOL    8
#define IW_AUTH_ROAMING_CONTROL         9
#define IW_AUTH_PRIVACY_INVOKED        10
#if CFG_SUPPORT_802_11W
#define IW_AUTH_MFP                    12

#define IW_AUTH_MFP_DISABLED    0	/* MFP disabled */
#define IW_AUTH_MFP_OPTIONAL    1	/* MFP optional */
#define IW_AUTH_MFP_REQUIRED    2	/* MFP required */
#endif

/* IW_AUTH_WPA_VERSION values (bit field) */
#define IW_AUTH_WPA_VERSION_DISABLED    0x00000001
#define IW_AUTH_WPA_VERSION_WPA         0x00000002
#define IW_AUTH_WPA_VERSION_WPA2        0x00000004

/* IW_AUTH_PAIRWISE_CIPHER and IW_AUTH_GROUP_CIPHER values (bit field) */
#define IW_AUTH_CIPHER_NONE     0x00000001
#define IW_AUTH_CIPHER_WEP40    0x00000002
#define IW_AUTH_CIPHER_TKIP     0x00000004
#define IW_AUTH_CIPHER_CCMP     0x00000008
#define IW_AUTH_CIPHER_WEP104   0x00000010

/* IW_AUTH_KEY_MGMT values (bit field) */
#define IW_AUTH_KEY_MGMT_802_1X     1
#define IW_AUTH_KEY_MGMT_PSK        2
#define IW_AUTH_KEY_MGMT_WPA_NONE   4

/* IW_AUTH_80211_AUTH_ALG values (bit field) */
#define IW_AUTH_ALG_OPEN_SYSTEM 0x00000001
#define IW_AUTH_ALG_SHARED_KEY  0x00000002
#define IW_AUTH_ALG_LEAP        0x00000004

/* IW_AUTH_ROAMING_CONTROL values */
#define IW_AUTH_ROAMING_ENABLE  0	/* driver/firmware based roaming */
#define IW_AUTH_ROAMING_DISABLE 1	/*
					 * user space program used for roaming
					 * control
					 */

#define SIOCSIWENCODEEXT 0x8B34	/* set encoding token & mode */
#define SIOCGIWENCODEEXT 0x8B35	/* get encoding token & mode */
/* SIOCSIWENCODEEXT definitions */
#define IW_ENCODE_SEQ_MAX_SIZE  8
/* struct iw_encode_ext ->alg */
#define IW_ENCODE_ALG_NONE  0
#define IW_ENCODE_ALG_WEP   1
#define IW_ENCODE_ALG_TKIP  2
#define IW_ENCODE_ALG_CCMP  3
#if CFG_SUPPORT_802_11W
#define IW_ENCODE_ALG_AES_CMAC  5
#endif

/* struct iw_encode_ext ->ext_flags */
#define IW_ENCODE_EXT_TX_SEQ_VALID  0x00000001
#define IW_ENCODE_EXT_RX_SEQ_VALID  0x00000002
#define IW_ENCODE_EXT_GROUP_KEY     0x00000004
#define IW_ENCODE_EXT_SET_TX_KEY    0x00000008

struct iw_encode_ext {
	__u32 ext_flags;	/*!< IW_ENCODE_EXT_* */
	__u8 tx_seq[IW_ENCODE_SEQ_MAX_SIZE];	/*!< LSB first */
	__u8 rx_seq[IW_ENCODE_SEQ_MAX_SIZE];	/*!< LSB first */
	struct sockaddr addr;	/*
				 * !< ff:ff:ff:ff:ff:ff for broadcast/multicast
				 *   (group) keys or unicast address for
				 *   individual keys
				 */
	__u16 alg;		/*!< IW_ENCODE_ALG_* */
	__u16 key_len;
	__u8 key[0];
};

#define SIOCSIWPMKSA        0x8B36	/* PMKSA cache operation */
#define IW_PMKSA_ADD        1
#define IW_PMKSA_REMOVE     2
#define IW_PMKSA_FLUSH      3

#define IW_PMKID_LEN        16

struct iw_pmksa {
	__u32 cmd;		/*!< IW_PMKSA_* */
	struct sockaddr bssid;
	__u8 pmkid[IW_PMKID_LEN];
};

#define IWEVGENIE   0x8C05	/*
				 * Generic IE (WPA, RSN, WMM, ..)
				 * (scan results); This includes id and
				 * length fields. One IWEVGENIE may
				 * contain more than one IE. Scan
				 * results may contain one or more
				 * IWEVGENIE events.
				 */
#define IWEVMICHAELMICFAILURE 0x8C06	/* Michael MIC failure
					 * (struct iw_michaelmicfailure)
					 */
#define IWEVASSOCREQIE  0x8C07	/*
				 * IEs used in (Re)Association Request.
				 * The data includes id and length
				 * fields and may contain more than one
				 * IE. This event is required in
				 * Managed mode if the driver
				 * generates its own WPA/RSN IE. This
				 * should be sent just before
				 * IWEVREGISTERED event for the
				 * association.
				 */
#define IWEVASSOCRESPIE 0x8C08	/*
				 * IEs used in (Re)Association
				 * Response. The data includes id and
				 * length fields and may contain more
				 * than one IE. This may be sent
				 * between IWEVASSOCREQIE and
				 * IWEVREGISTERED events for the
				 * association.
				 */
#define IWEVPMKIDCAND   0x8C09	/*
				 * PMKID candidate for RSN
				 * pre-authentication
				 * (struct iw_pmkid_cand)
				 */

#endif /* WIRELESS_EXT < 18 */

#if WIRELESS_EXT < 17
/* Statistics flags (bitmask in updated) */
#define IW_QUAL_QUAL_UPDATED    0x1	/* Value was updated since last read */
#define IW_QUAL_LEVEL_UPDATED   0x2
#define IW_QUAL_NOISE_UPDATED   0x4
#define IW_QUAL_QUAL_INVALID    0x10	/* Driver doesn't provide value */
#define IW_QUAL_LEVEL_INVALID   0x20
#define IW_QUAL_NOISE_INVALID   0x40
#endif

enum {
	IEEE80211_FILTER_TYPE_BEACON = 1 << 0,
	IEEE80211_FILTER_TYPE_PROBE_REQ = 1 << 1,
	IEEE80211_FILTER_TYPE_PROBE_RESP = 1 << 2,
	IEEE80211_FILTER_TYPE_ASSOC_REQ = 1 << 3,
	IEEE80211_FILTER_TYPE_ASSOC_RESP = 1 << 4,
	IEEE80211_FILTER_TYPE_AUTH = 1 << 5,
	IEEE80211_FILTER_TYPE_DEAUTH = 1 << 6,
	IEEE80211_FILTER_TYPE_DISASSOC = 1 << 7,
	IEEE80211_FILTER_TYPE_ALL = 0xFF	/* used to check the valid filter bits */
};

#if CFG_SUPPORT_WAPI
#define IW_AUTH_WAPI_ENABLED     0x20
#define IW_ENCODE_ALG_SMS4  0x20
#endif

#if CFG_SUPPORT_WAPI		/* Android+ */
#define IW_AUTH_KEY_MGMT_WAPI_PSK   3
#define IW_AUTH_KEY_MGMT_WAPI_CERT  4
#endif
#define IW_AUTH_KEY_MGMT_WPS  5

#if CFG_SUPPORT_802_11W
#define IW_AUTH_KEY_MGMT_802_1X_SHA256 7
#define IW_AUTH_KEY_MGMT_PSK_SHA256 8
#endif

#define IW_AUTH_ALG_FT			0x00000008
#define IW_AUTH_ALG_SAE			0x00000010
/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
extern const struct iw_handler_def wext_handler_def;

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
/* wireless extensions' ioctls */
int wext_support_ioctl(IN struct net_device *prDev, IN struct ifreq *prIfReq, IN int i4Cmd);

int
wext_set_rate(IN struct net_device *prNetDev,
	      IN struct iw_request_info *prIwReqInfo, IN struct iw_param *prRate, IN char *pcExtra);

void
wext_indicate_wext_event(IN P_GLUE_INFO_T prGlueInfo,
			 IN unsigned int u4Cmd, IN unsigned char *pucData, IN unsigned int u4DataLen);

struct iw_statistics *wext_get_wireless_stats(struct net_device *prDev);

BOOLEAN
wextSrchDesiredWPAIE(IN PUINT_8 pucIEStart,
		     IN INT_32 i4TotalIeLen, IN UINT_8 ucDesiredElemId, OUT PPUINT_8 ppucDesiredIE);

#if CFG_SUPPORT_WPS
BOOLEAN
wextSrchDesiredWPSIE(IN PUINT_8 pucIEStart,
		     IN INT_32 i4TotalIeLen, IN UINT_8 ucDesiredElemId, OUT PPUINT_8 ppucDesiredIE);
#endif

#if CFG_SUPPORT_HOTSPOT_2_0
BOOLEAN wextSrchDesiredHS20IE(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PPUINT_8 ppucDesiredIE);

BOOLEAN wextSrchDesiredAdvProtocolIE(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PPUINT_8 ppucDesiredIE);

BOOLEAN wextSrchDesiredOsenIE(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PPUINT_8 ppucDesiredIE);
#endif

#if CFG_SUPPORT_WAPI
BOOLEAN wextSrchDesiredWAPIIE(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PPUINT_8 ppucDesiredIE);
#endif

BOOLEAN wextSrchOkcAndPMKID(IN PUINT_8 pucIEStart, IN INT_32 i4TotalIeLen, OUT PPUINT_8 ppucPMKID, OUT PUINT_8 okc);
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* WIRELESS_EXT */

#endif /* _GL_WEXT_H */
