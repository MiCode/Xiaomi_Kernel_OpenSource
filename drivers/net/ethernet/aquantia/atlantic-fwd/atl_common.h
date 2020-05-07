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

#ifndef _ATL_COMMON_H_
#define _ATL_COMMON_H_

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/moduleparam.h>

#define ATL_VERSION "1.1.6"

struct atl_nic;

#include "atl_compat.h"
#include "atl_hw.h"
#include "atl_log.h"
#include "atl_ring_desc.h"
#include "atl_stats.h"

#define ATL_MAX_QUEUES 8

#include "atl_fwd.h"

enum {
	ATL_RXF_VLAN_BASE = 0,
	ATL_RXF_VLAN_MAX = ATL_VLAN_FLT_NUM,
	ATL_RXF_ETYPE_BASE = ATL_RXF_VLAN_BASE + ATL_RXF_VLAN_MAX,
	ATL_RXF_ETYPE_MAX = ATL_ETYPE_FLT_NUM,
	/* + 1 is for backward compatibility */
	ATL_RXF_NTUPLE_BASE = ATL_RXF_ETYPE_BASE + ATL_RXF_ETYPE_MAX + 1,
	ATL_RXF_NTUPLE_MAX = ATL_NTUPLE_FLT_NUM,
	ATL_RXF_FLEX_BASE = ATL_RXF_NTUPLE_BASE + ATL_RXF_NTUPLE_MAX,
	ATL_RXF_FLEX_MAX = 1,
};

enum atl_rxf_common_cmd {
	ATL_RXF_EN = BIT(31),
	ATL_RXF_RXQ_MSK = BIT(5) - 1,
	ATL_RXF_ACT_SHIFT = 16,
	ATL_RXF_ACT_MASK = BIT(3) - 1,
	ATL_RXF_ACT_TOHOST = BIT(0) << ATL_RXF_ACT_SHIFT,
};

enum atl_ntuple_cmd {
	ATL_NTC_EN = ATL_RXF_EN, /* Filter enabled */
	ATL_NTC_V6 = BIT(30),	/* IPv6 mode -- only valid in filters
				 * 0 and 4 */
	ATL_NTC_SA = BIT(29),	/* Match source address */
	ATL_NTC_DA = BIT(28),	/* Match destination address */
	ATL_NTC_SP = BIT(27),	/* Match source port */
	ATL_NTC_DP = BIT(26),	/* Match destination port */
	ATL_NTC_PROTO = BIT(25), /* Match L4 proto */
	ATL_NTC_ARP = BIT(24),
	ATL_NTC_RXQ = BIT(23),	/* Assign Rx queue */
	ATL_NTC_ACT_SHIFT = ATL_RXF_ACT_SHIFT,
	ATL_NTC_RXQ_SHIFT = 8,
	ATL_NTC_RXQ_MASK = ATL_RXF_RXQ_MSK << ATL_NTC_RXQ_SHIFT,
	ATL_NTC_L4_MASK = BIT(3) - 1,
	ATL_NTC_L4_TCP = 0,
	ATL_NTC_L4_UDP = 1,
	ATL_NTC_L4_SCTP = 2,
	ATL_NTC_L4_ICMP = 3,
};

enum atl2_ntuple_cmd {
	ATL2_NTC_L3_IPV4_EN = BIT(0), /* Filter enabled */
	ATL2_NTC_L3_IPV4_SA = BIT(1),
	ATL2_NTC_L3_IPV4_DA = BIT(2),
	ATL2_NTC_L3_IPV4_PROTO = BIT(3),
	ATL2_NTC_L3_IPV4_TAG_SHIFT = 4,
	ATL2_NTC_L3_IPV4_PROTO_SHIFT = 8,

	ATL2_NTC_L3_IPV6_EN = BIT(0x10), /* Filter enabled */
	ATL2_NTC_L3_IPV6_SA = BIT(0x11),
	ATL2_NTC_L3_IPV6_DA = BIT(0x12),
	ATL2_NTC_L3_IPV6_PROTO = BIT(0x13),
	ATL2_NTC_L3_IPV6_TAG_SHIFT = 0x14,
	ATL2_NTC_L3_IPV6_PROTO_SHIFT = 0x18,

	ATL2_NTC_L4_EN = BIT(0), /* Filter enabled */
	ATL2_NTC_L4_DP = BIT(1),
	ATL2_NTC_L4_SP = BIT(2),
};

struct atl2_rxf_l3 {
	union {
		struct {
			__be32 dst_ip4;
			__be32 src_ip4;
		};
		struct {
			__be32 dst_ip6[4];
			__be32 src_ip6[4];
		};
	};
	u16 proto;
	u32 cmd;
	u16 usage;
};

struct atl2_rxf_l4 {
	__be16 dst_port;
	__be16 src_port;
	u32 cmd;
	u16 usage;
};

struct atl_rxf_ntuple {
	union {
		struct {
			__be32 dst_ip4[ATL_RXF_NTUPLE_MAX];
			__be32 src_ip4[ATL_RXF_NTUPLE_MAX];
		};
		struct {
			__be32 dst_ip6[ATL_RXF_NTUPLE_MAX][4];
			__be32 src_ip6[ATL_RXF_NTUPLE_MAX][4];
		};
	};
	__be16 dst_port[ATL_RXF_NTUPLE_MAX];
	__be16 src_port[ATL_RXF_NTUPLE_MAX];

	struct atl2_rxf_l3 l3[ATL_RXF_NTUPLE_MAX];
	struct atl2_rxf_l4 l4[ATL_RXF_NTUPLE_MAX];
	s8 l3_idx[ATL_RXF_NTUPLE_MAX];
	s8 l4_idx[ATL_RXF_NTUPLE_MAX];
	uint32_t cmd[ATL_RXF_NTUPLE_MAX];
	int count;
};

enum atl_vlan_cmd {
	ATL_VLAN_EN = ATL_RXF_EN,
	ATL_VLAN_RXQ = BIT(28),
	ATL_VLAN_RXQ_SHIFT = 20,
	ATL_VLAN_RXQ_MASK = ATL_RXF_RXQ_MSK << ATL_VLAN_RXQ_SHIFT,
	ATL_VLAN_ACT_SHIFT = ATL_RXF_ACT_SHIFT,
	ATL_VLAN_VID_MASK = BIT(12) - 1,
};

#define ATL_VID_MAP_LEN BITS_TO_LONGS(BIT(12))

struct atl_rxf_vlan {
	uint32_t cmd[ATL_RXF_VLAN_MAX];
	int count;
	unsigned long map[ATL_VID_MAP_LEN];
	int vlans_active;
	int promisc_count;
};

enum atl_etype_cmd {
	ATL_ETYPE_EN = ATL_RXF_EN,
	ATL_ETYPE_RXQ = BIT(29),
	ATL_ETYPE_RXQ_SHIFT = 20,
	ATL_ETYPE_RXQ_MASK = ATL_RXF_RXQ_MSK << ATL_ETYPE_RXQ_SHIFT,
	ATL_ETYPE_ACT_SHIFT = ATL_RXF_ACT_SHIFT,
	ATL_ETYPE_VAL_MASK = BIT(16) - 1,
};

struct atl_rxf_etype {
	uint32_t cmd[ATL_RXF_ETYPE_MAX];
	int count;
};

enum atl_flex_cmd {
	ATL_FLEX_EN = ATL_RXF_EN,
	ATL_FLEX_RXQ = BIT(30),
	ATL_FLEX_RXQ_SHIFT = 20,
	ATL_FLEX_RXQ_MASK = ATL_RXF_RXQ_MSK << ATL_FLEX_RXQ_SHIFT,
	ATL_FLEX_ACT_SHIFT = ATL_RXF_ACT_SHIFT,
};

struct atl_rxf_flex {
	uint32_t cmd[ATL_RXF_FLEX_MAX];
	int count;
};

struct atl_queue_vec;

#define ATL_NUM_FWD_RINGS ATL_MAX_QUEUES
#define ATL_FWD_RING_BASE ATL_MAX_QUEUES /* Use TC 1 for offload
					  * engine rings */
#define ATL_NUM_MSI_VECS 32
#define ATL_NUM_NON_RING_IRQS 1

#define ATL_RXF_RING_ANY 32

#define ATL_FWD_MSI_BASE (ATL_MAX_QUEUES + ATL_NUM_NON_RING_IRQS)

enum atl_fwd_dir {
	ATL_FWDIR_RX = 0,
	ATL_FWDIR_TX = 1,
	ATL_FWDIR_NUM,
};

struct atl_fwd {
	unsigned long ring_map[ATL_FWDIR_NUM];
	struct atl_fwd_ring *rings[ATL_FWDIR_NUM][ATL_NUM_FWD_RINGS];
	unsigned long msi_map;
	struct blocking_notifier_head nh_clients;
};

#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
struct atl_fwdnl {
	struct atl_desc_ring ring_desc[ATL_NUM_FWD_RINGS * 2];
	/* State of forced redirections */
	int force_icmp_via;
	int force_tx_via;
	/* Deferred TX head cleanup */
	struct delayed_work *tx_cleanup_wq;
	u32 tx_bunch;
};
#endif /* CONFIG_ATLFWD_FWD_NETLINK */

struct atl_nic {
	struct net_device *ndev;

	struct atl_queue_vec *qvecs;
	int nvecs;
	struct atl_hw hw;
	unsigned flags;
	uint32_t priv_flags;
	struct timer_list work_timer;
	int max_mtu;
	unsigned int requested_nvecs;
	int requested_rx_size;
	int requested_tx_size;
	int rx_intr_delay;
	int tx_intr_delay;
	struct atl_global_stats stats;
	spinlock_t stats_lock;
	struct work_struct work;

#if IS_ENABLED(CONFIG_ATLFWD_FWD)
	struct atl_fwd fwd;
#endif
#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	struct atl_fwdnl fwdnl;
#endif

	struct atl_rxf_ntuple rxf_ntuple;
	struct atl_rxf_vlan rxf_vlan;
	struct atl_rxf_etype rxf_etype;
	struct atl_rxf_flex rxf_flex;
};

/* Flags only modified with RTNL lock held */
enum atl_nic_flags {
	ATL_FL_MULTIPLE_VECTORS = BIT(0),
	ATL_FL_WOL = BIT(1),
};

#define ATL_PF(_name) ATL_PF_ ## _name
#define ATL_PF_BIT(_name) ATL_PF_ ## _name ## _BIT
#define ATL_DEF_PF_BIT(_name) ATL_PF_BIT(_name) = BIT(ATL_PF(_name))

enum atl_priv_flags {
	ATL_PF_LPB_SYS_PB,
	ATL_PF_LPB_SYS_DMA,
	ATL_PF_LPB_NET_DMA,
	ATL_PF_LPB_INT_PHY,
	ATL_PF_LPB_EXT_PHY,
	ATL_PF_LPI_RX_MAC,
	ATL_PF_LPI_TX_MAC,
	ATL_PF_LPI_RX_PHY,
	ATL_PF_LPI_TX_PHY,
	ATL_PF_STATS_RESET,
	ATL_PF_STRIP_PAD,
	ATL_PF_MEDIA_DETECT,
};

enum atl_priv_flag_bits {
	ATL_DEF_PF_BIT(LPB_SYS_PB),
	ATL_DEF_PF_BIT(LPB_SYS_DMA),
	ATL_DEF_PF_BIT(LPB_NET_DMA),
	ATL_DEF_PF_BIT(LPB_INT_PHY),
	ATL_DEF_PF_BIT(LPB_EXT_PHY),

	ATL_PF_LPB_MASK = ATL_PF_BIT(LPB_SYS_DMA) | ATL_PF_BIT(LPB_SYS_PB) |
			  ATL_PF_BIT(LPB_NET_DMA) | ATL_PF_BIT(LPB_INT_PHY) |
			  ATL_PF_BIT(LPB_EXT_PHY),

	ATL_DEF_PF_BIT(LPI_RX_MAC),
	ATL_DEF_PF_BIT(LPI_TX_MAC),
	ATL_DEF_PF_BIT(LPI_RX_PHY),
	ATL_DEF_PF_BIT(LPI_TX_PHY),
	ATL_PF_LPI_MASK = ATL_PF_BIT(LPI_RX_MAC) | ATL_PF_BIT(LPI_TX_MAC) |
		ATL_PF_BIT(LPI_RX_PHY) | ATL_PF_BIT(LPI_TX_PHY),

	ATL_DEF_PF_BIT(STATS_RESET),

	ATL_DEF_PF_BIT(STRIP_PAD),
	ATL_DEF_PF_BIT(MEDIA_DETECT),

	ATL_PF_RW_MASK = ATL_PF_LPB_MASK | ATL_PF_BIT(STATS_RESET) |
		ATL_PF_BIT(STRIP_PAD) | ATL_PF_BIT(MEDIA_DETECT),
	ATL_PF_RO_MASK = ATL_PF_LPI_MASK,
};

#define ATL_MAX_MTU (16352 - ETH_FCS_LEN - ETH_HLEN)

#define ATL_MAX_RING_SIZE (8192 - 8)
#define ATL_RING_SIZE 4096

extern const char atl_driver_name[];

extern const struct ethtool_ops atl_ethtool_ops;

extern unsigned int atl_max_queues;
extern unsigned int atl_max_queues_non_msi;
extern unsigned atl_rx_linear;
extern unsigned atl_min_intr_delay;
extern bool atl_enable_msi;
extern bool atl_wq_non_msi;
extern unsigned int atl_tx_clean_budget;
extern unsigned int atl_tx_free_low;
extern unsigned int atl_tx_free_high;

#define atl_module_param(_name, _type, _mode)			\
	module_param_named(_name, atl_ ## _name, _type, _mode)

static inline void atl_intr_enable_non_ring(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	atl_intr_enable(hw, hw->non_ring_intr_mask);
}

static inline void atl_intr_disable_non_ring(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	atl_intr_disable(hw, hw->non_ring_intr_mask);
}

netdev_tx_t atl_start_xmit(struct sk_buff *skb, struct net_device *ndev);
int atl_vlan_rx_add_vid(struct net_device *ndev, __be16 proto, u16 vid);
int atl_vlan_rx_kill_vid(struct net_device *ndev, __be16 proto, u16 vid);
void atl_set_rx_mode(struct net_device *ndev);
int atl_set_features(struct net_device *ndev, netdev_features_t features);
void atl_get_stats64(struct net_device *ndev,
	struct rtnl_link_stats64 *stats);
int atl_setup_datapath(struct atl_nic *nic);
void atl_clear_datapath(struct atl_nic *nic);
int atl_start_rings(struct atl_nic *nic);
void atl_stop_rings(struct atl_nic *nic);
void atl_clear_rdm_cache(struct atl_nic *nic);
void atl_clear_tdm_cache(struct atl_nic *nic);
int atl_alloc_rings(struct atl_nic *nic);
void atl_free_rings(struct atl_nic *nic);
irqreturn_t atl_ring_irq(int irq, void *priv);
void atl_ring_work(struct work_struct *work);
void atl_start_hw_global(struct atl_nic *nic);
int atl_intr_init(struct atl_nic *nic);
void atl_intr_release(struct atl_nic *nic);
int atl_hw_reset(struct atl_hw *hw);
int atl_fw_init(struct atl_hw *hw);
int atl_fw_configure(struct atl_hw *hw);
int atl_reconfigure(struct atl_nic *nic);
void atl_reset_stats(struct atl_nic *nic);
void atl_update_global_stats(struct atl_nic *nic);
void atl_set_loopback(struct atl_nic *nic, int idx, bool on);
void atl_set_intr_mod(struct atl_nic *nic);
void atl_update_ntuple_flt(struct atl_nic *nic, int idx);
void atl_set_vlan_promisc(struct atl_hw *hw, int promisc);
int atl_hwsem_get(struct atl_hw *hw, int idx);
void atl_hwsem_put(struct atl_hw *hw, int idx);
int __atl_msm_read(struct atl_hw *hw, uint32_t addr, uint32_t *val);
int atl_msm_read(struct atl_hw *hw, uint32_t addr, uint32_t *val);
int __atl_msm_write(struct atl_hw *hw, uint32_t addr, uint32_t val);
int atl_msm_write(struct atl_hw *hw, uint32_t addr, uint32_t val);
int atl_update_eth_stats(struct atl_nic *nic);
void atl_adjust_eth_stats(struct atl_ether_stats *stats,
	struct atl_ether_stats *base, bool add);
void atl_fwd_release_rings(struct atl_nic *nic);
#if IS_ENABLED(CONFIG_ATLFWD_FWD)
enum atl_fwd_notify;
int atl_fwd_suspend_rings(struct atl_nic *nic);
int atl_fwd_resume_rings(struct atl_nic *nic);
void atl_fwd_notify(struct atl_nic *nic, enum atl_fwd_notify notif, void *data);
#else
static inline int atl_fwd_suspend_rings(struct atl_nic *nic) { return 0; }
static inline int atl_fwd_resume_rings(struct atl_nic *nic) { return 0; }
static inline void atl_fwd_notify(struct atl_nic *nic,
				  enum atl_fwd_notify notif, void *data) {}
#endif
int atl_get_lpi_timer(struct atl_nic *nic, uint32_t *lpi_delay);
void atl_refresh_rxfs(struct atl_nic *nic);
void atl_schedule_work(struct atl_nic *nic);
int atl_hwmon_init(struct atl_nic *nic);
void atl_thermal_check(struct atl_hw *hw, bool alarm);
int atl_update_thermal(struct atl_hw *hw);
int atl_update_thermal_flag(struct atl_hw *hw, int bit, bool val);
int atl_verify_thermal_limits(struct atl_hw *hw, struct atl_thermal *thermal);
int atl_do_reset(struct atl_nic *nic);
int atl_set_media_detect(struct atl_nic *nic, bool on);

#endif
