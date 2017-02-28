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

#include <linux/iopoll.h>

#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_pingpong.h"
#include "sde_dbg.h"

#define PP_TEAR_CHECK_EN                0x000
#define PP_SYNC_CONFIG_VSYNC            0x004
#define PP_SYNC_CONFIG_HEIGHT           0x008
#define PP_SYNC_WRCOUNT                 0x00C
#define PP_VSYNC_INIT_VAL               0x010
#define PP_INT_COUNT_VAL                0x014
#define PP_SYNC_THRESH                  0x018
#define PP_START_POS                    0x01C
#define PP_RD_PTR_IRQ                   0x020
#define PP_WR_PTR_IRQ                   0x024
#define PP_OUT_LINE_COUNT               0x028
#define PP_LINE_COUNT                   0x02C
#define PP_AUTOREFRESH_CONFIG           0x030

#define PP_FBC_MODE                     0x034
#define PP_FBC_BUDGET_CTL               0x038
#define PP_FBC_LOSSY_MODE               0x03C
#define PP_DSC_MODE                     0x0a0
#define PP_DCE_DATA_IN_SWAP             0x0ac
#define PP_DCE_DATA_OUT_SWAP            0x0c8

static struct sde_pingpong_cfg *_pingpong_offset(enum sde_pingpong pp,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->pingpong_count; i++) {
		if (pp == m->pingpong[i].id) {
			b->base_off = addr;
			b->blk_off = m->pingpong[i].base;
			b->length = m->pingpong[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_PINGPONG;
			return &m->pingpong[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

static int sde_hw_pp_setup_te_config(struct sde_hw_pingpong *pp,
		struct sde_hw_tear_check *te)
{
	struct sde_hw_blk_reg_map *c = &pp->hw;
	int cfg;

	cfg = BIT(19); /*VSYNC_COUNTER_EN */
	if (te->hw_vsync_mode)
		cfg |= BIT(20);

	cfg |= te->vsync_count;

	SDE_REG_WRITE(c, PP_SYNC_CONFIG_VSYNC, cfg);
	SDE_REG_WRITE(c, PP_SYNC_CONFIG_HEIGHT, te->sync_cfg_height);
	SDE_REG_WRITE(c, PP_VSYNC_INIT_VAL, te->vsync_init_val);
	SDE_REG_WRITE(c, PP_RD_PTR_IRQ, te->rd_ptr_irq);
	SDE_REG_WRITE(c, PP_START_POS, te->start_pos);
	SDE_REG_WRITE(c, PP_SYNC_THRESH,
			((te->sync_threshold_continue << 16) |
			 te->sync_threshold_start));
	SDE_REG_WRITE(c, PP_SYNC_WRCOUNT,
			(te->start_pos + te->sync_threshold_start + 1));

	return 0;
}

static int sde_hw_pp_setup_autorefresh_config(struct sde_hw_pingpong *pp,
		struct sde_hw_autorefresh *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 refresh_cfg;

	if (!pp || !cfg)
		return -EINVAL;
	c = &pp->hw;

	if (cfg->enable)
		refresh_cfg = BIT(31) | cfg->frame_count;
	else
		refresh_cfg = 0;

	SDE_REG_WRITE(c, PP_AUTOREFRESH_CONFIG, refresh_cfg);
	SDE_EVT32(pp->idx - PINGPONG_0, refresh_cfg);

	return 0;
}

static int sde_hw_pp_get_autorefresh_config(struct sde_hw_pingpong *pp,
		struct sde_hw_autorefresh *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 val;

	if (!pp || !cfg)
		return -EINVAL;

	c = &pp->hw;
	val = SDE_REG_READ(c, PP_AUTOREFRESH_CONFIG);
	cfg->enable = (val & BIT(31)) >> 31;
	cfg->frame_count = val & 0xffff;

	return 0;
}

static int sde_hw_pp_poll_timeout_wr_ptr(struct sde_hw_pingpong *pp,
		u32 timeout_us)
{
	struct sde_hw_blk_reg_map *c;
	u32 val;
	int rc;

	if (!pp)
		return -EINVAL;

	c = &pp->hw;
	rc = readl_poll_timeout(c->base_off + c->blk_off + PP_LINE_COUNT,
			val, (val & 0xffff) >= 1, 10, timeout_us);

	return rc;
}

static void sde_hw_pp_dsc_enable(struct sde_hw_pingpong *pp)
{
	struct sde_hw_blk_reg_map *c = &pp->hw;

	SDE_REG_WRITE(c, PP_DSC_MODE, 1);
}

static void sde_hw_pp_dsc_disable(struct sde_hw_pingpong *pp)
{
	struct sde_hw_blk_reg_map *c = &pp->hw;

	SDE_REG_WRITE(c, PP_DSC_MODE, 0);
}

static int sde_hw_pp_setup_dsc(struct sde_hw_pingpong *pp)
{
	struct sde_hw_blk_reg_map *pp_c = &pp->hw;
	int data;

	data = SDE_REG_READ(pp_c, PP_DCE_DATA_OUT_SWAP);
	data |= BIT(18); /* endian flip */
	SDE_REG_WRITE(pp_c, PP_DCE_DATA_OUT_SWAP, data);
	return 0;
}

static int sde_hw_pp_enable_te(struct sde_hw_pingpong *pp, bool enable)
{
	struct sde_hw_blk_reg_map *c = &pp->hw;

	SDE_REG_WRITE(c, PP_TEAR_CHECK_EN, enable);
	return 0;
}

static int sde_hw_pp_connect_external_te(struct sde_hw_pingpong *pp,
		bool enable_external_te)
{
	struct sde_hw_blk_reg_map *c = &pp->hw;
	u32 cfg;
	int orig;

	if (!pp)
		return -EINVAL;

	c = &pp->hw;
	cfg = SDE_REG_READ(c, PP_SYNC_CONFIG_VSYNC);
	orig = (bool)(cfg & BIT(20));
	if (enable_external_te)
		cfg |= BIT(20);
	else
		cfg &= ~BIT(20);
	SDE_REG_WRITE(c, PP_SYNC_CONFIG_VSYNC, cfg);
	SDE_EVT32(pp->idx - PINGPONG_0, cfg);

	return orig;
}

static int sde_hw_pp_get_vsync_info(struct sde_hw_pingpong *pp,
		struct sde_hw_pp_vsync_info *info)
{
	struct sde_hw_blk_reg_map *c = &pp->hw;
	u32 val;

	val = SDE_REG_READ(c, PP_VSYNC_INIT_VAL);
	info->rd_ptr_init_val = val & 0xffff;

	val = SDE_REG_READ(c, PP_INT_COUNT_VAL);
	info->rd_ptr_frame_count = (val & 0xffff0000) >> 16;
	info->rd_ptr_line_count = val & 0xffff;

	val = SDE_REG_READ(c, PP_LINE_COUNT);
	info->wr_ptr_line_count = val & 0xffff;

	return 0;
}

static void _setup_pingpong_ops(struct sde_hw_pingpong_ops *ops,
		unsigned long cap)
{
	ops->setup_tearcheck = sde_hw_pp_setup_te_config;
	ops->enable_tearcheck = sde_hw_pp_enable_te;
	ops->connect_external_te = sde_hw_pp_connect_external_te;
	ops->get_vsync_info = sde_hw_pp_get_vsync_info;
	ops->setup_autorefresh = sde_hw_pp_setup_autorefresh_config;
	ops->setup_dsc = sde_hw_pp_setup_dsc;
	ops->enable_dsc = sde_hw_pp_dsc_enable;
	ops->disable_dsc = sde_hw_pp_dsc_disable;
	ops->get_autorefresh = sde_hw_pp_get_autorefresh_config;
	ops->poll_timeout_wr_ptr = sde_hw_pp_poll_timeout_wr_ptr;
};

struct sde_hw_pingpong *sde_hw_pingpong_init(enum sde_pingpong idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_pingpong *c;
	struct sde_pingpong_cfg *cfg;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _pingpong_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	c->idx = idx;
	c->pingpong_hw_cap = cfg;
	_setup_pingpong_ops(&c->ops, c->pingpong_hw_cap->features);

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
			c->hw.blk_off + c->hw.length, c->hw.xin_id);

	return c;
}

void sde_hw_pingpong_destroy(struct sde_hw_pingpong *pp)
{
	kfree(pp);
}
