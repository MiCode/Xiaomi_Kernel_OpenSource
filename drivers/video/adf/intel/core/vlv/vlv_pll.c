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

#include <intel_adf_device.h>
#include <core/intel_dc_config.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_pll.h>


u32 vlv_pll_enable(struct vlv_pll *pll,
		struct drm_mode_modeinfo *mode)
{
	/* program the register values */
	return 0;
}

u32 vlv_pll_disable(struct vlv_pll *pll)
{
	/* program the register values */
	u32 val = 0;

	val = REG_READ(pll->offset);
	val &= ~DPLL_VCO_ENABLE;
	if (pll->pll_id == _DPLL_A)
		val &= ~DPLL_INTEGRATED_CRI_CLK_VLV;

	REG_WRITE(pll->offset, val);
	return 0;
}

bool vlv_pll_init(struct vlv_pll *pll, enum intel_pipe_type type,
		enum pipe pipe_id, enum port port_id)
{
	/* do any init needed for each pll */
	if (type == INTEL_PIPE_DSI)
		return vlv_dsi_pll_init(pll, pipe_id, port_id);

	/* FIXME: convert to proper pll */
	pll->pll_id = (enum pll) pipe_id;
	pll->offset = DPLL(pipe_id);
	pll->port_id = port_id;

	return true;
}

bool vlv_pll_destroy(struct vlv_pll *pll)
{
	return false;
}
