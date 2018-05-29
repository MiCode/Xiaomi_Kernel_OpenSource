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
#include <net/addrconf.h>
#include "hif.h"
#include "core.h"
#include "debug.h"
#include "wmi.h"
#include "wmi-ops.h"

static const struct wiphy_wowlan_support ath10k_wowlan_support = {
	.flags = WIPHY_WOWLAN_DISCONNECT |
		WIPHY_WOWLAN_MAGIC_PKT |
		WIPHY_WOWLAN_SUPPORTS_GTK_REKEY |
		WIPHY_WOWLAN_GTK_REKEY_FAILURE,
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
	struct ieee80211_bss_conf *bss = &arvif->vif->bss_conf;
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
		if (arvif->is_up && bss->assoc) {
			if (wowlan->disconnect) {
				__set_bit(WOW_DEAUTH_RECVD_EVENT, &wow_mask);
				__set_bit(WOW_DISASSOC_RECVD_EVENT, &wow_mask);
				__set_bit(WOW_BMISS_EVENT, &wow_mask);
				__set_bit(WOW_CSA_IE_EVENT, &wow_mask);
			}

			if (wowlan->magic_pkt)
				__set_bit(WOW_MAGIC_PKT_RECVD_EVENT, &wow_mask);
			if (wowlan->gtk_rekey_failure)
				__set_bit(WOW_GTK_ERR_EVENT, &wow_mask);
		}
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
ath10k_wow_fill_vdev_ns_offload_struct(struct ath10k_vif *arvif,
				       bool enable_offload)
{
	struct in6_addr addr[TARGET_NUM_STATIONS];
	struct wmi_ns_arp_offload_req *ns;
	struct wireless_dev *wdev;
	struct inet6_dev *in6_dev;
	struct in6_addr addr_type;
	struct inet6_ifaddr *ifa;
	struct ifacaddr6 *ifaca;
	struct list_head *addr_list;
	u32 scope, count = 0;
	int i;

	ns = &arvif->ns_offload;
	if (!enable_offload) {
		ns->offload_type = __cpu_to_le16(WMI_NS_ARP_OFFLOAD);
		ns->enable_offload = __cpu_to_le16(WMI_ARP_NS_OFFLOAD_DISABLE);
		return 0;
	}

	wdev = ieee80211_vif_to_wdev(arvif->vif);
	if (!wdev)
		return -ENODEV;

	in6_dev = __in6_dev_get(wdev->netdev);
	if (!in6_dev)
		return -ENODEV;

	memset(&addr, 0, TARGET_NUM_STATIONS * sizeof(struct in6_addr));
	memset(&addr_type, 0, sizeof(struct in6_addr));

	/* Unicast Addresses */
	read_lock_bh(&in6_dev->lock);
	list_for_each(addr_list, &in6_dev->addr_list) {
		if (count >= TARGET_NUM_STATIONS) {
			read_unlock_bh(&in6_dev->lock);
			return -EINVAL;
		}

		ifa = list_entry(addr_list, struct inet6_ifaddr, if_list);
		if (ifa->flags & IFA_F_DADFAILED)
			continue;
		scope = ipv6_addr_src_scope(&ifa->addr);
		switch (scope) {
		case IPV6_ADDR_SCOPE_GLOBAL:
		case IPV6_ADDR_SCOPE_LINKLOCAL:
			memcpy(&addr[count], &ifa->addr.s6_addr,
			       sizeof(ifa->addr.s6_addr));
			addr_type.s6_addr[count] = IPV6_ADDR_UNICAST;
			count += 1;
			break;
		}
	}

	/* Anycast Addresses */
	for (ifaca = in6_dev->ac_list; ifaca; ifaca = ifaca->aca_next) {
		if (count >= TARGET_NUM_STATIONS) {
			read_unlock_bh(&in6_dev->lock);
			return -EINVAL;
		}

		scope = ipv6_addr_src_scope(&ifaca->aca_addr);
		switch (scope) {
		case IPV6_ADDR_SCOPE_GLOBAL:
		case IPV6_ADDR_SCOPE_LINKLOCAL:
			memcpy(&addr[count], &ifaca->aca_addr,
			       sizeof(ifaca->aca_addr));
			addr_type.s6_addr[count] = IPV6_ADDR_ANY;
			count += 1;
			break;
		}
	}
	read_unlock_bh(&in6_dev->lock);

	/* Filling up the request structure
	 * Filling the self_addr with solicited address
	 * A Solicited-Node multicast address is created by
	 * taking the last 24 bits of a unicast or anycast
	 * address and appending them to the prefix
	 *
	 * FF02:0000:0000:0000:0000:0001:FFXX:XXXX
	 *
	 * here XX is the unicast/anycast bits
	 */
	for (i = 0; i < count; i++) {
		ns->info.self_addr[i].s6_addr[0] = 0xFF;
		ns->info.self_addr[i].s6_addr[1] = 0x02;
		ns->info.self_addr[i].s6_addr[11] = 0x01;
		ns->info.self_addr[i].s6_addr[12] = 0xFF;
		ns->info.self_addr[i].s6_addr[13] = addr[i].s6_addr[13];
		ns->info.self_addr[i].s6_addr[14] = addr[i].s6_addr[14];
		ns->info.self_addr[i].s6_addr[15] = addr[i].s6_addr[15];
		ns->info.slot_idx = i;
		memcpy(&ns->info.target_addr[i], &addr[i],
		       sizeof(struct in6_addr));
		ns->info.target_addr_valid.s6_addr[i] = 1;
		ns->info.target_ipv6_ac.s6_addr[i] = addr_type.s6_addr[i];
		memcpy(&ns->params.ipv6_addr, &ns->info.target_addr[i],
		       sizeof(struct in6_addr));
	}

	ns->offload_type = __cpu_to_le16(WMI_NS_ARP_OFFLOAD);
	ns->enable_offload = __cpu_to_le16(WMI_ARP_NS_OFFLOAD_ENABLE);
	ns->num_ns_offload_count = __cpu_to_le16(count);

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

		ret = ath10k_wow_fill_vdev_ns_offload_struct(arvif, offload);
		if (ret) {
			ath10k_err(ar, "NS-offload config failed, vdev: %d\n",
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

	ret =  ath10k_wow_cleanup(ar);
	if (ret) {
		ath10k_warn(ar, "failed to clear wow wakeup events: %d\n",
			    ret);
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

	ret = ath10k_wow_set_wakeups(ar, wowlan);
	if (ret) {
		ath10k_warn(ar, "failed to set wow wakeup events: %d\n",
			    ret);
		goto disable_ns_arp_offload;
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

static void ath10k_wow_op_report_wakeup_reason(struct ath10k *ar)
{
	struct cfg80211_wowlan_wakeup *wakeup = &ar->wow.wakeup;
	struct ath10k_vif *arvif;

	memset(wakeup, 0, sizeof(struct cfg80211_wowlan_wakeup));
	switch (ar->wow.wakeup_reason) {
	case WOW_REASON_UNSPECIFIED:
		wakeup = NULL;
		break;
	case WOW_REASON_RECV_MAGIC_PATTERN:
		wakeup->magic_pkt = true;
		break;
	case WOW_REASON_DEAUTH_RECVD:
	case WOW_REASON_DISASSOC_RECVD:
	case WOW_REASON_AP_ASSOC_LOST:
	case WOW_REASON_CSA_EVENT:
		wakeup->disconnect = true;
		break;
	case WOW_REASON_GTK_HS_ERR:
		wakeup->gtk_rekey_failure = true;
		break;
	}
	ar->wow.wakeup_reason = WOW_REASON_UNSPECIFIED;

	if (wakeup) {
		wakeup->pattern_idx = -1;
		list_for_each_entry(arvif, &ar->arvifs, list) {
			ieee80211_report_wowlan_wakeup(arvif->vif,
						       wakeup, GFP_KERNEL);
			if (wakeup->disconnect)
				ieee80211_resume_disconnect(arvif->vif);
		}
	} else {
		list_for_each_entry(arvif, &ar->arvifs, list)
			ieee80211_report_wowlan_wakeup(arvif->vif,
						       NULL, GFP_KERNEL);
	}
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

	ath10k_wow_op_report_wakeup_reason(ar);
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
	device_init_wakeup(ar->dev, true);

	return 0;
}

void ath10k_wow_deinit(struct ath10k *ar)
{
	if (test_bit(ATH10K_FW_FEATURE_WOWLAN_SUPPORT,
		     ar->running_fw->fw_file.fw_features) &&
		test_bit(WMI_SERVICE_WOW, ar->wmi.svc_map))
		device_init_wakeup(ar->dev, false);
}
