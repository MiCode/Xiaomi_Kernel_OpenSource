/** @file moal_uap_cfg80211.c
  *
  * @brief This file contains the functions for uAP CFG80211.
  *
  * Copyright (C) 2011-2012, Marvell International Ltd.
  *
  * This software file (the "File") is distributed by Marvell International
  * Ltd. under the terms of the GNU General Public License Version 2, June 1991
  * (the "License").  You may use, redistribute and/or modify this File in
  * accordance with the terms and conditions of the License, a copy of which
  * is available by writing to the Free Software Foundation, Inc.,
  * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
  * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
  *
  * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
  * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
  * this warranty disclaimer.
  *
  */

#include "moal_cfg80211.h"
#include "moal_uap_cfg80211.h"
/********************************************************
				Local Variables
********************************************************/

/********************************************************
				Global Variables
********************************************************/

/********************************************************
				Local Functions
********************************************************/

/********************************************************
				Global Functions
********************************************************/

/**
 * @brief send deauth to station
 *
 * @param                 A pointer to moal_private
 * @param mac			  A pointer to station mac address
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_deauth_station(moal_private * priv, u8 * mac_addr)
{
	mlan_ioctl_req *ioctl_req = NULL;
	mlan_ds_bss *bss = NULL;
	int ret = 0;

	ENTER();

	ioctl_req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (ioctl_req == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	bss = (mlan_ds_bss *) ioctl_req->pbuf;
	bss->sub_command = MLAN_OID_UAP_DEAUTH_STA;
	ioctl_req->req_id = MLAN_IOCTL_BSS;
	ioctl_req->action = MLAN_ACT_SET;

	memcpy(bss->param.deauth_param.mac_addr, mac_addr,
	       MLAN_MAC_ADDR_LENGTH);
#define  REASON_CODE_DEAUTH_LEAVING 3
	bss->param.deauth_param.reason_code = REASON_CODE_DEAUTH_LEAVING;
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, ioctl_req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}

done:
	if (ioctl_req)
		kfree(ioctl_req);
	LEAVE();
	return ret;
}

/**
 * @brief send deauth to all station
 *
 * @param                 A pointer to moal_private
 * @param mac			  A pointer to station mac address
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_deauth_all_station(moal_private * priv)
{
	int ret = -EFAULT;
	int i = 0;
	mlan_ds_get_info *info = NULL;
	mlan_ioctl_req *ioctl_req = NULL;

	ENTER();
	if (priv->media_connected == MFALSE) {
		PRINTM(MINFO, "cfg80211: Media not connected!\n");
		LEAVE();
		return 0;
	}
	PRINTM(MIOCTL, "del all station\n");
	/* Allocate an IOCTL request buffer */
	ioctl_req =
		(mlan_ioctl_req *)
		woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_get_info));
	if (ioctl_req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	info = (mlan_ds_get_info *) ioctl_req->pbuf;
	info->sub_command = MLAN_OID_UAP_STA_LIST;
	ioctl_req->req_id = MLAN_IOCTL_GET_INFO;
	ioctl_req->action = MLAN_ACT_GET;

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, ioctl_req, MOAL_IOCTL_WAIT)) {
		goto done;
	}
	if (!info->param.sta_list.sta_count)
		goto done;
	for (i = 0; i < info->param.sta_list.sta_count; i++) {
		PRINTM(MIOCTL, "deauth station " MACSTR "\n",
		       MAC2STR(info->param.sta_list.info[i].mac_address));
		ret = woal_deauth_station(priv,
					  info->param.sta_list.info[i].
					  mac_address);
	}
	woal_sched_timeout(200);
done:
	if (ioctl_req)
		kfree(ioctl_req);
	return ret;
}

/**
 * @brief Verify RSN IE
 *
 * @param rsn_ie          Pointer RSN IE
 * @param sys_config      Pointer to mlan_uap_bss_param structure
 *
 * @return                MTRUE/MFALSE
 */
static t_u8
woal_check_rsn_ie(IEEEtypes_Rsn_t * rsn_ie, mlan_uap_bss_param * sys_config)
{
	int left = 0;
	int count = 0;
	int i = 0;
	wpa_suite_auth_key_mgmt_t *key_mgmt = NULL;
	left = rsn_ie->len + 2;
	if (left < sizeof(IEEEtypes_Rsn_t))
		return MFALSE;
	sys_config->wpa_cfg.group_cipher = 0;
	sys_config->wpa_cfg.pairwise_cipher_wpa2 = 0;
	sys_config->key_mgmt = 0;
	/* check the group cipher */
	switch (rsn_ie->group_cipher.type) {
	case WPA_CIPHER_TKIP:
		sys_config->wpa_cfg.group_cipher = CIPHER_TKIP;
		break;
	case WPA_CIPHER_AES_CCM:
		sys_config->wpa_cfg.group_cipher = CIPHER_AES_CCMP;
		break;
	default:
		break;
	}
	count = le16_to_cpu(rsn_ie->pairwise_cipher.count);
	for (i = 0; i < count; i++) {
		switch (rsn_ie->pairwise_cipher.list[i].type) {
		case WPA_CIPHER_TKIP:
			sys_config->wpa_cfg.pairwise_cipher_wpa2 |= CIPHER_TKIP;
			break;
		case WPA_CIPHER_AES_CCM:
			sys_config->wpa_cfg.pairwise_cipher_wpa2 |=
				CIPHER_AES_CCMP;
			break;
		default:
			break;
		}
	}
	left -= sizeof(IEEEtypes_Rsn_t) + (count - 1) * sizeof(wpa_suite);
	if (left < sizeof(wpa_suite_auth_key_mgmt_t))
		return MFALSE;
	key_mgmt =
		(wpa_suite_auth_key_mgmt_t *) ((u8 *) rsn_ie +
					       sizeof(IEEEtypes_Rsn_t) +
					       (count - 1) * sizeof(wpa_suite));
	count = le16_to_cpu(key_mgmt->count);
	if (left <
	    (sizeof(wpa_suite_auth_key_mgmt_t) +
	     (count - 1) * sizeof(wpa_suite)))
		return MFALSE;
	for (i = 0; i < count; i++) {
		switch (key_mgmt->list[i].type) {
		case RSN_AKM_8021X:
			sys_config->key_mgmt |= KEY_MGMT_EAP;
			break;
		case RSN_AKM_PSK:
			sys_config->key_mgmt |= KEY_MGMT_PSK;
			break;
		case RSN_AKM_PSK_SHA256:
			sys_config->key_mgmt |= KEY_MGMT_PSK_SHA256;
			break;
		}
	}
	return MTRUE;
}

/**
 * @brief Verify WPA IE
 *
 * @param wpa_ie          Pointer WPA IE
 * @param sys_config      Pointer to mlan_uap_bss_param structure
 *
 * @return                MTRUE/MFALSE
 */
static t_u8
woal_check_wpa_ie(IEEEtypes_Wpa_t * wpa_ie, mlan_uap_bss_param * sys_config)
{
	int left = 0;
	int count = 0;
	int i = 0;
	wpa_suite_auth_key_mgmt_t *key_mgmt = NULL;
	left = wpa_ie->len + 2;
	if (left < sizeof(IEEEtypes_Wpa_t))
		return MFALSE;
	sys_config->wpa_cfg.group_cipher = 0;
	sys_config->wpa_cfg.pairwise_cipher_wpa = 0;
	switch (wpa_ie->group_cipher.type) {
	case WPA_CIPHER_TKIP:
		sys_config->wpa_cfg.group_cipher = CIPHER_TKIP;
		break;
	case WPA_CIPHER_AES_CCM:
		sys_config->wpa_cfg.group_cipher = CIPHER_AES_CCMP;
		break;
	default:
		break;
	}
	count = le16_to_cpu(wpa_ie->pairwise_cipher.count);
	for (i = 0; i < count; i++) {
		switch (wpa_ie->pairwise_cipher.list[i].type) {
		case WPA_CIPHER_TKIP:
			sys_config->wpa_cfg.pairwise_cipher_wpa |= CIPHER_TKIP;
			break;
		case WPA_CIPHER_AES_CCM:
			sys_config->wpa_cfg.pairwise_cipher_wpa |=
				CIPHER_AES_CCMP;
			break;
		default:
			break;
		}
	}
	left -= sizeof(IEEEtypes_Wpa_t) + (count - 1) * sizeof(wpa_suite);
	if (left < sizeof(wpa_suite_auth_key_mgmt_t))
		return MFALSE;
	key_mgmt =
		(wpa_suite_auth_key_mgmt_t *) ((u8 *) wpa_ie +
					       sizeof(IEEEtypes_Wpa_t) +
					       (count - 1) * sizeof(wpa_suite));
	count = le16_to_cpu(key_mgmt->count);
	if (left <
	    (sizeof(wpa_suite_auth_key_mgmt_t) +
	     (count - 1) * sizeof(wpa_suite)))
		return MFALSE;
	for (i = 0; i < count; i++) {
		switch (key_mgmt->list[i].type) {
		case RSN_AKM_8021X:
			sys_config->key_mgmt = KEY_MGMT_EAP;
			break;
		case RSN_AKM_PSK:
			sys_config->key_mgmt = KEY_MGMT_PSK;
			break;
		}
	}
	return MTRUE;
}

/**
 * @brief Find RSN/WPA IES
 *
 * @param ie              Pointer IE buffer
 * @param sys_config      Pointer to mlan_uap_bss_param structure
 *
 * @return                MTRUE/MFALSE
 */
static t_u8
woal_find_wpa_ies(const t_u8 * ie, int len, mlan_uap_bss_param * sys_config)
{
	int bytes_left = len;
	const t_u8 *pcurrent_ptr = ie;
	t_u16 total_ie_len;
	t_u8 element_len;
	t_u8 wpa2 = 0;
	t_u8 wpa = 0;
	t_u8 ret = MFALSE;
	IEEEtypes_ElementId_e element_id;
	IEEEtypes_VendorSpecific_t *pvendor_ie;
	const t_u8 wpa_oui[4] = { 0x00, 0x50, 0xf2, 0x01 };

	while (bytes_left >= 2) {
		element_id = (IEEEtypes_ElementId_e) (*((t_u8 *) pcurrent_ptr));
		element_len = *((t_u8 *) pcurrent_ptr + 1);
		total_ie_len = element_len + sizeof(IEEEtypes_Header_t);
		if (bytes_left < total_ie_len) {
			PRINTM(MERROR, "InterpretIE: Error in processing IE, "
			       "bytes left < IE length\n");
			bytes_left = 0;
			continue;
		}
		switch (element_id) {
		case RSN_IE:
			wpa2 = woal_check_rsn_ie((IEEEtypes_Rsn_t *)
						 pcurrent_ptr, sys_config);
			break;
		case VENDOR_SPECIFIC_221:
			pvendor_ie =
				(IEEEtypes_VendorSpecific_t *) pcurrent_ptr;
			if (!memcmp
			    (pvendor_ie->vend_hdr.oui, wpa_oui,
			     sizeof(pvendor_ie->vend_hdr.oui)) &&
			    (pvendor_ie->vend_hdr.oui_type == wpa_oui[3])) {
				wpa = woal_check_wpa_ie((IEEEtypes_Wpa_t *)
							pcurrent_ptr,
							sys_config);
			}
			break;
		default:
			break;
		}
		pcurrent_ptr += element_len + 2;
		/* Need to account for IE ID and IE Len */
		bytes_left -= (element_len + 2);
	}
	if (wpa && wpa2) {
		sys_config->protocol = PROTOCOL_WPA | PROTOCOL_WPA2;
		ret = MTRUE;
	} else if (wpa2) {
		sys_config->protocol = PROTOCOL_WPA2;
		ret = MTRUE;
	} else if (wpa) {
		sys_config->protocol = PROTOCOL_WPA;
		ret = MTRUE;
	}
	return ret;
}

/** secondary channel is below */
#define SECOND_CHANNEL_BELOW    0x30
/** secondary channel is above */
#define SECOND_CHANNEL_ABOVE    0x10
/** no secondary channel */
#define SECONDARY_CHANNEL_NONE     0x00

/**
 * @brief Get second channel offset
 *
 * @param chan 			  channel num
 * @return                second channel offset
 */
static t_u8
woal_get_second_channel_offset(int chan)
{
	t_u8 chan2Offset = SECONDARY_CHANNEL_NONE;

	switch (chan) {
	case 36:
	case 44:
	case 52:
	case 60:
	case 100:
	case 108:
	case 116:
	case 124:
	case 132:
	case 140:
	case 149:
	case 157:
		chan2Offset = SECOND_CHANNEL_ABOVE;
		break;
	case 40:
	case 48:
	case 56:
	case 64:
	case 104:
	case 112:
	case 120:
	case 128:
	case 136:
	case 144:
	case 153:
	case 161:
		chan2Offset = SECOND_CHANNEL_BELOW;
		break;
	case 165:
		/* Special Case: 20Mhz-only Channel */
		chan2Offset = SECONDARY_CHANNEL_NONE;
		break;
	}
	return chan2Offset;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
/**
 * @brief initialize AP or GO bss config
 *
 * @param priv            A pointer to moal private structure
 * @param params          A pointer to cfg80211_ap_settings structure
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_beacon_config(moal_private * priv,
			    struct cfg80211_ap_settings *params)
#else
/**
 * @brief initialize AP or GO bss config
 *
 * @param priv            A pointer to moal private structure
 * @param params          A pointer to beacon_parameters structure
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_beacon_config(moal_private * priv,
			    struct beacon_parameters *params)
#endif
{
	int ret = 0;
	mlan_uap_bss_param sys_config;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) || defined(COMPAT_WIRELESS)
	int i = 0;
#else
	const t_u8 *ssid_ie = NULL;
	struct ieee80211_mgmt *head = NULL;
	t_u16 capab_info = 0;
#endif
	t_u8 Rates_BG[13] =
		{ 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 0x30, 0x48,
		0x60, 0x6c, 0
	};
	t_u8 Rates_A[9] = { 0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c, 0 };
	t_u8 chan2Offset = 0;
#ifdef WIFI_DIRECT_SUPPORT
	t_u8 Rates_WFD[9] =
		{ 0x8c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c, 0 };
#endif

	ENTER();

	if (params == NULL) {
		ret = -EFAULT;
		goto done;
	}

	if (priv->bss_type != MLAN_BSS_TYPE_UAP
#ifdef WIFI_DIRECT_SUPPORT
	    && priv->bss_type != MLAN_BSS_TYPE_WIFIDIRECT
#endif
		) {
		ret = -EFAULT;
		goto done;
	}

	/* Initialize the uap bss values which are uploaded from firmware */
	if (MLAN_STATUS_SUCCESS != woal_set_get_sys_config(priv,
							   MLAN_ACT_GET,
							   MOAL_IOCTL_WAIT,
							   &sys_config)) {
		PRINTM(MERROR, "Error getting AP confiruration\n");
		ret = -EFAULT;
		goto done;
	}

	/* Setting the default values */
	sys_config.channel = 6;
	sys_config.preamble_type = 0;
	sys_config.mgmt_ie_passthru_mask = priv->mgmt_subtype_mask;
	memcpy(sys_config.mac_addr, priv->current_addr, ETH_ALEN);

	if (priv->bss_type == MLAN_BSS_TYPE_UAP) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
		if (params->beacon_interval)
			sys_config.beacon_period = params->beacon_interval;
#else
		if (params->interval)
			sys_config.beacon_period = params->interval;
#endif
		if (params->dtim_period)
			sys_config.dtim_period = params->dtim_period;
	}
	if (priv->channel) {
		memset(sys_config.rates, 0, sizeof(sys_config.rates));
		sys_config.channel = priv->channel;
		if (priv->channel <= MAX_BG_CHANNEL) {
			sys_config.band_cfg = BAND_CONFIG_2G;
#ifdef WIFI_DIRECT_SUPPORT
			if (priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT)
				memcpy(sys_config.rates, Rates_WFD,
				       sizeof(Rates_WFD));
			else
#endif
				memcpy(sys_config.rates, Rates_BG,
				       sizeof(Rates_BG));
		} else {
			sys_config.band_cfg = BAND_CONFIG_5G;
			chan2Offset =
				woal_get_second_channel_offset(priv->channel);
			if (chan2Offset) {
				sys_config.band_cfg |= chan2Offset;
				sys_config.ht_cap_info = 0x117e;
				sys_config.ampdu_param = 3;
			}
#ifdef WIFI_DIRECT_SUPPORT
			if (priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT)
				memcpy(sys_config.rates, Rates_WFD,
				       sizeof(Rates_WFD));
			else
#endif
				memcpy(sys_config.rates, Rates_A,
				       sizeof(Rates_A));
		}
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) || defined(COMPAT_WIRELESS)
	if (!params->ssid || !params->ssid_len) {
		ret = -EINVAL;
		goto done;
	}
	memcpy(sys_config.ssid.ssid, params->ssid,
	       MIN(MLAN_MAX_SSID_LENGTH, params->ssid_len));
	sys_config.ssid.ssid_len = MIN(MLAN_MAX_SSID_LENGTH, params->ssid_len);
	if (params->hidden_ssid)
		sys_config.bcast_ssid_ctl = 0;
	else
		sys_config.bcast_ssid_ctl = 1;
	if (params->auth_type == NL80211_AUTHTYPE_SHARED_KEY)
		sys_config.auth_mode = MLAN_AUTH_MODE_SHARED;
	else
		sys_config.auth_mode = MLAN_AUTH_MODE_OPEN;
	if (params->crypto.n_akm_suites) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
		woal_find_wpa_ies(params->beacon.tail,
				  (int)params->beacon.tail_len, &sys_config);
#else
		woal_find_wpa_ies(params->tail, params->tail_len, &sys_config);
#endif
	}
	for (i = 0; i < params->crypto.n_akm_suites; i++) {
		switch (params->crypto.akm_suites[i]) {
		case WLAN_AKM_SUITE_8021X:
			sys_config.key_mgmt |= KEY_MGMT_EAP;
			if ((params->crypto.
			     wpa_versions & NL80211_WPA_VERSION_1) &&
			    (params->crypto.
			     wpa_versions & NL80211_WPA_VERSION_2))
				sys_config.protocol =
					PROTOCOL_WPA | PROTOCOL_WPA2;
			else if (params->crypto.
				 wpa_versions & NL80211_WPA_VERSION_2)
				sys_config.protocol = PROTOCOL_WPA2;
			else if (params->crypto.
				 wpa_versions & NL80211_WPA_VERSION_1)
				sys_config.protocol = PROTOCOL_WPA;
			break;
		case WLAN_AKM_SUITE_PSK:
			sys_config.key_mgmt |= KEY_MGMT_PSK;
			if ((params->crypto.
			     wpa_versions & NL80211_WPA_VERSION_1) &&
			    (params->crypto.
			     wpa_versions & NL80211_WPA_VERSION_2))
				sys_config.protocol =
					PROTOCOL_WPA | PROTOCOL_WPA2;
			else if (params->crypto.
				 wpa_versions & NL80211_WPA_VERSION_2)
				sys_config.protocol = PROTOCOL_WPA2;
			else if (params->crypto.
				 wpa_versions & NL80211_WPA_VERSION_1)
				sys_config.protocol = PROTOCOL_WPA;
			break;
		}
	}
	sys_config.wpa_cfg.pairwise_cipher_wpa = 0;
	sys_config.wpa_cfg.pairwise_cipher_wpa2 = 0;
	for (i = 0; i < params->crypto.n_ciphers_pairwise; i++) {
		switch (params->crypto.ciphers_pairwise[i]) {
		case WLAN_CIPHER_SUITE_WEP40:
		case WLAN_CIPHER_SUITE_WEP104:
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			if (params->crypto.wpa_versions & NL80211_WPA_VERSION_1)
				sys_config.wpa_cfg.pairwise_cipher_wpa |=
					CIPHER_TKIP;
			if (params->crypto.wpa_versions & NL80211_WPA_VERSION_2)
				sys_config.wpa_cfg.pairwise_cipher_wpa2 |=
					CIPHER_TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			if (params->crypto.wpa_versions & NL80211_WPA_VERSION_1)
				sys_config.wpa_cfg.pairwise_cipher_wpa |=
					CIPHER_AES_CCMP;
			if (params->crypto.wpa_versions & NL80211_WPA_VERSION_2)
				sys_config.wpa_cfg.pairwise_cipher_wpa2 |=
					CIPHER_AES_CCMP;
			break;
		}
	}
	switch (params->crypto.cipher_group) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		if ((priv->cipher == WLAN_CIPHER_SUITE_WEP40) ||
		    (priv->cipher == WLAN_CIPHER_SUITE_WEP104)) {
			sys_config.protocol = PROTOCOL_STATIC_WEP;
			sys_config.key_mgmt = KEY_MGMT_NONE;
			sys_config.wpa_cfg.length = 0;
			sys_config.wep_cfg.key0.key_index = priv->key_index;
			sys_config.wep_cfg.key0.is_default = 1;
			sys_config.wep_cfg.key0.length = priv->key_len;
			memcpy(sys_config.wep_cfg.key0.key, priv->key_material,
			       priv->key_len);
		}
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		sys_config.wpa_cfg.group_cipher = CIPHER_TKIP;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		sys_config.wpa_cfg.group_cipher = CIPHER_AES_CCMP;
		break;
	}
#else
	/* Since in Android ICS 4.0.1's wpa_supplicant, there is no way to set
	   ssid when GO (AP) starts up, so get it from beacon head parameter
	   TODO: right now use hard code 24 -- ieee80211 header lenth, 12 --
	   fixed element length for beacon */
#define BEACON_IE_OFFSET	36
	/* Find SSID in head SSID IE id: 0, right now use hard code */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	ssid_ie = woal_parse_ie_tlv(params->beacon.head + BEACON_IE_OFFSET,
				    params->beacon.head_len - BEACON_IE_OFFSET,
				    0);
#else
	ssid_ie = woal_parse_ie_tlv(params->head + BEACON_IE_OFFSET,
				    params->head_len - BEACON_IE_OFFSET, 0);
#endif
	if (!ssid_ie) {
		PRINTM(MERROR, "No ssid IE found.\n");
		ret = -EFAULT;
		goto done;
	}
	if (*(ssid_ie + 1) > 32) {
		PRINTM(MERROR, "ssid len error: %d\n", *(ssid_ie + 1));
		ret = -EFAULT;
		goto done;
	}
	memcpy(sys_config.ssid.ssid, ssid_ie + 2, *(ssid_ie + 1));
	sys_config.ssid.ssid_len = *(ssid_ie + 1);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	head = (struct ieee80211_mgmt *)params->beacon.head;
#else
	head = (struct ieee80211_mgmt *)params->head;
#endif
	capab_info = le16_to_cpu(head->u.beacon.capab_info);
	PRINTM(MIOCTL, "capab_info=0x%x\n", head->u.beacon.capab_info);
	sys_config.auth_mode = MLAN_AUTH_MODE_OPEN;
	/** For ICS, we don't support OPEN mode */
	if ((priv->cipher == WLAN_CIPHER_SUITE_WEP40) ||
	    (priv->cipher == WLAN_CIPHER_SUITE_WEP104)) {
		sys_config.protocol = PROTOCOL_STATIC_WEP;
		sys_config.key_mgmt = KEY_MGMT_NONE;
		sys_config.wpa_cfg.length = 0;
		sys_config.wep_cfg.key0.key_index = priv->key_index;
		sys_config.wep_cfg.key0.is_default = 1;
		sys_config.wep_cfg.key0.length = priv->key_len;
		memcpy(sys_config.wep_cfg.key0.key, priv->key_material,
		       priv->key_len);
	} else {
		/** Get cipher and key_mgmt from RSN/WPA IE */
		if (capab_info & WLAN_CAPABILITY_PRIVACY) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
			if (MFALSE ==
			    woal_find_wpa_ies(params->beacon.tail,
					      (int)params->beacon.tail_len,
					      &sys_config))
#else
			if (MFALSE ==
			    woal_find_wpa_ies(params->tail, params->tail_len,
					      &sys_config))
#endif
			{
				/* hard code setting to wpa2-psk */
				sys_config.protocol = PROTOCOL_WPA2;
				sys_config.key_mgmt = KEY_MGMT_PSK;
				sys_config.wpa_cfg.pairwise_cipher_wpa2 =
					CIPHER_AES_CCMP;
				sys_config.wpa_cfg.group_cipher =
					CIPHER_AES_CCMP;
			}
		}
	}
#endif /* COMPAT_WIRELESS */
	/* If the security mode is configured as WEP or WPA-PSK, it will
	   disable 11n automatically, and if configured as open(off) or
	   wpa2-psk, it will automatically enable 11n */
	if ((sys_config.protocol == PROTOCOL_STATIC_WEP) ||
	    (sys_config.protocol == PROTOCOL_WPA))
		woal_uap_set_11n_status(&sys_config, MLAN_ACT_DISABLE);
	else
		woal_uap_set_11n_status(&sys_config, MLAN_ACT_ENABLE);
	if (MLAN_STATUS_SUCCESS != woal_set_get_sys_config(priv,
							   MLAN_ACT_SET,
							   MOAL_IOCTL_WAIT,
							   &sys_config)) {
		ret = -EFAULT;
		goto done;
	}
done:
	LEAVE();
	return ret;
}

#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
/**
 * @brief Callback function for virtual interface
 * 		setup
 *
 *  @param dev    A pointer to structure net_device
 *
 *  @return       N/A
 */
static void
woal_virt_if_setup(struct net_device *dev)
{
	ENTER();
	ether_setup(dev);
	dev->destructor = free_netdev;
	LEAVE();
}

/**
 * @brief This function adds a new interface. It will
 * 		allocate, initialize and register the device.
 *
 *  @param handle    A pointer to moal_handle structure
 *  @param bss_index BSS index number
 *  @param bss_type  BSS type
 *
 *  @return          A pointer to the new priv structure
 */
moal_private *
woal_alloc_virt_interface(moal_handle * handle, t_u8 bss_index, t_u8 bss_type,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
			  const
#endif
			  char *name)
{
	struct net_device *dev = NULL;
	moal_private *priv = NULL;
	ENTER();

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29)
#ifndef MAX_WMM_QUEUE
#define MAX_WMM_QUEUE   4
#endif
	/* Allocate an Ethernet device */
	if (!
	    (dev =
	     alloc_netdev_mq(sizeof(moal_private), name, woal_virt_if_setup,
			     MAX_WMM_QUEUE))) {
#else
	if (!
	    (dev =
	     alloc_netdev(sizeof(moal_private), name, woal_virt_if_setup))) {
#endif
		PRINTM(MFATAL, "Init virtual ethernet device failed\n");
		goto error;
	}
	/* Allocate device name */
	if ((dev_alloc_name(dev, name) < 0)) {
		PRINTM(MERROR, "Could not allocate device name\n");
		goto error;
	}

	priv = (moal_private *) netdev_priv(dev);
	/* Save the priv to handle */
	handle->priv[bss_index] = priv;

	/* Use the same handle structure */
	priv->phandle = handle;
	priv->netdev = dev;
	priv->bss_index = bss_index;
	priv->bss_type = bss_type;
	priv->bss_role = MLAN_BSS_ROLE_STA;

	INIT_LIST_HEAD(&priv->tcp_sess_queue);
	spin_lock_init(&priv->tcp_sess_lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	SET_MODULE_OWNER(dev);
#endif

	PRINTM(MCMND, "Alloc virtual interface%s\n", dev->name);

	LEAVE();
	return priv;
error:
	if (dev)
		free_netdev(dev);
	LEAVE();
	return NULL;
}

/**
 * @brief Request the driver to add a virtual interface
 *
 * @param wiphy           A pointer to wiphy structure
 * @param name            Virtual interface name
 * @param type            Virtual interface type
 * @param flags           Flags for the virtual interface
 * @param params          A pointer to vif_params structure
 * @param new_dev		  new net_device to return
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_add_virt_if(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
			  const
#endif
			  char *name, enum nl80211_iftype type, u32 * flags,
			  struct vif_params *params,
			  struct net_device **new_dev)
{
	int ret = 0;
	struct net_device *ndev = NULL;
	moal_private *priv, *new_priv;
	moal_handle *handle = (moal_handle *) woal_get_wiphy_priv(wiphy);
	struct wireless_dev *wdev = NULL;

	ENTER();
	ASSERT_RTNL();
	priv = (moal_private *) woal_get_priv_bss_type(handle,
						       MLAN_BSS_TYPE_WIFIDIRECT);
	if (priv->phandle->drv_mode.intf_num == priv->phandle->priv_num) {
		PRINTM(MERROR, "max virtual interface limit reached\n");
		LEAVE();
		return -ENOMEM;
	}
	if ((type != NL80211_IFTYPE_P2P_CLIENT) &&
	    (type != NL80211_IFTYPE_P2P_GO)) {
		PRINTM(MERROR, "Invalid iftype: %d\n", type);
		LEAVE();
		return -EINVAL;
	}

	handle = priv->phandle;
	/* Cancel previous scan req */
	woal_cancel_scan(priv, MOAL_IOCTL_WAIT);
	new_priv =
		woal_alloc_virt_interface(handle, handle->priv_num,
					  MLAN_BSS_TYPE_WIFIDIRECT, name);
	if (!new_priv) {
		PRINTM(MERROR, "Add virtual interface fail.");
		LEAVE();
		return -EFAULT;
	}
	handle->priv_num++;

	wdev = (struct wireless_dev *)&new_priv->w_dev;
	memset(wdev, 0, sizeof(struct wireless_dev));
	ndev = new_priv->netdev;
	SET_NETDEV_DEV(ndev, wiphy_dev(wiphy));
	ndev->ieee80211_ptr = wdev;
	wdev->iftype = type;
	wdev->wiphy = wiphy;
	new_priv->wdev = wdev;
	new_priv->bss_virtual = MTRUE;
	new_priv->pa_netdev = priv->netdev;

	woal_init_sta_dev(ndev, new_priv);

	/* Initialize priv structure */
	woal_init_priv(new_priv, MOAL_CMD_WAIT);
    /** Init to GO/CLIENT mode */
	if (type == NL80211_IFTYPE_P2P_CLIENT)
		woal_cfg80211_init_p2p_client(new_priv);
	else if (type == NL80211_IFTYPE_P2P_GO)
		woal_cfg80211_init_p2p_go(new_priv);
	ret = register_netdevice(ndev);
	if (ret) {
		handle->priv[new_priv->bss_index] = NULL;
		handle->priv_num--;
		free_netdev(ndev);
		ndev = NULL;
		PRINTM(MFATAL, "register net_device failed, ret=%d\n", ret);
		goto done;
	}
	netif_carrier_off(ndev);
	woal_stop_queue(ndev);
	if (new_dev)
		*new_dev = ndev;
#ifdef CONFIG_PROC_FS
	woal_create_proc_entry(new_priv);
#ifdef PROC_DEBUG
	woal_debug_entry(new_priv);
#endif /* PROC_DEBUG */
#endif /* CONFIG_PROC_FS */
done:
	if (ret) {
		if (ndev && ndev->reg_state == NETREG_REGISTERED)
			unregister_netdevice(ndev);
	}
	LEAVE();
	return ret;
}

/**
 *  @brief Notify mlan BSS will be removed.
 *
 *  @param priv          A pointer to moal_private structure
 *
 *  @return              MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_bss_remove(moal_private * priv)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_bss *bss = NULL;
	mlan_status status;

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = (mlan_ioctl_req *) woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		status = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	bss = (mlan_ds_bss *) req->pbuf;
	bss->sub_command = MLAN_OID_BSS_REMOVE;
	req->req_id = MLAN_IOCTL_BSS;
	req->action = MLAN_ACT_SET;
	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, MOAL_CMD_WAIT);

done:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 *  @brief This function removes an virtual interface.
 *
 *  @param wiphy    A pointer to the wiphy structure
 *  @param dev      A pointer to the net_device structure
 *
 *  @return         0 -- success, otherwise fail
 */
int
woal_cfg80211_del_virt_if(struct wiphy *wiphy, struct net_device *dev)
{
	int ret = 0;
	int i = 0;
	moal_private *priv = NULL;
	moal_private *vir_priv = NULL;
	moal_private *remain_priv = NULL;
	moal_handle *handle = (moal_handle *) woal_get_wiphy_priv(wiphy);

	priv = (moal_private *) woal_get_priv_bss_type(handle,
						       MLAN_BSS_TYPE_WIFIDIRECT);
	for (i = 0; i < priv->phandle->priv_num; i++) {
		if ((vir_priv = priv->phandle->priv[i])) {
			if (vir_priv->netdev == dev) {
				PRINTM(MIOCTL,
				       "Find virtual interface, index=%d\n", i);
				break;
			}
		}
	}
	if (vir_priv && vir_priv->netdev == dev) {
		woal_stop_queue(dev);
		netif_carrier_off(dev);
		netif_device_detach(dev);
		woal_cancel_scan(vir_priv, MOAL_IOCTL_WAIT);
		/* cancel previous remain on channel to avoid firmware hang */
		if (priv->phandle->remain_on_channel) {
			t_u8 channel_status;
			remain_priv =
				priv->phandle->priv[priv->phandle->
						    remain_bss_index];
			if (remain_priv) {
				if (woal_cfg80211_remain_on_channel_cfg
				    (remain_priv, MOAL_IOCTL_WAIT, MTRUE,
				     &channel_status, NULL, 0, 0))
					PRINTM(MERROR,
					       "del_virt_if: Fail to cancel remain on channel\n");

				if (priv->phandle->cookie) {
					cfg80211_remain_on_channel_expired(
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
										  remain_priv->
										  netdev,
#else
										  remain_priv->
										  wdev,
#endif
										  priv->
										  phandle->
										  cookie,
										  &priv->
										  phandle->
										  chan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
										  priv->
										  phandle->
										  channel_type,
#endif
										  GFP_ATOMIC);
					priv->phandle->cookie = 0;
				}
				priv->phandle->remain_on_channel = MFALSE;
			}
		}

		woal_clear_all_mgmt_ies(vir_priv);
		woal_cfg80211_deinit_p2p(vir_priv);
		woal_bss_remove(vir_priv);
#ifdef CONFIG_PROC_FS
#ifdef PROC_DEBUG
		/* Remove proc debug */
		woal_debug_remove(vir_priv);
#endif /* PROC_DEBUG */
		woal_proc_remove(vir_priv);
#endif /* CONFIG_PROC_FS */
		/* Last reference is our one */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
		PRINTM(MINFO, "refcnt = %d\n", atomic_read(&dev->refcnt));
#else
		PRINTM(MINFO, "refcnt = %d\n", netdev_refcnt_read(dev));
#endif
		PRINTM(MINFO, "netdev_finish_unregister: %s\n", dev->name);
		/* Clear the priv in handle */
		vir_priv->phandle->priv[vir_priv->bss_index] = NULL;
		priv->phandle->priv_num--;
		if (dev->reg_state == NETREG_REGISTERED)
			unregister_netdevice(dev);
	}
	return ret;
}

/**
 *  @brief This function removes an virtual interface.
 *
 *  @param handle    A pointer to the moal_handle structure
 *
 *  @return        N/A
 */
void
woal_remove_virtual_interface(moal_handle * handle)
{
	moal_private *priv = NULL;
	int vir_intf = 0;
	int i = 0;
	ENTER();
	rtnl_lock();
	for (i = 0; i < handle->priv_num; i++) {
		if ((priv = handle->priv[i])) {
			if (priv->bss_virtual) {
				PRINTM(MCMND, "Remove virtual interface %s\n",
				       priv->netdev->name);
				netif_device_detach(priv->netdev);
				if (priv->netdev->reg_state ==
				    NETREG_REGISTERED)
					unregister_netdevice(priv->netdev);
				handle->priv[i] = NULL;
				vir_intf++;
			}
		}
	}
	rtnl_unlock();
	handle->priv_num -= vir_intf;
	LEAVE();
}
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,37) || defined(COMPAT_WIRELESS)
/**
 * @brief Request the driver to add a virtual interface
 *
 * @param wiphy           A pointer to wiphy structure
 * @param name            Virtual interface name
 * @param type            Virtual interface type
 * @param flags           Flags for the virtual interface
 * @param params          A pointer to vif_params structure
 *
 * @return                A pointer to net_device -- success, otherwise null
 */
struct net_device *
woal_cfg80211_add_virtual_intf(struct wiphy *wiphy,
			       char *name, enum nl80211_iftype type,
			       u32 * flags, struct vif_params *params)
#else
/**
 * @brief Request the driver to add a virtual interface
 *
 * @param wiphy           A pointer to wiphy structure
 * @param name            Virtual interface name
 * @param type            Virtual interface type
 * @param flags           Flags for the virtual interface
 * @param params          A pointer to vif_params structure
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_add_virtual_intf(struct wiphy *wiphy,
			       char *name, enum nl80211_iftype type,
			       u32 * flags, struct vif_params *params)
#endif
#else
/**
 * @brief Request the driver to add a virtual interface
 *
 * @param wiphy           A pointer to wiphy structure
 * @param name            Virtual interface name
 * @param type            Virtual interface type
 * @param flags           Flags for the virtual interface
 * @param params          A pointer to vif_params structure
 *
 * @return                A pointer to wireless_dev -- success, otherwise null
 */
struct wireless_dev *
woal_cfg80211_add_virtual_intf(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
			       const
#endif
			       char *name, enum nl80211_iftype type,
			       u32 * flags, struct vif_params *params)
#endif
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,37) || defined(COMPAT_WIRELESS)
	struct net_device *ndev = NULL;
#endif
	int ret = 0;

	ENTER();
	PRINTM(MIOCTL, "add virtual intf: %d name: %s\n", type, name);
	switch (type) {
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_P2P_GO:
		ret = woal_cfg80211_add_virt_if(wiphy, name, type, flags,
						params, &ndev);
		break;
#endif
#endif
	default:
		PRINTM(MWARN, "Not supported if type: %d\n", type);
		ret = -EFAULT;
		break;
	}
	LEAVE();
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,37) || defined(COMPAT_WIRELESS)
	if (ret)
		return NULL;
	else
		return ndev;
#else
	return ret;
#endif
#else
	if (ret)
		return NULL;
	else
		return (ndev->ieee80211_ptr);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
/**
 * @brief Request the driver to del a virtual interface
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             The pointer to net_device
 *
 * @return               0 -- success, otherwise fail
 */
int
woal_cfg80211_del_virtual_intf(struct wiphy *wiphy, struct net_device *dev)
#else
/**
 * @brief Request the driver to del a virtual interface
 *
 * @param wiphy           A pointer to wiphy structure
 * @param wdev            The pointer to wireless_dev
 *
 * @return               0 -- success, otherwise fail
 */
int
woal_cfg80211_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
#endif
{
	int ret = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	struct net_device *dev = wdev->netdev;
#endif
	ENTER();

	PRINTM(MIOCTL, "del virtual intf %s\n", dev->name);
	ASSERT_RTNL();
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	ret = woal_cfg80211_del_virt_if(wiphy, dev);
#endif
#endif
	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
/**
 * @brief initialize AP or GO parameters

 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param params          A pointer to cfg80211_ap_settings structure
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_add_beacon(struct wiphy *wiphy,
			 struct net_device *dev,
			 struct cfg80211_ap_settings *params)
#else
/**
 * @brief initialize AP or GO parameters

 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param params          A pointer to beacon_parameters structure
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_add_beacon(struct wiphy *wiphy,
			 struct net_device *dev,
			 struct beacon_parameters *params)
#endif
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	int ret = 0;

	ENTER();

	PRINTM(MMSG, "wlan: Starting AP\n");
#ifdef STA_CFG80211
	/*** cancel pending scan */
	woal_cancel_scan(priv, MOAL_IOCTL_WAIT);
#endif
	if (params != NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
		priv->channel =
			ieee80211_frequency_to_channel(params->chandef.chan->
						       center_freq);
#else
		priv->channel =
			ieee80211_frequency_to_channel(params->channel->
						       center_freq);
#endif
#endif
		/* bss config */
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_beacon_config(priv, params)) {
			ret = -EFAULT;
			goto done;
		}

		/* set mgmt frame ies */
		if (MLAN_STATUS_SUCCESS != woal_cfg80211_mgmt_frame_ie(priv,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0) && !defined(COMPAT_WIRELESS)
								       params->
								       tail,
								       params->
								       tail_len,
								       NULL, 0,
								       NULL, 0,
								       NULL, 0,
								       MGMT_MASK_BEACON
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
								       params->
								       beacon.
								       tail,
								       params->
								       beacon.
								       tail_len,
								       params->
								       beacon.
								       proberesp_ies,
								       params->
								       beacon.
								       proberesp_ies_len,
								       params->
								       beacon.
								       assocresp_ies,
								       params->
								       beacon.
								       assocresp_ies_len,
#else
								       params->
								       tail,
								       params->
								       tail_len,
								       params->
								       proberesp_ies,
								       params->
								       proberesp_ies_len,
								       params->
								       assocresp_ies,
								       params->
								       assocresp_ies_len,
#endif
								       NULL, 0,
								       MGMT_MASK_BEACON
								       |
								       MGMT_MASK_PROBE_RESP
								       |
								       MGMT_MASK_ASSOC_RESP
#endif
		    )) {
			ret = -EFAULT;
			goto done;
		}
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) || defined(COMPAT_WIRELESS)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	if (params->beacon.beacon_ies && params->beacon.beacon_ies_len) {
		if (MLAN_STATUS_SUCCESS != woal_cfg80211_mgmt_frame_ie(priv,
								       params->
								       beacon.
								       beacon_ies,
								       params->
								       beacon.
								       beacon_ies_len,
								       NULL, 0,
								       NULL, 0,
								       NULL, 0,
								       MGMT_MASK_BEACON_WPS_P2P))
		{
			PRINTM(MERROR, "Failed to set beacon wps/p2p ie\n");
			ret = -EFAULT;
			goto done;
		}
	}
#else
	if (params->beacon_ies && params->beacon_ies_len) {
		if (MLAN_STATUS_SUCCESS != woal_cfg80211_mgmt_frame_ie(priv,
								       params->
								       beacon_ies,
								       params->
								       beacon_ies_len,
								       NULL, 0,
								       NULL, 0,
								       NULL, 0,
								       MGMT_MASK_BEACON_WPS_P2P))
		{
			PRINTM(MERROR, "Failed to set beacon wps/p2p ie\n");
			ret = -EFAULT;
			goto done;
		}
	}
#endif
#endif

	/* if the bss is stopped, then start it */
	if (priv->bss_started == MFALSE) {
		if (MLAN_STATUS_SUCCESS !=
		    woal_uap_bss_ctrl(priv, MOAL_IOCTL_WAIT, UAP_BSS_START)) {
			ret = -EFAULT;
			goto done;
		}
	}

	PRINTM(MMSG, "wlan: AP started\n");
done:
	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
/**
 * @brief set AP or GO parameter
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param params          A pointer to cfg80211_beacon_data structure
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_set_beacon(struct wiphy *wiphy,
			 struct net_device *dev,
			 struct cfg80211_beacon_data *params)
#else
/**
 * @brief set AP or GO parameter
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param params          A pointer to beacon_parameters structure
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_set_beacon(struct wiphy *wiphy,
			 struct net_device *dev,
			 struct beacon_parameters *params)
#endif
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	int ret = 0;

	ENTER();

	PRINTM(MIOCTL, "set beacon\n");
	if (params != NULL) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0) && !defined(COMPAT_WIRELESS)
		if (params->tail && params->tail_len) {
			if (MLAN_STATUS_SUCCESS !=
			    woal_cfg80211_mgmt_frame_ie(priv,
							params->tail,
							params->tail_len, NULL,
							0, NULL, 0, NULL, 0,
							MGMT_MASK_BEACON)) {
				ret = -EFAULT;
				goto done;
			}
		}
#else
		if (params->tail && params->tail_len) {
			if (MLAN_STATUS_SUCCESS !=
			    woal_cfg80211_mgmt_frame_ie(priv, params->tail,
							params->tail_len, NULL,
							0, NULL, 0, NULL, 0,
							MGMT_MASK_BEACON)) {
				ret = -EFAULT;
				goto done;
			}
		}
		if (params->beacon_ies && params->beacon_ies_len) {
			if (MLAN_STATUS_SUCCESS !=
			    woal_cfg80211_mgmt_frame_ie(priv,
							params->beacon_ies,
							params->beacon_ies_len,
							NULL, 0, NULL, 0, NULL,
							0,
							MGMT_MASK_BEACON_WPS_P2P))
			{
				PRINTM(MERROR,
				       "Failed to set beacon wps/p2p ie\n");
				ret = -EFAULT;
				goto done;
			}
		}
		if (params->proberesp_ies && params->proberesp_ies_len) {
			if (MLAN_STATUS_SUCCESS !=
			    woal_cfg80211_mgmt_frame_ie(priv, NULL, 0,
							params->proberesp_ies,
							params->
							proberesp_ies_len, NULL,
							0, NULL, 0,
							MGMT_MASK_PROBE_RESP)) {
				ret = -EFAULT;
				goto done;
			}
		}
		if (params->assocresp_ies && params->assocresp_ies_len) {
			if (MLAN_STATUS_SUCCESS !=
			    woal_cfg80211_mgmt_frame_ie(priv, NULL, 0, NULL, 0,
							params->assocresp_ies,
							params->
							assocresp_ies_len, NULL,
							0,
							MGMT_MASK_ASSOC_RESP)) {
				ret = -EFAULT;
				goto done;
			}
		}
#endif
	}

done:
	LEAVE();
	return ret;
}

/**
 * @brief reset AP or GO parameters
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_del_beacon(struct wiphy *wiphy, struct net_device *dev)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	int ret = 0;
#ifdef STA_SUPPORT
	moal_private *pmpriv = NULL;
#endif

	ENTER();

	PRINTM(MMSG, "wlan: Stoping AP\n");
	woal_deauth_all_station(priv);
	/* if the bss is still running, then stop it */
	if (priv->bss_started == MTRUE) {
		if (MLAN_STATUS_SUCCESS !=
		    woal_uap_bss_ctrl(priv, MOAL_IOCTL_WAIT, UAP_BSS_STOP)) {
			ret = -EFAULT;
			goto done;
		}
		if (MLAN_STATUS_SUCCESS !=
		    woal_uap_bss_ctrl(priv, MOAL_IOCTL_WAIT, UAP_BSS_RESET)) {
			ret = -EFAULT;
			goto done;
		}
		/* Set WLAN MAC addresses */
		if (MLAN_STATUS_SUCCESS != woal_request_set_mac_address(priv)) {
			PRINTM(MERROR, "Set MAC address failed\n");
			ret = -EFAULT;
			goto done;
		}
	}
	woal_clear_all_mgmt_ies(priv);

#ifdef STA_SUPPORT
	if (!woal_is_any_interface_active(priv->phandle)) {
		if ((pmpriv =
		     woal_get_priv((moal_handle *) priv->phandle,
				   MLAN_BSS_ROLE_STA))) {
			woal_set_scan_time(pmpriv, ACTIVE_SCAN_CHAN_TIME,
					   PASSIVE_SCAN_CHAN_TIME,
					   SPECIFIC_SCAN_CHAN_TIME);
		}
	}
#endif

	priv->cipher = 0;
	priv->key_len = 0;
	priv->channel = 0;
	PRINTM(MMSG, "wlan: AP stopped\n");
done:
	LEAVE();
	return ret;
}

/**
 * @brief del station
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param mac_addr		  A pointer to station mac address
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev,
			  u8 * mac_addr)
{
	ENTER();
    /** we can not send deauth here, it will cause WPS failure */
	if (mac_addr)
		PRINTM(MIOCTL, "del station: " MACSTR "\n", MAC2STR(mac_addr));
	else
		PRINTM(MIOCTL, "del all station\n");
	LEAVE();
	return 0;

}

/**
 * @brief Get station info
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param mac			  A pointer to station mac address
 * @param stainfo		  A pointer to station_info structure
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_uap_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev,
			      u8 * mac, struct station_info *stainfo)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	int ret = -EFAULT;
	int i = 0;
	mlan_ds_get_info *info = NULL;
	mlan_ioctl_req *ioctl_req = NULL;

	ENTER();
	if (priv->media_connected == MFALSE) {
		PRINTM(MINFO, "cfg80211: Media not connected!\n");
		LEAVE();
		return -ENOENT;
	}

	/* Allocate an IOCTL request buffer */
	ioctl_req =
		(mlan_ioctl_req *)
		woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_get_info));
	if (ioctl_req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	info = (mlan_ds_get_info *) ioctl_req->pbuf;
	info->sub_command = MLAN_OID_UAP_STA_LIST;
	ioctl_req->req_id = MLAN_IOCTL_GET_INFO;
	ioctl_req->action = MLAN_ACT_GET;

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, ioctl_req, MOAL_IOCTL_WAIT)) {
		goto done;
	}
	for (i = 0; i < info->param.sta_list.sta_count; i++) {
		if (!memcmp
		    (info->param.sta_list.info[i].mac_address, mac, ETH_ALEN)) {
			PRINTM(MIOCTL, "Get station: " MACSTR " RSSI=%d\n",
			       MAC2STR(mac),
			       (int)info->param.sta_list.info[i].rssi);
			stainfo->filled =
				STATION_INFO_INACTIVE_TIME |
				STATION_INFO_SIGNAL;
			stainfo->inactive_time = 0;
			stainfo->signal = info->param.sta_list.info[i].rssi;
			ret = 0;
			break;
		}
	}
done:
	if (ioctl_req)
		kfree(ioctl_req);
	LEAVE();
	return ret;
}

/**
 * @brief Register the device with cfg80211
 *
 * @param dev       A pointer to net_device structure
 * @param bss_type  BSS type
 *
 * @return          MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
woal_register_uap_cfg80211(struct net_device * dev, t_u8 bss_type)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	moal_private *priv = (moal_private *) netdev_priv(dev);
	struct wireless_dev *wdev = NULL;

	ENTER();

	wdev = (struct wireless_dev *)&priv->w_dev;
	memset(wdev, 0, sizeof(struct wireless_dev));

	wdev->wiphy = priv->phandle->wiphy;
	if (!wdev->wiphy) {
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	if (bss_type == MLAN_BSS_TYPE_UAP)
		wdev->iftype = NL80211_IFTYPE_AP;

	dev_net_set(dev, wiphy_net(wdev->wiphy));
	dev->ieee80211_ptr = wdev;
	SET_NETDEV_DEV(dev, wiphy_dev(wdev->wiphy));
	priv->wdev = wdev;

	LEAVE();
	return ret;
}
