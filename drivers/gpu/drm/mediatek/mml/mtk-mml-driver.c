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

#include <soc/mediatek/smi.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-core.h"
#include "mtk-mml-pq-core.h"
#include "mtk-mml-sys.h"

#define MML_MAX_CMDQ_CLTS	4

struct mml_dev {
	struct platform_device *pdev;
	struct mml_comp *comps[MML_MAX_COMPONENTS];
	struct mtk_mml_sys *sys;
	struct cmdq_base *cmdq_base;
	struct cmdq_client *cmdq_clts[MML_MAX_CMDQ_CLTS];
	u8 cmdq_clt_cnt;

	atomic_t drm_cnt;
	struct mml_drm_ctx *drm_ctx;
	struct mutex drm_ctx_mutex;
	struct mml_topology_cache *topology;
};

extern struct platform_driver mtk_mml_rdma_driver;
extern struct platform_driver mtk_mml_wrot_driver;
extern struct platform_driver mtk_mml_rsz_driver;
extern struct platform_driver mml_mutex_driver;
extern struct platform_driver mml_sys_driver;
extern struct platform_driver mtk_mml_hdr_driver;
extern struct platform_driver mtk_mml_color_driver;
extern struct platform_driver mtk_mml_aal_driver;
extern struct platform_driver mtk_mml_tdshp_driver;
extern struct platform_driver mtk_mml_tcc_driver;
extern struct platform_driver mtk_mml_fg_driver;
extern struct platform_driver mtk_mml_test_drv;

extern struct cmdq_base *cmdq_register_device(struct device *dev);
extern struct cmdq_client *cmdq_mbox_create(struct device *dev, int index);
extern void cmdq_mbox_destroy(struct cmdq_client *client);
extern int mml_topology_ip_init(void);
extern void mml_topology_ip_exit(void);

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

	/* make sure topology ready before client can use mml */
	if (!mml->topology)
		mml->topology = mml_topology_create(mml, mml->pdev,
			mml->cmdq_clts, mml->cmdq_clt_cnt);
	if (IS_ERR(mml->topology)) {
		mml_err("topology create fail %d", PTR_ERR(mml->topology));
		return (void *)mml->topology;
	}

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

struct mml_topology_cache *mml_topology_get_cache(struct mml_dev *mml)
{
	return IS_ERR(mml->topology) ? NULL : mml->topology;
}

struct mml_comp *mml_dev_get_comp_by_id(struct mml_dev *mml, u32 id)
{
	return mml->comps[id];
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

static inline int of_mml_read_comp_name_index(const struct device_node *np,
	int index, const char **name)
{
	return of_property_read_string_index(np, "comp-names", index, name);
}

static int comp_compare(struct device *dev, int subcomponent, void *data)
{
	u32 comp_id;
	u32 match_id = (u32)(uintptr_t)data;

	dev_dbg(dev, "%s %d -- match_id:%d\n", __func__,
		subcomponent, match_id);
	if (!of_mml_read_comp_id_index(dev->of_node, subcomponent, &comp_id)) {
		dev_dbg(dev, "%s -- comp_id:%d\n", __func__, comp_id);
		return match_id == comp_id;
	}
	return 0;
}

static inline int of_mml_read_comp_count(const struct device_node *np,
	u32 *count)
{
	return of_property_read_u32(np, "comp-count", count);
}

static int comp_master_init(struct device *dev, struct mml_dev *mml)
{
	struct component_match *match = NULL;
	u32 comp_count;
	ulong i;
	int ret;

	if (of_mml_read_comp_count(dev->of_node, &comp_count)) {
		dev_err(dev, "no comp-count in dts node\n");
		return -EINVAL;
	}
	dev_notice(dev, "%s -- comp-count:%d\n", __func__, comp_count);
	/* engine id 0 leaves empty, so begin with 1 */
	for (i = 1; i < comp_count; i++)
		component_match_add_typed(dev, &match, comp_compare, (void *)i);

	ret = component_master_add_with_match(dev, &mml_master_ops, match);
	if (ret)
		dev_err(dev, "failed to add match: %d\n", ret);

	return ret;
}

static void comp_master_deinit(struct device *dev)
{
	component_master_del(dev, &mml_master_ops);
}

static const char *comp_clock_names = "comp-clock-names";

static s32 __comp_init(struct platform_device *pdev, struct mml_comp *comp,
	const char *clkpropname)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	struct property *prop;
	const char *clkname;
	int i, ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get resource\n");
		return -EINVAL;
	}
	comp->base = devm_ioremap_resource(dev, res);
	comp->base_pa = res->start;

	/* ignore clks if clkpropname is null as subcomponent */
	if (!clkpropname)
		return 0;

	ret = 0;
	if (of_property_count_strings(node, clkpropname) > 0) {
		/* get named clks as component or subcomponent */
		i = 0;
		of_property_for_each_string(node, clkpropname, prop, clkname) {
			if (i >= ARRAY_SIZE(comp->clks)) {
				dev_err(dev, "out of clk array size %d in %s\n",
					i, node->full_name);
				ret = -E2BIG;
				break;
			}
			comp->clks[i] = of_clk_get_by_name(node, clkname);
			if (IS_ERR(comp->clks[i])) {
				dev_err(dev, "failed to get clk %s in %s\n",
					clkname, node->full_name);
				ret = PTR_ERR(comp->clks[i]);
			} else {
				i++;
			}
		}
		if (i < ARRAY_SIZE(comp->clks))
			comp->clks[i] = ERR_PTR(-ENOENT);
	} else if (clkpropname == comp_clock_names) {
		/* get all clks as component */
		for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
			comp->clks[i] = of_clk_get(node, i);
			if (IS_ERR(comp->clks[i]))
				break;
		}
		if (!i)
			dev_info(dev, "no clks in node %s\n", node->full_name);
	} else {
		dev_info(dev, "no %s property in node %s\n",
			 clkpropname, node->full_name);
	}
	return ret;
}

s32 mml_comp_init(struct platform_device *comp_pdev, struct mml_comp *comp)
{
	struct device *dev = &comp_pdev->dev;
	u32 comp_id;
	int ret;

	ret = of_mml_read_comp_id_index(dev->of_node, 0, &comp_id);
	if (ret) {
		dev_err(dev, "no comp-ids in component %s: %d\n",
			dev->of_node->full_name, ret);
		return -EINVAL;
	}
	comp->id = comp_id;
	ret = __comp_init(comp_pdev, comp, comp_clock_names);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(mml_comp_init);

s32 mml_subcomp_init(struct platform_device *comp_pdev,
	int subcomponent, struct mml_comp *comp)
{
	struct device *dev = &comp_pdev->dev;
	struct device_node *node = dev->of_node;
	u32 comp_id;
	const char *name_ptr = NULL;
	char name[32] = "";
	int ret;

	ret = of_mml_read_comp_id_index(node, subcomponent, &comp_id);
	if (ret) {
		dev_err(dev, "no comp-ids in subcomponent %d %s: %d\n",
			subcomponent, node->full_name, ret);
		return -EINVAL;
	}
	comp->id = comp_id;
	comp->sub_idx = subcomponent;
	if (!of_mml_read_comp_name_index(node, subcomponent, &name_ptr)) {
		comp->name = name_ptr;
		ret = snprintf(name, sizeof(name), "%s-clock-names", name_ptr);
		if (ret >= sizeof(name)) {
			dev_err(dev, "len:%d over name size:%d",
				ret, sizeof(name));
			name[sizeof(name) - 1] = '\0';
		}
		name_ptr = name;
	}
	ret = __comp_init(comp_pdev, comp, name_ptr);
	return ret;
}
EXPORT_SYMBOL_GPL(mml_subcomp_init);

s32 mml_comp_init_larb(struct mml_comp *comp, struct device *dev)
{
	struct device_node *node;
	struct platform_device *larb_pdev;

	/* get larb node from dts */
	node = of_parse_phandle(dev->of_node, "mediatek,larb", 0);
	if (!node) {
		mml_err("%s fail to parse mediatek,larb", __func__);
		return -ENOENT;
	}

	larb_pdev = of_find_device_by_node(node);
	if (WARN_ON(!larb_pdev)) {
		of_node_put(node);
		mml_log("%s no larb and defer", __func__);
		return -EPROBE_DEFER;
	}
	of_node_put(node);

	comp->larb_dev = &larb_pdev->dev;

	mml_log("%s dev %p larb dev %u comp %u",
		__func__, dev, larb_pdev, comp->id);

	return 0;
}

s32 mml_comp_pw_enable(struct mml_comp *comp)
{
	int ret;

	if (!comp->larb_dev) {
		mml_err("%s no larb for comp %u", __func__, comp->id);
		return 0;
	}

	ret = mtk_smi_larb_get(comp->larb_dev);

	if (ret)
		mml_err("%s enable fail ret:%d", __func__, ret);
	return ret;
}

s32 mml_comp_pw_disable(struct mml_comp *comp)
{
	if (!comp->larb_dev) {
		mml_err("%s no larb for comp %u", __func__, comp->id);
		return 0;
	}

	mtk_smi_larb_put(comp->larb_dev);

	return 0;
}

s32 mml_comp_clk_enable(struct mml_comp *comp)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR(comp->clks[i]))
			break;
		clk_prepare_enable(comp->clks[i]);
	}

	return 0;
}

s32 mml_comp_clk_disable(struct mml_comp *comp)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR(comp->clks[i]))
			break;
		clk_disable_unprepare(comp->clks[i]);
	}

	return 0;
}

static const struct mml_comp_hw_ops mml_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
};

s32 mml_register_comp(struct device *master, struct mml_comp *comp)
{
	struct mml_dev *mml = dev_get_drvdata(master);

	if (mml->comps[comp->id]) {
		dev_err(master, "duplicated component id:%d\n", comp->id);
		return -EINVAL;
	}
	mml->comps[comp->id] = comp;
	comp->bound = true;

	if (!comp->hw_ops)
		comp->hw_ops = &mml_hw_ops;

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


static int comp_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_dev *mml = dev_get_drvdata(dev);
	struct mtk_mml_sys *sys = mml->sys;

	return mml_sys_bind(dev, master, sys);
}

static void comp_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_dev *mml = dev_get_drvdata(dev);
	struct mtk_mml_sys *sys = mml->sys;

	mml_sys_unbind(dev, master, sys);
}

static const struct component_ops sys_comp_ops = {
	.bind	= comp_bind,
	.unbind = comp_unbind,
};

static bool dbg_probed;
static int mml_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_dev *mml;
	u8 i;
	int ret, thread_cnt;

	mml = devm_kzalloc(dev, sizeof(*mml), GFP_KERNEL);
	if (!mml)
		return -ENOMEM;

	mml->pdev = pdev;
	ret = comp_master_init(dev, mml);
	if (ret) {
		dev_err(dev, "failed to initialize mml component master\n");
		goto err_init_comp;
	}
	mml->sys = mml_sys_create(pdev, &sys_comp_ops);
	if (IS_ERR(mml->sys)) {
		ret = PTR_ERR(mml->sys);
		dev_err(dev, "failed to init mml sys: %d\n", ret);
		goto err_sys_add;
	}

	thread_cnt = of_count_phandle_with_args(
		dev->of_node, "mboxes", "#mbox-cells");
	if (thread_cnt <= 0 || thread_cnt > MML_MAX_CMDQ_CLTS)
		thread_cnt = MML_MAX_CMDQ_CLTS;

	mml->cmdq_base = cmdq_register_device(dev);
	for (i = 0; i < thread_cnt; i++) {
		mml->cmdq_clts[i] = cmdq_mbox_create(dev, i);
		if (IS_ERR(mml->cmdq_clts[i])) {
			ret = PTR_ERR(mml->cmdq_clts[i]);
			dev_err(dev, "unable to create cmdq mbox on %p:%d err %d",
				dev, i, ret);
			mml->cmdq_clts[i] = NULL;
			if (i == 0)
				goto err_mbox_create;
			else
				break;
		}
	}
	mml->cmdq_clt_cnt = i;

	platform_set_drvdata(pdev, mml);
	dbg_probed = true;
	return 0;

err_mbox_create:
	mml_sys_destroy(pdev, mml->sys, &sys_comp_ops);
	for (i = 0; i < MML_MAX_CMDQ_CLTS; i++)
		if (mml->cmdq_clts[i]) {
			cmdq_mbox_destroy(mml->cmdq_clts[i]);
			mml->cmdq_clts[i] = NULL;
		}
err_sys_add:
	comp_master_deinit(dev);
err_init_comp:
	devm_kfree(dev, mml);
	return ret;
}

static int mml_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_dev *mml = platform_get_drvdata(pdev);

	mml_sys_destroy(pdev, mml->sys, &sys_comp_ops);
	comp_master_deinit(dev);
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

static struct platform_driver mml_driver = {
	.probe = mml_probe,
	.remove = mml_remove,
	.driver = {
		.name = "mediatek-mml",
		.owner = THIS_MODULE,
		.pm = &mml_pm_ops,
		.of_match_table = mtk_mml_of_ids,
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

	ret = platform_driver_register(&mml_mutex_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mml_mutex_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mml_sys_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mml_sys_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_hdr_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_hdr_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_color_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_color_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_aal_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_aal_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_tdshp_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_tdshp_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_tcc_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_tcc_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_fg_driver);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_fg_driver.driver.name);
		return ret;
	}

	ret = platform_driver_register(&mtk_mml_test_drv);
	if (ret) {
		mml_err("failed to register %s driver",
			mtk_mml_test_drv.driver.name);
		return ret;
	}

	/* make sure topology for platform ready */
	mml_topology_ip_init();

	mml_pq_core_init();
	/* register pm notifier */

	return 0;
}
module_init(mml_driver_init);

static void __exit mml_driver_exit(void)
{
	mml_topology_ip_exit();

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
