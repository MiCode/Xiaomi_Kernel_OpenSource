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

#define pr_fmt(fmt) "sipa_usb: " fmt

#include <linux/debugfs.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>

#include "sipa_eth.h"

/* Device status */
#define DEV_ON 1
#define DEV_OFF 0

#define SIPA_USB_NAPI_WEIGHT 64
#define SIPA_USB_IFACE_PREF  "sipa_usb"

#ifdef CONFIG_DEBUG_FS
static struct dentry *root;
#endif

static void sipa_usb_flowctrl_handler(void *priv, int flowctrl)
{
	struct sipa_usb *usb = (struct sipa_usb *)priv;

	if (netif_queue_stopped(usb->ndev))
		netif_wake_queue(usb->ndev);
}

static void sipa_usb_notify_cb(void *priv, enum sipa_evt_type evt,
			       unsigned long data)
{
	switch (evt) {
	case SIPA_LEAVE_FLOWCTRL:
		pr_info("SIPA LEAVE FLOWCTRL\n");
		sipa_usb_flowctrl_handler(priv, 0);
		break;
	/* following cases may be deprecated */
	case SIPA_RECEIVE:
	case SIPA_ENTER_FLOWCTRL:
	default:
		break;
	}
}

static netdev_tx_t sipa_usb_start_xmit(struct sk_buff *skb,
				       struct net_device *ndev)
{
	struct sipa_usb *usb = netdev_priv(ndev);
	struct sipa_usb_init_data *pdata = usb->pdata;
	struct net_device_stats *stats;
	int ret = 0;
	int netid;
	u32 len;

	stats = usb->stats;
	if (usb->state != DEV_ON) {
		pr_err("sipa_usb called when %s is down\n", ndev->name);
		stats->tx_dropped++;
		netif_carrier_off(ndev);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	netid = pdata->netid;

	ret = skb_linearize(skb);
	if (unlikely(ret)) {
		stats->tx_dropped++;
		dev_kfree_skb_any(skb);
		return ret;
	}

	/* skb maybe freed by tx free thread
	 * after called sipa_nic_tx
	 */
	len = skb->len;

	ret = sipa_nic_tx(usb->nic_id, pdata->src_id, netid, skb);
	if (unlikely(ret != 0)) {
		pr_err("sipa_usb fail to send skb, ret %d\n", ret);
		if (ret == -EAGAIN || ret == -EINPROGRESS) {
			stats->tx_dropped++;
			stats->tx_errors++;
			netif_stop_queue(ndev);
			sipa_nic_trigger_flow_ctrl_work(usb->nic_id, ret);
			return NETDEV_TX_BUSY;
		}
	}

	/* update netdev statistics */
	stats->tx_packets++;
	stats->tx_bytes += len;

	return NETDEV_TX_OK;
}

/* Open interface */
static int sipa_usb_open(struct net_device *dev)
{
	struct sipa_usb *usb = netdev_priv(dev);
	struct sipa_usb_init_data *pdata = usb->pdata;
	int ret = 0;

	pr_info("dev 0x%p eth 0x%p open %s netid %d src_id %d\n",
		dev, usb, dev->name, pdata->netid, pdata->src_id);
	ret = sipa_nic_open(pdata->src_id,
			    pdata->netid,
			    sipa_usb_notify_cb,
			    (void *)usb);

	if (ret < 0) {
		pr_err("fail to open, ret %d\n", ret);
		return ret;
	}
	usb->nic_id = ret;
	usb->state = DEV_ON;

	sipa_ep_recv_enable(SIPA_EP_USB, false);

	sipa_rm_enable_usb_tether();
	sipa_rm_set_usb_eth_up();

	if (!netif_carrier_ok(usb->ndev)) {
		pr_info("set netif_carrier_on\n");
		netif_carrier_on(usb->ndev);
	}

	netif_start_queue(dev);

	return 0;
}

/* Close interface */
static int sipa_usb_close(struct net_device *dev)
{
	struct sipa_usb *usb = netdev_priv(dev);

	pr_info("close %s!\n", dev->name);

	sipa_nic_close(usb->nic_id);
	usb->state = DEV_OFF;

	sipa_ep_recv_enable(SIPA_EP_USB, true);

	netif_stop_queue(dev);

	sipa_rm_set_usb_eth_down();
	return 0;
}

static struct net_device_stats *sipa_usb_get_stats(struct net_device *dev)
{
	struct sipa_usb *usb = netdev_priv(dev);

	return usb->stats;
}

/* for yocto tethering */
static struct device_type gadget_type = {
	.name = "gadget",
};

static const struct net_device_ops sipa_usb_ops = {
	.ndo_open = sipa_usb_open,
	.ndo_stop = sipa_usb_close,
	.ndo_start_xmit = sipa_usb_start_xmit,
	.ndo_get_stats = sipa_usb_get_stats,
};

static ssize_t gro_enable_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sipa_eth *sipa_eth = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", sipa_eth->gro_enable);
}

static ssize_t gro_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	int gro_enable;
	struct sipa_usb *sipa_usb = dev_get_drvdata(dev);

	ret = kstrtou32(buf, 10, &gro_enable);
	if (ret)
		return ret;

	sipa_usb->gro_enable = gro_enable;

	return count;
}

static DEVICE_ATTR_RW(gro_enable);

static struct attribute *sipa_usb_attributes[] = {
	&dev_attr_gro_enable.attr,
	NULL,
};

static struct attribute_group sipa_usb_attribute_group = {
	.attrs = sipa_usb_attributes,
};

static int sipa_usb_parse_dt(struct sipa_usb_init_data **init,
			     struct device *dev)
{
	struct sipa_usb_init_data *pdata = NULL;
	struct device_node *np = dev->of_node;
	int ret;
	u32 udata;
	s32 sdata;

	if (!np)
		pr_err("dev of_node np is null\n");

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	snprintf(pdata->name, IFNAMSIZ, "%s%d",
		 SIPA_USB_IFACE_PREF, 0);

	ret = of_property_read_s32(np, "sprd,netid", &sdata);
	if (ret) {
		pr_err("read sprd,netid ret %d\n", ret);
		return ret;
	}
	/* dts reflect */
	pdata->netid = sdata - 1;

	ret = of_property_read_u32(np, "sprd,term-type", &udata);
	if (ret) {
		pr_err("read sprd,term-type ret %d\n", ret);
		return ret;
	}

	pdata->src_id = udata;

	*init = pdata;
	pr_info("after dt parse, name %s netid %d src_id %d\n",
		pdata->name, pdata->netid, pdata->src_id);
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int sipa_usb_debug_show(struct seq_file *m, void *v)
{
	struct sipa_usb *usb = (struct sipa_usb *)(m->private);
	struct net_device_stats *stats;
	struct sipa_usb_init_data *pdata;

	if (!usb) {
		pr_err("invalid data, sipa_usb is NULL\n");
		return -EINVAL;
	}
	pdata = usb->pdata;
	stats = usb->stats;

	seq_puts(m, "*************************************************\n");
	seq_printf(m, "DEVICE: %s, src_id %d, netid %d, state %s\n",
		   pdata->name, pdata->src_id, pdata->netid,
		   usb->state == DEV_ON ? "UP" : "DOWN");

	seq_printf(m, "SG feature %s\n",
		   usb->ndev->hw_features & NETIF_F_SG ? "on" : "off");
	seq_puts(m, "\nRX statistics:\n");
	seq_printf(m, "rx_bytes=%lu, rx_packets=%lu\n",
		   stats->rx_bytes,
		   stats->rx_packets);
	seq_printf(m, "rx_errors=%lu, rx_dropped=%lu\n",
		   stats->rx_errors,
		   stats->rx_dropped);

	seq_puts(m, "\nTX statistics:\n");
	seq_printf(m, "tx_bytes=%lu, tx_packets=%lu\n",
		   stats->tx_bytes,
		   stats->tx_packets);
	seq_printf(m, "tx_errors=%lu, tx_dropped=%lu\n",
		   stats->tx_errors,
		   stats->tx_dropped);

	seq_puts(m, "*************************************************\n");

	return 0;
}

static int sipa_usb_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_usb_debug_show, inode->i_private);
}

static const struct file_operations sipa_usb_debug_fops = {
	.open = sipa_usb_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sipa_usb_debugfs_mknod(void *root, void *data)
{
	struct sipa_usb *usb = (struct sipa_usb *)data;

	if (!usb)
		return -ENODEV;

	if (!root)
		return -ENXIO;

	debugfs_create_file(SIPA_USB_IFACE_PREF,
			    0444,
			    (struct dentry *)root,
			    data,
			    &sipa_usb_debug_fops);

	return 0;
}
#endif

static int sipa_usb_probe(struct platform_device *pdev)
{
	struct sipa_usb_init_data *pdata = pdev->dev.platform_data;
	struct net_device *ndev;
	struct sipa_usb *usb;
	char ifname[IFNAMSIZ];
	int ret;

	if (pdev->dev.of_node && !pdata) {
		ret = sipa_usb_parse_dt(&pdata, &pdev->dev);
		if (ret) {
			pr_err("failed to parse device tree, ret=%d\n", ret);
			return ret;
		}
		pdev->dev.platform_data = pdata;
	}

	strlcpy(ifname, pdata->name, IFNAMSIZ);
	ndev = alloc_netdev(sizeof(struct sipa_usb),
			    ifname,
			    NET_NAME_UNKNOWN,
			    ether_setup);

	if (!ndev) {
		pr_err("alloc_netdev() failed.\n");
		return -ENOMEM;
	}

	ndev->type = ARPHRD_ETHER;

	usb = netdev_priv(ndev);
	usb->ndev = ndev;
	usb->pdata = pdata;
	usb->stats = &ndev->stats;
	usb->gro_enable = 1;
	ndev->netdev_ops = &sipa_usb_ops;
	ndev->watchdog_timeo = 1 * HZ;
	ndev->irq = 0;
	ndev->dma = 0;

	random_ether_addr(ndev->dev_addr);

	SET_NETDEV_DEVTYPE(ndev, &gadget_type);

	ndev->hw_features |= NETIF_F_RXCSUM;
	ndev->hw_features |= NETIF_F_SG;
	ndev->features |= ndev->hw_features;
	/* Register new Ethernet interface */
	ret = register_netdev(ndev);
	if (ret) {
		pr_err("register_netdev() failed (%d)\n", ret);
		free_netdev(ndev);
		return ret;
	}

	usb->state = DEV_OFF;
	/* Set link as disconnected */
	netif_carrier_off(ndev);
	platform_set_drvdata(pdev, usb);
	ret = sysfs_create_group(&pdev->dev.kobj, &sipa_usb_attribute_group);
	if (ret) {
		pr_err("sysfs_create_group err %d\n", ret);
		free_netdev(ndev);
		return ret;
	}
#ifdef CONFIG_DEBUG_FS
	sipa_usb_debugfs_mknod(root, (void *)usb);
#endif
	return 0;
}

/* Cleanup Ethernet device driver. */
static int sipa_usb_remove(struct platform_device *pdev)
{
	struct sipa_usb *usb = platform_get_drvdata(pdev);

	unregister_netdev(usb->ndev);
	free_netdev(usb->ndev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id sipa_usb_match_table[] = {
	{ .compatible = "sprd,sipa_usb" },
	{ }
};

static struct platform_driver sipa_usb_driver = {
	.probe = sipa_usb_probe,
	.remove = sipa_usb_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = SIPA_USB_IFACE_PREF,
		.of_match_table = sipa_usb_match_table
	}
};

static void __init sipa_usb_debugfs_init(void)
{
#ifdef CONFIG_DEBUG_FS
	root = debugfs_create_dir(SIPA_USB_IFACE_PREF, NULL);
	if (!root)
		pr_err("failed to create sipa_usb debugfs dir\n");
#endif
}

static int __init sipa_usb_init(void)
{
	int ret;

	sipa_usb_debugfs_init();
	ret = platform_driver_register(&sipa_usb_driver);
	return ret;
}

static void __exit sipa_usb_exit(void)
{
	platform_driver_unregister(&sipa_usb_driver);
}

module_init(sipa_usb_init);
module_exit(sipa_usb_exit);

MODULE_AUTHOR("Wade.Shu");
MODULE_DESCRIPTION("Unisoc USB Ethernet device driver for IPA");
MODULE_LICENSE("GPL v2");
