// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/component.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/math64.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include <soc/mediatek/smi.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <slbc_ops.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-core.h"
#include "mtk-mml-pq-core.h"
#include "mtk-mml-sys.h"

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
	struct mutex clock_mutex;

	/* sram operation */
	struct slbc_data sram_data;
	s32 sram_cnt;
	struct mutex sram_mutex;
	/* The height of racing mode for each output tile in pixel. */
	u8 racing_height;
};

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

static void mml_qos_init(struct mml_dev *mml)
{
	struct device *dev = &mml->pdev->dev;
	struct mml_topology_cache *tp = mml_topology_get_cache(mml);
	struct dev_pm_opp *opp;
	int num;
	unsigned long freq = 0;
	u32 i;

	if (!tp) {
		mml_err("%s topology fail so stop qos", __func__);
		return;
	}

	mutex_init(&tp->qos_mutex);

	/* Create opp table from dts */
	dev_pm_opp_of_add_table(dev);

	/* Get regulator instance by name. */
	tp->reg = devm_regulator_get(dev, "dvfsrc-vcore");
	if (IS_ERR(tp->reg)) {
		mml_log("%s not support dvfs %d", __func__, (int)PTR_ERR(tp->reg));
		tp->reg = NULL;
		return;
	}

	num = dev_pm_opp_get_opp_count(dev); /* number of available opp */
	if (num <= 0) {
		mml_err("%s no available opp table %d", __func__, num);
		return;
	}

	tp->opp_cnt = (u32)num;
	if (tp->opp_cnt > ARRAY_SIZE(tp->opp_speeds)) {
		mml_err("%s opp num more than table size %u %u",
			__func__, tp->opp_cnt, (u32)ARRAY_SIZE(tp->opp_speeds));
		tp->opp_cnt = ARRAY_SIZE(tp->opp_speeds);
	}

	i = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		/* available freq from table, store in MHz */
		tp->opp_speeds[i] = (u32)div_u64(freq, 1000000);
		tp->opp_volts[i] = dev_pm_opp_get_voltage(opp);
		tp->freq_max = tp->opp_speeds[i];
		mml_log("mml opp %u: %uMHz\t%d",
			i, tp->opp_speeds[i], tp->opp_volts[i]);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
}

void mml_qos_update_tput(struct mml_dev *mml)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(mml);
	u32 tput = 0, i;
	int volt, ret;

	if (!tp || !tp->reg)
		return;

	for (i = 0; i < ARRAY_SIZE(tp->path_clts); i++) {
		/* select max one across clients */
		tput = max(tput, tp->path_clts[i].throughput);
	}

	for (i = 0; i < tp->opp_cnt; i++) {
		if (tput <= tp->opp_speeds[i])
			break;
	}
	volt = tp->opp_volts[min(i, tp->opp_cnt - 1)];
	ret = regulator_set_voltage(tp->reg, volt, INT_MAX);
	if (ret)
		mml_err("%s fail to set volt %d", __func__, volt);
	else
		mml_msg("%s volt %d (%u) tput %u", __func__, volt, i, tput);
}


struct mml_drm_ctx *mml_dev_get_drm_ctx(struct mml_dev *mml,
	struct mml_drm_param *disp,
	struct mml_drm_ctx *(*ctx_create)(struct mml_dev *mml,
	struct mml_drm_param *disp))
{
	struct mml_drm_ctx *ctx;

	/* make sure topology ready before client can use mml */
	if (!mml->topology) {
		mml->topology = mml_topology_create(mml, mml->pdev,
			mml->cmdq_clts, mml->cmdq_clt_cnt);
		mml_qos_init(mml);
	}
	if (IS_ERR(mml->topology)) {
		mml_err("topology create fail %ld", PTR_ERR(mml->topology));
		return (void *)mml->topology;
	}

	mutex_lock(&mml->drm_ctx_mutex);
	if (atomic_inc_return(&mml->drm_cnt) == 1)
		mml->drm_ctx = ctx_create(mml, disp);
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
EXPORT_SYMBOL_GPL(mml_dev_get_comp_by_id);

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
	return __comp_init(comp_pdev, comp, comp_clock_names);
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
	return __comp_init(comp_pdev, comp, name_ptr);
}
EXPORT_SYMBOL_GPL(mml_subcomp_init);

s32 mml_comp_init_larb(struct mml_comp *comp, struct device *dev)
{
	struct platform_device *larb_pdev;
	struct of_phandle_args larb_args;
	struct resource res;

	/* parse larb node and port from dts */
	if (of_parse_phandle_with_fixed_args(dev->of_node, "mediatek,larb",
		1, 0, &larb_args)) {
		mml_err("%s fail to parse mediatek,larb", __func__);
		return 0; /* -ENOENT; for FPGA no smi_larb */
	}
	comp->smi_port = larb_args.args[0];
	if (!of_address_to_resource(larb_args.np, 0, &res))
		comp->smi_base = res.start;

	larb_pdev = of_find_device_by_node(larb_args.np);
	if (WARN_ON(!larb_pdev)) {
		of_node_put(larb_args.np);
		mml_log("%s no larb and defer", __func__);
		return -EPROBE_DEFER;
	}
	/* larb dev for smi api */
	comp->larb_dev = &larb_pdev->dev;

	of_node_put(larb_args.np);

	/* also do mmqos and mmdvfs since dma component do init here */
	comp->icc_path = of_mtk_icc_get(dev, "mml_dma");
	if (IS_ERR_OR_NULL(comp->icc_path)) {
		mml_err("%s not support qos", __func__);
		comp->icc_path = NULL;
	}

	return 0;
}

s32 mml_comp_pw_enable(struct mml_comp *comp)
{
	int ret;

	comp->pw_cnt++;
	if (comp->pw_cnt > 1)
		return 0;
	if (comp->pw_cnt <= 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->pw_cnt);
		return -EINVAL;
	}

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
	comp->pw_cnt--;
	if (comp->pw_cnt > 0)
		return 0;
	if (comp->pw_cnt < 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->pw_cnt);
		return -EINVAL;
	}

	if (!comp->larb_dev) {
		mml_err("%s no larb for comp %u", __func__, comp->id);
		return 0;
	}

	mtk_smi_larb_put(comp->larb_dev);

	return 0;
}

s32 mml_comp_clk_enable(struct mml_comp *comp)
{
	u32 i;

	comp->clk_cnt++;
	if (comp->clk_cnt > 1)
		return 0;
	if (comp->clk_cnt <= 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->clk_cnt);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR(comp->clks[i]))
			break;
		clk_prepare_enable(comp->clks[i]);
	}

	return 0;
}

s32 mml_comp_clk_disable(struct mml_comp *comp)
{
	u32 i;

	comp->clk_cnt--;
	if (comp->clk_cnt > 0)
		return 0;
	if (comp->clk_cnt < 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->clk_cnt);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
		if (IS_ERR(comp->clks[i]))
			break;
		clk_disable_unprepare(comp->clks[i]);
	}

	return 0;
}

/* mml_calc_bw - calculate bandwidth by giving pixel and current throughput
 *
 * @data:	data size in bytes, size to each dma port
 * @pixel:	pixel count, one of max pixel count of all dma ports
 * @throughput:	throughput (frequency) in Mhz, to handling frame in time
 *
 * Note: throughput calculate by following formula:
 *		max_pixel / (end_time - current_time)
 *	which represents necessary cycle (or frequency in MHz) to process
 *	pixels in this time slot (before end time from current time).
 */
static u32 mml_calc_bw(u64 data, u32 pixel, u64 throughput)
{
	/* ocucpied bw efficiency is 1.33 while accessing DRAM */
	data = (u64)div_u64(data * 4 * throughput, 3);
	if (!pixel)
		pixel = 1;
	return (u32)div_u64(data, pixel);
}

void mml_comp_qos_set(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 throughput)
{
	struct mml_pipe_cache *cache = &task->config->cache[ccfg->pipe];
	u32 bandwidth, datasize;

	datasize = comp->hw_ops->qos_datasize_get(task, ccfg);
	bandwidth = mml_calc_bw(datasize, cache->max_pixel, throughput);
	mtk_icc_set_bw(comp->icc_path, MBps_to_icc(bandwidth), 0);
	mml_msg("%s comp %u %s qos bw %u by throughput %u pixel %u",
		__func__, comp->id, comp->name, bandwidth,
		throughput, cache->max_pixel);
}

void mml_comp_qos_clear(struct mml_comp *comp)
{
	mtk_icc_set_bw(comp->icc_path, 0, 0);
	mml_msg("%s comp %u %s qos bw clear", __func__, comp->id, comp->name);
}

static const struct mml_comp_hw_ops mml_hw_ops = {
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
};

void __iomem *mml_sram_get(struct mml_dev *mml)
{
	int ret;
	void __iomem *sram = NULL;

	mutex_lock(&mml->sram_mutex);

	if (!mml->sram_cnt) {
		ret = slbc_request(&mml->sram_data);
		if (ret < 0) {
			mml_err("%s request slbc fail %d", __func__, ret);
			goto done;
		}

		ret = slbc_power_on(&mml->sram_data);
		if (ret < 0) {
			mml_err("%s slbc power on fail %d", __func__, ret);
			goto done;
		}

		mml_msg("mml sram %#lx", (unsigned long)mml->sram_data.paddr);
	}

	mml->sram_cnt++;
	sram = mml->sram_data.paddr;

done:
	mutex_unlock(&mml->sram_mutex);
	return sram;
}

void mml_sram_put(struct mml_dev *mml)
{
	mutex_lock(&mml->sram_mutex);

	mml->sram_cnt--;
	if (!mml->sram_cnt) {
		slbc_power_off(&mml->sram_data);
		slbc_release(&mml->sram_data);
		goto done;
	} else if (mml->sram_cnt < 0) {
		mml_err("%s sram slbc count wrong %d", __func__, mml->sram_cnt);
		goto done;
	}

done:
	mutex_unlock(&mml->sram_mutex);
}

u8 mml_sram_get_racing_height(struct mml_dev *mml)
{
	return mml->racing_height;
}

void mml_clock_lock(struct mml_dev *mml)
{
	mutex_lock(&mml->clock_mutex);
}

void mml_clock_unlock(struct mml_dev *mml)
{
	mutex_unlock(&mml->clock_mutex);
}

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
	u32 i;
	int ret, thread_cnt;

	mml = devm_kzalloc(dev, sizeof(*mml), GFP_KERNEL);
	if (!mml)
		return -ENOMEM;

	mml->pdev = pdev;
	mutex_init(&mml->drm_ctx_mutex);
	mutex_init(&mml->clock_mutex);

	/* init sram request parameters */
	mutex_init(&mml->sram_mutex);
	mml->sram_data.uid = UID_MML;
	mml->sram_data.type = TP_BUFFER;
	mml->sram_data.flag = FG_POWER;

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

	if (of_property_read_u8(dev->of_node, "racing_height", &mml->racing_height))
		mml->racing_height = 64;	/* default height 64px */

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

static struct platform_driver mtk_mml_driver = {
	.probe = mml_probe,
	.remove = mml_remove,
	.driver = {
		.name = "mediatek-mml",
		.owner = THIS_MODULE,
		.pm = &mml_pm_ops,
		.of_match_table = mtk_mml_of_ids,
	},
};

static struct platform_driver *mml_drivers[] = {
	&mtk_mml_driver,
	&mml_sys_driver,
	&mml_aal_driver,
	&mml_color_driver,
	&mml_fg_driver,
	&mml_hdr_driver,
	&mml_mutex_driver,
	&mml_rdma_driver,
	&mml_rsz_driver,
	&mml_tcc_driver,
	&mml_tdshp_driver,
	&mml_wrot_driver,

	&mtk_mml_test_drv,
};

static int __init mml_driver_init(void)
{
	int ret;

	ret = platform_register_drivers(mml_drivers, ARRAY_SIZE(mml_drivers));
	if (ret) {
		mml_err("failed to register mml core drivers");
		return ret;
	}

	mml_pq_core_init();
	/* register pm notifier */

	return 0;
}
module_init(mml_driver_init);

static void __exit mml_driver_exit(void)
{
	platform_unregister_drivers(mml_drivers, ARRAY_SIZE(mml_drivers));
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
