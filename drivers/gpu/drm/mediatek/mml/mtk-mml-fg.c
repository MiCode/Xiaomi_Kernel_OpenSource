/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk-mml-drm-adaptor.h"

#include "tile_driver.h"
#include "tile_mdp_reg.h"

#define FG_TRIGGER	0x000
#define FG_CTRL_0	0x020
#define FG_CK_EN	0x024
#define FG_TILE_INFO_0	0x418
#define FG_TILE_INFO_1	0x41c

struct fg_data {
};

static const struct fg_data mt6893_fg_data = {
};

struct mml_comp_fg {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct fg_data *data;
	bool ddp_bound;
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
			   void *ptr_func, void *tile_data)
{
	TILE_FUNC_BLOCK_STRUCT *func = (TILE_FUNC_BLOCK_STRUCT*)ptr_func;
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

	cmdq_pkt_write(pkt, NULL, base_pa + FG_TRIGGER, 1 << 1, 0x00000002);
	cmdq_pkt_write(pkt, NULL, base_pa + FG_TRIGGER, 0, 0x00000002);
	return 0;
}

static s32 fg_config_frame(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	/* relay mode */
	cmdq_pkt_write(pkt, NULL, base_pa + FG_CTRL_0, 1, 0x00000001);
	cmdq_pkt_write(pkt, NULL, base_pa + FG_CK_EN, 0x7, 0x00000007);

	return 0;
}

static s32 fg_config_tile(struct mml_comp *comp, struct mml_task *task,
			  struct mml_comp_config *ccfg, u8 idx)
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
	u32 value[4];

	mml_err("fg component %u dump:", comp->id);

	value[0] = readl(base + FG_CTRL_0);
	value[1] = readl(base + FG_CK_EN);
	value[2] = readl(base + FG_TILE_INFO_0);
	value[3] = readl(base + FG_TILE_INFO_1);

	mml_err("FG_CTRL_0 %#010x FG_CK_EN %#010x",
		value[0], value[1]);
	mml_err("FG_TILE_INFO_0 %#010x FG_TILE_INFO_1 %#010x",
		value[2], value[3]);
}

static const struct mml_comp_debug_ops fg_debug_ops = {
	.dump = &fg_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_fg *fg = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
    s32 ret = -1, temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		ret = mml_register_comp(master, &fg->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		drm_dev = data;
		ret = mml_ddp_comp_register(drm_dev, &fg->ddp_comp);
		if (ret < 0)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			fg->ddp_bound = true;
	}

	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_fg *fg = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		mml_unregister_comp(master, &fg->comp);
	} else {
		drm_dev = data;
		mml_ddp_comp_unregister(drm_dev, &fg->ddp_comp);
		fg->ddp_bound = false;
	}
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
	priv->data = (const struct fg_data *)of_device_get_match_data(dev);

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

const struct of_device_id mtk_mml_fg_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_fg",
		.data = &mt6893_fg_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mml_fg_driver_dt_match);

struct platform_driver mtk_mml_fg_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-fg",
		.owner = THIS_MODULE,
		.of_match_table = mtk_mml_fg_driver_dt_match,
	},
};

//module_platform_driver(mtk_mml_fg_driver);

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
module_param_cb(fg_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(fg_ut_case, "mml fg UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML FG driver");
MODULE_LICENSE("GPL v2");
