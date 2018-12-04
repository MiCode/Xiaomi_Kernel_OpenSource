/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include "atl_common.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>

const char atl_driver_name[] = "atlantic-fwd";

int atl_max_queues = ATL_MAX_QUEUES;
module_param_named(max_queues, atl_max_queues, uint, 0444);

static unsigned int atl_rx_mod = 15, atl_tx_mod = 15;
module_param_named(rx_mod, atl_rx_mod, uint, 0444);
module_param_named(tx_mod, atl_tx_mod, uint, 0444);

static unsigned int atl_keep_link = 0;
module_param_named(keep_link, atl_keep_link, uint, 0644);

static void atl_link_up(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	if (hw->mcp.poll_link)
		mod_timer(&nic->link_timer, jiffies + HZ);

	hw->link_state.force_off = 0;
	hw->mcp.ops->set_link(hw, true);
}

static int atl_do_open(struct atl_nic *nic)
{
	int ret;

	ret = atl_start_rings(nic);
	if (ret)
		return ret;

	if (!atl_keep_link)
		atl_link_up(nic);

	return 0;
}

static int atl_open(struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);
	int ret;

	if (!test_bit(ATL_ST_CONFIGURED, &nic->state)) {
		/* A previous atl_reconfigure() had failed. Try once more. */
		ret = atl_setup_datapath(nic);
		if (ret)
			return ret;
	}

	ret = atl_alloc_rings(nic);
	if (ret)
		return ret;

	ret = netif_set_real_num_tx_queues(ndev, nic->nvecs);
	if (ret)
		goto free_rings;
	ret = netif_set_real_num_rx_queues(ndev, nic->nvecs);
	if (ret)
		goto free_rings;

	ret = atl_do_open(nic);
	if (ret)
		goto free_rings;

	netif_tx_start_all_queues(ndev);

	set_bit(ATL_ST_UP, &nic->state);
	return 0;

free_rings:
	atl_free_rings(nic);
	return ret;
}

static void atl_link_down(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	del_timer_sync(&nic->link_timer);
	hw->link_state.force_off = 1;
	hw->mcp.ops->set_link(hw, true);
	hw->link_state.link = 0;
	netif_carrier_off(nic->ndev);
}

static void atl_do_close(struct atl_nic *nic)
{
	if (!atl_keep_link)
		atl_link_down(nic);

	atl_stop_rings(nic);
}

static int atl_close(struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);

	/* atl_close() can be called a second time if
	 * atl_reconfigure() fails. Just return
	 */
	if (!test_and_clear_bit(ATL_ST_UP, &nic->state))
		return 0;

	netif_tx_stop_all_queues(ndev);

	atl_do_close(nic);
	atl_free_rings(nic);

	return 0;
}

#ifndef ATL_HAVE_MINMAX_MTU

static int atl_change_mtu(struct net_device *ndev, int mtu)
{
	struct atl_nic *nic = netdev_priv(ndev);

	if (mtu < 64 || mtu > nic->max_mtu)
		return -EINVAL;

	ndev->mtu = mtu;
	return 0;
}

#endif

static int atl_set_mac_address(struct net_device *ndev, void *priv)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_hw *hw = &nic->hw;
	struct sockaddr *addr = priv;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	ether_addr_copy(hw->mac_addr, addr->sa_data);
	ether_addr_copy(ndev->dev_addr, addr->sa_data);

	if (netif_running(ndev))
		atl_set_uc_flt(hw, 0, hw->mac_addr);

	return 0;
}

static const struct net_device_ops atl_ndev_ops = {
	.ndo_open = atl_open,
	.ndo_stop = atl_close,
	.ndo_start_xmit = atl_start_xmit,
	.ndo_vlan_rx_add_vid = atl_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid = atl_vlan_rx_kill_vid,
	.ndo_set_rx_mode = atl_set_rx_mode,
#ifndef ATL_HAVE_MINMAX_MTU
	.ndo_change_mtu = atl_change_mtu,
#endif
	.ndo_set_features = atl_set_features,
	.ndo_set_mac_address = atl_set_mac_address,
#ifdef ATL_COMPAT_CAST_NDO_GET_STATS64
	.ndo_get_stats64 = (void *)atl_get_stats64,
#else
	.ndo_get_stats64 = atl_get_stats64,
#endif
};

/* RTNL lock must be held */
int atl_reconfigure(struct atl_nic *nic)
{
	struct net_device *ndev = nic->ndev;
	int was_up = netif_running(ndev);
	int ret = 0;

	if (was_up)
		atl_close(ndev);

	atl_clear_datapath(nic);

	ret = atl_setup_datapath(nic);
	if (ret)
		goto err;

	/* Number of rings might have changed, re-init RSS
	 * redirection table.
	 */
	atl_init_rss_table(&nic->hw, nic->nvecs);

	if (was_up) {
		ret = atl_open(ndev);
		if (ret)
			goto err;
	}

	return 0;

err:
	if (was_up)
		dev_close(ndev);
	return ret;
}

static struct workqueue_struct *atl_wq;

void atl_schedule_work(struct atl_nic *nic)
{
	if (!test_and_set_bit(ATL_ST_WORK_SCHED, &nic->state))
		queue_work(atl_wq, &nic->work);
}

static void atl_work(struct work_struct *work)
{
	struct atl_nic *nic = container_of(work, struct atl_nic, work);

	atl_refresh_link(nic);
	clear_bit(ATL_ST_WORK_SCHED, &nic->state);
}

static void atl_link_timer(struct timer_list *timer)
{
	struct atl_nic *nic =
		container_of(timer, struct atl_nic, link_timer);

	atl_schedule_work(nic);
	mod_timer(&nic->link_timer, jiffies + HZ);
}

static const struct pci_device_id atl_pci_tbl[] = {
	{ PCI_VDEVICE(AQUANTIA, 0x0001), ATL_UNKNOWN},
	{ PCI_VDEVICE(AQUANTIA, 0xd107), ATL_AQC107},
	{ PCI_VDEVICE(AQUANTIA, 0x07b1), ATL_AQC107},
	{ PCI_VDEVICE(AQUANTIA, 0x87b1), ATL_AQC107},
	{ PCI_VDEVICE(AQUANTIA, 0xd108), ATL_AQC108},
	{ PCI_VDEVICE(AQUANTIA, 0x08b1), ATL_AQC108},
	{ PCI_VDEVICE(AQUANTIA, 0x88b1), ATL_AQC108},
	{ PCI_VDEVICE(AQUANTIA, 0xd109), ATL_AQC109},
	{ PCI_VDEVICE(AQUANTIA, 0x09b1), ATL_AQC109},
	{ PCI_VDEVICE(AQUANTIA, 0x89b1), ATL_AQC109},
	{ PCI_VDEVICE(AQUANTIA, 0xd100), ATL_AQC100},
	{ PCI_VDEVICE(AQUANTIA, 0x00b1), ATL_AQC107},
	{ PCI_VDEVICE(AQUANTIA, 0x80b1), ATL_AQC107},
	{ PCI_VDEVICE(AQUANTIA, 0x11b1), ATL_AQC108},
	{ PCI_VDEVICE(AQUANTIA, 0x91b1), ATL_AQC108},
	{ PCI_VDEVICE(AQUANTIA, 0x51b1), ATL_AQC108},
	{ PCI_VDEVICE(AQUANTIA, 0x12b1), ATL_AQC109},
	{ PCI_VDEVICE(AQUANTIA, 0x92b1), ATL_AQC109},
	{ PCI_VDEVICE(AQUANTIA, 0x52b1), ATL_AQC109},
	{}
};

static uint8_t atl_def_rss_key[ATL_RSS_KEY_SIZE] = {
	0x1e, 0xad, 0x71, 0x87, 0x65, 0xfc, 0x26, 0x7d,
	0x0d, 0x45, 0x67, 0x74, 0xcd, 0x06, 0x1a, 0x18,
	0xb6, 0xc1, 0xf0, 0xc7, 0xbb, 0x18, 0xbe, 0xf8,
	0x19, 0x13, 0x4b, 0xa9, 0xd0, 0x3e, 0xfe, 0x70,
	0x25, 0x03, 0xab, 0x50, 0x6a, 0x8b, 0x82, 0x0c
};

static void atl_setup_rss(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	memcpy(hw->rss_key, atl_def_rss_key, sizeof(hw->rss_key));

	atl_init_rss_table(hw, nic->nvecs);
}

static int atl_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int ret, pci_64 = 0;
	struct net_device *ndev;
	struct atl_nic *nic = NULL;
	struct atl_hw *hw;
	int disable_needed;

	if (atl_max_queues < 1 || atl_max_queues > ATL_MAX_QUEUES) {
		dev_err(&pdev->dev, "Bad atl_max_queues value %d, must be between 1 and %d inclusive\n",
			 atl_max_queues, ATL_MAX_QUEUES);
		return -EINVAL;
	}

	ret = pci_enable_device_mem(pdev);
	if (ret)
		return ret;

	if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64)))
		pci_64 = 1;
	else {
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (ret) {
			dev_err(&pdev->dev, "Set DMA mask failed: %d\n", ret);
			goto err_dma;
		}
	}

	ret = pci_request_mem_regions(pdev, atl_driver_name);
	if (ret) {
		dev_err(&pdev->dev, "Request PCI regions failed: %d\n", ret);
		goto err_pci_reg;
	}

	pci_set_master(pdev);

	ndev = alloc_etherdev_mq(sizeof(struct atl_nic), atl_max_queues);
	if (!ndev) {
		ret = -ENOMEM;
		goto err_alloc_ndev;
	}

	SET_NETDEV_DEV(ndev, &pdev->dev);
	nic = netdev_priv(ndev);
	nic->ndev = ndev;
	nic->hw.pdev = pdev;
	spin_lock_init(&nic->stats_lock);
	INIT_WORK(&nic->work, atl_work);
	mutex_init(&nic->hw.mcp.lock);
	__set_bit(ATL_ST_ENABLED, &nic->state);

	hw = &nic->hw;
	hw->regs = ioremap(pci_resource_start(pdev, 0),
				pci_resource_len(pdev, 0));
	if (!hw->regs) {
		ret = -EIO;
		goto err_ioremap;
	}

	ret = atl_hwinit(nic, id->driver_data);
	if (ret)
		goto err_hwinit;

	eth_platform_get_mac_address(&hw->pdev->dev, hw->mac_addr);
	if (!is_valid_ether_addr(hw->mac_addr)) {
		atl_dev_err("invalid MAC address: %*phC\n", ETH_ALEN,
			    hw->mac_addr);
		/* XXX Workaround for bad MAC addr in efuse. Maybe
		 * switch to some predefined one later.
		 */
		eth_random_addr(hw->mac_addr);
		/* ret = -EIO; */
		/* goto err_hwinit; */
	}

	ether_addr_copy(ndev->dev_addr, hw->mac_addr);
	atl_dev_dbg("got MAC address: %pM\n", hw->mac_addr);

	nic->requested_nvecs = atl_max_queues;
	nic->requested_tx_size = ATL_RING_SIZE;
	nic->requested_rx_size = ATL_RING_SIZE;
	nic->rx_intr_delay = atl_rx_mod;
	nic->tx_intr_delay = atl_tx_mod;

	ret = atl_setup_datapath(nic);
	if (ret)
		goto err_datapath;

	atl_setup_rss(nic);

	ndev->features |= NETIF_F_SG | NETIF_F_TSO | NETIF_F_TSO6 |
		NETIF_F_RXCSUM | NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
		NETIF_F_RXHASH | NETIF_F_LRO;

	ndev->vlan_features |= ndev->features;
	ndev->features |= NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HW_VLAN_CTAG_TX |
		NETIF_F_HW_VLAN_CTAG_FILTER;

	ndev->hw_features |= ndev->features | NETIF_F_RXALL;

	if (pci_64)
		ndev->features |= NETIF_F_HIGHDMA;

	ndev->features |= NETIF_F_NTUPLE;

	ndev->priv_flags |= IFF_UNICAST_FLT;

	timer_setup(&nic->link_timer, &atl_link_timer, 0);

	hw->mcp.ops->set_default_link(hw);
	hw->link_state.force_off = 1;
	hw->intr_mask = BIT(ATL_NUM_NON_RING_IRQS) - 1;
	ndev->netdev_ops = &atl_ndev_ops;
	ndev->mtu = 1500;
#ifdef ATL_HAVE_MINMAX_MTU
	ndev->max_mtu = nic->max_mtu;
#endif
	ndev->ethtool_ops = &atl_ethtool_ops;
	ret = register_netdev(ndev);
	if (ret)
		goto err_register;

	pci_set_drvdata(pdev, nic);
	netif_carrier_off(ndev);

	ret = atl_alloc_link_intr(nic);
	if (ret)
		goto err_link_intr;

	ret = atl_hwmon_init(nic);
	if (ret)
		goto err_hwmon_init;

	atl_start_hw_global(nic);
	if (atl_keep_link)
		atl_link_up(nic);

	return 0;

err_hwmon_init:
	atl_free_link_intr(nic);
err_link_intr:
	unregister_netdev(nic->ndev);
err_register:
	atl_clear_datapath(nic);
err_datapath:
err_hwinit:
	iounmap(hw->regs);
err_ioremap:
	disable_needed = test_and_clear_bit(ATL_ST_ENABLED, &nic->state);
	free_netdev(ndev);
err_alloc_ndev:
	pci_release_regions(pdev);
err_pci_reg:
err_dma:
	if (!nic || disable_needed)
		pci_disable_device(pdev);
	return ret;
}

static void atl_remove(struct pci_dev *pdev)
{
	int disable_needed;
	struct atl_nic *nic = pci_get_drvdata(pdev);

	if (!nic)
		return;

	netif_carrier_off(nic->ndev);
	atl_intr_disable_all(&nic->hw);
	/* atl_hw_reset(&nic->hw); */
	atl_free_link_intr(nic);
	unregister_netdev(nic->ndev);
	atl_fwd_release_rings(nic);
	atl_clear_datapath(nic);
	iounmap(nic->hw.regs);
	disable_needed = test_and_clear_bit(ATL_ST_ENABLED, &nic->state);
	cancel_work_sync(&nic->work);
	free_netdev(nic->ndev);
	pci_release_regions(pdev);
	if (disable_needed)
		pci_disable_device(pdev);
}

static int atl_suspend_common(struct device *dev, bool deep)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct atl_nic *nic = pci_get_drvdata(pdev);
	struct atl_hw *hw = &nic->hw;
	int ret;

	rtnl_lock();
	netif_device_detach(nic->ndev);

	if (netif_running(nic->ndev))
		atl_do_close(nic);

	if (deep && atl_keep_link)
		atl_link_down(nic);

	if (deep && nic->flags & ATL_FL_WOL) {
		ret = hw->mcp.ops->enable_wol(hw);
		if (ret)
			atl_dev_err("Enable WoL failed: %d\n", -ret);
	}

	pci_disable_device(pdev);
	__clear_bit(ATL_ST_ENABLED, &nic->state);

	rtnl_unlock();

	return 0;
}

static int atl_suspend_poweroff(struct device *dev)
{
	return atl_suspend_common(dev, true);
}

static int atl_freeze(struct device *dev)
{
	return atl_suspend_common(dev, false);
}

static int atl_resume_common(struct device *dev, bool deep)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct atl_nic *nic = pci_get_drvdata(pdev);
	int ret;

	rtnl_lock();

	ret = pci_enable_device_mem(pdev);
	if (ret)
		goto exit;

	pci_set_master(pdev);
	__set_bit(ATL_ST_ENABLED, &nic->state);

	if (deep) {
		ret = atl_hw_reset(&nic->hw);
		if (ret)
			goto exit;

		atl_start_hw_global(nic);
	}

	if (netif_running(nic->ndev))
		ret = atl_do_open(nic);

	if (deep && atl_keep_link)
		atl_link_up(nic);

	if (ret)
		goto exit;

	netif_device_attach(nic->ndev);

exit:
	rtnl_unlock();

	return ret;
}

static int atl_resume_restore(struct device *dev)
{
	return atl_resume_common(dev, true);
}

static int atl_thaw(struct device *dev)
{
	return atl_resume_common(dev, false);
}

static void atl_shutdown(struct pci_dev *pdev)
{
	atl_suspend_common(&pdev->dev, true);
}

const struct dev_pm_ops atl_pm_ops = {
	.suspend = atl_suspend_poweroff,
	.poweroff = atl_suspend_poweroff,
	.freeze = atl_freeze,
	.resume = atl_resume_restore,
	.restore = atl_resume_restore,
	.thaw = atl_thaw,
};

static struct pci_driver atl_pci_ops = {
	.name = atl_driver_name,
	.id_table = atl_pci_tbl,
	.probe = atl_probe,
	.remove = atl_remove,
	.shutdown = atl_shutdown,
#ifdef CONFIG_PM
	.driver.pm = &atl_pm_ops,
#endif
};

static int __init atl_module_init(void)
{
	int ret;

	atl_wq = create_singlethread_workqueue(atl_driver_name);
	if (!atl_wq) {
		pr_err("%s: Couldn't create workqueue\n", atl_driver_name);
		return -ENOMEM;
	}

	ret = pci_register_driver(&atl_pci_ops);
	if (ret) {
		destroy_workqueue(atl_wq);
		return ret;
	}

	return 0;
}
module_init(atl_module_init);

static void __exit atl_module_exit(void)
{
	pci_unregister_driver(&atl_pci_ops);

	if (atl_wq) {
		destroy_workqueue(atl_wq);
		atl_wq = NULL;
	}
}
module_exit(atl_module_exit);

MODULE_DEVICE_TABLE(pci, atl_pci_tbl);
MODULE_LICENSE("GPL v2");
MODULE_VERSION(ATL_VERSION);
MODULE_AUTHOR("Aquantia Corp.");
