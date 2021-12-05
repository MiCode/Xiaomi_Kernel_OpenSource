/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-pq-core.h"

#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"

#define RSZ_ENABLE			0x000
#define RSZ_CON_1			0x004
#define RSZ_CON_2			0x008
#define RSZ_INT_FLAG			0x00c
#define RSZ_INPUT_IMAGE			0x010
#define RSZ_OUTPUT_IMAGE		0x014
#define RSZ_HOR_COEFF_STEP		0x018
#define RSZ_VER_COEFF_STEP		0x01c
#define RSZ_LUMA_HOR_INT_OFFSET		0x020
#define RSZ_LUMA_HOR_SUB_OFFSET		0x024
#define RSZ_LUMA_VER_INT_OFFSET		0x028
#define RSZ_LUMA_VER_SUB_OFFSET		0x02c
#define RSZ_CHROMA_HOR_INT_OFFSET	0x030
#define RSZ_CHROMA_HOR_SUB_OFFSET	0x034
#define RSZ_RSV				0x040
#define RSZ_DEBUG_SEL			0x044
#define RSZ_DEBUG			0x048
#define RSZ_TAP_ADAPT			0x04c
#define RSZ_IBSE_SOFTCLIP		0x050
#define RSZ_IBSE_YLEVEL_1		0x054
#define RSZ_IBSE_YLEVEL_2		0x058
#define RSZ_IBSE_YLEVEL_3		0x05c
#define RSZ_IBSE_YLEVEL_4		0x060
#define RSZ_IBSE_YLEVEL_5		0x064
#define RSZ_IBSE_GAINCON_1		0x068
#define RSZ_IBSE_GAINCON_2		0x06c
#define RSZ_DEMO_IN_HMASK		0x070
#define RSZ_DEMO_IN_VMASK		0x074
#define RSZ_DEMO_OUT_HMASK		0x078
#define RSZ_DEMO_OUT_VMASK		0x07c
#define RSZ_SHADOW_CTRL			0x0f0
#define RSZ_ATPG			0x0fc
#define RSZ_PAT1_GEN_SET		0x100
#define RSZ_PAT1_GEN_FRM_SIZE		0x104
#define RSZ_PAT1_GEN_COLOR0		0x108
#define RSZ_PAT1_GEN_COLOR1		0x10c
#define RSZ_PAT1_GEN_COLOR2		0x110
#define RSZ_PAT1_GEN_POS		0x114
#define RSZ_PAT1_GEN_TILE_POS		0x124
#define RSZ_PAT1_GEN_TILE_OV		0x128
#define RSZ_PAT2_GEN_SET		0x200
#define RSZ_PAT2_GEN_COLOR0		0x208
#define RSZ_PAT2_GEN_COLOR1		0x20c
#define RSZ_PAT2_GEN_POS		0x214
#define RSZ_PAT2_GEN_CURSOR_RB0		0x218
#define RSZ_PAT2_GEN_CURSOR_RB1		0x21c
#define RSZ_PAT2_GEN_TILE_POS		0x224
#define RSZ_PAT2_GEN_TILE_OV		0x228
#define RSZ_ETC_CONTROL			0x22c
#define RSZ_ETC_SWITCH_MAX_MIN_1	0x230
#define RSZ_ETC_SWITCH_MAX_MIN_2	0x234
#define RSZ_ETC_RING			0x238
#define RSZ_ETC_RING_GAINCON_1		0x23c
#define RSZ_ETC_RING_GAINCON_2		0x240
#define RSZ_ETC_RING_GAINCON_3		0x244
#define RSZ_ETC_SIM_PROT_GAINCON_1	0x248
#define RSZ_ETC_SIM_PROT_GAINCON_2	0x24c
#define RSZ_ETC_SIM_PROT_GAINCON_3	0x250
#define RSZ_ETC_BLEND			0x254

#define RSZ_WAIT_TIMEOUT_MS 5000

struct rsz_data {
	u32 tile_width;
};

static const struct rsz_data mt6893_rsz_data = {
	.tile_width = 544
};

static const struct rsz_data mt6983_rsz_data = {
	.tile_width = 1636,
};

static const struct rsz_data mt6879_rsz_data = {
	.tile_width = 1360,
};

static const struct rsz_data mt6895_rsz0_data = {
	.tile_width = 1300,
};

static const struct rsz_data mt6895_rsz1_data = {
	.tile_width = 836,
};

static const struct rsz_data mt6895_rsz_data = {
	.tile_width = 520,
};

struct mml_comp_rsz {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct rsz_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct rsz_frame_data {
	bool relay_mode:1;
	bool use121filter:1;
};

static inline struct rsz_frame_data *rsz_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_rsz *comp_to_rsz(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_rsz, comp);
}

static bool rsz_can_relay(struct mml_frame_config *cfg,
			  const struct mml_frame_data *src,
			  const struct mml_frame_dest *dest)
{
	u32 out_width, out_height;

	if (cfg->info.dest_cnt > 1)
		return false;

	if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270) {
		out_width = dest->data.height;
		out_height = dest->data.width;
	} else {
		out_width = dest->data.width;
		out_height = dest->data.height;
	}
	if (dest->crop.r.width == src->width && src->width == out_width &&
	    dest->crop.r.height == src->height && src->height == out_height &&
	    dest->crop.x_sub_px == 0 && dest->crop.y_sub_px == 0 &&
	    dest->crop.w_sub_px == 0 && dest->crop.h_sub_px == 0)
		return true;

	return false;
}

static s32 rsz_prepare(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct rsz_frame_data *rsz_frm;
	s32 ret = 0;

	mml_trace_ex_begin("%s", __func__);

	rsz_frm = kzalloc(sizeof(*rsz_frm), GFP_KERNEL);
	ccfg->data = rsz_frm;
	rsz_frm->relay_mode = rsz_can_relay(cfg, src, dest);
	/* C42 conversion: drop if source is YUV422 or YUV420 */
	rsz_frm->use121filter = !MML_FMT_H_SUBSAMPLE(src->format);

	mml_pq_msg("%s pipe_id[%d] engine_id[%d]", __func__, ccfg->pipe, comp->id);
	if (!rsz_frm->relay_mode)
		ret = mml_pq_set_tile_init(task);

	mml_trace_ex_end();
	return ret;
}

static struct mml_pq_tile_init_result *get_tile_init_result(struct mml_task *task)
{
	return task->pq_task->tile_init.result;
}

static s32 prepare_tile_data(struct rsz_tile_data *data, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_pq_tile_init_result *result;
	struct mml_pq_rsz_tile_init_param *init_param;
	s32 ret;

	ret = mml_pq_get_tile_init_result(task, RSZ_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_tile_init_result(task);
		if (result && ccfg->node->out_idx < result->rsz_param_cnt) {
			mml_log("%s read rsz param index: %d job_id[%d]",
				__func__, ccfg->node->out_idx, task->job.jobid);
			init_param = &(result->rsz_param[ccfg->node->out_idx]);
			data->coeff_step_x = init_param->coeff_step_x;
			data->coeff_step_y = init_param->coeff_step_y;
			data->precision_x = init_param->precision_x;
			data->precision_y = init_param->precision_y;
			data->crop.r.left = init_param->crop_offset_x;
			data->crop.x_sub_px = init_param->crop_subpix_x;
			data->crop.r.top = init_param->crop_offset_y;
			data->crop.y_sub_px = init_param->crop_subpix_y;
			data->hor_scale = init_param->hor_dir_scale;
			data->hor_algo = init_param->hor_algorithm;
			data->ver_scale = init_param->ver_dir_scale;
			data->ver_algo = init_param->ver_algorithm;
			data->ver_first = init_param->vertical_first;
			data->ver_cubic_trunc = init_param->ver_cubic_trunc;
		} else if (result) {
			mml_err("%s read rsz param index: %d out of count %d",
				__func__, ccfg->node->out_idx, result->rsz_param_cnt);
		} else {
			mml_err("%s read rsz param index: %d",
				__func__, ccfg->node->out_idx);
		}
	} else {
		mml_err("get rsz param timeout: %d in %dms, job_id[%d]",
			ret, RSZ_WAIT_TIMEOUT_MS, task->job.jobid);
	}
	return 0;
}

static s32 rsz_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct tile_func_block *func,
			    union mml_tile_data *data)
{
	struct rsz_frame_data *rsz_frm = rsz_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_rsz *rsz = comp_to_rsz(comp);
	u32 in_crop_w, in_crop_h;

	mml_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s pipe_id[%d] engine_id[%d]", __func__, ccfg->pipe, comp->id);

	data->rsz.crop = dest->crop;
	if (!rsz_frm->relay_mode) {
		data->rsz.use_121filter = rsz_frm->use121filter;
		prepare_tile_data(&data->rsz, task, ccfg);
	}
	data->rsz.max_width = rsz->data->tile_width;
	/* RSZ support crop capability */
	func->type = TILE_TYPE_CROP_EN;
	func->init_func = tile_prz_init;
	func->for_func = tile_prz_for;
	func->back_func = tile_prz_back;
	func->data = data;

	func->enable_flag = !rsz_frm->relay_mode;

	in_crop_w = dest->crop.r.width;
	in_crop_h = dest->crop.r.height;
	if (in_crop_w + dest->crop.r.left > src->width)
		in_crop_w = src->width - dest->crop.r.left;
	if (in_crop_h + dest->crop.r.top > src->height)
		in_crop_h = src->height - dest->crop.r.top;
	if ((cfg->info.dest_cnt == 1 ||
	     !memcmp(&cfg->info.dest[0].crop,
		     &cfg->info.dest[1].crop,
		     sizeof(struct mml_crop))) &&
	    (dest->crop.r.width != src->width ||
	    dest->crop.r.height != src->height)) {
		func->full_size_x_in = in_crop_w;
		func->full_size_y_in = in_crop_h;
		data->rsz.crop.r.left -= dest->crop.r.left;
		data->rsz.crop.r.top -= dest->crop.r.top;
	} else {
		func->full_size_x_in = src->width;
		func->full_size_y_in = src->height;
	}
	if (dest->rotate == MML_ROT_90 ||
	    dest->rotate == MML_ROT_270) {
		func->full_size_x_out = dest->data.height;
		func->full_size_y_out = dest->data.width;
	} else {
		func->full_size_x_out = dest->data.width;
		func->full_size_y_out = dest->data.height;
	}

	mml_trace_ex_end();
	return 0;
}

static const struct mml_comp_tile_ops rsz_tile_ops = {
	.prepare = rsz_tile_prepare,
};

static void rsz_init(struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0x1, U32_MAX);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_SHADOW_CTRL, 0x2, U32_MAX);
}

static void rsz_relay(struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	/* relay mode */
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0x0, U32_MAX);
}

static s32 rsz_config_init(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	rsz_init(task->pkts[ccfg->pipe], comp->base_pa);
	return 0;
}

static s32 rsz_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct rsz_frame_data *rsz_frm = rsz_frm_data(ccfg);
	struct mml_pq_tile_init_result *result;

	mml_msg("%s is called", __func__);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ETC_CONTROL, 0x0, U32_MAX);

	if (rsz_frm->relay_mode) {
		rsz_relay(pkt, base_pa);
		return 0;
	}

	result = get_tile_init_result(task);
	if (result) {
		s32 i;
		struct mml_pq_reg *regs = result->rsz_regs[ccfg->node->out_idx];

		/* TODO: use different regs */
		mml_msg("%s:config rsz regs, count: %d", __func__,
			result->rsz_reg_cnt[ccfg->node->out_idx]);
		for (i = 0; i < result->rsz_reg_cnt[ccfg->node->out_idx]; i++) {
			cmdq_pkt_write(pkt, NULL, base_pa + regs[i].offset,
				regs[i].value, regs[i].mask);
			mml_msg("[rsz][config][%x] = %#x mask(%#x)",
				regs[i].offset, regs[i].value, regs[i].mask);
		}
	} else {
		mml_err("%s: not get result from user lib", __func__);
	}

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_CON_1,
		       rsz_frm->use121filter << 26, 0x04000000);

	mml_pq_put_tile_init_result(task);
	mml_msg("%s is end", __func__);
	return 0;
}

static s32 rsz_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* frame data should not change between each tile */
	const struct rsz_frame_data *rsz_frm = rsz_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 drs_lclip_en;
	u32 drs_padding_dis;
	u32 urs_clip_en;
	u32 rsz_input_w;
	u32 rsz_input_h;
	u32 rsz_output_w;
	u32 rsz_output_h;

	mml_msg("%s idx[%d]", __func__, idx);
	mml_pq_msg("%s pipe_id[%d] engine_id[%d]", __func__, ccfg->pipe, comp->id);

	if (!(tile->in.xe & 0x1))
		/* Odd coordinate, should pad 1 column */
		drs_padding_dis = 0;
	else
		/* Even coordinate, no padding required */
		drs_padding_dis = 1;

	if (rsz_frm->use121filter && tile->in.xs)
		drs_lclip_en = 1;
	else
		drs_lclip_en = 0;

	rsz_input_w = tile->in.xe - tile->in.xs + 1;
	rsz_input_h = tile->in.ye - tile->in.ys + 1;
	rsz_output_w = tile->out.xe - tile->out.xs + 1;
	rsz_output_h = tile->out.ye - tile->out.ys + 1;

	/* YUV422 to YUV444 upsampler */
	if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270) {
		if (tile->out.xe >= dest->data.height - 1)
			urs_clip_en = 0;
		else
			urs_clip_en = 1;
	} else {
		if (tile->out.xe >= dest->data.width - 1)
			urs_clip_en = 0;
		else
			urs_clip_en = 1;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_CON_2,
		       (drs_lclip_en << 11) +
		       (drs_padding_dis << 12) +
		       (urs_clip_en << 13), 0x00003800);

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_INPUT_IMAGE,
		       (rsz_input_h << 16) + rsz_input_w, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_LUMA_HOR_INT_OFFSET,
		       tile->luma.x, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_LUMA_HOR_SUB_OFFSET,
		       tile->luma.x_sub, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_LUMA_VER_INT_OFFSET,
		       tile->luma.y, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_LUMA_VER_SUB_OFFSET,
		       tile->luma.y_sub, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_CHROMA_HOR_INT_OFFSET,
		       tile->chroma.x, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_CHROMA_HOR_SUB_OFFSET,
		       tile->chroma.x_sub, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_OUTPUT_IMAGE,
		       (rsz_output_h << 16) + rsz_output_w, U32_MAX);

	return 0;
}

static const struct mml_comp_config_ops rsz_cfg_ops = {
	.prepare = rsz_prepare,
	.init = rsz_config_init,
	.frame = rsz_config_frame,
	.tile = rsz_config_tile,
};

static void rsz_task_done_callback(struct mml_comp *comp, struct mml_task *task,
					 struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const uint8_t dest_cnt = cfg->info.dest_cnt;

	if (dest_cnt == MML_MAX_OUTPUTS &&
	    ccfg->node->out_idx == MML_MAX_OUTPUTS - 1) {
		mml_pq_trace_ex_begin("%s", __func__);
		mml_msg("%s out_idx[%d] id[%d]", __func__,
			ccfg->node->out_idx, comp->id);
		mml_pq_rsz_callback(task);
		mml_pq_trace_ex_end();
	}
}

static const struct mml_comp_hw_ops rsz_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.task_done = rsz_task_done_callback,
};

const char *get_rsz_state(const u32 state)
{
	switch (state) {
	case 0x5:
		/* 0,1,0,1 */
		return "downstream hang";
	case 0xa:
		/* 1,0,1,0 */
		return "upstream hang";
	default:
		return "";
	}
}

static void rsz_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[30];
	u32 debug[8];
	u32 state;
	u32 request[4];
	u32 shadow_ctrl;

	mml_err("rsz component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + RSZ_SHADOW_CTRL);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + RSZ_SHADOW_CTRL);

	value[0] = readl(base + RSZ_ENABLE);
	value[1] = readl(base + RSZ_CON_1);
	value[2] = readl(base + RSZ_CON_2);
	value[3] = readl(base + RSZ_INT_FLAG);
	value[4] = readl(base + RSZ_INPUT_IMAGE);
	value[5] = readl(base + RSZ_OUTPUT_IMAGE);
	value[6] = readl(base + RSZ_HOR_COEFF_STEP);
	value[7] = readl(base + RSZ_VER_COEFF_STEP);
	value[8] = readl(base + RSZ_LUMA_HOR_INT_OFFSET);
	value[9] = readl(base + RSZ_LUMA_HOR_SUB_OFFSET);
	value[10] = readl(base + RSZ_LUMA_VER_INT_OFFSET);
	value[11] = readl(base + RSZ_LUMA_VER_SUB_OFFSET);
	value[12] = readl(base + RSZ_CHROMA_HOR_INT_OFFSET);
	value[13] = readl(base + RSZ_CHROMA_HOR_SUB_OFFSET);
	value[14] = readl(base + RSZ_RSV);
	value[15] = readl(base + RSZ_TAP_ADAPT);
	value[16] = readl(base + RSZ_IBSE_SOFTCLIP);
	value[17] = readl(base + RSZ_PAT1_GEN_SET);
	value[18] = readl(base + RSZ_PAT2_GEN_SET);
	value[19] = readl(base + RSZ_ETC_CONTROL);
	value[20] = readl(base + RSZ_ETC_SWITCH_MAX_MIN_1);
	value[21] = readl(base + RSZ_ETC_SWITCH_MAX_MIN_2);
	value[22] = readl(base + RSZ_ETC_RING);
	value[23] = readl(base + RSZ_ETC_RING_GAINCON_1);
	value[24] = readl(base + RSZ_ETC_RING_GAINCON_2);
	value[25] = readl(base + RSZ_ETC_RING_GAINCON_3);
	value[26] = readl(base + RSZ_ETC_SIM_PROT_GAINCON_1);
	value[27] = readl(base + RSZ_ETC_SIM_PROT_GAINCON_2);
	value[28] = readl(base + RSZ_ETC_SIM_PROT_GAINCON_3);
	value[29] = readl(base + RSZ_ETC_BLEND);

	writel(0x1, base + RSZ_DEBUG_SEL);
	debug[0] = readl(base + RSZ_DEBUG);
	writel(0x2, base + RSZ_DEBUG_SEL);
	debug[1] = readl(base + RSZ_DEBUG);
	writel(0x3, base + RSZ_DEBUG_SEL);
	debug[2] = readl(base + RSZ_DEBUG);
	writel(0x9, base + RSZ_DEBUG_SEL);
	debug[3] = readl(base + RSZ_DEBUG);
	writel(0xa, base + RSZ_DEBUG_SEL);
	debug[4] = readl(base + RSZ_DEBUG);
	writel(0xb, base + RSZ_DEBUG_SEL);
	debug[5] = readl(base + RSZ_DEBUG);
	writel(0xd, base + RSZ_DEBUG_SEL);
	debug[6] = readl(base + RSZ_DEBUG);
	writel(0xe, base + RSZ_DEBUG_SEL);
	debug[7] = readl(base + RSZ_DEBUG);

	mml_err("RSZ_ENABLE %#010x RSZ_CON_1 %#010x RSZ_CON_2 %#010x RSZ_INT_FLAG %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("RSZ_INPUT_IMAGE %#010x RSZ_OUTPUT_IMAGE %#010x",
		value[4], value[5]);
	mml_err("RSZ_HOR_COEFF_STEP %#010x RSZ_VER_COEFF_STEP %#010x",
		value[6], value[7]);
	mml_err("RSZ_LUMA_HOR_INT_OFFSET %#010x RSZ_LUMA_HOR_SUB_OFFSET %#010x",
		value[8], value[9]);
	mml_err("RSZ_LUMA_VER_INT_OFFSET %#010x RSZ_LUMA_VER_SUB_OFFSET %#010x",
		value[10], value[11]);
	mml_err("RSZ_CHROMA_HOR_INT_OFFSET %#010x RSZ_CHROMA_HOR_SUB_OFFSET %#010x",
		value[12], value[13]);
	mml_err("RSZ_RSV %#010x RSZ_TAP_ADAPT %#010x RSZ_IBSE_SOFTCLIP %#010x",
		value[14], value[15], value[16]);
	mml_err("RSZ_PAT1_GEN_SET %#010x RSZ_PAT2_GEN_SET %#010x",
		value[17], value[18]);
	mml_err("RSZ_ETC_CONTROL %#010x RSZ_ETC_RING %#010x RSZ_ETC_BLEND %#010x",
		value[19], value[22], value[29]);
	mml_err("RSZ_ETC_SWITCH_MAX_MIN_1 %#010x RSZ_ETC_SWITCH_MAX_MIN_2 %#010x",
		value[20], value[21]);
	mml_err("RSZ_ETC_RING_GAINCON_1 %#010x RSZ_ETC_RING_GAINCON_2 %#010x RSZ_ETC_RING_GAINCON_3 %#010x",
		value[23], value[24], value[25]);
	mml_err("RSZ_ETC_SIM_PROT_GAINCON_1 %#010x RSZ_ETC_SIM_PROT_GAINCON_2 %#010x RSZ_ETC_SIM_PROT_GAINCON_3 %#010x",
		value[26], value[27], value[28]);
	mml_err("RSZ_DEBUG_1 %#010x RSZ_DEBUG_2 %#010x RSZ_DEBUG_3 %#010x",
		debug[0], debug[1], debug[2]);
	mml_err("RSZ_DEBUG_9 %#010x RSZ_DEBUG_10 %#010x RSZ_DEBUG_11 %#010x",
		debug[3], debug[4], debug[5]);
	mml_err("RSZ_DEBUG_13 %#010x RSZ_DEBUG_14 %#010x",
		debug[6], debug[7]);

	state = debug[1] & 0xf;
	request[0] = state & 0x1;
	request[1] = (state & (0x1 << 1)) >> 1;
	request[2] = (state & (0x1 << 2)) >> 2;
	request[3] = (state & (0x1 << 3)) >> 3;
	mml_err("RSZ inRdy,inRsq,outRdy,outRsq: %d,%d,%d,%d (%s)",
		request[3], request[2], request[1], request[0], get_rsz_state(state));
}

static const struct mml_comp_debug_ops rsz_debug_ops = {
	.dump = &rsz_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rsz *rsz = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &rsz->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &rsz->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			rsz->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rsz *rsz = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &rsz->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &rsz->ddp_comp);
		rsz->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
};

static struct mml_comp_rsz *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_rsz *priv;
	s32 ret;
	bool add_ddp = true;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}
	/* assign ops */
	priv->comp.tile_ops = &rsz_tile_ops;
	priv->comp.config_ops = &rsz_cfg_ops;
	priv->comp.hw_ops = &rsz_hw_ops;
	priv->comp.debug_ops = &rsz_debug_ops;

	ret = mml_ddp_comp_init(dev, &priv->ddp_comp, &priv->comp,
				&ddp_comp_funcs);
	if (ret) {
		mml_log("failed to init ddp component: %d", ret);
		add_ddp = false;
	}

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (add_ddp)
		ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_rsz_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_rsz",
		.data = &mt6983_rsz_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_rsz",
		.data = &mt6893_rsz_data
	},
	{
		.compatible = "mediatek,mt6879-mml_rsz",
		.data = &mt6879_rsz_data
	},
	{
		.compatible = "mediatek,mt6895-mml_rsz0",
		.data = &mt6895_rsz0_data
	},
	{
		.compatible = "mediatek,mt6895-mml_rsz1",
		.data = &mt6895_rsz1_data
	},
	{
		.compatible = "mediatek,mt6895-mml_rsz2",
		.data = &mt6895_rsz_data
	},
	{
		.compatible = "mediatek,mt6895-mml_rsz3",
		.data = &mt6895_rsz_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_rsz_driver_dt_match);

struct platform_driver mml_rsz_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-rsz",
		.owner = THIS_MODULE,
		.of_match_table = mml_rsz_driver_dt_match,
	},
};

//module_platform_driver(mml_rsz_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (i = 0; i < dbg_probed_count; i++) {
			struct mml_comp *comp = &dbg_probed_components[i]->comp;

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml comp_id: %d.%d @%08x name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%08x pw: %d clk: %d\n",
				comp->larb_port, comp->larb_base,
				comp->pw_cnt, comp->clk_cnt);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -     ddp comp_id: %d bound: %d\n",
				dbg_probed_components[i]->ddp_comp.id,
				dbg_probed_components[i]->ddp_bound);
		}
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(rsz_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(rsz_debug, "mml rsz debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML RSZ driver");
MODULE_LICENSE("GPL v2");
