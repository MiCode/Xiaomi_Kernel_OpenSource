// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "sipa_dummy: " fmt

#include <linux/debugfs.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/netdev_features.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/ratelimit.h>
#include <linux/rtnetlink.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>
#include <linux/timekeeping.h>
#include <net/netfilter/nf_conntrack_ecache.h>

#include "sipa_eth.h"
#include "sipa_dummy.h"
#include "sipa_hal.h"
#include "sipa_priv.h"

static struct sipa_dummy *sipa_dummy;
static struct net_device *dummy_dev;
static struct dentry *dummy_debugfs_root;
int udp_frag_flow_num;
int udp_frag_flow_num;

#define NO_CONNTRACK 0

ATOMIC_NOTIFIER_HEAD(wifi_recv_skb_notifier_list);

/**
 * register_restart_handler - Register function to be called to reset
 * the system
 * @nb: Info about handler function to be called
 *
 * Registers a function to deal with the skb that belongs to the
 * wifi driver.
 */
int sipa_dummy_register_wifi_recv_handler(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&wifi_recv_skb_notifier_list,
					      nb);
}
EXPORT_SYMBOL(sipa_dummy_register_wifi_recv_handler);

/**
 * unregister_wifi_recv_handler - Unregisister wifi receive handler
 *
 * @nb: Hook to be unregistered
 *
 * Unreg a previously registered wifi receive func on dummy scheme.
 *
 * Returns zero on success, or %-ENOENT on failure.
 */
int sipa_dummy_unregister_wifi_recv_handler(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&wifi_recv_skb_notifier_list,
						nb);
}
EXPORT_SYMBOL(sipa_dummy_unregister_wifi_recv_handler);

static DEFINE_RATELIMIT_STATE(rlimit_dummy, 5 * HZ, 5);

struct sipa_dummy_ndev_info sipa_dummy_ndev_arr[SIPA_DUMMY_NDEV_MAX];

u8 dummy_mac_src[] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};
u8 dummy_mac_dst[] = {0x16, 0x15, 0x14, 0x13, 0x12, 0x11};

static char *sipa_dummy_src2str(u32 src_id)
{
	switch (src_id) {
	case SIPA_TERM_USB:
		return "TERM_USB";
	case SIPA_TERM_AP:
		return "TERM_AP";
	case SIPA_TERM_PCIE0:
		return "TERM_PCIE0";
	case SIPA_TERM_PCIE1:
		return "TERM_PCIE1";
	case SIPA_TERM_PCIE2:
		return "TERM_PCIE2";
	case SIPA_TERM_CP0:
		return "TERM_CP0";
	case SIPA_TERM_CP1:
		return "TERM_CP1";
	case SIPA_TERM_VCP:
		return "TERM_VCP";
	case SIPA_TERM_WIFI1:
		return "TERM_WIFI1";
	case SIPA_TERM_WIFI2:
		return "TERM_WIFI2";
	case SIPA_TERM_VAP0:
		return "TERM_VAP0";
	case SIPA_TERM_VAP1:
		return "TERM_VAP1";
	case SIPA_TERM_VAP2:
		return "TERM_VAP2";
	default:
		return "TERM_UKN";
	}
}

static char *sipa_dummy_ndev2str(int ndev_id)
{
	switch (ndev_id) {
	case SIPA_DUMMY_ETH0:
		return "sipa_eth0";
	case SIPA_DUMMY_ETH1:
		return "sipa_eth1";
	case SIPA_DUMMY_ETH2:
		return "sipa_eth2";
	case SIPA_DUMMY_ETH3:
		return "sipa_eth3";
	case SIPA_DUMMY_ETH4:
		return "sipa_eth4";
	case SIPA_DUMMY_ETH5:
		return "sipa_eth5";
	case SIPA_DUMMY_ETH6:
		return "sipa_eth6";
	case SIPA_DUMMY_ETH7:
		return "sipa_eth7";
	case SIPA_DUMMY_ETH8:
		return "sipa_eth8";
	case SIPA_DUMMY_ETH9:
		return "sipa_eth9";
	case SIPA_DUMMY_ETH10:
		return "sipa_eth10";
	case SIPA_DUMMY_ETH11:
		return "sipa_eth11";
	case SIPA_DUMMY_ETH12:
		return "sipa_eth12";
	case SIPA_DUMMY_ETH13:
		return "sipa_eth13";
	case SIPA_DUMMY_ETH14:
		return "sipa_eth14";
	case SIPA_DUMMY_ETH15:
		return "sipa_eth15";
	case SIPA_DUMMY_USB0:
		return "sipa_usb0";
	case SIPA_DUMMY_WIFI0:
		return "wlan0";
	default:
		return "unknown";
	}
}

static void sipa_dummy_prepare_skb(struct sk_buff *skb,
				   struct sipa_node_info node_info)
{
	struct iphdr *iph;
	struct ethhdr *peth;
	u32 src_id = node_info.src;
	u32 dst_id = node_info.dst;
	u8 n_ip_pkt = node_info.n_ip_pkt;

	switch (src_id) {
	case SIPA_TERM_WIFI1:
	case SIPA_TERM_WIFI2:
		break;
	case SIPA_TERM_USB:
		skb->protocol = eth_type_trans(skb, skb->dev);
		skb_reset_network_header(skb);
		break;
	case SIPA_TERM_CP0:
	case SIPA_TERM_CP1:
	case SIPA_TERM_VCP:
		skb_reset_mac_header(skb);
		peth = eth_hdr(skb);
		/* for half soft-half hard scheme, miss to ap dstid is 0 */
		if (!dst_id) {
			ether_addr_copy(peth->h_source, dummy_mac_src);
			ether_addr_copy(peth->h_dest, dummy_mac_dst);
			skb->pkt_type = PACKET_HOST;
		} else {
			if (n_ip_pkt)
				skb->pkt_type = PACKET_HOST;
			else
				skb->pkt_type = PACKET_OTHERHOST;
		}

		skb_set_network_header(skb, ETH_HLEN);
		iph = ip_hdr(skb);
		if (iph->version == 4)
			skb->protocol = htons(ETH_P_IP);
		else
			skb->protocol = htons(ETH_P_IPV6);

		peth->h_proto = skb->protocol;
		skb_pull_inline(skb, ETH_HLEN);
		skb_reset_network_header(skb);
		skb_pop_mac_header(skb);  //without mac header
		break;
	default:
		pr_info("ignore skb from %s\n", sipa_dummy_src2str(src_id));
		break;
	}
}

static int sipa_dummy_get_real_ndev(struct sipa_dummy *dummy,
				    struct sk_buff *skb,
				    int netid, u32 src_id)
{
	int ret = 0;
	struct sipa_dummy_ndev_info *ndev_info;

	switch (src_id) {
	case SIPA_TERM_WIFI1:
	case SIPA_TERM_WIFI2:
		dummy->stats.rx_packets++;
		dummy->stats.rx_bytes += skb->len;
		//sipa_dev_skb_maybe_print(skb, SIPA_DEV_RX, *conf);
		/* Then we relay skbs to wifi driver. */
		atomic_notifier_call_chain(&wifi_recv_skb_notifier_list,
					   0, skb);
		goto out2;
	case SIPA_TERM_USB:
		ndev_info = &sipa_dummy_ndev_arr[SIPA_DUMMY_USB0];
		goto out1;
	case SIPA_TERM_CP0:
	case SIPA_TERM_CP1:
	case SIPA_TERM_VCP:
		ndev_info = &sipa_dummy_ndev_arr[netid];
		goto out1;
	default:
		if (!src_id)
			pr_err("zero srcid on cpu%d!\n", smp_processor_id());

		pr_info("skb from %s ignored\n", sipa_dummy_src2str(src_id));
		ret = -EINVAL;
		goto out2;
	}

out1:
	if (ndev_info->state == SIPA_DUMMY_DEV_ON) {
		skb->dev = ndev_info->ndev;
	} else {
		pr_info("ndev %s down, src_id %d netid %d\n",
			ndev_info->ndev->name, src_id, netid);
		ret = -ENODEV;
	}
out2:
	return ret;
}

static void sipa_dummy_update_stats(struct sk_buff *skb,
				    struct sipa_dummy *dummy)
{
	struct net_device_stats *dummy_stats = &dummy->stats;
	struct net_device_stats *real_stats = &skb->dev->stats;
	unsigned int cur_cpu = smp_processor_id();

	dummy_stats->rx_packets++;
	dummy_stats->rx_bytes += skb->len;

	/* update rx statistics trigger on each cpu */
	dummy->per_stats[cur_cpu].rx_packets++;
	dummy->per_stats[cur_cpu].rx_bytes += skb->len;

	/* update rx statistics for the real netdevice also */
	real_stats->rx_packets++;
	real_stats->rx_bytes += skb->len;
}

static void sipa_dummy_set_bootts(struct sipa_dummy *dummy,
				  enum sipa_dummy_ts_field field)
{
	u64 cur_ts;
	struct sipa_dummy_ring *ring;

	ring = &dummy->rings[smp_processor_id()];
	cur_ts = ktime_get_boottime();
	switch (field) {
	case SIPA_DUMMY_TS_RD_EMPTY:
		ring->last_read_empty = cur_ts;
		break;
	case SIPA_DUMMY_TS_NAPI_COMPLETE:
		ring->last_napi_complete = cur_ts;
		break;
	case SIPA_DUMMY_TS_NAPI_RESCHEDULE:
		ring->last_napi_reschedule = cur_ts;
		break;
	case SIPA_DUMMY_TS_IRQ_TRIGGER:
		ring->last_irq_trigger = cur_ts;
		break;
	default:
		break;
	}
}

static int sipa_dummy_rx_clean(struct sipa_dummy *dummy, int budget,
			       struct napi_struct *napi, int fifoid)
{
	int skb_cnt = 0;
	int ret1, ret2;
	struct sk_buff *skb;
	struct sipa_node_info node_info = {0};
	struct net_device_stats *stats = &dummy->stats;
	struct sipa_eth *sipa_eth;
	struct sipa_usb *sipa_usb;

	while (skb_cnt < budget) {
		ret1 = sipa_nic_rx(&skb, &node_info, skb_cnt, fifoid);
		if (unlikely(ret1)) {
			if (ret1 == -EINVAL)
				stats->rx_errors++;
			if (ret1 == -ENODATA)
				sipa_dummy_set_bootts(dummy,
						      SIPA_DUMMY_TS_RD_EMPTY);
			break;
		}

		skb_cnt++;
		ret2 = sipa_dummy_get_real_ndev(dummy, skb,
						node_info.netid,
						node_info.src);
		if (unlikely(ret2)) {
			stats->rx_dropped++;
			dev_kfree_skb_any(skb);
			continue;
		}

		if (node_info.src == SIPA_TERM_WIFI1 || node_info.src == SIPA_TERM_WIFI2)
			continue;

		sipa_dummy_prepare_skb(skb, node_info);
		sipa_dummy_update_stats(skb, dummy);

		switch (node_info.src) {
		case SIPA_TERM_USB:
			sipa_usb = netdev_priv(skb->dev);

			if (sipa_usb->gro_enable)
				napi_gro_receive(napi, skb);
			else
				netif_receive_skb(skb);
			break;
		case SIPA_TERM_CP0:
		case SIPA_TERM_CP1:
		case SIPA_TERM_VCP:
			sipa_eth = netdev_priv(skb->dev);

			if (sipa_eth->gro_enable)
				napi_gro_receive(napi, skb);
			else
				netif_receive_skb(skb);
			break;
		default:
			dev_kfree_skb_any(skb);
			pr_err("invalid src = 0x%x\n", node_info.src);
			break;
		}
	}

	return skb_cnt;
}

static int sipa_dummy_rx_poll(struct napi_struct *napi, int budget)
{
	int pkts, num;
	struct sipa_dummy_ring *ring = container_of(napi,
						    struct sipa_dummy_ring,
						    napi);
	struct sipa_dummy *dummy = netdev_priv(ring->ndev);

	sipa_nic_switch_core(ring->fifoid);
	num = sipa_nic_sync_recv_pkts(budget, ring->fifoid);
	pkts = sipa_dummy_rx_clean(dummy, num, napi, ring->fifoid);
	sipa_nic_add_tx_fifo_rptr(pkts, ring->fifoid);
	sipa_nic_update_need_fill_cnt(pkts, ring->fifoid);

	sipa_recv_wake_up();
	if (pkts >= budget)
		return budget;

	sipa_dummy_set_bootts(dummy, SIPA_DUMMY_TS_NAPI_COMPLETE);
	napi_complete(napi);

	if (!sipa_nic_check_recv_queue_empty(ring->fifoid)) {
		/* not empty again, need re-schedule,
		 * otherwise might stop rcv in a low possibility
		 */
		sipa_dummy_set_bootts(dummy, SIPA_DUMMY_TS_NAPI_RESCHEDULE);
		napi_reschedule(napi);
	}

	return pkts;
}

#ifdef CONFIG_RPS
enum {
	SIPA_RPS_MODE_MULTI_LITTLE,
	SIPA_RPS_MODE_MULTI_MIDDLE,
	SIPA_RPS_MODE_MULTI_MIDDLE_LARGE,
};

bool sipa_dummy_set_rps_mode(int rps_mode)
{
	int rc;
	struct net_device *netdev;
	struct netdev_rx_queue *queue;
	struct kobject *kobj;
	struct attribute **attr;
	char buf[10] = {};
	size_t len = sizeof(buf);

	pr_info("start to set rps, rps_mode %d\n", rps_mode);

	switch (rps_mode) {
	case SIPA_RPS_MODE_MULTI_LITTLE:
		snprintf(buf, len, "%s", "0F");
		break;
	case SIPA_RPS_MODE_MULTI_MIDDLE:
		snprintf(buf, len, "%s", "70");
		break;
	case SIPA_RPS_MODE_MULTI_MIDDLE_LARGE:
		snprintf(buf, len, "%s", "F0");
		break;
	default:
		pr_err("unsupported rps mode %d\n", rps_mode);
		return false;
	}

	netdev = sipa_dummy_ndev_arr[SIPA_DUMMY_ETH0].ndev;
	if (!netdev) {
		pr_err("fail to get eth dev\n");
		return false;
	}
	/* get rx-0 */
	queue = netdev->_rx + 0;
	kobj = &queue->kobj;
	if (!kobj) {
		pr_err("kobj is null\n");
		return false;
	}

	/*
	 * in kernel5.15 default_attrs in not initialized,
	 * it replaced by attrs in default_groups
	 */
	attr = (*kobj->ktype->default_groups)->attrs;
	if (!attr) {
		pr_err("attr is null\n");
		return false;
	}

	/*
	 * get rx_queue_default_attrs[] => rps_cpus_attribute
	 * and "store_rps_map" func is invoked
	 */
	rc = kobj->ktype->sysfs_ops->store(kobj, attr[0], buf, len);

	pr_info("finish to set rps to %s, rc %d\n", buf, rc);
	return true;
}
EXPORT_SYMBOL(sipa_dummy_set_rps_mode);

bool sipa_dummy_set_rps_cpus(u8 rps_cpus)
{
	int rc;
	struct net_device *netdev;
	struct netdev_rx_queue *queue;
	struct kobject *kobj;
	struct attribute **attr;
	char buf[10] = {};
	size_t len = sizeof(buf);

	pr_info("start to set rps, rps_cpus 0x%x\n", rps_cpus);

	snprintf(buf, len, "%x", rps_cpus);

	netdev = sipa_dummy_ndev_arr[SIPA_DUMMY_ETH0].ndev;
	if (!netdev) {
		pr_err("fail to get eth dev\n");
		return false;
	}
	/* get rx-0 */
	queue = netdev->_rx + 0;
	kobj = &queue->kobj;
	if (!kobj) {
		pr_err("kobj is null\n");
		return false;
	}

	/*
	 * in kernel5.15 default_attrs in not initialized,
	 * it replaced by attrs in default_groups
	 */
	attr = (*kobj->ktype->default_groups)->attrs;
	if (!attr) {
		pr_err("attr is null\n");
		return false;
	}

	/*
	 * get rx_queue_default_attrs[] => rps_cpus_attribute
	 * and "store_rps_map" func is invoked
	 */
	rc = kobj->ktype->sysfs_ops->store(kobj, attr[0], buf, len);

	pr_info("finish to set rps cpus to %s, rc %d\n", buf, rc);
	return true;
}
EXPORT_SYMBOL(sipa_dummy_set_rps_cpus);
#endif

/* for sipa to invoke */
irqreturn_t sipa_dummy_recv_trigger(unsigned int cur_cpu)
{
	int err = 0;
	struct sipa_dummy *dummy;

	if (!dummy_dev) {
		if (__ratelimit(&rlimit_dummy))
			pr_err("dummy device not register yet");
		err = -ENODEV;
		goto err_exit;
	}

	dummy = netdev_priv(dummy_dev);
	sipa_dummy_set_bootts(dummy, SIPA_DUMMY_TS_IRQ_TRIGGER);
	dummy->rings[cur_cpu].fifoid = cur_cpu;
	napi_schedule(&dummy->rings[cur_cpu].napi);

err_exit:
	return err >= 0 ? IRQ_HANDLED : IRQ_NONE;
}
EXPORT_SYMBOL(sipa_dummy_recv_trigger);

static netdev_tx_t sipa_dummy_start_xmit(struct sk_buff *skb,
					 struct net_device *ndev)
{
	struct sipa_dummy *dummy = netdev_priv(ndev);

	/* update netdev statistics */
	dummy->stats.tx_packets++;
	dummy->stats.tx_bytes += skb->len;

	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

/* Open interface */
static int sipa_dummy_open(struct net_device *ndev)
{
	int i;
	struct sipa_dummy *dummy = netdev_priv(ndev);

	pr_info("dummy open\n");
	if (!netif_carrier_ok(ndev))
		netif_carrier_on(ndev);

	for (i = 0; i < SIPA_DUMMY_MAX_CPUS; i++)
		napi_enable(&dummy->rings[i].napi);
	netif_start_queue(ndev);

	return 0;
}

/* Close interface */
static int sipa_dummy_close(struct net_device *ndev)
{
	int i;

	struct sipa_dummy *dummy = netdev_priv(ndev);

	pr_info("dummy close\n");
	for (i = 0; i < SIPA_DUMMY_MAX_CPUS; i++)
		napi_disable(&dummy->rings[i].napi);
	netif_stop_queue(ndev);

	return 0;
}

static struct net_device_stats *sipa_dummy_get_stats(struct net_device *ndev)
{
	struct sipa_dummy *dummy = netdev_priv(ndev);

	return &dummy->stats;
}

static int sipa_dummy_change_mtu(struct net_device *ndev, int new_mtu)
{
	if (new_mtu < 0 || new_mtu > SIPA_DUMMY_MAX_PACKET_SIZE)
		return -EINVAL;

	ndev->mtu = new_mtu;
	return 0;
}

static int sipa_dummy_set_mac_address(struct net_device *ndev, void *priv)
{
	struct sipa_dummy *dummy = netdev_priv(ndev);
	struct sockaddr *addr = priv;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	ether_addr_copy(dummy->mac_addr, addr->sa_data);
	ether_addr_copy((u8 *)ndev->dev_addr, addr->sa_data);

	return 0;
}

static const struct net_device_ops sipa_dummy_ndev_ops = {
	.ndo_open = sipa_dummy_open,
	.ndo_stop = sipa_dummy_close,
	.ndo_start_xmit = sipa_dummy_start_xmit,
	.ndo_get_stats = sipa_dummy_get_stats,
	.ndo_change_mtu = sipa_dummy_change_mtu,
	.ndo_set_mac_address = sipa_dummy_set_mac_address,
};

static void sipa_dummy_setup(struct net_device *dev)
{
	ether_setup(dev);
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
}

static int sipa_dummy_debug_show(struct seq_file *m, void *v)
{
	int i;
	struct sipa_dummy *dummy = (struct sipa_dummy *)(m->private);

	if (!dummy) {
		pr_err("invalid data, sipa_dummy is NULL\n");
		return -EINVAL;
	}

	seq_printf(m, "DEVICE: %s\n",
		   dummy->ndev->name);

	seq_printf(m, "Total rx_packet %lu rx_bytes %lu\n",
		   dummy->stats.rx_packets,
		   dummy->stats.rx_bytes);

	seq_puts(m, "Stats List:\n");
	for (i = 0; i < SIPA_DUMMY_MAX_CPUS; i++) {
		seq_printf(m, "CPU%d: rx_packet %lu, rx_bytes %lu\n",
			   i, dummy->per_stats[i].rx_packets,
			   dummy->per_stats[i].rx_bytes);
		seq_printf(m, "last_read_empty\t\t%llu\n",
			   dummy->rings[i].last_read_empty);
		seq_printf(m, "last_napi_reschedule\t%llu\n",
			   dummy->rings[i].last_napi_reschedule);
		seq_printf(m, "last_napi_complete\t%llu\n",
			   dummy->rings[i].last_napi_complete);
		seq_printf(m, "last_irq_trigger\t%llu\n",
			   dummy->rings[i].last_irq_trigger);
	}

	seq_puts(m, "Dev List:\n");
	for (i = 0; i < SIPA_DUMMY_NDEV_MAX; i++) {
		struct sipa_dummy_ndev_info *ndev_info;

		ndev_info = &sipa_dummy_ndev_arr[i];
		if (!ndev_info->ndev) {
			seq_printf(m, "device %s: never used\n",
				   sipa_dummy_ndev2str(i));
			continue;
		}
		seq_printf(m, "device %s:\n",
			   sipa_dummy_ndev2str(i));

		seq_printf(m, "\b\b\bsrc_id %d netid %d state %d napi %p\n",
			   ndev_info->src_id, ndev_info->netid,
			   ndev_info->state, ndev_info->napi);
		seq_printf(m, "\b\b\brx_packets %lu rx_bytes %lu\n",
			   ndev_info->ndev->stats.rx_packets,
			   ndev_info->ndev->stats.rx_bytes);
		seq_printf(m, "\b\b\btx_packets %lu tx_bytes %lu\n",
			   ndev_info->ndev->stats.tx_packets,
			   ndev_info->ndev->stats.tx_bytes);
		seq_printf(m, "\b\b\bflow control %d\n",
			   netif_queue_stopped(ndev_info->ndev));
	}

	return 0;
}

static int sipa_dummy_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_dummy_debug_show, inode->i_private);
}

static const struct file_operations sipa_dummy_debug_fops = {
	.open = sipa_dummy_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sipa_dummy_affinity_show(struct seq_file *m, void *v)
{
	u32 size;
	char *buf;

	size = seq_get_buf(m, &buf);

	seq_printf(m, "now fifo 0 is on cpu %d\n", size);
	return 0;
}

static int sipa_dummy_affinity_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_dummy_affinity_show, inode->i_private);
}

static ssize_t sipa_dummy_affinity_write(struct file *filp,
					 const char __user *buf,
					 size_t count, loff_t *offp)
{
	int r;
	char buffer[64];
	unsigned long val;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	void __iomem *glb_base = ipa->glb_virt_base;

	if (count >= sizeof(buffer))
		return -ENOMEM;

	if (copy_from_user(&buffer[0], buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	r = kstrtoul(&buffer[0], 10, &val);
	if (r)
		return -EINVAL;

	ipa->glb_ops.map_multi_fifo_mode_en(glb_base, false);
	pm_stay_awake(ipa->dev);
	ipa->cpu_num = val;
	ipa->cpu_num_ano = val;

	if (!cpu_online(ipa->cpu_num)) {
		pm_relax(ipa->dev);
		return -EINVAL;
	}

	pr_info("set fifo 0 to cpu %lu\n", val);
	sipa_hal_config_irq_affinity(0, ipa->cpu_num);
	pm_relax(ipa->dev);

	*offp += count;
	return count;
}

static const struct file_operations sipa_dummy_affinity_fops = {
	.open = sipa_dummy_affinity_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = sipa_dummy_affinity_write,
};

static int sipa_dummy_debugfs_mknod(void *data)
{
	if (!dummy_debugfs_root) {
		pr_err("dummy root dir not exist\n");
		return -ENXIO;
	}

	debugfs_create_file("stats",
			    0444,
			    dummy_debugfs_root,
			    data,
			    &sipa_dummy_debug_fops);
	debugfs_create_file("affinity",
			    0644,
			    dummy_debugfs_root,
			    data,
			    &sipa_dummy_affinity_fops);
	return 0;
}

static void sipa_dummy_cfg_start(struct net_device *ndev)
{
	struct sipa_dummy *dummy = netdev_priv(ndev);
	struct sipa_dummy_cfg *cfg = &dummy->cfg;

	cfg->mtu = SIPA_DUMMY_MTU_DEF;
}

static void sipa_dummy_ndev_init(struct net_device *ndev)
{
	int i;
	struct sipa_dummy *dummy = netdev_priv(ndev);

	ndev->netdev_ops = &sipa_dummy_ndev_ops;
	ndev->mtu = dummy->cfg.mtu - ETH_HLEN;
	ndev->features &= ~(NETIF_F_IP_CSUM |
			    NETIF_F_IPV6_CSUM |
			    NETIF_F_HW_CSUM);
	eth_random_addr(dummy->mac_addr);
	ether_addr_copy(ndev->dev_addr, dummy->mac_addr);
	pr_info("got MAC address: %pM\n", ndev->dev_addr);

	for (i = 0; i < SIPA_DUMMY_MAX_CPUS; i++) {
		dummy->rings[i].ndev = dummy->ndev;
		netif_napi_add(ndev, &dummy->rings[i].napi,
			       sipa_dummy_rx_poll,
			       SIPA_DUMMY_NAPI_WEIGHT);
		dummy->rings[i].last_read_empty = 0;
		dummy->rings[i].last_napi_complete = 0;
		dummy->rings[i].last_napi_reschedule = 0;
		dummy->rings[i].last_irq_trigger = 0;
	}
}

static int sipa_dummy_probe(void)
{
	struct sipa_dummy *dummy;
	struct net_device *ndev;
	int ret;
	int i;
	const unsigned int txqs = 1;
	const unsigned int rxqs = SIPA_DUMMY_MAX_CPUS;

	ndev = alloc_netdev_mqs(sizeof(struct sipa_dummy),
				"sipa_dummy%d",
				NET_NAME_UNKNOWN,
				sipa_dummy_setup, txqs, rxqs);

	if (!ndev) {
		pr_err("alloc_netdev() failed.\n");
		return -ENOMEM;
	}

	/* It is too ugly here. */
	dummy_dev = ndev;
	dummy = netdev_priv(ndev);
	dummy->ndev = ndev;
	dummy->stats = ndev->stats;

	sipa_dummy_cfg_start(ndev);
	sipa_dummy_ndev_init(ndev);

	/* Register new Ethernet interface */
	ret = register_netdev(ndev);
	if (ret) {
		pr_err("register_netdev() failed (%d)\n", ret);
		for (i = 0; i < SIPA_DUMMY_MAX_CPUS; i++)
			netif_napi_del(&dummy->rings[i].napi);
		free_netdev(ndev);
		return ret;
	}

	/* need do open dummy after register */
	rtnl_lock();
	dev_open(ndev, NULL);
	rtnl_unlock();

	sipa_dummy = dummy;
	sipa_dummy_debugfs_mknod((void *)dummy);
	return 0;
}

static void sipa_dummy_netdev_join(struct net_device *ndev)
{
	void *priv;
	struct sipa_dummy_ndev_info *ndev_info;

	priv = netdev_priv(ndev);
	if (!strncmp(ndev->name, "sipa_usb", 8)) {
		struct sipa_usb *usb_priv = (struct sipa_usb *)priv;

		ndev_info = &sipa_dummy_ndev_arr[SIPA_DUMMY_USB0];
		ndev_info->src_id = usb_priv->pdata->src_id;
		ndev_info->netid = usb_priv->pdata->netid;
		ndev_info->ndev = ndev;
		ndev_info->napi = &usb_priv->napi;
		ndev_info->state = SIPA_DUMMY_DEV_OFF;
	} else if (!strncmp(ndev->name, "sipa_eth", 8)) {
		int netid = 0;
		struct sipa_eth *eth_priv = (struct sipa_eth *)priv;

		netid = eth_priv->pdata->netid;
		ndev_info = &sipa_dummy_ndev_arr[netid];
		ndev_info->src_id = eth_priv->pdata->src_id;
		ndev_info->netid = eth_priv->pdata->netid;
		ndev_info->ndev = ndev;
		ndev_info->napi = &eth_priv->napi;
		ndev_info->state = SIPA_DUMMY_DEV_OFF;
	}
}

static void sipa_dummy_netdev_set_state(struct net_device *ndev, bool up)
{
	void *priv;
	int netid = 0;
	struct sipa_dummy_ndev_info *ndev_info;

	if (!net_eq(ndev->nd_net.net, &init_net))
		return;
	priv = netdev_priv(ndev);
	if (!strncmp(ndev->name, "sipa_usb", 8)) {
		ndev_info = &sipa_dummy_ndev_arr[SIPA_DUMMY_USB0];
		ndev_info->state = up ? SIPA_DUMMY_DEV_ON : SIPA_DUMMY_DEV_OFF;
	} else if (!strncmp(ndev->name, "sipa_eth", 8)) {
		struct sipa_eth *eth_priv = (struct sipa_eth *)priv;

		netid = eth_priv->pdata->netid;
		ndev_info = &sipa_dummy_ndev_arr[netid];
		ndev_info->state = up ? SIPA_DUMMY_DEV_ON : SIPA_DUMMY_DEV_OFF;
	}
}

static int sipa_dummy_netdev_event_handler(struct notifier_block *nb,
					   unsigned long event, void *ptr)
{
	int ret = NOTIFY_DONE;
	struct net_device *ndev = netdev_notifier_info_to_dev(ptr);
	static int debug_cnt;

	if (!ndev)
		return ret;

	switch (event) {
	case NETDEV_REGISTER:
		ret = NOTIFY_OK;
		sipa_dummy_netdev_join(ndev);

		break;
	case NETDEV_UP:
		ret = NOTIFY_OK;
		sipa_dummy_netdev_set_state(ndev, true);
		if (!strncmp(ndev->name, "usb0", 4)) {
			sipa_nic_set_bypass_mode(false);
			debug_cnt++;
		}

		break;
	case NETDEV_DOWN:
		if (!strncmp(ndev->name, "usb0", 4)) {
			sipa_nic_set_bypass_mode(true);
			debug_cnt--;
		}

		fallthrough;
	case NETDEV_UNREGISTER:
		ret = NOTIFY_OK;
		sipa_dummy_netdev_set_state(ndev, false);

		break;
	default:
		break;
	}

	pr_info("dev %s evt %lu debug_cnt is %d\n", ndev->name, event, debug_cnt);

	return ret;
}

static struct notifier_block sipa_dummy_netdev_notifier = {
	.notifier_call = sipa_dummy_netdev_event_handler,
};

int sipa_dummy_reg_netdev_notifier(void)
{
	int ret;

	ret = register_netdevice_notifier(&sipa_dummy_netdev_notifier);
	pr_debug("reg netdev notifier, ret %d\n", ret);
	return ret;
}

void sipa_dummy_unreg_netdev_notifier(void)
{
	pr_debug("unreg netdev notifier\n");
	unregister_netdevice_notifier(&sipa_dummy_netdev_notifier);
}

static void sipa_dummy_debugfs_init(void)
{
	dummy_debugfs_root = debugfs_create_dir("sipa_dummy", NULL);
	if (!dummy_debugfs_root)
		pr_err("failed to create debugfs dir\n");
}

static void sipa_dummy_debugfs_remove(void)
{
	debugfs_remove_recursive(dummy_debugfs_root);
}

#if NO_CONNTRACK
static int
sipa_ctnetlink_conntrack_event(unsigned int events,
			       const struct nf_ct_event *item)
{
	struct nf_conntrack_tuple *tuple;

	tuple = &item->ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;

	if (events & (1 << IPCT_NEW)) {
		if (tuple->dst.protonum == IP_L4_PROTO_UDP &&
		    ntohs(tuple->dst.u.udp.port) == 5201) {
			sipa_udp_is_frag(true);
			udp_frag_flow_num++;
			return 0;
		}

		if (tuple->src.l3num != NFPROTO_IPV4)
			return 0;

		if (tuple->dst.protonum != IPPROTO_UDP)
			return 0;

		if (tuple->dst.u.udp.port == ntohs(5202)) {
			sipa_udp_is_port(true);
			pr_info("notify sipa with special port\n");
			return 0;
		}
	}

	if (events & (1 << IPCT_DESTROY)) {
		if (tuple->dst.protonum == IP_L4_PROTO_UDP &&
		    ntohs(tuple->dst.u.udp.port) == 5201) {
			if (udp_frag_flow_num == 1) {
				sipa_udp_is_frag(false);
				udp_frag_flow_num--;
			} else {
				udp_frag_flow_num--;
			}
			return 0;
		}

		if (tuple->src.l3num != NFPROTO_IPV4)
			return 0;

		if (tuple->dst.protonum != IPPROTO_UDP)
			return 0;

		if (tuple->dst.u.udp.port == ntohs(5202)) {
			sipa_udp_is_port(false);
			pr_info("notify sipa with special port destroy\n");
			return 0;
		}
	}

	return 0;
}

#ifdef CONFIG_NF_CONNTRACK_EVENTS
static struct nf_ct_event_notifier sipa_ctnl_notifier = {
	.ct_event = sipa_ctnetlink_conntrack_event,
};
#endif
#endif

int sipa_dummy_init(void)
{
	sipa_dummy_reg_netdev_notifier();
	sipa_dummy_debugfs_init();
#if NO_CONNTRACK
#ifdef CONFIG_NF_CONNTRACK_EVENTS
	nf_conntrack_register_notifier(&init_net, &sipa_ctnl_notifier);
#endif
#endif
	return sipa_dummy_probe();
}
EXPORT_SYMBOL_GPL(sipa_dummy_init);

void sipa_dummy_exit(void)
{
	sipa_dummy_unreg_netdev_notifier();
	sipa_dummy_debugfs_remove();
	dev_close(dummy_dev);
	unregister_netdev(dummy_dev);
	free_netdev(dummy_dev);
	dummy_dev = NULL;
}
EXPORT_SYMBOL_GPL(sipa_dummy_exit);

MODULE_AUTHOR("Wade.Shu");
MODULE_DESCRIPTION("Unisoc dummy device for IPA RX");
MODULE_LICENSE("GPL v2");
