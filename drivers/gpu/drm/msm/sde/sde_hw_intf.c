/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_intf.h"

#define INTF_TIMING_ENGINE_EN           0x000
#define INTF_CONFIG                     0x004
#define INTF_HSYNC_CTL                  0x008
#define INTF_VSYNC_PERIOD_F0            0x00C
#define INTF_VSYNC_PERIOD_F1            0x010
#define INTF_VSYNC_PULSE_WIDTH_F0       0x014
#define INTF_VSYNC_PULSE_WIDTH_F1       0x018
#define INTF_DISPLAY_V_START_F0         0x01C
#define INTF_DISPLAY_V_START_F1         0x020
#define INTF_DISPLAY_V_END_F0           0x024
#define INTF_DISPLAY_V_END_F1           0x028
#define INTF_ACTIVE_V_START_F0          0x02C
#define INTF_ACTIVE_V_START_F1          0x030
#define INTF_ACTIVE_V_END_F0            0x034
#define INTF_ACTIVE_V_END_F1            0x038
#define INTF_DISPLAY_HCTL               0x03C
#define INTF_ACTIVE_HCTL                0x040
#define INTF_BORDER_COLOR               0x044
#define INTF_UNDERFLOW_COLOR            0x048
#define INTF_HSYNC_SKEW                 0x04C
#define INTF_POLARITY_CTL               0x050
#define INTF_TEST_CTL                   0x054
#define INTF_TP_COLOR0                  0x058
#define INTF_TP_COLOR1                  0x05C
#define INTF_FRAME_LINE_COUNT_EN        0x0A8
#define INTF_FRAME_COUNT                0x0AC
#define   INTF_LINE_COUNT               0x0B0

#define   INTF_DEFLICKER_CONFIG         0x0F0
#define   INTF_DEFLICKER_STRNG_COEFF    0x0F4
#define   INTF_DEFLICKER_WEAK_COEFF     0x0F8

#define   INTF_DSI_CMD_MODE_TRIGGER_EN  0x084
#define   INTF_PANEL_FORMAT             0x090
#define   INTF_TPG_ENABLE               0x100
#define   INTF_TPG_MAIN_CONTROL         0x104
#define   INTF_TPG_VIDEO_CONFIG         0x108
#define   INTF_TPG_COMPONENT_LIMITS     0x10C
#define   INTF_TPG_RECTANGLE            0x110
#define   INTF_TPG_INITIAL_VALUE        0x114
#define   INTF_TPG_BLK_WHITE_PATTERN_FRAMES   0x118
#define   INTF_TPG_RGB_MAPPING          0x11C
#define   INTF_PROG_FETCH_START         0x170

#define   INTF_FRAME_LINE_COUNT_EN      0x0A8
#define   INTF_FRAME_COUNT              0x0AC
#define   INTF_LINE_COUNT               0x0B0

#define INTF_MISR_CTRL			0x180
#define INTF_MISR_SIGNATURE		0x184

#define MISR_FRAME_COUNT_MASK		0xFF
#define MISR_CTRL_ENABLE		BIT(8)
#define MISR_CTRL_STATUS		BIT(9)
#define MISR_CTRL_STATUS_CLEAR		BIT(10)
#define INTF_MISR_CTRL_FREE_RUN_MASK	BIT(31)

static struct sde_intf_cfg *_intf_offset(enum sde_intf intf,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->intf_count; i++) {
		if ((intf == m->intf[i].id) &&
		(m->intf[i].type != INTF_NONE)) {
			b->base_off = addr;
			b->blk_off = m->intf[i].base;
			b->length = m->intf[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_INTF;
			return &m->intf[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

static void sde_hw_intf_setup_timing_engine(struct sde_hw_intf *ctx,
		const struct intf_timing_params *p,
		const struct sde_format *fmt)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 hsync_period, vsync_period;
	u32 display_v_start, display_v_end;
	u32 hsync_start_x, hsync_end_x;
	u32 active_h_start, active_h_end;
	u32 active_v_start, active_v_end;
	u32 active_hctl, display_hctl, hsync_ctl;
	u32 polarity_ctl, den_polarity, hsync_polarity, vsync_polarity;
	u32 panel_format;
	u32 intf_cfg;

	/* read interface_cfg */
	intf_cfg = SDE_REG_READ(c, INTF_CONFIG);
	hsync_period = p->hsync_pulse_width + p->h_back_porch + p->width +
	p->h_front_porch;
	vsync_period = p->vsync_pulse_width + p->v_back_porch + p->height +
	p->v_front_porch;

	display_v_start = ((p->vsync_pulse_width + p->v_back_porch) *
	hsync_period) + p->hsync_skew;
	display_v_end = ((vsync_period - p->v_front_porch) * hsync_period) +
	p->hsync_skew - 1;

	if (ctx->cap->type == INTF_EDP) {
		display_v_start += p->hsync_pulse_width + p->h_back_porch;
		display_v_end -= p->h_front_porch;
	}

	hsync_start_x = p->h_back_porch + p->hsync_pulse_width;
	hsync_end_x = hsync_period - p->h_front_porch - 1;

	if (p->width != p->xres) {
		active_h_start = hsync_start_x;
		active_h_end = active_h_start + p->xres - 1;
	} else {
		active_h_start = 0;
		active_h_end = 0;
	}

	if (p->height != p->yres) {
		active_v_start = display_v_start;
		active_v_end = active_v_start + (p->yres * hsync_period) - 1;
	} else {
		active_v_start = 0;
		active_v_end = 0;
	}

	if (active_h_end) {
		active_hctl = (active_h_end << 16) | active_h_start;
		intf_cfg |= BIT(29);	/* ACTIVE_H_ENABLE */
	} else {
		active_hctl = 0;
	}

	if (active_v_end)
		intf_cfg |= BIT(30); /* ACTIVE_V_ENABLE */

	hsync_ctl = (hsync_period << 16) | p->hsync_pulse_width;
	display_hctl = (hsync_end_x << 16) | hsync_start_x;

	den_polarity = 0;
	hsync_polarity = p->hsync_polarity;
	vsync_polarity = p->vsync_polarity;
	polarity_ctl = (den_polarity << 2) | /*  DEN Polarity  */
		(vsync_polarity << 1) | /* VSYNC Polarity */
		(hsync_polarity << 0);  /* HSYNC Polarity */

	if (!SDE_FORMAT_IS_YUV(fmt))
		panel_format = (fmt->bits[C0_G_Y] |
				(fmt->bits[C1_B_Cb] << 2) |
				(fmt->bits[C2_R_Cr] << 4) |
				(0x21 << 8));
	 else
		/* Interface treats all the pixel data in RGB888 format */
		panel_format = (COLOR_8BIT |
				(COLOR_8BIT << 2) |
				(COLOR_8BIT << 4) |
				(0x21 << 8));

	SDE_REG_WRITE(c, INTF_HSYNC_CTL, hsync_ctl);
	SDE_REG_WRITE(c, INTF_VSYNC_PERIOD_F0, vsync_period * hsync_period);
	SDE_REG_WRITE(c, INTF_VSYNC_PULSE_WIDTH_F0,
			p->vsync_pulse_width * hsync_period);
	SDE_REG_WRITE(c, INTF_DISPLAY_HCTL, display_hctl);
	SDE_REG_WRITE(c, INTF_DISPLAY_V_START_F0, display_v_start);
	SDE_REG_WRITE(c, INTF_DISPLAY_V_END_F0, display_v_end);
	SDE_REG_WRITE(c, INTF_ACTIVE_HCTL,  active_hctl);
	SDE_REG_WRITE(c, INTF_ACTIVE_V_START_F0, active_v_start);
	SDE_REG_WRITE(c, INTF_ACTIVE_V_END_F0, active_v_end);
	SDE_REG_WRITE(c, INTF_BORDER_COLOR, p->border_clr);
	SDE_REG_WRITE(c, INTF_UNDERFLOW_COLOR, p->underflow_clr);
	SDE_REG_WRITE(c, INTF_HSYNC_SKEW, p->hsync_skew);
	SDE_REG_WRITE(c, INTF_POLARITY_CTL, polarity_ctl);
	SDE_REG_WRITE(c, INTF_FRAME_LINE_COUNT_EN, 0x3);
	SDE_REG_WRITE(c, INTF_CONFIG, intf_cfg);
	SDE_REG_WRITE(c, INTF_PANEL_FORMAT, panel_format);
}

static void sde_hw_intf_enable_timing_engine(
		struct sde_hw_intf *intf,
		u8 enable)
{
	struct sde_hw_blk_reg_map *c = &intf->hw;
	/* Note: Display interface select is handled in top block hw layer */
	SDE_REG_WRITE(c, INTF_TIMING_ENGINE_EN, enable != 0);
}

static void sde_hw_intf_setup_prg_fetch(
		struct sde_hw_intf *intf,
		const struct intf_prog_fetch *fetch)
{
	struct sde_hw_blk_reg_map *c = &intf->hw;
	int fetch_enable;

	/*
	 * Fetch should always be outside the active lines. If the fetching
	 * is programmed within active region, hardware behavior is unknown.
	 */

	fetch_enable = SDE_REG_READ(c, INTF_CONFIG);
	if (fetch->enable) {
		fetch_enable |= BIT(31);
		SDE_REG_WRITE(c, INTF_PROG_FETCH_START,
				fetch->fetch_start);
	} else {
		fetch_enable &= ~BIT(31);
	}

	SDE_REG_WRITE(c, INTF_CONFIG, fetch_enable);
}

static void sde_hw_intf_get_status(
		struct sde_hw_intf *intf,
		struct intf_status *s)
{
	struct sde_hw_blk_reg_map *c = &intf->hw;

	s->is_en = SDE_REG_READ(c, INTF_TIMING_ENGINE_EN);
	if (s->is_en) {
		s->frame_count = SDE_REG_READ(c, INTF_FRAME_COUNT);
		s->line_count = SDE_REG_READ(c, INTF_LINE_COUNT);
	} else {
		s->line_count = 0;
		s->frame_count = 0;
	}
}

static void sde_hw_intf_set_misr(struct sde_hw_intf *intf,
		struct sde_misr_params *misr_map)
{
	struct sde_hw_blk_reg_map *c = &intf->hw;
	u32 config = 0;

	if (!misr_map)
		return;

	SDE_REG_WRITE(c, INTF_MISR_CTRL, MISR_CTRL_STATUS_CLEAR);
	/* Clear data */
	wmb();

	if (misr_map->enable) {
		config = (MISR_FRAME_COUNT_MASK & 1) |
			(MISR_CTRL_ENABLE);

		SDE_REG_WRITE(c, INTF_MISR_CTRL, config);
	} else {
		SDE_REG_WRITE(c, INTF_MISR_CTRL, 0);
	}
}

static void sde_hw_intf_collect_misr(struct sde_hw_intf *intf,
		struct sde_misr_params *misr_map)
{
	struct sde_hw_blk_reg_map *c = &intf->hw;

	if (!misr_map)
		return;

	if (misr_map->enable) {
		if (misr_map->last_idx < misr_map->frame_count &&
			misr_map->last_idx < SDE_CRC_BATCH_SIZE)
			misr_map->crc_value[misr_map->last_idx] =
				SDE_REG_READ(c, INTF_MISR_SIGNATURE);
	}

	misr_map->enable =
		misr_map->enable & (misr_map->last_idx <= SDE_CRC_BATCH_SIZE);

	misr_map->last_idx++;
}

static void _setup_intf_ops(struct sde_hw_intf_ops *ops,
		unsigned long cap)
{
	ops->setup_timing_gen = sde_hw_intf_setup_timing_engine;
	ops->setup_prg_fetch  = sde_hw_intf_setup_prg_fetch;
	ops->get_status = sde_hw_intf_get_status;
	ops->enable_timing = sde_hw_intf_enable_timing_engine;
	ops->setup_misr = sde_hw_intf_set_misr;
	ops->collect_misr = sde_hw_intf_collect_misr;
}

struct sde_hw_intf *sde_hw_intf_init(enum sde_intf idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_intf *c;
	struct sde_intf_cfg *cfg;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _intf_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		pr_err("failed to create sde_hw_intf %d\n", idx);
		return ERR_PTR(-EINVAL);
	}

	/*
	 * Assign ops
	 */
	c->idx = idx;
	c->cap = cfg;
	c->mdss = m;
	_setup_intf_ops(&c->ops, c->cap->features);

	/*
	 * Perform any default initialization for the intf
	 */
	return c;
}

void sde_hw_intf_destroy(struct sde_hw_intf *intf)
{
	kfree(intf);
}

