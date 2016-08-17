/*
 * drivers/video/tegra/dc/mode.c
 *
 * Copyright (C) 2010 Google, Inc.
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
#include <linux/clk.h>
#include <linux/delay.h>

#include <mach/clk.h>
#include <mach/dc.h>
#include <trace/events/display.h>

#include "dc_reg.h"
#include "dc_priv.h"

/* return non-zero if constraint is violated */
static int calc_h_ref_to_sync(const struct tegra_dc_mode *mode, int *href)
{
	long a, b;

	/* Constraint 5: H_REF_TO_SYNC >= 0 */
	a = 0;

	/* Constraint 6: H_FRONT_PORT >= (H_REF_TO_SYNC + 1) */
	b = mode->h_front_porch - 1;

	/* Constraint 1: H_REF_TO_SYNC + H_SYNC_WIDTH + H_BACK_PORCH > 11 */
	if (a + mode->h_sync_width + mode->h_back_porch <= 11)
		a = 1 + 11 - mode->h_sync_width - mode->h_back_porch;
	/* check Constraint 1 and 6 */
	if (a > b)
		return 1;

	/* Constraint 4: H_SYNC_WIDTH >= 1 */
	if (mode->h_sync_width < 1)
		return 4;

	/* Constraint 7: H_DISP_ACTIVE >= 16 */
	if (mode->h_active < 16)
		return 7;

	if (href) {
		if (b > a && a % 2)
			*href = a + 1; /* use smallest even value */
		else
			*href = a; /* even or only possible value */
	}

	return 0;
}

static int calc_v_ref_to_sync(const struct tegra_dc_mode *mode, int *vref)
{
	long a;
	a = 1; /* Constraint 5: V_REF_TO_SYNC >= 1 */

	/* Constraint 2: V_REF_TO_SYNC + V_SYNC_WIDTH + V_BACK_PORCH > 1 */
	if (a + mode->v_sync_width + mode->v_back_porch <= 1)
		a = 1 + 1 - mode->v_sync_width - mode->v_back_porch;

	/* Constraint 6 */
	if (mode->v_front_porch < a + 1)
		a = mode->v_front_porch - 1;

	/* Constraint 4: V_SYNC_WIDTH >= 1 */
	if (mode->v_sync_width < 1)
		return 4;

	/* Constraint 7: V_DISP_ACTIVE >= 16 */
	if (mode->v_active < 16)
		return 7;

	if (vref)
		*vref = a;
	return 0;
}

static int calc_ref_to_sync(struct tegra_dc_mode *mode)
{
	int ret;
	ret = calc_h_ref_to_sync(mode, &mode->h_ref_to_sync);
	if (ret)
		return ret;
	ret = calc_v_ref_to_sync(mode, &mode->v_ref_to_sync);
	if (ret)
		return ret;

	return 0;
}

static bool check_ref_to_sync(struct tegra_dc_mode *mode)
{
	/* Constraint 1: H_REF_TO_SYNC + H_SYNC_WIDTH + H_BACK_PORCH > 11. */
	if (mode->h_ref_to_sync + mode->h_sync_width + mode->h_back_porch <= 11)
		return false;

	/* Constraint 2: V_REF_TO_SYNC + V_SYNC_WIDTH + V_BACK_PORCH > 1. */
	if (mode->v_ref_to_sync + mode->v_sync_width + mode->v_back_porch <= 1)
		return false;

	/* Constraint 3: V_FRONT_PORCH + V_SYNC_WIDTH + V_BACK_PORCH > 1
	 * (vertical blank). */
	if (mode->v_front_porch + mode->v_sync_width + mode->v_back_porch <= 1)
		return false;

	/* Constraint 4: V_SYNC_WIDTH >= 1; H_SYNC_WIDTH >= 1. */
	if (mode->v_sync_width < 1 || mode->h_sync_width < 1)
		return false;

	/* Constraint 5: V_REF_TO_SYNC >= 1; H_REF_TO_SYNC >= 0. */
	if (mode->v_ref_to_sync < 1 || mode->h_ref_to_sync < 0)
		return false;

	/* Constraint 6: V_FRONT_PORT >= (V_REF_TO_SYNC + 1);
	 * H_FRONT_PORT >= (H_REF_TO_SYNC + 1). */
	if (mode->v_front_porch < mode->v_ref_to_sync + 1 ||
		mode->h_front_porch < mode->h_ref_to_sync + 1)
		return false;

	/* Constraint 7: H_DISP_ACTIVE >= 16; V_DISP_ACTIVE >= 16. */
	if (mode->h_active < 16 || mode->v_active < 16)
		return false;

	return true;
}

static s64 calc_frametime_ns(const struct tegra_dc_mode *m)
{
	long h_total, v_total;
	h_total = m->h_active + m->h_front_porch + m->h_back_porch +
		m->h_sync_width;
	v_total = m->v_active + m->v_front_porch + m->v_back_porch +
		m->v_sync_width;
	return (!m->pclk) ? 0 : (s64)(div_s64(((s64)h_total * v_total *
					1000000000ULL), m->pclk));
}

/* return in 1000ths of a Hertz */
int tegra_dc_calc_refresh(const struct tegra_dc_mode *m)
{
	long h_total, v_total, refresh;
	long pclk;

	if (m->rated_pclk > 0)
		pclk = m->rated_pclk;
	else
		pclk = m->pclk;

	h_total = m->h_active + m->h_front_porch + m->h_back_porch +
		m->h_sync_width;
	v_total = m->v_active + m->v_front_porch + m->v_back_porch +
		m->v_sync_width;
	if (!pclk || !h_total || !v_total)
		return 0;
	refresh = pclk / h_total;
	refresh *= 1000;
	refresh /= v_total;
	return refresh;
}

#ifdef DEBUG
static void print_mode(struct tegra_dc *dc,
			const struct tegra_dc_mode *mode, const char *note)
{
	if (mode) {
		int refresh = tegra_dc_calc_refresh(mode);
		dev_info(&dc->ndev->dev, "%s():MODE:%dx%d@%d.%03uHz pclk=%d\n",
			note ? note : "",
			mode->h_active, mode->v_active,
			refresh / 1000, refresh % 1000,
			mode->pclk);
	}
}
#else /* !DEBUG */
static inline void print_mode(struct tegra_dc *dc,
			const struct tegra_dc_mode *mode, const char *note) { }
#endif /* DEBUG */

int tegra_dc_program_mode(struct tegra_dc *dc, struct tegra_dc_mode *mode)
{
	unsigned long val;
	unsigned long rate;
	unsigned long div;
	unsigned long pclk;

	print_mode(dc, mode, __func__);

	/* use default EMC rate when switching modes */
	dc->new_emc_clk_rate = tegra_dc_get_default_emc_clk_rate(dc);
	tegra_dc_program_bandwidth(dc, true);

	tegra_dc_writel(dc, 0x0, DC_DISP_DISP_TIMING_OPTIONS);
	tegra_dc_writel(dc, mode->h_ref_to_sync | (mode->v_ref_to_sync << 16),
			DC_DISP_REF_TO_SYNC);
	tegra_dc_writel(dc, mode->h_sync_width | (mode->v_sync_width << 16),
			DC_DISP_SYNC_WIDTH);
	tegra_dc_writel(dc, mode->h_back_porch | (mode->v_back_porch << 16),
			DC_DISP_BACK_PORCH);
	tegra_dc_writel(dc, mode->h_active | (mode->v_active << 16),
			DC_DISP_DISP_ACTIVE);
	tegra_dc_writel(dc, mode->h_front_porch | (mode->v_front_porch << 16),
			DC_DISP_FRONT_PORCH);

	tegra_dc_writel(dc, DE_SELECT_ACTIVE | DE_CONTROL_NORMAL,
			DC_DISP_DATA_ENABLE_OPTIONS);

	/* TODO: MIPI/CRT/HDMI clock cals */

	val = 0;
	if (!(dc->out->type == TEGRA_DC_OUT_DSI ||
		dc->out->type == TEGRA_DC_OUT_HDMI)) {
		val = DISP_DATA_FORMAT_DF1P1C;

		if (dc->out->align == TEGRA_DC_ALIGN_MSB)
			val |= DISP_DATA_ALIGNMENT_MSB;
		else
			val |= DISP_DATA_ALIGNMENT_LSB;

		if (dc->out->order == TEGRA_DC_ORDER_RED_BLUE)
			val |= DISP_DATA_ORDER_RED_BLUE;
		else
			val |= DISP_DATA_ORDER_BLUE_RED;
	}
	tegra_dc_writel(dc, val, DC_DISP_DISP_INTERFACE_CONTROL);

	rate = tegra_dc_clk_get_rate(dc);

	pclk = tegra_dc_pclk_round_rate(dc, mode->pclk);
	if (pclk < (mode->pclk / 100 * 99) ||
	    pclk > (mode->pclk / 100 * 109)) {
		dev_err(&dc->ndev->dev,
			"can't divide %ld clock to %d -1/+9%% %ld %d %d\n",
			rate, mode->pclk,
			pclk, (mode->pclk / 100 * 99),
			(mode->pclk / 100 * 109));
		return -EINVAL;
	}

	div = (rate * 2 / pclk) - 2;

	/* SW WAR for bug 1045373. To make the shift clk dividor effect under
	 * all circumstances, write N+2 to SHIFT_CLK_DIVIDER and activate it.
	 * After 2us delay, write the target values to it. */
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	tegra_dc_writel(dc, PIXEL_CLK_DIVIDER_PCD1 | SHIFT_CLK_DIVIDER(div + 2),
			DC_DISP_DISP_CLOCK_CONTROL);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	udelay(2);
#endif

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* Deprecated on t11x and t14x. */
	tegra_dc_writel(dc, 0x00010001,
			DC_DISP_SHIFT_CLOCK_OPTIONS);
#endif

	tegra_dc_writel(dc, PIXEL_CLK_DIVIDER_PCD1 | SHIFT_CLK_DIVIDER(div),
			DC_DISP_DISP_CLOCK_CONTROL);

#ifdef CONFIG_SWITCH
	switch_set_state(&dc->modeset_switch,
			 (mode->h_active << 16) | mode->v_active);
#endif

	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	dc->mode_dirty = false;

	trace_display_mode(dc, &dc->mode);
	return 0;
}

static int panel_sync_rate;

int tegra_dc_get_panel_sync_rate(void)
{
	return panel_sync_rate;
}
EXPORT_SYMBOL(tegra_dc_get_panel_sync_rate);

int tegra_dc_set_mode(struct tegra_dc *dc, const struct tegra_dc_mode *mode)
{
	mutex_lock(&dc->lock);
	memcpy(&dc->mode, mode, sizeof(dc->mode));
	dc->mode_dirty = true;

	if (dc->out->type == TEGRA_DC_OUT_RGB)
		panel_sync_rate = tegra_dc_calc_refresh(mode);
	else if (dc->out->type == TEGRA_DC_OUT_DSI)
		panel_sync_rate = dc->out->dsi->rated_refresh_rate * 1000;

	print_mode(dc, mode, __func__);
	dc->frametime_ns = calc_frametime_ns(mode);
	mutex_unlock(&dc->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_dc_set_mode);

int tegra_dc_to_fb_videomode(struct fb_videomode *fbmode,
	const struct tegra_dc_mode *mode)
{
	long mode_pclk;

	if (!fbmode || !mode || !mode->pclk)
		return -EINVAL;
	if (mode->rated_pclk >= 1000) /* handle DSI one-shot modes */
		mode_pclk = mode->rated_pclk;
	else if (mode->pclk >= 1000) /* normal continous modes */
		mode_pclk = mode->pclk;
	else
		mode_pclk = 0;
	memset(fbmode, 0, sizeof(*fbmode));
	fbmode->right_margin = mode->h_front_porch;
	fbmode->lower_margin = mode->v_front_porch;
	fbmode->hsync_len = mode->h_sync_width;
	fbmode->vsync_len = mode->v_sync_width;
	fbmode->left_margin = mode->h_back_porch;
	fbmode->upper_margin = mode->v_back_porch;
	fbmode->xres = mode->h_active;
	fbmode->yres = mode->v_active;
	fbmode->vmode = FB_VMODE_NONINTERLACED;
	if (mode->stereo_mode) {
#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
		/* Double the pixel clock and update v_active only for
		 * frame packed mode */
		mode_pclk /= 2;
		/* total v_active = yres*2 + activespace */
		fbmode->yres = (mode->v_active - mode->v_sync_width -
			mode->v_back_porch - mode->v_front_porch) / 2;
		fbmode->vmode |= FB_VMODE_STEREO_FRAME_PACK;
#else
		fbmode->vmode |= FB_VMODE_STEREO_LEFT_RIGHT;
#endif
	}

	if (!(mode->flags & TEGRA_DC_MODE_FLAG_NEG_H_SYNC))
		fbmode->sync |=  FB_SYNC_HOR_HIGH_ACT;
	if (!(mode->flags & TEGRA_DC_MODE_FLAG_NEG_V_SYNC))
		fbmode->sync |= FB_SYNC_VERT_HIGH_ACT;
	if (mode->avi_m == TEGRA_DC_MODE_AVI_M_16_9)
		fbmode->flag |= FB_FLAG_RATIO_16_9;
	else if (mode->avi_m == TEGRA_DC_MODE_AVI_M_4_3)
		fbmode->flag |= FB_FLAG_RATIO_4_3;

	if (mode_pclk >= 1000) /* else 0 */
		fbmode->pixclock = KHZ2PICOS(mode_pclk / 1000);
	fbmode->refresh = tegra_dc_calc_refresh(mode) / 1000;

	return 0;
}

int tegra_dc_update_mode(struct tegra_dc *dc)
{
	if (dc->mode_dirty)
		return tegra_dc_program_mode(dc, &dc->mode);
	return 0;
}

int tegra_dc_set_fb_mode(struct tegra_dc *dc,
		const struct fb_videomode *fbmode, bool stereo_mode)
{
	struct tegra_dc_mode mode;

	if (!fbmode->pixclock)
		return -EINVAL;

	memset(&mode, 0, sizeof(mode));
	mode.pclk = PICOS2KHZ(fbmode->pixclock) * 1000;
	mode.h_sync_width = fbmode->hsync_len;
	mode.v_sync_width = fbmode->vsync_len;
	mode.h_back_porch = fbmode->left_margin;
	mode.v_back_porch = fbmode->upper_margin;
	mode.h_active = fbmode->xres;
	mode.v_active = fbmode->yres;
	mode.h_front_porch = fbmode->right_margin;
	mode.v_front_porch = fbmode->lower_margin;
	mode.stereo_mode = stereo_mode;
	if (fbmode->flag & FB_FLAG_RATIO_16_9)
		mode.avi_m = TEGRA_DC_MODE_AVI_M_16_9;
	else if (fbmode->flag & FB_FLAG_RATIO_4_3)
		mode.avi_m = TEGRA_DC_MODE_AVI_M_4_3;
	else if (dc->out) { /* if ratio is unspecified, detect a default */
		unsigned h_size = dc->out->h_size;
		unsigned v_size = dc->out->v_size;

		/* get aspect ratio */
		if (h_size * 18 > v_size * 31 && h_size * 18 < v_size * 33)
			mode.avi_m = TEGRA_DC_MODE_AVI_M_16_9;
		if (h_size * 18 > v_size * 23 && h_size * 18 < v_size * 25)
			mode.avi_m = TEGRA_DC_MODE_AVI_M_4_3;
	}

	if (dc->out->type == TEGRA_DC_OUT_HDMI) {
		/* HDMI controller requires h_ref=1, v_ref=1 */
		mode.h_ref_to_sync = 1;
		mode.v_ref_to_sync = 1;
	} else {
		calc_ref_to_sync(&mode);
	}
	if (!check_ref_to_sync(&mode)) {
		dev_err(&dc->ndev->dev,
				"Display timing doesn't meet restrictions.\n");
		return -EINVAL;
	}
	dev_dbg(&dc->ndev->dev, "Using mode %dx%d pclk=%d href=%d vref=%d\n",
		mode.h_active, mode.v_active, mode.pclk,
		mode.h_ref_to_sync, mode.v_ref_to_sync
	);

#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
	/* Double the pixel clock and update v_active only for
	 * frame packed mode */
	if (mode.stereo_mode) {
		mode.pclk *= 2;
		/* total v_active = yres*2 + activespace */
		mode.v_active = fbmode->yres * 2 +
				fbmode->vsync_len +
				fbmode->upper_margin +
				fbmode->lower_margin;
	}
#endif

	mode.flags = 0;

	if (!(fbmode->sync & FB_SYNC_HOR_HIGH_ACT))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_H_SYNC;

	if (!(fbmode->sync & FB_SYNC_VERT_HIGH_ACT))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_V_SYNC;

	return tegra_dc_set_mode(dc, &mode);
}
EXPORT_SYMBOL(tegra_dc_set_fb_mode);
