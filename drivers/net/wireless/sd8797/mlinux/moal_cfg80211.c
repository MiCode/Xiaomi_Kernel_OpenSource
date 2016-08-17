/** @file moal_cfg80211.c
  *
  * @brief This file contains the functions for CFG80211.
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

#include    "moal_cfg80211.h"
#ifdef UAP_CFG80211
#ifdef UAP_SUPPORT
#include    "moal_uap.h"
#endif
#endif

/********************************************************
				Local Variables
********************************************************/
/** Supported rates to be advertised to the cfg80211 */
static struct ieee80211_rate cfg80211_rates[] = {
	{.bitrate = 10,.hw_value = 2,},
	{.bitrate = 20,.hw_value = 4,},
	{.bitrate = 55,.hw_value = 11},
	{.bitrate = 110,.hw_value = 22,},
	{.bitrate = 220,.hw_value = 44,},
	{.bitrate = 60,.hw_value = 12,},
	{.bitrate = 90,.hw_value = 18,},
	{.bitrate = 120,.hw_value = 24,},
	{.bitrate = 180,.hw_value = 36,},
	{.bitrate = 240,.hw_value = 48,},
	{.bitrate = 360,.hw_value = 72,},
	{.bitrate = 480,.hw_value = 96,},
	{.bitrate = 540,.hw_value = 108,},
	{.bitrate = 720,.hw_value = 144,},
};

/** Channel definitions for 2 GHz to be advertised to cfg80211 */
static struct ieee80211_channel cfg80211_channels_2ghz[] = {
	{.center_freq = 2412,.hw_value = 1,.max_power = 20},
	{.center_freq = 2417,.hw_value = 2,.max_power = 20},
	{.center_freq = 2422,.hw_value = 3,.max_power = 20},
	{.center_freq = 2427,.hw_value = 4,.max_power = 20},
	{.center_freq = 2432,.hw_value = 5,.max_power = 20},
	{.center_freq = 2437,.hw_value = 6,.max_power = 20},
	{.center_freq = 2442,.hw_value = 7,.max_power = 20},
	{.center_freq = 2447,.hw_value = 8,.max_power = 20},
	{.center_freq = 2452,.hw_value = 9,.max_power = 20},
	{.center_freq = 2457,.hw_value = 10,.max_power = 20},
	{.center_freq = 2462,.hw_value = 11,.max_power = 20},
	{.center_freq = 2467,.hw_value = 12,.max_power = 20},
	{.center_freq = 2472,.hw_value = 13,.max_power = 20},
	{.center_freq = 2484,.hw_value = 14,.max_power = 20},
};

/** Channel definitions for 5 GHz to be advertised to cfg80211 */
static struct ieee80211_channel cfg80211_channels_5ghz[] = {
	{.center_freq = 5040,.hw_value = 8,.max_power = 20},
	{.center_freq = 5060,.hw_value = 12,.max_power = 20},
	{.center_freq = 5080,.hw_value = 16,.max_power = 20},
	{.center_freq = 5170,.hw_value = 34,.max_power = 20},
	{.center_freq = 5190,.hw_value = 38,.max_power = 20},
	{.center_freq = 5210,.hw_value = 42,.max_power = 20},
	{.center_freq = 5230,.hw_value = 46,.max_power = 20},
	{.center_freq = 5180,.hw_value = 36,.max_power = 20},
	{.center_freq = 5200,.hw_value = 40,.max_power = 20},
	{.center_freq = 5220,.hw_value = 44,.max_power = 20},
	{.center_freq = 5240,.hw_value = 48,.max_power = 20},
	{.center_freq = 5260,.hw_value = 52,.max_power = 20},
	{.center_freq = 5280,.hw_value = 56,.max_power = 20},
	{.center_freq = 5300,.hw_value = 60,.max_power = 20},
	{.center_freq = 5320,.hw_value = 64,.max_power = 20},
	{.center_freq = 5500,.hw_value = 100,.max_power = 20},
	{.center_freq = 5520,.hw_value = 104,.max_power = 20},
	{.center_freq = 5540,.hw_value = 108,.max_power = 20},
	{.center_freq = 5560,.hw_value = 112,.max_power = 20},
	{.center_freq = 5580,.hw_value = 116,.max_power = 20},
	{.center_freq = 5600,.hw_value = 120,.max_power = 20},
	{.center_freq = 5620,.hw_value = 124,.max_power = 20},
	{.center_freq = 5640,.hw_value = 128,.max_power = 20},
	{.center_freq = 5660,.hw_value = 132,.max_power = 20},
	{.center_freq = 5680,.hw_value = 136,.max_power = 20},
	{.center_freq = 5700,.hw_value = 140,.max_power = 20},
	{.center_freq = 5745,.hw_value = 149,.max_power = 20},
	{.center_freq = 5765,.hw_value = 153,.max_power = 20},
	{.center_freq = 5785,.hw_value = 157,.max_power = 20},
	{.center_freq = 5805,.hw_value = 161,.max_power = 20},
	{.center_freq = 5825,.hw_value = 165,.max_power = 20},
};

/********************************************************
				Global Variables
********************************************************/
extern int cfg80211_wext;

struct ieee80211_supported_band cfg80211_band_2ghz = {
	.channels = cfg80211_channels_2ghz,
	.n_channels = ARRAY_SIZE(cfg80211_channels_2ghz),
	.bitrates = cfg80211_rates,
	.n_bitrates = ARRAY_SIZE(cfg80211_rates),
};

struct ieee80211_supported_band cfg80211_band_5ghz = {
	.channels = cfg80211_channels_5ghz,
	.n_channels = ARRAY_SIZE(cfg80211_channels_5ghz),
	.bitrates = cfg80211_rates + 5,
	.n_bitrates = ARRAY_SIZE(cfg80211_rates) - 5,
};

#ifndef WLAN_CIPHER_SUITE_SMS4
#define WLAN_CIPHER_SUITE_SMS4      0x00000020
#endif

#ifndef WLAN_CIPHER_SUITE_AES_CMAC
#define WLAN_CIPHER_SUITE_AES_CMAC  0x000FAC06
#endif

/* Supported crypto cipher suits to be advertised to cfg80211 */
const u32 cfg80211_cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
	WLAN_CIPHER_SUITE_SMS4,
	WLAN_CIPHER_SUITE_AES_CMAC,
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 29)
#ifdef UAP_SUPPORT
/** Network device handlers for uAP */
extern const struct net_device_ops woal_uap_netdev_ops;
#endif
#ifdef STA_SUPPORT
/** Network device handlers for STA */
extern const struct net_device_ops woal_netdev_ops;
#endif
#endif

/********************************************************
				Local Functions
********************************************************/

/********************************************************
				Global Functions
********************************************************/
/**
 * @brief Get the private structure from wiphy
 *
 * @param wiphy     A pointer to wiphy structure
 *
 * @return          Pointer to moal_private
 */
void *
woal_get_wiphy_priv(struct wiphy *wiphy)
{
	return (void *)(*(unsigned long *)wiphy_priv(wiphy));
}

/**
 * @brief Get the private structure from net device
 *
 * @param dev       A pointer to net_device structure
 *
 * @return          Pointer to moal_private
 */
void *
woal_get_netdev_priv(struct net_device *dev)
{
	return (void *)netdev_priv(dev);
}

/**
 *  @brief Check if any interface is active
 *
 *  @param handle        A pointer to moal_handle
 *
 *
 *  @return              MTRUE/MFALSE;
 */
t_u8
woal_is_any_interface_active(moal_handle * handle)
{
	int i;
	for (i = 0; i < handle->priv_num; i++) {
#ifdef STA_SUPPORT
		if (GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_STA) {
			if (handle->priv[i]->media_connected == MTRUE)
				return MTRUE;
		}
#endif
#ifdef UAP_SUPPORT
		if (GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_UAP) {
			if (handle->priv[i]->bss_started == MTRUE)
				return MTRUE;
		}
#endif
	}
	return MFALSE;
}

/**
 *  @brief Get current frequency of active interface
 *
 *  @param handle        A pointer to moal_handle
 *
 *  @return              channel frequency
 */
int
woal_get_active_intf_freq(moal_handle * handle)
{
	int i;
	for (i = 0; i < handle->priv_num; i++) {
#ifdef STA_SUPPORT
		if (GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_STA) {
			if (handle->priv[i]->media_connected == MTRUE)
				return ieee80211_channel_to_frequency(handle->
								      priv[i]->
								      channel,
								      (handle->
								       priv[i]->
								       channel
								       <=
								       14 ?
								       IEEE80211_BAND_2GHZ
								       :
								       IEEE80211_BAND_5GHZ));
		}
#endif
#ifdef UAP_SUPPORT
		if (GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_UAP) {
			if (handle->priv[i]->bss_started == MTRUE)
				return ieee80211_channel_to_frequency(handle->
								      priv[i]->
								      channel,
								      (handle->
								       priv[i]->
								       channel
								       <=
								       14 ?
								       IEEE80211_BAND_2GHZ
								       :
								       IEEE80211_BAND_5GHZ));
		}
#endif
	}
	return 0;
}

/**
 *  @brief Convert driver band configuration to IEEE band type
 *
 *  @param band     Driver band configuration
 *
 *  @return         IEEE band type
 */
t_u8
woal_band_cfg_to_ieee_band(t_u32 band)
{
	t_u8 ret_radio_type;

	ENTER();

	switch (band) {
	case BAND_A:
	case BAND_AN:
	case BAND_A | BAND_AN:
		ret_radio_type = IEEE80211_BAND_5GHZ;
		break;
	case BAND_B:
	case BAND_G:
	case BAND_B | BAND_G:
	case BAND_GN:
	case BAND_B | BAND_GN:
	default:
		ret_radio_type = IEEE80211_BAND_2GHZ;
		break;
	}

	LEAVE();
	return ret_radio_type;
}

/**
 *  @brief Set/Enable encryption key
 *
 *  @param priv             A pointer to moal_private structure
 *  @param is_enable_wep    Enable WEP default key
 *  @param cipher           Cipher suite selector
 *  @param key              A pointer to key
 *  @param key_len          Key length
 *  @param seq              A pointer to sequence
 *  @param seq_len          Sequence length
 *  @param key_index        Key index
 *  @param addr             Mac for which key is to be set
 *  @param disable          Key disabled or not
 *
 *  @return                 MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
mlan_status
woal_cfg80211_set_key(moal_private * priv, t_u8 is_enable_wep,
		      t_u32 cipher, const t_u8 * key, int key_len,
		      const t_u8 * seq, int seq_len, t_u8 key_index,
		      const t_u8 * addr, int disable)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	t_u8 bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_ENCRYPT_KEY;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_SET;

	if (is_enable_wep) {
		sec->param.encrypt_key.key_index = key_index;
		sec->param.encrypt_key.is_current_wep_key = MTRUE;
	} else if (!disable) {
#ifdef UAP_CFG80211
#ifdef UAP_SUPPORT
		if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
			if (key && key_len) {
				priv->key_len = key_len;
				memcpy(priv->key_material, key, key_len);
				priv->cipher = cipher;
				priv->key_index = key_index;
			}
			if ((cipher == WLAN_CIPHER_SUITE_WEP40) ||
			    (cipher == WLAN_CIPHER_SUITE_WEP104)) {
				PRINTM(MIOCTL, "Set WEP key\n");
				ret = MLAN_STATUS_SUCCESS;
				goto done;
			}
		}
#endif
#endif
		if (cipher != WLAN_CIPHER_SUITE_WEP40 &&
		    cipher != WLAN_CIPHER_SUITE_WEP104 &&
		    cipher != WLAN_CIPHER_SUITE_TKIP &&
		    cipher != WLAN_CIPHER_SUITE_SMS4 &&
		    cipher != WLAN_CIPHER_SUITE_AES_CMAC &&
		    cipher != WLAN_CIPHER_SUITE_CCMP) {
			PRINTM(MERROR, "Invalid cipher suite specified\n");
			ret = MLAN_STATUS_FAILURE;
			goto done;
		}
		sec->param.encrypt_key.key_index = key_index;
		if (key && key_len) {
			memcpy(sec->param.encrypt_key.key_material, key,
			       key_len);
			sec->param.encrypt_key.key_len = key_len;
		}
		/* Set WAPI key */
		if (cipher == WLAN_CIPHER_SUITE_SMS4) {
			sec->param.encrypt_key.is_wapi_key = MTRUE;
			if (seq_len) {
				memcpy(sec->param.encrypt_key.pn, seq, PN_SIZE);
				DBG_HEXDUMP(MCMD_D, "WAPI PN",
					    sec->param.encrypt_key.pn, seq_len);
			}
		}
		if (addr) {
			memcpy(sec->param.encrypt_key.mac_addr, addr, ETH_ALEN);
			if (0 ==
			    memcmp(sec->param.encrypt_key.mac_addr, bcast_addr,
				   ETH_ALEN))
				sec->param.encrypt_key.key_flags =
					KEY_FLAG_GROUP_KEY;
			else
				sec->param.encrypt_key.key_flags =
					KEY_FLAG_SET_TX_KEY;
		} else {
			memcpy(sec->param.encrypt_key.mac_addr, bcast_addr,
			       ETH_ALEN);
			sec->param.encrypt_key.key_flags = KEY_FLAG_GROUP_KEY;
		}
		if (seq && seq_len) {
			memcpy(sec->param.encrypt_key.pn, seq, seq_len);
			sec->param.encrypt_key.key_flags |=
				KEY_FLAG_RX_SEQ_VALID;
		}

		if (cipher == WLAN_CIPHER_SUITE_AES_CMAC) {
			sec->param.encrypt_key.key_flags |=
				KEY_FLAG_AES_MCAST_IGTK;
		}
	} else {
		if (key_index == KEY_INDEX_CLEAR_ALL)
			sec->param.encrypt_key.key_disable = MTRUE;
		else {
			sec->param.encrypt_key.key_remove = MTRUE;
			sec->param.encrypt_key.key_index = key_index;
		}
		sec->param.encrypt_key.key_flags = KEY_FLAG_REMOVE_KEY;
		if (addr)
			memcpy(sec->param.encrypt_key.mac_addr, addr, ETH_ALEN);
	}

	/* Send IOCTL request to MLAN */
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}

/**
 * @brief Set/Enable the WEP key to driver
 *
 * @param priv      A pointer to moal_private structure
 * @param key       A pointer to key data
 * @param key_len   Length of the key data
 * @param index     Key index
 *
 * @return          MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
woal_cfg80211_set_wep_keys(moal_private * priv, const t_u8 * key, int key_len,
			   t_u8 index)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	t_u32 cipher = 0;

	ENTER();

	if (key_len) {
		if (key_len == 5)
			cipher = WLAN_CIPHER_SUITE_WEP40;
		else
			cipher = WLAN_CIPHER_SUITE_WEP104;
		ret = woal_cfg80211_set_key(priv, 0, cipher, key, key_len, NULL,
					    0, index, NULL, 0);
	} else {
		/* No key provided so it is enable key. We want to just set the
		   transmit key index */
		woal_cfg80211_set_key(priv, 1, cipher, key, key_len, NULL, 0,
				      index, NULL, 0);
	}

	LEAVE();
	return ret;
}

/**
 * @brief clear all mgmt ies
 *
 * @param priv              A pointer to moal private structure
 * @return                  N/A
 */
void
woal_clear_all_mgmt_ies(moal_private * priv)
{
	t_u16 mask = 0;
	/* clear BEACON WPS/P2P IE */
	if (priv->beacon_wps_index != MLAN_CUSTOM_IE_AUTO_IDX_MASK) {
		PRINTM(MCMND, "Clear BEACON WPS ie\n");
		woal_cfg80211_mgmt_frame_ie(priv, NULL, 0, NULL, 0, NULL, 0,
					    NULL, 0, MGMT_MASK_BEACON_WPS_P2P);
	}
	/* clear mgmt frame ies */
	if (priv->probereq_index != MLAN_CUSTOM_IE_AUTO_IDX_MASK)
		mask |= MGMT_MASK_PROBE_REQ;
	if (priv->beacon_index != MLAN_CUSTOM_IE_AUTO_IDX_MASK)
		mask |= MGMT_MASK_BEACON;
	if (priv->proberesp_index != MLAN_CUSTOM_IE_AUTO_IDX_MASK)
		mask |= MGMT_MASK_PROBE_RESP;
	if (priv->assocresp_index != MLAN_CUSTOM_IE_AUTO_IDX_MASK)
		mask |= MGMT_MASK_ASSOC_RESP;
	if (mask) {
		PRINTM(MCMND, "Clear IES: 0x%x 0x%x 0x%x 0x%x\n",
		       priv->beacon_index, priv->probereq_index,
		       priv->proberesp_index, priv->assocresp_index);
		woal_cfg80211_mgmt_frame_ie(priv, NULL, 0, NULL, 0, NULL, 0,
					    NULL, 0, mask);
	}
}

#if defined(STA_SUPPORT) && defined(UAP_SUPPORT)
/**
 * @brief set bss role
 *
 * @param priv              A pointer to moal private structure
 * @param action            Action: set or get
 * @param role              A pointer to bss role
 *
 * @return                  0 -- success, otherwise fail
 */
int
woal_cfg80211_bss_role_cfg(moal_private * priv, t_u16 action, t_u8 * bss_role)
{
	int ret = 0;

	ENTER();

	if (action == MLAN_ACT_SET) {
		/* Reset interface */
		woal_reset_intf(priv, MOAL_IOCTL_WAIT, MFALSE);
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_bss_role_cfg(priv, action, MOAL_IOCTL_WAIT, bss_role)) {
		ret = -EFAULT;
		goto done;
	}

	if (action == MLAN_ACT_SET) {
		/* set back the mac address */
		woal_request_set_mac_address(priv);
		/* clear the mgmt ies */
		woal_clear_all_mgmt_ies(priv);
		/* Initialize private structures */
		woal_init_priv(priv, MOAL_IOCTL_WAIT);

		/* Enable interfaces */
		netif_device_attach(priv->netdev);
		woal_start_queue(priv->netdev);
	}

done:
	LEAVE();
	return ret;
}
#endif

#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION

/**
 * @brief initialize p2p client for wpa_supplicant
 *
 * @param priv			A pointer to moal private structure
 *
 * @return              0 -- success, otherwise fail
 */
int
woal_cfg80211_init_p2p_client(moal_private * priv)
{
	int ret = MLAN_STATUS_SUCCESS;
	t_u16 wifi_direct_mode = WIFI_DIRECT_MODE_DISABLE;
	t_u8 bss_role;

	ENTER();

	/* bss type check */
	if (priv->bss_type != MLAN_BSS_TYPE_WIFIDIRECT) {
		PRINTM(MERROR, "Unexpected bss type when init p2p client\n");
		ret = -EFAULT;
		goto done;
	}

	/* get the bss role */
	if (MLAN_STATUS_SUCCESS !=
	    woal_cfg80211_bss_role_cfg(priv, MLAN_ACT_GET, &bss_role)) {
		ret = -EFAULT;
		goto done;
	}

	if (bss_role != MLAN_BSS_ROLE_STA) {
		bss_role = MLAN_BSS_ROLE_STA;
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_bss_role_cfg(priv, MLAN_ACT_SET, &bss_role)) {
			ret = -EFAULT;
			goto done;
		}
	}

	wifi_direct_mode = WIFI_DIRECT_MODE_DISABLE;
	if (MLAN_STATUS_SUCCESS !=
	    woal_wifi_direct_mode_cfg(priv, MLAN_ACT_SET, &wifi_direct_mode)) {
		ret = -EFAULT;
		goto done;
	}

	/* first, init wifi direct to listen mode */
	wifi_direct_mode = WIFI_DIRECT_MODE_LISTEN;
	if (MLAN_STATUS_SUCCESS !=
	    woal_wifi_direct_mode_cfg(priv, MLAN_ACT_SET, &wifi_direct_mode)) {
		ret = -EFAULT;
		goto done;
	}

	/* second, init wifi direct client */
	wifi_direct_mode = WIFI_DIRECT_MODE_CLIENT;
	if (MLAN_STATUS_SUCCESS !=
	    woal_wifi_direct_mode_cfg(priv, MLAN_ACT_SET, &wifi_direct_mode)) {
		ret = -EFAULT;
		goto done;
	}
done:
	LEAVE();
	return ret;
}

/**
 * @brief initialize p2p GO for wpa_supplicant
 *
 * @param priv			A pointer to moal private structure
 *
 * @return              0 -- success, otherwise fail
 */
int
woal_cfg80211_init_p2p_go(moal_private * priv)
{
	int ret = MLAN_STATUS_SUCCESS;
	t_u16 wifi_direct_mode;
	t_u8 bss_role;

	ENTER();

	/* bss type check */
	if (priv->bss_type != MLAN_BSS_TYPE_WIFIDIRECT) {
		PRINTM(MERROR, "Unexpected bss type when init p2p GO\n");
		ret = -EFAULT;
		goto done;
	}

	wifi_direct_mode = WIFI_DIRECT_MODE_DISABLE;
	if (MLAN_STATUS_SUCCESS !=
	    woal_wifi_direct_mode_cfg(priv, MLAN_ACT_SET, &wifi_direct_mode)) {
		ret = -EFAULT;
		goto done;
	}

	/* first, init wifi direct to listen mode */
	wifi_direct_mode = WIFI_DIRECT_MODE_LISTEN;
	if (MLAN_STATUS_SUCCESS !=
	    woal_wifi_direct_mode_cfg(priv, MLAN_ACT_SET, &wifi_direct_mode)) {
		ret = -EFAULT;
		goto done;
	}

	/* second, init wifi direct to GO mode */
	wifi_direct_mode = WIFI_DIRECT_MODE_GO;
	if (MLAN_STATUS_SUCCESS !=
	    woal_wifi_direct_mode_cfg(priv, MLAN_ACT_SET, &wifi_direct_mode)) {
		ret = -EFAULT;
		goto done;
	}

	/* get the bss role, and set it to uAP */
	if (MLAN_STATUS_SUCCESS !=
	    woal_cfg80211_bss_role_cfg(priv, MLAN_ACT_GET, &bss_role)) {
		ret = -EFAULT;
		goto done;
	}

	if (bss_role != MLAN_BSS_ROLE_UAP) {
		bss_role = MLAN_BSS_ROLE_UAP;
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_bss_role_cfg(priv, MLAN_ACT_SET, &bss_role)) {
			ret = -EFAULT;
			goto done;
		}
	}

done:
	LEAVE();
	return ret;
}

/**
 * @brief reset bss role and wifi direct mode for wpa_supplicant
 *
 * @param priv			A pointer to moal private structure
 *
 * @return              0 -- success, otherwise fail
 */
int
woal_cfg80211_deinit_p2p(moal_private * priv)
{
	int ret = MLAN_STATUS_SUCCESS;
	t_u16 wifi_direct_mode;
	t_u8 bss_role;
	t_u8 channel_status;
	moal_private *remain_priv = NULL;

	ENTER();

	/* bss type check */
	if (priv->bss_type != MLAN_BSS_TYPE_WIFIDIRECT) {
		PRINTM(MERROR, "Unexpected bss type when deinit p2p\n");
		ret = -EFAULT;
		goto done;
	}

	/* cancel previous remain on channel */
	if (priv->phandle->remain_on_channel) {
		remain_priv =
			priv->phandle->priv[priv->phandle->remain_bss_index];
		if (!remain_priv) {
			PRINTM(MERROR,
			       "deinit_p2p: wrong remain_bss_index=%d\n",
			       priv->phandle->remain_bss_index);
			ret = -EFAULT;
			goto done;
		}
		if (woal_cfg80211_remain_on_channel_cfg
		    (remain_priv, MOAL_IOCTL_WAIT, MTRUE, &channel_status, NULL,
		     0, 0)) {
			PRINTM(MERROR,
			       "deinit_p2p: Fail to cancel remain on channel\n");
			ret = -EFAULT;
			goto done;
		}
		if (priv->phandle->cookie) {
			cfg80211_remain_on_channel_expired(
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
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
								  phandle->chan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
								  priv->
								  phandle->
								  channel_type,
#endif
								  GFP_ATOMIC);
			priv->phandle->cookie = 0;
		}
		priv->phandle->remain_on_channel = MFALSE;
	}

	/* get the bss role */
	if (MLAN_STATUS_SUCCESS != woal_cfg80211_bss_role_cfg(priv,
							      MLAN_ACT_GET,
							      &bss_role)) {
		ret = -EFAULT;
		goto done;
	}

	/* reset bss role */
	if (bss_role != MLAN_BSS_ROLE_STA) {
		bss_role = MLAN_BSS_ROLE_STA;
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_bss_role_cfg(priv, MLAN_ACT_SET, &bss_role)) {
			ret = -EFAULT;
			goto done;
		}
	}

	wifi_direct_mode = WIFI_DIRECT_MODE_DISABLE;
	if (MLAN_STATUS_SUCCESS !=
	    woal_wifi_direct_mode_cfg(priv, MLAN_ACT_SET, &wifi_direct_mode)) {
		ret = -EFAULT;
		goto done;
	}
done:
	LEAVE();
	return ret;
}
#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */

/**
 * @brief Request the driver to change the interface type
 *
 * @param wiphy         A pointer to wiphy structure
 * @param dev           A pointer to net_device structure
 * @param type          Virtual interface types
 * @param flags         Flags
 * @param params        A pointer to vif_params structure
 *
 * @return              0 -- success, otherwise fail
 */
int
woal_cfg80211_change_virtual_intf(struct wiphy *wiphy,
				  struct net_device *dev,
				  enum nl80211_iftype type, u32 * flags,
				  struct vif_params *params)
{
	int ret = 0;
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	mlan_ds_bss *bss = NULL;
	mlan_ioctl_req *req = NULL;
#if defined(STA_SUPPORT) && defined(UAP_SUPPORT)
	t_u8 bss_role;
#endif

	ENTER();

	if (priv->wdev->iftype == type) {
		PRINTM(MINFO, "Already set to required type\n");
		goto done;
	}
	PRINTM(MIOCTL, "%s: change virturl intf=%d\n", dev->name, type);
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	/** cancel previous remain on channel to avoid firmware hang */
	if (priv->phandle->remain_on_channel) {
		t_u8 channel_status;
		moal_private *remain_priv = NULL;
		remain_priv =
			priv->phandle->priv[priv->phandle->remain_bss_index];
		if (!remain_priv) {
			PRINTM(MERROR,
			       "change_virtual_intf:wrong remain_bss_index=%d\n",
			       priv->phandle->remain_bss_index);
			ret = -EFAULT;
			goto done;
		}
		if (woal_cfg80211_remain_on_channel_cfg
		    (remain_priv, MOAL_IOCTL_WAIT, MTRUE, &channel_status, NULL,
		     0, 0)) {
			PRINTM(MERROR,
			       "change_virtual_intf: Fail to cancel remain on channel\n");
			ret = -EFAULT;
			goto done;
		}
		if (priv->phandle->cookie) {
			cfg80211_remain_on_channel_expired(
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
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
								  phandle->chan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
								  priv->
								  phandle->
								  channel_type,
#endif
								  GFP_ATOMIC);
			priv->phandle->cookie = 0;
		}
		priv->phandle->remain_on_channel = MFALSE;
	}
#endif
#endif

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	bss = (mlan_ds_bss *) req->pbuf;
	bss->sub_command = MLAN_OID_BSS_MODE;
	req->req_id = MLAN_IOCTL_BSS;
	req->action = MLAN_ACT_SET;

	switch (type) {
	case NL80211_IFTYPE_ADHOC:
		bss->param.bss_mode = MLAN_BSS_MODE_IBSS;
		priv->wdev->iftype = NL80211_IFTYPE_ADHOC;
		PRINTM(MINFO, "Setting interface type to adhoc\n");
		break;
	case NL80211_IFTYPE_STATION:
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
		if (priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT
		    && (priv->wdev->iftype == NL80211_IFTYPE_AP
			|| priv->wdev->iftype == NL80211_IFTYPE_P2P_GO
			|| priv->wdev->iftype == NL80211_IFTYPE_P2P_CLIENT)) {
			if (priv->phandle->is_go_timer_set) {
				woal_cancel_timer(&priv->phandle->go_timer);
				priv->phandle->is_go_timer_set = MFALSE;
			}
			/* if we support wifi direct && priv->bss_type ==
			   wifi_direct, and currently the interface type is AP
			   or GO or client, that means wpa_supplicant deinit()
			   wifi direct interface, so we should deinit bss_role
			   and wifi direct mode, for other bss_type, we should
			   not update bss_role and wifi direct mode */

			if (MLAN_STATUS_SUCCESS !=
			    woal_cfg80211_deinit_p2p(priv)) {
				ret = -EFAULT;
				goto done;
			}
		}
#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */
#if defined(STA_SUPPORT) && defined(UAP_SUPPORT)
		if (priv->bss_type == MLAN_BSS_TYPE_UAP) {
			woal_cfg80211_del_beacon(wiphy, dev);
			bss_role = MLAN_BSS_ROLE_STA;
			woal_cfg80211_bss_role_cfg(priv, MLAN_ACT_SET,
						   &bss_role);
			PRINTM(MIOCTL, "set bss role for STA\n");
		}
#endif
		bss->param.bss_mode = MLAN_BSS_MODE_INFRA;
		priv->wdev->iftype = NL80211_IFTYPE_STATION;
		PRINTM(MINFO, "Setting interface type to managed\n");
		break;
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	case NL80211_IFTYPE_P2P_CLIENT:
		if (priv->phandle->is_go_timer_set) {
			woal_cancel_timer(&priv->phandle->go_timer);
			priv->phandle->is_go_timer_set = MFALSE;
		}

		if (MLAN_STATUS_SUCCESS != woal_cfg80211_init_p2p_client(priv)) {
			ret = -EFAULT;
			goto done;
		}

		bss->param.bss_mode = MLAN_BSS_MODE_INFRA;
		priv->wdev->iftype = NL80211_IFTYPE_P2P_CLIENT;
		PRINTM(MINFO, "Setting interface type to P2P client\n");

		break;
#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */
	case NL80211_IFTYPE_AP:
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	case NL80211_IFTYPE_P2P_GO:
		if (priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT) {
			if (MLAN_STATUS_SUCCESS !=
			    woal_cfg80211_init_p2p_go(priv)) {
				ret = -EFAULT;
				goto done;
			}
			priv->phandle->is_go_timer_set = MTRUE;
			woal_mod_timer(&priv->phandle->go_timer,
				       MOAL_TIMER_10S);
		}
		if (type == NL80211_IFTYPE_P2P_GO)
			priv->wdev->iftype = NL80211_IFTYPE_P2P_GO;
#endif
#endif
#if defined(STA_SUPPORT) && defined(UAP_SUPPORT)
		if (priv->bss_type == MLAN_BSS_TYPE_STA) {
			if (priv->probereq_index !=
			    MLAN_CUSTOM_IE_AUTO_IDX_MASK)
				woal_cfg80211_mgmt_frame_ie(priv, NULL, 0, NULL,
							    0, NULL, 0, NULL, 0,
							    MGMT_MASK_PROBE_REQ);
			bss_role = MLAN_BSS_ROLE_UAP;
			woal_cfg80211_bss_role_cfg(priv, MLAN_ACT_SET,
						   &bss_role);
			PRINTM(MIOCTL, "set bss role for AP\n");
		}
#endif
		if (type == NL80211_IFTYPE_AP)
			priv->wdev->iftype = NL80211_IFTYPE_AP;
		PRINTM(MINFO, "Setting interface type to P2P GO\n");

		/* there is no need for P2P GO to set bss_mode */
		goto done;

		break;

	case NL80211_IFTYPE_UNSPECIFIED:
		bss->param.bss_mode = MLAN_BSS_MODE_AUTO;
		priv->wdev->iftype = NL80211_IFTYPE_STATION;
		PRINTM(MINFO, "Setting interface type to auto\n");
		break;
	default:
		ret = -EINVAL;
		break;
	}
	if (ret)
		goto done;

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}

done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 * @brief Request the driver to change the value of fragment
 * threshold or rts threshold or retry limit
 *
 * @param wiphy         A pointer to wiphy structure
 * @param changed       Change flags
 *
 * @return              0 -- success, otherwise fail
 */
int
woal_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	moal_private *priv = NULL;
	moal_handle *handle = (moal_handle *) woal_get_wiphy_priv(wiphy);
#ifdef UAP_CFG80211
#ifdef UAP_SUPPORT
	mlan_uap_bss_param sys_cfg;
#endif
#endif
	int frag_thr = wiphy->frag_threshold;
	int rts_thr = wiphy->frag_threshold;
	int retry = wiphy->retry_long;

	ENTER();

	priv = woal_get_priv(handle, MLAN_BSS_ROLE_ANY);
	if (rts_thr == MLAN_FRAG_RTS_DISABLED)
		rts_thr = MLAN_RTS_MAX_VALUE;
	if (frag_thr == MLAN_FRAG_RTS_DISABLED)
		frag_thr = MLAN_FRAG_MAX_VALUE;

#ifdef UAP_CFG80211
#ifdef UAP_SUPPORT
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
		/* Initialize the invalid values so that the correct values
		   below are downloaded to firmware */
		woal_set_sys_config_invalid_data(&sys_cfg);
		sys_cfg.frag_threshold = frag_thr;
		sys_cfg.rts_threshold = rts_thr;
		sys_cfg.retry_limit = retry;

		if ((changed & WIPHY_PARAM_RTS_THRESHOLD) ||
		    (changed & WIPHY_PARAM_FRAG_THRESHOLD) ||
		    (changed &
		     (WIPHY_PARAM_RETRY_LONG | WIPHY_PARAM_RETRY_SHORT))) {
			if (woal_set_get_sys_config
			    (priv, MLAN_ACT_SET, MOAL_IOCTL_WAIT, &sys_cfg))
				goto fail;
		}
	}
#endif
#endif

#ifdef STA_CFG80211
#ifdef STA_SUPPORT
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) {
		if (changed & WIPHY_PARAM_RTS_THRESHOLD) {
			if (woal_set_get_rts
			    (priv, MLAN_ACT_SET, MOAL_IOCTL_WAIT, &rts_thr))
				goto fail;
		}
		if (changed & WIPHY_PARAM_FRAG_THRESHOLD) {
			if (woal_set_get_frag
			    (priv, MLAN_ACT_SET, MOAL_IOCTL_WAIT, &frag_thr))
				goto fail;
		}
		if (changed &
		    (WIPHY_PARAM_RETRY_LONG | WIPHY_PARAM_RETRY_SHORT))
			if (woal_set_get_retry
			    (priv, MLAN_ACT_SET, MOAL_IOCTL_WAIT, &retry))
				goto fail;
	}
#endif
#endif

	LEAVE();
	return 0;

fail:
	PRINTM(MERROR, "Failed to change wiphy params %x\n", changed);
	LEAVE();
	return -EFAULT;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 36) || defined(COMPAT_WIRELESS)
/**
 * @brief Request the driver to add a key
 *
 * @param wiphy         A pointer to wiphy structure
 * @param netdev        A pointer to net_device structure
 * @param key_index     Key index
 * @param pairwise      Flag to indicate pairwise or group (for kernel > 2.6.36)
 * @param mac_addr      MAC address (NULL for group key)
 * @param params        A pointer to key_params structure
 *
 * @return              0 -- success, otherwise fail
 */
#else
/**
 * @brief Request the driver to add a key
 *
 * @param wiphy         A pointer to wiphy structure
 * @param netdev        A pointer to net_device structure
 * @param key_index     Key index
 * @param mac_addr      MAC address (NULL for group key)
 * @param params        A pointer to key_params structure
 *
 * @return              0 -- success, otherwise fail
 */
#endif
int
woal_cfg80211_add_key(struct wiphy *wiphy, struct net_device *netdev,
		      t_u8 key_index,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 36) || defined(COMPAT_WIRELESS)
		      bool pairwise,
#endif
		      const t_u8 * mac_addr, struct key_params *params)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(netdev);

	ENTER();

	if (woal_cfg80211_set_key(priv, 0, params->cipher, params->key,
				  params->key_len, params->seq, params->seq_len,
				  key_index, mac_addr, 0)) {
		PRINTM(MERROR, "Error adding the crypto keys\n");
		LEAVE();
		return -EFAULT;
	}

	PRINTM(MINFO, "Crypto keys added\n");

	LEAVE();
	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 36) || defined(COMPAT_WIRELESS)
/**
 * @brief Request the driver to delete a key
 *
 * @param wiphy         A pointer to wiphy structure
 * @param netdev        A pointer to net_device structure
 * @param key_index     Key index
 * @param pairwise      Flag to indicate pairwise or group (for kernel > 2.6.36)
 * @param mac_addr      MAC address (NULL for group key)
 *
 * @return              0 -- success, otherwise fail
 */
#else
/**
 * @brief Request the driver to delete a key
 *
 * @param wiphy         A pointer to wiphy structure
 * @param netdev        A pointer to net_device structure
 * @param key_index     Key index
 * @param mac_addr      MAC address (NULL for group key)
 *
 * @return              0 -- success, otherwise fail
 */
#endif
int
woal_cfg80211_del_key(struct wiphy *wiphy, struct net_device *netdev,
		      t_u8 key_index,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 36) || defined(COMPAT_WIRELESS)
		      bool pairwise,
#endif
		      const t_u8 * mac_addr)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(netdev);

	ENTER();

	if (woal_cfg80211_set_key(priv, 0, 0, NULL, 0, NULL, 0, key_index,
				  mac_addr, 1)) {
		PRINTM(MERROR, "Error deleting the crypto keys\n");
		LEAVE();
		return -EFAULT;
	}

	PRINTM(MINFO, "Crypto keys deleted\n");
	LEAVE();
	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 37) || defined(COMPAT_WIRELESS)
/**
 * @brief Request to enable WEP key to driver
 *
 * @param wiphy         A pointer to wiphy structure
 * @param netdev        A pointer to net_device structure
 * @param key_index     Key index
 * @param ucast         Unicast flag (for kernel > 2.6.37)
 * @param mcast         Multicast flag (for kernel > 2.6.37)
 *
 * @return              0 -- success, otherwise fail
 */
#else
/**
 * @brief Request to enable WEP key to driver
 *
 * @param wiphy         A pointer to wiphy structure
 * @param netdev        A pointer to net_device structure
 * @param key_index     Key index
 *
 * @return              0 -- success, otherwise fail
 */
#endif
int
woal_cfg80211_set_default_key(struct wiphy *wiphy,
			      struct net_device *netdev, t_u8 key_index
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 37) || defined(COMPAT_WIRELESS)
			      , bool ucast, bool mcast
#endif
	)
{
	int ret = 0;
	moal_private *priv = (moal_private *) woal_get_netdev_priv(netdev);
	mlan_bss_info bss_info;

	ENTER();

	woal_get_bss_info(priv, MOAL_IOCTL_WAIT, &bss_info);
	if (!bss_info.wep_status) {
		LEAVE();
		return ret;
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_cfg80211_set_wep_keys(priv, NULL, 0, key_index)) {
		ret = -EFAULT;
	}

	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34) || defined(COMPAT_WIRELESS)
/**
 * @brief Request the driver to change the channel
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param chan            A pointer to ieee80211_channel structure
 * @param channel_type    Channel type of nl80211_channel_type
 *
 * @return                0 -- success, otherwise fail
 */
#else
/**
 * @brief Request the driver to change the channel
 *
 * @param wiphy           A pointer to wiphy structure
 * @param chan            A pointer to ieee80211_channel structure
 * @param channel_type    Channel type of nl80211_channel_type
 *
 * @return                0 -- success, otherwise fail
 */
#endif
int
woal_cfg80211_set_channel(struct wiphy *wiphy,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34) || defined(COMPAT_WIRELESS)
			  struct net_device *dev,
#endif
			  struct ieee80211_channel *chan,
			  enum nl80211_channel_type channel_type)
{
	int ret = 0;
	moal_private *priv = NULL;

	ENTER();

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34) || defined(COMPAT_WIRELESS)
	if (dev)
		priv = woal_get_netdev_priv(dev);
#endif
#endif
	if (!priv) {
		moal_handle *handle =
			(moal_handle *) woal_get_wiphy_priv(wiphy);
		if (handle)
			priv = woal_get_priv(handle, MLAN_BSS_ROLE_ANY);
	}
	if (priv) {
#ifdef STA_CFG80211
#ifdef STA_SUPPORT
		if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) {
			if (priv->media_connected == MTRUE) {
				PRINTM(MERROR,
				       "This configuration is valid only when station "
				       "is not connected\n");
				LEAVE();
				return -EINVAL;
			}
			ret = woal_set_rf_channel(priv, chan, channel_type);
		}
#endif
#endif
		priv->channel =
			ieee80211_frequency_to_channel(chan->center_freq);
	}
	/* set monitor channel support */

	LEAVE();
	return ret;
}
#endif

/**
 * @brief Request the driver to set the bitrate
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param peer            A pointer to peer address
 * @param mask            A pointer to cfg80211_bitrate_mask structure
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_set_bitrate_mask(struct wiphy *wiphy,
			       struct net_device *dev,
			       const u8 * peer,
			       const struct cfg80211_bitrate_mask *mask)
{
	int ret = 0;
	mlan_status status = MLAN_STATUS_SUCCESS;
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	mlan_bss_info bss_info;
	enum ieee80211_band band;
	mlan_ioctl_req *req = NULL;
	mlan_ds_rate *rate = NULL;
	mlan_rate_cfg_t *rate_cfg = NULL;

	ENTER();

	if (priv->media_connected == MFALSE) {
		PRINTM(MERROR, "Can not set data rate in disconnected state\n");
		ret = -EINVAL;
		goto done;
	}

	status = woal_get_bss_info(priv, MOAL_IOCTL_WAIT, &bss_info);
	if (status)
		goto done;
	band = woal_band_cfg_to_ieee_band(bss_info.bss_band);

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_rate));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	rate = (mlan_ds_rate *) req->pbuf;
	rate_cfg = &rate->param.rate_cfg;
	rate->sub_command = MLAN_OID_RATE_CFG;
	req->req_id = MLAN_IOCTL_RATE;
	req->action = MLAN_ACT_SET;
	rate_cfg->rate_type = MLAN_RATE_BITMAP;

	/* Fill HR/DSSS rates. */
	if (band == IEEE80211_BAND_2GHZ)
		rate_cfg->bitmap_rates[0] = mask->control[band].legacy & 0x000f;

	/* Fill OFDM rates */
	if (band == IEEE80211_BAND_2GHZ)
		rate_cfg->bitmap_rates[1] =
			(mask->control[band].legacy & 0x0ff0) >> 4;
	else
		rate_cfg->bitmap_rates[1] = mask->control[band].legacy;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	/* Fill MCS rates */
	rate_cfg->bitmap_rates[2] = mask->control[band].mcs[0];
	if (priv->phandle->card_type == CARD_TYPE_SD8797)
		rate_cfg->bitmap_rates[2] |= mask->control[band].mcs[1] << 8;
#endif

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}

done:
	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38) || defined(COMPAT_WIRELESS)
/**
 * @brief Request the driver to set antenna configuration
 *
 * @param wiphy           A pointer to wiphy structure
 * @param tx_ant          Bitmaps of allowed antennas to use for TX
 * @param rx_ant          Bitmaps of allowed antennas to use for RX
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_set_antenna(struct wiphy *wiphy, u32 tx_ant, u32 rx_ant)
{
	moal_handle *handle = (moal_handle *) woal_get_wiphy_priv(wiphy);
	moal_private *priv = NULL;
	mlan_ds_radio_cfg *radio = NULL;
	mlan_ioctl_req *req = NULL;
	int ret = 0;

	ENTER();

	if (!handle) {
		PRINTM(MFATAL, "Unable to get handle\n");
		ret = -EINVAL;
		goto done;
	}
	priv = woal_get_priv(handle, MLAN_BSS_ROLE_ANY);
	if (!priv) {
		ret = -EINVAL;
		goto done;
	}

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_radio_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	radio = (mlan_ds_radio_cfg *) req->pbuf;
	radio->sub_command = MLAN_OID_ANT_CFG;
	req->req_id = MLAN_IOCTL_RADIO_CFG;
	req->action = MLAN_ACT_SET;
	radio->param.ant_cfg.tx_antenna = tx_ant;
	radio->param.ant_cfg.rx_antenna = rx_ant;

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}

done:
	if (req)
		kfree(req);
	/* Driver must return -EINVAL to cfg80211 */
	if (ret)
		ret = -EINVAL;
	LEAVE();
	return ret;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
/**
 * @brief register/unregister mgmt frame forwarding
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param frame_type      Bit mask for mgmt frame type
 * @param reg             Register or unregister
 *
 * @return                0 -- success, otherwise fail
 */
void
woal_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
				  struct net_device *dev, u16 frame_type,
				  bool reg)
#else
/**
 * @brief register/unregister mgmt frame forwarding
 *
 * @param wiphy           A pointer to wiphy structure
 * @param wdev            A pointer to wireless_dev structure
 * @param frame_type      Bit mask for mgmt frame type
 * @param reg             Register or unregister
 *
 * @return                0 -- success, otherwise fail
 */
void
woal_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
				  struct wireless_dev *wdev, u16 frame_type,
				  bool reg)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
	struct net_device *dev = wdev->netdev;
#endif
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	mlan_status status = MLAN_STATUS_SUCCESS;
	t_u32 mgmt_subtype_mask = 0x0;
	t_u32 last_mgmt_subtype_mask = priv->mgmt_subtype_mask;

	ENTER();
#ifdef UAP_SUPPORT
	if ((priv->bss_type == MLAN_BSS_TYPE_UAP) &&
	    (frame_type == IEEE80211_STYPE_PROBE_REQ)) {
		PRINTM(MIOCTL, "Skip register PROBE_REQ in UAP mode\n");
		LEAVE();
		return;
	}
#endif
	if (reg == MTRUE) {
		/* set mgmt_subtype_mask based on origin value */
		mgmt_subtype_mask =
			last_mgmt_subtype_mask | BIT(frame_type >> 4);
	} else {
		/* clear mgmt_subtype_mask */
		mgmt_subtype_mask =
			last_mgmt_subtype_mask & ~BIT(frame_type >> 4);
	}
	PRINTM(MIOCTL,
	       "%s: mgmt_subtype_mask=0x%x last_mgmt_subtype_mask=0x%x\n",
	       dev->name, mgmt_subtype_mask, last_mgmt_subtype_mask);
	if (mgmt_subtype_mask != last_mgmt_subtype_mask) {

		last_mgmt_subtype_mask = mgmt_subtype_mask;
		/* Notify driver that a mgmt frame type was registered. * Note
		   that this callback may not sleep, and cannot run *
		   concurrently with itself. */
		status = woal_reg_rx_mgmt_ind(priv, MLAN_ACT_SET,
					      &mgmt_subtype_mask, MOAL_NO_WAIT);
		priv->mgmt_subtype_mask = last_mgmt_subtype_mask;
	}

	LEAVE();
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0) || defined(COMPAT_WIRELESS)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
/**
 * @brief tx mgmt frame
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param wdev                  A pointer to wireless_dev structure
 * @param chan                  A pointer to ieee80211_channel structure
 * @param offchan               Off channel or not
 * @param wait                  Duration to wait
 * @param buf                   Frame buffer
 * @param len                   Frame length
 * @param no_cck                No CCK check
 * @param dont_wait_for_ack     Do not wait for ACK
 * @param cookie                A pointer to frame cookie
 *
 * @return                0 -- success, otherwise fail
 */
#else
/**
 * @brief tx mgmt frame
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param wdev                  A pointer to wireless_dev structure
 * @param chan                  A pointer to ieee80211_channel structure
 * @param offchan               Off channel or not
 * @param channel_type          Channel type
 * @param channel_type_valid    Is channel type valid or not
 * @param wait                  Duration to wait
 * @param buf                   Frame buffer
 * @param len                   Frame length
 * @param no_cck                No CCK check
 * @param dont_wait_for_ack     Do not wait for ACK
 * @param cookie                A pointer to frame cookie
 *
 * @return                0 -- success, otherwise fail
 */
#endif
#else
/**
 * @brief tx mgmt frame
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param dev                   A pointer to net_device structure
 * @param chan                  A pointer to ieee80211_channel structure
 * @param offchan               Off channel or not
 * @param channel_type          Channel type
 * @param channel_type_valid    Is channel type valid or not
 * @param wait                  Duration to wait
 * @param buf                   Frame buffer
 * @param len                   Frame length
 * @param no_cck                No CCK check
 * @param dont_wait_for_ack     Do not wait for ACK
 * @param cookie                A pointer to frame cookie
 *
 * @return                0 -- success, otherwise fail
 */
#endif
#else
/**
 * @brief tx mgmt frame
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param dev                   A pointer to net_device structure
 * @param chan                  A pointer to ieee80211_channel structure
 * @param offchan               Off channel or not
 * @param channel_type          Channel type
 * @param channel_type_valid    Is channel type valid or not
 * @param wait                  Duration to wait
 * @param buf                   Frame buffer
 * @param len                   Frame length
 * @param no_cck                No CCK check
 * @param cookie                A pointer to frame cookie
 *
 * @return                0 -- success, otherwise fail
 */
#endif
#else
/**
 * @brief tx mgmt frame
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param dev                   A pointer to net_device structure
 * @param chan                  A pointer to ieee80211_channel structure
 * @param offchan               Off channel or not
 * @param channel_type          Channel type
 * @param channel_type_valid    Is channel type valid or not
 * @param wait                  Duration to wait
 * @param buf                   Frame buffer
 * @param len                   Frame length
 * @param cookie                A pointer to frame cookie
 *
 * @return                0 -- success, otherwise fail
 */
#endif
int
woal_cfg80211_mgmt_tx(struct wiphy *wiphy,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
		      struct net_device *dev,
#else
		      struct wireless_dev *wdev,
#endif
		      struct ieee80211_channel *chan, bool offchan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
		      enum nl80211_channel_type channel_type,
		      bool channel_type_valid,
#endif
		      unsigned int wait, const u8 * buf, size_t len,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0) || defined(COMPAT_WIRELESS)
		      bool no_cck,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
		      bool dont_wait_for_ack,
#endif
		      u64 * cookie)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
	struct net_device *dev = wdev->netdev;
#endif
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	int ret = 0;
	pmlan_buffer pmbuf = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;
	t_u16 packet_len = 0;
	t_u8 addr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	t_u16 framectrl;
	t_u32 pkt_type;
	t_u32 tx_control;
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	t_u8 channel_status;
	t_u32 duration;
	moal_private *remain_priv = NULL;
#endif
#endif

	ENTER();

	if (buf == NULL || len == 0) {
		PRINTM(MERROR, "woal_cfg80211_mgmt_tx() corrupt data\n");
		ret = -EFAULT;
		goto done;
	}

	/* frame subtype == probe response, that means we are in listen phase,
	   so we should not call remain_on_channel_cfg because
	   remain_on_channl already handled it. frame subtype == action, that
	   means we are in PD/GO negotiation, so we should call
	   remain_on_channel_cfg in order to receive action frame from peer
	   device */
	framectrl = ((const struct ieee80211_mgmt *)buf)->frame_control;
	PRINTM(MIOCTL, "Mgmt TX %s => framectrl = 0x%x freq = %d\n", dev->name,
	       framectrl, chan->center_freq);
	if ((GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) &&
	    (framectrl == IEEE80211_STYPE_PROBE_RESP)) {
		PRINTM(MIOCTL, "Skip send probe_resp in GO/UAP mode\n");
		goto done;
	}
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	if ((priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT) &&
	    (framectrl == IEEE80211_STYPE_ACTION)) {
		if (priv->phandle->is_go_timer_set) {
			woal_cancel_timer(&priv->phandle->go_timer);
			priv->phandle->is_go_timer_set = MFALSE;
		}
#define MGMT_TX_DEFAULT_WAIT_TIME	   1000
		/** cancel previous remain on channel */
		if (priv->phandle->remain_on_channel) {
			remain_priv =
				priv->phandle->priv[priv->phandle->
						    remain_bss_index];
			if (!remain_priv) {
				PRINTM(MERROR,
				       "mgmt_tx:Wrong remain_bss_index=%d\n",
				       priv->phandle->remain_bss_index);
				ret = -EFAULT;
				goto done;
			}
			if (woal_cfg80211_remain_on_channel_cfg
			    (remain_priv, MOAL_IOCTL_WAIT, MTRUE,
			     &channel_status, NULL, 0, 0)) {
				PRINTM(MERROR,
				       "mgmt_tx:Fail to cancel remain on channel\n");
				ret = -EFAULT;
				goto done;
			}
			if (priv->phandle->cookie) {
				cfg80211_remain_on_channel_expired(
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
									  priv->
									  phandle->
									  channel_type,
#endif
									  GFP_ATOMIC);
				priv->phandle->cookie = 0;
			}
			priv->phandle->remain_on_channel = MFALSE;
		}
#ifdef STA_CFG80211
		/** cancel pending scan */
		woal_cancel_scan(priv, MOAL_IOCTL_WAIT);
#endif
		duration = wait;
		if (!wait)
			duration = MGMT_TX_DEFAULT_WAIT_TIME;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
		if (channel_type_valid)
			ret = woal_cfg80211_remain_on_channel_cfg(priv,
								  MOAL_IOCTL_WAIT,
								  MFALSE,
								  &channel_status,
								  chan,
								  channel_type,
								  duration);
		else
#endif
			ret = woal_cfg80211_remain_on_channel_cfg(priv,
								  MOAL_IOCTL_WAIT,
								  MFALSE,
								  &channel_status,
								  chan, 0,
								  duration);
		if (ret) {
			PRINTM(MERROR, "Fail to configure remain on channel\n");
			ret = -EFAULT;
			goto done;
		}
		priv->phandle->remain_on_channel = MTRUE;
		priv->phandle->remain_bss_index = priv->bss_index;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
		priv->phandle->channel_type = channel_type;
#endif
		memcpy(&priv->phandle->chan, chan,
		       sizeof(struct ieee80211_channel));
		PRINTM(MIOCTL, "%s: Mgmt Tx: Set remain channel=%d\n",
		       dev->name,
		       ieee80211_frequency_to_channel(chan->center_freq));
	}
#endif
#endif
#define MRVL_PKT_TYPE_MGMT_FRAME 0xE5
	/* pkt_type + tx_control */
#define HEADER_SIZE				8
	packet_len = (t_u16) len + MLAN_MAC_ADDR_LENGTH;
	pmbuf = woal_alloc_mlan_buffer(priv->phandle,
				       MLAN_MIN_DATA_HEADER_LEN + HEADER_SIZE +
				       packet_len + sizeof(packet_len));
	if (!pmbuf) {
		PRINTM(MERROR, "Fail to allocate mlan_buffer\n");
		ret = -ENOMEM;
		goto done;
	}
	*cookie = random32() | 1;
	pmbuf->data_offset = MLAN_MIN_DATA_HEADER_LEN;
	pkt_type = MRVL_PKT_TYPE_MGMT_FRAME;
	tx_control = 0;
	/* Add pkt_type and tx_control */
	memcpy(pmbuf->pbuf + pmbuf->data_offset, &pkt_type, sizeof(pkt_type));
	memcpy(pmbuf->pbuf + pmbuf->data_offset + sizeof(pkt_type), &tx_control,
	       sizeof(tx_control));
	/* frmctl + durationid + addr1 + addr2 + addr3 + seqctl */
#define PACKET_ADDR4_POS		(2 + 2 + 6 + 6 + 6 + 2)
	memcpy(pmbuf->pbuf + pmbuf->data_offset + HEADER_SIZE, &packet_len,
	       sizeof(packet_len));
	memcpy(pmbuf->pbuf + pmbuf->data_offset + HEADER_SIZE +
	       sizeof(packet_len), buf, PACKET_ADDR4_POS);
	memcpy(pmbuf->pbuf + pmbuf->data_offset + HEADER_SIZE +
	       sizeof(packet_len)
	       + PACKET_ADDR4_POS, addr, MLAN_MAC_ADDR_LENGTH);
	memcpy(pmbuf->pbuf + pmbuf->data_offset + HEADER_SIZE +
	       sizeof(packet_len)
	       + PACKET_ADDR4_POS + MLAN_MAC_ADDR_LENGTH,
	       buf + PACKET_ADDR4_POS, len - PACKET_ADDR4_POS);

	pmbuf->data_len = HEADER_SIZE + packet_len + sizeof(packet_len);
	pmbuf->buf_type = MLAN_BUF_TYPE_RAW_DATA;
	pmbuf->bss_index = priv->bss_index;

	status = mlan_send_packet(priv->phandle->pmlan_adapter, pmbuf);

	switch (status) {
	case MLAN_STATUS_PENDING:
		atomic_inc(&priv->phandle->tx_pending);
		queue_work(priv->phandle->workqueue, &priv->phandle->main_work);

#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
		/* Delay 30ms to guarantee the packet has been already tx'ed,
		   becuase if we call cfg80211_mgmt_tx_status() immediately,
		   then wpa_supplicant will call cancel_remain_on_channel(),
		   which may affect the mgmt frame tx. Meanwhile it is only
		   necessary for P2P action handshake to wait 30ms. */
		if ((priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT) &&
		    (framectrl == IEEE80211_STYPE_ACTION))
			woal_sched_timeout(30);
#endif
#endif

		/* Notify the mgmt tx status */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37) || defined(COMPAT_WIRELESS)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
		cfg80211_mgmt_tx_status(dev, *cookie, buf, len, true,
					GFP_ATOMIC);
#else
		cfg80211_mgmt_tx_status(priv->wdev, *cookie, buf, len, true,
					GFP_ATOMIC);
#endif
#endif
		break;
	case MLAN_STATUS_SUCCESS:
		woal_free_mlan_buffer(priv->phandle, pmbuf);
		break;
	case MLAN_STATUS_FAILURE:
	default:
		woal_free_mlan_buffer(priv->phandle, pmbuf);
		ret = -EFAULT;
		break;
	}

done:
	LEAVE();
	return ret;
}

/**
 * @brief Look up specific IE in a buf
 *
 * @param ie              Pointer to IEs
 * @param len             Total length of ie
 * @param id              Element id to lookup
 *
 * @return                Pointer of the specific IE -- success, NULL -- fail
 */
const t_u8 *
woal_parse_ie_tlv(const t_u8 * ie, int len, t_u8 id)
{
	int left_len = len;
	const t_u8 *pos = ie;
	int length;

	/* IE format: | u8 | id | | u8 | len | | var | data | */
	while (left_len >= 2) {
		length = *(pos + 1);
		if ((*pos == id) && (length + 2) <= left_len)
			return pos;
		pos += (length + 2);
		left_len -= (length + 2);
	}

	return NULL;
}

/**
 * @brief Add custom ie to mgmt frames.
 *
 * @param priv                  A pointer to moal private structure
 * @param beacon_ies_data       Beacon ie
 * @param beacon_index          The index for beacon when auto index
 * @param proberesp_ies_data    Probe resp ie
 * @param proberesp_index       The index for probe resp when auto index
 * @param assocresp_ies_data    Assoc resp ie
 * @param assocresp_index       The index for assoc resp when auto index
 * @param probereq_ies_data     Probe req ie
 * @param probereq_index        The index for probe req when auto index *
 *
 * @return              0 -- success, otherwise fail
 */
static int
woal_cfg80211_custom_ie(moal_private * priv,
			custom_ie * beacon_ies_data, t_u16 * beacon_index,
			custom_ie * proberesp_ies_data, t_u16 * proberesp_index,
			custom_ie * assocresp_ies_data, t_u16 * assocresp_index,
			custom_ie * probereq_ies_data, t_u16 * probereq_index)
{
	mlan_ioctl_req *ioctl_req = NULL;
	mlan_ds_misc_cfg *misc = NULL;
	mlan_ds_misc_custom_ie *custom_ie = NULL;
	t_u8 *pos = NULL;
	t_u16 len = 0;
	int ret = 0;

	ENTER();

	custom_ie = kmalloc(sizeof(mlan_ds_misc_custom_ie), GFP_KERNEL);
	if (!custom_ie) {
		ret = -ENOMEM;
		goto done;
	}

	memset(custom_ie, 0x00, sizeof(mlan_ds_misc_custom_ie));
	custom_ie->type = TLV_TYPE_MGMT_IE;

	pos = (t_u8 *) custom_ie->ie_data_list;
	if (beacon_ies_data) {
		len = sizeof(*beacon_ies_data) - MAX_IE_SIZE
			+ beacon_ies_data->ie_length;
		memcpy(pos, beacon_ies_data, len);
		pos += len;
		custom_ie->len += len;
	}

	if (proberesp_ies_data) {
		len = sizeof(*proberesp_ies_data) - MAX_IE_SIZE
			+ proberesp_ies_data->ie_length;
		memcpy(pos, proberesp_ies_data, len);
		pos += len;
		custom_ie->len += len;
	}

	if (assocresp_ies_data) {
		len = sizeof(*assocresp_ies_data) - MAX_IE_SIZE
			+ assocresp_ies_data->ie_length;
		memcpy(pos, assocresp_ies_data, len);
		custom_ie->len += len;
	}

	if (probereq_ies_data) {
		len = sizeof(*probereq_ies_data) - MAX_IE_SIZE
			+ probereq_ies_data->ie_length;
		memcpy(pos, probereq_ies_data, len);
		pos += len;
		custom_ie->len += len;
	}
	ioctl_req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (ioctl_req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	misc = (mlan_ds_misc_cfg *) ioctl_req->pbuf;
	misc->sub_command = MLAN_OID_MISC_CUSTOM_IE;
	ioctl_req->req_id = MLAN_IOCTL_MISC_CFG;
	ioctl_req->action = MLAN_ACT_SET;

	memcpy(&misc->param.cust_ie, custom_ie, sizeof(mlan_ds_misc_custom_ie));

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, ioctl_req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}

	/* get the assigned index */
	pos = (t_u8 *) (&misc->param.cust_ie.ie_data_list[0].ie_index);
	if (beacon_ies_data && beacon_ies_data->ie_length
	    && beacon_ies_data->ie_index == MLAN_CUSTOM_IE_AUTO_IDX_MASK) {
		/* save beacon ie index after auto-indexing */
		*beacon_index = misc->param.cust_ie.ie_data_list[0].ie_index;
		len = sizeof(*beacon_ies_data) - MAX_IE_SIZE
			+ beacon_ies_data->ie_length;
		pos += len;
	}

	if (proberesp_ies_data && proberesp_ies_data->ie_length
	    && proberesp_ies_data->ie_index == MLAN_CUSTOM_IE_AUTO_IDX_MASK) {
		/* save probe resp ie index after auto-indexing */
		*proberesp_index = *((t_u16 *) pos);
		len = sizeof(*proberesp_ies_data) - MAX_IE_SIZE
			+ proberesp_ies_data->ie_length;
		pos += len;
	}

	if (assocresp_ies_data && assocresp_ies_data->ie_length
	    && assocresp_ies_data->ie_index == MLAN_CUSTOM_IE_AUTO_IDX_MASK) {
		/* save assoc resp ie index after auto-indexing */
		*assocresp_index = *((t_u16 *) pos);
		len = sizeof(*assocresp_ies_data) - MAX_IE_SIZE
			+ assocresp_ies_data->ie_length;
		pos += len;
	}
	if (probereq_ies_data && probereq_ies_data->ie_length
	    && probereq_ies_data->ie_index == MLAN_CUSTOM_IE_AUTO_IDX_MASK) {
		/* save probe resp ie index after auto-indexing */
		*probereq_index = *((t_u16 *) pos);
		len = sizeof(*probereq_ies_data) - MAX_IE_SIZE
			+ probereq_ies_data->ie_length;
		pos += len;
	}

	if (ioctl_req->status_code == MLAN_ERROR_IOCTL_FAIL)
		ret = -EFAULT;

done:
	if (ioctl_req)
		kfree(ioctl_req);
	if (custom_ie)
		kfree(custom_ie);
	LEAVE();
	return ret;
}

/**
 * @brief Find first P2P ie
 *
 * @param ie              Pointer to IEs
 * @param len             Total length of ie
 * @param ie_out		  Pointer to out IE buf
 *
 * @return                out IE length
 */
static t_u16
woal_get_first_p2p_ie(const t_u8 * ie, int len, t_u8 * ie_out)
{
	int left_len = len;
	const t_u8 *pos = ie;
	int length;
	t_u8 id = 0;
	t_u16 out_len = 0;
	IEEEtypes_VendorSpecific_t *pVendorIe = NULL;
	const u8 p2p_oui[4] = { 0x50, 0x6f, 0x9a, 0x09 };

	while (left_len >= 2) {
		length = *(pos + 1);
		id = *pos;
		if ((length + 2) > left_len)
			break;
		if (id == VENDOR_SPECIFIC_221) {
			pVendorIe = (IEEEtypes_VendorSpecific_t *) pos;
			if (!memcmp
			    (pVendorIe->vend_hdr.oui, p2p_oui,
			     sizeof(pVendorIe->vend_hdr.oui)) &&
			    pVendorIe->vend_hdr.oui_type == p2p_oui[3]) {
				memcpy(ie_out + out_len, pos, length + 2);
				out_len += length + 2;
				break;
			}
		}
		pos += (length + 2);
		left_len -= (length + 2);
	}
	return out_len;
}

/**
 * @brief Filter specific IE in ie buf
 *
 * @param ie              Pointer to IEs
 * @param len             Total length of ie
 * @param ie_out		  Pointer to out IE buf
 * @param wps_flag	      flag for wps/p2p
 *
 * @return                out IE length
 */
static t_u16
woal_filter_beacon_ies(const t_u8 * ie, int len, t_u8 * ie_out, t_u16 wps_flag)
{
	int left_len = len;
	const t_u8 *pos = ie;
	int length;
	t_u8 id = 0;
	t_u16 out_len = 0;
	IEEEtypes_VendorSpecific_t *pVendorIe = NULL;
	const u8 wps_oui[4] = { 0x00, 0x50, 0xf2, 0x04 };
	const u8 p2p_oui[4] = { 0x50, 0x6f, 0x9a, 0x09 };
	const u8 wfd_oui[4] = { 0x50, 0x6f, 0x9a, 0x0a };
	const t_u8 wmm_oui[4] = { 0x00, 0x50, 0xf2, 0x02 };
	t_u8 find_p2p_ie = MFALSE;

	/* ERP_INFO/EXTENDED_SUPPORT_RATES/HT_CAPABILITY/HT_OPERATION/WMM and
	   WPS/P2P/WFD IE will be fileter out */
	while (left_len >= 2) {
		length = *(pos + 1);
		id = *pos;
		if ((length + 2) > left_len)
			break;
		switch (id) {
		case EXTENDED_SUPPORTED_RATES:
		case WLAN_EID_ERP_INFO:
		case HT_CAPABILITY:
		case HT_OPERATION:
			break;
		case VENDOR_SPECIFIC_221:
			/* filter out wmm ie */
			pVendorIe = (IEEEtypes_VendorSpecific_t *) pos;
			if (!memcmp
			    (pVendorIe->vend_hdr.oui, wmm_oui,
			     sizeof(pVendorIe->vend_hdr.oui)) &&
			    pVendorIe->vend_hdr.oui_type == wmm_oui[3])
				break;
			/* filter out wps ie */
			if ((!memcmp
			     (pVendorIe->vend_hdr.oui, wps_oui,
			      sizeof(pVendorIe->vend_hdr.oui)) &&
			     pVendorIe->vend_hdr.oui_type == wps_oui[3]) &&
			    (wps_flag & IE_MASK_WPS))
				break;
			/* filter out first p2p ie */
			if ((!memcmp
			     (pVendorIe->vend_hdr.oui, p2p_oui,
			      sizeof(pVendorIe->vend_hdr.oui)) &&
			     pVendorIe->vend_hdr.oui_type == p2p_oui[3])) {
				if (!find_p2p_ie && (wps_flag & IE_MASK_P2P)) {
					find_p2p_ie = MTRUE;
					break;
				}
			}
			/* filter out wfd ie */
			if ((!memcmp
			     (pVendorIe->vend_hdr.oui, wfd_oui,
			      sizeof(pVendorIe->vend_hdr.oui)) &&
			     pVendorIe->vend_hdr.oui_type == wfd_oui[3]) &&
			    (wps_flag & IE_MASK_WFD))
				break;
			memcpy(ie_out + out_len, pos, length + 2);
			out_len += length + 2;
			break;
		default:
			memcpy(ie_out + out_len, pos, length + 2);
			out_len += length + 2;
			break;
		}
		pos += (length + 2);
		left_len -= (length + 2);
	}
	return out_len;
}

#ifdef WIFI_DIRECT_SUPPORT
/**
 * @brief Check if selected_registrar_on in wps_ie
 *
 * @param ie              Pointer to IEs
 * @param len             Total length of ie
 *
 * @return                MTRUE/MFALSE
 */
t_u8
is_selected_registrar_on(const t_u8 * ie, int len)
{
#define WPS_IE_FIX_LEN		6
#define TLV_ID_SELECTED_REGISTRAR 0x1041
	int left_len = len - WPS_IE_FIX_LEN;
	TLV_Generic_t *tlv = (TLV_Generic_t *) (ie + WPS_IE_FIX_LEN);
	u16 tlv_type, tlv_len;
	u8 *pos = NULL;
	while (left_len > sizeof(TLV_Generic_t)) {
		tlv_type = ntohs(tlv->type);
		tlv_len = ntohs(tlv->len);
		if (tlv_type == TLV_ID_SELECTED_REGISTRAR) {
			PRINTM(MIOCTL, "Selected Registrar found !");
			pos = (u8 *) tlv + sizeof(TLV_Generic_t);
			if (*pos == 1)
				return MTRUE;
			else
				return MFALSE;
		}
		tlv = (TLV_Generic_t *) ((u8 *) tlv + tlv_len +
					 sizeof(TLV_Generic_t));
		left_len -= tlv_len + sizeof(TLV_Generic_t);
	}
	return MFALSE;
}

/**
 * @brief Check if selected_registrar_on in ies
 *
 * @param ie              Pointer to IEs
 * @param len             Total length of ie
 *
 *
 * @return                MTRUE/MFALSE
 */
static t_u16
woal_is_selected_registrar_on(const t_u8 * ie, int len)
{
	int left_len = len;
	const t_u8 *pos = ie;
	int length;
	t_u8 id = 0;
	IEEEtypes_VendorSpecific_t *pVendorIe = NULL;
	const u8 wps_oui[4] = { 0x00, 0x50, 0xf2, 0x04 };

	while (left_len >= 2) {
		length = *(pos + 1);
		id = *pos;
		if ((length + 2) > left_len)
			break;
		switch (id) {
		case VENDOR_SPECIFIC_221:
			pVendorIe = (IEEEtypes_VendorSpecific_t *) pos;
			if (!memcmp
			    (pVendorIe->vend_hdr.oui, wps_oui,
			     sizeof(pVendorIe->vend_hdr.oui)) &&
			    pVendorIe->vend_hdr.oui_type == wps_oui[3]) {
				PRINTM(MIOCTL, "Find WPS ie\n");
				return is_selected_registrar_on(pos,
								length + 2);
			}
			break;
		default:
			break;
		}
		pos += (length + 2);
		left_len -= (length + 2);
	}
	return MFALSE;
}
#endif

/**
 * @brief config AP or GO for mgmt frame ies.
 *
 * @param priv                  A pointer to moal private structure
 * @param beacon_ies            A pointer to beacon ies
 * @param beacon_ies_len        Beacon ies length
 * @param proberesp_ies         A pointer to probe resp ies
 * @param proberesp_ies_len     Probe resp ies length
 * @param assocresp_ies         A pointer to probe resp ies
 * @param assocresp_ies_len     Assoc resp ies length
 * @param probereq_ies          A pointer to probe req ies
 * @param probereq_ies_len      Probe req ies length *
 * @param mask					Mgmt frame mask
 *
 * @return                      0 -- success, otherwise fail
 */
int
woal_cfg80211_mgmt_frame_ie(moal_private * priv,
			    const t_u8 * beacon_ies, size_t beacon_ies_len,
			    const t_u8 * proberesp_ies,
			    size_t proberesp_ies_len,
			    const t_u8 * assocresp_ies,
			    size_t assocresp_ies_len, const t_u8 * probereq_ies,
			    size_t probereq_ies_len, t_u16 mask)
{
	int ret = 0;
	t_u8 *pos = NULL;
	custom_ie *beacon_ies_data = NULL;
	custom_ie *proberesp_ies_data = NULL;
	custom_ie *assocresp_ies_data = NULL;
	custom_ie *probereq_ies_data = NULL;

	/* static variables for mgmt frame ie auto-indexing */
	t_u16 beacon_index = priv->beacon_index;
	t_u16 proberesp_index = priv->proberesp_index;
	t_u16 assocresp_index = priv->assocresp_index;
	t_u16 probereq_index = priv->probereq_index;
	t_u16 beacon_wps_index = priv->beacon_wps_index;
	t_u16 proberesp_p2p_index = priv->proberesp_p2p_index;

	ENTER();

	if (mask & MGMT_MASK_BEACON_WPS_P2P) {
		beacon_ies_data = kmalloc(sizeof(custom_ie), GFP_KERNEL);
		if (!beacon_ies_data) {
			ret = -ENOMEM;
			goto done;
		}
		memset(beacon_ies_data, 0x00, sizeof(custom_ie));
		if (beacon_ies && beacon_ies_len) {
#ifdef WIFI_DIRECT_SUPPORT
			if (woal_is_selected_registrar_on
			    (beacon_ies, beacon_ies_len)) {
				PRINTM(MIOCTL, "selected_registrar is on\n");
				priv->phandle->is_go_timer_set = MTRUE;
				woal_mod_timer(&priv->phandle->go_timer,
					       MOAL_TIMER_10S);
			} else
				PRINTM(MIOCTL, "selected_registrar is off\n");
#endif
			beacon_ies_data->ie_index = beacon_wps_index;
			beacon_ies_data->mgmt_subtype_mask = MGMT_MASK_BEACON;
			beacon_ies_data->ie_length = beacon_ies_len;
			pos = beacon_ies_data->ie_buffer;
			memcpy(pos, beacon_ies, beacon_ies_len);
		} else {
			/* clear the beacon wps ies */
			if (beacon_wps_index > MAX_MGMT_IE_INDEX) {
				PRINTM(MERROR,
				       "Invalid beacon wps index for mgmt frame ie.\n");
				goto done;
			}

			beacon_ies_data->ie_index = beacon_wps_index;
			beacon_ies_data->mgmt_subtype_mask =
				MLAN_CUSTOM_IE_DELETE_MASK;
			beacon_ies_data->ie_length = 0;
			beacon_wps_index = MLAN_CUSTOM_IE_AUTO_IDX_MASK;
		}
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_custom_ie(priv, beacon_ies_data,
					    &beacon_wps_index,
					    proberesp_ies_data,
					    &proberesp_index,
					    assocresp_ies_data,
					    &assocresp_index, probereq_ies_data,
					    &probereq_index)) {
			PRINTM(MERROR, "Fail to set beacon wps IE\n");
			ret = -EFAULT;
		}
		priv->beacon_wps_index = beacon_wps_index;
		PRINTM(MIOCTL, "beacon_wps_index=0x%x\n", beacon_wps_index);
		goto done;
	}

	if (mask & MGMT_MASK_BEACON) {
		beacon_ies_data = kmalloc(sizeof(custom_ie), GFP_KERNEL);
		if (!beacon_ies_data) {
			ret = -ENOMEM;
			goto done;
		}
	}

	if (mask & MGMT_MASK_PROBE_RESP) {
		/** set or clear proberesp ie */
		if (proberesp_ies_len ||
		    (!proberesp_ies_len && !beacon_ies_len)) {
			proberesp_ies_data =
				kmalloc(sizeof(custom_ie), GFP_KERNEL);
			if (!proberesp_ies_data) {
				ret = -ENOMEM;
				goto done;
			}
		}
	}

	if (mask & MGMT_MASK_ASSOC_RESP) {
		/** set or clear assocresp ie */
		if (assocresp_ies_len ||
		    (!assocresp_ies_len && !beacon_ies_len)) {
			assocresp_ies_data =
				kmalloc(sizeof(custom_ie), GFP_KERNEL);
			if (!assocresp_ies_data) {
				ret = -ENOMEM;
				goto done;
			}
		}
	}
	if (mask & MGMT_MASK_PROBE_REQ) {
		probereq_ies_data = kmalloc(sizeof(custom_ie), GFP_KERNEL);
		if (!probereq_ies_data) {
			ret = -ENOMEM;
			goto done;
		}
	}

	if (beacon_ies_data) {
		memset(beacon_ies_data, 0x00, sizeof(custom_ie));
		if (beacon_ies && beacon_ies_len) {
			/* set the beacon ies */
			beacon_ies_data->ie_index = beacon_index;
			beacon_ies_data->mgmt_subtype_mask = MGMT_MASK_BEACON |
				MGMT_MASK_ASSOC_RESP | MGMT_MASK_PROBE_RESP;
			beacon_ies_data->ie_length =
				woal_filter_beacon_ies(beacon_ies,
						       beacon_ies_len,
						       beacon_ies_data->
						       ie_buffer,
						       IE_MASK_WPS | IE_MASK_WFD
						       | IE_MASK_P2P);
		} else {
			/* clear the beacon ies */
			if (beacon_index > MAX_MGMT_IE_INDEX) {
				PRINTM(MINFO,
				       "Invalid beacon index for mgmt frame ie.\n");
				goto done;
			}

			beacon_ies_data->ie_index = beacon_index;
			beacon_ies_data->mgmt_subtype_mask =
				MLAN_CUSTOM_IE_DELETE_MASK;
			beacon_ies_data->ie_length = 0;
			beacon_index = MLAN_CUSTOM_IE_AUTO_IDX_MASK;
		}
	}

	if (proberesp_ies_data) {
		memset(proberesp_ies_data, 0x00, sizeof(custom_ie));
		if (proberesp_ies && proberesp_ies_len) {
			/* set the probe response p2p ies */
			proberesp_ies_data->ie_index = proberesp_p2p_index;
			proberesp_ies_data->mgmt_subtype_mask =
				MGMT_MASK_PROBE_RESP;
			proberesp_ies_data->ie_length =
				woal_get_first_p2p_ie(proberesp_ies,
						      proberesp_ies_len,
						      proberesp_ies_data->
						      ie_buffer);
		} else {
			/* clear the probe response p2p ies */
			if (proberesp_p2p_index > MAX_MGMT_IE_INDEX) {
				PRINTM(MERROR,
				       "Invalid proberesp_p2p_index for mgmt frame ie.\n");
				goto done;
			}
			proberesp_ies_data->ie_index = proberesp_p2p_index;
			proberesp_ies_data->mgmt_subtype_mask =
				MLAN_CUSTOM_IE_DELETE_MASK;
			proberesp_ies_data->ie_length = 0;
			proberesp_p2p_index = MLAN_CUSTOM_IE_AUTO_IDX_MASK;
		}
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_custom_ie(priv, NULL, &beacon_index,
					    proberesp_ies_data,
					    &proberesp_p2p_index, NULL,
					    &assocresp_index, NULL,
					    &probereq_index)) {
			PRINTM(MERROR, "Fail to set proberesp p2p IE\n");
			ret = -EFAULT;
			goto done;
		}
		priv->proberesp_p2p_index = proberesp_p2p_index;
		PRINTM(MIOCTL, "proberesp_p2p=0x%x\n", proberesp_p2p_index);
		memset(proberesp_ies_data, 0x00, sizeof(custom_ie));
		if (proberesp_ies && proberesp_ies_len) {
			/* set the probe response ies */
			proberesp_ies_data->ie_index = proberesp_index;
			proberesp_ies_data->mgmt_subtype_mask =
				MGMT_MASK_PROBE_RESP;
			if (MLAN_CUSTOM_IE_AUTO_IDX_MASK == proberesp_index)
				proberesp_ies_data->mgmt_subtype_mask |=
					MLAN_CUSTOM_IE_NEW_MASK;
			proberesp_ies_data->ie_length =
				woal_filter_beacon_ies(proberesp_ies,
						       proberesp_ies_len,
						       proberesp_ies_data->
						       ie_buffer, IE_MASK_P2P);
		} else {
			/* clear the probe response ies */
			if (proberesp_index > MAX_MGMT_IE_INDEX) {
				PRINTM(MERROR,
				       "Invalid probe resp index for mgmt frame ie.\n");
				goto done;
			}
			proberesp_ies_data->ie_index = proberesp_index;
			proberesp_ies_data->mgmt_subtype_mask =
				MLAN_CUSTOM_IE_DELETE_MASK;
			proberesp_ies_data->ie_length = 0;
			proberesp_index = MLAN_CUSTOM_IE_AUTO_IDX_MASK;
		}
	}
	if (assocresp_ies_data) {
		memset(assocresp_ies_data, 0x00, sizeof(custom_ie));
		if (assocresp_ies && assocresp_ies_len) {
			/* set the assoc response ies */
			assocresp_ies_data->ie_index = assocresp_index;
			assocresp_ies_data->mgmt_subtype_mask =
				MGMT_MASK_ASSOC_RESP;
			assocresp_ies_data->ie_length = assocresp_ies_len;
			pos = assocresp_ies_data->ie_buffer;
			memcpy(pos, assocresp_ies, assocresp_ies_len);
		} else {
			/* clear the assoc response ies */
			if (assocresp_index > MAX_MGMT_IE_INDEX) {
				PRINTM(MERROR,
				       "Invalid assoc resp index for mgmt frame ie.\n");
				goto done;
			}

			assocresp_ies_data->ie_index = assocresp_index;
			assocresp_ies_data->mgmt_subtype_mask =
				MLAN_CUSTOM_IE_DELETE_MASK;
			assocresp_ies_data->ie_length = 0;
			assocresp_index = MLAN_CUSTOM_IE_AUTO_IDX_MASK;
		}
	}

	if (probereq_ies_data) {
		memset(probereq_ies_data, 0x00, sizeof(custom_ie));
		if (probereq_ies && probereq_ies_len) {
			/* set the probe req ies */
			probereq_ies_data->ie_index = probereq_index;
			probereq_ies_data->mgmt_subtype_mask =
				MGMT_MASK_PROBE_REQ;
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
			if (priv->bss_type != MLAN_BSS_TYPE_WIFIDIRECT) {
				/* filter out P2P/WFD ie */
				probereq_ies_data->ie_length =
					woal_filter_beacon_ies(probereq_ies,
							       probereq_ies_len,
							       probereq_ies_data->
							       ie_buffer,
							       IE_MASK_P2P |
							       IE_MASK_WFD);
			} else {
#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */
				probereq_ies_data->ie_length = probereq_ies_len;
				pos = probereq_ies_data->ie_buffer;
				memcpy(pos, probereq_ies, probereq_ies_len);
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
			}
#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */
		} else {
			/* clear the probe req ies */
			if (probereq_index > MAX_MGMT_IE_INDEX) {
				PRINTM(MERROR,
				       "Invalid probe req index for mgmt frame ie.\n");
				goto done;
			}
			probereq_ies_data->ie_index = probereq_index;
			probereq_ies_data->mgmt_subtype_mask =
				MLAN_CUSTOM_IE_DELETE_MASK;
			probereq_ies_data->ie_length = 0;
			probereq_index = MLAN_CUSTOM_IE_AUTO_IDX_MASK;
		}
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_cfg80211_custom_ie(priv, beacon_ies_data, &beacon_index,
				    proberesp_ies_data, &proberesp_index,
				    assocresp_ies_data, &assocresp_index,
				    probereq_ies_data, &probereq_index)) {
		PRINTM(MERROR,
		       "Fail to set beacon proberesp assoc probereq IES\n");
		ret = -EFAULT;
		goto done;
	}
	if (beacon_ies_data)
		priv->beacon_index = beacon_index;
	if (assocresp_ies_data)
		priv->assocresp_index = assocresp_index;
	if (proberesp_ies_data)
		priv->proberesp_index = proberesp_index;
	if (probereq_ies_data)
		priv->probereq_index = probereq_index;
	PRINTM(MIOCTL, "beacon=%x assocresp=%x proberesp=%x probereq=%x\n",
	       beacon_index, assocresp_index, proberesp_index, probereq_index);
done:
	if (beacon_ies_data)
		kfree(beacon_ies_data);
	if (proberesp_ies_data)
		kfree(proberesp_ies_data);
	if (assocresp_ies_data)
		kfree(assocresp_ies_data);
	if (probereq_ies_data)
		kfree(probereq_ies_data);

	LEAVE();

	return ret;
}

/**
 *  @brief Sets up the CFG802.11 specific HT capability fields
 *  with default values
 *
 *  @param ht_info      A pointer to ieee80211_sta_ht_cap structure
 *  @param dev_cap      Device capability informations
 *  @param mcs_set      Device MCS sets
 *
 *  @return             N/A
 */
void
woal_cfg80211_setup_ht_cap(struct ieee80211_sta_ht_cap *ht_info,
			   t_u32 dev_cap, t_u8 * mcs_set)
{
	ENTER();

	ht_info->ht_supported = true;
	ht_info->ampdu_factor = 0x3;
	ht_info->ampdu_density = 0;

	memset(&ht_info->mcs, 0, sizeof(ht_info->mcs));
	ht_info->cap = 0;
	if (mcs_set)
		memcpy(ht_info->mcs.rx_mask, mcs_set,
		       sizeof(ht_info->mcs.rx_mask));
	if (dev_cap & MBIT(8))	/* 40Mhz intolarance enabled */
		ht_info->cap |= IEEE80211_HT_CAP_40MHZ_INTOLERANT;
	if (dev_cap & MBIT(17))	/* Channel width 20/40Mhz support */
		ht_info->cap |= IEEE80211_HT_CAP_SUP_WIDTH_20_40;
	if ((dev_cap >> 20) & 0x03)	/* Delayed ACK supported */
		ht_info->cap |= IEEE80211_HT_CAP_DELAY_BA;
	if (dev_cap & MBIT(22))	/* Rx LDPC supported */
		ht_info->cap |= IEEE80211_HT_CAP_LDPC_CODING;
	if (dev_cap & MBIT(23))	/* Short GI @ 20Mhz supported */
		ht_info->cap |= IEEE80211_HT_CAP_SGI_20;
	if (dev_cap & MBIT(24))	/* Short GI @ 40Mhz supported */
		ht_info->cap |= IEEE80211_HT_CAP_SGI_40;
	if (dev_cap & MBIT(25))	/* Tx STBC supported */
		ht_info->cap |= IEEE80211_HT_CAP_TX_STBC;
	if (dev_cap & MBIT(26))	/* Rx STBC supported */
		ht_info->cap |= IEEE80211_HT_CAP_RX_STBC;
	if (dev_cap & MBIT(27))	/* MIMO PS supported */
		ht_info->cap |= IEEE80211_HT_CAP_SM_PS;
	if (dev_cap & MBIT(29))	/* Green field supported */
		ht_info->cap |= IEEE80211_HT_CAP_GRN_FLD;
	if (dev_cap & MBIT(31))	/* MAX AMSDU supported */
		ht_info->cap |= IEEE80211_HT_CAP_MAX_AMSDU;
	ht_info->mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;

	LEAVE();
}
