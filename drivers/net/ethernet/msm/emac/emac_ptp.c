/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/phy.h>
#include <linux/net_tstamp.h>
#include "emac.h"
#include "emac_hw.h"
#include "emac_ptp.h"

#define TS_TX_FIFO_SYNC_RST (TX_INDX_FIFO_SYNC_RST | TX_TS_FIFO_SYNC_RST)
#define TS_RX_FIFO_SYNC_RST (RX_TS_FIFO1_SYNC_RST  | RX_TS_FIFO2_SYNC_RST)
#define TS_FIFO_SYNC_RST    (TS_TX_FIFO_SYNC_RST | TS_RX_FIFO_SYNC_RST)

struct emac_tstamp_hw_delay {
	int phy_mode;
	u32 speed;
	u32 tx;
	u32 rx;
};

static const struct emac_tstamp_hw_delay emac_ptp_hw_delay[] = {
	{ PHY_INTERFACE_MODE_SGMII, 1000, 16, 60 },
	{ PHY_INTERFACE_MODE_SGMII, 100, 280, 100 },
	{ PHY_INTERFACE_MODE_SGMII, 10, 2400, 400 },
	{ 0 }
};

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

static const struct emac_tstamp_hw_delay *emac_get_ptp_hw_delay(u32 link_speed,
								int phy_mode)
{
	const struct emac_tstamp_hw_delay *info = emac_ptp_hw_delay;
	u32 speed;

	switch (link_speed) {
	case EMAC_LINK_SPEED_1GB_FULL:
		speed = 1000;
		break;
	case EMAC_LINK_SPEED_100_FULL:
	case EMAC_LINK_SPEED_100_HALF:
		speed = 100;
		break;
	case EMAC_LINK_SPEED_10_FULL:
	case EMAC_LINK_SPEED_10_HALF:
		speed = 10;
		break;
	default:
		speed = 0;
		break;
	}

	for (info = emac_ptp_hw_delay; info->phy_mode; info++) {
		if (info->phy_mode == phy_mode && info->speed == speed)
			return info;
	}

	return NULL;
}

static int emac_hw_adjust_tstamp_offset(struct emac_hw *hw,
					enum emac_ptp_clk_mode clk_mode,
					u32 link_speed)
{
	const struct emac_tstamp_hw_delay *delay_info;

	delay_info = emac_get_ptp_hw_delay(link_speed, hw->adpt->phy_mode);

	if (clk_mode == emac_ptp_clk_mode_oc_one_step) {
		u32 latency = (delay_info) ? delay_info->tx : 0;

		emac_reg_update32(hw, EMAC_1588, EMAC_P1588_TX_LATENCY,
				  TX_LATENCY_BMSK, latency << TX_LATENCY_SHFT);
		wmb();
	}

	if (delay_info) {
		hw->tstamp_rx_offset = delay_info->rx;
		hw->tstamp_tx_offset = delay_info->tx;
	} else {
		hw->tstamp_rx_offset = 0;
		hw->tstamp_tx_offset = 0;
	}

	return 0;
}

static int emac_hw_1588_core_disable(struct emac_hw *hw)
{
	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
			  DIS_1588_CLKS, DIS_1588_CLKS);
	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR10,
			  DIS_1588, DIS_1588);
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG,
			  BYPASS_O, BYPASS_O);
	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_PTP_EXPANDED_INT_MASK, 0);
	wmb();

	CLI_HW_FLAG(PTP_EN);
	return 0;
}

static int emac_hw_1588_core_enable(struct emac_hw *hw,
				    enum emac_ptp_mode mode,
				    enum emac_ptp_clk_mode clk_mode,
				    u32 link_speed,
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
	wmb(); /* ensure P1588_CTRL_REG is set before we proceed */

	emac_hw_adjust_tstamp_offset(hw, clk_mode, link_speed);

	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG, CLOCK_MODE_BMSK,
			  (clk_mode << CLOCK_MODE_SHFT));
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG, ETH_MODE_SW,
			  (link_speed == EMAC_LINK_SPEED_1GB_FULL) ?
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

	/* Reset the timestamp FIFO */
	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
			  TS_FIFO_SYNC_RST, TS_FIFO_SYNC_RST);
	wmb();
	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
			  TS_FIFO_SYNC_RST, 0);
	wmb();

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
				       EMAC_LINK_SPEED_1GB_FULL,
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

int emac_ptp_set_linkspeed(struct emac_hw *hw, u32 link_speed)
{
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);
	emac_reg_update32(hw, EMAC_1588, EMAC_P1588_CTRL_REG, ETH_MODE_SW,
		  (link_speed == EMAC_LINK_SPEED_1GB_FULL) ? 0 : ETH_MODE_SW);
	wmb(); /* ensure ETH_MODE_SW is set before we proceed */
	emac_hw_adjust_tstamp_offset(hw, hw->ptp_clk_mode, link_speed);
	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return 0;
}

static int emac_ptp_settime(struct emac_hw *hw, const struct timespec *ts)
{
	int ret = 0;
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);
	if (!CHK_HW_FLAG(PTP_EN))
		ret = -EPERM;
	else
		rtc_settime(hw, ts);
	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return ret;
}

static int emac_ptp_gettime(struct emac_hw *hw, struct timespec *ts)
{
	int ret = 0;
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);
	if (!CHK_HW_FLAG(PTP_EN))
		ret = -EPERM;
	else
		rtc_gettime(hw, ts);
	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return ret;
}

int emac_ptp_adjtime(struct emac_hw *hw, s64 delta)
{
	int ret = 0;
	unsigned long flag;

	spin_lock_irqsave(&hw->ptp_lock, flag);
	if (!CHK_HW_FLAG(PTP_EN))
		ret = -EPERM;
	else
		rtc_adjtime(hw, delta);
	spin_unlock_irqrestore(&hw->ptp_lock, flag);

	return ret;
}

int emac_tstamp_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	struct hwtstamp_config cfg;

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

		/* Reset the TX timestamp FIFO */
		emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
				  TS_TX_FIFO_SYNC_RST, TS_TX_FIFO_SYNC_RST);
		wmb();
		emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
				  TS_TX_FIFO_SYNC_RST, 0);
		wmb();

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

		/* Reset the RX timestamp FIFO */
		emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
				  TS_RX_FIFO_SYNC_RST, TS_RX_FIFO_SYNC_RST);
		wmb();
		emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
				  TS_RX_FIFO_SYNC_RST, 0);
		wmb();

		SET_HW_FLAG(TS_RX_EN);
		break;
	}

	return copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)) ?
		-EFAULT : 0;
}

static int emac_ptp_sysfs_cmd(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct emac_adapter *adpt = netdev_priv(to_net_dev(dev));
	struct timespec ts;
	int ret = -EINVAL;

	if (!strncmp(buf, "setTs", 5)) {
		getnstimeofday(&ts);
		ret = emac_ptp_settime(&adpt->hw, &ts);
		if (!ret)
			ret = count;
	}

	return ret;
}

static int emac_ptp_sysfs_tstamp_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct emac_adapter *adpt = netdev_priv(to_net_dev(dev));
	struct timespec ts = { 0 };
	struct timespec ts_now = { 0 };
	int count = PAGE_SIZE;
	int retval;

	retval = emac_ptp_gettime(&adpt->hw, &ts);
	if (retval)
		return retval;

	getnstimeofday(&ts_now);
	retval = scnprintf(buf, count,
			  "%12u.%09u tstamp  %12u.%08u time-of-day\n",
			  (int)ts.tv_sec, (int)ts.tv_nsec,
			  (int)ts_now.tv_sec, (int)ts_now.tv_nsec);

	return retval;
}

/* display ethernet mac time as well as the time of the next mac pps pulse */
static int emac_ptp_sysfs_mtnp_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct emac_adapter *adpt = netdev_priv(to_net_dev(dev));
	int                  count = PAGE_SIZE;
	struct timespec      ts;
	int                  ret;

	ret = emac_ptp_gettime(&adpt->hw, &ts);
	if (ret)
		return ret;

	return scnprintf(buf, count, "%ld %ld %ld %ld\n",
			 ts.tv_sec,
			 ts.tv_nsec,
			 (ts.tv_nsec != 0) ? ts.tv_sec : ts.tv_sec + 1,
			 NSEC_PER_SEC - ts.tv_nsec);
}

/* Do a "slam" of a very particular time into the time registers... */
static int emac_ptp_sysfs_slam(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct emac_adapter *adpt = netdev_priv(to_net_dev(dev));
	uint32_t             sec = 0;
	uint32_t             nsec = 0;
	int                  ret = -EINVAL;

	if (sscanf(buf, "%u %u", &sec, &nsec) == 2) {
		struct timespec ts = {sec, nsec};
		ret = emac_ptp_settime(&adpt->hw, &ts);
		if (ret) {
			pr_err("%s: emac_ptp_settime failed.\n", __func__);
			return ret;
		}
		ret = count;
	} else
		pr_err("%s: sscanf failed.\n", __func__);

	return ret;
}

/* Do a coarse time ajustment (ie. coarsely adjust (+/-) the time
 * registers by the passed offset)
 */
static int emac_ptp_sysfs_cadj(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct emac_adapter *adpt = netdev_priv(to_net_dev(dev));
	int64_t              offset = 0;
	int                  ret = -EINVAL;

	if (sscanf(buf, "%lld", &offset) == 1) {
		struct timespec ts;
		uint64_t        new_offset;
		uint32_t        sec;
		uint32_t        nsec;

		ret = emac_ptp_gettime(&adpt->hw, &ts);
		if (ret) {
			pr_err("%s: emac_ptp_gettime failed.\n", __func__);
			return ret;
		}

		sec  = ts.tv_sec;
		nsec = ts.tv_nsec;

		new_offset = (((uint64_t) sec * NSEC_PER_SEC) +
			      (uint64_t) nsec) + offset;

		nsec = do_div(new_offset, NSEC_PER_SEC);
		sec  = new_offset;

		ts.tv_sec  = sec;
		ts.tv_nsec = nsec;

		ret = emac_ptp_settime(&adpt->hw, &ts);
		if (ret) {
			pr_err("%s: emac_ptp_settime failed.\n", __func__);
			return ret;
		}
		ret = count;
	} else
		pr_err("%s: sscanf failed.\n", __func__);

	return ret;
}

/* Do a fine time ajustment (ie. have the timestamp registers adjust
 * themselves by the passed amount).
 */
static int emac_ptp_sysfs_fadj(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct emac_adapter *adpt = netdev_priv(to_net_dev(dev));
	int64_t              offset = 0;
	int                  ret = -EINVAL;

	if (sscanf(buf, "%lld", &offset) == 1) {
		ret = emac_ptp_adjtime(&adpt->hw, offset);
		if (ret) {
			pr_err("%s: emac_ptp_adjtime failed.\n", __func__);
			return ret;
		}
		ret = count;
	} else {
		pr_err("%s: sscanf failed.\n", __func__);
	}

	return ret;
}

static DEVICE_ATTR(cmd, 0222, NULL, emac_ptp_sysfs_cmd);
static DEVICE_ATTR(tstamp, 0444, emac_ptp_sysfs_tstamp_show, NULL);
static DEVICE_ATTR(mtnp, 0444, emac_ptp_sysfs_mtnp_show, NULL);
static DEVICE_ATTR(slam, 0222, NULL, emac_ptp_sysfs_slam);
static DEVICE_ATTR(cadj, 0222, NULL, emac_ptp_sysfs_cadj);
static DEVICE_ATTR(fadj, 0222, NULL, emac_ptp_sysfs_fadj);

static void emac_ptp_sysfs_create(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	if (device_create_file(&netdev->dev, &dev_attr_cmd) ||
	    device_create_file(&netdev->dev, &dev_attr_tstamp) ||
	    device_create_file(&netdev->dev, &dev_attr_mtnp) ||
	    device_create_file(&netdev->dev, &dev_attr_slam) ||
	    device_create_file(&netdev->dev, &dev_attr_cadj) ||
	    device_create_file(&netdev->dev, &dev_attr_fadj))
		emac_err(adpt, "emac_ptp: failed to create sysfs files\n");
}

static void emac_ptp_sysfs_remove(struct net_device *netdev)
{
	device_remove_file(&netdev->dev, &dev_attr_cmd);
	device_remove_file(&netdev->dev, &dev_attr_tstamp);
}

int emac_ptp_init(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	int ret = 0;

	spin_lock_init(&hw->ptp_lock);
	emac_ptp_sysfs_create(netdev);
	ret = emac_hw_1588_core_disable(hw);

	return ret;
}

void emac_ptp_remove(struct net_device *netdev)
{
	emac_ptp_sysfs_remove(netdev);
}
