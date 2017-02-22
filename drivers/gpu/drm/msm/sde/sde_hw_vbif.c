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
#include "sde_hw_vbif.h"

#define VBIF_VERSION			0x0000
#define VBIF_CLK_FORCE_CTRL0		0x0008
#define VBIF_CLK_FORCE_CTRL1		0x000C
#define VBIF_QOS_REMAP_00		0x0020
#define VBIF_QOS_REMAP_01		0x0024
#define VBIF_QOS_REMAP_10		0x0028
#define VBIF_QOS_REMAP_11		0x002C
#define VBIF_WRITE_GATHTER_EN		0x00AC
#define VBIF_IN_RD_LIM_CONF0		0x00B0
#define VBIF_IN_RD_LIM_CONF1		0x00B4
#define VBIF_IN_RD_LIM_CONF2		0x00B8
#define VBIF_IN_WR_LIM_CONF0		0x00C0
#define VBIF_IN_WR_LIM_CONF1		0x00C4
#define VBIF_IN_WR_LIM_CONF2		0x00C8
#define VBIF_OUT_RD_LIM_CONF0		0x00D0
#define VBIF_OUT_WR_LIM_CONF0		0x00D4
#define VBIF_XIN_HALT_CTRL0		0x0200
#define VBIF_XIN_HALT_CTRL1		0x0204

static void sde_hw_set_limit_conf(struct sde_hw_vbif *vbif,
		u32 xin_id, bool rd, u32 limit)
{
	struct sde_hw_blk_reg_map *c = &vbif->hw;
	u32 reg_val;
	u32 reg_off;
	u32 bit_off;

	if (rd)
		reg_off = VBIF_IN_RD_LIM_CONF0;
	else
		reg_off = VBIF_IN_WR_LIM_CONF0;

	reg_off += (xin_id / 4) * 4;
	bit_off = (xin_id % 4) * 8;
	reg_val = SDE_REG_READ(c, reg_off);
	reg_val &= ~(0xFF << bit_off);
	reg_val |= (limit) << bit_off;
	SDE_REG_WRITE(c, reg_off, reg_val);
}

static u32 sde_hw_get_limit_conf(struct sde_hw_vbif *vbif,
		u32 xin_id, bool rd)
{
	struct sde_hw_blk_reg_map *c = &vbif->hw;
	u32 reg_val;
	u32 reg_off;
	u32 bit_off;
	u32 limit;

	if (rd)
		reg_off = VBIF_IN_RD_LIM_CONF0;
	else
		reg_off = VBIF_IN_WR_LIM_CONF0;

	reg_off += (xin_id / 4) * 4;
	bit_off = (xin_id % 4) * 8;
	reg_val = SDE_REG_READ(c, reg_off);
	limit = (reg_val >> bit_off) & 0xFF;

	return limit;
}

static void sde_hw_set_halt_ctrl(struct sde_hw_vbif *vbif,
		u32 xin_id, bool enable)
{
	struct sde_hw_blk_reg_map *c = &vbif->hw;
	u32 reg_val;

	reg_val = SDE_REG_READ(c, VBIF_XIN_HALT_CTRL0);

	if (enable)
		reg_val |= BIT(xin_id);
	else
		reg_val &= ~BIT(xin_id);

	SDE_REG_WRITE(c, VBIF_XIN_HALT_CTRL0, reg_val);
}

static bool sde_hw_get_halt_ctrl(struct sde_hw_vbif *vbif,
		u32 xin_id)
{
	struct sde_hw_blk_reg_map *c = &vbif->hw;
	u32 reg_val;

	reg_val = SDE_REG_READ(c, VBIF_XIN_HALT_CTRL1);

	return (reg_val & BIT(xin_id)) ? true : false;
}

static void _setup_vbif_ops(struct sde_hw_vbif_ops *ops,
		unsigned long cap)
{
	ops->set_limit_conf = sde_hw_set_limit_conf;
	ops->get_limit_conf = sde_hw_get_limit_conf;
	ops->set_halt_ctrl = sde_hw_set_halt_ctrl;
	ops->get_halt_ctrl = sde_hw_get_halt_ctrl;
}

static const struct sde_vbif_cfg *_top_offset(enum sde_vbif vbif,
		const struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->vbif_count; i++) {
		if (vbif == m->vbif[i].id) {
			b->base_off = addr;
			b->blk_off = m->vbif[i].base;
			b->length = m->vbif[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_VBIF;
			return &m->vbif[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

struct sde_hw_vbif *sde_hw_vbif_init(enum sde_vbif idx,
		void __iomem *addr,
		const struct sde_mdss_cfg *m)
{
	struct sde_hw_vbif *c;
	const struct sde_vbif_cfg *cfg;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _top_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/*
	 * Assign ops
	 */
	c->idx = idx;
	c->cap = cfg;
	_setup_vbif_ops(&c->ops, c->cap->features);

	return c;
}

void sde_hw_vbif_destroy(struct sde_hw_vbif *vbif)
{
	kfree(vbif);
}
