/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/delay.h>
#include <linux/sched.h>
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

//#if defined(CONFIG_MTK_DRAMC)
//#include "mtk_dramc.h"
//#endif
#include "mtk_layering_rule.h"
#ifdef MTK_FB_MMDVFS_SUPPORT
#include "mmdvfs_mgr.h"
#include "mmdvfs_pmqos.h"
#endif
#include "mtk_log.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_graphics_base.h"

static struct layering_rule_ops l_rule_ops;
static struct layering_rule_info_t l_rule_info;

static DEFINE_SPINLOCK(hrt_table_lock);

/* To backup for primary display drm_mtk_layer_config */
static struct drm_mtk_layer_config *g_input_config;

static int emi_bound_table[HRT_BOUND_NUM][HRT_LEVEL_NUM] = {
	/* HRT_BOUND_TYPE_LP4 */
	{100, 300, 500, 600},
};

static int larb_bound_table[HRT_BOUND_NUM][HRT_LEVEL_NUM] = {
	/* HRT_BOUND_TYPE_LP4 */
	{100, 300, 500, 600},
};

/**
 * The layer mapping table define ovl layer dispatch rule for both
 * primary and secondary display.Each table has 16 elements which
 * represent the layer mapping rule by the number of input layers.
 */
static uint16_t layer_mapping_table[HRT_TB_NUM] = {
	0x0003, 0x007E, 0x007A, 0x0001
};
static uint16_t layer_mapping_table_vds_switch[HRT_TB_NUM] = {
	0x0078, 0x0078, 0x0078, 0x0078
};

/**
 * The larb mapping table represent the relation between LARB and OVL.
 */
static uint16_t larb_mapping_table[HRT_TB_NUM] = {
	0x0001, 0x0010, 0x0010, 0x0001
};
static uint16_t larb_mapping_tb_vds_switch[HRT_TB_NUM] = {
	0x0010, 0x0010, 0x0010, 0x0001
};

/**
 * The OVL mapping table is used to get the OVL index of correcponding layer.
 * The bit value 1 means the position of the last layer in OVL engine.
 */
static uint16_t ovl_mapping_table[HRT_TB_NUM] = {
	0x0002, 0x0045, 0x0045, 0x0001
};
static uint16_t ovl_mapping_tb_vds_switch[HRT_TB_NUM] = {
	0x0045, 0x0045, 0x0045, 0x0045
};

#define GET_SYS_STATE(sys_state)                                               \
	((l_rule_info.hrt_sys_state >> sys_state) & 0x1)

static void layering_rule_senario_decision(unsigned int scn_decision_flag,
					   unsigned int scale_num)
{
/*TODO: need MMP support*/
#if 0
	mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_START,
			 l_rule_info.addon_scn[0], l_rule_info.layer_tb_idx |
			 (l_rule_info.bound_tb_idx << 16));
#endif
	l_rule_info.primary_fps = 60;
	l_rule_info.bound_tb_idx = HRT_BOUND_TYPE_LP4;

	if (scn_decision_flag & SCN_NEED_GAME_PQ)
		l_rule_info.addon_scn[HRT_PRIMARY] = GAME_PQ;
	else if (scn_decision_flag & SCN_NEED_VP_PQ)
		l_rule_info.addon_scn[HRT_PRIMARY] = VP_PQ;
	else if (scale_num == 1)
		l_rule_info.addon_scn[HRT_PRIMARY] = ONE_SCALING;
	else if (scale_num == 2)
		l_rule_info.addon_scn[HRT_PRIMARY] = TWO_SCALING;
	else
		l_rule_info.addon_scn[HRT_PRIMARY] = NONE;

	if (scn_decision_flag & SCN_TRIPLE_DISP) {
		l_rule_info.addon_scn[HRT_SECONDARY] = TRIPLE_DISP;
		l_rule_info.addon_scn[HRT_THIRD] = TRIPLE_DISP;
	} else {
		l_rule_info.addon_scn[HRT_SECONDARY] = NONE;
		l_rule_info.addon_scn[HRT_THIRD] = NONE;
	}
/*TODO: need MMP support*/
#if 0
	mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_END,
			 l_rule_info.addon_scn[0], l_rule_info.layer_tb_idx |
			 (l_rule_info.bound_tb_idx << 16));
#endif
}

/* A OVL supports at most 1 yuv layers */
static void filter_by_yuv_layers(struct drm_mtk_layering_info *disp_info)
{
	unsigned int disp_idx = 0, i = 0;
	struct drm_mtk_layer_config *info;
	unsigned int yuv_gpu_cnt;
	unsigned int yuv_layer_gpu[12];
	int yuv_layer_ovl = -1;

	for (disp_idx = 0 ; disp_idx < HRT_TYPE_NUM ; disp_idx++) {
		yuv_layer_ovl = -1;
		yuv_gpu_cnt = 0;

		/* cal gpu_layer_cnt & yuv_layer_cnt */
		for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
			info = &(disp_info->input_config[disp_idx][i]);
			if (mtk_is_gles_layer(disp_info, disp_idx, i))
				continue;

			if (mtk_is_yuv(info->src_fmt)) {
				if (info->secure == 1 &&
				    yuv_layer_ovl < 0) {
					yuv_layer_ovl = i;
				} else {
					yuv_layer_gpu[yuv_gpu_cnt] = i;
					yuv_gpu_cnt++;
				}
			}
		}

		if (yuv_gpu_cnt == 0)
			continue;

		if (yuv_layer_ovl >= 0) {
			//if have sec layer, rollback the others to gpu
			for (i = 0; i < yuv_gpu_cnt; i++)
				mtk_rollback_layer_to_GPU(disp_info,
					disp_idx, yuv_layer_gpu[i]);
		} else {
			/* keep the 1st normal yuv layer,
			 * rollback the others to gpu
			 */
			for (i = 1; i < yuv_gpu_cnt; i++)
				mtk_rollback_layer_to_GPU(disp_info,
					disp_idx, yuv_layer_gpu[i]);
		}
	}
}

static void filter_2nd_display(struct drm_mtk_layering_info *disp_info)
{
	unsigned int i = 0, j = 0;

	for (i = HRT_SECONDARY; i < HRT_TYPE_NUM; i++) {
		unsigned int max_layer_cnt = SECONDARY_OVL_LAYER_NUM;
		unsigned int layer_cnt = 0;

		if (is_triple_disp(disp_info) && i == HRT_SECONDARY)
			max_layer_cnt = 1;
		for (j = 0; j < disp_info->layer_num[i]; j++) {
			if (mtk_is_gles_layer(disp_info, i, j))
				continue;

			layer_cnt++;
			if (layer_cnt >= max_layer_cnt)
				mtk_rollback_layer_to_GPU(disp_info, i, j);
		}
	}
}

static bool is_ovl_wcg(enum mtk_drm_dataspace ds)
{
	bool ret = false;

	switch (ds) {
	case MTK_DRM_DATASPACE_V0_SCRGB:
	case MTK_DRM_DATASPACE_V0_SCRGB_LINEAR:
	case MTK_DRM_DATASPACE_DISPLAY_P3:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static bool is_ovl_standard(struct drm_device *dev, enum mtk_drm_dataspace ds)
{
	struct mtk_drm_private *priv = dev->dev_private;
	enum mtk_drm_dataspace std = ds & MTK_DRM_DATASPACE_STANDARD_MASK;
	bool ret = false;

	if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_OVL_WCG) &&
	    is_ovl_wcg(ds))
		return ret;

	switch (std) {
	case MTK_DRM_DATASPACE_STANDARD_BT2020:
	case MTK_DRM_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE:
		ret = false;
		break;
	default:
		ret = true;
		break;
	}
	return ret;
}

static void filter_by_wcg(struct drm_device *dev,
			  struct drm_mtk_layering_info *disp_info)
{
	unsigned int i, j;
	struct drm_mtk_layer_config *c;

	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		c = &disp_info->input_config[HRT_PRIMARY][i];
		if (is_ovl_standard(dev, c->dataspace) ||
		    mtk_has_layer_cap(c, MTK_MDP_HDR_LAYER))
			continue;

		mtk_rollback_layer_to_GPU(disp_info, HRT_PRIMARY, i);
	}

	for (i = HRT_SECONDARY; i < HRT_TYPE_NUM; i++)
		for (j = 0; j < disp_info->layer_num[i]; j++) {
			c = &disp_info->input_config[i][j];
			if (!is_ovl_wcg(c->dataspace) &&
			    (is_ovl_standard(dev, c->dataspace) ||
			     mtk_has_layer_cap(c, MTK_MDP_HDR_LAYER)))
				continue;

			mtk_rollback_layer_to_GPU(disp_info, i, j);
		}
}

static bool can_be_compress(uint32_t format)
{
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6833) || \
	defined(CONFIG_MACH_MT6781)
	if (mtk_is_yuv(format))
		return 0;
#else
	if (mtk_is_yuv(format) || format == DRM_FORMAT_RGB565 ||
	    format == DRM_FORMAT_BGR565)
		return 0;
#endif

	return 1;
}

static void filter_by_fbdc(struct drm_mtk_layering_info *disp_info)
{
	unsigned int i, j;
	struct drm_mtk_layer_config *c;

	/* primary: check fmt */
	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		c = &(disp_info->input_config[HRT_PRIMARY][i]);

		if (!c->compress)
			continue;

		if (can_be_compress(c->src_fmt) == 0)
			mtk_rollback_compress_layer_to_GPU(disp_info,
							   HRT_PRIMARY, i);
	}

	/* secondary: rollback all */
	for (i = HRT_SECONDARY; i < HRT_TYPE_NUM; i++)
		for (j = 0; j < disp_info->layer_num[i]; j++) {
			c = &(disp_info->input_config[i][j]);

			if (!c->compress ||
				mtk_is_gles_layer(disp_info, i, j))
				continue;

			/* if the layer is already gles layer,
			 * do not set NO_FBDC to reduce BW access
			 */
			mtk_rollback_compress_layer_to_GPU(disp_info, i, j);
		}
}

static bool filter_by_hw_limitation(struct drm_device *dev,
				    struct drm_mtk_layering_info *disp_info)
{
	bool flag = false;

	filter_by_wcg(dev, disp_info);

	filter_by_yuv_layers(disp_info);

	/* Is this nessasary? */
	filter_2nd_display(disp_info);

	return flag;
}

static uint16_t get_mapping_table(struct drm_device *dev, int disp_idx,
				  enum DISP_HW_MAPPING_TB_TYPE tb_type,
				  int param);
static int layering_get_valid_hrt(struct drm_crtc *crtc,
					struct drm_display_mode *mode);

static void copy_hrt_bound_table(struct drm_mtk_layering_info *disp_info,
			int is_larb, int *hrt_table, struct drm_device *dev)
{
	unsigned long flags = 0;
	int valid_num, ovl_bound, i;
	struct drm_crtc *crtc;
	struct drm_display_mode *mode;

	/* Not used in 6779 */
	if (is_larb)
		return;

	drm_for_each_crtc(crtc, dev) {
		if (drm_crtc_index(crtc) == 0)
			break;
	}

	mode = mtk_drm_crtc_avail_disp_mode(crtc,
		disp_info->disp_mode_idx[0]);

	/* update table if hrt bw is enabled */
	spin_lock_irqsave(&hrt_table_lock, flags);
	valid_num = layering_get_valid_hrt(crtc, mode);
	ovl_bound = mtk_get_phy_layer_limit(
		get_mapping_table(dev, 0, DISP_HW_LAYER_TB, MAX_PHY_OVL_CNT));
	valid_num = min(valid_num, ovl_bound * 100);

	for (i = 0; i < HRT_LEVEL_NUM; i++)
		emi_bound_table[l_rule_info.bound_tb_idx][i] = valid_num;
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

static uint16_t get_mapping_table(struct drm_device *dev, int disp_idx,
				  enum DISP_HW_MAPPING_TB_TYPE tb_type,
				  int param)
{
	uint16_t map = 0;
	uint16_t tmp_map = 0;
	int i;
	int cnt = 0;
	struct drm_crtc *crtc;
	const struct mtk_addon_scenario_data *addon_data = NULL;
	struct mtk_drm_private *priv = dev->dev_private;

	drm_for_each_crtc(crtc, dev) {
		if (drm_crtc_index(crtc) == disp_idx) {
			addon_data =
				mtk_addon_get_scenario_data(__func__,
					crtc,
					l_rule_info.addon_scn[disp_idx]);
			break;
		}
	}

	if (!addon_data) {
		DDPPR_ERR("disp_idx:%d cannot get addon data\n", disp_idx);
		return 0;
	}

	switch (tb_type) {
	case DISP_HW_OVL_TB:
		map = ovl_mapping_table[addon_data->hrt_type];
		if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_VDS_PATH_SWITCH) &&
			priv->need_vds_path_switch)
			map = ovl_mapping_tb_vds_switch[addon_data->hrt_type];
		break;
	case DISP_HW_LARB_TB:
		map = larb_mapping_table[addon_data->hrt_type];
		if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_VDS_PATH_SWITCH) &&
			priv->need_vds_path_switch)
			map = larb_mapping_tb_vds_switch[addon_data->hrt_type];
		break;
	case DISP_HW_LAYER_TB:
		if (param <= MAX_PHY_OVL_CNT && param >= 0) {
			tmp_map = layer_mapping_table[addon_data->hrt_type];
			if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_VDS_PATH_SWITCH) &&
				priv->need_vds_path_switch)
				tmp_map = layer_mapping_table_vds_switch[
					addon_data->hrt_type];

			for (i = 0, map = 0; i < 16; i++) {
				if (cnt == param)
					break;

				if (tmp_map & 0x1) {
					map |= (0x1 << i);
					cnt++;
				}
				tmp_map >>= 1;
			}
		}
		break;
	default:
		break;
	}
	return map;
}

void mtk_layering_rule_init(struct drm_device *dev)
{
	struct mtk_drm_private *private = dev->dev_private;

	l_rule_info.primary_fps = 60;
	l_rule_info.hrt_idx = 0;
	mtk_register_layering_rule_ops(&l_rule_ops, &l_rule_info);

	mtk_set_layering_opt(
		LYE_OPT_RPO,
		mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_RPO));
	mtk_set_layering_opt(LYE_OPT_EXT_LAYER,
			     mtk_drm_helper_get_opt(private->helper_opt,
						    MTK_DRM_OPT_OVL_EXT_LAYER));
	mtk_set_layering_opt(LYE_OPT_CLEAR_LAYER,
			     mtk_drm_helper_get_opt(private->helper_opt,
						    MTK_DRM_OPT_CLEAR_LAYER));
}

static bool _rollback_all_to_GPU_for_idle(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;

	/* Slghtly modify this function for TUI */

	if (atomic_read(&priv->rollback_all))
		return true;

	if (!mtk_drm_helper_get_opt(priv->helper_opt,
				    MTK_DRM_OPT_IDLEMGR_BY_REPAINT) ||
	    !atomic_read(&priv->idle_need_repaint)) {
		atomic_set(&priv->idle_need_repaint, 0);
		return false;
	}

	atomic_set(&priv->idle_need_repaint, 0);

	return true;
}

unsigned long long _layering_get_frame_bw(struct drm_crtc *crtc,
						struct drm_display_mode *mode)
{
	static unsigned long long bw_base;
	static int fps;
	unsigned int vact_fps;
	int width = mode->hdisplay, height = mode->vdisplay;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc->panel_ext && mtk_crtc->panel_ext->params) {
		struct mtk_panel_params *params;

		params = mtk_crtc->panel_ext->params;
		if (params->dyn_fps.switch_en == 1 &&
			params->dyn_fps.vact_timing_fps != 0)
			vact_fps = params->dyn_fps.vact_timing_fps;
		else
			vact_fps = mode->vrefresh;
	} else
		vact_fps = mode->vrefresh;
	DDPINFO("%s,vrefresh = %d", __func__, vact_fps);

	if (fps == vact_fps)
		return bw_base;

	fps = vact_fps;

	bw_base = (unsigned long long)width * height * fps * 125 * 4;

#if BITS_PER_LONG == 32
	do_div(bw_base, 100 * 1024 * 1024);
#else
	bw_base /= 100 * 1024 * 1024;
#endif

	return bw_base;
}

static int layering_get_valid_hrt(struct drm_crtc *crtc,
					struct drm_display_mode *mode)
{
	unsigned long long dvfs_bw = 0;
#ifdef MTK_FB_MMDVFS_SUPPORT
	unsigned long long tmp = 0;
	struct mtk_ddp_comp *output_comp;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	dvfs_bw = mm_hrt_get_available_hrt_bw(get_virtual_port(VIRTUAL_DISP));
	if (dvfs_bw == 0xffffffffffffffff) {
		DDPPR_ERR("mm_hrt_get_available_hrt_bw=-1\n");
		return 600;
	}

	dvfs_bw *= 10000;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL,
			GET_FRAME_HRT_BW_BY_DATARATE, &tmp);
	if (!tmp) {
		DDPPR_ERR("Get frame hrt bw by datarate is zero\n");
		return 600;
	}
#if BITS_PER_LONG == 32
	do_div(dvfs_bw, tmp * 100);
#else
	dvfs_bw /= tmp * 100;
#endif

	/* error handling when requested BW is less than 2 layers */
	if (dvfs_bw < 200) {
		// disp_aee_print("avail BW less than 2 layers, BW: %llu\n",
		//	dvfs_bw);
		DDPPR_ERR("avail BW less than 2 layers, BW: %llu\n", dvfs_bw);
		dvfs_bw = 200;
	}

	DDPINFO("get avail HRT BW:%u : %llu %llu\n",
		mm_hrt_get_available_hrt_bw(get_virtual_port(VIRTUAL_DISP)),
		dvfs_bw, tmp);
#else
	dvfs_bw = 600;
#endif

	return dvfs_bw;
}

void mtk_update_layering_opt_by_disp_opt(enum MTK_DRM_HELPER_OPT opt, int value)
{
	switch (opt) {
	case MTK_DRM_OPT_OVL_EXT_LAYER:
		mtk_set_layering_opt(LYE_OPT_EXT_LAYER, value);
		break;
	case MTK_DRM_OPT_RPO:
		mtk_set_layering_opt(LYE_OPT_RPO, value);
		break;
	case MTK_DRM_OPT_CLEAR_LAYER:
		mtk_set_layering_opt(LYE_OPT_CLEAR_LAYER, value);
		break;
	default:
		break;
	}
}

unsigned int _layering_rule_get_hrt_idx(void)
{
	return l_rule_info.hrt_idx;
}

#define SET_CLIP_R(clip, clip_r) (clip |= ((clip_r & 0xFF) << 0))
#define SET_CLIP_B(clip, clip_b) (clip |= ((clip_b & 0xFF) << 8))
#define SET_CLIP_L(clip, clip_l) (clip |= ((clip_l & 0xFF) << 16))
#define SET_CLIP_T(clip, clip_t) (clip |= ((clip_t & 0xFF) << 24))

#define GET_CLIP_R(clip) ((clip >> 0) & 0xFF)
#define GET_CLIP_B(clip) ((clip >> 8) & 0xFF)
#define GET_CLIP_L(clip) ((clip >> 16) & 0xFF)
#define GET_CLIP_T(clip) ((clip >> 24) & 0xFF)

static void calc_clip_x(struct drm_mtk_layer_config *cfg)
{
	unsigned int tile_w = 16;
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

static void calc_clip_y(struct drm_mtk_layer_config *cfg)
{
	unsigned int tile_h = 4;
	unsigned int src_y_s, src_y_e; /* aligned */
	unsigned int clip_t = 0, clip_b = 0;

	src_y_s = (cfg->src_offset_y) & ~(tile_h - 1);

	src_y_e = (cfg->src_offset_y + cfg->src_height + tile_h - 1) &
		  ~(tile_h - 1);

	clip_t = cfg->src_offset_y - src_y_s;
	clip_b = src_y_e - cfg->src_offset_y - cfg->src_height;

	SET_CLIP_T(cfg->clip, clip_t);
	SET_CLIP_B(cfg->clip, clip_b);
}

static void backup_input_config(struct drm_mtk_layering_info *disp_info)
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
	size = sizeof(struct drm_mtk_layer_config) *
	       disp_info->layer_num[HRT_PRIMARY];
	g_input_config = kzalloc(size, GFP_KERNEL);

	if (g_input_config == 0) {
		DDPPR_ERR("%s: allocate memory fail\n", __func__);
		return;
	}

	/* memory copy */
	memcpy(g_input_config, disp_info->input_config[HRT_PRIMARY], size);
}

static void fbdc_pre_calculate(struct drm_mtk_layering_info *disp_info)
{
	unsigned int i = 0;
	struct drm_mtk_layer_config *cfg = NULL;

	/* backup g_input_config */
	backup_input_config(disp_info);

	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		cfg = &(disp_info->input_config[HRT_PRIMARY][i]);

		cfg->clip = 0;

		if (!cfg->compress)
			continue;

		if (mtk_is_gles_layer(disp_info, HRT_PRIMARY, i))
			continue;

		if (cfg->src_height != cfg->dst_height ||
		    cfg->src_width != cfg->dst_width)
			continue;

		calc_clip_x(cfg);
		calc_clip_y(cfg);
	}
}

static void
fbdc_adjust_layout_for_ext_grouping(struct drm_mtk_layering_info *disp_info)
{
	int i = 0;
	struct drm_mtk_layer_config *c;
	unsigned int dst_offset_x, dst_offset_y;
	unsigned int clip_r, clip_b, clip_l, clip_t;

	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		c = &(disp_info->input_config[HRT_PRIMARY][i]);

		/* skip if not compress, gles, resize */
		if (!c->compress ||
		    mtk_is_gles_layer(disp_info, HRT_PRIMARY, i) ||
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

static int get_below_ext_layer(struct drm_mtk_layering_info *disp_info,
			       int disp_idx, int cur)
{
	struct drm_mtk_layer_config *c, *tmp_c;
	int phy_id = -1, ext_id = -1, l_dst_offset_y = -1, i;

	if (disp_idx < 0)
		return -1;

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
		if (mtk_is_gles_layer(disp_info, disp_idx, i))
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

static void
fbdc_adjust_layout_for_overlap_calc(struct drm_mtk_layering_info *disp_info)
{
	int i = 0, ext_id = 0;
	struct drm_mtk_layer_config *c, *ext_c;

	/* adjust dst layout because src clip */
	fbdc_adjust_layout_for_ext_grouping(disp_info);

	/* adjust dst layout because of buffer pre-fetch */
	for (i = 0; i < disp_info->layer_num[HRT_PRIMARY]; i++) {
		/* skip gles layer */
		if (mtk_is_gles_layer(disp_info, HRT_PRIMARY, i))
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

		if (mtk_is_layer_id_valid(disp_info, HRT_PRIMARY, ext_id) ==
		    true) {
			ext_c = &(disp_info->input_config[HRT_PRIMARY][ext_id]);
			if (ext_c->compress)
				c->dst_height += (GET_CLIP_T(ext_c->clip) + 4);
		}
	}
}

static void fbdc_adjust_layout(struct drm_mtk_layering_info *disp_info,
			       enum ADJUST_LAYOUT_PURPOSE p)
{
	if (p == ADJUST_LAYOUT_EXT_GROUPING)
		fbdc_adjust_layout_for_ext_grouping(disp_info);
	else
		fbdc_adjust_layout_for_overlap_calc(disp_info);
}

static void fbdc_restore_layout(struct drm_mtk_layering_info *dst_info,
				enum ADJUST_LAYOUT_PURPOSE p)
{
	int i = 0;
	struct drm_mtk_layer_config *layer_info_s, *layer_info_d;

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
	.scenario_decision = layering_rule_senario_decision,
	.get_bound_table = get_bound_table,
	/* HRT table would change so do not use get_hrt_bound
	 * in layering_rule_base. Instead, copy hrt table before calculation
	 */
	.get_hrt_bound = NULL,
	.copy_hrt_bound_table = copy_hrt_bound_table,
	.get_mapping_table = get_mapping_table,
	.rollback_to_gpu_by_hw_limitation = filter_by_hw_limitation,
	.rollback_all_to_GPU_for_idle = _rollback_all_to_GPU_for_idle,
	.fbdc_pre_calculate = fbdc_pre_calculate,
	.fbdc_adjust_layout = fbdc_adjust_layout,
	.fbdc_restore_layout = fbdc_restore_layout,
	.fbdc_rule = filter_by_fbdc,
};
