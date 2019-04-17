/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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
#include "sde_hw_catalog.h"
#include "sde_hw_util.h"
#include "sde_hw_mdss.h"
#include "sde_hw_lm.h"
#include "sde_ad4.h"

#define AD_STATE_READY(x) \
	(((x) & ad4_init) && \
	((x) & ad4_cfg) && \
	((x) & ad4_mode) && \
	(((x) & ad4_input) | ((x) & ad4_strength)))

#define MERGE_WIDTH_RIGHT 6
#define MERGE_WIDTH_LEFT 5
#define AD_IPC_FRAME_COUNT 2

enum ad4_ops_bitmask {
	ad4_init = BIT(AD_INIT),
	ad4_cfg = BIT(AD_CFG),
	ad4_mode = BIT(AD_MODE),
	ad4_input = BIT(AD_INPUT),
	ad4_strength = BIT(AD_STRENGTH),
	ad4_ops_max = BIT(31),
};

enum ad4_state {
	ad4_state_idle,
	ad4_state_startup,
	ad4_state_run,
	/* idle power collapse suspend state */
	ad4_state_ipcs,
	/* idle power collapse resume state */
	ad4_state_ipcr,
	/* manual mode state */
	ad4_state_manual,
	ad4_state_max,
};

typedef int (*ad4_prop_setup)(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *ad);

static int ad4_params_check(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);

static int ad4_no_op_setup(struct sde_hw_dspp *dspp, struct sde_ad_hw_cfg *cfg);
static int ad4_setup_debug(struct sde_hw_dspp *dspp, struct sde_ad_hw_cfg *cfg);
static int ad4_setup_debug_manual(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_mode_setup(struct sde_hw_dspp *dspp, enum ad4_modes mode);
static int ad4_mode_setup_common(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_init_setup(struct sde_hw_dspp *dspp, struct sde_ad_hw_cfg *cfg);
static int ad4_init_setup_idle(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_init_setup_run(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_init_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_cfg_setup(struct sde_hw_dspp *dspp, struct sde_ad_hw_cfg *cfg);
static int ad4_cfg_setup_idle(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_cfg_setup_run(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_cfg_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_input_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_input_setup_idle(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_input_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_suspend_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_assertive_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_assertive_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_backlight_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_backlight_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_strength_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_strength_setup_idle(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);

static int ad4_ipc_suspend_setup_run(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_ipc_suspend_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_ipc_resume_setup_ipcs(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_ipc_reset_setup_startup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_ipc_reset_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);
static int ad4_cfg_ipc_reset(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);

static int ad4_vsync_update(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg);

static ad4_prop_setup prop_set_func[ad4_state_max][AD_PROPMAX] = {
	[ad4_state_idle][AD_MODE] = ad4_mode_setup_common,
	[ad4_state_idle][AD_INIT] = ad4_init_setup_idle,
	[ad4_state_idle][AD_CFG] = ad4_cfg_setup_idle,
	[ad4_state_idle][AD_INPUT] = ad4_input_setup_idle,
	[ad4_state_idle][AD_SUSPEND] = ad4_suspend_setup,
	[ad4_state_idle][AD_ASSERTIVE] = ad4_assertive_setup,
	[ad4_state_idle][AD_BACKLIGHT] = ad4_backlight_setup,
	[ad4_state_idle][AD_STRENGTH] = ad4_strength_setup_idle,
	[ad4_state_idle][AD_IPC_SUSPEND] = ad4_no_op_setup,
	[ad4_state_idle][AD_IPC_RESUME] = ad4_no_op_setup,
	[ad4_state_idle][AD_IPC_RESET] = ad4_no_op_setup,
	[ad4_state_idle][AD_VSYNC_UPDATE] = ad4_no_op_setup,

	[ad4_state_startup][AD_MODE] = ad4_mode_setup_common,
	[ad4_state_startup][AD_INIT] = ad4_init_setup,
	[ad4_state_startup][AD_CFG] = ad4_cfg_setup,
	[ad4_state_startup][AD_INPUT] = ad4_input_setup,
	[ad4_state_startup][AD_SUSPEND] = ad4_suspend_setup,
	[ad4_state_startup][AD_ASSERTIVE] = ad4_assertive_setup,
	[ad4_state_startup][AD_BACKLIGHT] = ad4_backlight_setup,
	[ad4_state_startup][AD_IPC_SUSPEND] = ad4_no_op_setup,
	[ad4_state_startup][AD_STRENGTH] = ad4_no_op_setup,
	[ad4_state_startup][AD_IPC_RESUME] = ad4_no_op_setup,
	[ad4_state_startup][AD_IPC_RESET] = ad4_ipc_reset_setup_startup,
	[ad4_state_startup][AD_VSYNC_UPDATE] = ad4_vsync_update,

	[ad4_state_run][AD_MODE] = ad4_mode_setup_common,
	[ad4_state_run][AD_INIT] = ad4_init_setup_run,
	[ad4_state_run][AD_CFG] = ad4_cfg_setup_run,
	[ad4_state_run][AD_INPUT] = ad4_input_setup,
	[ad4_state_run][AD_SUSPEND] = ad4_suspend_setup,
	[ad4_state_run][AD_ASSERTIVE] = ad4_assertive_setup,
	[ad4_state_run][AD_BACKLIGHT] = ad4_backlight_setup,
	[ad4_state_run][AD_STRENGTH] = ad4_no_op_setup,
	[ad4_state_run][AD_IPC_SUSPEND] = ad4_ipc_suspend_setup_run,
	[ad4_state_run][AD_IPC_RESUME] = ad4_no_op_setup,
	[ad4_state_run][AD_IPC_RESET] = ad4_setup_debug,
	[ad4_state_run][AD_VSYNC_UPDATE] = ad4_vsync_update,

	[ad4_state_ipcs][AD_MODE] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_INIT] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_CFG] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_INPUT] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_SUSPEND] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_ASSERTIVE] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_BACKLIGHT] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_STRENGTH] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_IPC_SUSPEND] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_IPC_RESUME] = ad4_ipc_resume_setup_ipcs,
	[ad4_state_ipcs][AD_IPC_RESET] = ad4_no_op_setup,
	[ad4_state_ipcs][AD_VSYNC_UPDATE] = ad4_no_op_setup,

	[ad4_state_ipcr][AD_MODE] = ad4_mode_setup_common,
	[ad4_state_ipcr][AD_INIT] = ad4_init_setup_ipcr,
	[ad4_state_ipcr][AD_CFG] = ad4_cfg_setup_ipcr,
	[ad4_state_ipcr][AD_INPUT] = ad4_input_setup_ipcr,
	[ad4_state_ipcr][AD_SUSPEND] = ad4_suspend_setup,
	[ad4_state_ipcr][AD_ASSERTIVE] = ad4_assertive_setup_ipcr,
	[ad4_state_ipcr][AD_BACKLIGHT] = ad4_backlight_setup_ipcr,
	[ad4_state_ipcr][AD_STRENGTH] = ad4_no_op_setup,
	[ad4_state_ipcr][AD_IPC_SUSPEND] = ad4_ipc_suspend_setup_ipcr,
	[ad4_state_ipcr][AD_IPC_RESUME] = ad4_no_op_setup,
	[ad4_state_ipcr][AD_IPC_RESET] = ad4_ipc_reset_setup_ipcr,
	[ad4_state_ipcr][AD_VSYNC_UPDATE] = ad4_no_op_setup,

	[ad4_state_manual][AD_MODE] = ad4_mode_setup_common,
	[ad4_state_manual][AD_INIT] = ad4_init_setup,
	[ad4_state_manual][AD_CFG] = ad4_cfg_setup,
	[ad4_state_manual][AD_INPUT] = ad4_no_op_setup,
	[ad4_state_manual][AD_SUSPEND] = ad4_no_op_setup,
	[ad4_state_manual][AD_ASSERTIVE] = ad4_no_op_setup,
	[ad4_state_manual][AD_BACKLIGHT] = ad4_no_op_setup,
	[ad4_state_manual][AD_STRENGTH] = ad4_strength_setup,
	[ad4_state_manual][AD_IPC_SUSPEND] = ad4_no_op_setup,
	[ad4_state_manual][AD_IPC_RESUME] = ad4_no_op_setup,
	[ad4_state_manual][AD_IPC_RESET] = ad4_setup_debug_manual,
	[ad4_state_manual][AD_VSYNC_UPDATE] = ad4_no_op_setup,
};

struct ad4_info {
	enum ad4_state state;
	u32 completed_ops_mask;
	bool ad4_support;
	enum ad4_modes mode;
	bool is_master;
	u32 last_assertive;
	u32 cached_assertive;
	u64 last_als;
	u64 cached_als;
	u64 last_bl;
	u64 cached_bl;
	u32 last_str;
	u32 frame_count;
	u32 frmt_mode;
	u32 irdx_control_0;
	u32 tf_ctrl;
	u32 vc_control_0;
	u32 frame_pushes;
};

static struct ad4_info info[DSPP_MAX] = {
	[DSPP_0] = {ad4_state_idle, 0, true, AD4_OFF, false, 0x80, 0x80},
	[DSPP_1] = {ad4_state_idle, 0, true, AD4_OFF, false, 0x80, 0x80},
	[DSPP_2] = {ad4_state_max, 0, false, AD4_OFF, false, 0x80, 0x80},
	[DSPP_3] = {ad4_state_max, 0, false, AD4_OFF, false, 0x80, 0x80},
};

void sde_setup_dspp_ad4(struct sde_hw_dspp *dspp, void *ad_cfg)
{
	int ret = 0;
	struct sde_ad_hw_cfg *cfg = ad_cfg;

	ret = ad4_params_check(dspp, ad_cfg);
	if (ret)
		return;

	ret = prop_set_func[info[dspp->idx].state][cfg->prop](dspp, ad_cfg);
	if (ret)
		DRM_ERROR("op failed %d ret %d\n", cfg->prop, ret);
}

int sde_validate_dspp_ad4(struct sde_hw_dspp *dspp, u32 *prop)
{

	if (!dspp || !prop) {
		DRM_ERROR("invalid params dspp %pK prop %pK\n", dspp, prop);
		return -EINVAL;
	}

	if (*prop >= AD_PROPMAX) {
		DRM_ERROR("invalid prop set %d\n", *prop);
		return -EINVAL;
	}

	if (dspp->idx >= DSPP_MAX || !info[dspp->idx].ad4_support) {
		DRM_ERROR("ad4 not supported for dspp idx %d\n", dspp->idx);
		return -EINVAL;
	}

	return 0;
}

static int ad4_params_check(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	struct sde_hw_mixer *hw_lm;

	if (!dspp || !cfg || !cfg->hw_cfg) {
		DRM_ERROR("invalid dspp %pK cfg %pk hw_cfg %pK\n",
			dspp, cfg, ((cfg) ? (cfg->hw_cfg) : NULL));
		return -EINVAL;
	}

	if (!cfg->hw_cfg->mixer_info) {
		DRM_ERROR("invalid mixed info\n");
		return -EINVAL;
	}

	if (dspp->idx >= DSPP_MAX || !info[dspp->idx].ad4_support) {
		DRM_ERROR("ad4 not supported for dspp idx %d\n", dspp->idx);
		return -EINVAL;
	}

	if (cfg->prop >= AD_PROPMAX) {
		DRM_ERROR("invalid prop set %d\n", cfg->prop);
		return -EINVAL;
	}

	if (info[dspp->idx].state >= ad4_state_max) {
		DRM_ERROR("in max state for dspp idx %d\n", dspp->idx);
		return -EINVAL;
	}

	if (!prop_set_func[info[dspp->idx].state][cfg->prop]) {
		DRM_ERROR("prop set not implemented for state %d prop %d\n",
				info[dspp->idx].state, cfg->prop);
		return -EINVAL;
	}

	if (!cfg->hw_cfg->num_of_mixers ||
	    cfg->hw_cfg->num_of_mixers > MAX_MIXERS_PER_CRTC) {
		DRM_ERROR("invalid mixer cnt %d\n",
				cfg->hw_cfg->num_of_mixers);
		return -EINVAL;
	}
	hw_lm = cfg->hw_cfg->mixer_info;
	if (!hw_lm) {
		DRM_ERROR("invalid mixer info\n");
		return -EINVAL;
	}

	if (cfg->hw_cfg->num_of_mixers == 1 &&
	    hw_lm->cfg.out_height != cfg->hw_cfg->displayv &&
	    hw_lm->cfg.out_width != cfg->hw_cfg->displayh) {
		DRM_ERROR("single_lm lmh %d lmw %d displayh %d displayw %d\n",
			hw_lm->cfg.out_height, hw_lm->cfg.out_width,
			cfg->hw_cfg->displayh, cfg->hw_cfg->displayv);
		return -EINVAL;
	} else if (hw_lm->cfg.out_height != cfg->hw_cfg->displayv &&
		   hw_lm->cfg.out_width != (cfg->hw_cfg->displayh >> 1)) {
		DRM_ERROR("dual_lm lmh %d lmw %d displayh %d displayw %d\n",
			hw_lm->cfg.out_height, hw_lm->cfg.out_width,
			cfg->hw_cfg->displayh, cfg->hw_cfg->displayv);
		return -EINVAL;
	}

	return 0;
}

static int ad4_no_op_setup(struct sde_hw_dspp *dspp, struct sde_ad_hw_cfg *cfg)
{
	return 0;
}

static int ad4_setup_debug(struct sde_hw_dspp *dspp, struct sde_ad_hw_cfg *cfg)
{
	u32 strength = 0;
	struct sde_hw_mixer *hw_lm;

	hw_lm = cfg->hw_cfg->mixer_info;
	if ((cfg->hw_cfg->num_of_mixers == 2) && hw_lm->cfg.right_mixer)
		/* this AD core is the salve core */
		return 0;

	strength = SDE_REG_READ(&dspp->hw, dspp->cap->sblk->ad.base + 0x4c);
	pr_debug("%s(): AD strength = %d\n", __func__, strength);

	return 0;
}

static int ad4_setup_debug_manual(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u32 strength = 0;
	struct sde_hw_mixer *hw_lm;

	hw_lm = cfg->hw_cfg->mixer_info;
	if ((cfg->hw_cfg->num_of_mixers == 2) && hw_lm->cfg.right_mixer)
		/* this AD core is the salve core */
		return 0;

	strength = SDE_REG_READ(&dspp->hw, dspp->cap->sblk->ad.base + 0x15c);
	pr_debug("%s(): AD strength = %d in manual mode\n", __func__, strength);

	return 0;
}

static int ad4_mode_setup(struct sde_hw_dspp *dspp, enum ad4_modes mode)
{
	u32 blk_offset;

	if (mode == AD4_OFF) {
		blk_offset = 0x04;
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
				0x101);
		info[dspp->idx].state = ad4_state_idle;
		pr_debug("%s(): AD state move to idle\n", __func__);
		info[dspp->idx].completed_ops_mask = 0;
		/* reset last values to register default */
		info[dspp->idx].last_assertive = 0x80;
		info[dspp->idx].cached_assertive = U8_MAX;
		info[dspp->idx].last_bl = 0xFFFF;
		info[dspp->idx].cached_bl = U64_MAX;
		info[dspp->idx].last_als = 0x0;
		info[dspp->idx].cached_als = U64_MAX;
	} else {
		if (mode == AD4_MANUAL) {
			/*vc_control_0 */
			blk_offset = 0x138;
			SDE_REG_WRITE(&dspp->hw,
				dspp->cap->sblk->ad.base + blk_offset, 0);
			/* irdx_control_0 */
			blk_offset = 0x13c;
			SDE_REG_WRITE(&dspp->hw,
				dspp->cap->sblk->ad.base + blk_offset,
				info[dspp->idx].irdx_control_0);
		}
		if (info[dspp->idx].state == ad4_state_idle) {
			if (mode == AD4_MANUAL) {
				info[dspp->idx].state = ad4_state_manual;
				pr_debug("%s(): AD state move to manual\n",
					__func__);
			} else {
				info[dspp->idx].frame_count = 0;
				info[dspp->idx].state = ad4_state_startup;
				pr_debug("%s(): AD state move to startup\n",
					__func__);
			}
		}
		blk_offset = 0x04;
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
				0x100);
	}

	return 0;
}

static int ad4_init_setup(struct sde_hw_dspp *dspp, struct sde_ad_hw_cfg *cfg)
{
	u32 frame_start, frame_end, proc_start, proc_end;
	struct sde_hw_mixer *hw_lm;
	u32 blk_offset, tile_ctl, val, i;
	u32 off1, off2, off3, off4, off5, off6;
	struct drm_msm_ad4_init *init;

	if (!cfg->hw_cfg->payload) {
		info[dspp->idx].completed_ops_mask &= ~ad4_init;
		return 0;
	}

	if (cfg->hw_cfg->len != sizeof(struct drm_msm_ad4_init)) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(struct drm_msm_ad4_init), cfg->hw_cfg->len,
			cfg->hw_cfg->payload);
		return -EINVAL;
	}

	hw_lm = cfg->hw_cfg->mixer_info;
	if (cfg->hw_cfg->num_of_mixers == 1) {
		frame_start = 0;
		frame_end = 0xffff;
		proc_start = 0;
		proc_end = 0xffff;
		tile_ctl = 0;
		info[dspp->idx].is_master = true;
	} else {
		tile_ctl = 0x5;
		if (hw_lm->cfg.right_mixer) {
			frame_start = (cfg->hw_cfg->displayh >> 1) -
				MERGE_WIDTH_RIGHT;
			frame_end = cfg->hw_cfg->displayh - 1;
			proc_start = (cfg->hw_cfg->displayh >> 1);
			proc_end = frame_end;
			tile_ctl |= 0x10;
			info[dspp->idx].is_master = false;
		} else {
			frame_start = 0;
			frame_end = (cfg->hw_cfg->displayh >> 1) +
				MERGE_WIDTH_LEFT;
			proc_start = 0;
			proc_end = (cfg->hw_cfg->displayh >> 1) - 1;
			tile_ctl |= 0x10;
			info[dspp->idx].is_master = true;
		}
	}

	init = cfg->hw_cfg->payload;

	info[dspp->idx].frmt_mode = (init->init_param_009 & (BIT(14) - 1));

	blk_offset = 0xc;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			init->init_param_010);

	init->init_param_012 = cfg->hw_cfg->displayv & (BIT(17) - 1);
	init->init_param_011 = cfg->hw_cfg->displayh & (BIT(17) - 1);
	blk_offset = 0x10;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			((init->init_param_011 << 16) | init->init_param_012));

	blk_offset = 0x14;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			tile_ctl);

	blk_offset = 0x44;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			((((init->init_param_013) & (BIT(17) - 1)) << 16) |
			(init->init_param_014 & (BIT(17) - 1))));

	blk_offset = 0x5c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_015 & (BIT(16) - 1)));
	blk_offset = 0x60;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_016 & (BIT(8) - 1)));
	blk_offset = 0x64;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_017 & (BIT(12) - 1)));
	blk_offset = 0x68;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_018 & (BIT(12) - 1)));
	blk_offset = 0x6c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_019 & (BIT(12) - 1)));
	blk_offset = 0x70;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_020 & (BIT(16) - 1)));
	blk_offset = 0x74;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_021 & (BIT(8) - 1)));
	blk_offset = 0x78;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_022 & (BIT(8) - 1)));
	blk_offset = 0x7c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_023 & (BIT(16) - 1)));
	blk_offset = 0x80;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		(((init->init_param_024 & (BIT(16) - 1)) << 16) |
		((init->init_param_025 & (BIT(16) - 1)))));
	blk_offset = 0x84;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		(((init->init_param_026 & (BIT(16) - 1)) << 16) |
		((init->init_param_027 & (BIT(16) - 1)))));

	blk_offset = 0x90;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_028 & (BIT(16) - 1)));
	blk_offset = 0x94;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_029 & (BIT(16) - 1)));

	blk_offset = 0x98;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		(((init->init_param_035 & (BIT(16) - 1)) << 16) |
		((init->init_param_030 & (BIT(16) - 1)))));

	blk_offset = 0x9c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		(((init->init_param_032 & (BIT(16) - 1)) << 16) |
		((init->init_param_031 & (BIT(16) - 1)))));
	blk_offset = 0xa0;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		(((init->init_param_034 & (BIT(16) - 1)) << 16) |
		((init->init_param_033 & (BIT(16) - 1)))));

	blk_offset = 0xb4;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_036 & (BIT(8) - 1)));
	blk_offset = 0xcc;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_037 & (BIT(8) - 1)));
	blk_offset = 0xc0;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_038 & (BIT(8) - 1)));
	blk_offset = 0xd8;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_039 & (BIT(8) - 1)));

	blk_offset = 0xe8;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_040 & (BIT(16) - 1)));

	blk_offset = 0xf4;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_041 & (BIT(8) - 1)));

	blk_offset = 0x100;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_042 & (BIT(16) - 1)));

	blk_offset = 0x10c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_043 & (BIT(8) - 1)));

	blk_offset = 0x120;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_044 & (BIT(16) - 1)));
	blk_offset = 0x124;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_045 & (BIT(16) - 1)));

	blk_offset = 0x128;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_046 & (BIT(1) - 1)));
	blk_offset = 0x12c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_047 & (BIT(8) - 1)));

	info[dspp->idx].irdx_control_0 = (init->init_param_048 & (BIT(5) - 1));

	blk_offset = 0x140;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_049 & (BIT(8) - 1)));

	blk_offset = 0x144;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_050 & (BIT(8) - 1)));
	blk_offset = 0x148;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		(((init->init_param_051 & (BIT(8) - 1)) << 8) |
		((init->init_param_052 & (BIT(8) - 1)))));

	blk_offset = 0x14c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_053 & (BIT(10) - 1)));
	blk_offset = 0x150;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_054 & (BIT(10) - 1)));
	blk_offset = 0x154;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_055 & (BIT(8) - 1)));

	blk_offset = 0x158;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_056 & (BIT(8) - 1)));
	blk_offset = 0x164;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_057 & (BIT(8) - 1)));
	blk_offset = 0x168;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_058 & (BIT(4) - 1)));

	blk_offset = 0x17c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(frame_start & (BIT(16) - 1)));
	blk_offset = 0x180;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(frame_end & (BIT(16) - 1)));
	blk_offset = 0x184;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(proc_start & (BIT(16) - 1)));
	blk_offset = 0x188;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(proc_end & (BIT(16) - 1)));

	blk_offset = 0x18c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_059 & (BIT(4) - 1)));

	blk_offset = 0x190;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		(((init->init_param_061 & (BIT(8) - 1)) << 8) |
		((init->init_param_060 & (BIT(8) - 1)))));

	blk_offset = 0x194;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_062 & (BIT(10) - 1)));

	blk_offset = 0x1a0;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_063 & (BIT(10) - 1)));
	blk_offset = 0x1a4;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_064 & (BIT(10) - 1)));
	blk_offset = 0x1a8;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_065 & (BIT(10) - 1)));
	blk_offset = 0x1ac;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_066 & (BIT(8) - 1)));
	blk_offset = 0x1b0;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_067 & (BIT(8) - 1)));
	blk_offset = 0x1b4;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_068 & (BIT(6) - 1)));

	blk_offset = 0x460;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_069 & (BIT(16) - 1)));
	blk_offset = 0x464;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_070 & (BIT(10) - 1)));
	blk_offset = 0x468;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_071 & (BIT(10) - 1)));
	blk_offset = 0x46c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_072 & (BIT(10) - 1)));
	blk_offset = 0x470;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_073 & (BIT(8) - 1)));
	blk_offset = 0x474;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_074 & (BIT(10) - 1)));
	blk_offset = 0x478;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(init->init_param_075 & (BIT(10) - 1)));

	off1 = 0x1c0;
	off2 = 0x210;
	off3 = 0x260;
	off4 = 0x2b0;
	off5 = 0x380;
	off6 = 0x3d0;
	for (i = 0; i < AD4_LUT_GRP0_SIZE - 1; i = i + 2) {
		val = (init->init_param_001[i] & (BIT(16) - 1));
		val |= ((init->init_param_001[i + 1] & (BIT(16) - 1))
				<< 16);
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off1, val);
		off1 += 4;

		val = (init->init_param_002[i] & (BIT(16) - 1));
		val |= ((init->init_param_002[i + 1] & (BIT(16) - 1))
				<< 16);
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off2, val);
		off2 += 4;

		val = (init->init_param_003[i] & (BIT(16) - 1));
		val |= ((init->init_param_003[i + 1] & (BIT(16) - 1))
				<< 16);
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off3, val);
		off3 += 4;

		val = (init->init_param_004[i] & (BIT(16) - 1));
		val |= ((init->init_param_004[i + 1] & (BIT(16) - 1))
				<< 16);
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off4, val);
		off4 += 4;

		val = (init->init_param_007[i] & (BIT(16) - 1));
		val |= ((init->init_param_007[i + 1] &
				(BIT(16) - 1)) << 16);
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off5, val);
		off5 += 4;

		val = (init->init_param_008[i] & (BIT(12) - 1));
		val |= ((init->init_param_008[i + 1] &
				(BIT(12) - 1)) << 16);
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off6, val);
		off6 += 4;
	}
	/* write last index data */
	i = AD4_LUT_GRP0_SIZE - 1;
	val = ((init->init_param_001[i] & (BIT(16) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off1, val);
	val = ((init->init_param_002[i] & (BIT(16) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off2, val);
	val = ((init->init_param_003[i] & (BIT(16) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off3, val);
	val = ((init->init_param_004[i] & (BIT(16) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off4, val);
	val = ((init->init_param_007[i] & (BIT(16) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off5, val);
	val = ((init->init_param_008[i] & (BIT(12) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off6, val);

	off1 = 0x300;
	off2 = 0x340;
	for (i = 0; i < AD4_LUT_GRP1_SIZE; i = i + 2) {
		val = (init->init_param_005[i] & (BIT(16) - 1));
		val |= ((init->init_param_005[i + 1] &
				(BIT(16) - 1)) << 16);
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off1, val);
		off1 += 4;

		val = (init->init_param_006[i] & (BIT(16) - 1));
		val |= ((init->init_param_006[i + 1] & (BIT(16) - 1))
				<< 16);
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + off2, val);
		off2 += 4;
	}

	return 0;
}

static int ad4_cfg_setup(struct sde_hw_dspp *dspp, struct sde_ad_hw_cfg *cfg)
{
	u32 blk_offset, val;
	struct drm_msm_ad4_cfg *ad_cfg;

	if (!cfg->hw_cfg->payload) {
		info[dspp->idx].completed_ops_mask &= ~ad4_cfg;
		return 0;
	}

	if (cfg->hw_cfg->len != sizeof(struct drm_msm_ad4_cfg)) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(struct drm_msm_ad4_cfg), cfg->hw_cfg->len,
			cfg->hw_cfg->payload);
		return -EINVAL;
	}
	ad_cfg = cfg->hw_cfg->payload;

	blk_offset = 0x18;
	val = (ad_cfg->cfg_param_002 & (BIT(16) - 1));
	val |= ((ad_cfg->cfg_param_001 & (BIT(16) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_004 & (BIT(16) - 1));
	val |= ((ad_cfg->cfg_param_003 & (BIT(16) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0x20;
	val = (ad_cfg->cfg_param_005 & (BIT(8) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset = 0x24;
	val = (ad_cfg->cfg_param_006 & (BIT(7) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	info[dspp->idx].tf_ctrl = (ad_cfg->cfg_param_008 & (BIT(8) - 1));

	blk_offset = 0x38;
	val = (ad_cfg->cfg_param_009 & (BIT(10) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0x3c;
	val = (ad_cfg->cfg_param_010 & (BIT(12) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = ((ad_cfg->cfg_param_011 & (BIT(16) - 1)) << 16);
	val |= (ad_cfg->cfg_param_012 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0x88;
	val = (ad_cfg->cfg_param_013 & (BIT(8) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_014 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0xa4;
	val = (ad_cfg->cfg_param_015 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_016 & (BIT(10) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_017 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_018 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0xc4;
	val = (ad_cfg->cfg_param_019 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_020 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0xb8;
	val = (ad_cfg->cfg_param_021 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_022 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0xd0;
	val = (ad_cfg->cfg_param_023 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_024 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0xdc;
	val = (ad_cfg->cfg_param_025 & (BIT(16) - 1));
	val |= ((ad_cfg->cfg_param_026 & (BIT(16) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_027 & (BIT(16) - 1));
	val |= ((ad_cfg->cfg_param_028 & (BIT(16) - 1)) << 16);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_029 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0xec;
	val = (ad_cfg->cfg_param_030 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_031 & (BIT(12) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0xf8;
	val = (ad_cfg->cfg_param_032 & (BIT(10) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_033 & (BIT(8) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0x104;
	val = (ad_cfg->cfg_param_034 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_035 & (BIT(12) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0x110;
	val = (ad_cfg->cfg_param_036 & (BIT(12) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_037 & (BIT(12) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_038 & (BIT(8) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_039 & (BIT(8) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0x134;
	val = (ad_cfg->cfg_param_040 & (BIT(12) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	info[dspp->idx].vc_control_0 = (ad_cfg->cfg_param_041 & (BIT(7) - 1));

	blk_offset = 0x160;
	val = (ad_cfg->cfg_param_043 & (BIT(10) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	blk_offset = 0x16c;
	val = (ad_cfg->cfg_param_044 & (BIT(8) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_045 & (BIT(8) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	blk_offset += 4;
	val = (ad_cfg->cfg_param_046 & (BIT(16) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);

	info[dspp->idx].frame_pushes = ad_cfg->cfg_param_047;

	return 0;
}

static int ad4_input_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u64 *val, als;
	u32 blk_offset;

	if (cfg->hw_cfg->len != sizeof(u64) && cfg->hw_cfg->payload) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(u64), cfg->hw_cfg->len, cfg->hw_cfg->payload);
		return -EINVAL;
	}

	blk_offset = 0x28;
	if (cfg->hw_cfg->payload) {
		val = cfg->hw_cfg->payload;
	} else {
		als = 0;
		val = &als;
	}
	info[dspp->idx].last_als = (*val & (BIT(16) - 1));
	info[dspp->idx].completed_ops_mask |= ad4_input;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			info[dspp->idx].last_als);
	return 0;
}

static int ad4_suspend_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	info[dspp->idx].state = ad4_state_idle;
	pr_debug("%s(): AD state move to idle\n", __func__);
	info[dspp->idx].completed_ops_mask = 0;
	return 0;
}

static int ad4_mode_setup_common(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{

	if (cfg->hw_cfg->len != sizeof(u64) || !cfg->hw_cfg->payload) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(u64), cfg->hw_cfg->len, cfg->hw_cfg->payload);
		return -EINVAL;
	}

	info[dspp->idx].mode = *((enum ad4_modes *)
					(cfg->hw_cfg->payload));
	info[dspp->idx].completed_ops_mask |= ad4_mode;

	if (AD_STATE_READY(info[dspp->idx].completed_ops_mask) ||
					info[dspp->idx].mode == AD4_OFF)
		ad4_mode_setup(dspp, info[dspp->idx].mode);

	return 0;
}

static int ad4_init_setup_idle(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	int ret;
	u32 blk_offset;

	if (!cfg->hw_cfg->payload) {
		info[dspp->idx].completed_ops_mask &= ~ad4_init;
		return 0;
	}

	ret = ad4_init_setup(dspp, cfg);
	if (ret)
		return ret;

	/* enable memory initialization*/
	/* frmt mode */
	blk_offset = 0x8;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(info[dspp->idx].frmt_mode & 0x1fff));
	/* memory init */
	blk_offset = 0x450;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, 0x1);

	/* enforce 0 initial strength when powering up AD config */
	/* irdx_control_0 */
	blk_offset = 0x13c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, 0x6);

	info[dspp->idx].completed_ops_mask |= ad4_init;

	if (AD_STATE_READY(info[dspp->idx].completed_ops_mask))
		ad4_mode_setup(dspp, info[dspp->idx].mode);

	return 0;
}

static int ad4_init_setup_run(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	int ret;
	u32 blk_offset;

	if (!cfg->hw_cfg->payload) {
		info[dspp->idx].completed_ops_mask &= ~ad4_init;
		return 0;
	}

	ret = ad4_init_setup(dspp, cfg);
	if (ret)
		return ret;

	/* disable memory initialization*/
	/* frmt mode */
	blk_offset = 0x8;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(info[dspp->idx].frmt_mode | 0x2000));
	/* no need to explicitly set memory initialization sequence,
	 * since AD hw were not powered off.
	 */

	/* irdx_control_0 */
	blk_offset = 0x13c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			info[dspp->idx].irdx_control_0);

	return 0;
}

static int ad4_init_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	int ret;
	u32 blk_offset;

	if (!cfg->hw_cfg->payload) {
		info[dspp->idx].completed_ops_mask &= ~ad4_init;
		return 0;
	}

	ret = ad4_init_setup(dspp, cfg);
	if (ret)
		return ret;
	/* no need to explicitly set memory initialization sequence,
	 * since register reset values are the correct configuration
	 */
	/* frmt mode */
	blk_offset = 0x8;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(info[dspp->idx].frmt_mode | 0x2000));
	/* irdx_control_0 */
	blk_offset = 0x13c;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			info[dspp->idx].irdx_control_0);

	info[dspp->idx].completed_ops_mask |= ad4_init;
	if (AD_STATE_READY(info[dspp->idx].completed_ops_mask))
		ad4_mode_setup(dspp, info[dspp->idx].mode);

	return 0;
}

static int ad4_cfg_setup_idle(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	int ret;
	u32 blk_offset;

	if (!cfg->hw_cfg->payload) {
		info[dspp->idx].completed_ops_mask &= ~ad4_cfg;
		return 0;
	}

	ret = ad4_cfg_setup(dspp, cfg);
	if (ret)
		return ret;

	/* enforce 0 initial strength when powering up AD config */
	/* assertiveness */
	blk_offset = 0x30;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, 0x0);
	/* tf control */
	blk_offset = 0x34;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, 0x55);

	/* vc_control_0 */
	blk_offset = 0x138;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		info[dspp->idx].vc_control_0);

	info[dspp->idx].completed_ops_mask |= ad4_cfg;
	if (AD_STATE_READY(info[dspp->idx].completed_ops_mask))
		ad4_mode_setup(dspp, info[dspp->idx].mode);
	return 0;
}

static int ad4_cfg_setup_run(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	int ret;
	u32 blk_offset;

	if (!cfg->hw_cfg->payload) {
		info[dspp->idx].completed_ops_mask &= ~ad4_cfg;
		return 0;
	}

	ret = ad4_cfg_setup(dspp, cfg);
	if (ret)
		return ret;

	/* assertiveness */
	blk_offset = 0x30;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			info[dspp->idx].last_assertive);
	/* tf control */
	blk_offset = 0x34;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		info[dspp->idx].tf_ctrl);
	/* vc_control_0 */
	blk_offset = 0x138;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		info[dspp->idx].vc_control_0);

	return 0;
}

static int ad4_cfg_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	int ret;
	u32 blk_offset;

	if (!cfg->hw_cfg->payload) {
		info[dspp->idx].completed_ops_mask &= ~ad4_cfg;
		return 0;
	}

	ret = ad4_cfg_setup(dspp, cfg);
	if (ret)
		return ret;

	/* assertiveness */
	blk_offset = 0x30;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			info[dspp->idx].last_assertive);

	info[dspp->idx].completed_ops_mask |= ad4_cfg;
	if (AD_STATE_READY(info[dspp->idx].completed_ops_mask))
		ad4_mode_setup(dspp, info[dspp->idx].mode);
	return 0;
}

static int ad4_input_setup_idle(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	int ret;

	ret = ad4_input_setup(dspp, cfg);
	if (ret)
		return ret;

	info[dspp->idx].completed_ops_mask |= ad4_input;
	if (AD_STATE_READY(info[dspp->idx].completed_ops_mask))
		ad4_mode_setup(dspp, info[dspp->idx].mode);

	return 0;
}

static int ad4_input_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u64 *val, als;
	u32 blk_offset;

	if (cfg->hw_cfg->len != sizeof(u64) && cfg->hw_cfg->payload) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(u64), cfg->hw_cfg->len, cfg->hw_cfg->payload);
		return -EINVAL;
	}

	blk_offset = 0x28;
	if (cfg->hw_cfg->payload) {
		val = cfg->hw_cfg->payload;
	} else {
		als = 0;
		val = &als;
	}
	info[dspp->idx].cached_als = *val & (BIT(16) - 1);
	info[dspp->idx].completed_ops_mask |= ad4_input;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			info[dspp->idx].last_als);

	if (AD_STATE_READY(info[dspp->idx].completed_ops_mask))
		ad4_mode_setup(dspp, info[dspp->idx].mode);

	return 0;
}

static int ad4_assertive_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u64 *val, assertive;
	u32 blk_offset;

	if (cfg->hw_cfg->len != sizeof(u64) && cfg->hw_cfg->payload) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(u64), cfg->hw_cfg->len, cfg->hw_cfg->payload);
		return -EINVAL;
	}

	blk_offset = 0x30;
	if (cfg->hw_cfg->payload) {
		val = cfg->hw_cfg->payload;
	} else {
		assertive = 0;
		val = &assertive;
	}

	info[dspp->idx].last_assertive = *val & (BIT(8) - 1);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			(info[dspp->idx].last_assertive));
	return 0;
}

static int ad4_assertive_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u64 *val, assertive;
	u32 blk_offset;

	if (cfg->hw_cfg->len != sizeof(u64) && cfg->hw_cfg->payload) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(u64), cfg->hw_cfg->len, cfg->hw_cfg->payload);
		return -EINVAL;
	}

	blk_offset = 0x30;
	if (cfg->hw_cfg->payload) {
		val = cfg->hw_cfg->payload;
	} else {
		assertive = 0;
		val = &assertive;
	}

	info[dspp->idx].cached_assertive = *val & (BIT(8) - 1);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			info[dspp->idx].last_assertive);

	return 0;
}

static int ad4_backlight_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u64 *val, bl;
	u32 blk_offset;

	if (cfg->hw_cfg->len != sizeof(u64) && cfg->hw_cfg->payload) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(u64), cfg->hw_cfg->len, cfg->hw_cfg->payload);
		return -EINVAL;
	}

	blk_offset = 0x2c;
	if (cfg->hw_cfg->payload) {
		val = cfg->hw_cfg->payload;
	} else {
		bl = 0;
		val = &bl;
	}

	info[dspp->idx].last_bl = *val & (BIT(16) - 1);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			info[dspp->idx].last_bl);
	return 0;
}

static int ad4_backlight_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u64 *val, bl;
	u32 blk_offset;

	if (cfg->hw_cfg->len != sizeof(u64) && cfg->hw_cfg->payload) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(u64), cfg->hw_cfg->len, cfg->hw_cfg->payload);
		return -EINVAL;
	}

	blk_offset = 0x2c;
	if (cfg->hw_cfg->payload) {
		val = cfg->hw_cfg->payload;
	} else {
		bl = 0;
		val = &bl;
	}

	info[dspp->idx].cached_bl = *val & (BIT(16) - 1);
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
			info[dspp->idx].last_bl);

	return 0;
}

void sde_read_intr_resp_ad4(struct sde_hw_dspp *dspp, u32 event,
		u32 *resp_in, u32 *resp_out)
{
	if (!dspp || !resp_in || !resp_out) {
		DRM_ERROR("invalid params dspp %pK resp_in %pK resp_out %pK\n",
				dspp, resp_in, resp_out);
		return;
	}

	switch (event) {
	case AD4_IN_OUT_BACKLIGHT:
		*resp_in = SDE_REG_READ(&dspp->hw,
				dspp->cap->sblk->ad.base + 0x2c);
		*resp_out = SDE_REG_READ(&dspp->hw,
				dspp->cap->sblk->ad.base + 0x48);
		pr_debug("%s(): AD4 input BL %u, output BL %u\n", __func__,
			(*resp_in), (*resp_out));
		break;
	default:
		break;
	}
}

static int ad4_ipc_suspend_setup_run(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u32 strength = 0, i = 0;
	struct sde_hw_mixer *hw_lm;

	hw_lm = cfg->hw_cfg->mixer_info;
	if ((cfg->hw_cfg->num_of_mixers == 2) && hw_lm->cfg.right_mixer) {
		/* this AD core is the salve core */
		for (i = DSPP_0; i < DSPP_MAX; i++) {
			if (info[i].is_master) {
				strength = info[i].last_str;
				break;
			}
		}
	} else {
		strength = SDE_REG_READ(&dspp->hw,
				dspp->cap->sblk->ad.base + 0x4c);
		pr_debug("%s(): AD strength = %d\n", __func__, strength);
	}
	info[dspp->idx].last_str = strength;
	info[dspp->idx].state = ad4_state_ipcs;
	pr_debug("%s(): AD state move to ipcs\n", __func__);

	return 0;
}

static int ad4_ipc_resume_setup_ipcs(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u32 blk_offset, val;

	info[dspp->idx].frame_count = 0;
	info[dspp->idx].state = ad4_state_ipcr;
	pr_debug("%s(): AD state move to ipcr\n", __func__);

	/* no need to rewrite frmt_mode bit 13 and mem_init,
	 * since the default register values are exactly what
	 * we wanted.
	 */

	/* ipc resume with manual strength */
	/* tf control */
	blk_offset = 0x34;
	val = (0x55 & (BIT(8) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	/* set manual strength */
	blk_offset = 0x15c;
	val = (info[dspp->idx].last_str & (BIT(10) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	/* enable manual mode */
	blk_offset = 0x138;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, 0);

	return 0;
}

static int ad4_ipc_suspend_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	info[dspp->idx].state = ad4_state_ipcs;
	pr_debug("%s(): AD state move to ipcs\n", __func__);
	return 0;
}

static int ad4_ipc_reset_setup_ipcr(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	int ret;
	u32 strength = 0, i = 0;
	struct sde_hw_mixer *hw_lm;

	/* Read AD calculator strength output during the 2 frames of manual
	 * strength mode, and assign the strength output to last_str
	 * when frame count reaches AD_IPC_FRAME_COUNT to avoid flickers
	 * caused by strength was not converged before entering IPC mode
	 */
	hw_lm = cfg->hw_cfg->mixer_info;
	if ((cfg->hw_cfg->num_of_mixers == 2) && hw_lm->cfg.right_mixer) {
		/* this AD core is the salve core */
		for (i = DSPP_0; i < DSPP_MAX; i++) {
			if (info[i].is_master) {
				strength = info[i].last_str;
				break;
			}
		}
	} else {
		strength = SDE_REG_READ(&dspp->hw,
				dspp->cap->sblk->ad.base + 0x4c);
		pr_debug("%s(): AD strength = %d\n", __func__, strength);
	}

	if (info[dspp->idx].frame_count == AD_IPC_FRAME_COUNT) {
		info[dspp->idx].state = ad4_state_run;
		pr_debug("%s(): AD state move to run\n", __func__);
		info[dspp->idx].last_str = strength;
		ret = ad4_cfg_ipc_reset(dspp, cfg);
		if (ret)
			return ret;
	} else {
		info[dspp->idx].frame_count++;
	}

	return 0;
}

static int ad4_cfg_ipc_reset(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u32 blk_offset;

	/* revert manual strength */
	/* tf control */
	blk_offset = 0x34;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		info[dspp->idx].tf_ctrl);
	/* vc_control_0 */
	blk_offset = 0x138;
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
		info[dspp->idx].vc_control_0);

	/* reset cached ALS, backlight and assertiveness */
	if (info[dspp->idx].cached_als != U64_MAX) {
		SDE_REG_WRITE(&dspp->hw,
				dspp->cap->sblk->ad.base + 0x28,
				info[dspp->idx].cached_als);
		info[dspp->idx].last_als = info[dspp->idx].cached_als;
		info[dspp->idx].cached_als = U64_MAX;
	}
	if (info[dspp->idx].cached_bl != U64_MAX) {
		SDE_REG_WRITE(&dspp->hw,
				dspp->cap->sblk->ad.base + 0x2c,
				info[dspp->idx].cached_bl);
		info[dspp->idx].last_bl = info[dspp->idx].cached_bl;
		info[dspp->idx].cached_bl = U64_MAX;
	}
	if (info[dspp->idx].cached_assertive != U8_MAX) {
		SDE_REG_WRITE(&dspp->hw,
				dspp->cap->sblk->ad.base + 0x30,
				info[dspp->idx].cached_assertive);
		info[dspp->idx].last_assertive =
				info[dspp->idx].cached_assertive;
		info[dspp->idx].cached_assertive = U8_MAX;
	}

	return 0;
}

static int ad4_ipc_reset_setup_startup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u32 blk_offset;

	if (info[dspp->idx].frame_count == AD_IPC_FRAME_COUNT) {
		info[dspp->idx].state = ad4_state_run;
		pr_debug("%s(): AD state move to run\n", __func__);

		/* revert enforce 0 initial strength */
		/* irdx_control_0 */
		blk_offset = 0x13c;
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
				info[dspp->idx].irdx_control_0);
		/* assertiveness */
		blk_offset = 0x30;
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
				info[dspp->idx].last_assertive);
		/* tf control */
		blk_offset = 0x34;
		SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset,
				info[dspp->idx].tf_ctrl);
	} else {
		info[dspp->idx].frame_count++;
	}

	return 0;
}

static int ad4_strength_setup(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u64 strength = 0, val;
	u32 blk_offset = 0x15c;

	if (cfg->hw_cfg->len != sizeof(u64) && cfg->hw_cfg->payload) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(u64), cfg->hw_cfg->len, cfg->hw_cfg->payload);
		return -EINVAL;
	}

	if (cfg->hw_cfg->payload)
		strength = *((u64 *)cfg->hw_cfg->payload);
	else
		strength = 0;

	/* set manual strength */
	info[dspp->idx].completed_ops_mask |= ad4_strength;
	val = (strength & (BIT(10) - 1));
	SDE_REG_WRITE(&dspp->hw, dspp->cap->sblk->ad.base + blk_offset, val);
	return 0;
}

static int ad4_strength_setup_idle(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	int ret;

	ret = ad4_strength_setup(dspp, cfg);
	if (ret)
		return ret;

	if (AD_STATE_READY(info[dspp->idx].completed_ops_mask))
		ad4_mode_setup(dspp, info[dspp->idx].mode);
	return 0;
}

static int ad4_vsync_update(struct sde_hw_dspp *dspp,
		struct sde_ad_hw_cfg *cfg)
{
	u32 *count;
	struct sde_hw_mixer *hw_lm;

	if (cfg->hw_cfg->len != sizeof(u32) || !cfg->hw_cfg->payload) {
		DRM_ERROR("invalid sz param exp %zd given %d cfg %pK\n",
			sizeof(u32), cfg->hw_cfg->len, cfg->hw_cfg->payload);
		return -EINVAL;
	}

	count = (u32 *)(cfg->hw_cfg->payload);
	hw_lm = cfg->hw_cfg->mixer_info;

	if (hw_lm && !hw_lm->cfg.right_mixer &&
		(*count < info[dspp->idx].frame_pushes))
		(*count)++;

	return 0;
}
