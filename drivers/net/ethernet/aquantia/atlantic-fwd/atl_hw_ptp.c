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

#include <linux/mdio.h>

#include "atl_hw_ptp.h"
#include "atl_desc.h"
#include "atl_mdio.h"

#define FRAC_PER_NS 0x100000000LL

#define ATL_HW_MAC_COUNTER_HZ   312500000ll
#define ATL_HW_PHY_COUNTER_HZ   160000000ll

/* register address for bitfield PTP Digital Clock Read Enable */
#define HW_ATL_PCS_PTP_CLOCK_READ_ENABLE_ADR 0x00004628
/* lower bit position of bitfield PTP Digital Clock Read Enable */
#define HW_ATL_PCS_PTP_CLOCK_READ_ENABLE_SHIFT 4
/* width of bitfield PTP Digital Clock Read Enable */
#define HW_ATL_PCS_PTP_CLOCK_READ_ENABLE_WIDTH 1

/* register address for ptp counter reading */
#define HW_ATL_PCS_PTP_TS_VAL_ADDR(index) (0x00004900 + (index) * 0x4)

static void hw_atl_pcs_ptp_clock_read_enable(struct atl_hw *hw,
				      u32 ptp_clock_read_enable)
{
	atl_write_bits(hw, HW_ATL_PCS_PTP_CLOCK_READ_ENABLE_ADR,
			    HW_ATL_PCS_PTP_CLOCK_READ_ENABLE_SHIFT,
			    HW_ATL_PCS_PTP_CLOCK_READ_ENABLE_WIDTH,
			    ptp_clock_read_enable);
}

static u32 hw_atl_pcs_ptp_clock_get(struct atl_hw *hw, u32 index)
{
	return atl_read(hw, HW_ATL_PCS_PTP_TS_VAL_ADDR(index));
}

#define get_ptp_ts_val_u64(self, indx) \
	((u64)(hw_atl_pcs_ptp_clock_get(self, indx) & 0xffff))

void hw_atl_get_ptp_ts(struct atl_hw *hw, u64 *stamp)
{
	u64 ns;

	hw_atl_pcs_ptp_clock_read_enable(hw, 1);
	hw_atl_pcs_ptp_clock_read_enable(hw, 0);
	ns = (get_ptp_ts_val_u64(hw, 0) +
	      (get_ptp_ts_val_u64(hw, 1) << 16)) * NSEC_PER_SEC +
	     (get_ptp_ts_val_u64(hw, 3) +
	      (get_ptp_ts_val_u64(hw, 4) << 16));

	*stamp = ns + hw->ptp_clk_offset;
}

static void hw_atl_adj_params_get(u64 freq, s64 adj, u32 *ns, u32 *fns)
{
	/* For accuracy, the digit is extended */
	s64 base_ns = ((adj + NSEC_PER_SEC) * NSEC_PER_SEC);
	u64 nsi_frac = 0;
	u64 nsi;

	base_ns = div64_s64(base_ns, freq);
	nsi = div64_u64(base_ns, NSEC_PER_SEC);

	if (base_ns != nsi * NSEC_PER_SEC) {
		s64 divisor = div64_s64((s64)NSEC_PER_SEC * NSEC_PER_SEC,
					base_ns - nsi * NSEC_PER_SEC);
		nsi_frac = div64_s64(FRAC_PER_NS * NSEC_PER_SEC, divisor);
	}

	*ns = (u32)nsi;
	*fns = (u32)nsi_frac;
}

static void
hw_atl_mac_adj_param_calc(struct ptp_adj_freq *ptp_adj_freq, u64 phyfreq,
			  u64 macfreq)
{
	s64 adj_fns_val;
	s64 fns_in_sec_phy = phyfreq * (ptp_adj_freq->fns_phy +
					FRAC_PER_NS * ptp_adj_freq->ns_phy);
	s64 fns_in_sec_mac = macfreq * (ptp_adj_freq->fns_mac +
					FRAC_PER_NS * ptp_adj_freq->ns_mac);
	s64 fault_in_sec_phy = FRAC_PER_NS * NSEC_PER_SEC - fns_in_sec_phy;
	s64 fault_in_sec_mac = FRAC_PER_NS * NSEC_PER_SEC - fns_in_sec_mac;
	/* MAC MCP counter freq is macfreq / 4 */
	s64 diff_in_mcp_overflow = (fault_in_sec_mac - fault_in_sec_phy) *
				   4 * FRAC_PER_NS;

	diff_in_mcp_overflow = div64_s64(diff_in_mcp_overflow,
					 ATL_HW_MAC_COUNTER_HZ);
	adj_fns_val = (ptp_adj_freq->fns_mac + FRAC_PER_NS *
		       ptp_adj_freq->ns_mac) + diff_in_mcp_overflow;

	ptp_adj_freq->mac_ns_adj = div64_s64(adj_fns_val, FRAC_PER_NS);
	ptp_adj_freq->mac_fns_adj = adj_fns_val - ptp_adj_freq->mac_ns_adj *
				    FRAC_PER_NS;
}

int hw_atl_adj_sys_clock(struct atl_hw *hw, s64 delta)
{
	hw->ptp_clk_offset += delta;

	atl_write(hw, ATL_RX_SPARE_CTRL0, lower_32_bits(hw->ptp_clk_offset));
	atl_write(hw, ATL_RX_SPARE_CTRL1, upper_32_bits(hw->ptp_clk_offset));

	return 0;
}

int hw_atl_ts_to_sys_clock(struct atl_hw *hw, u64 ts, u64 *time)
{
	*time = hw->ptp_clk_offset + ts;
	return 0;
}

int hw_atl_adj_clock_freq(struct atl_hw *hw, s32 ppb)
{
	struct ptp_msg_fw_request fwreq;
	struct atl_mcp *mcp = &hw->mcp;

	memset(&fwreq, 0, sizeof(fwreq));

	fwreq.msg_id = ptp_adj_freq_msg;
	hw_atl_adj_params_get(ATL_HW_MAC_COUNTER_HZ, ppb,
				 &fwreq.adj_freq.ns_mac,
				 &fwreq.adj_freq.fns_mac);
	hw_atl_adj_params_get(ATL_HW_PHY_COUNTER_HZ, ppb,
				 &fwreq.adj_freq.ns_phy,
				 &fwreq.adj_freq.fns_phy);
	hw_atl_mac_adj_param_calc(&fwreq.adj_freq,
				     ATL_HW_PHY_COUNTER_HZ,
				     ATL_HW_MAC_COUNTER_HZ);

	return mcp->ops->send_ptp_req(hw, &fwreq);
}

int hw_atl_gpio_pulse(struct atl_hw *hw, u32 index, u64 start, u32 period)
{
	struct ptp_msg_fw_request fwreq;
	struct atl_mcp *mcp = &hw->mcp;

	memset(&fwreq, 0, sizeof(fwreq));

	fwreq.msg_id = ptp_gpio_ctrl_msg;
	fwreq.gpio_ctrl.index = index;
	fwreq.gpio_ctrl.period = period;
	/* Apply time offset */
	fwreq.gpio_ctrl.start = start;

	return mcp->ops->send_ptp_req(hw, &fwreq);
}

int hw_atl_extts_gpio_enable(struct atl_hw *hw, u32 index, u32 enable)
{
	/* Enable/disable Sync1588 GPIO Timestamping */
	return atl_mdio_write(hw, 0, MDIO_MMD_PCS, 0xc611, enable ? 0x71 : 0);
}

int hw_atl_get_sync_ts(struct atl_hw *hw, u64 *ts)
{
	u16 nsec_l = 0;
	u16 nsec_h = 0;
	u16 sec_l = 0;
	u16 sec_h = 0;
	int ret;

	if (!ts)
		return -1;

	/* PTP external GPIO clock seconds count 15:0 */
	ret = atl_mdio_read(hw, 0, MDIO_MMD_PCS, 0xc914, &sec_l);
	/* PTP external GPIO clock seconds count 31:16 */
	if (!ret)
		ret = atl_mdio_read(hw, 0, MDIO_MMD_PCS, 0xc915, &sec_h);
	/* PTP external GPIO clock nanoseconds count 15:0 */
	if (!ret)
		ret = atl_mdio_read(hw, 0, MDIO_MMD_PCS, 0xc916, &nsec_l);
	/* PTP external GPIO clock nanoseconds count 31:16 */
	if (!ret)
		ret = atl_mdio_read(hw, 0, MDIO_MMD_PCS, 0xc917, &nsec_h);

	*ts = ((u64)nsec_h << 16) + nsec_l + (((u64)sec_h << 16) + sec_l) * NSEC_PER_SEC;

	return ret;
}

u16 hw_atl_rx_extract_ts(struct atl_hw *hw, u8 *p, unsigned int len,
			 u64 *timestamp)
{
	unsigned int offset = 14;
	struct ethhdr *eth;
	u64 sec;
	u8 *ptr;
	u32 ns;

	if (len <= offset || !timestamp)
		return 0;

	/* The TIMESTAMP in the end of package has following format:
	 * (big-endian)
	 *   struct {
	 *     uint64_t sec;
	 *     uint32_t ns;
	 *     uint16_t stream_id;
	 *   };
	 */
	ptr = p + (len - offset);
	memcpy(&sec, ptr, sizeof(sec));
	ptr += sizeof(sec);
	memcpy(&ns, ptr, sizeof(ns));

	sec = be64_to_cpu(sec) & 0xffffffffffffllu;
	ns = be32_to_cpu(ns);
	*timestamp = sec * NSEC_PER_SEC + ns + hw->ptp_clk_offset;

	eth = (struct ethhdr *)p;

	return (eth->h_proto == htons(ETH_P_1588)) ? 12 : 14;
}

void hw_atl_extract_hwts(struct atl_hw *hw, struct atl_rx_desc_hwts_wb *hwts_wb,
			 u64 *timestamp)
{
	u64 tmp, sec, ns;

	sec = 0;
	tmp = hwts_wb->sec_lw0 & 0x3ff;
	sec += tmp;
	tmp = (u64)((hwts_wb->sec_lw1 >> 16) & 0xffff) << 10;
	sec += tmp;
	tmp = (u64)(hwts_wb->sec_hw & 0xfff) << 26;
	sec += tmp;
	tmp = (u64)((hwts_wb->sec_hw >> 22) & 0x3ff) << 38;
	sec += tmp;
	if (sec == 0xffffffffffff && hwts_wb->ns == 0xffffffff)
		return;
	ns = sec * NSEC_PER_SEC + hwts_wb->ns;
	if (timestamp)
		*timestamp = ns + hw->ptp_clk_offset;
}
