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

struct wrot_data {
};

struct wrot_data mt6893_wrot_data = {
};

struct mtk_mml_wrot {
	struct mml_comp mml_comp;
	struct wrot_data *data;
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_mml_wrot *wrot = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &wrot->mml_comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mtk_mml_wrot *wrot = dev_get_drvdata(dev);

	mml_unregister_comp(master, &wrot->mml_comp);
}


static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mtk_mml_wrot *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_mml_wrot *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (struct wrot_data*)of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->mml_comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}
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

const struct of_device_id mtk_mml_wrot_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6893-mml_wrot",
	  .data = &mt6893_wrot_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_mml_wrot_driver_dt_match);

struct platform_driver mtk_mml_wrot_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
			.name = "mediatek-mml-wrot",
			.owner = THIS_MODULE,
			.of_match_table = mtk_mml_wrot_driver_dt_match,
		},
};

//module_platform_driver(mtk_mml_wrot_driver);

static s32 ut_case;
static s32 ut_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = sscanf(val, "%d", &ut_case);
	if (result != 1) {
		mml_err("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}
	mml_log("%s: case_id=%d\n", __func__, ut_case);

	switch (ut_case) {
	case 0:
		mml_log("use read to dump current pwm setting\n");
		break;
	default:
		mml_err("invalid case_id: %d\n", ut_case);
		break;
	}

	mml_log("%s END\n", __func__);
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
				dbg_probed_components[i]->mml_comp.id);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -      mml_binded: %d\n",
				dbg_probed_components[i]->mml_comp.bound);
		}
	default:
		mml_err("not support read for case_id: %d\n", ut_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static struct kernel_param_ops up_param_ops = {
	.set = ut_set,
	.get = ut_get,
};
module_param_cb(wrot_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(wrot_ut_case, "mml wrot UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML WROT driver");
MODULE_LICENSE("GPL v2");
