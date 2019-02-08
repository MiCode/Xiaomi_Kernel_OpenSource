/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm-shd:%s:%d] " fmt, __func__, __LINE__

#include <uapi/drm/sde_drm.h>
#include "sde_hw_top.h"
#include "shd_drm.h"

#ifndef SHD_HW_H
#define SHD_HW_H

struct sde_shd_ctl_mixer_cfg {
	u32 mixercfg;
	u32 mixercfg_ext;
	u32 mixercfg_ext2;
	u32 mixercfg_ext3;

	u32 mixercfg_mask;
	u32 mixercfg_ext_mask;
	u32 mixercfg_ext2_mask;
	u32 mixercfg_ext3_mask;
};

struct sde_shd_hw_ctl {
	struct sde_hw_ctl base;
	struct shd_stage_range range;
	struct sde_hw_ctl *orig;
	u32 flush_mask;
	struct sde_shd_ctl_mixer_cfg mixer_cfg[MAX_BLOCKS];
};

struct sde_shd_mixer_cfg {
	uint32_t fg_alpha;
	uint32_t bg_alpha;
	uint32_t blend_op;
	bool dirty;

	struct sde_hw_dim_layer dim_layer;
	bool dim_layer_enable;
};

struct sde_shd_hw_mixer {
	struct sde_hw_mixer base;
	struct shd_stage_range range;
	struct sde_rect roi;
	struct sde_hw_mixer *orig;
	struct sde_shd_mixer_cfg cfg[SDE_STAGE_MAX];
};

void sde_shd_hw_flush(struct sde_hw_ctl *ctl_ctx,
	struct sde_hw_mixer *lm_ctx[CRTC_DUAL_MIXERS], int lm_num);

void sde_shd_hw_ctl_init_op(struct sde_hw_ctl *ctx);

void sde_shd_hw_lm_init_op(struct sde_hw_mixer *ctx);

#endif
