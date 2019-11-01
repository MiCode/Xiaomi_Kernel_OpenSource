/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_HW_H_
#define _ATL_HW_H_

#include <linux/compiler.h>
#include <linux/if_ether.h>
#include <linux/mutex.h>
#include <linux/types.h>
#ifdef NETIF_F_HW_MACSEC
#include <net/macsec.h>
#endif

#include "atl_fw.h"
#include "atl_regs.h"

union atl_desc;
struct atl_nic;

#define PCI_VENDOR_ID_AQUANTIA 0x1d6a

/* clock is 3.2 ns*/
#define ATL_HW_CLOCK_TO_US(clk)  (clk * 32 / 10000)

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

enum atl_board {
	ATL_UNKNOWN,
	ATL_AQC107,
	ATL_AQC108,
	ATL_AQC109,
	ATL_AQC100,
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
#ifdef NETIF_F_HW_MACSEC
#define ATL_MACSEC_MAX_SC 32
#define ATL_MACSEC_MAX_SA 32
enum atl_macsec_sc_sa {
	atl_macsec_sa_sc_4sa_8sc,
	atl_macsec_sa_sc_not_used,
	atl_macsec_sa_sc_2sa_16sc,
	atl_macsec_sa_sc_1sa_32sc,
};

struct atl_macsec_common_stats {
	/* Ingress Common Counters */
	struct {
		uint64_t ctl_pkts;
		uint64_t tagged_miss_pkts;
		uint64_t untagged_miss_pkts;
		uint64_t notag_pkts;
		uint64_t untagged_pkts;
		uint64_t bad_tag_pkts;
		uint64_t no_sci_pkts;
		uint64_t unknown_sci_pkts;
		uint64_t ctrl_prt_pass_pkts;
		uint64_t unctrl_prt_pass_pkts;
		uint64_t ctrl_prt_fail_pkts;
		uint64_t unctrl_prt_fail_pkts;
		uint64_t too_long_pkts;
		uint64_t igpoc_ctl_pkts;
		uint64_t ecc_error_pkts;
		uint64_t unctrl_hit_drop_redir;
	} in;

	/* Egress Common Counters */
	struct {
		uint64_t ctl_pkts;
		uint64_t unknown_sa_pkts;
		uint64_t untagged_pkts;
		uint64_t too_long;
		uint64_t ecc_error_pkts;
		uint64_t unctrl_hit_drop_redir;
	} out;
};

    /* Ingress SA Counters */
struct atl_macsec_rx_sa_stats {
    uint64_t untagged_hit_pkts;
    uint64_t ctrl_hit_drop_redir_pkts;
    uint64_t not_using_sa;
    uint64_t unused_sa;
    uint64_t not_valid_pkts;
    uint64_t invalid_pkts;
    uint64_t ok_pkts;
    uint64_t late_pkts;
    uint64_t delayed_pkts;
    uint64_t unchecked_pkts;
    uint64_t validated_octets;
    uint64_t decrypted_octets;
};

    /* Egress SA Counters */
struct atl_macsec_tx_sa_stats {
    uint64_t sa_hit_drop_redirect;
    uint64_t sa_protected2_pkts;
    uint64_t sa_protected_pkts;
    uint64_t sa_encrypted_pkts;
};

    /* Egress SC Counters */
struct atl_macsec_tx_sc_stats {
    uint64_t sc_protected_pkts;
    uint64_t sc_encrypted_pkts;
    uint64_t sc_protected_octets;
    uint64_t sc_encrypted_octets;
};

struct atl_macsec_cfg {
	enum atl_macsec_sc_sa sc_sa;
	/* Egress channel configuration */
	unsigned long txsc_idx_busy;
	struct atl_macsec_txsc {
		uint32_t hw_sc_idx;
		unsigned long tx_sa_idx_busy;
		const struct macsec_secy *sw_secy;
		/* It is not OK to store key in driver but it is until ... */
		u8 tx_sa_key[MACSEC_NUM_AN][MACSEC_KEYID_LEN];
		struct atl_macsec_tx_sc_stats stats;
		struct atl_macsec_tx_sa_stats tx_sa_stats[MACSEC_NUM_AN];
	} atl_txsc[ATL_MACSEC_MAX_SC];
	/* Ingress channel configuration */
	unsigned long rxsc_idx_busy;
	struct atl_macsec_rxsc {
		uint32_t hw_sc_idx;
		unsigned long rx_sa_idx_busy;
		const struct macsec_secy *sw_secy;
		const struct macsec_rx_sc *sw_rxsc;
		/* TODO: we shouldn't store keys in the driver */
		u8 rx_sa_key[MACSEC_NUM_AN][MACSEC_KEYID_LEN];
		struct atl_macsec_rx_sa_stats rx_sa_stats[MACSEC_NUM_AN];
	} atl_rxsc[ATL_MACSEC_MAX_SC];
	struct atl_macsec_common_stats stats;
};
#endif

#define ATL_WAKE_SUPPORTED (WAKE_MAGIC | WAKE_PHY)
struct atl_hw {
	uint8_t __iomem *regs;
	struct pci_dev *pdev;
	unsigned long state;
	struct atl_link_state link_state;
	unsigned wol_mode;
	struct atl_mcp mcp;
	uint32_t non_ring_intr_mask;
	uint8_t mac_addr[ETH_ALEN];
#define ATL_RSS_KEY_SIZE 40
	uint8_t rss_key[ATL_RSS_KEY_SIZE];
#define ATL_RSS_TBL_SIZE (1 << 6)
	uint8_t rss_tbl[ATL_RSS_TBL_SIZE];
	struct atl_thermal thermal;
#define ATL_FW_CFG_DUMP_SIZE 2
	uint32_t fw_cfg_dump[ATL_FW_CFG_DUMP_SIZE];
#ifdef NETIF_F_HW_MACSEC
	struct atl_macsec_cfg macsec_cfg;
#endif
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

static inline void atl_set_vlan_promisc(struct atl_hw *hw, int promisc)
{
	atl_write_bit(hw, ATL_RX_VLAN_FLT_CTRL1, 1, !!promisc);
}

int atl_read_mcp_mem(struct atl_hw *hw, uint32_t mcp_addr, void *host_addr,
	unsigned size);
int atl_hwinit(struct atl_hw *hw, enum atl_board brd_id);
void atl_refresh_link(struct atl_nic *nic);
void atl_set_rss_key(struct atl_hw *hw);
void atl_set_rss_tbl(struct atl_hw *hw);
void atl_set_uc_flt(struct atl_hw *hw, int idx, uint8_t mac_addr[ETH_ALEN]);

int atl_alloc_descs(struct atl_nic *nic, struct atl_hw_ring *ring);
void atl_free_descs(struct atl_nic *nic, struct atl_hw_ring *ring);
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
