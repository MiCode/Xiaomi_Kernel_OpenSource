/*
 * Copyright (c) 2013-2014, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>

#include <linux/msm-bus.h>

#include "ufshcd.h"
#include "unipro.h"
#include "ufs-msm.h"
#include "ufs-msm-phy.h"

static int ufs_msm_get_speed_mode(struct ufs_pa_layer_attr *p, char *result);
static int ufs_msm_get_bus_vote(struct ufs_msm_host *host,
		const char *speed_mode);
static int ufs_msm_set_bus_vote(struct ufs_msm_host *host, int vote);

static int ufs_msm_get_connected_tx_lanes(struct ufs_hba *hba, u32 *tx_lanes)
{
	int err = 0;

	err = ufshcd_dme_get(hba,
			UIC_ARG_MIB(PA_CONNECTEDTXDATALANES), tx_lanes);
	if (err)
		dev_err(hba->dev, "%s: couldn't read PA_CONNECTEDTXDATALANES %d\n",
				__func__, err);

	return err;
}

static int ufs_msm_host_clk_get(struct device *dev,
		const char *name, struct clk **clk_out)
{
	struct clk *clk;
	int err = 0;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		err = PTR_ERR(clk);
		dev_err(dev, "%s: failed to get %s err %d",
				__func__, name, err);
	} else {
		*clk_out = clk;
	}

	return err;
}

static int ufs_msm_host_clk_enable(struct device *dev,
		const char *name, struct clk *clk)
{
	int err = 0;

	err = clk_prepare_enable(clk);
	if (err)
		dev_err(dev, "%s: %s enable failed %d\n", __func__, name, err);

	return err;
}

static void ufs_msm_disable_lane_clks(struct ufs_msm_host *host)
{
	if (!host->is_lane_clks_enabled)
		return;

	clk_disable_unprepare(host->tx_l1_sync_clk);
	clk_disable_unprepare(host->tx_l0_sync_clk);
	clk_disable_unprepare(host->rx_l1_sync_clk);
	clk_disable_unprepare(host->rx_l0_sync_clk);

	host->is_lane_clks_enabled = false;
}

static int ufs_msm_enable_lane_clks(struct ufs_msm_host *host)
{
	int err = 0;
	struct device *dev = host->hba->dev;

	if (host->is_lane_clks_enabled)
		return 0;

	err = ufs_msm_host_clk_enable(dev,
			"rx_lane0_sync_clk", host->rx_l0_sync_clk);
	if (err)
		goto out;

	err = ufs_msm_host_clk_enable(dev,
			"rx_lane1_sync_clk", host->rx_l1_sync_clk);
	if (err)
		goto disable_rx_l0;

	err = ufs_msm_host_clk_enable(dev,
			"tx_lane0_sync_clk", host->tx_l0_sync_clk);
	if (err)
		goto disable_rx_l1;

	err = ufs_msm_host_clk_enable(dev,
			"tx_lane1_sync_clk", host->tx_l1_sync_clk);
	if (err)
		goto disable_tx_l0;

	host->is_lane_clks_enabled = true;
	goto out;

disable_tx_l0:
	clk_disable_unprepare(host->tx_l0_sync_clk);
disable_rx_l1:
	clk_disable_unprepare(host->rx_l1_sync_clk);
disable_rx_l0:
	clk_disable_unprepare(host->rx_l0_sync_clk);
out:
	return err;
}

static int ufs_msm_init_lane_clks(struct ufs_msm_host *host)
{
	int err = 0;
	struct device *dev = host->hba->dev;

	err = ufs_msm_host_clk_get(dev,
			"rx_lane0_sync_clk", &host->rx_l0_sync_clk);
	if (err)
		goto out;

	err = ufs_msm_host_clk_get(dev,
			"rx_lane1_sync_clk", &host->rx_l1_sync_clk);
	if (err)
		goto out;

	err = ufs_msm_host_clk_get(dev,
			"tx_lane0_sync_clk", &host->tx_l0_sync_clk);
	if (err)
		goto out;

	err = ufs_msm_host_clk_get(dev,
			"tx_lane1_sync_clk", &host->tx_l1_sync_clk);
out:
	return err;
}

static int ufs_msm_link_startup_post_change(struct ufs_hba *hba)
{
	struct ufs_msm_host *host = hba->priv;
	struct phy *phy = host->generic_phy;
	u32 tx_lanes;
	int err = 0;

	err = ufs_msm_get_connected_tx_lanes(hba, &tx_lanes);
	if (err)
		goto out;

	err = ufs_msm_phy_set_tx_lane_enable(phy, tx_lanes);
	if (err)
		dev_err(hba->dev, "%s: ufs_msm_phy_set_tx_lane_enable failed\n",
			__func__);

out:
	return err;
}

static int ufs_msm_check_hibern8(struct ufs_hba *hba)
{
	int err;
	u32 tx_fsm_val = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(HBRN8_POLL_TOUT_MS);

	do {
		err = ufshcd_dme_get(hba,
			UIC_ARG_MIB(MPHY_TX_FSM_STATE), &tx_fsm_val);
		if (err || tx_fsm_val == TX_FSM_HIBERN8)
			break;

		/* sleep for max. 200us */
		usleep_range(100, 200);
	} while (time_before(jiffies, timeout));

	/*
	 * we might have scheduled out for long during polling so
	 * check the state again.
	 */
	if (time_after(jiffies, timeout))
		err = ufshcd_dme_get(hba,
				UIC_ARG_MIB(MPHY_TX_FSM_STATE), &tx_fsm_val);

	if (err) {
		dev_err(hba->dev, "%s: unable to get TX_FSM_STATE, err %d\n",
				__func__, err);
	} else if (tx_fsm_val != TX_FSM_HIBERN8) {
		err = tx_fsm_val;
		dev_err(hba->dev, "%s: invalid TX_FSM_STATE = %d\n",
				__func__, err);
	}

	return err;
}

static int ufs_msm_power_up_sequence(struct ufs_hba *hba)
{
	struct ufs_msm_host *host = hba->priv;
	struct phy *phy = host->generic_phy;
	int ret = 0;
	u8 major;
	u16 minor, step;

	/* Assert PHY reset and apply PHY calibration values */
	ufs_msm_assert_reset(hba);
	/* provide 1ms delay to let the reset pulse propagate */
	usleep_range(1000, 1100);

	ufs_msm_get_controller_revision(hba, &major, &minor, &step);
	ufs_msm_phy_save_controller_version(phy, major, minor, step);
	ret = ufs_msm_phy_calibrate_phy(phy);
	if (ret) {
		dev_err(hba->dev, "%s: ufs_msm_phy_calibrate_phy() failed, ret = %d\n",
			__func__, ret);
		goto out;
	}

	/* De-assert PHY reset and start serdes */
	ufs_msm_deassert_reset(hba);

	/*
	 * after reset deassertion, phy will need all ref clocks,
	 * voltage, current to settle down before starting serdes.
	 */
	usleep_range(1000, 1100);
	if (ufs_msm_phy_start_serdes(phy)) {
		dev_err(hba->dev, "%s: ufs_msm_phy_start_serdes() failed, ret = %d\n",
			__func__, ret);
		goto out;
	}

	ret = ufs_msm_phy_is_pcs_ready(phy);
	if (ret)
		dev_err(hba->dev, "%s: is_physical_coding_sublayer_ready() failed, ret = %d\n",
			__func__, ret);

out:
	return ret;
}

static int ufs_msm_hce_enable_notify(struct ufs_hba *hba, bool status)
{
	struct ufs_msm_host *host = hba->priv;
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		ufs_msm_power_up_sequence(hba);
		/*
		 * The PHY PLL output is the source of tx/rx lane symbol
		 * clocks, hence, enable the lane clocks only after PHY
		 * is initialized.
		 */
		err = ufs_msm_enable_lane_clks(host);
		break;
	case POST_CHANGE:
		/* check if UFS PHY moved from DISABLED to HIBERN8 */
		err = ufs_msm_check_hibern8(hba);
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}
	return err;
}

/**
 * Returns non-zero for success (which rate of core_clk) and 0
 * in case of a failure
 */
static unsigned long
ufs_msm_cfg_timers(struct ufs_hba *hba, u32 gear, u32 hs, u32 rate)
{
	struct ufs_clk_info *clki;
	u32 core_clk_period_in_ns;
	u32 tx_clk_cycles_per_us = 0;
	unsigned long core_clk_rate = 0;
	u32 core_clk_cycles_per_us = 0;

	static u32 pwm_fr_table[][2] = {
		{UFS_PWM_G1, 0x1},
		{UFS_PWM_G2, 0x1},
		{UFS_PWM_G3, 0x1},
		{UFS_PWM_G4, 0x1},
	};

	static u32 hs_fr_table_rA[][2] = {
		{UFS_HS_G1, 0x1F},
		{UFS_HS_G2, 0x3e},
	};

	static u32 hs_fr_table_rB[][2] = {
		{UFS_HS_G1, 0x24},
		{UFS_HS_G2, 0x49},
	};

	if (gear == 0) {
		dev_err(hba->dev, "%s: invalid gear = %d\n", __func__, gear);
		goto out_error;
	}

	list_for_each_entry(clki, &hba->clk_list_head, list) {
		if (!strcmp(clki->name, "core_clk"))
			core_clk_rate = clk_get_rate(clki->clk);
	}

	/* If frequency is smaller than 1MHz, set to 1MHz */
	if (core_clk_rate < DEFAULT_CLK_RATE_HZ)
		core_clk_rate = DEFAULT_CLK_RATE_HZ;

	core_clk_cycles_per_us = core_clk_rate / USEC_PER_SEC;
	ufshcd_writel(hba, core_clk_cycles_per_us, REG_UFS_SYS1CLK_1US);

	core_clk_period_in_ns = NSEC_PER_SEC / core_clk_rate;
	core_clk_period_in_ns <<= OFFSET_CLK_NS_REG;
	core_clk_period_in_ns &= MASK_CLK_NS_REG;

	switch (hs) {
	case FASTAUTO_MODE:
	case FAST_MODE:
		if (rate == PA_HS_MODE_A) {
			if (gear > ARRAY_SIZE(hs_fr_table_rA)) {
				dev_err(hba->dev,
					"%s: index %d exceeds table size %zu\n",
					__func__, gear,
					ARRAY_SIZE(hs_fr_table_rA));
				goto out_error;
			}
			tx_clk_cycles_per_us = hs_fr_table_rA[gear-1][1];
		} else if (rate == PA_HS_MODE_B) {
			if (gear > ARRAY_SIZE(hs_fr_table_rB)) {
				dev_err(hba->dev,
					"%s: index %d exceeds table size %zu\n",
					__func__, gear,
					ARRAY_SIZE(hs_fr_table_rB));
				goto out_error;
			}
			tx_clk_cycles_per_us = hs_fr_table_rB[gear-1][1];
		} else {
			dev_err(hba->dev, "%s: invalid rate = %d\n",
				__func__, rate);
			goto out_error;
		}
		break;
	case SLOWAUTO_MODE:
	case SLOW_MODE:
		if (gear > ARRAY_SIZE(pwm_fr_table)) {
			dev_err(hba->dev,
					"%s: index %d exceeds table size %zu\n",
					__func__, gear,
					ARRAY_SIZE(pwm_fr_table));
			goto out_error;
		}
		tx_clk_cycles_per_us = pwm_fr_table[gear-1][1];
		break;
	case UNCHANGED:
	default:
		dev_err(hba->dev, "%s: invalid mode = %d\n", __func__, hs);
		goto out_error;
	}

	/* this register 2 fields shall be written at once */
	ufshcd_writel(hba, core_clk_period_in_ns | tx_clk_cycles_per_us,
						REG_UFS_TX_SYMBOL_CLK_NS_US);
	goto out;

out_error:
	core_clk_rate = 0;
out:
	return core_clk_rate;
}

static int ufs_msm_link_startup_notify(struct ufs_hba *hba, bool status)
{
	unsigned long core_clk_rate = 0;
	u32 core_clk_cycles_per_100ms;

	switch (status) {
	case PRE_CHANGE:
		core_clk_rate = ufs_msm_cfg_timers(hba, UFS_PWM_G1,
						   SLOWAUTO_MODE, 0);
		if (!core_clk_rate) {
			dev_err(hba->dev, "%s: ufs_msm_cfg_timers() failed\n",
				__func__);
			return -EINVAL;
		}
		core_clk_cycles_per_100ms =
			(core_clk_rate / MSEC_PER_SEC) * 100;
		ufshcd_writel(hba, core_clk_cycles_per_100ms,
					REG_UFS_PA_LINK_STARTUP_TIMER);
		break;
	case POST_CHANGE:
		ufs_msm_link_startup_post_change(hba);
		break;
	default:
		break;
	}

	return 0;
}

static int ufs_msm_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct ufs_msm_host *host = hba->priv;
	struct phy *phy = host->generic_phy;
	int ret = 0;

	if (ufs_msm_is_link_off(hba)) {
		/*
		 * Disable the tx/rx lane symbol clocks before PHY is
		 * powered down as the PLL source should be disabled
		 * after downstream clocks are disabled.
		 */
		ufs_msm_disable_lane_clks(host);
		phy_power_off(phy);

		goto out;
	}

	/*
	 * If UniPro link is not active, PHY ref_clk, main PHY analog power
	 * rail and low noise analog power rail for PLL can be switched off.
	 */
	if (!ufs_msm_is_link_active(hba)) {
		if (ufs_msm_phy_is_cfg_restore_quirk_enabled(phy) &&
		    ufs_msm_is_link_hibern8(hba)) {
			ret = ufs_msm_phy_save_configuration(phy);
			if (ret)
				dev_err(hba->dev, "%s: failed ufs_msm_phy_save_configuration %d\n",
					__func__, ret);
		}
		phy_power_off(phy);
	}

out:
	return ret;
}

static bool ufs_msm_is_phy_config_restore_required(struct ufs_hba *hba)
{
	struct ufs_msm_host *host = hba->priv;
	struct phy *phy = host->generic_phy;

	return ufs_msm_phy_is_cfg_restore_quirk_enabled(phy)
		&& ufshcd_is_link_hibern8(hba)
		&& hba->is_sys_suspended;
}

static int ufs_msm_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct ufs_msm_host *host = hba->priv;
	struct phy *phy = host->generic_phy;
	int err;

	if (ufs_msm_is_phy_config_restore_required(hba)) {
		ufs_msm_assert_reset(hba);
		/* provide 1ms delay to let the reset pulse propagate */
		usleep_range(1000, 1100);
	}

	err = phy_power_on(phy);
	if (err) {
		dev_err(hba->dev, "%s: failed enabling regs, err = %d\n",
			__func__, err);
		goto out;
	}

	if (ufs_msm_is_phy_config_restore_required(hba)) {
		ufs_msm_phy_restore_swi_regs(phy);

		/* De-assert PHY reset and start serdes */
		ufs_msm_deassert_reset(hba);

		/*
		 * after reset deassertion, phy will need all ref clocks,
		 * voltage, current to settle down before starting serdes.
		 */
		usleep_range(1000, 1100);

		err = phy_resume(host->generic_phy);
	}

	hba->is_sys_suspended = false;
out:
	return err;
}

struct ufs_msm_dev_params {
	u32 pwm_rx_gear;	/* pwm rx gear to work in */
	u32 pwm_tx_gear;	/* pwm tx gear to work in */
	u32 hs_rx_gear;		/* hs rx gear to work in */
	u32 hs_tx_gear;		/* hs tx gear to work in */
	u32 rx_lanes;		/* number of rx lanes */
	u32 tx_lanes;		/* number of tx lanes */
	u32 rx_pwr_pwm;		/* rx pwm working pwr */
	u32 tx_pwr_pwm;		/* tx pwm working pwr */
	u32 rx_pwr_hs;		/* rx hs working pwr */
	u32 tx_pwr_hs;		/* tx hs working pwr */
	u32 hs_rate;		/* rate A/B to work in HS */
	u32 desired_working_mode;
};

/**
 * as every power mode, according to the UFS spec, have a defined
 * number that are not corresponed to their order or power
 * consumption (i.e 5, 2, 4, 1 respectively from low to high),
 * we need to map them into array, so we can scan it easily
 * in order to find the minimum required power mode.
 * also, we can use this routine to go the other way around,
 * and from array index, the fetch the correspond power mode.
 */
static int map_unmap_pwr_mode(u32 mode, bool is_pwr_to_arr)
{
	enum {SL_MD = 0, SLA_MD = 1, FS_MD = 2, FSA_MD = 3, UNDEF = 4};
	int ret = -EINVAL;

	if (is_pwr_to_arr) {
		switch (mode) {
		case SLOW_MODE:
			ret = SL_MD;
			break;
		case SLOWAUTO_MODE:
			ret = SLA_MD;
			break;
		case FAST_MODE:
			ret = FS_MD;
			break;
		case FASTAUTO_MODE:
			ret = FSA_MD;
			break;
		default:
			ret = UNDEF;
			break;
		}
	} else {
		switch (mode) {
		case SL_MD:
			ret = SLOW_MODE;
			break;
		case SLA_MD:
			ret = SLOWAUTO_MODE;
			break;
		case FS_MD:
			ret = FAST_MODE;
			break;
		case FSA_MD:
			ret = FASTAUTO_MODE;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

#define NUM_OF_SUPPORTED_MODES	5
static int get_pwr_dev_param(struct ufs_msm_dev_params *msm_param,
				struct ufs_pa_layer_attr *dev_max,
				struct ufs_pa_layer_attr *dev_req)
{
	int arr[NUM_OF_SUPPORTED_MODES] = {0};
	int i;
	int min_power;
	int min_msm_gear;
	int min_dev_gear;
	bool is_max_dev_hs;
	bool is_max_msm_hs;

	/**
	 * mapping the max. supported power mode of the device
	 * and the max. pre-defined support power mode of the vendor
	 * in order to scan them easily
	 */
	arr[map_unmap_pwr_mode(dev_max->pwr_rx, true)]++;
	arr[map_unmap_pwr_mode(dev_max->pwr_tx, true)]++;

	if (msm_param->desired_working_mode == SLOW) {
		arr[map_unmap_pwr_mode(msm_param->rx_pwr_pwm, true)]++;
		arr[map_unmap_pwr_mode(msm_param->tx_pwr_pwm, true)]++;
	} else {
		arr[map_unmap_pwr_mode(msm_param->rx_pwr_hs, true)]++;
		arr[map_unmap_pwr_mode(msm_param->tx_pwr_hs, true)]++;
	}

	for (i = 0; i < NUM_OF_SUPPORTED_MODES; ++i) {
		if (arr[i] != 0)
			break;
	}

	/* no supported power mode found */
	if (i == NUM_OF_SUPPORTED_MODES) {
		return -EINVAL;
	} else {
		min_power = map_unmap_pwr_mode(i, false);
		if (min_power >= 0)
			dev_req->pwr_rx = dev_req->pwr_tx = min_power;
		else
			return -EINVAL;
	}

	/**
	 * we would like tx to work in the minimum number of lanes
	 * between device capability and vendor preferences.
	 * the same decision will be made for rx.
	 */
	dev_req->lane_tx = min_t(u32, dev_max->lane_tx, msm_param->tx_lanes);
	dev_req->lane_rx = min_t(u32, dev_max->lane_rx, msm_param->rx_lanes);

	if (dev_max->pwr_rx == SLOW_MODE ||
	    dev_max->pwr_rx == SLOWAUTO_MODE)
		is_max_dev_hs = false;
	else
		is_max_dev_hs = true;

	/* setting the device maximum gear */
	min_dev_gear = min_t(u32, dev_max->gear_rx, dev_max->gear_tx);

	/**
	 * setting the desired gear to be the minimum according to the desired
	 * power mode
	 */
	if (msm_param->desired_working_mode == SLOW) {
		is_max_msm_hs = false;
		min_msm_gear = min_t(u32, msm_param->pwm_rx_gear,
						msm_param->pwm_tx_gear);
	} else {
		is_max_msm_hs = true;
		min_msm_gear = min_t(u32, msm_param->hs_rx_gear,
						msm_param->hs_tx_gear);
	}

	/**
	 * if both device capabilities and vendor pre-defined preferences are
	 * both HS or both PWM then set the minimum gear to be the
	 * chosen working gear.
	 * if one is PWM and one is HS then the one that is PWM get to decide
	 * what the gear, as he is the one that also decided previously what
	 * pwr the device will be configured to.
	 */
	if ((is_max_dev_hs && is_max_msm_hs) ||
	    (!is_max_dev_hs && !is_max_msm_hs)) {
		dev_req->gear_rx = dev_req->gear_tx =
			min_t(u32, min_dev_gear, min_msm_gear);
	} else if (!is_max_dev_hs) {
		dev_req->gear_rx = dev_req->gear_tx = min_dev_gear;
	} else {
		dev_req->gear_rx = dev_req->gear_tx = min_msm_gear;
	}

	dev_req->hs_rate = msm_param->hs_rate;

	return 0;
}

static int ufs_msm_update_bus_bw_vote(struct ufs_msm_host *host)
{
	int vote;
	int err = 0;
	char mode[BUS_VECTOR_NAME_LEN];

	err = ufs_msm_get_speed_mode(&host->dev_req_params, mode);
	if (err)
		goto out;

	vote = ufs_msm_get_bus_vote(host, mode);
	if (vote >= 0)
		err = ufs_msm_set_bus_vote(host, vote);
	else
		err = vote;

out:
	if (err)
		dev_err(host->hba->dev, "%s: failed %d\n", __func__, err);
	else
		host->bus_vote.saved_vote = vote;
	return err;
}

static int ufs_msm_pwr_change_notify(struct ufs_hba *hba,
				     bool status,
				     struct ufs_pa_layer_attr *dev_max_params,
				     struct ufs_pa_layer_attr *dev_req_params)
{
	u32 val;
	struct ufs_msm_host *host = hba->priv;
	struct phy *phy = host->generic_phy;
	struct ufs_msm_dev_params ufs_msm_cap;
	int ret = 0;
	int res = 0;

	if (!dev_req_params) {
		pr_err("%s: incoming dev_req_params is NULL\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	switch (status) {
	case PRE_CHANGE:
		ufs_msm_cap.tx_lanes = UFS_MSM_LIMIT_NUM_LANES_TX;
		ufs_msm_cap.rx_lanes = UFS_MSM_LIMIT_NUM_LANES_RX;
		ufs_msm_cap.hs_rx_gear = UFS_MSM_LIMIT_HSGEAR_RX;
		ufs_msm_cap.hs_tx_gear = UFS_MSM_LIMIT_HSGEAR_TX;
		ufs_msm_cap.pwm_rx_gear = UFS_MSM_LIMIT_PWMGEAR_RX;
		ufs_msm_cap.pwm_tx_gear = UFS_MSM_LIMIT_PWMGEAR_TX;
		ufs_msm_cap.rx_pwr_pwm = UFS_MSM_LIMIT_RX_PWR_PWM;
		ufs_msm_cap.tx_pwr_pwm = UFS_MSM_LIMIT_TX_PWR_PWM;
		ufs_msm_cap.rx_pwr_hs = UFS_MSM_LIMIT_RX_PWR_HS;
		ufs_msm_cap.tx_pwr_hs = UFS_MSM_LIMIT_TX_PWR_HS;
		ufs_msm_cap.hs_rate = UFS_MSM_LIMIT_HS_RATE;
		ufs_msm_cap.desired_working_mode =
					UFS_MSM_LIMIT_DESIRED_MODE;

		ret = get_pwr_dev_param(&ufs_msm_cap, dev_max_params,
							dev_req_params);
		if (ret) {
			pr_err("%s: failed to determine capabilities\n",
					__func__);
			goto out;
		}

		break;
	case POST_CHANGE:
		if (!ufs_msm_cfg_timers(hba, dev_req_params->gear_rx,
					dev_req_params->pwr_rx,
					dev_req_params->hs_rate)) {
			dev_err(hba->dev, "%s: ufs_msm_cfg_timers() failed\n",
				__func__);
			/*
			 * we return error code at the end of the routine,
			 * but continue to configure UFS_PHY_TX_LANE_ENABLE
			 * and bus voting as usual
			 */
			ret = -EINVAL;
		}

		val = ~(MAX_U32 << dev_req_params->lane_tx);
		res = ufs_msm_phy_set_tx_lane_enable(phy, val);
		if (res) {
			dev_err(hba->dev, "%s: ufs_msm_phy_set_tx_lane_enable() failed res = %d\n",
				__func__, res);
			ret = res;
		}

		/* cache the power mode parameters to use internally */
		memcpy(&host->dev_req_params,
				dev_req_params, sizeof(*dev_req_params));
		ufs_msm_update_bus_bw_vote(host);
		break;
	default:
		ret = -EINVAL;
		break;
	}
out:
	return ret;
}

/**
 * ufs_msm_advertise_quirks - advertise the known MSM UFS controller quirks
 * @hba: host controller instance
 *
 * MSM UFS host controller might have some non standard behaviours (quirks)
 * than what is specified by UFSHCI specification. Advertise all such
 * quirks to standard UFS host controller driver so standard takes them into
 * account.
 */
static void ufs_msm_advertise_quirks(struct ufs_hba *hba)
{
	struct ufs_msm_host *host = hba->priv;
	u8 major;
	u16 minor, step;

	ufs_msm_get_controller_revision(hba, &major, &minor, &step);

	if ((major == 0x1) && (minor == 0x001) && (step == 0x0001))
		hba->quirks |= (UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS
			      | UFSHCD_QUIRK_BROKEN_INTR_AGGR
			      | UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP
			      | UFSHCD_QUIRK_BROKEN_LCC);
	else if ((major == 0x1) && (minor == 0x002) && (step == 0x0000))
		hba->quirks |= (UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS
			      | UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP
			      | UFSHCD_QUIRK_BROKEN_LCC);

	phy_advertise_quirks(host->generic_phy);
}

static int ufs_msm_get_bus_vote(struct ufs_msm_host *host,
		const char *speed_mode)
{
	struct device *dev = host->hba->dev;
	struct device_node *np = dev->of_node;
	int err;
	const char *key = "qcom,bus-vector-names";

	if (!speed_mode) {
		err = -EINVAL;
		goto out;
	}

	if (host->bus_vote.is_max_bw_needed && !!strcmp(speed_mode, "MIN"))
		err = of_property_match_string(np, key, "MAX");
	else
		err = of_property_match_string(np, key, speed_mode);

out:
	if (err < 0)
		dev_err(dev, "%s: Invalid %s mode %d\n",
				__func__, speed_mode, err);
	return err;
}

static int ufs_msm_set_bus_vote(struct ufs_msm_host *host, int vote)
{
	int err = 0;

	if (vote != host->bus_vote.curr_vote) {
		err = msm_bus_scale_client_update_request(
				host->bus_vote.client_handle, vote);
		if (err) {
			dev_err(host->hba->dev,
				"%s: msm_bus_scale_client_update_request() failed: bus_client_handle=0x%x, vote=%d, err=%d\n",
				__func__, host->bus_vote.client_handle,
				vote, err);
			goto out;
		}

		host->bus_vote.curr_vote = vote;
	}
out:
	return err;
}

static int ufs_msm_get_speed_mode(struct ufs_pa_layer_attr *p, char *result)
{
	int err = 0;
	int gear = max_t(u32, p->gear_rx, p->gear_tx);
	int lanes = max_t(u32, p->lane_rx, p->lane_tx);
	int pwr = max_t(u32, map_unmap_pwr_mode(p->pwr_rx, true),
			map_unmap_pwr_mode(p->pwr_tx, true));

	/* default to PWM Gear 1, Lane 1 if power mode is not initialized */
	if (!gear)
		gear = 1;

	if (!lanes)
		lanes = 1;

	if (!p->pwr_rx && !p->pwr_tx)
		pwr = 0;

	pwr = map_unmap_pwr_mode(pwr, false);
	if (pwr < 0) {
		err = pwr;
		goto out;
	}

	if (pwr == FAST_MODE || pwr == FASTAUTO_MODE)
		snprintf(result, BUS_VECTOR_NAME_LEN, "%s_R%s_G%d_L%d", "HS",
				p->hs_rate == PA_HS_MODE_B ? "B" : "A",
				gear, lanes);
	else
		snprintf(result, BUS_VECTOR_NAME_LEN, "%s_G%d_L%d",
				"PWM", gear, lanes);
out:
	return err;
}



static int ufs_msm_setup_clocks(struct ufs_hba *hba, bool on)
{
	struct ufs_msm_host *host = hba->priv;
	int err;
	int vote = 0;

	/*
	 * In case ufs_msm_init() is not yet done, simply ignore.
	 * This ufs_msm_setup_clocks() shall be called from
	 * ufs_msm_init() after init is done.
	 */
	if (!host)
		return 0;

	if (on) {
		err = ufs_msm_phy_enable_iface_clk(host->generic_phy);
		if (err)
			goto out;

		vote = host->bus_vote.saved_vote;
		if (vote == host->bus_vote.min_bw_vote)
			ufs_msm_update_bus_bw_vote(host);
	} else {
		/* M-PHY RMMI interface clocks can be turned off */
		ufs_msm_phy_disable_iface_clk(host->generic_phy);
		vote = host->bus_vote.min_bw_vote;
	}

	err = ufs_msm_set_bus_vote(host, vote);
	if (err)
		dev_err(hba->dev, "%s: set bus vote failed %d\n",
				__func__, err);

out:
	return err;
}

static ssize_t
show_ufs_to_mem_max_bus_bw(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_msm_host *host = hba->priv;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			host->bus_vote.is_max_bw_needed);
}

static ssize_t
store_ufs_to_mem_max_bus_bw(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_msm_host *host = hba->priv;
	uint32_t value;

	if (!kstrtou32(buf, 0, &value)) {
		host->bus_vote.is_max_bw_needed = !!value;
		ufs_msm_update_bus_bw_vote(host);
	}

	return count;
}

static int ufs_msm_bus_register(struct ufs_msm_host *host)
{
	int err;
	struct msm_bus_scale_pdata *bus_pdata;
	struct device *dev = host->hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *np = dev->of_node;

	bus_pdata = msm_bus_cl_get_pdata(pdev);
	if (!bus_pdata) {
		dev_err(dev, "%s: failed to get bus vectors\n", __func__);
		err = -ENODATA;
		goto out;
	}

	err = of_property_count_strings(np, "qcom,bus-vector-names");
	if (err < 0 || err != bus_pdata->num_usecases) {
		dev_err(dev, "%s: qcom,bus-vector-names not specified correctly %d\n",
				__func__, err);
		goto out;
	}

	host->bus_vote.client_handle = msm_bus_scale_register_client(bus_pdata);
	if (!host->bus_vote.client_handle) {
		dev_err(dev, "%s: msm_bus_scale_register_client failed\n",
				__func__);
		err = -EFAULT;
		goto out;
	}

	/* cache the vote index for minimum and maximum bandwidth */
	host->bus_vote.min_bw_vote = ufs_msm_get_bus_vote(host, "MIN");
	host->bus_vote.max_bw_vote = ufs_msm_get_bus_vote(host, "MAX");

	host->bus_vote.max_bus_bw.show = show_ufs_to_mem_max_bus_bw;
	host->bus_vote.max_bus_bw.store = store_ufs_to_mem_max_bus_bw;
	sysfs_attr_init(&host->bus_vote.max_bus_bw.attr);
	host->bus_vote.max_bus_bw.attr.name = "max_bus_bw";
	host->bus_vote.max_bus_bw.attr.mode = S_IRUGO | S_IWUSR;
	err = device_create_file(dev, &host->bus_vote.max_bus_bw);
out:
	return err;
}

#define	ANDROID_BOOT_DEV_MAX	30
static char android_boot_dev[ANDROID_BOOT_DEV_MAX];
static int get_android_boot_dev(char *str)
{
	strlcpy(android_boot_dev, str, ANDROID_BOOT_DEV_MAX);
	return 1;
}
__setup("androidboot.bootdevice=", get_android_boot_dev);

/**
 * ufs_msm_init - bind phy with controller
 * @hba: host controller instance
 *
 * Binds PHY with controller and powers up PHY enabling clocks
 * and regulators.
 *
 * Returns -EPROBE_DEFER if binding fails, returns negative error
 * on phy power up failure and returns zero on success.
 */
static int ufs_msm_init(struct ufs_hba *hba)
{
	int err;
	struct device *dev = hba->dev;
	struct ufs_msm_host *host;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host) {
		err = -ENOMEM;
		dev_err(dev, "%s: no memory for msm ufs host\n", __func__);
		goto out;
	}

	host->hba = hba;
	host->generic_phy = devm_phy_get_by_index(dev, 0);

	if (IS_ERR(host->generic_phy)) {
		err = PTR_ERR(host->generic_phy);
		dev_err(dev, "PHY get failed %d\n", err);
		goto out;
	}

	hba->priv = (void *)host;

	err = ufs_msm_bus_register(host);
	if (err)
		goto out_host_free;

	phy_init(host->generic_phy);
	err = phy_power_on(host->generic_phy);
	if (err)
		goto out_unregister_bus;

	err = ufs_msm_init_lane_clks(host);
	if (err)
		goto out_disable_phy;

	ufs_msm_advertise_quirks(hba);

	hba->caps |= UFSHCD_CAP_CLK_GATING | UFSHCD_CAP_CLK_SCALING;
	hba->caps |= UFSHCD_CAP_AUTO_BKOPS_SUSPEND;
	ufs_msm_setup_clocks(hba, true);
	goto out;

out_disable_phy:
	phy_power_off(host->generic_phy);
out_unregister_bus:
	phy_exit(host->generic_phy);
	msm_bus_scale_unregister_client(host->bus_vote.client_handle);
out_host_free:
	devm_kfree(dev, host);
	hba->priv = NULL;
out:
	return err;
}

static void ufs_msm_exit(struct ufs_hba *hba)
{
	struct ufs_msm_host *host = hba->priv;

	msm_bus_scale_unregister_client(host->bus_vote.client_handle);
	ufs_msm_disable_lane_clks(host);
	phy_power_off(host->generic_phy);
}


void ufs_msm_clk_scale_notify(struct ufs_hba *hba)
{
	struct ufs_msm_host *host = hba->priv;
	struct ufs_pa_layer_attr *dev_req_params = &host->dev_req_params;

	if (!dev_req_params)
		return;

	ufs_msm_cfg_timers(hba, dev_req_params->gear_rx,
				dev_req_params->pwr_rx,
				dev_req_params->hs_rate);
	ufs_msm_update_bus_bw_vote(host);
}
/**
 * struct ufs_hba_msm_vops - UFS MSM specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initializaiton.
 */
const struct ufs_hba_variant_ops ufs_hba_msm_vops = {
	.name                   = "msm",
	.init                   = ufs_msm_init,
	.exit                   = ufs_msm_exit,
	.clk_scale_notify	= ufs_msm_clk_scale_notify,
	.setup_clocks           = ufs_msm_setup_clocks,
	.hce_enable_notify      = ufs_msm_hce_enable_notify,
	.link_startup_notify    = ufs_msm_link_startup_notify,
	.pwr_change_notify	= ufs_msm_pwr_change_notify,
	.suspend		= ufs_msm_suspend,
	.resume			= ufs_msm_resume,
};
EXPORT_SYMBOL(ufs_hba_msm_vops);
