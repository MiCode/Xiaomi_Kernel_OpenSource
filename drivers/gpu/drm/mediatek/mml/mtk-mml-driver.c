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
#include <linux/debugfs.h>
#include <linux/minmax.h>

#include <soc/mediatek/smi.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <slbc_ops.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-core.h"
#include "mtk-mml-pq-core.h"
#include "mtk-mml-sys.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-color.h"

struct mml_record {
	u32 jobid;

	u64 src_iova[MML_MAX_PLANES];
	u64 dest_iova[MML_MAX_PLANES];

	u32 src_size[MML_MAX_PLANES];
	u32 dest_size[MML_MAX_PLANES];

	u32 src_plane_offset[MML_MAX_PLANES];
	u32 dest_plane_offset[MML_MAX_PLANES];

	u64 src_iova_map_time;
	u64 dest_iova_map_time;
	u64 src_iova_unmap_time;
	u64 dest_iova_unmap_time;
};

/* 512 records
 * note that (MML_RECORD_NUM - 1) will use as mask during track,
 * so change this variable by 1 << N
 */
#define MML_RECORD_NUM		(1 << 9)
#define MML_RECORD_NUM_MASK	(MML_RECORD_NUM - 1)

struct mml_dev {
	struct platform_device *pdev;
	struct mml_comp *comps[MML_MAX_COMPONENTS];
	struct mml_sys *sys;
	struct cmdq_base *cmdq_base;
	struct cmdq_client *cmdq_clts[MML_MAX_CMDQ_CLTS];
	u8 cmdq_clt_cnt;

	atomic_t drm_cnt;
	struct mml_drm_ctx *drm_ctx;
	atomic_t dle_cnt;
	struct mml_dle_ctx *dle_ctx;
	struct mml_topology_cache *topology;
	struct mutex ctx_mutex;
	struct mutex clock_mutex;

	/* sram operation */
	bool racing_en;
	struct slbc_data sram_data;
	s32 sram_cnt;
	struct mutex sram_mutex;
	/* The height of racing mode for each output tile in pixel. */
	u8 racing_height;
	/* inline rotate sync event */
	u16 event_mml_ready;
	u16 event_disp_ready;
	u16 event_mml_stop;

	/* wack lock to prevent system off */
	struct wakeup_source *wake_lock;
	s32 wake_ref;
	struct mutex wake_ref_mutex;

	/* mml record to tracking task */
	struct dentry *record_entry;
	struct mml_record records[MML_RECORD_NUM];
	struct mutex record_mutex;
	u16 record_idx;
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
		tp->opp_speeds[i] = (u32)div_u64(freq, 1000000) - 5;
		tp->opp_volts[i] = dev_pm_opp_get_voltage(opp);
		tp->freq_max = tp->opp_speeds[i];
		mml_log("mml opp %u: %uMHz\t%d",
			i, tp->opp_speeds[i], tp->opp_volts[i]);
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
}

u32 mml_qos_update_tput(struct mml_dev *mml)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(mml);
	u32 tput = 0, i;
	int volt, ret;

	if (!tp || !tp->reg)
		return 0;

	for (i = 0; i < ARRAY_SIZE(tp->path_clts); i++) {
		/* select max one across clients */
		tput = max(tput, tp->path_clts[i].throughput);
	}

	for (i = 0; i < tp->opp_cnt; i++) {
		if (tput < tp->opp_speeds[i])
			break;
	}
	i = min(i, tp->opp_cnt - 1);
	volt = tp->opp_volts[i];
	mml_trace_begin("mml_volt_%u", volt);
	ret = regulator_set_voltage(tp->reg, volt, INT_MAX);
	mml_trace_end();
	if (ret)
		mml_err("%s fail to set volt %d", __func__, volt);
	else
		mml_msg("%s volt %d (%u) tput %u", __func__, volt, i, tput);

	return tp->opp_speeds[i];
}

static void create_dev_topology_locked(struct mml_dev *mml)
{
	/* make sure topology ready before client can use mml */
	if (!mml->topology) {
		mml->topology = mml_topology_create(mml, mml->pdev,
			mml->cmdq_clts, mml->cmdq_clt_cnt);
		mml_qos_init(mml);
	}
	if (IS_ERR(mml->topology))
		mml_err("topology create fail %ld", PTR_ERR(mml->topology));
}

struct mml_drm_ctx *mml_dev_get_drm_ctx(struct mml_dev *mml,
	struct mml_drm_param *disp,
	struct mml_drm_ctx *(*ctx_create)(struct mml_dev *mml,
	struct mml_drm_param *disp))
{
	struct mml_drm_ctx *ctx;

	mutex_lock(&mml->ctx_mutex);

	create_dev_topology_locked(mml);
	if (IS_ERR(mml->topology)) {
		ctx = ERR_CAST(mml->topology);
		goto exit;
	}

	if (atomic_inc_return(&mml->drm_cnt) == 1)
		mml->drm_ctx = ctx_create(mml, disp);
	ctx = mml->drm_ctx;

exit:
	mutex_unlock(&mml->ctx_mutex);
	return ctx;
}

void mml_dev_put_drm_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_drm_ctx *ctx))
{
	struct mml_drm_ctx *ctx;
	int cnt;

	mutex_lock(&mml->ctx_mutex);
	ctx = mml->drm_ctx;
	cnt = atomic_dec_if_positive(&mml->drm_cnt);
	if (cnt == 0)
		mml->drm_ctx = NULL;
	mutex_unlock(&mml->ctx_mutex);
	if (cnt == 0)
		ctx_release(ctx);

	WARN_ON(cnt < 0);
}

struct mml_dle_ctx *mml_dev_get_dle_ctx(struct mml_dev *mml,
	struct mml_dle_param *dl,
	struct mml_dle_ctx *(*ctx_create)(struct mml_dev *mml,
	struct mml_dle_param *dl))
{
	struct mml_dle_ctx *ctx;

	mutex_lock(&mml->ctx_mutex);

	create_dev_topology_locked(mml);
	if (IS_ERR(mml->topology)) {
		ctx = ERR_CAST(mml->topology);
		goto exit;
	}

	if (atomic_inc_return(&mml->dle_cnt) == 1)
		mml->dle_ctx = ctx_create(mml, dl);
	ctx = mml->dle_ctx;

exit:
	mutex_unlock(&mml->ctx_mutex);
	return ctx;
}

void mml_dev_put_dle_ctx(struct mml_dev *mml,
	void (*ctx_release)(struct mml_dle_ctx *ctx))
{
	struct mml_dle_ctx *ctx;
	int cnt;

	mutex_lock(&mml->ctx_mutex);
	ctx = mml->dle_ctx;
	cnt = atomic_dec_if_positive(&mml->dle_cnt);
	if (cnt == 0)
		mml->dle_ctx = NULL;
	mutex_unlock(&mml->ctx_mutex);
	if (cnt == 0)
		ctx_release(ctx);

	WARN_ON(cnt < 0);
}

struct mml_topology_cache *mml_topology_get_cache(struct mml_dev *mml)
{
	return IS_ERR(mml->topology) ? NULL : mml->topology;
}
EXPORT_SYMBOL_GPL(mml_topology_get_cache);

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
	u32 comp_count = 0;
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

static inline int of_mml_read_comp_name_index(const struct device_node *np,
	int index, const char **name)
{
	return of_property_read_string_index(np, "comp-names", index, name);
}

static const char *comp_clock_names = "comp-clock-names";

static s32 comp_init(struct platform_device *pdev, struct mml_comp *comp,
	const char *clkpropname)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct resource *res;
	struct property *prop;
	const char *clkname;
	int i, ret;

	comp->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (!res) {
		dev_err(dev, "failed to get resource\n");
		return -EINVAL;
	}
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
	} else if (!comp->sub_idx) {
		/* get all clks as component */
		for (i = 0; i < ARRAY_SIZE(comp->clks); i++) {
			comp->clks[i] = of_clk_get(node, i);
			if (IS_ERR(comp->clks[i]))
				break;
		}
		if (!i)
			dev_info(dev, "no clks in node %s\n", node->full_name);
	} else {
		/* no named clks as subcomponent */
		dev_info(dev, "no %s property in node %s\n",
			 clkpropname, node->full_name);
	}

	return ret;
}

s32 mml_comp_init(struct platform_device *comp_pdev, struct mml_comp *comp)
{
	struct device *dev = &comp_pdev->dev;
	struct device_node *node = dev->of_node;
	u32 comp_id;
	int ret;

	ret = of_mml_read_comp_id_index(node, 0, &comp_id);
	if (ret) {
		dev_err(dev, "no comp-ids in component %s: %d\n",
			node->full_name, ret);
		return -EINVAL;
	}
	comp->id = comp_id;
	of_mml_read_comp_name_index(node, 0, &comp->name);
	return comp_init(comp_pdev, comp, comp_clock_names);
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
	if (!of_mml_read_comp_name_index(node, subcomponent, &comp->name)) {
		ret = snprintf(name, sizeof(name), "%s-clock-names", comp->name);
		if (ret >= sizeof(name)) {
			dev_err(dev, "len:%d over name size:%d",
				ret, sizeof(name));
			name[sizeof(name) - 1] = '\0';
		}
		name_ptr = name;
	} else if (!comp->sub_idx) {
		name_ptr = comp_clock_names;
	}
	return comp_init(comp_pdev, comp, name_ptr);
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
		return -ENOENT;
	}
	comp->larb_port = larb_args.args[0];
	if (!of_address_to_resource(larb_args.np, 0, &res))
		comp->larb_base = res.start;

	larb_pdev = of_find_device_by_node(larb_args.np);
	of_node_put(larb_args.np);
	if (WARN_ON(!larb_pdev)) {
		mml_log("%s no larb and defer", __func__);
		return -EPROBE_DEFER;
	}
	/* larb dev for smi api */
	comp->larb_dev = &larb_pdev->dev;

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
	int ret;

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
		ret = clk_prepare_enable(comp->clks[i]);
		if (ret)
			mml_err("%s clk_prepare_enable fail %d", __func__, ret);
	}

	return 0;
}

#define call_hw_op(_comp, op, ...) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp, ##__VA_ARGS__) : 0)

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

	/* clear bandwidth before disable if this component support dma */
	call_hw_op(comp, qos_clear);

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
	/* 1536 is the worst bw calculated by DE */
	return min_t(u32, div_u64(data, pixel), 1536);
}

static u32 mml_calc_bw_racing(u32 datasize)
{
	/* hrt bw: width * height * bpp * fps * 1.25 * 1.33 = HRT MB/s
	 *
	 * width * height * bpp = datasize in bytes
	 * the 1.25 (v-blanking) separate to * 10 / 8
	 * the 1.33 (occupy bandwidth) separate to * 4 / 3
	 *
	 * so div_u64((u64)(datasize * 120 * 10 * 4) >> 3, 3 * 1000000)
	 */
	return (u32)div_u64((u64)datasize, 5000);
}

void mml_comp_qos_set(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 throughput, u32 tput_up)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	u32 bandwidth, datasize, hrt_bw;
	bool hrt;

	datasize = comp->hw_ops->qos_datasize_get(task, ccfg);
	if (!datasize) {
		hrt = false;
		bandwidth = 0;
		hrt_bw = 0;
	} else if (cfg->info.mode == MML_MODE_RACING) {
		hrt = true;
		bandwidth = mml_calc_bw_racing(datasize);
		if (unlikely(mml_racing_urgent))
			bandwidth = U32_MAX;
		hrt_bw = cfg->disp_hrt;
	} else {
		hrt = false;
		bandwidth = mml_calc_bw(datasize, cache->max_pixel, throughput);
		hrt_bw = 0;
	}

	/* store for debug log */
	task->pipe[ccfg->pipe].bandwidth = max(bandwidth,
		task->pipe[ccfg->pipe].bandwidth);
	mml_trace_begin("mml_bw_%u_%u", bandwidth, hrt_bw);
	mtk_icc_set_bw(comp->icc_path, MBps_to_icc(bandwidth), hrt_bw);
	mml_trace_end();

	mml_msg_qos("%s comp %u %s qos bw %u(%u) by throughput %u pixel %u size %u%s",
		__func__, comp->id, comp->name, bandwidth, hrt_bw / 1000,
		throughput, cache->max_pixel, datasize,
		hrt ? " hrt" : "");
}

void mml_comp_qos_clear(struct mml_comp *comp)
{
	mtk_icc_set_bw(comp->icc_path, 0, 0);
	mml_msg_qos("%s comp %u %s qos bw clear", __func__, comp->id, comp->name);
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

bool mml_racing_enable(struct mml_dev *mml)
{
	return mml->racing_en;
}
EXPORT_SYMBOL_GPL(mml_racing_enable);

u8 mml_sram_get_racing_height(struct mml_dev *mml)
{
	return mml->racing_height;
}

u16 mml_ir_get_mml_ready_event(struct mml_dev *mml)
{
	return mml->event_mml_ready;
}

u16 mml_ir_get_disp_ready_event(struct mml_dev *mml)
{
	return mml->event_disp_ready;
}

u16 mml_ir_get_mml_stop_event(struct mml_dev *mml)
{
	return mml->event_mml_stop;
}

void mml_dump_thread(struct mml_dev *mml)
{
	u32 i;

	for (i = 0; i < MML_MAX_CMDQ_CLTS; i++)
		if (mml->cmdq_clts[i])
			cmdq_thread_dump(mml->cmdq_clts[i]->chan, NULL, NULL, NULL);
}

void mml_clock_lock(struct mml_dev *mml)
{
	mutex_lock(&mml->clock_mutex);
}

void mml_clock_unlock(struct mml_dev *mml)
{
	mutex_unlock(&mml->clock_mutex);
}

void mml_lock_wake_lock(struct mml_dev *mml, bool lock)
{
	mutex_lock(&mml->wake_ref_mutex);
	if (lock) {
		mml->wake_ref++;
		if (mml->wake_ref == 1)
			__pm_stay_awake(mml->wake_lock);
	} else {
		mml->wake_ref--;
		if (mml->wake_ref == 0)
			__pm_relax(mml->wake_lock);

		if (mml->wake_ref < 0)
			mml_err("%s wake_ref < 0", __func__);
	}
	mutex_unlock(&mml->wake_ref_mutex);
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

void mml_record_track(struct mml_dev *mml, struct mml_task *task)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_task_buffer *buf = &task->buf;
	struct mml_record *record;
	u32 i;

	mutex_lock(&mml->record_mutex);

	record = &mml->records[mml->record_idx];

	record->jobid = task->job.jobid;
	for (i = 0; i < MML_MAX_PLANES; i++) {
		buf = &task->buf;

		record->src_iova[i] = buf->src.dma[i].iova;
		record->dest_iova[i] = buf->dest[0].dma[i].iova;
		record->src_size[i] = buf->src.size[i];
		record->dest_size[i] = buf->dest[0].size[i];
		record->src_plane_offset[i] = cfg->info.src.plane_offset[i];
		record->dest_plane_offset[i] = cfg->info.dest[0].data.plane_offset[i];
	}
	record->src_iova_map_time = buf->src.map_time;
	record->dest_iova_map_time = buf->dest[0].map_time;
	record->src_iova_unmap_time = buf->src.unmap_time;
	record->dest_iova_unmap_time = buf->dest[0].unmap_time;

	mml->record_idx = (mml->record_idx + 1) & MML_RECORD_NUM_MASK;

	mutex_unlock(&mml->record_mutex);
}

#define REC_TITLE "Index,Job ID," \
	"src map time,src unmap time,src,src size,plane 0,plane 1, plane 2," \
	"dest map time,dest unmap time,dest,dest size,plane 0,plane 1, plane 2"

static int mml_record_print(struct seq_file *seq, void *data)
{
	struct mml_dev *mml = (struct mml_dev *)seq->private;
	struct mml_record *record;
	u32 i, idx;

	/* Protect only index part, since it is ok to print race data,
	 * but not good to hurt performance of mml_record_track.
	 */
	mutex_lock(&mml->record_mutex);
	idx = mml->record_idx;
	mutex_unlock(&mml->record_mutex);

	seq_puts(seq, REC_TITLE ",\n");
	for (i = 0; i < ARRAY_SIZE(mml->records); i++) {
		record = &mml->records[idx];
		seq_printf(seq, "%u,%u,%llu,%llu,%#llx,%u,%u,%u,%u,%llu,%llu,%#llx,%u,%u,%u,%u,\n",
			idx,
			record->jobid,
			record->src_iova_map_time,
			record->src_iova_unmap_time,
			record->src_iova[0],
			record->src_size[0],
			record->src_plane_offset[0],
			record->src_plane_offset[1],
			record->src_plane_offset[2],
			record->dest_iova_map_time,
			record->dest_iova_unmap_time,
			record->dest_iova[0],
			record->dest_size[0],
			record->dest_plane_offset[0],
			record->dest_plane_offset[1],
			record->dest_plane_offset[2]);
		idx = (idx + 1) & MML_RECORD_NUM_MASK;
	}

	return 0;
}

void mml_record_dump(struct mml_dev *mml)
{
	struct mml_record *record;
	u32 i, idx;
	/* dump 10 records only */
	const u32 dump_count = 10;

	/* Protect only index part, since it is ok to print race data,
	 * but not good to hurt performance of mml_record_track.
	 */
	mutex_lock(&mml->record_mutex);
	idx = (mml->record_idx + MML_RECORD_NUM - dump_count) & MML_RECORD_NUM_MASK;
	mutex_unlock(&mml->record_mutex);

	mml_err(REC_TITLE);
	for (i = 0; i < dump_count; i++) {
		record = &mml->records[idx];
		mml_err("%u,%u,%llu,%llu,%#llx,%u,%u,%u,%u,%llu,%llu,%#llx,%u,%u,%u,%u",
			idx,
			record->jobid,
			record->src_iova_map_time,
			record->src_iova_unmap_time,
			record->src_iova[0],
			record->src_size[0],
			record->src_plane_offset[0],
			record->src_plane_offset[1],
			record->src_plane_offset[2],
			record->dest_iova_map_time,
			record->dest_iova_unmap_time,
			record->dest_iova[0],
			record->dest_size[0],
			record->dest_plane_offset[0],
			record->dest_plane_offset[1],
			record->dest_plane_offset[2]);
		idx = (idx + 1) & MML_RECORD_NUM_MASK;
	}
}

static int mml_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, mml_record_print, inode->i_private);
}

static const struct file_operations mml_record_fops = {
	.owner = THIS_MODULE,
	.open = mml_record_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void mml_record_init(struct mml_dev *mml)
{
	struct dentry *dir;
	bool exists = false;

	mutex_init(&mml->record_mutex);

	dir = debugfs_lookup("mml", NULL);
	if (!dir) {
		dir = debugfs_create_dir("mml", NULL);
		if (IS_ERR(dir) && PTR_ERR(dir) != -EEXIST) {
			mml_err("debugfs_create_dir mml failed:%ld", PTR_ERR(dir));
			return;
		}
	} else
		exists = true;

	mml->record_entry = debugfs_create_file(
		"mml-record", 0444, dir, mml, &mml_record_fops);
	if (IS_ERR(mml->record_entry)) {
		mml_err("debugfs_create_file mml-record failed:%ld",
			PTR_ERR(mml->record_entry));
		mml->record_entry = NULL;
	}

	if (exists)
		dput(dir);
}

static int sys_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_dev *mml = dev_get_drvdata(dev);
	struct mml_sys *sys = mml->sys;

	return mml_sys_bind(dev, master, sys, data);
}

static void sys_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_dev *mml = dev_get_drvdata(dev);
	struct mml_sys *sys = mml->sys;

	mml_sys_unbind(dev, master, sys, data);
}

static const struct component_ops sys_comp_ops = {
	.bind	= sys_bind,
	.unbind = sys_unbind,
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
	mutex_init(&mml->ctx_mutex);
	mutex_init(&mml->clock_mutex);
	mutex_init(&mml->wake_ref_mutex);

	/* init sram request parameters */
	mutex_init(&mml->sram_mutex);
	mml->sram_data.uid = UID_MML;
	mml->sram_data.type = TP_BUFFER;
	mml->sram_data.flag = FG_POWER;

	ret = comp_master_init(dev, mml);
	if (ret) {
		dev_err(dev, "failed to initialize mml component master\n");
		goto err_init_master;
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

	mml->racing_en = of_property_read_bool(dev->of_node, "racing-enable");
	if (mml->racing_en)
		mml_log("racing mode enable");

	if (of_property_read_u8(dev->of_node, "racing_height", &mml->racing_height))
		mml->racing_height = 64;	/* default height 64px */

	if (!of_property_read_u16(dev->of_node, "event_ir_mml_ready", &mml->event_mml_ready))
		mml_log("racing event event_mml_ready %hu", mml->event_mml_ready);

	if (!of_property_read_u16(dev->of_node, "event_ir_disp_ready", &mml->event_disp_ready))
		mml_log("racing event event_disp_ready %hu", mml->event_disp_ready);

	if (!of_property_read_u16(dev->of_node, "event_ir_mml_stop", &mml->event_mml_stop))
		mml_log("racing event event_mml_stop %hu", mml->event_mml_stop);

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

	mml->wake_lock = wakeup_source_register(dev, "mml_pm_lock");
	mml_record_init(mml);
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
err_init_master:
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

	wakeup_source_unregister(mml->wake_lock);
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

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	&mtk_mml_test_drv,
#endif
};

static int __init mml_driver_init(void)
{
	int ret;

	ret = platform_register_drivers(mml_drivers, ARRAY_SIZE(mml_drivers));
	if (ret) {
		mml_err("failed to register mml core drivers: %d", ret);
		return ret;
	}

	mml_mmp_init();

	ret = mml_pq_core_init();
	if (ret)
		mml_err("failed to init mml pq core: %d", ret);

	/* register pm notifier */

	return ret;
}
module_init(mml_driver_init);

static void __exit mml_driver_exit(void)
{
	mml_pq_core_uninit();
	platform_unregister_drivers(mml_drivers, ARRAY_SIZE(mml_drivers));
}
module_exit(mml_driver_exit);

static s32 dbg_case;
static int dbg_set(const char *val, const struct kernel_param *kp)
{
	int result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump current setting");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static int dbg_get(char *buf, const struct kernel_param *kp)
{
	int length = 0;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed: %d\n", dbg_case, dbg_probed);
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
module_param_cb(drv_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(drv_debug, "mml driver debug case");

MODULE_DESCRIPTION("MediaTek multimedia-layer driver");
MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
