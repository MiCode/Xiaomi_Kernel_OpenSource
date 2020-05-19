// SPDX-License-Identifier: GPL-2.0-only
/* Atlantic Network Driver
 *
 * Copyright (C) 2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/dma-mapping.h>
#include <linux/if_vlan.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/workqueue.h>
#include <net/genetlink.h>

#include "atl_compat.h"
#include "atl_fwdnl.h"

#include "atl_common.h"
#include "atl_fwdnl_params.h"
#include "atl_ring.h"
#include "atl_trace.h"

#define ATL_FWDNL_PREFIX "atl_fwdnl: "
/* Forward declaration, actual definition is at the bottom of the file */
static struct genl_family atlfwd_nl_family;
static bool is_tx_ring_index(const int ring_index);
static bool is_rx_ring_index(const int ring_index);
static bool is_tx_ring(const struct atl_fwd_ring *ring);
static bool is_rx_ring(const struct atl_fwd_ring *ring);
static struct atl_fwd_ring *get_fwd_ring(struct net_device *netdev,
					 const int ring_index,
					 struct genl_info *info,
					 const bool err_if_released);
static int enable_fwd_queue(struct atl_nic *nic);
static int atlfwd_nl_transmit_skb_ring(struct atl_fwd_ring *ring,
				       struct sk_buff *skb);
static void atlfwd_nl_tx_head_poll(struct work_struct *);
static void atlfwd_nl_rx_poll(struct timer_list *);
static int release_ring(struct net_device *ndev, struct genl_info *info,
			struct atl_fwd_ring *ring);
static int disable_ring(struct net_device *ndev, struct genl_info *info,
			struct atl_fwd_ring *ring);
static int release_event(struct net_device *ndev, struct genl_info *info,
			 struct atl_fwd_ring *ring);

/* workqueue data to perform tx cleanup */
struct atlfwd_nl_tx_cleanup_workdata {
	/* NB! delayed_work must be the first field in this structure */
	struct delayed_work dwork;
	struct net_device *ndev;
};

/* data used to perform rx polling */
struct atlfwd_nl_rx_poll_workdata {
	struct atl_fwd_ring *ring;
	struct timer_list timer;
};

/* Register generic netlink family upon module initialization */
int __init atlfwd_nl_init(void)
{
	return genl_register_family(&atlfwd_nl_family);
}

/* Populate private fwdnl data on probe */
void atlfwd_nl_on_probe(struct net_device *ndev)
{
	struct atlfwd_nl_rx_poll_workdata *timer = NULL;
	struct atlfwd_nl_tx_cleanup_workdata *wq = NULL;
	struct atl_nic *nic = netdev_priv(ndev);
	int i = 0;

	nic->fwdnl.force_icmp_via = S32_MIN;
	nic->fwdnl.force_tx_via = S32_MIN;

	wq = devm_kzalloc(&ndev->dev, sizeof(*wq), GFP_KERNEL);
	/* Memory allocation failure is not critical here.
	 * Yes, we won't be able to poll, but we still have other means
	 * of performing the cleanup.
	 */
	if (likely(wq)) {
		INIT_DELAYED_WORK(&wq->dwork, atlfwd_nl_tx_head_poll);
		wq->ndev = ndev;
		nic->fwdnl.tx_cleanup_wq = &wq->dwork;
	}

	for (i = 0; i != ARRAY_SIZE(nic->fwdnl.ring_desc); i++) {
		nic->fwdnl.ring_desc[i].nic = nic;

		if (is_tx_ring_index(i))
			continue;

		timer = devm_kzalloc(&ndev->dev, sizeof(*timer), GFP_KERNEL);
		if (likely(timer)) {
			timer_setup(&timer->timer, atlfwd_nl_rx_poll, 0);
			nic->fwdnl.ring_desc[i].rx_poll_timer = &timer->timer;
		} else
			pr_warn(ATL_FWDNL_PREFIX "RX timer creation failed!");
	}
}

/* Cancel all pending work on remove request */
void atlfwd_nl_on_remove(struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_fwd_ring *ring = NULL;
	int i = 0;

	if (nic->fwdnl.tx_cleanup_wq)
		cancel_delayed_work_sync(nic->fwdnl.tx_cleanup_wq);

	for (i = 0; i != ATL_NUM_FWD_RINGS * 2; i++) {
		ring = get_fwd_ring(ndev, i, NULL, false);
		if (ring)
			release_ring(ndev, NULL, ring);
	}

	for (i = 0; i != ARRAY_SIZE(nic->fwdnl.ring_desc); i++)
		nic->fwdnl.ring_desc[i].rx_poll_timer = NULL;
}

/* Enables FWD egress queue, if necessary */
int atlfwd_nl_on_open(struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);

	if (nic->fwdnl.force_icmp_via != S32_MIN ||
	    nic->fwdnl.force_tx_via != S32_MIN)
		return enable_fwd_queue(nic);

	return 0;
}

/* Unregister generic netlink family upon module exit */
void __exit atlfwd_nl_exit(void)
{
	genl_unregister_family(&atlfwd_nl_family);
}

void atl_fwd_get_ring_stats(struct atl_fwd_ring *ring,
			    struct atl_ring_stats *stats)
{
	struct atl_desc_ring *desc = atlfwd_nl_get_fwd_ring_desc(ring);

	if (likely(desc != NULL)) {
		unsigned int start;

		do {
			start = u64_stats_fetch_begin_irq(&desc->syncp);
			memcpy(stats, &desc->stats, sizeof(*stats));
		} while (u64_stats_fetch_retry_irq(&desc->syncp, start));
	} else {
		memset(stats, 0, sizeof(*stats));
	}
}

/* Returns true, if skb should be sent via FWD. false otherwise. */
static bool atlfwd_nl_should_redirect(const struct sk_buff *skb,
				      struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);

	if (unlikely(nic->fwdnl.force_tx_via != S32_MIN)) {
		/* Redirect everything, but:
		 * . LSO is not supported at the moment
		 * . VLAN is not supported at the moment
		 */
		return (skb_shinfo(skb)->gso_size == 0 &&
			!skb_vlan_tag_present(skb));
	}

	if (unlikely(nic->fwdnl.force_icmp_via != S32_MIN)) {
		uint8_t l4_proto = 0;

		switch (skb->protocol) {
		case htons(ETH_P_IP):
			l4_proto = ip_hdr(skb)->protocol;
			break;
		case htons(ETH_P_IPV6):
			l4_proto = ipv6_hdr(skb)->nexthdr;
			break;
		}

		switch (l4_proto) {
		case IPPROTO_ICMP:
			return true;
		}
	}

	return false;
}

netdev_tx_t atlfwd_nl_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);
	const bool err_if_released = false;
	struct atl_fwd_ring *ring = NULL;
	int ring_index = S32_MIN;

	if (nic->fwdnl.force_tx_via != S32_MIN)
		ring_index = nic->fwdnl.force_tx_via;
	else if (nic->fwdnl.force_icmp_via != S32_MIN)
		ring_index = nic->fwdnl.force_icmp_via;
	else
		return -EFAULT;

	ring = get_fwd_ring(ndev, ring_index, NULL, err_if_released);
	if (unlikely(ring == NULL))
		return -EFAULT;

	return atlfwd_nl_transmit_skb_ring(ring, skb);
}

u16 atlfwd_nl_select_queue_fallback(struct net_device *dev, struct sk_buff *skb,
				    struct net_device *sb_dev,
				    select_queue_fallback_t fallback)
{
	static atomic_t fwd_idx_collisions = ATOMIC_INIT(0);
	struct atl_nic *nic = netdev_priv(dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0) &&                           \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 0)
	u16 idx = fallback(dev, skb);
#else
	u16 idx = fallback(dev, skb, sb_dev);
#endif

	if (likely(dev->real_num_tx_queues == nic->nvecs))
		/* no FWD queues enabled, only standard ones */
		return idx;

	if (atlfwd_nl_should_redirect(skb, dev))
		/* FWD queue enabled, skb meets redirection criteria */
		return nic->nvecs;

	/* FWD queue enabled, but skb doesn't meet redirection criteria.
	 * Send via standard ring.
	 */
	if (idx >= nic->nvecs) {
		atomic_inc_return(&fwd_idx_collisions);
		idx = 0;
	}

	return idx;
}

/* Returns true, if a given TX FWD ring is created/requested.
 * Ring index argument is 0-based, ATL_FWD_RING_BASE is added automatically.
 */
bool atlfwd_nl_is_tx_fwd_ring_created(struct net_device *ndev,
				      const int fwd_ring_index)
{
	const int ring_index = ATL_FWD_RING_BASE + fwd_ring_index;
	struct atl_nic *nic = netdev_priv(ndev);

	return test_bit(ring_index, &nic->fwd.ring_map[ATL_FWDIR_TX]);
}

/* Returns true, if a given RX FWD ring is created/requested.
 * Ring index argument is 0-based, ATL_FWD_RING_BASE is added automatically.
 */
bool atlfwd_nl_is_rx_fwd_ring_created(struct net_device *ndev,
				      const int fwd_ring_index)
{
	const int ring_index = ATL_FWD_RING_BASE + fwd_ring_index;
	struct atl_nic *nic = netdev_priv(ndev);

	return test_bit(ring_index, &nic->fwd.ring_map[ATL_FWDIR_RX]);
}

struct atl_fwd_ring *atlfwd_nl_get_fwd_ring(struct net_device *ndev,
					    const int ring_index)
{
	return get_fwd_ring(ndev, ring_index, NULL, false);
}

/* Set the netlink error message
 *
 * Before Linux 4.12 we could only put it in kernel logs.
 * Starting with 4.12 we can also use extack to pass the message to user-mode.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
#define ATLFWD_NL_SET_ERR_MSG(info, msg)                                       \
	do {                                                                   \
		if (info)                                                      \
			GENL_SET_ERR_MSG(info, msg);                           \
		else                                                           \
			pr_warn(ATL_FWDNL_PREFIX "%s\n", msg);                 \
	} while (0)
#else
#define ATLFWD_NL_SET_ERR_MSG(info, msg) pr_warn(ATL_FWDNL_PREFIX "%s\n", msg)
#endif

/* Returns true, if the given net_device was allocated by FWD driver.
 */
bool is_atlfwd_device(const struct net_device *dev)
{
	static size_t atl_len;

	if (unlikely(atl_len == 0))
		atl_len = strlen(atl_driver_name);

	if (likely(dev && dev->dev.parent)) {
		const char *driver_name = dev_driver_string(dev->dev.parent);
		const size_t len = min_t(size_t, atl_len, strlen(driver_name));

		return (len == atl_len) &&
		       !strncmp(driver_name, atl_driver_name, len);
	}

	return false;
}

/* Attempt to auto-deduce the device
 *
 * This is possible if and only if there's exactly 1 ATL device in the system.
 *
 * Returns NULL on error, net_device pointer otherwise.
 */
static struct net_device *atlfwd_nl_auto_deduce_dev(struct genl_info *info)
{
	struct net *net = genl_info_net(info);
	struct net_device *ndev = NULL;
	bool found = false;

	rcu_read_lock();
	for_each_netdev_rcu (net, ndev) {
		if (is_atlfwd_device(ndev)) {
			dev_hold(ndev);
			found = true;
			break;
		}
	}
	rcu_read_unlock();

	if (!found)
		ATLFWD_NL_SET_ERR_MSG(
			info, "No ATL devices found or a wrong driver is used");

	return found ? ndev : NULL;
}

/* Get net_device by name
 *
 * The name is usually provided by the user-mode tool via the IFNAME attribute.
 * But it can also be null, if the tool hasn't provided the attribute and wants
 * us to attempt to auto deduce the device.
 *
 * Returns NULL on error, net_device pointer otherwise.
 */
static struct net_device *atlfwd_nl_get_dev_by_name(const char *dev_name,
						    struct genl_info *info)
{
	struct net_device *netdev = NULL;

	if (dev_name == NULL) {
		/* No dev_name provided in request, try to auto-deduce. */
		return atlfwd_nl_auto_deduce_dev(info);
	}

	netdev = dev_get_by_name(genl_info_net(info), dev_name);
	if (unlikely(netdev == NULL)) {
		ATLFWD_NL_SET_ERR_MSG(info, "No matching device found");
		return NULL;
	}

	if (unlikely(!is_atlfwd_device(netdev))) {
		ATLFWD_NL_SET_ERR_MSG(
			info,
			"Requested device is not an ATL device or a wrong driver is used");
		goto err_devput;
	}

	return netdev;

err_devput:
	dev_put(netdev);
	return NULL;
}

/* Helper function to obtain the netlink attribute string data
 *
 * Returns NULL, if the attribute doesn't exist.
 */
static const char *
atlfwd_attr_to_str_or_null(struct genl_info *info,
			   const enum atlfwd_nl_attribute attr)
{
	if (likely(!info->attrs[attr]))
		return NULL;

	return (char *)nla_data(info->attrs[attr]);
}

/* Helper function to obtain the netlink attribute s32 data
 *
 * Returns S32_MIN, if attribute is missing. Actual attribute value otherwise.
 */
static int atlfwd_attr_to_s32_priv(struct genl_info *info,
				   const enum atlfwd_nl_attribute attr,
				   const bool optional)
{
	if (info->attrs[attr])
		return nla_get_s32(info->attrs[attr]);

	if (!optional) {
		pr_warn(ATL_FWDNL_PREFIX
			"attribute %d check is missing in pre_doit\n",
			attr);
		ATLFWD_NL_SET_ERR_MSG(
			info,
			"Required attribute is missing (and internal error)");
	}
	return S32_MIN;
}

static int atlfwd_attr_to_s32(struct genl_info *info,
			      const enum atlfwd_nl_attribute attr)
{
	return atlfwd_attr_to_s32_priv(info, attr, false);
}

/* Similar to previous function, but doesn't produce warnings,
 * because attribute is optional.
 */
static int atlfwd_attr_to_s32_optional(struct genl_info *info,
				       const enum atlfwd_nl_attribute attr)
{
	return atlfwd_attr_to_s32_priv(info, attr, true);
}

static bool is_tx_ring_index(const int ring_index)
{
	return ring_index % 2;
}

static bool is_rx_ring_index(const int ring_index)
{
	return !(is_tx_ring_index(ring_index));
}

static bool is_tx_ring(const struct atl_fwd_ring *ring)
{
	return !!(ring->flags & ATL_FWR_TX);
}

static bool is_rx_ring(const struct atl_fwd_ring *ring)
{
	return !(ring->flags & ATL_FWR_TX);
}

/* Converts regular ring index to "normalized" internal representation.
 * This "normalized" value is the only thing known to user-mode.
 */
static int nl_ring_index(const struct atl_fwd_ring *ring)
{
	int idx = 0;

	if (ring->idx < ATL_FWD_RING_BASE) {
		pr_warn(ATL_FWDNL_PREFIX
			"Got an unexpected ring index %d (was expecting %d or greater)\n",
			ring->idx, ATL_FWD_RING_BASE);
		return S32_MIN;
	}

	/* Expose a 0-based index to the user-mode */
	idx = ring->idx - ATL_FWD_RING_BASE;

	/* Use even (0,2,...) indexes for RX rings, odd - for TX. */
	idx *= 2;
	if (is_tx_ring(ring))
		idx++;
	pr_debug(ATL_FWDNL_PREFIX "Normalized ring index %d\n", idx);

	return idx;
}

/* Checks if a given ring index (obtained from user-mode => "normalized")
 * is valid.
 */
static bool is_valid_ring_index(const int ring_index)
{
	return (ring_index >= 0 && ring_index < ATL_NUM_FWD_RINGS * 2);
}

static struct sk_buff *nl_reply_create(void)
{
	return nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
}

static void *nl_reply_init(struct sk_buff *msg, struct genl_info *info)
{
	void *hdr = NULL;

	if (unlikely(msg == NULL))
		return NULL;

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq,
			  &atlfwd_nl_family, 0, info->genlhdr->cmd);
	if (hdr == NULL) {
		ATLFWD_NL_SET_ERR_MSG(info, "Reply message creation failed");
		nlmsg_free(msg);
	}

	return hdr;
}

static bool nl_reply_add_attr(struct sk_buff *msg, void *hdr,
			      struct genl_info *info,
			      const enum atlfwd_nl_attribute attr,
			      const int value)
{
	if (unlikely(msg == NULL))
		return false;

	if (nla_put_s32(msg, attr, value) != 0) {
		ATLFWD_NL_SET_ERR_MSG(info,
				      "Failed to put the reply attribute");
		genlmsg_cancel(msg, hdr);
		nlmsg_free(msg);
		return false;
	}

	return true;
}

static int nl_reply_send(struct sk_buff *msg, void *hdr, struct genl_info *info)
{
	genlmsg_end(msg, hdr);
	return genlmsg_reply(msg, info);
}

static int atlfwd_nl_send_reply(struct genl_info *info,
				const enum atlfwd_nl_attribute attr,
				const int value)
{
	struct sk_buff *msg = nl_reply_create();
	void *hdr = nl_reply_init(msg, info);

	if (unlikely(msg == NULL))
		return -ENOBUFS;
	if (unlikely(hdr == NULL))
		return -EMSGSIZE;

	if (unlikely(!nl_reply_add_attr(msg, hdr, info, attr, value)))
		return -EMSGSIZE;

	return nl_reply_send(msg, hdr, info);
}

/* Get the FWD ring descriptor for a given FWD ring.
 *
 * Returns NULL on error.
 */
struct atl_desc_ring *atlfwd_nl_get_fwd_ring_desc(struct atl_fwd_ring *ring)
{
	int ring_index = S32_MIN;

	if (unlikely(ring == NULL))
		return NULL;

	ring_index = nl_ring_index(ring);
	if (unlikely(ring_index == S32_MIN))
		return NULL;

	return &ring->nic->fwdnl.ring_desc[ring_index];
}

/* Get the FWD ring by index.
 *
 * This function obtains the FWD ring descriptor/pointer from the private
 * part of net_device data.
 *
 * Returns NULL on error.
 */
static struct atl_fwd_ring *get_fwd_ring(struct net_device *netdev,
					 const int ring_index,
					 struct genl_info *info,
					 const bool err_if_released)
{
	const int dir_tx = is_tx_ring_index(ring_index);
	const int norm_index = (dir_tx ? ring_index - 1 : ring_index) / 2;
	struct atl_nic *nic = netdev_priv(netdev);
	struct atl_fwd_ring *ring = nic->fwd.rings[dir_tx][norm_index];

	pr_debug(ATL_FWDNL_PREFIX "index=%d/n%d\n", ring_index, norm_index);

	if (unlikely(norm_index < 0 || norm_index >= ATL_NUM_FWD_RINGS)) {
		ATLFWD_NL_SET_ERR_MSG(info, "Ring index is out of bounds");
		return NULL;
	}

	if (unlikely(ring == NULL && err_if_released))
		ATLFWD_NL_SET_ERR_MSG(info,
				      "Requested ring is NULL / released");

	return ring;
}

static uint32_t atlfwd_nl_ring_hw_head(struct atl_fwd_ring *ring)
{
	return atl_read(&ring->nic->hw, ATL_RING_HEAD(&ring->hw)) & 0x1fff;
}

static uint32_t atlfwd_nl_ring_hw_tail(struct atl_fwd_ring *ring)
{
	return atl_read(&ring->nic->hw, ATL_RING_TAIL(&ring->hw)) & 0x1fff;
}

static int atlfwd_nl_ring_occupied(struct atl_fwd_ring *ring, u32 sw_tail)
{
	int busy = sw_tail - atlfwd_nl_ring_hw_head(ring);

	if (busy < 0)
		busy += ring->hw.size;

	return busy;
}

/* Returns true if the space (number of free elements) is sufficient to store
 * 'needed' amount of elements.
 */
static bool atlfwd_nl_tx_full(struct atl_desc_ring *ring,
			      const unsigned int needed)
{
	int space = ring_space(ring);

	pr_debug(ATL_FWDNL_PREFIX "Needed %u, actual %d\n", needed, space);
	if (likely(space >= needed))
		return false;

	return true;
}

static bool atlfwd_nl_queue_stopped(struct atl_nic *nic,
				    struct atl_desc_ring *ring_desc)
{
	return __netif_subqueue_stopped(nic->ndev, nic->nvecs);
}

static void atlfwd_nl_stop_queue(struct atl_nic *nic,
				 struct atl_desc_ring *ring_desc)
{
	pr_debug(ATL_FWDNL_PREFIX "Stopping TX queue %d\n", nic->nvecs);
	netif_stop_subqueue(nic->ndev, nic->nvecs);
}

static void atlfwd_nl_restart_queue(struct atl_nic *nic,
				    struct atl_desc_ring *ring_desc)
{
	pr_debug(ATL_FWDNL_PREFIX "Restarting TX queue %d\n", nic->nvecs);
	netif_start_subqueue(nic->ndev, nic->nvecs);
	atl_update_ring_stat(ring_desc, tx.tx_restart, 1);
}

static int enable_fwd_queue(struct atl_nic *nic)
{
	if (unlikely(!test_bit(ATL_ST_UP, &nic->hw.state)))
		return 0;

	if (nic->ndev->real_num_tx_queues == nic->nvecs)
		return netif_set_real_num_tx_queues(nic->ndev, nic->nvecs + 1);

	return 0;
}

static void disable_fwd_queue(struct atl_nic *nic)
{
	if (unlikely(!test_bit(ATL_ST_UP, &nic->hw.state)))
		return;

	if (nic->ndev->real_num_tx_queues != nic->nvecs)
		netif_set_real_num_tx_queues(nic->ndev, nic->nvecs);
}

/* Returns true if the space is sufficient after TX queue stop,
 * e.g. if another CPU has freed some space.
 */
static bool atlfwd_nl_tx_full_after_stop(struct atl_nic *nic,
					 struct atl_desc_ring *ring,
					 const unsigned int needed)
{
	atlfwd_nl_stop_queue(nic, ring);

	smp_mb();

	/* Check if another CPU has freed some space */
	return atlfwd_nl_tx_full(ring, needed);
}

/* Returns true, if the number of occupied elements is above a given threshold.
 *
 * E.g. if threshold is 4, then we'll return true here, if number of occupied
 * elements is greater than 1/4 of the total ring size.
 */
static bool atlfwd_nl_tx_above_threshold(struct atl_desc_ring *ring,
					 const unsigned int threshold_frac)
{
	if (unlikely(threshold_frac == 0))
		return false;

	return ring->hw.size - ring_space(ring) >
	       ring->hw.size / threshold_frac;
}

/* Returns checksum offload flags for TX descriptor */
static unsigned int
atlfwd_nl_skb_checksum_offload_cmd(const struct sk_buff *skb)
{
	unsigned int cmd = 0;

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		/* Checksum offload has been requested by the stack */
		uint8_t l4_proto = 0;

		switch (skb->protocol) {
		case htons(ETH_P_IP):
			cmd |= tx_desc_cmd_ipv4cs;
			l4_proto = ip_hdr(skb)->protocol;
			break;
		case htons(ETH_P_IPV6):
			l4_proto = ipv6_hdr(skb)->nexthdr;
			break;
		}

		switch (l4_proto) {
		case IPPROTO_TCP:
		case IPPROTO_UDP:
			cmd |= tx_desc_cmd_l4cs;
			break;
		}
	}

	return cmd;
}

static unsigned int atlfwd_nl_num_txd_for_skb(struct sk_buff *skb)
{
	unsigned int len = skb_headlen(skb);
	unsigned int num_txd =
		(len - len % ATL_DATA_PER_TXD) / ATL_DATA_PER_TXD + 1;
	unsigned int frag;

	for (frag = 0; frag != skb_shinfo(skb)->nr_frags; frag++) {
		len = skb_frag_size(&skb_shinfo(skb)->frags[frag]);
		num_txd +=
			(len - len % ATL_DATA_PER_TXD) / ATL_DATA_PER_TXD + 1;
	}

	return num_txd;
}

/* Returns true, if HW has completed processing (head is equal to tail).
 * Returns false, if there some work is still pending.
 */
static bool atlfwd_nl_tx_head_poll_ring(struct atl_fwd_ring *ring)
{
	struct atl_desc_ring *desc = atlfwd_nl_get_fwd_ring_desc(ring);
	uint32_t budget = atl_tx_clean_budget;
	unsigned int free_high = 0;
	struct device *dev = NULL;
	unsigned int packets = 0;
	unsigned int bytes = 0;
	uint32_t hw_head = 0;
	uint32_t sw_head = 0;

	if (unlikely(desc == NULL))
		return true;

	dev = &ring->nic->hw.pdev->dev;
	hw_head = atlfwd_nl_ring_hw_head(ring);
	sw_head = READ_ONCE(desc->head);
	pr_debug(ATL_FWDNL_PREFIX "hw_head=%d, sw_head=%d\n", hw_head, sw_head);

	do {
		struct atl_txbuf *txbuf = &desc->txbufs[sw_head];

		if (sw_head == hw_head)
			break;

		bytes += txbuf->bytes;
		packets += txbuf->packets;

		if (dma_unmap_len(txbuf, len)) {
			dma_unmap_single(dev, dma_unmap_addr(txbuf, daddr),
					 dma_unmap_len(txbuf, len),
					 DMA_TO_DEVICE);
			dma_unmap_len_set(txbuf, len, 0);
		}

		if (txbuf->skb) {
			napi_consume_skb(txbuf->skb, budget);
			txbuf->skb = NULL;
		}

		bump_ptr(sw_head, desc, 1);
	} while (budget--);

	WRITE_ONCE(desc->head, sw_head);
	pr_debug(ATL_FWDNL_PREFIX "bytes=%u, packets=%u, sw_head=%d\n", bytes,
		 packets, sw_head);

	u64_stats_update_begin(&desc->syncp);
	desc->stats.tx.bytes += bytes;
	desc->stats.tx.packets += packets;
	u64_stats_update_end(&desc->syncp);

	free_high = min_t(typeof(atl_tx_free_high), atl_tx_free_high,
			  ring->hw.size - 1);
	if (unlikely(atlfwd_nl_queue_stopped(ring->nic, desc)) &&
	    likely(test_bit(ATL_ST_RINGS_RUNNING, &ring->nic->hw.state)) &&
	    likely(!atlfwd_nl_tx_full(desc, free_high))) {
		atlfwd_nl_restart_queue(ring->nic, desc);
	}

	return (hw_head == READ_ONCE(desc->tail));
}

static void atlfwd_nl_tx_head_poll(struct work_struct *work)
{
	struct atlfwd_nl_tx_cleanup_workdata *data =
		(struct atlfwd_nl_tx_cleanup_workdata *)work;
	struct net_device *ndev = data->ndev;
	struct atl_nic *nic = netdev_priv(ndev);
	const bool err_if_released = false;
	struct atl_fwd_ring *ring = NULL;
	bool poll_finished = true;
	int ring_index = S32_MIN;

	ring_index = nic->fwdnl.force_tx_via;
	if (ring_index != S32_MIN) {
		ring = get_fwd_ring(ndev, ring_index, NULL, err_if_released);
		if (!atlfwd_nl_tx_head_poll_ring(ring))
			poll_finished = false;
	}

	if (ring_index != nic->fwdnl.force_icmp_via) {
		ring_index = nic->fwdnl.force_icmp_via;
		if (ring_index != S32_MIN) {
			ring = get_fwd_ring(ndev, ring_index, NULL,
					    err_if_released);
			if (!atlfwd_nl_tx_head_poll_ring(ring))
				poll_finished = false;
		}
	}

	if (!poll_finished && likely(atlfwd_nl_tx_clean_threshold_msec != 0))
		schedule_delayed_work(
			nic->fwdnl.tx_cleanup_wq,
			msecs_to_jiffies(atlfwd_nl_tx_clean_threshold_msec));
}

static int atlfwd_nl_receive_skb(struct atl_desc_ring *ring,
				 struct sk_buff *skb)
{
	struct net_device *ndev = ring->nic->ndev;

	return atl_fwd_receive_skb(ndev, skb);
}

static void atlfwd_nl_rx_poll(struct timer_list *timer)
{
	struct atlfwd_nl_rx_poll_workdata *data =
		from_timer(data, timer, timer);
	struct atl_desc_ring *desc = atlfwd_nl_get_fwd_ring_desc(data->ring);
	int budget = atlfwd_nl_rx_clean_budget;

	atl_clean_rx(desc, budget, atlfwd_nl_receive_skb);

	if (likely(altfwd_nl_rx_poll_interval_msec != 0)) {
		timer->expires =
			jiffies +
			msecs_to_jiffies(altfwd_nl_rx_poll_interval_msec);
		add_timer(timer);
	}
}

static void atl_fwd_txbuf_free(struct device *dev, struct atl_txbuf *txbuf)
{
	if (dma_unmap_len(txbuf, len)) {
		dma_unmap_single(dev, dma_unmap_addr(txbuf, daddr),
				 dma_unmap_len(txbuf, len), DMA_TO_DEVICE);
	}

	memset(txbuf, 0, sizeof(*txbuf));
}

static int atlfwd_nl_transmit_skb_ring(struct atl_fwd_ring *ring,
				       struct sk_buff *skb)
{
	const unsigned int num_txd = atlfwd_nl_num_txd_for_skb(skb);
	unsigned int frags = skb_shinfo(skb)->nr_frags;
	struct atl_desc_ring *ring_desc = NULL;
	unsigned int frag_len = skb_headlen(skb);
	struct atl_txbuf *first_buf = NULL;
	struct atl_nic *nic = ring->nic;
	u32 bunch = nic->fwdnl.tx_bunch;
	struct atl_txbuf *txbuf = NULL;
	struct atl_hw *hw = &nic->hw;
	struct device *dev = &hw->pdev->dev;
	unsigned int free_low = 0;
	dma_addr_t frag_daddr = 0;
	skb_frag_t *frag = NULL;
	struct atl_tx_desc desc;
	uint32_t desc_idx;

	ring_desc = atlfwd_nl_get_fwd_ring_desc(ring);
	if (unlikely(ring_desc == NULL))
		return -EFAULT;

	desc_idx = ring_desc->tail;

	if (atlfwd_nl_tx_above_threshold(ring_desc,
					 atlfwd_nl_tx_clean_threshold_frac)) {
		cancel_delayed_work_sync(nic->fwdnl.tx_cleanup_wq);
		atlfwd_nl_tx_head_poll(&nic->fwdnl.tx_cleanup_wq->work);
	}

	if (unlikely(atlfwd_nl_tx_full(ring_desc, num_txd))) {
		if (atlfwd_nl_tx_full_after_stop(nic, ring_desc, num_txd)) {
			atl_update_ring_stat(ring_desc, tx.tx_busy, 1);
			return NETDEV_TX_BUSY;
		}

		atlfwd_nl_restart_queue(nic, ring_desc);
	}

	memset(&desc, 0, sizeof(desc));
	desc.cmd = tx_desc_cmd_fcs;
	desc.cmd |= atlfwd_nl_skb_checksum_offload_cmd(skb);
	desc.ct_en = 0;
	desc.type = tx_desc_type_desc;
	desc.pay_len = skb->len;

	first_buf = &ring_desc->txbufs[desc_idx];
	txbuf = first_buf;
	memset(txbuf, 0, sizeof(*txbuf));

	frag_daddr = dma_map_single(dev, skb->data, frag_len, DMA_TO_DEVICE);
	for (frag = &skb_shinfo(skb)->frags[0];; frag++) {
		dma_addr_t daddr = frag_daddr;
		unsigned int len = frag_len;

		if (dma_mapping_error(dev, daddr))
			goto err_dma;

		desc.daddr = cpu_to_le64(daddr);
		while (len > ATL_DATA_PER_TXD) {
			desc.len = cpu_to_le16(ATL_DATA_PER_TXD);
			txbuf->bytes = ATL_DATA_PER_TXD;
			WRITE_ONCE(ring->hw.descs[desc_idx].tx, desc);
			bump_ptr(desc_idx, ring_desc, 1);
			txbuf = &ring_desc->txbufs[desc_idx];
			memset(txbuf, 0, sizeof(*txbuf));
			daddr += ATL_DATA_PER_TXD;
			len -= ATL_DATA_PER_TXD;
			desc.daddr = cpu_to_le64(daddr);
		}
		desc.len = cpu_to_le16(len);
		txbuf->bytes = len; /* populate bytes for each descriptor */
		/* populate DMA addr/len for last descriptor of the fragment
		 * (it's safe to unmap once HW processes this descriptor)
		 */
		dma_unmap_len_set(txbuf, len, frag_len);
		dma_unmap_addr_set(txbuf, daddr, frag_daddr);

		if (!frags)
			break;

		WRITE_ONCE(ring->hw.descs[desc_idx].tx, desc);
		bump_ptr(desc_idx, ring_desc, 1);
		txbuf = &ring_desc->txbufs[desc_idx];
		memset(txbuf, 0, sizeof(*txbuf));
		frag_len = skb_frag_size(frag);
		frag_daddr =
			skb_frag_dma_map(dev, frag, 0, frag_len, DMA_TO_DEVICE);

		frags--;
	}

	/* Last descriptor */
	desc.eop = 1;
#if defined(ATL_TX_DESC_WB) || defined(ATL_TX_HEAD_WB)
	desc.cmd |= tx_desc_cmd_wb;
#endif
	/* populate skb for each packet
	 * (it's safe to consume skb once HW processes this descriptor)
	 */
	txbuf->packets = 1;
	txbuf->skb = skb;
	WRITE_ONCE(ring->hw.descs[desc_idx].tx, desc);
	bump_ptr(desc_idx, ring_desc, 1);
	ring_desc->tail = desc_idx;

	if (likely(atlfwd_nl_tx_clean_threshold_msec != 0))
		schedule_delayed_work(
			nic->fwdnl.tx_cleanup_wq,
			msecs_to_jiffies(atlfwd_nl_tx_clean_threshold_msec));

	/* Stop the queue, if there is no space for another packet */
	free_low = min_t(typeof(atl_tx_free_low), atl_tx_free_low,
			 ring->hw.size / 2);
	if (unlikely(atlfwd_nl_tx_full(ring_desc, free_low))) {
		if (atlfwd_nl_speculative_queue_stop)
			atlfwd_nl_stop_queue(nic, ring_desc);
	} else if (skb_xmit_more(skb)) {
		/* Delay bumping the HW tail if another packet is pending */
		return NETDEV_TX_OK;
	}

	wmb();

	if (atlfwd_nl_ring_occupied(ring, desc_idx) > bunch)
		atl_write(hw, ATL_TX_RING_TAIL(ring), desc_idx);

	return NETDEV_TX_OK;

err_dma:
	dev_err(dev, "%s failed\n", __func__);
	for (;;) {
		atl_fwd_txbuf_free(dev, txbuf);
		if (txbuf == first_buf)
			break;
		bump_ptr(desc_idx, ring_desc, -1);
		txbuf = &ring_desc->txbufs[desc_idx];
	}
	atl_update_ring_stat(ring_desc, tx.dma_map_failed, 1);
	return -EFAULT;
}

static struct net_device *
get_ndev_or_null(struct genl_info *info,
		 const enum atlfwd_nl_attribute ifname_attr)
{
	const char *ifname = atlfwd_attr_to_str_or_null(info, ifname_attr);

	return atlfwd_nl_get_dev_by_name(ifname, info);
}

typedef int (*no_attr_handler)(struct net_device *dev, struct genl_info *info);
static int cmd_with_no_attr(struct sk_buff *skb, struct genl_info *info,
			    no_attr_handler handler)
{
	struct net_device *ndev = get_ndev_or_null(info, ATL_FWD_ATTR_IFNAME);
	int result = 0;

	if (ndev == NULL)
		return -ENODEV;

	result = handler(ndev, info);

	dev_put(ndev);
	return result;
}

#define MANDATORY_ATTR true
#define OPTIONAL_ATTR false
typedef int (*s32_attr_handler)(struct net_device *dev, struct genl_info *info,
				const int value);
static int cmd_with_s32_attr(struct sk_buff *skb, struct genl_info *info,
			     const enum atlfwd_nl_attribute s32_attr,
			     const bool mandatory, s32_attr_handler handler)
{
	struct net_device *ndev = get_ndev_or_null(info, ATL_FWD_ATTR_IFNAME);
	int value = S32_MIN;
	int result = 0;

	if (ndev == NULL)
		return -ENODEV;

	if (mandatory) {
		value = atlfwd_attr_to_s32(info, s32_attr);
		if (unlikely(value == S32_MIN)) {
			result = -EINVAL;
			goto err_netdev;
		}
	} else {
		value = atlfwd_attr_to_s32_optional(info, s32_attr);
	}

	result = handler(ndev, info, value);

err_netdev:
	dev_put(ndev);
	return result;
}

typedef int (*ring_attr_handler)(struct net_device *dev, struct genl_info *info,
				 struct atl_fwd_ring *ring);
static int cmd_with_ring_index_attr(struct sk_buff *skb, struct genl_info *info,
				    ring_attr_handler handler)
{
	struct net_device *ndev = get_ndev_or_null(info, ATL_FWD_ATTR_IFNAME);
	const bool err_if_released = true;
	struct atl_fwd_ring *ring = NULL;
	int ring_index = S32_MIN;
	int result = 0;

	if (ndev == NULL)
		return -ENODEV;

	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring = get_fwd_ring(ndev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL)) {
		result = -EINVAL;
		goto err_netdev;
	}

	result = handler(ndev, info, ring);

err_netdev:
	dev_put(ndev);
	return result;
}

/* ATL_FWD_CMD_REQUEST_RING handler */
static int request_ring(struct net_device *ndev, struct genl_info *info)
{
	const int page_order =
		atlfwd_attr_to_s32(info, ATL_FWD_ATTR_PAGE_ORDER);
	const int ring_size = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_SIZE);
	const int buf_size = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_BUF_SIZE);
	const int flags = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_FLAGS);
	struct atl_desc_ring *ring_desc = NULL;
	struct atl_fwd_ring *ring = NULL;
	int ring_index = S32_MIN;
	int result = 0;

	if (unlikely(flags == S32_MIN || ring_size == S32_MIN ||
		     buf_size == S32_MIN || page_order == S32_MIN))
		return -EINVAL;

	pr_debug(ATL_FWDNL_PREFIX "flags=%d\n", flags);
	pr_debug(ATL_FWDNL_PREFIX "ring_size=%d\n", ring_size);
	pr_debug(ATL_FWDNL_PREFIX "buf_size=%d\n", buf_size);
	pr_debug(ATL_FWDNL_PREFIX "page_order=%d\n", page_order);

	ring = atl_fwd_request_ring(ndev, flags, ring_size, buf_size,
				    page_order, NULL);
	if (IS_ERR_OR_NULL(ring))
		return PTR_ERR(ring);

	pr_debug(ATL_FWDNL_PREFIX "Got ring %d (%p)\n", ring->idx, ring);

	ring_desc = atlfwd_nl_get_fwd_ring_desc(ring);
	ring_index = nl_ring_index(ring);
	if (unlikely(ring_desc == NULL || ring_index == S32_MIN)) {
		ATLFWD_NL_SET_ERR_MSG(info, "Internal error");
		result = -EFAULT;
		goto err_relring;
	}

	memcpy(&ring_desc->hw, &ring->hw, sizeof(ring_desc->hw));

	if (is_tx_ring(ring)) {
		ring_desc->txbufs = kcalloc(
			ring_size, sizeof(*ring_desc->txbufs), GFP_KERNEL);
		if (unlikely(ring_desc->txbufs == NULL)) {
			result = -ENOMEM;
			goto err_relring;
		}

		result = atl_init_tx_ring(ring_desc);
	} else {
		ring_desc->rxbufs = kcalloc(
			ring_size, sizeof(*ring_desc->rxbufs), GFP_KERNEL);
		if (unlikely(ring_desc->rxbufs == NULL)) {
			result = -ENOMEM;
			goto err_relring;
		}

		result = atl_init_rx_ring(ring_desc);
	}
	if (result)
		goto err_relring;

	u64_stats_init(&ring_desc->syncp);
	memset(&ring_desc->stats, 0, sizeof(ring_desc->stats));

	return atlfwd_nl_send_reply(info, ATL_FWD_ATTR_RING_INDEX, ring_index);

err_relring:
	atl_fwd_release_ring(ring);
	return result;
}

static int doit_request_ring(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_no_attr(skb, info, request_ring);
}

/* ATL_FWD_CMD_RELEASE_RING handler */
static int release_ring(struct net_device *ndev, struct genl_info *info,
			struct atl_fwd_ring *ring)
{
	struct atl_desc_ring *ring_desc = atlfwd_nl_get_fwd_ring_desc(ring);

	if (unlikely(ring_desc == NULL)) {
		ATLFWD_NL_SET_ERR_MSG(info, "Internal error");
		return -EFAULT;
	}

	disable_ring(ndev, info, ring);
	release_event(ndev, info, ring);

	memset(&ring_desc->hw, 0, sizeof(ring_desc->hw));
	if (is_tx_ring(ring)) {
		kfree(ring_desc->txbufs);
		ring_desc->txbufs = NULL;
	} else {
		kfree(ring_desc->rxbufs);
		ring_desc->rxbufs = NULL;
	}

	pr_debug(ATL_FWDNL_PREFIX "Releasing ring %d (%p)\n",
		 nl_ring_index(ring), ring);
	atl_fwd_release_ring(ring);

	return 0;
}

static int doit_release_ring(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, release_ring);
}

/* ATL_FWD_CMD_ENABLE_RING handler */
static int enable_ring(struct net_device *ndev, struct genl_info *info,
		       struct atl_fwd_ring *ring)
{
	struct atl_desc_ring *desc = NULL;

	pr_debug(ATL_FWDNL_PREFIX "Enabling ring %d (%p)\n",
		 nl_ring_index(ring), ring);

	if (is_rx_ring(ring)) {
		struct atlfwd_nl_rx_poll_workdata *data = NULL;

		desc = atlfwd_nl_get_fwd_ring_desc(ring);
		if (unlikely(desc == NULL)) {
			ATLFWD_NL_SET_ERR_MSG(info, "Internal error");
			return -EFAULT;
		}

		if (unlikely(desc->rx_poll_timer == NULL)) {
			ATLFWD_NL_SET_ERR_MSG(
				info,
				"RX polling timer doesn't exist. The ring might not function properly.");
			goto err_enablering;
		}

		data = from_timer(data, desc->rx_poll_timer, timer);
		data->ring = ring;
		if (likely(altfwd_nl_rx_poll_interval_msec != 0)) {
			desc->rx_poll_timer->expires =
				jiffies +
				msecs_to_jiffies(
					altfwd_nl_rx_poll_interval_msec);
			add_timer(desc->rx_poll_timer);
		}
	}

err_enablering:
	return atl_fwd_enable_ring(ring);
}

static int doit_enable_ring(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, enable_ring);
}

/* ATL_FWD_CMD_DISABLE_RING handler */
static int disable_ring(struct net_device *ndev, struct genl_info *info,
			struct atl_fwd_ring *ring)
{
	struct atl_desc_ring *desc = NULL;

	pr_debug(ATL_FWDNL_PREFIX "Disabling ring %d (%p)\n",
		 nl_ring_index(ring), ring);
	atl_fwd_disable_ring(ring);

	if (is_tx_ring(ring))
		return 0;

	// RX ring
	desc = atlfwd_nl_get_fwd_ring_desc(ring);
	if (unlikely(desc == NULL || desc->rx_poll_timer == NULL))
		return 0;

	del_timer_sync(desc->rx_poll_timer);
	atl_clear_rx_bufs(desc);

	return 0;
}

static int doit_disable_ring(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, disable_ring);
}

/* ATL_FWD_CMD_DUMP_RING handler */
static int dump_ring(struct net_device *ndev, struct genl_info *info,
		     struct atl_fwd_ring *ring)
{
	const uint32_t head = atlfwd_nl_ring_hw_head(ring);
	const uint32_t tail = atlfwd_nl_ring_hw_tail(ring);
	bool dir_tx = is_tx_ring(ring);
	int j;

	pr_debug(ATL_FWDNL_PREFIX "Dumping ring %d (%p)\n", nl_ring_index(ring),
		 ring);

	pr_info("[%d] head=%d tail=%d", ring->idx, head, tail);

	for (j = 0; j != ring->hw.size; j++) {
		if (dir_tx) {
			pr_info("[%d] [%d] 0x%llx, DD=%d", ring->idx, j,
				ring->hw.descs[j].tx.daddr,
				ring->hw.descs[j].tx.dd);
			trace_atl_tx_descr(ring->idx, j,
					   (u64 *)ring->hw.descs[j].raw);
		} else {
			pr_info("[%d] [%d] 0x%llx, DD=%d", ring->idx, j,
				ring->hw.descs[j].rx.daddr,
				ring->hw.descs[j].rx.dd);
			trace_atl_rx_descr(ring->idx, j,
					   (u64 *)ring->hw.descs[j].raw);
		}
	}

	return 0;
}

static int doit_dump_ring(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, dump_ring);
}

/* ATL_FWD_CMD_SET_TX_BUNCH handler */
static int set_tx_bunch(struct net_device *ndev, struct genl_info *info,
			const int bunch)
{
	struct atl_nic *nic = netdev_priv(ndev);

	pr_debug(ATL_FWDNL_PREFIX "Set TX bunch %d\n", bunch);
	nic->fwdnl.tx_bunch = bunch;

	return 0;
}

static int doit_set_tx_bunch(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_s32_attr(skb, info, ATL_FWD_ATTR_TX_BUNCH_SIZE,
				 MANDATORY_ATTR, set_tx_bunch);
}

/* ATL_FWD_CMD_REQUEST_EVENT handler */
static int request_event(struct net_device *ndev, struct genl_info *info,
			 struct atl_fwd_ring *ring)
{
	struct atl_desc_ring *desc = atlfwd_nl_get_fwd_ring_desc(ring);
	int flags;

	if (is_tx_ring(ring)) {
		if (unlikely(ring->evt)) {
			ATLFWD_NL_SET_ERR_MSG(info, "Event exists already.");
			return -EFAULT;
		}

		desc->tx_evt = kzalloc(sizeof(*desc->tx_evt), GFP_KERNEL);
		if (unlikely(!desc->tx_evt))
			return -ENOBUFS;

		/* right now we support head pointer writeback only */
		flags = ATL_FWD_EVT_TXWB;
		desc->tx_evt->ring = ring;
		desc->tx_evt->flags = flags;
		if (desc->tx_evt->flags & ATL_FWD_EVT_TXWB)
			desc->tx_evt->tx_head_wrb = dma_map_single(
				&ring->nic->hw.pdev->dev, &desc->tx_hw_head,
				sizeof(desc->tx_hw_head), DMA_FROM_DEVICE);

		pr_debug(ATL_FWDNL_PREFIX "Requesting event for ring %d (%p)\n",
			 nl_ring_index(ring), ring);

		return atl_fwd_request_event(desc->tx_evt);
	}

	return 0;
}

static int doit_request_event(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, request_event);
}

/* ATL_FWD_CMD_RELEASE_EVENT handler */
static int release_event(struct net_device *ndev, struct genl_info *info,
			 struct atl_fwd_ring *ring)
{
	if (is_tx_ring(ring) && ring->evt) {
		struct atl_desc_ring *desc = atlfwd_nl_get_fwd_ring_desc(ring);

		pr_debug(ATL_FWDNL_PREFIX "Releasing event for ring %d (%p)\n",
			 nl_ring_index(ring), ring);
		atl_fwd_release_event(ring->evt);

		if (desc->tx_evt->flags & ATL_FWD_EVT_TXWB)
			dma_unmap_single(&ring->nic->hw.pdev->dev,
					 desc->tx_evt->tx_head_wrb,
					 sizeof(desc->tx_hw_head),
					 DMA_FROM_DEVICE);

		kfree(desc->tx_evt);
		desc->tx_evt = NULL;
	}

	return 0;
}

static int doit_release_event(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, release_event);
}

/* ATL_FWD_CMD_ENABLE_EVENT handler */
static int enable_event(struct net_device *ndev, struct genl_info *info,
			struct atl_fwd_ring *ring)
{
	if (is_tx_ring(ring) && ring->evt) {
		pr_debug(ATL_FWDNL_PREFIX "Enabling event for ring %d (%p)\n",
			 nl_ring_index(ring), ring);

		return atl_fwd_enable_event(ring->evt);
	}

	return 0;
}
static int doit_enable_event(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, enable_event);
}

/* ATL_FWD_CMD_DISABLE_EVENT handler */
static int disable_event(struct net_device *ndev, struct genl_info *info,
			 struct atl_fwd_ring *ring)
{
	if (is_tx_ring(ring) && ring->evt) {
		pr_debug(ATL_FWDNL_PREFIX "Disabling event for ring %d (%p)\n",
			 nl_ring_index(ring), ring);

		return atl_fwd_disable_event(ring->evt);
	}

	return 0;
}

static int doit_disable_event(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, disable_event);
}

/* ATL_FWD_CMD_DISABLE_REDIRECTIONS handler */
static int disable_redirections(struct net_device *ndev, struct genl_info *info)
{
	struct atl_nic *nic = netdev_priv(ndev);

	pr_debug(ATL_FWDNL_PREFIX "All forced redirections are disabled now\n");
	disable_fwd_queue(nic);
	nic->fwdnl.force_icmp_via = S32_MIN;
	nic->fwdnl.force_tx_via = S32_MIN;

	return 0;
}

static int doit_disable_redirections(struct sk_buff *skb,
				     struct genl_info *info)
{
	return cmd_with_no_attr(skb, info, disable_redirections);
}

/* ATL_FWD_CMD_FORCE_ICMP_TX_VIA handler */
static int force_icmp_tx_via(struct net_device *ndev, struct genl_info *info,
			     struct atl_fwd_ring *ring)
{
	struct atl_nic *nic = netdev_priv(ndev);
	int ring_index = 0;

	if (unlikely(!is_tx_ring(ring))) {
		ATLFWD_NL_SET_ERR_MSG(info, "Expected TX ring");
		return -EINVAL;
	}

	ring_index = nl_ring_index(ring);
	pr_debug(ATL_FWDNL_PREFIX
		 "All egress ICMP traffic is now forced via ring %d (%p)\n",
		 ring_index, ring);
	nic->fwdnl.force_icmp_via = ring_index;
	enable_fwd_queue(nic);

	return 0;
}

static int doit_force_icmp_tx_via(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, force_icmp_tx_via);
}

/* ATL_FWD_CMD_FORCE_TX_VIA handler */
static int force_tx_via(struct net_device *ndev, struct genl_info *info,
			struct atl_fwd_ring *ring)
{
	struct atl_nic *nic = netdev_priv(ndev);
	int ring_index = 0;

	if (unlikely(!is_tx_ring(ring))) {
		ATLFWD_NL_SET_ERR_MSG(info, "Expected TX ring");
		return -EINVAL;
	}

	ring_index = nl_ring_index(ring);
	pr_debug(ATL_FWDNL_PREFIX
		 "All egress traffic is now forced via ring %d (%p)\n",
		 ring_index, ring);
	nic->fwdnl.force_tx_via = ring_index;
	enable_fwd_queue(nic);

	return 0;
}

static int doit_force_tx_via(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, force_tx_via);
}

/* ATL_FWD_CMD_RING_STATUS processor */
static int atlfwd_nl_add_ring_status(struct net_device *netdev,
				     struct sk_buff *msg, void *hdr,
				     struct genl_info *info,
				     const int ring_index)
{
	const bool err_if_released = false;
	const struct atl_fwd_ring *ring =
		get_fwd_ring(netdev, ring_index, info, err_if_released);

	pr_debug(ATL_FWDNL_PREFIX "Ring %d (%p) status\n", ring_index, ring);

	if (unlikely(!nl_reply_add_attr(msg, hdr, info, ATL_FWD_ATTR_RING_INDEX,
					ring_index)))
		return -EMSGSIZE;

	if (unlikely(!nl_reply_add_attr(msg, hdr, info, ATL_FWD_ATTR_RING_IS_TX,
					is_tx_ring_index(ring_index))))
		return -EMSGSIZE;

	if (ring != NULL) {
		if (unlikely(!nl_reply_add_attr(
			    msg, hdr, info, ATL_FWD_ATTR_RING_STATUS,
			    (!(ring->state & ATL_FWR_ST_ENABLED) ?
				     ATL_FWD_RING_STATUS_CREATED_DISABLED :
				     ATL_FWD_RING_STATUS_ENABLED))))
			return -EMSGSIZE;
		if (unlikely(!nl_reply_add_attr(msg, hdr, info,
						ATL_FWD_ATTR_RING_SIZE,
						ring->hw.size)))
			return -EMSGSIZE;
		if (unlikely(!nl_reply_add_attr(msg, hdr, info,
						ATL_FWD_ATTR_RING_FLAGS,
						ring->flags)))
			return -EMSGSIZE;
	} else {
		if (unlikely(!nl_reply_add_attr(msg, hdr, info,
						ATL_FWD_ATTR_RING_STATUS,
						ATL_FWD_RING_STATUS_RELEASED)))
			return -EMSGSIZE;
	}

	return 0;
}

static int ring_status(struct net_device *ndev, struct genl_info *info,
		       const int ring_index)
{
	struct sk_buff *msg = nl_reply_create();
	void *hdr = nl_reply_init(msg, info);
	int last_idx = ATL_NUM_FWD_RINGS * 2;
	int first_idx = 0;
	int result = 0;
	int idx = 0;

	if (unlikely(msg == NULL))
		return -ENOBUFS;
	if (unlikely(hdr == NULL))
		return -EMSGSIZE;

	if (ring_index != S32_MIN) {
		if (unlikely(!is_valid_ring_index(ring_index)))
			return -EINVAL;

		first_idx = ring_index;
		last_idx = ring_index + 1;
	}

	for (idx = first_idx; idx != last_idx; idx++) {
		result = atlfwd_nl_add_ring_status(ndev, msg, hdr, info, idx);
		if (unlikely(result < 0))
			return result;
	}

	return nl_reply_send(msg, hdr, info);
}

static int doit_ring_status(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_s32_attr(skb, info, ATL_FWD_ATTR_RING_INDEX,
				 OPTIONAL_ATTR, ring_status);
}

/* ATL_FWD_CMD_GET_RX_QUEUE/ATL_FWD_CMD_GET_TX_QUEUE processor */
static int get_queue_index(struct net_device *ndev, struct genl_info *info,
			   struct atl_fwd_ring *ring)
{
	struct sk_buff *msg = nl_reply_create();
	void *hdr = nl_reply_init(msg, info);

	if (unlikely(msg == NULL))
		return -ENOBUFS;
	if (unlikely(hdr == NULL))
		return -EMSGSIZE;

	if (unlikely(!nl_reply_add_attr(msg, hdr, info,
					ATL_FWD_ATTR_QUEUE_INDEX, ring->idx)))
		return -EMSGSIZE;

	return nl_reply_send(msg, hdr, info);
}
static int doit_get_queue(struct sk_buff *skb, struct genl_info *info)
{
	return cmd_with_ring_index_attr(skb, info, get_queue_index);
}

/* This handler is called before the actual command handler.
 *
 * Returns 0 on success, error otherwise.
 */
static int atlfwd_nl_pre_doit(const struct genl_ops *ops, struct sk_buff *skb,
			      struct genl_info *info)
{
	enum atlfwd_nl_attribute missing_attr = ATL_FWD_ATTR_INVALID;
	int ring_index = S32_MIN;
	int ret = 0;

	/* First, check that all mandatory attributes are present */
	switch (ops->cmd) {
	case ATL_FWD_CMD_REQUEST_RING:
		if (!info->attrs[ATL_FWD_ATTR_FLAGS])
			missing_attr = ATL_FWD_ATTR_FLAGS;
		else if (!info->attrs[ATL_FWD_ATTR_RING_SIZE])
			missing_attr = ATL_FWD_ATTR_RING_SIZE;
		else if (!info->attrs[ATL_FWD_ATTR_BUF_SIZE])
			missing_attr = ATL_FWD_ATTR_BUF_SIZE;
		else if (!info->attrs[ATL_FWD_ATTR_PAGE_ORDER])
			missing_attr = ATL_FWD_ATTR_PAGE_ORDER;
		break;
	case ATL_FWD_CMD_RELEASE_RING:
	case ATL_FWD_CMD_ENABLE_RING:
	case ATL_FWD_CMD_DISABLE_RING:
	case ATL_FWD_CMD_REQUEST_EVENT:
	case ATL_FWD_CMD_RELEASE_EVENT:
	case ATL_FWD_CMD_ENABLE_EVENT:
	case ATL_FWD_CMD_DISABLE_EVENT:
	case ATL_FWD_CMD_DUMP_RING:
	case ATL_FWD_CMD_FORCE_ICMP_TX_VIA:
	case ATL_FWD_CMD_FORCE_TX_VIA:
	case ATL_FWD_CMD_GET_RX_QUEUE:
	case ATL_FWD_CMD_GET_TX_QUEUE:
		if (!info->attrs[ATL_FWD_ATTR_RING_INDEX])
			missing_attr = ATL_FWD_ATTR_RING_INDEX;
		break;
	case ATL_FWD_CMD_RING_STATUS:
		/* the only attribute is optional => nothing to check */
		break;
	case ATL_FWD_CMD_SET_TX_BUNCH:
		if (!info->attrs[ATL_FWD_ATTR_TX_BUNCH_SIZE])
			missing_attr = ATL_FWD_ATTR_TX_BUNCH_SIZE;
		break;
	case ATL_FWD_CMD_DISABLE_REDIRECTIONS:
		/* no attributes => nothing to check */
		break;
	default:
		ATLFWD_NL_SET_ERR_MSG(info, "Unknown command");
		return -EINVAL;
	}

	if (unlikely(missing_attr != ATL_FWD_ATTR_INVALID)) {
		pr_warn(ATL_FWDNL_PREFIX "Required attribute is missing: %d\n",
			missing_attr);
		ATLFWD_NL_SET_ERR_MSG(info, "Required attribute is missing");
		return -EINVAL;
	}

	/* Now, check additional pre-conditions (if any) */
	if (info->attrs[ATL_FWD_ATTR_RING_INDEX])
		ring_index = nla_get_s32(info->attrs[ATL_FWD_ATTR_RING_INDEX]);

	switch (ops->cmd) {
	case ATL_FWD_CMD_GET_RX_QUEUE:
		if (!is_rx_ring_index(ring_index)) {
			ATLFWD_NL_SET_ERR_MSG(info, "Expected RX ring.");
			return -EINVAL;
		}
		break;
	case ATL_FWD_CMD_GET_TX_QUEUE:
		if (!is_tx_ring_index(ring_index)) {
			ATLFWD_NL_SET_ERR_MSG(info, "Expected TX ring.");
			return -EINVAL;
		}
		break;
	}

	return ret;
}

/* netlink-specific structures */
static const struct nla_policy atlfwd_nl_policy[NUM_ATL_FWD_ATTR] = {
	[ATL_FWD_ATTR_IFNAME] = { .type = NLA_NUL_STRING, .len = IFNAMSIZ - 1 },
	[ATL_FWD_ATTR_FLAGS] = { .type = NLA_S32 },
	[ATL_FWD_ATTR_RING_SIZE] = { .type = NLA_S32 },
	[ATL_FWD_ATTR_BUF_SIZE] = { .type = NLA_S32 },
	[ATL_FWD_ATTR_PAGE_ORDER] = { .type = NLA_S32 },
	[ATL_FWD_ATTR_RING_INDEX] = { .type = NLA_S32 },
	[ATL_FWD_ATTR_TX_BUNCH_SIZE] = { .type = NLA_S32 },
	[ATL_FWD_ATTR_QUEUE_INDEX] = { .type = NLA_S32 },
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
#define ATLFWD_NL_OP_POLICY(op_policy) .policy = op_policy
#else
#define ATLFWD_NL_OP_POLICY(op_policy)
#endif

static const struct genl_ops atlfwd_nl_ops[] = {
	{ .cmd = ATL_FWD_CMD_REQUEST_RING,
	  .doit = doit_request_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_RELEASE_RING,
	  .doit = doit_release_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_ENABLE_RING,
	  .doit = doit_enable_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_DISABLE_RING,
	  .doit = doit_disable_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_DUMP_RING,
	  .doit = doit_dump_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_SET_TX_BUNCH,
	  .doit = doit_set_tx_bunch,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_REQUEST_EVENT,
	  .doit = doit_request_event,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_RELEASE_EVENT,
	  .doit = doit_release_event,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_ENABLE_EVENT,
	  .doit = doit_enable_event,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_DISABLE_EVENT,
	  .doit = doit_disable_event,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_DISABLE_REDIRECTIONS,
	  .doit = doit_disable_redirections,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_FORCE_ICMP_TX_VIA,
	  .doit = doit_force_icmp_tx_via,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_FORCE_TX_VIA,
	  .doit = doit_force_tx_via,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_RING_STATUS,
	  .doit = doit_ring_status,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_GET_RX_QUEUE,
	  .doit = doit_get_queue,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_GET_TX_QUEUE,
	  .doit = doit_get_queue,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
};

static struct genl_family atlfwd_nl_family = {
	.hdrsize = 0, /* no private header */
	.name = ATL_FWD_GENL_NAME, /* have users key off the name instead */
	.version = 1, /* no particular meaning now */
	.maxattr = ATL_FWD_ATTR_MAX,
	.netnsok = false,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
	.policy = atlfwd_nl_policy,
#endif
	.pre_doit = atlfwd_nl_pre_doit,
	.ops = atlfwd_nl_ops,
	.n_ops = ARRAY_SIZE(atlfwd_nl_ops),
	.module = THIS_MODULE,
};
