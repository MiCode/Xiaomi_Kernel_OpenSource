/*
 * drivers/video/tegra/dc/lut.c
 *
 * Copyright (c) 2010-2012, NVIDIA CORPORATION, All rights reserved.
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

void tegra_dc_init_lut_defaults(struct tegra_dc_lut *lut)
{
	int i;
	for (i = 0; i < 256; i++)
		lut->r[i] = lut->g[i] = lut->b[i] = (u8)i;
}

static int tegra_dc_loop_lut(struct tegra_dc *dc,
			     struct tegra_dc_win *win,
			     int(*lambda)(struct tegra_dc *dc, int i, u32 rgb))
{
	struct tegra_dc_lut *lut = &win->lut;
	struct tegra_dc_lut *global_lut = &dc->fb_lut;
	int i;
	for (i = 0; i < 256; i++) {

		u32 r = (u32)lut->r[i];
		u32 g = (u32)lut->g[i];
		u32 b = (u32)lut->b[i];

		if (!(win->ppflags & TEGRA_WIN_PPFLAG_CP_FBOVERRIDE)) {
			r = (u32)global_lut->r[r];
			g = (u32)global_lut->g[g];
			b = (u32)global_lut->b[b];
		}

		if (!lambda(dc, i, r | (g<<8) | (b<<16)))
			return 0;
	}
	return 1;
}

static int tegra_dc_lut_isdefaults_lambda(struct tegra_dc *dc, int i, u32 rgb)
{
	if (rgb != (i | (i<<8) | (i<<16)))
		return 0;
	return 1;
}

static int tegra_dc_set_lut_setreg_lambda(struct tegra_dc *dc, int i, u32 rgb)
{
	tegra_dc_writel(dc, rgb, DC_WIN_COLOR_PALETTE(i));
	return 1;
}

void tegra_dc_set_lut(struct tegra_dc *dc, struct tegra_dc_win *win)
{
	unsigned long val = tegra_dc_readl(dc, DC_WIN_WIN_OPTIONS);

	tegra_dc_loop_lut(dc, win, tegra_dc_set_lut_setreg_lambda);

	if (win->ppflags & TEGRA_WIN_PPFLAG_CP_ENABLE)
		val |= CP_ENABLE;
	else
		val &= ~CP_ENABLE;

	tegra_dc_writel(dc, val, DC_WIN_WIN_OPTIONS);
}

static int tegra_dc_update_winlut(struct tegra_dc *dc, int win_idx, int fbovr)
{
	struct tegra_dc_win *win = &dc->windows[win_idx];

	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);

	if (!dc->enabled) {
		tegra_dc_release_dc_out(dc);
		tegra_dc_io_end(dc);
		mutex_unlock(&dc->lock);
		return -EFAULT;
	}

	if (fbovr > 0)
		win->ppflags |= TEGRA_WIN_PPFLAG_CP_FBOVERRIDE;
	else if (fbovr == 0)
		win->ppflags &= ~TEGRA_WIN_PPFLAG_CP_FBOVERRIDE;

	if (!tegra_dc_loop_lut(dc, win, tegra_dc_lut_isdefaults_lambda))
		win->ppflags |= TEGRA_WIN_PPFLAG_CP_ENABLE;
	else
		win->ppflags &= ~TEGRA_WIN_PPFLAG_CP_ENABLE;

	tegra_dc_writel(dc, WINDOW_A_SELECT << win_idx,
			DC_CMD_DISPLAY_WINDOW_HEADER);

	tegra_dc_set_lut(dc, win);

	mutex_unlock(&dc->lock);

	tegra_dc_update_windows(&win, 1);
	tegra_dc_sync_windows(&win, 1);

	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	return 0;
}

int tegra_dc_update_lut(struct tegra_dc *dc, int win_idx, int fboveride)
{
	if (win_idx > -1)
		return tegra_dc_update_winlut(dc, win_idx, fboveride);

	for (win_idx = 0; win_idx < DC_N_WINDOWS; win_idx++) {
		int err = tegra_dc_update_winlut(dc, win_idx, fboveride);
		if (err)
			return err;
	}

	return 0;
}
EXPORT_SYMBOL(tegra_dc_update_lut);

