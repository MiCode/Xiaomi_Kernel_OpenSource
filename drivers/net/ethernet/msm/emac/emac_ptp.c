/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* MSM EMAC Ethernet Controller PTP support
 */

#include <linux/net_tstamp.h>
#include "emac.h"
#include "emac_hw.h"
#include "emac_ptp.h"

static inline u32 clk_to_ptp_inc_value(u32 clk)
{
	u32 ns = 1000000000/clk;
	u32 fract = 0;
	u32 mask = 1 << 25;
	u32 c = clk >> 1;
	u32 v = 1000000000 % clk;

	while (v != 0 && mask) {
		if (v >= c) {
			fract |= mask;
			v -= c;
		}
		c >>= 1;
		mask >>= 1;
	}

	return (ns << 26) | fract;
}

static int emac_hw_1588_core_disable(struct emac_hw *hw)
{
	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
			  DIS_1588_CLKS, DIS_1588_CLKS);
	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR10,
			  DIS_1588, DIS_1588);
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG, 0, BYPASS_O);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_PTP_EXPANDED_INT_MASK, 0);
	wmb();

	CLI_HW_FLAG(PTP_EN);
	return 0;
}

static int emac_hw_1588_core_enable(struct emac_hw *hw,
				    enum emac_ptp_mode mode,
				    enum emac_ptp_clk_mode clk_mode,
				    enum emac_mac_speed mac_speed,
				    u32 rtc_ref_clkrate)
{
	u32 v;

	if (mode != emac_ptp_mode_slave) {
		emac_dbg(hw->adpt, hw, "mode %d not supported\n", mode);
		return -EINVAL;
	}

	if ((clk_mode != emac_ptp_clk_mode_oc_one_step) &&
	    (clk_mode != emac_ptp_clk_mode_oc_two_step)) {
		emac_dbg(hw->adpt, hw, "invalid ptp clk mode %d\n", clk_mode);
		return -EINVAL;
	}

	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
			  DIS_1588_CLKS, 0);
	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR10, DIS_1588, 0);
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG, BYPASS_O, 0);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_PTP_EXPANDED_INT_MASK, 0);
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_RTC_EXPANDED_CONFIG,
			  RTC_READ_MODE, RTC_READ_MODE);
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG, ATTACH_EN, 0);
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG, CLOCK_MODE_BMSK,
			  (clk_mode << CLOCK_MODE_SHFT));
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG, ETH_MODE_SW,
			  (mac_speed == emac_mac_speed_1000) ?
			  0 : ETH_MODE_SW);

	/* set RTC increment every 8ns to fit 125MHZ clock */
	v = clk_to_ptp_inc_value(rtc_ref_clkrate);

	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_INC_VALUE_2,
		     (v >> 16) & INC_VALUE_2_BMSK);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_INC_VALUE_1,
		     v & INC_VALUE_1_BMSK);
	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR10,
			  RD_CLR_1588, RD_CLR_1588);
	wmb();

	v = emac_reg_r32(hw, EMAC_1588, EMAC_P1588_PTP_EXPANDED_INT_STATUS);

	SET_HW_FLAG(PTP_EN);
	return 0;
}

static void rtc_settime(struct emac_hw *hw, const struct timespec *ts)
{
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_RTC_PRELOADED_5, 0);

	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_RTC_PRELOADED_4,
		     (ts->tv_sec >> 16) & RTC_PRELOADED_4_BMSK);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_RTC_PRELOADED_3,
		     ts->tv_sec & RTC_PRELOADED_3_BMSK);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_RTC_PRELOADED_2,
		     (ts->tv_nsec >> 16) & RTC_PRELOADED_2_BMSK);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_RTC_PRELOADED_1,
		     ts->tv_nsec & RTC_PRELOADED_1_BMSK);

	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_RTC_EXPANDED_CONFIG,
			  LOAD_RTC, LOAD_RTC);
	wmb();
}

static void rtc_gettime(struct emac_hw *hw, struct timespec *ts)
{
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_RTC_EXPANDED_CONFIG,
			  RTC_SNAPSHOT, RTC_SNAPSHOT);
	wmb();

	ts->tv_sec = emac_reg_field_r32(hw, EMAC_1588, EMAC_P1588_REAL_TIME_5,
					REAL_TIME_5_BMSK, REAL_TIME_5_SHFT);
	ts->tv_sec = (u64)ts->tv_sec << 32;
	ts->tv_sec |= emac_reg_field_r32(hw, EMAC_1588, EMAC_P1588_REAL_TIME_4,
					 REAL_TIME_4_BMSK, REAL_TIME_4_SHFT);
	ts->tv_sec <<= 16;
	ts->tv_sec |= emac_reg_field_r32(hw, EMAC_1588, EMAC_P1588_REAL_TIME_3,
					 REAL_TIME_3_BMSK, REAL_TIME_3_SHFT);

	ts->tv_nsec = emac_reg_field_r32(hw, EMAC_1588, EMAC_P1588_REAL_TIME_2,
					 REAL_TIME_2_BMSK, REAL_TIME_2_SHFT);
	ts->tv_nsec <<= 16;
	ts->tv_nsec |= emac_reg_field_r32(hw, EMAC_1588, EMAC_P1588_REAL_TIME_1,
					  REAL_TIME_1_BMSK, REAL_TIME_1_SHFT);
}

static void rtc_adjtime(struct emac_hw *hw, s64 delta)
{
	s32 delta_ns;
	s32 delta_sec;

	delta_sec = div_s64_rem(delta, 1000000000LL, &delta_ns);

	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_SEC_OFFSET_3, 0);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_SEC_OFFSET_2,
		     (delta_sec >> 16) & SEC_OFFSET_2_BMSK);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_SEC_OFFSET_1,
		     delta_sec & SEC_OFFSET_1_BMSK);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_NANO_OFFSET_2,
		     (delta_ns >> 16) & NANO_OFFSET_2_BMSK);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_NANO_OFFSET_1,
		     (delta_ns & NANO_OFFSET_1_BMSK));
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_ADJUST_RTC, 1);
	wmb();
}

int emac_ptp_init(struct emac_hw *hw)
{
	int ret = 0;

	if (!CHK_HW_FLAG(PTP_CAP)) {
		emac_err(hw->adpt, "not ieee-1588 capable\n");
		return -ENOTSUPP;
	}

	spin_lock_init(&hw->ptp_lock);
	ret = emac_hw_1588_core_disable(hw);

	return ret;
}

int emac_ptp_config(struct emac_hw *hw)
{
	struct timespec ts;
	int ret = 0;
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);

	if (CHK_HW_FLAG(PTP_EN))
		goto unlock_out;

	ret = emac_hw_1588_core_enable(hw,
				       emac_ptp_mode_slave,
				       hw->ptp_clk_mode,
				       emac_mac_speed_1000,
				       hw->rtc_ref_clkrate);
	if (ret)
		goto unlock_out;

	getnstimeofday(&ts);
	rtc_settime(hw, &ts);

unlock_out:
	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return ret;
}

int emac_ptp_stop(struct emac_hw *hw)
{
	int ret = 0;
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);

	if (CHK_HW_FLAG(PTP_EN))
		ret = emac_hw_1588_core_disable(hw);

	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return ret;
}

int emac_ptp_set_linkspeed(struct emac_hw *hw, enum emac_mac_speed link_speed)
{
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG, ETH_MODE_SW,
		  (link_speed == emac_mac_speed_1000) ? 0 : ETH_MODE_SW);
	wmb();
	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return 0;
}

int emac_ptp_settime(struct emac_hw *hw, const struct timespec *ts)
{
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);
	rtc_settime(hw, ts);
	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return 0;
}

int emac_ptp_gettime(struct emac_hw *hw, struct timespec *ts)
{
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);
	rtc_gettime(hw, ts);
	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return 0;
}

int emac_ptp_adjtime(struct emac_hw *hw, s64 delta)
{
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);
	rtc_adjtime(hw, delta);
	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return 0;
}

int emac_tstamp_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	struct hwtstamp_config cfg;

	if (!CHK_HW_FLAG(PTP_CAP))
		return -ENOTSUPP;

	if (copy_from_user(&cfg, ifr->ifr_data, sizeof(cfg)))
		return -EFAULT;

	switch (cfg.tx_type) {
	case HWTSTAMP_TX_OFF:
		emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
				  TX_TS_ENABLE, 0);
		wmb();
		CLI_HW_FLAG(TS_TX_EN);
		break;
	case HWTSTAMP_TX_ON:
		if (CHK_HW_FLAG(TS_TX_EN))
			break;
		emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
				  TX_TS_ENABLE, TX_TS_ENABLE);
		wmb();
		SET_HW_FLAG(TS_TX_EN);
		break;
	default:
		return -ERANGE;
	}

	switch (cfg.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		CLI_HW_FLAG(TS_RX_EN);
		break;
	default:
		cfg.rx_filter = HWTSTAMP_FILTER_ALL;
		if (CHK_HW_FLAG(TS_RX_EN))
			break;
		SET_HW_FLAG(TS_RX_EN);
		break;
	}

	return copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)) ?
		-EFAULT : 0;
}
