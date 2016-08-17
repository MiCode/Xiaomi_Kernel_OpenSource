/** @file moal_sta_cfg80211.c
  *
  * @brief This file contains the functions for STA CFG80211.
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
#include "moal_sta_cfg80211.h"
#include "moal_eth_ioctl.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
static void
#else
static int
#endif

woal_cfg80211_reg_notifier(struct wiphy *wiphy,
			   struct regulatory_request *request);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
static int woal_cfg80211_scan(struct wiphy *wiphy,
			      struct cfg80211_scan_request *request);
#else
static int woal_cfg80211_scan(struct wiphy *wiphy, struct net_device *dev,
			      struct cfg80211_scan_request *request);
#endif

static int woal_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev,
				 struct cfg80211_connect_params *sme);

static int woal_cfg80211_disconnect(struct wiphy *wiphy,
				    struct net_device *dev, t_u16 reason_code);

static int woal_cfg80211_get_station(struct wiphy *wiphy,
				     struct net_device *dev,
				     t_u8 * mac, struct station_info *sinfo);

static int woal_cfg80211_dump_station(struct wiphy *wiphy,
				      struct net_device *dev, int idx,
				      t_u8 * mac, struct station_info *sinfo);

static int woal_cfg80211_dump_survey(struct wiphy *wiphy,
				     struct net_device *dev, int idx,
				     struct survey_info *survey);

static int woal_cfg80211_set_power_mgmt(struct wiphy *wiphy,
					struct net_device *dev, bool enabled,
					int timeout);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35) || defined(COMPAT_WIRELESS)
static int woal_cfg80211_set_cqm_rssi_config(struct wiphy *wiphy,
					     struct net_device *dev,
					     s32 rssi_thold, u32 rssi_hyst);
#endif

static int woal_cfg80211_set_tx_power(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
				      struct wireless_dev *wdev,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36) && !defined(COMPAT_WIRELESS)
				      enum tx_power_setting type,
#else
				      enum nl80211_tx_power_setting type,
#endif
				      int dbm);

static int woal_cfg80211_join_ibss(struct wiphy *wiphy,
				   struct net_device *dev,
				   struct cfg80211_ibss_params *params);

static int woal_cfg80211_leave_ibss(struct wiphy *wiphy,
				    struct net_device *dev);

#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
static int woal_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
					     struct wireless_dev *wdev,
#else
					     struct net_device *dev,
#endif
					     u64 cookie);

static int woal_cfg80211_remain_on_channel(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
					   struct wireless_dev *wdev,
#else
					   struct net_device *dev,
#endif
					   struct ieee80211_channel *chan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
					   enum nl80211_channel_type
					   channel_type,
#endif
					   unsigned int duration, u64 * cookie);

static int woal_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
						  struct wireless_dev *wdev,
#else
						  struct net_device *dev,
#endif
						  u64 cookie);
#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) || defined(COMPAT_WIRELESS)
int woal_cfg80211_sched_scan_start(struct wiphy *wiphy,
				   struct net_device *dev,
				   struct cfg80211_sched_scan_request *request);
int woal_cfg80211_sched_scan_stop(struct wiphy *wiphy, struct net_device *dev);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)|| defined(COMPAT_WIRELESS)
int woal_cfg80211_resume(struct wiphy *wiphy);
int woal_cfg80211_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow);
#endif

/** cfg80211 operations */
static struct cfg80211_ops woal_cfg80211_ops = {
	.change_virtual_intf = woal_cfg80211_change_virtual_intf,
	.scan = woal_cfg80211_scan,
	.connect = woal_cfg80211_connect,
	.disconnect = woal_cfg80211_disconnect,
	.get_station = woal_cfg80211_get_station,
	.dump_station = woal_cfg80211_dump_station,
	.dump_survey = woal_cfg80211_dump_survey,
	.set_wiphy_params = woal_cfg80211_set_wiphy_params,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
	.set_channel = woal_cfg80211_set_channel,
#endif
	.join_ibss = woal_cfg80211_join_ibss,
	.leave_ibss = woal_cfg80211_leave_ibss,
	.add_key = woal_cfg80211_add_key,
	.del_key = woal_cfg80211_del_key,
	.set_default_key = woal_cfg80211_set_default_key,
	.set_power_mgmt = woal_cfg80211_set_power_mgmt,
	.set_tx_power = woal_cfg80211_set_tx_power,
	.set_bitrate_mask = woal_cfg80211_set_bitrate_mask,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) || defined(COMPAT_WIRELESS)
	.sched_scan_start = woal_cfg80211_sched_scan_start,
	.sched_scan_stop = woal_cfg80211_sched_scan_stop,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)|| defined(COMPAT_WIRELESS)
	.suspend = woal_cfg80211_suspend,
	.resume = woal_cfg80211_resume,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) || defined(COMPAT_WIRELESS)
	.set_antenna = woal_cfg80211_set_antenna,
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35) || defined(COMPAT_WIRELESS)
	.set_cqm_rssi_config = woal_cfg80211_set_cqm_rssi_config,
#endif
#ifdef UAP_CFG80211
	.add_virtual_intf = woal_cfg80211_add_virtual_intf,
	.del_virtual_intf = woal_cfg80211_del_virtual_intf,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
	.start_ap = woal_cfg80211_add_beacon,
	.change_beacon = woal_cfg80211_set_beacon,
	.stop_ap = woal_cfg80211_del_beacon,
#else
	.add_beacon = woal_cfg80211_add_beacon,
	.set_beacon = woal_cfg80211_set_beacon,
	.del_beacon = woal_cfg80211_del_beacon,
#endif
	.del_station = woal_cfg80211_del_station,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37) || defined(COMPAT_WIRELESS)
	.mgmt_frame_register = woal_cfg80211_mgmt_frame_register,
	.mgmt_tx = woal_cfg80211_mgmt_tx,
#endif
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	.mgmt_tx_cancel_wait = woal_cfg80211_mgmt_tx_cancel_wait,
	.remain_on_channel = woal_cfg80211_remain_on_channel,
	.cancel_remain_on_channel = woal_cfg80211_cancel_remain_on_channel,
#endif
#endif
#endif
};

/** Region code mapping */
typedef struct _region_code_t
{
    /** Region */
	t_u8 region[COUNTRY_CODE_LEN];
} region_code_t;

/********************************************************
				Local Variables
********************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37) || defined(COMPAT_WIRELESS)
static const struct ieee80211_txrx_stypes
	ieee80211_mgmt_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_ADHOC] = {
				  .tx = 0x0000,
				  .rx = 0x0000,
				  },
	[NL80211_IFTYPE_STATION] = {
				    .tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				    BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
				    .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				    BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
				    },
	[NL80211_IFTYPE_AP] = {
			       .tx = 0xffff,
			       .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
			       BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
			       BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
			       BIT(IEEE80211_STYPE_DISASSOC >> 4) |
			       BIT(IEEE80211_STYPE_AUTH >> 4) |
			       BIT(IEEE80211_STYPE_DEAUTH >> 4) |
			       BIT(IEEE80211_STYPE_ACTION >> 4),
			       },
	[NL80211_IFTYPE_AP_VLAN] = {
				    .tx = 0x0000,
				    .rx = 0x0000,
				    },
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	[NL80211_IFTYPE_P2P_CLIENT] = {
				       .tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				       BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
				       .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				       BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
				       },
	[NL80211_IFTYPE_P2P_GO] = {
				   .tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				   BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
				   .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
				   BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
				   BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
				   BIT(IEEE80211_STYPE_DISASSOC >> 4) |
				   BIT(IEEE80211_STYPE_AUTH >> 4) |
				   BIT(IEEE80211_STYPE_DEAUTH >> 4) |
				   BIT(IEEE80211_STYPE_ACTION >> 4),
				   },
#endif
#endif
	[NL80211_IFTYPE_MESH_POINT] = {
				       .tx = 0x0000,
				       .rx = 0x0000,
				       },

};
#endif

extern moal_handle *m_handle[];
extern int hw_test;
/** Region alpha2 string */
char *reg_alpha2 = NULL;

/********************************************************
				Global Variables
********************************************************/

/********************************************************
				Local Functions
********************************************************/

/**
 *  @brief This function check cfg80211 special region code.
 *
 *  @param region_string         Region string
 *
 *  @return     MTRUE/MFALSE
 */
t_u8
is_cfg80211_special_region_code(char *region_string)
{
	t_u8 i;
	t_u8 size = 0;
	region_code_t cfg80211_special_region_code[] =
		{ {"00 "}, {"99 "}, {"98 "}, {"97 "} };

	size = sizeof(cfg80211_special_region_code) / sizeof(region_code_t);

	for (i = 0; i < COUNTRY_CODE_LEN && region_string[i]; i++) {
		region_string[i] = toupper(region_string[i]);
	}

	for (i = 0; i < size; i++) {
		if (!memcmp(region_string,
			    cfg80211_special_region_code[i].region,
			    COUNTRY_CODE_LEN)) {
			PRINTM(MIOCTL, "special region code=%s\n",
			       region_string);
			return MTRUE;
		}
	}
	return MFALSE;
}

/**
 * @brief Get the encryption mode from cipher
 *
 * @param cipher        Cipher cuite
 * @param wpa_enabled   WPA enable or disable
 *
 * @return              MLAN_ENCRYPTION_MODE_*
 */
static int
woal_cfg80211_get_encryption_mode(t_u32 cipher, int *wpa_enabled)
{
	int encrypt_mode;

	ENTER();

	*wpa_enabled = 0;
	switch (cipher) {
	case MW_AUTH_CIPHER_NONE:
		encrypt_mode = MLAN_ENCRYPTION_MODE_NONE;
		break;
	case WLAN_CIPHER_SUITE_WEP40:
		encrypt_mode = MLAN_ENCRYPTION_MODE_WEP40;
		break;
	case WLAN_CIPHER_SUITE_WEP104:
		encrypt_mode = MLAN_ENCRYPTION_MODE_WEP104;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		encrypt_mode = MLAN_ENCRYPTION_MODE_TKIP;
		*wpa_enabled = 1;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		encrypt_mode = MLAN_ENCRYPTION_MODE_CCMP;
		*wpa_enabled = 1;
		break;
	default:
		encrypt_mode = -1;
	}

	LEAVE();
	return encrypt_mode;
}

/**
 *  @brief get associate failure status code
 *
 *  @param priv     Pointer to the moal_private driver data struct
 *
 *  @return         IEEE status code
 */
static int
woal_get_assoc_status(moal_private * priv)
{
	int ret = WLAN_STATUS_UNSPECIFIED_FAILURE;
	t_u16 status = (t_u16) (priv->assoc_status & 0xffff);
	t_u16 cap = (t_u16) (priv->assoc_status >> 16);

	switch (cap) {
	case 0xfffd:
	case 0xfffe:
		ret = status;
		break;
	case 0xfffc:
		ret = WLAN_STATUS_AUTH_TIMEOUT;
		break;
	default:
		break;
	}
	PRINTM(MCMND, "Assoc fail: status=%d, cap=0x%x, IEEE status=%d\n",
	       status, cap, ret);
	return ret;
}

/**
 *  @brief Check the pairwise or group cipher for
 *  WEP enabled or not
 *
 *  @param cipher       MLAN Cipher cuite
 *
 *  @return             1 -- enable or 0 -- disable
 */
static int
woal_cfg80211_is_alg_wep(t_u32 cipher)
{
	int alg = 0;
	ENTER();

	if (cipher == MLAN_ENCRYPTION_MODE_WEP40 ||
	    cipher == MLAN_ENCRYPTION_MODE_WEP104)
		alg = 1;

	LEAVE();
	return alg;
}

/**
 *  @brief Convert NL802.11 channel type into driver channel type
 *
 * The mapping is as follows -
 *      NL80211_CHAN_NO_HT     -> NO_SEC_CHANNEL
 *      NL80211_CHAN_HT20      -> NO_SEC_CHANNEL
 *      NL80211_CHAN_HT40PLUS  -> SEC_CHANNEL_ABOVE
 *      NL80211_CHAN_HT40MINUS -> SEC_CHANNEL_BELOW
 *      Others                 -> NO_SEC_CHANNEL
 *
 *  @param channel_type     Channel type
 *
 *  @return                 Driver channel type
 */
static int
woal_cfg80211_channel_type_to_channel(enum nl80211_channel_type channel_type)
{
	int channel;

	ENTER();

	switch (channel_type) {
	case NL80211_CHAN_NO_HT:
	case NL80211_CHAN_HT20:
		channel = NO_SEC_CHANNEL;
		break;
	case NL80211_CHAN_HT40PLUS:
		channel = SEC_CHANNEL_ABOVE;
		break;
	case NL80211_CHAN_HT40MINUS:
		channel = SEC_CHANNEL_BELOW;
		break;
	default:
		channel = NO_SEC_CHANNEL;
	}
	LEAVE();
	return channel;
}

/**
 *  @brief Convert secondary channel type to NL80211 channel type
 *
 * The mapping is as follows -
 *      NO_SEC_CHANNEL      -> NL80211_CHAN_HT20
 *      SEC_CHANNEL_ABOVE   -> NL80211_CHAN_HT40PLUS
 *      SEC_CHANNEL_BELOW   -> NL80211_CHAN_HT40MINUS
 *      Others              -> NL80211_CHAN_HT20
 *
 *  @param channel_type     Driver channel type
 *
 *  @return                 nl80211_channel_type type
 */
static enum nl80211_channel_type
woal_channel_to_nl80211_channel_type(int channel_type)
{
	enum nl80211_channel_type channel;

	ENTER();

	switch (channel_type) {
	case NO_SEC_CHANNEL:
		channel = NL80211_CHAN_HT20;
		break;
	case SEC_CHANNEL_ABOVE:
		channel = NL80211_CHAN_HT40PLUS;
		break;
	case SEC_CHANNEL_BELOW:
		channel = NL80211_CHAN_HT40MINUS;
		break;
	default:
		channel = NL80211_CHAN_HT20;
	}
	LEAVE();
	return channel;
}

/**
 *  @brief Convert NL80211 interface type to MLAN_BSS_MODE_*
 *
 *  @param iftype   Interface type of NL80211
 *
 *  @return         Driver bss mode
 */
static t_u32
woal_nl80211_iftype_to_mode(enum nl80211_iftype iftype)
{
	switch (iftype) {
	case NL80211_IFTYPE_ADHOC:
		return MLAN_BSS_MODE_IBSS;
	case NL80211_IFTYPE_STATION:
		return MLAN_BSS_MODE_INFRA;
	case NL80211_IFTYPE_UNSPECIFIED:
	default:
		return MLAN_BSS_MODE_AUTO;
	}
}

/**
 *  @brief Control WPS Session Enable/Disable
 *
 *  @param priv     Pointer to the moal_private driver data struct
 *  @param enable   enable/disable flag
 *
 *  @return          0 --success, otherwise fail
 */
static int
woal_wps_cfg(moal_private * priv, int enable)
{
	int ret = 0;
	mlan_ds_wps_cfg *pwps = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	PRINTM(MINFO, "WOAL_WPS_SESSION\n");

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_wps_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	pwps = (mlan_ds_wps_cfg *) req->pbuf;
	req->req_id = MLAN_IOCTL_WPS_CFG;
	req->action = MLAN_ACT_SET;
	pwps->sub_command = MLAN_OID_WPS_CFG_SESSION;
	if (enable)
		pwps->param.wps_session = MLAN_WPS_CFG_SESSION_START;
	else
		pwps->param.wps_session = MLAN_WPS_CFG_SESSION_END;

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
 * @brief configure ASSOC IE
 *
 * @param priv				A pointer to moal private structure
 * @param ie				A pointer to ie data
 * @param ie_len			The length of ie data
 *
 * @return                  0 -- success, otherwise fail
 */
static int
woal_cfg80211_assoc_ies_cfg(moal_private * priv, t_u8 * ie, int ie_len)
{
	int bytes_left = ie_len;
	t_u8 *pcurrent_ptr = ie;
	int total_ie_len;
	t_u8 element_len;
	int ret = MLAN_STATUS_SUCCESS;
	IEEEtypes_ElementId_e element_id;
	IEEEtypes_VendorSpecific_t *pvendor_ie;
	t_u8 wps_oui[] = { 0x00, 0x50, 0xf2, 0x04 };

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
			if (MLAN_STATUS_SUCCESS !=
			    woal_set_get_gen_ie(priv, MLAN_ACT_SET,
						pcurrent_ptr, &total_ie_len)) {
				PRINTM(MERROR, "Fail to set RSN IE\n");
				ret = -EFAULT;
				goto done;
			}
			PRINTM(MIOCTL, "Set RSN IE\n");
			break;
		case VENDOR_SPECIFIC_221:
			pvendor_ie =
				(IEEEtypes_VendorSpecific_t *) pcurrent_ptr;
			if (!memcmp
			    (pvendor_ie->vend_hdr.oui, wps_oui,
			     sizeof(pvendor_ie->vend_hdr.oui)) &&
			    (pvendor_ie->vend_hdr.oui_type == wps_oui[3])) {
				PRINTM(MIOCTL, "Enable WPS session\n");
				woal_wps_cfg(priv, MTRUE);
			}
			if (MLAN_STATUS_SUCCESS !=
			    woal_set_get_gen_ie(priv, MLAN_ACT_SET,
						pcurrent_ptr, &total_ie_len)) {
				PRINTM(MERROR,
				       "Fail to Set VENDOR SPECIFIC IE\n");
				ret = -EFAULT;
				goto done;
			}
			PRINTM(MIOCTL,
			       "Set VENDOR SPECIFIC IE, OUI: %02x:%02x:%02x:%02x\n",
			       pvendor_ie->vend_hdr.oui[0],
			       pvendor_ie->vend_hdr.oui[1],
			       pvendor_ie->vend_hdr.oui[2],
			       pvendor_ie->vend_hdr.oui_type);
			break;
		default:
			if (MLAN_STATUS_SUCCESS !=
			    woal_set_get_gen_ie(priv, MLAN_ACT_SET,
						pcurrent_ptr, &total_ie_len)) {
				PRINTM(MERROR, "Fail to set GEN IE\n");
				ret = -EFAULT;
				goto done;
			}
			PRINTM(MIOCTL, "Set GEN IE\n");
			break;
		}
		pcurrent_ptr += element_len + 2;
		/* Need to account for IE ID and IE Len */
		bytes_left -= (element_len + 2);
	}
done:
	return ret;
}

/**
 * @brief Send domain info command to FW
 *
 * @param priv      A pointer to moal_private structure
 *
 * @return          MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static mlan_status
woal_send_domain_info_cmd_fw(moal_private * priv)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	enum ieee80211_band band;
	struct ieee80211_supported_band *sband = NULL;
	struct ieee80211_channel *channel = NULL;
	t_u8 no_of_sub_band = 0;
	t_u8 no_of_parsed_chan = 0;
	t_u8 first_chan = 0, next_chan = 0, max_pwr = 0;
	t_u8 i, flag = 0;
	mlan_ds_11d_cfg *cfg_11d = NULL;
	mlan_ioctl_req *req = NULL;

	ENTER();

	if (!priv->wdev || !priv->wdev->wiphy) {
		PRINTM(MERROR, "No wdev or wiphy in priv\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	band = priv->phandle->band;
	if (!priv->wdev->wiphy->bands[band]) {
		PRINTM(MERROR, "11D: setting domain info in FW failed band=%d",
		       band);
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	if (MTRUE ==
	    is_cfg80211_special_region_code(priv->phandle->country_code)) {
		PRINTM(MIOCTL,
		       "skip region code config, cfg80211 special region code: %s\n",
		       priv->phandle->country_code);
		goto done;
	}
	PRINTM(MIOCTL, "Send domain info: country=%c%c band=%d\n",
	       priv->phandle->country_code[0], priv->phandle->country_code[1],
	       band);
	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_11d_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	cfg_11d = (mlan_ds_11d_cfg *) req->pbuf;
	cfg_11d->sub_command = MLAN_OID_11D_DOMAIN_INFO;
	req->req_id = MLAN_IOCTL_11D_CFG;
	req->action = MLAN_ACT_SET;

	/* Set country code */
	cfg_11d->param.domain_info.country_code[0] =
		priv->phandle->country_code[0];
	cfg_11d->param.domain_info.country_code[1] =
		priv->phandle->country_code[1];
	cfg_11d->param.domain_info.country_code[2] = ' ';
	cfg_11d->param.domain_info.band = band;

	sband = priv->wdev->wiphy->bands[band];
	for (i = 0; (i < sband->n_channels) &&
	     (no_of_sub_band < MRVDRV_MAX_SUBBAND_802_11D); i++) {
		channel = &sband->channels[i];
		if (channel->flags & IEEE80211_CHAN_DISABLED)
			continue;

		if (!flag) {
			flag = 1;
			next_chan = first_chan = (t_u32) channel->hw_value;
			max_pwr = channel->max_power;
			no_of_parsed_chan = 1;
			continue;
		}

		if (channel->hw_value == next_chan + 1 &&
		    channel->max_power == max_pwr) {
			next_chan++;
			no_of_parsed_chan++;
		} else {
			cfg_11d->param.domain_info.sub_band[no_of_sub_band]
				.first_chan = first_chan;
			cfg_11d->param.domain_info.sub_band[no_of_sub_band]
				.no_of_chan = no_of_parsed_chan;
			cfg_11d->param.domain_info.sub_band[no_of_sub_band]
				.max_tx_pwr = max_pwr;
			no_of_sub_band++;
			next_chan = first_chan = (t_u32) channel->hw_value;
			max_pwr = channel->max_power;
			no_of_parsed_chan = 1;
		}
	}

	if (flag) {
		cfg_11d->param.domain_info.sub_band[no_of_sub_band]
			.first_chan = first_chan;
		cfg_11d->param.domain_info.sub_band[no_of_sub_band]
			.no_of_chan = no_of_parsed_chan;
		cfg_11d->param.domain_info.sub_band[no_of_sub_band]
			.max_tx_pwr = max_pwr;
		no_of_sub_band++;
	}
	cfg_11d->param.domain_info.no_of_sub_band = no_of_sub_band;

	/* Send domain info command to FW */
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		PRINTM(MERROR, "11D: Error setting domain info in FW\n");
		goto done;
	}

done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 * @brief Request the driver to change the channel and
 * change domain info according to that channel
 *
 * @param priv            A pointer to moal_private structure
 * @param chan            A pointer to ieee80211_channel structure
 * @param channel_type    Channel type of nl80211_channel_type
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_set_rf_channel(moal_private * priv,
		    struct ieee80211_channel *chan,
		    enum nl80211_channel_type channel_type)
{
	int ret = 0;
	t_u32 mode, config_bands = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_radio_cfg *radio_cfg = NULL;

	ENTER();

	if (!chan) {
		LEAVE();
		return EINVAL;
	}
	mode = woal_nl80211_iftype_to_mode(priv->wdev->iftype);
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_radio_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	radio_cfg = (mlan_ds_radio_cfg *) req->pbuf;
	radio_cfg->sub_command = MLAN_OID_BAND_CFG;
	req->req_id = MLAN_IOCTL_RADIO_CFG;
	/* Get config_bands, adhoc_start_band and adhoc_channel values from
	   MLAN */
	req->action = MLAN_ACT_GET;
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}
	req->action = MLAN_ACT_SET;
	priv->phandle->band = chan->band;
	/* Set appropriate bands */
	if (chan->band == IEEE80211_BAND_2GHZ)
		config_bands = BAND_B | BAND_G | BAND_GN;
	else
		config_bands = BAND_AN | BAND_A;
	if (mode == MLAN_BSS_MODE_IBSS) {
		radio_cfg->param.band_cfg.adhoc_start_band = config_bands;
		radio_cfg->param.band_cfg.adhoc_channel =
			ieee80211_frequency_to_channel(chan->center_freq);
	}
	/* Set channel offset */
	radio_cfg->param.band_cfg.adhoc_chan_bandwidth =
		woal_cfg80211_channel_type_to_channel(channel_type);

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = -EFAULT;
		goto done;
	}
	woal_send_domain_info_cmd_fw(priv);

	PRINTM(MINFO,
	       "Setting band %d, channel bandwidth %d and mode = %d channel=%d\n",
	       config_bands, radio_cfg->param.band_cfg.adhoc_chan_bandwidth,
	       mode, ieee80211_frequency_to_channel(chan->center_freq));

	if (MLAN_STATUS_SUCCESS !=
	    woal_change_adhoc_chan(priv,
				   ieee80211_frequency_to_channel(chan->
								  center_freq)))
	{
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
 *  @brief Set ewpa mode
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param wait_option          Wait option
 *  @param ssid_bssid           A pointer to mlan_ssid_bssid structure
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status
woal_set_ewpa_mode(moal_private * priv, t_u8 wait_option,
		   mlan_ssid_bssid * ssid_bssid)
{
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_sec_cfg *sec = NULL;
	mlan_status status = MLAN_STATUS_SUCCESS;

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_sec_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	/* Fill request buffer */
	sec = (mlan_ds_sec_cfg *) req->pbuf;
	sec->sub_command = MLAN_OID_SEC_CFG_PASSPHRASE;
	req->req_id = MLAN_IOCTL_SEC_CFG;
	req->action = MLAN_ACT_GET;

	/* Try Get All */
	memset(&sec->param.passphrase, 0, sizeof(mlan_ds_passphrase));
	memcpy(&sec->param.passphrase.ssid, &ssid_bssid->ssid,
	       sizeof(sec->param.passphrase.ssid));
	memcpy(&sec->param.passphrase.bssid, &ssid_bssid->bssid,
	       MLAN_MAC_ADDR_LENGTH);
	sec->param.passphrase.psk_type = MLAN_PSK_QUERY;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);
	if (status != MLAN_STATUS_SUCCESS)
		goto error;
	sec->param.ewpa_enabled = MFALSE;
	if (sec->param.passphrase.psk_type == MLAN_PSK_PASSPHRASE) {
		if (sec->param.passphrase.psk.passphrase.passphrase_len > 0) {
			sec->param.ewpa_enabled = MTRUE;
		}
	} else if (sec->param.passphrase.psk_type == MLAN_PSK_PMK)
		sec->param.ewpa_enabled = MTRUE;

	sec->sub_command = MLAN_OID_SEC_CFG_EWPA_ENABLED;
	req->action = MLAN_ACT_SET;

	/* Send IOCTL request to MLAN */
	status = woal_request_ioctl(priv, req, wait_option);

error:
	if (req && (status != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return status;
}

/**
 * @brief Set encryption mode and enable WPA
 *
 * @param priv          A pointer to moal_private structure
 * @param encrypt_mode  Encryption mode
 * @param wpa_enabled   WPA enable or not
 *
 * @return              0 -- success, otherwise fail
 */
static int
woal_cfg80211_set_auth(moal_private * priv, int encrypt_mode, int wpa_enabled)
{
	int ret = 0;

	ENTER();

	if (MLAN_STATUS_SUCCESS !=
	    woal_set_encrypt_mode(priv, MOAL_IOCTL_WAIT, encrypt_mode))
		ret = -EFAULT;

	if (wpa_enabled) {
		if (MLAN_STATUS_SUCCESS !=
		    woal_set_wpa_enable(priv, MOAL_IOCTL_WAIT, 1))
			ret = -EFAULT;
	}

	LEAVE();
	return ret;
}

/**
 * @brief Informs the CFG802.11 subsystem of a new BSS connection.
 *
 * The following information are sent to the CFG802.11 subsystem
 * to register the new BSS connection. If we do not register the new BSS,
 * a kernel panic will result.
 *      - MAC address
 *      - Capabilities
 *      - Beacon period
 *      - RSSI value
 *      - Channel
 *      - Supported rates IE
 *      - Extended capabilities IE
 *      - DS parameter set IE
 *      - HT Capability IE
 *      - Vendor Specific IE (221)
 *      - WPA IE
 *      - RSN IE
 *
 * @param priv            A pointer to moal_private structure
 * @param ssid_bssid      A pointer to A pointer to mlan_ssid_bssid structure
 * @param wait_option     wait_option
 *
 * @return          MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
woal_inform_bss_from_scan_result(moal_private * priv,
				 mlan_ssid_bssid * ssid_bssid, t_u8 wait_option)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	struct ieee80211_channel *chan;
	mlan_scan_resp scan_resp;
	BSSDescriptor_t *scan_table;
	t_u64 ts = 0;
	u16 cap_info = 0;
	int i = 0;
	struct cfg80211_bss *pub = NULL;
	ENTER();
	if (!priv->wdev || !priv->wdev->wiphy) {
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}

	memset(&scan_resp, 0, sizeof(scan_resp));
	if (MLAN_STATUS_SUCCESS != woal_get_scan_table(priv,
						       wait_option,
						       &scan_resp)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	if (scan_resp.num_in_scan_table) {
		scan_table = (BSSDescriptor_t *) scan_resp.pscan_table;
		for (i = 0; i < scan_resp.num_in_scan_table; i++) {
			if (ssid_bssid) {
				/* Inform specific BSS only */
				if (memcmp
				    (ssid_bssid->ssid.ssid,
				     scan_table[i].ssid.ssid,
				     ssid_bssid->ssid.ssid_len) ||
				    memcmp(ssid_bssid->bssid,
					   scan_table[i].mac_address, ETH_ALEN))
					continue;
			}
			if (!scan_table[i].freq) {
				scan_table[i].freq =
					ieee80211_channel_to_frequency((int)
								       scan_table
								       [i].
								       channel,
								       woal_band_cfg_to_ieee_band
								       (scan_table
									[i].
									bss_band));
			}
			chan = ieee80211_get_channel(priv->wdev->wiphy,
						     scan_table[i].freq);
			if (!chan) {
				PRINTM(MERROR,
				       "Fail to get chan with freq: channel=%d freq=%d\n",
				       (int)scan_table[i].channel,
				       (int)scan_table[i].freq);
				continue;
			}
			memcpy(&ts, scan_table[i].time_stamp, sizeof(ts));
			memcpy(&cap_info, &scan_table[i].cap_info,
			       sizeof(cap_info));
			pub = cfg80211_inform_bss(priv->wdev->wiphy, chan,
						  scan_table[i].mac_address, ts,
						  cap_info,
						  scan_table[i].beacon_period,
						  scan_table[i].pbeacon_buf +
						  WLAN_802_11_FIXED_IE_SIZE,
						  scan_table[i].
						  beacon_buf_size -
						  WLAN_802_11_FIXED_IE_SIZE,
						  -RSSI_DBM_TO_MDM(scan_table
								   [i].rssi),
						  GFP_KERNEL);
			if (pub) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
				pub->len_information_elements =
					pub->len_beacon_ies;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
				cfg80211_put_bss(priv->wdev->wiphy, pub);
#else
				cfg80211_put_bss(pub);
#endif
			}
		}
	}
done:
	LEAVE();
	return ret;
}

/**
 * @brief Informs the CFG802.11 subsystem of a new IBSS connection.
 *
 * The following information are sent to the CFG802.11 subsystem
 * to register the new IBSS connection. If we do not register the
 * new IBSS, a kernel panic will result.
 *      - MAC address
 *      - Capabilities
 *      - Beacon period
 *      - RSSI value
 *      - Channel
 *      - Supported rates IE
 *      - Extended capabilities IE
 *      - DS parameter set IE
 *      - HT Capability IE
 *      - Vendor Specific IE (221)
 *      - WPA IE
 *      - RSN IE
 *
 * @param priv              A pointer to moal_private structure
 * @param cahn              A pointer to ieee80211_channel structure
 * @param beacon_interval   Beacon interval
 *
 * @return                  MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static mlan_status
woal_cfg80211_inform_ibss_bss(moal_private * priv,
			      struct ieee80211_channel *chan,
			      t_u16 beacon_interval)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_bss_info bss_info;
	mlan_ds_get_signal signal;
	t_u8 ie_buf[MLAN_MAX_SSID_LENGTH + sizeof(IEEEtypes_Header_t)];
	int ie_len = 0;
	struct cfg80211_bss *bss = NULL;

	ENTER();

	ret = woal_get_bss_info(priv, MOAL_IOCTL_WAIT, &bss_info);
	if (ret)
		goto done;

	memset(ie_buf, 0, sizeof(ie_buf));
	ie_buf[0] = WLAN_EID_SSID;
	ie_buf[1] = bss_info.ssid.ssid_len;

	memcpy(&ie_buf[sizeof(IEEEtypes_Header_t)],
	       &bss_info.ssid.ssid, bss_info.ssid.ssid_len);
	ie_len = ie_buf[1] + sizeof(IEEEtypes_Header_t);

	/* Get signal information from the firmware */
	memset(&signal, 0, sizeof(mlan_ds_get_signal));
	if (MLAN_STATUS_SUCCESS !=
	    woal_get_signal_info(priv, MOAL_IOCTL_WAIT, &signal)) {
		PRINTM(MERROR, "Error getting signal information\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	bss = cfg80211_inform_bss(priv->wdev->wiphy, chan,
				  bss_info.bssid, 0, WLAN_CAPABILITY_IBSS,
				  beacon_interval, ie_buf, ie_len,
				  signal.bcn_rssi_avg, GFP_KERNEL);
	if (bss)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
		cfg80211_put_bss(priv->wdev->wiphy, bss);
#else
		cfg80211_put_bss(bss);
#endif
done:
	LEAVE();
	return ret;
}

/**
 * @brief Process country IE before assoicate
 *
 * @param priv            A pointer to moal_private structure
 * @param bss             A pointer to cfg80211_bss structure
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_process_country_ie(moal_private * priv, struct cfg80211_bss *bss)
{
	u8 *country_ie, country_ie_len;
	int ret = 0;
	mlan_ioctl_req *req = NULL;
	mlan_ds_11d_cfg *cfg_11d = NULL;

	ENTER();
	country_ie = (u8 *) ieee80211_bss_get_ie(bss, WLAN_EID_COUNTRY);
	if (!country_ie) {
		PRINTM(MIOCTL, "No country IE found!\n");
		woal_send_domain_info_cmd_fw(priv);
		LEAVE();
		return 0;
	}

	country_ie_len = country_ie[1];
	if (country_ie_len < IEEE80211_COUNTRY_IE_MIN_LEN) {
		PRINTM(MIOCTL, "Wrong Country IE length!\n");
		woal_send_domain_info_cmd_fw(priv);
		LEAVE();
		return 0;
	}
	PRINTM(MIOCTL, "Find bss country IE: %c%c band=%d\n", country_ie[2],
	       country_ie[3], priv->phandle->band);
	priv->phandle->country_code[0] = country_ie[2];
	priv->phandle->country_code[1] = country_ie[3];
	priv->phandle->country_code[2] = ' ';
	if (MLAN_STATUS_SUCCESS !=
	    woal_set_region_code(priv, priv->phandle->country_code))
		PRINTM(MERROR, "Set country code failed!\n");

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_11d_cfg));
	if (req == NULL) {
		PRINTM(MERROR, "Fail to allocate mlan_ds_11d_cfg buffer\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	cfg_11d = (mlan_ds_11d_cfg *) req->pbuf;
	cfg_11d->sub_command = MLAN_OID_11D_DOMAIN_INFO;
	req->req_id = MLAN_IOCTL_11D_CFG;
	req->action = MLAN_ACT_SET;

	/* Set country code */
	cfg_11d->param.domain_info.country_code[0] =
		priv->phandle->country_code[0];
	cfg_11d->param.domain_info.country_code[1] =
		priv->phandle->country_code[1];
	cfg_11d->param.domain_info.country_code[2] = ' ';

    /** IEEE80211_BAND_2GHZ or IEEE80211_BAND_5GHZ */
	cfg_11d->param.domain_info.band = priv->phandle->band;

	country_ie_len -= IEEE80211_COUNTRY_STRING_LEN;
	cfg_11d->param.domain_info.no_of_sub_band =
		country_ie_len / sizeof(struct ieee80211_country_ie_triplet);
	memcpy((u8 *) cfg_11d->param.domain_info.sub_band,
	       &country_ie[2] + IEEE80211_COUNTRY_STRING_LEN, country_ie_len);

	/* Send domain info command to FW */
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		PRINTM(MERROR, "11D: Error setting domain info in FW\n");
		goto done;
	}
done:
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

/**
 * @brief Request scan based on connect parameter
 *
 * @param priv            A pointer to moal_private structure
 * @param conn_param      A pointer to connect parameters
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_connect_scan(moal_private * priv,
			   struct cfg80211_connect_params *conn_param)
{
	moal_handle *handle = priv->phandle;
	int ret = 0;
	wlan_user_scan_cfg scan_req;
	enum ieee80211_band band;
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *ch;
	int chan_idx = 0, i;
	ENTER();
	if (handle->scan_pending_on_block == MTRUE) {
		PRINTM(MINFO, "scan already in processing...\n");
		LEAVE();
		return ret;
	}
#ifdef REASSOCIATION
	if (MOAL_ACQ_SEMAPHORE_BLOCK(&handle->reassoc_sem)) {
		PRINTM(MERROR, "Acquire semaphore error, woal_do_combo_scan\n");
		LEAVE();
		return -EBUSY;
	}
#endif /* REASSOCIATION */
	priv->report_scan_result = MTRUE;
	memset(&scan_req, 0x00, sizeof(scan_req));
	memcpy(scan_req.ssid_list[0].ssid, conn_param->ssid,
	       conn_param->ssid_len);
	scan_req.ssid_list[0].max_len = 0;
	if (conn_param->channel) {
		scan_req.chan_list[0].chan_number =
			conn_param->channel->hw_value;
		scan_req.chan_list[0].radio_type = conn_param->channel->band;
		if (conn_param->channel->
		    flags & (IEEE80211_CHAN_PASSIVE_SCAN |
			     IEEE80211_CHAN_RADAR))
			scan_req.chan_list[0].scan_type =
				MLAN_SCAN_TYPE_PASSIVE;
		else
			scan_req.chan_list[0].scan_type = MLAN_SCAN_TYPE_ACTIVE;
		scan_req.chan_list[0].scan_time = 0;
	} else {
		for (band = 0; (band < IEEE80211_NUM_BANDS); band++) {
			if (!priv->wdev->wiphy->bands[band])
				continue;
			sband = priv->wdev->wiphy->bands[band];
			for (i = 0; (i < sband->n_channels); i++) {
				ch = &sband->channels[i];
				if (ch->flags & IEEE80211_CHAN_DISABLED)
					continue;
				scan_req.chan_list[chan_idx].radio_type = band;
				if (ch->
				    flags & (IEEE80211_CHAN_PASSIVE_SCAN |
					     IEEE80211_CHAN_RADAR))
					scan_req.chan_list[chan_idx].scan_type =
						MLAN_SCAN_TYPE_PASSIVE;
				else
					scan_req.chan_list[chan_idx].scan_type =
						MLAN_SCAN_TYPE_ACTIVE;
				scan_req.chan_list[chan_idx].chan_number =
					(u32) ch->hw_value;
				chan_idx++;
			}
		}
	}
	ret = woal_request_userscan(priv, MOAL_IOCTL_WAIT, &scan_req);
#ifdef REASSOCIATION
	MOAL_REL_SEMAPHORE(&handle->reassoc_sem);
#endif
	LEAVE();
	return ret;

}

/**
 * @brief Request the driver for (re)association
 *
 * @param priv            A pointer to moal_private structure
 * @param sme             A pointer to connect parameters
 *
 * @return                0 -- success, otherwise fail
 */
int
woal_cfg80211_assoc(moal_private * priv, void *sme)
{
	struct cfg80211_ibss_params *ibss_param = NULL;
	struct cfg80211_connect_params *conn_param = NULL;
	mlan_802_11_ssid req_ssid;
	mlan_ssid_bssid ssid_bssid;
	mlan_ds_radio_cfg *radio_cfg = NULL;
	mlan_ioctl_req *req = NULL;
	int ret = 0;
	t_u32 auth_type = 0, mode;
	int wpa_enabled = 0;
	int group_enc_mode = 0, pairwise_enc_mode = 0;
	int alg_is_wep = 0;

	t_u8 *ssid, ssid_len = 0, *bssid;
	t_u8 *ie = NULL;
	int ie_len = 0;
	struct ieee80211_channel *channel = NULL;
	t_u16 beacon_interval = 0;
	bool privacy;
	struct cfg80211_bss *bss = NULL;

	ENTER();

	mode = woal_nl80211_iftype_to_mode(priv->wdev->iftype);

	if (mode == MLAN_BSS_MODE_IBSS) {
		ibss_param = (struct cfg80211_ibss_params *)sme;
		ssid = ibss_param->ssid;
		ssid_len = ibss_param->ssid_len;
		bssid = ibss_param->bssid;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
		channel = ibss_param->channel;
#else
		channel = ibss_param->chandef.chan;
#endif
		if (channel)
			priv->phandle->band = channel->band;
		if (ibss_param->ie_len)
			ie = ibss_param->ie;
		ie_len = ibss_param->ie_len;
		beacon_interval = ibss_param->beacon_interval;
		privacy = ibss_param->privacy;

	} else {
		conn_param = (struct cfg80211_connect_params *)sme;
		ssid = conn_param->ssid;
		ssid_len = conn_param->ssid_len;
		bssid = conn_param->bssid;
		channel = conn_param->channel;
		if (channel)
			priv->phandle->band = channel->band;
		if (conn_param->ie_len)
			ie = conn_param->ie;
		ie_len = conn_param->ie_len;
		privacy = conn_param->privacy;
		bss = cfg80211_get_bss(priv->wdev->wiphy, channel, bssid, ssid,
				       ssid_len, WLAN_CAPABILITY_ESS,
				       WLAN_CAPABILITY_ESS);
		if (bss) {
			woal_process_country_ie(priv, bss);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
			cfg80211_put_bss(priv->wdev->wiphy, bss);
#else
			cfg80211_put_bss(bss);
#endif
		} else
			woal_send_domain_info_cmd_fw(priv);
	}

	memset(&req_ssid, 0, sizeof(mlan_802_11_ssid));
	memset(&ssid_bssid, 0, sizeof(mlan_ssid_bssid));

	req_ssid.ssid_len = ssid_len;
	if (ssid_len > MW_ESSID_MAX_SIZE) {
		PRINTM(MERROR, "Invalid SSID - aborting\n");
		ret = -EINVAL;
		goto done;
	}

	memcpy(req_ssid.ssid, ssid, ssid_len);
	if (!req_ssid.ssid_len || req_ssid.ssid[0] < 0x20) {
		PRINTM(MERROR, "Invalid SSID - aborting\n");
		ret = -EINVAL;
		goto done;
	}

	if ((mode == MLAN_BSS_MODE_IBSS) && channel) {
		/* Get the secondary channel offset */
		req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_radio_cfg));
		if (req == NULL) {
			ret = -ENOMEM;
			goto done;
		}
		radio_cfg = (mlan_ds_radio_cfg *) req->pbuf;
		radio_cfg->sub_command = MLAN_OID_BAND_CFG;
		req->req_id = MLAN_IOCTL_RADIO_CFG;
		req->action = MLAN_ACT_GET;

		if (MLAN_STATUS_SUCCESS !=
		    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
			ret = -EFAULT;
			goto done;
		}
		if (MLAN_STATUS_SUCCESS != woal_set_rf_channel(priv,
							       channel,
							       woal_channel_to_nl80211_channel_type
							       (radio_cfg->
								param.band_cfg.
								adhoc_chan_bandwidth)))
		{
			ret = -EFAULT;
			goto done;
		}
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_set_ewpa_mode(priv, MOAL_IOCTL_WAIT, &ssid_bssid)) {
		ret = -EFAULT;
		goto done;
	}

	if (MLAN_STATUS_SUCCESS !=
	    woal_cfg80211_set_key(priv, 0, 0, NULL, 0, NULL, 0,
				  KEY_INDEX_CLEAR_ALL, NULL, 1)) {
		/* Disable keys and clear all previous security settings */
		ret = -EFAULT;
		goto done;
	}

	if (ie && ie_len) {	/* Set the IE */
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_assoc_ies_cfg(priv, ie, ie_len)) {
			ret = -EFAULT;
			goto done;
		}
	}

	if (conn_param && mode != MLAN_BSS_MODE_IBSS) {
		/* These parameters are only for managed mode */
		if (conn_param->auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM)
			auth_type = MLAN_AUTH_MODE_OPEN;
		else if (conn_param->auth_type == NL80211_AUTHTYPE_SHARED_KEY)
			auth_type = MLAN_AUTH_MODE_SHARED;
		else if (conn_param->auth_type == NL80211_AUTHTYPE_NETWORK_EAP)
			auth_type = MLAN_AUTH_MODE_NETWORKEAP;
		else
			auth_type = MLAN_AUTH_MODE_AUTO;

		if (MLAN_STATUS_SUCCESS !=
		    woal_set_auth_mode(priv, MOAL_IOCTL_WAIT, auth_type)) {
			ret = -EFAULT;
			goto done;
		}

		if (conn_param->crypto.n_ciphers_pairwise) {
			pairwise_enc_mode =
				woal_cfg80211_get_encryption_mode(conn_param->
								  crypto.ciphers_pairwise
								  [0],
								  &wpa_enabled);
			ret = woal_cfg80211_set_auth(priv, pairwise_enc_mode,
						     wpa_enabled);
			if (ret)
				goto done;
		}

		if (conn_param->crypto.cipher_group) {
			group_enc_mode =
				woal_cfg80211_get_encryption_mode(conn_param->
								  crypto.cipher_group,
								  &wpa_enabled);
			ret = woal_cfg80211_set_auth(priv, group_enc_mode,
						     wpa_enabled);
			if (ret)
				goto done;
		}

		if (conn_param->key) {
			alg_is_wep =
				woal_cfg80211_is_alg_wep(pairwise_enc_mode) |
				woal_cfg80211_is_alg_wep(group_enc_mode);
			if (alg_is_wep) {
				PRINTM(MINFO, "Setting wep encryption with "
				       "key len %d\n", conn_param->key_len);
				/* Set the WEP key */
				if (MLAN_STATUS_SUCCESS !=
				    woal_cfg80211_set_wep_keys(priv,
							       conn_param->key,
							       conn_param->
							       key_len,
							       conn_param->
							       key_idx)) {
					ret = -EFAULT;
					goto done;
				}
				/* Enable the WEP key by key index */
				if (MLAN_STATUS_SUCCESS !=
				    woal_cfg80211_set_wep_keys(priv, NULL, 0,
							       conn_param->
							       key_idx)) {
					ret = -EFAULT;
					goto done;
				}
			}
		}
	}

	if (mode == MLAN_BSS_MODE_IBSS) {
		mlan_ds_bss *bss = NULL;
		/* Change beacon interval */
		if ((beacon_interval < MLAN_MIN_BEACON_INTERVAL) ||
		    (beacon_interval > MLAN_MAX_BEACON_INTERVAL)) {
			ret = -EINVAL;
			goto done;
		}
		if (req)
			kfree(req);
		req = NULL;

		req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_bss));
		if (req == NULL) {
			ret = -ENOMEM;
			goto done;
		}
		bss = (mlan_ds_bss *) req->pbuf;
		req->req_id = MLAN_IOCTL_BSS;
		req->action = MLAN_ACT_SET;
		bss->sub_command = MLAN_OID_IBSS_BCN_INTERVAL;
		bss->param.bcn_interval = beacon_interval;
		if (MLAN_STATUS_SUCCESS !=
		    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
			ret = -EFAULT;
			goto done;
		}

		/* "privacy" is set only for ad-hoc mode */
		if (privacy) {
			/*
			 * Keep MLAN_ENCRYPTION_MODE_WEP40 for now so that
			 * the firmware can find a matching network from the
			 * scan. cfg80211 does not give us the encryption
			 * mode at this stage so just setting it to wep here
			 */
			if (MLAN_STATUS_SUCCESS !=
			    woal_set_auth_mode(priv, MOAL_IOCTL_WAIT,
					       MLAN_AUTH_MODE_OPEN)) {
				ret = -EFAULT;
				goto done;
			}

			wpa_enabled = 0;
			ret = woal_cfg80211_set_auth(priv,
						     MLAN_ENCRYPTION_MODE_WEP104,
						     wpa_enabled);
			if (ret)
				goto done;
		}
	}
	memcpy(&ssid_bssid.ssid, &req_ssid, sizeof(mlan_802_11_ssid));
	if (bssid)
		memcpy(&ssid_bssid.bssid, bssid, ETH_ALEN);
	if (MLAN_STATUS_SUCCESS != woal_find_essid(priv, &ssid_bssid)) {
		/* Do specific SSID scanning */
		if (mode != MLAN_BSS_MODE_IBSS)
			ret = woal_cfg80211_connect_scan(priv, conn_param);
		else
			ret = woal_request_scan(priv, MOAL_IOCTL_WAIT,
						&req_ssid);
		if (ret) {
			ret = -EFAULT;
			goto done;
		}
	}

	/* Disconnect before try to associate */
	if (mode == MLAN_BSS_MODE_IBSS)
		woal_disconnect(priv, MOAL_IOCTL_WAIT, NULL);

	if (mode != MLAN_BSS_MODE_IBSS) {
		if (MLAN_STATUS_SUCCESS !=
		    woal_find_best_network(priv, MOAL_IOCTL_WAIT,
					   &ssid_bssid)) {
			ret = -EFAULT;
			goto done;
		}
		/* Inform the BSS information to kernel, otherwise kernel will
		   give a panic after successful assoc */
		if (MLAN_STATUS_SUCCESS !=
		    woal_inform_bss_from_scan_result(priv, &ssid_bssid,
						     MOAL_IOCTL_WAIT)) {
			ret = -EFAULT;
			goto done;
		}
	} else if (MLAN_STATUS_SUCCESS !=
		   woal_find_best_network(priv, MOAL_IOCTL_WAIT, &ssid_bssid))
		/* Adhoc start, Check the channel command */
		woal_11h_channel_check_ioctl(priv);

	PRINTM(MINFO, "Trying to associate to %s and bssid " MACSTR "\n",
	       (char *)req_ssid.ssid, MAC2STR(ssid_bssid.bssid));

	/* Zero SSID implies use BSSID to connect */
	if (bssid)
		memset(&ssid_bssid.ssid, 0, sizeof(mlan_802_11_ssid));
	else			/* Connect to BSS by ESSID */
		memset(&ssid_bssid.bssid, 0, MLAN_MAC_ADDR_LENGTH);

	if (MLAN_STATUS_SUCCESS !=
	    woal_bss_start(priv, MOAL_IOCTL_WAIT, &ssid_bssid)) {
		ret = -EFAULT;
		goto done;
	}

	/* Inform the IBSS information to kernel, otherwise kernel will give a
	   panic after successful assoc */
	if (mode == MLAN_BSS_MODE_IBSS) {
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_inform_ibss_bss(priv, channel,
						  beacon_interval)) {
			ret = -EFAULT;
			goto done;
		}
	}

done:
	if (ret) {
		/* clear the encryption mode */
		woal_cfg80211_set_auth(priv, MLAN_ENCRYPTION_MODE_NONE, MFALSE);
		/* clear IE */
		ie_len = 0;
		if (MLAN_STATUS_SUCCESS !=
		    woal_set_get_gen_ie(priv, MLAN_ACT_SET, NULL, &ie_len)) {
			PRINTM(MERROR, "Could not clear RSN IE\n");
			ret = -EFAULT;
		}
	}
	if (req)
		kfree(req);
	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0) || defined(COMPAT_WIRELESS)
/**
 *  @brief Set/Get DTIM period
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param action               Action set or get
 *  @param wait_option          Wait option
 *  @param value                DTIM period
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
static mlan_status
woal_set_get_dtim_period(moal_private * priv,
			 t_u32 action, t_u8 wait_option, t_u8 * value)
{
	mlan_ioctl_req *req = NULL;
	mlan_ds_snmp_mib *mib = NULL;
	mlan_status ret = MLAN_STATUS_SUCCESS;

	ENTER();

	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_snmp_mib));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	mib = (mlan_ds_snmp_mib *) req->pbuf;
	mib->sub_command = MLAN_OID_SNMP_MIB_DTIM_PERIOD;
	req->req_id = MLAN_IOCTL_SNMP_MIB;
	req->action = action;

	if (action == MLAN_ACT_SET) {
		mib->param.dtim_period = *value;
	}

	/* Send IOCTL request to MLAN */
	ret = woal_request_ioctl(priv, req, wait_option);
	if (ret == MLAN_STATUS_SUCCESS && action == MLAN_ACT_GET) {
		*value = (t_u8) mib->param.dtim_period;
	}

done:
	if (req && (ret != MLAN_STATUS_PENDING))
		kfree(req);
	LEAVE();
	return ret;
}
#endif

/**
 * @brief Request the driver to dump the station information
 *
 * @param priv            A pointer to moal_private structure
 * @param sinfo           A pointer to station_info structure
 *
 * @return                0 -- success, otherwise fail
 */
static mlan_status
woal_cfg80211_dump_station_info(moal_private * priv, struct station_info *sinfo)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_get_signal signal;
	mlan_ioctl_req *req = NULL;
	mlan_ds_rate *rate = NULL;
	t_u16 Rates[12] =
		{ 0x02, 0x04, 0x0B, 0x16, 0x0C, 0x12, 0x18, 0x24, 0x30, 0x48,
	      0x60, 0x6c };
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0) || defined(COMPAT_WIRELESS)
	mlan_bss_info bss_info;
	t_u8 dtim_period = 0;
#endif

	ENTER();
	sinfo->filled = STATION_INFO_RX_BYTES | STATION_INFO_TX_BYTES |
		STATION_INFO_RX_PACKETS | STATION_INFO_TX_PACKETS |
		STATION_INFO_SIGNAL | STATION_INFO_TX_BITRATE;

	/* Get signal information from the firmware */
	memset(&signal, 0, sizeof(mlan_ds_get_signal));
	if (MLAN_STATUS_SUCCESS !=
	    woal_get_signal_info(priv, MOAL_IOCTL_WAIT, &signal)) {
		PRINTM(MERROR, "Error getting signal information\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_rate));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	rate = (mlan_ds_rate *) req->pbuf;
	rate->sub_command = MLAN_OID_GET_DATA_RATE;
	req->req_id = MLAN_IOCTL_RATE;
	req->action = MLAN_ACT_GET;
	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	if (rate->param.data_rate.tx_data_rate >= MLAN_RATE_INDEX_MCS0) {
		sinfo->txrate.flags = RATE_INFO_FLAGS_MCS;
		if (rate->param.data_rate.tx_ht_bw == MLAN_HT_BW40) {
			sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
		}
		if (rate->param.data_rate.tx_ht_gi == MLAN_HT_SGI) {
			sinfo->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		}
		sinfo->txrate.mcs =
			rate->param.data_rate.tx_data_rate -
			MLAN_RATE_INDEX_MCS0;
	} else {
		/* Bit rate is in 500 kb/s units. Convert it to 100kb/s units */
		sinfo->txrate.legacy =
			Rates[rate->param.data_rate.tx_data_rate] * 5;
	}
	sinfo->rx_bytes = priv->stats.rx_bytes;
	sinfo->tx_bytes = priv->stats.tx_bytes;
	sinfo->rx_packets = priv->stats.rx_packets;
	sinfo->tx_packets = priv->stats.tx_packets;
	sinfo->signal = signal.bcn_rssi_avg;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0) || defined(COMPAT_WIRELESS)
	/* Update BSS information */
	sinfo->filled |= STATION_INFO_BSS_PARAM;
	sinfo->bss_param.flags = 0;
	ret = woal_get_bss_info(priv, MOAL_IOCTL_WAIT, &bss_info);
	if (ret)
		goto done;
	if (bss_info.capability_info & WLAN_CAPABILITY_SHORT_PREAMBLE)
		sinfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_PREAMBLE;
	if (bss_info.capability_info & WLAN_CAPABILITY_SHORT_SLOT_TIME)
		sinfo->bss_param.flags |= BSS_PARAM_FLAGS_SHORT_SLOT_TIME;
	sinfo->bss_param.beacon_interval = bss_info.beacon_interval;
	/* Get DTIM period */
	ret = woal_set_get_dtim_period(priv, MLAN_ACT_GET,
				       MOAL_IOCTL_WAIT, &dtim_period);
	if (ret) {
		PRINTM(MERROR, "Get DTIM period failed\n");
		goto done;
	}
	sinfo->bss_param.dtim_period = dtim_period;
#endif

done:
	if (req)
		kfree(req);

	LEAVE();
	return ret;
}

/********************************************************
				Global Functions
********************************************************/

/**
 * @brief Request the driver to change regulatory domain
 *
 * @param wiphy           A pointer to wiphy structure
 * @param request         A pointer to regulatory_request structure
 *
 * @return                0
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
static void
#else
static int
#endif
woal_cfg80211_reg_notifier(struct wiphy *wiphy,
			   struct regulatory_request *request)
{
	moal_private *priv = NULL;
	moal_handle *handle = (moal_handle *) woal_get_wiphy_priv(wiphy);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	int ret = 0;
#endif

	ENTER();

	priv = woal_get_priv(handle, MLAN_BSS_ROLE_ANY);
	if (!priv) {
		PRINTM(MFATAL, "Unable to get priv in %s()\n", __FUNCTION__);
		LEAVE();
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
		return -EINVAL;
#else
		return;
#endif
	}

	PRINTM(MIOCTL, "cfg80211 regulatory domain callback "
	       "%c%c\n", request->alpha2[0], request->alpha2[1]);
	if (MTRUE == is_cfg80211_special_region_code(request->alpha2)) {
		PRINTM(MIOCTL, "Skip configure special region code\n");
		LEAVE();
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
		return ret;
#else
		return;
#endif
	}
	handle->country_code[0] = request->alpha2[0];
	handle->country_code[1] = request->alpha2[1];
	handle->country_code[2] = ' ';
	if (MLAN_STATUS_SUCCESS !=
	    woal_set_region_code(priv, handle->country_code))
		PRINTM(MERROR, "Set country code failed!\n");
	switch (request->initiator) {
	case NL80211_REGDOM_SET_BY_DRIVER:
		PRINTM(MIOCTL, "Regulatory domain BY_DRIVER\n");
		break;
	case NL80211_REGDOM_SET_BY_CORE:
		PRINTM(MIOCTL, "Regulatory domain BY_CORE\n");
		break;
	case NL80211_REGDOM_SET_BY_USER:
		PRINTM(MIOCTL, "Regulatory domain BY_USER\n");
		break;
		/* TODO: apply driver specific changes in channel flags based
		   on the request initiator if necessory. * */
	case NL80211_REGDOM_SET_BY_COUNTRY_IE:
		PRINTM(MIOCTL, "Regulatory domain BY_COUNTRY_IE\n");
		break;
	}
	if (priv->wdev && priv->wdev->wiphy &&
	    (request->initiator != NL80211_REGDOM_SET_BY_COUNTRY_IE))
		woal_send_domain_info_cmd_fw(priv);
	LEAVE();
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	return ret;
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
/**
 * @brief Request the driver to do a scan. Always returning
 * zero meaning that the scan request is given to driver,
 * and will be valid until passed to cfg80211_scan_done().
 * To inform scan results, call cfg80211_inform_bss().
 *
 * @param wiphy           A pointer to wiphy structure
 * @param request         A pointer to cfg80211_scan_request structure
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
#else
/**
 * @brief Request the driver to do a scan. Always returning
 * zero meaning that the scan request is given to driver,
 * and will be valid until passed to cfg80211_scan_done().
 * To inform scan results, call cfg80211_inform_bss().
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param request         A pointer to cfg80211_scan_request structure
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_scan(struct wiphy *wiphy, struct net_device *dev,
		   struct cfg80211_scan_request *request)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	struct net_device *dev = request->wdev->netdev;
#endif
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	wlan_user_scan_cfg scan_req;
	mlan_bss_info bss_info;
	struct ieee80211_channel *chan;
	int ret = 0, i;

	ENTER();

	PRINTM(MINFO, "Received scan request on %s\n", dev->name);
#ifdef UAP_CFG80211
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
		LEAVE();
		cfg80211_scan_done(request, MTRUE);
		return 0;
	}
#endif

	if (priv->phandle->scan_pending_on_block == MTRUE) {
		PRINTM(MINFO, "scan already in processing...\n");
		LEAVE();
		return -EBUSY;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) || defined(COMPAT_WIRELESS)
	if (priv->last_event & EVENT_BG_SCAN_REPORT) {
		PRINTM(MINFO, "block scan while pending BGSCAN result\n");
		priv->last_event = 0;
		cfg80211_scan_done(request, MTRUE);
		LEAVE();
		return 0;
	}
#endif
#if defined(STA_CFG80211) || defined(UAP_CFG80211)
#ifdef WIFI_DIRECT_SUPPORT
	if (priv->phandle->is_go_timer_set) {
		PRINTM(MINFO, "block scan in go timer....\n");
		cfg80211_scan_done(request, MTRUE);
		LEAVE();
		return 0;
	}
#endif
#endif
	memset(&bss_info, 0, sizeof(bss_info));
	if (MLAN_STATUS_SUCCESS ==
	    woal_get_bss_info(priv, MOAL_IOCTL_WAIT, &bss_info)) {
		if (bss_info.scan_block) {
			PRINTM(MINFO, "block scan in mlan module...\n");
			LEAVE();
			return -EBUSY;
		}
	}
	if (priv->scan_request && priv->scan_request != request) {
		LEAVE();
		return -EBUSY;
	}
	priv->scan_request = request;

	memset(&scan_req, 0x00, sizeof(scan_req));
	for (i = 0; i < priv->scan_request->n_ssids; i++) {
		memcpy(scan_req.ssid_list[i].ssid,
		       priv->scan_request->ssids[i].ssid,
		       priv->scan_request->ssids[i].ssid_len);
		if (priv->scan_request->ssids[i].ssid_len)
			scan_req.ssid_list[i].max_len = 0;
		else
			scan_req.ssid_list[i].max_len = 0xff;
		PRINTM(MIOCTL, "scan: ssid=%s\n", scan_req.ssid_list[i].ssid);
	}
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	if (priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT &&
	    priv->scan_request->n_ssids) {
		if (!memcmp(scan_req.ssid_list[0].ssid, "DIRECT-", 7)) {
			scan_req.ssid_list[0].max_len = 0xfe;
		}
	}
#endif
#endif
	for (i = 0;
	     i < MIN(WLAN_USER_SCAN_CHAN_MAX, priv->scan_request->n_channels);
	     i++) {
		chan = priv->scan_request->channels[i];
		scan_req.chan_list[i].chan_number = chan->hw_value;
		scan_req.chan_list[i].radio_type = chan->band;
		if (chan->
		    flags & (IEEE80211_CHAN_PASSIVE_SCAN |
			     IEEE80211_CHAN_RADAR))
			scan_req.chan_list[i].scan_type =
				MLAN_SCAN_TYPE_PASSIVE;
		else
			scan_req.chan_list[i].scan_type = MLAN_SCAN_TYPE_ACTIVE;
		scan_req.chan_list[i].scan_time = 0;
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
		if (priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT &&
		    priv->scan_request->n_ssids) {
			if (!memcmp(scan_req.ssid_list[0].ssid, "DIRECT-", 7))
				scan_req.chan_list[i].scan_time =
					MIN_SPECIFIC_SCAN_CHAN_TIME;
		}
#endif
#endif
	}
	if (priv->scan_request->ie && priv->scan_request->ie_len) {
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_mgmt_frame_ie(priv, NULL, 0,
						NULL, 0, NULL, 0,
						(t_u8 *) priv->scan_request->ie,
						priv->scan_request->ie_len,
						MGMT_MASK_PROBE_REQ)) {
			PRINTM(MERROR, "Fail to set scan request IE\n");
			ret = -EFAULT;
			goto done;
		}
	} else {
		/** Clear SCAN IE in Firmware */
		if (priv->probereq_index != MLAN_CUSTOM_IE_AUTO_IDX_MASK)
			woal_cfg80211_mgmt_frame_ie(priv, NULL, 0, NULL, 0,
						    NULL, 0, NULL, 0,
						    MGMT_MASK_PROBE_REQ);
	}
	if (MLAN_STATUS_SUCCESS != woal_do_scan(priv, &scan_req)) {
		PRINTM(MERROR, "woal_do_scan fails!\n");
		ret = -EAGAIN;
		goto done;
	}
done:
	if (ret) {
		cfg80211_scan_done(request, MTRUE);
		priv->scan_request = NULL;
	}
	LEAVE();
	return ret;
}

/**
 * @brief Request the driver to connect to the ESS with
 * the specified parameters from kernel
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param sme             A pointer to cfg80211_connect_params structure
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev,
		      struct cfg80211_connect_params *sme)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	int ret = 0;
	mlan_bss_info bss_info;
	mlan_ssid_bssid ssid_bssid;

	ENTER();

	PRINTM(MINFO, "Received association request on %s\n", dev->name);
#ifdef UAP_CFG80211
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
		LEAVE();
		return 0;
	}
#endif
	if (priv->wdev->iftype != NL80211_IFTYPE_STATION
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37) || defined(COMPAT_WIRELESS)
	    && priv->wdev->iftype != NL80211_IFTYPE_P2P_CLIENT
#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */
		) {
		PRINTM(MERROR, "Received infra assoc request "
		       "when station not in infra mode\n");
		LEAVE();
		return -EINVAL;
	}

	memset(&ssid_bssid, 0, sizeof(ssid_bssid));
	memcpy(&ssid_bssid.ssid.ssid, sme->ssid, sme->ssid_len);
	ssid_bssid.ssid.ssid_len = sme->ssid_len;
	if (sme->bssid)
		memcpy(&ssid_bssid.bssid, sme->bssid, ETH_ALEN);
	if (MTRUE == woal_is_connected(priv, &ssid_bssid)) {
		/* Inform the BSS information to kernel, otherwise * kernel
		   will give a panic after successful assoc */
		woal_inform_bss_from_scan_result(priv, &ssid_bssid,
						 MOAL_IOCTL_WAIT);
		cfg80211_connect_result(priv->netdev, priv->cfg_bssid, NULL, 0,
					NULL, 0, WLAN_STATUS_SUCCESS,
					GFP_KERNEL);
		PRINTM(MMSG, "wlan: already connected to bssid " MACSTR "\n",
		       MAC2STR(priv->cfg_bssid));
		LEAVE();
		return 0;
	}

	/** cancel pending scan */
	woal_cancel_scan(priv, MOAL_IOCTL_WAIT);
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	if (priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT
	    && (priv->wdev->iftype == NL80211_IFTYPE_STATION
		|| priv->wdev->iftype == NL80211_IFTYPE_P2P_CLIENT)) {
		/* if bsstype == wifi direct, and iftype == station or p2p
		   client, that means wpa_supplicant wants to enable wifi
		   direct functionality, so we should init p2p client. Note
		   that due to kernel iftype check, ICS wpa_supplicant could
		   not updaet iftype to init p2p client, so we have to done it
		   here. */
		if (MLAN_STATUS_SUCCESS != woal_cfg80211_init_p2p_client(priv)) {
			PRINTM(MERROR,
			       "Init p2p client for wpa_supplicant failed.\n");
			ret = -EFAULT;

			LEAVE();
			return ret;
		}
	}
#endif
#endif

	priv->cfg_connect = MTRUE;
	if (priv->scan_type == MLAN_SCAN_TYPE_PASSIVE)
		woal_set_scan_type(priv, MLAN_SCAN_TYPE_ACTIVE);
	priv->assoc_status = 0;
	ret = woal_cfg80211_assoc(priv, (void *)sme);

	if (priv->scan_type == MLAN_SCAN_TYPE_PASSIVE)
		woal_set_scan_type(priv, MLAN_SCAN_TYPE_PASSIVE);
	priv->cfg_connect = MFALSE;
	if (!ret) {
		cfg80211_connect_result(priv->netdev, priv->cfg_bssid, NULL, 0,
					NULL, 0, WLAN_STATUS_SUCCESS,
					GFP_KERNEL);
		PRINTM(MMSG,
		       "wlan: Connected to bssid " MACSTR " successfully\n",
		       MAC2STR(priv->cfg_bssid));
		priv->rssi_low = DEFAULT_RSSI_LOW_THRESHOLD;
		if (priv->bss_type == MLAN_BSS_TYPE_STA)
			woal_save_conn_params(priv, sme);
		memset(&bss_info, 0, sizeof(bss_info));
		woal_get_bss_info(priv, MOAL_IOCTL_WAIT, &bss_info);
		priv->channel = bss_info.bss_chan;

	} else {
		PRINTM(MINFO, "wlan: Failed to connect to bssid " MACSTR "\n",
		       MAC2STR(priv->cfg_bssid));
		cfg80211_connect_result(priv->netdev, priv->cfg_bssid, NULL, 0,
					NULL, 0, woal_get_assoc_status(priv),
					GFP_KERNEL);
		memset(priv->cfg_bssid, 0, ETH_ALEN);
	}

	LEAVE();
	return 0;
}

/**
 * @brief Request the driver to disconnect
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param reason_code     Reason code
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_disconnect(struct wiphy *wiphy, struct net_device *dev,
			 t_u16 reason_code)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);

	ENTER();
	PRINTM(MINFO, "Received disassociation request on %s\n", dev->name);
#ifdef UAP_CFG80211
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
		LEAVE();
		return 0;
	}
#endif
	if (priv->phandle->driver_state) {
		PRINTM(MERROR,
		       "Block woal_cfg80211_disconnect in abnormal driver state\n");
		LEAVE();
		return 0;
	}

	if (priv->cfg_disconnect) {
		PRINTM(MERROR, "Disassociation already in progress\n");
		LEAVE();
		return -EBUSY;
	}

	if (priv->media_connected == MFALSE) {
		LEAVE();
		return -EINVAL;
	}

	priv->cfg_disconnect = 1;

	if (woal_disconnect(priv, MOAL_IOCTL_WAIT, priv->cfg_bssid) !=
	    MLAN_STATUS_SUCCESS) {
		LEAVE();
		return -EFAULT;
	}

	PRINTM(MINFO,
	       "Successfully disconnected from " MACSTR ": Reason code %d\n",
	       MAC2STR(priv->cfg_bssid), reason_code);

	memset(priv->cfg_bssid, 0, ETH_ALEN);
	if (priv->bss_type == MLAN_BSS_TYPE_STA)
		woal_clear_conn_params(priv);
	priv->channel = 0;

	LEAVE();
	return 0;
}

/**
 * @brief Request the driver to get the station information
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param mac             MAC address of the station
 * @param sinfo           A pointer to station_info structure
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_get_station(struct wiphy *wiphy,
			  struct net_device *dev,
			  t_u8 * mac, struct station_info *sinfo)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);

	ENTER();

#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
		LEAVE();
		return woal_uap_cfg80211_get_station(wiphy, dev, mac, sinfo);
	}
#endif
#endif
	if (priv->media_connected == MFALSE) {
		PRINTM(MINFO, "cfg80211: Media not connected!\n");
		LEAVE();
		return -ENOENT;
	}
	if (memcmp(mac, priv->cfg_bssid, ETH_ALEN)) {
		PRINTM(MINFO, "cfg80211: Request not for this station!\n");
		LEAVE();
		return -ENOENT;
	}

	if (MLAN_STATUS_SUCCESS != woal_cfg80211_dump_station_info(priv, sinfo)) {
		PRINTM(MERROR, "cfg80211: Failed to get station info\n");
		ret = -EFAULT;
	}

	LEAVE();
	return ret;
}

/**
 * @brief Request the driver to dump the station information
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param idx             Station index
 * @param mac             MAC address of the station
 * @param sinfo           A pointer to station_info structure
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_dump_station(struct wiphy *wiphy,
			   struct net_device *dev, int idx,
			   t_u8 * mac, struct station_info *sinfo)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);

	ENTER();

	if (!priv->media_connected || idx != 0) {
		PRINTM(MINFO, "cfg80211: Media not connected or"
		       " not for this station!\n");
		LEAVE();
		return -ENOENT;
	}

	memcpy(mac, priv->cfg_bssid, ETH_ALEN);

	if (MLAN_STATUS_SUCCESS != woal_cfg80211_dump_station_info(priv, sinfo)) {
		PRINTM(MERROR, "cfg80211: Failed to get station info\n");
		ret = -EFAULT;
	}

	LEAVE();
	return ret;
}

/**
 * @brief Request the driver to dump survey info
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param idx             Station index
 * @param survey          A pointer to survey_info structure
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_dump_survey(struct wiphy *wiphy, struct net_device *dev,
			  int idx, struct survey_info *survey)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	mlan_bss_info bss_info;
	enum ieee80211_band band;
	ENTER();
#ifdef UAP_CFG80211
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
		LEAVE();
		return -EFAULT;
	}
#endif
	if (!priv->media_connected || idx != 0) {
		PRINTM(MINFO, "cfg80211: Media not connected or"
		       " not for this station!\n");
		LEAVE();
		return -ENOENT;
	}

	memset(&bss_info, 0, sizeof(bss_info));
	if (MLAN_STATUS_SUCCESS !=
	    woal_get_bss_info(priv, MOAL_IOCTL_WAIT, &bss_info)) {
		ret = -EFAULT;
		goto done;
	}
	band = woal_band_cfg_to_ieee_band(bss_info.bss_band);
	survey->channel =
		ieee80211_get_channel(wiphy,
				      ieee80211_channel_to_frequency(bss_info.
								     bss_chan,
								     band));

	if (bss_info.bcn_nf_last) {
		survey->filled = SURVEY_INFO_NOISE_DBM;
		survey->noise = bss_info.bcn_nf_last;
	}
done:
	LEAVE();
	return ret;
}

/**
 * @brief Request the driver to Join the specified
 * IBSS (or create if necessary)
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param params          A pointer to cfg80211_ibss_params structure
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_join_ibss(struct wiphy *wiphy, struct net_device *dev,
			struct cfg80211_ibss_params *params)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	int ret = 0;

	ENTER();

	if (priv->wdev->iftype != NL80211_IFTYPE_ADHOC) {
		PRINTM(MERROR, "Request IBSS join received "
		       "when station not in ibss mode\n");
		LEAVE();
		return -EINVAL;
	}

	ret = woal_cfg80211_assoc(priv, (void *)params);

	if (!ret) {
		cfg80211_ibss_joined(priv->netdev, priv->cfg_bssid, GFP_KERNEL);
		PRINTM(MINFO, "Joined/created adhoc network with bssid"
		       MACSTR " successfully\n", MAC2STR(priv->cfg_bssid));
	} else {
		PRINTM(MINFO, "Failed creating/joining adhoc network\n");
		memset(priv->cfg_bssid, 0, ETH_ALEN);
	}

	LEAVE();
	return ret;
}

/**
 * @brief Request the driver to leave the IBSS
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_leave_ibss(struct wiphy *wiphy, struct net_device *dev)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);

	ENTER();

	if (priv->cfg_disconnect) {
		PRINTM(MERROR, "IBSS leave already in progress\n");
		LEAVE();
		return -EBUSY;
	}

	if (priv->media_connected == MFALSE) {
		LEAVE();
		return -EINVAL;
	}

	priv->cfg_disconnect = 1;

	PRINTM(MINFO, "Leaving from IBSS " MACSTR "\n",
	       MAC2STR(priv->cfg_bssid));
	if (woal_disconnect(priv, MOAL_IOCTL_WAIT, priv->cfg_bssid) !=
	    MLAN_STATUS_SUCCESS) {
		LEAVE();
		return -EFAULT;
	}

	memset(priv->cfg_bssid, 0, ETH_ALEN);

	LEAVE();
	return 0;
}

/**
 * @brief Request the driver to change the IEEE power save
 * mdoe
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param enabled         Enable or disable
 * @param timeout         Timeout value
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_set_power_mgmt(struct wiphy *wiphy,
			     struct net_device *dev, bool enabled, int timeout)
{
	int ret = 0, disabled;
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);

	ENTER();
	if (hw_test) {
		PRINTM(MIOCTL, "block set power in hw_test mode\n");
		LEAVE();
		return ret;
	}
	if (priv->phandle->driver_state) {
		PRINTM(MERROR,
		       "Block woal_cfg80211_set_power_mgmt in abnormal driver state\n");
		LEAVE();
		return 0;
	}
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	if (priv->bss_type == MLAN_BSS_TYPE_WIFIDIRECT) {
		PRINTM(MIOCTL, "skip set power for p2p interface\n");
		LEAVE();
		return ret;
	}
#endif
#endif
	if (enabled)
		disabled = 0;
	else
		disabled = 1;

	if (MLAN_STATUS_SUCCESS !=
	    woal_set_get_power_mgmt(priv, MLAN_ACT_SET, &disabled, timeout)) {
		ret = -EOPNOTSUPP;
	}

	LEAVE();
	return ret;
}

/**
 * @brief Request the driver to change the transmit power
 *
 * @param wiphy           A pointer to wiphy structure
 * @param type            TX power adjustment type
 * @param dbm             TX power in dbm
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_set_tx_power(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
			   struct wireless_dev *wdev,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36) && !defined(COMPAT_WIRELESS)
			   enum tx_power_setting type,
#else
			   enum nl80211_tx_power_setting type,
#endif
			   int dbm)
{
	int ret = 0;
	moal_private *priv = NULL;
	moal_handle *handle = (moal_handle *) woal_get_wiphy_priv(wiphy);
	mlan_power_cfg_t power_cfg;

	ENTER();

	priv = woal_get_priv(handle, MLAN_BSS_ROLE_ANY);
	if (!priv) {
		PRINTM(MFATAL, "Unable to get priv in %s()\n", __FUNCTION__);
		LEAVE();
		return -EFAULT;
	}

	if (type) {
		power_cfg.is_power_auto = 0;
		power_cfg.power_level = dbm;
	} else
		power_cfg.is_power_auto = 1;

	if (MLAN_STATUS_SUCCESS !=
	    woal_set_get_tx_power(priv, MLAN_ACT_SET, &power_cfg))
		ret = -EFAULT;

	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35) || defined(COMPAT_WIRELESS)
/**
 * CFG802.11 operation handler for connection quality monitoring.
 *
 * @param wiphy           A pointer to wiphy structure
 * @param dev             A pointer to net_device structure
 * @param rssi_thold	  rssi threshold
 * @param rssi_hyst		  rssi hysteresis
 */
static int
woal_cfg80211_set_cqm_rssi_config(struct wiphy *wiphy,
				  struct net_device *dev,
				  s32 rssi_thold, u32 rssi_hyst)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	ENTER();
	priv->cqm_rssi_thold = rssi_thold;
	priv->cqm_rssi_hyst = rssi_hyst;

	PRINTM(MIOCTL, "rssi_thold=%d rssi_hyst=%d\n",
	       (int)rssi_thold, (int)rssi_hyst);
	woal_set_rssi_threshold(priv, 0, MOAL_IOCTL_WAIT);
	LEAVE();
	return 0;
}
#endif

#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
/**
 * @brief remain on channel config
 *
 * @param priv              A pointer to moal_private structure
 * @param wait_option       Wait option
 * @param cancel			cancel remain on channel flag
 * @param status            A pointer to status, success, in process or reject
 * @param chan              A pointer to ieee80211_channel structure
 * @param channel_type      channel_type,
 * @param duration          Duration wait to receive frame
 *
 * @return                  0 -- success, otherwise fail
 */
int
woal_cfg80211_remain_on_channel_cfg(moal_private * priv,
				    t_u8 wait_option, t_u8 remove,
				    t_u8 * status,
				    struct ieee80211_channel *chan,
				    enum nl80211_channel_type channel_type,
				    t_u32 duration)
{
	mlan_ds_remain_chan chan_cfg;
	int ret = 0;

	ENTER();

	if (!status || (!chan && !remove)) {
		LEAVE();
		return -EFAULT;
	}
	memset(&chan_cfg, 0, sizeof(mlan_ds_remain_chan));
	if (remove) {
		chan_cfg.remove = MTRUE;
	} else {
		if (priv->phandle->is_go_timer_set) {
			PRINTM(MINFO,
			       "block remain on channel while go timer is on\n");
			LEAVE();
			return -EBUSY;
		}
		if (chan->band == IEEE80211_BAND_2GHZ)
			chan_cfg.bandcfg = 0;
		else if (chan->band == IEEE80211_BAND_5GHZ)
			chan_cfg.bandcfg = 1;
		switch (channel_type) {
		case NL80211_CHAN_HT40MINUS:
			chan_cfg.bandcfg |= SEC_CHANNEL_BELOW;
			break;
		case NL80211_CHAN_HT40PLUS:
			chan_cfg.bandcfg |= SEC_CHANNEL_ABOVE;
			break;

		case NL80211_CHAN_NO_HT:
		case NL80211_CHAN_HT20:
		default:
			break;
		}
		chan_cfg.channel =
			ieee80211_frequency_to_channel(chan->center_freq);
		chan_cfg.remain_period = duration;
	}
	if (MLAN_STATUS_SUCCESS ==
	    woal_set_remain_channel_ioctl(priv, wait_option, &chan_cfg))
		*status = chan_cfg.status;
	else
		ret = -EFAULT;
	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
/**
 * @brief tx mgmt frame
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param wdev                  A pointer to wireless_dev structure
 * @param cookie                A pointer to frame cookie
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
				  struct wireless_dev *wdev, u64 cookie)
#else
/**
 * @brief tx mgmt frame
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param dev                   A pointer to net_device structure
 * @param cookie                A pointer to frame cookie
 *
 * @return                0 -- success, otherwise fail
 */
static int
woal_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
				  struct net_device *dev, u64 cookie)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	struct net_device *dev = wdev->netdev;
#endif
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	int ret = 0;
	t_u8 status = 1;
	moal_private *remain_priv = NULL;

	ENTER();

	if (priv->phandle->remain_on_channel) {
		remain_priv =
			priv->phandle->priv[priv->phandle->remain_bss_index];
		if (!remain_priv) {
			PRINTM(MERROR,
			       "mgmt_tx_cancel_wait: Wrong remain_bss_index=%d\n",
			       priv->phandle->remain_bss_index);
			ret = -EFAULT;
			goto done;
		}
		if (woal_cfg80211_remain_on_channel_cfg
		    (remain_priv, MOAL_IOCTL_WAIT, MTRUE, &status, NULL, 0,
		     0)) {
			PRINTM(MERROR,
			       "mgmt_tx_cancel_wait: Fail to cancel remain on channel\n");
			ret = -EFAULT;
			goto done;
		}
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
								  phandle->chan,
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

done:
	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
/**
 * @brief Make chip remain on channel
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param wdev                  A pointer to wireless_dev structure
 * @param chan                  A pointer to ieee80211_channel structure
 * @param channel_type          Channel type
 * @param duration              Duration for timer
 * @param cookie                A pointer to timer cookie
 *
 * @return                  0 -- success, otherwise fail
 */
static int
woal_cfg80211_remain_on_channel(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				struct ieee80211_channel *chan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
				enum nl80211_channel_type channel_type,
#endif
				unsigned int duration, u64 * cookie)
#else
/**
 * @brief Make chip remain on channel
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param dev                   A pointer to net_device structure
 * @param chan                  A pointer to ieee80211_channel structure
 * @param channel_type          Channel type
 * @param duration              Duration for timer
 * @param cookie                A pointer to timer cookie
 *
 * @return                  0 -- success, otherwise fail
 */
static int
woal_cfg80211_remain_on_channel(struct wiphy *wiphy,
				struct net_device *dev,
				struct ieee80211_channel *chan,
				enum nl80211_channel_type channel_type,
				unsigned int duration, u64 * cookie)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	struct net_device *dev = wdev->netdev;
#endif
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	int ret = 0;
	t_u8 status = 1;
	moal_private *remain_priv = NULL;

	ENTER();

	if (!chan || !cookie) {
		PRINTM(MERROR, "Invalid parameter for remain on channel\n");
		ret = -EFAULT;
		goto done;
	}
	/** cancel previous remain on channel */
	if (priv->phandle->remain_on_channel) {
		remain_priv =
			priv->phandle->priv[priv->phandle->remain_bss_index];
		if (!remain_priv) {
			PRINTM(MERROR,
			       "remain_on_channel: Wrong remain_bss_index=%d\n",
			       priv->phandle->remain_bss_index);
			ret = -EFAULT;
			goto done;
		}
		if (woal_cfg80211_remain_on_channel_cfg
		    (remain_priv, MOAL_IOCTL_WAIT, MTRUE, &status, NULL, 0,
		     0)) {
			PRINTM(MERROR,
			       "remain_on_channel: Fail to cancel remain on channel\n");
			ret = -EFAULT;
			goto done;
		}
		priv->phandle->cookie = 0;
		priv->phandle->remain_on_channel = MFALSE;
	}
	/** cancel pending scan */
	woal_cancel_scan(priv, MOAL_IOCTL_WAIT);
	if (MLAN_STATUS_SUCCESS !=
	    woal_cfg80211_remain_on_channel_cfg(priv, MOAL_IOCTL_WAIT,
						MFALSE, &status, chan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
						channel_type,
#else
						0,
#endif
						(t_u32) duration)) {
		ret = -EFAULT;
		goto done;
	}

	if (status == 0) {
		/* remain on channel operation success */
		/* we need update the value cookie */
		*cookie = (u64) random32() | 1;
		priv->phandle->remain_on_channel = MTRUE;
		priv->phandle->remain_bss_index = priv->bss_index;
		priv->phandle->cookie = *cookie;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
		priv->phandle->channel_type = channel_type;
#endif
		memcpy(&priv->phandle->chan, chan,
		       sizeof(struct ieee80211_channel));
		cfg80211_ready_on_channel(
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
						 dev,
#else
						 priv->wdev,
#endif
						 *cookie, chan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
						 channel_type,
#endif
						 duration, GFP_KERNEL);
		PRINTM(MIOCTL,
		       "%s: Set remain on Channel: channel=%d cookie = %#llx\n",
		       dev->name,
		       ieee80211_frequency_to_channel(chan->center_freq),
		       priv->phandle->cookie);
	}
done:
	LEAVE();
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
/**
 * @brief Cancel remain on channel
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param wdev                  A pointer to wireless_dev structure
 * @param cookie                A pointer to timer cookie
 *
 * @return                  0 -- success, otherwise fail
 */
static int
woal_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
				       struct wireless_dev *wdev, u64 cookie)
#else
/**
 * @brief Cancel remain on channel
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param dev                   A pointer to net_device structure
 * @param cookie                A pointer to timer cookie
 *
 * @return                  0 -- success, otherwise fail
 */
static int
woal_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
				       struct net_device *dev, u64 cookie)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
	struct net_device *dev = wdev->netdev;
#endif
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	moal_private *remain_priv = NULL;
	int ret = 0;
	t_u8 status = 1;

	ENTER();
	PRINTM(MIOCTL, "Cancel remain on Channel: cookie = %#llx\n", cookie);
	remain_priv = priv->phandle->priv[priv->phandle->remain_bss_index];
	if (!remain_priv) {
		PRINTM(MERROR,
		       "cancel_remain_on_channel: Wrong remain_bss_index=%d\n",
		       priv->phandle->remain_bss_index);
		ret = -EFAULT;
		goto done;
	}
	if (woal_cfg80211_remain_on_channel_cfg
	    (remain_priv, MOAL_IOCTL_WAIT, MTRUE, &status, NULL, 0, 0)) {
		PRINTM(MERROR,
		       "cancel_remain_on_channel: Fail to cancel remain on channel\n");
		ret = -EFAULT;
		goto done;
	}

	priv->phandle->remain_on_channel = MFALSE;
	if (priv->phandle->cookie)
		priv->phandle->cookie = 0;
done:
	LEAVE();
	return ret;
}
#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) || defined(COMPAT_WIRELESS)
/**
 * @brief start sched scan
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param dev                   A pointer to net_device structure
 * @param request               A pointer to struct cfg80211_sched_scan_request
 *
 * @return                  0 -- success, otherwise fail
 */
int
woal_cfg80211_sched_scan_start(struct wiphy *wiphy,
			       struct net_device *dev,
			       struct cfg80211_sched_scan_request *request)
{
	struct ieee80211_channel *chan = NULL;
	int i = 0;
	int ret = 0;
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	struct cfg80211_ssid *ssid = NULL;
	ENTER();
#ifdef UAP_CFG80211
	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP) {
		LEAVE();
		return -EFAULT;
	}
#endif

	memset(&priv->scan_cfg, 0, sizeof(priv->scan_cfg));
	if ((!request || !request->n_ssids || !request->n_match_sets)) {
		PRINTM(MERROR, "Invalid sched_scan req parameter\n");
		LEAVE();
		return -EINVAL;
	}
	PRINTM(MIOCTL,
	       "%s sched scan: n_ssids=%d n_match_sets=%d n_channels=%d interval=%d ie_len=%d\n",
	       priv->netdev->name, request->n_ssids, request->n_match_sets,
	       request->n_channels, request->interval, request->ie_len);
	/** cancel pending scan */
	woal_cancel_scan(priv, MOAL_IOCTL_WAIT);
	for (i = 0; i < request->n_match_sets; i++) {
		ssid = &request->match_sets[i].ssid;
		strncpy(priv->scan_cfg.ssid_list[i].ssid, ssid->ssid,
			ssid->ssid_len);
		priv->scan_cfg.ssid_list[i].max_len = 0;
		PRINTM(MIOCTL, "sched scan: ssid=%s\n", ssid->ssid);
	}
	for (i = 0; i < MIN(WLAN_BG_SCAN_CHAN_MAX, request->n_channels); i++) {
		chan = request->channels[i];
		priv->scan_cfg.chan_list[i].chan_number = chan->hw_value;
		priv->scan_cfg.chan_list[i].radio_type = chan->band;
		if (chan->
		    flags & (IEEE80211_CHAN_PASSIVE_SCAN |
			     IEEE80211_CHAN_RADAR))
			priv->scan_cfg.chan_list[i].scan_type =
				MLAN_SCAN_TYPE_PASSIVE;
		else
			priv->scan_cfg.chan_list[i].scan_type =
				MLAN_SCAN_TYPE_ACTIVE;
		priv->scan_cfg.chan_list[i].scan_time = 0;
	}

	/** set scan request IES */
	if (request->ie && request->ie_len) {
		if (MLAN_STATUS_SUCCESS !=
		    woal_cfg80211_mgmt_frame_ie(priv, NULL, 0,
						NULL, 0, NULL, 0,
						(t_u8 *) request->ie,
						request->ie_len,
						MGMT_MASK_PROBE_REQ)) {
			PRINTM(MERROR, "Fail to set sched scan IE\n");
			ret = -EFAULT;
			goto done;
		}
	} else {
		/** Clear SCAN IE in Firmware */
		if (priv->probereq_index != MLAN_CUSTOM_IE_AUTO_IDX_MASK)
			woal_cfg80211_mgmt_frame_ie(priv, NULL, 0, NULL, 0,
						    NULL, 0, NULL, 0,
						    MGMT_MASK_PROBE_REQ);
	}

	/* Interval between scan cycles in milliseconds,supplicant set to 10
	   second */
	/* We want to use 30 second for per scan cycle */
	priv->scan_cfg.scan_interval = MIN_BGSCAN_INTERVAL;
	if (request->interval > MIN_BGSCAN_INTERVAL)
		priv->scan_cfg.scan_interval = request->interval;

	priv->scan_cfg.repeat_count = DEF_REPEAT_COUNT;
	priv->scan_cfg.report_condition = BG_SCAN_SSID_MATCH;
	priv->scan_cfg.bss_type = MLAN_BSS_MODE_INFRA;
	priv->scan_cfg.action = BG_SCAN_ACT_SET;
	priv->scan_cfg.enable = MTRUE;

	if (MLAN_STATUS_SUCCESS ==
	    woal_request_bgscan(priv, MOAL_IOCTL_WAIT, &priv->scan_cfg)) {
		priv->sched_scanning = MTRUE;
		priv->bg_scan_start = MTRUE;
		priv->bg_scan_reported = MFALSE;
	} else
		ret = -EFAULT;
done:
	LEAVE();
	return ret;
}

/**
 * @brief stop sched scan
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param dev                   A pointer to net_device structure
 *
 * @return                  	0 -- success, otherwise fail
 */
int
woal_cfg80211_sched_scan_stop(struct wiphy *wiphy, struct net_device *dev)
{
	moal_private *priv = (moal_private *) woal_get_netdev_priv(dev);
	ENTER();
	PRINTM(MIOCTL, "sched scan stop\n");
	priv->sched_scanning = MFALSE;
	woal_stop_bg_scan(priv, MOAL_NO_WAIT);
	priv->bg_scan_start = MFALSE;
	priv->bg_scan_reported = MFALSE;
	LEAVE();
	return 0;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) || defined(COMPAT_WIRELESS)
/**
 * @brief cfg80211_resume handler
 *
 * @param wiphy                 A pointer to wiphy structure
 *
 * @return                  	0 -- success, otherwise fail
 */
int
woal_cfg80211_resume(struct wiphy *wiphy)
{
	moal_handle *handle = (moal_handle *) woal_get_wiphy_priv(wiphy);
	int i;
	for (i = 0; i < MIN(handle->priv_num, MLAN_MAX_BSS_NUM); i++) {
		if (handle->priv[i] &&
		    (GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_STA)) {
			if ((handle->priv[i]->last_event & EVENT_BG_SCAN_REPORT)
			    && handle->priv[i]->sched_scanning) {
				woal_inform_bss_from_scan_result(handle->
								 priv[i], NULL,
								 MOAL_CMD_WAIT);
				cfg80211_sched_scan_results(handle->priv[i]->
							    wdev->wiphy);
				handle->priv[i]->last_event = 0;
				PRINTM(MIOCTL,
				       "Report sched scan result in cfg80211 resume\n");
			}
		}
	}
	handle->cfg80211_suspend = MFALSE;
	PRINTM(MIOCTL, "woal_cfg80211_resume\n");
	return 0;
}

/**
 * @brief cfg80211_suspend handler
 *
 * @param wiphy                 A pointer to wiphy structure
 * @param wow 					A pointer to cfg80211_wowlan
 *
 * @return                  	0 -- success, otherwise fail
 */
int
woal_cfg80211_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow)
{
	moal_handle *handle = (moal_handle *) woal_get_wiphy_priv(wiphy);
	int i;
	for (i = 0; i < MIN(handle->priv_num, MLAN_MAX_BSS_NUM); i++) {
		if (handle->priv[i] &&
		    (GET_BSS_ROLE(handle->priv[i]) == MLAN_BSS_ROLE_STA)) {
			if (handle->priv[i]->scan_request) {
				PRINTM(MIOCTL,
				       "Cancel pending scan in woal_cfg80211_suspend\n");
				woal_cancel_scan(handle->priv[i],
						 MOAL_IOCTL_WAIT);
			}
			handle->priv[i]->last_event = 0;
		}
	}
	PRINTM(MIOCTL, "woal_cfg80211_suspended\n");
	handle->cfg80211_suspend = MTRUE;
	return 0;
}
#endif

/**
 * @brief Save connect parameters for roaming
 *
 * @param priv            A pointer to moal_private
 * @param sme             A pointer to cfg80211_connect_params structure
 */
void
woal_save_conn_params(moal_private * priv, struct cfg80211_connect_params *sme)
{
	ENTER();
	memcpy(&priv->sme_current, sme, sizeof(struct cfg80211_connect_params));
	if (sme->channel) {
		priv->sme_current.channel = &priv->conn_chan;
		memcpy(priv->sme_current.channel, sme->channel,
		       sizeof(struct ieee80211_channel));
	}
	if (sme->bssid) {
		priv->sme_current.bssid = priv->conn_bssid;
		memcpy(priv->sme_current.bssid, sme->bssid,
		       MLAN_MAC_ADDR_LENGTH);
	}
	if (sme->ssid && sme->ssid_len) {
		priv->sme_current.ssid = priv->conn_ssid;
		memset(priv->conn_ssid, 0, MLAN_MAX_SSID_LENGTH);
		memcpy(priv->sme_current.ssid, sme->ssid, sme->ssid_len);
	}
	if (sme->ie && sme->ie_len) {
		priv->sme_current.ie = kzalloc(sme->ie_len, GFP_KERNEL);
		memcpy(priv->sme_current.ie, sme->ie, sme->ie_len);
	}
	if (sme->key && sme->key_len && (sme->key_len <= MAX_WEP_KEY_SIZE)) {
		priv->sme_current.key = priv->conn_wep_key;
		memcpy((t_u8 *) priv->sme_current.key, sme->key, sme->key_len);
	}
}

/**
 * @brief clear connect parameters for ing
 *
 * @param priv            A pointer to moal_private
 */
void
woal_clear_conn_params(moal_private * priv)
{
	ENTER();
	if (priv->sme_current.ie_len && priv->sme_current.ie)
		kfree(priv->sme_current.ie);
	memset(&priv->sme_current, 0, sizeof(struct cfg80211_connect_params));
	priv->roaming_required = MFALSE;
	LEAVE();
}

/**
 * @brief Start roaming: driver handle roaming
 *
 * @param priv      A pointer to moal_private structure
 *
 * @return          N/A
 */
void
woal_start_roaming(moal_private * priv)
{
	mlan_ds_get_signal signal;
	mlan_ssid_bssid ssid_bssid;
	char rssi_low[10];
	int ret = 0;
	mlan_ds_misc_assoc_rsp assoc_rsp;
	IEEEtypes_AssocRsp_t *passoc_rsp = NULL;

	ENTER();
	if (priv->last_event & EVENT_BG_SCAN_REPORT) {
		woal_inform_bss_from_scan_result(priv, NULL, MOAL_CMD_WAIT);
		PRINTM(MIOCTL, "Report bgscan result\n");
	}
	if (priv->media_connected == MFALSE || !priv->sme_current.ssid_len) {
		PRINTM(MIOCTL, "Not connected, ignore roaming\n");
		LEAVE();
		return;
	}

	/* Get signal information from the firmware */
	memset(&signal, 0, sizeof(mlan_ds_get_signal));
	if (MLAN_STATUS_SUCCESS !=
	    woal_get_signal_info(priv, MOAL_CMD_WAIT, &signal)) {
		PRINTM(MERROR, "Error getting signal information\n");
		ret = -EFAULT;
		goto done;
	}
	memset(&ssid_bssid, 0, sizeof(mlan_ssid_bssid));
	ssid_bssid.ssid.ssid_len = priv->sme_current.ssid_len;
	memcpy(ssid_bssid.ssid.ssid, priv->sme_current.ssid,
	       priv->sme_current.ssid_len);
	if (MLAN_STATUS_SUCCESS !=
	    woal_find_best_network(priv, MOAL_IOCTL_WAIT, &ssid_bssid)) {
		PRINTM(MIOCTL, "Can not find better network\n");
		ret = -EFAULT;
		goto done;
	}
	/* check if we found different AP */
	if (!memcmp(&ssid_bssid.bssid, priv->cfg_bssid, MLAN_MAC_ADDR_LENGTH)) {
		PRINTM(MIOCTL, "This is the same AP, no roaming\n");
		ret = -EFAULT;
		goto done;
	}
	PRINTM(MIOCTL, "Find AP: bssid=" MACSTR ", signal=%d\n",
	       MAC2STR(ssid_bssid.bssid), ssid_bssid.rssi);
	/* check signal */
	if (!(priv->last_event & EVENT_PRE_BCN_LOST)) {
		if ((abs(signal.bcn_rssi_avg) - abs(ssid_bssid.rssi)) <
		    DELTA_RSSI) {
			PRINTM(MERROR, "New AP's signal is not good too.\n");
			ret = -EFAULT;
			goto done;
		}
	}
	/* start roaming to new AP */
	priv->sme_current.bssid = priv->conn_bssid;
	memcpy(priv->sme_current.bssid, &ssid_bssid.bssid,
	       MLAN_MAC_ADDR_LENGTH);
	ret = woal_cfg80211_assoc(priv, (void *)&priv->sme_current);
	if (!ret) {
		woal_inform_bss_from_scan_result(priv, NULL, MOAL_CMD_WAIT);
		memset(&assoc_rsp, 0, sizeof(mlan_ds_misc_assoc_rsp));
		woal_get_assoc_rsp(priv, &assoc_rsp);
		passoc_rsp = (IEEEtypes_AssocRsp_t *) assoc_rsp.assoc_resp_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0) || defined(COMPAT_WIRELESS)
		cfg80211_roamed(priv->netdev, NULL, priv->cfg_bssid,
				priv->sme_current.ie, priv->sme_current.ie_len,
				passoc_rsp->ie_buffer,
				assoc_rsp.assoc_resp_len -
				ASSOC_RESP_FIXED_SIZE, GFP_KERNEL);
#else
		cfg80211_roamed(priv->netdev, priv->cfg_bssid,
				priv->sme_current.ie, priv->sme_current.ie_len,
				passoc_rsp->ie_buffer,
				assoc_rsp.assoc_resp_len -
				ASSOC_RESP_FIXED_SIZE, GFP_KERNEL);
#endif
		PRINTM(MIOCTL, "Roamed to bssid " MACSTR " successfully\n",
		       MAC2STR(priv->cfg_bssid));
	} else {
		PRINTM(MIOCTL, "Roaming to bssid " MACSTR " failed\n",
		       MAC2STR(ssid_bssid.bssid));
	}
done:
	/* config rssi low threshold again */
	priv->last_event = 0;
	priv->rssi_low = DEFAULT_RSSI_LOW_THRESHOLD;
	sprintf(rssi_low, "%d", priv->rssi_low);
	woal_set_rssi_low_threshold(priv, rssi_low, MOAL_CMD_WAIT);
	LEAVE();
	return;
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
woal_register_sta_cfg80211(struct net_device * dev, t_u8 bss_type)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	moal_private *priv = (moal_private *) netdev_priv(dev);
	struct wireless_dev *wdev = NULL;
	int disabled = 0;

	ENTER();

	wdev = (struct wireless_dev *)&priv->w_dev;
	memset(wdev, 0, sizeof(struct wireless_dev));
	wdev->wiphy = priv->phandle->wiphy;
	if (!wdev->wiphy) {
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	if (bss_type == MLAN_BSS_TYPE_STA) {
		wdev->iftype = NL80211_IFTYPE_STATION;
		priv->roaming_enabled = MFALSE;
		priv->roaming_required = MFALSE;
	}
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	if (bss_type == MLAN_BSS_TYPE_WIFIDIRECT)
		wdev->iftype = NL80211_IFTYPE_STATION;
#endif
#endif

	dev_net_set(dev, wiphy_net(wdev->wiphy));
	dev->ieee80211_ptr = wdev;
	SET_NETDEV_DEV(dev, wiphy_dev(wdev->wiphy));
	priv->wdev = wdev;
	/* Get IEEE power save mode */
	if (MLAN_STATUS_SUCCESS ==
	    woal_set_get_power_mgmt(priv, MLAN_ACT_GET, &disabled, 0)) {
		/* Save the IEEE power save mode to wiphy, because after *
		   warmreset wiphy power save should be updated instead * of
		   using the last saved configuration */
		if (disabled)
			priv->wdev->ps = MFALSE;
		else
			priv->wdev->ps = MTRUE;
	}
	woal_send_domain_info_cmd_fw(priv);
	LEAVE();
	return ret;
}

/**
 * @brief Initialize the wiphy
 *
 * @param priv            A pointer to moal_private structure
 * @param wait_option     Wait option
 *
 * @return                MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
woal_cfg80211_init_wiphy(moal_private * priv, t_u8 wait_option)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	int retry_count, rts_thr, frag_thr;
	struct wiphy *wiphy = NULL;
	mlan_ioctl_req *req = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) || defined(COMPAT_WIRELESS)
	mlan_ds_radio_cfg *radio = NULL;
#endif
	mlan_ds_11n_cfg *cfg_11n = NULL;
	t_u32 hw_dev_cap;

	ENTER();

	wiphy = priv->phandle->wiphy;
	/* Get 11n tx parameters from MLAN */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_11n_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	cfg_11n = (mlan_ds_11n_cfg *) req->pbuf;
	cfg_11n->sub_command = MLAN_OID_11N_HTCAP_CFG;
	req->req_id = MLAN_IOCTL_11N_CFG;
	req->action = MLAN_ACT_GET;
	cfg_11n->param.htcap_cfg.hw_cap_req = MTRUE;

	if (MLAN_STATUS_SUCCESS != woal_request_ioctl(priv, req, wait_option)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	hw_dev_cap = cfg_11n->param.htcap_cfg.htcap;

	/* Get supported MCS sets */
	memset(req->pbuf, 0, sizeof(mlan_ds_11n_cfg));
	cfg_11n->sub_command = MLAN_OID_11N_CFG_SUPPORTED_MCS_SET;
	req->req_id = MLAN_IOCTL_11N_CFG;
	req->action = MLAN_ACT_GET;

	if (MLAN_STATUS_SUCCESS != woal_request_ioctl(priv, req, wait_option)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Initialize parameters for 2GHz and 5GHz bands */
	woal_cfg80211_setup_ht_cap(&wiphy->bands[IEEE80211_BAND_2GHZ]->ht_cap,
				   hw_dev_cap,
				   cfg_11n->param.supported_mcs_set);
	/* For 2.4G band only card, this shouldn't be set */
	if (wiphy->bands[IEEE80211_BAND_5GHZ])
		woal_cfg80211_setup_ht_cap(&wiphy->bands[IEEE80211_BAND_5GHZ]->
					   ht_cap, hw_dev_cap,
					   cfg_11n->param.supported_mcs_set);
	if (req)
		kfree(req);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38) || defined(COMPAT_WIRELESS)
	/* Get antenna modes */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_radio_cfg));
	if (req == NULL) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	radio = (mlan_ds_radio_cfg *) req->pbuf;
	radio->sub_command = MLAN_OID_ANT_CFG;
	req->req_id = MLAN_IOCTL_RADIO_CFG;
	req->action = MLAN_ACT_GET;

	if (MLAN_STATUS_SUCCESS !=
	    woal_request_ioctl(priv, req, MOAL_IOCTL_WAIT)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Set available antennas to wiphy */
	wiphy->available_antennas_tx = radio->param.ant_cfg.tx_antenna;
	wiphy->available_antennas_rx = radio->param.ant_cfg.rx_antenna;
#endif /* LINUX_VERSION_CODE */

	/* Set retry limit count to wiphy */
	if (MLAN_STATUS_SUCCESS !=
	    woal_set_get_retry(priv, MLAN_ACT_GET, wait_option, &retry_count)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	wiphy->retry_long = (t_u8) retry_count;
	wiphy->retry_short = (t_u8) retry_count;
	wiphy->max_scan_ie_len = MAX_IE_SIZE;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37) || defined(COMPAT_WIRELESS)
	wiphy->mgmt_stypes = ieee80211_mgmt_stypes;
#endif
#if defined(WIFI_DIRECT_SUPPORT)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	wiphy->max_remain_on_channel_duration = MAX_REMAIN_ON_CHANNEL_DURATION;
#endif /* KERNEL_VERSION */
#endif /* WIFI_DIRECT_SUPPORT && V14_FEATURE */

	/* Set RTS threshold to wiphy */
	if (MLAN_STATUS_SUCCESS !=
	    woal_set_get_rts(priv, MLAN_ACT_GET, wait_option, &rts_thr)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	if (rts_thr < MLAN_RTS_MIN_VALUE || rts_thr > MLAN_RTS_MAX_VALUE)
		rts_thr = MLAN_FRAG_RTS_DISABLED;
	wiphy->rts_threshold = (t_u32) rts_thr;

	/* Set fragment threshold to wiphy */
	if (MLAN_STATUS_SUCCESS !=
	    woal_set_get_frag(priv, MLAN_ACT_GET, wait_option, &frag_thr)) {
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	if (frag_thr < MLAN_RTS_MIN_VALUE || frag_thr > MLAN_RTS_MAX_VALUE)
		frag_thr = MLAN_FRAG_RTS_DISABLED;
	wiphy->frag_threshold = (t_u32) frag_thr;

done:
	LEAVE();
	if (req)
		kfree(req);
	return ret;
}

/*
 * This function registers the device with CFG802.11 subsystem.
 *
 * @param priv       A pointer to moal_private
 *
 */
mlan_status
woal_register_cfg80211(moal_private * priv)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	struct wiphy *wiphy;
	void *wdev_priv = NULL;
	mlan_fw_info fw_info;
	char *country = NULL;
	int index = 0;

	ENTER();

	wiphy = wiphy_new(&woal_cfg80211_ops, sizeof(moal_handle *));
	if (!wiphy) {
		PRINTM(MERROR, "Could not allocate wiphy device\n");
		ret = MLAN_STATUS_FAILURE;
		goto err_wiphy;
	}
	wiphy->max_scan_ssids = MRVDRV_MAX_SSID_LIST_LENGTH;
	wiphy->max_scan_ie_len = MAX_IE_SIZE;
	wiphy->interface_modes = 0;
	wiphy->interface_modes =
		MBIT(NL80211_IFTYPE_STATION) | MBIT(NL80211_IFTYPE_ADHOC) |
		MBIT(NL80211_IFTYPE_AP);

	/* Make this wiphy known to this driver only */
	wiphy->privid = mrvl_wiphy_privid;
	/* Supported bands */
	wiphy->bands[IEEE80211_BAND_2GHZ] = &cfg80211_band_2ghz;

	woal_request_get_fw_info(priv, MOAL_CMD_WAIT, &fw_info);
	if (fw_info.fw_bands & BAND_A) {
		wiphy->bands[IEEE80211_BAND_5GHZ] = &cfg80211_band_5ghz;
	/** reduce scan time from 110ms to 80ms */
		woal_set_scan_time(priv, INIT_ACTIVE_SCAN_CHAN_TIME,
				   INIT_PASSIVE_SCAN_CHAN_TIME,
				   INIT_SPECIFIC_SCAN_CHAN_TIME);
	} else
		woal_set_scan_time(priv, ACTIVE_SCAN_CHAN_TIME,
				   PASSIVE_SCAN_CHAN_TIME,
				   SPECIFIC_SCAN_CHAN_TIME);
	woal_enable_ext_scan(priv, MTRUE);
	priv->phandle->band = IEEE80211_BAND_2GHZ;

	/* Initialize cipher suits */
	wiphy->cipher_suites = cfg80211_cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(cfg80211_cipher_suites);

	memcpy(wiphy->perm_addr, priv->current_addr, ETH_ALEN);
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

	wiphy->flags = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
	wiphy->flags |=
		WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL | WIPHY_FLAG_OFFCHAN_TX;
	wiphy->flags |=
		WIPHY_FLAG_HAVE_AP_SME | WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD;
#endif
#ifdef ANDROID_KERNEL
	wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) || defined(COMPAT_WIRELESS)
	wiphy->flags |= WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
	wiphy->max_sched_scan_ssids = MRVDRV_MAX_SSID_LIST_LENGTH;
	wiphy->max_sched_scan_ie_len = MAX_IE_SIZE;
	wiphy->max_match_sets = MRVDRV_MAX_SSID_LIST_LENGTH;
#endif

	wiphy->reg_notifier = woal_cfg80211_reg_notifier;

	/* Set struct moal_handle pointer in wiphy_priv */
	wdev_priv = wiphy_priv(wiphy);
	*(unsigned long *)wdev_priv = (unsigned long)priv->phandle;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39) || defined(COMPAT_WIRELESS)
	set_wiphy_dev(wiphy, (struct device *)priv->phandle->hotplug_device);
#endif
	/* Set phy name */
	for (index = 0; index < MAX_MLAN_ADAPTER; index++) {
		if (m_handle[index] == priv->phandle) {
			dev_set_name(&wiphy->dev, "mwiphy%d", index);
			break;
		}
	}
	if (wiphy_register(wiphy) < 0) {
		PRINTM(MERROR, "Wiphy device registration failed!\n");
		ret = MLAN_STATUS_FAILURE;
		goto err_wiphy;
	}
    /** we will try driver parameter first */
	if (reg_alpha2 && woal_is_valid_alpha2(reg_alpha2)) {
		PRINTM(MIOCTL, "Notify reg_alpha2 %c%c\n", reg_alpha2[0],
		       reg_alpha2[1]);
		regulatory_hint(wiphy, reg_alpha2);
	} else {
		country = region_code_2_string(fw_info.region_code);
		if (country) {
			PRINTM(MIOCTL, "Notify hw region code=%d %c%c\n",
			       fw_info.region_code, country[0], country[1]);
			regulatory_hint(wiphy, country);
		} else
			PRINTM(MERROR, "hw region code=%d not supported\n",
			       fw_info.region_code);
	}

	priv->phandle->wiphy = wiphy;
	woal_cfg80211_init_wiphy(priv, MOAL_CMD_WAIT);

	return ret;
err_wiphy:
	if (wiphy)
		wiphy_free(wiphy);
	LEAVE();
	return ret;
}

module_param(reg_alpha2, charp, 0);
MODULE_PARM_DESC(reg_alpha2, "Regulatory alpha2");
