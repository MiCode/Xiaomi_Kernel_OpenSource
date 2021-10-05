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
#include "atl_hw.h"
#include "atl_drviface.h"

#define ATL_FW_CFG_DUMP_SIZE 2

struct atl_link_type atl_link_types[] = {
#define LINK_TYPE(_idx, _name, _speed, _duplex, _ethtl_idx, _fw1_bit, _fw2_bit)	\
	[_idx] = {							\
		.name = _name,						\
		.speed = _speed,					\
		.duplex = _duplex,					\
		.ethtool_idx = _ethtl_idx,				\
		.fw_bits = {						\
		[0] = _fw1_bit,						\
		[1] = _fw2_bit,						\
		},							\
	},

	LINK_TYPE(atl_link_type_idx_10m_half,
		"10BaseTX-HD", 10, DUPLEX_HALF,
		ETHTOOL_LINK_MODE_10baseT_Half_BIT, 0, 0)
	LINK_TYPE(atl_link_type_idx_10m,
		"10BaseTX-FD", 10, DUPLEX_FULL,
		ETHTOOL_LINK_MODE_10baseT_Full_BIT, 0, BIT(1))
	LINK_TYPE(atl_link_type_idx_100m_half,
		"100BaseTX-HD", 100, DUPLEX_HALF,
		ETHTOOL_LINK_MODE_100baseT_Half_BIT, 0, 0)
	LINK_TYPE(atl_link_type_idx_100m,
		"100BaseTX-FD", 100, DUPLEX_FULL,
		ETHTOOL_LINK_MODE_100baseT_Full_BIT, 0x20, BIT(5))
	LINK_TYPE(atl_link_type_idx_1g_half,
		"1000BaseT-HD", 1000, DUPLEX_HALF,
		ETHTOOL_LINK_MODE_1000baseT_Half_BIT, 0, 0)
	LINK_TYPE(atl_link_type_idx_1g,
		"1000BaseT-FD", 1000, DUPLEX_FULL,
		ETHTOOL_LINK_MODE_1000baseT_Full_BIT, 0x10, BIT(8))
	LINK_TYPE(atl_link_type_idx_2p5g,
		"2.5GBaseT-FD", 2500, DUPLEX_FULL,
		ETHTOOL_LINK_MODE_2500baseT_Full_BIT, 8, BIT(9))
	LINK_TYPE(atl_link_type_idx_5g,
		"5GBaseT-FD", 5000, DUPLEX_FULL,
		ETHTOOL_LINK_MODE_5000baseT_Full_BIT, 2, BIT(10))
	LINK_TYPE(atl_link_type_idx_10g,
		"10GBaseT-FD", 10000, DUPLEX_FULL,
		ETHTOOL_LINK_MODE_10000baseT_Full_BIT, 1, BIT(11))
};
#define ATL_FW2_LINK_MSK (BIT(5) | BIT(8) | BIT(9) | BIT(10) | BIT(11))

const int atl_num_rates = ARRAY_SIZE(atl_link_types);

/* fw lock must be held */
static int __atl_fw1_wait_fw_init(struct atl_hw *hw)
{
	uint32_t hostData_addr;
	uint32_t id, new_id;
	int ret;

	mdelay(10);

	busy_wait(2000, mdelay(1), hostData_addr,
		  atl_read(hw, ATL_MCP_SCRATCH(FW_STAT_STRUCT)),
		  hostData_addr == 0);

	atl_dev_dbg("got hostData address: 0x%x\n", hostData_addr);

	ret = atl_read_mcp_mem(hw, hostData_addr + 4, &id, 4);
	if (ret)
		return  ret;

	busy_wait(10000, mdelay(1), ret,
		  atl_read_mcp_mem(hw, hostData_addr + 4, &new_id, 4),
		  !ret && new_id == id);
	if (ret)
		return ret;
	if (new_id == id) {
		atl_dev_err("timeout waiting for FW to start (initial transactionId 0x%x, hostData addr 0x%x)\n",
			    id, hostData_addr);
		return -EIO;
	}

	/* return fw1_wait_drviface(hw, NULL); */
	return 0;
}

/* fw lock must be held */
static int __atl_fw2_wait_fw_init(struct atl_hw *hw)
{
	uint32_t reg;
	int ret = 0;

	busy_wait(1000, mdelay(1), reg, atl_read(hw, ATL_GLOBAL_FW_IMAGE_ID),
		!reg);
	if (!reg)
		return -EIO;

	hw->mcp.req_high = atl_read(hw,
				 ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH));

	hw->mcp.fw_stat_addr = atl_read(hw, ATL_MCP_SCRATCH(FW_STAT_STRUCT));
	hw->mcp.rpc_addr = atl_read(hw, ATL_MCP_SCRATCH(FW2_RPC_DATA));

	ret = atl_read_fwstat_word(hw, atl_fw2_stat_settings_addr,
		&hw->mcp.fw_settings_addr);
	if (ret)
		return ret;

	ret = atl_read_fwstat_word(hw, atl_fw2_stat_settings_len,
		&hw->mcp.fw_settings_len);
	if (ret)
		return ret;

	return 0;
}

static struct atl_link_type *atl_parse_fw_bits(struct atl_hw *hw,
	uint32_t low, uint32_t high, int fw_idx)
{
	struct atl_link_state *lstate = &hw->link_state;
	unsigned int lp_adv = 0, adv = lstate->advertized;
	struct atl_link_type *link;
	bool eee = false;
	int last = -1;
	int i;

	atl_for_each_rate(i, link) {
		uint32_t link_bit = link->fw_bits[fw_idx];

		if (!(low & link_bit))
			continue;

		if (high & link_bit)
			lp_adv |= BIT(i + ATL_EEE_BIT_OFFT);

		lp_adv |= BIT(i);
		if (adv & BIT(i))
			last = i;
	}

	lstate->lp_advertized = lp_adv;

	link = 0;
	if (last >= 0) {
		link = &atl_link_types[last];
		if ((lp_adv & BIT(last + ATL_EEE_BIT_OFFT)) &&
			(adv & BIT(last + ATL_EEE_BIT_OFFT)))
			eee = true;
	}

	lstate->link = link;
	lstate->eee = eee;
	return link;
}

static struct atl_link_type *atl_fw1_check_link(struct atl_hw *hw)
{
	uint32_t reg;
	struct atl_link_type *link;

	atl_lock_fw(hw);
	reg = atl_read(hw, ATL_MCP_SCRATCH(FW1_LINK_STS));

	if ((reg & 0xf) != 2)
		reg = 0;

	reg = (reg >> 16) & 0xff;

	link = atl_parse_fw_bits(hw, reg, 0, 0);

	atl_unlock_fw(hw);
	return link;
}

static struct atl_link_type *atl_fw2_check_link(struct atl_hw *hw)
{
	struct atl_link_type *link;
	struct atl_link_state *lstate = &hw->link_state;
	enum atl_fc_mode fc = atl_fc_none;
	uint32_t low;
	uint32_t high;
	bool alarm;

	low = atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_LOW));
	high = atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH));

	link = atl_parse_fw_bits(hw, low, high, 1);

	alarm = !!(low & atl_fw2_thermal_alarm);
	atl_thermal_check(hw, alarm);

	/* Thermal check might have reset link due to throttling */
	link = lstate->link;

	if (link) {
		if (high & atl_fw2_pause)
			fc |= atl_fc_rx;
		if (high & atl_fw2_asym_pause)
			fc |= atl_fc_tx;
	}

	lstate->fc.cur = fc;

	return link;
}

/* fw lock must be held */
static int __atl_fw1_get_link_caps(struct atl_hw *hw)
{
	return 0;
}

/* fw lock must be held */
static int __atl_fw2_get_link_caps(struct atl_hw *hw)
{
	struct atl_nic *nic = container_of(hw, struct atl_nic, hw);
	struct atl_mcp *mcp = &hw->mcp;
	uint32_t fw_stat_addr = mcp->fw_stat_addr;
	struct atl_link_type *rate;
	unsigned int supported = 0;
	uint32_t caps[2], caps_ex;
	uint32_t mask = atl_fw2_pause_mask | atl_fw2_link_drop;
	int i, ret;

	atl_dev_dbg("Host data struct addr: %#x\n", fw_stat_addr);
	ret = atl_read_mcp_mem(hw, fw_stat_addr + atl_fw2_stat_lcaps,
		caps, 8);
	if (ret)
		return ret;
	ret = atl_read_fwstat_word(hw, atl_fw2_stat_caps_ex, &caps_ex);
	if (ret)
		return ret;

	mcp->caps_low = caps[0];
	mcp->caps_high = caps[1];
	mcp->caps_ex = caps_ex;
	mcp->wdog_disabled = !(mcp->caps_ex & atl_fw2_ex_caps_mac_heartbeat);
	atl_dev_dbg("Got link caps: %#x %#x %#x\n", caps[0], caps[1], caps_ex);

	atl_for_each_rate(i, rate) {
		uint32_t bit = rate->fw_bits[1];

		if (bit & caps[0]) {
			supported |= BIT(i);
			if (bit & caps[1]) {
				supported |= BIT(i + ATL_EEE_BIT_OFFT);
				mask |= bit;
			}
		}
	}

	mcp->req_high_mask = ~mask;
	hw->link_state.supported = supported;
	hw->link_state.lp_lowest = fls(supported) - 1;

	nic->rxf_flex.base_index = 0;
	nic->rxf_flex.available = ATL_FLEX_FLT_NUM;
	nic->rxf_mac.base_index = 0;
	nic->rxf_mac.available = ATL_UC_FLT_NUM;
	nic->rxf_etype.base_index = 0;
	nic->rxf_etype.available = ATL_ETYPE_FLT_NUM - 1; /* 1 reserved by FW */
	nic->rxf_vlan.base_index = 0;
	nic->rxf_vlan.available = ATL_VLAN_FLT_NUM;
	nic->rxf_ntuple.l3_v4_base_index = 0;
	nic->rxf_ntuple.l3_v4_available = ATL_NTUPLE_FLT_NUM;
	nic->rxf_ntuple.l3_v6_base_index = 0;
	nic->rxf_ntuple.l3_v6_available = ATL_NTUPLE_V6_FLT_NUM;
	nic->rxf_ntuple.l4_base_index = 0;
	nic->rxf_ntuple.l4_available = ATL_NTUPLE_FLT_NUM;

	return ret;
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

static inline bool atl_fw1_set_link_needed(struct atl_link_state *lstate)
{
	bool ret = false;

	if (atl_link_adv(lstate) != lstate->prev_advertized) {
		ret = true;
		lstate->prev_advertized = atl_link_adv(lstate);
	}

	return ret;
}

static inline bool atl_fw2_set_link_needed(struct atl_link_state *lstate)
{
	struct atl_fc_state *fc = &lstate->fc;
	bool ret = false;

	if (fc->req != fc->prev_req) {
		ret = true;
		fc->prev_req = fc->req;
	}

	return atl_fw1_set_link_needed(lstate) || ret;
}

static uint64_t atl_set_fw_bits(struct atl_hw *hw, int fw_idx)
{
	unsigned int adv = atl_link_adv(&hw->link_state);
	struct atl_link_type *ltype;
	uint64_t link = 0;
	int i;

	atl_for_each_rate(i, ltype) {
		uint32_t bit = ltype->fw_bits[fw_idx];

		if (adv & BIT(i)) {
			link |= bit;
			if (adv & BIT(i + ATL_EEE_BIT_OFFT))
				link |= (uint64_t)bit << 32;
		}
	}

	return link;
}

static void atl_fw1_set_link(struct atl_hw *hw, bool force)
{
	uint32_t bits;

	if (!force && !atl_fw1_set_link_needed(&hw->link_state))
		return;

	atl_lock_fw(hw);

	bits = (atl_set_fw_bits(hw, 0) << 16) | 2;
	atl_write(hw, ATL_MCP_SCRATCH(FW1_LINK_REQ), bits);

	atl_unlock_fw(hw);
}

/* fw lock must be held */
static void __atl_fw2_set_link(struct atl_hw *hw)
{
	struct atl_link_state *lstate = &hw->link_state;
	uint32_t hi_bits;
	uint64_t bits;

	hi_bits = hw->mcp.req_high & hw->mcp.req_high_mask;

	if (lstate->fc.req & atl_fc_rx)
		hi_bits |= atl_fw2_pause | atl_fw2_asym_pause;

	if (lstate->fc.req & atl_fc_tx)
		hi_bits ^= atl_fw2_asym_pause;

	bits = atl_set_fw_bits(hw, 1);
	hi_bits |= bits >> 32;

	if (lstate->force_off)
		hi_bits |= atl_fw2_link_drop;

	hw->mcp.req_high = hi_bits;
	atl_write_mask_bits(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_LOW),
			    ATL_FW2_LINK_MSK, bits);
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), hi_bits);
}

static void atl_fw2_set_link(struct atl_hw *hw, bool force)
{
	if (!force && !atl_fw2_set_link_needed(&hw->link_state))
		return;

	atl_lock_fw(hw);
	__atl_fw2_set_link(hw);
	atl_unlock_fw(hw);
}

static int atl_fw1_unsupported(struct atl_hw *hw)
{
	return -EOPNOTSUPP;
}

static int __atl_fw2_restart_aneg(struct atl_hw *hw)
{
	/* Autoneg restart is self-clearing, no need to track via
	 * mcp->req_high */
	atl_set_bits(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), BIT(31));
	return 0;
}

static int atl_fw2_restart_aneg(struct atl_hw *hw)
{
	atl_lock_fw(hw);
	__atl_fw2_restart_aneg(hw);
	atl_unlock_fw(hw);
	return 0;
}

static void atl_fw1_set_default_link(struct atl_hw *hw)
{
	struct atl_link_state *lstate = &hw->link_state;

	lstate->autoneg = true;
	lstate->advertized = hw->link_state.supported;
}

static void atl_fw2_set_default_link(struct atl_hw *hw)
{
	struct atl_link_state *lstate = &hw->link_state;

	atl_fw1_set_default_link(hw);
	lstate->fc.req = atl_fc_full;
	lstate->eee_enabled = 0;
	lstate->advertized &= ~ATL_EEE_MASK;
}

static int atl_fw1_enable_wol(struct atl_hw *hw, unsigned int wol_mode)
{
	return -EOPNOTSUPP;
}
static int atl_fw2_enable_wol(struct atl_hw *hw, unsigned int wol_mode)
{
	int ret = 0;
	struct offloadInfo *info;
	struct drvIface *msg = NULL;
	uint32_t val, wol_bits = 0, req_high = hw->mcp.req_high;
	uint32_t low_req, wol_ex_flags = 0;

	atl_lock_fw(hw);

	low_req = atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_LOW));

	if (wol_mode & atl_fw_wake_on_link) {
		wol_bits |= atl_fw2_wake_on_link;
		wol_ex_flags |= atl_fw2_wol_ex_wake_on_link_keep_rate;
		low_req &= ~atl_fw2_wake_on_link_force;
	}

	if (wol_mode & atl_fw_wake_on_link_rtpm) {
		wol_bits |= atl_fw2_wake_on_link;
		low_req |= atl_fw2_wake_on_link_force;
	}

	if (wol_mode & atl_fw_wake_on_magic) {
		wol_bits |= atl_fw2_nic_proxy | atl_fw2_wol;
		wol_ex_flags |= atl_fw2_wol_ex_wake_on_magic_keep_rate;

		ret = -ENOMEM;
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		if (!msg)
			goto unlock;

		info = &msg->fw2xOffloads;
		info->version = 0;
		info->len = sizeof(*info);
		memcpy(info->macAddr, hw->mac_addr, ETH_ALEN);

		ret = atl_write_mcp_mem(hw, 0, msg,
			(info->len + offsetof(struct drvIface, fw2xOffloads)
				+ 3) & ~3, MCP_AREA_CONFIG);
		if (ret) {
			atl_dev_err("Failed to upload sleep proxy info to FW\n");
			goto unlock_free;
		}
	}

	if (hw->mcp.caps_ex & atl_fw2_ex_caps_wol_ex) {
		ret = atl_write_fwsettings_word(hw, atl_fw2_setings_wol_ex, 
						wol_ex_flags);
		if (ret)
			goto unlock_free;
	}

	req_high |= wol_bits;
	req_high &= ~atl_fw2_link_drop;
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_LOW), low_req);
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), req_high);
	busy_wait(100, mdelay(1), val,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH)),
		(val & wol_bits) != wol_bits);

	ret = (val & wol_bits) == wol_bits ? 0 : -EIO;
	if (ret)
		atl_dev_err("Timeout waiting for WoL enable\n");

unlock_free:
	kfree(msg);
unlock:
	atl_unlock_fw(hw);
	return ret;
}

int atl_read_mcp_word(struct atl_hw *hw, uint32_t offt, uint32_t *val)
{
	int ret;

	ret = atl_read_mcp_mem(hw, offt & ~3, val, 4);
	if (ret)
		return ret;

	*val >>= 8 * (offt & 3);
	return 0;
}

/* fw lock must be held */
static int __atl_fw2_get_phy_temperature(struct atl_hw *hw, int *temp)
{
	uint32_t req, res;
	int ret = 0;

	if (test_bit(ATL_ST_RESETTING, &hw->state))
		return 0;

	hw->mcp.req_high ^= atl_fw2_phy_temp;
	req = hw->mcp.req_high;
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), req);

	busy_wait(1000, udelay(10), res,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH)),
		((res ^ req) & atl_fw2_phy_temp) != 0);
	if (((res ^ req) & atl_fw2_phy_temp) != 0) {
		atl_dev_err("Timeout waiting for PHY temperature\n");
		return -EIO;
	}

	ret = atl_read_fwstat_word(hw, atl_fw2_stat_temp, &res);
	if (ret)
		return ret;

	*temp = (int16_t)(res & 0xFFFF) * 1000 / 256;

	return ret;
}

static int atl_fw2_get_phy_temperature(struct atl_hw *hw, int *temp)
{
	int ret;

	atl_lock_fw(hw);
	ret = __atl_fw2_get_phy_temperature(hw, temp);
	atl_unlock_fw(hw);
	return ret;
}

static int atl_fw2_dump_cfg(struct atl_hw *hw)
{
	if (!hw->mcp.fw_cfg_dump)
		hw->mcp.fw_cfg_dump = devm_kzalloc(&hw->pdev->dev,
						   ATL_FW_CFG_DUMP_SIZE,
						   GFP_KERNEL);
	/* save link configuration */
	hw->mcp.fw_cfg_dump[0] =
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_LOW));
	hw->mcp.fw_cfg_dump[1] =
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH)) & 0x400F18;

	return 0;
}

static int atl_fw2_restore_cfg(struct atl_hw *hw)
{
	if (!hw->mcp.fw_cfg_dump)
		return 0;

	/* restore link configuration */
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_LOW),
		  hw->mcp.fw_cfg_dump[0]);
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH),
		  hw->mcp.fw_cfg_dump[1]);

	return 0;
}

static int atl_fw2_set_phy_loopback(struct atl_nic *nic, u32 mode)
{
	bool on = !!(nic->priv_flags & BIT(mode));
	struct atl_hw *hw = &nic->hw;

	atl_lock_fw(hw);

	switch (mode) {
	case ATL_PF_LPB_INT_PHY:
		atl_write_bit(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), 27, on);
		break;
	case ATL_PF_LPB_EXT_PHY:
		atl_write_bit(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), 26, on);
		break;
	}

	atl_unlock_fw(hw);

	return 0;
}

static int atl_fw2_update_statistics(struct atl_hw *hw)
{
	uint32_t req;
	int res = 0;

	hw->mcp.req_high ^= atl_fw2_statistics;
	req = hw->mcp.req_high;
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), req);

	busy_wait(10000, udelay(10), res,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH)),
		((res ^ req) & atl_fw2_statistics) != 0);
	if (((res ^ req) & atl_fw2_statistics) != 0) {
		atl_dev_err("Timeout waiting for statistics\n");
		return -EIO;
	}
	return 0;
}

static int atl_fw2_set_mediadetect(struct atl_hw *hw, bool on)
{
	int ret = 0;

	if (hw->mcp.fw_rev < 0x0301005a)
		return -EOPNOTSUPP;

	atl_lock_fw(hw);

	ret = atl_write_fwsettings_word(hw, atl_fw2_setings_media_detect, on);
	if (ret)
		goto unlock;

	/* request statistics just to force FW to read settings */
	ret =  atl_fw2_update_statistics(hw);
unlock:
	atl_unlock_fw(hw);
	return ret;
}

static int atl_fw2_set_downshift(struct atl_hw *hw, bool on)
{
	uint32_t req;
	int res = 0;
	int ret = 0;

	atl_lock_fw(hw);

	if (on)
		hw->mcp.req_high |= atl_fw2_downshift;
	else
		hw->mcp.req_high &= ~atl_fw2_downshift;
	req = hw->mcp.req_high;
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), req);

	busy_wait(10000, udelay(10), res,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH)),
		((res ^ req) & atl_fw2_downshift) != 0);
	if (((res ^ req) & atl_fw2_downshift) != 0) {
		atl_dev_err("Timeout waiting for statistics\n");
		ret = -EIO;
	}

	atl_unlock_fw(hw);
	return ret;
}

static int __atl_fw2x_apply_msm_settings(struct atl_hw *hw)
{

	uint32_t msg_id = atl_fw2_msm_settings_apply;
	uint32_t high_status, high_req = 0;
	int ret = 0;

	if (!(hw->mcp.caps_ex & atl_fw2_ex_caps_msm_settings_apply))
		return __atl_fw2_restart_aneg(hw);

	ret = atl_write_mcp_mem(hw, 0, &msg_id, sizeof(msg_id),
				MCP_AREA_CONFIG);
	if (ret) {
		atl_dev_err("Failed to upload macsec request: %d\n", ret);
		atl_unlock_fw(hw);
		return ret;
	}

	high_req = atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH));
	high_req ^= atl_fw2_fw_request;
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), high_req);

	busy_wait(1000, mdelay(1), high_status,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH)),
		((high_req ^ high_status) & atl_fw2_fw_request) != 0);
	if (((high_req ^ high_status) & atl_fw2_fw_request) != 0) {
		atl_dev_err("Timeout waiting for fw request\n");
		atl_unlock_fw(hw);
		return -EIO;
	}

	return ret;
}

static int atl_fw2_set_pad_stripping(struct atl_hw *hw, bool on)
{
	uint32_t msm_opts;
	int ret = 0;

	if (hw->mcp.fw_rev < 0x0300008e)
		return -EOPNOTSUPP;

	atl_lock_fw(hw);

	ret = atl_read_fwsettings_word(hw, atl_fw2_setings_msm_opts,
		&msm_opts);
	if (ret) {
		if (ret == -EINVAL)
			ret = -EOPNOTSUPP;

		goto unlock;
	}

	msm_opts &= ~atl_fw2_settings_msm_opts_strip_pad;
	if (on)
		msm_opts |= BIT(atl_fw2_settings_msm_opts_strip_pad_shift);

	ret = atl_write_fwsettings_word(hw, atl_fw2_setings_msm_opts,
		msm_opts);
	if (ret)
		goto unlock;

	ret = __atl_fw2x_apply_msm_settings(hw);
unlock:
	atl_unlock_fw(hw);
	return ret;
}

static int atl_fw2_send_macsec_request(struct atl_hw *hw,
				struct macsec_msg_fw_request *req,
				struct macsec_msg_fw_response *response)
{
	int ret = 0;
	uint32_t low_status, low_req = 0;

	if (!req || !response)
		return -EINVAL;

	if ((hw->mcp.caps_low & atl_fw2_macsec) == 0)
		return -EOPNOTSUPP;

	atl_lock_fw(hw);

	/* Write macsec request to cfg memory */
	ret = atl_write_mcp_mem(hw, 0, req, (sizeof(*req) + 3) & ~3,
				MCP_AREA_CONFIG);
	if (ret) {
		atl_dev_err("Failed to upload macsec request: %d\n", ret);
		atl_unlock_fw(hw);
		return ret;
	}

	/* Toggle 0x368.CAPS_LO_MACSEC bit */
	low_req = atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_LOW));
	low_req ^= atl_fw2_macsec;
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_LOW), low_req);

	busy_wait(1000, mdelay(1), low_status,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_LOW)),
		((low_req ^ low_status) & atl_fw2_macsec) != 0);
	if (((low_req ^ low_status) & atl_fw2_macsec) != 0) {
		atl_dev_err("Timeout waiting for macsec request\n");
		atl_unlock_fw(hw);
		return -EIO;
	}

	/* Read status of write operation */
	ret = atl_read_rpc_mem(hw, sizeof(u32), (u32 *)(void *)response,
			       sizeof(*response));

	atl_unlock_fw(hw);
	return ret;
}

/* fw lock must be held */
static int __atl_fw2_get_hbeat(struct atl_hw *hw, uint16_t *hbeat)
{
	int ret;
	uint32_t val;

	ret = atl_read_fwstat_word(hw, atl_fw2_stat_phy_hbeat, &val);
	if (ret)
		atl_dev_err("FW watchdog: failure reading PHY heartbeat: %d\n",
			-ret);
	else
		*hbeat = val & 0xffff;

	return ret;
}

static int atl_fw1_get_mac_addr(struct atl_hw *hw, uint8_t *buf)
{
	uint32_t efuse_shadow_addr =
		atl_read(hw, ATL_MCP_SCRATCH(FW1_EFUSE_SHADOW));
	uint8_t tmp[8];
	int ret;

	if (!efuse_shadow_addr)
		return false;

	ret = atl_read_mcp_mem(hw, efuse_shadow_addr + 40 * 4, tmp, 8);
	*(uint32_t *)buf = htonl(*(uint32_t *)tmp);
	*(uint16_t *)&buf[4] = (uint16_t)htonl(*(uint32_t *)&tmp[4]);

	return ret;
}

static int atl_fw2_get_mac_addr(struct atl_hw *hw, uint8_t *buf)
{
	uint32_t efuse_shadow_addr =
		atl_read(hw, ATL_MCP_SCRATCH(FW2_EFUSE_SHADOW));
	uint8_t tmp[8];
	int ret;

	if (!efuse_shadow_addr)
		return false;

	ret = atl_read_mcp_mem(hw, efuse_shadow_addr + 40 * 4, tmp, 8);
	*(uint32_t *)buf = htonl(*(uint32_t *)tmp);
	*(uint16_t *)&buf[4] = (uint16_t)htonl(*(uint32_t *)&tmp[4]);

	return ret;
}

/* fw lock must be held */
static int __atl_fw2_set_thermal_monitor(struct atl_hw *hw, bool enable)
{
	struct atl_mcp *mcp = &hw->mcp;
	int ret;
	uint32_t high;

	if (enable) {
		struct atl_fw2_thermal_cfg cfg __attribute__((__aligned__(4)));

		cfg.msg_id = 0x17;
		cfg.shutdown_temp = hw->thermal.crit;
		cfg.high_temp = hw->thermal.high;
		cfg.normal_temp = hw->thermal.low;

		ret = atl_write_mcp_mem(hw, 0, &cfg, (sizeof(cfg) + 3) & ~3,
			MCP_AREA_CONFIG);
		if (ret) {
			atl_dev_err("Failed to upload thermal thresholds to firmware: %d\n",
				ret);
			return ret;
		}

		mcp->req_high |= atl_fw2_set_thermal;
	} else
		mcp->req_high &= ~atl_fw2_set_thermal;

	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), mcp->req_high);
	busy_wait(1000, udelay(10), high,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH)),
		!!(high & atl_fw2_set_thermal) != enable);
	if (!!(high & atl_fw2_set_thermal) != enable) {
		atl_dev_err("Timeout waiting for thermal monitoring FW request\n");
		return -EIO;
	}

	return 0;
}

static int atl_fw2_update_thermal(struct atl_hw *hw)
{
	bool enable = !!(hw->thermal.flags & atl_thermal_monitor);
	struct atl_mcp *mcp = &hw->mcp;
	int ret = 0;
	bool alarm;

	if (!(mcp->caps_high & atl_fw2_set_thermal)) {
		if (hw->thermal.flags & atl_thermal_monitor)
			atl_dev_warn("Thermal monitoring not supported by firmware\n");
		hw->thermal.flags &=
			~(atl_thermal_monitor | atl_thermal_throttle);
		return 0;
	}

	atl_lock_fw(hw);

	if (!enable || (mcp->req_high & atl_fw2_set_thermal)) {
		/* If monitoring is on and we need to change the
		 * thresholds, we need to temporarily disable thermal
		 * monitoring first. */
		ret = __atl_fw2_set_thermal_monitor(hw, false);
		if (ret) {
			atl_unlock_fw(hw);
			return ret;
		}
	}

	if (enable)
		ret = __atl_fw2_set_thermal_monitor(hw, true);
	atl_unlock_fw(hw);

	/* Thresholds might have changed, recheck state. */
	alarm = !!(atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_LOW)) &
			atl_fw2_thermal_alarm);
	atl_thermal_check(hw, alarm);

	return ret;
}

static int atl_fw2_send_ptp_request(struct atl_hw *hw,
				    struct ptp_msg_fw_request *msg)
{
	u32 high_req, high_status;
	size_t size;
	int ret = 0;

	if (!msg)
		return -EINVAL;

	size = sizeof(msg->msg_id);
	switch (msg->msg_id) {
	case ptp_gpio_ctrl_msg:
		size += sizeof(msg->gpio_ctrl);
		break;
	case ptp_adj_freq_msg:
		size += sizeof(msg->adj_freq);
		break;
	case ptp_adj_clock_msg:
		size += sizeof(msg->adj_clock);
		break;
	default:
		return -EINVAL;
	}

	atl_lock_fw(hw);

	/* Write ptp request to cfg memory */
	ret = atl_write_mcp_mem(hw, 0, msg, (size + 3) & ~3, MCP_AREA_CONFIG);
	if (ret) {
		atl_dev_err("Failed to upload ptp request: %d\n", ret);
		goto err_exit;
	}

	high_req = atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH));
	high_req ^= atl_fw2_fw_request;
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), high_req);

	busy_wait(1000, mdelay(1), high_status,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH)),
		((high_req ^ high_status) & atl_fw2_fw_request) != 0);
	if (((high_req ^ high_status) & atl_fw2_fw_request) != 0) {
		atl_dev_err("Timeout waiting for fw request\n");
		ret = -EIO;
		goto err_exit;
	}

	/* Toggle statistics bit for FW to update */
	ret = atl_fw2_update_statistics(hw);

err_exit:
	atl_unlock_fw(hw);
	return ret;
}

static void atl_fw3_set_ptp(struct atl_hw *hw, bool on)
{
	u32 all_ptp_features = atl_fw2_ex_caps_phy_ptp_en | atl_fw2_ex_caps_ptp_gpio_en;
	u32 ptp_opts;

	atl_lock_fw(hw);
	ptp_opts = atl_read(hw, ATL_MCP_SCRATCH(FW3_EXT_RES));
	if (on)
		ptp_opts |= all_ptp_features;
	else
		ptp_opts &= ~all_ptp_features;

	atl_write(hw, ATL_MCP_SCRATCH(FW3_EXT_REQ), ptp_opts);
	atl_unlock_fw(hw);
}

static struct atl_fw_ops atl_fw_ops[2] = {
	[0] = {
		.__wait_fw_init = __atl_fw1_wait_fw_init,
		.set_link = atl_fw1_set_link,
		.check_link = atl_fw1_check_link,
		.__get_link_caps = __atl_fw1_get_link_caps,
		.restart_aneg = atl_fw1_unsupported,
		.set_default_link = atl_fw1_set_default_link,
		.enable_wol = atl_fw1_enable_wol,
		.get_phy_temperature = (void *)atl_fw1_unsupported,
		.dump_cfg = atl_fw1_unsupported,
		.restore_cfg = atl_fw1_unsupported,
		.set_phy_loopback = (void *)atl_fw1_unsupported,
		.set_mediadetect = (void *)atl_fw1_unsupported,
		.set_downshift = (void *)atl_fw1_unsupported,
		.send_macsec_req = (void *)atl_fw1_unsupported,
		.set_pad_stripping = (void *)atl_fw1_unsupported,
		.__get_hbeat = (void *)atl_fw1_unsupported,
		.get_mac_addr = atl_fw1_get_mac_addr,
		.update_thermal = atl_fw1_unsupported,
		.deinit = atl_fw1_unsupported,
	},
	[1] = {
		.__wait_fw_init = __atl_fw2_wait_fw_init,
		.set_link = atl_fw2_set_link,
		.check_link = atl_fw2_check_link,
		.__get_link_caps = __atl_fw2_get_link_caps,
		.restart_aneg = atl_fw2_restart_aneg,
		.set_default_link = atl_fw2_set_default_link,
		.enable_wol = atl_fw2_enable_wol,
		.get_phy_temperature = atl_fw2_get_phy_temperature,
		.dump_cfg = atl_fw2_dump_cfg,
		.restore_cfg = atl_fw2_restore_cfg,
		.set_phy_loopback = atl_fw2_set_phy_loopback,
		.set_mediadetect = atl_fw2_set_mediadetect,
		.set_downshift = atl_fw2_set_downshift,
		.send_macsec_req = atl_fw2_send_macsec_request,
		.set_pad_stripping = atl_fw2_set_pad_stripping,
		.__get_hbeat = __atl_fw2_get_hbeat,
		.get_mac_addr = atl_fw2_get_mac_addr,
		.update_thermal = atl_fw2_update_thermal,
		.send_ptp_req = atl_fw2_send_ptp_request,
		.set_ptp = atl_fw3_set_ptp,
		.deinit = atl_fw1_unsupported,
	},
};

struct atl_thermal_limit {
	uintptr_t offset;
	const char *name;
	unsigned min;
	unsigned max;
};
#define atl_def_thermal_limit(_name, _field, _min, _max)	\
{								\
	.offset = offsetof(struct atl_thermal, _field),		\
	.name = _name,						\
	.min = _min,						\
	.max = _max,						\
},

static struct atl_thermal_limit atl_thermal_limits[] = {
	atl_def_thermal_limit("Shutdown", crit, 108, 118)
	atl_def_thermal_limit("High", high, 90, 107)
	atl_def_thermal_limit("Normal", low, 50, 85)
};

int atl_verify_thermal_limits(struct atl_hw *hw, struct atl_thermal *thermal)
{
	int i;
	bool ignore = !!(thermal->flags & atl_thermal_ignore_lims);

	for (i = 0; i < ARRAY_SIZE(atl_thermal_limits); i++) {
		struct atl_thermal_limit *lim = &atl_thermal_limits[i];
		unsigned val = *((uint8_t *)thermal + lim->offset);

		if (val >= lim->min && val <= lim->max)
			continue;

		if (ignore) {
			atl_dev_init_warn("%s temperature threshold out of range (%d - %d): %d, allowing anyway\n",
				lim->name, lim->min, lim->max, val);
			continue;
		} else {
			atl_dev_init_err("%s temperature threshold out of range (%d - %d): %d\n",
				lim->name, lim->min, lim->max, val);
			return -EINVAL;
		}
	}

	return 0;
}

int atl_update_thermal(struct atl_hw *hw)
{
	int ret;

	ret = atl_verify_thermal_limits(hw, &hw->thermal);
	if (ret)
		return ret;

	if (test_bit(ATL_ST_RESETTING, &hw->state))
		/* After reset, atl_fw_init() will apply the settings
		 * skipped here */
		return 0;

	ret = hw->mcp.ops->update_thermal(hw);

	return ret;
}

int atl_update_thermal_flag(struct atl_hw *hw, int bit, bool val)
{
	struct atl_thermal *thermal = &hw->thermal;
	unsigned flags, changed;
	int ret = 0;

	flags = thermal->flags;

	switch (bit) {
	case atl_thermal_monitor_shift:
		if (!val)
			/* Disable throttling along with monitoring */
			flags &= ~atl_thermal_throttle;
		else
			if (!(hw->mcp.caps_high & atl_fw2_set_thermal)) {
				atl_dev_err("Thermal monitoring not supported by firmware\n");
				ret = -EINVAL;
			}
		break;

	case atl_thermal_throttle_shift:
		if (val && !(flags & atl_thermal_monitor)) {
			atl_dev_err("Thermal monitoring needs to be enabled before enabling throttling\n");
			ret = -EINVAL;
		}
		break;

	case atl_thermal_ignore_lims_shift:
		break;

	default:
		ret = -EINVAL;
		break;
	}
	if (ret)
		return ret;

	flags &= ~BIT(bit);
	flags |= val << bit;

	changed = flags ^ thermal->flags;
	thermal->flags = flags;

	if (test_bit(ATL_ST_RESETTING, &hw->state))
		/* After reset, atl_fw_init() will apply the settings
		 * skipped here */
		return ret;

	if (changed & atl_thermal_monitor) {
		ret = hw->mcp.ops->update_thermal(hw);
	} else if (changed & atl_thermal_throttle &&
		   hw->link_state.thermal_throttled)
		hw->mcp.ops->set_link(hw, true);

	if (ret)
		/* __atl_fw2_update_thermal() failed. Revert flag
		 * changes */
		thermal->flags ^= changed;

	return ret;
}

static unsigned int atl_wdog_period = 1100;
module_param_named(wdog_period, atl_wdog_period, uint, 0644);

int atl_fw_init(struct atl_hw *hw)
{
	uint32_t tries, reg, major;
	int ret;
	struct atl_mcp *mcp = &hw->mcp;

	tries = busy_wait(10000, mdelay(1), reg, atl_read(hw, 0x18), !reg);
	if (!reg) {
		atl_dev_err("Timeout waiting for FW version\n");
		return -EIO;
	}
	atl_dev_dbg("FW startup took %d ms\n", tries);

	major = (reg >> 24) & 0xff;
	if (!major || major > 3) {
		atl_dev_err("Unsupported FW major version: %u\n", major);
		return -EINVAL;
	}
	if (major > 2)
		major--;
	mcp->ops = &atl_fw_ops[major - 1];
	mcp->fw_rev = reg;

	ret = mcp->ops->__wait_fw_init(hw);
	if (ret)
		return ret;

	ret = mcp->ops->__get_hbeat(hw, &mcp->phy_hbeat);
	if (ret)
		return ret;
	mcp->next_wdog = jiffies + 2 * HZ;

	ret = mcp->ops->__get_link_caps(hw);

	return ret;
}

void atl_fw_watchdog(struct atl_hw *hw)
{
	struct atl_mcp *mcp = &hw->mcp;
	int ret;
	uint16_t hbeat;

	if (mcp->wdog_disabled || !time_after(jiffies, mcp->next_wdog))
		return;

	if (test_bit(ATL_ST_RESETTING, &hw->state) ||
	    !test_bit(ATL_ST_ENABLED, &hw->state))
		return;

	atl_lock_fw(hw);

	ret = mcp->ops->__get_hbeat(hw, &hbeat);
	if (ret) {
		atl_dev_err("FW watchdog: failure reading PHY heartbeat: %d\n",
			-ret);
		goto out;
	}

	if (hbeat == mcp->phy_hbeat) {
		atl_dev_err("FW watchdog: FW hang (PHY heartbeat stuck at %hd), resetting\n", hbeat);
		set_bit(ATL_ST_RESET_NEEDED, &hw->state);
	}

	mcp->phy_hbeat = hbeat;

out:
	mcp->next_wdog = jiffies + atl_wdog_period * HZ / 1000;
	atl_unlock_fw(hw);
}
