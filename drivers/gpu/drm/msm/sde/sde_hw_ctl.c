/* Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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
#include "sde_hw_ctl.h"
#include "sde_dbg.h"
#include "sde_kms.h"
#include "sde_reg_dma.h"

#define   CTL_LAYER(lm)                 \
	(((lm) == LM_5) ? (0x024) : (((lm) - LM_0) * 0x004))
#define   CTL_LAYER_EXT(lm)             \
	(0x40 + (((lm) - LM_0) * 0x004))
#define   CTL_LAYER_EXT2(lm)             \
	(0x70 + (((lm) - LM_0) * 0x004))
#define   CTL_LAYER_EXT3(lm)             \
	(0xA0 + (((lm) - LM_0) * 0x004))
#define   CTL_TOP                       0x014
#define   CTL_FLUSH                     0x018
#define   CTL_START                     0x01C
#define   CTL_PREPARE                   0x0d0
#define   CTL_SW_RESET                  0x030
#define   CTL_SW_RESET_OVERRIDE         0x060
#define   CTL_LAYER_EXTN_OFFSET         0x40
#define   CTL_ROT_TOP                   0x0C0
#define   CTL_ROT_FLUSH                 0x0C4
#define   CTL_ROT_START                 0x0CC

#define   CTL_MERGE_3D_ACTIVE           0x0E4
#define   CTL_DSC_ACTIVE                0x0E8
#define   CTL_WB_ACTIVE                 0x0EC
#define   CTL_CWB_ACTIVE                0x0F0
#define   CTL_INTF_ACTIVE               0x0F4
#define   CTL_CDM_ACTIVE                0x0F8

#define   CTL_MERGE_3D_FLUSH           0x100
#define   CTL_DSC_FLUSH                0x104
#define   CTL_WB_FLUSH                 0x108
#define   CTL_CWB_FLUSH                0x10C
#define   CTL_INTF_FLUSH               0x110
#define   CTL_CDM_FLUSH                0x114
#define   CTL_PERIPH_FLUSH             0x128

#define  CTL_INTF_MASTER               0x134

#define CTL_MIXER_BORDER_OUT            BIT(24)
#define CTL_FLUSH_MASK_ROT              BIT(27)
#define CTL_FLUSH_MASK_CTL              BIT(17)

#define CTL_NUM_EXT			4
#define CTL_SSPP_MAX_RECTS		2

#define SDE_REG_RESET_TIMEOUT_US        2000
#define SDE_REG_WAIT_RESET_TIMEOUT_US        100000

#define UPDATE_MASK(m, idx, en)           \
	((m) = (en) ? ((m) | BIT((idx))) : ((m) & ~BIT((idx))))

/**
 * List of SSPP bits in CTL_FLUSH
 */
static const u32 sspp_tbl[SSPP_MAX] = { SDE_NONE, 0, 1, 2, 18, 3, 4, 5,
	19, 11, 12, 24, 25, SDE_NONE, SDE_NONE};

/**
 * List of layer mixer bits in CTL_FLUSH
 */
static const u32 mixer_tbl[LM_MAX] = {SDE_NONE, 6, 7, 8, 9, 10, 20,
	SDE_NONE};

/**
 * List of DSPP bits in CTL_FLUSH
 */
static const u32 dspp_tbl[DSPP_MAX] = {SDE_NONE, 13, 14, 15, 21};

/**
 * List of DSPP PA LUT bits in CTL_FLUSH
 */
static const u32 dspp_pav_tbl[DSPP_MAX] = {SDE_NONE, 3, 4, 5, 19};

/**
 * List of CDM LUT bits in CTL_FLUSH
 */
static const u32 cdm_tbl[CDM_MAX] = {SDE_NONE, 26};

/**
 * List of WB bits in CTL_FLUSH
 */
static const u32 wb_tbl[WB_MAX] = {SDE_NONE, SDE_NONE, SDE_NONE, 16};

/**
 * List of ROT bits in CTL_FLUSH
 */
static const u32 rot_tbl[ROT_MAX] = {SDE_NONE, 27};

/**
 * List of INTF bits in CTL_FLUSH
 */
static const u32 intf_tbl[INTF_MAX] = {SDE_NONE, 31, 30, 29, 28};

/**
 * Below definitions are for CTL supporting SDE_CTL_ACTIVE_CFG,
 * certain blocks have the individual flush control as well,
 * for such blocks flush is done by flushing individual control and
 * top level control.
 */

/**
 * list of WB bits in CTL_WB_FLUSH
 */
static const u32 wb_flush_tbl[WB_MAX] = {SDE_NONE, SDE_NONE, SDE_NONE, 2};

/**
 * list of INTF bits in CTL_INTF_FLUSH
 */
static const u32 intf_flush_tbl[INTF_MAX] = {SDE_NONE, 0, 1, 2, 3, 4, 5};

/**
 * list of DSC bits in CTL_DSC_FLUSH
 */
static const u32 dsc_flush_tbl[DSC_MAX] = {SDE_NONE, 0, 1, 2, 3, 4, 5};

/**
 * list of MERGE_3D bits in CTL_MERGE_3D_FLUSH
 */
static const u32 merge_3d_tbl[MERGE_3D_MAX] = {SDE_NONE, 0, 1, 2};

/**
 * list of CDM bits in CTL_CDM_FLUSH
 */
static const u32 cdm_flush_tbl[CDM_MAX] = {SDE_NONE, 0};

/**
 * list of CWB bits in CTL_CWB_FLUSH
 */
static const u32 cwb_flush_tbl[CWB_MAX] = {SDE_NONE, SDE_NONE, 1, 2, 3,
	4, 5};

/**
 * struct ctl_sspp_stage_reg_map: Describes bit layout for a sspp stage cfg
 * @ext: Index to indicate LAYER_x_EXT id for given sspp
 * @start: Start position of blend stage bits for given sspp
 * @bits: Number of bits from @start assigned for given sspp
 * @sec_bit_mask: Bitmask to add to LAYER_x_EXT1 for missing bit of sspp
 */
struct ctl_sspp_stage_reg_map {
	u32 ext;
	u32 start;
	u32 bits;
	u32 sec_bit_mask;
};

/* list of ctl_sspp_stage_reg_map for all the sppp */
static const struct ctl_sspp_stage_reg_map
sspp_reg_cfg_tbl[SSPP_MAX][CTL_SSPP_MAX_RECTS] = {
	/* SSPP_NONE */{ {0, 0, 0, 0}, {0, 0, 0, 0} },
	/* SSPP_VIG0 */{ {0, 0, 3, BIT(0)}, {3, 0, 4, 0} },
	/* SSPP_VIG1 */{ {0, 3, 3, BIT(2)}, {3, 4, 4, 0} },
	/* SSPP_VIG2 */{ {0, 6, 3, BIT(4)}, {3, 8, 4, 0} },
	/* SSPP_VIG3 */{ {0, 26, 3, BIT(6)}, {3, 12, 4, 0} },
	/* SSPP_RGB0 */{ {0, 9, 3, BIT(8)}, {0, 0, 0, 0} },
	/* SSPP_RGB1 */{ {0, 12, 3, BIT(10)}, {0, 0, 0, 0} },
	/* SSPP_RGB2 */{ {0, 15, 3, BIT(12)}, {0, 0, 0, 0} },
	/* SSPP_RGB3 */{ {0, 29, 3, BIT(14)}, {0, 0, 0, 0} },
	/* SSPP_DMA0 */{ {0, 18, 3, BIT(16)}, {2, 8, 4, 0} },
	/* SSPP_DMA1 */{ {0, 21, 3, BIT(18)}, {2, 12, 4, 0} },
	/* SSPP_DMA2 */{ {2, 0, 4, 0}, {2, 16, 4, 0} },
	/* SSPP_DMA3 */{ {2, 4, 4, 0}, {2, 20, 4, 0} },
	/* SSPP_CURSOR0 */{ {1, 20, 4, 0}, {0, 0, 0, 0} },
	/* SSPP_CURSOR1 */{ {0, 26, 4, 0}, {0, 0, 0, 0} }
};

/**
 * Individual flush bit in CTL_FLUSH
 */
#define  WB_IDX         16
#define  DSC_IDX        22
#define  MERGE_3D_IDX   23
#define  CDM_IDX        26
#define  CWB_IDX        28
#define  PERIPH_IDX     30
#define  INTF_IDX       31

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
			b->length = m->ctl[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_CTL;
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

static inline int sde_hw_ctl_trigger_start(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	SDE_REG_WRITE(&ctx->hw, CTL_START, 0x1);
	return 0;
}

static inline int sde_hw_ctl_get_start_state(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	return SDE_REG_READ(&ctx->hw, CTL_START);
}

static inline int sde_hw_ctl_trigger_pending(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	SDE_REG_WRITE(&ctx->hw, CTL_PREPARE, 0x1);
	return 0;
}

static inline int sde_hw_ctl_trigger_rot_start(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	/* ROT flush bit is latched during ROT start, so set it first */
	if (CTL_FLUSH_MASK_ROT & ctx->flush.pending_flush_mask) {
		ctx->flush.pending_flush_mask &= ~CTL_FLUSH_MASK_ROT;
		SDE_REG_WRITE(&ctx->hw, CTL_FLUSH, CTL_FLUSH_MASK_ROT);
	}
	SDE_REG_WRITE(&ctx->hw, CTL_ROT_START, BIT(0));
	return 0;
}

static inline int sde_hw_ctl_clear_pending_flush(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	memset(&ctx->flush, 0, sizeof(ctx->flush));
	return 0;
}

static inline int sde_hw_ctl_update_pending_flush(struct sde_hw_ctl *ctx,
	struct sde_ctl_flush_cfg *cfg)
{
	if (!ctx || !cfg)
		return -EINVAL;

	ctx->flush.pending_flush_mask |= cfg->pending_flush_mask;
	return 0;
}

static int sde_hw_ctl_get_pending_flush(struct sde_hw_ctl *ctx,
		struct sde_ctl_flush_cfg *cfg)
{
	if (!ctx || !cfg)
		return -EINVAL;

	memcpy(cfg, &ctx->flush, sizeof(*cfg));
	return 0;
}

static inline int sde_hw_ctl_trigger_flush(struct sde_hw_ctl *ctx)
{

	if (!ctx)
		return -EINVAL;

	SDE_REG_WRITE(&ctx->hw, CTL_FLUSH, ctx->flush.pending_flush_mask);
	return 0;
}

static inline u32 sde_hw_ctl_get_flush_register(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	u32 rot_op_mode;

	if (!ctx)
		return 0;

	c = &ctx->hw;
	rot_op_mode = SDE_REG_READ(c, CTL_ROT_TOP) & 0x3;

	/* rotate flush bit is undefined if offline mode, so ignore it */
	if (rot_op_mode == SDE_CTL_ROT_OP_MODE_OFFLINE)
		return SDE_REG_READ(c, CTL_FLUSH) & ~CTL_FLUSH_MASK_ROT;

	return SDE_REG_READ(c, CTL_FLUSH);
}

static inline int sde_hw_ctl_update_bitmask_sspp(struct sde_hw_ctl *ctx,
		enum sde_sspp sspp,
		bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(sspp > SSPP_NONE) || !(sspp < SSPP_MAX)) {
		SDE_ERROR("Unsupported pipe %d\n", sspp);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, sspp_tbl[sspp], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_mixer(struct sde_hw_ctl *ctx,
		enum sde_lm lm,
		bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(lm > SDE_NONE) || !(lm < LM_MAX)) {
		SDE_ERROR("Unsupported mixer %d\n", lm);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, mixer_tbl[lm], enable);
	ctx->flush.pending_flush_mask |= CTL_FLUSH_MASK_CTL;

	return 0;
}

static inline int sde_hw_ctl_update_bitmask_dspp(struct sde_hw_ctl *ctx,
		enum sde_dspp dspp,
		bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(dspp > SDE_NONE) || !(dspp < DSPP_MAX)) {
		SDE_ERROR("Unsupported dspp %d\n", dspp);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, dspp_tbl[dspp], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_dspp_pavlut(struct sde_hw_ctl *ctx,
		enum sde_dspp dspp, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(dspp > SDE_NONE) || !(dspp < DSPP_MAX)) {
		SDE_ERROR("Unsupported dspp %d\n", dspp);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, dspp_pav_tbl[dspp], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_cdm(struct sde_hw_ctl *ctx,
		enum sde_cdm cdm,
		bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(cdm > SDE_NONE) || !(cdm < CDM_MAX) || (cdm == CDM_1)) {
		SDE_ERROR("Unsupported cdm %d\n", cdm);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, cdm_tbl[cdm], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_wb(struct sde_hw_ctl *ctx,
		enum sde_wb wb, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(wb > SDE_NONE) || !(wb < WB_MAX) ||
			(wb == WB_0) || (wb == WB_1)) {
		SDE_ERROR("Unsupported wb %d\n", wb);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, wb_tbl[wb], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_rot(struct sde_hw_ctl *ctx,
		enum sde_rot rot, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(rot > SDE_NONE) || !(rot < ROT_MAX)) {
		SDE_ERROR("Unsupported rot %d\n", rot);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, rot_tbl[rot], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_intf(struct sde_hw_ctl *ctx,
		enum sde_intf intf, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(intf > SDE_NONE) || !(intf < INTF_MAX) || (intf > INTF_4)) {
		SDE_ERROR("Unsupported intf %d\n", intf);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_flush_mask, intf_tbl[intf], enable);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_wb_v1(struct sde_hw_ctl *ctx,
		enum sde_wb wb, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (wb != WB_2) {
		SDE_ERROR("Unsupported wb %d\n", wb);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_wb_flush_mask, wb_flush_tbl[wb], enable);
	if (ctx->flush.pending_wb_flush_mask)
		UPDATE_MASK(ctx->flush.pending_flush_mask, WB_IDX, 1);
	else
		UPDATE_MASK(ctx->flush.pending_flush_mask, WB_IDX, 0);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_intf_v1(struct sde_hw_ctl *ctx,
		enum sde_intf intf, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(intf > SDE_NONE) || !(intf < INTF_MAX)) {
		SDE_ERROR("Unsupported intf %d\n", intf);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_intf_flush_mask, intf_flush_tbl[intf],
			enable);
	if (ctx->flush.pending_intf_flush_mask)
		UPDATE_MASK(ctx->flush.pending_flush_mask, INTF_IDX, 1);
	else
		UPDATE_MASK(ctx->flush.pending_flush_mask, INTF_IDX, 0);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_periph_v1(struct sde_hw_ctl *ctx,
		enum sde_intf intf, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(intf > SDE_NONE) || !(intf < INTF_MAX)) {
		SDE_ERROR("Unsupported intf %d\n", intf);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_periph_flush_mask, intf_flush_tbl[intf],
			enable);
	if (ctx->flush.pending_periph_flush_mask)
		UPDATE_MASK(ctx->flush.pending_flush_mask, PERIPH_IDX, 1);
	else
		UPDATE_MASK(ctx->flush.pending_flush_mask, PERIPH_IDX, 0);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_dsc_v1(struct sde_hw_ctl *ctx,
		enum sde_dsc dsc, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(dsc > SDE_NONE) || !(dsc < DSC_MAX)) {
		SDE_ERROR("Unsupported dsc %d\n", dsc);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_dsc_flush_mask, dsc_flush_tbl[dsc],
			enable);
	if (ctx->flush.pending_dsc_flush_mask)
		UPDATE_MASK(ctx->flush.pending_flush_mask, DSC_IDX, 1);
	else
		UPDATE_MASK(ctx->flush.pending_flush_mask, DSC_IDX, 0);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_merge3d_v1(struct sde_hw_ctl *ctx,
		enum sde_merge_3d merge_3d, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (!(merge_3d > SDE_NONE) || !(merge_3d < MERGE_3D_MAX)) {
		SDE_ERROR("Unsupported merge_3d %d\n", merge_3d);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_merge_3d_flush_mask,
			merge_3d_tbl[merge_3d], enable);
	if (ctx->flush.pending_merge_3d_flush_mask)
		UPDATE_MASK(ctx->flush.pending_flush_mask, MERGE_3D_IDX, 1);
	else
		UPDATE_MASK(ctx->flush.pending_flush_mask, MERGE_3D_IDX, 0);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_cdm_v1(struct sde_hw_ctl *ctx,
		enum sde_cdm cdm, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if (cdm != CDM_0) {
		SDE_ERROR("Unsupported cdm %d\n", cdm);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_cdm_flush_mask, cdm_flush_tbl[cdm],
			enable);
	if (ctx->flush.pending_cdm_flush_mask)
		UPDATE_MASK(ctx->flush.pending_flush_mask, CDM_IDX, 1);
	else
		UPDATE_MASK(ctx->flush.pending_flush_mask, CDM_IDX, 0);
	return 0;
}

static inline int sde_hw_ctl_update_bitmask_cwb_v1(struct sde_hw_ctl *ctx,
		enum sde_cwb cwb, bool enable)
{
	if (!ctx)
		return -EINVAL;

	if ((cwb < CWB_1) || (cwb >= CWB_MAX)) {
		SDE_ERROR("Unsupported cwb %d\n", cwb);
		return -EINVAL;
	}

	UPDATE_MASK(ctx->flush.pending_cwb_flush_mask, cwb_flush_tbl[cwb],
			enable);
	if (ctx->flush.pending_cwb_flush_mask)
		UPDATE_MASK(ctx->flush.pending_flush_mask, CWB_IDX, 1);
	else
		UPDATE_MASK(ctx->flush.pending_flush_mask, CWB_IDX, 0);
	return 0;
}

static inline int sde_hw_ctl_update_pending_flush_v1(
		struct sde_hw_ctl *ctx,
		struct sde_ctl_flush_cfg *cfg)
{
	if (!ctx || !cfg)
		return -EINVAL;

	ctx->flush.pending_flush_mask |= cfg->pending_flush_mask;
	ctx->flush.pending_intf_flush_mask |= cfg->pending_intf_flush_mask;
	ctx->flush.pending_cdm_flush_mask |= cfg->pending_cdm_flush_mask;
	ctx->flush.pending_wb_flush_mask |= cfg->pending_wb_flush_mask;
	ctx->flush.pending_dsc_flush_mask |= cfg->pending_dsc_flush_mask;
	ctx->flush.pending_merge_3d_flush_mask |=
		cfg->pending_merge_3d_flush_mask;
	ctx->flush.pending_cwb_flush_mask |= cfg->pending_cwb_flush_mask;
	ctx->flush.pending_periph_flush_mask |= cfg->pending_periph_flush_mask;
	return 0;
}

static inline int sde_hw_ctl_trigger_flush_v1(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return -EINVAL;

	if (ctx->flush.pending_flush_mask & BIT(WB_IDX))
		SDE_REG_WRITE(&ctx->hw, CTL_WB_FLUSH,
				ctx->flush.pending_wb_flush_mask);
	if (ctx->flush.pending_flush_mask & BIT(DSC_IDX))
		SDE_REG_WRITE(&ctx->hw, CTL_DSC_FLUSH,
				ctx->flush.pending_dsc_flush_mask);
	if (ctx->flush.pending_flush_mask & BIT(MERGE_3D_IDX))
		SDE_REG_WRITE(&ctx->hw, CTL_MERGE_3D_FLUSH,
				ctx->flush.pending_merge_3d_flush_mask);
	if (ctx->flush.pending_flush_mask & BIT(CDM_IDX))
		SDE_REG_WRITE(&ctx->hw, CTL_CDM_FLUSH,
				ctx->flush.pending_cdm_flush_mask);
	if (ctx->flush.pending_flush_mask & BIT(CWB_IDX))
		SDE_REG_WRITE(&ctx->hw, CTL_CWB_FLUSH,
				ctx->flush.pending_cwb_flush_mask);
	if (ctx->flush.pending_flush_mask & BIT(INTF_IDX))
		SDE_REG_WRITE(&ctx->hw, CTL_INTF_FLUSH,
				ctx->flush.pending_intf_flush_mask);
	if (ctx->flush.pending_flush_mask & BIT(PERIPH_IDX))
		SDE_REG_WRITE(&ctx->hw, CTL_PERIPH_FLUSH,
				ctx->flush.pending_periph_flush_mask);

	SDE_REG_WRITE(&ctx->hw, CTL_FLUSH, ctx->flush.pending_flush_mask);
	return 0;
}

static inline u32 sde_hw_ctl_get_intf_v1(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	u32 intf_active;

	if (!ctx) {
		pr_err("Invalid input argument\n");
		return 0;
	}

	c = &ctx->hw;
	intf_active = SDE_REG_READ(c, CTL_INTF_ACTIVE);

	return intf_active;
}

static inline u32 sde_hw_ctl_get_intf(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	u32 ctl_top;
	u32 intf_active = 0;

	if (!ctx) {
		pr_err("Invalid input argument\n");
		return 0;
	}

	c = &ctx->hw;
	ctl_top = SDE_REG_READ(c, CTL_TOP);

	intf_active = (ctl_top > 0) ?
		BIT(ctl_top - 1) : 0;

	return intf_active;
}

static u32 sde_hw_ctl_poll_reset_status(struct sde_hw_ctl *ctx, u32 timeout_us)
{
	struct sde_hw_blk_reg_map *c;
	ktime_t timeout;
	u32 status;

	if (!ctx)
		return 0;

	c = &ctx->hw;
	timeout = ktime_add_us(ktime_get(), timeout_us);

	/*
	 * it takes around 30us to have mdp finish resetting its ctl path
	 * poll every 50us so that reset should be completed at 1st poll
	 */
	do {
		status = SDE_REG_READ(c, CTL_SW_RESET);
		status &= 0x1;
		if (status)
			usleep_range(20, 50);
	} while (status && ktime_compare_safe(ktime_get(), timeout) < 0);

	return status;
}

static u32 sde_hw_ctl_get_reset_status(struct sde_hw_ctl *ctx)
{
	if (!ctx)
		return 0;
	return (u32)SDE_REG_READ(&ctx->hw, CTL_SW_RESET);
}

static int sde_hw_ctl_reset_control(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;

	if (!ctx)
		return 0;

	c = &ctx->hw;
	pr_debug("issuing hw ctl reset for ctl:%d\n", ctx->idx);
	SDE_REG_WRITE(c, CTL_SW_RESET, 0x1);
	if (sde_hw_ctl_poll_reset_status(ctx, SDE_REG_RESET_TIMEOUT_US))
		return -EINVAL;

	return 0;
}

static void sde_hw_ctl_hard_reset(struct sde_hw_ctl *ctx, bool enable)
{
	struct sde_hw_blk_reg_map *c;

	if (!ctx)
		return;

	c = &ctx->hw;
	pr_debug("hw ctl hard reset for ctl:%d, %d\n",
			ctx->idx - CTL_0, enable);
	SDE_REG_WRITE(c, CTL_SW_RESET_OVERRIDE, enable);
}

static int sde_hw_ctl_wait_reset_status(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	u32 status;

	if (!ctx)
		return 0;

	c = &ctx->hw;
	status = SDE_REG_READ(c, CTL_SW_RESET);
	status &= 0x01;
	if (!status)
		return 0;

	pr_debug("hw ctl reset is set for ctl:%d\n", ctx->idx);
	if (sde_hw_ctl_poll_reset_status(ctx, SDE_REG_WAIT_RESET_TIMEOUT_US)) {
		pr_err("hw recovery is not complete for ctl:%d\n", ctx->idx);
		return -EINVAL;
	}

	return 0;
}

static void sde_hw_ctl_clear_all_blendstages(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	int i;

	if (!ctx)
		return;

	c = &ctx->hw;
	for (i = 0; i < ctx->mixer_count; i++) {
		int mixer_id = ctx->mixer_hw_caps[i].id;

		SDE_REG_WRITE(c, CTL_LAYER(mixer_id), 0);
		SDE_REG_WRITE(c, CTL_LAYER_EXT(mixer_id), 0);
		SDE_REG_WRITE(c, CTL_LAYER_EXT2(mixer_id), 0);
		SDE_REG_WRITE(c, CTL_LAYER_EXT3(mixer_id), 0);
	}
}

static void sde_hw_ctl_setup_blendstage(struct sde_hw_ctl *ctx,
	enum sde_lm lm, struct sde_hw_stage_cfg *stage_cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 mixercfg = 0, mixercfg_ext = 0, mix, ext;
	u32 mixercfg_ext2 = 0, mixercfg_ext3 = 0;
	int i, j;
	u8 stages;
	int pipes_per_stage;

	if (!ctx)
		return;

	c = &ctx->hw;
	stages = _mixer_stages(ctx->mixer_hw_caps, ctx->mixer_count, lm);
	if (stages < 0)
		return;

	if (test_bit(SDE_MIXER_SOURCESPLIT,
		&ctx->mixer_hw_caps->features))
		pipes_per_stage = PIPES_PER_STAGE;
	else
		pipes_per_stage = 1;

	if (!stage_cfg)
		goto exit;

	for (i = 0; i <= stages; i++) {
		/* overflow to ext register if 'i + 1 > 7' */
		mix = (i + 1) & 0x7;
		ext = i >= 7;

		for (j = 0 ; j < pipes_per_stage; j++) {
			enum sde_sspp_multirect_index rect_index =
				stage_cfg->multirect_index[i][j];

			switch (stage_cfg->stage[i][j]) {
			case SSPP_VIG0:
				if (rect_index == SDE_SSPP_RECT_1) {
					mixercfg_ext3 |= ((i + 1) & 0xF) << 0;
				} else {
					mixercfg |= mix << 0;
					mixercfg_ext |= ext << 0;
				}
				break;
			case SSPP_VIG1:
				if (rect_index == SDE_SSPP_RECT_1) {
					mixercfg_ext3 |= ((i + 1) & 0xF) << 4;
				} else {
					mixercfg |= mix << 3;
					mixercfg_ext |= ext << 2;
				}
				break;
			case SSPP_VIG2:
				if (rect_index == SDE_SSPP_RECT_1) {
					mixercfg_ext3 |= ((i + 1) & 0xF) << 8;
				} else {
					mixercfg |= mix << 6;
					mixercfg_ext |= ext << 4;
				}
				break;
			case SSPP_VIG3:
				if (rect_index == SDE_SSPP_RECT_1) {
					mixercfg_ext3 |= ((i + 1) & 0xF) << 12;
				} else {
					mixercfg |= mix << 26;
					mixercfg_ext |= ext << 6;
				}
				break;
			case SSPP_RGB0:
				mixercfg |= mix << 9;
				mixercfg_ext |= ext << 8;
				break;
			case SSPP_RGB1:
				mixercfg |= mix << 12;
				mixercfg_ext |= ext << 10;
				break;
			case SSPP_RGB2:
				mixercfg |= mix << 15;
				mixercfg_ext |= ext << 12;
				break;
			case SSPP_RGB3:
				mixercfg |= mix << 29;
				mixercfg_ext |= ext << 14;
				break;
			case SSPP_DMA0:
				if (rect_index == SDE_SSPP_RECT_1) {
					mixercfg_ext2 |= ((i + 1) & 0xF) << 8;
				} else {
					mixercfg |= mix << 18;
					mixercfg_ext |= ext << 16;
				}
				break;
			case SSPP_DMA1:
				if (rect_index == SDE_SSPP_RECT_1) {
					mixercfg_ext2 |= ((i + 1) & 0xF) << 12;
				} else {
					mixercfg |= mix << 21;
					mixercfg_ext |= ext << 18;
				}
				break;
			case SSPP_DMA2:
				if (rect_index == SDE_SSPP_RECT_1) {
					mixercfg_ext2 |= ((i + 1) & 0xF) << 16;
				} else {
					mix |= (i + 1) & 0xF;
					mixercfg_ext2 |= mix << 0;
				}
				break;
			case SSPP_DMA3:
				if (rect_index == SDE_SSPP_RECT_1) {
					mixercfg_ext2 |= ((i + 1) & 0xF) << 20;
				} else {
					mix |= (i + 1) & 0xF;
					mixercfg_ext2 |= mix << 4;
				}
				break;
			case SSPP_CURSOR0:
				mixercfg_ext |= ((i + 1) & 0xF) << 20;
				break;
			case SSPP_CURSOR1:
				mixercfg_ext |= ((i + 1) & 0xF) << 26;
				break;
			default:
				break;
			}
		}
	}

exit:
	if ((!mixercfg && !mixercfg_ext && !mixercfg_ext2 && !mixercfg_ext3) ||
			(stage_cfg && !stage_cfg->stage[0][0]))
		mixercfg |= CTL_MIXER_BORDER_OUT;

	SDE_REG_WRITE(c, CTL_LAYER(lm), mixercfg);
	SDE_REG_WRITE(c, CTL_LAYER_EXT(lm), mixercfg_ext);
	SDE_REG_WRITE(c, CTL_LAYER_EXT2(lm), mixercfg_ext2);
	SDE_REG_WRITE(c, CTL_LAYER_EXT3(lm), mixercfg_ext3);
}

static u32 sde_hw_ctl_get_staged_sspp(struct sde_hw_ctl *ctx, enum sde_lm lm,
		struct sde_sspp_index_info *info, u32 info_max_cnt)
{
	int i, j;
	u32 count = 0;
	u32 mask = 0;
	bool staged;
	u32 mixercfg[CTL_NUM_EXT];
	struct sde_hw_blk_reg_map *c;
	const struct ctl_sspp_stage_reg_map *sspp_cfg;

	if (!ctx || (lm >= LM_MAX) || !info)
		return count;

	c = &ctx->hw;
	mixercfg[0] = SDE_REG_READ(c, CTL_LAYER(lm));
	mixercfg[1] = SDE_REG_READ(c, CTL_LAYER_EXT(lm));
	mixercfg[2] = SDE_REG_READ(c, CTL_LAYER_EXT2(lm));
	mixercfg[3] = SDE_REG_READ(c, CTL_LAYER_EXT3(lm));

	for (i = SSPP_VIG0; i < SSPP_MAX; i++) {
		for (j = 0; j < CTL_SSPP_MAX_RECTS; j++) {
			if (count >= info_max_cnt)
				goto end;

			sspp_cfg = &sspp_reg_cfg_tbl[i][j];
			if (!sspp_cfg->bits || sspp_cfg->ext >= CTL_NUM_EXT)
				continue;

			mask = ((0x1 << sspp_cfg->bits) - 1) << sspp_cfg->start;
			staged = mixercfg[sspp_cfg->ext] & mask;
			if (!staged)
				staged = mixercfg[1] & sspp_cfg->sec_bit_mask;

			if (staged) {
				info[count].sspp = i;
				info[count].is_virtual = j;
				count++;
			}
		}
	}

end:
	return count;
}

static int sde_hw_ctl_intf_cfg_v1(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg_v1 *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 intf_active = 0;
	u32 wb_active = 0;
	u32 merge_3d_active = 0;
	u32 cwb_active = 0;
	u32 mode_sel = 0;
	u32 cdm_active = 0;
	u32 intf_master = 0;
	u32 i;

	if (!ctx)
		return -EINVAL;

	c = &ctx->hw;
	for (i = 0; i < cfg->intf_count; i++) {
		if (cfg->intf[i])
			intf_active |= BIT(cfg->intf[i] - INTF_0);
	}

	if (cfg->intf_count > 1)
		intf_master = BIT(cfg->intf_master - INTF_0);

	for (i = 0; i < cfg->wb_count; i++) {
		if (cfg->wb[i])
			wb_active |= BIT(cfg->wb[i] - WB_0);
	}

	for (i = 0; i < cfg->merge_3d_count; i++) {
		if (cfg->merge_3d[i])
			merge_3d_active |= BIT(cfg->merge_3d[i] - MERGE_3D_0);
	}

	for (i = 0; i < cfg->cwb_count; i++) {
		if (cfg->cwb[i])
			cwb_active |= BIT(cfg->cwb[i] - CWB_0);
	}

	for (i = 0; i < cfg->cdm_count; i++) {
		if (cfg->cdm[i])
			cdm_active |= BIT(cfg->cdm[i] - CDM_0);
	}

	if (cfg->intf_mode_sel == SDE_CTL_MODE_SEL_CMD)
		mode_sel |= BIT(17);

	SDE_REG_WRITE(c, CTL_TOP, mode_sel);
	SDE_REG_WRITE(c, CTL_WB_ACTIVE, wb_active);
	SDE_REG_WRITE(c, CTL_CWB_ACTIVE, cwb_active);
	SDE_REG_WRITE(c, CTL_INTF_ACTIVE, intf_active);
	SDE_REG_WRITE(c, CTL_CDM_ACTIVE, cdm_active);
	SDE_REG_WRITE(c, CTL_MERGE_3D_ACTIVE, merge_3d_active);
	SDE_REG_WRITE(c, CTL_INTF_MASTER, intf_master);
	return 0;
}

static int sde_hw_ctl_reset_post_disable(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg_v1 *cfg, u32 merge_3d_idx)
{
	struct sde_hw_blk_reg_map *c;
	u32 intf_active = 0, wb_active = 0, merge_3d_active = 0;
	u32 intf_flush = 0, wb_flush = 0;
	u32 i;

	if (!ctx || !cfg) {
		SDE_ERROR("invalid hw_ctl or hw_intf blk\n");
		return -EINVAL;
	}

	c = &ctx->hw;
	for (i = 0; i < cfg->intf_count; i++) {
		if (cfg->intf[i]) {
			intf_active &= ~BIT(cfg->intf[i] - INTF_0);
			intf_flush |= BIT(cfg->intf[i] - INTF_0);
		}
	}

	for (i = 0; i < cfg->wb_count; i++) {
		if (cfg->wb[i]) {
			wb_active &= ~BIT(cfg->wb[i] - WB_0);
			wb_flush |= BIT(cfg->wb[i] - WB_0);
		}
	}

	if (merge_3d_idx) {
		/* disable and flush merge3d_blk */
		ctx->flush.pending_merge_3d_flush_mask =
			BIT(merge_3d_idx - MERGE_3D_0);
		merge_3d_active &= ~BIT(merge_3d_idx - MERGE_3D_0);
		SDE_REG_WRITE(c, CTL_MERGE_3D_ACTIVE, merge_3d_active);
	}
	sde_hw_ctl_clear_all_blendstages(ctx);

	if (cfg->intf_count) {
		ctx->flush.pending_intf_flush_mask = intf_flush;
		UPDATE_MASK(ctx->flush.pending_flush_mask, INTF_IDX, 1);
		SDE_REG_WRITE(c, CTL_INTF_ACTIVE, intf_active);
	}

	if (cfg->wb_count) {
		ctx->flush.pending_wb_flush_mask = wb_flush;
		UPDATE_MASK(ctx->flush.pending_flush_mask, WB_IDX, 1);
		SDE_REG_WRITE(c, CTL_WB_ACTIVE, wb_active);
	}

	return 0;
}

static int sde_hw_ctl_update_cwb_cfg(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg_v1 *cfg, bool enable)
{
	int i;
	u32 cwb_active = 0;
	u32 merge_3d_active = 0;
	u32 wb_active = 0;
	struct sde_hw_blk_reg_map *c;

	if (!ctx)
		return -EINVAL;

	c = &ctx->hw;
	cwb_active = SDE_REG_READ(c, CTL_CWB_ACTIVE);
	for (i = 0; i < cfg->cwb_count; i++) {
		if (cfg->cwb[i])
			cwb_active |= BIT(cfg->cwb[i] - CWB_0);
	}

	merge_3d_active = SDE_REG_READ(c, CTL_MERGE_3D_ACTIVE);
	for (i = 0; i < cfg->merge_3d_count; i++) {
		if (cfg->merge_3d[i])
			merge_3d_active |= BIT(cfg->merge_3d[i] - MERGE_3D_0);
	}

	if (enable) {
		wb_active = BIT(2);
		SDE_REG_WRITE(c, CTL_WB_ACTIVE, wb_active);
		SDE_REG_WRITE(c, CTL_MERGE_3D_ACTIVE, merge_3d_active);
		SDE_REG_WRITE(c, CTL_CWB_ACTIVE, cwb_active);
	} else {
		SDE_REG_WRITE(c, CTL_WB_ACTIVE, 0x0);
		SDE_REG_WRITE(c, CTL_MERGE_3D_ACTIVE, 0x0);
		SDE_REG_WRITE(c, CTL_CWB_ACTIVE, 0x0);
	}

	return 0;
}

static int sde_hw_ctl_dsc_cfg(struct sde_hw_ctl *ctx,
		struct sde_ctl_dsc_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 dsc_active = 0;
	int i;

	if (!ctx)
		return -EINVAL;

	c = &ctx->hw;
	for (i = 0; i < cfg->dsc_count; i++)
		if (cfg->dsc[i])
			dsc_active |= BIT(cfg->dsc[i] - DSC_0);

	SDE_REG_WRITE(c, CTL_DSC_ACTIVE, dsc_active);
	return 0;
}

static int sde_hw_ctl_intf_cfg(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 intf_cfg = 0;

	if (!ctx)
		return -EINVAL;

	c = &ctx->hw;
	intf_cfg |= (cfg->intf & 0xF) << 4;

	if (cfg->wb)
		intf_cfg |= (cfg->wb & 0x3) + 2;

	if (cfg->mode_3d) {
		intf_cfg |= BIT(19);
		intf_cfg |= (cfg->mode_3d - 0x1) << 20;
	}

	switch (cfg->intf_mode_sel) {
	case SDE_CTL_MODE_SEL_VID:
		intf_cfg &= ~BIT(17);
		intf_cfg &= ~(0x3 << 15);
		break;
	case SDE_CTL_MODE_SEL_CMD:
		intf_cfg |= BIT(17);
		intf_cfg |= ((cfg->stream_sel & 0x3) << 15);
		break;
	default:
		pr_err("unknown interface type %d\n", cfg->intf_mode_sel);
		return -EINVAL;
	}

	SDE_REG_WRITE(c, CTL_TOP, intf_cfg);
	return 0;
}

static void sde_hw_ctl_update_wb_cfg(struct sde_hw_ctl *ctx,
		struct sde_hw_intf_cfg *cfg, bool enable)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 intf_cfg = 0;

	if (!cfg->wb)
		return;

	intf_cfg = SDE_REG_READ(c, CTL_TOP);
	if (enable)
		intf_cfg |= (cfg->wb & 0x3) + 2;
	else
		intf_cfg &= ~((cfg->wb & 0x3) + 2);

	SDE_REG_WRITE(c, CTL_TOP, intf_cfg);
}

static inline u32 sde_hw_ctl_read_ctl_top(struct sde_hw_ctl *ctx)
{
	struct sde_hw_blk_reg_map *c;
	u32 ctl_top;

	if (!ctx) {
		pr_err("Invalid input argument\n");
		return 0;
	}
	c = &ctx->hw;
	ctl_top = SDE_REG_READ(c, CTL_TOP);
	return ctl_top;
}

static inline u32 sde_hw_ctl_read_ctl_layers(struct sde_hw_ctl *ctx, int index)
{
	struct sde_hw_blk_reg_map *c;
	u32 ctl_top;

	if (!ctx) {
		pr_err("Invalid input argument\n");
		return 0;
	}
	c = &ctx->hw;
	ctl_top = SDE_REG_READ(c, CTL_LAYER(index));
	pr_debug("Ctl_layer value = 0x%x\n", ctl_top);
	return ctl_top;
}

static int sde_hw_ctl_setup_sbuf_cfg(struct sde_hw_ctl *ctx,
	struct sde_ctl_sbuf_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c;
	u32 val;

	if (!ctx)
		return -EINVAL;

	c = &ctx->hw;
	val = cfg->rot_op_mode & 0x3;

	SDE_REG_WRITE(c, CTL_ROT_TOP, val);
	return 0;
}

static int sde_hw_reg_dma_flush(struct sde_hw_ctl *ctx, bool blocking)
{
	struct sde_hw_reg_dma_ops *ops = sde_reg_dma_get_ops();

	if (!ctx)
		return -EINVAL;

	if (ops && ops->last_command)
		return ops->last_command(ctx, DMA_CTL_QUEUE0,
		    (blocking ? REG_DMA_WAIT4_COMP : REG_DMA_NOWAIT));

	return 0;

}

static void _setup_ctl_ops(struct sde_hw_ctl_ops *ops,
		unsigned long cap)
{
	if (cap & BIT(SDE_CTL_ACTIVE_CFG)) {
		ops->update_pending_flush =
			sde_hw_ctl_update_pending_flush_v1;
		ops->trigger_flush = sde_hw_ctl_trigger_flush_v1;

		ops->setup_intf_cfg_v1 = sde_hw_ctl_intf_cfg_v1;
		ops->update_cwb_cfg = sde_hw_ctl_update_cwb_cfg;
		ops->setup_dsc_cfg = sde_hw_ctl_dsc_cfg;

		ops->update_bitmask_cdm = sde_hw_ctl_update_bitmask_cdm_v1;
		ops->update_bitmask_wb = sde_hw_ctl_update_bitmask_wb_v1;
		ops->update_bitmask_intf = sde_hw_ctl_update_bitmask_intf_v1;
		ops->update_bitmask_dsc = sde_hw_ctl_update_bitmask_dsc_v1;
		ops->update_bitmask_merge3d =
			sde_hw_ctl_update_bitmask_merge3d_v1;
		ops->update_bitmask_cwb = sde_hw_ctl_update_bitmask_cwb_v1;
		ops->update_bitmask_periph =
			sde_hw_ctl_update_bitmask_periph_v1;
		ops->get_ctl_intf = sde_hw_ctl_get_intf_v1;
		ops->reset_post_disable = sde_hw_ctl_reset_post_disable;
	} else {
		ops->update_pending_flush = sde_hw_ctl_update_pending_flush;
		ops->trigger_flush = sde_hw_ctl_trigger_flush;

		ops->setup_intf_cfg = sde_hw_ctl_intf_cfg;

		ops->update_bitmask_cdm = sde_hw_ctl_update_bitmask_cdm;
		ops->update_bitmask_wb = sde_hw_ctl_update_bitmask_wb;
		ops->update_bitmask_intf = sde_hw_ctl_update_bitmask_intf;
		ops->get_ctl_intf = sde_hw_ctl_get_intf;
	}
	ops->clear_pending_flush = sde_hw_ctl_clear_pending_flush;
	ops->get_pending_flush = sde_hw_ctl_get_pending_flush;
	ops->get_flush_register = sde_hw_ctl_get_flush_register;
	ops->trigger_start = sde_hw_ctl_trigger_start;
	ops->trigger_pending = sde_hw_ctl_trigger_pending;
	ops->read_ctl_top = sde_hw_ctl_read_ctl_top;
	ops->read_ctl_layers = sde_hw_ctl_read_ctl_layers;
	ops->update_wb_cfg = sde_hw_ctl_update_wb_cfg;
	ops->reset = sde_hw_ctl_reset_control;
	ops->get_reset = sde_hw_ctl_get_reset_status;
	ops->hard_reset = sde_hw_ctl_hard_reset;
	ops->wait_reset_status = sde_hw_ctl_wait_reset_status;
	ops->clear_all_blendstages = sde_hw_ctl_clear_all_blendstages;
	ops->setup_blendstage = sde_hw_ctl_setup_blendstage;
	ops->get_staged_sspp = sde_hw_ctl_get_staged_sspp;
	ops->update_bitmask_sspp = sde_hw_ctl_update_bitmask_sspp;
	ops->update_bitmask_mixer = sde_hw_ctl_update_bitmask_mixer;
	ops->update_bitmask_dspp = sde_hw_ctl_update_bitmask_dspp;
	ops->update_bitmask_dspp_pavlut = sde_hw_ctl_update_bitmask_dspp_pavlut;
	ops->reg_dma_flush = sde_hw_reg_dma_flush;
	ops->get_start_state = sde_hw_ctl_get_start_state;
	if (cap & BIT(SDE_CTL_SBUF)) {
		ops->update_bitmask_rot = sde_hw_ctl_update_bitmask_rot;
		ops->setup_sbuf_cfg = sde_hw_ctl_setup_sbuf_cfg;
		ops->trigger_rot_start = sde_hw_ctl_trigger_rot_start;
	}
};

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_ctl *sde_hw_ctl_init(enum sde_ctl idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_ctl *c;
	struct sde_ctl_cfg *cfg;
	int rc;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _ctl_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		pr_err("failed to create sde_hw_ctl %d\n", idx);
		return ERR_PTR(-EINVAL);
	}

	c->caps = cfg;
	_setup_ctl_ops(&c->ops, c->caps->features);
	c->idx = idx;
	c->mixer_count = m->mixer_count;
	c->mixer_hw_caps = m->mixer;

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_CTL, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
			c->hw.blk_off + c->hw.length, c->hw.xin_id);

	return c;

blk_init_error:
	kzfree(c);

	return ERR_PTR(rc);
}

void sde_hw_ctl_destroy(struct sde_hw_ctl *ctx)
{
	if (ctx)
		sde_hw_blk_destroy(&ctx->base);
	kfree(ctx);
}
