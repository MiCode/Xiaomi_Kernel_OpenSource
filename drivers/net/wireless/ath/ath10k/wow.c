/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "mac.h"

#include <net/mac80211.h>
#include "hif.h"
#include "core.h"
#include "debug.h"
#include "wmi.h"
#include "wmi-ops.h"

static const struct wiphy_wowlan_support ath10k_wowlan_support = {
	.flags = WIPHY_WOWLAN_DISCONNECT |
		 WIPHY_WOWLAN_MAGIC_PKT,
	.pattern_min_len = WOW_MIN_PATTERN_SIZE,
	.pattern_max_len = WOW_MAX_PATTERN_SIZE,
	.max_pkt_offset = WOW_MAX_PKT_OFFSET,
};

static int ath10k_wow_vif_cleanup(struct ath10k_vif *arvif)
{
	struct ath10k *ar = arvif->ar;
	int i, ret;

	for (i = 0; i < WOW_EVENT_MAX; i++) {
		ret = ath10k_wmi_wow_add_wakeup_event(ar, arvif->vdev_id, i, 0);
		if (ret) {
			ath10k_warn(ar, "failed to issue wow wakeup for event %s on vdev %i: %d\n",
				    wow_wakeup_event(i), arvif->vdev_id, ret);
			return ret;
		}
	}

	for (i = 0; i < ar->wow.max_num_patterns; i++) {
		ret = ath10k_wmi_wow_del_pattern(ar, arvif->vdev_id, i);
		if (ret) {
			ath10k_warn(ar, "failed to delete wow pattern %d for vdev %i: %d\n",
				    i, arvif->vdev_id, ret);
			return ret;
		}
	}

	return 0;
}

static int ath10k_wow_cleanup(struct ath10k *ar)
{
	struct ath10k_vif *arvif;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		ret = ath10k_wow_vif_cleanup(arvif);
		if (ret) {
			ath10k_warn(ar, "failed to clean wow wakeups on vdev %i: %d\n",
				    arvif->vdev_id, ret);
			return ret;
		}
	}

	return 0;
}

static int ath10k_vif_wow_set_wakeups(struct ath10k_vif *arvif,
				      struct cfg80211_wowlan *wowlan)
{
	int ret, i;
	unsigned long wow_mask = 0;
	struct ath10k *ar = arvif->ar;
	const struct cfg80211_pkt_pattern *patterns = wowlan->patterns;
	int pattern_id = 0;

	/* Setup requested WOW features */
	switch (arvif->vdev_type) {
	case WMI_VDEV_TYPE_IBSS:
		__set_bit(WOW_BEACON_EVENT, &wow_mask);
		 /* fall through */
	case WMI_VDEV_TYPE_AP:
		__set_bit(WOW_DEAUTH_RECVD_EVENT, &wow_mask);
		__set_bit(WOW_DISASSOC_RECVD_EVENT, &wow_mask);
		__set_bit(WOW_PROBE_REQ_WPS_IE_EVENT, &wow_mask);
		__set_bit(WOW_AUTH_REQ_EVENT, &wow_mask);
		__set_bit(WOW_ASSOC_REQ_EVENT, &wow_mask);
		__set_bit(WOW_HTT_EVENT, &wow_mask);
		__set_bit(WOW_RA_MATCH_EVENT, &wow_mask);
		break;
	case WMI_VDEV_TYPE_STA:
		if (wowlan->disconnect) {
			__set_bit(WOW_DEAUTH_RECVD_EVENT, &wow_mask);
			__set_bit(WOW_DISASSOC_RECVD_EVENT, &wow_mask);
			__set_bit(WOW_BMISS_EVENT, &wow_mask);
			__set_bit(WOW_CSA_IE_EVENT, &wow_mask);
		}

		if (wowlan->magic_pkt)
			__set_bit(WOW_MAGIC_PKT_RECVD_EVENT, &wow_mask);
		break;
	default:
		break;
	}

	for (i = 0; i < wowlan->n_patterns; i++) {
		u8 bitmask[WOW_MAX_PATTERN_SIZE] = {};
		int j;

		if (patterns[i].pattern_len > WOW_MAX_PATTERN_SIZE)
			continue;

		/* convert bytemask to bitmask */
		for (j = 0; j < patterns[i].pattern_len; j++)
			if (patterns[i].mask[j / 8] & BIT(j % 8))
				bitmask[j] = 0xff;

		ret = ath10k_wmi_wow_add_pattern(ar, arvif->vdev_id,
						 pattern_id,
						 patterns[i].pattern,
						 bitmask,
						 patterns[i].pattern_len,
						 patterns[i].pkt_offset);
		if (ret) {
			ath10k_warn(ar, "failed to add pattern %i to vdev %i: %d\n",
				    pattern_id,
				    arvif->vdev_id, ret);
			return ret;
		}

		pattern_id++;
		__set_bit(WOW_PATTERN_MATCH_EVENT, &wow_mask);
	}

	for (i = 0; i < WOW_EVENT_MAX; i++) {
		if (!test_bit(i, &wow_mask))
			continue;
		ret = ath10k_wmi_wow_add_wakeup_event(ar, arvif->vdev_id, i, 1);
		if (ret) {
			ath10k_warn(ar, "failed to enable wakeup event %s on vdev %i: %d\n",
				    wow_wakeup_event(i), arvif->vdev_id, ret);
			return ret;
		}
	}

	return 0;
}

static int ath10k_wow_set_wakeups(struct ath10k *ar,
				  struct cfg80211_wowlan *wowlan)
{
	struct ath10k_vif *arvif;
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	list_for_each_entry(arvif, &ar->arvifs, list) {
		ret = ath10k_vif_wow_set_wakeups(arvif, wowlan);
		if (ret) {
			ath10k_warn(ar, "failed to set wow wakeups on vdev %i: %d\n",
				    arvif->vdev_id, ret);
			return ret;
		}
	}

	return 0;
}

static int ath10k_wow_enable(struct ath10k *ar)
{
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	reinit_completion(&ar->target_suspend);

	ret = ath10k_wmi_wow_enable(ar);
	if (ret) {
		ath10k_warn(ar, "failed to issue wow enable: %d\n", ret);
		return ret;
	}

	ret = wait_for_completion_timeout(&ar->target_suspend, 3 * HZ);
	if (ret == 0) {
		ath10k_warn(ar, "timed out while waiting for suspend completion\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int ath10k_wow_wakeup(struct ath10k *ar)
{
	int ret;

	lockdep_assert_held(&ar->conf_mutex);

	reinit_completion(&ar->wow.wakeup_completed);

	ret = ath10k_wmi_wow_host_wakeup_ind(ar);
	if (ret) {
		ath10k_warn(ar, "failed to send wow wakeup indication: %d\n",
			    ret);
		return ret;
	}

	ret = wait_for_completion_timeout(&ar->wow.wakeup_completed, 3 * HZ);
	if (ret == 0) {
		ath10k_warn(ar, "timed out while waiting for wow wakeup completion\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int
ath10k_wow_fill_vdev_arp_offload_struct(struct ath10k_vif *arvif,
					bool enable_offload)
{
	struct in_device *in_dev;
	struct in_ifaddr *ifa;
	bool offload_params_found = false;
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(arvif->vif);
	struct wmi_ns_arp_offload_req *arp = &arvif->arp_offload;

	if (!enable_offload) {
		arp->offload_type = __cpu_to_le16(WMI_IPV4_ARP_REPLY_OFFLOAD);
		arp->enable_offload = __cpu_to_le16(WMI_ARP_NS_OFFLOAD_DISABLE);
		return 0;
	}

	if (!wdev)
		return -ENODEV;
	if (!wdev->netdev)
		return -ENODEV;
	in_dev = __in_dev_get_rtnl(wdev->netdev);
	if (!in_dev)
		return -ENODEV;

	arp->offload_type = __cpu_to_le16(WMI_IPV4_ARP_REPLY_OFFLOAD);
	arp->enable_offload = __cpu_to_le16(WMI_ARP_NS_OFFLOAD_ENABLE);
	for (ifa = in_dev->ifa_list; ifa; ifa = ifa->ifa_next) {
		if (!memcmp(ifa->ifa_label, wdev->netdev->name, IFNAMSIZ)) {
			offload_params_found = true;
			break;
		}
	}

	if (!offload_params_found)
		return -ENODEV;
	memcpy(&arp->params.ipv4_addr, &ifa->ifa_local,
	       sizeof(arp->params.ipv4_addr));

	return 0;
}

static int ath10k_wow_enable_ns_arp_offload(struct ath10k *ar, bool offload)
{
	struct ath10k_vif *arvif;
	int ret;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->vdev_type != WMI_VDEV_TYPE_STA)
			continue;

		if (!arvif->is_up)
			continue;

		ret = ath10k_wow_fill_vdev_arp_offload_struct(arvif, offload);
		if (ret) {
			ath10k_err(ar, "ARP-offload config failed, vdev: %d\n",
				   arvif->vdev_id);
			return ret;
		}

		ret = ath10k_wmi_set_arp_ns_offload(ar, arvif);
		if (ret) {
			ath10k_err(ar, "failed to send offload cmd, vdev: %d\n",
				   arvif->vdev_id);
			return ret;
		}
	}

	return 0;
}

static int ath10k_config_wow_listen_interval(struct ath10k *ar)
{
	int ret;
	u32 param = ar->wmi.vdev_param->listen_interval;
	u8 listen_interval = ar->hw_values->default_listen_interval;
	struct ath10k_vif *arvif;

	if (!listen_interval)
		return 0;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->vdev_type != WMI_VDEV_TYPE_STA)
			continue;
		ret = ath10k_wmi_vdev_set_param(ar, arvif->vdev_id,
						param, listen_interval);
		if (ret) {
			ath10k_err(ar, "failed to config LI for vdev_id: %d\n",
				   arvif->vdev_id);
			return ret;
		}
	}

	return 0;
}

void ath10k_wow_op_set_rekey_data(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  struct cfg80211_gtk_rekey_data *data)
{
	struct ath10k *ar = hw->priv;
	struct ath10k_vif *arvif = ath10k_vif_to_arvif(vif);

	mutex_lock(&ar->conf_mutex);
	memcpy(&arvif->gtk_rekey_data.kek, data->kek, NL80211_KEK_LEN);
	memcpy(&arvif->gtk_rekey_data.kck, data->kck, NL80211_KCK_LEN);
	arvif->gtk_rekey_data.replay_ctr =
			__cpu_to_le64(*(__le64 *)data->replay_ctr);
	arvif->gtk_rekey_data.valid = true;
	mutex_unlock(&ar->conf_mutex);
}

static int ath10k_wow_config_gtk_offload(struct ath10k *ar, bool gtk_offload)
{
	struct ath10k_vif *arvif;
	struct ieee80211_bss_conf *bss;
	struct wmi_gtk_rekey_data *rekey_data;
	int ret;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->vdev_type != WMI_VDEV_TYPE_STA)
			continue;

		bss = &arvif->vif->bss_conf;
		if (!arvif->is_up || !bss->assoc)
			continue;

		rekey_data = &arvif->gtk_rekey_data;
		if (!rekey_data->valid)
			continue;

		if (gtk_offload)
			rekey_data->enable_offload = WMI_GTK_OFFLOAD_ENABLE;
		else
			rekey_data->enable_offload = WMI_GTK_OFFLOAD_DISABLE;
		ret = ath10k_wmi_gtk_offload(ar, arvif);
		if (ret) {
			ath10k_err(ar, "GTK offload failed for vdev_id: %d\n",
				   arvif->vdev_id);
			return ret;
		}
	}

	return 0;
}

int ath10k_wow_op_suspend(struct ieee80211_hw *hw,
			  struct cfg80211_wowlan *wowlan)
{
	struct ath10k *ar = hw->priv;
	int ret;

	mutex_lock(&ar->conf_mutex);

	if (WARN_ON(!test_bit(ATH10K_FW_FEATURE_WOWLAN_SUPPORT,
			      ar->running_fw->fw_file.fw_features))) {
		ret = 1;
		goto exit;
	}

	ret = ath10k_wow_config_gtk_offload(ar, true);
	if (ret) {
		ath10k_warn(ar, "failed to enable GTK offload: %d\n", ret);
		goto exit;
	}

	ret = ath10k_wow_enable_ns_arp_offload(ar, true);
	if (ret) {
		ath10k_warn(ar, "failed to enable ARP-NS offload: %d\n", ret);
		goto disable_gtk_offload;
	}

	ret =  ath10k_wow_cleanup(ar);
	if (ret) {
		ath10k_warn(ar, "failed to clear wow wakeup events: %d\n",
			    ret);
		goto disable_ns_arp_offload;
	}

	ret = ath10k_wow_set_wakeups(ar, wowlan);
	if (ret) {
		ath10k_warn(ar, "failed to set wow wakeup events: %d\n",
			    ret);
		goto cleanup;
	}

	ret = ath10k_config_wow_listen_interval(ar);
	if (ret) {
		ath10k_warn(ar, "failed to config wow listen interval: %d\n",
			    ret);
		goto cleanup;
	}

	ret = ath10k_wow_enable(ar);
	if (ret) {
		ath10k_warn(ar, "failed to start wow: %d\n", ret);
		goto cleanup;
	}

	ret = ath10k_hif_suspend(ar);
	if (ret) {
		ath10k_warn(ar, "failed to suspend hif: %d\n", ret);
		goto wakeup;
	}

	goto exit;

wakeup:
	ath10k_wow_wakeup(ar);

cleanup:
	ath10k_wow_cleanup(ar);

disable_ns_arp_offload:
	ath10k_wow_enable_ns_arp_offload(ar, false);

disable_gtk_offload:
	ath10k_wow_config_gtk_offload(ar, false);
exit:
	mutex_unlock(&ar->conf_mutex);
	return ret ? 1 : 0;
}

void ath10k_wow_op_set_wakeup(struct ieee80211_hw *hw, bool enabled)
{
	struct ath10k *ar = hw->priv;

	mutex_lock(&ar->conf_mutex);
	if (test_bit(ATH10K_FW_FEATURE_WOWLAN_SUPPORT,
		     ar->running_fw->fw_file.fw_features)) {
		device_set_wakeup_enable(ar->dev, enabled);
	}
	mutex_unlock(&ar->conf_mutex);
}

int ath10k_wow_op_resume(struct ieee80211_hw *hw)
{
	struct ath10k *ar = hw->priv;
	int ret;

	mutex_lock(&ar->conf_mutex);

	if (WARN_ON(!test_bit(ATH10K_FW_FEATURE_WOWLAN_SUPPORT,
			      ar->running_fw->fw_file.fw_features))) {
		ret = 1;
		goto exit;
	}

	ret = ath10k_hif_resume(ar);
	if (ret) {
		ath10k_warn(ar, "failed to resume hif: %d\n", ret);
		goto exit;
	}

	ret = ath10k_wow_wakeup(ar);
	if (ret) {
		ath10k_warn(ar, "failed to wakeup from wow: %d\n", ret);
		goto exit;
	}

	ret = ath10k_wow_enable_ns_arp_offload(ar, false);
	if (ret) {
		ath10k_warn(ar, "failed to disable ARP-NS offload: %d\n", ret);
		goto exit;
	}

	ret = ath10k_wow_config_gtk_offload(ar, false);
	if (ret)
		ath10k_warn(ar, "failed to disable GTK offload: %d\n", ret);

exit:
	if (ret) {
		switch (ar->state) {
		case ATH10K_STATE_ON:
			ar->state = ATH10K_STATE_RESTARTING;
			ret = 1;
			break;
		case ATH10K_STATE_OFF:
		case ATH10K_STATE_RESTARTING:
		case ATH10K_STATE_RESTARTED:
		case ATH10K_STATE_UTF:
		case ATH10K_STATE_WEDGED:
			ath10k_warn(ar, "encountered unexpected device state %d on resume, cannot recover\n",
				    ar->state);
			ret = -EIO;
			break;
		}
	}

	mutex_unlock(&ar->conf_mutex);
	return ret;
}

int ath10k_wow_init(struct ath10k *ar)
{
	if (!test_bit(ATH10K_FW_FEATURE_WOWLAN_SUPPORT,
		      ar->running_fw->fw_file.fw_features))
		return 0;

	if (WARN_ON(!test_bit(WMI_SERVICE_WOW, ar->wmi.svc_map)))
		return -EINVAL;

	ar->wow.wowlan_support = ath10k_wowlan_support;
	ar->wow.wowlan_support.n_patterns = ar->wow.max_num_patterns;
	ar->hw->wiphy->wowlan = &ar->wow.wowlan_support;

	device_set_wakeup_capable(ar->dev, true);

	return 0;
}
