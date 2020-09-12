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
#include <linux/file.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <mt-plat/mtk_chip.h>
#include <drm/drm_modes.h>
#include <drm/drm_property.h>
#ifdef CONFIG_MTK_DCS
#include <mt-plat/mtk_meminfo.h>
#endif
#include "mtk_layering_rule.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_assert.h"
#include "mtk_log.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_fbdev.h"
#define CREATE_TRACE_POINTS
#include "mtk_layer_layout_trace.h"
#include "mtk_drm_gem.h"

static struct drm_mtk_layering_info layering_info;
#ifdef HRT_UT_DEBUG
static int debug_resolution_level;
#endif
static struct layering_rule_info_t *l_rule_info;
static struct layering_rule_ops *l_rule_ops;
static int ext_id_tuning(struct drm_device *dev,
			  struct drm_mtk_layering_info *disp_info,
			  int disp_idx);
static unsigned int roll_gpu_for_idle;
static int g_emi_bound_table[HRT_LEVEL_NUM];

#define RSZ_TILE_LENGTH 1440
#define RSZ_IN_MAX_HEIGHT 4096
#define DISP_RSZ_LAYER_NUM 2

static struct {
	enum LYE_HELPER_OPT opt;
	unsigned int val;
	const char *desc;
} help_info[] = {
	{LYE_OPT_DUAL_PIPE, 0, "LYE_OPT_DUAL_PIPE"},
	{LYE_OPT_EXT_LAYER, 0, "LYE_OPT_EXTENDED_LAYER"},
	{LYE_OPT_RPO, 0, "LYE_OPT_RPO"},
	{LYE_OPT_CLEAR_LAYER, 0, "LYE_OPT_CLEAR_LAYER"},
};

void mtk_set_layering_opt(enum LYE_HELPER_OPT opt, int value)
{
	if (opt >= LYE_OPT_NUM) {
		DDPMSG("%s invalid layering opt:%d\n", __func__, opt);
		return;
	}
	if (value < 0) {
		DDPPR_ERR("%s invalid opt value:%d\n", __func__, value);
		return;
	}


	help_info[opt].val = !!value;
}

static int get_layering_opt(enum LYE_HELPER_OPT opt)
{
	if (opt >= LYE_OPT_NUM) {
		DDPMSG("%s invalid layering opt:%d\n", __func__, opt);
		return -1;
	}

	return help_info[opt].val;
}

/*
 * bool is_decouple_path(struct drm_mtk_layering_info *disp_info) {
 *	if (disp_info->disp_mode[HRT_PRIMARY] != 1)
 *		return true;
 *	else
 *		return false;
 * }
 */

/*
 * static bool is_argb_fmt(uint32_t format) {
 *	switch (format) {
 *	case DRM_FORMAT_ARGB8888:
 *	case DRM_FORMAT_ABGR8888:
 *	case DRM_FORMAT_RGBA8888:
 *	case DRM_FORMAT_BGRA8888:
 *		return true;
 *	default:
 *		return false;
 *	}
 * }
 */

bool mtk_is_yuv(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		return true;
	default:
		return false;
	}
}

bool mtk_is_layer_id_valid(struct drm_mtk_layering_info *disp_info,
			   int disp_idx, int i)
{
	if (i < 0 || i >= disp_info->layer_num[disp_idx])
		return false;
	else
		return true;
}

bool mtk_is_gles_layer(struct drm_mtk_layering_info *disp_info, int disp_idx,
		       int layer_idx)
{
	if (layer_idx >= disp_info->gles_head[disp_idx] &&
	    layer_idx <= disp_info->gles_tail[disp_idx])
		return true;
	else
		return false;
}

inline bool mtk_has_layer_cap(struct drm_mtk_layer_config *layer_info,
			      enum MTK_LAYERING_CAPS l_caps)
{
	if (layer_info->layer_caps & l_caps)
		return true;
	return false;
}

static int is_overlap_on_yaxis(struct drm_mtk_layer_config *lhs,
			       struct drm_mtk_layer_config *rhs)
{
	/*
	 * HWC may adjust the offset of yuv layer due to alignment limitation
	 * after querying layering rule.
	 * So it have chance to make yuv layer overlap with other ext layer.
	 * We add the workaround here to avoid the yuv as the base layer of
	 * extended layer and will remove it once the HWC correct the problem.
	 */
	if (mtk_is_yuv(lhs->src_fmt))
		return 1;

	if ((lhs->dst_offset_y + lhs->dst_height <= rhs->dst_offset_y) ||
	    (rhs->dst_offset_y + rhs->dst_height <= lhs->dst_offset_y))
		return 0;
	return 1;
}

static bool is_layer_across_each_pipe(struct drm_mtk_layer_config *layer_info)
{
/* TODO: support dual pipe */
#if 0
	int dst_x, dst_w;

	if (!get_layering_opt(LYE_OPT_DUAL_PIPE))
		return true;

	dst_x = layer_info->dst_offset_x;
	dst_w = layer_info->dst_width;
	if ((dst_x + dst_w <= primary_display_get_width() / 2) ||
	    (dst_x > primary_display_get_width() / 2))
		return false;
#endif
	return true;
}

static inline bool is_extended_layer(struct drm_mtk_layer_config *layer_info)
{
	return (layer_info->ext_sel_layer != -1);
}

static bool is_extended_base_layer_valid(struct drm_mtk_layer_config *configs,
					 int layer_idx)
{
	if ((layer_idx == 0) ||
		(configs->src_fmt == MTK_DRM_FORMAT_DIM) ||
		mtk_has_layer_cap(configs, MTK_DISP_RSZ_LAYER))
		return false;

	/*
	 * Under dual pipe, if the layer is not included in each pipe,
	 * it cannot be used as a base layer for extended layer
	 * because extended layer would not find base layer in one of
	 * display pipe.
	 * So always mark this specific layer as overlap to avoid the fail case.
	 */
	if (!is_layer_across_each_pipe(configs))
		return false;

	return true;
}

static inline bool is_extended_over_limit(int ext_cnt)
{
	if (ext_cnt > 3)
		return true;
	return false;
}

/**
 * check if continuous ext layers are overlapped with each other
 * also need to check the below nearest phy layer
 * which these ext layers will be attached to
 * 1. check all ext layers, if overlapped with any one, change it to phy layer
 * 2. if more than 1 ext layer exist, need to check the phy layer
 */
static int is_continuous_ext_layer_overlap(struct drm_mtk_layer_config *configs,
					   int curr)
{
	int overlapped;
	struct drm_mtk_layer_config *src_info, *dst_info;
	int i;

	overlapped = 0;
	dst_info = &configs[curr];
	for (i = curr - 1; i >= 0; i--) {
		src_info = &configs[i];
		if (is_extended_layer(src_info)) {
			overlapped |= is_overlap_on_yaxis(src_info, dst_info);
			if (overlapped)
				break;
		} else {
			overlapped |= is_overlap_on_yaxis(src_info, dst_info);
			if (!is_extended_base_layer_valid(src_info, i))
				overlapped |= 1;
			break;
		}
	}
	return overlapped;
}

bool is_triple_disp(struct drm_mtk_layering_info *disp_info)
{
	if (disp_info->layer_num[HRT_PRIMARY] &&
		disp_info->layer_num[HRT_SECONDARY] &&
		disp_info->layer_num[HRT_THIRD])
		return true;
	return false;
}

static int get_phy_ovl_layer_cnt(struct drm_mtk_layering_info *info,
				int disp_idx)
{
	int total_cnt = 0;
	int i;
	struct drm_mtk_layer_config *layer_info;

	if (info->layer_num[disp_idx] > 0) {
		total_cnt = info->layer_num[disp_idx];

		if (info->gles_head[disp_idx] >= 0) {
			total_cnt -= info->gles_tail[disp_idx] -
				     info->gles_head[disp_idx];
		}

		if (get_layering_opt(LYE_OPT_EXT_LAYER)) {
			for (i = 0; i < info->layer_num[disp_idx]; i++) {
				layer_info = &info->input_config[disp_idx][i];
				if (is_extended_layer(layer_info) &&
				    !mtk_is_gles_layer(info, disp_idx, i))
					total_cnt--;
			}
		}
	}
	return total_cnt;
}

int mtk_get_phy_layer_limit(uint16_t layer_map_tb)
{
	int total_cnt = 0;
	int i;

	for (i = 0; i < 16; i++) {
		if (layer_map_tb & 0x1)
			total_cnt++;
		layer_map_tb >>= 1;
	}
	return total_cnt;
}

static int get_ovl_by_phy(struct drm_device *dev, int disp_idx,
			  uint16_t layer_map_tb, int phy_layer_idx)
{
	uint16_t ovl_mapping_tb;
	int i, ovl_idx = 0, layer_idx = 0;

	ovl_mapping_tb =
		l_rule_ops->get_mapping_table(dev, disp_idx, DISP_HW_OVL_TB, 0);
	for (layer_idx = 0; layer_idx < MAX_PHY_OVL_CNT; layer_idx++) {
		if (layer_map_tb & 0x1) {
			if (phy_layer_idx == 0)
				break;
			phy_layer_idx--;
		}
		layer_map_tb >>= 1;
	}

	if (layer_idx == MAX_PHY_OVL_CNT) {
		DDPPR_ERR("%s fail, phy_layer_idx:%d\n", __func__,
			  phy_layer_idx);
		return -1;
	}

	for (i = 0; i < layer_idx; i++) {
		if (ovl_mapping_tb & 0x1)
			ovl_idx++;
		ovl_mapping_tb >>= 1;
	}
#ifdef HRT_DEBUG_LEVEL2
	DDPMSG("%s,phy:%d,layer_tb:0x%x,L_idx:%d ovl_idx:%d, ov_tb:0x%x\n",
	       __func__, phy_layer_idx, layer_map_tb, layer_idx, ovl_idx,
	       ovl_mapping_tb);
#endif
	return ovl_idx;
}

static int get_phy_ovl_index(struct drm_device *dev, int disp_idx,
			     int layer_idx)
{
	uint16_t ovl_mapping_tb =
		l_rule_ops->get_mapping_table(dev, disp_idx, DISP_HW_OVL_TB, 0);
	int phy_layer_cnt, layer_flag;

	phy_layer_cnt = 0;
	layer_flag = 1 << layer_idx;
	while (layer_idx) {
		layer_idx--;
		layer_flag >>= 1;
		if (ovl_mapping_tb & layer_flag)
			break;
		phy_layer_cnt++;
	}

	return phy_layer_cnt;
}

static int get_larb_by_ovl(struct drm_device *dev, int ovl_idx, int disp_idx)
{
	uint16_t larb_mapping_tb;
	int larb_idx;

	larb_mapping_tb = l_rule_ops->get_mapping_table(dev, disp_idx,
							DISP_HW_LARB_TB, 0);
	larb_idx = (larb_mapping_tb >> ovl_idx * 4) & 0xF;

	return larb_idx;
}

static void dump_disp_info(struct drm_mtk_layering_info *disp_info,
			   enum DISP_DEBUG_LEVEL debug_level)
{
	int i, j;
	struct drm_mtk_layer_config *layer_info;

#define _HRT_FMT \
	"HRT hrt_num:0x%x/mod:%d/dal:%d/addon_scn:(%d, %d, %d)/bd_tb:%d/i:%d\n"
#define _L_FMT \
	"L%d->%d/(%d,%d,%d,%d)/(%d,%d,%d,%d)/f0x%x/ds%d/e%d/cap0x%x/compr%d/secure%d\n"

	if (debug_level < DISP_DEBUG_LEVEL_INFO) {
		DDPMSG(_HRT_FMT,
			disp_info->hrt_num,
			disp_info->disp_mode_idx[0],
			l_rule_info->dal_enable,
			l_rule_info->addon_scn[HRT_PRIMARY],
			l_rule_info->addon_scn[HRT_SECONDARY],
			l_rule_info->addon_scn[HRT_THIRD],
			l_rule_info->bound_tb_idx,
			roll_gpu_for_idle);

		for (i = 0; i < HRT_TYPE_NUM; i++) {
			if (disp_info->layer_num[i] <= 0)
				continue;

			DDPMSG("HRT D%d/M%d/LN%d/hrt:0x%x/G(%d,%d)/id%u\n", i,
			       disp_info->disp_mode[i], disp_info->layer_num[i],
			       disp_info->hrt_num, disp_info->gles_head[i],
			       disp_info->gles_tail[i], disp_info->hrt_idx);

			for (j = 0; j < disp_info->layer_num[i]; j++) {
				layer_info = &disp_info->input_config[i][j];
				DDPMSG(_L_FMT, j, layer_info->ovl_id,
				       layer_info->src_offset_x,
				       layer_info->src_offset_y,
				       layer_info->src_width,
				       layer_info->src_height,
				       layer_info->dst_offset_x,
				       layer_info->dst_offset_y,
				       layer_info->dst_width,
				       layer_info->dst_height,
				       layer_info->src_fmt,
				       layer_info->dataspace,
				       layer_info->ext_sel_layer,
				       layer_info->layer_caps,
				       layer_info->compress,
				       layer_info->secure);
			}
		}
	} else {
		DDPINFO(_HRT_FMT, disp_info->hrt_num,
			disp_info->disp_mode_idx[0],
			l_rule_info->dal_enable,
			l_rule_info->addon_scn[HRT_PRIMARY],
			l_rule_info->addon_scn[HRT_SECONDARY],
			l_rule_info->addon_scn[HRT_THIRD],
			l_rule_info->bound_tb_idx,
			roll_gpu_for_idle);

		for (i = 0; i < HRT_TYPE_NUM; i++) {
			if (disp_info->layer_num[i] <= 0)
				continue;

			DDPINFO("HRT D%d/M%d/LN%d/hrt:0x%x/G(%d,%d)/id%u\n", i,
				disp_info->disp_mode[i],
				disp_info->layer_num[i], disp_info->hrt_num,
				disp_info->gles_head[i],
				disp_info->gles_tail[i], disp_info->hrt_idx);

			for (j = 0; j < disp_info->layer_num[i]; j++) {
				layer_info = &disp_info->input_config[i][j];
				DDPINFO(_L_FMT, j, layer_info->ovl_id,
					layer_info->src_offset_x,
					layer_info->src_offset_y,
					layer_info->src_width,
					layer_info->src_height,
					layer_info->dst_offset_x,
					layer_info->dst_offset_y,
					layer_info->dst_width,
					layer_info->dst_height,
					layer_info->src_fmt,
					layer_info->dataspace,
					layer_info->ext_sel_layer,
					layer_info->layer_caps,
					layer_info->compress,
					layer_info->secure);
			}
		}
	}
}

static void dump_disp_trace(struct drm_mtk_layering_info *disp_info)
{
	int i, j;
	struct drm_mtk_layer_config *c;
	const int len = 1000;
	char msg[len];
	int n = 0;

	for (i = 0; i < HRT_TYPE_NUM; i++) {
		if (disp_info->layer_num[i] <= 0)
			continue;

		n = snprintf(msg, len, "D%d,ovp:%d,dal:%d,LN:%d,G(%d,%d)",
			     i, disp_info->hrt_weight, l_rule_info->dal_enable,
			     disp_info->layer_num[i], disp_info->gles_head[i],
			     disp_info->gles_tail[i]);

		for (j = 0; j < disp_info->layer_num[i]; j++) {
			c = &disp_info->input_config[i][j];
			n += snprintf(msg + n, len - n,
				      "|L%d->%d(%u,%u,%ux%u),f:0x%x,c:%d",
				      j, c->ovl_id, c->dst_offset_x,
				      c->dst_offset_y, c->dst_width,
				      c->dst_height, c->src_fmt, c->compress);
		}

		trace_layer_layout(msg);
	}
}

static void
print_disp_info_to_log_buffer(struct drm_mtk_layering_info *disp_info)
{
/*Todo: support fix log buffer*/
#if 0
	char *status_buf;
	int i, j, n;
	struct drm_mtk_layer_config *layer_info;

	status_buf = get_dprec_status_ptr(0);
	if (status_buf == NULL)
		return;

	n = 0;
	n += snprintf(status_buf + n, LOGGER_BUFFER_SIZE - n,
		"Last hrt query data[start]\n");
	for (i = 0; i < 2; i++) {
		n += snprintf(status_buf + n, LOGGER_BUFFER_SIZE - n,
			"HRT D%d/M%d/LN%d/hrt_num:%d/G(%d,%d)/fps:%d\n",
			i, disp_info->disp_mode[i], disp_info->layer_num[i],
			disp_info->hrt_num, disp_info->gles_head[i],
			disp_info->gles_tail[i], l_rule_info->primary_fps);

		for (j = 0; j < disp_info->layer_num[i]; j++) {
			layer_info = &disp_info->input_config[i][j];
			n += snprintf(status_buf + n, LOGGER_BUFFER_SIZE - n,
			"L%d->%d/of(%d,%d)/wh(%d,%d)/fmt:0x%x/compr:%u\n",
				j, layer_info->ovl_id,
				layer_info->dst_offset_x,
				layer_info->dst_offset_y,
				layer_info->dst_width,
				layer_info->dst_height,
				layer_info->src_fmt,
				layer_info->compress);
		}
	}
	n += snprintf(status_buf + n, LOGGER_BUFFER_SIZE - n,
		"Last hrt query data[end]\n");
#endif
}

void mtk_rollback_layer_to_GPU(struct drm_mtk_layering_info *disp_info,
			       int disp_idx, int i)
{
	if (mtk_is_layer_id_valid(disp_info, disp_idx, i) == false)
		return;

	if (disp_info->gles_head[disp_idx] == -1 ||
	    disp_info->gles_head[disp_idx] > i)
		disp_info->gles_head[disp_idx] = i;
	if (disp_info->gles_tail[disp_idx] == -1 ||
	    disp_info->gles_tail[disp_idx] < i)
		disp_info->gles_tail[disp_idx] = i;
	disp_info->input_config[disp_idx][i].ext_sel_layer = -1;
}

/* rollback and set NO_FBDC flag */
void mtk_rollback_compress_layer_to_GPU(struct drm_mtk_layering_info *disp_info,
					int disp_idx, int i)
{
	if (mtk_is_layer_id_valid(disp_info, disp_idx, i) == false)
		return;

	if (disp_info->input_config[disp_idx][i].compress == 0)
		return;

	mtk_rollback_layer_to_GPU(disp_info, disp_idx, i);
	disp_info->input_config[disp_idx][i].layer_caps |= MTK_NO_FBDC;
}

int mtk_rollback_resize_layer_to_GPU_range(
	struct drm_mtk_layering_info *disp_info, int disp_idx, int start_idx,
	int end_idx)
{
	int i;
	struct drm_mtk_layer_config *lc;

	if (disp_info->layer_num[disp_idx] <= 0) {
		/* direct skip */
		return 0;
	}

	if (start_idx < 0 || end_idx >= disp_info->layer_num[disp_idx])
		return -EINVAL;

	for (i = start_idx; i <= end_idx; i++) {
		lc = &disp_info->input_config[disp_idx][i];
		if ((lc->src_height != lc->dst_height) ||
		    (lc->src_width != lc->dst_width)) {
			if (mtk_has_layer_cap(lc, MTK_MDP_RSZ_LAYER))
				continue;

			if (disp_info->gles_head[disp_idx] == -1 ||
			    disp_info->gles_head[disp_idx] > i)
				disp_info->gles_head[disp_idx] = i;
			if (disp_info->gles_tail[disp_idx] == -1 ||
			    disp_info->gles_tail[disp_idx] < i)
				disp_info->gles_tail[disp_idx] = i;
		}
	}

	if (disp_info->gles_head[disp_idx] != -1) {
		for (i = disp_info->gles_head[disp_idx];
		     i <= disp_info->gles_tail[disp_idx]; i++) {
			lc = &disp_info->input_config[disp_idx][i];
			lc->ext_sel_layer = -1;
		}
	}

	return 0;
}

int mtk_rollback_all_resize_layer_to_GPU(
	struct drm_mtk_layering_info *disp_info, int disp_idx)
{
	mtk_rollback_resize_layer_to_GPU_range(
		disp_info, disp_idx, 0, disp_info->layer_num[disp_idx] - 1);

	return 0;
}

static int _rollback_to_GPU_bottom_up(struct drm_mtk_layering_info *info,
				      int disp, int ovl_limit)
{
	int available_ovl_num, i, j;
	struct drm_mtk_layer_config *l_info;

	available_ovl_num = ovl_limit;
	for (i = 0; i < info->layer_num[disp]; i++) {
		l_info = &info->input_config[disp][i];
		if (is_extended_layer(l_info))
			continue;
		available_ovl_num--;

		if (mtk_is_gles_layer(info, disp, i)) {
			info->gles_head[disp] = i;
			if (info->gles_tail[disp] == -1) {
				info->gles_tail[disp] = i;
				for (j = i + 1; j < info->layer_num[disp];
				     j++) {
					l_info = &info->input_config[disp][j];
					if (is_extended_layer(l_info))
						info->gles_tail[disp] = j;
					else
						break;
				}
			}
			break;
		} else if (available_ovl_num <= 0) {
			available_ovl_num = 0;
			info->gles_head[disp] = i;
			info->gles_tail[disp] = info->layer_num[disp] - 1;
			break;
		}
	}

	if (available_ovl_num < 0)
		DDPPR_ERR("%s available_ovl_num invalid:%d\n", __func__,
			  available_ovl_num);

	return available_ovl_num;
}

static int _rollback_to_GPU_top_down(struct drm_mtk_layering_info *disp_info,
				     int disp, int ovl_limit)
{
	int available_ovl_num, i;
	int tmp_ext = -1;
	struct drm_mtk_layer_config *layer_info;

	available_ovl_num = ovl_limit;
	for (i = disp_info->layer_num[disp] - 1; i > disp_info->gles_tail[disp];
	     i--) {
		layer_info = &disp_info->input_config[disp][i];
		if (!is_extended_layer(layer_info)) {
			if (mtk_is_gles_layer(disp_info, disp, i))
				break;
			if (available_ovl_num <= 0) {
				available_ovl_num = 0;
				if (tmp_ext == -1)
					disp_info->gles_tail[disp] = i;
				else
					disp_info->gles_tail[disp] = tmp_ext;
				break;
			}
			tmp_ext = -1;
			available_ovl_num--;
		} else {
			if (tmp_ext == -1)
				tmp_ext = i;
		}
	}

	if (available_ovl_num < 0)
		DDPPR_ERR("%s available_ovl_num invalid:%d\n", __func__,
			  available_ovl_num);

	return available_ovl_num;
}

static int rollback_to_GPU(struct drm_mtk_layering_info *info, int disp,
			   int available)
{
	int available_ovl_num, i;
	bool has_gles_layer = false;
	struct drm_mtk_layer_config *l_info;

	available_ovl_num = available;

	if (info->gles_head[disp] != -1)
		has_gles_layer = true;

	available_ovl_num =
		_rollback_to_GPU_bottom_up(info, disp, available_ovl_num);
	if (has_gles_layer)
		available_ovl_num = _rollback_to_GPU_top_down(
			info, disp, available_ovl_num);

	if (info->gles_head[disp] == -1 && info->gles_tail[disp] == -1)
		goto out;

	if (mtk_is_layer_id_valid(info, disp, info->gles_head[disp]) == false) {
		dump_disp_info(info, DISP_DEBUG_LEVEL_CRITICAL);
		DDPAEE("invalid gles_head:%d, aval:%d\n",
			  info->gles_head[disp], available);
		goto out;
	}

	if (mtk_is_layer_id_valid(info, disp, info->gles_tail[disp]) == false) {
		dump_disp_info(info, DISP_DEBUG_LEVEL_CRITICAL);
		DDPAEE("invalid gles_tail:%d, aval:%d\n",
			  info->gles_tail[disp], available);
		goto out;
	}

	/* Clear extended layer for all GLES layer */
	for (i = info->gles_head[disp]; i <= info->gles_tail[disp]; i++) {
		l_info = &info->input_config[disp][i];
		l_info->ext_sel_layer = -1;
	}

	if (info->gles_tail[disp] + 1 < info->layer_num[disp]) {
		l_info = &info->input_config[disp][info->gles_tail[disp] + 1];
		if (is_extended_layer(l_info))
			l_info->ext_sel_layer = -1;
	}

out:
	return available_ovl_num;
}

static bool unset_disp_rsz_attr(struct drm_mtk_layering_info *disp_info,
				int disp_idx)
{
	struct drm_mtk_layer_config *c =
		&disp_info->input_config[disp_idx][HRT_PRIMARY];

	if (l_rule_info->addon_scn[HRT_PRIMARY] == ONE_SCALING &&
	    mtk_has_layer_cap(c, MTK_MDP_RSZ_LAYER) &&
	    mtk_has_layer_cap(c, MTK_DISP_RSZ_LAYER)) {
		c->layer_caps &= ~MTK_DISP_RSZ_LAYER;
		l_rule_info->addon_scn[HRT_PRIMARY] = NONE;
		return true;
	}
	return false;
}

static int _filter_by_ovl_cnt(struct drm_device *dev,
			      struct drm_mtk_layering_info *disp_info,
			      int disp_idx)
{
	int ovl_num_limit, phy_ovl_cnt;
	uint16_t l_tb;

	if (disp_info->layer_num[disp_idx] <= 0) {
		/* direct skip */
		return 0;
	}

retry:
	phy_ovl_cnt = get_phy_ovl_layer_cnt(disp_info, disp_idx);
	l_tb = l_rule_ops->get_mapping_table(dev, disp_idx, DISP_HW_LAYER_TB,
					     MAX_PHY_OVL_CNT);

	ovl_num_limit = mtk_get_phy_layer_limit(l_tb);
	if (disp_idx == 0 && l_rule_info->dal_enable)
		ovl_num_limit--;

#ifdef HRT_DEBUG_LEVEL2
	DDPMSG("phy_ovl_cnt:%d,ovl_n_limit:%d\n", phy_ovl_cnt, ovl_num_limit);
#endif
	if (phy_ovl_cnt <= ovl_num_limit)
		return 0;

	if (unset_disp_rsz_attr(disp_info, disp_idx))
		goto retry;

	rollback_to_GPU(disp_info, disp_idx, ovl_num_limit);
	return 0;
}

static void ext_id_adjustment_and_retry(struct drm_device *dev,
					struct drm_mtk_layering_info *info,
					int disp_idx, int layer_idx)
{
	int j, ext_idx;
	struct drm_mtk_layer_config *layer_info;

	ext_idx = -1;
	for (j = layer_idx; j < layer_idx + 3; j++) {
		layer_info = &info->input_config[disp_idx][j];

		if (ext_idx == -1) {
			layer_info->ext_sel_layer = -1;
			if (is_extended_base_layer_valid(layer_info, j))
				ext_idx = j;
		} else {
			layer_info->ext_sel_layer = ext_idx;
		}
		if (j == (info->layer_num[disp_idx] - 1) ||
		    !is_extended_layer(&info->input_config[disp_idx][j + 1]))
			break;
	}
#ifdef HRT_DEBUG_LEVEL2
	DDPMSG("[%s]cannot feet current layer layout\n", __func__);
	dump_disp_info(info, DISP_DEBUG_LEVEL_ERR);
#endif
	ext_id_tuning(dev, info, disp_idx);
}

static int ext_id_tuning(struct drm_device *dev,
			  struct drm_mtk_layering_info *info, int disp)
{
	uint16_t ovl_tb, l_tb;
	int phy_ovl_cnt, i;
	int ext_cnt = 0, cur_phy_cnt = 0;
	struct drm_mtk_layer_config *layer_info;

	if (info->layer_num[disp] <= 0) {
		/* direct skip */
		return 0;
	}

	_filter_by_ovl_cnt(dev, info, disp);
	phy_ovl_cnt = get_phy_ovl_layer_cnt(info, disp);
	if (phy_ovl_cnt > MAX_PHY_OVL_CNT) {
		DDPPR_ERR("phy_ovl_cnt(%d) over OVL count limit\n",
			  phy_ovl_cnt);
		phy_ovl_cnt = MAX_PHY_OVL_CNT;
	}

	ovl_tb = l_rule_ops->get_mapping_table(dev, disp, DISP_HW_OVL_TB, 0);
	l_tb = l_rule_ops->get_mapping_table(dev, disp, DISP_HW_LAYER_TB,
					     phy_ovl_cnt);

	if (l_rule_info->dal_enable) {
		l_tb = l_rule_ops->get_mapping_table(
			dev, disp, DISP_HW_LAYER_TB, MAX_PHY_OVL_CNT);
		l_tb &= HRT_AEE_LAYER_MASK;
	}

	for (i = 0; i < info->layer_num[disp]; i++) {
		layer_info = &info->input_config[disp][i];
		if (is_extended_layer(layer_info)) {
			ext_cnt++;
			if (is_extended_over_limit(ext_cnt)) {
				ext_id_adjustment_and_retry(dev, info, disp, i);
				break;
			}
		} else {
#ifdef HRT_DEBUG_LEVEL2
			DDPMSG("i:%d, cur_phy_cnt:%d\n", i, cur_phy_cnt);
#endif
			if (mtk_is_gles_layer(info, disp, i) &&
			    (i != info->gles_head[disp])) {
#ifdef HRT_DEBUG_LEVEL2
				DDPMSG("is gles layer, continue\n");
#endif
				continue;
			}
			if (cur_phy_cnt > 0) {
				int cur_ovl, pre_ovl;

				cur_ovl = get_ovl_by_phy(dev, disp, l_tb,
							 cur_phy_cnt);
				pre_ovl = get_ovl_by_phy(dev, disp, l_tb,
							 cur_phy_cnt - 1);
				if (cur_ovl != pre_ovl)
					ext_cnt = 0;
			}
			cur_phy_cnt++;
		}
	}

	return 0;
}

static int rollback_all_to_GPU(struct drm_mtk_layering_info *disp_info,
			       int disp_idx)
{
	if (disp_info->layer_num[disp_idx] <= 0) {
		/* direct skip */
		return 0;
	}

	disp_info->gles_head[disp_idx] = 0;
	disp_info->gles_tail[disp_idx] = disp_info->layer_num[disp_idx] - 1;
	return 0;
}

static int filter_by_ovl_cnt(struct drm_device *dev,
			     struct drm_mtk_layering_info *disp_info)
{
	int ret, disp_idx;

	/* 0->primary display, 1->secondary display */
	for (disp_idx = 0; disp_idx < HRT_TYPE_NUM; disp_idx++) {
		if (get_layering_opt(LYE_OPT_EXT_LAYER))
			ret = ext_id_tuning(dev, disp_info, disp_idx);
		else
			ret = _filter_by_ovl_cnt(dev, disp_info, disp_idx);
	}

#ifdef HRT_DEBUG_LEVEL2
	DDPMSG("[%s result]\n", __func__);
	dump_disp_info(disp_info, DISP_DEBUG_LEVEL_INFO);
#endif
	return ret;
}

static struct hrt_sort_entry *x_entry_list, *y_entry_list;

#ifdef HRT_DEBUG_LEVEL2
static int dump_entry_list(bool sort_by_y)
{
	struct hrt_sort_entry *temp;
	struct drm_mtk_layer_config *layer_info;

	if (sort_by_y)
		temp = y_entry_list;
	else
		temp = x_entry_list;

	DDPMSG("%s, sort_by_y:%d, addr:0x%p\n", __func__, sort_by_y, temp);
	while (temp) {
		layer_info = temp->layer_info;
		DDPMSG("key:%d, offset(%d, %d), w/h(%d, %d), overlap_w:%d\n",
		       temp->key, layer_info->dst_offset_x,
		       layer_info->dst_offset_y, layer_info->dst_width,
		       layer_info->dst_height, temp->overlap_w);
		temp = temp->tail;
	}
	DDPMSG("%s end\n", __func__);
	return 0;
}
#endif

static int insert_entry(struct hrt_sort_entry **head,
			struct hrt_sort_entry *sort_entry)
{
	struct hrt_sort_entry *temp;

	temp = *head;
	while (temp) {
		if (sort_entry->key < temp->key ||
		    ((sort_entry->key == temp->key) &&
		     (sort_entry->overlap_w > 0))) {
			sort_entry->head = temp->head;
			sort_entry->tail = temp;
			if (temp->head != NULL)
				temp->head->tail = sort_entry;
			else
				*head = sort_entry;
			temp->head = sort_entry;
			break;
		}

		if (temp->tail == NULL) {
			temp->tail = sort_entry;
			sort_entry->head = temp;
			sort_entry->tail = NULL;
			break;
		}
		temp = temp->tail;
	}

	return 0;
}

static int add_layer_entry(struct drm_mtk_layer_config *l_info, bool sort_by_y,
			   int overlap_w)
{
	struct hrt_sort_entry *begin_t, *end_t;
	struct hrt_sort_entry **p_entry;

	begin_t = kzalloc(sizeof(struct hrt_sort_entry), GFP_KERNEL);
	end_t = kzalloc(sizeof(struct hrt_sort_entry), GFP_KERNEL);

	begin_t->head = NULL;
	begin_t->tail = NULL;
	end_t->head = NULL;
	end_t->tail = NULL;
	if (sort_by_y) {
		begin_t->key = l_info->dst_offset_y;
		end_t->key = l_info->dst_offset_y + l_info->dst_height - 1;
		p_entry = &y_entry_list;
	} else {
		begin_t->key = l_info->dst_offset_x;
		end_t->key = l_info->dst_offset_x + l_info->dst_width - 1;
		p_entry = &x_entry_list;
	}

	begin_t->overlap_w = overlap_w;
	begin_t->layer_info = l_info;
	end_t->overlap_w = -overlap_w;
	end_t->layer_info = l_info;

	if (*p_entry == NULL) {
		*p_entry = begin_t;
		begin_t->head = NULL;
		begin_t->tail = end_t;
		end_t->head = begin_t;
		end_t->tail = NULL;
	} else {
		/* Inser begin entry */
		insert_entry(p_entry, begin_t);
#ifdef HRT_DEBUG_LEVEL2
		DDPMSG("Insert key:%d\n", begin_t->key);
		dump_entry_list(sort_by_y);
#endif
		/* Inser end entry */
		insert_entry(p_entry, end_t);
#ifdef HRT_DEBUG_LEVEL2
		DDPMSG("Insert key:%d\n", end_t->key);
		dump_entry_list(sort_by_y);
#endif
	}

	return 0;
}

static int remove_layer_entry(struct drm_mtk_layer_config *layer_info,
			      bool sort_by_y)
{
	struct hrt_sort_entry *temp, *free_entry;

	if (sort_by_y)
		temp = y_entry_list;
	else
		temp = x_entry_list;

	while (temp) {
		if (temp->layer_info == layer_info) {
			free_entry = temp;
			temp = temp->tail;
			if (free_entry->head == NULL) {
				/* Free head entry */
				if (temp != NULL)
					temp->head = NULL;
				if (sort_by_y)
					y_entry_list = temp;
				else
					x_entry_list = temp;
				kfree(free_entry);
			} else {
				free_entry->head->tail = free_entry->tail;
				if (temp)
					temp->head = free_entry->head;
				kfree(free_entry);
			}
		} else {
			temp = temp->tail;
		}
	}
	return 0;
}

static int free_all_layer_entry(bool sort_by_y)
{
	struct hrt_sort_entry *cur_entry, *next_entry;

	if (sort_by_y)
		cur_entry = y_entry_list;
	else
		cur_entry = x_entry_list;

	while (cur_entry) {
		next_entry = cur_entry->tail;
		kfree(cur_entry);
		cur_entry = next_entry;
	}

	if (sort_by_y)
		y_entry_list = NULL;
	else
		x_entry_list = NULL;

	return 0;
}

static int scan_x_overlap(struct drm_mtk_layering_info *disp_info,
			  int disp_index, int ovl_overlap_limit_w)
{
	struct hrt_sort_entry *tmp_entry;
	int overlap_w_sum, max_overlap;

	overlap_w_sum = 0;
	max_overlap = 0;
	tmp_entry = x_entry_list;
	while (tmp_entry) {
		overlap_w_sum += tmp_entry->overlap_w;
		max_overlap = (overlap_w_sum > max_overlap) ? overlap_w_sum
							    : max_overlap;
		tmp_entry = tmp_entry->tail;
	}
	return max_overlap;
}

static int scan_y_overlap(struct drm_mtk_layering_info *disp_info,
			  int disp_index, int ovl_overlap_limit_w)
{
	struct hrt_sort_entry *tmp_entry;
	int overlap_w_sum, tmp_overlap, max_overlap;

	overlap_w_sum = 0;
	tmp_overlap = 0;
	max_overlap = 0;
	tmp_entry = y_entry_list;
	while (tmp_entry) {
		overlap_w_sum += tmp_entry->overlap_w;
		if (tmp_entry->overlap_w > 0) {
			add_layer_entry(tmp_entry->layer_info, false,
					tmp_entry->overlap_w);
		} else {
			remove_layer_entry(tmp_entry->layer_info, false);
		}

		if (overlap_w_sum > ovl_overlap_limit_w &&
		    overlap_w_sum > max_overlap) {
			tmp_overlap = scan_x_overlap(disp_info, disp_index,
						     ovl_overlap_limit_w);
		} else {
			tmp_overlap = overlap_w_sum;
		}

		max_overlap =
			(tmp_overlap > max_overlap) ? tmp_overlap : max_overlap;
		tmp_entry = tmp_entry->tail;
	}

	return max_overlap;
}

static int get_hrt_level(int sum_w, int is_larb)
{
	int hrt_level;
	int *bound_table;
	enum DISP_HW_MAPPING_TB_TYPE type;

	if (is_larb)
		type = DISP_HW_LARB_BOUND_TB;
	else
		type = DISP_HW_EMI_BOUND_TB;

	bound_table = l_rule_ops->get_bound_table(type);
	for (hrt_level = 0; hrt_level < HRT_LEVEL_NUM; hrt_level++) {
		if (bound_table[hrt_level] != -1 &&
		    sum_w <= bound_table[hrt_level] * HRT_UINT_BOUND_BPP)
			return hrt_level;
	}
	return hrt_level;
}

static bool has_hrt_limit(struct drm_mtk_layering_info *disp_info, int disp_idx)
{
	if (disp_info->layer_num[disp_idx] <= 0)
		return false;

	if (disp_info->disp_mode[disp_idx] == MTK_DRM_SESSION_DC_MIRROR)
		return false;

	return true;
}

static int get_hrt_disp_num(struct drm_mtk_layering_info *disp_info)
{
	int cnt = 0, i;

	for (i = 0; i < HRT_TYPE_NUM; i++)
		if (has_hrt_limit(disp_info, i))
			cnt++;

	return cnt;
}

/**
 * Return the HRT layer weight.
 * If the layer_info is NULL, return GLES layer weight.
 */
static int get_layer_weight(int disp_idx,
			    struct drm_mtk_layer_config *layer_info)
{
	int bpp, weight;

	if (layer_info)
		bpp = mtk_get_format_bpp(layer_info->src_fmt);
	else
		bpp = HRT_UINT_BOUND_BPP;
#ifdef CONFIG_MTK_HDMI_SUPPORT
	if (disp_idx == HRT_SECONDARY) {
		/* To Be Impl */
#if 0
		struct disp_session_info dispif_info;

		/* For seconary display, set the weight 4K@30 as 2K@60.	*/
		hdmi_get_dev_info(true, &dispif_info);

		if (dispif_info.displayWidth > 2560)
			weight = HRT_UINT_WEIGHT * 2;
		else if (dispif_info.displayWidth > 1920)
			weight = HRT_UINT_WEIGHT;
		else
			weight = HRT_UINT_WEIGHT / 2;

		if (dispif_info.vsyncFPS <= 30)
			weight /= 2;

		return weight * bpp;
#endif
	}
#endif

	weight = HRT_UINT_WEIGHT;

	return weight * bpp;
}

static int _calc_hrt_num(struct drm_device *dev,
			 struct drm_mtk_layering_info *disp_info, int disp,
			 int hrt_type, bool force_scan_y, bool has_dal_layer)
{
	int i, sum_overlap_w, overlap_l_bound;
	uint16_t layer_map;
	int overlap_w, layer_idx, phy_layer_idx, ovl_cnt;
	bool has_gles = false;
	struct drm_mtk_layer_config *layer_info;

	if (!has_hrt_limit(disp_info, disp))
		return 0;

	/* 1.Initial overlap conditions. */
	sum_overlap_w = 0;
	/*
	 * The parameters of hrt table are base on ARGB color format.
	 * Multiply the bpp of it.
	 */
	overlap_l_bound = g_emi_bound_table[0] * HRT_UINT_BOUND_BPP;

	/*
	 * 2.Add each layer info to layer list and sort it by yoffset.
	 * Also add up each layer overlap weight.
	 */
	layer_idx = -1;
	ovl_cnt = get_phy_ovl_layer_cnt(disp_info, disp);
	layer_map = l_rule_ops->get_mapping_table(dev, disp, DISP_HW_LAYER_TB,
						  ovl_cnt);

	if (l_rule_info->dal_enable) {
		layer_map = l_rule_ops->get_mapping_table(
			dev, disp, DISP_HW_LAYER_TB, MAX_PHY_OVL_CNT);
		layer_map &= HRT_AEE_LAYER_MASK;
	}

	for (i = 0; i < disp_info->layer_num[disp]; i++) {
		int ovl_idx;

		layer_info = &disp_info->input_config[disp][i];
		if (disp_info->gles_head[disp] == -1 ||
		    (i < disp_info->gles_head[disp] ||
		     i > disp_info->gles_tail[disp])) {
			if (hrt_type != HRT_TYPE_EMI) {
				if (layer_idx == -1)
					layer_idx = 0;
				else if (!is_extended_layer(layer_info))
					layer_idx++;

				phy_layer_idx =
					get_phy_ovl_index(dev, disp, layer_idx);
				ovl_idx = get_ovl_by_phy(dev, disp, layer_map,
							 layer_idx);
				if (get_larb_by_ovl(dev, ovl_idx, disp) !=
				    hrt_type)
					continue;
			}
			overlap_w = get_layer_weight(disp, layer_info);
			sum_overlap_w += overlap_w;
			add_layer_entry(layer_info, true, overlap_w);
		} else if (i == disp_info->gles_head[disp]) {
			/* Add GLES layer */
			if (hrt_type != HRT_TYPE_EMI) {
				if (layer_idx == -1)
					layer_idx = 0;
				else if (!is_extended_layer(layer_info))
					layer_idx++;

				phy_layer_idx =
					get_phy_ovl_index(dev, disp, layer_idx);
				ovl_idx = get_ovl_by_phy(dev, disp, layer_map,
							 layer_idx);

				if (get_larb_by_ovl(dev, ovl_idx, disp) !=
				    hrt_type)
					continue;
			}
			has_gles = true;
		}
	}
	/* Add overlap weight of Gles layer and Assert layer. */
	if (has_gles)
		sum_overlap_w += get_layer_weight(disp, NULL);

	if (has_dal_layer)
		sum_overlap_w += HRT_AEE_WEIGHT;

	/*
	 * 3.Calculate the HRT bound if the total layer weight over the
	 * lower bound or has secondary display.
	 */
	if (sum_overlap_w > overlap_l_bound ||
	    has_hrt_limit(disp_info, HRT_SECONDARY) || force_scan_y) {
		sum_overlap_w =
			scan_y_overlap(disp_info, disp, overlap_l_bound);
		/* Add overlap weight of Gles layer and Assert layer. */
		if (has_gles)
			sum_overlap_w += get_layer_weight(disp, NULL);
		if (has_dal_layer)
			sum_overlap_w += HRT_AEE_WEIGHT;
	}

#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("%s disp:%d, disp:%d, hrt_type:%d, sum_overlap_w:%d\n", __func__,
	       disp, disp, hrt_type, sum_overlap_w);
#endif

	free_all_layer_entry(true);
	return sum_overlap_w;
}

#ifdef HAS_LARB_HRT
static int calc_larb_hrt_level(struct drm_device *dev,
			       struct drm_mtk_layering_info *disp_info)
{
	int larb_hrt_level, i, sum_overlap_w;

	larb_hrt_level = 0;
	for (i = HRT_TYPE_LARB0; i <= HRT_TYPE_LARB1; i++) {
		int tmp_hrt_level;

		sum_overlap_w = _calc_hrt_num(dev, disp_info, HRT_PRIMARY, i,
					      true, l_rule_info->dal_enable);
		sum_overlap_w += _calc_hrt_num(dev, disp_info, HRT_SECONDARY, i,
					       true, false);
		sum_overlap_w += _calc_hrt_num(dev, disp_info, HRT_THIRD, i,
					       true, false);
		tmp_hrt_level = get_hrt_level(sum_overlap_w, true);
		if (tmp_hrt_level > larb_hrt_level)
			larb_hrt_level = tmp_hrt_level;
	}

	return larb_hrt_level;
}
#endif

static int calc_hrt_num(struct drm_device *dev,
			struct drm_mtk_layering_info *disp_info)
{
	int emi_hrt_level;
	int sum_overlap_w = 0;
#ifdef HAS_LARB_HRT
	int larb_hrt_level;
#endif
	int overlap_num;
	/* TODO support display helper */
	/* bool scan_overlap = !!disp_helper_get_option(DISP_OPT_HRT_MODE); */
	bool scan_overlap = true;

	/* Calculate HRT for EMI level */
	if (has_hrt_limit(disp_info, HRT_PRIMARY)) {
		sum_overlap_w =
			_calc_hrt_num(dev, disp_info, HRT_PRIMARY, HRT_TYPE_EMI,
				      scan_overlap, l_rule_info->dal_enable);
	}
	if (has_hrt_limit(disp_info, HRT_SECONDARY)) {
		sum_overlap_w +=
			_calc_hrt_num(dev, disp_info, HRT_SECONDARY,
				      HRT_TYPE_EMI, scan_overlap, false);
	}
	if (has_hrt_limit(disp_info, HRT_THIRD)) {
		sum_overlap_w += _calc_hrt_num(dev, disp_info, HRT_THIRD,
					       HRT_TYPE_EMI, scan_overlap,
					       false);
	}

	emi_hrt_level = get_hrt_level(sum_overlap_w, false);

	overlap_num = sum_overlap_w / 200;

	/*
	 * The larb bound always meets the limit for HRT_LEVEL2
	 * in 8+4 ovl architecture.
	 * So calculate larb bound only for HRT_LEVEL2.
	 */
	disp_info->hrt_num = emi_hrt_level;
#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("EMI hrt lv2:%d,overlap_w:%d\n", emi_hrt_level, sum_overlap_w);
#endif

#ifdef HAS_LARB_HRT
	/* Need to calculate larb hrt for HRT_LEVEL_LOW level. */
	/* TODO: Should revise larb calculation statement here */
	/* if (hrt_level != HRT_LEVEL_NUM - 2) */
	/*	return hrt_level; */

	/* Check Larb Bound here */
	larb_hrt_level = calc_larb_hrt_level(dev, disp_info);

#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("Larb hrt level:%d\n", larb_hrt_level);
#endif

	if (emi_hrt_level < larb_hrt_level)
		disp_info->hrt_num = larb_hrt_level;
	else
#endif
		disp_info->hrt_num = emi_hrt_level;

	return overlap_num;
}

/**
 * dispatch which one layer could be ext layer
 */
static int ext_layer_grouping(struct drm_device *dev,
			      struct drm_mtk_layering_info *disp_info)
{
	int cont_ext_layer_cnt = 0, ext_idx = 0;
	int is_ext_layer, disp_idx, i;
	struct drm_mtk_layer_config *src_info, *dst_info;
	int available_layers = 0, phy_layer_cnt = 0;

	for (disp_idx = 0; disp_idx < HRT_TYPE_NUM; disp_idx++) {
		/* initialize ext layer info */
		for (i = 0; i < disp_info->layer_num[disp_idx]; i++)
			disp_info->input_config[disp_idx][i].ext_sel_layer = -1;

		if (!get_layering_opt(LYE_OPT_EXT_LAYER))
			continue;

#ifndef LAYERING_SUPPORT_EXT_LAYER_ON_2ND_DISP
		if (disp_idx != HRT_PRIMARY)
			continue;
#endif

		/*
		 * If the physical layer > input layer,
		 * then skip using extended layer.
		 */
		phy_layer_cnt =
			mtk_get_phy_layer_limit(l_rule_ops->get_mapping_table(
				dev, disp_idx, DISP_HW_LAYER_TB,
				MAX_PHY_OVL_CNT));
		/* Remove the rule here so that we can have more oppotunity to
		 * test extended layer
		 * if (phy_layer_cnt > disp_info->layer_num[disp_idx])
		 *	continue;
		 */

		for (i = 1; i < disp_info->layer_num[disp_idx]; i++) {
			dst_info = &disp_info->input_config[disp_idx][i];
			src_info = &disp_info->input_config[disp_idx][i - 1];
			/* skip other GPU layers */
			if (mtk_is_gles_layer(disp_info, disp_idx, i) ||
			    mtk_is_gles_layer(disp_info, disp_idx, i - 1)) {
				cont_ext_layer_cnt = 0;
				if (i > disp_info->gles_tail[disp_idx]) {
					int tmp;

					tmp = disp_info->gles_tail[disp_idx] -
					      disp_info->gles_head[disp_idx];
					ext_idx = i - tmp;
				}
				continue;
			}

			is_ext_layer = !is_continuous_ext_layer_overlap(
				disp_info->input_config[disp_idx], i);

			/*
			 * The yuv layer is not supported as extended layer
			 * as the HWC has a special for yuv content.
			 */
			if (mtk_is_yuv(dst_info->src_fmt))
				is_ext_layer = false;

			if (is_ext_layer && cont_ext_layer_cnt < 3) {
				++cont_ext_layer_cnt;
				dst_info->ext_sel_layer = ext_idx;
			} else {
				cont_ext_layer_cnt = 0;
				ext_idx = i;
				if (i > disp_info->gles_tail[disp_idx]) {
					ext_idx -=
						disp_info->gles_tail[disp_idx] -
						disp_info->gles_head[disp_idx];
				}
			}
		}
	}

#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("[ext layer grouping]\n");
	dump_disp_info(disp_info, DISP_DEBUG_LEVEL_INFO);
#endif

	return available_layers;
}

void lye_add_lye_priv_blob(struct mtk_plane_comp_state *comp_state,
			   struct mtk_drm_lyeblob_ids *lyeblob_ids,
			   int plane_idx, int disp_idx,
			   struct drm_device *drm_dev)
{
	struct drm_property_blob *blob;

	blob = drm_property_create_blob(
		drm_dev, sizeof(struct mtk_plane_comp_state), comp_state);
	lyeblob_ids->lye_plane_blob_id[disp_idx][plane_idx] = blob->base.id;
}

static int mtk_lye_get_comp_id(int disp_idx, struct drm_device *drm_dev,
			       int layer_map_idx)
{
	uint16_t ovl_mapping_tb = l_rule_ops->get_mapping_table(
		drm_dev, disp_idx, DISP_HW_OVL_TB, 0);
	struct mtk_drm_private *priv = drm_dev->dev_private;

	/* TODO: The component ID should be changed by ddp path and platforms */
	if (disp_idx == 0) {
		if (HRT_GET_FIRST_SET_BIT(ovl_mapping_tb) >= layer_map_idx)
			return DDP_COMPONENT_DMDP_RDMA0;
		/* When open VDS path switch feature, primary OVL have OVL0 only */
		else if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_VDS_PATH_SWITCH) &&
			priv->need_vds_path_switch)
			return DDP_COMPONENT_OVL0;
		else if (HRT_GET_FIRST_SET_BIT(
				 ovl_mapping_tb -
				 HRT_GET_FIRST_SET_BIT(ovl_mapping_tb)) >=
			 layer_map_idx)
			return DDP_COMPONENT_OVL0_2L;
		else
			return DDP_COMPONENT_OVL0;
	}
#if defined(CONFIG_MACH_MT6885)
	else if (disp_idx == 1)
		return DDP_COMPONENT_OVL2_2L;
	else
		return DDP_COMPONENT_OVL1_2L;
#else
	/* When open VDS path switch feature, vds OVL is OVL0_2L */
	else if (mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_VDS_PATH_SWITCH))
		return DDP_COMPONENT_OVL0_2L;
	else
		return DDP_COMPONENT_OVL2_2L;
#endif
}

static int mtk_lye_get_lye_id(int disp_idx, struct drm_device *drm_dev,
			      int layer_map_idx)
{
	int cnt = 0;

	if (layer_map_idx != 0)
		while (!(layer_map_idx & 0x1)) {
			cnt++;
			layer_map_idx >>= 1;
		}
	layer_map_idx = cnt;
	return get_phy_ovl_index(drm_dev, disp_idx, layer_map_idx);
}

static void clear_layer(struct drm_mtk_layering_info *disp_info)
{
	int di = 0;
	int i = 0;
	struct drm_mtk_layer_config *c;

	if (!get_layering_opt(LYE_OPT_CLEAR_LAYER))
		return;

	for (di = 0; di < HRT_TYPE_NUM; di++) {
		int g_head = disp_info->gles_head[di];
		int top = -1;

		if (disp_info->layer_num[di] <= 0)
			continue;
		if (g_head == -1)
			continue;

		for (i = disp_info->layer_num[di] - 1; i >= g_head; i--) {
			c = &disp_info->input_config[di][i];
			if (mtk_has_layer_cap(c, MTK_LAYERING_OVL_ONLY) &&
			    mtk_has_layer_cap(c, MTK_CLIENT_CLEAR_LAYER)) {
				top = i;
				break;
			}
		}
		if (top == -1)
			continue;
		if (!mtk_is_gles_layer(disp_info, di, top))
			continue;

		DDPMSG("%s:D%d:L%d\n", __func__, di, top);

		disp_info->gles_head[di] = 0;
		disp_info->gles_tail[di] = disp_info->layer_num[di] - 1;

		c = &disp_info->input_config[di][top];
		if (top == disp_info->gles_head[di])
			disp_info->gles_head[di]++;
		else if (top == disp_info->gles_tail[di])
			disp_info->gles_tail[di]--;
		else
			c->layer_caps |= MTK_DISP_CLIENT_CLEAR_LAYER;

		if ((c->src_width < c->dst_width &&
		     c->src_height < c->dst_height) &&
		     get_layering_opt(LYE_OPT_RPO) &&
		    top < disp_info->gles_tail[di]) {
			c->layer_caps |= MTK_DISP_RSZ_LAYER;
			l_rule_info->addon_scn[di] = ONE_SCALING;
		} else {
			c->layer_caps &= ~MTK_DISP_RSZ_LAYER;
			l_rule_info->addon_scn[di] = NONE;

			if ((c->src_width != c->dst_width ||
			     c->src_height != c->dst_height) &&
			    !mtk_has_layer_cap(c, MTK_MDP_RSZ_LAYER)) {
				c->layer_caps &= ~MTK_DISP_CLIENT_CLEAR_LAYER;
				DDPMSG("%s:remove clear(rsz), caps:0x%08x\n",
				       __func__, c->layer_caps);
			}
		}

		for (i = 0; i < disp_info->layer_num[di]; i++) {
			c = &disp_info->input_config[di][i];
			c->ext_sel_layer = -1;
			if (i != top)
				c->layer_caps &= ~MTK_DISP_RSZ_LAYER;
		}
	}
}

static int _dispatch_lye_blob_idx(struct drm_mtk_layering_info *disp_info,
				  int layer_map, int disp_idx,
				  struct mtk_drm_lyeblob_ids *lyeblob_ids,
				  struct drm_device *drm_dev)
{
	struct drm_mtk_layer_config *layer_info;
	int ext_cnt = 0, plane_idx = 0, layer_map_idx = 0;
	struct mtk_plane_comp_state comp_state;
	int prev_comp_id = -1;
	int i;
	int clear_idx = -1;
#if defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833)
	int no_compress_layer_num = 0;
#endif

	for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
		layer_info = &disp_info->input_config[disp_idx][i];
		if (mtk_has_layer_cap(layer_info,
				      MTK_DISP_CLIENT_CLEAR_LAYER)) {
			clear_idx = i;
			break;
		}
	}

	for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {

		layer_info = &disp_info->input_config[disp_idx][i];

		if (clear_idx < 0 &&
		    mtk_has_layer_cap(layer_info, MTK_DISP_CLIENT_CLEAR_LAYER))
			continue;

		if (i < clear_idx) {
			continue;
		} else if (i == clear_idx) {
			i = -1;
			clear_idx = -1;
		}

		comp_state.layer_caps = layer_info->layer_caps;

		if (mtk_is_gles_layer(disp_info, disp_idx, i) &&
		    i != disp_info->gles_head[disp_idx]) {
			layer_info->ovl_id = plane_idx - 1;
			continue;
		}

		if (!is_extended_layer(layer_info))
			layer_map &= ~layer_map_idx;

		layer_map_idx = HRT_GET_FIRST_SET_BIT(layer_map);
		comp_state.comp_id =
			mtk_lye_get_comp_id(disp_idx, drm_dev, layer_map_idx);
		comp_state.lye_id =
			mtk_lye_get_lye_id(disp_idx, drm_dev, layer_map_idx);

		if (is_extended_layer(layer_info)) {
			comp_state.ext_lye_id = LYE_EXT0 + ext_cnt;
			ext_cnt++;
		} else {
			if (comp_state.comp_id != prev_comp_id)
				ext_cnt = 0;
			comp_state.ext_lye_id = LYE_NORMAL;
		}
#if defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833)
		if (disp_idx == 0 &&
			(comp_state.comp_id == DDP_COMPONENT_OVL0_2L) &&
			!is_extended_layer(layer_info) &&
			layer_info->compress != 1) {

			DDPINFO("%s layer_id %d no compress phy layer\n",
				__func__, i);
			no_compress_layer_num++;
		}
#endif
		lye_add_lye_priv_blob(&comp_state, lyeblob_ids, plane_idx,
				      disp_idx, drm_dev);

		layer_info->ovl_id = plane_idx;
		plane_idx++;
		prev_comp_id = comp_state.comp_id;
	}
#if defined(CONFIG_MACH_MT6853) || defined(CONFIG_MACH_MT6833)
	if (disp_idx == 0) {
		HRT_SET_NO_COMPRESS_FLAG(disp_info->hrt_num,
				no_compress_layer_num);
		DDPINFO("%s disp_info->hrt_num=0x%x,no_comp_layer_num=%d\n",
				__func__, disp_info->hrt_num,
				no_compress_layer_num);
	}
#endif
	return 0;
}

static int dispatch_ovl_id(struct drm_mtk_layering_info *disp_info,
			   struct mtk_drm_lyeblob_ids *lyeblob_ids,
			   struct drm_device *drm_dev)
{
	int disp_idx;
	bool no_disp = true;

	for (disp_idx = 0; disp_idx < HRT_TYPE_NUM; disp_idx++)
		if (disp_info->layer_num[disp_idx] > 0) {
			no_disp = false;
			break;
		}

	if (no_disp) {
		DDPINFO("There is no disp need dispatch\n");
		return 0;
	}

	/* Dispatch gles range if necessary */
	if (disp_info->hrt_num > HRT_LEVEL_NUM - 1) {
		int max_ovl_cnt = g_emi_bound_table[HRT_LEVEL_NUM - 1];
		int valid_ovl_cnt = max_ovl_cnt;
		int hrt_disp_num = get_hrt_disp_num(disp_info);

		if (l_rule_info->dal_enable)
			valid_ovl_cnt -= (HRT_AEE_WEIGHT / HRT_UINT_BOUND_BPP);
		valid_ovl_cnt /= HRT_UINT_WEIGHT;

		hrt_disp_num--;
		for (disp_idx = HRT_TYPE_NUM - 1; disp_idx >= 0; disp_idx--) {
			if (!has_hrt_limit(disp_info, disp_idx))
				continue;
			valid_ovl_cnt =
				rollback_to_GPU(disp_info, disp_idx,
					valid_ovl_cnt - hrt_disp_num);
			valid_ovl_cnt += hrt_disp_num;
			hrt_disp_num--;
		}

		/* ajust hrt_num */
		disp_info->hrt_num =
			get_hrt_level(max_ovl_cnt * HRT_UINT_BOUND_BPP, 0);
		disp_info->hrt_weight = max_ovl_cnt * 2 / HRT_UINT_WEIGHT;
	}

	clear_layer(disp_info);

	/* Dispatch OVL id */
	for (disp_idx = 0; disp_idx < HRT_TYPE_NUM; disp_idx++) {
		int ovl_cnt;
		uint16_t layer_map;
		struct drm_mtk_layer_config *c;
		bool clear = false;
		int i = 0;

		if (disp_info->layer_num[disp_idx] <= 0)
			continue;

		for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
			c = &disp_info->input_config[disp_idx][i];
			if (mtk_has_layer_cap(c, MTK_DISP_CLIENT_CLEAR_LAYER)) {
				clear = true;
				break;
			}
		}

		ovl_cnt = get_phy_ovl_layer_cnt(disp_info, disp_idx);
		if (clear)
			ovl_cnt++;
		layer_map = l_rule_ops->get_mapping_table(
			drm_dev, disp_idx, DISP_HW_LAYER_TB, ovl_cnt);

		if (l_rule_info->dal_enable) {
			layer_map = l_rule_ops->get_mapping_table(
				drm_dev, disp_idx, DISP_HW_LAYER_TB,
				MAX_PHY_OVL_CNT);
			layer_map &= HRT_AEE_LAYER_MASK;
		}
		_dispatch_lye_blob_idx(disp_info, layer_map,
			disp_idx, lyeblob_ids, drm_dev);
	}
	return 0;
}

static int check_layering_result(struct drm_mtk_layering_info *info)
{
	int disp_idx;
	bool no_disp = true;

	for (disp_idx = 0; disp_idx < HRT_TYPE_NUM; disp_idx++)
		if (info->layer_num[disp_idx] > 0) {
			no_disp = false;
			break;
		}

	if (no_disp) {
		DDPINFO("There is no disp need check\n");
		return 0;
	}

	for (disp_idx = 0; disp_idx < HRT_TYPE_NUM; disp_idx++) {
		int layer_num, max_ovl_id, ovl_layer_num;

		if (info->layer_num[disp_idx] <= 0)
			continue;

		if (disp_idx == HRT_PRIMARY)
			ovl_layer_num = PRIMARY_OVL_LAYER_NUM;
		else
			ovl_layer_num = SECONDARY_OVL_LAYER_NUM;
		layer_num = info->layer_num[disp_idx];
		max_ovl_id = info->input_config[disp_idx][layer_num - 1].ovl_id;

		if (max_ovl_id >= ovl_layer_num)
			DDPAEE("Inv ovl:%d,disp:%d\n", max_ovl_id, disp_idx);
	}
	return 0;
}

static int check_disp_info(struct drm_mtk_layering_info *disp_info)
{
	int disp_idx, ghead, gtail;
	int i;

	if (disp_info == NULL) {
		DDPPR_ERR("[HRT]disp_info is empty\n");
		return -1;
	}

	for (i = 0; i < 3; i++) {
		int mode = disp_info->disp_mode[i];
		int layer_num = disp_info->layer_num[i];

		if (mode < 0 || mode >= MTK_DRM_SESSION_NUM) {
			DDPPR_ERR("[HRT] i %d, invalid mode %d\n", i, mode);
			return -1;
		}

		if (layer_num < 0) {
			DDPPR_ERR("[HRT] i %d, invalid layer num %d\n",
				  i, layer_num);
			return -1;
		}
	}

	/* these are set by kernel, should be 0 */
	if (disp_info->res_idx || disp_info->hrt_weight || disp_info->hrt_idx) {
		DDPPR_ERR("[HRT] fail, res_idx %d, hrt_weight %u, hrt_idx %u\n",
			  disp_info->res_idx,
			  disp_info->hrt_weight,
			  disp_info->hrt_idx);
		return -1;
	}

	for (disp_idx = 0; disp_idx < HRT_TYPE_NUM; disp_idx++) {
		if (disp_info->layer_num[disp_idx] > 0 &&
		    disp_info->input_config[disp_idx] == NULL) {
			DDPPR_ERR(
				"[HRT]input config is empty,disp:%d,l_num:%d\n",
				disp_idx, disp_info->layer_num[disp_idx]);
			return -1;
		}

		ghead = disp_info->gles_head[disp_idx];
		gtail = disp_info->gles_tail[disp_idx];
		if ((ghead < 0 && gtail >= 0) || (gtail < 0 && ghead >= 0)) {
			dump_disp_info(disp_info, DISP_DEBUG_LEVEL_ERR);
			DDPPR_ERR("[HRT]gles invalid,disp:%d,head:%d,tail:%d\n",
				  disp_idx, disp_info->gles_head[disp_idx],
				  disp_info->gles_tail[disp_idx]);
			return -1;
		}
	}

	return 0;
}

static int
_copy_layer_info_from_disp(struct drm_mtk_layering_info *disp_info_user,
			   int debug_mode, int disp_idx)
{
	struct drm_mtk_layering_info *l_info = &layering_info;
	unsigned long int layer_size = 0;
	int ret = 0, layer_num = 0;

	if (l_info->layer_num[disp_idx] <= 0) {
		/* direct skip */
		return 0;
	}

	layer_num = l_info->layer_num[disp_idx];
	layer_size = sizeof(struct drm_mtk_layer_config) * layer_num;
	l_info->input_config[disp_idx] = kzalloc(layer_size, GFP_KERNEL);

	if (l_info->input_config[disp_idx] == NULL) {
		DDPPR_ERR("%s:%d invalid input_config[%d]:0x%p\n",
			__func__, __LINE__,
			disp_idx, l_info->input_config[disp_idx]);
		return -ENOMEM;
	}

	if (debug_mode) {
		memcpy(l_info->input_config[disp_idx],
		       disp_info_user->input_config[disp_idx], layer_size);
	} else {
		if (copy_from_user(l_info->input_config[disp_idx],
				   disp_info_user->input_config[disp_idx],
				   layer_size)) {
			DDPPR_ERR("%s:%d copy failed:(0x%p,0x%p), size:%ld\n",
					__func__, __LINE__,
					l_info->input_config[disp_idx],
					disp_info_user->input_config[disp_idx],
					layer_size);
			return -EFAULT;
		}
	}

	return ret;
}

static int set_disp_info(struct drm_mtk_layering_info *disp_info_user,
			 int debug_mode)
{
	int i;

	memcpy(&layering_info, disp_info_user,
		sizeof(struct drm_mtk_layering_info));

	for (i = 0; i < HRT_TYPE_NUM; i++)
		_copy_layer_info_from_disp(disp_info_user, debug_mode, i);

	memset(l_rule_info->addon_scn, 0x0, sizeof(l_rule_info->addon_scn));
	return 0;
}

static int
_copy_layer_info_by_disp(struct drm_mtk_layering_info *disp_info_user,
			 int debug_mode, int disp_idx)
{
	struct drm_mtk_layering_info *l_info = &layering_info;
	unsigned long int layer_size = 0;
	int ret = 0;

	if (l_info->layer_num[disp_idx] <= 0) {
		/* direct skip */
		return -EFAULT;
	}

	disp_info_user->gles_head[disp_idx] = l_info->gles_head[disp_idx];
	disp_info_user->gles_tail[disp_idx] = l_info->gles_tail[disp_idx];

	layer_size = sizeof(struct drm_mtk_layer_config) *
		     disp_info_user->layer_num[disp_idx];

	if (debug_mode) {
		memcpy(disp_info_user->input_config[disp_idx],
		       l_info->input_config[disp_idx], layer_size);
	} else {
		if (copy_to_user(disp_info_user->input_config[disp_idx],
				 l_info->input_config[disp_idx], layer_size)) {
			DDPINFO("[DISP][FB]: copy_to_user failed! line:%d\n",
				__LINE__);
			ret = -EFAULT;
		}
		kfree(l_info->input_config[disp_idx]);
	}

	return ret;
}

static int copy_layer_info_to_user(struct drm_mtk_layering_info *disp_info_user,
				   int debug_mode)
{
	int ret = 0, i;
	struct drm_mtk_layering_info *l_info = &layering_info;

	disp_info_user->hrt_num = l_info->hrt_num;
	disp_info_user->hrt_idx = l_info->hrt_idx;
	disp_info_user->hrt_weight = l_info->hrt_weight;
	for (i = 0; i < HRT_TYPE_NUM; i++)
		_copy_layer_info_by_disp(disp_info_user, debug_mode, i);

	return ret;
}

#ifdef HRT_UT_DEBUG
static int set_hrt_state(enum HRT_SYS_STATE sys_state, int en)
{
	switch (sys_state) {
	case DISP_HRT_MJC_ON:
		if (en)
			l_rule_info->hrt_sys_state |= (1 << sys_state);
		else
			l_rule_info->hrt_sys_state &= ~(1 << sys_state);
		break;
	case DISP_HRT_FORCE_DUAL_OFF:
		if (en)
			l_rule_info->hrt_sys_state |= (1 << sys_state);
		else
			l_rule_info->hrt_sys_state &= ~(1 << sys_state);
		break;
	case DISP_HRT_MULTI_TUI_ON:
		if (en)
			l_rule_info->hrt_sys_state |= (1 << sys_state);
		else
			l_rule_info->hrt_sys_state &= ~(1 << sys_state);
		break;
	default:
		DDPPR_ERR("unknown hrt scenario\n");
		break;
	}

	DDPMSG("Set hrt sys_state:%d, en:%d\n", sys_state, en);
	return 0;
}
#endif

void mtk_register_layering_rule_ops(struct layering_rule_ops *ops,
				    struct layering_rule_info_t *info)
{
	l_rule_ops = ops;
	l_rule_info = info;
}

void lye_add_blob_ids(struct drm_mtk_layering_info *l_info,
		      struct mtk_drm_lyeblob_ids *lyeblob_ids,
		      struct drm_device *drm_dev,
		      int crtc_num,
		      int crtc_mask)
{
	struct drm_property_blob *blob;
	struct mtk_lye_ddp_state lye_state;
	struct mtk_drm_private *mtk_drm = drm_dev->dev_private;

	memcpy(lye_state.scn, l_rule_info->addon_scn, sizeof(lye_state.scn));
	lye_state.lc_tgt_layer = 0;

	blob = drm_property_create_blob(
		drm_dev, sizeof(struct mtk_lye_ddp_state), &lye_state);

	lyeblob_ids->lye_idx = l_rule_info->hrt_idx;
	lyeblob_ids->frame_weight = l_info->hrt_weight;
	lyeblob_ids->hrt_num = l_info->hrt_num;
	lyeblob_ids->ddp_blob_id = blob->base.id;
	lyeblob_ids->ref_cnt = crtc_num;
	lyeblob_ids->ref_cnt_mask = crtc_mask;
	lyeblob_ids->free_cnt_mask = crtc_mask;
	INIT_LIST_HEAD(&lyeblob_ids->list);
	mutex_lock(&mtk_drm->lyeblob_list_mutex);
	list_add_tail(&lyeblob_ids->list, &mtk_drm->lyeblob_head);
	mutex_unlock(&mtk_drm->lyeblob_list_mutex);
}

static bool is_rsz_valid(struct drm_mtk_layer_config *c)
{
	if (c->src_width == c->dst_width && c->src_height == c->dst_height)
		return false;
	if (c->src_width > c->dst_width || c->src_height > c->dst_height)
		return false;
	/*
	 * HWC adjusts MDP layer alignment after query_valid_layer.
	 * This makes the decision of layering rule unreliable. Thus we
	 * add constraint to avoid frame_cfg becoming scale-down.
	 *
	 * TODO: If HWC adjusts MDP layer alignment before
	 * query_valid_layer, we could remove this if statement.
	 */

	/* HWC adjusts MDP layer alignment, we remove this if statement */
#if 0
	if ((mtk_has_layer_cap(c, MTK_MDP_RSZ_LAYER) ||
	     mtk_has_layer_cap(c, MTK_DISP_RSZ_LAYER)) &&
	    (c->dst_width - c->src_width <= MDP_ALIGNMENT_MARGIN ||
	     c->dst_height - c->src_height <= MDP_ALIGNMENT_MARGIN))
		return false;
#endif
	return true;
}

static int is_same_ratio(struct drm_mtk_layer_config *ref,
			 struct drm_mtk_layer_config *c)
{
	int diff_w, diff_h;

	if (!ref->dst_width || !ref->dst_height) {
		DDPPR_ERR("%s:ref dst(%dx%d)\n", __func__, ref->dst_width,
			  ref->dst_height);
		return -EINVAL;
	}

	diff_w = (c->dst_width * ref->src_width + (ref->dst_width - 1)) /
			 ref->dst_width -
		 c->src_width;
	diff_h = (c->dst_height * ref->src_height + (ref->dst_height - 1)) /
			 ref->dst_height -
		 c->src_height;
	if (abs(diff_w) > 1 || abs(diff_h) > 1)
		return false;

	return true;
}

#define RATIO_LIMIT  2
static bool same_ratio_limitation(struct drm_crtc *crtc,
			struct drm_mtk_layer_config *tgt, int limitation)
{
	int panel_w = 0, panel_h = 0;
	int diff_w = 0, diff_h = 0;

	panel_w = crtc->mode.hdisplay;
	panel_h = crtc->mode.vdisplay;
	diff_w = tgt->dst_width - tgt->src_width;
	diff_h = tgt->dst_height - tgt->src_height;
	if (panel_w <= 0 || panel_h <= 0)
		return false;
	if (((100 * diff_w/panel_w < limitation) && (diff_w > 0)) ||
			((100 * diff_h/panel_h < limitation) && (diff_h > 0)))
		return true;
	else
		return false;
}

static int RPO_rule(struct drm_crtc *crtc,
		struct drm_mtk_layering_info *disp_info, int disp_idx,
		bool has_pq)
{
	struct drm_mtk_layer_config *c = NULL;
	struct drm_mtk_layer_config *ref_layer = NULL;
	struct mtk_rect src_layer_roi = {0};
	struct mtk_rect dst_layer_roi = {0};
	struct mtk_rect src_roi = {0};
	struct mtk_rect dst_roi = {0};
	int rsz_idx = -1;
	int i = 0;

	/* if need pq, we only support one resize layer for DMDP */
	if (has_pq) {
		c = &disp_info->input_config[disp_idx][i];

		if (c->src_width == c->dst_width &&
		    c->src_height == c->dst_height)
			return 0;

		if (c->src_width > c->dst_width ||
		    c->src_height > c->dst_height)
			return 0;

		c->layer_caps |= MTK_DMDP_RSZ_LAYER;
		rsz_idx = i;

		goto done;
	}

	for (i = 0; i < disp_info->layer_num[disp_idx] &&
						i < DISP_RSZ_LAYER_NUM; i++) {
		c = &disp_info->input_config[disp_idx][i];

		/*if (i == 0 && c->src_fmt == MTK_DRM_FORMAT_DIM)
		 *	continue;
		 */

		if (disp_info->gles_head[disp_idx] >= 0 &&
		    disp_info->gles_head[disp_idx] <= i)
			break;

		/* RSZ HW limitation */
		/* 4x4 < input resolution size */
		if ((c->src_width <= 4) || (c->src_height <= 4))
			break;

		if (!is_rsz_valid(c))
			break;

		if (!ref_layer)
			ref_layer = c;
		else if (is_same_ratio(ref_layer, c) <= 0 &&
				is_same_ratio(c, ref_layer) <= 0)
			break;
		else if (same_ratio_limitation(crtc, c, RATIO_LIMIT))
			break;

		mtk_rect_make(&src_layer_roi,
			((c->dst_offset_x * c->src_width * 10)
				/ c->dst_width + 5) / 10,
			((c->dst_offset_y * c->src_height * 10)
				/ c->dst_height + 5) / 10,
			c->src_width, c->src_height);
		mtk_rect_make(&dst_layer_roi,
			c->dst_offset_x, c->dst_offset_y,
			c->dst_width, c->dst_height);
		mtk_rect_join(&src_layer_roi, &src_roi, &src_roi);
		mtk_rect_join(&dst_layer_roi, &dst_roi, &dst_roi);
		if (src_roi.width > dst_roi.width ||
		    src_roi.height > dst_roi.height) {
			DDPPR_ERR(
				"L%d:scale down(%d,%d,%dx%d)->(%d,%d,%dx%d)\n",
				i, src_roi.x, src_roi.y, src_roi.width,
				src_roi.height, dst_roi.x, dst_roi.y,
				dst_roi.width, dst_roi.height);
			break;
		}

		if (src_roi.width > RSZ_TILE_LENGTH ||
		    src_roi.height > RSZ_IN_MAX_HEIGHT)
			break;

		c->layer_caps |= MTK_DISP_RSZ_LAYER;
		rsz_idx = i;
	}

done:
	return rsz_idx + 1;
}

/* resizing_rule - layering rule resize layer layout */
static unsigned int resizing_rule(struct drm_device *dev,
			struct drm_mtk_layering_info *disp_info,
			bool has_pq)
{
	unsigned int scale_num = 0;
	struct drm_crtc *crtc;

	/* RPO only support primary */
	if (disp_info->layer_num[HRT_SECONDARY] > 0)
		mtk_rollback_all_resize_layer_to_GPU(disp_info, HRT_SECONDARY);
	if (disp_info->layer_num[HRT_THIRD] > 0)
		mtk_rollback_all_resize_layer_to_GPU(disp_info, HRT_THIRD);

	if (disp_info->layer_num[HRT_PRIMARY] > 0) {
		drm_for_each_crtc(crtc, dev)
			if (drm_crtc_index(crtc) == 0)
				break;

		if (crtc)
			scale_num = RPO_rule(crtc, disp_info, HRT_PRIMARY,
					has_pq);
		mtk_rollback_resize_layer_to_GPU_range(disp_info, HRT_PRIMARY,
			scale_num, disp_info->layer_num[HRT_PRIMARY] - 1);
	}

	return scale_num;
}

static unsigned int get_scn_decision_flag(
	struct drm_mtk_layering_info *disp_info)
{
	unsigned int scn_decision_flag = 0;

	if (is_triple_disp(disp_info))
		scn_decision_flag |= SCN_TRIPLE_DISP;
	return scn_decision_flag;
}

static int get_crtc_num(
	struct drm_mtk_layering_info *disp_info_user,
	int *crtc_mask)
{
	int i;
	int crtc_num;
	int input_config_num;

	if (!crtc_mask) {
		DDPPR_ERR("%s:%d null crtc_mask\n",
				__func__, __LINE__);
		return 0;
	}

	switch (disp_info_user->disp_mode[0]) {
	case MTK_DRM_SESSION_DL:
	case MTK_DRM_SESSION_DC_MIRROR:
		crtc_num = 1;
		break;
	case MTK_DRM_SESSION_DOUBLE_DL:
		crtc_num = 2;
		break;
	case MTK_DRM_SESSION_TRIPLE_DL:
		crtc_num = 3;
		break;
	default:
		crtc_num = 0;
		break;
	}

	/*
	 * when CRTC 0 disabled, disp_mode[0] would be 0,
	 * but it might still exist other display.
	 * Thus traverse each CRTC's disp_mode for
	 * active CRTC number
	 */
	if (crtc_num == 0) {
		for (i = 0 ; i < 3; i++)
			crtc_num += !!disp_info_user->disp_mode[i];
	}

	/* check input config number */
	input_config_num = 0;
	*crtc_mask = 0;
	for (i = 0; i < 3; i++) {
		if (disp_info_user->input_config[i]) {
			*crtc_mask |= (1 << i);
			input_config_num++;
		}
	}

	if (input_config_num != crtc_num) {
		DDPPR_ERR("%s:%d mode[%d] num:%d not matched config num:%d\n",
				__func__, __LINE__,
				disp_info_user->disp_mode[0],
				crtc_num, input_config_num);
		crtc_num = min(crtc_num, input_config_num);
	}

	return crtc_num;
}

static int layering_rule_start(struct drm_mtk_layering_info *disp_info_user,
			       int debug_mode, struct drm_device *dev)
{
	int ret;
	int overlap_num;
	struct mtk_drm_lyeblob_ids *lyeblob_ids;
	unsigned int scale_num = 0;
	unsigned int scn_decision_flag = 0;
	int crtc_num, crtc_mask;
	int disp_idx;

	DRM_MMP_EVENT_START(layering, (unsigned long)disp_info_user,
			(unsigned long)dev);

	roll_gpu_for_idle = 0;

	if (l_rule_ops == NULL || l_rule_info == NULL) {
		DRM_MMP_MARK(layering, 0, 0);
		DRM_MMP_EVENT_END(layering, 0, 0);
		DDPPR_ERR("Layering rule has not been initialize:(%p,%p)\n",
				l_rule_ops, l_rule_info);
		return -EFAULT;
	}

	if (check_disp_info(disp_info_user) < 0) {
		DRM_MMP_MARK(layering, 0, 1);
		DRM_MMP_EVENT_END(layering, 0, 0);
		DDPPR_ERR("check_disp_info fail\n");
		return -EFAULT;
	}

	if (set_disp_info(disp_info_user, debug_mode)) {
		DRM_MMP_MARK(layering, 0, 2);
		DRM_MMP_EVENT_END(layering, 0, 0);
		return -EFAULT;
	}

	print_disp_info_to_log_buffer(&layering_info);
#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("[Input data]\n");
	dump_disp_info(&layering_info, DISP_DEBUG_LEVEL_INFO);
#endif

	l_rule_info->hrt_idx++;
	if (l_rule_info->hrt_idx == 0xffffffff)
		l_rule_info->hrt_idx = 0;

	l_rule_ops->copy_hrt_bound_table(&layering_info,
		0, g_emi_bound_table, dev);

	/* 1.Pre-distribution */
	l_rule_info->dal_enable = mtk_drm_dal_enable();

	if (l_rule_ops->rollback_to_gpu_by_hw_limitation)
		ret = l_rule_ops->rollback_to_gpu_by_hw_limitation(
			dev, &layering_info);

	scn_decision_flag = get_scn_decision_flag(&layering_info);
	/* Check and choose the Resize Scenario */
	if (get_layering_opt(LYE_OPT_RPO)) {
		bool has_pq = (scn_decision_flag & SCN_NEED_VP_PQ)
				| (scn_decision_flag & SCN_NEED_GAME_PQ);
		scale_num = resizing_rule(dev, &layering_info, has_pq);
	} else {
		mtk_rollback_all_resize_layer_to_GPU(&layering_info,
						     HRT_PRIMARY);
		mtk_rollback_all_resize_layer_to_GPU(&layering_info,
			HRT_SECONDARY);
		mtk_rollback_all_resize_layer_to_GPU(&layering_info,
			HRT_THIRD);
	}

	/* fbdc_rule should be after resizing_rule
	 * for optimizing secondary display BW
	 */
	if (l_rule_ops->fbdc_rule)
		l_rule_ops->fbdc_rule(&layering_info);

	/* Add for FBDC */
	if (l_rule_ops->fbdc_pre_calculate)
		l_rule_ops->fbdc_pre_calculate(&layering_info);

	/* Initial HRT conditions */
	l_rule_ops->scenario_decision(scn_decision_flag, scale_num);

	/* Layer Grouping */
	if (l_rule_ops->fbdc_adjust_layout)
		l_rule_ops->fbdc_adjust_layout(&layering_info,
					       ADJUST_LAYOUT_EXT_GROUPING);

	ret = ext_layer_grouping(dev, &layering_info);

	if (l_rule_ops->fbdc_restore_layout)
		l_rule_ops->fbdc_restore_layout(&layering_info,
						ADJUST_LAYOUT_EXT_GROUPING);

	/* GLES adjustment and ext layer checking */
	ret = filter_by_ovl_cnt(dev, &layering_info);

	/*
	 * 2.Overlapping
	 * Calculate overlap number of available input layers.
	 * If the overlap number is out of bound, then decrease
	 * the number of available layers to overlap number.
	 */
	/* [PVRIC] change dst layout before calculate overlap */
	if (l_rule_ops->fbdc_adjust_layout)
		l_rule_ops->fbdc_adjust_layout(&layering_info,
					       ADJUST_LAYOUT_OVERLAP_CAL);
	overlap_num = calc_hrt_num(dev, &layering_info);
	layering_info.hrt_weight = overlap_num;
	DDPINFO("overlap_num %u\n", layering_info.hrt_weight);

	if (l_rule_ops->fbdc_restore_layout)
		l_rule_ops->fbdc_restore_layout(&layering_info,
						ADJUST_LAYOUT_OVERLAP_CAL);

	/*
	 * 3.Dispatching
	 * Fill layer id for each input layers.
	 * All the gles layers set as same layer id.
	 */
	if (l_rule_ops->rollback_all_to_GPU_for_idle != NULL &&
			l_rule_ops->rollback_all_to_GPU_for_idle(dev)) {
		int i;

		roll_gpu_for_idle = 1;
		rollback_all_to_GPU(&layering_info, HRT_PRIMARY);
		/* TODO: assume resize layer would be 2 */
		for (i = 0 ; i < layering_info.layer_num[disp_idx] ; i++)
			layering_info.input_config[HRT_PRIMARY][i].layer_caps &=
				~MTK_DISP_RSZ_LAYER;
		l_rule_info->addon_scn[HRT_PRIMARY] = NONE;
		layering_info.hrt_num = HRT_LEVEL_LEVEL0;
		layering_info.hrt_weight = 2;
	}

	lyeblob_ids = kzalloc(sizeof(struct mtk_drm_lyeblob_ids), GFP_KERNEL);

	ret = dispatch_ovl_id(&layering_info, lyeblob_ids, dev);

	check_layering_result(&layering_info);

	layering_info.hrt_idx = l_rule_info->hrt_idx;
	HRT_SET_AEE_FLAG(layering_info.hrt_num, l_rule_info->dal_enable);
	HRT_SET_WROT_SRAM_FLAG(layering_info.hrt_num, l_rule_info->wrot_sram);
	dump_disp_info(&layering_info, DISP_DEBUG_LEVEL_INFO);

	dump_disp_trace(&layering_info);

	/* Remove MMP */
	/* mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_PULSE,
	 *		 layering_info.hrt_num,
	 *		 (layering_info.gles_head[0] << 24) |
	 *		 (layering_info.gles_tail[0] << 16) |
	 *		 (layering_info.layer_num[0] << 8) |
	 *		 layering_info.layer_num[1]);
	 */
	crtc_num = get_crtc_num(disp_info_user, &crtc_mask);
	lye_add_blob_ids(&layering_info, lyeblob_ids, dev, crtc_num, crtc_mask);

	for (disp_idx = 0; disp_idx < HRT_TYPE_NUM; disp_idx++)
		DRM_MMP_MARK(layering, layering_info.hrt_num,
				(layering_info.gles_head[disp_idx] << 24) |
				(layering_info.gles_tail[disp_idx] << 16) |
				(layering_info.layer_num[disp_idx] << 8) |
				disp_idx);

	ret = copy_layer_info_to_user(disp_info_user, debug_mode);

	DRM_MMP_EVENT_END(layering, (unsigned long)disp_info_user,
			(unsigned long)dev);

	return ret;
}

/**** UT Program ****/
#ifdef HRT_UT_DEBUG
static void debug_set_layer_data(struct drm_mtk_layering_info *disp_info,
				 int disp_id, int data_type, int value)
{
	static int layer_id = -1;
	struct drm_mtk_layer_config *layer_info = NULL;

	if (data_type != HRT_LAYER_DATA_ID && layer_id == -1)
		return;

	layer_info = &disp_info->input_config[disp_id][layer_id];
	switch (data_type) {
	case HRT_LAYER_DATA_ID:
		layer_id = value;
		break;
	case HRT_LAYER_DATA_SRC_FMT:
		layer_info->src_fmt = value;
		break;
	case HRT_LAYER_DATA_DST_OFFSET_X:
		layer_info->dst_offset_x = value;
		break;
	case HRT_LAYER_DATA_DST_OFFSET_Y:
		layer_info->dst_offset_y = value;
		break;
	case HRT_LAYER_DATA_DST_WIDTH:
		layer_info->dst_width = value;
		break;
	case HRT_LAYER_DATA_DST_HEIGHT:
		layer_info->dst_height = value;
		break;
	case HRT_LAYER_DATA_SRC_WIDTH:
		layer_info->src_width = value;
		break;
	case HRT_LAYER_DATA_SRC_HEIGHT:
		layer_info->src_height = value;
		break;
	case HRT_LAYER_DATA_SRC_OFFSET_X:
		layer_info->src_offset_x = value;
		break;
	case HRT_LAYER_DATA_SRC_OFFSET_Y:
		layer_info->src_offset_y = value;
		break;
	case HRT_LAYER_DATA_COMPRESS:
		layer_info->compress = value;
		break;
	case HRT_LAYER_DATA_CAPS:
		layer_info->layer_caps = value;
		break;
	default:
		break;
	}
}

static char *parse_hrt_data_value(char *start, long int *value)
{
	char *tok_start = NULL, *tok_end = NULL;
	int ret;

	tok_start = strchr(start + 1, ']');
	tok_end = strchr(tok_start + 1, '[');
	if (tok_end)
		*tok_end = 0;
	ret = kstrtol(tok_start + 1, 10, value);
	if (ret)
		DDPINFO("Parsing error gles_num:%d, p:%s, ret:%d\n",
			(int)*value, tok_start + 1, ret);

	return tok_end;
}

static void print_drm_mtk_layer_config(struct drm_mtk_layer_config *c)
{
	DDPMSG("L%u/(%u,%u,%u,%u)/(%u,%u,%u,%u)/cpr%d/e%d/cap0x%x/cl%x\n",
	       c->ovl_id, c->src_offset_x, c->src_offset_y, c->src_width,
	       c->src_height, c->dst_offset_x, c->dst_offset_y, c->dst_width,
	       c->dst_height, c->compress, c->ext_sel_layer, c->layer_caps,
	       c->clip);
}

static void print_hrt_result(struct drm_mtk_layering_info *disp_info)
{
	unsigned int i = 0, j = 0;

	for (i = 0; i < HRT_TYPE_NUM; i++) {
		DDPMSG("### DISP%d ###\n", i);
		DDPMSG("[head]%d[tail]%d\n", disp_info->gles_head[i],
		       disp_info->gles_tail[i]);
		DDPMSG("[hrt_num]%d\n", disp_info->hrt_num);
		for (j = 0; j < disp_info->layer_num[i]; j++)
			print_drm_mtk_layer_config(
				&(disp_info->input_config[i][j]));
	}
}

static int load_hrt_test_data(struct drm_mtk_layering_info *disp_info,
			      struct drm_device *dev)
{
	char filename[] = "/sdcard/hrt_data.txt";
	char line_buf[512];
	char *tok;
	struct file *filp;
	mm_segment_t oldfs;
	int ret, pos, i;
	long int disp_id, test_case;
	bool is_end = false, is_test_pass = false;
	struct drm_mtk_layer_config *input_config;

	pos = 0;
	test_case = -1;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(filename, O_RDONLY, 0777);
	if (IS_ERR(filp)) {
		DDPINFO("File open error:%s\n", filename);
		return -1;
	}

	if (!filp->f_op) {
		DDPINFO("File Operation Method Error!!\n");
		return -1;
	}

	while (1) {
		ret = filp->f_op->llseek(filp, filp->f_pos, pos);
		memset(line_buf, 0x0, sizeof(line_buf));
		ret = filp->f_op->read(filp, line_buf, sizeof(line_buf),
				       &filp->f_pos);
		tok = strchr(line_buf, '\n');
		if (tok != NULL)
			*tok = '\0';
		else
			is_end = true;

		pos += strlen(line_buf) + 1;
		filp->f_pos = pos;

		if (strncmp(line_buf, "#", 1) == 0) {
			continue;
		} else if (strncmp(line_buf, "[layer_num]", 11) == 0) {
			unsigned long int layer_num = 0;
			unsigned long int layer_size = 0;

			tok = parse_hrt_data_value(line_buf, &layer_num);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);

			if (disp_id >= HRT_TYPE_NUM)
				goto end;

			if (layer_num != 0) {
				layer_size =
					sizeof(struct drm_mtk_layer_config) *
					layer_num;
				disp_info->input_config[disp_id] =
					kzalloc(layer_size, GFP_KERNEL);
			}
			disp_info->layer_num[disp_id] = layer_num;

			if (disp_info->input_config[disp_id] == NULL)
				return 0;
		} else if (strncmp(line_buf, "[set_layer]", 11) == 0) {
			unsigned long int tmp_info;

			tok = strchr(line_buf, ']');
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			for (i = 0; i < HRT_LAYER_DATA_NUM; i++) {
				tok = parse_hrt_data_value(tok, &tmp_info);
				debug_set_layer_data(disp_info, disp_id, i,
						     tmp_info);
			}
		} else if (strncmp(line_buf, "[test_start]", 12) == 0) {
			tok = parse_hrt_data_value(line_buf, &test_case);
			layering_rule_start(disp_info, 1, dev);
			is_test_pass = true;
		} else if (strncmp(line_buf, "[test_end]", 10) == 0) {
			kfree(disp_info->input_config[0]);
			kfree(disp_info->input_config[1]);
			memset(disp_info, 0x0,
			       sizeof(struct drm_mtk_layering_info));
			is_end = true;
		} else if (strncmp(line_buf, "[print_out_test_result]", 23) ==
			   0) {
			DDPINFO("Test case %d is %s\n", (int)test_case,
				is_test_pass ? "Pass" : "Fail");
		} else if (strncmp(line_buf, "[layer_result]", 14) == 0) {
			long int layer_result = 0, layer_id;

			tok = strchr(line_buf, ']');
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &layer_id);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &layer_result);
			input_config =
				&disp_info->input_config[disp_id][layer_id];
			if (layer_result != input_config->ovl_id) {
				DDPINFO("case:%d,ovl_id incorrect,%d/%d\n",
					(int)test_case, input_config->ovl_id,
					(int)layer_result);
				is_test_pass = false;
			}
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &layer_result);
			if (layer_result != input_config->ext_sel_layer) {
				DDPINFO("case:%d,ext_sel_layer wrong,%d/%d\n",
					(int)test_case,
					input_config->ext_sel_layer,
					(int)layer_result);
				is_test_pass = false;
			}
		} else if (strncmp(line_buf, "[gles_result]", 13) == 0) {
			long int gles_num = 0;

			tok = strchr(line_buf, ']');
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &gles_num);
			if (gles_num != disp_info->gles_head[disp_id]) {
				DDPINFO("case:%d,gles head err,%d/%d\n",
					(int)test_case,
					disp_info->gles_head[disp_id],
					(int)gles_num);
				is_test_pass = false;
			}

			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &gles_num);
			if (gles_num != disp_info->gles_tail[disp_id]) {
				DDPINFO("case:%d,gles tail err,%d/%d\n",
					(int)test_case,
					disp_info->gles_tail[disp_id],
					(int)gles_num);
				is_test_pass = false;
			}
		} else if (strncmp(line_buf, "[hrt_result]", 12) == 0) {
			unsigned long int hrt_num = 0;
			int path_scen;

			tok = parse_hrt_data_value(line_buf, &hrt_num);
			if (hrt_num != HRT_GET_DVFS_LEVEL(disp_info->hrt_num))
				DDPINFO("case:%d,hrt num err,%d/%d\n",
					(int)test_case,
					HRT_GET_DVFS_LEVEL(disp_info->hrt_num),
					(int)hrt_num);

			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &hrt_num);
			path_scen = HRT_GET_PATH_SCENARIO(disp_info->hrt_num) &
				    0x1F;
			if (hrt_num != path_scen) {
				DDPINFO("case:%d,hrt path err,%d/%d\n",
					(int)test_case, path_scen,
					(int)hrt_num);
				is_test_pass = false;
			}

			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &hrt_num);
			if (hrt_num !=
			    HRT_GET_SCALE_SCENARIO(disp_info->hrt_num)) {
				DDPINFO("case:%d, hrt scale err,%d/%d\n",
					(int)test_case,
					HRT_GET_SCALE_SCENARIO(
						disp_info->hrt_num),
					(int)hrt_num);
				is_test_pass = false;
			}

		} else if (strncmp(line_buf, "[change_layer_num]", 18) == 0) {
			unsigned long int layer_num = 0;

			tok = parse_hrt_data_value(line_buf, &layer_num);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			disp_info->layer_num[disp_id] = layer_num;
		} else if (!strncmp(line_buf, "[force_dual_pipe_off]", 21)) {
			unsigned long int force_off = 0;

			tok = parse_hrt_data_value(line_buf, &force_off);
			set_hrt_state(DISP_HRT_FORCE_DUAL_OFF, force_off);
		} else if (!strncmp(line_buf, "[resolution_level]", 18)) {
			unsigned long int resolution_level = 0;

			tok = parse_hrt_data_value(line_buf, &resolution_level);
			debug_resolution_level = resolution_level;
		} else if (!strncmp(line_buf, "[set_gles]", 10)) {
			long int gles_num = 0;

			tok = strchr(line_buf, ']');
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &gles_num);
			disp_info->gles_head[disp_id] = gles_num;

			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &gles_num);
			disp_info->gles_tail[disp_id] = gles_num;
		} else if (!strncmp(line_buf, "[disp_mode]", 11)) {
			unsigned long int disp_mode = 0;

			tok = parse_hrt_data_value(line_buf, &disp_mode);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			disp_info->disp_mode[disp_id] = disp_mode;
		} else if (!strncmp(line_buf, "[print_out_hrt_result]", 22))
			print_hrt_result(disp_info);

		if (is_end)
			break;
	}

end:
	filp_close(filp, NULL);
	set_fs(oldfs);
	DDPINFO("end set_fs\n");
	return 0;
}

static int gen_hrt_pattern(struct drm_device *dev)
{
#ifdef HRT_UT_DEBUG
	struct drm_mtk_layering_info disp_info;
	struct drm_mtk_layer_config *layer_info;
	int i;

	memset(&disp_info, 0x0, sizeof(struct drm_mtk_layering_info));
	disp_info.gles_head[0] = -1;
	disp_info.gles_head[1] = -1;
	disp_info.gles_tail[0] = -1;
	disp_info.gles_tail[1] = -1;
	if (!load_hrt_test_data(&disp_info))
		return 0;

	/* Primary Display */
	disp_info.disp_mode[0] = DRM_DISP_SESSION_DIRECT_LINK_MODE;
	disp_info.layer_num[0] = 5;
	disp_info.gles_head[0] = 3;
	disp_info.gles_tail[0] = 5;
	disp_info.input_config[0] =
		kzalloc(sizeof(struct drm_mtk_layer_config) * 5, GFP_KERNEL);
	layer_info = disp_info.input_config[0];
	for (i = 0; i < disp_info.layer_num[0]; i++)
		layer_info[i].src_fmt = DRM_FORMAT_ARGB8888;

	layer_info = disp_info.input_config[0];
	layer_info[0].dst_offset_x = 0;
	layer_info[0].dst_offset_y = 0;
	layer_info[0].dst_width = 1080;
	layer_info[0].dst_height = 1920;
	layer_info[1].dst_offset_x = 0;
	layer_info[1].dst_offset_y = 0;
	layer_info[1].dst_width = 1080;
	layer_info[1].dst_height = 1920;
	layer_info[2].dst_offset_x = 269;
	layer_info[2].dst_offset_y = 72;
	layer_info[2].dst_width = 657;
	layer_info[2].dst_height = 612;
	layer_info[3].dst_offset_x = 0;
	layer_info[3].dst_offset_y = 0;
	layer_info[3].dst_width = 1080;
	layer_info[3].dst_height = 72;
	layer_info[4].dst_offset_x = 1079;
	layer_info[4].dst_offset_y = 72;
	layer_info[4].dst_width = 1;
	layer_info[4].dst_height = 1704;

	/* Secondary Display */
	disp_info.disp_mode[1] = DRM_DISP_SESSION_DIRECT_LINK_MODE;
	disp_info.layer_num[1] = 0;
	disp_info.gles_head[1] = -1;
	disp_info.gles_tail[1] = -1;

	DDPMSG("free test pattern\n");
	kfree(disp_info.input_config[0]);
	msleep(50);
#endif
	return 0;
}
#endif

/**** UT Program end ****/
int mtk_layering_rule_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct drm_mtk_layering_info *disp_info_user = data;
	int ret;

	ret = layering_rule_start(disp_info_user, 0, dev);
	if (ret < 0)
		DDPPR_ERR("layering_rule_start error:%d\n", ret);

	return 0;
}
