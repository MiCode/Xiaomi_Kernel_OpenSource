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

#include <linux/delay.h>
#include "sde_hwio.h"
#include "sde_hw_mdp_ctl.h"

#define   CTL_LAYER(lm)                 \
	(((lm) == LM_5) ? (0x024) : (((lm) - LM_0) * 0x004))
#define   CTL_LAYER_EXT(lm)             \
	(0x40 + (((lm) - LM_0) * 0x004))
#define   CTL_TOP                       0x014
#define   CTL_FLUSH                     0x018
#define   CTL_START                     0x01C
#define   CTL_PACK_3D                   0x020
#define   CTL_SW_RESET                  0x030
#define   CTL_LAYER_EXTN_OFFSET         0x40

#define SDE_REG_RESET_TIMEOUT_COUNT    20

static struct sde_ctl_cfg *_ctl_offset(enum sde_ctl ctl,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->ctl_count; i++) {
		if (ctl == m->ctl[i].id) {
			b->base_off = addr;
			b->blk_off = m->ctl[i].base;
			b->hwversion = m->hwversion;
			return &m->ctl[i];
		}
	}
	return ERR_PTR(-ENOMEM);
}

static int _mixer_stages(const struct sde_lm_cfg *mixer, int count,
		enum sde_lm lm)
{
	int i;
	int stages = -EINVAL;

	for (i = 0; i < count; i++) {
		if (lm == mixer[i].id) {
			stages = mixer[i].sblk->maxblendstages;
			break;
		}
	}

	return stages;
}

static inline void sde_hw_ctl_force_start(struct sde_hw_ctl *ctx)
{
	SDE_REG_WRITE(&ctx->hw, CTL_START, 0x1);
}

static inline void sde_hw_ctl_setup_flush(struct sde_hw_ctl *ctx, u32 flushbits)
{
	SDE_REG_WRITE(&ctx->hw, CTL_FLUSH, flushbits);
}

static inline int sde_hw_ctl_get_bitmask_sspp(struct sde_hw_ctl *ctx,
		u32 *flushbits, enum sde_sspp sspp)
{
	switch (sspp) {
	case SSPP_VIG0:
		*flushbits |=  BIT(0);
		break;
	case SSPP_VIG1:
		*flushbits |= BIT(1);
		break;
	case SSPP_VIG2:
		*flushbits |= BIT(2);
		break;
	case SSPP_VIG3:
		*flushbits |= BIT(18);
		break;
	case SSPP_RGB0:
		*flushbits |= BIT(3);
		break;
	case SSPP_RGB1:
		*flushbits |= BIT(4);
		break;
	case SSPP_RGB2:
		*flushbits |= BIT(5);
		break;
	case SSPP_RGB3:
		*flushbits |= BIT(19);
		break;
	case SSPP_DMA0:
		*flushbits |= BIT(11);
		break;
	case SSPP_DMA1:
		*flushbits |= BIT(12);
		break;
	case SSPP_CURSOR0:
		*flushbits |= BIT(22);
		break;
	case SSPP_CURSOR1:
		*flushbits |= BIT(23);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static inline int sde_hw_ctl_get_bitmask_mixer(struct sde_hw_ctl *ctx,
		u32 *flushbits, enum sde_lm lm)
{
	switch (lm) {
	case LM_0:
		*flushbits |= BIT(6);
		break;
	case LM_1:
		*flushbits |= BIT(7);
		break;
	case LM_2:
		*flushbits |= BIT(8);
		break;
	case LM_3:
		*flushbits |= BIT(9);
		break;
	case LM_4:
		*flushbits |= BIT(10);
		break;
	case LM_5:
		*flushbits |= BIT(20);
		break;
	default:
		return -EINVAL;
	}
	*flushbits |= BIT(17); /* CTL */
	return 0;
}

static inline int sde_hw_ctl_get_bitmask_dspp(struct sde_hw_ctl *ctx,
		u32 *flushbits, enum sde_dspp dspp)
{
	switch (dspp) {
	case DSPP_0:
		*flushbits |= BIT(13);
		break;
	case DSPP_1:
		*flushbits |= BIT(14);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static inline int sde_hw_ctl_get_bitmask_intf(struct sde_hw_ctl *ctx,
		u32 *flushbits, enum sde_intf intf)
{
	switch (intf) {
	case INTF_0:
		*flushbits |= BIT(31);
		break;
	case INTF_1:
		*flushbits |= BIT(30);
		break;
	case INTF_2:
		*flushbits |= BIT(29);
		break;
	case INTF_3:
		*flushbits |= BIT(28);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static inline int sde_hw_ctl_get_bitmask_cdm(struct sde_hw_ctl *ctx,
		u32 *flushbits, enum sde_cdm cdm)
{
	switch (cdm) {
	case CDM_0:
		*flushbits |= BIT(26);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sde_hw_ctl_reset_control(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	int count = SDE_REG_RESET_TIMEOUT_COUNT;
	int reset;

	SDE_REG_WRITE(c, CTL_SW_RESET, 0x1);

	for (; count > 0; count--) {
		/* insert small delay to avoid spinning the cpu while waiting */
		usleep_range(20, 50);
		reset = SDE_REG_READ(c, CTL_SW_RESET);
		if (reset == 0)
			return 0;
	}

	return -EINVAL;
}

static void sde_hw_ctl_setup_blendstage(struct sde_hw_ctl *ctx,
		enum sde_lm lm,
		struct sde_hw_stage_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 mixercfg, mixercfg_ext = 0;
	int i, j;
	u8 stages;
	int pipes_per_stage;

	stages = _mixer_stages(ctx->mixer_hw_caps, ctx->mixer_count, lm);
	if (WARN_ON(stages < 0))
		return;

	if (test_bit(SDE_MIXER_SOURCESPLIT,
		&ctx->mixer_hw_caps->features))
		pipes_per_stage = PIPES_PER_STAGE;
	else
		pipes_per_stage = 1;

	mixercfg = cfg->border_enable << 24; /* BORDER_OUT */

	for (i = 0; i <= stages; i++) {
		for (j = 0; j < pipes_per_stage; j++) {
			switch (cfg->stage[i][j]) {
			case SSPP_VIG0:
				mixercfg |= (i + 1) << 0;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 0;
				break;
			case SSPP_VIG1:
				mixercfg |= (i + 1) << 3;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 2;
				break;
			case SSPP_VIG2:
				mixercfg |= (i + 1) << 6;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 4;
				break;
			case SSPP_VIG3:
				mixercfg |= (i + 1) << 26;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 4;
				break;
			case SSPP_RGB0:
				mixercfg |= (i + 1) << 9;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 8;
				break;
			case SSPP_RGB1:
				mixercfg |= (i + 1) << 12;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 10;
				break;
			case SSPP_RGB2:
				mixercfg |= (i + 1) << 15;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 12;
				break;
			case SSPP_RGB3:
				mixercfg |= (i + 1) << 29;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 14;
				break;
			case SSPP_DMA0:
				mixercfg |= (i + 1) << 0;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 0;
				break;
			case SSPP_DMA1:
				mixercfg |= (i + 1) << 0;
				mixercfg_ext |= ((i > SDE_STAGE_5) ? 1:0) << 0;
				break;
			case SSPP_CURSOR0:
				mixercfg_ext |= (i + 1) << 20;
				break;
			case SSPP_CURSOR1:
				mixercfg_ext |= (i + 1) << 26;
				break;
			default:
				break;
			}
		}
	}

	SDE_REG_WRITE(c, CTL_LAYER(lm), mixercfg);
	SDE_REG_WRITE(c, CTL_LAYER_EXT(lm), mixercfg_ext);
}

static void sde_hw_ctl_intf_cfg(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 intf_cfg = 0;

	intf_cfg |= (cfg->intf & 0xF) << 4;

	if (cfg->wb)
		intf_cfg |= (cfg->wb & 0x3) + 2;

	if (cfg->mode_3d) {
		intf_cfg |= BIT(19);
		intf_cfg |= (cfg->mode_3d - 1) << 20;
	}

	SDE_REG_WRITE(c, CTL_TOP, intf_cfg);
}

static void _setup_ctl_ops(struct sde_hw_ctl_ops *ops,
		unsigned long cap)
{
	ops->setup_flush = sde_hw_ctl_setup_flush;
	ops->setup_start = sde_hw_ctl_force_start;
	ops->setup_intf_cfg = sde_hw_ctl_intf_cfg;
	ops->reset = sde_hw_ctl_reset_control;
	ops->setup_blendstage = sde_hw_ctl_setup_blendstage;
	ops->get_bitmask_sspp = sde_hw_ctl_get_bitmask_sspp;
	ops->get_bitmask_mixer = sde_hw_ctl_get_bitmask_mixer;
	ops->get_bitmask_dspp = sde_hw_ctl_get_bitmask_dspp;
	ops->get_bitmask_intf = sde_hw_ctl_get_bitmask_intf;
	ops->get_bitmask_cdm = sde_hw_ctl_get_bitmask_cdm;
};

struct sde_hw_ctl *sde_hw_ctl_init(enum sde_ctl idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_ctl *c;
	struct sde_ctl_cfg *cfg;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _ctl_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		pr_err("Error Panic\n");
		return ERR_PTR(-EINVAL);
	}

	c->caps = cfg;
	_setup_ctl_ops(&c->ops, c->caps->features);
	c->idx = idx;
	c->mixer_count = m->mixer_count;
	c->mixer_hw_caps = m->mixer;

	return c;
}

void sde_hw_ctl_destroy(struct sde_hw_ctl *ctx)
{
	kfree(ctx);
}
