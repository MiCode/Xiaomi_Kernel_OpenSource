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

struct rsz_data {
};

struct rsz_data mt6893_rsz_data = {
};

struct mtk_mml_rsz {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp mml_comp;
	struct clk *clk;
	struct rsz_data *data;
	bool mml_binded;
	bool ddp_binded;
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_mml_rsz *rsz = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 ret, temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		ret = mml_register_comp(master, &rsz->mml_comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			rsz->mml_binded = true;
	} else {
		drm_dev = data;
		ret = mtk_ddp_comp_register(drm_dev, &rsz->ddp_comp);
		if (ret < 0)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			rsz->ddp_binded = true;
	}

	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mtk_mml_rsz *rsz = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		mml_unregister_comp(master, &rsz->mml_comp);
		rsz->mml_binded = false;
	} else {
		drm_dev = data;
		mtk_ddp_comp_unregister(drm_dev, &rsz->ddp_comp);
		rsz->ddp_binded = false;
	}
}


static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mtk_mml_rsz *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_mml_rsz *priv;
	s32 ret;
	s32 comp_id = -1;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (struct rsz_data*)of_device_get_match_data(dev);

	if (!of_property_read_u32(dev->of_node, "comp-id", &comp_id)) {
		priv->mml_comp.comp_id = comp_id;
	}
	dbg_probed_components[dbg_probed_count++] = priv;

	ret = component_add(dev, &mml_comp_ops);
	ret = component_add(dev, &mml_comp_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
	}

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mtk_mml_rsz_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6893-mml_rsz",
	  .data = &mt6893_rsz_data},
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
				dbg_probed_components[i]->mml_comp.comp_id);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      mml_binded: %d\n",
				dbg_probed_components[i]->mml_binded);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      ddp_comp_id: %d\n",
				dbg_probed_components[i]->ddp_comp.id);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      ddp_binded: %d\n",
				dbg_probed_components[i]->ddp_binded);
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