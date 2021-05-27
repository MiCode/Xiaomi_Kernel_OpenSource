// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */


#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/component.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-core.h"

#define MML_MAX_ENGINES		50

struct mml_dev {
	struct platform_device *pdev;
	struct mml_comp *comps[MML_MAX_ENGINES];
	struct cmdq_base *cmdq_base;
	struct cmdq_client *cmdq_clt;

	atomic_t drm_cnt;
	struct mml_drm_ctx *drm_ctx;
	struct mutex drm_ctx_mutex;
};

struct cmdq_base *cmdq_register_device(struct device *dev);
struct cmdq_client *cmdq_mbox_create(struct device *dev, int index);

extern struct platform_driver mtk_mml_rdma_driver;
extern struct platform_driver mtk_mml_wrot_driver;
extern struct platform_driver mtk_mml_rsz_driver;

struct platform_device *mml_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *mml_node;
	struct platform_device *mml_pdev;

	mml_node = of_parse_phandle(dev->of_node, "mediatek,mml", 0);
	if (!mml_node) {
		dev_err(dev, "cannot get mml node\n");
		return NULL;
	}

	mml_pdev = of_find_device_by_node(mml_node);
	of_node_put(mml_node);
	if (WARN_ON(!mml_pdev)) {
		dev_err(dev, "mml pdev failed\n");
		return NULL;
	}

	return mml_pdev;
}
EXPORT_SYMBOL_GPL(mml_get_plat_device);

struct mml_drm_ctx *mml_dev_get_drm_ctx(struct mml_dev *mml,
	struct mml_drm_ctx *(*ctx_create)(struct mml_dev *mml))
{
	struct mml_drm_ctx *ctx;

	mutex_lock(&mml->drm_ctx_mutex);
	if (atomic_inc_return(&mml->drm_cnt) == 1)
		mml->drm_ctx = ctx_create(mml);
	ctx = mml->drm_ctx;
	mutex_unlock(&mml->drm_ctx_mutex);
	return ctx;
}

void mml_dev_put_drm_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_drm_ctx *ctx))
{
	struct mml_drm_ctx *ctx;
	int cnt;

	mutex_lock(&mml->drm_ctx_mutex);
	ctx = mml->drm_ctx;
	cnt = atomic_dec_if_positive(&mml->drm_cnt);
	if (cnt == 0)
		mml->drm_ctx = NULL;
	mutex_unlock(&mml->drm_ctx_mutex);
	if (cnt == 0)
		ctx_release(ctx);

	WARN_ON(cnt < 0);
}


static int master_bind(struct device *dev)
{
	return component_bind_all(dev, NULL);
}

static void master_unbind(struct device *dev)
{
	component_unbind_all(dev, NULL);
}

static const struct component_master_ops mml_master_ops = {
	.bind = master_bind,
	.unbind = master_unbind,
};

static int component_compare(struct device *dev, void *data)
{
	u32 comp_id;
	u32 match_id = (u32)(uintptr_t)data;

	dev_dbg(dev, "%s -- match_id:%d\n", __func__, match_id);
	if (!of_property_read_u32(dev->of_node, "comp-id", &comp_id)) {
		dev_dbg(dev, "%s -- comp_id:%d\n", __func__, comp_id);
		return match_id == comp_id;
	}
	return 0;
}

static int comp_sys_init(struct device *dev, struct mml_dev *mml)
{
	struct component_match *match = NULL;
	u32 comp_count;
	ulong i;
	int ret;

	if (of_property_read_u32(dev->of_node, "comp-count", &comp_count)) {
		dev_err(dev, "No comp-count in dts node\n");
		return -EINVAL;
	}
	dev_notice(dev, "%s -- comp-count:%d\n", __func__, comp_count);
	for (i = 0; i < comp_count; i++)
		component_match_add(dev, &match, component_compare, (void *)i);

	ret = component_master_add_with_match(dev, &mml_master_ops, match);
	if (ret)
		dev_err(dev, "Failed to add match: %d\n", ret);

	return ret;
}

static void comp_sys_deinit(struct device *dev)
{
	component_master_del(dev, &mml_master_ops);
}

static s32 __comp_init(struct platform_device *pdev, struct mml_comp *comp)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	int i;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Failed to get resource\n");
		return -EINVAL;
	}
	comp->base = devm_ioremap_resource(dev, res);
	comp->base_pa = res->start;
	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		comp->clks[i] = of_clk_get(node, i);
		if (IS_ERR(comp->clks[i]))
			break;
	}
	return 0;
}

s32 mml_comp_init(struct platform_device *comp_pdev, struct mml_comp *comp)
{
	struct device *dev = &comp_pdev->dev;
	struct device_node *node = dev->of_node;
	u32 comp_id;
	int ret;

	ret = of_property_read_u32(node, "comp-id", &comp_id);
	if (ret) {
		dev_err(dev, "No comp-id in component %s: %d\n",
			node->full_name, ret);
		return -EINVAL;
	}
	comp->id = comp_id;
	ret = __comp_init(comp_pdev, comp);
	if (ret)
		return ret;
	/* TODO: get larb device for smi prepare */
	return 0;
}
EXPORT_SYMBOL_GPL(mml_comp_init);

s32 mml_subcomp_init(struct platform_device *main_pdev,
	int subcomponent, struct mml_comp *comp)
{
	struct device *dev = &main_pdev->dev;
	struct device_node *node = dev->of_node;
	u32 comp_id;
	int ret;

	ret = of_property_read_u32_index(node, "mml-comp-ids",
		subcomponent, &comp_id);
	if (ret) {
		dev_err(dev, "No mml-comp-ids in component %s: %d\n",
			node->full_name, ret);
		return -EINVAL;
	}
	comp->id = comp_id;
	ret = __comp_init(main_pdev, comp);
	return ret;
}
EXPORT_SYMBOL_GPL(mml_subcomp_init);

s32 mml_register_comp(struct device *master, struct mml_comp *comp)
{
	struct mml_dev *mml = dev_get_drvdata(master);

	if (mml->comps[comp->id]) {
		dev_err(master, "Duplicated component id: %d\n", comp->id);
		return -EINVAL;
	}
	mml->comps[comp->id] = comp;
	comp->bound = true;
	return 0;
}
EXPORT_SYMBOL_GPL(mml_register_comp);

void mml_unregister_comp(struct device *master, struct mml_comp *comp)
{
	struct mml_dev *mml = dev_get_drvdata(master);

	if (mml->comps[comp->id] == comp) {
		mml->comps[comp->id] = NULL;
		comp->bound = false;
	}
}
EXPORT_SYMBOL_GPL(mml_unregister_comp);

static bool dbg_probed;
static int mml_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_dev *mml;
	int ret;

	mml = devm_kzalloc(dev, sizeof(*mml), GFP_KERNEL);
	if (!mml)
		return -ENOMEM;

	mml->pdev = pdev;
	ret = comp_sys_init(dev, mml);
	if (ret) {
		dev_err(dev, "Failed to initialize mml comp system\n");
		goto err_init_comp;
	}
	//ret = mml_sys_comp_add(dev);
	if (ret) {
		dev_err(dev, "Failed to add mmlsys component: %d\n", ret);
		goto err_sys_add;
	}

	mml->cmdq_base = cmdq_register_device(dev);
	mml->cmdq_clt = cmdq_mbox_create(dev, 0);
	if (IS_ERR(mml->cmdq_clt)) {
		dev_err(dev, "Unable to create cmdq mbox on %p:%d\n", dev, 0);
		ret = PTR_ERR(mml->cmdq_clt);
		goto err_mbox_create;
	}

	platform_set_drvdata(pdev, mml);
	dbg_probed = true;
	return 0;

err_mbox_create:
	//mml_sys_comp_del(dev);
err_sys_add:
	comp_sys_deinit(dev);
err_init_comp:
	devm_kfree(dev, mml);
	return ret;
}

static int mml_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_dev *mml = platform_get_drvdata(pdev);

	comp_sys_deinit(dev);
	devm_kfree(dev, mml);
	return 0;
}

static int __maybe_unused mml_pm_suspend(struct device *dev)
{
	dev_notice(dev, "%s ignore\n", __func__);
	return 0;
}

static int __maybe_unused mml_pm_resume(struct device *dev)
{
	dev_notice(dev, "%s ignore\n", __func__);
	return 0;
}

static int __maybe_unused mml_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;
	return mml_pm_suspend(dev);
}

static int __maybe_unused mml_resume(struct device *dev)
{
	if (pm_runtime_active(dev))
		return 0;
	return mml_pm_resume(dev);
}

static const struct dev_pm_ops mml_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mml_suspend, mml_resume)
	SET_RUNTIME_PM_OPS(mml_pm_suspend, mml_pm_resume, NULL)
};

static const struct of_device_id mml_of_ids[] = {
	{
		.compatible = "mediatek,mt6893-mml",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_of_ids);

static struct platform_driver mml_driver = {
	.probe = mml_probe,
	.remove = mml_remove,
	.driver = {
		.name = "mtk-mml",
		.owner = THIS_MODULE,
		.pm = &mml_pm_ops,
		.of_match_table = mml_of_ids,
	},
};

static int __init mml_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&mml_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mml_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_rsz_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_rsz_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_rdma_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_rdma_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_wrot_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_wrot_driver.driver.name);
		return ret;
	}

	/* register pm notifier */

	return 0;
}
module_init(mml_driver_init);

static void __exit mml_driver_exit(void)
{
	platform_driver_unregister(&mml_driver);
}
module_exit(mml_driver_exit);

static s32 ut_case;
static int ut_set(const char *val, const struct kernel_param *kp)
{
	int result;

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

static int ut_get(char *buf, const struct kernel_param *kp)
{
	int length = 0;

	switch (ut_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed: %d\n", ut_case, dbg_probed);
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
module_param_cb(ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(ut_case, "mml platform driver UT test case");

MODULE_DESCRIPTION("MediaTek multimedia-layer driver");
MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
