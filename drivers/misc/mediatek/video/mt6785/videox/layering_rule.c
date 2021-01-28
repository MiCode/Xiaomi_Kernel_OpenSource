/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#if defined(CONFIG_MTK_DRAMC)
#include "mtk_dramc.h"
#endif

#include "mmdvfs_pmqos.h"

#include "layering_rule.h"
#include "disp_drv_log.h"
#include "ddp_rsz.h"
#include "primary_display.h"
#include "disp_lowpower.h"
#include "mtk_disp_mgr.h"
#include "disp_rect.h"
#include "lcm_drv.h"
#include "ddp_ovl_wcg.h"
#include "ddp_ovl.h"


static struct layering_rule_ops l_rule_ops;
static struct layering_rule_info_t l_rule_info;

static DEFINE_SPINLOCK(hrt_table_lock);

/* To backup for primary display layer_config */
static struct layer_config *g_input_config;

int emi_bound_table[HRT_BOUND_NUM][HRT_LEVEL_NUM] = {
	/* HRT_BOUND_TYPE_LP4 */
	{100, 300, 500, 600},
};

int larb_bound_table[HRT_BOUND_NUM][HRT_LEVEL_NUM] = {
	/* HRT_BOUND_TYPE_LP4 */
	{100, 300, 500, 600},
};

/**
 * The layer mapping table define ovl layer dispatch rule for both
 * primary and secondary display.Each table has 16 elements which
 * represent the layer mapping rule by the number of input layers.
 */
#ifdef CONFIG_MTK_HIGH_FRAME_RATE /*todo: use lcm_height to decide table*/
static int layer_mapping_table[HRT_TB_NUM][TOTAL_OVL_LAYER_NUM] = {
	/* HRT_TB_TYPE_GENERAL */
	{0x00010001, 0x00030003, 0x00030007, 0x0003000F, 0x0003001F, 0x0003003F,
	0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F, 0x0003003C},
	/* HRT_TB_TYPE_RPO_L0 */
	{0x00010001, 0x00030005, 0x0003000D, 0x0003001D, 0x0003003D, 0x0003003D,
	0x0003003D, 0x0003003D, 0x0003003D, 0x0003003D, 0x0003003D, 0x0003003D},
	/* HRT_TB_TYPE_RPO_L0L1 */
	{0x00010001, 0x00030003, 0x00030007, 0x0003000F, 0x0003001F, 0x0003003F,
	0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F},
};
#else
static int layer_mapping_table[HRT_TB_NUM][TOTAL_OVL_LAYER_NUM] = {
	/* HRT_TB_TYPE_GENERAL */
	{0x00010001, 0x00030003, 0x00030007, 0x0003000F, 0x0003001F, 0x0003003F,
	0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F},
	/* HRT_TB_TYPE_RPO_L0 */
	{0x00010001, 0x00030005, 0x0003000D, 0x0003001D, 0x0003003D, 0x0003003D,
	0x0003003D, 0x0003003D, 0x0003003D, 0x0003003D, 0x0003003D, 0x0003003D},
	/* HRT_TB_TYPE_RPO_L0L1 */
	{0x00010001, 0x00030003, 0x00030007, 0x0003000F, 0x0003001F, 0x0003003F,
	0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F, 0x0003003F},
};
#endif

/**
 * The larb mapping table represent the relation between LARB and OVL.
 */
static int larb_mapping_table[HRT_TB_NUM] = {
	0x00010010, 0x00010010, 0x00010010,
};

/**
 * The OVL mapping table is used to get the OVL index of correcponding layer.
 * The bit value 1 means the position of the last layer in OVL engine.
 */
static int ovl_mapping_table[HRT_TB_NUM] = {
	0x00020022, 0x00020022, 0x00020022,
};

#define GET_SYS_STATE(sys_state) \
	((l_rule_info.hrt_sys_state >> sys_state) & 0x1)
static inline bool support_color_format(enum DISP_FORMAT src_fmt)
{
	switch (src_fmt) {
	case DISP_FORMAT_RGB565:
	case DISP_FORMAT_RGB888:
	case DISP_FORMAT_BGR888:
	case DISP_FORMAT_ARGB8888:
	case DISP_FORMAT_ABGR8888:
	case DISP_FORMAT_RGBA8888:
	case DISP_FORMAT_BGRA8888:
	case DISP_FORMAT_YUV422:
	case DISP_FORMAT_XRGB8888:
	case DISP_FORMAT_XBGR8888:
	case DISP_FORMAT_RGBX8888:
	case DISP_FORMAT_BGRX8888:
	case DISP_FORMAT_UYVY:
	case DISP_FORMAT_PARGB8888:
	case DISP_FORMAT_PABGR8888:
	case DISP_FORMAT_PRGBA8888:
	case DISP_FORMAT_PBGRA8888:
	case DISP_FORMAT_DIM:
		return true;
	default:
		return false;
	}

	return false;
}

static bool has_rsz_layer(struct disp_layer_info *disp_info, int disp_idx)
{
	int i = 0;
	struct layer_config *c = NULL;

	for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
		c = &disp_info->input_config[disp_idx][i];

		if (is_gles_layer(disp_info, disp_idx, i))
			continue;

		if ((c->src_height != c->dst_height) ||
		    (c->src_width != c->dst_width))
			return true;
	}

	return false;
}

static bool is_rsz_valid(struct layer_config *c)
{
	if (c->src_width == c->dst_width &&
	    c->src_height == c->dst_height)
		return false;
	if (c->src_width > c->dst_width ||
	    c->src_height > c->dst_height)
		return false;
	/*
	 * HWC adjusts MDP layer alignment after query_valid_layer.
	 * This makes the decision of layering rule unreliable. Thus we
	 * add constraint to avoid frame_cfg becoming scale-down.
	 *
	 * TODO: If HWC adjusts MDP layer alignment before
	 * query_valid_layer, we could remove this if statement.
	 */
	if ((has_layer_cap(c, MDP_RSZ_LAYER) ||
	     has_layer_cap(c, MDP_ROT_LAYER)) &&
	    (c->dst_width - c->src_width <= MDP_ALIGNMENT_MARGIN ||
	     c->dst_height - c->src_height <= MDP_ALIGNMENT_MARGIN))
		return false;

	return true;
}

static int is_same_ratio(struct layer_config *ref, struct layer_config *c)
{
	int diff_w, diff_h;

	if (!ref->dst_width || !ref->dst_height) {
		DISP_PR_ERR("%s:ref dst(%dx%d)\n", __func__,
			    ref->dst_width, ref->dst_height);
		return -EINVAL;
	}

	diff_w = (c->dst_width * ref->src_width + (ref->dst_width - 1))
			/ ref->dst_width - c->src_width;
	diff_h = (c->dst_height * ref->src_height + (ref->dst_height - 1))
			/ ref->dst_height - c->src_height;
	if (abs(diff_w) > 1 || abs(diff_h) > 1)
		return false;

	return true;
}

static bool is_RPO(struct disp_layer_info *disp_info, int disp_idx,
		   int *rsz_idx)
{
	int i = 0;
	struct layer_config *c = NULL;
	int gpu_rsz_idx = 0;
	struct layer_config *ref_layer = NULL;
	struct disp_rect src_layer_roi = { 0 };
	struct disp_rect dst_layer_roi = { 0 };
	struct disp_rect src_roi = { 0 };
	struct disp_rect dst_roi = { 0 };

	if (disp_info->layer_num[disp_idx] <= 0)
		return false;

	*rsz_idx = -1;
	for (i = 0; i < disp_info->layer_num[disp_idx] && i < 2; i++) {
		c = &disp_info->input_config[disp_idx][i];

		if (i == 0 && c->src_fmt == DISP_FORMAT_DIM)
			continue;

		if (disp_info->gles_head[disp_idx] >= 0 &&
		    disp_info->gles_head[disp_idx] <= i)
			break;

		if (!is_rsz_valid(c))
			break;

		if (!ref_layer)
			ref_layer = c;
		else if (is_same_ratio(ref_layer, c) <= 0)
			break;

		rect_make(&src_layer_roi,
			c->dst_offset_x * c->src_width / c->dst_width,
			c->dst_offset_y * c->src_height / c->dst_height,
			c->src_width, c->src_height);
		rect_make(&dst_layer_roi,
			c->dst_offset_x, c->dst_offset_y,
			c->dst_width, c->dst_height);
		rect_join(&src_layer_roi, &src_roi, &src_roi);
		rect_join(&dst_layer_roi, &dst_roi, &dst_roi);
		if (src_roi.width > dst_roi.width ||
		    src_roi.height > dst_roi.height) {
			DISP_PR_ERR(
				"L%d:scale down(%d,%d,%dx%d)->(%d,%d,%dx%d)\n",
				    i, src_roi.x, src_roi.y, src_roi.width,
				    src_roi.height, dst_roi.x, dst_roi.y,
				    dst_roi.width, dst_roi.height);
			break;
		}

		if (src_roi.width > RSZ_TILE_LENGTH - RSZ_ALIGNMENT_MARGIN ||
		    src_roi.height > RSZ_IN_MAX_HEIGHT)
			break;

		c->layer_caps |= DISP_RSZ_LAYER;
		*rsz_idx = i;
	}
	if (*rsz_idx == -1)
		return false;
	if (*rsz_idx == 2) {
		DISP_PR_ERR("%s:error: rsz layer idx >= 2\n", __func__);
		return false;
	}

	for (i = *rsz_idx + 1; i < disp_info->layer_num[disp_idx]; i++) {
		c = &disp_info->input_config[disp_idx][i];
		if (c->src_width != c->dst_width ||
		    c->src_height != c->dst_height) {
			if (has_layer_cap(c, MDP_RSZ_LAYER))
				continue;

			gpu_rsz_idx = i;
			break;
		}
	}

	if (gpu_rsz_idx)
		rollback_resize_layer_to_GPU_range(disp_info, disp_idx,
			gpu_rsz_idx, disp_info->layer_num[disp_idx] - 1);

	return true;
}

/* lr_rsz_layout - layering rule resize layer layout */
static bool lr_rsz_layout(struct disp_layer_info *disp_info)
{
	int disp_idx;

	if (is_ext_path(disp_info))
		rollback_all_resize_layer_to_GPU(disp_info, HRT_SECONDARY);

	for (disp_idx = 0; disp_idx < 2; disp_idx++) {
		int rsz_idx = 0;

		if (disp_info->layer_num[disp_idx] <= 0)
			continue;

		/* only support resize layer on Primary Display */
		if (disp_idx == HRT_SECONDARY)
			continue;

		if (!has_rsz_layer(disp_info, disp_idx)) {
			l_rule_info.scale_rate = HRT_SCALE_NONE;
			l_rule_info.disp_path = HRT_PATH_UNKNOWN;
		} else if (is_RPO(disp_info, disp_idx, &rsz_idx)) {
			if (rsz_idx == 0)
				l_rule_info.disp_path = HRT_PATH_RPO_L0;
			else if (rsz_idx == 1)
				l_rule_info.disp_path = HRT_PATH_RPO_L0L1;
			else
				DISP_PR_ERR("%s:RPO but rsz_idx(%d) error\n",
					    __func__, rsz_idx);
		} else {
			rollback_all_resize_layer_to_GPU(disp_info,
							 HRT_PRIMARY);
			l_rule_info.scale_rate = HRT_SCALE_NONE;
			l_rule_info.disp_path = HRT_PATH_UNKNOWN;
		}
	}

	return 0;
}

static bool
lr_unset_disp_rsz_attr(struct disp_layer_info *disp_info, int disp_idx)
{
	struct layer_config *c = &disp_info->input_config[disp_idx][0];

	if (l_rule_info.disp_path == HRT_PATH_RPO_L0 &&
	    has_layer_cap(c, MDP_RSZ_LAYER) &&
	    has_layer_cap(c, DISP_RSZ_LAYER)) {
		c->layer_caps &= ~DISP_RSZ_LAYER;
		l_rule_info.disp_path = HRT_PATH_GENERAL;
		l_rule_info.layer_tb_idx = HRT_TB_TYPE_GENERAL;
		return true;
	}
	return false;
}

static void lr_gpu_change_rsz_info(void)
{
}

static void layering_rule_senario_decision(struct disp_layer_info *disp_info)
{
	mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_START,
			 l_rule_info.disp_path, l_rule_info.layer_tb_idx |
			 (l_rule_info.bound_tb_idx << 16));

	if (GET_SYS_STATE(DISP_HRT_MULTI_TUI_ON)) {
		l_rule_info.disp_path = HRT_PATH_GENERAL;
		l_rule_info.layer_tb_idx = HRT_TB_TYPE_GENERAL;
	} else {
		if (l_rule_info.disp_path == HRT_PATH_RPO_L0) {
			l_rule_info.layer_tb_idx = HRT_TB_TYPE_RPO_L0;
		} else if (l_rule_info.disp_path == HRT_PATH_RPO_L0L1) {
			l_rule_info.layer_tb_idx = HRT_TB_TYPE_RPO_L0L1;
		} else {
			l_rule_info.layer_tb_idx = HRT_TB_TYPE_GENERAL;
			l_rule_info.disp_path = HRT_PATH_GENERAL;
		}
	}

	l_rule_info.primary_fps = 60;

	l_rule_info.bound_tb_idx = HRT_BOUND_TYPE_LP4;

	mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_END,
			 l_rule_info.disp_path, l_rule_info.layer_tb_idx |
			 (l_rule_info.bound_tb_idx << 16));
}

/* A OVL supports at most 2 yuv layers */
static void filter_by_yuv_layers(struct disp_layer_info *disp_info)
{
	unsigned int disp_idx = 0, i = 0;
	struct layer_config *info;
	unsigned int yuv_cnt;

	/* ovl support total 1 yuv layer ,align to mt6853*/
	for (disp_idx = 0 ; disp_idx < 2 ; disp_idx++) {
		yuv_cnt = 0;
		for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
			info = &(disp_info->input_config[disp_idx][i]);
			if (is_gles_layer(disp_info, disp_idx, i))
				continue;
			if (is_yuv(info->src_fmt)) {
				yuv_cnt++;
				if (yuv_cnt > 1)
					rollback_layer_to_GPU(disp_info,
						disp_idx, i);
			}
			if (support_color_format(info->src_fmt) != true)
				rollback_layer_to_GPU(disp_info,
						disp_idx, i);
		}
	}
}

static bool filter_by_sec_display(struct disp_layer_info *disp_info)
{
	bool flag = false;
	unsigned int i, layer_cnt = 0;

	for (i = 0; i < disp_info->layer_num[HRT_SECONDARY]; i++) {
		if (is_gles_layer(disp_info, HRT_SECONDARY, i))
			continue;

		layer_cnt++;
		if (layer_cnt > SECONDARY_OVL_LAYER_NUM) {
			rollback_layer_to_GPU(disp_info, HRT_SECONDARY, i);
			flag = false;
		}
	}

	return flag;
}

static void filter_by_wcg(struct disp_layer_info *disp_info)
{
	unsigned int i = 0;
	struct layer_config *c;

	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		c = &disp_info->input_config[HRT_PRIMARY][i];
		if (is_ovl_standard(c->dataspace) ||
		    has_layer_cap(c, MDP_HDR_LAYER))
			continue;

		rollback_layer_to_GPU(disp_info, HRT_PRIMARY, i);
	}

	for (i = 0; i < disp_info->layer_num[HRT_SECONDARY]; i++) {
		c = &disp_info->input_config[HRT_SECONDARY][i];
		if (!is_ovl_wcg(c->dataspace) &&
		    (is_ovl_standard(c->dataspace) ||
		     has_layer_cap(c, MDP_HDR_LAYER)))
			continue;

		rollback_layer_to_GPU(disp_info, HRT_SECONDARY, i);
	}

}

bool can_be_compress(enum DISP_FORMAT format)
{
	if (is_yuv(format) || format == DISP_FORMAT_RGB565 ||
		format == DISP_FORMAT_RGBA_FP16 ||
		format == DISP_FORMAT_PRGBA_FP16)
		return 0;
	else
		return 1;
}

static void filter_by_fbdc(struct disp_layer_info *disp_info)
{
	unsigned int i = 0;
	struct layer_config *c;

	/* primary: check fmt */
	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		c = &(disp_info->input_config[HRT_PRIMARY][i]);

		if (!c->compress)
			continue;

		if (can_be_compress(c->src_fmt) == 0)
			rollback_compress_layer_to_GPU(disp_info,
				HRT_PRIMARY, i);
	}

	/* secondary: rollback all */
	for (i = 0; i < disp_info->layer_num[HRT_SECONDARY]; i++) {
		c = &(disp_info->input_config[HRT_SECONDARY][i]);

		if (!c->compress)
			continue;

		/* if the layer is already gles layer, do not set NO_FBDC to
		 * reduce BW access
		 */
		if (is_gles_layer(disp_info, HRT_SECONDARY, i) == false)
			rollback_compress_layer_to_GPU(disp_info,
				HRT_SECONDARY, i);
	}
}

static bool filter_by_hw_limitation(struct disp_layer_info *disp_info)
{
	bool flag = false;

	filter_by_wcg(disp_info);

	filter_by_yuv_layers(disp_info);

	flag |= filter_by_sec_display(disp_info);

	return flag;
}

static int get_mapping_table(enum DISP_HW_MAPPING_TB_TYPE tb_type, int param);

void copy_hrt_bound_table(int is_larb, int *hrt_table,
	int active_config_id)
{
	unsigned long flags = 0;
	int valid_num, ovl_bound;
	int i;

	/* Not used in 6779 */
	if (is_larb)
		return;

	/* update table if hrt bw is enabled */
	spin_lock_irqsave(&hrt_table_lock, flags);
#ifdef MTK_FB_MMDVFS_SUPPORT
	valid_num = layering_get_valid_hrt(active_config_id);
#else
	valid_num = 200;
#endif
	ovl_bound = get_phy_layer_limit(
		get_mapping_table(
		DISP_HW_LAYER_TB,
		MAX_PHY_OVL_CNT - 1), 0);
	valid_num = min(valid_num, ovl_bound * 100);
	DISPINFO("%s:valid_num:%d,ovl_bound:%d\n",
		__func__, valid_num, ovl_bound);

	for (i = 0; i < HRT_LEVEL_NUM; i++) {
		emi_bound_table[l_rule_info.bound_tb_idx][i] =
			valid_num;
	}
	spin_unlock_irqrestore(&hrt_table_lock, flags);

	for (i = 0; i < HRT_LEVEL_NUM; i++)
		hrt_table[i] = emi_bound_table[l_rule_info.bound_tb_idx][i];
}

static int *get_bound_table(enum DISP_HW_MAPPING_TB_TYPE tb_type)
{
	switch (tb_type) {
	case DISP_HW_EMI_BOUND_TB:
		return emi_bound_table[l_rule_info.bound_tb_idx];
	case DISP_HW_LARB_BOUND_TB:
		return larb_bound_table[l_rule_info.bound_tb_idx];
	default:
		break;
	}
	return NULL;
}

static bool lr_frame_has_wcg(struct disp_layer_info *disp_info,
			     enum HRT_DISP_TYPE di)
{
	int i = 0;

	for (i = 0; i < disp_info->layer_num[di]; i++) {
		struct layer_config *c = &disp_info->input_config[di][i];

		if (is_ovl_wcg(c->dataspace))
			return true;
	}

	return false;
}

static int get_mapping_table(enum DISP_HW_MAPPING_TB_TYPE tb_type, int param)
{
	int layer_tb_idx = l_rule_info.layer_tb_idx;
	int map = -1;

	switch (tb_type) {
	case DISP_HW_OVL_TB:
		map = ovl_mapping_table[layer_tb_idx];
		break;
	case DISP_HW_LARB_TB:
		map = larb_mapping_table[layer_tb_idx];
		break;
	case DISP_HW_LAYER_TB:
		if (param < MAX_PHY_OVL_CNT && param >= 0)
			map = layer_mapping_table[layer_tb_idx][param];
		break;
	default:
		break;
	}
	return map;
}

void layering_rule_init(void)
{
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	unsigned int rc_mode =
			disp_helper_get_option(DISP_OPT_ROUND_CORNER_MODE);
	struct LCM_PARAMS *lcm_param = disp_lcm_get_params(primary_get_lcm());
#endif

	int opt = 0;
	l_rule_info.primary_fps = 60;
	l_rule_info.hrt_idx = 0;
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	l_rule_info.primary_fps = primary_display_get_default_disp_fps(0);
#endif
	register_layering_rule_ops(&l_rule_ops, &l_rule_info);

	opt = disp_helper_get_option(DISP_OPT_RPO);
	if (opt != -1)
		set_layering_opt(LYE_OPT_RPO,
			disp_helper_get_option(DISP_OPT_RPO));

	opt = disp_helper_get_option(DISP_OPT_OVL_EXT_LAYER);
	if (opt != -1)
		set_layering_opt(LYE_OPT_EXT_LAYER,
			 disp_helper_get_option(DISP_OPT_OVL_EXT_LAYER));

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	set_round_corner_opt(LYE_OPT_ROUND_CORNER,
			(disp_helper_get_option(DISP_OPT_ROUND_CORNER) & 0xFF) |
			((rc_mode & 0xFF) << 8) |
			((lcm_param->round_corner_en & 0xFF) << 16));
#endif
}

static bool _adaptive_dc_enabled(void)
{
#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	/* RDMA does not support rotation */
	return false;
#endif

	if (disp_mgr_has_mem_session() ||
	    !disp_helper_get_option(DISP_OPT_DC_BY_HRT) || is_DAL_Enabled())
		return false;

	return true;
}

static bool _rollback_all_to_GPU_for_idle(void)
{
	if (disp_mgr_has_mem_session() ||
	    !disp_helper_get_option(DISP_OPT_IDLEMGR_BY_REPAINT) ||
	    !atomic_read(&idle_need_repaint)) {
		atomic_set(&idle_need_repaint, 0);
		return false;
	}

	atomic_set(&idle_need_repaint, 0);

	return true;
}

unsigned long long layering_get_frame_bw(int active_cfg_id)
{
	static unsigned long long bw_base;
	unsigned int timing_fps = 60;
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	unsigned int _vact_timing_FPS = 6000;/*real fps * 100*/
#endif
	if (bw_base)
		return bw_base;

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/* should use the real timing fps such as VFP solution*/
	primary_display_get_cfg_fps(
			active_cfg_id, NULL, &_vact_timing_FPS);
	timing_fps = _vact_timing_FPS / 100;
#endif
	/*ToDo: Resolution switch
	 *if resolution changed also need change bw_base
	 */
	bw_base = (unsigned long long) primary_display_get_width() *
		primary_display_get_height() * timing_fps * 125 * 4;
	bw_base /= 100 * 1024 * 1024;

	return bw_base;
}
#ifdef MTK_FB_MMDVFS_SUPPORT
int layering_get_valid_hrt(int active_config_id)
{
	unsigned long long dvfs_bw;
	unsigned long long tmp;
	int tmp_bw;

	tmp_bw = mm_hrt_get_available_hrt_bw(get_virtual_port(VIRTUAL_DISP));
	tmp = layering_get_frame_bw(active_config_id);
	if (tmp_bw < 0) {
		DISP_PR_ERR("avail BW less than 0,DRAMC not ready!\n");
		dvfs_bw = 200;
		goto done;
	}
	dvfs_bw = 10000 * tmp_bw;
	dvfs_bw /= tmp * 100;

	/* error handling when requested BW is less than 2 layers */
	if (dvfs_bw < 200) {
		disp_aee_print("avail BW less than 2 layers, BW: %llu\n",
			dvfs_bw);
		dvfs_bw = 200;
	}
done:
	DISPINFO(
		"get avail HRT BW:%u : %llu %llu\n",
		mm_hrt_get_available_hrt_bw(
		get_virtual_port(VIRTUAL_DISP)),
		dvfs_bw, tmp);

	return dvfs_bw;

}

#endif
void update_layering_opt_by_disp_opt(enum DISP_HELPER_OPT opt, int value)
{
	switch (opt) {
	case DISP_OPT_OVL_EXT_LAYER:
		set_layering_opt(LYE_OPT_EXT_LAYER, value);
		break;
	case DISP_OPT_RPO:
		set_layering_opt(LYE_OPT_RPO, value);
		break;
	default:
		break;
	}
}

unsigned int layering_rule_get_hrt_idx(void)
{
	return l_rule_info.hrt_idx;
}

#define SET_CLIP_R(clip, clip_r)	(clip |= ((clip_r & 0xFF) << 0))
#define SET_CLIP_B(clip, clip_b)	(clip |= ((clip_b & 0xFF) << 8))
#define SET_CLIP_L(clip, clip_l)	(clip |= ((clip_l & 0xFF) << 16))
#define SET_CLIP_T(clip, clip_t)	(clip |= ((clip_t & 0xFF) << 24))

#define GET_CLIP_R(clip)		((clip >> 0) & 0xFF)
#define GET_CLIP_B(clip)		((clip >> 8) & 0xFF)
#define GET_CLIP_L(clip)		((clip >> 16) & 0xFF)
#define GET_CLIP_T(clip)		((clip >> 24) & 0xFF)

void calc_clip_x(struct layer_config *cfg)
{
	unsigned int tile_w = OVL_COMPRESS_TILE_W;
	unsigned int src_x_s, src_x_e; /* aligned */
	unsigned int clip_l = 0, clip_r = 0;

	src_x_s = (cfg->src_offset_x) & ~(tile_w - 1);

	src_x_e = (cfg->src_offset_x + cfg->src_width + tile_w - 1) &
		~(tile_w - 1);

	clip_l = cfg->src_offset_x - src_x_s;
	clip_r = src_x_e - cfg->src_offset_x - cfg->src_width;

	SET_CLIP_R(cfg->clip, clip_r);
	SET_CLIP_L(cfg->clip, clip_l);
}

void calc_clip_y(struct layer_config *cfg)
{
	unsigned int tile_h = OVL_COMPRESS_TILE_H;
	unsigned int src_y_s, src_y_e; /* aligned */
	unsigned int clip_t = 0, clip_b = 0;
	bool half_tile_align = OVL_HALF_TILE_ALIGN;

	if (half_tile_align) {
		src_y_s = (cfg->src_offset_y) & ~((tile_h >> 1) - 1);

		src_y_e = (cfg->src_offset_y + cfg->src_height +
			(tile_h >> 1) - 1) & ~((tile_h >> 1) - 1);

	} else {
		src_y_s = (cfg->src_offset_y) & ~(tile_h - 1);

		src_y_e = (cfg->src_offset_y + cfg->src_height +
			tile_h - 1) & ~(tile_h - 1);
	}
	clip_t = cfg->src_offset_y - src_y_s;
	clip_b = src_y_e - cfg->src_offset_y - cfg->src_height;

	SET_CLIP_T(cfg->clip, clip_t);
	SET_CLIP_B(cfg->clip, clip_b);
}

static void backup_input_config(struct disp_layer_info *disp_info)
{
	unsigned int size = 0;

	/* free before use */
	if (g_input_config != 0) {
		kfree(g_input_config);
		g_input_config = 0;
	}

	if (disp_info->layer_num[HRT_PRIMARY] <= 0 ||
		disp_info->input_config[HRT_PRIMARY] == NULL)
		return;

	/* memory allocate */
	size = sizeof(struct layer_config) * disp_info->layer_num[HRT_PRIMARY];
	g_input_config = kzalloc(size, GFP_KERNEL);

	if (g_input_config == 0) {
		DISP_PR_ERR("%s: allocate memory fail\n", __func__);
		return;
	}

	/* memory copy */
	memcpy(g_input_config, disp_info->input_config[HRT_PRIMARY], size);
}

static void fbdc_pre_calculate(struct disp_layer_info *disp_info)
{
	unsigned int i = 0;
	struct layer_config *cfg = NULL;

	/* backup g_input_config */
	backup_input_config(disp_info);

	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		cfg = &(disp_info->input_config[HRT_PRIMARY][i]);

		cfg->clip = 0;

		if (!cfg->compress)
			continue;

		if (is_gles_layer(disp_info, HRT_PRIMARY, i))
			continue;

		if (cfg->src_height != cfg->dst_height ||
			cfg->src_width != cfg->dst_width)
			continue;

		calc_clip_x(cfg);
		calc_clip_y(cfg);
	}
}

void fbdc_adjust_layout_for_ext_grouping(struct disp_layer_info *disp_info)
{
	int i = 0;
	struct layer_config *c;
	unsigned int dst_offset_x, dst_offset_y;
	unsigned int clip_r, clip_b, clip_l, clip_t;

	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		c = &(disp_info->input_config[HRT_PRIMARY][i]);

		/* skip if not compress, gles, resize */
		if (!c->compress || is_gles_layer(disp_info, HRT_PRIMARY, i) ||
			(c->src_height != c->dst_height) ||
			(c->src_width != c->dst_width))
			continue;

		dst_offset_x = c->dst_offset_x;
		dst_offset_y = c->dst_offset_y;

		clip_r = GET_CLIP_R(c->clip);
		clip_b = GET_CLIP_B(c->clip);
		clip_l = GET_CLIP_L(c->clip);
		clip_t = GET_CLIP_T(c->clip);

		/* bounary handling */
		if (dst_offset_x < clip_l)
			c->dst_offset_x = 0;
		else
			c->dst_offset_x -= clip_l;
		if (dst_offset_y < clip_t)
			c->dst_offset_y = 0;
		else
			c->dst_offset_y -= clip_t;

		c->dst_width += (clip_r + dst_offset_x - c->dst_offset_x);
		c->dst_height += (clip_b + dst_offset_y - c->dst_offset_y);
	}
}

int get_below_ext_layer(struct disp_layer_info *disp_info, int disp_idx,
	int cur)
{
	struct layer_config *c, *tmp_c;
	int phy_id = -1, ext_id = -1, l_dst_offset_y = -1, i;

	c = &(disp_info->input_config[disp_idx][cur]);

	/* search for phy */
	if (c->ext_sel_layer != -1) {
		for (i = cur - 1; i >= 0; i--) {
			tmp_c = &(disp_info->input_config[disp_idx][i]);
			if (tmp_c->ext_sel_layer == -1)
				phy_id = i;
		}
		if (phy_id == -1) /* error handle */
			return -1;
	} else
		phy_id = cur;

	/* traverse the ext layer below cur */
	tmp_c = &(disp_info->input_config[disp_idx][phy_id]);
	if (tmp_c->dst_offset_y > c->dst_offset_y) {
		ext_id = phy_id;
		l_dst_offset_y = tmp_c->dst_offset_y;
	}

	for (i = phy_id + 1; i <= phy_id + 3; i++) {
		/* skip itself */
		if (i == cur)
			continue;

		/* hit max num, stop */
		if (i >= disp_info->layer_num[disp_idx])
			break;

		/* hit gles, stop */
		if (is_gles_layer(disp_info, disp_idx, i))
			break;

		tmp_c = &(disp_info->input_config[disp_idx][i]);

		/* hit phy layer, stop */
		if (tmp_c->ext_sel_layer == -1)
			break;

		if (tmp_c->dst_offset_y > c->dst_offset_y) {
			if (l_dst_offset_y == -1 ||
				l_dst_offset_y > tmp_c->dst_offset_y) {
				ext_id = i;
				l_dst_offset_y = tmp_c->dst_offset_y;
			}
		}
	}

	return ext_id;
}

/*mt6785 Although tile is 32*8 but line buffer is 4 lines pixels*/
void fbdc_adjust_layout_for_overlap_calc(struct disp_layer_info *disp_info)
{
	int i = 0, ext_id = 0;
	struct layer_config *c, *ext_c;

	/* adjust dst layout because src clip */
	fbdc_adjust_layout_for_ext_grouping(disp_info);

	/* adjust dst layout because of buffer pre-fetch */
	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		/* skip gles layer */
		if (is_gles_layer(disp_info, HRT_PRIMARY, i))
			continue;

		c = &(disp_info->input_config[HRT_PRIMARY][i]);

		/* skip resize layer */
		if ((c->src_height != c->dst_height) ||
		    (c->src_width != c->dst_width))
			continue;

		/* if compressed, shift up 4 lines because pre-fetching */
		if (c->compress) {
			if (c->dst_height > 4)
				c->dst_height -= 4;
			else
				c->dst_height = 1;
		}

		/* if there is compressed ext layer below this layer,
		 * add pre-fetch lines behind it
		 */
		ext_id = get_below_ext_layer(disp_info, HRT_PRIMARY, i);

		if (is_layer_id_valid(disp_info, HRT_PRIMARY,
			ext_id) == true) {
			ext_c =
			    &(disp_info->input_config[HRT_PRIMARY][ext_id]);
			if (ext_c->compress) {
				c->dst_height +=
				    (GET_CLIP_T(ext_c->clip) + 4);
			}
		}
	}
}

static void fbdc_adjust_layout(struct disp_layer_info *disp_info,
	enum ADJUST_LAYOUT_PURPOSE p)
{
	if (p == ADJUST_LAYOUT_EXT_GROUPING)
		fbdc_adjust_layout_for_ext_grouping(disp_info);
	else
		fbdc_adjust_layout_for_overlap_calc(disp_info);
}

static void fbdc_restore_layout(struct disp_layer_info *dst_info,
	enum ADJUST_LAYOUT_PURPOSE p)
{
	int i = 0;
	struct layer_config *layer_info_s, *layer_info_d;

	if (g_input_config == 0)
		return;

	for (i = 0; i < dst_info->layer_num[HRT_PRIMARY]; i++) {
		layer_info_d = &(dst_info->input_config[HRT_PRIMARY][i]);
		layer_info_s = &(g_input_config[i]);

		layer_info_d->dst_offset_x = layer_info_s->dst_offset_x;
		layer_info_d->dst_offset_y = layer_info_s->dst_offset_y;
		layer_info_d->dst_width = layer_info_s->dst_width;
		layer_info_d->dst_height = layer_info_s->dst_height;
	}
}

static struct layering_rule_ops l_rule_ops = {
	.resizing_rule = lr_rsz_layout,
	.rsz_by_gpu_info_change = lr_gpu_change_rsz_info,
	.scenario_decision = layering_rule_senario_decision,
	.get_bound_table = get_bound_table,
	/* HRT table would change so do not use get_hrt_bound
	 * in layering_rule_base. Instead, copy hrt table before calculation
	 */
	.get_hrt_bound = NULL,
	.copy_hrt_bound_table = copy_hrt_bound_table,
	.get_mapping_table = get_mapping_table,
	.rollback_to_gpu_by_hw_limitation = filter_by_hw_limitation,
	.unset_disp_rsz_attr = lr_unset_disp_rsz_attr,
	.adaptive_dc_enabled = _adaptive_dc_enabled,
	.rollback_all_to_GPU_for_idle = _rollback_all_to_GPU_for_idle,
	.frame_has_wcg = lr_frame_has_wcg,
	.fbdc_pre_calculate = fbdc_pre_calculate,
	.fbdc_adjust_layout = fbdc_adjust_layout,
	.fbdc_restore_layout = fbdc_restore_layout,
	.fbdc_rule = filter_by_fbdc,
};
