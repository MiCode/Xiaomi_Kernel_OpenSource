/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"

#include "tile_driver.h"

#define FG_TRIGGER	0x000
#define FG_STATUS	0x004
#define FG_CTRL_0	0x020
#define FG_CK_EN	0x024
#define FG_BACK_DOOR_0	0x02c
#define FG_PIC_INFO_0	0x400
#define FG_PIC_INFO_1	0x404
#define FG_TILE_INFO_0	0x418
#define FG_TILE_INFO_1	0x41c
#define FG_DEBUG_0	0x500
#define FG_DEBUG_1	0x504
#define FG_DEBUG_2	0x508
#define FG_DEBUG_3	0x50c
#define FG_DEBUG_4	0x510
#define FG_DEBUG_5	0x514
#define FG_DEBUG_6	0x518

struct fg_data {
};

static const struct fg_data mt6893_fg_data = {
};

struct mml_comp_fg {
	struct mml_comp comp;
	const struct fg_data *data;
};

/* meta data for each different frame config */
struct fg_frame_data {
	u8 out_idx;
};

#define fg_frm_data(i)	((struct fg_frame_data *)(i->data))

static s32 fg_prepare(struct mml_comp *comp, struct mml_task *task,
		      struct mml_comp_config *ccfg)
{
	struct fg_frame_data *fg_frm;

	fg_frm = kzalloc(sizeof(*fg_frm), GFP_KERNEL);
	ccfg->data = fg_frm;
	/* cache out index for easy use */
	fg_frm->out_idx = ccfg->node->out_idx;

	return 0;
}

static s32 fg_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg,
			   struct tile_func_block *func,
			   union mml_tile_data *data)
{
	struct fg_frame_data *fg_frm = fg_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_frame_dest *dest = &cfg->info.dest[fg_frm->out_idx];

	func->enable_flag = true;

	if (cfg->info.dest_cnt == 1 &&
	    (dest->crop.r.width != src->width ||
	    dest->crop.r.height != src->height)) {
		u32 in_crop_w, in_crop_h;

		in_crop_w = dest->crop.r.width;
		in_crop_h = dest->crop.r.height;
		if (in_crop_w + dest->crop.r.left > src->width)
			in_crop_w = src->width - dest->crop.r.left;
		if (in_crop_h + dest->crop.r.top > src->height)
			in_crop_h = src->height - dest->crop.r.top;
		func->full_size_x_in = in_crop_w + dest->crop.r.left;
		func->full_size_y_in = in_crop_h + dest->crop.r.top;
	} else {
 		func->full_size_x_in = src->width;
		func->full_size_y_in = src->height;
	}
	func->full_size_x_out = func->full_size_x_in;
	func->full_size_y_out = func->full_size_y_in;

	return 0;
}

static const struct mml_comp_tile_ops fg_tile_ops = {
	.prepare = fg_tile_prepare,
};

static s32 fg_init(struct mml_comp *comp, struct mml_task *task,
		   struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	cmdq_pkt_write(pkt, NULL, base_pa + FG_TRIGGER, 0, U32_MAX);
	return 0;
}

static s32 fg_config_frame(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	/* relay mode */
	cmdq_pkt_write(pkt, NULL, base_pa + FG_CTRL_0, 1, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + FG_CK_EN, 0x7, U32_MAX);

	return 0;
}

static s32 fg_config_tile(struct mml_comp *comp, struct mml_task *task,
			  struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 width = tile->in.xe - tile->in.xs + 1;
	u32 height = tile->in.ye - tile->in.ys + 1;

	cmdq_pkt_write(pkt, NULL, base_pa + FG_TILE_INFO_0,
		(tile->in.xs & 0xffff) + ((width & 0xffff) << 16), U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + FG_TILE_INFO_1,
		(tile->in.ys & 0xffff) + ((height & 0xffff) << 16), U32_MAX);
	return 0;
}

static const struct mml_comp_config_ops fg_cfg_ops = {
	.prepare = fg_prepare,
	.init = fg_init,
	.frame = fg_config_frame,
	.tile = fg_config_tile,
};

static void fg_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[16];

	mml_err("fg component %u dump:", comp->id);

	value[0] = readl(base + FG_TRIGGER);
	value[1] = readl(base + FG_STATUS);
	value[2] = readl(base + FG_CTRL_0);
	value[3] = readl(base + FG_CK_EN);
	value[4] = readl(base + FG_BACK_DOOR_0);
	value[5] = readl(base + FG_PIC_INFO_0);
	value[6] = readl(base + FG_PIC_INFO_1);
	value[7] = readl(base + FG_TILE_INFO_0);
	value[8] = readl(base + FG_TILE_INFO_1);
	value[9] = readl(base + FG_DEBUG_0);
	value[10] = readl(base + FG_DEBUG_1);
	value[11] = readl(base + FG_DEBUG_2);
	value[12] = readl(base + FG_DEBUG_3);
	value[13] = readl(base + FG_DEBUG_4);
	value[14] = readl(base + FG_DEBUG_5);
	value[15] = readl(base + FG_DEBUG_6);

	mml_err("FG_TRIGGER %#010x FG_STATUS %#010x",
		value[0], value[1]);
	mml_err("FG_CTRL_0 %#010x FG_CK_EN %#010x",
		value[2], value[3]);
	mml_err("FG_BACK_DOOR_0 %#010x FG_PIC_INFO_0 %#010x FG_PIC_INFO_1 %#010x",
		value[4], value[5], value[6]);
	mml_err("FG_TILE_INFO_0 %#010x FG_TILE_INFO_1 %#010x",
		value[7], value[8]);
	mml_err("FG_DEBUG_0 %#010x FG_DEBUG_1 %#010x FG_DEBUG_2 %#010x",
		value[9], value[10], value[11]);
	mml_err("FG_DEBUG_3 %#010x FG_DEBUG_4 %#010x FG_DEBUG_5 %#010x",
		value[12], value[13], value[14]);
	mml_err("FG_DEBUG_6 %#010x ", value[15]);
}

static const struct mml_comp_debug_ops fg_debug_ops = {
	.dump = &fg_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_fg *fg = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &fg->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_fg *fg = dev_get_drvdata(dev);

	mml_unregister_comp(master, &fg->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_fg *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_fg *priv;
	s32 ret;

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
	priv->comp.tile_ops = &fg_tile_ops;
	priv->comp.config_ops = &fg_cfg_ops;
	priv->comp.debug_ops = &fg_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_fg_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_fg",
		.data = &mt6893_fg_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_fg_driver_dt_match);

struct platform_driver mml_fg_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-fg",
		.owner = THIS_MODULE,
		.of_match_table = mml_fg_driver_dt_match,
	},
};

//module_platform_driver(mml_fg_driver);

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
module_param_cb(fg_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(fg_ut_case, "mml fg UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML FG driver");
MODULE_LICENSE("GPL v2");
