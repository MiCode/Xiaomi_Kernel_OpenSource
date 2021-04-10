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

#include <linux/etherdevice.h>

#include "atl_common.h"
#include "atl_hw.h"
#include "atl2_fw.h"
#include "atl_fw.h"

#define ATL2_FW_READ_TRY_MAX 1000

#define atl2_shared_buffer_write(HW, ITEM, VARIABLE) \
	BUILD_BUG_ON_MSG((offsetof(struct fw_interface_in, ITEM) % \
			 sizeof(u32)) != 0,\
			 "Unaligned write " # ITEM);\
	BUILD_BUG_ON_MSG((sizeof(VARIABLE) %  sizeof(u32)) != 0,\
			 "Unaligned write length " # ITEM);\
	atl2_mif_shared_buf_write(HW,\
		(offsetof(struct fw_interface_in, ITEM) / sizeof(u32)),\
		(u32 *)&VARIABLE, sizeof(VARIABLE) / sizeof(u32))

#define atl2_shared_buffer_get(HW, ITEM, VARIABLE) \
	BUILD_BUG_ON_MSG((offsetof(struct fw_interface_in, ITEM) % \
			 sizeof(u32)) != 0,\
			 "Unaligned get " # ITEM);\
	BUILD_BUG_ON_MSG((sizeof(VARIABLE) %  sizeof(u32)) != 0,\
			 "Unaligned get length " # ITEM);\
	atl2_mif_shared_buf_get(HW, \
		(offsetof(struct fw_interface_in, ITEM) / sizeof(u32)),\
		(u32 *)&VARIABLE, \
		sizeof(VARIABLE) / sizeof(u32))

/* This should never be used on non atomic fields,
 * treat any > u32 read as non atomic.
 */
#define atl2_shared_buffer_read(HW, ITEM, VARIABLE) \
{\
	BUILD_BUG_ON_MSG((offsetof(struct fw_interface_out, ITEM) % \
			 sizeof(u32)) != 0,\
			 "Unaligned read " # ITEM);\
	BUILD_BUG_ON_MSG((sizeof(VARIABLE) %  sizeof(u32)) != 0,\
			 "Unaligned read length " # ITEM);\
	BUILD_BUG_ON_MSG(sizeof(VARIABLE) > sizeof(u32),\
			 "Non atomic read " # ITEM);\
	atl2_mif_shared_buf_read(HW, \
		(offsetof(struct fw_interface_out, ITEM) / sizeof(u32)),\
		(u32 *)&VARIABLE, \
		sizeof(VARIABLE) / sizeof(u32));\
}

static void atl2_mif_shared_buf_get(struct atl_hw *hw, int offset,
				       u32 *data, int len)
{
	int i;

	for (i = 0; i < len; i++)
		data[i] = atl_read(hw, ATL2_MIF_SHARED_BUFFER_IN(offset + i));
}

static void atl2_mif_shared_boot_buf_write(struct atl_hw *hw, int offset,
					   u32 *data, int len)
{
	int i;

	for (i = 0; i < len / sizeof(u32); i++)
		atl_write(hw, ATL2_MIF_SHARED_BUFFER_BOOT(i), data[i]);
}

static void atl2_mif_shared_buf_write(struct atl_hw *hw, int offset,
				  u32 *data, int len)
{
	int i;

	for (i = 0; i < len; i++)
		atl_write(hw, ATL2_MIF_SHARED_BUFFER_IN(offset + i),
				data[i]);
}

static void atl2_mif_shared_buf_read(struct atl_hw *hw, int offset,
				     u32 *data, int len)
{
	int i;

	for (i = 0; i < len; i++)
		data[i] = atl_read(hw, ATL2_MIF_SHARED_BUFFER_OUT(offset + i));
}

static void atl2_mif_host_finished_write_set(struct atl_hw *hw, u32 finish)
{
	atl_write_bits(hw, ATL2_MIF_HOST_FINISHED_WRITE, 0, 1, finish);
}

static u32 atl2_mif_mcp_finished_read_get(struct atl_hw *hw)
{
	return atl_read(hw, ATL2_MIF_MCP_FINISHED_READ) & 1;
}

#define atl2_shared_buffer_read_safe(HW, ITEM, DATA) \
	_atl2_shared_buffer_read_safe((HW), \
		(offsetof(struct fw_interface_out, ITEM) / sizeof(u32)),\
		sizeof(((struct fw_interface_out *)0)->ITEM) / sizeof(u32),\
		(DATA))

/* There are two 16bit transaction counters. The one is incremented at start of
 * changing non atomic value, the other one - at the end. So we need to wait for
 * ma moment when they are equal each other at the reading non atomic memory and
 * at the end of read.
 */
static int _atl2_shared_buffer_read_safe(struct atl_hw *hw,
		     uint32_t offset, uint32_t dwords,
		     void *data)
{
	struct transaction_counter_s tid1, tid2;
	int cnt = 0;

	do {
		do {
			atl2_shared_buffer_read(hw, transaction_id, tid1);
			cnt++;
			if (cnt > ATL2_FW_READ_TRY_MAX)
				return -ETIME;
			if (tid1.transaction_cnt_a != tid1.transaction_cnt_b)
				udelay(1);
		} while (tid1.transaction_cnt_a != tid1.transaction_cnt_b);

		atl2_mif_shared_buf_read(hw, offset, (u32 *)data, dwords);

		atl2_shared_buffer_read(hw, transaction_id, tid2);

		cnt++;
		//TODO: no much sense to handle these errors
		// therefore put WARN here to make it visible
		if (cnt > ATL2_FW_READ_TRY_MAX)
			return -ETIME;
	} while (tid2.transaction_cnt_a != tid2.transaction_cnt_b ||
		 tid1.transaction_cnt_a != tid2.transaction_cnt_a);

	return 0;
}

static inline int atl2_shared_buffer_finish_ack(struct atl_hw *hw)
{
	u32 val;
	int err = 0;

	atl2_mif_host_finished_write_set(hw, 1U);

	busy_wait(1000, udelay(100), val,
		  atl2_mif_mcp_finished_read_get(hw),
		  val != 0);

	if (val != 0)
		err = -ETIME;
	WARN(err, "atl2_shared_buffer_finish_ack");

	return err;
}

static int atl2_fw_get_filter_caps(struct atl_hw *hw)
{
	struct atl_nic *nic = container_of(hw, struct atl_nic, hw);
	struct filter_caps_s filter_caps;
	u32 tag_top;
	int err;

	err = atl2_shared_buffer_read_safe(hw, filter_caps, &filter_caps);
	if (err)
		return err;

	hw->art_base_index = filter_caps.rslv_tbl_base_index * 8;
	hw->art_available = filter_caps.rslv_tbl_count * 8;
	if (hw->art_available == 0)
		hw->art_available = 128;
	nic->rxf_flex.available = 1;
	nic->rxf_flex.base_index = filter_caps.flexible_filter_mask >> 1;
	nic->rxf_mac.base_index = filter_caps.l2_filters_base_index;
	nic->rxf_mac.available = filter_caps.l2_filter_count;
	nic->rxf_etype.base_index = filter_caps.ethertype_filter_base_index;
	nic->rxf_etype.available = filter_caps.ethertype_filter_count;
	nic->rxf_etype.tag_top =
		(nic->rxf_etype.available >= ATL2_RPF_ETYPE_TAGS) ?
		 (ATL2_RPF_ETYPE_TAGS) : (ATL2_RPF_ETYPE_TAGS >> 1);
	nic->rxf_vlan.base_index = filter_caps.vlan_filter_base_index;
	/* 0 - no tag, 1 - reserved for vlan-filter-offload filters */
	tag_top = (filter_caps.vlan_filter_count == ATL_VLAN_FLT_NUM) ?
		  (ATL_VLAN_FLT_NUM - 2) :
		  (ATL_VLAN_FLT_NUM / 2 - 2);
	nic->rxf_vlan.available = min_t(u32, filter_caps.vlan_filter_count - 2,
					tag_top);
	nic->rxf_ntuple.l3_v4_base_index = filter_caps.l3_ip4_filter_base_index;
	nic->rxf_ntuple.l3_v4_available = min_t(u32,
						filter_caps.l3_ip4_filter_count,
						ATL_NTUPLE_FLT_NUM - 1);
	nic->rxf_ntuple.l3_v6_base_index = filter_caps.l3_ip6_filter_base_index;
	nic->rxf_ntuple.l3_v6_available = filter_caps.l3_ip6_filter_count;
	nic->rxf_ntuple.l4_base_index = filter_caps.l4_filter_base_index;
	nic->rxf_ntuple.l4_available = min_t(u32, filter_caps.l4_filter_count,
						ATL_NTUPLE_FLT_NUM - 1);

	return 0;
}

static int __atl2_fw_wait_init(struct atl_hw *hw)
{
	struct request_policy_s request_policy;
	struct link_control_s link_control;
	uint32_t mtu;
	int err;

	BUILD_BUG_ON_MSG(sizeof(struct link_options_s) != 0x4,
			 "linkOptions invalid size");
	BUILD_BUG_ON_MSG(sizeof(struct thermal_shutdown_s) != 0x4,
			 "thermalShutdown invalid size");
	BUILD_BUG_ON_MSG(sizeof(struct sleep_proxy_s) != 0x958,
			 "sleepProxy invalid size");
	BUILD_BUG_ON_MSG(sizeof(struct pause_quanta_s) != 0x18,
			 "pauseQuanta invalid size");
	BUILD_BUG_ON_MSG(sizeof(struct cable_diag_control_s) != 0x4,
			 "cableDiagControl invalid size");
	BUILD_BUG_ON_MSG(sizeof(struct statistics_s) != 0x74,
			 "statistics_s invalid size");


	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_in, mtu) != 0,
			 "mtu invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_in, mac_address) != 0x8,
			 "macAddress invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_in,
				  link_control) != 0x10,
			 "linkControl invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_in,
				  link_options) != 0x18,
			 "linkOptions invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_in,
				  thermal_shutdown) != 0x20,
			 "thermalShutdown invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_in, sleep_proxy) != 0x28,
			 "sleepProxy invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_in,
				  pause_quanta) != 0x984,
			 "pauseQuanta invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_in,
				  cable_diag_control) != 0xA44,
			 "cableDiagControl invalid offset");

	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out, version) != 0x04,
			 "version invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out, link_status) != 0x14,
			 "linkStatus invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out,
				  wol_status) != 0x18,
			 "wolStatus invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out,
				  mac_health_monitor) != 0x610,
			 "macHealthMonitor invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out,
				  phy_health_monitor) != 0x620,
			 "phyHealthMonitor invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out,
				  cable_diag_status) != 0x630,
			 "cableDiagStatus invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out,
				  device_link_caps) != 0x648,
			 "deviceLinkCaps invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out,
				 sleep_proxy_caps) != 0x650,
			 "sleepProxyCaps invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out,
				  lkp_link_caps) != 0x660,
			 "lkpLinkCaps invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out, core_dump) != 0x668,
			 "coreDump invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out, stats) != 0x700,
			 "stats invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out, filter_caps) != 0x774,
			 "filter_caps invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out,
				  management_status) != 0x78c,
			 "management_status invalid offset");
	BUILD_BUG_ON_MSG(offsetof(struct fw_interface_out, trace) != 0x800,
			 "trace invalid offset");

	err = atl2_fw_get_filter_caps(hw);
	if (err)
		return err;

	atl2_shared_buffer_get(hw, link_control, link_control);
	link_control.mode = ATL2_HOST_MODE_ACTIVE;
	atl2_shared_buffer_write(hw, link_control, link_control);

	atl2_shared_buffer_get(hw, mtu, mtu);
	mtu = ATL_MAX_MTU + ETH_FCS_LEN + ETH_HLEN;
	atl2_shared_buffer_write(hw, mtu, mtu);

	atl2_shared_buffer_get(hw, request_policy, request_policy);
	request_policy.bcast.accept = 1;
	request_policy.bcast.queue_or_tc = 1;
	request_policy.bcast.rx_queue_tc_index = 0;
	request_policy.mcast.accept = 1;
	request_policy.mcast.queue_or_tc = 1;
	request_policy.mcast.rx_queue_tc_index = 0;
	request_policy.promisc.queue_or_tc = 1;
	request_policy.promisc.rx_queue_tc_index = 0;
	atl2_shared_buffer_write(hw, request_policy, request_policy);

	return atl2_shared_buffer_finish_ack(hw);
}

int atl2_fw_set_filter_policy(struct atl_hw *hw, bool promisc, bool allmulti)
{
	struct request_policy_s request_policy;

	atl2_shared_buffer_get(hw, request_policy, request_policy);

	request_policy.promisc.all = promisc;
	request_policy.mcast.promisc = allmulti;

	atl2_shared_buffer_write(hw, request_policy, request_policy);
	return atl2_shared_buffer_finish_ack(hw);
}

static int atl2_fw_deinit(struct atl_hw *hw)
{
	struct link_control_s link_control;
	int err = 0;

	atl2_shared_buffer_get(hw, link_control, link_control);
	link_control.mode = ATL2_HOST_MODE_SHUTDOWN;

	atl2_shared_buffer_write(hw, link_control, link_control);
	err = atl2_shared_buffer_finish_ack(hw);

	return err;
}

static inline unsigned int atl_link_adv(struct atl_link_state *lstate)
{
	struct atl_hw *hw = container_of(lstate, struct atl_hw, link_state);

	if (lstate->thermal_throttled
		&& hw->thermal.flags & atl_thermal_throttle)
		/* FW doesn't provide raw LP's advertized rates, only
		 * the rates adverized both by us and LP. Here we
		 * advertize not just the throttled_to rate, but also
		 * all the lower rates as well. That way if LP changes
		 * or dynamically starts to adverize a lower rate than
		 * throttled_to, we will notice that in
		 * atl_thermal_check() and switch to that lower
		 * rate there.
		 */
		return (lstate->advertized & ATL_EEE_MASK) |
		       (BIT(lstate->throttled_to + 1) - 1);

	return lstate->advertized;
}

static bool atl2_fw_set_link_needed(struct atl_link_state *lstate)
{
	struct atl_fc_state *fc = &lstate->fc;
	bool ret = false;

	if (fc->req != fc->prev_req)
		ret = true;

	if (atl_link_adv(lstate) != lstate->prev_advertized)
		ret = true;

	return ret;
}

static void atl2_set_rate(struct atl_hw *hw,
				   struct link_options_s *link_options)
{
	unsigned int adv = atl_link_adv(&hw->link_state);

	link_options->rate_10M_hd =  !!(adv & BIT(atl_link_type_idx_10m_half));
	link_options->rate_100M_hd = !!(adv & BIT(atl_link_type_idx_100m_half));
	link_options->rate_1G_hd   = !!(adv & BIT(atl_link_type_idx_1g_half));
	link_options->rate_10M = !!(adv & BIT(atl_link_type_idx_10m));
	link_options->rate_100M = !!(adv & BIT(atl_link_type_idx_100m));
	link_options->rate_1G   = !!(adv & BIT(atl_link_type_idx_1g));
	link_options->rate_2P5G = !!(adv & BIT(atl_link_type_idx_2p5g));
	link_options->rate_5G   = !!(adv & BIT(atl_link_type_idx_5g));
	link_options->rate_10G  = !!(adv & BIT(atl_link_type_idx_10g));
	link_options->rate_N5G = link_options->rate_5G;
	link_options->rate_N2P5G = link_options->rate_2P5G;
}

static void atl2_set_eee(struct atl_link_state *lstate,
			 struct link_options_s *link_options, bool eee_enabled)
{
	uint32_t eee_advertized = lstate->advertized >> ATL_EEE_BIT_OFFT;

	if (!eee_enabled) {
		link_options->eee_100M = 0;
		link_options->eee_1G = 0;
		link_options->eee_2P5G = 0;
		link_options->eee_5G = 0;
		link_options->eee_10G = 0;
		return;
	}

	link_options->eee_100M = !!(eee_advertized & BIT(atl_link_type_idx_100m));
	link_options->eee_1G = !!(eee_advertized & BIT(atl_link_type_idx_1g));
	link_options->eee_2P5G = !!(eee_advertized & BIT(atl_link_type_idx_2p5g));
	link_options->eee_5G = !!(eee_advertized & BIT(atl_link_type_idx_5g));
	link_options->eee_10G = !!(eee_advertized & BIT(atl_link_type_idx_10g));
}

/* fw lock must be held */
static void __atl2_fw_set_link(struct atl_hw *hw)
{
	struct atl_link_state *lstate = &hw->link_state;
	struct link_options_s link_options;
	int err = 0;

	atl2_shared_buffer_get(hw, link_options, link_options);

	link_options.pause_rx = link_options.pause_tx = 0;
	if (lstate->fc.req & atl_fc_rx) {
		link_options.pause_rx = 1;
		link_options.pause_tx = 1;
	}
	if (lstate->fc.req & atl_fc_tx)
		link_options.pause_tx ^= 1;

	link_options.link_up = 1;
	if (lstate->force_off)
		link_options.link_up = 0;

	atl2_set_rate(hw, &link_options);
	atl2_set_eee(lstate, &link_options, lstate->eee_enabled);

	atl2_shared_buffer_write(hw, link_options, link_options);
	err = atl2_shared_buffer_finish_ack(hw);

	lstate->prev_advertized = atl_link_adv(lstate);
	lstate->fc.prev_req = lstate->fc.req;
}

static void atl2_fw_set_link(struct atl_hw *hw, bool force)
{
	if (!force && !atl2_fw_set_link_needed(&hw->link_state))
		return;

	atl_lock_fw(hw);
	__atl2_fw_set_link(hw);
	atl_unlock_fw(hw);
}

static u32 a2_fw_caps_to_mask(struct device_link_caps_s *link_caps)
{
	u32 supported = 0;

	if (link_caps->rate_10G)
		supported |= BIT(atl_link_type_idx_10g);
	if (link_caps->rate_5G)
		supported |= BIT(atl_link_type_idx_5g);
	if (link_caps->rate_2P5G)
		supported |= BIT(atl_link_type_idx_2p5g);
	if (link_caps->rate_1G)
		supported |= BIT(atl_link_type_idx_1g);
	if (link_caps->rate_100M)
		supported |= BIT(atl_link_type_idx_100m);
	if (link_caps->rate_10M)
		supported |= BIT(atl_link_type_idx_10m);
	if (link_caps->rate_1G_hd)
		supported |= BIT(atl_link_type_idx_1g_half);
	if (link_caps->rate_100M_hd)
		supported |= BIT(atl_link_type_idx_100m_half);
	if (link_caps->rate_10M_hd)
		supported |= BIT(atl_link_type_idx_10m_half);

	if (link_caps->eee_10G)
		supported |= BIT(atl_link_type_idx_10g) << ATL_EEE_BIT_OFFT;
	if (link_caps->eee_5G)
		supported |= BIT(atl_link_type_idx_5g) << ATL_EEE_BIT_OFFT;
	if (link_caps->eee_2P5G)
		supported |= BIT(atl_link_type_idx_2p5g) << ATL_EEE_BIT_OFFT;
	if (link_caps->eee_1G)
		supported |= BIT(atl_link_type_idx_1g) << ATL_EEE_BIT_OFFT;
	if (link_caps->eee_100M)
		supported |= BIT(atl_link_type_idx_100m) << ATL_EEE_BIT_OFFT;

	return supported;
}

static u32 a2_fw_lkp_to_mask(struct lkp_link_caps_s *lkp_link_caps)
{
	u32 rate = 0;

	if (lkp_link_caps->rate_10G)
		rate |= BIT(atl_link_type_idx_10g);
	if (lkp_link_caps->rate_5G)
		rate |= BIT(atl_link_type_idx_5g);
	if (lkp_link_caps->rate_2P5G)
		rate |= BIT(atl_link_type_idx_2p5g);
	if (lkp_link_caps->rate_1G)
		rate |= BIT(atl_link_type_idx_1g);
	if (lkp_link_caps->rate_100M)
		rate |= BIT(atl_link_type_idx_100m);
	if (lkp_link_caps->rate_10M)
		rate |= BIT(atl_link_type_idx_10m);
	if (lkp_link_caps->rate_1G_hd)
		rate |= BIT(atl_link_type_idx_1g_half);
	if (lkp_link_caps->rate_100M_hd)
		rate |= BIT(atl_link_type_idx_100m_half);
	if (lkp_link_caps->rate_10M_hd)
		rate |= BIT(atl_link_type_idx_10m_half);

	if (lkp_link_caps->eee_10G)
		rate |= BIT(atl_link_type_idx_10g) << ATL_EEE_BIT_OFFT;
	if (lkp_link_caps->eee_5G)
		rate |= BIT(atl_link_type_idx_5g) << ATL_EEE_BIT_OFFT;
	if (lkp_link_caps->eee_2P5G)
		rate |= BIT(atl_link_type_idx_2p5g) << ATL_EEE_BIT_OFFT;
	if (lkp_link_caps->eee_1G)
		rate |= BIT(atl_link_type_idx_1g) << ATL_EEE_BIT_OFFT;
	if (lkp_link_caps->eee_100M)
		rate |= BIT(atl_link_type_idx_100m) << ATL_EEE_BIT_OFFT;

	return rate;
}

static int __atl2_fw_update_link_status(struct atl_hw *hw)
{
	struct atl_link_state *lstate = &hw->link_state;
	struct link_status_s link_status;
	struct lkp_link_caps_s lkp_link_caps;
	enum atl_fc_mode fc = atl_fc_none;

	atl2_shared_buffer_read(hw, link_status, link_status);

	switch (link_status.link_rate) {
	case ATL2_FW_LINK_RATE_10G:
		lstate->link = &atl_link_types[atl_link_type_idx_10g];
		break;
	case ATL2_FW_LINK_RATE_5G:
		lstate->link = &atl_link_types[atl_link_type_idx_5g];
		break;
	case ATL2_FW_LINK_RATE_2G5:
		lstate->link = &atl_link_types[atl_link_type_idx_2p5g];
		break;
	case ATL2_FW_LINK_RATE_1G:
		lstate->link = link_status.duplex ?
				&atl_link_types[atl_link_type_idx_1g] :
				&atl_link_types[atl_link_type_idx_1g_half];
		break;
	case ATL2_FW_LINK_RATE_100M:
		lstate->link = link_status.duplex ?
				&atl_link_types[atl_link_type_idx_100m] :
				&atl_link_types[atl_link_type_idx_100m_half];
		break;
	case ATL2_FW_LINK_RATE_10M:
		lstate->link = link_status.duplex ?
				&atl_link_types[atl_link_type_idx_10m] :
				&atl_link_types[atl_link_type_idx_10m_half];
		break;
	default:
		lstate->link = NULL;
	}

	if (link_status.pause_rx)
		fc |= atl_fc_rx;
	if (link_status.pause_tx)
		fc |= atl_fc_tx;
	lstate->fc.cur = fc;
	lstate->eee = link_status.eee;

	atl2_shared_buffer_read(hw, lkp_link_caps, lkp_link_caps);

	lstate->lp_advertized = a2_fw_lkp_to_mask(&lkp_link_caps);

	return 0;
}

static struct atl_link_type *atl2_fw_check_link(struct atl_hw *hw)
{
	struct atl_link_type *link;
	struct atl_link_state *lstate = &hw->link_state;
	struct phy_health_monitor_s phy_health_monitor;
	int ret = 0;

	memset(&phy_health_monitor, 0, sizeof(phy_health_monitor));

	atl_lock_fw(hw);

	__atl2_fw_update_link_status(hw);

	ret = atl2_shared_buffer_read_safe(hw, phy_health_monitor,
					   &phy_health_monitor);
	atl_unlock_fw(hw);

	atl_thermal_check(hw, phy_health_monitor.phy_hot_warning);

	/* Thermal check might have reset link due to throttling */
	link = lstate->link;

	return link;
}

/* fw lock must be held */
static int __atl2_fw_get_link_caps(struct atl_hw *hw)
{
	struct device_link_caps_s device_link_caps;
	struct atl_mcp *mcp = &hw->mcp;
	unsigned int supported = 0;
	int ret = 0;


	atl2_shared_buffer_read(hw, device_link_caps, device_link_caps);

	mcp->wdog_disabled = true;

	supported  = a2_fw_caps_to_mask(&device_link_caps);

	hw->link_state.supported = supported;
	hw->link_state.lp_lowest = fls(supported) - 1;
	mcp->caps_low = atl_fw2_wake_on_link_force;

	return ret;
}

static int atl2_fw_restart_aneg(struct atl_hw *hw)
{
	struct link_options_s link_options;
	int ret = 0;

	atl2_shared_buffer_get(hw, link_options, link_options);

	link_options.link_renegotiate = 1;

	atl2_shared_buffer_write(hw, link_options, link_options);
	ret = atl2_shared_buffer_finish_ack(hw);

	link_options.link_renegotiate = 0;
	atl2_shared_buffer_write(hw, link_options, link_options);

	return ret;
}

static void atl2_fw_set_default_link(struct atl_hw *hw)
{
	struct atl_link_state *lstate = &hw->link_state;

	lstate->autoneg = true;
	lstate->advertized = hw->link_state.supported;
	lstate->fc.req = atl_fc_full;
	lstate->eee_enabled = 0;
	lstate->advertized &= ~ATL_EEE_MASK;
}

static int atl2_fw_get_phy_temperature(struct atl_hw *hw, int *temp)
{
	struct phy_health_monitor_s phy_health_monitor;
	int ret = 0;

	atl_lock_fw(hw);

	ret = atl2_shared_buffer_read_safe(hw, phy_health_monitor,
					   &phy_health_monitor);

	*temp = (int8_t)(phy_health_monitor.phy_temperature) * 1000;

	atl_unlock_fw(hw);

	return ret;
}

static int atl2_fw_get_mac_addr(struct atl_hw *hw, uint8_t *mac)
{
	struct mac_address_aligned_s mac_address;
	int err = 0;

	atl2_shared_buffer_get(hw, mac_address, mac_address);

	ether_addr_copy(mac, (u8 *)mac_address.aligned.mac_address);

	return err;
}

/* fw lock must be held */
static int __atl2_fw_get_hbeat(struct atl_hw *hw, uint16_t *hbeat)
{
	struct phy_health_monitor_s phy_health_monitor;
	int ret = 0;

	ret = atl2_shared_buffer_read_safe(hw, phy_health_monitor,
					   &phy_health_monitor);

	*hbeat = phy_health_monitor.phy_heart_beat;

	return ret;
}

static int atl2_fw_set_phy_loopback(struct atl_nic *nic, u32 mode)
{
	struct device_link_caps_s device_link_caps;
	bool on = !!(nic->priv_flags & BIT(mode));
	struct atl_hw *hw = &nic->hw;
	struct link_options_s link_options;
	int ret = 0;

	atl_lock_fw(hw);

	atl2_shared_buffer_read(hw, device_link_caps, device_link_caps);
	atl2_shared_buffer_get(hw, link_options, link_options);

	switch (mode) {
	case ATL_PF_LPB_INT_PHY:
		if (!device_link_caps.internal_loopback) {
			ret = -EOPNOTSUPP;
			goto unlock;
		}

		link_options.internal_loopback = on;
		break;
	case ATL_PF_LPB_EXT_PHY:
		if (!device_link_caps.external_loopback) {
			ret = -EOPNOTSUPP;
			goto unlock;
		}
		link_options.external_loopback = on;
		break;
	}

	atl2_shared_buffer_write(hw, link_options, link_options);
	ret = atl2_shared_buffer_finish_ack(hw);

unlock:
	atl_unlock_fw(hw);

	return ret;
}

static int atl2_fw_enable_wol(struct atl_hw *hw, unsigned int wol_mode)
{
	struct link_options_s link_options;
	struct link_control_s link_control;
	struct wake_on_lan_s wake_on_lan;
	struct mac_address_aligned_s mac_address;
	int ret = 0;

	atl_lock_fw(hw);

	atl2_shared_buffer_get(hw, sleep_proxy, wake_on_lan);

	if (wol_mode & atl_fw_wake_on_link) {
		wake_on_lan.wake_on_link_up = 1;
		if (test_bit(ATL_ST_UP, &hw->state))
			wake_on_lan.restore_link_before_wake = 1;
	}

	if (wol_mode & atl_fw_wake_on_link_rtpm) {
		wake_on_lan.wake_on_link_up = 1;
	}

	if (wol_mode & atl_fw_wake_on_magic) {
		wake_on_lan.wake_on_magic_packet = 1;
		if (test_bit(ATL_ST_UP, &hw->state))
			wake_on_lan.restore_link_before_wake = 1;
	}

	ether_addr_copy(mac_address.aligned.mac_address, hw->mac_addr);

	atl2_shared_buffer_write(hw, mac_address, mac_address);
	atl2_shared_buffer_write(hw, sleep_proxy, wake_on_lan);
	atl2_shared_buffer_get(hw, link_control, link_control);
	link_control.mode = ATL2_HOST_MODE_SLEEP_PROXY;
	atl2_shared_buffer_write(hw, link_control, link_control);

	atl2_shared_buffer_get(hw, link_options, link_options);
	link_options.link_up = 1;
	atl2_shared_buffer_write(hw, link_options, link_options);

	ret = atl2_shared_buffer_finish_ack(hw);

	atl_unlock_fw(hw);
	return ret;
}

static int atl2_fw_update_thermal(struct atl_hw *hw)
{
	bool enable = !!(hw->thermal.flags & atl_thermal_monitor);
	struct phy_health_monitor_s phy_health_monitor;
	struct thermal_shutdown_s thermal_shutdown;
	int ret = 0;

	memset(&phy_health_monitor, 0, sizeof(phy_health_monitor));

	atl_lock_fw(hw);

	atl2_shared_buffer_get(hw, thermal_shutdown, thermal_shutdown);
	thermal_shutdown.shutdown_enable = enable;
	thermal_shutdown.shutdown_temp_threshold = hw->thermal.crit;
	thermal_shutdown.warning_hot_tempThreshold = hw->thermal.high;
	thermal_shutdown.warning_cold_temp_threshold = hw->thermal.low;
	atl2_shared_buffer_write(hw, thermal_shutdown, thermal_shutdown);
	ret = atl2_shared_buffer_finish_ack(hw);

	atl2_shared_buffer_read_safe(hw, phy_health_monitor,
				     &phy_health_monitor);
	atl_unlock_fw(hw);

	/* Thresholds might have changed, recheck state. */
	atl_thermal_check(hw, phy_health_monitor.phy_hot_warning);

	return ret;
}

static int atl2_fw_set_pad_stripping(struct atl_hw *hw, bool on)
{
	struct link_control_s link_control;
	int err = 0;

	atl_lock_fw(hw);

	atl2_shared_buffer_get(hw, link_control, link_control);
	link_control.enable_frame_padding_removal_rx = on;
	atl2_shared_buffer_write(hw, link_control, link_control);
	err = atl2_shared_buffer_finish_ack(hw);

	atl_unlock_fw(hw);

	return err;
}

#define ATL2_FW_CFG_DUMP_SIZE (4 + (sizeof(struct link_options_s) * 3) + \
			      (sizeof(struct link_control_s) * 3))
static int atl2_fw_dump_cfg(struct atl_hw *hw)
{
	if (!hw->mcp.fw_cfg_dump)
		hw->mcp.fw_cfg_dump = devm_kzalloc(&hw->pdev->dev,
						   ATL2_FW_CFG_DUMP_SIZE,
						   GFP_KERNEL);
	if (!hw->mcp.fw_cfg_dump)
		return -ENOMEM;

	/* save link configuration */
	hw->mcp.fw_cfg_dump[0] = ((2 * 3 * 4) << 16) | 3;
	hw->mcp.fw_cfg_dump[1] = offsetof(struct fw_interface_in, link_control);
	atl2_shared_buffer_get(hw, link_control, hw->mcp.fw_cfg_dump[2]);
	hw->mcp.fw_cfg_dump[3] = ~0;

	hw->mcp.fw_cfg_dump[4] = offsetof(struct fw_interface_in, link_options);
	atl2_shared_buffer_get(hw, link_options, hw->mcp.fw_cfg_dump[5]);
	hw->mcp.fw_cfg_dump[6] = ~0;

	return 0;
}

static void atl2_confirm_buffer_write(struct atl_hw *hw,
				      uint32_t offset, uint32_t len)
{
	struct data_buffer_status_s buffer_status;

	buffer_status.data_offset = offset;
	buffer_status.data_length = len;
	atl2_shared_buffer_write(hw, data_buffer_status, buffer_status);
}

static int atl2_fw_restore_cfg(struct atl_hw *hw)
{
	uint32_t req_adr = atl_read(hw, ATL2_MIF_BOOT_READ_REQ_ADR);
	uint32_t req_len = atl_read(hw, ATL2_MIF_BOOT_READ_REQ_LEN);

	if (req_adr == ATL2_ITI_ADDRESS_START) {
		struct fw_iti_hdr iti_header;

		memset(&iti_header, 0, sizeof(iti_header));
		iti_header.instuction_bitmask = BIT(2);
		iti_header.iti[0].type = 2;
		iti_header.iti[0].length = ATL2_FW_CFG_DUMP_SIZE;
		atl2_mif_shared_boot_buf_write(hw, 0, (void *)&iti_header,
					       sizeof(iti_header));
		atl2_confirm_buffer_write(hw, req_adr, req_len);

		atl2_mif_host_finished_write_set(hw, 1U);

		return 0;
	} else if (req_adr == ATL2_ITI_ADDRESS_BLOCK_1) {
		atl2_confirm_buffer_write(hw, req_adr, req_len);
		atl2_mif_shared_boot_buf_write(hw, 0,
			(void *)hw->mcp.fw_cfg_dump, ATL2_FW_CFG_DUMP_SIZE);
		atl2_mif_host_finished_write_set(hw, 1U);
		return 0;
	}

	atl_dev_err("FW requests invalid address %#x", req_adr);

	return -EFAULT;
}

static int atl2_fw_set_mediadetect(struct atl_hw *hw, bool on)
{
	struct link_options_s link_options;

	atl2_shared_buffer_get(hw, link_options, link_options);

	link_options.low_power_autoneg = on;

	atl2_shared_buffer_write(hw, link_options, link_options);

	return  atl2_shared_buffer_finish_ack(hw);
}

static int atl2_fw_set_downshift(struct atl_hw *hw, bool on)
{
	struct link_options_s link_options;

	atl2_shared_buffer_get(hw, link_options, link_options);

	link_options.downshift = on;

	atl2_shared_buffer_write(hw, link_options, link_options);

	return  atl2_shared_buffer_finish_ack(hw);
}

static int atl2_fw_unsupported(struct atl_hw *hw)
{
	return -EOPNOTSUPP;
}

int atl2_get_fw_version(struct atl_hw *hw)
{
	struct atl_mcp *mcp = &hw->mcp;
	struct version_s version;

	atl2_shared_buffer_read_safe(hw, version, &version);
	mcp->fw_rev = version.bundle.major << 24 | version.bundle.minor << 16 |
		      version.bundle.build;

	mcp->interface_ver = version.drv_iface_ver;
	return 0;
}

static struct atl_fw_ops atl2_fw_ops = {
		.__wait_fw_init = __atl2_fw_wait_init,
		.deinit = atl2_fw_deinit,
		.set_link = atl2_fw_set_link,
		.check_link = atl2_fw_check_link,
		.__get_link_caps = __atl2_fw_get_link_caps,
		.restart_aneg = atl2_fw_restart_aneg,
		.set_default_link = atl2_fw_set_default_link,
		.get_phy_temperature = atl2_fw_get_phy_temperature,
		.set_mediadetect = atl2_fw_set_mediadetect,
		.set_downshift = atl2_fw_set_downshift,
		.send_macsec_req = (void *)atl2_fw_unsupported,
		.set_pad_stripping = atl2_fw_set_pad_stripping,
		.get_mac_addr = atl2_fw_get_mac_addr,
		.__get_hbeat = __atl2_fw_get_hbeat,
		.set_phy_loopback = atl2_fw_set_phy_loopback,
		.enable_wol = atl2_fw_enable_wol,
		.dump_cfg = (void *)atl2_fw_dump_cfg,
		.restore_cfg = atl2_fw_restore_cfg,
		.update_thermal = atl2_fw_update_thermal,
	};

int atl2_fw_init(struct atl_hw *hw)
{
	struct atl_mcp *mcp = &hw->mcp;
	int ret;

	atl2_get_fw_version(hw);

	mcp->ops = &atl2_fw_ops;
	atl_dev_dbg("Detect ATL2FW %x\n", mcp->fw_rev);

	ret = mcp->ops->__wait_fw_init(hw);
	if (ret)
		return ret;

	mcp->fw_stat_addr = 0;
	mcp->rpc_addr = 0;

	ret = mcp->ops->__get_hbeat(hw, &mcp->phy_hbeat);
	if (ret)
		return ret;
	mcp->next_wdog = jiffies + 2 * HZ;

	ret = mcp->ops->__get_link_caps(hw);
	if (ret)
		return ret;

	return ret;
}
