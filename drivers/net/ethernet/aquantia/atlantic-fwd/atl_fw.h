/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_FW_H_
#define _ATL_FW_H_

struct atl_hw;

struct atl_link_type {
	unsigned speed;
	unsigned ethtool_idx;
	uint32_t fw_bits[2];
	const char *name;
};

extern struct atl_link_type atl_link_types[];
extern const int atl_num_rates;

#define atl_for_each_rate(idx, type)		\
	for (idx = 0, type = atl_link_types;	\
	     idx < atl_num_rates;		\
	     idx++, type++)

#define atl_define_bit(_name, _bit)		\
	_name ## _shift = (_bit),		\
	_name = BIT(_name ## _shift),

enum atl_fw2_opts {
	atl_define_bit(atl_fw2_pause, 3)
	atl_define_bit(atl_fw2_asym_pause, 4)
	atl_fw2_pause_mask = atl_fw2_pause | atl_fw2_asym_pause,
	atl_define_bit(atl_fw2_phy_temp, 18)
	atl_define_bit(atl_fw2_nic_proxy, 0x17)
	atl_define_bit(atl_fw2_wol, 0x18)
};

enum atl_fw2_stat_offt {
	atl_fw2_stat_temp = 0x50,
	atl_fw2_stat_lcaps = 0x84,
};

enum atl_fc_mode {
	atl_fc_none = 0,
	atl_define_bit(atl_fc_rx, 0)
	atl_define_bit(atl_fc_tx, 1)
	atl_fc_full = atl_fc_rx | atl_fc_tx,
};

struct atl_fc_state {
	enum atl_fc_mode req;
	enum atl_fc_mode prev_req;
	enum atl_fc_mode cur;
};

#define ATL_EEE_BIT_OFFT 16
#define ATL_EEE_MASK ~(BIT(ATL_EEE_BIT_OFFT) - 1)

struct atl_link_state{
	/* The following three bitmaps use alt_link_types[] indices
	 * as link bit positions. Conversion to/from ethtool bits is
	 * done in atl_ethtool.c. */
	unsigned supported;
	unsigned advertized;
	unsigned lp_advertized;
	unsigned prev_advertized;
	bool force_off;
	bool autoneg;
	bool eee;
	bool eee_enabled;
	struct atl_link_type *link;
	struct atl_fc_state fc;
};

struct atl_fw_ops {
	void (*set_link)(struct atl_hw *hw, bool force);
	struct atl_link_type *(*check_link)(struct atl_hw *hw);
	int (*wait_fw_init)(struct atl_hw *hw);
	int (*get_link_caps)(struct atl_hw *hw);
	int (*restart_aneg)(struct atl_hw *hw);
	void (*set_default_link)(struct atl_hw *hw);
	int (*enable_wol)(struct atl_hw *hw);
	int (*get_phy_temperature)(struct atl_hw *hw, int *temp);
	unsigned efuse_shadow_addr_reg;
};

int atl_read_fwstat_word(struct atl_hw *hw, uint32_t offt, uint32_t *val);

#endif
