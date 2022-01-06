/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-pq-core.h"
#include "tile_driver.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_color]" fmt


#define COLOR_CFG_MAIN			0x400
#define COLOR_PXL_CNT_MAIN		0x404
#define COLOR_LINE_CNT_MAIN		0x408
#define COLOR_WIN_X_MAIN		0x40c
#define COLOR_WIN_Y_MAIN		0x410
#define COLOR_TIMING_DETECTION_0	0x418
#define COLOR_TIMING_DETECTION_1	0x41c
#define COLOR_DBG_CFG_MAIN		0x420
#define COLOR_C_BOOST_MAIN		0x428
#define COLOR_C_BOOST_MAIN_2		0x42c
#define COLOR_LUMA_ADJ			0x430
#define COLOR_G_PIC_ADJ_MAIN_1		0x434
#define COLOR_G_PIC_ADJ_MAIN_2		0x438
#define COLOR_POS_MAIN			0x484
#define COLOR_INK_DATA_MAIN		0x488
#define COLOR_INK_DATA_MAIN_CR		0x48c
#define COLOR_CAP_IN_DATA_MAIN		0x490
#define COLOR_CAP_IN_DATA_MAIN_CR	0x494
#define COLOR_CAP_OUT_DATA_MAIN		0x498
#define COLOR_CAP_OUT_DATA_MAIN_CR	0x49c
#define COLOR_Y_SLOPE_1_0_MAIN		0x4a0
#define COLOR_Y_SLOPE_3_2_MAIN		0x4a4
#define COLOR_Y_SLOPE_5_4_MAIN		0x4a8
#define COLOR_Y_SLOPE_7_6_MAIN		0x4ac
#define COLOR_Y_SLOPE_9_8_MAIN		0x4b0
#define COLOR_Y_SLOPE_11_10_MAIN	0x4b4
#define COLOR_Y_SLOPE_13_12_MAIN	0x4b8
#define COLOR_Y_SLOPE_15_14_MAIN	0x4bc
#define COLOR_LOCAL_HUE_CD_0		0x620
#define COLOR_LOCAL_HUE_CD_1		0x624
#define COLOR_LOCAL_HUE_CD_2		0x628
#define COLOR_LOCAL_HUE_CD_3		0x62c
#define COLOR_LOCAL_HUE_CD_4		0x630
#define COLOR_TWO_D_WINDOW_1		0x740
#define COLOR_TWO_D_W1_RESULT		0x74c
#define COLOR_SAT_HIST_X_CFG_MAIN	0x768
#define COLOR_SAT_HIST_Y_CFG_MAIN	0x76c
#define COLOR_BWS_2			0x79c
#define COLOR_CRC_0			0x7e0
#define COLOR_CRC_1			0x7e4
#define COLOR_CRC_2			0x7e8
#define COLOR_CRC_3			0x7ec
#define COLOR_CRC_4			0x7f0
#define COLOR_PARTIAL_SAT_GAIN1_0	0x7fc
#define COLOR_PARTIAL_SAT_GAIN1_1	0x800
#define COLOR_PARTIAL_SAT_GAIN1_2	0x804
#define COLOR_PARTIAL_SAT_GAIN1_3	0x808
#define COLOR_PARTIAL_SAT_GAIN1_4	0x80c
#define COLOR_PARTIAL_SAT_GAIN2_0	0x810
#define COLOR_PARTIAL_SAT_GAIN2_1	0x814
#define COLOR_PARTIAL_SAT_GAIN2_2	0x818
#define COLOR_PARTIAL_SAT_GAIN2_3	0x81c
#define COLOR_PARTIAL_SAT_GAIN2_4	0x820
#define COLOR_PARTIAL_SAT_GAIN3_0	0x824
#define COLOR_PARTIAL_SAT_GAIN3_1	0x828
#define COLOR_PARTIAL_SAT_GAIN3_2	0x82c
#define COLOR_PARTIAL_SAT_GAIN3_3	0x830
#define COLOR_PARTIAL_SAT_GAIN3_4	0x834
#define COLOR_PARTIAL_SAT_POINT1_0	0x838
#define COLOR_PARTIAL_SAT_POINT1_1	0x83c
#define COLOR_PARTIAL_SAT_POINT1_2	0x840
#define COLOR_PARTIAL_SAT_POINT1_3	0x844
#define COLOR_PARTIAL_SAT_POINT1_4	0x848
#define COLOR_PARTIAL_SAT_POINT2_0	0x84c
#define COLOR_PARTIAL_SAT_POINT2_1	0x850
#define COLOR_PARTIAL_SAT_POINT2_2	0x854
#define COLOR_PARTIAL_SAT_POINT2_3	0x858
#define COLOR_PARTIAL_SAT_POINT2_4	0x85c
#define COLOR_CM_CONTROL		0x860
#define COLOR_CM_W1_HUE_0		0x864
#define COLOR_CM_W1_HUE_1		0x868
#define COLOR_CM_W1_HUE_2		0x86c
#define COLOR_CM_W1_HUE_3		0x870
#define COLOR_CM_W1_HUE_4		0x874
#define COLOR_CM_W1_LUMA_0		0x878
#define COLOR_CM_W1_LUMA_1		0x87c
#define COLOR_CM_W1_LUMA_2		0x880
#define COLOR_CM_W1_LUMA_3		0x884
#define COLOR_CM_W1_LUMA_4		0x888
#define COLOR_CM_W1_SAT_0		0x88c
#define COLOR_CM_W1_SAT_1		0x890
#define COLOR_CM_W1_SAT_2		0x894
#define COLOR_CM_W1_SAT_3		0x898
#define COLOR_CM_W1_SAT_4		0x89c
#define COLOR_CM_W2_HUE_0		0x8a0
#define COLOR_CM_W2_HUE_1		0x8a4
#define COLOR_CM_W2_HUE_2		0x8a8
#define COLOR_CM_W2_HUE_3		0x8ac
#define COLOR_CM_W2_HUE_4		0x8b0
#define COLOR_CM_W2_LUMA_0		0x8b4
#define COLOR_CM_W2_LUMA_1		0x8b8
#define COLOR_CM_W2_LUMA_2		0x8bc
#define COLOR_CM_W2_LUMA_3		0x8c0
#define COLOR_CM_W2_LUMA_4		0x8c4
#define COLOR_CM_W2_SAT_0		0x8c8
#define COLOR_CM_W2_SAT_1		0x8cc
#define COLOR_CM_W2_SAT_2		0x8d0
#define COLOR_CM_W2_SAT_3		0x8d4
#define COLOR_CM_W2_SAT_4		0x8d8
#define COLOR_CM_W3_HUE_0		0x8dc
#define COLOR_CM_W3_HUE_1		0x8e0
#define COLOR_CM_W3_HUE_2		0x8e4
#define COLOR_CM_W3_HUE_3		0x8e8
#define COLOR_CM_W3_HUE_4		0x8ec
#define COLOR_CM_W3_LUMA_0		0x8f0
#define COLOR_CM_W3_LUMA_1		0x8f4
#define COLOR_CM_W3_LUMA_2		0x8f8
#define COLOR_CM_W3_LUMA_3		0x8fc
#define COLOR_CM_W3_LUMA_4		0x900
#define COLOR_CM_W3_SAT_0		0x904
#define COLOR_CM_W3_SAT_1		0x908
#define COLOR_CM_W3_SAT_2		0x90c
#define COLOR_CM_W3_SAT_3		0x910
#define COLOR_CM_W3_SAT_4		0x914
#define COLOR_START			0xc00
#define COLOR_INTEN			0xc04
#define COLOR_INTSTA			0xc08
#define COLOR_OUT_SEL			0xc0c
#define COLOR_FRAME_DONE_DEL		0xc10
#define COLOR_CRC			0xc14
#define COLOR_SW_SCRATCH		0xc18
#define COLOR_CK_ON			0xc28
#define COLOR_INTERNAL_IP_WIDTH		0xc50
#define COLOR_INTERNAL_IP_HEIGHT	0xc54
#define COLOR_CM1_EN			0xc60
#define COLOR_CM2_EN			0xca0
#define COLOR_SHADOW_CTRL		0xcb0
#define COLOR_R0_CRC			0xcf0
#define COLOR_S_GAIN_BY_Y0_0		0xcf4
#define COLOR_S_GAIN_BY_Y0_1		0xcf8
#define COLOR_S_GAIN_BY_Y0_2		0xcfc
#define COLOR_S_GAIN_BY_Y0_3		0xd00
#define COLOR_S_GAIN_BY_Y0_4		0xd04
#define COLOR_S_GAIN_BY_Y64_0		0xd08
#define COLOR_S_GAIN_BY_Y64_1		0xd0c
#define COLOR_S_GAIN_BY_Y64_2		0xd10
#define COLOR_S_GAIN_BY_Y64_3		0xd14
#define COLOR_S_GAIN_BY_Y64_4		0xd18
#define COLOR_S_GAIN_BY_Y128_0		0xd1c
#define COLOR_S_GAIN_BY_Y128_1		0xd20
#define COLOR_S_GAIN_BY_Y128_2		0xd24
#define COLOR_S_GAIN_BY_Y128_3		0xd28
#define COLOR_S_GAIN_BY_Y128_4		0xd2c
#define COLOR_S_GAIN_BY_Y192_0		0xd30
#define COLOR_S_GAIN_BY_Y192_1		0xd34
#define COLOR_S_GAIN_BY_Y192_2		0xd38
#define COLOR_S_GAIN_BY_Y192_3		0xd3c
#define COLOR_S_GAIN_BY_Y192_4		0xd40
#define COLOR_S_GAIN_BY_Y256_0		0xd44
#define COLOR_S_GAIN_BY_Y256_1		0xd48
#define COLOR_S_GAIN_BY_Y256_2		0xd4c
#define COLOR_S_GAIN_BY_Y256_3		0xd50
#define COLOR_S_GAIN_BY_Y256_4		0xd54
#define COLOR_LSP_1			0xd58
#define COLOR_LSP_2			0xd5c

#define COLOR_WAIT_TIMEOUT_MS (50)
#define COLOR_REG_NUM (155)

struct color_data {
};

struct mml_comp_color {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct color_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct color_frame_data {
	u16 labels[COLOR_REG_NUM];
};

#define color_frm_data(i)	((struct color_frame_data *)(i->data))

static s32 color_prepare(struct mml_comp *comp, struct mml_task *task,
			 struct mml_comp_config *ccfg)
{
	struct color_frame_data *color_frm = NULL;

	color_frm = kzalloc(sizeof(*color_frm), GFP_KERNEL);

	ccfg->data = color_frm;
	return 0;
}

static s32 color_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	s32 ret = 0;

	if (!dest->pq_config.en_color)
		return ret;

	mml_pq_trace_ex_begin("%s", __func__);
	ret = mml_pq_set_comp_config(task);
	mml_pq_trace_ex_end();
	return ret;
}

static s32 color_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg,
			      struct tile_func_block *func,
			      union mml_tile_data *data)
{
	struct mml_frame_dest *dest =
		&task->config->info.dest[ccfg->node->out_idx];

	func->enable_flag = dest->pq_config.en_color;
	return 0;
}

static const struct mml_comp_tile_ops color_tile_ops = {
	.prepare = color_tile_prepare,
};

static void color_init(struct cmdq_pkt *pkt, const phys_addr_t base_pa)
{
	/* relay mode */
	cmdq_pkt_write(pkt, NULL, base_pa + COLOR_START, 3, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + COLOR_CM1_EN, 0, 0x00000001);
	cmdq_pkt_write(pkt, NULL, base_pa + COLOR_CM2_EN, 0, 0x00000001);
	/* Enable shadow */
	cmdq_pkt_write(pkt, NULL, base_pa + COLOR_SHADOW_CTRL, 0x2, U32_MAX);
}

static u32 color_get_label_count(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	mml_pq_msg("%s pipe_id[%d] engine_id[%d] en_color[%d]", __func__,
		ccfg->pipe, comp->id, dest->pq_config.en_color);

	if (!dest->pq_config.en_color)
		return 0;

	return COLOR_REG_NUM;
}

static s32 color_config_init(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	color_init(task->pkts[ccfg->pipe], comp->base_pa);
	return 0;
}

static struct mml_pq_comp_config_result *get_color_comp_config_result(
	struct mml_task *task)
{
	return task->pq_task->comp_config.result;
}

static s32 color_config_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct color_frame_data *color_frm = color_frm_data(ccfg);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_pq_comp_config_result *result = NULL;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];
	s32 ret = 0;

	if (!dest->pq_config.en_color) {
		cmdq_pkt_write(pkt, NULL, base_pa + COLOR_START, 0x3, U32_MAX);
		return ret;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + COLOR_START, 0x1, U32_MAX);

	ret = mml_pq_get_comp_config_result(task, COLOR_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_color_comp_config_result(task);
		if (result) {
			s32 i;
			struct mml_pq_reg *regs = result->color_regs;

			/* TODO: use different regs */
			mml_pq_msg("%s:config color regs, count: %d", __func__,
				result->color_reg_cnt);
			for (i = 0; i < result->color_reg_cnt; i++) {
				mml_write(pkt, base_pa + regs[i].offset, regs[i].value,
					regs[i].mask, reuse, cache,
					&color_frm->labels[i]);
				mml_pq_msg("[color][config][%x] = %#x mask(%#x)",
					regs[i].offset, regs[i].value, regs[i].mask);
			}
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_comp_config_clear(task);
		mml_pq_err("get color param timeout: %d in %dms",
			ret, COLOR_WAIT_TIMEOUT_MS);
	}
	return ret;
}

static s32 color_config_tile(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 width = tile->in.xe - tile->in.xs + 1;
	u32 height = tile->in.ye - tile->in.ys + 1;

	cmdq_pkt_write(pkt, NULL, base_pa + COLOR_INTERNAL_IP_WIDTH, width, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + COLOR_INTERNAL_IP_HEIGHT, height, U32_MAX);

	return 0;
}

static s32 color_config_post(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_frame_dest *dest = &task->config->info.dest[ccfg->node->out_idx];

	if (dest->pq_config.en_color)
		mml_pq_put_comp_config_result(task);
	return 0;
}

static s32 color_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct color_frame_data *color_frm = color_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];
	struct mml_pq_comp_config_result *result = NULL;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	s32 ret = 0;

	if (!dest->pq_config.en_color)
		return ret;

	ret = mml_pq_get_comp_config_result(task, COLOR_WAIT_TIMEOUT_MS);
	if (!ret) {
		result = get_color_comp_config_result(task);
		if (result) {
			s32 i;
			struct mml_pq_reg *regs = result->color_regs;

			/* TODO: use different regs */
			mml_pq_msg("%s:config color regs, count: %d", __func__,
				result->color_reg_cnt);
			for (i = 0; i < result->color_reg_cnt; i++) {
				mml_update(reuse, color_frm->labels[i], regs[i].value);
				mml_pq_msg("[color][config][%x] = %#x mask(%#x)",
					regs[i].offset, regs[i].value, regs[i].mask);
			}
		} else {
			mml_pq_err("%s: not get result from user lib", __func__);
		}
	} else {
		mml_pq_err("get color param timeout: %d in %dms",
			ret, COLOR_WAIT_TIMEOUT_MS);
	}
	return ret;
}

static const struct mml_comp_config_ops color_cfg_ops = {
	.prepare = color_prepare,
	.buf_prepare = color_buf_prepare,
	.get_label_count = color_get_label_count,
	.init = color_config_init,
	.frame = color_config_frame,
	.tile = color_config_tile,
	.post = color_config_post,
	.reframe = color_reconfig_frame,
};

static void color_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[13];
	u32 shadow_ctrl;

	mml_err("color component %u dump:", comp->id);

	/* Enable shadow read working */
	shadow_ctrl = readl(base + COLOR_SHADOW_CTRL);
	shadow_ctrl |= 0x4;
	writel(shadow_ctrl, base + COLOR_SHADOW_CTRL);

	value[0] = readl(base + COLOR_CFG_MAIN);
	value[1] = readl(base + COLOR_PXL_CNT_MAIN);
	value[2] = readl(base + COLOR_LINE_CNT_MAIN);
	value[3] = readl(base + COLOR_WIN_X_MAIN);
	value[4] = readl(base + COLOR_WIN_Y_MAIN);
	value[5] = readl(base + COLOR_DBG_CFG_MAIN);
	value[6] = readl(base + COLOR_START);
	value[7] = readl(base + COLOR_INTEN);
	value[8] = readl(base + COLOR_INTSTA);
	value[9] = readl(base + COLOR_OUT_SEL);
	value[10] = readl(base + COLOR_FRAME_DONE_DEL);
	value[11] = readl(base + COLOR_INTERNAL_IP_WIDTH);
	value[12] = readl(base + COLOR_INTERNAL_IP_HEIGHT);

	mml_err("COLOR_CFG_MAIN %#010x COLOR_PXL_CNT_MAIN %#010x COLOR_LINE_CNT_MAIN %#010x",
		value[0], value[1], value[2]);
	mml_err("COLOR_WIN_X_MAIN %#010x COLOR_WIN_Y_MAIN %#010x",
		value[3], value[4]);
	mml_err("COLOR_DBG_CFG_MAIN %#010x COLOR_START %#010x COLOR_INTEN %#010x",
		value[5], value[6], value[7]);
	mml_err("COLOR_INTSTA %#010x COLOR_OUT_SEL %#010x COLOR_FRAME_DONE_DEL %#010x",
		value[8], value[9], value[10]);
	mml_err("COLOR_INTERNAL_IP_WIDTH %#010x COLOR_INTERNAL_IP_HEIGHT %#010x",
		value[11], value[12]);
}

static const struct mml_comp_debug_ops color_debug_ops = {
	.dump = &color_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_color *color = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	s32 ret;

	if (!drm_dev) {
		ret = mml_register_comp(master, &color->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		ret = mml_ddp_comp_register(drm_dev, &color->ddp_comp);
		if (ret)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			color->ddp_bound = true;
	}
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_color *color = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	if (!drm_dev) {
		mml_unregister_comp(master, &color->comp);
	} else {
		mml_ddp_comp_unregister(drm_dev, &color->ddp_comp);
		color->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static const struct mtk_ddp_comp_funcs ddp_comp_funcs = {
};

static struct mml_comp_color *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_color *priv;
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
	priv->comp.tile_ops = &color_tile_ops;
	priv->comp.config_ops = &color_cfg_ops;
	priv->comp.debug_ops = &color_debug_ops;

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

const struct of_device_id mml_color_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_color",
	},
	{
		.compatible = "mediatek,mt6893-mml_color",
	},
	{
		.compatible = "mediatek,mt6879-mml_color",
	},
	{
		.compatible = "mediatek,mt6895-mml_color",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_color_driver_dt_match);

struct platform_driver mml_color_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-color",
		.owner = THIS_MODULE,
		.of_match_table = mml_color_driver_dt_match,
	},
};

//module_platform_driver(mml_color_driver);

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
module_param_cb(color_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(color_debug, "mml color debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML COLOR driver");
MODULE_LICENSE("GPL v2");
