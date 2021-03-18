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

#include "layering_rule.h"

static struct layering_rule_ops l_rule_ops;
static struct layering_rule_info_t l_rule_info;

int emi_bound_table[HRT_BOUND_NUM][HRT_LEVEL_NUM] = {
	/* HRT_BOUND_TYPE_LP4 */
	{0, 0, 300, 400},
	/* HRT_BOUND_TYPE_LP3 */
	{0, 0, 300, 400},
	/* HRT_BOUND_TYPE_LP4_1CH */
	{0, 300, 600, 800},
	/* HRT_BOUND_TYPE_LP4_HYBRID */
	{0, 0, 300, 400},
	/* HRT_BOUND_TYPE_LP3_PLUSP */
	{0, 0, 200, 400},
};

int larb_bound_table[HRT_BOUND_NUM][HRT_LEVEL_NUM] = {
	/* HRT_BOUND_TYPE_LP4 */
	{1200, 1200, 1200, 1200},
	/*  */
	{1200, 1200, 1200, 1200},
	/* HRT_BOUND_TYPE_LP4_1CH */
	{1200, 1200, 1200, 1200},
	/* HRT_BOUND_TYPE_LP4_HYBRID */
	{1200, 1200, 1200, 1200},
};

/**
 * The layer mapping table define ovl layer dispatch rule for both
 * primary and secondary display.Each table has 16 elements which
 * represent the layer mapping rule by the number of input layers.
 */
#ifndef CONFIG_MTK_ROUND_CORNER_SUPPORT
static int layer_mapping_table[HRT_TB_NUM][MAX_PHY_OVL_CNT] = {
	/* HRT_TB_TYPE_GENERAL */
	{0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	 0x0000000F, 0x0000000F, 0x0000000F},
};
#else
static int layer_mapping_table[HRT_TB_NUM][MAX_PHY_OVL_CNT] = {
	{0x00000001, 0x00000003, 0x00000007, 0x00000007,
	 0x00000007},
};
#endif
/**
 * The larb mapping table represent the relation between LARB and OVL.
 */
static int larb_mapping_table[HRT_TB_NUM] = {
	0x00000001,
};

/**
 * The OVL mapping table is used to get the OVL index of correcponding layer.
 * The bit value 1 means the position of the last layer in OVL engine.
 */
static int ovl_mapping_table[HRT_TB_NUM] = {
	0x00000008,
};

#define GET_SYS_STATE(sys_state)                                               \
	((l_rule_info.hrt_sys_state >> sys_state) & 0x1)

static void layering_rule_senario_decision(struct disp_layer_info *disp_info)
{
	mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_START,
			 l_rule_info.disp_path, l_rule_info.layer_tb_idx |
				 (l_rule_info.bound_tb_idx << 16));

	if (GET_SYS_STATE(DISP_HRT_MULTI_TUI_ON)) {
		l_rule_info.disp_path = HRT_PATH_GENERAL;
		/* layer_tb_idx = HRT_TB_TYPE_MULTI_WINDOW_TUI;*/
		l_rule_info.layer_tb_idx = HRT_TB_TYPE_GENERAL;
	} else {
		l_rule_info.layer_tb_idx = HRT_TB_TYPE_GENERAL;
		l_rule_info.disp_path = HRT_PATH_GENERAL;
	}

	l_rule_info.primary_fps = 60;

	if (primary_display_get_height() <= 1440)
		l_rule_info.bound_tb_idx = HRT_BOUND_TYPE_LP4;
	else
		l_rule_info.bound_tb_idx = HRT_BOUND_TYPE_LP3_PLUSP;

	mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_END,
			 l_rule_info.disp_path, l_rule_info.layer_tb_idx |
				 (l_rule_info.bound_tb_idx << 16));
}

static bool filter_by_hw_limitation(struct disp_layer_info *disp_info)
{
	bool flag = false;
	unsigned int i = 0;
	struct layer_config *info;
	unsigned int disp_idx = 0;
	unsigned int layer_cnt = 0;

	for (disp_idx = 0 ; disp_idx < 2; ++disp_idx) {
		if (disp_info->layer_num[disp_idx] < 1)
			continue;

		/* display not support RGBA1010102 & RGBA_FP16 */
		for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
			info = &(disp_info->input_config[disp_idx][i]);
			if (info->src_fmt != DISP_FORMAT_RGBA1010102 &&
					info->src_fmt != DISP_FORMAT_RGBA_FP16)
				continue;

			/* push to GPU */
			if (disp_info->gles_head[disp_idx] == -1 ||
			    i < disp_info->gles_head[disp_idx])
				disp_info->gles_head[disp_idx] = i;
			if (disp_info->gles_tail[disp_idx] == -1 ||
			    i > disp_info->gles_tail[disp_idx])
				disp_info->gles_tail[disp_idx] = i;
		}
	}

	disp_idx = 1;
	for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
		info = &(disp_info->input_config[disp_idx][i]);
		if (is_gles_layer(disp_info, disp_idx, i))
			continue;

		layer_cnt++;
		if (layer_cnt > SECONDARY_OVL_LAYER_NUM) {
			/* push to GPU */
			if (disp_info->gles_head[disp_idx] == -1 ||
			    i < disp_info->gles_head[disp_idx])
				disp_info->gles_head[disp_idx] = i;
			if (disp_info->gles_tail[disp_idx] == -1 ||
			    i > disp_info->gles_tail[disp_idx])
				disp_info->gles_tail[disp_idx] = i;

			flag = false;
		}
	}
	return flag;
}

static void clear_layer(struct disp_layer_info *disp_info)
{
	int di = 0;
	int i = 0;
	struct layer_config *c;

	for (di = 0; di < 2; di++) {
		int g_head = disp_info->gles_head[di];
		int top = -1;

		if (disp_info->layer_num[di] <= 0)
			continue;
		if (g_head == -1)
			continue;

		for (i = disp_info->layer_num[di] - 1; i >= g_head; i--) {
			c = &disp_info->input_config[di][i];
			if (has_layer_cap(c, LAYERING_OVL_ONLY) &&
			    has_layer_cap(c, CLIENT_CLEAR_LAYER)) {
				top = i;
				break;
			}
		}
		if (top == -1)
			continue;
		if (!is_gles_layer(disp_info, di, top))
			continue;

		c = &disp_info->input_config[di][top];
		c->layer_caps |= DISP_CLIENT_CLEAR_LAYER;
		DISPMSG("%s:D%d:L%d\n", __func__, di, top);

		disp_info->gles_head[di] = 0;
		disp_info->gles_tail[di] = disp_info->layer_num[di] - 1;
		for (i = 0; i < disp_info->layer_num[di]; i++) {
			c = &disp_info->input_config[di][i];

			c->ext_sel_layer = -1;

			if (i == top)
				c->ovl_id = 0;
			else
				c->ovl_id = 1;
		}
	}
}

static int get_hrt_bound(int is_larb, int hrt_level)
{
	if (is_larb)
		return larb_bound_table[l_rule_info.bound_tb_idx][hrt_level];
	else
		return emi_bound_table[l_rule_info.bound_tb_idx][hrt_level];
}

static int *get_bound_table(enum DISP_HW_MAPPING_TB_TYPE tb_type)
{
	switch (tb_type) {
	case DISP_HW_EMI_BOUND_TB:
		return emi_bound_table[l_rule_info.bound_tb_idx];
	case DISP_HW_LARB_BOUND_TB:
		return larb_bound_table[l_rule_info.bound_tb_idx];
	default:
		return NULL;
	}
}

static int get_mapping_table(enum DISP_HW_MAPPING_TB_TYPE tb_type, int param)
{
	int layer_tb_idx = l_rule_info.layer_tb_idx;

	switch (tb_type) {
	case DISP_HW_OVL_TB:
		return ovl_mapping_table[layer_tb_idx];
	case DISP_HW_LARB_TB:
		return larb_mapping_table[layer_tb_idx];
	case DISP_HW_LAYER_TB:
		if (param < MAX_PHY_OVL_CNT && param >= 0)
			return layer_mapping_table[layer_tb_idx][param];
		else
			return -1;
	default:
		return -1;
	}
}

void layering_rule_init(void)
{
	l_rule_info.primary_fps = 60;
	register_layering_rule_ops(&l_rule_ops, &l_rule_info);
}

static struct layering_rule_ops l_rule_ops = {
	.scenario_decision = layering_rule_senario_decision,
	.get_bound_table = get_bound_table,
	.get_hrt_bound = get_hrt_bound,
	.get_mapping_table = get_mapping_table,
	.rollback_to_gpu_by_hw_limitation = filter_by_hw_limitation,
	.clear_layer = clear_layer,
};
