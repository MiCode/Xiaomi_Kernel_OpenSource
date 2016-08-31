/*
 * drivers/video/tegra/host/gk20a/therm_gk20a.c
 *
 * GK20A Therm
 *
 * Copyright (c) 2011 - 2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../dev.h"

#include "../t124/t124.h"

#include "gk20a.h"
#include "hw_chiplet_pwr_gk20a.h"
#include "hw_gr_gk20a.h"
#include "hw_therm_gk20a.h"

static int gk20a_init_therm_reset_enable_hw(struct gk20a *g)
{
	return 0;
}

static int gk20a_init_therm_setup_sw(struct gk20a *g)
{
	return 0;
}

static int gk20a_init_therm_setup_hw(struct gk20a *g)
{
	/* program NV_THERM registers */
	gk20a_writel(g, therm_use_a_r(), NV_THERM_USE_A_INIT);
	gk20a_writel(g, therm_evt_ext_therm_0_r(),
		NV_THERM_EVT_EXT_THERM_0_INIT);
	gk20a_writel(g, therm_evt_ext_therm_1_r(),
		NV_THERM_EVT_EXT_THERM_1_INIT);
	gk20a_writel(g, therm_evt_ext_therm_2_r(),
		NV_THERM_EVT_EXT_THERM_2_INIT);

/*
	u32 data;

	data = gk20a_readl(g, gr_gpcs_tpcs_l1c_cfg_r());
	data = set_field(data, gr_gpcs_tpcs_l1c_cfg_blkactivity_enable_m(),
		gr_gpcs_tpcs_l1c_cfg_blkactivity_enable_enable_f());
	gk20a_writel(g, gr_gpcs_tpcs_l1c_cfg_r(), data);

	data = gk20a_readl(g, gr_gpcs_tpcs_l1c_pm_r());
	data = set_field(data, gr_gpcs_tpcs_l1c_pm_enable_m(),
		gr_gpcs_tpcs_l1c_pm_enable_enable_f());
	gk20a_writel(g, gr_gpcs_tpcs_l1c_pm_r(), data);

	data = gk20a_readl(g, gr_gpcs_tpcs_sm_pm_ctrl_r());
	data = set_field(data, gr_gpcs_tpcs_sm_pm_ctrl_core_enable_m(),
		gr_gpcs_tpcs_sm_pm_ctrl_core_enable_enable_f());
	data = set_field(data, gr_gpcs_tpcs_sm_pm_ctrl_qctl_enable_m(),
		gr_gpcs_tpcs_sm_pm_ctrl_qctl_enable_enable_f());
	gk20a_writel(g, gr_gpcs_tpcs_sm_pm_ctrl_r(), data);

	data = gk20a_readl(g, gr_gpcs_tpcs_sm_halfctl_ctrl_r());
	data = set_field(data, gr_gpcs_tpcs_sm_halfctl_ctrl_sctl_blkactivity_enable_m(),
		gr_gpcs_tpcs_sm_halfctl_ctrl_sctl_blkactivity_enable_enable_f());
	gk20a_writel(g, gr_gpcs_tpcs_sm_halfctl_ctrl_r(), data);

	data = gk20a_readl(g, gr_gpcs_tpcs_sm_debug_sfe_control_r());
	data = set_field(data, gr_gpcs_tpcs_sm_debug_sfe_control_blkactivity_enable_m(),
		gr_gpcs_tpcs_sm_debug_sfe_control_blkactivity_enable_enable_f());
	gk20a_writel(g, gr_gpcs_tpcs_sm_debug_sfe_control_r(), data);

	gk20a_writel(g, therm_peakpower_config6_r(0),
		therm_peakpower_config6_trigger_cfg_1h_intr_f() |
		therm_peakpower_config6_trigger_cfg_1l_intr_f());

	gk20a_writel(g, chiplet_pwr_gpcs_config_1_r(),
		chiplet_pwr_gpcs_config_1_ba_enable_yes_f());
	gk20a_writel(g, chiplet_pwr_fbps_config_1_r(),
		chiplet_pwr_fbps_config_1_ba_enable_yes_f());

	data = gk20a_readl(g, therm_config1_r());
	data = set_field(data, therm_config1_ba_enable_m(),
		therm_config1_ba_enable_yes_f());
	gk20a_writel(g, therm_config1_r(), data);

	gk20a_writel(g, gr_gpcs_tpcs_sm_power_throttle_r(), 0x441a);

	gk20a_writel(g, therm_weight_1_r(), 0xd3);
	gk20a_writel(g, chiplet_pwr_gpcs_weight_6_r(), 0x7d);
	gk20a_writel(g, chiplet_pwr_gpcs_weight_7_r(), 0xff);
	gk20a_writel(g, chiplet_pwr_fbps_weight_0_r(), 0x13000000);
	gk20a_writel(g, chiplet_pwr_fbps_weight_1_r(), 0x19);

	gk20a_writel(g, therm_peakpower_config8_r(0), 0x8);
	gk20a_writel(g, therm_peakpower_config9_r(0), 0x0);

	gk20a_writel(g, therm_evt_ba_w0_t1h_r(), 0x100);

	gk20a_writel(g, therm_use_a_r(), therm_use_a_ba_w0_t1h_yes_f());

	gk20a_writel(g, therm_peakpower_config1_r(0),
		therm_peakpower_config1_window_period_2m_f() |
		therm_peakpower_config1_ba_sum_shift_20_f() |
		therm_peakpower_config1_window_en_enabled_f());

	gk20a_writel(g, therm_peakpower_config2_r(0),
		therm_peakpower_config2_ba_threshold_1h_val_f(1) |
		therm_peakpower_config2_ba_threshold_1h_en_enabled_f());

	gk20a_writel(g, therm_peakpower_config4_r(0),
		therm_peakpower_config4_ba_threshold_1l_val_f(1) |
		therm_peakpower_config4_ba_threshold_1l_en_enabled_f());
*/
	return 0;
}

int gk20a_init_therm_support(struct gk20a *g)
{
	u32 err;

	nvhost_dbg_fn("");

	err = gk20a_init_therm_reset_enable_hw(g);
	if (err)
		return err;

	err = gk20a_init_therm_setup_sw(g);
	if (err)
		return err;

	err = gk20a_init_therm_setup_hw(g);
	if (err)
		return err;

	return err;
}
