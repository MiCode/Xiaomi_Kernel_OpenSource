// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */
#include <drm/msm_drm_pp.h>
#include "sde_hw_color_proc_common_v4.h"
#include "sde_hw_color_proc_v4.h"

static int sde_write_3d_gamut(struct sde_hw_blk_reg_map *hw,
		struct drm_msm_3d_gamut *payload, u32 base,
		u32 *opcode, u32 pipe, u32 scale_tbl_a_len,
		u32 scale_tbl_b_len)
{
	u32 reg, tbl_len, tbl_off, scale_off, i, j;
	u32 scale_tbl_len, scale_tbl_off;
	u32 *scale_data;

	if (!payload || !opcode || !hw) {
		DRM_ERROR("invalid payload %pK opcode %pK hw %pK\n",
			payload, opcode, hw);
		return -EINVAL;
	}

	switch (payload->mode) {
	case GAMUT_3D_MODE_17:
		tbl_len = GAMUT_3D_MODE17_TBL_SZ;
		tbl_off = 0;
		if (pipe == DSPP) {
			scale_off = GAMUT_SCALEA_OFFSET_OFF;
			*opcode = gamut_mode_17;
		} else {
			*opcode = (*opcode & (BIT(5) - 1)) >> 2;
			if (*opcode == gamut_mode_17b)
				*opcode = gamut_mode_17;
			else
				*opcode = gamut_mode_17b;
			scale_off = (*opcode == gamut_mode_17) ?
				GAMUT_SCALEA_OFFSET_OFF :
				GAMUT_SCALEB_OFFSET_OFF;
		}
		break;
	case GAMUT_3D_MODE_13:
		*opcode = (*opcode & (BIT(4) - 1)) >> 2;
		if (*opcode == gamut_mode_13a)
			*opcode = gamut_mode_13b;
		else
			*opcode = gamut_mode_13a;
		tbl_len = GAMUT_3D_MODE13_TBL_SZ;
		tbl_off = (*opcode == gamut_mode_13a) ? 0 :
			GAMUT_MODE_13B_OFF;
		scale_off = (*opcode == gamut_mode_13a) ?
			GAMUT_SCALEA_OFFSET_OFF : GAMUT_SCALEB_OFFSET_OFF;
		*opcode <<= 2;
		break;
	case GAMUT_3D_MODE_5:
		*opcode = gamut_mode_5 << 2;
		tbl_len = GAMUT_3D_MODE5_TBL_SZ;
		tbl_off = GAMUT_MODE_5_OFF;
		scale_off = GAMUT_SCALEB_OFFSET_OFF;
		break;
	default:
		DRM_ERROR("invalid mode %d\n", payload->mode);
		return -EINVAL;
	}

	if (payload->flags & GAMUT_3D_MAP_EN)
		*opcode |= GAMUT_MAP_EN;
	*opcode |= GAMUT_EN;

	for (i = 0; i < GAMUT_3D_TBL_NUM; i++) {
		reg = GAMUT_TABLE0_SEL << i;
		reg |= ((tbl_off) & (BIT(11) - 1));
		SDE_REG_WRITE(hw, base + GAMUT_TABLE_SEL_OFF, reg);
		for (j = 0; j < tbl_len; j++) {
			SDE_REG_WRITE(hw, base + GAMUT_LOWER_COLOR_OFF,
					payload->col[i][j].c2_c1);
			SDE_REG_WRITE(hw, base + GAMUT_UPPER_COLOR_OFF,
					payload->col[i][j].c0);
		}
	}

	if ((*opcode & GAMUT_MAP_EN)) {
		if (scale_off == GAMUT_SCALEA_OFFSET_OFF)
			scale_tbl_len = scale_tbl_a_len;
		else
			scale_tbl_len = scale_tbl_b_len;
		for (i = 0; i < GAMUT_3D_SCALE_OFF_TBL_NUM; i++) {
			scale_tbl_off = base + scale_off +
					i * scale_tbl_len * sizeof(u32);
			scale_data = &payload->scale_off[i][0];
			for (j = 0; j < scale_tbl_len; j++)
				SDE_REG_WRITE(hw,
					scale_tbl_off + (j * sizeof(u32)),
					scale_data[j]);
		}
	}
	SDE_REG_WRITE(hw, base, *opcode);
	return 0;
}

void sde_setup_dspp_3d_gamutv4(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_3d_gamut *payload;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 op_mode;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	op_mode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->gamut.base);
	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable gamut feature\n");
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->gamut.base, 0);
		return;
	}

	payload = hw_cfg->payload;
	sde_write_3d_gamut(&ctx->hw, payload, ctx->cap->sblk->gamut.base,
		&op_mode, DSPP, GAMUT_3D_SCALE_OFF_SZ, GAMUT_3D_SCALEB_OFF_SZ);

}

void sde_setup_dspp_3d_gamutv41(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_3d_gamut *payload;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u32 op_mode;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	op_mode = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->gamut.base);
	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable gamut feature\n");
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->gamut.base, 0);
		return;
	}

	payload = hw_cfg->payload;
	sde_write_3d_gamut(&ctx->hw, payload, ctx->cap->sblk->gamut.base,
		&op_mode, DSPP, GAMUT_3D_SCALE_OFF_SZ, GAMUT_3D_SCALE_OFF_SZ);
}

void sde_setup_dspp_igcv3(struct sde_hw_dspp *ctx, void *cfg)
{
	struct drm_msm_igc_lut *lut_cfg;
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	int i = 0, j = 0;
	u32 *addr[IGC_TBL_NUM];
	u32 offset = 0;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable igc feature\n");
		SDE_REG_WRITE(&ctx->hw, IGC_OPMODE_OFF, 0);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_igc_lut)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_igc_lut));
		return;
	}

	lut_cfg = hw_cfg->payload;

	addr[0] = lut_cfg->c0;
	addr[1] = lut_cfg->c1;
	addr[2] = lut_cfg->c2;

	for (i = 0; i < IGC_TBL_NUM; i++) {
		offset = IGC_C0_OFF + (i * sizeof(u32));

		for (j = 0; j < IGC_TBL_LEN; j++) {
			addr[i][j] &= IGC_DATA_MASK;
			addr[i][j] |= IGC_DSPP_SEL_MASK(ctx->idx - 1);
			if (j == 0)
				addr[i][j] |= IGC_INDEX_UPDATE;
			/* IGC lut registers are part of DSPP Top HW block */
			SDE_REG_WRITE(&ctx->hw_top, offset, addr[i][j]);
		}
	}

	if (lut_cfg->flags & IGC_DITHER_ENABLE) {
		SDE_REG_WRITE(&ctx->hw, IGC_DITHER_OFF,
			lut_cfg->strength & IGC_DITHER_DATA_MASK);
	}

	SDE_REG_WRITE(&ctx->hw, IGC_OPMODE_OFF, IGC_EN);
}

void sde_setup_dspp_pccv4(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct drm_msm_pcc *pcc_cfg;
	struct drm_msm_pcc_coeff *coeffs = NULL;
	int i = 0;
	u32 base = 0;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid param ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("disable pcc feature\n");
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base, 0);
		return;
	}

	if (hw_cfg->len != sizeof(struct drm_msm_pcc)) {
		DRM_ERROR("invalid size of payload len %d exp %zd\n",
				hw_cfg->len, sizeof(struct drm_msm_pcc));
		return;
	}

	pcc_cfg = hw_cfg->payload;

	for (i = 0; i < PCC_NUM_PLANES; i++) {
		base = ctx->cap->sblk->pcc.base + (i * sizeof(u32));
		switch (i) {
		case 0:
			coeffs = &pcc_cfg->r;
			SDE_REG_WRITE(&ctx->hw,
				base + PCC_RR_OFF, pcc_cfg->r_rr);
			SDE_REG_WRITE(&ctx->hw,
				base + PCC_GG_OFF, pcc_cfg->r_gg);
			SDE_REG_WRITE(&ctx->hw,
				base + PCC_BB_OFF, pcc_cfg->r_bb);
			break;
		case 1:
			coeffs = &pcc_cfg->g;
			SDE_REG_WRITE(&ctx->hw,
				base + PCC_RR_OFF, pcc_cfg->g_rr);
			SDE_REG_WRITE(&ctx->hw,
				base + PCC_GG_OFF, pcc_cfg->g_gg);
			SDE_REG_WRITE(&ctx->hw,
				base + PCC_BB_OFF, pcc_cfg->g_bb);
			break;
		case 2:
			coeffs = &pcc_cfg->b;
			SDE_REG_WRITE(&ctx->hw,
				base + PCC_RR_OFF, pcc_cfg->b_rr);
			SDE_REG_WRITE(&ctx->hw,
				base + PCC_GG_OFF, pcc_cfg->b_gg);
			SDE_REG_WRITE(&ctx->hw,
				base + PCC_BB_OFF, pcc_cfg->b_bb);
			break;
		default:
			DRM_ERROR("invalid pcc plane: %d\n", i);
			return;
		}

		SDE_REG_WRITE(&ctx->hw, base + PCC_C_OFF, coeffs->c);
		SDE_REG_WRITE(&ctx->hw, base + PCC_R_OFF, coeffs->r);
		SDE_REG_WRITE(&ctx->hw, base + PCC_G_OFF, coeffs->g);
		SDE_REG_WRITE(&ctx->hw, base + PCC_B_OFF, coeffs->b);
		SDE_REG_WRITE(&ctx->hw, base + PCC_RG_OFF, coeffs->rg);
		SDE_REG_WRITE(&ctx->hw, base + PCC_RB_OFF, coeffs->rb);
		SDE_REG_WRITE(&ctx->hw, base + PCC_GB_OFF, coeffs->gb);
		SDE_REG_WRITE(&ctx->hw, base + PCC_RGB_OFF, coeffs->rgb);
	}

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->pcc.base, PCC_EN);
}

void sde_setup_dspp_ltm_threshv1(struct sde_hw_dspp *ctx, void *cfg)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	u64 thresh = 0;

	if (!ctx || !cfg) {
		DRM_ERROR("invalid parameters ctx %pK cfg %pK\n", ctx, cfg);
		return;
	}

	if (!hw_cfg->payload) {
		DRM_DEBUG_DRIVER("Disable LTM noise thresh feature\n");
		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->ltm.base + 0x60, 0);
		return;
	}

	thresh = *((u64 *)hw_cfg->payload);

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->ltm.base + 0x60,
			(thresh & 0x3FF));
}

void sde_setup_dspp_ltm_hist_bufferv1(struct sde_hw_dspp *ctx, u64 addr)
{
	struct drm_msm_ltm_stats_data *hist = NULL;
	u64 lh_addr, hs_addr;

	if (!ctx || !addr) {
		DRM_ERROR("invalid parameter ctx %pK addr 0x%llx\n", ctx, addr);
		return;
	}

	hist = (struct drm_msm_ltm_stats_data *)addr;
	lh_addr = (u64)(&hist->stats_02[0]);
	hs_addr = (u64)(&hist->stats_03[0]);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->ltm.base + 0x70,
			(addr & 0xFFFFFF00));
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->ltm.base + 0x74,
			(lh_addr & 0xFFFFFF00));
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->ltm.base + 0x78,
			(hs_addr & 0xFFFFFF00));
}

void sde_setup_dspp_ltm_hist_ctrlv1(struct sde_hw_dspp *ctx, void *cfg,
				    bool enable, u64 addr)
{
	struct sde_hw_cp_cfg *hw_cfg = cfg;
	struct sde_ltm_phase_info phase;
	u32 op_mode, offset;

	if (!ctx) {
		DRM_ERROR("invalid parameters ctx %pK\n", ctx);
		return;
	}

	if (enable && (!addr || !cfg)) {
		DRM_ERROR("invalid addr 0x%llx cfg %pK\n", addr, cfg);
		return;
	}

	offset = ctx->cap->sblk->ltm.base + 0x4;
	op_mode = SDE_REG_READ(&ctx->hw, offset);

	if (!enable) {
		if (op_mode & BIT(1))
			op_mode &= ~BIT(0);
		else
			op_mode &= LTM_CONFIG_MERGE_MODE_ONLY;

		SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->ltm.base + 0x4,
			(op_mode & 0x1FFFFFF));
		return;
	}

	if (ctx->idx >= DSPP_MAX) {
		DRM_ERROR("Invalid idx %d\n", ctx->idx);
		return;
	}

	memset(&phase, 0, sizeof(phase));
	sde_ltm_get_phase_info(hw_cfg, &phase);

	if (phase.portrait_en)
		op_mode |= BIT(2);
	else
		op_mode &= ~BIT(2);

	if (phase.merge_en)
		op_mode |= BIT(16);
	else
		op_mode &= ~(BIT(16) | BIT(17));

	offset = ctx->cap->sblk->ltm.base + 0x8;
	SDE_REG_WRITE(&ctx->hw, offset, (phase.init_h[ctx->idx] & 0x7FFFFFF));
	offset += 4;
	SDE_REG_WRITE(&ctx->hw, offset, (phase.init_v & 0xFFFFFF));
	offset += 4;
	SDE_REG_WRITE(&ctx->hw, offset, (phase.inc_h & 0xFFFFFF));
	offset += 4;
	SDE_REG_WRITE(&ctx->hw, offset, (phase.inc_v & 0xFFFFFF));

	op_mode |= BIT(0);
	sde_setup_dspp_ltm_hist_bufferv1(ctx, addr);

	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->ltm.base + 0x4,
			(op_mode & 0x1FFFFFF));
}

void sde_ltm_read_intr_status(struct sde_hw_dspp *ctx, u32 *status)
{
	u32 clear;

	if (!ctx || !status) {
		DRM_ERROR("invalid parameters ctx %pK status %pK\n", ctx,
				status);
		return;
	}

	*status = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->ltm.base + 0x54);
	pr_debug("%s(): LTM interrupt status 0x%x\n", __func__, *status);
	/* clear the hist_sat and hist_merge_sat bits */
	clear = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->ltm.base + 0x58);
	clear |= BIT(1) | BIT(2);
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->ltm.base + 0x58, clear);
}

void sde_ltm_clear_merge_mode(struct sde_hw_dspp *ctx)
{
	u32 clear;

	if (!ctx) {
		DRM_ERROR("invalid parameters ctx %pK\n", ctx);
		return;
	}

	/* clear the merge_mode bit */
	clear = SDE_REG_READ(&ctx->hw, ctx->cap->sblk->ltm.base + 0x04);
	clear &= ~LTM_CONFIG_MERGE_MODE_ONLY;
	SDE_REG_WRITE(&ctx->hw, ctx->cap->sblk->ltm.base + 0x04, clear);
}

void sde_demura_backlight_cfg(struct sde_hw_dspp *dspp, u64 val)
{
	u32 demura_base;
	u32 backlight;

	if (!dspp) {
		DRM_ERROR("invalid parameter ctx %pK", dspp);
		return;
	}
	demura_base = dspp->cap->sblk->demura.base;
	backlight = (val & REG_MASK_ULL(11));
	backlight |= ((val & REG_MASK_SHIFT_ULL(11, 32)) >> 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->demura.base + 0x8,
			backlight);
}
