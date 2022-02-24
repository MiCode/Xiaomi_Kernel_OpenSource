// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include "disp_session.h"
#include "disp_drv_log.h"
#include "cmdq_record.h"
#include "disp_rect.h"
#include "ddp_manager.h"
#include "disp_lcm.h"
#include "primary_display.h"

#include "disp_partial.h"

static void _convert_picture_to_ovl_dirty(struct disp_input_config *src,
		struct disp_rect *in, struct disp_rect *out)
{
	struct disp_rect layer_roi = {0, 0, 0, 0};
	struct disp_rect pic_roi = {0, 0, 0, 0};
	struct disp_rect result = {0, 0, 0, 0};

	layer_roi.x = src->src_offset_x;
	layer_roi.y = src->src_offset_y;
	layer_roi.width = src->src_width;
	layer_roi.height = src->src_height;

	pic_roi.x = in->x;
	pic_roi.y = in->y;
	pic_roi.width = in->width;
	pic_roi.height = in->height;

	rect_intersect(&layer_roi, &pic_roi, &result);

	out->x = result.x - layer_roi.x + src->tgt_offset_x;
	out->y = result.y - layer_roi.y + src->tgt_offset_y;
	out->width = result.width;
	out->height = result.height;

	DISPDBG("pic to ovl dirty(%d,%d,%dx%d)->(%d,%d,%dx%d)\n",
		pic_roi.x, pic_roi.y, pic_roi.width, pic_roi.height,
		out->x, out->y, out->width, out->height);
}

int disp_partial_compute_ovl_roi(struct disp_frame_cfg_t *cfg,
		struct disp_ddp_path_config *old_cfg, struct disp_rect *result)
{
	int i, j;
	int disable_layer = 0;
	struct OVL_CONFIG_STRUCT *old_ovl_cfg = NULL;
	struct disp_rect layer_roi = {0, 0, 0, 0};

	if (ddp_debug_force_roi()) {
		assign_full_lcm_roi(result);
		result->x = ddp_debug_force_roi_x();
		result->y = ddp_debug_force_roi_y();
		result->width = ddp_debug_force_roi_w();
		result->height = ddp_debug_force_roi_h();
		return 0;
	}

	for (i = 0; i < cfg->input_layer_num; i++) {
		struct disp_rect layer_total_roi = {0, 0, 0, 0};
		struct disp_input_config *input_cfg = &cfg->input_cfg[i];

		if (!input_cfg->layer_enable) {
			disable_layer++;
			continue;
		}

		if (input_cfg->dirty_roi_num) {
			struct layer_dirty_roi *layer_roi_addr =
				input_cfg->dirty_roi_addr;

			DISPDBG("layer %d dirty num %d\n",
				i, input_cfg->dirty_roi_num);
			/* 1. compute picture dirty roi*/
			for (j = 0; j < input_cfg->dirty_roi_num; j++) {
				layer_roi.x = layer_roi_addr[j].dirty_x;
				layer_roi.y = layer_roi_addr[j].dirty_y;
				layer_roi.width = layer_roi_addr[j].dirty_w;
				layer_roi.height = layer_roi_addr[j].dirty_h;
				rect_join(&layer_roi, &layer_total_roi,
					&layer_total_roi);
			}
			/* 2. convert picture dirty to ovl dirty */
			if (!rect_isEmpty(&layer_total_roi))
				_convert_picture_to_ovl_dirty(input_cfg,
					&layer_total_roi, &layer_total_roi);
		}

		/* 3. full dirty if num euals 0 */
		if (input_cfg->dirty_roi_num == 0 && input_cfg->layer_enable) {
			DISPDBG("layer %d dirty num 0\n", i);
			assign_full_lcm_roi(result);
			/* break if full lcm roi */
			break;
		}
		/* 4. deal with other cases:layer disable, dim layer*/
		old_ovl_cfg = &(old_cfg->ovl_config[input_cfg->layer_id]);
		rect_join(&layer_total_roi, result, result);

		/*break if roi is full lcm */
		if (is_equal_full_lcm(result))
			break;
	}

	if (disable_layer >= cfg->input_layer_num) {
		DISPMSG(" all layer disabled, force full roi\n");
		assign_full_lcm_roi(result);
	}

	return 0;
}

int disp_partial_get_project_option(void)
{
	struct device_node *mtkfb_node = NULL;
	static int inited;
	static int supported;

	if (inited)
		return supported;

	mtkfb_node = of_find_compatible_node(NULL, NULL, "mediatek,mtkfb");
	if (!mtkfb_node)
		goto out;

	of_property_read_u32(mtkfb_node, "partial-update", &supported);

out:
	inited = 1;
	return supported;
}

int disp_partial_is_support(void)
{
	struct disp_lcm_handle *plcm = primary_get_lcm();

	if (disp_partial_get_project_option() &&
		disp_lcm_is_partial_support(plcm) &&
		!disp_lcm_is_video_mode(plcm) &&
		disp_helper_get_option(DISP_OPT_PARTIAL_UPDATE))
		return 1;

	return 0;
}

void assign_full_lcm_roi(struct disp_rect *roi)
{
	roi->x = 0;
	roi->y = 0;
	roi->width = disp_helper_get_option(DISP_OPT_FAKE_LCM_WIDTH);
	roi->height = disp_helper_get_option(DISP_OPT_FAKE_LCM_HEIGHT);
}

int is_equal_full_lcm(const struct disp_rect *roi)
{
	static struct disp_rect full_roi;

	if (full_roi.width == 0)
		assign_full_lcm_roi(&full_roi);

	return rect_equal(&full_roi, roi);
}

void disp_patial_lcm_validate_roi(struct disp_lcm_handle *plcm,
	struct disp_rect *roi)
{
	int x = roi->x;
	int y = roi->y;
	int w = roi->width;
	int h = roi->height;

	disp_lcm_validate_roi(plcm, &roi->x, &roi->y, &roi->width,
		&roi->height);
	DISPDBG("lcm verify partial(%d,%d,%dx%d) to (%d,%d,%dx%d)\n",
		x, y, w, h, roi->x, roi->y, roi->width, roi->height);
}

int disp_partial_update_roi_to_lcm(disp_path_handle dp_handle,
				   struct disp_rect partial, void *cmdq_handle)
{
	return dpmgr_path_update_partial_roi(dp_handle, partial, cmdq_handle);
}
