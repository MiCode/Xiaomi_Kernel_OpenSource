/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VLV_PLL_H_
#define _VLV_PLL_H_

#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_regs.h>

struct dsi_mnp {
	u32 dsi_pll_ctrl;
	u32 dsi_pll_div;
};

struct vlv_pll {
	bool assigned;
	enum port port_id;
	enum pll pll_id;
	u32 offset;
	u32 multiplier_offset;
	u32 dpio_stat_offset;
	u32 phy_ctrl_offset;
	u32 phy_stat_offset;
	u32 clock_type;
	struct dsi_config *config;
};

u32 vlv_dsi_pll_enable(struct vlv_pll *pll,
		struct drm_mode_modeinfo *mode);
u32 vlv_dsi_pll_disable(struct vlv_pll *pll);
u32 vlv_dsi_pll_calc_mnp(struct vlv_pll *pll, u32 dsi_clk,
		struct dsi_mnp *dsi_mnp);
u32 vlv_pll_enable(struct vlv_pll *pll,
		struct drm_mode_modeinfo *mode);
u32 vlv_pll_disable(struct vlv_pll *pll);

u32 chv_dsi_pll_calc_mnp(struct vlv_pll *pll, u32 dsi_clk,
		struct dsi_mnp *dsi_mnp);
bool vlv_dsi_pll_init(struct vlv_pll *pll, enum pipe epipe, enum port port);
bool vlv_pll_init(struct vlv_pll *pll, enum intel_pipe_type type,
		enum pipe epipe, enum port eport);
bool vlv_pll_destroy(struct vlv_pll *pll);

#endif /*_VLV_PLL_H_*/
