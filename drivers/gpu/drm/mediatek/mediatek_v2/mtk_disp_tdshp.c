// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_disp_tdshp.h"

static DECLARE_WAIT_QUEUE_HEAD(g_tdshp_size_wq);
static bool g_tdshp_get_size_available;
static struct DISP_TDSHP_DISPLAY_SIZE g_tdshp_size;

// define spinlock here
static DEFINE_MUTEX(g_tdshp_global_lock);
static DEFINE_SPINLOCK(g_tdshp_clock_lock);

// It's a work around for no comp assigned in functions.
static struct mtk_ddp_comp *default_comp;
static struct mtk_ddp_comp *tdshp1_default_comp;

#define index_of_tdshp(module) ((module == DDP_COMPONENT_TDSHP0) ? 0 : 1)
#define DISP_TDSHP_HW_ENGINE_NUM (2)
static unsigned int g_tdshp_relay_value[DISP_TDSHP_HW_ENGINE_NUM] = { 0, 0 };
static struct DISP_TDSHP_REG *g_disp_tdshp_regs[DISP_TDSHP_HW_ENGINE_NUM] = { NULL };

static atomic_t g_tdshp_is_clock_on[DISP_TDSHP_HW_ENGINE_NUM] = { ATOMIC_INIT(0),
	ATOMIC_INIT(0)};

enum TDSHP_IOCTL_CMD {
	SET_TDSHP_REG,
	BYPASS_TDSHP,
};

struct mtk_disp_tdshp_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

struct mtk_disp_tdshp {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_tdshp_data *data;
};

static inline struct mtk_disp_tdshp *comp_to_disp_tdshp(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_tdshp, ddp_comp);
}

static int mtk_disp_tdshp_write_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	struct DISP_TDSHP_REG *disp_tdshp_regs;

	int ret = 0;
	int id = index_of_tdshp(comp->id);

	if (lock)
		mutex_lock(&g_tdshp_global_lock);

	disp_tdshp_regs = g_disp_tdshp_regs[id];
	if (disp_tdshp_regs == NULL) {
		DDPINFO("%s: table [%d] not initialized\n", __func__, id);
		ret = -EFAULT;
		goto thshp_write_reg_unlock;
	}

	pr_notice("tdshp_en: %x, tdshp_limit: %x, tdshp_ylev_256: %x",
			disp_tdshp_regs->tdshp_en, disp_tdshp_regs->tdshp_limit,
			disp_tdshp_regs->tdshp_ylev_256);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CFG, 0x2 | g_tdshp_relay_value[id], 0x11);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_00,
		(disp_tdshp_regs->tdshp_softcoring_gain << 0 |
				disp_tdshp_regs->tdshp_gain_high << 8 |
				disp_tdshp_regs->tdshp_gain_mid << 16 |
				disp_tdshp_regs->tdshp_ink_sel << 24 |
				disp_tdshp_regs->tdshp_bypass_high << 29 |
				disp_tdshp_regs->tdshp_bypass_mid << 30 |
				disp_tdshp_regs->tdshp_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_01,
		(disp_tdshp_regs->tdshp_limit_ratio << 0 |
				disp_tdshp_regs->tdshp_gain << 4 |
				disp_tdshp_regs->tdshp_coring_zero << 16 |
				disp_tdshp_regs->tdshp_coring_thr << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_02,
		(disp_tdshp_regs->tdshp_coring_value << 8 |
				disp_tdshp_regs->tdshp_bound << 16 |
				disp_tdshp_regs->tdshp_limit << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_03,
		(disp_tdshp_regs->tdshp_sat_proc << 0 |
				disp_tdshp_regs->tdshp_ac_lpf_coe << 8 |
				disp_tdshp_regs->tdshp_clip_thr << 16 |
				disp_tdshp_regs->tdshp_clip_ratio << 24 |
				disp_tdshp_regs->tdshp_clip_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_05,
		(disp_tdshp_regs->tdshp_ylev_p048 << 0 |
		disp_tdshp_regs->tdshp_ylev_p032 << 8 |
		disp_tdshp_regs->tdshp_ylev_p016 << 16 |
		disp_tdshp_regs->tdshp_ylev_p000 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_06,
		(disp_tdshp_regs->tdshp_ylev_p112 << 0 |
		disp_tdshp_regs->tdshp_ylev_p096 << 8 |
		disp_tdshp_regs->tdshp_ylev_p080 << 16 |
		disp_tdshp_regs->tdshp_ylev_p064 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_07,
		(disp_tdshp_regs->tdshp_ylev_p176 << 0 |
		disp_tdshp_regs->tdshp_ylev_p160 << 8 |
		disp_tdshp_regs->tdshp_ylev_p144 << 16 |
		disp_tdshp_regs->tdshp_ylev_p128 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_08,
		(disp_tdshp_regs->tdshp_ylev_p240 << 0 |
		disp_tdshp_regs->tdshp_ylev_p224 << 8 |
		disp_tdshp_regs->tdshp_ylev_p208 << 16 |
		disp_tdshp_regs->tdshp_ylev_p192 << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_09,
		(disp_tdshp_regs->tdshp_ylev_en << 14 |
		disp_tdshp_regs->tdshp_ylev_alpha << 16 |
		disp_tdshp_regs->tdshp_ylev_256 << 24), ~0);

	// PBC1
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_00,
		(disp_tdshp_regs->pbc1_radius_r << 0 |
		disp_tdshp_regs->pbc1_theta_r << 6 |
		disp_tdshp_regs->pbc1_rslope_1 << 12 |
		disp_tdshp_regs->pbc1_gain << 22 |
		disp_tdshp_regs->pbc1_lpf_en << 30 |
		disp_tdshp_regs->pbc1_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_01,
		(disp_tdshp_regs->pbc1_lpf_gain << 0 |
		disp_tdshp_regs->pbc1_tslope << 6 |
		disp_tdshp_regs->pbc1_radius_c << 16 |
		disp_tdshp_regs->pbc1_theta_c << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_02,
		(disp_tdshp_regs->pbc1_edge_slope << 0 |
		disp_tdshp_regs->pbc1_edge_thr << 8 |
		disp_tdshp_regs->pbc1_edge_en << 14 |
		disp_tdshp_regs->pbc1_conf_gain << 16 |
		disp_tdshp_regs->pbc1_rslope << 22), ~0);
	// PBC2
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_03,
		(disp_tdshp_regs->pbc2_radius_r << 0 |
		disp_tdshp_regs->pbc2_theta_r << 6 |
		disp_tdshp_regs->pbc2_rslope_1 << 12 |
		disp_tdshp_regs->pbc2_gain << 22 |
		disp_tdshp_regs->pbc2_lpf_en << 30 |
		disp_tdshp_regs->pbc2_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_04,
		(disp_tdshp_regs->pbc2_lpf_gain << 0 |
		disp_tdshp_regs->pbc2_tslope << 6 |
		disp_tdshp_regs->pbc2_radius_c << 16 |
		disp_tdshp_regs->pbc2_theta_c << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_05,
		(disp_tdshp_regs->pbc2_edge_slope << 0 |
		disp_tdshp_regs->pbc2_edge_thr << 8 |
		disp_tdshp_regs->pbc2_edge_en << 14 |
		disp_tdshp_regs->pbc2_conf_gain << 16 |
		disp_tdshp_regs->pbc2_rslope << 22), ~0);
	// PBC3
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_06,
		(disp_tdshp_regs->pbc3_radius_r << 0 |
		disp_tdshp_regs->pbc3_theta_r << 6 |
		disp_tdshp_regs->pbc3_rslope_1 << 12 |
		disp_tdshp_regs->pbc3_gain << 22 |
		disp_tdshp_regs->pbc3_lpf_en << 30 |
		disp_tdshp_regs->pbc3_en << 31), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_07,
		(disp_tdshp_regs->pbc3_lpf_gain << 0 |
		disp_tdshp_regs->pbc3_tslope << 6 |
		disp_tdshp_regs->pbc3_radius_c << 16 |
		disp_tdshp_regs->pbc3_theta_c << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_PBC_08,
		(disp_tdshp_regs->pbc3_edge_slope << 0 |
		disp_tdshp_regs->pbc3_edge_thr << 8 |
		disp_tdshp_regs->pbc3_edge_en << 14 |
		disp_tdshp_regs->pbc3_conf_gain << 16 |
		disp_tdshp_regs->pbc3_rslope << 22), ~0);

//#ifdef TDSHP_2_0
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_10,
		(disp_tdshp_regs->tdshp_mid_softlimit_ratio << 0 |
		disp_tdshp_regs->tdshp_mid_coring_zero << 16 |
		disp_tdshp_regs->tdshp_mid_coring_thr << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_11,
		(disp_tdshp_regs->tdshp_mid_softcoring_gain << 0 |
		disp_tdshp_regs->tdshp_mid_coring_value << 8 |
		disp_tdshp_regs->tdshp_mid_bound << 16 |
		disp_tdshp_regs->tdshp_mid_limit << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_12,
		(disp_tdshp_regs->tdshp_high_softlimit_ratio << 0 |
		disp_tdshp_regs->tdshp_high_coring_zero << 16 |
		disp_tdshp_regs->tdshp_high_coring_thr << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_TDSHP_13,
		(disp_tdshp_regs->tdshp_high_softcoring_gain << 0 |
		disp_tdshp_regs->tdshp_high_coring_value << 8 |
		disp_tdshp_regs->tdshp_high_bound << 16 |
		disp_tdshp_regs->tdshp_high_limit << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_00,
		(disp_tdshp_regs->edf_clip_ratio_inc << 0 |
		disp_tdshp_regs->edf_edge_gain << 8 |
		disp_tdshp_regs->edf_detail_gain << 16 |
		disp_tdshp_regs->edf_flat_gain << 24 |
		disp_tdshp_regs->edf_gain_en << 31), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_01,
		(disp_tdshp_regs->edf_edge_th << 0 |
		disp_tdshp_regs->edf_detail_fall_th << 9 |
		disp_tdshp_regs->edf_detail_rise_th << 18 |
		disp_tdshp_regs->edf_flat_th << 25), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_02,
		(disp_tdshp_regs->edf_edge_slope << 0 |
		disp_tdshp_regs->edf_detail_fall_slope << 8 |
		disp_tdshp_regs->edf_detail_rise_slope << 16 |
		disp_tdshp_regs->edf_flat_slope << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_03,
		(disp_tdshp_regs->edf_edge_mono_slope << 0 |
		disp_tdshp_regs->edf_edge_mono_th << 8 |
		disp_tdshp_regs->edf_edge_mag_slope << 16 |
		disp_tdshp_regs->edf_edge_mag_th << 24), 0xFFFFFFFF);

	// DISP TDSHP no DISP_EDF_GAIN_04
	//cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_04,
	//	(disp_tdshp_regs->edf_edge_trend_flat_mag << 8 |
	//	disp_tdshp_regs->edf_edge_trend_slope << 16 |
	//	disp_tdshp_regs->edf_edge_trend_th << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_EDF_GAIN_05,
		(disp_tdshp_regs->edf_bld_wgt_mag << 0 |
		disp_tdshp_regs->edf_bld_wgt_mono << 8), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_C_BOOST_MAIN,
		(disp_tdshp_regs->tdshp_cboost_gain << 0 |
		disp_tdshp_regs->tdshp_cboost_en << 13 |
		disp_tdshp_regs->tdshp_cboost_lmt_l << 16 |
		disp_tdshp_regs->tdshp_cboost_lmt_u << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_C_BOOST_MAIN_2,
		(disp_tdshp_regs->tdshp_cboost_yoffset << 0 |
		disp_tdshp_regs->tdshp_cboost_yoffset_sel << 16 |
		disp_tdshp_regs->tdshp_cboost_yconst << 24), 0xFFFFFFFF);

//#endif // TDSHP_2_0

//#ifdef TDSHP_3_0
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_00,
		(disp_tdshp_regs->tdshp_post_ylev_p048 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p032 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p016 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p000 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_01,
		(disp_tdshp_regs->tdshp_post_ylev_p112 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p096 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p080 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p064 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_02,
		(disp_tdshp_regs->tdshp_post_ylev_p176 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p160 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p144 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p128 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_03,
		(disp_tdshp_regs->tdshp_post_ylev_p240 << 0 |
		disp_tdshp_regs->tdshp_post_ylev_p224 << 8 |
		disp_tdshp_regs->tdshp_post_ylev_p208 << 16 |
		disp_tdshp_regs->tdshp_post_ylev_p192 << 24), 0xFFFFFFFF);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_POST_YLEV_04,
		(disp_tdshp_regs->tdshp_post_ylev_en << 14 |
		disp_tdshp_regs->tdshp_post_ylev_alpha << 16 |
		disp_tdshp_regs->tdshp_post_ylev_256 << 24), 0xFFFFFFFF);
//#endif // TDSHP_3_0

thshp_write_reg_unlock:
	if (lock)
		mutex_unlock(&g_tdshp_global_lock);

	return ret;
}

static int mtk_disp_tdshp_set_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_TDSHP_REG *user_tdshp_regs)
{
	int ret = 0;
	int id;
	struct DISP_TDSHP_REG *tdshp_regs, *old_tdshp_regs;

	pr_notice("%s\n", __func__);

	tdshp_regs = kmalloc(sizeof(struct DISP_TDSHP_REG), GFP_KERNEL);
	if (tdshp_regs == NULL) {
		DDPPR_ERR("%s: no memory\n", __func__);
		return -EFAULT;
	}

	if (user_tdshp_regs == NULL) {
		ret = -EFAULT;
		kfree(tdshp_regs);
	} else {
		memcpy(tdshp_regs, user_tdshp_regs,
			sizeof(struct DISP_TDSHP_REG));
		id = index_of_tdshp(comp->id);

		if (id >= 0 && id < 2) {
			mutex_lock(&g_tdshp_global_lock);

			old_tdshp_regs = g_disp_tdshp_regs[id];
			g_disp_tdshp_regs[id] = tdshp_regs;

			pr_notice("%s: Set module(%d) lut\n", __func__, comp->id);
			ret = mtk_disp_tdshp_write_reg(comp, handle, 0);

			mutex_unlock(&g_tdshp_global_lock);

			if (old_tdshp_regs != NULL)
				kfree(old_tdshp_regs);
		} else {
			DDPPR_ERR("%s: invalid ID = %d\n", __func__, comp->id);
			ret = -EFAULT;
		}
	}

	return ret;
}

static int disp_tdshp_wait_size(unsigned long timeout)
{
	int ret = 0;

	if (g_tdshp_get_size_available == false) {
		ret = wait_event_interruptible(g_tdshp_size_wq,
			g_tdshp_get_size_available == true);

		DDPINFO("size_available = 1, Wake up, ret = %d\n", ret);
	} else {
		DDPINFO("size_available = 0\n");
	}

	return ret;
}

int mtk_drm_ioctl_tdshp_set_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_TDSHP0];
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_crtc_user_cmd(crtc, comp, SET_TDSHP_REG, data);
}

int mtk_drm_ioctl_tdshp_get_size(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	u32 width = 0, height = 0;
	struct DISP_TDSHP_DISPLAY_SIZE *dst =
			(struct DISP_TDSHP_DISPLAY_SIZE *)data;

	pr_notice("%s", __func__);

	crtc = list_first_entry(&(dev)->mode_config.crtc_list,
		typeof(*crtc), head);

	mtk_drm_crtc_get_panel_original_size(crtc, &width, &height);
	if (width == 0 || height == 0) {
		DDPFUNC("panel original size error(%dx%d).\n", width, height);
		width = crtc->mode.hdisplay;
		height = crtc->mode.vdisplay;
	}

	g_tdshp_size.lcm_width = width;
	g_tdshp_size.lcm_height = height;

	disp_tdshp_wait_size(60);

	pr_notice("%s ---", __func__);
	memcpy(dst, &g_tdshp_size, sizeof(g_tdshp_size));

	return 0;
}

static void mtk_disp_tdshp_config(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int width;
	unsigned int val;

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->source_bpc == 8)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CTRL, ((0x1 << 2) | 0x1), ~0);
	else if (cfg->source_bpc == 10)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CTRL, ((0x0 << 2) | 0x1), ~0);
	else
		DDPPR_ERR("%s: Invalid bpc: %u\n", __func__, cfg->bpc);

	if (comp->mtk_crtc->is_dual_pipe)
		width = cfg->w / 2;
	else
		width = cfg->w;

	val = (width << 16) | (cfg->h);

	DDPINFO("%s: 0x%08x\n", __func__, val);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_INPUT_SIZE, val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_OUTPUT_SIZE, val, ~0);
	// DISP_TDSHP_OUTPUT_OFFSET
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_OUTPUT_OFFSET, 0x0, ~0);

	g_tdshp_size.height = cfg->h;
	g_tdshp_size.width = cfg->w;
	if (g_tdshp_get_size_available == false) {
		g_tdshp_get_size_available = true;
		wake_up_interruptible(&g_tdshp_size_wq);
		pr_notice("size available: (w, h)=(%d, %d)+\n", width, cfg->h);
	}
}

static void mtk_disp_tdshp_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	pr_notice("%s, comp_id: %d, bypass: %d\n",
			__func__, index_of_tdshp(comp->id));

	if (bypass == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CFG, 0x1, 0x1);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_00, (0x1 << 31), (0x1 << 31));
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CTRL, 0xfffffffd, ~0);
		g_tdshp_relay_value[index_of_tdshp(comp->id)] = 0x1;
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_TDSHP_CFG, 0x0, 0x1);
		g_tdshp_relay_value[index_of_tdshp(comp->id)] = 0x0;
	}
}

static int mtk_disp_tdshp_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	unsigned int cmd, void *data)
{
	pr_notice("%s, cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case SET_TDSHP_REG:
	{
		struct DISP_TDSHP_REG *config = data;

		if (mtk_disp_tdshp_set_reg(comp, handle, config) < 0) {
			DDPPR_ERR("%s: failed\n", __func__);
			return -EFAULT;
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_tdshp1 = priv->ddp_comp[DDP_COMPONENT_TDSHP1];

			if (mtk_disp_tdshp_set_reg(comp_tdshp1, handle, config) < 0) {
				DDPPR_ERR("%s: comp_tdshp1 failed\n", __func__);
				return -EFAULT;
			}
		}

		mtk_crtc_check_trigger(comp->mtk_crtc, false, false);
	}
	break;
	case BYPASS_TDSHP:
	{
		unsigned int *value = data;

		mtk_disp_tdshp_bypass(comp, *value, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_tdshp1 = priv->ddp_comp[DDP_COMPONENT_TDSHP1];

			mtk_disp_tdshp_bypass(comp_tdshp1, *value, handle);
		}
	}
	break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static void mtk_disp_tdshp_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("line: %d\n", __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CTRL, DISP_TDSHP_EN, 0x1);
	mtk_disp_tdshp_write_reg(comp, handle, 0);
}

static void mtk_disp_tdshp_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("line: %d\n", __LINE__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_TDSHP_CTRL, 0x0, 0x1);
}

static void mtk_disp_tdshp_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_tdshp *tdshp = comp_to_disp_tdshp(comp);

	DDPINFO("id(%d)\n", comp->id);
	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&g_tdshp_is_clock_on[index_of_tdshp(comp->id)], 1);

	if (tdshp->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, TDSHP_BYPASS_SHADOW,
			DISP_TDSHP_SHADOW_CTRL, TDSHP_BYPASS_SHADOW);
}

static void mtk_disp_tdshp_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;

	DDPINFO("id(%d)\n", comp->id);
	spin_lock_irqsave(&g_tdshp_clock_lock, flags);
	DDPINFO("%s @ %d......... spin_trylock_irqsave -- ",
		__func__, __LINE__);
	atomic_set(&g_tdshp_is_clock_on[index_of_tdshp(comp->id)], 0);
	spin_unlock_irqrestore(&g_tdshp_clock_lock, flags);
	DDPINFO("%s @ %d......... spin_unlock_irqrestore ",
		__func__, __LINE__);
	mtk_ddp_comp_clk_unprepare(comp);
}

void mtk_disp_tdshp_first_cfg(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	pr_notice("%s\n", __func__);
	mtk_disp_tdshp_config(comp, cfg, handle);
}

static const struct mtk_ddp_comp_funcs mtk_disp_tdshp_funcs = {
	.config = mtk_disp_tdshp_config,
	.first_cfg = mtk_disp_tdshp_first_cfg,
	.start = mtk_disp_tdshp_start,
	.stop = mtk_disp_tdshp_stop,
	.bypass = mtk_disp_tdshp_bypass,
	.user_cmd = mtk_disp_tdshp_user_cmd,
	.prepare = mtk_disp_tdshp_prepare,
	.unprepare = mtk_disp_tdshp_unprepare,
};

static int mtk_disp_tdshp_bind(struct device *dev, struct device *master,
			void *data)
{
	struct mtk_disp_tdshp *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	pr_notice("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	pr_notice("%s-\n", __func__);
	return 0;
}

static void mtk_disp_tdshp_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_tdshp *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	pr_notice("%s+\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
	pr_notice("%s-\n", __func__);
}

static const struct component_ops mtk_disp_tdshp_component_ops = {
	.bind = mtk_disp_tdshp_bind,
	.unbind = mtk_disp_tdshp_unbind,
};

void mtk_disp_tdshp_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x4, 0x8, 0xC);
	mtk_cust_dump_reg(baddr, 0x14, 0x18, 0x1C, 0x20);
	mtk_cust_dump_reg(baddr, 0x24, 0x40, 0x44, 0x48);
	mtk_cust_dump_reg(baddr, 0x4C, 0x50, 0x54, 0x58);
	mtk_cust_dump_reg(baddr, 0x5C, 0x60, 0xE0, 0xE4);
	mtk_cust_dump_reg(baddr, 0xFC, 0x100, 0x104, 0x108);
	mtk_cust_dump_reg(baddr, 0x10C, 0x110, 0x114, 0x118);
	mtk_cust_dump_reg(baddr, 0x11C, 0x120, 0x124, 0x128);
	mtk_cust_dump_reg(baddr, 0x12C, 0x14C, 0x300, 0x304);
	mtk_cust_dump_reg(baddr, 0x308, 0x30C, 0x314, 0x320);
	mtk_cust_dump_reg(baddr, 0x324, 0x328, 0x32C, 0x330);
	mtk_cust_dump_reg(baddr, 0x334, 0x338, 0x33C, 0x340);
	mtk_cust_dump_reg(baddr, 0x344, 0x354, 0x358, 0x360);
	mtk_cust_dump_reg(baddr, 0x368, 0x36C, 0x374, 0x378);
	mtk_cust_dump_reg(baddr, 0x37C, 0x384, 0x388, 0x480);
	mtk_cust_dump_reg(baddr, 0x484, 0x488, 0x48C, 0x490);
	mtk_cust_dump_reg(baddr, 0x67C, -1, -1, -1);
}

static int mtk_disp_tdshp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_tdshp *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	pr_notice("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_TDSHP);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_tdshp_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	if (!default_comp && comp_id == DDP_COMPONENT_TDSHP0)
		default_comp = &priv->ddp_comp;
	if (!tdshp1_default_comp && comp_id == DDP_COMPONENT_TDSHP1)
		tdshp1_default_comp = &priv->ddp_comp;

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_tdshp_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	pr_notice("%s-\n", __func__);

	return ret;
}

static int mtk_disp_tdshp_remove(struct platform_device *pdev)
{
	struct mtk_disp_tdshp *priv = dev_get_drvdata(&pdev->dev);

	pr_notice("%s+\n", __func__);
	component_del(&pdev->dev, &mtk_disp_tdshp_component_ops);

	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	pr_notice("%s-\n", __func__);
	return 0;
}

static const struct mtk_disp_tdshp_data mt6983_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6895_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6879_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_tdshp_data mt6855_tdshp_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_tdshp_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6983-disp-tdshp",
	  .data = &mt6983_tdshp_driver_data},
	{ .compatible = "mediatek,mt6895-disp-tdshp",
	  .data = &mt6895_tdshp_driver_data},
	{ .compatible = "mediatek,mt6879-disp-tdshp",
	  .data = &mt6879_tdshp_driver_data},
	{ .compatible = "mediatek,mt6855-disp-tdshp",
	  .data = &mt6855_tdshp_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_tdshp_driver_dt_match);

struct platform_driver mtk_disp_tdshp_driver = {
	.probe = mtk_disp_tdshp_probe,
	.remove = mtk_disp_tdshp_remove,
	.driver = {
			.name = "mediatek-disp-tdshp",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_tdshp_driver_dt_match,
		},
};

void disp_tdshp_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;

	ret = mtk_crtc_user_cmd(crtc, default_comp, BYPASS_TDSHP, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}
