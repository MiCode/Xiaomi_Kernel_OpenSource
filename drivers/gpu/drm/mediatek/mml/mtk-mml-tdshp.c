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


#define TDSHP_WAIT_TIMEOUT_MS (50)
#define DS_REG_NUM (36)
#define REG_NOT_SUPPORT 0xfff

enum mml_tdshp_reg_index {
	TDSHP_00,
	HIST_CFG_00,
	HIST_CFG_01,
	TDSHP_CTRL,
	TDSHP_INTEN,
	TDSHP_INTSTA,
	TDSHP_STATUS,
	TDSHP_CFG,
	TDSHP_INPUT_COUNT,
	TDSHP_OUTPUT_COUNT,
	TDSHP_INPUT_SIZE,
	TDSHP_OUTPUT_OFFSET,
	TDSHP_OUTPUT_SIZE,
	TDSHP_BLANK_WIDTH,
	/* REGION_PQ_SIZE_PARAMETER_MODE_SEGMENTATION_LENGTH */
	TDSHP_REGION_PQ_PARAM,
	TDSHP_SHADOW_CTRL,
	TDSHP_REG_MAX_COUNT
};

static const u16 tdshp_reg_table_mt6983[TDSHP_REG_MAX_COUNT] = {
	[TDSHP_00] = 0x000,
	[HIST_CFG_00] = 0x064,
	[HIST_CFG_01] = 0x068,
	[TDSHP_CTRL] = 0x100,
	[TDSHP_INTEN] = 0x104,
	[TDSHP_INTSTA] = 0x108,
	[TDSHP_STATUS] = 0x10c,
	[TDSHP_CFG] = 0x110,
	[TDSHP_INPUT_COUNT] = 0x114,
	[TDSHP_OUTPUT_COUNT] = 0x11c,
	[TDSHP_INPUT_SIZE] = 0x120,
	[TDSHP_OUTPUT_OFFSET] = 0x124,
	[TDSHP_OUTPUT_SIZE] = 0x128,
	[TDSHP_BLANK_WIDTH] = 0x12c,
	[TDSHP_REGION_PQ_PARAM] = REG_NOT_SUPPORT,
	[TDSHP_SHADOW_CTRL] = 0x67c
};

static const u16 tdshp_reg_table_mt6985[TDSHP_REG_MAX_COUNT] = {
	[TDSHP_00] = 0x000,
	[HIST_CFG_00] = 0x064,
	[HIST_CFG_01] = 0x068,
	[TDSHP_CTRL] = 0x100,
	[TDSHP_INTEN] = 0x104,
	[TDSHP_INTSTA] = 0x108,
	[TDSHP_STATUS] = 0x10c,
	[TDSHP_CFG] = 0x110,
	[TDSHP_INPUT_COUNT] = 0x114,
	[TDSHP_OUTPUT_COUNT] = 0x11c,
	[TDSHP_INPUT_SIZE] = 0x120,
	[TDSHP_OUTPUT_OFFSET] = 0x124,
	[TDSHP_OUTPUT_SIZE] = 0x128,
	[TDSHP_BLANK_WIDTH] = 0x12c,
	[TDSHP_REGION_PQ_PARAM] = 0x680,
	[TDSHP_SHADOW_CTRL] = 0x724
};

struct tdshp_data {
	u32 tile_width;
	/* u32 min_hfg_width; 9: HFG min + TDSHP crop */
	const u16 *reg_table;
};

static const struct tdshp_data mt6893_tdshp_data = {
	.tile_width = 528,
	.reg_table = tdshp_reg_table_mt6983,
};

static const struct tdshp_data mt6983_tdshp_data = {
	.tile_width = 1628,
	.reg_table = tdshp_reg_table_mt6983,
};

static const struct tdshp_data mt6879_tdshp_data = {
	.tile_width = 1344,
	.reg_table = tdshp_reg_table_mt6983,
};

static const struct tdshp_data mt6895_tdshp_data = {
	.tile_width = 1926,
	.reg_table = tdshp_reg_table_mt6983,
};

static const struct tdshp_data mt6985_tdshp_data = {
	.tile_width = 1666,
	.reg_table = tdshp_reg_table_mt6985,
};

static const struct tdshp_data mt6886_tdshp_data = {
	.tile_width = 1300,
	.reg_table = tdshp_reg_table_mt6983,
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
	bool config_success;
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

	data->tdshp.relay_mode = dest->pq_config.en_sharp ? false : true;
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

static void tdshp_init(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);

	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_CTRL], 0x1, 0x00000001);
	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_CFG], 0x2, 0x00000002);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL,
		base_pa + tdshp->data->reg_table[TDSHP_SHADOW_CTRL], 0x2, U32_MAX);
}

static void tdshp_relay(struct mml_comp *comp, struct cmdq_pkt *pkt, const phys_addr_t base_pa,
			u32 relay)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);

	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_CFG], relay, 0x00000001);
}

static void tdshp_config_region_pq(struct mml_comp *comp, struct cmdq_pkt *pkt,
			const phys_addr_t base_pa, const struct mml_pq_config *cfg)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);

	if (tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM] != REG_NOT_SUPPORT) {
		if (!cfg->en_region_pq) {
			mml_pq_msg("%s:disable region pq", __func__);

			cmdq_pkt_write(pkt, NULL,
				base_pa + tdshp->data->reg_table[TDSHP_REGION_PQ_PARAM],
				0, U32_MAX);
		}
	}
}

static s32 tdshp_config_init(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	tdshp_init(comp, task->pkts[ccfg->pipe], comp->base_pa);
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
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);

	const phys_addr_t base_pa = comp->base_pa;
	struct mml_pq_comp_config_result *result;
	s32 ret;
	s32 i;
	struct mml_pq_reg *regs = NULL;

	if (MML_FMT_10BIT(src->format) || MML_FMT_10BIT(dest->data.format)) {
		cmdq_pkt_write(pkt, NULL,
			base_pa + tdshp->data->reg_table[TDSHP_CTRL], 0, 0x00000004);
	} else {
		cmdq_pkt_write(pkt, NULL,
			base_pa + tdshp->data->reg_table[TDSHP_CTRL], 0x4, 0x00000004);
	}

	tdshp_config_region_pq(comp, pkt, base_pa, &dest->pq_config);

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc) {
		/* relay mode */
		if (cfg->info.mode == MML_MODE_DDP_ADDON) {
			/* enable to crop */
			tdshp_relay(comp, pkt, base_pa, 0x0);
			cmdq_pkt_write(pkt, NULL,
				base_pa + tdshp->data->reg_table[TDSHP_00], 0, 1 << 31);
		} else {
			tdshp_relay(comp, pkt, base_pa, 0x1);
		}
		return 0;
	}
	tdshp_relay(comp, pkt, base_pa, 0x0);

	do {
		ret = mml_pq_get_comp_config_result(task, TDSHP_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			tdshp_frm->config_success = false;
			mml_pq_err("get ds param timeout: %d in %dms",
				ret, TDSHP_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_tdshp_comp_config_result(task);
		if (!result) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->ds_regs;

	/* TODO: use different regs */
	mml_pq_msg("%s:config ds regs, count: %d", __func__, result->ds_reg_cnt);
	tdshp_frm->config_success = true;
	for (i = 0; i < result->ds_reg_cnt; i++) {
		if (cfg->info.mode == MML_MODE_DDP_ADDON) {
			/* no reuse support in addon mode*/
			cmdq_pkt_write(pkt, NULL, base_pa + regs[i].offset,
				regs[i].value, regs[i].mask);
		} else {
			mml_write(pkt, base_pa + regs[i].offset, regs[i].value,
				regs[i].mask, reuse, cache,
				&tdshp_frm->labels[i]);
		}
		mml_pq_msg("[ds][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}

exit:
	return ret;
}

static s32 tdshp_config_tile(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);

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

	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_INPUT_SIZE],
		(tdshp_input_w << 16) + tdshp_input_h, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_OUTPUT_OFFSET],
		(tdshp_crop_x_offset << 16) + tdshp_crop_y_offset, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[TDSHP_OUTPUT_SIZE],
		(tdshp_output_w << 16) + tdshp_output_h, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[HIST_CFG_00],
		((tile->out.xe - tile->in.xs) << 16) +
		(tdshp_hist_left - tile->in.xs), U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + tdshp->data->reg_table[HIST_CFG_01],
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
	s32 i;
	struct mml_pq_reg *regs = NULL;

	if (!dest->pq_config.en_sharp && !dest->pq_config.en_dc)
		return ret;

	do {
		ret = mml_pq_get_comp_config_result(task, TDSHP_WAIT_TIMEOUT_MS);
		if (ret) {
			mml_pq_comp_config_clear(task);
			mml_pq_err("get tdshp param timeout: %d in %dms",
				ret, TDSHP_WAIT_TIMEOUT_MS);
			ret = -ETIMEDOUT;
			goto exit;
		}

		result = get_tdshp_comp_config_result(task);
		if (!result || !tdshp_frm->config_success) {
			mml_pq_err("%s: not get result from user lib", __func__);
			ret = -EBUSY;
			goto exit;
		}
	} while ((mml_pq_debug_mode & MML_PQ_SET_TEST) && result->is_set_test);

	regs = result->ds_regs;
	//TODO: use different regs
	mml_pq_msg("%s:config ds regs, count: %d is_set_test[%d]", __func__, result->ds_reg_cnt,
		result->is_set_test);
	for (i = 0; i < result->ds_reg_cnt; i++) {
		mml_update(reuse, tdshp_frm->labels[i], regs[i].value);
		mml_pq_msg("[ds][config][%x] = %#x mask(%#x)",
			regs[i].offset, regs[i].value, regs[i].mask);
	}

exit:
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
	.repost = tdshp_config_post,
};

static void tdshp_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_tdshp *tdshp = comp_to_tdshp(comp);
	void __iomem *base = comp->base;
	u32 value[11];
	u32 shadow_ctrl;

	mml_err("tdshp component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + tdshp->data->reg_table[TDSHP_SHADOW_CTRL]);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + tdshp->data->reg_table[TDSHP_SHADOW_CTRL]);

	value[0] = readl(base + tdshp->data->reg_table[TDSHP_CTRL]);
	value[1] = readl(base + tdshp->data->reg_table[TDSHP_INTEN]);
	value[2] = readl(base + tdshp->data->reg_table[TDSHP_INTSTA]);
	value[3] = readl(base + tdshp->data->reg_table[TDSHP_STATUS]);
	value[4] = readl(base + tdshp->data->reg_table[TDSHP_CFG]);
	value[5] = readl(base + tdshp->data->reg_table[TDSHP_INPUT_COUNT]);
	value[6] = readl(base + tdshp->data->reg_table[TDSHP_OUTPUT_COUNT]);
	value[7] = readl(base + tdshp->data->reg_table[TDSHP_INPUT_SIZE]);
	value[8] = readl(base + tdshp->data->reg_table[TDSHP_OUTPUT_OFFSET]);
	value[9] = readl(base + tdshp->data->reg_table[TDSHP_OUTPUT_SIZE]);
	value[10] = readl(base + tdshp->data->reg_table[TDSHP_BLANK_WIDTH]);

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
	{
		.compatible = "mediatek,mt6985-mml_tdshp",
		.data = &mt6985_tdshp_data,
	},
	{
		.compatible = "mediatek,mt6886-mml_tdshp",
		.data = &mt6886_tdshp_data
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
