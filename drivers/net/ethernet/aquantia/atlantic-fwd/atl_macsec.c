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

#include "atl_macsec.h"
#if IS_ENABLED(CONFIG_MACSEC) && defined(NETIF_F_HW_MACSEC)
#include "atl_common.h"
#include <linux/rtnetlink.h>

#include "macsec/macsec_api.h"
#define ATL_MACSEC_KEY_LEN_128_BIT 16
#define ATL_MACSEC_KEY_LEN_192_BIT 24
#define ATL_MACSEC_KEY_LEN_256_BIT 32

static unsigned int atl_macsec_bridge = 1;
module_param_named(macsec_bridge, atl_macsec_bridge, uint, 0644);

enum atl_clear_type {
	/* update HW configuration */
	ATL_CLEAR_HW = BIT(0),
	/* update SW configuration (busy bits, pointers) */
	ATL_CLEAR_SW = BIT(1),
	/* update both HW and SW configuration */
	ATL_CLEAR_ALL = ATL_CLEAR_HW | ATL_CLEAR_SW,
};

static int atl_clear_txsc(struct atl_nic *nic, const int txsc_idx,
			  enum atl_clear_type clear_type);
static int atl_clear_txsa(struct atl_nic *nic, struct atl_macsec_txsc *atl_txsc,
			  const int sa_num, enum atl_clear_type clear_type);
static int atl_clear_rxsc(struct atl_nic *nic, const int rxsc_idx,
			  enum atl_clear_type clear_type);
static int atl_clear_rxsa(struct atl_nic *nic, struct atl_macsec_rxsc *atl_rxsc,
			  const int sa_num, enum atl_clear_type clear_type);
static int atl_clear_secy(struct atl_nic *nic, const struct macsec_secy *secy,
			  enum atl_clear_type clear_type);
static int atl_apply_macsec_cfg(struct atl_hw *hw);
static int atl_apply_secy_cfg(struct atl_hw *hw,
			      const struct macsec_secy *secy);

static void atl_ether_addr_to_mac(u32 mac[2], unsigned char *emac)
{
	u32 tmp[2] = { 0 };

	memcpy(((u8 *)tmp) + 2, emac, ETH_ALEN);

	mac[0] = swab32(tmp[1]);
	mac[1] = swab32(tmp[0]);
}

/* There's a 1:1 mapping between SecY and TX SC */
static int atl_get_txsc_idx_from_secy(struct atl_hw *hw,
				      const struct macsec_secy *secy)
{
	int i;

	if (unlikely(!secy))
		return -1;

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (hw->macsec_cfg.atl_txsc[i].sw_secy == secy)
			return i;
	}
	return -1;
}

static int atl_get_rxsc_idx_from_rxsc(struct atl_hw *hw,
				      const struct macsec_rx_sc *rxsc)
{
	int i;

	if (unlikely(!rxsc))
		return -1;

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (hw->macsec_cfg.atl_rxsc[i].sw_rxsc == rxsc)
			return i;
	}

	return -1;
}

static int atl_get_txsc_idx_from_sc_idx(const enum atl_macsec_sc_sa sc_sa,
					const unsigned int sc_idx)
{
	switch (sc_sa) {
	case atl_macsec_sa_sc_4sa_8sc:
		return sc_idx >> 2;
	case atl_macsec_sa_sc_2sa_16sc:
		return sc_idx >> 1;
	case atl_macsec_sa_sc_1sa_32sc:
		return sc_idx;
	default:
		WARN_ONCE(1, "Invalid sc_sa");
	}
	return -1;
}

/* Rotate keys u32[8] */
static void atl_rotate_keys(u32 (*key)[8], int key_len)
{
	u32 tmp[8] = { 0 };

	memcpy(&tmp, key, sizeof(tmp));
	memset(*key, 0, sizeof(*key));

	if (key_len == ATL_MACSEC_KEY_LEN_128_BIT) {
		(*key)[0] = swab32(tmp[3]);
		(*key)[1] = swab32(tmp[2]);
		(*key)[2] = swab32(tmp[1]);
		(*key)[3] = swab32(tmp[0]);
	} else if (key_len == ATL_MACSEC_KEY_LEN_192_BIT) {
		(*key)[0] = swab32(tmp[5]);
		(*key)[1] = swab32(tmp[4]);
		(*key)[2] = swab32(tmp[3]);
		(*key)[3] = swab32(tmp[2]);
		(*key)[4] = swab32(tmp[1]);
		(*key)[5] = swab32(tmp[0]);
	} else if (key_len == ATL_MACSEC_KEY_LEN_256_BIT) {
		(*key)[0] = swab32(tmp[7]);
		(*key)[1] = swab32(tmp[6]);
		(*key)[2] = swab32(tmp[5]);
		(*key)[3] = swab32(tmp[4]);
		(*key)[4] = swab32(tmp[3]);
		(*key)[5] = swab32(tmp[2]);
		(*key)[6] = swab32(tmp[1]);
		(*key)[7] = swab32(tmp[0]);
	} else {
		pr_warn("Rotate_keys: invalid key_len\n");
	}
}

#define STATS_2x32_TO_64(stat_field)                                           \
	(((u64)stat_field[1] << 32) | stat_field[0])

static int atl_get_macsec_common_stats(struct atl_hw *hw,
				       struct atl_macsec_common_stats *stats)
{
	struct aq_mss_ingress_common_counters ingress_counters;
	struct aq_mss_egress_common_counters egress_counters;
	int ret;

	/* MACSEC counters */
	ret = aq_mss_get_ingress_common_counters(hw, &ingress_counters);
	if (unlikely(ret))
		return ret;

	stats->in.ctl_pkts = STATS_2x32_TO_64(ingress_counters.ctl_pkts);
	stats->in.tagged_miss_pkts =
		STATS_2x32_TO_64(ingress_counters.tagged_miss_pkts);
	stats->in.untagged_miss_pkts =
		STATS_2x32_TO_64(ingress_counters.untagged_miss_pkts);
	stats->in.notag_pkts = STATS_2x32_TO_64(ingress_counters.notag_pkts);
	stats->in.untagged_pkts =
		STATS_2x32_TO_64(ingress_counters.untagged_pkts);
	stats->in.bad_tag_pkts =
		STATS_2x32_TO_64(ingress_counters.bad_tag_pkts);
	stats->in.no_sci_pkts = STATS_2x32_TO_64(ingress_counters.no_sci_pkts);
	stats->in.unknown_sci_pkts =
		STATS_2x32_TO_64(ingress_counters.unknown_sci_pkts);
	stats->in.ctrl_prt_pass_pkts =
		STATS_2x32_TO_64(ingress_counters.ctrl_prt_pass_pkts);
	stats->in.unctrl_prt_pass_pkts =
		STATS_2x32_TO_64(ingress_counters.unctrl_prt_pass_pkts);
	stats->in.ctrl_prt_fail_pkts =
		STATS_2x32_TO_64(ingress_counters.ctrl_prt_fail_pkts);
	stats->in.unctrl_prt_fail_pkts =
		STATS_2x32_TO_64(ingress_counters.unctrl_prt_fail_pkts);
	stats->in.too_long_pkts =
		STATS_2x32_TO_64(ingress_counters.too_long_pkts);
	stats->in.igpoc_ctl_pkts =
		STATS_2x32_TO_64(ingress_counters.igpoc_ctl_pkts);
	stats->in.ecc_error_pkts =
		STATS_2x32_TO_64(ingress_counters.ecc_error_pkts);
	stats->in.unctrl_hit_drop_redir =
		STATS_2x32_TO_64(ingress_counters.unctrl_hit_drop_redir);

	ret = aq_mss_get_egress_common_counters(hw, &egress_counters);
	if (unlikely(ret))
		return ret;
	stats->out.ctl_pkts = STATS_2x32_TO_64(egress_counters.ctl_pkt);
	stats->out.unknown_sa_pkts =
		STATS_2x32_TO_64(egress_counters.unknown_sa_pkts);
	stats->out.untagged_pkts =
		STATS_2x32_TO_64(egress_counters.untagged_pkts);
	stats->out.too_long = STATS_2x32_TO_64(egress_counters.too_long);
	stats->out.ecc_error_pkts =
		STATS_2x32_TO_64(egress_counters.ecc_error_pkts);
	stats->out.unctrl_hit_drop_redir =
		STATS_2x32_TO_64(egress_counters.unctrl_hit_drop_redir);

	return 0;
}

static int atl_get_rxsa_stats(struct atl_hw *hw, int sa_idx,
			      struct atl_macsec_rx_sa_stats *stats)
{
	struct aq_mss_ingress_sa_counters i_sa_counters;
	int ret;

	ret = aq_mss_get_ingress_sa_counters(hw, &i_sa_counters, sa_idx);
	if (unlikely(ret))
		return ret;

	stats->untagged_hit_pkts =
		STATS_2x32_TO_64(i_sa_counters.untagged_hit_pkts);
	stats->ctrl_hit_drop_redir_pkts =
		STATS_2x32_TO_64(i_sa_counters.ctrl_hit_drop_redir_pkts);
	stats->not_using_sa = STATS_2x32_TO_64(i_sa_counters.not_using_sa);
	stats->unused_sa = STATS_2x32_TO_64(i_sa_counters.unused_sa);
	stats->not_valid_pkts = STATS_2x32_TO_64(i_sa_counters.not_valid_pkts);
	stats->invalid_pkts = STATS_2x32_TO_64(i_sa_counters.invalid_pkts);
	stats->ok_pkts = STATS_2x32_TO_64(i_sa_counters.ok_pkts);
	stats->late_pkts = STATS_2x32_TO_64(i_sa_counters.late_pkts);
	stats->delayed_pkts = STATS_2x32_TO_64(i_sa_counters.delayed_pkts);
	stats->unchecked_pkts = STATS_2x32_TO_64(i_sa_counters.unchecked_pkts);
	stats->validated_octets =
		STATS_2x32_TO_64(i_sa_counters.validated_octets);
	stats->decrypted_octets =
		STATS_2x32_TO_64(i_sa_counters.decrypted_octets);

	return 0;
}

static int atl_get_txsa_stats(struct atl_hw *hw, int sa_idx,
			      struct atl_macsec_tx_sa_stats *stats)
{
	struct aq_mss_egress_sa_counters e_sa_counters;
	int ret;

	ret = aq_mss_get_egress_sa_counters(hw, &e_sa_counters, sa_idx);
	if (unlikely(ret))
		return ret;

	stats->sa_hit_drop_redirect =
		STATS_2x32_TO_64(e_sa_counters.sa_hit_drop_redirect);
	stats->sa_protected2_pkts =
		STATS_2x32_TO_64(e_sa_counters.sa_protected2_pkts);
	stats->sa_protected_pkts =
		STATS_2x32_TO_64(e_sa_counters.sa_protected_pkts);
	stats->sa_encrypted_pkts =
		STATS_2x32_TO_64(e_sa_counters.sa_encrypted_pkts);

	return 0;
}

static int atl_get_txsa_next_pn(struct atl_hw *hw, int sa_idx, u32 *pn)
{
	struct aq_mss_egress_sa_record sa_rec;
	int ret;

	ret = aq_mss_get_egress_sa_record(hw, &sa_rec, sa_idx);
	if (likely(!ret))
		*pn = sa_rec.next_pn;

	return ret;
}

static int atl_get_rxsa_next_pn(struct atl_hw *hw, int sa_idx, u32 *pn)
{
	struct aq_mss_ingress_sa_record sa_rec;
	int ret;

	ret = aq_mss_get_ingress_sa_record(hw, &sa_rec, sa_idx);
	if (likely(!ret))
		*pn = (!sa_rec.sat_nextpn) ? sa_rec.next_pn : 0;

	return ret;
}

static int atl_get_txsc_stats(struct atl_hw *hw, int sc_idx,
			      struct atl_macsec_tx_sc_stats *stats)
{
	struct aq_mss_egress_sc_counters e_sc_counters;
	int ret;

	ret = aq_mss_get_egress_sc_counters(hw, &e_sc_counters, sc_idx);
	if (unlikely(ret))
		return ret;

	stats->sc_protected_pkts =
		STATS_2x32_TO_64(e_sc_counters.sc_protected_pkts);
	stats->sc_encrypted_pkts =
		STATS_2x32_TO_64(e_sc_counters.sc_encrypted_pkts);
	stats->sc_protected_octets =
		STATS_2x32_TO_64(e_sc_counters.sc_protected_octets);
	stats->sc_encrypted_octets =
		STATS_2x32_TO_64(e_sc_counters.sc_encrypted_octets);

	return 0;
}

int atl_macsec_rx_sa_cnt(struct atl_hw *hw)
{
	int i, cnt = 0;

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (!test_bit(i, &hw->macsec_cfg.rxsc_idx_busy))
			continue;
		cnt += hweight_long(hw->macsec_cfg.atl_rxsc[i].rx_sa_idx_busy);
	}

	return cnt;
}

int atl_macsec_tx_sc_cnt(struct atl_hw *hw)
{
	return hweight_long(hw->macsec_cfg.txsc_idx_busy);
}

int atl_macsec_tx_sa_cnt(struct atl_hw *hw)
{
	int i, cnt = 0;

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (!test_bit(i, &hw->macsec_cfg.txsc_idx_busy))
			continue;
		cnt += hweight_long(hw->macsec_cfg.atl_txsc[i].tx_sa_idx_busy);
	}

	return cnt;
}

int atl_macsec_update_stats(struct atl_hw *hw)
{
	struct atl_macsec_txsc *atl_txsc;
	struct atl_macsec_rxsc *atl_rxsc;
	int i, sa_idx, assoc_num;
	int ret = 0;

	atl_get_macsec_common_stats(hw, &hw->macsec_cfg.stats);

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (!(hw->macsec_cfg.txsc_idx_busy & BIT(i)))
			continue;
		atl_txsc = &hw->macsec_cfg.atl_txsc[i];

		ret = atl_get_txsc_stats(hw, atl_txsc->hw_sc_idx,
					 &atl_txsc->stats);
		if (ret)
			return ret;

		for (assoc_num = 0; assoc_num < MACSEC_NUM_AN; assoc_num++) {
			if (!test_bit(assoc_num, &atl_txsc->tx_sa_idx_busy))
				continue;
			sa_idx = atl_txsc->hw_sc_idx | assoc_num;
			ret = atl_get_txsa_stats(
				hw, sa_idx, &atl_txsc->tx_sa_stats[assoc_num]);
			if (ret)
				return ret;
		}
	}

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (!(test_bit(i, &hw->macsec_cfg.rxsc_idx_busy)))
			continue;
		atl_rxsc = &hw->macsec_cfg.atl_rxsc[i];

		for (assoc_num = 0; assoc_num < MACSEC_NUM_AN; assoc_num++) {
			if (!test_bit(assoc_num, &atl_rxsc->rx_sa_idx_busy))
				continue;
			sa_idx = atl_rxsc->hw_sc_idx | assoc_num;

			ret = atl_get_rxsa_stats(
				hw, sa_idx, &atl_rxsc->rx_sa_stats[assoc_num]);
			if (ret)
				return ret;
		}
	}

	return ret;
}

int atl_init_macsec(struct atl_hw *hw)
{
	struct aq_mss_ingress_prectlf_record rx_prectlf_rec;
	u32 ctl_ether_types[2] = { ETH_P_PAE, ETH_P_PAUSE};
	struct macsec_msg_fw_response resp;
	struct macsec_msg_fw_request msg;
	int num_ctl_ether_types = 0;
	int index = 0, tbl_idx;
	int ret;

	rtnl_lock();
	memset(&msg, 0, sizeof(msg));
	memset(&resp, 0, sizeof(resp));

	if (hw->mcp.ops->send_macsec_req) {
		struct macsec_cfg_request cfg;

		memset(&cfg, 0, sizeof(cfg));
		cfg.enabled = 1;
		cfg.egress_threshold = 0xffffffff;
		cfg.ingress_threshold = 0xffffffff;
		cfg.interrupts_enabled = 1;

		msg.msg_type = macsec_cfg_msg;
		msg.cfg = cfg;

		ret = hw->mcp.ops->send_macsec_req(hw, &msg, &resp);
		if (ret)
			goto unlock;
	}

	/* Init Ethertype bypass filters */
	memset(&rx_prectlf_rec, 0, sizeof(rx_prectlf_rec));
	rx_prectlf_rec.match_type = 4; /* Match eth_type only */
	rx_prectlf_rec.action = 0; /* Bypass MACSEC modules */
	aq_mss_set_ingress_prectlf_record(hw, &rx_prectlf_rec, 0);
	aq_mss_set_packet_edit_control(hw, 0xb068);
	for (index = 0; index < ARRAY_SIZE(ctl_ether_types); index++) {
		struct aq_mss_egress_ctlf_record tx_ctlf_rec;

		memset(&rx_prectlf_rec, 0, sizeof(rx_prectlf_rec));
		memset(&tx_ctlf_rec, 0, sizeof(tx_ctlf_rec));

		if (ctl_ether_types[index] == 0)
			continue;

		tx_ctlf_rec.eth_type = ctl_ether_types[index];
		tx_ctlf_rec.match_type = 4; /* Match eth_type only */
		tx_ctlf_rec.match_mask = 0xf; /* match for eth_type */
		tx_ctlf_rec.action = 0; /* Bypass MACSEC modules */
		tbl_idx = NUMROWS_EGRESSCTLFRECORD - num_ctl_ether_types - 1;
		aq_mss_set_egress_ctlf_record(hw, &tx_ctlf_rec, tbl_idx);

		rx_prectlf_rec.eth_type = ctl_ether_types[index];
		rx_prectlf_rec.match_type = 4; /* Match eth_type only */
		rx_prectlf_rec.match_mask = 0xf; /* match for eth_type */
		rx_prectlf_rec.action = 0; /* Bypass MACSEC modules */
		tbl_idx =
			NUMROWS_INGRESSPRECTLFRECORD - num_ctl_ether_types - 1;
		aq_mss_set_ingress_prectlf_record(hw, &rx_prectlf_rec, tbl_idx);

		num_ctl_ether_types++;
	}

	ret = atl_apply_macsec_cfg(hw);

unlock:
	rtnl_unlock();
	return ret;
}

static int atl_mdo_dev_open(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	int ret = 0;

	if (ctx->prepare)
		return 0;

	if (netif_carrier_ok(nic->ndev))
		ret = atl_apply_secy_cfg(&nic->hw, ctx->secy);

	atl_fwd_notify(nic, ATL_FWD_NOTIFY_MACSEC_ON, ctx->secy->netdev);

	return ret;
}

static int atl_mdo_dev_stop(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);

	if (ctx->prepare)
		return 0;

	atl_fwd_notify(nic, ATL_FWD_NOTIFY_MACSEC_OFF, ctx->secy->netdev);

	return atl_clear_secy(nic, ctx->secy, ATL_CLEAR_HW);
}

static int atl_set_txsc(struct atl_hw *hw, int txsc_idx)
{
	struct atl_macsec_txsc *atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	const struct macsec_secy *secy = atl_txsc->sw_secy;
	struct aq_mss_egress_class_record tx_class_rec;
	unsigned int sc_idx = atl_txsc->hw_sc_idx;
	struct aq_mss_egress_sc_record sc_rec;
	__be64 nsci;
	int ret = 0;

	memset(&tx_class_rec, 0, sizeof(tx_class_rec));
	memset(&sc_rec, 0, sizeof(sc_rec));

	atl_ether_addr_to_mac(tx_class_rec.mac_sa, secy->netdev->dev_addr);

	atl_dev_dbg("set secy: sci %#llx, sc_idx=%d, protect=%d, curr_an=%d\n",
		    secy->sci, sc_idx, secy->protect_frames,
		    secy->tx_sc.encoding_sa);

	nsci = cpu_to_be64((__force u64)secy->sci);
	memcpy(tx_class_rec.sci, &nsci, sizeof(nsci));
	tx_class_rec.sci_mask = 0;

	if (!atl_macsec_bridge)
		tx_class_rec.sa_mask = 0x3f;

	tx_class_rec.action = 0; /* forward to SA/SC table */
	tx_class_rec.valid = 1;

	tx_class_rec.sc_idx = sc_idx;

	tx_class_rec.sc_sa = hw->macsec_cfg.sc_sa;

	ret = aq_mss_set_egress_class_record(hw, &tx_class_rec, txsc_idx);
	if (ret)
		return ret;

	sc_rec.protect = secy->protect_frames;
	if (secy->tx_sc.encrypt)
		sc_rec.tci |= BIT(1);
	if (secy->tx_sc.scb)
		sc_rec.tci |= BIT(2);
	if (secy->tx_sc.send_sci)
		sc_rec.tci |= BIT(3);
	if (secy->tx_sc.end_station)
		sc_rec.tci |= BIT(4);
	/* The C bit is clear if and only if the Secure Data is
	 * exactly the same as the User Data and the ICV is 16 octets long.
	 */
	if (!(secy->icv_len == 16 && !secy->tx_sc.encrypt))
		sc_rec.tci |= BIT(0);

	sc_rec.an_roll = 0;

	switch (secy->key_len) {
	case ATL_MACSEC_KEY_LEN_128_BIT:
		sc_rec.sak_len = 0;
		break;
	case ATL_MACSEC_KEY_LEN_192_BIT:
		sc_rec.sak_len = 1;
		break;
	case ATL_MACSEC_KEY_LEN_256_BIT:
		sc_rec.sak_len = 2;
		break;
	default:
		WARN_ONCE(1, "Invalid sc_sa");
		return -EINVAL;
	}

	sc_rec.curr_an = secy->tx_sc.encoding_sa;
	sc_rec.valid = 1;
	sc_rec.fresh = 1;

	if (atl_macsec_bridge) {
		ret = aq_mss_set_drop_igprc_miss_packets(hw, 1);
		if (ret)
			return ret;
	}

	return aq_mss_set_egress_sc_record(hw, &sc_rec, sc_idx);
}

static u32 sc_idx_max(const enum atl_macsec_sc_sa sc_sa)
{
	u32 result = 0;

	switch (sc_sa) {
	case atl_macsec_sa_sc_4sa_8sc:
		result = 8;
		break;
	case atl_macsec_sa_sc_2sa_16sc:
		result = 16;
		break;
	case atl_macsec_sa_sc_1sa_32sc:
		result = 32;
		break;
	default:
		break;
	};

	return result;
}

static u32 atl_to_hw_sc_idx(const u32 sc_idx, const enum atl_macsec_sc_sa sc_sa)
{
	switch (sc_sa) {
	case atl_macsec_sa_sc_4sa_8sc:
		return sc_idx << 2;
	case atl_macsec_sa_sc_2sa_16sc:
		return sc_idx << 1;
	case atl_macsec_sa_sc_1sa_32sc:
		return sc_idx;
	default:
		/* Should never happen */
		break;
	};

	WARN_ON(true);
	return sc_idx;
}

static enum atl_macsec_sc_sa sc_sa_from_num_an(const int num_an)
{
	enum atl_macsec_sc_sa sc_sa = atl_macsec_sa_sc_not_used;

	switch (num_an) {
	case 4:
		sc_sa = atl_macsec_sa_sc_4sa_8sc;
		break;
	case 2:
		sc_sa = atl_macsec_sa_sc_2sa_16sc;
		break;
	case 1:
		sc_sa = atl_macsec_sa_sc_1sa_32sc;
		break;
	default:
		break;
	}

	return sc_sa;
}

static int atl_mdo_add_secy(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	const struct macsec_secy *secy = ctx->secy;
	struct atl_hw *hw = &nic->hw;
	enum atl_macsec_sc_sa sc_sa;
	u32 txsc_idx;
	int ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	if (secy->xpn)
		return -EOPNOTSUPP;
#endif

	sc_sa = sc_sa_from_num_an(MACSEC_NUM_AN);
	if (sc_sa == atl_macsec_sa_sc_not_used)
		return -EINVAL;

	if (atl_macsec_bridge && hweight32(hw->macsec_cfg.txsc_idx_busy))
		return -ENOSPC;

	if (hweight32(hw->macsec_cfg.txsc_idx_busy) >= sc_idx_max(sc_sa))
		return -ENOSPC;

	txsc_idx = ffz(hw->macsec_cfg.txsc_idx_busy);
	if (txsc_idx == ATL_MACSEC_MAX_SC)
		return -ENOSPC;

	if (ctx->prepare)
		return 0;

	hw->macsec_cfg.sc_sa = sc_sa;
	hw->macsec_cfg.atl_txsc[txsc_idx].hw_sc_idx =
		atl_to_hw_sc_idx(txsc_idx, sc_sa);
	hw->macsec_cfg.atl_txsc[txsc_idx].sw_secy = secy;
	atl_dev_dbg("add secy: txsc_idx=%d, sc_idx=%d\n", txsc_idx,
		    hw->macsec_cfg.atl_txsc[txsc_idx].hw_sc_idx);

	if (netif_carrier_ok(nic->ndev) && netif_running(secy->netdev))
		ret = atl_set_txsc(hw, txsc_idx);

	set_bit(txsc_idx, &hw->macsec_cfg.txsc_idx_busy);

	return 0;
}

static int atl_mdo_upd_secy(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	const struct macsec_secy *secy;
	struct atl_hw *hw = &nic->hw;
	int txsc_idx;
	int ret = 0;

	txsc_idx = atl_get_txsc_idx_from_secy(&nic->hw, ctx->secy);
	if (txsc_idx < 0)
		return -ENOENT;

	if (ctx->prepare)
		return 0;

	secy = hw->macsec_cfg.atl_txsc[txsc_idx].sw_secy;

	if (netif_carrier_ok(nic->ndev) && netif_running(secy->netdev))
		ret = atl_set_txsc(hw, txsc_idx);

	return ret;
}

static int atl_clear_txsc(struct atl_nic *nic, const int txsc_idx,
			  enum atl_clear_type clear_type)
{
	struct atl_hw *hw = &nic->hw;
	struct atl_macsec_txsc *tx_sc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	struct aq_mss_egress_class_record tx_class_rec;
	struct aq_mss_egress_sc_record sc_rec;
	int ret = 0;
	int sa_num;

	memset(&tx_class_rec, 0, sizeof(tx_class_rec));
	memset(&sc_rec, 0, sizeof(sc_rec));

	for_each_set_bit (sa_num, &tx_sc->tx_sa_idx_busy, ATL_MACSEC_MAX_SA) {
		ret = atl_clear_txsa(nic, tx_sc, sa_num, clear_type);
		if (ret)
			return ret;
	}

	if (clear_type & ATL_CLEAR_HW) {
		ret = aq_mss_set_egress_class_record(hw, &tx_class_rec,
						     txsc_idx);
		if (ret)
			return ret;

		sc_rec.fresh = 1;
		ret = aq_mss_set_egress_sc_record(hw, &sc_rec,
						  tx_sc->hw_sc_idx);
		if (ret)
			return ret;

		if (atl_macsec_bridge) {
			ret = aq_mss_set_drop_igprc_miss_packets(hw, 0);
			if (ret)
				return ret;
		}
	}

	if (clear_type & ATL_CLEAR_SW) {
		clear_bit(txsc_idx, &hw->macsec_cfg.txsc_idx_busy);
		hw->macsec_cfg.atl_txsc[txsc_idx].sw_secy = NULL;
	}

	return ret;
}

static int atl_mdo_del_secy(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);

	if (ctx->prepare)
		return 0;

	return atl_clear_secy(nic, ctx->secy, ATL_CLEAR_ALL);
}

static int atl_update_txsa(struct atl_hw *hw, unsigned int sc_idx,
			   const struct macsec_secy *secy,
			   const struct macsec_tx_sa *tx_sa,
			   const unsigned char *key, unsigned char an)
{
	struct aq_mss_egress_sakey_record key_rec;
	const unsigned int sa_idx = sc_idx | an;
	struct aq_mss_egress_sa_record sa_rec;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
	const u32 next_pn = tx_sa->next_pn;
#else
	const u32 next_pn = tx_sa->next_pn_halves.lower;
#endif
	int ret = 0;

	atl_dev_dbg("set tx_sa %d: active=%d, next_pn=%d\n", an, tx_sa->active,
		    next_pn);

	memset(&sa_rec, 0, sizeof(sa_rec));
	sa_rec.valid = tx_sa->active;
	sa_rec.fresh = 1;
	sa_rec.next_pn = next_pn;

	ret = aq_mss_set_egress_sa_record(hw, &sa_rec, sa_idx);
	if (ret) {
		atl_dev_err("aq_mss_set_egress_sa_record failed with %d\n",
			    ret);
		return ret;
	}

	if (!key)
		return ret;

	memset(&key_rec, 0, sizeof(key_rec));
	memcpy(&key_rec.key, key, secy->key_len);

	atl_rotate_keys(&key_rec.key, secy->key_len);

	ret = aq_mss_set_egress_sakey_record(hw, &key_rec, sa_idx);
	if (ret)
		atl_dev_err("aq_mss_set_egress_sakey_record failed with %d\n",
			    ret);

	return ret;
}

static int atl_mdo_add_txsa(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	const struct macsec_secy *secy = ctx->secy;
	struct atl_macsec_txsc *atl_txsc;
	struct atl_hw *hw = &nic->hw;
	int txsc_idx;

	txsc_idx = atl_get_txsc_idx_from_secy(hw, secy);
	if (txsc_idx < 0)
		return -EINVAL;

	if (ctx->prepare)
		return 0;

	atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	set_bit(ctx->sa.assoc_num, &atl_txsc->tx_sa_idx_busy);

	memcpy(atl_txsc->tx_sa_key[ctx->sa.assoc_num], ctx->sa.key,
	       secy->key_len);

	if (netif_carrier_ok(nic->ndev) && netif_running(secy->netdev))
		return atl_update_txsa(&nic->hw, atl_txsc->hw_sc_idx, secy,
				       ctx->sa.tx_sa, ctx->sa.key,
				       ctx->sa.assoc_num);

	return 0;
}

static int atl_mdo_upd_txsa(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	const struct macsec_secy *secy = ctx->secy;
	struct atl_macsec_txsc *atl_txsc;
	struct atl_hw *hw = &nic->hw;
	int txsc_idx;

	txsc_idx = atl_get_txsc_idx_from_secy(hw, secy);
	if (txsc_idx < 0)
		return -EINVAL;

	if (ctx->prepare)
		return 0;

	atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	if (netif_carrier_ok(nic->ndev) && netif_running(secy->netdev))
		return atl_update_txsa(&nic->hw, atl_txsc->hw_sc_idx, secy,
				       ctx->sa.tx_sa, NULL, ctx->sa.assoc_num);

	return 0;
}

static int atl_clear_txsa(struct atl_nic *nic, struct atl_macsec_txsc *atl_txsc,
			  const int sa_num, enum atl_clear_type clear_type)
{
	int sa_idx = atl_txsc->hw_sc_idx | sa_num;
	struct atl_hw *hw = &nic->hw;
	int ret = 0;

	if (clear_type & ATL_CLEAR_SW)
		clear_bit(sa_num, &atl_txsc->tx_sa_idx_busy);

	if ((clear_type & ATL_CLEAR_HW) && netif_carrier_ok(nic->ndev)) {
		struct aq_mss_egress_sakey_record key_rec;
		struct aq_mss_egress_sa_record sa_rec;

		memset(&sa_rec, 0, sizeof(sa_rec));

		sa_rec.fresh = 1;

		ret = aq_mss_set_egress_sa_record(hw, &sa_rec, sa_idx);
		if (ret)
			return ret;

		memset(&key_rec, 0, sizeof(key_rec));
		return aq_mss_set_egress_sakey_record(hw, &key_rec, sa_idx);
	}

	return 0;
}

static int atl_mdo_del_txsa(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	int txsc_idx;

	txsc_idx = atl_get_txsc_idx_from_secy(&nic->hw, ctx->secy);
	if (txsc_idx < 0)
		return -EINVAL;

	if (ctx->prepare)
		return 0;

	return atl_clear_txsa(nic, &nic->hw.macsec_cfg.atl_txsc[txsc_idx],
			      ctx->sa.assoc_num, ATL_CLEAR_ALL);
}

static int atl_rxsc_validate_frames(const enum macsec_validation_type validate)
{
	switch (validate) {
	case MACSEC_VALIDATE_DISABLED:
		return 2;
	case MACSEC_VALIDATE_CHECK:
		return 1;
	case MACSEC_VALIDATE_STRICT:
		return 0;
	default:
		break;
	}

	/* should never be here */
	WARN_ON(true);
	return 0;
}

static int atl_set_rxsc(struct atl_hw *hw, const u32 rxsc_idx)
{
	const struct atl_macsec_rxsc *atl_rxsc =
		&hw->macsec_cfg.atl_rxsc[rxsc_idx];
	struct aq_mss_ingress_preclass_record pre_class_record;
	const struct macsec_rx_sc *rx_sc = atl_rxsc->sw_rxsc;
	struct aq_mss_ingress_prectlf_record rx_prectlf_rec;
	const struct macsec_secy *secy = atl_rxsc->sw_secy;
	const u32 hw_sc_idx = atl_rxsc->hw_sc_idx;
	struct aq_mss_ingress_sc_record sc_record;
	__be64 nsci;
	int ret = 0;

	atl_dev_dbg("set rx_sc: rxsc_idx=%d, sci %#llx, hw_sc_idx=%d\n",
		    rxsc_idx, rx_sc->sci, hw_sc_idx);

	if (atl_macsec_bridge) {
		memset(&rx_prectlf_rec, 0, sizeof(rx_prectlf_rec));
		aq_mss_set_ingress_prectlf_record(hw, &rx_prectlf_rec, 0);
		aq_mss_set_packet_edit_control(hw, 0);
	}

	memset(&pre_class_record, 0, sizeof(pre_class_record));
	nsci = cpu_to_be64((__force u64)rx_sc->sci);
	memcpy(pre_class_record.sci, &nsci, sizeof(nsci));
	pre_class_record.sci_mask = 0xff;
	/* match all MACSEC ethertype packets */
	pre_class_record.eth_type = ETH_P_MACSEC;
	pre_class_record.eth_type_mask = 0x3;

	atl_ether_addr_to_mac(pre_class_record.mac_sa, (char *)&rx_sc->sci);
	pre_class_record.sa_mask = 0x3f;

	pre_class_record.an_mask = hw->macsec_cfg.sc_sa;
	pre_class_record.sc_idx = hw_sc_idx;
	/* strip SecTAG & forward for decryption */
	pre_class_record.action = 0x0;
	pre_class_record.valid = 1;

	ret = aq_mss_set_ingress_preclass_record(hw, &pre_class_record,
						 2 * rxsc_idx + 1);
	if (ret) {
		atl_dev_err(
			"aq_mss_set_ingress_preclass_record failed with %d\n",
			ret);
		return ret;
	}

	/* If SCI is absent, then match by SA alone */
	pre_class_record.sci_mask = 0;
	pre_class_record.sci_from_table = 1;

	ret = aq_mss_set_ingress_preclass_record(hw, &pre_class_record,
						 2 * rxsc_idx);
	if (ret) {
		atl_dev_err(
			"aq_mss_set_ingress_preclass_record failed with %d\n",
			ret);
		return ret;
	}

	memset(&sc_record, 0, sizeof(sc_record));
	sc_record.validate_frames =
		atl_rxsc_validate_frames(secy->validate_frames);
	if (secy->replay_protect) {
		sc_record.replay_protect = 1;
		sc_record.anti_replay_window = secy->replay_window;
	} else {
		/* HW workaround to not drop Delayed frames */
		sc_record.anti_replay_window = ~0;
	}
	sc_record.valid = 1;
	sc_record.fresh = 1;

	ret = aq_mss_set_ingress_sc_record(hw, &sc_record, hw_sc_idx);
	if (ret) {
		atl_dev_err("aq_mss_set_ingress_sc_record failed with %d\n",
			    ret);
		return ret;
	}

	return ret;
}

static int atl_mdo_add_rxsc(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_macsec_cfg *cfg = &nic->hw.macsec_cfg;
	const u32 rxsc_idx_max = sc_idx_max(cfg->sc_sa);
	u32 rxsc_idx;
	int ret = 0;

	if (hweight32(cfg->rxsc_idx_busy) >= rxsc_idx_max)
		return -ENOSPC;

	rxsc_idx = ffz(cfg->rxsc_idx_busy);
	if (rxsc_idx >= rxsc_idx_max)
		return -ENOSPC;

	if (ctx->prepare)
		return 0;

	cfg->atl_rxsc[rxsc_idx].hw_sc_idx =
		atl_to_hw_sc_idx(rxsc_idx, cfg->sc_sa);
	cfg->atl_rxsc[rxsc_idx].sw_secy = ctx->secy;
	cfg->atl_rxsc[rxsc_idx].sw_rxsc = ctx->rx_sc;
	atl_nic_dbg("add rxsc: rxsc_idx=%u, hw_sc_idx=%u, rxsc=%p\n", rxsc_idx,
		    cfg->atl_rxsc[rxsc_idx].hw_sc_idx,
		    cfg->atl_rxsc[rxsc_idx].sw_rxsc);

	if (netif_carrier_ok(nic->ndev) && netif_running(ctx->secy->netdev))
		ret = atl_set_rxsc(&nic->hw, rxsc_idx);

	if (ret < 0)
		return ret;

	set_bit(rxsc_idx, &cfg->rxsc_idx_busy);

	return 0;
}

static int atl_mdo_upd_rxsc(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	int rxsc_idx;

	rxsc_idx = atl_get_rxsc_idx_from_rxsc(&nic->hw, ctx->rx_sc);
	if (rxsc_idx < 0)
		return -ENOENT;

	if (ctx->prepare)
		return 0;

	if (netif_carrier_ok(nic->ndev) && netif_running(ctx->secy->netdev))
		return atl_set_rxsc(&nic->hw, rxsc_idx);

	return 0;
}

static int atl_clear_rxsc(struct atl_nic *nic, const int rxsc_idx,
			  enum atl_clear_type clear_type)
{
	struct atl_hw *hw = &nic->hw;
	struct atl_macsec_rxsc *rx_sc = &hw->macsec_cfg.atl_rxsc[rxsc_idx];
	int ret = 0;
	int sa_num;

	for_each_set_bit (sa_num, &rx_sc->rx_sa_idx_busy, ATL_MACSEC_MAX_SA) {
		ret = atl_clear_rxsa(nic, rx_sc, sa_num, clear_type);
		if (ret)
			return ret;
	}

	if (clear_type & ATL_CLEAR_HW) {
		struct aq_mss_ingress_preclass_record pre_class_record;
		struct aq_mss_ingress_prectlf_record rx_prectlf;
		struct aq_mss_ingress_sc_record sc_record;

		/* if last rxsc is removed then pass macsec packets to host */
		if (atl_macsec_bridge &&
		    hweight_long(hw->macsec_cfg.rxsc_idx_busy) == 1) {
			memset(&rx_prectlf, 0, sizeof(rx_prectlf));
			rx_prectlf.match_type = 4; /* Match eth_type only */
			rx_prectlf.action = 0;
			aq_mss_set_ingress_prectlf_record(hw, &rx_prectlf, 0);
			aq_mss_set_packet_edit_control(hw, 0xb068);
		}

		memset(&pre_class_record, 0, sizeof(pre_class_record));
		memset(&sc_record, 0, sizeof(sc_record));

		ret = aq_mss_set_ingress_preclass_record(hw, &pre_class_record,
							 2 * rxsc_idx);
		if (ret) {
			atl_dev_err(
				"aq_mss_set_ingress_preclass_record failed with %d\n",
				ret);
			return ret;
		}

		ret = aq_mss_set_ingress_preclass_record(hw, &pre_class_record,
							 2 * rxsc_idx + 1);
		if (ret) {
			atl_dev_err(
				"aq_mss_set_ingress_preclass_record failed with %d\n",
				ret);
			return ret;
		}

		sc_record.fresh = 1;
		ret = aq_mss_set_ingress_sc_record(hw, &sc_record,
						   rx_sc->hw_sc_idx);
		if (ret)
			return ret;
	}

	if (clear_type & ATL_CLEAR_SW) {
		clear_bit(rxsc_idx, &hw->macsec_cfg.rxsc_idx_busy);
		hw->macsec_cfg.atl_rxsc[rxsc_idx].sw_secy = NULL;
		hw->macsec_cfg.atl_rxsc[rxsc_idx].sw_rxsc = NULL;
	}

	return ret;
}

static int atl_mdo_del_rxsc(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	enum atl_clear_type clear_type = ATL_CLEAR_SW;
	int rxsc_idx;

	rxsc_idx = atl_get_rxsc_idx_from_rxsc(&nic->hw, ctx->rx_sc);
	if (rxsc_idx < 0)
		return -ENOENT;

	if (ctx->prepare)
		return 0;

	if (netif_carrier_ok(nic->ndev))
		clear_type = ATL_CLEAR_ALL;

	return atl_clear_rxsc(nic, rxsc_idx, clear_type);
}

static int atl_update_rxsa(struct atl_hw *hw, const unsigned int sc_idx,
			   const struct macsec_secy *secy,
			   const struct macsec_rx_sa *rx_sa,
			   const unsigned char *key, const unsigned char an)
{
	struct aq_mss_ingress_sakey_record sa_key_record;
	struct aq_mss_ingress_sa_record sa_record;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
	const u32 next_pn = rx_sa->next_pn;
#else
	const u32 next_pn = rx_sa->next_pn_halves.lower;
#endif
	const int sa_idx = sc_idx | an;
	int ret = 0;

	atl_dev_dbg("set rx_sa %d: active=%d, next_pn=%d\n", an, rx_sa->active,
		    next_pn);

	memset(&sa_record, 0, sizeof(sa_record));
	sa_record.valid = rx_sa->active;
	sa_record.fresh = 1;
	sa_record.next_pn = next_pn;

	ret = aq_mss_set_ingress_sa_record(hw, &sa_record, sa_idx);
	if (ret) {
		atl_dev_err("aq_mss_set_ingress_sa_record failed with %d\n",
			    ret);
		return ret;
	}

	if (!key)
		return ret;

	memset(&sa_key_record, 0, sizeof(sa_key_record));
	memcpy(&sa_key_record.key, key, secy->key_len);

	switch (secy->key_len) {
	case ATL_MACSEC_KEY_LEN_128_BIT:
		sa_key_record.key_len = 0;
		break;
	case ATL_MACSEC_KEY_LEN_192_BIT:
		sa_key_record.key_len = 1;
		break;
	case ATL_MACSEC_KEY_LEN_256_BIT:
		sa_key_record.key_len = 2;
		break;
	default:
		return -1;
	}

	atl_rotate_keys(&sa_key_record.key, secy->key_len);

	ret = aq_mss_set_ingress_sakey_record(hw, &sa_key_record, sa_idx);
	if (ret)
		atl_dev_err("aq_mss_set_ingress_sakey_record failed with %d\n",
			    ret);

	return ret;
}

static int atl_mdo_add_rxsa(struct macsec_context *ctx)
{
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	const struct macsec_secy *secy = ctx->secy;
	struct atl_hw *hw = &nic->hw;
	int rxsc_idx;

	rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, rx_sc);
	if (rxsc_idx < 0)
		return -EINVAL;

	if (ctx->prepare)
		return 0;

	set_bit(ctx->sa.assoc_num,
		&hw->macsec_cfg.atl_rxsc[rxsc_idx].rx_sa_idx_busy);

	memcpy(hw->macsec_cfg.atl_rxsc[rxsc_idx].rx_sa_key[ctx->sa.assoc_num],
	       ctx->sa.key, secy->key_len);

	if (netif_carrier_ok(nic->ndev) && netif_running(secy->netdev))
		return atl_update_rxsa(
			hw, hw->macsec_cfg.atl_rxsc[rxsc_idx].hw_sc_idx, secy,
			ctx->sa.rx_sa, ctx->sa.key, ctx->sa.assoc_num);

	return 0;
}
static int atl_mdo_upd_rxsa(struct macsec_context *ctx)
{
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	const struct macsec_secy *secy = ctx->secy;
	struct atl_hw *hw = &nic->hw;
	int rxsc_idx;

	rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, rx_sc);
	if (rxsc_idx < 0)
		return -EINVAL;

	if (ctx->prepare)
		return 0;

	if (netif_carrier_ok(nic->ndev) && netif_running(secy->netdev))
		return atl_update_rxsa(
			hw, hw->macsec_cfg.atl_rxsc[rxsc_idx].hw_sc_idx, secy,
			ctx->sa.rx_sa, NULL, ctx->sa.assoc_num);

	return 0;
}

static int atl_clear_rxsa(struct atl_nic *nic, struct atl_macsec_rxsc *atl_rxsc,
			  const int sa_num, enum atl_clear_type clear_type)
{
	int sa_idx = atl_rxsc->hw_sc_idx | sa_num;
	struct atl_hw *hw = &nic->hw;
	int ret = 0;

	if (clear_type & ATL_CLEAR_SW)
		clear_bit(sa_num, &atl_rxsc->rx_sa_idx_busy);

	if ((clear_type & ATL_CLEAR_HW) && netif_carrier_ok(nic->ndev)) {
		struct aq_mss_ingress_sakey_record sa_key_record;
		struct aq_mss_ingress_sa_record sa_record;

		memset(&sa_key_record, 0, sizeof(sa_key_record));
		memset(&sa_record, 0, sizeof(sa_record));

		sa_record.fresh = 1;
		ret = aq_mss_set_ingress_sa_record(hw, &sa_record, sa_idx);
		if (ret)
			return ret;

		return aq_mss_set_ingress_sakey_record(hw, &sa_key_record,
						       sa_idx);
	}

	return ret;
}

static int atl_mdo_del_rxsa(struct macsec_context *ctx)
{
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	int rxsc_idx;

	rxsc_idx = atl_get_rxsc_idx_from_rxsc(&nic->hw, rx_sc);
	if (rxsc_idx < 0)
		return -EINVAL;

	if (ctx->prepare)
		return 0;

	return atl_clear_rxsa(nic, &nic->hw.macsec_cfg.atl_rxsc[rxsc_idx],
			      ctx->sa.assoc_num, ATL_CLEAR_ALL);
}

static int atl_mdo_get_dev_stats(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_hw *hw = &nic->hw;
	struct atl_macsec_common_stats *stats = &hw->macsec_cfg.stats;

	if (ctx->prepare)
		return 0;

	atl_get_macsec_common_stats(hw, stats);

	ctx->stats.dev_stats->OutPktsUntagged = stats->out.untagged_pkts;
	ctx->stats.dev_stats->InPktsUntagged = stats->in.untagged_pkts;
	ctx->stats.dev_stats->OutPktsTooLong = stats->out.too_long;
	ctx->stats.dev_stats->InPktsNoTag = stats->in.notag_pkts;
	ctx->stats.dev_stats->InPktsBadTag = stats->in.bad_tag_pkts;
	ctx->stats.dev_stats->InPktsUnknownSCI = stats->in.unknown_sci_pkts;
	ctx->stats.dev_stats->InPktsNoSCI = stats->in.no_sci_pkts;
	ctx->stats.dev_stats->InPktsOverrun = 0;

	return 0;
}

static int atl_mdo_get_tx_sc_stats(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_macsec_tx_sc_stats *stats;
	struct atl_macsec_txsc *atl_txsc;
	struct atl_hw *hw = &nic->hw;
	int txsc_idx;

	txsc_idx = atl_get_txsc_idx_from_secy(hw, ctx->secy);
	if (txsc_idx < 0)
		return -ENOENT;

	if (ctx->prepare)
		return 0;

	atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	stats = &atl_txsc->stats;
	atl_get_txsc_stats(hw, atl_txsc->hw_sc_idx, &atl_txsc->stats);

	ctx->stats.tx_sc_stats->OutPktsProtected = stats->sc_protected_pkts;
	ctx->stats.tx_sc_stats->OutPktsEncrypted = stats->sc_encrypted_pkts;
	ctx->stats.tx_sc_stats->OutOctetsProtected = stats->sc_protected_octets;
	ctx->stats.tx_sc_stats->OutOctetsEncrypted = stats->sc_encrypted_octets;

	return 0;
}

static int atl_mdo_get_tx_sa_stats(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_macsec_tx_sa_stats *stats;
	struct atl_macsec_txsc *atl_txsc;
	const struct macsec_secy *secy;
	struct atl_hw *hw = &nic->hw;
	struct macsec_tx_sa *tx_sa;
	unsigned int sa_idx;
	int txsc_idx;
	u32 next_pn;
	int ret;

	txsc_idx = atl_get_txsc_idx_from_secy(hw, ctx->secy);
	if (txsc_idx < 0)
		return -EINVAL;

	if (ctx->prepare)
		return 0;

	atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	sa_idx = atl_txsc->hw_sc_idx | ctx->sa.assoc_num;
	stats = &atl_txsc->tx_sa_stats[ctx->sa.assoc_num];
	ret = atl_get_txsa_stats(hw, sa_idx, stats);
	if (ret)
		return ret;

	ctx->stats.tx_sa_stats->OutPktsProtected = stats->sa_protected_pkts;
	ctx->stats.tx_sa_stats->OutPktsEncrypted = stats->sa_encrypted_pkts;

	secy = atl_txsc->sw_secy;
	tx_sa = rcu_dereference_bh(secy->tx_sc.sa[ctx->sa.assoc_num]);
	ret = atl_get_txsa_next_pn(hw, sa_idx, &next_pn);
	if (ret == 0) {
		spin_lock_bh(&tx_sa->lock);
		tx_sa->next_pn = next_pn;
		spin_unlock_bh(&tx_sa->lock);
	}

	return ret;
}

static int atl_mdo_get_rx_sc_stats(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_macsec_rx_sa_stats *stats;
	struct atl_macsec_rxsc *atl_rxsc;
	struct atl_hw *hw = &nic->hw;
	unsigned int sa_idx;
	int rxsc_idx;
	int ret = 0;
	int i;

	rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, ctx->rx_sc);
	if (rxsc_idx < 0)
		return -ENOENT;

	if (ctx->prepare)
		return 0;

	atl_rxsc = &hw->macsec_cfg.atl_rxsc[rxsc_idx];
	for (i = 0; i < MACSEC_NUM_AN; i++) {
		if (!test_bit(i, &atl_rxsc->rx_sa_idx_busy))
			continue;

		stats = &atl_rxsc->rx_sa_stats[i];
		sa_idx = atl_rxsc->hw_sc_idx | i;
		ret = atl_get_rxsa_stats(hw, sa_idx, stats);
		if (ret)
			break;

		ctx->stats.rx_sc_stats->InOctetsValidated +=
			stats->validated_octets;
		ctx->stats.rx_sc_stats->InOctetsDecrypted +=
			stats->decrypted_octets;
		ctx->stats.rx_sc_stats->InPktsUnchecked +=
			stats->unchecked_pkts;
		ctx->stats.rx_sc_stats->InPktsDelayed += stats->delayed_pkts;
		ctx->stats.rx_sc_stats->InPktsOK += stats->ok_pkts;
		ctx->stats.rx_sc_stats->InPktsInvalid += stats->invalid_pkts;
		ctx->stats.rx_sc_stats->InPktsLate += stats->late_pkts;
		ctx->stats.rx_sc_stats->InPktsNotValid += stats->not_valid_pkts;
		ctx->stats.rx_sc_stats->InPktsNotUsingSA += stats->not_using_sa;
		ctx->stats.rx_sc_stats->InPktsUnusedSA += stats->unused_sa;
	}

	return ret;
}

static int atl_mdo_get_rx_sa_stats(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_macsec_rx_sa_stats *stats;
	struct atl_macsec_rxsc *atl_rxsc;
	struct atl_hw *hw = &nic->hw;
	struct macsec_rx_sa *rx_sa;
	unsigned int sa_idx;
	int rxsc_idx;
	u32 next_pn;
	int ret;

	rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, ctx->rx_sc);
	if (rxsc_idx < 0)
		return -EINVAL;

	if (ctx->prepare)
		return 0;

	atl_rxsc = &hw->macsec_cfg.atl_rxsc[rxsc_idx];
	stats = &atl_rxsc->rx_sa_stats[ctx->sa.assoc_num];
	sa_idx = atl_rxsc->hw_sc_idx | ctx->sa.assoc_num;
	ret = atl_get_rxsa_stats(hw, sa_idx, stats);
	if (ret)
		return ret;

	ctx->stats.rx_sa_stats->InPktsOK = stats->ok_pkts;
	ctx->stats.rx_sa_stats->InPktsInvalid = stats->invalid_pkts;
	ctx->stats.rx_sa_stats->InPktsNotValid = stats->not_valid_pkts;
	ctx->stats.rx_sa_stats->InPktsNotUsingSA = stats->not_using_sa;
	ctx->stats.rx_sa_stats->InPktsUnusedSA = stats->unused_sa;

	rx_sa = rcu_dereference_bh(atl_rxsc->sw_rxsc->sa[ctx->sa.assoc_num]);
	ret = atl_get_rxsa_next_pn(hw, sa_idx, &next_pn);
	if (ret == 0) {
		spin_lock_bh(&rx_sa->lock);
		rx_sa->next_pn = next_pn;
		spin_unlock_bh(&rx_sa->lock);
	}

	return ret;
}

static int atl_apply_txsc_cfg(struct atl_hw *hw, const int txsc_idx)
{
	struct atl_macsec_txsc *atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	const struct macsec_secy *secy = atl_txsc->sw_secy;
	struct macsec_tx_sa *tx_sa;
	int ret = 0;
	int i;

	if (!netif_running(secy->netdev))
		return ret;

	ret = atl_set_txsc(hw, txsc_idx);
	if (ret)
		return ret;

	for (i = 0; i < MACSEC_NUM_AN; i++) {
		tx_sa = rcu_dereference_bh(secy->tx_sc.sa[i]);
		if (tx_sa) {
			ret = atl_update_txsa(hw, atl_txsc->hw_sc_idx, secy,
					      tx_sa, atl_txsc->tx_sa_key[i], i);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static int atl_apply_rxsc_cfg(struct atl_hw *hw, const int rxsc_idx)
{
	struct atl_macsec_rxsc *atl_rxsc = &hw->macsec_cfg.atl_rxsc[rxsc_idx];
	const struct macsec_secy *secy = atl_rxsc->sw_secy;
	struct macsec_rx_sa *rx_sa;
	int ret = 0;
	int i;

	if (!netif_running(secy->netdev))
		return ret;

	ret = atl_set_rxsc(hw, rxsc_idx);
	if (ret)
		return ret;

	for (i = 0; i < MACSEC_NUM_AN; i++) {
		rx_sa = rcu_dereference_bh(atl_rxsc->sw_rxsc->sa[i]);
		if (rx_sa) {
			ret = atl_update_rxsa(hw, atl_rxsc->hw_sc_idx, secy,
					      rx_sa, atl_rxsc->rx_sa_key[i], i);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static int atl_clear_secy(struct atl_nic *nic, const struct macsec_secy *secy,
			  enum atl_clear_type clear_type)
{
	struct atl_hw *hw = &nic->hw;
	struct macsec_rx_sc *rx_sc;
	int txsc_idx;
	int rxsc_idx;
	int ret = 0;

	txsc_idx = atl_get_txsc_idx_from_secy(hw, secy);
	if (txsc_idx >= 0) {
		ret = atl_clear_txsc(nic, txsc_idx, clear_type);
		if (ret)
			return ret;
	}

	for (rx_sc = rcu_dereference_bh(secy->rx_sc); rx_sc;
	     rx_sc = rcu_dereference_bh(rx_sc->next)) {
		rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, rx_sc);
		if (rxsc_idx < 0)
			continue;

		ret = atl_clear_rxsc(nic, rxsc_idx, clear_type);
		if (ret)
			return ret;
	}

	return ret;
}

static int atl_apply_secy_cfg(struct atl_hw *hw, const struct macsec_secy *secy)
{
	struct macsec_rx_sc *rx_sc;
	int txsc_idx;
	int rxsc_idx;
	int ret = 0;

	txsc_idx = atl_get_txsc_idx_from_secy(hw, secy);
	if (txsc_idx >= 0)
		atl_apply_txsc_cfg(hw, txsc_idx);

	for (rx_sc = rcu_dereference_bh(secy->rx_sc); rx_sc && rx_sc->active;
	     rx_sc = rcu_dereference_bh(rx_sc->next)) {
		rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, rx_sc);
		if (unlikely(rxsc_idx < 0))
			continue;

		ret = atl_apply_rxsc_cfg(hw, rxsc_idx);
		if (ret)
			return ret;
	}

	return ret;
}

static int atl_apply_macsec_cfg(struct atl_hw *hw)
{
	int ret = 0;
	int i;

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (hw->macsec_cfg.txsc_idx_busy & BIT(i)) {
			ret = atl_apply_txsc_cfg(hw, i);
			if (ret)
				return ret;
		}
	}

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (hw->macsec_cfg.rxsc_idx_busy & BIT(i)) {
			ret = atl_apply_rxsc_cfg(hw, i);
			if (ret)
				return ret;
		}
	}

	return ret;
}

const struct macsec_ops atl_macsec_ops = {
	.mdo_dev_open = atl_mdo_dev_open,
	.mdo_dev_stop = atl_mdo_dev_stop,
	.mdo_add_secy = atl_mdo_add_secy,
	.mdo_upd_secy = atl_mdo_upd_secy,
	.mdo_del_secy = atl_mdo_del_secy,
	.mdo_add_rxsc = atl_mdo_add_rxsc,
	.mdo_upd_rxsc = atl_mdo_upd_rxsc,
	.mdo_del_rxsc = atl_mdo_del_rxsc,
	.mdo_add_rxsa = atl_mdo_add_rxsa,
	.mdo_upd_rxsa = atl_mdo_upd_rxsa,
	.mdo_del_rxsa = atl_mdo_del_rxsa,
	.mdo_add_txsa = atl_mdo_add_txsa,
	.mdo_upd_txsa = atl_mdo_upd_txsa,
	.mdo_del_txsa = atl_mdo_del_txsa,
	.mdo_get_dev_stats = atl_mdo_get_dev_stats,
	.mdo_get_tx_sc_stats = atl_mdo_get_tx_sc_stats,
	.mdo_get_tx_sa_stats = atl_mdo_get_tx_sa_stats,
	.mdo_get_rx_sc_stats = atl_mdo_get_rx_sc_stats,
	.mdo_get_rx_sa_stats = atl_mdo_get_rx_sa_stats,
};

static int atl_sa_from_sa_idx(enum atl_macsec_sc_sa sc_sa, int sa_idx)
{
	switch (sc_sa) {
	case atl_macsec_sa_sc_4sa_8sc:
		return sa_idx & 3;
	case atl_macsec_sa_sc_2sa_16sc:
		return sa_idx & 1;
	case atl_macsec_sa_sc_1sa_32sc:
		return 0;
	default:
		WARN_ONCE(1, "Invalid sc_sa");
	}
	return -EINVAL;
}

static int atl_sc_idx_from_sa_idx(enum atl_macsec_sc_sa sc_sa, int sa_idx)
{
	switch (sc_sa) {
	case atl_macsec_sa_sc_4sa_8sc:
		return sa_idx & ~3;
	case atl_macsec_sa_sc_2sa_16sc:
		return sa_idx & ~1;
	case atl_macsec_sa_sc_1sa_32sc:
		return sa_idx;
	default:
		WARN_ONCE(1, "Invalid sc_sa");
	}
	return -EINVAL;
}

static void atl_check_txsa_expiration(struct atl_nic *nic)
{
	u32 egress_sa_expired, egress_sa_threshold_expired;
	unsigned int sc_idx = 0, txsc_idx = 0;
	struct atl_macsec_txsc *atl_txsc;
	const struct macsec_secy *secy;
	struct atl_hw *hw = &nic->hw;
	enum atl_macsec_sc_sa sc_sa;
	struct macsec_tx_sa *tx_sa;
	unsigned char an = 0;
	int ret;
	int i;

	sc_sa = hw->macsec_cfg.sc_sa;

	ret = aq_mss_get_egress_sa_expired(hw, &egress_sa_expired);
	if (unlikely(ret))
		return;

	ret = aq_mss_get_egress_sa_threshold_expired(
		hw, &egress_sa_threshold_expired);

	for (i = 0; i < ATL_MACSEC_MAX_SA; i++) {
		if (egress_sa_expired & BIT(i)) {
			an = atl_sa_from_sa_idx(sc_sa, i);
			sc_idx = atl_sc_idx_from_sa_idx(sc_sa, i);
			txsc_idx = atl_get_txsc_idx_from_sc_idx(sc_sa, sc_idx);
			if (txsc_idx < 0)
				continue;

			atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
			if (!(hw->macsec_cfg.txsc_idx_busy & BIT(txsc_idx))) {
				netdev_warn(
					nic->ndev,
					"PN threshold expired on invalid TX SC");
				continue;
			}

			secy = atl_txsc->sw_secy;
			if (!netif_running(secy->netdev)) {
				netdev_warn(
					nic->ndev,
					"PN threshold expired on down TX SC");
				continue;
			}

			if (unlikely(!(atl_txsc->tx_sa_idx_busy & BIT(an)))) {
				netdev_warn(
					nic->ndev,
					"PN threshold expired on invalid TX SA");
				continue;
			}

			tx_sa = rcu_dereference_bh(secy->tx_sc.sa[an]);
			macsec_pn_wrapped((struct macsec_secy *)secy, tx_sa);
		}
	}

	aq_mss_set_egress_sa_expired(hw, egress_sa_expired);
	if (likely(!ret))
		aq_mss_set_egress_sa_threshold_expired(
			hw, egress_sa_threshold_expired);
}

void atl_macsec_work(struct atl_nic *nic)
{
	if ((nic->hw.mcp.caps_low & atl_fw2_macsec) == 0)
		return;

	if (!netif_carrier_ok(nic->ndev))
		return;

	rtnl_lock();
	atl_check_txsa_expiration(nic);
	rtnl_unlock();
}
#endif
