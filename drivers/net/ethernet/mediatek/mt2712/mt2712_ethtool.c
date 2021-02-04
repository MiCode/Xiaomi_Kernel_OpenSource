/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mt2712_yheader.h"
#include "mt2712_ethtool.h"

struct stats {
	char stat_string[ETH_GSTRING_LEN];
	int sizeof_stat;
	int stat_offset;
};

/* HW extra status */
#define EXTRA_STAT(name) \
	{#name, FIELD_SIZEOF(struct extra_stats, name), \
	offsetof(struct prv_data, xstats.name)}

static const struct stats gstrings_stats[] = {
	EXTRA_STAT(q_re_alloc_rx_buf_failed[0]),
	EXTRA_STAT(q_re_alloc_rx_buf_failed[1]),
	EXTRA_STAT(q_re_alloc_rx_buf_failed[2]),
	EXTRA_STAT(q_re_alloc_rx_buf_failed[3]),
	EXTRA_STAT(q_re_alloc_rx_buf_failed[4]),
	EXTRA_STAT(q_re_alloc_rx_buf_failed[5]),
	EXTRA_STAT(q_re_alloc_rx_buf_failed[6]),
	EXTRA_STAT(q_re_alloc_rx_buf_failed[7]),

	/* Tx/Rx IRQ error info */
	EXTRA_STAT(tx_process_stopped_irq_n[0]),
	EXTRA_STAT(tx_process_stopped_irq_n[1]),
	EXTRA_STAT(tx_process_stopped_irq_n[2]),
	EXTRA_STAT(tx_process_stopped_irq_n[3]),
	EXTRA_STAT(tx_process_stopped_irq_n[4]),
	EXTRA_STAT(tx_process_stopped_irq_n[5]),
	EXTRA_STAT(tx_process_stopped_irq_n[6]),
	EXTRA_STAT(tx_process_stopped_irq_n[7]),
	EXTRA_STAT(rx_process_stopped_irq_n[0]),
	EXTRA_STAT(rx_process_stopped_irq_n[1]),
	EXTRA_STAT(rx_process_stopped_irq_n[2]),
	EXTRA_STAT(rx_process_stopped_irq_n[3]),
	EXTRA_STAT(rx_process_stopped_irq_n[4]),
	EXTRA_STAT(rx_process_stopped_irq_n[5]),
	EXTRA_STAT(rx_process_stopped_irq_n[6]),
	EXTRA_STAT(rx_process_stopped_irq_n[7]),
	EXTRA_STAT(tx_buf_unavailable_irq_n[0]),
	EXTRA_STAT(tx_buf_unavailable_irq_n[1]),
	EXTRA_STAT(tx_buf_unavailable_irq_n[2]),
	EXTRA_STAT(tx_buf_unavailable_irq_n[3]),
	EXTRA_STAT(tx_buf_unavailable_irq_n[4]),
	EXTRA_STAT(tx_buf_unavailable_irq_n[5]),
	EXTRA_STAT(tx_buf_unavailable_irq_n[6]),
	EXTRA_STAT(tx_buf_unavailable_irq_n[7]),
	EXTRA_STAT(rx_buf_unavailable_irq_n[0]),
	EXTRA_STAT(rx_buf_unavailable_irq_n[1]),
	EXTRA_STAT(rx_buf_unavailable_irq_n[2]),
	EXTRA_STAT(rx_buf_unavailable_irq_n[3]),
	EXTRA_STAT(rx_buf_unavailable_irq_n[4]),
	EXTRA_STAT(rx_buf_unavailable_irq_n[5]),
	EXTRA_STAT(rx_buf_unavailable_irq_n[6]),
	EXTRA_STAT(rx_buf_unavailable_irq_n[7]),
	EXTRA_STAT(rx_watchdog_irq_n),
	EXTRA_STAT(fatal_bus_error_irq_n),
	/* Tx/Rx IRQ Events */
	EXTRA_STAT(tx_normal_irq_n[0]),
	EXTRA_STAT(tx_normal_irq_n[1]),
	EXTRA_STAT(tx_normal_irq_n[2]),
	EXTRA_STAT(tx_normal_irq_n[3]),
	EXTRA_STAT(tx_normal_irq_n[4]),
	EXTRA_STAT(tx_normal_irq_n[5]),
	EXTRA_STAT(tx_normal_irq_n[6]),
	EXTRA_STAT(tx_normal_irq_n[7]),
	EXTRA_STAT(rx_normal_irq_n[0]),
	EXTRA_STAT(rx_normal_irq_n[1]),
	EXTRA_STAT(rx_normal_irq_n[2]),
	EXTRA_STAT(rx_normal_irq_n[3]),
	EXTRA_STAT(rx_normal_irq_n[4]),
	EXTRA_STAT(rx_normal_irq_n[5]),
	EXTRA_STAT(rx_normal_irq_n[6]),
	EXTRA_STAT(rx_normal_irq_n[7]),
	EXTRA_STAT(napi_poll_n),
	EXTRA_STAT(tx_clean_n[0]),
	EXTRA_STAT(tx_clean_n[1]),
	EXTRA_STAT(tx_clean_n[2]),
	EXTRA_STAT(tx_clean_n[3]),
	EXTRA_STAT(tx_clean_n[4]),
	EXTRA_STAT(tx_clean_n[5]),
	EXTRA_STAT(tx_clean_n[6]),
	EXTRA_STAT(tx_clean_n[7]),
	/* Tx/Rx frames */
	EXTRA_STAT(tx_pkt_n),
	EXTRA_STAT(rx_pkt_n),
	EXTRA_STAT(tx_timestamp_captured_n),
	EXTRA_STAT(rx_timestamp_captured_n),

	/* Tx/Rx frames per channels/queues */
	EXTRA_STAT(q_tx_pkt_n[0]),
	EXTRA_STAT(q_tx_pkt_n[1]),
	EXTRA_STAT(q_tx_pkt_n[2]),
	EXTRA_STAT(q_tx_pkt_n[3]),
	EXTRA_STAT(q_tx_pkt_n[4]),
	EXTRA_STAT(q_tx_pkt_n[5]),
	EXTRA_STAT(q_tx_pkt_n[6]),
	EXTRA_STAT(q_tx_pkt_n[7]),
	EXTRA_STAT(q_rx_pkt_n[0]),
	EXTRA_STAT(q_rx_pkt_n[1]),
	EXTRA_STAT(q_rx_pkt_n[2]),
	EXTRA_STAT(q_rx_pkt_n[3]),
	EXTRA_STAT(q_rx_pkt_n[4]),
	EXTRA_STAT(q_rx_pkt_n[5]),
	EXTRA_STAT(q_rx_pkt_n[6]),
	EXTRA_STAT(q_rx_pkt_n[7]),
};

#define EXTRA_STAT_LEN ARRAY_SIZE(gstrings_stats)

/* HW MAC Management counters (if supported) */
#define MMC_STAT(m)	\
	{ #m, FIELD_SIZEOF(struct mmc_counters, m),	\
	offsetof(struct prv_data, mmc.m)}

static const struct stats mmc[] = {
	/* MMC TX counters */
	MMC_STAT(mmc_tx_octetcount_gb),
	MMC_STAT(mmc_tx_framecount_gb),
	MMC_STAT(mmc_tx_broadcastframe_g),
	MMC_STAT(mmc_tx_multicastframe_g),
	MMC_STAT(mmc_tx_64_octets_gb),
	MMC_STAT(mmc_tx_65_to_127_octets_gb),
	MMC_STAT(mmc_tx_128_to_255_octets_gb),
	MMC_STAT(mmc_tx_256_to_511_octets_gb),
	MMC_STAT(mmc_tx_512_to_1023_octets_gb),
	MMC_STAT(mmc_tx_1024_to_max_octets_gb),
	MMC_STAT(mmc_tx_unicast_gb),
	MMC_STAT(mmc_tx_multicast_gb),
	MMC_STAT(mmc_tx_broadcast_gb),
	MMC_STAT(mmc_tx_underflow_error),
	MMC_STAT(mmc_tx_singlecol_g),
	MMC_STAT(mmc_tx_multicol_g),
	MMC_STAT(mmc_tx_deferred),
	MMC_STAT(mmc_tx_latecol),
	MMC_STAT(mmc_tx_exesscol),
	MMC_STAT(mmc_tx_carrier_error),
	MMC_STAT(mmc_tx_octetcount_g),
	MMC_STAT(mmc_tx_framecount_g),
	MMC_STAT(mmc_tx_excessdef),
	MMC_STAT(mmc_tx_pause_frame),
	MMC_STAT(mmc_tx_vlan_frame_g),

	/* MMC RX counters */
	MMC_STAT(mmc_rx_framecount_gb),
	MMC_STAT(mmc_rx_octetcount_gb),
	MMC_STAT(mmc_rx_octetcount_g),
	MMC_STAT(mmc_rx_broadcastframe_g),
	MMC_STAT(mmc_rx_multicastframe_g),
	MMC_STAT(mmc_rx_crc_errror),
	MMC_STAT(mmc_rx_align_error),
	MMC_STAT(mmc_rx_run_error),
	MMC_STAT(mmc_rx_jabber_error),
	MMC_STAT(mmc_rx_undersize_g),
	MMC_STAT(mmc_rx_oversize_g),
	MMC_STAT(mmc_rx_64_octets_gb),
	MMC_STAT(mmc_rx_65_to_127_octets_gb),
	MMC_STAT(mmc_rx_128_to_255_octets_gb),
	MMC_STAT(mmc_rx_256_to_511_octets_gb),
	MMC_STAT(mmc_rx_512_to_1023_octets_gb),
	MMC_STAT(mmc_rx_1024_to_max_octets_gb),
	MMC_STAT(mmc_rx_unicast_g),
	MMC_STAT(mmc_rx_length_error),
	MMC_STAT(mmc_rx_outofrangetype),
	MMC_STAT(mmc_rx_pause_frames),
	MMC_STAT(mmc_rx_fifo_overflow),
	MMC_STAT(mmc_rx_vlan_frames_gb),
	MMC_STAT(mmc_rx_watchdog_error),

	/* IPC */
	MMC_STAT(mmc_rx_ipc_intr_mask),
	MMC_STAT(mmc_rx_ipc_intr),

	/* IPv4 */
	MMC_STAT(mmc_rx_ipv4_gd),
	MMC_STAT(mmc_rx_ipv4_hderr),
	MMC_STAT(mmc_rx_ipv4_nopay),
	MMC_STAT(mmc_rx_ipv4_frag),
	MMC_STAT(mmc_rx_ipv4_udsbl),

	/* IPV6 */
	MMC_STAT(mmc_rx_ipv6_gd_octets),
	MMC_STAT(mmc_rx_ipv6_hderr_octets),
	MMC_STAT(mmc_rx_ipv6_nopay_octets),

	/* Protocols */
	MMC_STAT(mmc_rx_udp_gd),
	MMC_STAT(mmc_rx_udp_err),
	MMC_STAT(mmc_rx_tcp_gd),
	MMC_STAT(mmc_rx_tcp_err),
	MMC_STAT(mmc_rx_icmp_gd),
	MMC_STAT(mmc_rx_icmp_err),

	/* IPv4 */
	MMC_STAT(mmc_rx_ipv4_gd_octets),
	MMC_STAT(mmc_rx_ipv4_hderr_octets),
	MMC_STAT(mmc_rx_ipv4_nopay_octets),
	MMC_STAT(mmc_rx_ipv4_frag_octets),
	MMC_STAT(mmc_rx_ipv4_udsbl_octets),

	/* IPV6 */
	MMC_STAT(mmc_rx_ipv6_gd),
	MMC_STAT(mmc_rx_ipv6_hderr),
	MMC_STAT(mmc_rx_ipv6_nopay),

	/* Protocols */
	MMC_STAT(mmc_rx_udp_gd_octets),
	MMC_STAT(mmc_rx_udp_err_octets),
	MMC_STAT(mmc_rx_tcp_gd_octets),
	MMC_STAT(mmc_rx_tcp_err_octets),
	MMC_STAT(mmc_rx_icmp_gd_octets),
	MMC_STAT(mmc_rx_icmp_err_octets),
};

#define MMC_STATS_LEN ARRAY_SIZE(mmc)

#define SPEED_UNKNOWN -1
#define DUPLEX_UNKNOWN 0xff
static int getsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct prv_data *pdata = netdev_priv(dev);
	int ret = 0;

	if (!pdata->phydev) {
		pr_alert("%s: PHY is not registered\n", dev->name);
		return -ENODEV;
	}

	if (!netif_running(dev)) {
		pr_alert("%s: interface is disabled: we cannot track link speed / duplex settings\n", dev->name);
		return -EBUSY;
	}

	cmd->transceiver = XCVR_EXTERNAL;

	spin_lock_irq(&pdata->lock);
	ret = phy_ethtool_gset(pdata->phydev, cmd);
	spin_unlock_irq(&pdata->lock);

	return ret;
}

static int setsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct prv_data *pdata = netdev_priv(dev);
	int ret = 0;

	spin_lock_irq(&pdata->lock);
	ret = phy_ethtool_sset(pdata->phydev, cmd);
	spin_unlock_irq(&pdata->lock);

	return ret;
}

static const struct ethtool_ops ethtool_ops = {
	.get_link = ethtool_op_get_link,
	.get_pauseparam = get_pauseparam,
	.set_pauseparam = set_pauseparam,
	.get_settings = getsettings,
	.set_settings = setsettings,
	.get_coalesce = get_coalesce,
	.set_coalesce = set_coalesce,
	.get_ethtool_stats = get_ethtool_stats,
	.get_strings = get_strings,
	.get_sset_count = get_sset_count,
	.get_ts_info = get_ts_info,
};

const struct ethtool_ops *get_ethtool_ops(void)
{
	return (const struct ethtool_ops *)&ethtool_ops;
}

/* \details This function is invoked by kernel when user request to get the
 * pause parameters through standard ethtool command.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] Pause – pointer to ethtool_pauseparam structure.
 *
 * \return void
 */
static void get_pauseparam(struct net_device *dev, struct ethtool_pauseparam *pause)
{
	struct prv_data *pdata = netdev_priv(dev);

	pause->rx_pause = 0;
	pause->tx_pause = 0;
	pause->autoneg = pdata->phydev->autoneg;

	if ((pdata->flow_ctrl & MTK_FLOW_CTRL_RX) == MTK_FLOW_CTRL_RX)
		pause->rx_pause = 1;

	if ((pdata->flow_ctrl & MTK_FLOW_CTRL_TX) == MTK_FLOW_CTRL_TX)
		pause->tx_pause = 1;
}

/* \details This function is invoked by kernel when user request to set the
 * pause parameters through standard ethtool command.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] pause – pointer to ethtool_pauseparam structure.
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */
static int set_pauseparam(struct net_device *dev, struct ethtool_pauseparam *pause)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct phy_device *phydev = pdata->phydev;
	int new_pause = MTK_FLOW_CTRL_OFF;
	int ret = 0;

	if (pause->rx_pause)
		new_pause |= MTK_FLOW_CTRL_RX;
	if (pause->tx_pause)
		new_pause |= MTK_FLOW_CTRL_TX;

	if (new_pause == pdata->flow_ctrl && !pause->autoneg)
		return -EINVAL;

	pdata->flow_ctrl = new_pause;

	phydev->autoneg = pause->autoneg;
	if (phydev->autoneg) {
		if (netif_running(dev))
			ret = phy_start_aneg(phydev);
	} else {
		configure_flow_ctrl(pdata);
	}

	return ret;
}

void configure_flow_ctrl(struct prv_data *pdata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned int q_inx;

	if ((pdata->flow_ctrl & MTK_FLOW_CTRL_RX) == MTK_FLOW_CTRL_RX)
		hw_if->enable_rx_flow_ctrl();
	else
		hw_if->disable_rx_flow_ctrl();

	/* As ethtool does not provide queue level configuration
	 * Tx flow control is disabled/enabled for all transmit queues
	 */
	if ((pdata->flow_ctrl & MTK_FLOW_CTRL_TX) == MTK_FLOW_CTRL_TX) {
		for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++)
			hw_if->enable_tx_flow_ctrl(q_inx);
	} else {
		for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++)
			hw_if->disable_tx_flow_ctrl(q_inx);
	}

	pdata->oldflow_ctrl = pdata->flow_ctrl;
}

/* \details This function is invoked by kernel when user request to get the
 * various device settings through standard ethtool command. This function
 * support to get the PHY related settings like link status, interface type,
 * auto-negotiation parameters and pause parameters etc.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] cmd – pointer to ethtool_cmd structure.
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */
#define SPEED_UNKNOWN -1
#define DUPLEX_UNKNOWN 0xff

u32 usec2riwt(u32 usec, struct prv_data *pdata)
{
	u32 ret = 0;

	/* Eg:
	 * System clock is 62.5MHz, each clock cycle would then be 16ns
	 * For value 0x1 in watchdog timer, device would wait for 256
	 * clock cycles,
	 * ie, (16ns x 256) => 4.096us (rounding off to 4us)
	 * So formula with above values is,
	 * ret = usec/4
	 */
	ret = (usec * (SYSCLOCK / 1000000)) / 256;

	return ret;
}

static u32 riwt2usec(u32 riwt, struct prv_data *pdata)
{
	u32 ret = 0;

	/* using formula from 'usec2riwt' */
	ret = (riwt * 256) / (SYSCLOCK / 1000000);

	return ret;
}

/* \details This function is invoked by kernel when user request to get
 * interrupt coalescing parameters. As coalescing parameters are same
 * for all the channels, so this function will get coalescing
 * details from channel zero and return.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] wol – pointer to ethtool_coalesce structure.
 *
 * \return int
 *
 * \retval 0
 */
static int get_coalesce(struct net_device *dev, struct ethtool_coalesce *ec)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct rx_wrapper_descriptor *rx_desc_data =
	    GET_RX_WRAPPER_DESC(0);

	memset(ec, 0, sizeof(struct ethtool_coalesce));

	ec->rx_coalesce_usecs = riwt2usec(rx_desc_data->rx_riwt, pdata);
	ec->rx_max_coalesced_frames = rx_desc_data->rx_coal_frames;

	return 0;
}

/* \details This function is invoked by kernel when user request to set
 * interrupt coalescing parameters. This driver maintains same coalescing
 * parameters for all the channels, hence same changes will be applied to
 * all the channels.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] wol – pointer to ethtool_coalesce structure.
 *
 * \return int
 *
 * \retval zero on success and -ve number on failure.
 */
static int set_coalesce(struct net_device *dev, struct ethtool_coalesce *ec)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct rx_wrapper_descriptor *rx_desc_data =
	    GET_RX_WRAPPER_DESC(0);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned int rx_riwt, rx_usec, local_use_riwt, q_inx;

	/* Check for not supported parameters  */
	if ((ec->rx_coalesce_usecs_irq) ||
	    (ec->rx_max_coalesced_frames_irq) || (ec->tx_coalesce_usecs_irq) ||
	    (ec->use_adaptive_rx_coalesce) || (ec->use_adaptive_tx_coalesce) ||
	    (ec->pkt_rate_low) || (ec->rx_coalesce_usecs_low) ||
	    (ec->rx_max_coalesced_frames_low) || (ec->tx_coalesce_usecs_high) ||
	    (ec->tx_max_coalesced_frames_low) || (ec->pkt_rate_high) ||
	    (ec->tx_coalesce_usecs_low) || (ec->rx_coalesce_usecs_high) ||
	    (ec->rx_max_coalesced_frames_high) ||
	    (ec->tx_max_coalesced_frames_irq) ||
	    (ec->stats_block_coalesce_usecs) ||
	    (ec->tx_max_coalesced_frames_high) || (ec->rate_sample_interval) ||
	    (ec->tx_coalesce_usecs) || (ec->tx_max_coalesced_frames))
		return -EOPNOTSUPP;

	/* both rx_coalesce_usecs and rx_max_coalesced_frames should
	 * be > 0 in order for coalescing to be active.
	 */
	if ((ec->rx_coalesce_usecs <= 3) || (ec->rx_max_coalesced_frames <= 1))
		local_use_riwt = 0;
	else
		local_use_riwt = 1;

	pr_err("RX COALESCING is %s\n", (local_use_riwt ? "ENABLED" : "DISABLED"));

	rx_riwt = usec2riwt(ec->rx_coalesce_usecs, pdata);

	/* Check the bounds of values for RX */
	if (rx_riwt > MAX_DMA_RIWT) {
		rx_usec = riwt2usec(MAX_DMA_RIWT, pdata);
		pr_err("RX Coalesing is limited to %d usecs\n", rx_usec);
		return -EINVAL;
	}
	if (ec->rx_max_coalesced_frames > RX_DESC_CNT) {
		pr_err("RX Coalesing is limited to %d frames\n", RX_MAX_FRAMES);
		return -EINVAL;
	}
	if (rx_desc_data->rx_coal_frames != ec->rx_max_coalesced_frames &&
	    netif_running(dev)) {
		pr_err("Coalesce frame parameter can be changed only if interface is down\n");
		return -EINVAL;
	}
	/* The selected parameters are applied to all the
	 * receive queues equally, so all the queue configurations
	 * are in sync
	 */
	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++) {
		rx_desc_data = GET_RX_WRAPPER_DESC(q_inx);
		rx_desc_data->use_riwt = local_use_riwt;
		rx_desc_data->rx_riwt = rx_riwt;
		rx_desc_data->rx_coal_frames = ec->rx_max_coalesced_frames;
		hw_if->config_rx_watchdog(q_inx, rx_desc_data->rx_riwt);
	}

	return 0;
}

/* \details This function is invoked by kernel when user
 * requests to get the extended statistics about the device.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] data – pointer in which extended statistics
 *                   should be put.
 *
 * \return void
 */
static void get_ethtool_stats(
			struct net_device *dev,
			struct ethtool_stats *dummy,
			u64 *data)
{
	struct prv_data *pdata = netdev_priv(dev);
	int i, j = 0;

	if (pdata->hw_feat.mmc_sel) {
		mmc_read(&pdata->mmc);

		for (i = 0; i < MMC_STATS_LEN; i++) {
			char *p = (char *)pdata +
					mmc[i].stat_offset;

			data[j++] = (mmc[i].sizeof_stat ==
				sizeof(u64)) ? (*(u64 *)p) : (*(u32 *)p);
		}
	}

	for (i = 0; i < EXTRA_STAT_LEN; i++) {
		char *p = (char *)pdata +
				gstrings_stats[i].stat_offset;
		data[j++] = (gstrings_stats[i].sizeof_stat ==
				sizeof(u64)) ? (*(u64 *)p) : (*(u32 *)p);
	}
}

/* \details This function returns a set of strings that describe
 * the requested objects.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] data – pointer in which requested string should be put.
 *
 * \return void
 */
static void get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	struct prv_data *pdata = netdev_priv(dev);
	int i;
	u8 *p = data;

	switch (stringset) {
	case ETH_SS_STATS:
		if (pdata->hw_feat.mmc_sel) {
			for (i = 0; i < MMC_STATS_LEN; i++) {
				memcpy(p, mmc[i].stat_string, ETH_GSTRING_LEN);
				p += ETH_GSTRING_LEN;
			}
		}

		for (i = 0; i < EXTRA_STAT_LEN; i++) {
			memcpy(p, gstrings_stats[i].stat_string, ETH_GSTRING_LEN);
			p += ETH_GSTRING_LEN;
		}
		break;
	default:
		WARN_ON(1);
	}
}

/* \details This function gets number of strings that @get_strings
 * will write.
 *
 * \param[in] dev – pointer to net device structure.
 *
 * \return int
 *
 * \retval +ve(>0) on success, 0 if that string is not
 * defined and -ve on failure.
 */
static int get_sset_count(struct net_device *dev, int sset)
{
	struct prv_data *pdata = netdev_priv(dev);
	int len = 0;

	switch (sset) {
	case ETH_SS_STATS:
		if (pdata->hw_feat.mmc_sel)
			len = MMC_STATS_LEN;
		len += EXTRA_STAT_LEN;
		break;
	default:
		len = -EOPNOTSUPP;
	}

	return len;
}

/* \details This function is invoked by kernel when user
 * request to enable/disable tso feature.
 *
 * \param[in] dev – pointer to net device structure.
 * \param[in] data – 1/0 for enabling/disabling tso.
 *
 * \return int
 *
 * \retval 0 on success and -ve on failure.
 */
static int get_ts_info(struct net_device *ndev, struct ethtool_ts_info *info)
{
	struct prv_data *pdata = netdev_priv(ndev);

	info->so_timestamping = SOF_TIMESTAMPING_TX_SOFTWARE |
				SOF_TIMESTAMPING_RX_SOFTWARE |
				SOF_TIMESTAMPING_SOFTWARE |
				SOF_TIMESTAMPING_TX_HARDWARE |
				SOF_TIMESTAMPING_RX_HARDWARE |
				SOF_TIMESTAMPING_RAW_HARDWARE;

	if (pdata->ptp_clock)
		info->phc_index = ptp_clock_index(pdata->ptp_clock);
	else
		info->phc_index = -1;

	info->tx_types = (1 << HWTSTAMP_TX_OFF) | (1 << HWTSTAMP_TX_ON);

	info->rx_filters = (1 << HWTSTAMP_FILTER_NONE) |
			   (1 << HWTSTAMP_FILTER_PTP_V1_L4_EVENT) |
			   (1 << HWTSTAMP_FILTER_PTP_V1_L4_SYNC) |
			   (1 << HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_EVENT) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_SYNC) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_DELAY_REQ) |
			   (1 << HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) |
			   (1 << HWTSTAMP_FILTER_ALL);
	return 0;
}
