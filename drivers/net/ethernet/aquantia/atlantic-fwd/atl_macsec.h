/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_MACSEC_H_
#define _ATL_MACSEC_H_

#include <linux/netdevice.h>
#ifdef NETIF_F_HW_MACSEC

#include "net/macsec.h"

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

struct atl_macsec_txsc {
	uint32_t hw_sc_idx;
	unsigned long tx_sa_idx_busy;
	const struct macsec_secy *sw_secy;
	/* It is not OK to store key in driver but it is until ... */
	uint8_t tx_sa_key[MACSEC_NUM_AN][MACSEC_KEYID_LEN];
	struct atl_macsec_tx_sc_stats stats;
	struct atl_macsec_tx_sa_stats tx_sa_stats[MACSEC_NUM_AN];
};

struct atl_macsec_rxsc {
	uint32_t hw_sc_idx;
	unsigned long rx_sa_idx_busy;
	const struct macsec_secy *sw_secy;
	const struct macsec_rx_sc *sw_rxsc;
	/* TODO: we shouldn't store keys in the driver */
	uint8_t rx_sa_key[MACSEC_NUM_AN][MACSEC_KEYID_LEN];
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

#endif /* NETIF_F_HW_MACSEC */

#endif /* _ATL_MACSEC_H_ */
