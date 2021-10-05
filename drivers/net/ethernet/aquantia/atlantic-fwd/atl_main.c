// SPDX-License-Identifier: GPL-2.0-only
/* Atlantic Network Driver
 *
 * Copyright (C) 2017 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "atl_common.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/pm_runtime.h>
#include "atl_macsec.h"
#include "atl_ptp.h"

const char atl_driver_name[] = "atlantic-fwd";

unsigned int atl_max_queues = ATL_MAX_QUEUES;
module_param_named(max_queues, atl_max_queues, uint, 0444);
unsigned int atl_max_queues_non_msi = 1;
module_param_named(max_queues_non_msi, atl_max_queues_non_msi, uint, 0444);

static unsigned int atl_rx_mod = 15, atl_tx_mod = 15;
module_param_named(rx_mod, atl_rx_mod, uint, 0444);
module_param_named(tx_mod, atl_tx_mod, uint, 0444);

static unsigned int atl_keep_link = 0;
module_param_named(keep_link, atl_keep_link, uint, 0644);

static unsigned int atl_sleep_delay = 10000;
module_param_named(sleep_delay, atl_sleep_delay, uint, 0644);

static unsigned int atl_rx_ring_size = ATL_RING_SIZE;
static unsigned int atl_tx_ring_size = ATL_RING_SIZE;
module_param_named(rx_ring_size, atl_rx_ring_size, uint, 0444);
module_param_named(tx_ring_size, atl_tx_ring_size, uint, 0444);

static void atl_start_link(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	hw->link_state.force_off = 0;
	hw->mcp.ops->set_link(hw, true);
	set_bit(ATL_ST_UPDATE_LINK, &hw->state);
	atl_schedule_work(nic);
}

static void atl_stop_link(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;
	bool was_up = netif_carrier_ok(nic->ndev);

	hw->link_state.force_off = 1;
	hw->mcp.ops->set_link(hw, true);
	hw->link_state.link = 0;
	netif_carrier_off(nic->ndev);
	if (was_up)
		pm_runtime_put(&nic->hw.pdev->dev);
}

static int atl_start(struct atl_nic *nic)
{
	int ret = 0;

	atl_start_hw_global(nic);
	atl_set_rx_mode(nic->ndev);

	if (atl_keep_link || netif_running(nic->ndev))
		atl_start_link(nic);

	if (netif_running(nic->ndev))
		ret = atl_start_rings(nic);

	if (ret && !atl_keep_link)
		atl_stop_link(nic);

	/* if (ret) */
	/* 	goto out; */
	/* ret = atl_fwd_resume_rings(nic); */

/* out: */
	return ret;
}

static void atl_stop(struct atl_nic *nic, bool drop_link)
{
	atl_stop_rings(nic);

	/* if (drop_link) { */
	/*	atl_stop_fwd_rings(nic); */
	/* } */

	if (drop_link)
		atl_stop_link(nic);
}

static int atl_open(struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);
	int ret;

	pm_runtime_get_sync(&nic->hw.pdev->dev);

	if (!test_bit(ATL_ST_CONFIGURED, &nic->hw.state)) {
		/* A previous atl_reconfigure() had failed. Try once more. */
		ret = atl_setup_datapath(nic);
		if (ret)
			goto out;
	}

	ret = atl_alloc_rings(nic);
	if (ret)
		goto out;

	ret = netif_set_real_num_tx_queues(ndev, nic->nvecs);
	if (ret)
		goto free_rings;
	ret = netif_set_real_num_rx_queues(ndev, nic->nvecs);
	if (ret)
		goto free_rings;

	ret = atl_start(nic);
	if (ret)
		goto free_rings;

	set_bit(ATL_ST_UP, &nic->hw.state);

	pm_runtime_put_sync(&nic->hw.pdev->dev);

#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	atlfwd_nl_on_open(nic->ndev);
#endif

	return 0;

free_rings:
	atl_free_rings(nic);
out:
	pm_runtime_put_noidle(&nic->hw.pdev->dev);
	return ret;
}

static int atl_close(struct atl_nic *nic, bool drop_link)
{
	/* atl_close() can be called a second time if
	 * atl_reconfigure() fails. Just return
	 */
	if (!test_and_clear_bit(ATL_ST_UP, &nic->hw.state))
		return 0;

	pm_runtime_get_sync(&nic->hw.pdev->dev);

	atl_stop(nic, drop_link);
	atl_free_rings(nic);
	if (drop_link)
		atl_reconfigure(nic);

	pm_runtime_put_sync(&nic->hw.pdev->dev);

	return 0;
}

static int atl_ndo_close(struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);

	return atl_close(nic, !atl_keep_link);
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

static int atl_hwtstamp_config(struct atl_nic *nic, struct hwtstamp_config *config)
{
	if (config->flags)
		return -EINVAL;

	switch (config->tx_type) {
	case HWTSTAMP_TX_OFF:
	case HWTSTAMP_TX_ON:
		break;
	default:
		return -ERANGE;
	}

	switch (config->rx_filter) {
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
		config->rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		break;
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_NONE:
		break;
	default:
		return -ERANGE;
	}

	return atl_ptp_hwtstamp_config_set(nic, config);
}

static int atl_hwtstamp_set(struct atl_nic *nic, struct ifreq *ifr)
{
	struct hwtstamp_config config;
	int err;

	if (!nic->ptp)
		return -EOPNOTSUPP;

	if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
		return -EFAULT;

	err = atl_hwtstamp_config(nic, &config);
	if (err)
		return err;

	return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ?
	       -EFAULT : 0;
}

static int atl_hwtstamp_get(struct atl_nic *nic, struct ifreq *ifr)
{
	struct hwtstamp_config config;

	if (!nic->ptp)
		return -EOPNOTSUPP;

	atl_ptp_hwtstamp_config_get(nic, &config);
	return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ?
	       -EFAULT : 0;
}

static int atl_ndo_ioctl(struct net_device *ndev, struct ifreq *ifr, int cmd)
{
	struct atl_nic *nic = netdev_priv(ndev);

	pm_runtime_get_sync(&nic->hw.pdev->dev);

	switch (cmd) {
	case SIOCSHWTSTAMP:
		return atl_hwtstamp_set(nic, ifr);

	case SIOCGHWTSTAMP:
		return atl_hwtstamp_get(nic, ifr);
	}

	pm_runtime_put(&nic->hw.pdev->dev);

	return -EOPNOTSUPP;
}

static int atl_set_mac_address(struct net_device *ndev, void *priv)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_hw *hw = &nic->hw;
	struct sockaddr *addr = priv;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	ether_addr_copy(hw->mac_addr, addr->sa_data);
	ether_addr_copy(ndev->dev_addr, addr->sa_data);

	if (netif_running(ndev) && pm_runtime_active(&nic->hw.pdev->dev))
		atl_set_uc_flt(hw, nic->rxf_mac.base_index, hw->mac_addr);

	return 0;
}

static const struct net_device_ops atl_ndev_ops = {
	.ndo_open = atl_open,
	.ndo_stop = atl_ndo_close,
	.ndo_start_xmit = atl_start_xmit,
#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	.ndo_select_queue = atlfwd_nl_select_queue,
#endif
	.ndo_vlan_rx_add_vid = atl_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid = atl_vlan_rx_kill_vid,
	.ndo_set_rx_mode = atl_set_rx_mode,
#ifndef ATL_HAVE_MINMAX_MTU
	.ndo_change_mtu = atl_change_mtu,
#endif
	.ndo_set_features = atl_set_features,
	.ndo_do_ioctl = atl_ndo_ioctl,
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

	nic->hw.mcp.ops->dump_cfg(&nic->hw);

	if (was_up)
		atl_close(nic, false);

	atl_clear_datapath(nic);

	atl_fwd_suspend_rings(nic);

	ret = atl_hw_reset(&nic->hw);
	if (ret) {
		atl_nic_err("HW reset failed, re-trying\n");
		goto err;
	}

	ret = atl_setup_datapath(nic);
	if (ret)
		goto err;

	/* Re-enable link interrupts disabled in atl_clear_datapath() */
	atl_intr_enable_non_ring(nic);

	/* Number of rings might have changed, re-init RSS
	 * redirection table.
	 */
	atl_init_rss_table(&nic->hw, nic->nvecs);

	if (was_up) {
		ret = atl_open(ndev);
		if (ret)
			goto err;
	}

	ret = atl_fwd_resume_rings(nic);
	if (ret)
		goto err;

	return 0;

err:
	if (was_up)
		dev_close(ndev);
	return ret;
}

static struct workqueue_struct *atl_wq;

void atl_schedule_work(struct atl_nic *nic)
{
	if (!test_and_set_bit(ATL_ST_WORK_SCHED, &nic->hw.state))
		queue_work(atl_wq, &nic->work);
}

int atl_do_reset(struct atl_nic *nic)
{
	bool was_up = netif_running(nic->ndev);
	struct atl_hw *hw = &nic->hw;
	int ret;

	set_bit(ATL_ST_RESETTING, &hw->state);

	rtnl_lock();

	hw->mcp.ops->dump_cfg(hw);

	atl_stop(nic, true);

	atl_fwd_suspend_rings(nic);

	ret = atl_hw_reset(hw);
	if (ret) {
		atl_nic_err("HW reset failed, re-trying\n");
		if (!test_and_set_bit(ATL_ST_DETACHED, &hw->state))
			netif_device_detach(nic->ndev);
		goto out;
	}
	clear_bit(ATL_ST_RESETTING, &hw->state);

	if (was_up) {
		ret = atl_start(nic);
		if (ret)
			goto out;
	}

	ret = atl_fwd_resume_rings(nic);
	if (ret)
		goto out;

	if (test_and_clear_bit(ATL_ST_DETACHED, &hw->state))
		netif_device_attach(nic->ndev);

out:
	rtnl_unlock();
	return ret;
}

static int atl_check_reset(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;
	bool reset;

	if (!test_bit(ATL_ST_ENABLED, &hw->state))
		/* We're suspending, postpone resets till resume */
		return 0;

	reset = test_and_clear_bit(ATL_ST_RESET_NEEDED, &hw->state);

	if (!reset)
		return 0;

	return atl_do_reset(nic);
}

int atl_fw_configure(struct atl_hw *hw)
{
	struct atl_nic *nic = container_of(hw, struct atl_nic, hw);
	int ret;

	ret = hw->mcp.ops->set_mediadetect(hw,
			!!(nic->priv_flags & ATL_PF_BIT(MEDIA_DETECT)));
	if (ret && ret != -EOPNOTSUPP)
		return ret;
	ret = hw->mcp.ops->set_downshift(hw,
			!!(nic->priv_flags & ATL_PF_BIT(DOWNSHIFT)));
	if (ret && ret != -EOPNOTSUPP)
		return ret;
	ret = hw->mcp.ops->set_pad_stripping(hw,
			!!(nic->priv_flags & ATL_PF_BIT(STRIP_PAD)));
	if (ret && ret != -EOPNOTSUPP)
		return ret;
	ret = hw->mcp.ops->update_thermal(hw);
	if (ret == -EOPNOTSUPP)
		ret = 0;

	return ret;
}

static void atl_work(struct work_struct *work)
{
	struct atl_nic *nic = container_of(work, struct atl_nic, work);
	struct atl_hw *hw = &nic->hw;
	int ret;

	atl_ptp_work(nic);

	clear_bit(ATL_ST_WORK_SCHED, &hw->state);

	atl_fw_watchdog(hw);
	ret = atl_check_reset(nic);
	if (ret)
		goto out;
	atl_refresh_link(nic);
#if IS_ENABLED(CONFIG_MACSEC) && defined(NETIF_F_HW_MACSEC)
	atl_macsec_work(nic);
#endif
out:
	if (test_bit(ATL_ST_ENABLED, &hw->state))
	    mod_timer(&nic->work_timer, jiffies + HZ);
}

static void atl_work_timer(struct timer_list *timer)
{
	struct atl_nic *nic =
		container_of(timer, struct atl_nic, work_timer);

	atl_schedule_work(nic);
}

static const struct pci_device_id atl_pci_tbl[] = {
	{ PCI_VDEVICE(AQUANTIA, 0x0001), ATL_UNKNOWN},
	{ PCI_VDEVICE(AQUANTIA, 0xd107), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x07b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x87b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0xd108), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x08b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x88b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0xd109), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x09b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x89b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0xd100), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x00b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x80b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x11b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x91b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x12b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x92b1), ATL_ATLANTIC},
	{ PCI_VDEVICE(AQUANTIA, 0x00c0), ATL_ANTIGUA},
	{ PCI_VDEVICE(AQUANTIA, 0x04c0), ATL_ANTIGUA},
	{ PCI_VDEVICE(AQUANTIA, 0x12c0), ATL_ANTIGUA},
	{ PCI_VDEVICE(AQUANTIA, 0x14c0), ATL_ANTIGUA},
	{ PCI_VDEVICE(AQUANTIA, 0x93c0), ATL_ANTIGUA},
	{ PCI_VDEVICE(AQUANTIA, 0x94c0), ATL_ANTIGUA},
	{ PCI_VDEVICE(AQUANTIA, 0x34c0), ATL_ANTIGUA},
	{ PCI_VDEVICE(AQUANTIA, 0x11c0), ATL_ANTIGUA},
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
	/* Number of queues:
	 * Extra TX queue is used for redirection to FWD ring.
	 */
#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	const unsigned int txqs = atl_max_queues + 1;
#else
	const unsigned int txqs = atl_max_queues;
#endif
	const unsigned int rxqs = atl_max_queues;
	int ret, pci_64 = 0;
	struct net_device *ndev;
	struct atl_nic *nic = NULL;
	struct atl_hw *hw;
	int disable_needed = 0;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_forbid(&pdev->dev);

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

	ndev = alloc_etherdev_mqs(sizeof(struct atl_nic), txqs, rxqs);
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

#if IS_ENABLED(CONFIG_ATLFWD_FWD)
	BLOCKING_INIT_NOTIFIER_HEAD(&nic->fwd.nh_clients);
#endif

	hw = &nic->hw;
	__set_bit(ATL_ST_ENABLED, &hw->state);
	hw->regs = ioremap(pci_resource_start(pdev, 0),
				pci_resource_len(pdev, 0));
	if (!hw->regs) {
		ret = -EIO;
		goto err_ioremap;
	}

	nic->priv_flags = ATL_PF_BIT(DOWNSHIFT);
	ret = atl_hwinit(hw, id->driver_data);
	if (ret)
		goto err_hwinit;

	hw->mcp.ops->set_default_link(hw);
	hw->link_state.force_off = 1;

	pci_set_master(pdev);

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

	ret = atl_ptp_init(nic);
	if (ret)
		goto err_ptp_init;

	nic->requested_nvecs = atl_max_queues;
	nic->requested_tx_size = (atl_tx_ring_size & ~7);
	nic->requested_rx_size = (atl_rx_ring_size & ~7);
	nic->rx_intr_delay = atl_rx_mod;
	nic->tx_intr_delay = atl_tx_mod;

	ret = atl_ptp_ring_alloc(nic);
	if (ret)
		goto err_ptp_alloc;

	ret = atl_setup_datapath(nic);
	if (ret)
		goto err_datapath;

	atl_setup_rss(nic);

	ndev->features |= NETIF_F_SG | NETIF_F_TSO | NETIF_F_TSO6 |
		NETIF_F_RXCSUM | NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
		NETIF_F_RXHASH;

	ndev->vlan_features |= ndev->features;
	ndev->features |= NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HW_VLAN_CTAG_TX |
		NETIF_F_HW_VLAN_CTAG_FILTER;

	ndev->hw_features |= ndev->features | NETIF_F_RXALL | NETIF_F_LRO;

	if (pci_64)
		ndev->features |= NETIF_F_HIGHDMA;

#if IS_ENABLED(CONFIG_MACSEC) && defined(NETIF_F_HW_MACSEC)
	if (hw->mcp.caps_low & atl_fw2_macsec)
		ndev->features |= NETIF_F_HW_MACSEC;
#endif

	ndev->features |= NETIF_F_NTUPLE;

	ndev->priv_flags |= IFF_UNICAST_FLT;

	timer_setup(&nic->work_timer, &atl_work_timer, 0);

	hw->non_ring_intr_mask = BIT(ATL_NUM_NON_RING_IRQS) - 1;
	ndev->netdev_ops = &atl_ndev_ops;
#if IS_ENABLED(CONFIG_MACSEC) && defined(NETIF_F_HW_MACSEC)
	if (hw->mcp.caps_low & atl_fw2_macsec)
		ndev->macsec_ops = &atl_macsec_ops,
#endif
	ndev->mtu = 1500;
#ifdef ATL_HAVE_MINMAX_MTU
	ndev->max_mtu = nic->max_mtu;
#endif
	ndev->ethtool_ops = &atl_ethtool_ops;
	ret = register_netdev(ndev);
	if (ret)
		goto err_register;

	ret = atl_ptp_register(nic);
	if (ret)
		goto err_ptp_register;

	pci_set_drvdata(pdev, nic);
	netif_carrier_off(ndev);

	/* Safe to ignore ret value here. atl_start() only returns
	 * errors when rings are started. We could race with someone
	 * doing ifup on newly created netdev, but either they will
	 * succeed with grabbing RTNL first and handle ring-related
	 * errors there, or we will be first and just bring the
	 * global HW up. */
	rtnl_lock();
	atl_start(nic);
	rtnl_unlock();

	ret = atl_hwmon_init(nic);
	if (ret)
		goto err_hwmon_init;

	if (hw->mcp.caps_low & atl_fw2_wake_on_link_force)
		pm_runtime_put_noidle(&pdev->dev);

	atl_intr_enable_non_ring(nic);
	mod_timer(&nic->work_timer, jiffies + HZ);

#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	atlfwd_nl_on_probe(nic->ndev);
#endif

	return 0;

err_hwmon_init:
	atl_stop(nic, true);
err_ptp_register:
	unregister_netdev(nic->ndev);
err_register:
	atl_clear_datapath(nic);
err_datapath:
	atl_ptp_ring_free(nic);
err_ptp_alloc:
	atl_ptp_free(nic);
err_ptp_init:
err_hwinit:
	iounmap(hw->regs);
err_ioremap:
	disable_needed = test_and_clear_bit(ATL_ST_ENABLED, &hw->state);
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
	struct atl_nic *nic = pci_get_drvdata(pdev);
	int disable_needed;
	bool wol_force;

	if (!nic)
		return;

#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	atlfwd_nl_on_remove(nic->ndev);
#endif

	atl_stop(nic, true);
	disable_needed = test_and_clear_bit(ATL_ST_ENABLED, &nic->hw.state);
	wol_force = !!(nic->hw.mcp.caps_low & atl_fw2_wake_on_link_force);
	del_timer_sync(&nic->work_timer);
	cancel_work_sync(&nic->work);
	atl_intr_disable_all(&nic->hw);
	atl_ptp_unregister(nic);
	unregister_netdev(nic->ndev);

#if IS_ENABLED(CONFIG_ATLFWD_FWD)
	atl_fwd_release_rings(nic);
#endif

	atl_clear_datapath(nic);

	atl_ptp_ring_free(nic);
	atl_ptp_free(nic);

	iounmap(nic->hw.regs);
	free_netdev(nic->ndev);
	pci_release_regions(pdev);

	if (wol_force)
		pm_runtime_get_sync(&pdev->dev);

	if (disable_needed)
		pci_disable_device(pdev);
}

static int atl_suspend_common(struct device *dev, unsigned int wol_mode)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct atl_nic *nic = pci_get_drvdata(pdev);
	struct atl_hw *hw = &nic->hw;
	bool rtnlocked;
	int ret;

	rtnlocked = rtnl_trylock();
	hw->mcp.ops->dump_cfg(hw);

	atl_stop(nic, true);

	atl_intr_disable_non_ring(nic);

	atl_clear_rdm_cache(nic);
	atl_clear_tdm_cache(nic);

	if (wol_mode) {
		ret = hw->mcp.ops->enable_wol(hw, wol_mode);
		if (ret)
			atl_dev_err("Enable WoL failed: %d\n", -ret);
	} else {
		hw->mcp.ops->deinit(hw);
	}

	clear_bit(ATL_ST_ENABLED, &hw->state);
	cancel_work_sync(&nic->work);
	clear_bit(ATL_ST_WORK_SCHED, &hw->state);

	pci_disable_device(pdev);
	pci_save_state(pdev);
	pci_prepare_to_sleep(pdev);

	if (rtnlocked)
		rtnl_unlock();

	return 0;
}

static int atl_suspend_poweroff(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct atl_nic *nic = pci_get_drvdata(pdev);
	struct atl_hw *hw = &nic->hw;

	if (!test_and_set_bit(ATL_ST_DETACHED, &hw->state))
		netif_device_detach(nic->ndev);

	return atl_suspend_common(dev, hw->wol_mode);
}

static int atl_freeze(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct atl_nic *nic = pci_get_drvdata(pdev);
	struct atl_hw *hw = &nic->hw;

	if (!test_and_set_bit(ATL_ST_DETACHED, &hw->state))
		netif_device_detach(nic->ndev);

	return atl_suspend_common(dev, 0);
}

static int atl_resume_common(struct device *dev, bool deep)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct atl_nic *nic = pci_get_drvdata(pdev);
	bool rtnlocked;
	int ret;

	rtnlocked = rtnl_trylock();

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);

	ret = pci_enable_device_mem(pdev);
	if (ret)
		goto exit;

	if (deep) {
		atl_fwd_suspend_rings(nic);
		ret = atl_hw_reset(&nic->hw);
		if (ret)
			goto exit;
	}

	set_bit(ATL_ST_ENABLED, &nic->hw.state);
	pci_set_master(pdev);

	if (test_bit(ATL_ST_UP, &nic->hw.state)) {
		ret = atl_start(nic);
		if (ret)
			goto exit;
	}

	if (deep) {
		ret = atl_fwd_resume_rings(nic);
		if (ret)
			goto exit;
	}

exit:
	if (rtnlocked)
		rtnl_unlock();

	return ret;
}

static int atl_resume_restore(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct atl_nic *nic = pci_get_drvdata(pdev);

	if (test_and_clear_bit(ATL_ST_DETACHED, &nic->hw.state))
		netif_device_attach(nic->ndev);

	return atl_resume_common(dev, true);
}

static int atl_thaw(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct atl_nic *nic = pci_get_drvdata(pdev);

	if (test_and_clear_bit(ATL_ST_DETACHED, &nic->hw.state))
		netif_device_attach(nic->ndev);

	return atl_resume_common(dev, false);
}

static void atl_shutdown(struct pci_dev *pdev)
{
	struct atl_nic *nic = pci_get_drvdata(pdev);
	atl_suspend_common(&pdev->dev, nic->hw.wol_mode);
}

static int atl_pm_runtime_resume(struct device *dev)
{
	return atl_resume_common(dev, true);
}

static int atl_pm_runtime_suspend(struct device *dev)
{
	return atl_suspend_common(dev, atl_fw_wake_on_link_rtpm);
}

static int atl_pm_runtime_idle(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct atl_nic *nic = pci_get_drvdata(pdev);

	/* pm_runtime_idle may be called during probe */
	if (!nic || nic->hw.pdev != pdev)
		return -EBUSY;

	if (!netif_carrier_ok(nic->ndev)) {
		pm_schedule_suspend(dev, atl_sleep_delay);
	}

	return -EBUSY;
}

const struct dev_pm_ops atl_pm_ops = {
	.suspend = atl_suspend_poweroff,
	.poweroff = atl_suspend_poweroff,
	.freeze = atl_freeze,
	.resume = atl_resume_restore,
	.restore = atl_resume_restore,
	.thaw = atl_thaw,
	SET_RUNTIME_PM_OPS(atl_pm_runtime_suspend, atl_pm_runtime_resume,
			   atl_pm_runtime_idle)
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

struct atl_thermal atl_def_thermal;

static bool atl_def_thermal_monitor = true, atl_def_thermal_throttle = false,
	atl_def_thermal_ignore_lims = false;
module_param_named(thermal_monitor, atl_def_thermal_monitor, bool, 0444);
module_param_named(thermal_throttle, atl_def_thermal_throttle, bool, 0444);
module_param_named(thermal_ignore_limits, atl_def_thermal_ignore_lims, bool, 0444);

static uint8_t atl_def_thermal_crit = 108, atl_def_thermal_high = 100,
	atl_def_thermal_low = 80;
module_param_named(thermal_crit, atl_def_thermal_crit, byte, 0444);
module_param_named(thermal_high, atl_def_thermal_high, byte, 0444);
module_param_named(thermal_low, atl_def_thermal_low, byte, 0444);

static int __init atl_module_init(void)
{
	struct atl_hw *hw = NULL;
	int ret;

	if ((atl_tx_ring_size < 8) || (atl_tx_ring_size > ATL_MAX_RING_SIZE)) {
		atl_dev_init_err(
			"Bad atl_tx_ring_size value %d, must be between 8 and %d inclusive\n",
			atl_tx_ring_size, ATL_MAX_RING_SIZE);
		return -EINVAL;
	}
	if ((atl_rx_ring_size < 8) || (atl_rx_ring_size > ATL_MAX_RING_SIZE)) {
		atl_dev_init_err(
			"Bad atl_rx_ring_size value %d, must be between 8 and %d inclusive\n",
			atl_rx_ring_size, ATL_MAX_RING_SIZE);
		return -EINVAL;
	}

	atl_def_thermal.flags =
		atl_def_thermal_monitor << atl_thermal_monitor_shift |
		atl_def_thermal_throttle << atl_thermal_throttle_shift |
		atl_def_thermal_ignore_lims << atl_thermal_ignore_lims_shift;
	atl_def_thermal.crit = atl_def_thermal_crit;
	atl_def_thermal.high = atl_def_thermal_high;
	atl_def_thermal.low = atl_def_thermal_low;

	ret = atl_verify_thermal_limits(hw, &atl_def_thermal);
	if (ret)
		return ret;

	if (atl_max_queues < 1 || atl_max_queues > ATL_MAX_QUEUES) {
		atl_dev_init_err(
			"Bad atl_max_queues value %d, must be between 1 and %d inclusive\n",
			atl_max_queues, ATL_MAX_QUEUES);
		return -EINVAL;
	}

	if (atl_max_queues_non_msi < 1 ||
	    atl_max_queues_non_msi > atl_max_queues) {
		atl_dev_init_err(
			"Bad atl_max_queues_non_msi value %d, must be between 1 and %d inclusive\n",
			atl_max_queues_non_msi, atl_max_queues);
		return -EINVAL;
	}

	atl_wq = create_singlethread_workqueue(atl_driver_name);
	if (!atl_wq) {
		pr_err("%s: Couldn't create workqueue\n", atl_driver_name);
		return -ENOMEM;
	}

	ret = pci_register_driver(&atl_pci_ops);
	if (ret)
		goto err_pci_reg;

#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	ret = atlfwd_nl_init();
	if (ret)
		goto err_fwd_netlink;
#endif

	return 0;

#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
err_fwd_netlink:
#endif
	pci_unregister_driver(&atl_pci_ops);
err_pci_reg:
	destroy_workqueue(atl_wq);
	return ret;
}
module_init(atl_module_init);

static void __exit atl_module_exit(void)
{
#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	atlfwd_nl_exit();
#endif

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
