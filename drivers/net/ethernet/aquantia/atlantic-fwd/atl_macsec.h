/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_MACSEC_H_
#define _ATL_MACSEC_H_

#include <linux/netdevice.h>
#if IS_ENABLED(CONFIG_MACSEC) && defined(NETIF_F_HW_MACSEC)

#include "net/macsec.h"

struct atl_hw;
struct atl_nic;

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
		u64 ctl_pkts;
		u64 tagged_miss_pkts;
		u64 untagged_miss_pkts;
		u64 notag_pkts;
		u64 untagged_pkts;
		u64 bad_tag_pkts;
		u64 no_sci_pkts;
		u64 unknown_sci_pkts;
		u64 ctrl_prt_pass_pkts;
		u64 unctrl_prt_pass_pkts;
		u64 ctrl_prt_fail_pkts;
		u64 unctrl_prt_fail_pkts;
		u64 too_long_pkts;
		u64 igpoc_ctl_pkts;
		u64 ecc_error_pkts;
		u64 unctrl_hit_drop_redir;
	} in;

	/* Egress Common Counters */
	struct {
		u64 ctl_pkts;
		u64 unknown_sa_pkts;
		u64 untagged_pkts;
		u64 too_long;
		u64 ecc_error_pkts;
		u64 unctrl_hit_drop_redir;
	} out;
};

/* Ingress SA Counters */
struct atl_macsec_rx_sa_stats {
	u64 untagged_hit_pkts;
	u64 ctrl_hit_drop_redir_pkts;
	u64 not_using_sa;
	u64 unused_sa;
	u64 not_valid_pkts;
	u64 invalid_pkts;
	u64 ok_pkts;
	u64 late_pkts;
	u64 delayed_pkts;
	u64 unchecked_pkts;
	u64 validated_octets;
	u64 decrypted_octets;
};

/* Egress SA Counters */
struct atl_macsec_tx_sa_stats {
	u64 sa_hit_drop_redirect;
	u64 sa_protected2_pkts;
	u64 sa_protected_pkts;
	u64 sa_encrypted_pkts;
};

/* Egress SC Counters */
struct atl_macsec_tx_sc_stats {
	u64 sc_protected_pkts;
	u64 sc_encrypted_pkts;
	u64 sc_protected_octets;
	u64 sc_encrypted_octets;
};

struct atl_macsec_txsc {
	u32 hw_sc_idx;
	unsigned long tx_sa_idx_busy;
	const struct macsec_secy *sw_secy;
	/* It is not OK to store key in driver but it is until ... */
	u8 tx_sa_key[MACSEC_NUM_AN][MACSEC_KEYID_LEN];
	struct atl_macsec_tx_sc_stats stats;
	struct atl_macsec_tx_sa_stats tx_sa_stats[MACSEC_NUM_AN];
};

struct atl_macsec_rxsc {
	u32 hw_sc_idx;
	unsigned long rx_sa_idx_busy;
	const struct macsec_secy *sw_secy;
	const struct macsec_rx_sc *sw_rxsc;
	/* TODO: we shouldn't store keys in the driver */
	u8 rx_sa_key[MACSEC_NUM_AN][MACSEC_KEYID_LEN];
	struct atl_macsec_rx_sa_stats rx_sa_stats[MACSEC_NUM_AN];
};

struct atl_macsec_cfg {
	enum atl_macsec_sc_sa sc_sa;
	/* Egress channel configuration */
	unsigned long txsc_idx_busy;
	struct atl_macsec_txsc atl_txsc[ATL_MACSEC_MAX_SC];
	/* Ingress channel configuration */
	unsigned long rxsc_idx_busy;
	struct atl_macsec_rxsc atl_rxsc[ATL_MACSEC_MAX_SC];
	struct atl_macsec_common_stats stats;
};

extern const struct macsec_ops atl_macsec_ops;

int atl_init_macsec(struct atl_hw *hw);
void atl_macsec_work(struct atl_nic *nic);
int atl_macsec_rx_sa_cnt(struct atl_hw *hw);
int atl_macsec_tx_sc_cnt(struct atl_hw *hw);
int atl_macsec_tx_sa_cnt(struct atl_hw *hw);
int atl_macsec_update_stats(struct atl_hw *hw);

#endif

#endif /* _ATL_MACSEC_H_ */
