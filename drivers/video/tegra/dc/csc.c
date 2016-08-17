/*
 * drivers/video/tegra/dc/csc.c
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/types.h>
#include <mach/dc.h>

#include "dc_reg.h"
#include "dc_priv.h"

void tegra_dc_init_csc_defaults(struct tegra_dc_csc *csc)
{
	csc->yof   = 0x00f0;
	csc->kyrgb = 0x012a;
	csc->kur   = 0x0000;
	csc->kvr   = 0x0198;
	csc->kug   = 0x039b;
	csc->kvg   = 0x032f;
	csc->kub   = 0x0204;
	csc->kvb   = 0x0000;
}

void tegra_dc_set_csc(struct tegra_dc *dc, struct tegra_dc_csc *csc)
{
	tegra_dc_writel(dc, csc->yof,	DC_WIN_CSC_YOF);
	tegra_dc_writel(dc, csc->kyrgb,	DC_WIN_CSC_KYRGB);
	tegra_dc_writel(dc, csc->kur,	DC_WIN_CSC_KUR);
	tegra_dc_writel(dc, csc->kvr,	DC_WIN_CSC_KVR);
	tegra_dc_writel(dc, csc->kug,	DC_WIN_CSC_KUG);
	tegra_dc_writel(dc, csc->kvg,	DC_WIN_CSC_KVG);
	tegra_dc_writel(dc, csc->kub,	DC_WIN_CSC_KUB);
	tegra_dc_writel(dc, csc->kvb,	DC_WIN_CSC_KVB);
}

int tegra_dc_update_csc(struct tegra_dc *dc, int win_idx)
{
	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return -EFAULT;
	}

	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);
	tegra_dc_writel(dc, WINDOW_A_SELECT << win_idx,
			DC_CMD_DISPLAY_WINDOW_HEADER);

	tegra_dc_set_csc(dc, &dc->windows[win_idx].csc);
	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);

	mutex_unlock(&dc->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_update_csc);

