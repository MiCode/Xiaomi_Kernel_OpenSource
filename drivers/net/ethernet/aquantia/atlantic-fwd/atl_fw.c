/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */
#include "atl_common.h"
#include "atl_hw.h"
#include "atl_drviface.h"

struct atl_link_type atl_link_types[] = {
#define LINK_TYPE(_name, _speed, _ethtl_idx, _fw1_bit, _fw2_bit)	\
	{								\
		.name = _name,						\
		.speed = _speed,					\
		.ethtool_idx = _ethtl_idx,				\
		.fw_bits = {						\
		[0] = _fw1_bit,						\
		[1] = _fw2_bit,						\
		},							\
	},

	LINK_TYPE("100BaseTX-FD", 100, ETHTOOL_LINK_MODE_100baseT_Full_BIT,
		0x20, 1 << 5)
	LINK_TYPE("1000BaseT-FD", 1000, ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
		0x10, 1 << 8)
	LINK_TYPE("2.5GBaseT-FD", 2500, ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
		8, 1 << 9)
	LINK_TYPE("5GBaseT-FD", 5000, ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
		2, 1 << 10)
	LINK_TYPE("10GBaseT-FD", 10000, ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
		1, 1 << 11)
};

const int atl_num_rates = ARRAY_SIZE(atl_link_types);

static inline void atl_lock_fw(struct atl_hw *hw)
{
	mutex_lock(&hw->mcp.lock);
}

static inline void atl_unlock_fw(struct atl_hw *hw)
{
	mutex_unlock(&hw->mcp.lock);
}

static int atl_fw1_wait_fw_init(struct atl_hw *hw)
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

static int atl_fw2_wait_fw_init(struct atl_hw *hw)
{
	uint32_t reg;

	busy_wait(1000, mdelay(1), reg, atl_read(hw, ATL_GLOBAL_FW_IMAGE_ID),
		!reg);
	if (!reg)
		return -EIO;
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
	uint32_t low;
	uint32_t high;
	enum atl_fc_mode fc = atl_fc_none;

	atl_lock_fw(hw);

	low = atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_LOW));
	high = atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH));

	link = atl_parse_fw_bits(hw, low, high, 1);
	if (!link)
		goto unlock;

	if (high & atl_fw2_pause)
		fc |= atl_fc_rx;
	if (high & atl_fw2_asym_pause)
		fc |= atl_fc_tx;

	lstate->fc.cur = fc;

unlock:
	atl_unlock_fw(hw);
	return link;
}

static int atl_fw1_get_link_caps(struct atl_hw *hw)
{
	return 0;
}

static int atl_fw2_get_link_caps(struct atl_hw *hw)
{
	uint32_t fw_stat_addr = hw->mcp.fw_stat_addr;
	unsigned int supported = 0;
	uint32_t caps[2];
	int i, ret;

	atl_lock_fw(hw);

	atl_dev_dbg("Host data struct addr: %#x\n", fw_stat_addr);
	ret = atl_read_mcp_mem(hw, fw_stat_addr + atl_fw2_stat_lcaps,
		caps, 8);
	if (ret)
		goto unlock;

	for (i = 0; i < atl_num_rates; i++)
		if (atl_link_types[i].fw_bits[1] & caps[0]) {
			supported |= BIT(i);
			if (atl_link_types[i].fw_bits[1] & caps[1])
				supported |= BIT(i + ATL_EEE_BIT_OFFT);
		}

	hw->link_state.supported = supported;

unlock:
	atl_unlock_fw(hw);
	return ret;
}

static inline unsigned int atl_link_adv(struct atl_link_state *lstate)
{
	return lstate->force_off ? 0 : lstate->advertized;
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

static void atl_fw2_set_link(struct atl_hw *hw, bool force)
{
	struct atl_link_state *lstate = &hw->link_state;
	uint32_t hi_bits = 0;
	uint64_t bits;

	if (!force && !atl_fw2_set_link_needed(lstate))
		return;

	atl_lock_fw(hw);

	if (lstate->fc.req & atl_fc_rx)
		hi_bits |= atl_fw2_pause | atl_fw2_asym_pause;

	if (lstate->fc.req & atl_fc_tx)
		hi_bits ^= atl_fw2_asym_pause;

	bits = atl_set_fw_bits(hw, 1);

	hi_bits |= bits >> 32;

	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_LOW), bits);
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), hi_bits);

	atl_unlock_fw(hw);
}

static int atl_fw1_unsupported(struct atl_hw *hw)
{
	return -EOPNOTSUPP;
}

static int atl_fw2_restart_aneg(struct atl_hw *hw)
{
	atl_lock_fw(hw);
	atl_set_bits(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), BIT(31));
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
	lstate->eee_enabled = 1;
}

static int atl_fw2_enable_wol(struct atl_hw *hw)
{
	int ret;
	struct offloadInfo *info;
	struct drvIface *msg;
	uint32_t val, wol_bits = atl_fw2_nic_proxy | atl_fw2_wol;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	info = &msg->fw2xOffloads;
	info->version = 0;
	info->len = sizeof(*info);
	memcpy(info->macAddr, hw->mac_addr, ETH_ALEN);

	atl_lock_fw(hw);

	ret = atl_write_mcp_mem(hw, 0, msg,
		(info->len + offsetof(struct drvIface, fw2xOffloads) + 3) & ~3);
	if (ret) {
		atl_dev_err("Failed to upload sleep proxy info to FW\n");
		goto free;
	}

	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_LOW), 0);
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), wol_bits);
	busy_wait(100, mdelay(1), val,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH)),
		(val & wol_bits) != wol_bits);

	ret = (val & wol_bits) == wol_bits ? 0 : -EIO;
	if (ret)
		atl_dev_err("Timeout waiting for WoL enable\n");

free:
	atl_unlock_fw(hw);
	kfree(msg);
	return ret;
}

int atl_read_fwstat_word(struct atl_hw *hw, uint32_t offt, uint32_t *val)
{
	int ret;
	uint32_t addr = hw->mcp.fw_stat_addr + (offt & ~3);

	ret = atl_read_mcp_mem(hw, addr, val, 4);
	if (ret)
		return ret;

	*val >>= 8 * (offt & 3);
	return 0;
}

static int atl_fw2_get_phy_temperature(struct atl_hw *hw, int *temp)
{
	uint32_t req, res;
	int ret = 0;

	atl_lock_fw(hw);

	req = atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH));
	req ^= atl_fw2_phy_temp;
	atl_write(hw, ATL_MCP_SCRATCH(FW2_LINK_REQ_HIGH), req);

	busy_wait(1000, udelay(10), res,
		atl_read(hw, ATL_MCP_SCRATCH(FW2_LINK_RES_HIGH)),
		((res ^ req) & atl_fw2_phy_temp) != 0);
	if (((res ^ req) & atl_fw2_phy_temp) != 0) {
		atl_dev_err("Timeout waiting for PHY temperature\n");
		ret = -EIO;
		goto unlock;
	}

	ret = atl_read_fwstat_word(hw, atl_fw2_stat_temp, &res);
	if (ret)
		goto unlock;

	*temp = (res & 0xffff) * 1000 / 256;

unlock:
	atl_unlock_fw(hw);
	return ret;
}

static struct atl_fw_ops atl_fw_ops[2] = {
	[0] = {
		.wait_fw_init = atl_fw1_wait_fw_init,
		.set_link = atl_fw1_set_link,
		.check_link = atl_fw1_check_link,
		.get_link_caps = atl_fw1_get_link_caps,
		.restart_aneg = atl_fw1_unsupported,
		.set_default_link = atl_fw1_set_default_link,
		.enable_wol = atl_fw1_unsupported,
		.get_phy_temperature = (void *)atl_fw1_unsupported,
		.efuse_shadow_addr_reg = ATL_MCP_SCRATCH(FW1_EFUSE_SHADOW),
	},
	[1] = {
		.wait_fw_init = atl_fw2_wait_fw_init,
		.set_link = atl_fw2_set_link,
		.check_link = atl_fw2_check_link,
		.get_link_caps = atl_fw2_get_link_caps,
		.restart_aneg = atl_fw2_restart_aneg,
		.set_default_link = atl_fw2_set_default_link,
		.enable_wol = atl_fw2_enable_wol,
		.get_phy_temperature = atl_fw2_get_phy_temperature,
		.efuse_shadow_addr_reg = ATL_MCP_SCRATCH(FW2_EFUSE_SHADOW),
	},
};

int atl_fw_init(struct atl_hw *hw)
{
	uint32_t tries, reg, major;

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
	hw->mcp.ops = &atl_fw_ops[major - 1];
	hw->mcp.poll_link = major == 1;
	hw->mcp.fw_rev = reg;
	hw->mcp.fw_stat_addr = atl_read(hw, ATL_MCP_SCRATCH(FW_STAT_STRUCT));

	return hw->mcp.ops->wait_fw_init(hw);
}
