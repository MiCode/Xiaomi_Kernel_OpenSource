/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/dma-mapping.h>
#include <linux/if_vlan.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/workqueue.h>
#include <net/genetlink.h>

#include "atl_fwdnl.h"

#include "atl_common.h"
#include "atl_compat.h"
#include "atl_ring.h"
#include "atl_trace.h"

#define ATL_FWDNL_PREFIX "atl_fwdnl: "
/* Forward declaration, actual definition is at the bottom of the file */
static struct genl_family atlfwd_nl_family;
static struct atl_fwd_desc_ring *get_fwd_ring_desc(struct atl_fwd_ring *ring);
static struct atl_fwd_ring *get_fwd_ring(struct net_device *netdev,
					 const int ring_index,
					 struct genl_info *info,
					 const bool err_if_released);
static unsigned int atlfwd_nl_dev_cache_index(const struct net_device *netdev);
static int atlfwd_nl_transmit_skb_ring(struct atl_fwd_ring *ring,
				       struct sk_buff *skb);
static void atlfwd_nl_tx_head_poll(struct work_struct *);

/* FWD ring descriptor
 * Similar to atl_desc_ring, but has less fields.
 *
 * Note: it's not a part of atl_fwd_ring on purpose.
 */
struct atl_fwd_desc_ring {
	struct atl_hw_ring hw;
	uint32_t head;
	uint32_t tail;
	union {
		struct atl_txbuf *txbufs;
	};
	struct u64_stats_sync syncp;
	struct atl_ring_stats stats;
	u32 tx_hw_head;
	struct atl_fwd_event tx_evt;
	struct atl_fwd_event rx_evt;
};

/* workqueue data to process tx head updates */
struct atlfwd_nl_tx_head_data {
	struct delayed_work wrk;
	int atlfwd_dev_index;
};

/* Register generic netlink family upon module initialization */
int __init atlfwd_nl_init(void)
{
	return genl_register_family(&atlfwd_nl_family);
}

#define MAX_NUM_ATLFWD_DEVICES 16 /* Maximum number of entries in cache */
/* ATL devices cache
 *
 * To make sure we are trying to communicate with supported devices only.
 */
static struct {
	int ifindex;
	struct net_device *ndev;
	struct atl_fwd_desc_ring ring[ATL_NUM_FWD_RINGS * 2];
	/* State of forced redirections */
	int force_icmp_via;
	int force_tx_via;
	/* Deferred TX head updates polling */
	struct atlfwd_nl_tx_head_data tx_head_wq;
	u32 tx_bunch;
} s_atlfwd_devices[MAX_NUM_ATLFWD_DEVICES];
static int s_atlfwd_devices_cnt; /* Total number of entries in cache */

/* Remember the ATL device on probe */
void atlfwd_nl_on_probe(struct net_device *ndev)
{
	if (s_atlfwd_devices_cnt >= MAX_NUM_ATLFWD_DEVICES) {
		pr_warn(ATL_FWDNL_PREFIX
			"Device cache exceeded, consider increasing MAX_NUM_ATLFWD_DEVICES\n");
		return;
	}

	int i;

	for (i = 0; i != MAX_NUM_ATLFWD_DEVICES; i++) {
		if (s_atlfwd_devices[i].ifindex == 0) {
			s_atlfwd_devices[i].ifindex = ndev->ifindex;
			s_atlfwd_devices[i].ndev = ndev;
			s_atlfwd_devices[i].force_icmp_via = S32_MIN;
			s_atlfwd_devices[i].force_tx_via = S32_MIN;
			INIT_DELAYED_WORK(&s_atlfwd_devices[i].tx_head_wq.wrk,
					  atlfwd_nl_tx_head_poll);
			s_atlfwd_devices[i].tx_head_wq.atlfwd_dev_index = i;
			s_atlfwd_devices[i].tx_bunch = 0;
			s_atlfwd_devices_cnt++;
			break;
		}
	}

	if (i == MAX_NUM_ATLFWD_DEVICES)
		pr_warn(ATL_FWDNL_PREFIX
			"Device cache has issues: counter and the actual cache contents are inconsistent\n");
}

/* Remove the ATL device from the cache on remove request */
void atlfwd_nl_on_remove(struct net_device *ndev)
{
	int i;

	for (i = 0; i != MAX_NUM_ATLFWD_DEVICES; i++) {
		if (s_atlfwd_devices[i].ifindex == ndev->ifindex) {
			cancel_delayed_work_sync(
				&s_atlfwd_devices[i].tx_head_wq.wrk);
			s_atlfwd_devices_cnt--;
			s_atlfwd_devices[i].ifindex = 0;
			s_atlfwd_devices[i].ndev = NULL;
			break;
		}
	}

	if (i == MAX_NUM_ATLFWD_DEVICES)
		pr_warn(ATL_FWDNL_PREFIX "Device cache miss for '%s' (%d)\n",
			ndev->name, ndev->ifindex);
}

/* Unregister generic netlink family upon module exit */
void __exit atlfwd_nl_exit(void)
{
	genl_unregister_family(&atlfwd_nl_family);
}

void atl_fwd_get_ring_stats(struct atl_fwd_ring *ring,
			    struct atl_ring_stats *stats)
{
	struct atl_fwd_desc_ring *desc = get_fwd_ring_desc(ring);

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
bool atlfwd_nl_is_redirected(const struct sk_buff *skb, struct net_device *ndev)
{
	const unsigned int idx = atlfwd_nl_dev_cache_index(ndev);

	if (idx == MAX_NUM_ATLFWD_DEVICES) {
		pr_warn(ATL_FWDNL_PREFIX
			"%s called for non-ATL device (ifindex %d)\n",
			__func__, ndev->ifindex);
		return false;
	}

	if (unlikely(s_atlfwd_devices[idx].force_tx_via != S32_MIN)) {
		/* Redirect everything, but:
		 * . LSO is not supported at the moment
		 * . VLAN is not supported at the moment
		 */
		return (skb_shinfo(skb)->gso_size == 0 &&
			!skb_vlan_tag_present(skb));
	}

	if (unlikely(s_atlfwd_devices[idx].force_icmp_via != S32_MIN)) {
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
	const unsigned int idx = atlfwd_nl_dev_cache_index(ndev);
	const bool err_if_released = false;
	struct atl_fwd_ring *ring = NULL;
	int ring_index = S32_MIN;

	if (idx == MAX_NUM_ATLFWD_DEVICES) {
		pr_warn(ATL_FWDNL_PREFIX
			"%s called for non-ATL device (ifindex %d)\n",
			__func__, ndev->ifindex);
		return -EFAULT;
	}

	if (s_atlfwd_devices[idx].force_tx_via != S32_MIN)
		ring_index = s_atlfwd_devices[idx].force_tx_via;
	else if (s_atlfwd_devices[idx].force_icmp_via != S32_MIN)
		ring_index = s_atlfwd_devices[idx].force_icmp_via;
	else
		return -EFAULT;

	ring = get_fwd_ring(ndev, ring_index, NULL, err_if_released);
	if (unlikely(ring == NULL))
		return -EFAULT;

	return atlfwd_nl_transmit_skb_ring(ring, skb);
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

/* Set the netlink error message
 *
 * Before Linux 4.12 we could only put it in kernel logs.
 * Starting with 4.12 we can also use extack to pass the message to user-mode.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
#define ATLFWD_NL_SET_ERR_MSG(info, msg) GENL_SET_ERR_MSG(info, msg)
#else
#define ATLFWD_NL_SET_ERR_MSG(info, msg) pr_warn(ATL_FWDNL_PREFIX "%s\n", msg)
#endif

/* Attempt to auto-deduce the device
 *
 * This is possible if and only if there's exactly 1 ATL device in the system.
 *
 * Returns NULL on error, net_device pointer otherwise.
 */
static struct net_device *atlfwd_nl_auto_deduce_dev(struct genl_info *info)
{
	int i;

	if (s_atlfwd_devices_cnt > 1) {
		ATLFWD_NL_SET_ERR_MSG(
			info,
			"Device name is required since there are several ATL devices");
		return NULL;
	}

	for (i = 0; i != MAX_NUM_ATLFWD_DEVICES; i++) {
		const int ifindex = s_atlfwd_devices[i].ifindex;

		if (ifindex != 0) {
			pr_debug(ATL_FWDNL_PREFIX
				 "Found ifindex %d (auto-deduced)\n",
				 ifindex);
			return dev_get_by_index(genl_info_net(info), ifindex);
		}
	}

	ATLFWD_NL_SET_ERR_MSG(info, "No ATL devices found at the moment");
	return NULL;
}

/* Get the index (in cache) of the given ATL device.
 *
 * Returns MAX_NUM_ATLFWD_DEVICES on error, valid index otherwise.
 */
static unsigned int atlfwd_nl_dev_cache_index(const struct net_device *netdev)
{
	int i;

	for (i = 0; i != MAX_NUM_ATLFWD_DEVICES; i++) {
		if (s_atlfwd_devices[i].ifindex != 0 &&
		    s_atlfwd_devices[i].ifindex == netdev->ifindex)
			break;
	}

	return i;
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
	unsigned int idx = MAX_NUM_ATLFWD_DEVICES;
	struct net_device *netdev = NULL;

	if (dev_name == NULL) {
		/* No dev_name provided in request, try to auto-deduce. */
		return atlfwd_nl_auto_deduce_dev(info);
	}

	netdev = dev_get_by_name(genl_info_net(info), dev_name);
	if (netdev == NULL) {
		ATLFWD_NL_SET_ERR_MSG(info, "No matching device found");
		return NULL;
	}

	/* Check ATL device cache to make sure an ATL device was requested */
	idx = atlfwd_nl_dev_cache_index(netdev);
	if (idx != MAX_NUM_ATLFWD_DEVICES) {
		pr_debug(ATL_FWDNL_PREFIX "Found ifindex %d for '%s'\n",
			 s_atlfwd_devices[idx].ifindex, dev_name);
		return netdev;
	}

	ATLFWD_NL_SET_ERR_MSG(info, "Requested device is not an ATL device");
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

static bool is_tx_ring(const int ring_index)
{
	return ring_index % 2;
}

/* Converts regular ring index to "normalized" internal representation.
 * This "normalized" value is the only thing known to user-mode.
 */
static int nl_ring_index(const struct atl_fwd_ring *ring)
{
	const bool is_tx = !!(ring->flags & ATL_FWR_TX);
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
	if (is_tx)
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

static void *nl_reply_init(struct sk_buff *msg, struct genl_info *info,
			   const enum atlfwd_nl_command reply_cmd)
{
	void *hdr = NULL;

	if (unlikely(msg == NULL))
		return NULL;

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq,
			  &atlfwd_nl_family, 0, reply_cmd);
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
				const enum atlfwd_nl_command reply_cmd,
				const enum atlfwd_nl_attribute attr,
				const int value)
{
	struct sk_buff *msg = nl_reply_create();
	void *hdr = nl_reply_init(msg, info, reply_cmd);

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
static struct atl_fwd_desc_ring *get_fwd_ring_desc(struct atl_fwd_ring *ring)
{
	unsigned int atlfwd_dev_idx = MAX_NUM_ATLFWD_DEVICES;
	int ring_index = S32_MIN;

	if (unlikely(ring == NULL))
		return NULL;

	atlfwd_dev_idx = atlfwd_nl_dev_cache_index(ring->nic->ndev);
	if (unlikely(atlfwd_dev_idx == MAX_NUM_ATLFWD_DEVICES))
		return NULL;

	ring_index = nl_ring_index(ring);
	if (unlikely(ring_index == S32_MIN))
		return NULL;

	return &s_atlfwd_devices[atlfwd_dev_idx].ring[ring_index];
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
	const int dir_tx = is_tx_ring(ring_index);
	const int norm_index = (dir_tx ? ring_index - 1 : ring_index) / 2;
	struct atl_nic *nic = netdev_priv(netdev);
	struct atl_fwd_ring *ring = nic->fwd.rings[dir_tx][norm_index];

	pr_debug(ATL_FWDNL_PREFIX "index=%d/n%d\n", ring_index, norm_index);

	if (unlikely(norm_index < 0 || norm_index >= ATL_NUM_FWD_RINGS)) {
		if (info)
			ATLFWD_NL_SET_ERR_MSG(info,
					      "Ring index is out of bounds");
		return NULL;
	}

	if (unlikely(ring == NULL && info && err_if_released))
		ATLFWD_NL_SET_ERR_MSG(info,
				      "Requested ring is NULL / released");

	return ring;
}

static uint32_t atlfwd_nl_ring_hw_head(struct atl_fwd_ring *ring)
{
	if (ring->flags & ATL_FWR_TX)
		return atl_read(&ring->nic->hw, ATL_TX_RING_HEAD(ring)) &
		       0x1fff;

	return atl_read(&ring->nic->hw, ATL_RX_RING_HEAD(ring)) & 0x1fff;
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
static bool atlfwd_nl_tx_full(struct atl_fwd_desc_ring *ring, const int needed)
{
	int space = ring_space(ring);

	if (likely(space >= needed))
		return false;

	return true;
}

/* Returns true, if the number of occupied elements is above a given threshold.
 *
 * E.g. if threshold is 4, then we'll return true here, if number of occupied
 * elements is greater than 1/4 of the total ring size.
 */
static bool atlfwd_nl_tx_above_threshold(struct atl_fwd_desc_ring *ring,
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

static int atlfwd_nl_num_txd_for_skb(struct sk_buff *skb)
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

static unsigned int atlfwd_nl_tx_clean_budget = 256;
module_param_named(fwdnl_tx_clean_budget, atlfwd_nl_tx_clean_budget, uint,
		   0644);

/* Returns true, if HW has completed processing (head is equal to tail).
 * Returns false, if there some work is still pending.
 */
static bool atlfwd_nl_tx_head_poll_ring(struct atl_fwd_ring *ring)
{
	uint32_t budget = atlfwd_nl_tx_clean_budget;
	struct atl_fwd_desc_ring *desc = NULL;
	struct device *dev = NULL;
	unsigned int packets = 0;
	unsigned int bytes = 0;
	uint32_t hw_head = 0;
	uint32_t sw_head = 0;

	desc = get_fwd_ring_desc(ring);
	if (unlikely(ring == NULL || desc == NULL))
		return true;

	dev = &ring->nic->hw.pdev->dev;
	hw_head = atlfwd_nl_ring_hw_head(ring);
	sw_head = READ_ONCE(desc->head);

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

		bump_ptr(sw_head, ring, 1);
	} while (budget--);

	WRITE_ONCE(desc->head, sw_head);

	u64_stats_update_begin(&desc->syncp);
	desc->stats.tx.bytes += bytes;
	desc->stats.tx.packets += packets;
	u64_stats_update_end(&desc->syncp);

	return (hw_head == READ_ONCE(desc->tail));
}

static unsigned int atlfwd_nl_tx_clean_threshold_msec = 200;
module_param_named(fwdnl_tx_clean_threshold_msec,
		   atlfwd_nl_tx_clean_threshold_msec, uint, 0644);
static unsigned int atlfwd_nl_tx_clean_threshold_frac = 8;
module_param_named(fwdnl_tx_clean_threshold_frac,
		   atlfwd_nl_tx_clean_threshold_frac, uint, 0644);

static void atlfwd_nl_tx_head_poll(struct work_struct *work)
{
	struct atlfwd_nl_tx_head_data *data =
		(struct atlfwd_nl_tx_head_data *)work;
	const unsigned int idx = data->atlfwd_dev_index;
	struct net_device *ndev = s_atlfwd_devices[idx].ndev;
	const bool err_if_released = false;
	struct atl_fwd_ring *ring = NULL;
	bool poll_finished = true;
	int ring_index = S32_MIN;

	ring_index = s_atlfwd_devices[idx].force_tx_via;
	if (ring_index != S32_MIN) {
		ring = get_fwd_ring(ndev, ring_index, NULL, err_if_released);
		if (!atlfwd_nl_tx_head_poll_ring(ring))
			poll_finished = false;
	}

	if (ring_index != s_atlfwd_devices[idx].force_icmp_via) {
		ring_index = s_atlfwd_devices[idx].force_icmp_via;
		if (ring_index != S32_MIN) {
			ring = get_fwd_ring(ndev, ring_index, NULL,
					    err_if_released);
			if (!atlfwd_nl_tx_head_poll_ring(ring))
				poll_finished = false;
		}
	}

	if (!poll_finished && likely(atlfwd_nl_tx_clean_threshold_msec != 0))
		schedule_delayed_work(
			&s_atlfwd_devices[idx].tx_head_wq.wrk,
			msecs_to_jiffies(atlfwd_nl_tx_clean_threshold_msec));
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
	unsigned int atlfwd_dev_idx = MAX_NUM_ATLFWD_DEVICES;
	unsigned int frags = skb_shinfo(skb)->nr_frags;
	struct atl_fwd_desc_ring *ring_desc = NULL;
	struct net_device *ndev = ring->nic->ndev;
	unsigned int frag_len = skb_headlen(skb);
	skb_frag_t *frag = NULL;
	struct atl_hw *hw = &ring->nic->hw;
	struct device *dev = &hw->pdev->dev;
	struct atl_txbuf *first_buf = NULL;
	struct atl_txbuf *txbuf = NULL;
	dma_addr_t frag_daddr = 0;
	struct atl_tx_desc desc;
	uint32_t desc_idx;
	int bunch;

	atlfwd_dev_idx = atlfwd_nl_dev_cache_index(ndev);
	bunch = s_atlfwd_devices[atlfwd_dev_idx].tx_bunch;
	ring_desc = get_fwd_ring_desc(ring);
	if (unlikely(atlfwd_dev_idx == MAX_NUM_ATLFWD_DEVICES) ||
	    unlikely(ring_desc == NULL))
		return -EFAULT;

	desc_idx = ring_desc->tail;

	if (atlfwd_nl_tx_above_threshold(ring_desc,
					 atlfwd_nl_tx_clean_threshold_frac)) {
		cancel_delayed_work_sync(
			&s_atlfwd_devices[atlfwd_dev_idx].tx_head_wq.wrk);
		atlfwd_nl_tx_head_poll(
			&s_atlfwd_devices[atlfwd_dev_idx].tx_head_wq.wrk.work);
	}

	if (atlfwd_nl_tx_full(ring_desc, atlfwd_nl_num_txd_for_skb(skb))) {
		u64_stats_update_begin(&ring_desc->syncp);
		ring_desc->stats.tx.tx_busy++;
		u64_stats_update_end(&ring_desc->syncp);
		return NETDEV_TX_BUSY;
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
			bump_ptr(desc_idx, ring, 1);
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
		bump_ptr(desc_idx, ring, 1);
		txbuf = &ring_desc->txbufs[desc_idx];
		memset(txbuf, 0, sizeof(*txbuf));
		frag_len = skb_frag_size(frag);
		frag_daddr = skb_frag_dma_map(dev, frag, 0, frag_len, DMA_TO_DEVICE);

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
	bump_ptr(desc_idx, ring, 1);
	ring_desc->tail = desc_idx;

	wmb();

	if (atlfwd_nl_ring_occupied(ring, desc_idx) > bunch)
		atl_write(hw, ATL_TX_RING_TAIL(ring), desc_idx);

	if (likely(atlfwd_nl_tx_clean_threshold_msec != 0))
		schedule_delayed_work(
			&s_atlfwd_devices[atlfwd_dev_idx].tx_head_wq.wrk,
			msecs_to_jiffies(atlfwd_nl_tx_clean_threshold_msec));

	return NETDEV_TX_OK;

err_dma:
	dev_err(dev, "%s failed\n", __func__);
	for (;;) {
		atl_fwd_txbuf_free(dev, txbuf);
		if (txbuf == first_buf)
			break;
		bump_ptr(desc_idx, ring, -1);
		txbuf = &ring_desc->txbufs[desc_idx];
	}
	u64_stats_update_begin(&ring_desc->syncp);
	ring_desc->stats.tx.dma_map_failed++;
	u64_stats_update_end(&ring_desc->syncp);
	return -EFAULT;
}

/* ATL_FWD_CMD_REQUEST_RING handler */
static int atlfwd_nl_request_ring(struct sk_buff *skb, struct genl_info *info)
{
	struct atl_fwd_desc_ring *ring_desc = NULL;
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	const char *ifname = NULL;
	int page_order = S32_MIN;
	int ring_index = S32_MIN;
	int ring_size = S32_MIN;
	int buf_size = S32_MIN;
	int flags = S32_MIN;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	flags = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_FLAGS);
	ring_size = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_SIZE);
	buf_size = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_BUF_SIZE);
	page_order = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_PAGE_ORDER);
	if (unlikely(flags == S32_MIN || ring_size == S32_MIN ||
		     buf_size == S32_MIN || page_order == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	pr_debug(ATL_FWDNL_PREFIX "flags=%d\n", flags);
	pr_debug(ATL_FWDNL_PREFIX "ring_size=%d\n", ring_size);
	pr_debug(ATL_FWDNL_PREFIX "buf_size=%d\n", buf_size);
	pr_debug(ATL_FWDNL_PREFIX "page_order=%d\n", page_order);

	ring = atl_fwd_request_ring(netdev, flags, ring_size, buf_size,
				    page_order, NULL);
	if (IS_ERR_OR_NULL(ring)) {
		result = PTR_ERR(ring);
		goto err_netdev;
	}

	pr_debug(ATL_FWDNL_PREFIX "Got ring %d (%p)\n", ring->idx, ring);

	ring_desc = get_fwd_ring_desc(ring);
	ring_index = nl_ring_index(ring);
	if (unlikely(ring_desc == NULL || ring_index == S32_MIN)) {
		ATLFWD_NL_SET_ERR_MSG(info, "Internal error");
		result = -EFAULT;
		atl_fwd_release_ring(ring);
		goto err_netdev;
	}

	memcpy(&ring_desc->hw, &ring->hw, sizeof(ring_desc->hw));

	if (ring->flags & ATL_FWR_TX) {
		/* TX ring */
		ring_desc->head = 0;
		ring_desc->tail = 0;
		ring_desc->txbufs = kcalloc(
			ring_size, sizeof(*ring_desc->txbufs), GFP_KERNEL);
		if (ring_desc->txbufs == NULL) {
			result = -ENOMEM;
			atl_fwd_release_ring(ring);
			goto err_netdev;
		}
	} else {
		ring_desc->txbufs = NULL;
	}
	u64_stats_init(&ring_desc->syncp);
	memset(&ring_desc->stats, 0, sizeof(ring_desc->stats));

	result = atlfwd_nl_send_reply(info, ATL_FWD_CMD_RELEASE_RING,
				      ATL_FWD_ATTR_RING_INDEX, ring_index);

err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_RELEASE_RING handler */
static int atlfwd_nl_release_ring(struct sk_buff *skb, struct genl_info *info)
{
	struct atl_fwd_desc_ring *ring_desc = NULL;
	const bool err_if_released = true;
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	const char *ifname = NULL;
	int ring_index = S32_MIN;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring_desc = get_fwd_ring_desc(ring);
	if (unlikely(ring_desc == NULL)) {
		ATLFWD_NL_SET_ERR_MSG(info, "Internal error");
		result = -EFAULT;
		goto err_netdev;
	}

	memset(&ring_desc->hw, 0, sizeof(ring_desc->hw));
	kfree(ring_desc->txbufs);
	ring_desc->txbufs = NULL;

	pr_debug(ATL_FWDNL_PREFIX "Releasing ring %d (%p)\n", ring_index, ring);
	atl_fwd_release_ring(ring);

err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_ENABLE_RING handler */
static int atlfwd_nl_enable_ring(struct sk_buff *skb, struct genl_info *info)
{
	const bool err_if_released = true;
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	const char *ifname = NULL;
	int ring_index = S32_MIN;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL)) {
		result = -EINVAL;
		goto err_netdev;
	}

	pr_debug(ATL_FWDNL_PREFIX "Enabling ring %d (%p)\n", ring_index, ring);
	atl_fwd_enable_ring(ring);

err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_DISABLE_RING handler */
static int atlfwd_nl_disable_ring(struct sk_buff *skb, struct genl_info *info)
{
	const bool err_if_released = true;
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	const char *ifname = NULL;
	int ring_index = S32_MIN;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL)) {
		result = -EINVAL;
		goto err_netdev;
	}

	pr_debug(ATL_FWDNL_PREFIX "Disabling ring %d (%p)\n", ring_index, ring);
	atl_fwd_disable_ring(ring);

err_netdev:
	dev_put(netdev);
	return result;
}

void atl_fwd_dump_ring(struct atl_fwd_ring *ring)
{
	struct atl_hw *hw = &ring->nic->hw;
	bool dir_tx = ring->flags & ATL_FWR_TX;
	int j;

	uint32_t head;
	uint32_t tail;

	if (dir_tx) {
		head = atl_read(hw, ATL_TX_RING_HEAD(&ring->hw)) & 0x1fff;
		tail = atl_read(hw, ATL_TX_RING_TAIL(&ring->hw)) & 0x1fff;
	} else {
		head = atl_read(hw, ATL_RX_RING_HEAD(&ring->hw)) & 0x1fff;
		tail = atl_read(hw, ATL_RX_RING_TAIL(&ring->hw)) & 0x1fff;
	}

	pr_info("[%d] head=%d tail=%d", ring->idx, head, tail);

	for (j = 0; j != ring->hw.size; j++) {
		if (dir_tx) {
			pr_info("[%d] [%d] 0x%llx, DD=%d", ring->idx, j, ring->hw.descs[j].tx.daddr, ring->hw.descs[j].tx.dd);
			trace_atl_tx_descr(ring->idx, j, (u64*)ring->hw.descs[j].raw);
		} else {
			pr_info("[%d] [%d] 0x%llx, DD=%d", ring->idx, j, ring->hw.descs[j].rx.daddr, ring->hw.descs[j].rx.dd);
			trace_atl_rx_descr(ring->idx, j, (u64*)ring->hw.descs[j].raw);
		}
	}
}

/* ATL_FWD_CMD_DUMP_RING handler */
static int atlfwd_nl_dump_ring(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	bool err_if_released = false;
	const char *ifname = NULL;
	int ring_index = S32_MIN;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL)) {
		result = -EINVAL;
		goto err_netdev;
	}

	pr_debug(ATL_FWDNL_PREFIX "Dumping ring %d (%p)\n",
		 ring_index & 0x0000FFFF, ring);
	atl_fwd_dump_ring(ring);

err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_SET_TX_BUNCH handler */
static int atlfwd_nl_set_tx_bunch(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *netdev = NULL;
	const char *ifname = NULL;
	s32 bunch = 0;
	int result = 0;
	int dev_idx;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	bunch = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_TX_BUNCH_SIZE);
	if (unlikely(bunch == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	pr_debug(ATL_FWDNL_PREFIX "Set  TX bunch %d\n", bunch);
	dev_idx = atlfwd_nl_dev_cache_index(netdev);
	s_atlfwd_devices[dev_idx].tx_bunch = bunch;

err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_REQUEST_EVENT handler */
static int atlfwd_nl_request_event(struct sk_buff *skb, struct genl_info *info)
{
	struct atl_fwd_desc_ring *desc = NULL;
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	bool err_if_released = true;
	const char *ifname = NULL;
	int ring_index = S32_MIN;
	int flags;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL)) {
		result = -EINVAL;
		goto err_netdev;
	}

	desc = get_fwd_ring_desc(ring);

	if (is_tx_ring(ring_index)) {
		/* right now we support only head pointer writeback */
		flags = ATL_FWD_EVT_TXWB;
		desc->tx_evt.ring = ring;
		desc->tx_evt.flags = flags;
		if (desc->tx_evt.flags & ATL_FWD_EVT_TXWB)
			desc->tx_evt.tx_head_wrb =
				  dma_map_single(&ring->nic->hw.pdev->dev,
						 &desc->tx_hw_head,
						 sizeof(desc->tx_hw_head),
						 DMA_FROM_DEVICE);
	}
	result = atl_fwd_request_event(&desc->tx_evt);
	if (result) {
		goto err_netdev;
	}


err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_RELEASE_EVENT handler */
static int atlfwd_nl_release_event(struct sk_buff *skb, struct genl_info *info)
{
	struct atl_fwd_desc_ring *desc = NULL;
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	bool err_if_released = true;
	const char *ifname = NULL;
	int ring_index = S32_MIN;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL)) {
		result = -EINVAL;
		goto err_netdev;
	}

	pr_debug(ATL_FWDNL_PREFIX "Releasing event for ring %d (%p)\n", ring_index, ring);
	atl_fwd_release_event(ring->evt);

	desc = get_fwd_ring_desc(ring);

	if (desc->tx_evt.flags & ATL_FWD_EVT_TXWB)
		dma_unmap_single(&ring->nic->hw.pdev->dev,
				desc->tx_evt.tx_head_wrb,
				sizeof(desc->tx_hw_head),
				DMA_FROM_DEVICE);
err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_ENABLE_EVENT handler */
static int atlfwd_nl_enable_event(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	const char *ifname = NULL;
	bool err_if_released = true;
	int ring_index = S32_MIN;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL)) {
		result = -EINVAL;
		goto err_netdev;
	}

	pr_debug(ATL_FWDNL_PREFIX "Enabling event for ring %d (%p)\n", ring_index, ring);
	atl_fwd_enable_event(ring->evt);

err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_DISABLE_EVENT handler */
static int atlfwd_nl_disable_event(struct sk_buff *skb, struct genl_info *info)
{
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	const char *ifname = NULL;
	bool err_if_released = true;
	int ring_index = S32_MIN;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN)) {
		result = -EINVAL;
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL)) {
		result = -EINVAL;
		goto err_netdev;
	}

	pr_debug(ATL_FWDNL_PREFIX "Disabling event for ring %d (%p)\n", ring_index, ring);
	atl_fwd_disable_event(ring->evt);

err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_DISABLE_REDIRECTIONS handler */
static int atlfwd_nl_disable_redirections(struct sk_buff *skb,
					  struct genl_info *info)
{
	unsigned int idx = MAX_NUM_ATLFWD_DEVICES;
	struct net_device *netdev = NULL;
	const char *ifname = NULL;
	int result = 0;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	idx = atlfwd_nl_dev_cache_index(netdev);
	if (idx == MAX_NUM_ATLFWD_DEVICES) {
		pr_warn(ATL_FWDNL_PREFIX
			"%s called for non-ATL device (ifindex %d)\n",
			__func__, netdev->ifindex);
		return -EFAULT;
	}

	pr_debug(ATL_FWDNL_PREFIX "All forced redirections are disabled now\n");
	s_atlfwd_devices[idx].force_icmp_via = S32_MIN;
	s_atlfwd_devices[idx].force_tx_via = S32_MIN;

	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_FORCE_ICMP_TX_VIA handler */
static int atlfwd_nl_force_icmp_tx_via(struct sk_buff *skb,
				       struct genl_info *info)
{
	unsigned int idx = MAX_NUM_ATLFWD_DEVICES;
	const bool err_if_released = true;
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	const char *ifname = NULL;
	int ring_index = S32_MIN;
	int result = -EINVAL;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	idx = atlfwd_nl_dev_cache_index(netdev);
	if (idx == MAX_NUM_ATLFWD_DEVICES) {
		pr_warn(ATL_FWDNL_PREFIX
			"%s called for non-ATL device (ifindex %d)\n",
			__func__, netdev->ifindex);
		return -EFAULT;
	}

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN))
		goto err_netdev;
	if (unlikely(!is_tx_ring(ring_index))) {
		ATLFWD_NL_SET_ERR_MSG(info, "Expected TX ring");
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL))
		goto err_netdev;

	result = 0;
	pr_debug(ATL_FWDNL_PREFIX
		 "All egress ICMP traffic is now forced via ring %d (%p)\n",
		 ring_index, ring);
	s_atlfwd_devices[idx].force_icmp_via = ring_index;

err_netdev:
	dev_put(netdev);
	return result;
}

/* ATL_FWD_CMD_FORCE_TX_VIA handler */
static int atlfwd_nl_force_tx_via(struct sk_buff *skb, struct genl_info *info)
{
	unsigned int idx = MAX_NUM_ATLFWD_DEVICES;
	const bool err_if_released = true;
	struct net_device *netdev = NULL;
	struct atl_fwd_ring *ring = NULL;
	const char *ifname = NULL;
	int ring_index = S32_MIN;
	int result = -EINVAL;

	/* 1. get net_device */
	ifname = atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	netdev = atlfwd_nl_get_dev_by_name(ifname, info);
	if (netdev == NULL)
		return -ENODEV;

	idx = atlfwd_nl_dev_cache_index(netdev);
	if (idx == MAX_NUM_ATLFWD_DEVICES) {
		pr_warn(ATL_FWDNL_PREFIX
			"%s called for non-ATL device (ifindex %d)\n",
			__func__, netdev->ifindex);
		return -EFAULT;
	}

	/* 2. parse mandatory attributes */
	ring_index = atlfwd_attr_to_s32(info, ATL_FWD_ATTR_RING_INDEX);
	if (unlikely(ring_index == S32_MIN))
		goto err_netdev;
	if (unlikely(!is_tx_ring(ring_index))) {
		ATLFWD_NL_SET_ERR_MSG(info, "Expected TX ring");
		goto err_netdev;
	}

	ring = get_fwd_ring(netdev, ring_index, info, err_if_released);
	if (unlikely(ring == NULL))
		goto err_netdev;

	result = 0;
	pr_debug(ATL_FWDNL_PREFIX
		 "All egress traffic is now forced via ring %d (%p)\n",
		 ring_index, ring);
	s_atlfwd_devices[idx].force_tx_via = ring_index;

err_netdev:
	dev_put(netdev);
	return result;
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
					is_tx_ring(ring_index))))
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

static int atlfwd_nl_ring_status(struct sk_buff *skb, struct genl_info *info)
{
	int first_idx = 0;
	int last_idx = ATL_NUM_FWD_RINGS * 2;
	struct sk_buff *msg = NULL;
	void *hdr = NULL;
	int result = 0;
	// 1. get net_device
	const char *ifname =
		atlfwd_attr_to_str_or_null(info, ATL_FWD_ATTR_IFNAME);
	struct net_device *netdev = atlfwd_nl_get_dev_by_name(ifname, info);

	if (netdev == NULL)
		return -ENODEV;
	// attribute is optional for this command
	int ring_index =
		atlfwd_attr_to_s32_optional(info, ATL_FWD_ATTR_RING_INDEX);

	if (ring_index != S32_MIN) {
		if (unlikely(!is_valid_ring_index(ring_index))) {
			result = -EINVAL;
			goto err_netdev;
		}

		first_idx = ring_index;
		last_idx = ring_index + 1;
	}

	msg = nl_reply_create();
	hdr = nl_reply_init(msg, info, ATL_FWD_CMD_RING_STATUS);
	if (unlikely(msg == NULL)) {
		result = -ENOBUFS;
		goto err_netdev;
	}
	if (unlikely(hdr == NULL)) {
		result = -EMSGSIZE;
		goto err_netdev;
	}

	for (ring_index = first_idx; ring_index != last_idx; ring_index++) {
		result = atlfwd_nl_add_ring_status(netdev, msg, hdr, info,
						   ring_index);
		if (unlikely(result < 0))
			goto err_netdev;
	}

	result = nl_reply_send(msg, hdr, info);

err_netdev:
	dev_put(netdev);
	return result;
}

/* This handler is called before the actual command handler.
 *
 * At the moment we simply check that all mandatory attributes are present.
 *
 * Returns 0 on success, error otherwise.
 */
static int atlfwd_nl_pre_doit(const struct genl_ops *ops, struct sk_buff *skb,
			      struct genl_info *info)
{
	enum atlfwd_nl_attribute missing_attr = ATL_FWD_ATTR_INVALID;
	int ret = 0;

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
		ret = -EINVAL;
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
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
#define ATLFWD_NL_OP_POLICY(op_policy) .policy = op_policy
#else
#define ATLFWD_NL_OP_POLICY(op_policy)
#endif

static const struct genl_ops atlfwd_nl_ops[] = {
	{ .cmd = ATL_FWD_CMD_REQUEST_RING,
	  .doit = atlfwd_nl_request_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_RELEASE_RING,
	  .doit = atlfwd_nl_release_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_ENABLE_RING,
	  .doit = atlfwd_nl_enable_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_DISABLE_RING,
	  .doit = atlfwd_nl_disable_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_DUMP_RING,
	  .doit = atlfwd_nl_dump_ring,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_SET_TX_BUNCH,
	  .doit = atlfwd_nl_set_tx_bunch,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_REQUEST_EVENT,
	  .doit = atlfwd_nl_request_event,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_RELEASE_EVENT,
	  .doit = atlfwd_nl_release_event,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_ENABLE_EVENT,
	  .doit = atlfwd_nl_enable_event,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_DISABLE_EVENT,
	  .doit = atlfwd_nl_disable_event,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_DISABLE_REDIRECTIONS,
	  .doit = atlfwd_nl_disable_redirections,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_FORCE_ICMP_TX_VIA,
	  .doit = atlfwd_nl_force_icmp_tx_via,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_FORCE_TX_VIA,
	  .doit = atlfwd_nl_force_tx_via,
	  ATLFWD_NL_OP_POLICY(atlfwd_nl_policy) },
	{ .cmd = ATL_FWD_CMD_RING_STATUS,
	  .doit = atlfwd_nl_ring_status,
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
