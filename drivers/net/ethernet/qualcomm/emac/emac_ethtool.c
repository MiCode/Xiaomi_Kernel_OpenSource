/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

/* Qualcomm Technologies, Inc. EMAC Ethernet Controller ethtool support
 */

#include <linux/ethtool.h>
#include <linux/pm_runtime.h>

#include "emac.h"
#include "emac_hw.h"

#define EMAC_MAX_REG_SIZE     10
#define EMAC_STATS_LEN        51
static const char *const emac_ethtool_stat_strings[] = {
	"rx ok",
	"rx bcast",
	"rx mcast",
	"rx pause",
	"rx ctrl",
	"rx fcs err",
	"rx len err",
	"rx byte cnt",
	"rx runt",
	"rx frag",
	"rx sz 64",
	"rx sz 65 127",
	"rx sz 128 255",
	"rx sz 256 511",
	"rx sz 512 1023",
	"rx sz 1024 1518",
	"rx sz 1519 max",
	"rx sz ov",
	"rx rxf ov",
	"rx align err",
	"rx bcast byte cnt",
	"rx mcast byte cnt",
	"rx err addr",
	"rx crc align",
	"rx jubbers",
	"tx ok",
	"tx bcast",
	"tx mcast",
	"tx pause",
	"tx exc defer",
	"tx ctrl",
	"tx defer",
	"tx byte cnt",
	"tx sz 64",
	"tx sz 65 127",
	"tx sz 128 255",
	"tx sz 256 511",
	"tx sz 512 1023",
	"tx sz 1024 1518",
	"tx sz 1519 max",
	"tx 1 col",
	"tx 2 col",
	"tx late col",
	"tx abort col",
	"tx underrun",
	"tx rd eop",
	"tx len err",
	"tx trunc",
	"tx bcast byte",
	"tx mcast byte",
	"tx col",
};

static int emac_get_settings(struct net_device *netdev,
			     struct ethtool_cmd *ecmd)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_phy *phy = &adpt->phy;

	ecmd->supported = (SUPPORTED_10baseT_Half   |
			   SUPPORTED_10baseT_Full   |
			   SUPPORTED_100baseT_Half  |
			   SUPPORTED_100baseT_Full  |
			   SUPPORTED_1000baseT_Full |
			   SUPPORTED_Autoneg        |
			   SUPPORTED_TP);

	ecmd->advertising = ADVERTISED_TP;
	if (phy->autoneg) {
		ecmd->advertising |= ADVERTISED_Autoneg;
		ecmd->advertising |= phy->autoneg_advertised;
		ecmd->autoneg = AUTONEG_ENABLE;
	} else {
		ecmd->autoneg = AUTONEG_DISABLE;
	}

	ecmd->port = PORT_TP;
	ecmd->phy_address = phy->addr;
	ecmd->transceiver = XCVR_INTERNAL;

	if (phy->link_up) {
		switch (phy->link_speed) {
		case EMAC_LINK_SPEED_10_HALF:
			ecmd->speed = SPEED_10;
			ecmd->duplex = DUPLEX_HALF;
			break;
		case EMAC_LINK_SPEED_10_FULL:
			ecmd->speed = SPEED_10;
			ecmd->duplex = DUPLEX_FULL;
			break;
		case EMAC_LINK_SPEED_100_HALF:
			ecmd->speed = SPEED_100;
			ecmd->duplex = DUPLEX_HALF;
			break;
		case EMAC_LINK_SPEED_100_FULL:
			ecmd->speed = SPEED_100;
			ecmd->duplex = DUPLEX_FULL;
			break;
		case EMAC_LINK_SPEED_1GB_FULL:
			ecmd->speed = SPEED_1000;
			ecmd->duplex = DUPLEX_FULL;
			break;
		default:
			ecmd->speed = -1;
			ecmd->duplex = -1;
			break;
		}
	} else {
		ecmd->speed = -1;
		ecmd->duplex = -1;
	}

	return 0;
}

static int emac_set_settings(struct net_device *netdev,
			     struct ethtool_cmd *ecmd)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_phy *phy = &adpt->phy;
	u32 advertised, old;
	int retval = 0;
	bool autoneg;
	bool if_running = netif_running(adpt->netdev);

	emac_info(adpt, link, "ethtool cmd autoneg %d, speed %d, duplex %d\n",
		  ecmd->autoneg, ecmd->speed, ecmd->duplex);

	while (TEST_N_SET_FLAG(adpt, ADPT_STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	old = phy->autoneg_advertised;
	advertised = 0;
	if (ecmd->autoneg == AUTONEG_ENABLE) {
		advertised = EMAC_LINK_SPEED_DEFAULT;
		autoneg = true;
	} else {
		u32 speed = ecmd->speed;

		autoneg = false;
		if (speed == SPEED_1000) {
			if (ecmd->duplex != DUPLEX_FULL) {
				emac_warn(adpt, hw,
					  "1000M half is invalid\n");
				CLR_FLAG(adpt, ADPT_STATE_RESETTING);
				return -EINVAL;
			}
			advertised = EMAC_LINK_SPEED_1GB_FULL;
		} else if (speed == SPEED_100) {
			if (ecmd->duplex == DUPLEX_FULL)
				advertised = EMAC_LINK_SPEED_100_FULL;
			else
				advertised = EMAC_LINK_SPEED_100_HALF;
		} else {
			if (ecmd->duplex == DUPLEX_FULL)
				advertised = EMAC_LINK_SPEED_10_FULL;
			else
				advertised = EMAC_LINK_SPEED_10_HALF;
		}
	}

	if ((phy->autoneg == autoneg) &&
	    (phy->autoneg_advertised == advertised))
		goto done;

	pm_runtime_get_sync(netdev->dev.parent);

	/* If there is no EPHY, the EMAC internal PHY may get reset in
	 * emac_phy_setup_link_speed. Reset the MAC to avoid the memory
	 * corruption.
	 */
	if (!phy->external && if_running)
		emac_down(adpt, EMAC_HW_CTRL_RESET_MAC);

	retval = emac_phy_setup_link_speed(adpt, advertised, autoneg,
					   !phy->disable_fc_autoneg);
	if (retval) {
		emac_phy_setup_link_speed(adpt, old, autoneg,
					  !phy->disable_fc_autoneg);
	}

	if (if_running) {
		/* If there is no EPHY, bring up the interface */
		if (!phy->external)
			emac_up(adpt);
	}
	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);

done:
	CLR_FLAG(adpt, ADPT_STATE_RESETTING);
	return retval;
}

static void emac_get_pauseparam(struct net_device *netdev,
				struct ethtool_pauseparam *pause)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_phy *phy = &adpt->phy;

	if (phy->disable_fc_autoneg)
		pause->autoneg = 0;
	else
		pause->autoneg = 1;

	if (phy->cur_fc_mode == EMAC_FC_RX_PAUSE) {
		pause->rx_pause = 1;
	} else if (phy->cur_fc_mode == EMAC_FC_TX_PAUSE) {
		pause->tx_pause = 1;
	} else if (phy->cur_fc_mode == EMAC_FC_FULL) {
		pause->rx_pause = 1;
		pause->tx_pause = 1;
	}
}

static int emac_set_pauseparam(struct net_device *netdev,
			       struct ethtool_pauseparam *pause)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_phy *phy = &adpt->phy;
	enum emac_flow_ctrl req_fc_mode;
	bool disable_fc_autoneg;
	int retval = 0;

	while (TEST_N_SET_FLAG(adpt, ADPT_STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	req_fc_mode        = phy->req_fc_mode;
	disable_fc_autoneg = phy->disable_fc_autoneg;

	if (pause->autoneg != AUTONEG_ENABLE)
		disable_fc_autoneg = true;
	else
		disable_fc_autoneg = false;

	if (pause->rx_pause && pause->tx_pause) {
		req_fc_mode = EMAC_FC_FULL;
	} else if (pause->rx_pause && !pause->tx_pause) {
		req_fc_mode = EMAC_FC_RX_PAUSE;
	} else if (!pause->rx_pause && pause->tx_pause) {
		req_fc_mode = EMAC_FC_TX_PAUSE;
	} else if (!pause->rx_pause && !pause->tx_pause) {
		req_fc_mode = EMAC_FC_NONE;
	} else {
		CLR_FLAG(adpt, ADPT_STATE_RESETTING);
		return -EINVAL;
	}
	pm_runtime_get_sync(netdev->dev.parent);

	if ((phy->req_fc_mode != req_fc_mode) ||
	    (phy->disable_fc_autoneg != disable_fc_autoneg)) {
		phy->req_fc_mode	= req_fc_mode;
		phy->disable_fc_autoneg	= disable_fc_autoneg;
		if (phy->external)
			retval = emac_phy_setup_link(adpt,
						     phy->autoneg_advertised,
						     phy->autoneg,
						     !disable_fc_autoneg);
		if (!retval)
			emac_phy_config_fc(adpt);
	}
	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);

	CLR_FLAG(adpt, ADPT_STATE_RESETTING);
	return retval;
}

static u32 emac_get_msglevel(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	return adpt->msg_enable;
}

static void emac_set_msglevel(struct net_device *netdev, u32 data)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	adpt->msg_enable = data;
}

static int emac_get_regs_len(struct net_device *netdev)
{
	return EMAC_MAX_REG_SIZE * sizeof(32);
}

static void emac_get_regs(struct net_device *netdev,
			  struct ethtool_regs *regs, void *buff)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	u16 i;
	u32 *val = buff;
	static const u32 reg[EMAC_MAX_REG_SIZE] = {
		EMAC_DMA_MAS_CTRL, EMAC_MAC_CTRL, EMAC_WOL_CTRL0,
		EMAC_TXQ_CTRL_0, EMAC_RXQ_CTRL_0, EMAC_DMA_CTRL, EMAC_INT_MASK,
		EMAC_AXI_MAST_CTRL, EMAC_CORE_HW_VERSION, EMAC_MISC_CTRL,
	};

	regs->version = 0;
	regs->len = EMAC_MAX_REG_SIZE * sizeof(u32);

	memset(val, 0, EMAC_MAX_REG_SIZE * sizeof(u32));
	pm_runtime_get_sync(netdev->dev.parent);
	for (i = 0; i < ARRAY_SIZE(reg); i++)
		val[i] = emac_reg_r32(hw, EMAC, reg[i]);
	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);
}

static void emac_get_drvinfo(struct net_device *netdev,
			     struct ethtool_drvinfo *drvinfo)
{
	strlcpy(drvinfo->driver,  emac_drv_name, sizeof(drvinfo->driver));
	strlcpy(drvinfo->version, emac_drv_version, sizeof(drvinfo->version));
	strlcpy(drvinfo->bus_info, "axi", sizeof(drvinfo->bus_info));

	drvinfo->regdump_len = emac_get_regs_len(netdev);
}

static int emac_wol_exclusion(struct emac_adapter *adpt,
			      struct ethtool_wolinfo *wol)
{
	struct emac_hw *hw = &adpt->hw;

	/* WOL not supported except for the following */
	switch (hw->devid) {
	case EMAC_DEV_ID:
		return 0;
	default:
		wol->supported = 0;
		return -EINVAL;
	}
}

static void emac_get_wol(struct net_device *netdev,
			 struct ethtool_wolinfo *wol)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	wol->supported = WAKE_MAGIC | WAKE_PHY;
	wol->wolopts = 0;

	if (adpt->wol & EMAC_WOL_MAGIC)
		wol->wolopts |= WAKE_MAGIC;
	if (adpt->wol & EMAC_WOL_PHY)
		wol->wolopts |= WAKE_PHY;
}

static int emac_set_wol(struct net_device *netdev, struct ethtool_wolinfo *wol)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	if (wol->wolopts & (WAKE_ARP | WAKE_MAGICSECURE |
			    WAKE_UCAST | WAKE_BCAST | WAKE_MCAST))
		return -EOPNOTSUPP;

	if (emac_wol_exclusion(adpt, wol))
		return wol->wolopts ? -EOPNOTSUPP : 0;

	adpt->wol = 0;
	if (wol->wolopts & WAKE_MAGIC)
		adpt->wol |= EMAC_WOL_MAGIC;
	if (wol->wolopts & WAKE_PHY)
		adpt->wol |= EMAC_WOL_PHY;

	return 0;
}

static int emac_get_intr_coalesce(struct net_device *netdev,
				  struct ethtool_coalesce *ec)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;

	/* irq moderator timers have resolution of 2us */
	ec->tx_coalesce_usecs = ((hw->irq_mod & IRQ_MODERATOR_INIT_BMSK) >>
				 IRQ_MODERATOR_INIT_SHFT) * 2;
	ec->rx_coalesce_usecs = ((hw->irq_mod & IRQ_MODERATOR2_INIT_BMSK) >>
				 IRQ_MODERATOR2_INIT_SHFT) * 2;

	return 0;
}

static int emac_set_intr_coalesce(struct net_device *netdev,
				  struct ethtool_coalesce *ec)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	u32 val;

	/* irq moderator timers have resolution of 2us */
	val = ((ec->rx_coalesce_usecs / 2) << IRQ_MODERATOR2_INIT_SHFT) &
		IRQ_MODERATOR2_INIT_BMSK;
	val |= ((ec->tx_coalesce_usecs / 2) << IRQ_MODERATOR_INIT_SHFT) &
		IRQ_MODERATOR_INIT_BMSK;

	pm_runtime_get_sync(netdev->dev.parent);
	emac_reg_w32(hw, EMAC, EMAC_IRQ_MOD_TIM_INIT, val);
	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);
	hw->irq_mod = val;

	return 0;
}

static void emac_get_ringparam(struct net_device *netdev,
			       struct ethtool_ringparam *ring)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	ring->rx_max_pending = EMAC_MAX_RX_DESCS;
	ring->tx_max_pending = EMAC_MAX_TX_DESCS;
	ring->rx_pending = adpt->num_rxdescs;
	ring->tx_pending = adpt->num_txdescs;
}

static int emac_set_ringparam(struct net_device *netdev,
			      struct ethtool_ringparam *ring)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	int retval = 0;

	if ((ring->rx_mini_pending) || (ring->rx_jumbo_pending))
		return -EINVAL;

	adpt->num_txdescs = clamp_t(u32, ring->tx_pending,
				    EMAC_MIN_TX_DESCS, EMAC_MAX_TX_DESCS);

	adpt->num_rxdescs = clamp_t(u32, ring->rx_pending,
				    EMAC_MIN_RX_DESCS, EMAC_MAX_RX_DESCS);

	if (netif_running(netdev))
		retval = emac_resize_rings(netdev);

	return retval;
}

static int emac_nway_reset(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	if (netif_running(netdev))
		emac_reinit_locked(adpt);
	return 0;
}

static int emac_get_sset_count(struct net_device *netdev, int sset)
{
	switch (sset) {
	case ETH_SS_TEST:
		return 0;
	case ETH_SS_STATS:
		return EMAC_STATS_LEN;
	default:
		return -EOPNOTSUPP;
	}
}

static void emac_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	u16 i;

	switch (stringset) {
	case ETH_SS_TEST:
		break;
	case ETH_SS_STATS:
		for (i = 0; i < EMAC_STATS_LEN; i++) {
			strlcpy(data, emac_ethtool_stat_strings[i],
				ETH_GSTRING_LEN);
			data += ETH_GSTRING_LEN;
		}
		break;
	}
}

static void emac_get_ethtool_stats(struct net_device *netdev,
				   struct ethtool_stats *stats,
				   u64 *data)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	emac_update_hw_stats(adpt);
	memcpy(data, &adpt->hw_stats, EMAC_STATS_LEN * sizeof(u64));
}

static const struct ethtool_ops emac_ethtool_ops = {
	.get_settings    = emac_get_settings,
	.set_settings    = emac_set_settings,
	.get_pauseparam  = emac_get_pauseparam,
	.set_pauseparam  = emac_set_pauseparam,
	.get_drvinfo     = emac_get_drvinfo,
	.get_regs_len    = emac_get_regs_len,
	.get_regs        = emac_get_regs,
	.get_wol         = emac_get_wol,
	.set_wol         = emac_set_wol,
	.get_msglevel    = emac_get_msglevel,
	.set_msglevel    = emac_set_msglevel,
	.get_coalesce    = emac_get_intr_coalesce,
	.set_coalesce    = emac_set_intr_coalesce,
	.get_ringparam   = emac_get_ringparam,
	.set_ringparam   = emac_set_ringparam,
	.nway_reset      = emac_nway_reset,
	.get_link        = ethtool_op_get_link,
	.get_sset_count  = emac_get_sset_count,
	.get_strings	 = emac_get_strings,
	.get_ethtool_stats = emac_get_ethtool_stats,
};

/* Set ethtool operations */
void emac_set_ethtool_ops(struct net_device *netdev)
{
	netdev->ethtool_ops = &emac_ethtool_ops;
}
