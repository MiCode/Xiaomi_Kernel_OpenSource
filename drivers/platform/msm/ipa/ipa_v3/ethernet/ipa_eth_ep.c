/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/if_vlan.h>

#include "ipa_eth_i.h"

static void handle_ipa_receive(struct ipa_eth_channel *ch,
			       unsigned long data)
{
	bool success = false;
	struct sk_buff *skb = (struct sk_buff *) data;

	ch->exception_total++;

#ifndef IPA_ETH_EP_LOOPBACK
	success = ch->process_skb && !ch->process_skb(ch, skb);
#else
	success = !ipa_tx_dp(IPA_CLIENT_AQC_ETHERNET_CONS, skb, NULL);
	if (success)
		ch->exception_loopback++;
#endif

	if (!success) {
		ch->exception_drops++;
		dev_kfree_skb_any(skb);
	}
}

static void ipa_ep_client_notifier(void *priv, enum ipa_dp_evt_type evt,
				   unsigned long data)
{
	struct ipa_eth_channel *ch = (struct ipa_eth_channel *) priv;

	if (evt == IPA_RECEIVE)
		handle_ipa_receive(ch, data);
}

enum ipa_eth_phdr_type {
	IPA_ETH_PHDR_V4,
	IPA_ETH_PHDR_V6,
	IPA_ETH_PHDR_MAX = IPA_ETH_PHDR_V6,
};

#define IPA_ETH_PHDR_NUM (IPA_ETH_PHDR_MAX + 1)

struct ipa_eth_phdr_add_ioc {
	struct ipa_ioc_add_hdr ioc;
	struct ipa_hdr_add hdrs[IPA_ETH_PHDR_NUM];
};

static const struct ipa_eth_phdr_add_ioc ADD_HDR_TEMPLATE = {
	.ioc = {
		.commit = 1,
		.num_hdrs = IPA_ETH_PHDR_NUM,
	},
	.hdrs = {
		[IPA_ETH_PHDR_V4] = {
			.is_partial = 1,
			.is_eth2_ofst_valid = 1,
			.eth2_ofst = 0,
		},
		[IPA_ETH_PHDR_V6] = {
			.is_partial = 1,
			.is_eth2_ofst_valid = 1,
			.eth2_ofst = 0,
		},
	},
};

static inline __be16 phdr_type_to_proto(enum ipa_eth_phdr_type type)
{
	return (type == IPA_ETH_PHDR_V4) ?
			htons(ETH_P_IP) : htons(ETH_P_IPV6);
}

static u8 ipa_eth_init_ethhdr(struct ethhdr *eth_hdr,
				enum ipa_eth_phdr_type type,
				struct net_device *net_dev)
{
	memset(eth_hdr, 0, sizeof(*eth_hdr));

	eth_hdr->h_proto = phdr_type_to_proto(type);
	memcpy(&eth_hdr->h_source, net_dev->dev_addr, ETH_ALEN);

	return ETH_HLEN;
}

static u8 ipa_eth_init_vlan_ethhdr(struct vlan_ethhdr *eth_hdr,
				enum ipa_eth_phdr_type type,
				struct net_device *net_dev)
{
	memset(eth_hdr, 0, sizeof(*eth_hdr));

	eth_hdr->h_vlan_proto = htons(ETH_P_8021Q);
	eth_hdr->h_vlan_encapsulated_proto = phdr_type_to_proto(type);
	memcpy(&eth_hdr->h_source, net_dev->dev_addr, ETH_ALEN);

	return VLAN_ETH_HLEN;
}

static void ipa_eth_init_hdr_add(struct ipa_hdr_add *hdr_add,
			bool vlan_mode,
			enum ipa_eth_phdr_type type,
			struct net_device *net_dev)
{
	const char *fmt_str = (type == IPA_ETH_PHDR_V4) ? "%s_ipv4" : "%s_ipv6";

	snprintf(hdr_add->name, sizeof(hdr_add->name), fmt_str, net_dev->name);

	hdr_add->hdr_len = vlan_mode ?
		ipa_eth_init_vlan_ethhdr((void *)hdr_add->hdr, type, net_dev) :
		ipa_eth_init_ethhdr((void *)hdr_add->hdr, type, net_dev);

	hdr_add->type = vlan_mode ? IPA_HDR_L2_802_1Q : IPA_HDR_L2_ETHERNET_II;
}

/**
 * ipa_eth_ep_init_headers() - Install partial headers
 * @eth_dev: Offload device
 *
 * Installs partial headers in IPA for IPv4 and IPv6 traffic.
 *
 * Return: 0 on success, negative errno code otherwise
 */
int ipa_eth_ep_init_headers(struct ipa_eth_device *eth_dev)
{
	int rc;
	bool vlan_mode;
	struct net_device *net_dev = eth_dev->net_dev;
	struct ipa_eth_phdr_add_ioc phdr_add = ADD_HDR_TEMPLATE;
	struct ipa_hdr_add *hdr_v4 = &phdr_add.hdrs[IPA_ETH_PHDR_V4];
	struct ipa_hdr_add *hdr_v6 = &phdr_add.hdrs[IPA_ETH_PHDR_V6];

	rc = ipa3_is_vlan_mode(IPA_VLAN_IF_ETH, &vlan_mode);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Could not determine IPA VLAN mode");
		return rc;
	}

	ipa_eth_init_hdr_add(hdr_v4, vlan_mode, IPA_ETH_PHDR_V4, net_dev);
	ipa_eth_init_hdr_add(hdr_v6, vlan_mode, IPA_ETH_PHDR_V6, net_dev);

	rc = ipa_add_hdr(&phdr_add.ioc);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to install partial headers");
	} else {
		eth_dev->phdr_v4_handle = hdr_v4->hdr_hdl;
		eth_dev->phdr_v6_handle = hdr_v6->hdr_hdl;
	}

	return rc;
}

struct ipa_eth_phdr_del_ioc {
	struct ipa_ioc_del_hdr ioc;
	struct ipa_hdr_del hdrs[IPA_ETH_PHDR_NUM];
};

static const struct ipa_eth_phdr_del_ioc DEL_HDR_TEMPLATE = {
	.ioc = {
		.commit = 1,
		.num_hdls = IPA_ETH_PHDR_NUM,
	},
};

int ipa_eth_ep_deinit_headers(struct ipa_eth_device *eth_dev)
{
	int rc;
	struct ipa_eth_phdr_del_ioc phdr_del = DEL_HDR_TEMPLATE;

	phdr_del.hdrs[IPA_ETH_PHDR_V4].hdl = eth_dev->phdr_v4_handle;
	phdr_del.hdrs[IPA_ETH_PHDR_V6].hdl = eth_dev->phdr_v6_handle;

	rc = ipa_del_hdr(&phdr_del.ioc);
	if (rc)
		ipa_eth_dev_err(eth_dev, "Failed to remove partial headers");

	return rc;
}

static inline size_t __list_size(struct list_head *head)
{
	size_t count = 0;
	struct list_head *l;

	list_for_each(l, head)
		count++;

	return count;
}

static void ipa_eth_ep_init_rx_props(
		struct ipa_ioc_rx_intf_prop *props,
		struct ipa_eth_channel *ch,
		bool is_ipv4, bool vlan_mode)
{
	props->ip = is_ipv4 ? IPA_IP_v4 : IPA_IP_v6;
	props->src_pipe = ch->ipa_client;
	props->hdr_l2_type = vlan_mode ?
			IPA_HDR_L2_802_1Q : IPA_HDR_L2_ETHERNET_II;
}

static void ipa_eth_ep_init_tx_props(
		struct ipa_ioc_tx_intf_prop *props,
		struct ipa_eth_channel *ch,
		bool is_ipv4, bool vlan_mode)
{
	const char *fmt_str = is_ipv4 ? "%s_ipv4" : "%s_ipv6";

	props->ip = is_ipv4 ? IPA_IP_v4 : IPA_IP_v6;
	props->dst_pipe = ch->ipa_client;
	props->hdr_l2_type = vlan_mode ?
			IPA_HDR_L2_802_1Q : IPA_HDR_L2_ETHERNET_II;

	snprintf(props->hdr_name, sizeof(props->hdr_name), fmt_str,
			ch->eth_dev->net_dev->name);
}

static int ipa_eth_ep_init_tx_intf(
		struct ipa_eth_device *eth_dev,
		struct ipa_tx_intf *tx_intf,
		bool vlan_mode)
{
	u32 num_props;
	struct ipa_eth_channel *ch;

	/* one each for IPv4 and IPv6 */
	num_props = __list_size(&eth_dev->tx_channels) * 2;

	tx_intf->prop = kcalloc(num_props, sizeof(*tx_intf->prop), GFP_KERNEL);
	if (!tx_intf->prop) {
		ipa_eth_dev_err(eth_dev, "Failed to alloc tx props");
		return -ENOMEM;
	}

	tx_intf->num_props = 0;
	list_for_each_entry(ch, &eth_dev->tx_channels, channel_list) {
		ipa_eth_ep_init_tx_props(&tx_intf->prop[tx_intf->num_props++],
			ch, true, vlan_mode);

		ipa_eth_ep_init_tx_props(&tx_intf->prop[tx_intf->num_props++],
			ch, false, vlan_mode);
	}

	return 0;
}

static int ipa_eth_ep_init_rx_intf(
		struct ipa_eth_device *eth_dev,
		struct ipa_rx_intf *rx_intf,
		bool vlan_mode)
{
	u32 num_props;
	struct ipa_eth_channel *ch;

	/* one each for IPv4 and IPv6 */
	num_props = __list_size(&eth_dev->rx_channels) * 2;

	rx_intf->prop = kcalloc(num_props, sizeof(*rx_intf->prop), GFP_KERNEL);
	if (!rx_intf->prop) {
		ipa_eth_dev_err(eth_dev, "Failed to alloc rx props");
		return -ENOMEM;
	}

	rx_intf->num_props = 0;
	list_for_each_entry(ch, &eth_dev->rx_channels, channel_list) {
		ipa_eth_ep_init_rx_props(&rx_intf->prop[rx_intf->num_props++],
			ch, true, vlan_mode);

		ipa_eth_ep_init_rx_props(&rx_intf->prop[rx_intf->num_props++],
			ch, false, vlan_mode);
	}

	return 0;
}

/**
 * ipa_eth_ep_register_interface() - Set Rx and Tx properties and register the
 *                                   interface with IPA
 * @eth_dev: Offload device to register
 *
 * Register a logical interface with IPA. The API expects netdev and channels
 * allocated prior to being called.
 *
 * Return: 0 on success, negative errno code otherwise
 */
int ipa_eth_ep_register_interface(struct ipa_eth_device *eth_dev)
{
	int rc;
	bool vlan_mode;
	struct ipa_tx_intf tx_intf;
	struct ipa_rx_intf rx_intf;

	memset(&tx_intf, 0, sizeof(tx_intf));
	memset(&rx_intf, 0, sizeof(rx_intf));

	rc = ipa3_is_vlan_mode(IPA_VLAN_IF_ETH, &vlan_mode);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Could not determine IPA VLAN mode");
		goto free_and_exit;
	}

	rc = ipa_eth_ep_init_tx_intf(eth_dev, &tx_intf, vlan_mode);
	if (rc)
		goto free_and_exit;

	rc = ipa_eth_ep_init_rx_intf(eth_dev, &rx_intf, vlan_mode);
	if (rc)
		goto free_and_exit;

	rc = ipa_register_intf(eth_dev->net_dev->name, &tx_intf, &rx_intf);
	if (!rc)
		ipa_eth_send_msg_connect(eth_dev);

free_and_exit:
	kzfree(tx_intf.prop);
	kzfree(rx_intf.prop);

	return rc;
}

/**
 * ipa_eth_ep_unregister_interface() - Unregister a previously registered
 *                                     interface
 * @eth_dev: Offload device to unregister
 */
int ipa_eth_ep_unregister_interface(struct ipa_eth_device *eth_dev)
{
	int rc;

	rc = ipa_deregister_intf(eth_dev->net_dev->name);
	if (!rc)
		ipa_eth_send_msg_disconnect(eth_dev);

	return rc;
}

/**
 * ipa_eth_ep_init_ctx - Initialize IPA endpoint context for a channel
 * @ch: Channel for which EP ctx need to be initialized
 * @vlan_mode: true if VLAN mode is enabled for the EP
 */
void ipa_eth_ep_init_ctx(struct ipa_eth_channel *ch, bool vlan_mode)
{
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	if (ep_ctx->valid)
		return;

	memset(ep_ctx, 0, offsetof(typeof(*ep_ctx), sys));

	ep_ctx->valid = 1;
	ep_ctx->client = ch->ipa_client;
	ep_ctx->client_notify = ipa_ep_client_notifier;
	ep_ctx->priv = ch;

	ep_ctx->cfg.nat.nat_en = IPA_CLIENT_IS_PROD(ch->ipa_client) ?
					IPA_SRC_NAT : IPA_BYPASS_NAT;
	ep_ctx->cfg.hdr.hdr_len = vlan_mode ? VLAN_ETH_HLEN : ETH_HLEN;
	ep_ctx->cfg.mode.mode = IPA_BASIC;
}

/**
 * ipa_eth_ep_deinit_ctx - Deinitialize IPA endpoint context for a channel
 * @ch: Channel for which EP ctx need to be deinitialized
 */
void ipa_eth_ep_deinit_ctx(struct ipa_eth_channel *ch)
{
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	if (!ep_ctx->valid)
		return;

	ep_ctx->valid = false;

	memset(ep_ctx, 0, offsetof(typeof(*ep_ctx), sys));
}

static int __ipa_eth_ep_init(struct ipa_eth_channel *ch,
				struct ipa3_ep_context *ep_ctx)
{
	int rc;

	rc = ipa3_cfg_ep(ch->ipa_ep_num, &ep_ctx->cfg);
	if (rc) {
		ipa_eth_dev_err(ch->eth_dev,
				"Failed to configure EP %d", ch->ipa_ep_num);
		goto err_exit;
	}

	if (IPA_CLIENT_IS_PROD(ch->ipa_client))
		ipa3_install_dflt_flt_rules(ch->ipa_ep_num);

err_exit:
	return rc;
}

/**
 * ipa_eth_ep_init() - Initialize IPA endpoint for a channel
 * @ch: Channel for which EP need to be initialized
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_ep_init(struct ipa_eth_channel *ch)
{
	int rc;
	struct ipa3_ep_context *ep_ctx;

	ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];
	if (!ep_ctx->valid) {
		ipa_eth_dev_bug(ch->eth_dev, "EP context is not initialiazed");
		return -EFAULT;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	rc = __ipa_eth_ep_init(ch, ep_ctx);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return rc;
}
EXPORT_SYMBOL(ipa_eth_ep_init);

/**
 * ipa_eth_ep_deinit() - Deinitialize IPA endpoint for a channel
 * @ch: Channel for which EP need to be deinitialized
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_ep_deinit(struct ipa_eth_channel *ch)
{
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	if (IPA_CLIENT_IS_PROD(ch->ipa_client))
		ipa3_delete_dflt_flt_rules(ch->ipa_ep_num);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}
EXPORT_SYMBOL(ipa_eth_ep_deinit);

/**
 * ipa_eth_ep_start() - Start an IPA endpoint
 * @ch: Channel for which the IPA EP need to be started
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_ep_start(struct ipa_eth_channel *ch)
{
	int rc = ipa3_enable_data_path(ch->ipa_ep_num);

	if (rc)
		ipa_eth_dev_err(ch->eth_dev,
				"Failed to start EP %d", ch->ipa_ep_num);

	return rc;
}
EXPORT_SYMBOL(ipa_eth_ep_start);

/**
 * ipa_eth_ep_stop() - Stop an IPA endpoint
 * @ch: Channel for which the IPA EP need to be stopped
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_ep_stop(struct ipa_eth_channel *ch)
{
	int rc = ipa3_disable_data_path(ch->ipa_ep_num);

	if (rc)
		ipa_eth_dev_err(ch->eth_dev,
				"Failed to stop EP %d", ch->ipa_ep_num);

	return rc;
}
EXPORT_SYMBOL(ipa_eth_ep_stop);
