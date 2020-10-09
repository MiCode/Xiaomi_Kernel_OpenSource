/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2017 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_HW_H_
#define _ATL_HW_H_

#include <linux/compiler.h>
#include <linux/if_ether.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include "atl_fw.h"
#include "atl_macsec.h"
#include "atl_regs.h"

union atl_desc;
struct atl_nic;

#define PCI_VENDOR_ID_AQUANTIA 0x1d6a

/* clock is 3.2 ns*/
#define ATL_HW_CLOCK_TO_US(clk)  (clk * 32 / 10000)

#define ATL2_ACTION(ACTION, RSS, INDEX, VALID, TS_VALID) \
	((((ACTION & 0x3U) << 8) | \
	((RSS & 0x1U) << 7) | \
	((INDEX & 0x3FU) << 2) | \
	((TS_VALID & 0x1U) << 1)) | \
	((VALID & 0x1U) << 0))

#define ATL2_ACTION_DROP ATL2_ACTION(0, 0, 0, 1, 0)
#define ATL2_ACTION_DISABLE ATL2_ACTION(0, 0, 0, 0, 0)
#define ATL2_ACTION_ASSIGN_QUEUE(QUEUE) ATL2_ACTION(1, 0, (QUEUE), 1, 0)
#define ATL2_ACTION_ASSIGN_TC(TC) ATL2_ACTION(1, 1, (TC), 1, 0)

enum {
	ATL2_RPF_L2_PROMISC_OFF_INDEX = 0,
	ATL2_RPF_VLAN_PROMISC_OFF_INDEX,
	ATL2_RPF_L3L4_USER_INDEX,
	ATL2_RPF_ET_PCP_USER_INDEX = ATL2_RPF_L3L4_USER_INDEX + 16,
	ATL2_RPF_VLAN_USER_INDEX  = ATL2_RPF_ET_PCP_USER_INDEX + 16,
	ATL2_RPF_FLEX_USER_INDEX  = ATL2_RPF_VLAN_USER_INDEX + 16,
	ATL2_RPF_DEFAULT_RULE_INDEX  = ATL2_RPF_FLEX_USER_INDEX + 1,
};

#define ATL2_RPF_TAG_UC_OFFSET      0x0
#define ATL2_RPF_TAG_ALLMC_OFFSET   0x6
#define ATL2_RPF_TAG_ET_OFFSET      0x7
#define ATL2_RPF_TAG_VLAN_OFFSET    0xA
#define ATL2_RPF_TAG_UNTAG_OFFSET   0xE
#define ATL2_RPF_TAG_L3_V4_OFFSET   0xF
#define ATL2_RPF_TAG_L3_V6_OFFSET   0x12
#define ATL2_RPF_TAG_L4_OFFSET      0x15
#define ATL2_RPF_TAG_L4_FLEX_OFFSET 0x18
#define ATL2_RPF_TAG_FLEX_OFFSET    0x1B
#define ATL2_RPF_TAG_PCP_OFFSET     0x1D

#define ATL2_RPF_TAG_UC_MASK    (0x0000003F << ATL2_RPF_TAG_UC_OFFSET)
#define ATL2_RPF_TAG_ALLMC_MASK (0x00000001 << ATL2_RPF_TAG_ALLMC_OFFSET)
#define ATL2_RPF_TAG_UNTAG_MASK (0x00000001 << ATL2_RPF_TAG_UNTAG_OFFSET)
#define ATL2_RPF_TAG_VLAN_MASK  (0x0000000F << ATL2_RPF_TAG_VLAN_OFFSET)
#define ATL2_RPF_TAG_ET_MASK    (0x00000007 << ATL2_RPF_TAG_ET_OFFSET)
#define ATL2_RPF_TAG_L3_V4_MASK (0x00000007 << ATL2_RPF_TAG_L3_V4_OFFSET)
#define ATL2_RPF_TAG_L3_V6_MASK (0x00000007 << ATL2_RPF_TAG_L3_V6_OFFSET)
#define ATL2_RPF_TAG_L4_MASK    (0x00000007 << ATL2_RPF_TAG_L4_OFFSET)
#define ATL2_RPF_TAG_FLEX_MASK  (0x00000003 << ATL2_RPF_TAG_FLEX_OFFSET)
#define ATL2_RPF_TAG_PCP_MASK   (0x00000007 << ATL2_RPF_TAG_PCP_OFFSET)

#define ATL2_RPF_TAG_BASE_BC    (1 << ATL2_RPF_TAG_UC_OFFSET)
#define ATL2_RPF_TAG_BASE_UC    (2 << ATL2_RPF_TAG_UC_OFFSET)

#define ATL2_FW_HOSTLOAD_REQ_LEN_MAX 0x1000

#define busy_wait(tries, wait, lvalue, fetch, cond)	\
({							\
	uint32_t _dummy = 0;				\
	int i = (tries);				\
	int orig = i;					\
	(void)_dummy;					\
	do {						\
		wait;					\
		(lvalue) = (fetch);			\
	} while ((cond) && --i);			\
	(orig - i);					\
})

enum atl_chip {
	ATL_UNKNOWN,
	ATL_ATLANTIC,
	ATL_ANTIGUA,
};

struct atl_thermal {
	unsigned flags;
	uint8_t crit;
	uint8_t high;
	uint8_t low;
};

extern struct atl_thermal atl_def_thermal;

enum atl_nic_state {
	ATL_ST_ENABLED,
	ATL_ST_CONFIGURED,
	ATL_ST_RINGS_RUNNING,
	/* ATL_ST_FWD_RINGS_RUNNING, */
	ATL_ST_UP,
	ATL_ST_WORK_SCHED,
	ATL_ST_UPDATE_LINK,
	ATL_ST_RESETTING,
	ATL_ST_RESET_NEEDED,
	ATL_ST_GLOBAL_CONF_NEEDED,
	ATL_ST_START_NEEDED,
	ATL_ST_DETACHED,
};

#define ATL_WAKE_SUPPORTED (WAKE_MAGIC | WAKE_PHY)
struct atl_hw {
	atomic_t flags;
	uint8_t __iomem *regs;
	struct pci_dev *pdev;
	unsigned long state;
	enum atl_chip chip_id;
	u32 chip_rev;
	bool new_rpf;
	struct atl_link_state link_state;
	uint32_t lpi_timer;
	unsigned wol_mode;
	struct atl_mcp mcp;
	uint32_t non_ring_intr_mask;
	uint8_t mac_addr[ETH_ALEN];
#define ATL_RSS_KEY_SIZE 40
	uint8_t rss_key[ATL_RSS_KEY_SIZE];
#define ATL_RSS_TBL_SIZE (1 << 6)
	uint8_t rss_tbl[ATL_RSS_TBL_SIZE];
	struct atl_thermal thermal;
#if IS_ENABLED(CONFIG_MACSEC) && defined(NETIF_F_HW_MACSEC)
	struct atl_macsec_cfg macsec_cfg;
#endif
	s64 ptp_clk_offset;
	int art_base_index;
	int art_available;
};

struct atl_hw_ring {
	union atl_desc *descs;
	uint32_t size;
	uint32_t reg_base;
	dma_addr_t daddr;
};

enum mcp_area {
	MCP_AREA_CONFIG = 0x80000000,
	MCP_AREA_SETTINGS = 0x20000000,
};

#define offset_ptr(ptr, ring, amount)					\
	({								\
		struct atl_hw_ring *hw_ring = (ring);			\
		uint32_t size = hw_ring->size;				\
									\
		uint32_t res = (ptr) + (amount);			\
		if ((int32_t)res < 0)					\
			res += size;					\
		else if (res >= size)					\
			res -= size;					\
		res;							\
	})

void atl_check_unplug(struct atl_hw *hw, uint32_t addr);

static inline uint32_t atl_read(struct atl_hw *hw, uint32_t addr)
{
	uint8_t __iomem *base = READ_ONCE(hw->regs);
	uint32_t val = 0xffffffff;

	if (unlikely(!base))
		return val;

	val = readl(base + addr);
	if (unlikely(val == 0xffffffff))
		atl_check_unplug(hw, addr);
	return val;
}

static inline void atl_write(struct atl_hw *hw, uint32_t addr, uint32_t val)
{
	uint8_t __iomem *base = READ_ONCE(hw->regs);

	if (unlikely(!base))
		return;

	writel(val, base + addr);
}


static inline void atl_write_mask_bits(struct atl_hw *hw, uint32_t addr,
			     uint32_t mask, uint32_t val)
{
	atl_write(hw, addr,
		  (atl_read(hw, addr) & ~mask) | (val & mask));
}

static inline void atl_write_bits(struct atl_hw *hw, uint32_t addr,
			     uint32_t shift, uint32_t width, uint32_t val)
{
	uint32_t mask = ((1u << width) - 1) << shift;

	atl_write(hw, addr,
		  (atl_read(hw, addr) & ~mask) | ((val << shift) & mask));
}

static inline void atl_write_bit(struct atl_hw *hw, uint32_t addr,
			    uint32_t shift, uint32_t val)
{
	atl_write_bits(hw, addr, shift, 1, val);
}

static inline void atl_set_bits(struct atl_hw *hw, uint32_t addr,
	uint32_t bits)
{
	atl_write(hw, addr, atl_read(hw, addr) | bits);
}

static inline void atl_clear_bits(struct atl_hw *hw, uint32_t addr,
	uint32_t bits)
{
	atl_write(hw, addr, atl_read(hw, addr) & ~bits);
}

static inline void atl_intr_enable(struct atl_hw *hw, uint32_t mask)
{
	atl_write(hw, ATL_INTR_MSK_SET, mask);
}

static inline void atl_intr_disable(struct atl_hw *hw, uint32_t mask)
{
	atl_write(hw, ATL_INTR_MSK_CLEAR, mask);
}

static inline void atl_intr_disable_all(struct atl_hw *hw)
{
	atl_intr_disable(hw, 0xffffffff);
}

static inline unsigned atl_fw_major(struct atl_hw *hw)
{
	return (hw->mcp.fw_rev >> 24) & 0xff;
}

static inline void atl_init_rss_table(struct atl_hw *hw, int nvecs)
{
	int i;

	for (i = 0; i < ATL_RSS_TBL_SIZE; i++)
		hw->rss_tbl[i] = i % nvecs;
}

int atl2_act_rslvr_table_set(struct atl_hw *hw, u8 location,
			     u32 tag, u32 mask, u32 action);
static inline void atl2_rpf_vlan_flr_tag_set(struct atl_hw *hw, u32 tag,
					     u32 filter)
{
	atl_write_bits(hw, ATL_RX_VLAN_FLT(filter), 12, 4, tag);
}

static inline void atl2_rpf_etht_flr_tag_set(struct atl_hw *hw, u32 tag,
					     u32 filter)
{
	atl_write_bits(hw, ATL2_RX_ETYPE_TAG(filter), 0, 3, tag);
}

static inline void atl2_rpf_l3_v4_da_set(struct atl_hw *hw, u32 filter, u32 val)
{
	u32 dword = filter % 4;
	u32 addr_set = 6 + ((filter < 4) ? (0) :  (1));

	atl_write(hw, ATL2_RPF_L3_DA(addr_set) + 4 * dword, swab32(val));
}

static inline void atl2_rpf_l3_v4_sa_set(struct atl_hw *hw, u32 filter, u32 val)
{
	u32 dword = filter % 4;
	u32 addr_set = 6 + ((filter < 4) ? (0) :  (1));

	atl_write(hw, ATL2_RPF_L3_SA(addr_set) + 4 * dword, swab32(val));
}

static inline void atl2_rpf_l3_v6_sa_set(struct atl_hw *hw, u32 filter,
					 u32 val[4])
{
	int i;

	for (i = 0; i < 4; i++)
		atl_write(hw, ATL2_RPF_L3_SA(filter) + 4 * i, swab32(val[i]));
}

static inline void atl2_rpf_l3_v6_da_set(struct atl_hw *hw, u32 filter,
					 u32 val[4])
{
	int i;

	for (i = 0; i < 4; i++)
		atl_write(hw, ATL2_RPF_L3_DA(filter) + 4 * i, swab32(val[i]));
}

static inline void atl2_rpf_flex_flr_tag_set(struct atl_hw *hw, u32 tag,
					     u32 filter)
{
	atl_write_bits(hw, ATL_RX_FLEX_FLT_CTRL(filter), 0x19, 2, tag);
}

int atl_read_mcp_mem(struct atl_hw *hw, uint32_t mcp_addr, void *host_addr,
	unsigned size);
int atl_hwinit(struct atl_hw *hw, enum atl_chip chip_id);
void atl_refresh_link(struct atl_nic *nic);
void atl_set_rss_key(struct atl_hw *hw);
int atl_set_rss_tbl(struct atl_hw *hw);
void atl_set_uc_flt(struct atl_hw *hw, int idx, uint8_t mac_addr[ETH_ALEN]);

int atl_alloc_descs(struct atl_nic *nic, struct atl_hw_ring *ring, size_t extra);
void atl_free_descs(struct atl_nic *nic, struct atl_hw_ring *ring, size_t extra);
void atl_set_intr_bits(struct atl_hw *hw, int idx, int rxbit, int txbit);
int atl_alloc_link_intr(struct atl_nic *nic);
void atl_free_link_intr(struct atl_nic *nic);
int atl_write_mcp_mem(struct atl_hw *hw, uint32_t offt, void *addr,
	size_t size, enum mcp_area area);

static inline int atl_write_fwcfg_word(struct atl_hw *hw, uint32_t offt,
	uint32_t val)
{
	return atl_write_mcp_mem(hw, offt, &val, sizeof(val), MCP_AREA_CONFIG);
}

static inline int atl_write_fwsettings_word(struct atl_hw *hw, uint32_t offt,
	uint32_t val)
{
	return atl_write_mcp_mem(hw, offt, &val, sizeof(val), MCP_AREA_SETTINGS);
}

static inline int atl_read_fwstat_word(struct atl_hw *hw, uint32_t offt,
	uint32_t *val)
{
	return atl_read_mcp_word(hw, offt + hw->mcp.fw_stat_addr, val);
}

static inline int atl_read_rpc_mem(struct atl_hw *hw, uint32_t offt,
				   uint32_t *val, size_t length)
{
	return atl_read_mcp_mem(hw, offt + hw->mcp.rpc_addr, val, length);
}

static inline int atl_read_fwsettings_word(struct atl_hw *hw, uint32_t offt,
	uint32_t *val)
{
	if (offt >= hw->mcp.fw_settings_len)
		return -EINVAL;

	return atl_read_mcp_word(hw, offt + hw->mcp.fw_settings_addr, val);
}

static inline void atl_lock_fw(struct atl_hw *hw)
{
	mutex_lock(&hw->mcp.lock);
}

static inline void atl_unlock_fw(struct atl_hw *hw)
{
	mutex_unlock(&hw->mcp.lock);
}

void atl_fw_watchdog(struct atl_hw *hw);

#endif
