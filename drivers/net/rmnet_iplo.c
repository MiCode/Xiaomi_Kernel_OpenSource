/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/msm_rmnet.h>

unsigned int multiplication_factor = 1;
module_param(multiplication_factor, uint, 0644);

struct net_device *mydevice;

static void iplo_do_ip_loopback(struct sk_buff *skb, int ip_offset)
{
	struct iphdr *hdr;
	struct icmphdr *icmp;
	__be32 ipaddr;
	int i;

	hdr = (struct iphdr *)(skb->data + ip_offset);
	ipaddr = hdr->saddr;
	hdr->saddr = hdr->daddr;
	hdr->daddr = ipaddr;
	switch (hdr->protocol) {
	case 1: /* ICMP */
		icmp = (struct icmphdr *)(skb->data + ip_offset + hdr->ihl * 4);
		if (icmp->type == ICMP_ECHO)
			icmp->type = ICMP_ECHOREPLY;
		break;
	case 11: /* UDP */
		break;
	case 6: /* TCP */
		break;
	default:
		break;
	}
	if (multiplication_factor < 2) {
		netif_rx(skb);
		skb->dev->stats.tx_packets++;
	} else {
		for (i = 0; i < (multiplication_factor - 1); i++) {
			netif_rx(skb_copy(skb, GFP_ATOMIC));
			skb->dev->stats.tx_packets++;
		}
		netif_rx(skb);
		skb->dev->stats.tx_packets++;
	}
}

static netdev_tx_t iplo_vnd_start_xmit(struct sk_buff *skb,
				       struct net_device *dev)
{
	int ip_offset = 0;

	switch (ntohs(skb->protocol)) {
	case ETH_P_MAP:
		ip_offset = 4;
	case ETH_P_IP:
		iplo_do_ip_loopback(skb, ip_offset);
		dev->stats.rx_packets++;
		break;
	default:
		dev->stats.tx_dropped++;
		kfree_skb(skb);
		break;
	}
	return NETDEV_TX_OK;
}

static int iplo_vnd_ioctl_extended(struct net_device *dev, struct ifreq *ifr)
{
	struct rmnet_ioctl_extended_s ext_cmd;
	int rc = 0;

	rc = copy_from_user(&ext_cmd, ifr->ifr_ifru.ifru_data,
			    sizeof(struct rmnet_ioctl_extended_s));

	if (rc) {
		pr_err("%s() copy_from_user failed, error %d\n", __func__, rc);
		return rc;
	}

	switch (ext_cmd.extended_ioctl) {
	case RMNET_IOCTL_SET_MRU:
		break;
	case RMNET_IOCTL_GET_EPID:
		ext_cmd.u.data = 100;
		break;
	case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
		ext_cmd.u.data = 0;
		break;
	case RMNET_IOCTL_GET_DRIVER_NAME:
		strlcpy(ext_cmd.u.if_name, "rmnet_mhi",
			sizeof(ext_cmd.u.if_name));
		break;
	default:
		rc = -EINVAL;
		break;
	}

	rc = copy_to_user(ifr->ifr_ifru.ifru_data, &ext_cmd,
			  sizeof(struct rmnet_ioctl_extended_s));

	if (rc)
		pr_err("%s() copy_to_user failed, error %d\n", __func__, rc);

	return rc;
}

static int iplo_vnd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int rc = 0;

	struct rmnet_ioctl_data_s ioctl_data;

	switch (cmd) {
	case RMNET_IOCTL_SET_LLP_IP:        /* Set RAWIP protocol */
		break;
	case RMNET_IOCTL_GET_LLP:           /* Get link protocol state */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
				 sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_GET_OPMODE:        /* Get operation mode      */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
				 sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_SET_QOS_ENABLE:
		rc = -EINVAL;
		break;
	case RMNET_IOCTL_SET_QOS_DISABLE:
		rc = 0;
		break;
	case RMNET_IOCTL_OPEN:
	case RMNET_IOCTL_CLOSE:
		/* We just ignore them and return success */
		rc = 0;
		break;
	case RMNET_IOCTL_EXTENDED:
		rc = iplo_vnd_ioctl_extended(dev, ifr);
		break;
	default:
		/* Don't fail any IOCTL right now */
		rc = 0;
		break;
	}

	return rc;
}

static int iplo_vnd_change_mtu(struct net_device *dev, int new_mtu)
{
	if (0 > new_mtu || 16384 < new_mtu)
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

static const struct net_device_ops iplo_device_ops = {
	.ndo_init = 0,
	.ndo_do_ioctl = iplo_vnd_ioctl,
	.ndo_start_xmit = iplo_vnd_start_xmit,
	.ndo_change_mtu = iplo_vnd_change_mtu,
};

static void iplo_device_setup(struct net_device *dev)
{
	dev->flags |= IFF_NOARP;
	dev->netdev_ops = &iplo_device_ops;
	dev->mtu = 1500;
	dev->needed_headroom = 0;
	dev->watchdog_timeo = 100;
	dev->header_ops = 0;
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
	dev->tx_queue_len = 1000;
}

int __init rmnet_iplo_init(void)
{
	int rc;

	pr_err("iplo: Module is coming up\n");
	mydevice = alloc_netdev(100, "rmnet_mhi0", NET_NAME_ENUM,
				iplo_device_setup);
	if (!mydevice) {
		pr_err("iplo: Failed to to allocate netdev for iplo\n");
		return -EINVAL;
	}
	rtnl_lock();
	rc = register_netdevice(mydevice);
	rtnl_unlock();
	if (rc != 0) {
		pr_err("iplo: Failed to to register netdev [%s]\n",
		       mydevice->name);
		free_netdev(mydevice);
		return -EINVAL;
	}
	return 0;
}

void __exit rmnet_iplo_exit(void)
{
	unregister_netdev(mydevice);
	free_netdev(mydevice);
	pr_err("iplo: Module is going away\n");
}

module_init(rmnet_iplo_init)
module_exit(rmnet_iplo_exit)
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("RmNet IP Loop Back Driver");
