/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * All rights reserved.
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
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include <linux/etherdevice.h>
#include <net/mac80211.h>
#include <net/netlink.h>
#include "mvm.h"
#include "vendor-cmd.h"

#ifdef CPTCFG_IWLWIFI_LTE_COEX
#include "lte-coex.h"
#endif

static const struct nla_policy
iwl_mvm_vendor_attr_policy[NUM_IWL_MVM_VENDOR_ATTR] = {
	[IWL_MVM_VENDOR_ATTR_LOW_LATENCY] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_COUNTRY] = { .type = NLA_STRING, .len = 2 },
	[IWL_MVM_VENDOR_ATTR_FILTER_ARP_NA] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_FILTER_GTK] = { .type = NLA_FLAG },
	[IWL_MVM_VENDOR_ATTR_ADDR] = { .len = ETH_ALEN },
	[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L] = { .type = NLA_U32 },
	[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H] = { .type = NLA_U32 },
};

static int iwl_mvm_parse_vendor_data(struct nlattr **tb,
				     const void *data, int data_len)
{
	if (!data)
		return -EINVAL;

	return nla_parse(tb, MAX_IWL_MVM_VENDOR_ATTR, data, data_len,
			 iwl_mvm_vendor_attr_policy);
}

static int iwl_mvm_set_low_latency(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct nlattr *tb[NUM_IWL_MVM_VENDOR_ATTR];
	int err = iwl_mvm_parse_vendor_data(tb, data, data_len);
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);

	if (err)
		return err;

	if (!vif)
		return -ENODEV;

	mutex_lock(&mvm->mutex);
	err = iwl_mvm_update_low_latency(mvm, vif,
					 tb[IWL_MVM_VENDOR_ATTR_LOW_LATENCY]);
	mutex_unlock(&mvm->mutex);

	return err;
}

static int iwl_mvm_get_low_latency(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	struct iwl_mvm_vif *mvmvif;
	struct sk_buff *skb;

	if (!vif)
		return -ENODEV;
	mvmvif = iwl_mvm_vif_from_mac80211(vif);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;
	if (mvmvif->low_latency &&
	    nla_put_flag(skb, IWL_MVM_VENDOR_ATTR_LOW_LATENCY)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_mvm_set_country(struct wiphy *wiphy,
			       struct wireless_dev *wdev,
			       const void *data, int data_len)
{
	struct ieee80211_regdomain *regd;
	struct nlattr *tb[NUM_IWL_MVM_VENDOR_ATTR];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	int retval;

	if (!iwl_mvm_is_lar_supported(mvm))
		return -EOPNOTSUPP;

	retval = iwl_mvm_parse_vendor_data(tb, data, data_len);
	if (retval)
		return retval;

	if (!tb[IWL_MVM_VENDOR_ATTR_COUNTRY])
		return -EINVAL;

	mutex_lock(&mvm->mutex);

	/* set regdomain information to FW */
	regd = iwl_mvm_get_regdomain(wiphy,
				     nla_data(tb[IWL_MVM_VENDOR_ATTR_COUNTRY]),
				     iwl_mvm_is_wifi_mcc_supported(mvm) ?
				     MCC_SOURCE_3G_LTE_HOST :
				     MCC_SOURCE_OLD_FW, NULL);
	if (IS_ERR_OR_NULL(regd)) {
		retval = -EIO;
		goto unlock;
	}

	retval = regulatory_set_wiphy_regd(wiphy, regd);
	kfree(regd);
unlock:
	mutex_unlock(&mvm->mutex);
	return retval;
}

#ifdef CPTCFG_IWLWIFI_LTE_COEX
static int iwl_vendor_lte_coex_state_cmd(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_state_cmd *cmd = data;
	struct sk_buff *skb;
	int err = LTE_OK;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: state cmd:\n\tstate: %d\n",
		       cmd->lte_state);

	switch (cmd->lte_state) {
	case LTE_OFF:
		if (mvm->lte_state.has_config &&
		    mvm->lte_state.state != LTE_CONNECTED) {
			err = LTE_STATE_ERR;
			goto out;
		}
		mvm->lte_state.state = LTE_OFF;
		mvm->lte_state.has_config = 0;
		mvm->lte_state.has_rprtd_chan = 0;
		mvm->lte_state.has_sps = 0;
		mvm->lte_state.has_ft = 0;
		break;
	case LTE_IDLE:
		if (!mvm->lte_state.has_static ||
		    (mvm->lte_state.has_config &&
		     mvm->lte_state.state != LTE_CONNECTED)) {
			err = LTE_STATE_ERR;
			goto out;
		}
		mvm->lte_state.has_config = 0;
		mvm->lte_state.has_sps = 0;
		mvm->lte_state.state = LTE_IDLE;
		break;
	case LTE_CONNECTED:
		if (!(mvm->lte_state.has_config)) {
			err = LTE_STATE_ERR;
			goto out;
		}
		mvm->lte_state.state = LTE_CONNECTED;
		break;
	default:
		err = LTE_ILLEGAL_PARAMS;
		goto out;
	}

	mvm->lte_state.config.lte_state = cpu_to_le32(mvm->lte_state.state);

	mutex_lock(&mvm->mutex);
	if (iwl_mvm_send_lte_coex_config_cmd(mvm))
		err = LTE_OTHER_ERR;
	mutex_unlock(&mvm->mutex);

out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_vendor_lte_coex_config_cmd(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_config_info_cmd *cmd = data;
	struct iwl_lte_coex_static_params_cmd *stat = &mvm->lte_state.stat;
	struct sk_buff *skb;
	int err = LTE_OK;
	int i, j;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: config cmd:\n");

	/* send static config only once in the FW life */
	if (mvm->lte_state.has_static)
		goto out;

	for (i = 0; i < LTE_MWS_CONF_LENGTH; i++) {
		IWL_DEBUG_COEX(mvm, "\tmws config data[%d]: %d\n", i,
			       cmd->mws_conf_data[i]);
		stat->mfu_config[i] = cpu_to_le32(cmd->mws_conf_data[i]);
	}

	if (cmd->safe_power_table[0] != LTE_SAFE_PT_FIRST ||
	    cmd->safe_power_table[LTE_SAFE_PT_LENGTH - 1] !=
							LTE_SAFE_PT_LAST) {
		err = LTE_ILLEGAL_PARAMS;
		goto out;
	}

	/* power table must be ascending ordered */
	j = LTE_SAFE_PT_FIRST;
	for (i = 0; i < LTE_SAFE_PT_LENGTH; i++) {
		IWL_DEBUG_COEX(mvm, "\tsafe power table[%d]: %d\n", i,
			       cmd->safe_power_table[i]);
		if (cmd->safe_power_table[i] < j) {
			err = LTE_ILLEGAL_PARAMS;
			goto out;
		}
		j = cmd->safe_power_table[i];
		stat->tx_power_in_dbm[i] = cmd->safe_power_table[i];
	}

	mutex_lock(&mvm->mutex);
	if (iwl_mvm_send_lte_coex_static_params_cmd(mvm))
		err = LTE_OTHER_ERR;
	else
		mvm->lte_state.has_static = 1;
	mutex_unlock(&mvm->mutex);

out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int in_range(int val, int min, int max)
{
	return (val >= min) && (val <= max);
}

static int iwl_vendor_lte_coex_dynamic_info_cmd(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_dynamic_info_cmd *cmd = data;
	struct iwl_lte_coex_config_cmd *config = &mvm->lte_state.config;
	struct sk_buff *skb;
	int err = LTE_OK;
	int i;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	if (!mvm->lte_state.has_static ||
	    (mvm->lte_state.has_config &&
	     mvm->lte_state.state != LTE_CONNECTED)) {
		err = LTE_STATE_ERR;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: dynamic cmd:\n"
		       "\tlte band[0]: %d, chan[0]: %d\n\ttx range: %d - %d\n"
		       "\trx range: %d - %d\n", cmd->lte_connected_bands[0],
		       cmd->lte_connected_bands[1], cmd->wifi_tx_safe_freq_min,
		       cmd->wifi_tx_safe_freq_max, cmd->wifi_rx_safe_freq_min,
		       cmd->wifi_rx_safe_freq_max);

	/* TODO: validate lte connected bands and channel, and frame struct */
	config->lte_band = cpu_to_le32(cmd->lte_connected_bands[0]);
	config->lte_chan = cpu_to_le32(cmd->lte_connected_bands[1]);
	for (i = 0; i < LTE_FRAME_STRUCT_LENGTH; i++) {
		IWL_DEBUG_COEX(mvm, "\tframe structure[%d]: %d\n", i,
			       cmd->lte_frame_structure[i]);
		config->lte_frame_structure[i] =
				cpu_to_le32(cmd->lte_frame_structure[i]);
	}
	if (!in_range(cmd->wifi_tx_safe_freq_min, LTE_FRQ_MIN, LTE_FRQ_MAX) ||
	    !in_range(cmd->wifi_tx_safe_freq_max, LTE_FRQ_MIN, LTE_FRQ_MAX) ||
	    !in_range(cmd->wifi_rx_safe_freq_min, LTE_FRQ_MIN, LTE_FRQ_MAX) ||
	    !in_range(cmd->wifi_rx_safe_freq_max, LTE_FRQ_MIN, LTE_FRQ_MAX) ||
	    cmd->wifi_tx_safe_freq_max < cmd->wifi_tx_safe_freq_min ||
	    cmd->wifi_rx_safe_freq_max < cmd->wifi_rx_safe_freq_min) {
		err = LTE_ILLEGAL_PARAMS;
		goto out;
	}
	config->tx_safe_freq_min = cpu_to_le32(cmd->wifi_tx_safe_freq_min);
	config->tx_safe_freq_max = cpu_to_le32(cmd->wifi_tx_safe_freq_max);
	config->rx_safe_freq_min = cpu_to_le32(cmd->wifi_rx_safe_freq_min);
	config->rx_safe_freq_max = cpu_to_le32(cmd->wifi_rx_safe_freq_max);
	for (i = 0; i < LTE_TX_POWER_LENGTH; i++) {
		IWL_DEBUG_COEX(mvm, "\twifi max tx power[%d]: %d\n", i,
			       cmd->wifi_max_tx_power[i]);
		if (!in_range(cmd->wifi_max_tx_power[i], LTE_MAX_TX_MIN,
			      LTE_MAX_TX_MAX)) {
			err = LTE_ILLEGAL_PARAMS;
			goto out;
		}
		config->max_tx_power[i] = cmd->wifi_max_tx_power[i];
	}

	mvm->lte_state.has_config = 1;

	if (mvm->lte_state.state == LTE_CONNECTED) {
		mutex_lock(&mvm->mutex);
		if (iwl_mvm_send_lte_coex_config_cmd(mvm))
			err = LTE_OTHER_ERR;
		mutex_unlock(&mvm->mutex);
	}
out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int iwl_vendor_lte_sps_cmd(struct wiphy *wiphy,
				  struct wireless_dev *wdev, const void *data,
				  int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_sps_info_cmd *cmd = data;
	struct iwl_lte_coex_sps_cmd *sps = &mvm->lte_state.sps;
	struct sk_buff *skb;
	int err = LTE_OK;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: sps cmd:\n\tsps info: %d\n",
		       cmd->sps_info);

	if (mvm->lte_state.state != LTE_CONNECTED) {
		err = LTE_STATE_ERR;
		goto out;
	}

	/* TODO: validate SPS */
	sps->lte_semi_persistent_info = cpu_to_le32(cmd->sps_info);

	mutex_lock(&mvm->mutex);
	if (iwl_mvm_send_lte_sps_cmd(mvm))
		err = LTE_OTHER_ERR;
	else
		mvm->lte_state.has_sps = 1;
	mutex_unlock(&mvm->mutex);

out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}

static int
iwl_vendor_lte_coex_wifi_reported_channel_cmd(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	const struct lte_coex_wifi_reported_chan_cmd *cmd = data;
	struct iwl_lte_coex_wifi_reported_channel_cmd *rprtd_chan =
						&mvm->lte_state.rprtd_chan;
	struct sk_buff *skb;
	int err = LTE_OK;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;

	if (data_len != sizeof(*cmd)) {
		err = LTE_INVALID_DATA;
		goto out;
	}

	IWL_DEBUG_COEX(mvm, "LTE-COEX: wifi reported channel cmd:\n"
		       "\tchannel: %d, bandwith: %d\n", cmd->chan,
		       cmd->bandwidth);

	if (!in_range(cmd->chan, LTE_RC_CHAN_MIN, LTE_RC_CHAN_MAX) ||
	    !in_range(cmd->bandwidth, LTE_RC_BW_MIN, LTE_RC_BW_MAX)) {
		err = LTE_ILLEGAL_PARAMS;
		goto out;
	}

	rprtd_chan->channel = cpu_to_le32(cmd->chan);
	rprtd_chan->bandwidth = cpu_to_le32(cmd->bandwidth);

	mutex_lock(&mvm->mutex);
	if (iwl_mvm_send_lte_coex_wifi_reported_channel_cmd(mvm))
		err = LTE_OTHER_ERR;
	else
		mvm->lte_state.has_rprtd_chan = 1;
	mutex_unlock(&mvm->mutex);

out:
	if (err)
		iwl_mvm_reset_lte_state(mvm);

	if (nla_put_u8(skb, NLA_BINARY, err)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}
#endif /* CPTCFG_IWLWIFI_LTE_COEX */

static int iwl_vendor_frame_filter_cmd(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int data_len)
{
	struct nlattr *tb[NUM_IWL_MVM_VENDOR_ATTR];
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	int err = iwl_mvm_parse_vendor_data(tb, data, data_len);

	if (err)
		return err;
	vif->filter_grat_arp_unsol_na =
		tb[IWL_MVM_VENDOR_ATTR_FILTER_ARP_NA];
	vif->filter_gtk = tb[IWL_MVM_VENDOR_ATTR_FILTER_GTK];

	return 0;
}

#ifdef CPTCFG_IWLMVM_TDLS_PEER_CACHE
static int iwl_vendor_tdls_peer_cache_add(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct nlattr *tb[NUM_IWL_MVM_VENDOR_ATTR];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_tdls_peer_counter *cnt;
	u8 *addr;
	struct ieee80211_vif *vif = wdev_to_ieee80211_vif(wdev);
	int err = iwl_mvm_parse_vendor_data(tb, data, data_len);

	if (err)
		return err;

	if (vif->type != NL80211_IFTYPE_STATION ||
	    !tb[IWL_MVM_VENDOR_ATTR_ADDR])
		return -EINVAL;

	mutex_lock(&mvm->mutex);
	if (mvm->tdls_peer_cache_cnt >= IWL_MVM_TDLS_CNT_MAX_PEERS) {
		err = -ENOSPC;
		goto out_unlock;
	}

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	rcu_read_lock();
	cnt = iwl_mvm_tdls_peer_cache_find(mvm, addr);
	rcu_read_unlock();
	if (cnt) {
		err = -EEXIST;
		goto out_unlock;
	}

	cnt = kzalloc(sizeof(*cnt), GFP_KERNEL);
	if (!cnt) {
		err = -ENOMEM;
		goto out_unlock;
	}

	IWL_DEBUG_TDLS(mvm, "Adding %pM to TDLS peer cache\n", addr);
	ether_addr_copy(cnt->mac.addr, addr);
	cnt->vif = vif;
	list_add_tail_rcu(&cnt->list, &mvm->tdls_peer_cache_list);
	mvm->tdls_peer_cache_cnt++;

out_unlock:
	mutex_unlock(&mvm->mutex);
	return err;
}

static int iwl_vendor_tdls_peer_cache_del(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct nlattr *tb[NUM_IWL_MVM_VENDOR_ATTR];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_tdls_peer_counter *cnt;
	u8 *addr;
	int err = iwl_mvm_parse_vendor_data(tb, data, data_len);

	if (err)
		return err;

	if (!tb[IWL_MVM_VENDOR_ATTR_ADDR])
		return -EINVAL;

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	mutex_lock(&mvm->mutex);
	rcu_read_lock();
	cnt = iwl_mvm_tdls_peer_cache_find(mvm, addr);
	if (!cnt) {
		IWL_DEBUG_TDLS(mvm, "%pM not found in TDLS peer cache\n", addr);
		err = -ENOENT;
		goto out_unlock;
	}

	IWL_DEBUG_TDLS(mvm, "Removing %pM from TDLS peer cache\n", addr);
	mvm->tdls_peer_cache_cnt--;
	list_del_rcu(&cnt->list);
	kfree_rcu(cnt, rcu_head);

out_unlock:
	rcu_read_unlock();
	mutex_unlock(&mvm->mutex);
	return err;
}

static int iwl_vendor_tdls_peer_cache_query(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	struct nlattr *tb[NUM_IWL_MVM_VENDOR_ATTR];
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_mvm_tdls_peer_counter *cnt;
	struct sk_buff *skb;
	u32 rx_bytes, tx_bytes;
	u8 *addr;
	int err = iwl_mvm_parse_vendor_data(tb, data, data_len);

	if (err)
		return err;

	if (!tb[IWL_MVM_VENDOR_ATTR_ADDR])
		return -EINVAL;

	addr = nla_data(tb[IWL_MVM_VENDOR_ATTR_ADDR]);

	rcu_read_lock();
	cnt = iwl_mvm_tdls_peer_cache_find(mvm, addr);
	if (!cnt) {
		IWL_DEBUG_TDLS(mvm, "%pM not found in TDLS peer cache\n",
			       addr);
		err = -ENOENT;
	} else {
		rx_bytes = cnt->rx_bytes;
		tx_bytes = cnt->tx_bytes;
	}
	rcu_read_unlock();
	if (err)
		return err;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, 100);
	if (!skb)
		return -ENOMEM;
	if (nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_TX_BYTES, tx_bytes) ||
	    nla_put_u32(skb, IWL_MVM_VENDOR_ATTR_RX_BYTES, rx_bytes)) {
		kfree_skb(skb);
		return -ENOBUFS;
	}

	return cfg80211_vendor_cmd_reply(skb);
}
#endif /* CPTCFG_IWLMVM_TDLS_PEER_CACHE */

static int iwl_vendor_set_nic_txpower_limit(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int data_len)
{
	struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
	struct iwl_mvm *mvm = IWL_MAC80211_GET_MVM(hw);
	struct iwl_dev_tx_power_cmd cmd = {
		.set_mode = cpu_to_le32(1),
		.dev_24 = cpu_to_le16(IWL_DEV_MAX_TX_POWER),
		.dev_52_low = cpu_to_le16(IWL_DEV_MAX_TX_POWER),
		.dev_52_high = cpu_to_le16(IWL_DEV_MAX_TX_POWER),
	};
	struct nlattr *tb[NUM_IWL_MVM_VENDOR_ATTR];
	int err;

	if (!(mvm->fw->ucode_capa.api[0] & IWL_UCODE_TLV_API_TX_POWER_DEV))
		return -EOPNOTSUPP;

	err = iwl_mvm_parse_vendor_data(tb, data, data_len);
	if (err)
		return err;

	if (tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24]) {
		s32 txp = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_24]);

		if (txp < 0 || txp > IWL_DEV_MAX_TX_POWER)
			return -EINVAL;
		cmd.dev_24 = cpu_to_le16(txp);
	}

	if (tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L]) {
		s32 txp = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52L]);

		if (txp < 0 || txp > IWL_DEV_MAX_TX_POWER)
			return -EINVAL;
		cmd.dev_52_low = cpu_to_le16(txp);
	}

	if (tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H]) {
		s32 txp = nla_get_u32(tb[IWL_MVM_VENDOR_ATTR_TXP_LIMIT_52H]);

		if (txp < 0 || txp > IWL_DEV_MAX_TX_POWER)
			return -EINVAL;
		cmd.dev_52_high = cpu_to_le16(txp);
	}

	mvm->txp_cmd = cmd;

	err = iwl_mvm_send_cmd_pdu(mvm, REDUCE_TX_POWER_CMD, 0,
				   sizeof(cmd), &cmd);
	if (err)
		IWL_ERR(mvm, "failed to update device TX power: %d\n", err);
	return 0;
}

static const struct wiphy_vendor_command iwl_mvm_vendor_commands[] = {
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_LOW_LATENCY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_set_low_latency,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_GET_LOW_LATENCY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_get_low_latency,
	},
#ifdef CPTCFG_IWLWIFI_LTE_COEX
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_STATE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_coex_state_cmd,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_COEX_CONFIG_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_coex_config_cmd,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_COEX_DYNAMIC_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_coex_dynamic_info_cmd,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_COEX_SPS_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_sps_cmd,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_LTE_COEX_WIFI_RPRTD_CHAN,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_lte_coex_wifi_reported_channel_cmd,
	},
#endif /* CPTCFG_IWLWIFI_LTE_COEX */
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_COUNTRY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_mvm_set_country,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_PROXY_FRAME_FILTERING,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_frame_filter_cmd,
	},
#ifdef CPTCFG_IWLMVM_TDLS_PEER_CACHE
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_ADD,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_tdls_peer_cache_add,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_DEL,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_tdls_peer_cache_del,
	},
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_TDLS_PEER_CACHE_QUERY,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_tdls_peer_cache_query,
	},
#endif /* CPTCFG_IWLMVM_TDLS_PEER_CACHE */
	{
		.info = {
			.vendor_id = INTEL_OUI,
			.subcmd = IWL_MVM_VENDOR_CMD_SET_NIC_TXPOWER_LIMIT,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = iwl_vendor_set_nic_txpower_limit,
	},
};

#ifdef CPTCFG_IWLMVM_TCM
static const struct nl80211_vendor_cmd_info iwl_mvm_vendor_events[] = {
	{
		.vendor_id = INTEL_OUI,
		.subcmd = IWL_MVM_VENDOR_CMD_TCM_EVENT,
	},
};
#endif

void iwl_mvm_set_wiphy_vendor_commands(struct wiphy *wiphy)
{
	wiphy->vendor_commands = iwl_mvm_vendor_commands;
	wiphy->n_vendor_commands = ARRAY_SIZE(iwl_mvm_vendor_commands);
#ifdef CPTCFG_IWLMVM_TCM
	wiphy->vendor_events = iwl_mvm_vendor_events;
	wiphy->n_vendor_events = ARRAY_SIZE(iwl_mvm_vendor_events);
#endif
}

#ifdef CPTCFG_IWLMVM_TCM
void iwl_mvm_send_tcm_event(struct iwl_mvm *mvm, struct ieee80211_vif *vif)
{
	struct sk_buff *msg = cfg80211_vendor_event_alloc(mvm->hw->wiphy,
							  200, 0, GFP_ATOMIC);

	if (!msg)
		return;

	if (vif) {
		struct iwl_mvm_vif *mvmvif = iwl_mvm_vif_from_mac80211(vif);

		if (nla_put(msg, IWL_MVM_VENDOR_ATTR_VIF_ADDR,
			    ETH_ALEN, vif->addr) ||
		    nla_put_u8(msg, IWL_MVM_VENDOR_ATTR_VIF_LL,
			       iwl_mvm_vif_low_latency(mvmvif)) ||
		    nla_put_u8(msg, IWL_MVM_VENDOR_ATTR_VIF_LOAD,
			       mvm->tcm.result.load[mvmvif->id]))
			goto nla_put_failure;
	}

	if (nla_put_u8(msg, IWL_MVM_VENDOR_ATTR_LL, iwl_mvm_low_latency(mvm)) ||
	    nla_put_u8(msg, IWL_MVM_VENDOR_ATTR_LOAD,
		       mvm->tcm.result.global_load))
		goto nla_put_failure;

	cfg80211_vendor_event(msg, GFP_ATOMIC);
	return;

 nla_put_failure:
	kfree_skb(msg);
}
#endif
