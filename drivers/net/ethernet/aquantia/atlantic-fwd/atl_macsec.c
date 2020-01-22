/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include "atl_common.h"
#ifdef NETIF_F_HW_MACSEC
#include "atl_macsec.h"
#include <linux/rtnetlink.h>

#include "macsec/macsec_api.h"
#define ATL_MACSEC_KEY_LEN_128_BIT 16
#define ATL_MACSEC_KEY_LEN_192_BIT 24
#define ATL_MACSEC_KEY_LEN_256_BIT 32

enum atl_clear_type {
	/* update HW configuration */
	ATL_CLEAR_HW = BIT(0),
	/* update SW configuration (busy bits, pointers) */
	ATL_CLEAR_SW = BIT(1),
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
static int atl_macsec_apply_cfg(struct atl_hw *hw);
static int atl_macsec_apply_secy_cfg(struct atl_hw *hw,
				     const struct macsec_secy *secy);

static void ether_addr_to_mac(uint32_t mac[2], unsigned char *emac)
{
	uint32_t tmp[2] = { 0 };

	memcpy(((uint8_t *)tmp) + 2, emac, ETH_ALEN);

	mac[0] = swab32(tmp[1]);
	mac[1] = swab32(tmp[0]);
}

/* There's a 1:1 mapping between SecY and TX SC */
static int atl_get_txsc_idx_from_secy(struct atl_hw *hw,
				      const struct macsec_secy *secy)
{
	int i;

	if (unlikely(secy == NULL))
		return -1;

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (hw->macsec_cfg.atl_txsc[i].sw_secy == secy) {
			return i;
			break;
		}
	}
	return -1;
}

static int atl_get_rxsc_idx_from_rxsc(struct atl_hw *hw,
				      const struct macsec_rx_sc *rxsc)
{
	int i;

	if (unlikely(rxsc == NULL))
		return -1;

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (hw->macsec_cfg.atl_rxsc[i].sw_rxsc == rxsc)
			return i;
	}

	return -1;
}

static int atl_macsec_txsc_idx_from_sc_idx(enum atl_macsec_sc_sa sc_sa,
					   unsigned int sc_idx,
					   unsigned int *txsc_idx)
{
	switch (sc_sa) {
	case atl_macsec_sa_sc_4sa_8sc:
		*txsc_idx = sc_idx >> 2;
		return 0;
	case atl_macsec_sa_sc_2sa_16sc:
		*txsc_idx = sc_idx >> 1;
		return 0;
	case atl_macsec_sa_sc_1sa_32sc:
		*txsc_idx = sc_idx;
		return 0;
	default:
		WARN_ONCE(1, "Invalid sc_sa");
	}
	return -EINVAL;
}

/* Rotate keys uint32_t[8] */
static void atl_rotate_keys(uint32_t (*key)[8], int key_len)
{
	uint32_t tmp[8] = { 0 };

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
	(((uint64_t)stat_field[1] << 32) | stat_field[0])

static int atl_macsec_get_common_stats(struct atl_hw *hw,
				       struct atl_macsec_common_stats *stats)
{
	AQ_API_SEC_EgressCommonCounters egress_counters;
	AQ_API_SEC_IngressCommonCounters ingress_counters;
	int ret;

	/* MACSEC counters */
	ret = AQ_API_GetIngressCommonCounters(hw, &ingress_counters);
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

	ret = AQ_API_GetEgressCommonCounters(hw, &egress_counters);
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

static int atl_macsec_get_rx_sa_stats(struct atl_hw *hw, int sa_idx,
				      struct atl_macsec_rx_sa_stats *stats)
{
	AQ_API_SEC_IngressSACounters i_sa_counters;
	int ret;

	ret = AQ_API_GetIngressSACounters(hw, &i_sa_counters, sa_idx);
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

static int atl_macsec_get_tx_sa_stats(struct atl_hw *hw, int sa_idx,
				      struct atl_macsec_tx_sa_stats *stats)
{
	AQ_API_SEC_EgressSACounters e_sa_counters;
	int ret;

	ret = AQ_API_GetEgressSACounters(hw, &e_sa_counters, sa_idx);
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

static int atl_macsec_get_tx_sa_next_pn(struct atl_hw *hw, int sa_idx, u32 *pn)
{
	AQ_API_SEC_EgressSARecord matchSARecord;
	int ret;

	ret = AQ_API_GetEgressSARecord(hw, &matchSARecord, sa_idx);
	if (likely(!ret))
		*pn = matchSARecord.next_pn;

	return ret;
}

static int atl_macsec_get_rx_sa_next_pn(struct atl_hw *hw, int sa_idx, u32 *pn)
{
	AQ_API_SEC_IngressSARecord matchSARecord;
	int ret;

	ret = AQ_API_GetIngressSARecord(hw, &matchSARecord, sa_idx);
	if (likely(!ret))
		*pn = (!matchSARecord.sat_nextpn) ? matchSARecord.next_pn : 0;

	return ret;
}

static int atl_macsec_get_tx_sc_stats(struct atl_hw *hw, int sc_idx,
				      struct atl_macsec_tx_sc_stats *stats)
{
	AQ_API_SEC_EgressSCCounters e_sc_counters;
	int ret;

	ret = AQ_API_GetEgressSCCounters(hw, &e_sc_counters, sc_idx);
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

	atl_macsec_get_common_stats(hw, &hw->macsec_cfg.stats);

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (!(hw->macsec_cfg.txsc_idx_busy & BIT(i)))
			continue;
		atl_txsc = &hw->macsec_cfg.atl_txsc[i];

		ret = atl_macsec_get_tx_sc_stats(hw, atl_txsc->hw_sc_idx,
						 &atl_txsc->stats);
		if (ret)
			return ret;

		for (assoc_num = 0; assoc_num < MACSEC_NUM_AN; assoc_num++) {
			if (!test_bit(assoc_num, &atl_txsc->tx_sa_idx_busy))
				continue;
			sa_idx = atl_txsc->hw_sc_idx | assoc_num;
			ret = atl_macsec_get_tx_sa_stats(
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

			ret = atl_macsec_get_rx_sa_stats(
				hw, sa_idx, &atl_rxsc->rx_sa_stats[assoc_num]);
			if (ret)
				return ret;
		}
	}

	return ret;
}

int atl_init_macsec(struct atl_hw *hw)
{
	struct macsec_msg_fw_request msg = { 0 };
	struct macsec_msg_fw_response resp = { 0 };
	int num_ctl_ether_types = 0;
	int index = 0, tbl_idx;
	int ret;

	rtnl_lock();

	if (hw->mcp.ops->send_macsec_req != NULL) {
		struct macsec_cfg cfg = { 0 };

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
	uint32_t ctl_ether_types[1] = { ETH_P_PAE };
	for (index = 0; index < ARRAY_SIZE(ctl_ether_types); index++) {
		if (ctl_ether_types[index] == 0)
			continue;
		AQ_API_SEC_EgressCTLFRecord egressCTLFRecord = { 0 };
		egressCTLFRecord.eth_type = ctl_ether_types[index];
		egressCTLFRecord.match_type = 4; /* Match eth_type only */
		egressCTLFRecord.match_mask = 0xf; /* match for eth_type */
		egressCTLFRecord.action = 0; /* Bypass MACSEC modules */
		tbl_idx = NUMROWS_EGRESSCTLFRECORD - num_ctl_ether_types - 1;
		AQ_API_SetEgressCTLFRecord(hw, &egressCTLFRecord, tbl_idx);

		AQ_API_SEC_IngressPreCTLFRecord ingressPreCTLFRecord = { 0 };
		ingressPreCTLFRecord.eth_type = ctl_ether_types[index];
		ingressPreCTLFRecord.match_type = 4; /* Match eth_type only */
		ingressPreCTLFRecord.match_mask = 0xf; /* match for eth_type */
		ingressPreCTLFRecord.action = 0; /* Bypass MACSEC modules */
		tbl_idx =
			NUMROWS_INGRESSPRECTLFRECORD - num_ctl_ether_types - 1;
		AQ_API_SetIngressPreCTLFRecord(hw, &ingressPreCTLFRecord,
					       tbl_idx);

		num_ctl_ether_types++;
	}

	ret = atl_macsec_apply_cfg(hw);

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
		ret = atl_macsec_apply_secy_cfg(&nic->hw, ctx->secy);

	return ret;
}

static int atl_mdo_dev_stop(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);

	return atl_clear_secy(nic, ctx->secy, ATL_CLEAR_HW);
}

static int atl_set_txsc(struct atl_hw *hw, int txsc_idx)
{
	struct atl_macsec_txsc *atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	const struct macsec_secy *secy = atl_txsc->sw_secy;
	unsigned int sc_idx = atl_txsc->hw_sc_idx;
	int ret = 0;

	AQ_API_SEC_EgressClassRecord matchEgressClassRecord = { 0 };

	ether_addr_to_mac(matchEgressClassRecord.mac_sa,
			  secy->netdev->dev_addr);

	atl_dev_dbg("set secy: sci %#llx, sc_idx=%d, protect=%d, curr_an=%d\n",
		    secy->sci, sc_idx, secy->protect_frames,
		    secy->tx_sc.encoding_sa);

	matchEgressClassRecord.sci[1] = swab32(secy->sci & 0xffffffff);
	matchEgressClassRecord.sci[0] = swab32(secy->sci >> 32);
	matchEgressClassRecord.sci_mask = 0;

	matchEgressClassRecord.sa_mask = 0x3f;

	matchEgressClassRecord.action = 0; /* forward to SA/SC table */
	matchEgressClassRecord.valid = 1;

	matchEgressClassRecord.sc_idx = sc_idx;

	matchEgressClassRecord.sc_sa = hw->macsec_cfg.sc_sa;

	ret = AQ_API_SetEgressClassRecord(hw, &matchEgressClassRecord,
					  txsc_idx);
	if (ret)
		return ret;

	AQ_API_SEC_EgressSCRecord matchSCRecord = { 0 };

	matchSCRecord.protect = secy->protect_frames;
	if (secy->tx_sc.encrypt)
		matchSCRecord.tci |= BIT(1);
	if (secy->tx_sc.scb)
		matchSCRecord.tci |= BIT(2);
	if (secy->tx_sc.send_sci)
		matchSCRecord.tci |= BIT(3);
	if (secy->tx_sc.end_station)
		matchSCRecord.tci |= BIT(4);
	/* The C bit is clear if and only if the Secure Data is
	 * exactly the same as the User Data and the ICV is 16 octets long.
	 */
	if (!(secy->icv_len == 16 && !secy->tx_sc.encrypt))
		matchSCRecord.tci |= BIT(0);

	matchSCRecord.an_roll = 0;

	switch (secy->key_len) {
	case ATL_MACSEC_KEY_LEN_128_BIT:
		matchSCRecord.sak_len = 0;
		break;
	case ATL_MACSEC_KEY_LEN_192_BIT:
		matchSCRecord.sak_len = 1;
		break;
	case ATL_MACSEC_KEY_LEN_256_BIT:
		matchSCRecord.sak_len = 2;
		break;
	default:
		WARN_ONCE(1, "Invalid sc_sa");
		return -EINVAL;
	}

	matchSCRecord.curr_an = secy->tx_sc.encoding_sa;
	matchSCRecord.valid = 1;
	matchSCRecord.fresh = 1;

	return AQ_API_SetEgressSCRecord(hw, &matchSCRecord, sc_idx);
}

static uint32_t sc_idx_max(const enum atl_macsec_sc_sa sc_sa)
{
	uint32_t result = 0;

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

static uint32_t to_hw_sc_idx(const uint32_t sc_idx,
			     const enum atl_macsec_sc_sa sc_sa)
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
	uint32_t txsc_idx;
	int ret = 0;

	sc_sa = sc_sa_from_num_an(MACSEC_NUM_AN);
	if (sc_sa == atl_macsec_sa_sc_not_used)
		return -EINVAL;

	if (hweight32(hw->macsec_cfg.txsc_idx_busy) >= sc_idx_max(sc_sa))
		return -ENOSPC;

	txsc_idx = ffz(hw->macsec_cfg.txsc_idx_busy);
	if (txsc_idx == ATL_MACSEC_MAX_SC)
		return -ENOSPC;

	if (ctx->prepare)
		return 0;

	hw->macsec_cfg.sc_sa = sc_sa;
	hw->macsec_cfg.atl_txsc[txsc_idx].hw_sc_idx =
		to_hw_sc_idx(txsc_idx, sc_sa);
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
	int txsc_idx = atl_get_txsc_idx_from_secy(&nic->hw, ctx->secy);
	const struct macsec_secy *secy;
	struct atl_hw *hw = &nic->hw;
	int ret = 0;

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
	AQ_API_SEC_EgressClassRecord matchEgressClassRecord = { 0 };
	AQ_API_SEC_EgressSCRecord matchSCRecord = { 0 };
	int ret = 0;
	int sa_num;

	for_each_set_bit (sa_num, &tx_sc->tx_sa_idx_busy, ATL_MACSEC_MAX_SA) {
		ret = atl_clear_txsa(nic, tx_sc, sa_num, clear_type);
		if (ret)
			return ret;
	}

	if (clear_type & ATL_CLEAR_HW) {
		ret = AQ_API_SetEgressClassRecord(hw, &matchEgressClassRecord,
						  txsc_idx);
		if (ret)
			return ret;

		matchSCRecord.fresh = 1;
		ret = AQ_API_SetEgressSCRecord(hw, &matchSCRecord,
					       tx_sc->hw_sc_idx);
		if (ret)
			return ret;
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

	return atl_clear_secy(nic, ctx->secy, ATL_CLEAR_HW | ATL_CLEAR_SW);
}

static int atl_update_txsa(struct atl_hw *hw, unsigned int sc_idx,
			   const struct macsec_secy *secy,
			   const struct macsec_tx_sa *tx_sa,
			   const unsigned char *key, unsigned char an)
{
	AQ_API_SEC_EgressSAKeyRecord matchKeyRecord = { 0 };
	AQ_API_SEC_EgressSARecord matchSARecord = { 0 };
	unsigned int sa_idx = sc_idx | an;
	int ret = 0;

	atl_dev_dbg("set tx_sa %d: active=%d, next_pn=%d\n", an, tx_sa->active,
		    tx_sa->next_pn);

	matchSARecord.valid = tx_sa->active;
	matchSARecord.fresh = 1;
	matchSARecord.next_pn = tx_sa->next_pn;

	ret = AQ_API_SetEgressSARecord(hw, &matchSARecord, sa_idx);
	if (ret) {
		atl_dev_err("AQ_API_SetEgressSARecord failed with %d\n", ret);
		return ret;
	}

	if (!key)
		return ret;

	memcpy(&matchKeyRecord.key, key, secy->key_len);

	atl_rotate_keys(&matchKeyRecord.key, secy->key_len);

	ret = AQ_API_SetEgressSAKeyRecord(hw, &matchKeyRecord, sa_idx);
	if (ret)
		atl_dev_err("AQ_API_SetEgressSAKeyRecord failed with %d\n",
			    ret);

	return ret;
}

static int atl_mdo_add_txsa(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	int txsc_idx = atl_get_txsc_idx_from_secy(&nic->hw, ctx->secy);
	struct atl_hw *hw = &nic->hw;
	struct atl_macsec_txsc *atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	const struct macsec_secy *secy = ctx->secy;

	if (ctx->prepare)
		return 0;

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
	int txsc_idx = atl_get_txsc_idx_from_secy(&nic->hw, ctx->secy);
	struct atl_hw *hw = &nic->hw;
	struct atl_macsec_txsc *atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	const struct macsec_secy *secy = ctx->secy;

	if (ctx->prepare)
		return 0;

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
		AQ_API_SEC_EgressSARecord matchSARecord = { 0 };
		matchSARecord.fresh = 1;

		ret = AQ_API_SetEgressSARecord(hw, &matchSARecord, sa_idx);
		if (ret)
			return ret;

		AQ_API_SEC_EgressSAKeyRecord matchKeyRecord = { 0 };

		return AQ_API_SetEgressSAKeyRecord(hw, &matchKeyRecord, sa_idx);
	}

	return 0;
}

static int atl_mdo_del_txsa(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	int txsc_idx = atl_get_txsc_idx_from_secy(&nic->hw, ctx->secy);

	if (ctx->prepare)
		return 0;

	return atl_clear_txsa(nic, &nic->hw.macsec_cfg.atl_txsc[txsc_idx],
			      ctx->sa.assoc_num, ATL_CLEAR_HW | ATL_CLEAR_SW);
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

static int atl_set_rxsc(struct atl_hw *hw, const uint32_t rxsc_idx)
{
	const struct atl_macsec_rxsc *atl_rxsc =
		&hw->macsec_cfg.atl_rxsc[rxsc_idx];
	AQ_API_SEC_IngressPreClassRecord pre_class_record = { 0 };
	const struct macsec_rx_sc *rx_sc = atl_rxsc->sw_rxsc;
	const struct macsec_secy *secy = atl_rxsc->sw_secy;
	const uint32_t hw_sc_idx = atl_rxsc->hw_sc_idx;
	AQ_API_SEC_IngressSCRecord sc_record = { 0 };
	int ret = 0;

	atl_dev_dbg("set rx_sc: rxsc_idx=%d, sci %#llx, hw_sc_idx=%d\n",
		    rxsc_idx, rx_sc->sci, hw_sc_idx);

	pre_class_record.sci[1] = swab32(rx_sc->sci & 0xffffffff);
	pre_class_record.sci[0] = swab32(rx_sc->sci >> 32);
	pre_class_record.sci_mask = 0xff;
	/* match all MACSEC ethertype packets */
	pre_class_record.eth_type = ETH_P_MACSEC;
	pre_class_record.eth_type_mask = 0x3;

	ether_addr_to_mac(pre_class_record.mac_sa, (char *)&rx_sc->sci);
	pre_class_record.sa_mask = 0x3f;

	pre_class_record.an_mask = hw->macsec_cfg.sc_sa;
	pre_class_record.sc_idx = hw_sc_idx;
	/* strip SecTAG & forward for decryption */
	pre_class_record.action = 0x0;
	pre_class_record.valid = 1;

	ret = AQ_API_SetIngressPreClassRecord(hw, &pre_class_record,
					      2 * rxsc_idx + 1);
	if (ret) {
		atl_dev_err("AQ_API_SetIngressPreClassRecord failed with %d\n",
			    ret);
		return ret;
	}

	/* If SCI is absent, then match by SA alone */
	pre_class_record.sci_mask = 0;
	pre_class_record.sci_from_table = 1;

	ret = AQ_API_SetIngressPreClassRecord(hw, &pre_class_record,
					      2 * rxsc_idx);
	if (ret) {
		atl_dev_err("AQ_API_SetIngressPreClassRecord failed with %d\n",
			    ret);
		return ret;
	}

	sc_record.validate_frames =
		atl_rxsc_validate_frames(secy->validate_frames);
	if (secy->replay_protect) {
		sc_record.replay_protect = 1;
		sc_record.anti_replay_window = secy->replay_window;
	}
	sc_record.valid = 1;
	sc_record.fresh = 1;

	ret = AQ_API_SetIngressSCRecord(hw, &sc_record, hw_sc_idx);
	if (ret) {
		atl_dev_err("AQ_API_SetIngressSCRecord failed with %d\n", ret);
		return ret;
	}

	return ret;
}

static int atl_mdo_add_rxsc(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_macsec_cfg *cfg = &nic->hw.macsec_cfg;
	const uint32_t rxsc_idx_max = sc_idx_max(cfg->sc_sa);
	uint32_t rxsc_idx;
	int ret = 0;

	if (hweight32(cfg->rxsc_idx_busy) >= rxsc_idx_max)
		return -ENOSPC;

	rxsc_idx = ffz(cfg->rxsc_idx_busy);
	if (rxsc_idx >= rxsc_idx_max)
		return -ENOSPC;

	if (ctx->prepare)
		return 0;

	cfg->atl_rxsc[rxsc_idx].hw_sc_idx = to_hw_sc_idx(rxsc_idx, cfg->sc_sa);
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
	int rxsc_idx = atl_get_rxsc_idx_from_rxsc(&nic->hw, ctx->rx_sc);

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
		AQ_API_SEC_IngressPreClassRecord pre_class_record = { 0 };
		AQ_API_SEC_IngressSCRecord sc_record = { 0 };

		ret = AQ_API_SetIngressPreClassRecord(hw, &pre_class_record,
						      2 * rxsc_idx);
		if (ret) {
			atl_dev_err(
				"AQ_API_SetIngressPreClassRecord failed with %d\n",
				ret);
			return ret;
		}

		ret = AQ_API_SetIngressPreClassRecord(hw, &pre_class_record,
						      2 * rxsc_idx + 1);
		if (ret) {
			atl_dev_err(
				"AQ_API_SetIngressPreClassRecord failed with %d\n",
				ret);
			return ret;
		}

		sc_record.fresh = 1;
		ret = AQ_API_SetIngressSCRecord(hw, &sc_record,
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
	int rxsc_idx = atl_get_rxsc_idx_from_rxsc(&nic->hw, ctx->rx_sc);
	enum atl_clear_type clear_type = ATL_CLEAR_SW;

	if (rxsc_idx < 0)
		return -ENOENT;

	if (ctx->prepare)
		return 0;

	if (netif_carrier_ok(nic->ndev))
		clear_type |= ATL_CLEAR_HW;

	return atl_clear_rxsc(nic, rxsc_idx, clear_type);
}

static int atl_update_rxsa(struct atl_hw *hw, const unsigned int sc_idx,
			   const struct macsec_secy *secy,
			   const struct macsec_rx_sa *rx_sa,
			   const unsigned char *key, const unsigned char an)
{
	AQ_API_SEC_IngressSAKeyRecord sa_key_record = { 0 };
	AQ_API_SEC_IngressSARecord sa_record = { 0 };
	const int sa_idx = sc_idx | an;
	int ret = 0;

	atl_dev_dbg("set rx_sa %d: active=%d, next_pn=%d\n", an, rx_sa->active,
		    rx_sa->next_pn);

	sa_record.valid = rx_sa->active;
	sa_record.fresh = 1;
	sa_record.next_pn = rx_sa->next_pn;

	ret = AQ_API_SetIngressSARecord(hw, &sa_record, sa_idx);
	if (ret) {
		atl_dev_err("AQ_API_SetIngressSARecord failed with %d\n", ret);
		return ret;
	}

	if (!key)
		return ret;

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

	ret = AQ_API_SetIngressSAKeyRecord(hw, &sa_key_record, sa_idx);
	if (ret)
		atl_dev_err("AQ_API_SetIngressSAKeyRecord failed with %d\n",
			    ret);

	return ret;
}

static int atl_mdo_add_rxsa(struct macsec_context *ctx)
{
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	const struct macsec_secy *secy = ctx->secy;
	struct atl_hw *hw = &nic->hw;
	const int rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, rx_sc);

	WARN_ON(rxsc_idx < 0);

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
	const int rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, rx_sc);

	WARN_ON(rxsc_idx < 0);

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
		AQ_API_SEC_IngressSAKeyRecord sa_key_record = { 0 };
		AQ_API_SEC_IngressSARecord sa_record = { 0 };

		sa_record.fresh = 1;
		ret = AQ_API_SetIngressSARecord(hw, &sa_record, sa_idx);
		if (ret)
			return ret;

		return AQ_API_SetIngressSAKeyRecord(hw, &sa_key_record, sa_idx);
	}

	return ret;
}

static int atl_mdo_del_rxsa(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	const struct macsec_rx_sc *rx_sc = ctx->sa.rx_sa->sc;
	const int rxsc_idx = atl_get_rxsc_idx_from_rxsc(&nic->hw, rx_sc);

	WARN_ON(rxsc_idx < 0);

	if (ctx->prepare)
		return 0;

	return atl_clear_rxsa(nic, &nic->hw.macsec_cfg.atl_rxsc[rxsc_idx],
			      ctx->sa.assoc_num, ATL_CLEAR_HW | ATL_CLEAR_SW);
}

static int atl_mdo_get_dev_stats(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_hw *hw = &nic->hw;
	struct atl_macsec_common_stats *stats = &hw->macsec_cfg.stats;

	if (ctx->prepare)
		return 0;

	atl_macsec_get_common_stats(hw, stats);

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
	struct atl_hw *hw = &nic->hw;
	int txsc_idx = atl_get_txsc_idx_from_secy(hw, ctx->secy);
	struct atl_macsec_txsc *atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	struct atl_macsec_tx_sc_stats *stats = &atl_txsc->stats;

	if (ctx->prepare)
		return 0;

	atl_macsec_get_tx_sc_stats(hw, atl_txsc->hw_sc_idx, &atl_txsc->stats);

	ctx->stats.tx_sc_stats->OutPktsProtected = stats->sc_protected_pkts;
	ctx->stats.tx_sc_stats->OutPktsEncrypted = stats->sc_encrypted_pkts;
	ctx->stats.tx_sc_stats->OutOctetsProtected = stats->sc_protected_octets;
	ctx->stats.tx_sc_stats->OutOctetsEncrypted = stats->sc_encrypted_octets;

	return 0;
}

static int atl_mdo_get_tx_sa_stats(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_hw *hw = &nic->hw;
	int txsc_idx = atl_get_txsc_idx_from_secy(hw, ctx->secy);
	struct atl_macsec_txsc *atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	struct macsec_tx_sa *tx_sa;
	struct atl_macsec_tx_sa_stats *stats =
		&atl_txsc->tx_sa_stats[ctx->sa.assoc_num];
	unsigned int sa_idx;
	int ret;

	if (ctx->prepare)
		return 0;

	sa_idx = atl_txsc->hw_sc_idx | ctx->sa.assoc_num;
	ret = atl_macsec_get_tx_sa_stats(hw, sa_idx, stats);
	if (ret)
		return ret;

	ctx->stats.tx_sa_stats->OutPktsProtected = stats->sa_protected_pkts;
	ctx->stats.tx_sa_stats->OutPktsEncrypted = stats->sa_encrypted_pkts;

	tx_sa = atl_txsc->sw_secy->tx_sc.sa[ctx->sa.assoc_num];
	ret = atl_macsec_get_tx_sa_next_pn(hw, sa_idx, &tx_sa->next_pn);
	return ret;
}

static int atl_mdo_get_rx_sc_stats(struct macsec_context *ctx)
{
	struct atl_nic *nic = netdev_priv(ctx->netdev);
	struct atl_hw *hw = &nic->hw;
	const int rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, ctx->rx_sc);
	struct atl_macsec_rxsc *atl_rxsc = &hw->macsec_cfg.atl_rxsc[rxsc_idx];
	struct atl_macsec_rx_sa_stats *stats = NULL;
	unsigned int sa_idx;
	int ret = 0;
	int i;

	if (ctx->prepare)
		return 0;

	for (i = 0; i < MACSEC_NUM_AN; i++) {
		if (!test_bit(i, &atl_rxsc->rx_sa_idx_busy))
			continue;

		stats = &atl_rxsc->rx_sa_stats[i];
		sa_idx = atl_rxsc->hw_sc_idx | i;
		ret = atl_macsec_get_rx_sa_stats(hw, sa_idx, stats);
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
	struct atl_hw *hw = &nic->hw;
	const int rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, ctx->rx_sc);
	struct atl_macsec_rxsc *atl_rxsc = &hw->macsec_cfg.atl_rxsc[rxsc_idx];
	struct atl_macsec_rx_sa_stats *stats =
		&atl_rxsc->rx_sa_stats[ctx->sa.assoc_num];
	struct macsec_rx_sa *rx_sa;
	unsigned int sa_idx;
	int ret;

	if (ctx->prepare)
		return 0;

	sa_idx = atl_rxsc->hw_sc_idx | ctx->sa.assoc_num;
	ret = atl_macsec_get_rx_sa_stats(hw, sa_idx, stats);
	if (ret)
		return ret;

	ctx->stats.rx_sa_stats->InPktsOK = stats->ok_pkts;
	ctx->stats.rx_sa_stats->InPktsInvalid = stats->invalid_pkts;
	ctx->stats.rx_sa_stats->InPktsNotValid = stats->not_valid_pkts;
	ctx->stats.rx_sa_stats->InPktsNotUsingSA = stats->not_using_sa;
	ctx->stats.rx_sa_stats->InPktsUnusedSA = stats->unused_sa;

	rx_sa = atl_rxsc->sw_rxsc->sa[ctx->sa.assoc_num];
	ret = atl_macsec_get_rx_sa_next_pn(hw, sa_idx, &rx_sa->next_pn);

	return ret;
}

static int atl_macsec_apply_txsc_cfg(struct atl_hw *hw, const int txsc_idx)
{
	struct atl_macsec_txsc *atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
	const struct macsec_secy *secy = atl_txsc->sw_secy;
	int ret = 0;
	int i;

	if (!netif_running(secy->netdev))
		return ret;

	ret = atl_set_txsc(hw, txsc_idx);
	if (ret)
		return ret;

	for (i = 0; i < MACSEC_NUM_AN; i++) {
		if (secy->tx_sc.sa[i]) {
			ret = atl_update_txsa(hw, atl_txsc->hw_sc_idx, secy,
					      secy->tx_sc.sa[i],
					      atl_txsc->tx_sa_key[i], i);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static int atl_macsec_apply_rxsc_cfg(struct atl_hw *hw, const int rxsc_idx)
{
	struct atl_macsec_rxsc *atl_rxsc = &hw->macsec_cfg.atl_rxsc[rxsc_idx];
	const struct macsec_rx_sc *rx_sc = atl_rxsc->sw_rxsc;
	const struct macsec_secy *secy = atl_rxsc->sw_secy;
	int ret = 0;
	int i;

	if (!netif_running(secy->netdev))
		return ret;

	ret = atl_set_rxsc(hw, rxsc_idx);
	if (ret)
		return ret;

	for (i = 0; i < MACSEC_NUM_AN; i++) {
		if (rx_sc->sa[i]) {
			ret = atl_update_rxsa(hw, atl_rxsc->hw_sc_idx, secy,
					      rx_sc->sa[i],
					      atl_rxsc->rx_sa_key[i], i);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static int atl_clear_secy(struct atl_nic *nic, const struct macsec_secy *secy,
			  enum atl_clear_type clear_type)
{
	int txsc_idx = atl_get_txsc_idx_from_secy(&nic->hw, secy);
	struct atl_hw *hw = &nic->hw;
	struct macsec_rx_sc *rx_sc;
	int rxsc_idx;
	int ret = 0;

	if (txsc_idx >= 0) {
		ret = atl_clear_txsc(nic, txsc_idx, clear_type);
		if (ret)
			return ret;
	}

	for (rx_sc = rcu_dereference_bh(secy->rx_sc); rx_sc;
	     rx_sc = rcu_dereference_bh(rx_sc->next)) {
		rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, rx_sc);
		WARN_ON(rxsc_idx < 0);
		if (rxsc_idx < 0)
			continue;

		ret = atl_clear_rxsc(nic, rxsc_idx, clear_type);
		if (ret)
			return ret;
	}

	return ret;
}

static int atl_macsec_apply_secy_cfg(struct atl_hw *hw,
				     const struct macsec_secy *secy)
{
	int txsc_idx = atl_get_txsc_idx_from_secy(hw, secy);
	struct macsec_rx_sc *rx_sc;
	int rxsc_idx;
	int ret = 0;

	atl_macsec_apply_txsc_cfg(hw, txsc_idx);

	for (rx_sc = rcu_dereference_bh(secy->rx_sc); rx_sc && rx_sc->active;
	     rx_sc = rcu_dereference_bh(rx_sc->next)) {
		rxsc_idx = atl_get_rxsc_idx_from_rxsc(hw, rx_sc);
		WARN_ON(rxsc_idx < 0);
		if (unlikely(rxsc_idx < 0))
			continue;

		ret = atl_macsec_apply_rxsc_cfg(hw, rxsc_idx);
		if (ret)
			return ret;
	}

	return ret;
}

static int atl_macsec_apply_cfg(struct atl_hw *hw)
{
	int i;
	int ret = 0;

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (hw->macsec_cfg.txsc_idx_busy & BIT(i)) {
			ret = atl_macsec_apply_txsc_cfg(hw, i);
			if (ret)
				return ret;
		}
	}

	for (i = 0; i < ATL_MACSEC_MAX_SC; i++) {
		if (hw->macsec_cfg.rxsc_idx_busy & BIT(i)) {
			ret = atl_macsec_apply_rxsc_cfg(hw, i);
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

static int atl_macsec_sa_from_sa_idx(enum atl_macsec_sc_sa sc_sa, int sa_idx)
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

static int atl_macsec_sc_idx_from_sa_idx(enum atl_macsec_sc_sa sc_sa,
					 int sa_idx)
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

void atl_macsec_check_txsa_expiration(struct atl_nic *nic)
{
	uint32_t egress_sa_expired, egress_sa_threshold_expired;
	unsigned int sc_idx = 0, txsc_idx = 0;
	struct atl_macsec_txsc *atl_txsc;
	struct atl_hw *hw = &nic->hw;
	enum atl_macsec_sc_sa sc_sa;
	struct macsec_tx_sa *tx_sa;
	unsigned char an = 0;
	int ret;
	int i;

	sc_sa = hw->macsec_cfg.sc_sa;

	ret = AQ_API_GetEgressSAExpired(hw, &egress_sa_expired);
	if (unlikely(ret))
		return;

	ret = AQ_API_GetEgressSAThresholdExpired(hw,
						 &egress_sa_threshold_expired);

	for (i = 0; i < ATL_MACSEC_MAX_SA; i++) {
		if (egress_sa_expired & BIT(i)) {
			an = atl_macsec_sa_from_sa_idx(sc_sa, i);
			sc_idx = atl_macsec_sc_idx_from_sa_idx(sc_sa, i);
			atl_macsec_txsc_idx_from_sc_idx(sc_sa, sc_idx,
							&txsc_idx);
			atl_txsc = &hw->macsec_cfg.atl_txsc[txsc_idx];
			if (!(hw->macsec_cfg.txsc_idx_busy & BIT(txsc_idx))) {
				netdev_warn(
					nic->ndev,
					"PN threshold expired on invalid TX SC");
				continue;
			}
			if (!netif_running(atl_txsc->sw_secy->netdev)) {
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

			tx_sa = atl_txsc->sw_secy->tx_sc.sa[an];

			spin_lock_bh(&tx_sa->lock);
			tx_sa->next_pn = 0;
			tx_sa->active = false;
			netdev_dbg(nic->ndev,
				   "PN wrapped, transitioning to !oper\n");
			spin_unlock_bh(&tx_sa->lock);
		}
	}

	AQ_API_SetEgressSAExpired(hw, egress_sa_expired);
	if (likely(!ret))
		AQ_API_SetEgressSAThresholdExpired(hw,
						   egress_sa_threshold_expired);
}

void atl_macsec_work(struct atl_nic *nic)
{
	if ((nic->hw.mcp.caps_low & atl_fw2_macsec) == 0)
		return;

	if (!netif_carrier_ok(nic->ndev))
		return;

	rtnl_lock();
	atl_macsec_check_txsa_expiration(nic);
	rtnl_unlock();
}
#endif
