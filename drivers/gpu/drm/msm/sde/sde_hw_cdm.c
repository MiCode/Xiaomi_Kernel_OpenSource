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

#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_cdm.h"
#include "sde_dbg.h"
#include "sde_kms.h"

#define CDM_CSC_10_OPMODE                  0x000
#define CDM_CSC_10_BASE                    0x004

#define CDM_CDWN2_OP_MODE                  0x100
#define CDM_CDWN2_CLAMP_OUT                0x104
#define CDM_CDWN2_PARAMS_3D_0              0x108
#define CDM_CDWN2_PARAMS_3D_1              0x10C
#define CDM_CDWN2_COEFF_COSITE_H_0         0x110
#define CDM_CDWN2_COEFF_COSITE_H_1         0x114
#define CDM_CDWN2_COEFF_COSITE_H_2         0x118
#define CDM_CDWN2_COEFF_OFFSITE_H_0        0x11C
#define CDM_CDWN2_COEFF_OFFSITE_H_1        0x120
#define CDM_CDWN2_COEFF_OFFSITE_H_2        0x124
#define CDM_CDWN2_COEFF_COSITE_V           0x128
#define CDM_CDWN2_COEFF_OFFSITE_V          0x12C
#define CDM_CDWN2_OUT_SIZE                 0x130

#define CDM_HDMI_PACK_OP_MODE              0x200
#define CDM_CSC_10_MATRIX_COEFF_0          0x004

/**
 * Horizontal coefficients for cosite chroma downscale
 * s13 representation of coefficients
 */
static u32 cosite_h_coeff[] = {0x00000016, 0x000001cc, 0x0100009e};

/**
 * Horizontal coefficients for offsite chroma downscale
 */
static u32 offsite_h_coeff[] = {0x000b0005, 0x01db01eb, 0x00e40046};

/**
 * Vertical coefficients for cosite chroma downscale
 */
static u32 cosite_v_coeff[] = {0x00080004};
/**
 * Vertical coefficients for offsite chroma downscale
 */
static u32 offsite_v_coeff[] = {0x00060002};

/* Limited Range rgb2yuv coeff with clamp and bias values for CSC 10 module */
static struct sde_csc_cfg rgb2yuv_cfg = {
	{
		0x0083, 0x0102, 0x0032,
		0x1fb5, 0x1f6c, 0x00e1,
		0x00e1, 0x1f45, 0x1fdc
	},
	{ 0x00, 0x00, 0x00 },
	{ 0x0040, 0x0200, 0x0200 },
	{ 0x000, 0x3ff, 0x000, 0x3ff, 0x000, 0x3ff },
	{ 0x040, 0x3ac, 0x040, 0x3c0, 0x040, 0x3c0 },
};

static struct sde_cdm_cfg *_cdm_offset(enum sde_cdm cdm,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->cdm_count; i++) {
		if (cdm == m->cdm[i].id) {
			b->base_off = addr;
			b->blk_off = m->cdm[i].base;
			b->length = m->cdm[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_CDM;
			return &m->cdm[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

static int sde_hw_cdm_setup_csc_10bit(struct sde_hw_cdm *ctx,
		struct sde_csc_cfg *data)
{
	sde_hw_csc_setup(&ctx->hw, CDM_CSC_10_MATRIX_COEFF_0, data, true);

	return 0;
}

static int sde_hw_cdm_setup_cdwn(struct sde_hw_cdm *ctx,
		struct sde_hw_cdm_cfg *cfg)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	u32 opmode = 0;
	u32 out_size = 0;

	if (cfg->output_bit_depth == CDM_CDWN_OUTPUT_10BIT)
		opmode &= ~BIT(7);
	else
		opmode |= BIT(7);

	/* ENABLE DWNS_H bit */
	opmode |= BIT(1);

	switch (cfg->h_cdwn_type) {
	case CDM_CDWN_DISABLE:
		/* CLEAR METHOD_H field */
		opmode &= ~(0x18);
		/* CLEAR DWNS_H bit */
		opmode &= ~BIT(1);
		break;
	case CDM_CDWN_PIXEL_DROP:
		/* Clear METHOD_H field (pixel drop is 0) */
		opmode &= ~(0x18);
		break;
	case CDM_CDWN_AVG:
		/* Clear METHOD_H field (Average is 0x1) */
		opmode &= ~(0x18);
		opmode |= (0x1 << 0x3);
		break;
	case CDM_CDWN_COSITE:
		/* Clear METHOD_H field (Average is 0x2) */
		opmode &= ~(0x18);
		opmode |= (0x2 << 0x3);
		/* Co-site horizontal coefficients */
		SDE_REG_WRITE(c, CDM_CDWN2_COEFF_COSITE_H_0,
				cosite_h_coeff[0]);
		SDE_REG_WRITE(c, CDM_CDWN2_COEFF_COSITE_H_1,
				cosite_h_coeff[1]);
		SDE_REG_WRITE(c, CDM_CDWN2_COEFF_COSITE_H_2,
				cosite_h_coeff[2]);
		break;
	case CDM_CDWN_OFFSITE:
		/* Clear METHOD_H field (Average is 0x3) */
		opmode &= ~(0x18);
		opmode |= (0x3 << 0x3);

		/* Off-site horizontal coefficients */
		SDE_REG_WRITE(c, CDM_CDWN2_COEFF_OFFSITE_H_0,
				offsite_h_coeff[0]);
		SDE_REG_WRITE(c, CDM_CDWN2_COEFF_OFFSITE_H_1,
				offsite_h_coeff[1]);
		SDE_REG_WRITE(c, CDM_CDWN2_COEFF_OFFSITE_H_2,
				offsite_h_coeff[2]);
		break;
	default:
		pr_err("%s invalid horz down sampling type\n", __func__);
		return -EINVAL;
	}

	/* ENABLE DWNS_V bit */
	opmode |= BIT(2);

	switch (cfg->v_cdwn_type) {
	case CDM_CDWN_DISABLE:
		/* CLEAR METHOD_V field */
		opmode &= ~(0x60);
		/* CLEAR DWNS_V bit */
		opmode &= ~BIT(2);
		break;
	case CDM_CDWN_PIXEL_DROP:
		/* Clear METHOD_V field (pixel drop is 0) */
		opmode &= ~(0x60);
		break;
	case CDM_CDWN_AVG:
		/* Clear METHOD_V field (Average is 0x1) */
		opmode &= ~(0x60);
		opmode |= (0x1 << 0x5);
		break;
	case CDM_CDWN_COSITE:
		/* Clear METHOD_V field (Average is 0x2) */
		opmode &= ~(0x60);
		opmode |= (0x2 << 0x5);
		/* Co-site vertical coefficients */
		SDE_REG_WRITE(c,
				CDM_CDWN2_COEFF_COSITE_V,
				cosite_v_coeff[0]);
		break;
	case CDM_CDWN_OFFSITE:
		/* Clear METHOD_V field (Average is 0x3) */
		opmode &= ~(0x60);
		opmode |= (0x3 << 0x5);

		/* Off-site vertical coefficients */
		SDE_REG_WRITE(c,
				CDM_CDWN2_COEFF_OFFSITE_V,
				offsite_v_coeff[0]);
		break;
	default:
		return -EINVAL;
	}

	if (cfg->v_cdwn_type || cfg->h_cdwn_type)
		opmode |= BIT(0); /* EN CDWN module */
	else
		opmode &= ~BIT(0);

	out_size = (cfg->output_width & 0xFFFF) |
		((cfg->output_height & 0xFFFF) << 16);
	SDE_REG_WRITE(c, CDM_CDWN2_OUT_SIZE, out_size);
	SDE_REG_WRITE(c, CDM_CDWN2_OP_MODE, opmode);
	SDE_REG_WRITE(c, CDM_CDWN2_CLAMP_OUT,
			((0x3FF << 16) | 0x0));

	return 0;
}

int sde_hw_cdm_enable(struct sde_hw_cdm *ctx,
		struct sde_hw_cdm_cfg *cdm)
{
	struct sde_hw_blk_reg_map *c = &ctx->hw;
	const struct sde_format *fmt = cdm->output_fmt;
	struct cdm_output_cfg cdm_cfg = { 0 };
	u32 opmode = 0;
	u32 csc = 0;

	if (!SDE_FORMAT_IS_YUV(fmt))
		return -EINVAL;

	if (cdm->output_type == CDM_CDWN_OUTPUT_HDMI) {
		if (fmt->chroma_sample != SDE_CHROMA_H1V2)
			return -EINVAL; /*unsupported format */
		opmode = BIT(0);
		opmode |= (fmt->chroma_sample << 1);
		cdm_cfg.intf_en = true;
	} else {
		opmode = 0;
		cdm_cfg.wb_en = true;
	}

	csc |= BIT(2);
	csc &= ~BIT(1);
	csc |= BIT(0);

	if (ctx->hw_mdp && ctx->hw_mdp->ops.setup_cdm_output)
		ctx->hw_mdp->ops.setup_cdm_output(ctx->hw_mdp, &cdm_cfg);

	SDE_REG_WRITE(c, CDM_CSC_10_OPMODE, csc);
	SDE_REG_WRITE(c, CDM_HDMI_PACK_OP_MODE, opmode);
	return 0;
}

void sde_hw_cdm_disable(struct sde_hw_cdm *ctx)
{
	struct cdm_output_cfg cdm_cfg = { 0 };

	if (ctx->hw_mdp && ctx->hw_mdp->ops.setup_cdm_output)
		ctx->hw_mdp->ops.setup_cdm_output(ctx->hw_mdp, &cdm_cfg);
}

static void _setup_cdm_ops(struct sde_hw_cdm_ops *ops,
	unsigned long features)
{
	ops->setup_csc_data = sde_hw_cdm_setup_csc_10bit;
	ops->setup_cdwn = sde_hw_cdm_setup_cdwn;
	ops->enable = sde_hw_cdm_enable;
	ops->disable = sde_hw_cdm_disable;
}

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_cdm *sde_hw_cdm_init(enum sde_cdm idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m,
		struct sde_hw_mdp *hw_mdp)
{
	struct sde_hw_cdm *c;
	struct sde_cdm_cfg *cfg;
	int rc;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _cdm_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	c->idx = idx;
	c->caps = cfg;
	_setup_cdm_ops(&c->ops, c->caps->features);
	c->hw_mdp = hw_mdp;

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_CDM, idx, &sde_hw_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	sde_dbg_reg_register_dump_range(SDE_DBG_NAME, cfg->name, c->hw.blk_off,
			c->hw.blk_off + c->hw.length, c->hw.xin_id);

	/*
	 * Perform any default initialization for the chroma down module
	 * @setup default csc coefficients
	 */
	sde_hw_cdm_setup_csc_10bit(c, &rgb2yuv_cfg);

	return c;

blk_init_error:
	kzfree(c);

	return ERR_PTR(rc);
}

void sde_hw_cdm_destroy(struct sde_hw_cdm *cdm)
{
	if (cdm)
		sde_hw_blk_destroy(&cdm->base);
	kfree(cdm);
}
