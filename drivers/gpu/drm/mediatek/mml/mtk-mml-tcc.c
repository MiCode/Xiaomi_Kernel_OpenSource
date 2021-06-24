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
#include "mtk-mml-drm-adaptor.h"
#include "mtk_drm_ddp_comp.h"

#include "tile_driver.h"
#include "tile_mdp_reg.h"

#define TCC_CTRL	0x000

struct tcc_data {
};

static const struct tcc_data mt6893_tcc_data = {
};

struct mml_comp_tcc {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct tcc_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct tcc_frame_data {
	u8 out_idx;
};

#define tcc_frm_data(i)	((struct tcc_frame_data *)(i->data))

static s32 tcc_prepare(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg)
{
	struct tcc_frame_data *tcc_frm;

	tcc_frm = kzalloc(sizeof(*tcc_frm), GFP_KERNEL);
	ccfg->data = tcc_frm;
	/* cache out index for easy use */
	tcc_frm->out_idx = ccfg->node->out_idx;

	return 0;
}

static s32 tcc_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    void *ptr_func, void *tile_data)
{
	TILE_FUNC_BLOCK_STRUCT *func = (TILE_FUNC_BLOCK_STRUCT*)ptr_func;
	struct tcc_frame_data *tcc_frm = tcc_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[tcc_frm->out_idx];

	func->enable_flag = true;

	if (dest->rotate == MML_ROT_90 ||
	    dest->rotate == MML_ROT_270) {
		func->full_size_x_in = dest->data.height;
		func->full_size_y_in = dest->data.width;
		func->full_size_x_out = dest->data.height;
		func->full_size_y_out = dest->data.height;
	} else {
		func->full_size_x_in = dest->data.height;
		func->full_size_y_in = dest->data.height;
		func->full_size_x_out = dest->data.height;
		func->full_size_y_out = dest->data.height;
	}

	return 0;
}

static const struct mml_comp_tile_ops tcc_tile_ops = {
	.prepare = tcc_tile_prepare,
};

static s32 tcc_init(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	cmdq_pkt_write(pkt, NULL, base_pa + TCC_CTRL, 1, 0x00000001);
	return 0;
}

static s32 tcc_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	/* relay mode */
	cmdq_pkt_write(pkt, NULL, base_pa + TCC_CTRL, 1 << 29, 0x20000000);

	return 0;
}

static s32 tcc_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 width = tile->in.xe - tile->in.xs + 1;
	u32 height = tile->in.ye - tile->in.ys + 1;

	cmdq_pkt_write(pkt, NULL, base_pa + TCC_CTRL,
		((height & 0x3fff) << 15) + ((width & 0x3fff) << 1),
		0x1ffffffe);

	return 0;
}

static const struct mml_comp_config_ops tcc_cfg_ops = {
	.prepare = tcc_prepare,
	.init = tcc_init,
	.frame = tcc_config_frame,
	.tile = tcc_config_tile,
};

static void tcc_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[1];

	mml_err("tcc component %u dump:", comp->id);

	value[0] = readl(base + TCC_CTRL);

	mml_err("TCC_CTRL %#010x", value[0]);
}

static const struct mml_comp_debug_ops tcc_debug_ops = {
	.dump = &tcc_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_tcc *tcc = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 ret = -1, temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		ret = mml_register_comp(master, &tcc->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		drm_dev = data;
		ret = mml_ddp_comp_register(drm_dev, &tcc->ddp_comp);
		if (ret < 0)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			tcc->ddp_bound = true;
	}

	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_tcc *tcc = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		mml_unregister_comp(master, &tcc->comp);
	} else {
		drm_dev = data;
		mml_ddp_comp_unregister(drm_dev, &tcc->ddp_comp);
		tcc->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_tcc *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_tcc *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (const struct tcc_data *)of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.tile_ops = &tcc_tile_ops;
	priv->comp.config_ops = &tcc_cfg_ops;
	priv->comp.debug_ops = &tcc_debug_ops;

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

const struct of_device_id mtk_mml_tcc_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_tcc",
		.data = &mt6893_tcc_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mml_tcc_driver_dt_match);

struct platform_driver mtk_mml_tcc_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-tcc",
		.owner = THIS_MODULE,
		.of_match_table = mtk_mml_tcc_driver_dt_match,
	},
};

//module_platform_driver(mtk_mml_tcc_driver);

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
module_param_cb(tcc_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(tcc_ut_case, "mml tcc UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML TCC driver");
MODULE_LICENSE("GPL v2");
