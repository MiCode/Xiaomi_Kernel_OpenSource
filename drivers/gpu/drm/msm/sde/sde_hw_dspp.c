/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include <drm/msm_drm_pp.h>
#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_dspp.h"
#include "sde_hw_color_processing.h"
#include "sde_dbg.h"
#include "sde_ad4.h"
#include "sde_kms.h"

static struct sde_dspp_cfg *_dspp_offset(enum sde_dspp dspp,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	if (!m || !addr || !b)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < m->dspp_count; i++) {
		if (dspp == m->dspp[i].id) {
			b->base_off = addr;
			b->blk_off = m->dspp[i].base;
			b->length = m->dspp[i].len;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_DSPP;
			return &m->dspp[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

static void _setup_dspp_ops(struct sde_hw_dspp *c, unsigned long features)
{
	int i = 0, ret;

	if (!c || !c->cap || !c->cap->sblk)
		return;

	for (i = 0; i < SDE_DSPP_MAX; i++) {
		if (!test_bit(i, &features))
			continue;
		switch (i) {
		case SDE_DSPP_PCC:
			if (c->cap->sblk->pcc.version ==
				(SDE_COLOR_PROCESS_VER(0x1, 0x7)))
				c->ops.setup_pcc = sde_setup_dspp_pcc_v1_7;
			else if (c->cap->sblk->pcc.version ==
					(SDE_COLOR_PROCESS_VER(0x4, 0x0))) {
				ret = reg_dmav1_init_dspp_op_v4(i, c->idx);
				if (!ret)
					c->ops.setup_pcc =
						reg_dmav1_setup_dspp_pccv4;
				else
					c->ops.setup_pcc =
						sde_setup_dspp_pccv4;
			}
			break;
		case SDE_DSPP_HSIC:
			if (c->cap->sblk->hsic.version ==
				SDE_COLOR_PROCESS_VER(0x1, 0x7))
				c->ops.setup_pa_hsic =
					sde_setup_dspp_pa_hsic_v17;
			break;
		case SDE_DSPP_MEMCOLOR:
			if (c->cap->sblk->memcolor.version ==
				SDE_COLOR_PROCESS_VER(0x1, 0x7)) {
				c->ops.setup_pa_memcol_skin =
					sde_setup_dspp_memcol_skin_v17;
				c->ops.setup_pa_memcol_sky =
					sde_setup_dspp_memcol_sky_v17;
				c->ops.setup_pa_memcol_foliage =
					sde_setup_dspp_memcol_foliage_v17;
				c->ops.setup_pa_memcol_prot =
					sde_setup_dspp_memcol_prot_v17;
			}
			break;
		case SDE_DSPP_SIXZONE:
			if (c->cap->sblk->sixzone.version ==
				SDE_COLOR_PROCESS_VER(0x1, 0x7))
				c->ops.setup_sixzone =
					sde_setup_dspp_sixzone_v17;
			break;
		case SDE_DSPP_DITHER:
			if (c->cap->sblk->dither.version ==
				SDE_COLOR_PROCESS_VER(0x1, 0x7))
				c->ops.setup_pa_dither =
					sde_setup_dspp_dither_v1_7;
			break;
		case SDE_DSPP_VLUT:
			if (c->cap->sblk->vlut.version ==
				(SDE_COLOR_PROCESS_VER(0x1, 0x7))) {
				c->ops.setup_vlut =
				    sde_setup_dspp_pa_vlut_v1_7;
			} else if (c->cap->sblk->vlut.version ==
					(SDE_COLOR_PROCESS_VER(0x1, 0x8))) {
				ret = reg_dmav1_init_dspp_op_v4(i, c->idx);
				if (!ret)
					c->ops.setup_vlut =
					reg_dmav1_setup_dspp_vlutv18;
				else
					c->ops.setup_vlut =
					sde_setup_dspp_pa_vlut_v1_8;
			}
			break;
		case SDE_DSPP_HIST:
			if (c->cap->sblk->hist.version ==
				(SDE_COLOR_PROCESS_VER(0x1, 0x7))) {
				c->ops.setup_histogram =
				    sde_setup_dspp_hist_v1_7;
				c->ops.read_histogram =
				    sde_read_dspp_hist_v1_7;
				c->ops.lock_histogram =
				    sde_lock_dspp_hist_v1_7;
			}
			break;
		case SDE_DSPP_GAMUT:
			if (c->cap->sblk->gamut.version ==
					SDE_COLOR_PROCESS_VER(0x4, 0)) {
				ret = reg_dmav1_init_dspp_op_v4(i, c->idx);
				if (!ret)
					c->ops.setup_gamut =
						reg_dmav1_setup_dspp_3d_gamutv4;
				else
					c->ops.setup_gamut =
						sde_setup_dspp_3d_gamutv4;
			}
			break;
		case SDE_DSPP_GC:
			if (c->cap->sblk->gc.version ==
					SDE_COLOR_PROCESS_VER(0x1, 8)) {
				ret = reg_dmav1_init_dspp_op_v4(i, c->idx);
				if (!ret)
					c->ops.setup_gc =
						reg_dmav1_setup_dspp_gcv18;
				/** programming for v18 through ahb is same
				 * as v17 hence assign v17 function
				 */
				else
					c->ops.setup_gc =
						sde_setup_dspp_gc_v1_7;
			}
			break;
		case SDE_DSPP_IGC:
			if (c->cap->sblk->igc.version ==
					SDE_COLOR_PROCESS_VER(0x3, 0x1)) {
				ret = reg_dmav1_init_dspp_op_v4(i, c->idx);
				if (!ret)
					c->ops.setup_igc =
						reg_dmav1_setup_dspp_igcv31;
				else
					c->ops.setup_igc =
						sde_setup_dspp_igcv3;
			}
			break;
		case SDE_DSPP_AD:
			if (c->cap->sblk->ad.version ==
			    SDE_COLOR_PROCESS_VER(4, 0)) {
				c->ops.setup_ad = sde_setup_dspp_ad4;
				c->ops.ad_read_intr_resp =
					sde_read_intr_resp_ad4;
				c->ops.validate_ad = sde_validate_dspp_ad4;
			}
			break;
		default:
			break;
		}
	}
}

static struct sde_hw_blk_ops sde_hw_ops = {
	.start = NULL,
	.stop = NULL,
};

struct sde_hw_dspp *sde_hw_dspp_init(enum sde_dspp idx,
			void __iomem *addr,
			struct sde_mdss_cfg *m)
{
	struct sde_hw_dspp *c;
	struct sde_dspp_cfg *cfg;
	int rc;

	if (!addr || !m)
		return ERR_PTR(-EINVAL);

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _dspp_offset(idx, m, addr, &c->hw);
	if (IS_ERR_OR_NULL(cfg)) {
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/* Populate DSPP Top HW block */
	c->hw_top.base_off = addr;
	c->hw_top.blk_off = m->dspp_top.base;
	c->hw_top.length = m->dspp_top.len;
	c->hw_top.hwversion = m->hwversion;
	c->hw_top.log_mask = SDE_DBG_MASK_DSPP;

	/* Assign ops */
	c->idx = idx;
	c->cap = cfg;
	_setup_dspp_ops(c, c->cap->features);

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_DSPP, idx, &sde_hw_ops);
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

void sde_hw_dspp_destroy(struct sde_hw_dspp *dspp)
{
	if (dspp) {
		reg_dmav1_deinit_dspp_ops(dspp->idx);
		sde_hw_blk_destroy(&dspp->base);
	}
	kfree(dspp);
}
