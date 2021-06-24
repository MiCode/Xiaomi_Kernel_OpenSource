/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-drm-adaptor.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk-mml-pq-core.h"

#include "tile_driver.h"
#include "tile_mdp_reg.h"

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

struct rsz_data {
	u32 tile_width;
};

static const struct rsz_data mt6893_rsz_data = {
	.tile_width = 544
};

struct mml_comp_rsz {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct rsz_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct rsz_frame_data {
	/* 0 or 1 for 1st or 2nd out port */
	u8 out_idx;

	bool use121filter:1;
};

#define rsz_frm_data(i)	((struct rsz_frame_data *)(i->data))
#define RSZ_WAIT_TIMEOUT_MS 50

static inline struct mml_comp_rsz *comp_to_rsz(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_rsz, comp);
}

static s32 rsz_config_scale(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct rsz_frame_data *rsz_frm;
	s32 ret = 0;

	rsz_frm = kzalloc(sizeof(*rsz_frm), GFP_KERNEL);
	ccfg->data = rsz_frm;
	/* cache out index for easy use */
	rsz_frm->out_idx = ccfg->node->out_idx;

	ret = mml_pq_tile_init(task);
	if (ret)
		return ret;

	return ret;
}

static bool rsz_relay(struct mml_frame_config *cfg,
		      struct mml_frame_data *src,
		      struct mml_frame_dest *dest)
{
	if (cfg->info.dest_cnt == 1) {
		u32 out_width, out_height;

		if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270) {
			out_width = dest->data.height;
			out_height = dest->data.width;
		} else {
			out_width = dest->data.width;
			out_height = dest->data.height;
		}
		if (dest->crop.r.width == src->width &&
		    src->width == out_width &&
		    dest->crop.r.height == src->height &&
		    src->height == out_height &&
		    dest->crop.x_sub_px == 0 && dest->crop.y_sub_px == 0)
			return true;
		else
			return false;
	} else
		return false;
}

static struct mml_pq_tile_init_result *get_init_result(struct mml_task *task)
{
	struct mml_pq_sub_task *sub_task = NULL;

	if (task->pq_task)
		sub_task = &task->pq_task->tile_init;
	if (sub_task)
		return (struct mml_pq_tile_init_result *)sub_task->result;
	else
		return NULL;
}

static s32 rsz_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    void *ptr_func, void *tile_data)
{
	TILE_FUNC_BLOCK_STRUCT *func = (TILE_FUNC_BLOCK_STRUCT*)ptr_func;
	struct rsz_tile_data *rsz_data = &((struct mml_tile_data*)tile_data)->rsz_data;
	struct rsz_frame_data *rsz_frm = rsz_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_frame_dest *dest = &cfg->info.dest[rsz_frm->out_idx];
	struct mml_comp_rsz *rsz = comp_to_rsz(comp);
	struct mml_pq_tile_init_result *result;
	struct mml_pq_rsz_tile_init_param *init_param;
	bool rsz_relay_mode = rsz_relay(cfg, src, dest);
	u32 in_crop_w, in_crop_h;
	s32 ret;

	ret = mml_pq_get_tile_init_result(task, RSZ_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_init_result(task);
		if (rsz_frm->out_idx < result->rsz_param_cnt) {
			mml_log("read rsz param index: %d", rsz_frm->out_idx);
			init_param = &(result->rsz_param[rsz_frm->out_idx]);
			rsz_data->coef_step_x = init_param->coeff_step_x;
			rsz_data->coef_step_y = init_param->coeff_step_y;
			rsz_data->precision_x = init_param->precision_x;
			rsz_data->precision_y = init_param->precision_y;
			rsz_data->crop.r.left = init_param->crop_offset_x;
			rsz_data->crop.x_sub_px = init_param->crop_subpix_x;
			rsz_data->crop.r.top = init_param->crop_offset_y;
			rsz_data->crop.y_sub_px = init_param->crop_subpix_y;
			rsz_data->hor_scale = init_param->hor_dir_scale;
			rsz_data->hor_algo = init_param->hor_algorithm;
			rsz_data->vir_scale = init_param->ver_dir_scale;
			rsz_data->ver_algo = init_param->ver_algorithm;
			rsz_data->ver_first = init_param->vertical_first;
			rsz_data->ver_cubic_trunc = init_param->ver_cubic_trunc;
			mml_log("read rsz param index: %d done", rsz_frm->out_idx);
		} else {
			mml_err("read rsz param index: %d out of count",
				rsz_frm->out_idx, result->rsz_param_cnt);
		}
	} else {
		mml_err("get rsz param timeout: %d in %dms",
			ret, RSZ_WAIT_TIMEOUT_MS);
	}

	rsz_data->max_width = rsz->data->tile_width;
	func->func_data = (struct TILE_FUNC_DATA_STRUCT*)(rsz_data);


	if (rsz_relay_mode)
		func->enable_flag = false;
	else
		func->enable_flag = true;

	in_crop_w = dest->crop.r.width;
	in_crop_h = dest->crop.r.height;
	if (in_crop_w + dest->crop.r.left > src->width)
		in_crop_w = src->width - dest->crop.r.left;
	if (in_crop_h + dest->crop.r.top > src->height)
		in_crop_h = src->height - dest->crop.r.top;
	if (cfg->info.dest_cnt == 1 &&
	    (dest->crop.r.width != src->width ||
	    dest->crop.r.height != src->height)) {
		func->full_size_x_in = in_crop_w + dest->crop.r.left;
		func->full_size_y_in = in_crop_h + dest->crop.r.top;
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

	return 0;
}

static const struct mml_comp_tile_ops rsz_tile_ops = {
	.prepare = rsz_tile_prepare,
};

static s32 rsz_init(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0x10000, 0x00010000);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0, 0x00010000);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0x1, 0x00000001);
	return 0;
}

static s32 rsz_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct rsz_frame_data *rsz_frm = rsz_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_frame_dest *dest = &cfg->info.dest[rsz_frm->out_idx];
	struct mml_pq_tile_init_result *result;
	bool rsz_relay_mode = rsz_relay(cfg, src, dest);

	mml_log("%s is called", __func__);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ETC_CONTROL, 0x0, U32_MAX);

	if (rsz_relay_mode) {
		/* relay mode */
		cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0, 0x00000001);
		return 0;
	}

	result = get_init_result(task);
	if (result) {
		s32 i;
		struct mml_pq_reg *regs = result->rsz_regs;
		/* TODO: use different regs */
		mml_log("%s:config rsz regs, count: %d", __func__, result->rsz_reg_cnt);
		for (i = 0; i < result->rsz_reg_cnt; i++) {
			cmdq_pkt_write(pkt, NULL, base_pa + regs[i].offset,
				regs[i].value, regs[i].mask);
			mml_log("[rsz][config][%x] = %#x mask(%#x)",
				regs[i].offset, regs[i].value, regs[i].mask);
		}
	} else {
		mml_err("%s: not get result from user lib", __func__);
	}

	rsz_frm->use121filter = !MML_FMT_H_SUBSAMPLE(src->format);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_CON_1,
		       rsz_frm->use121filter << 26, 0x04000000);
	mml_log("%s is end", __func__);
	return 0;
}

static s32 rsz_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* frame data should not change between each tile */
	const struct rsz_frame_data *rsz_frm = rsz_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[rsz_frm->out_idx];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 drs_lclip_en;
	u32 drs_padding_dis;
	u32 urs_clip_en;
	u32 rsz_input_w;
	u32 rsz_input_h;
	u32 rsz_output_w;
	u32 rsz_output_h;

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
	if (dest->rotate == MML_ROT_90 ||
	    dest->rotate == MML_ROT_270) {
		if (tile->out.xe >= dest->data.height- 1)
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
	.prepare = rsz_config_scale,
	.init = rsz_init,
	.frame = rsz_config_frame,
	.tile = rsz_config_tile,
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

	mml_err("rsz component %u dump:", comp->id);

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

	writel(0x1, (volatile void *)base + RSZ_DEBUG_SEL);
	debug[0] = readl(base + RSZ_DEBUG);
	writel(0x2, (volatile void *)base + RSZ_DEBUG_SEL);
	debug[1] = readl(base + RSZ_DEBUG);
	writel(0x3, (volatile void *)base + RSZ_DEBUG_SEL);
	debug[2] = readl(base + RSZ_DEBUG);
	writel(0x9, (volatile void *)base + RSZ_DEBUG_SEL);
	debug[3] = readl(base + RSZ_DEBUG);
	writel(0xa, (volatile void *)base + RSZ_DEBUG_SEL);
	debug[4] = readl(base + RSZ_DEBUG);
	writel(0xb, (volatile void *)base + RSZ_DEBUG_SEL);
	debug[5] = readl(base + RSZ_DEBUG);
	writel(0xd, (volatile void *)base + RSZ_DEBUG_SEL);
	debug[6] = readl(base + RSZ_DEBUG);
	writel(0xe, (volatile void *)base + RSZ_DEBUG_SEL);
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
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 ret = -1, temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		ret = mml_register_comp(master, &rsz->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		drm_dev = data;
		ret = mml_ddp_comp_register(drm_dev, &rsz->ddp_comp);
		if (ret < 0)
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
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		mml_unregister_comp(master, &rsz->comp);
	} else {
		drm_dev = data;
		mml_ddp_comp_unregister(drm_dev, &rsz->ddp_comp);
		rsz->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_rsz *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_rsz *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (const struct rsz_data *)of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.tile_ops = &rsz_tile_ops;
	priv->comp.config_ops = &rsz_cfg_ops;
	priv->comp.debug_ops = &rsz_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
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

const struct of_device_id mtk_mml_rsz_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_rsz",
		.data = &mt6893_rsz_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mml_rsz_driver_dt_match);

struct platform_driver mtk_mml_rsz_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-rsz",
		.owner = THIS_MODULE,
		.of_match_table = mtk_mml_rsz_driver_dt_match,
	},
};

//module_platform_driver(mtk_mml_rsz_driver);

static s32 ut_case;
static s32 ut_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = sscanf(val, "%d", &ut_case);
	if (result != 1) {
		mml_err("invalid input: %s, result(%d)", val, result);
		return -EINVAL;
	}
	mml_log("%s: case_id=%d", __func__, ut_case);

	switch (ut_case) {
	case 0:
		mml_log("use read to dump current pwm setting");
		break;
	default:
		mml_err("invalid case_id: %d", ut_case);
		break;
	}

	mml_log("%s END", __func__);
	return 0;
}

static s32 ut_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (ut_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", ut_case, dbg_probed_count);
		for(i = 0; i < dbg_probed_count; i++) {
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml_comp_id: %d\n", i,
				dbg_probed_components[i]->comp.id);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      mml_bound: %d\n",
				dbg_probed_components[i]->comp.bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      ddp_comp_id: %d\n",
				dbg_probed_components[i]->ddp_comp.id);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      ddp_bound: %d\n",
				dbg_probed_components[i]->ddp_bound);
		}
	default:
		mml_err("not support read for case_id: %d", ut_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static struct kernel_param_ops up_param_ops = {
	.set = ut_set,
	.get = ut_get,
};
module_param_cb(rsz_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(rsz_ut_case, "mml rsz UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML RSZ driver");
MODULE_LICENSE("GPL v2");
