/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
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

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_tdshp]" fmt


#define TDSHP_00		0x000
#define TDSHP_01		0x004
#define TDSHP_02		0x008
#define TDSHP_03		0x00c
#define TDSHP_05		0x014
#define TDSHP_06		0x018
#define TDSHP_07		0x01c
#define TDSHP_08		0x020
#define TDSHP_09		0x024
#define PBC_00			0x040
#define PBC_01			0x044
#define PBC_02			0x048
#define PBC_03			0x04c
#define PBC_04			0x050
#define PBC_05			0x054
#define PBC_06			0x058
#define PBC_07			0x05c
#define PBC_08			0x060
#define HIST_CFG_00		0x064
#define HIST_CFG_01		0x068
#define LUMA_HIST_00		0x06c
#define LUMA_HIST_01		0x070
#define LUMA_HIST_02		0x074
#define LUMA_HIST_03		0x078
#define LUMA_HIST_04		0x07c
#define LUMA_HIST_05		0x080
#define LUMA_HIST_06		0x084
#define LUMA_HIST_07		0x08c
#define LUMA_HIST_08		0x090
#define LUMA_HIST_09		0x094
#define LUMA_HIST_10		0x098
#define LUMA_HIST_11		0x09c
#define LUMA_HIST_12		0x0a0
#define LUMA_HIST_13		0x0a4
#define LUMA_HIST_14		0x0a8
#define LUMA_HIST_15		0x0ac
#define LUMA_HIST_16		0x0b0
#define LUMA_SUM		0x0b4
#define Y_FTN_1_0_MAIN		0x0bc
#define Y_FTN_3_2_MAIN		0x0c0
#define Y_FTN_5_4_MAIN		0x0c4
#define Y_FTN_7_6_MAIN		0x0c8
#define Y_FTN_9_8_MAIN		0x0cc
#define Y_FTN_11_10_MAIN	0x0d0
#define Y_FTN_13_12_MAIN	0x0d4
#define Y_FTN_15_14_MAIN	0x0d8
#define Y_FTN_17_16_MAIN	0x0dc
#define C_BOOST_MAIN		0x0e0
#define C_BOOST_MAIN_2		0x0e4
#define TDSHP_ATPG		0x0fc
#define TDSHP_CTRL		0x100
#define TDSHP_INTEN		0x104
#define TDSHP_INTSTA		0x108
#define TDSHP_STATUS		0x10c
#define TDSHP_CFG		0x110
#define TDSHP_INPUT_COUNT	0x114
#define TDSHP_CHKSUM		0x118
#define TDSHP_OUTPUT_COUNT	0x11c
#define TDSHP_INPUT_SIZE	0x120
#define TDSHP_OUTPUT_OFFSET	0x124
#define TDSHP_OUTPUT_SIZE	0x128
#define TDSHP_BLANK_WIDTH	0x12c
#define TDSHP_DUMMY_REG		0x14c
#define LUMA_HIST_INIT_00	0x200
#define LUMA_HIST_INIT_01	0x204
#define LUMA_HIST_INIT_02	0x208
#define LUMA_HIST_INIT_03	0x20c
#define LUMA_HIST_INIT_04	0x210
#define LUMA_HIST_INIT_05	0x214
#define LUMA_HIST_INIT_06	0x218
#define LUMA_HIST_INIT_07	0x21c
#define LUMA_HIST_INIT_08	0x220
#define LUMA_HIST_INIT_09	0x224
#define LUMA_HIST_INIT_10	0x228
#define LUMA_HIST_INIT_11	0x22c
#define LUMA_HIST_INIT_12	0x230
#define LUMA_HIST_INIT_13	0x234
#define LUMA_HIST_INIT_14	0x238
#define LUMA_HIST_INIT_15	0x23c
#define LUMA_HIST_INIT_16	0x240
#define LUMA_SUM_INIT		0x244
#define DC_DBG_CFG_MAIN		0x250
#define DC_TWO_D_W1		0x25c
#define DC_TWO_D_W1_RESULT_INIT	0x260
#define DC_TWO_D_W1_RESULT	0x264
#define EDF_GAIN_00		0x300
#define EDF_GAIN_01		0x304
#define EDF_GAIN_02		0x308
#define EDF_GAIN_03		0x30c
#define EDF_GAIN_05		0x314
#define TDSHP_10		0x320
#define TDSHP_11		0x324
#define TDSHP_12		0x328
#define TDSHP_13		0x32c
#define PAT1_GEN_SET		0x330
#define PAT1_GEN_FRM_SIZE	0x334
#define PAT1_GEN_COLOR0		0x338
#define PAT1_GEN_COLOR1		0x33c
#define PAT1_GEN_COLOR2		0x340
#define PAT1_GEN_POS		0x344
#define PAT1_GEN_TILE_POS	0x354
#define PAT1_GEN_TILE_OV	0x358
#define PAT2_GEN_SET		0x360
#define PAT2_GEN_COLOR0		0x368
#define PAT2_GEN_COLOR1		0x36c
#define PAT2_GEN_POS		0x374
#define PAT2_GEN_CURSOR_RB0	0x378
#define PAT2_GEN_CURSOR_RB1	0x37c
#define PAT2_GEN_TILE_POS	0x384
#define PAT2_GEN_TILE_OV	0x388
#define BITPLUS_00		0x38c
#define BITPLUS_01		0x390
#define BITPLUS_02		0x394
#define CONTOUR_HIST_INIT_00	0x398
#define CONTOUR_HIST_INIT_01	0x39c
#define CONTOUR_HIST_INIT_02	0x3a0
#define CONTOUR_HIST_INIT_03	0x3a4
#define CONTOUR_HIST_INIT_04	0x3a8
#define CONTOUR_HIST_INIT_05	0x3ac
#define CONTOUR_HIST_INIT_06	0x3b0
#define CONTOUR_HIST_INIT_07	0x3b4
#define CONTOUR_HIST_INIT_08	0x3b8
#define CONTOUR_HIST_INIT_09	0x3bc
#define CONTOUR_HIST_INIT_10	0x3c0
#define CONTOUR_HIST_INIT_11	0x3c4
#define CONTOUR_HIST_INIT_12	0x3c8
#define CONTOUR_HIST_INIT_13	0x3cc
#define CONTOUR_HIST_INIT_14	0x3d0
#define CONTOUR_HIST_INIT_15	0x3d4
#define CONTOUR_HIST_INIT_16	0x3d8
#define CONTOUR_HIST_00		0x3dc
#define CONTOUR_HIST_01		0x3e0
#define CONTOUR_HIST_02		0x3e4
#define CONTOUR_HIST_03		0x3e8
#define CONTOUR_HIST_04		0x3ec
#define CONTOUR_HIST_05		0x3f0
#define CONTOUR_HIST_06		0x3f4
#define CONTOUR_HIST_07		0x3f8
#define CONTOUR_HIST_08		0x3fc
#define CONTOUR_HIST_09		0x400
#define CONTOUR_HIST_10		0x404
#define CONTOUR_HIST_11		0x408
#define CONTOUR_HIST_12		0x40c
#define CONTOUR_HIST_13		0x410
#define CONTOUR_HIST_14		0x414
#define CONTOUR_HIST_15		0x418
#define CONTOUR_HIST_16		0x41c
#define DC_SKIN_RANGE0		0x420
#define DC_SKIN_RANGE1		0x424
#define DC_SKIN_RANGE2		0x428
#define DC_SKIN_RANGE3		0x42c
#define DC_SKIN_RANGE4		0x430
#define DC_SKIN_RANGE5		0x434
#define POST_YLEV_00		0x480
#define POST_YLEV_01		0x484
#define POST_YLEV_02		0x488
#define POST_YLEV_03		0x48c
#define POST_YLEV_04		0x490
#define TDSHP_SHADOW_CTRL	0x67c

#define TDSHP_WAIT_TIMEOUT_MS (50)
#define DS_REG_NUM (36)

struct tdshp_data {
	u32 tile_width;
	/* u32 min_hfg_width; 9: HFG min + TDSHP crop */
};

static const struct tdshp_data mt6893_tdshp_data = {
	.tile_width = 528
};

static const struct tdshp_data mt6983_tdshp_data = {
	.tile_width = 1628,
};

static const struct tdshp_data mt6879_tdshp_data = {
	.tile_width = 1344,
};

static const struct tdshp_data mt6895_tdshp_data = {
	.tile_width = 1926,
};

struct mml_comp_tdshp {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct tdshp_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct tdshp_frame_data {
	u16 labels[DS_REG_NUM];
};

#define tdshp_frm_data(i)	((struct tdshp_frame_data *)(i->data))

static inline struct mml_comp_tdshp *comp_to_tdshp(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_tdshp, comp);
}

static s32 tdshp_prepare(struct mml_comp *comp, struct mml_task *task,
			 struct mml_comp_config *ccfg)
{
	struct tdshp_frame_data *tdshp_frm = NULL;

	tdshp_frm = kzalloc(sizeof(*tdshp_frm), GFP_KERNEL);

	ccfg->data = tdshp_frm;
	return 0;
}

static s32 tdshp_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	s32 ret = 0;

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc)
		return ret;

	mml_pq_trace_ex_begin("%s", __func__);
	ret = mml_pq_set_comp_config(task);
	mml_pq_trace_ex_end();
	return ret;
}

static s32 tdshp_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg,
			      struct tile_func_block *func,
			      union mml_tile_data *data)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);

	data->tdshp.max_width = tdshp->data->tile_width;
	/* TDSHP support crop capability, if no HFG. */
	func->type = TILE_TYPE_CROP_EN;
	func->init_func = tile_tdshp_init;
	func->for_func = tile_crop_for;
	func->data = data;

	func->enable_flag = dest->pq_config.en_sharp ||
			    cfg->info.mode == MML_MODE_DDP_ADDON;

	if (dest->rotate == MML_ROT_90 ||
	    dest->rotate == MML_ROT_270) {
		func->full_size_x_in = dest->data.height;
		func->full_size_y_in = dest->data.width;
		func->full_size_x_out = dest->data.height;
		func->full_size_y_out = dest->data.width;
	} else {
		func->full_size_x_in = dest->data.width;
		func->full_size_y_in = dest->data.height;
		func->full_size_x_out = dest->data.width;
		func->full_size_y_out = dest->data.height;
	}

	return 0;
}

static const struct mml_comp_tile_ops tdshp_tile_ops = {
	.prepare = tdshp_tile_prepare,
};

static u32 tdshp_get_label_count(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_sharp[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_sharp);

	if (!dest->pq_config.en_sharp)
		return 0;

	return DS_REG_NUM;
}

static void tdshp_init(struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CTRL, 0x1, 0x00000001);
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CFG, 0x2, 0x00000002);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_SHADOW_CTRL, 0x2, U32_MAX);
}

static void tdshp_relay(struct cmdq_pkt *pkt, const phys_addr_t base_pa,
			u32 relay)
{
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CFG, relay, 0x00000001);
}

static s32 tdshp_config_init(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	tdshp_init(task->pkts[ccfg->pipe], comp->base_pa);
	return 0;
}

static struct mml_pq_comp_config_result *get_tdshp_comp_config_result(
	struct mml_task *task)
{
	return task->pq_task->comp_config.result;
}

static s32 tdshp_config_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	struct mml_pq_comp_config_result *result;
	s32 ret;

	if (MML_FMT_10BIT(src->format) || MML_FMT_10BIT(dest->data.format))
		cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CTRL, 0, 0x00000004);
	else
		cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CTRL, 0x4, 0x00000004);

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc) {
		/* relay mode */
		if (cfg->info.mode == MML_MODE_DDP_ADDON) {
			/* enable to crop */
			tdshp_relay(pkt, base_pa, 0x0);
			cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_00, 0, 1 << 31);
		} else {
			tdshp_relay(pkt, base_pa, 0x1);
		}
		return 0;
	}
	tdshp_relay(pkt, base_pa, 0x0);

	ret = mml_pq_get_comp_config_result(task, TDSHP_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_tdshp_comp_config_result(task);
		if (result) {
			s32 i;
			struct mml_pq_reg *regs = result->ds_regs;

			/* TODO: use different regs */
			mml_pq_msg("%s:config ds regs, count: %d", __func__, result->ds_reg_cnt);
			for (i = 0; i < result->ds_reg_cnt; i++) {
				mml_write(pkt, base_pa + regs[i].offset, regs[i].value,
					regs[i].mask, reuse, cache,
					&tdshp_frm->labels[i]);
				mml_pq_msg("[ds][config][%x] = %#x mask(%#x)",
					regs[i].offset, regs[i].value, regs[i].mask);
			}
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_comp_config_clear(task);
		mml_pq_err("get ds param timeout: %d in %dms",
			ret, TDSHP_WAIT_TIMEOUT_MS);
	}

	return ret;
}

static s32 tdshp_config_tile(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 tdshp_input_w;
	u32 tdshp_input_h;
	u32 tdshp_output_w;
	u32 tdshp_output_h;
	u32 tdshp_crop_x_offset;
	u32 tdshp_crop_y_offset;
	u32 tdshp_hist_left;
	u32 tdshp_hist_top;

	mml_pq_msg("%s idx[%d] engine_id[%d]", __func__, idx, comp->id);

	tdshp_input_w = tile->in.xe - tile->in.xs + 1;
	tdshp_input_h = tile->in.ye - tile->in.ys + 1;
	tdshp_output_w = tile->out.xe - tile->out.xs + 1;
	tdshp_output_h = tile->out.ye - tile->out.ys + 1;
	tdshp_crop_x_offset = tile->out.xs - tile->in.xs;
	tdshp_crop_y_offset = tile->out.ys - tile->in.ys;
	/* TODO: need official implementation */
	tdshp_hist_left = 0xffff;
	tdshp_hist_top = 0xffff;

	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_INPUT_SIZE,
		(tdshp_input_w << 16) + tdshp_input_h, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_OUTPUT_OFFSET,
		(tdshp_crop_x_offset << 16) + tdshp_crop_y_offset, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_OUTPUT_SIZE,
		(tdshp_output_w << 16) + tdshp_output_h, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + HIST_CFG_00,
		((tile->out.xe - tile->in.xs) << 16) +
		(tdshp_hist_left - tile->in.xs), U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HIST_CFG_01,
		((tile->out.ye - tile->in.ys) << 16) +
		(tdshp_hist_top - tile->in.ys), U32_MAX);

	return 0;
}

static s32 tdshp_config_post(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];

	if (dest->pq_config.en_sharp || dest->pq_config.en_dc)
		mml_pq_put_comp_config_result(task);
	return 0;
}

static s32 tdshp_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;

	struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];

	struct mml_pq_comp_config_result *result = NULL;
	s32 ret = 0;

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc)
		return ret;

	ret = mml_pq_get_comp_config_result(task, TDSHP_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_tdshp_comp_config_result(task);
		if (result) {
			s32 i;
			struct mml_pq_reg *regs = result->ds_regs;
			//TODO: use different regs
			mml_pq_msg("%s:config ds regs, count: %d", __func__, result->ds_reg_cnt);
			for (i = 0; i < result->ds_reg_cnt; i++) {
				mml_update(reuse, tdshp_frm->labels[i], regs[i].value);
				mml_pq_msg("[ds][config][%x] = %#x mask(%#x)",
					regs[i].offset, regs[i].value, regs[i].mask);
			}
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_err("get ds param timeout: %d in %dms",
			ret, TDSHP_WAIT_TIMEOUT_MS);
	}

	return ret;
}


static const struct mml_comp_config_ops tdshp_cfg_ops = {
	.prepare = tdshp_prepare,
	.buf_prepare = tdshp_buf_prepare,
	.get_label_count = tdshp_get_label_count,
	.init = tdshp_config_init,
	.frame = tdshp_config_frame,
	.tile = tdshp_config_tile,
	.post = tdshp_config_post,
	.reframe = tdshp_reconfig_frame,
};

static void tdshp_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[11];
	u32 shadow_ctrl;

	mml_err("tdshp component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + TDSHP_SHADOW_CTRL);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + TDSHP_SHADOW_CTRL);

	value[0] = readl(base + TDSHP_CTRL);
	value[1] = readl(base + TDSHP_INTEN);
	value[2] = readl(base + TDSHP_INTSTA);
	value[3] = readl(base + TDSHP_STATUS);
	value[4] = readl(base + TDSHP_CFG);
	value[5] = readl(base + TDSHP_INPUT_COUNT);
	value[6] = readl(base + TDSHP_OUTPUT_COUNT);
	value[7] = readl(base + TDSHP_INPUT_SIZE);
	value[8] = readl(base + TDSHP_OUTPUT_OFFSET);
	value[9] = readl(base + TDSHP_OUTPUT_SIZE);
	value[10] = readl(base + TDSHP_BLANK_WIDTH);

	mml_err("TDSHP_CTRL %#010x TDSHP_INTEN %#010x TDSHP_INTSTA %#010x TDSHP_STATUS %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("TDSHP_CFG %#010x TDSHP_INPUT_COUNT %#010x TDSHP_OUTPUT_COUNT %#010x",
		value[4], value[5], value[6]);
	mml_err("TDSHP_INPUT_SIZE %#010x TDSHP_OUTPUT_OFFSET %#010x TDSHP_OUTPUT_SIZE %#010x",
		value[7], value[8], value[9]);
	mml_err("TDSHP_BLANK_WIDTH %#010x", value[10]);
}

static const struct mml_comp_debug_ops tdshp_debug_ops = {
	.dump = &tdshp_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_tdshp *tdshp = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &tdshp->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &tdshp->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			tdshp->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_tdshp *tdshp = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &tdshp->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &tdshp->ddp_comp);
		tdshp->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
};

static struct mml_comp_tdshp *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_tdshp *priv;
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
	priv->comp.tile_ops = &tdshp_tile_ops;
	priv->comp.config_ops = &tdshp_cfg_ops;
	priv->comp.debug_ops = &tdshp_debug_ops;

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

const struct of_device_id mml_tdshp_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_tdshp",
		.data = &mt6983_tdshp_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_tdshp",
		.data = &mt6893_tdshp_data
	},
	{
		.compatible = "mediatek,mt6879-mml_tdshp",
		.data = &mt6879_tdshp_data
	},
	{
		.compatible = "mediatek,mt6895-mml_tdshp",
		.data = &mt6895_tdshp_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_tdshp_driver_dt_match);

struct platform_driver mml_tdshp_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-tdshp",
		.owner = THIS_MODULE,
		.of_match_table = mml_tdshp_driver_dt_match,
	},
};

//module_platform_driver(mml_tdshp_driver);

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
module_param_cb(tdshp_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(tdshp_debug, "mml tdshp debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML TDSHP driver");
MODULE_LICENSE("GPL v2");
