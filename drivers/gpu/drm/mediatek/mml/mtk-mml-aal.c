/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/component.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-drm-adaptor.h"
#include "mtk_drm_ddp_comp.h"

#include "tile_driver.h"
#include "tile_mdp_reg.h"

#define AAL_EN				0x000
#define AAL_RESET			0x004
#define AAL_INTEN			0x008
#define AAL_INTSTA			0x00c
#define AAL_STATUS			0x010
#define AAL_CFG				0x020
#define AAL_INPUT_COUNT			0x024
#define AAL_OUTPUT_COUNT		0x028
#define AAL_CHKSUM			0x02c
#define AAL_SIZE			0x030
#define AAL_OUTPUT_SIZE			0x034
#define AAL_OUTPUT_OFFSET		0x038
#define AAL_DUMMY_REG			0x0c0
#define AAL_SRAM_CFG			0x0c4
#define AAL_SRAM_STATUS			0x0c8
#define AAL_SRAM_RW_IF_0		0x0cc
#define AAL_SRAM_RW_IF_1		0x0d0
#define AAL_SRAM_RW_IF_2		0x0d4
#define AAL_SRAM_RW_IF_3		0x0d8
#define AAL_SHADOW_CTRL			0x0f0
#define AAL_TILE_02			0x0f4
#define AAL_DRE_BLOCK_INFO_07		0x0f8
#define AAL_ATPG			0x0fc
#define AAL_DREI_PAT_GEN_SET		0x100
#define AAL_DREI_PAT_GEN_COLOR0		0x108
#define AAL_DREI_PAT_GEN_COLOR1		0x10c
#define AAL_DREO_PAT_GEN_SET		0x130
#define AAL_DREO_PAT_GEN_COLOR0		0x138
#define AAL_DREO_PAT_GEN_COLOR1		0x13c
#define AAL_DREO_PAT_GEN_POS		0x144
#define AAL_DREO_PAT_GEN_CURSOR_RB0	0x148
#define AAL_DREO_PAT_GEN_CURSOR_RB1	0x14c
#define AAL_CABCO_PAT_GEN_SET		0x160
#define AAL_CABCO_PAT_GEN_FRM_SIZE	0x164
#define AAL_CABCO_PAT_GEN_COLOR0	0x168
#define AAL_CABCO_PAT_GEN_COLOR1	0x16c
#define AAL_CABCO_PAT_GEN_COLOR2	0x170
#define AAL_CABCO_PAT_GEN_POS		0x174
#define AAL_CABCO_PAT_GEN_CURSOR_RB0	0x178
#define AAL_CABCO_PAT_GEN_CURSOR_RB1	0x17c
#define AAL_CABCO_PAT_GEN_RAMP		0x180
#define AAL_CABCO_PAT_GEN_TILE_POS	0x184
#define AAL_CABCO_PAT_GEN_TILE_OV	0x188
#define AAL_CFG_MAIN			0x200
#define AAL_MAX_HIST_CONFIG_00		0x204
#define AAL_DRE_FLT_FORCE_00		0x358
#define AAL_DRE_FLT_FORCE_01		0x35c
#define AAL_DRE_FLT_FORCE_02		0x360
#define AAL_DRE_FLT_FORCE_03		0x364
#define AAL_DRE_FLT_FORCE_04		0x368
#define AAL_DRE_FLT_FORCE_05		0x36c
#define AAL_DRE_FLT_FORCE_06		0x370
#define AAL_DRE_FLT_FORCE_07		0x374
#define AAL_DRE_FLT_FORCE_08		0x378
#define AAL_DRE_FLT_FORCE_09		0x37c
#define AAL_DRE_FLT_FORCE_10		0x380
#define AAL_DRE_FLT_FORCE_11		0x384
#define AAL_DRE_MAPPING_00		0x3b4
#define AAL_DBG_CFG_MAIN		0x45c
#define AAL_WIN_X_MAIN			0x460
#define AAL_WIN_Y_MAIN			0x464
#define AAL_DRE_BLOCK_INFO_00		0x468
#define AAL_DRE_BLOCK_INFO_01		0x46c
#define AAL_DRE_BLOCK_INFO_02		0x470
#define AAL_DRE_BLOCK_INFO_03		0x474
#define AAL_DRE_BLOCK_INFO_04		0x478
#define AAL_DRE_CHROMA_HIST_00		0x480
#define AAL_DRE_CHROMA_HIST_01		0x484
#define AAL_DRE_ALPHA_BLEND_00		0x488
#define AAL_DRE_BITPLUS_00		0x48c
#define AAL_DRE_BITPLUS_01		0x490
#define AAL_DRE_BITPLUS_02		0x494
#define AAL_DRE_BITPLUS_03		0x498
#define AAL_DRE_BITPLUS_04		0x49c
#define AAL_DRE_BLOCK_INFO_05		0x4b4
#define AAL_DRE_BLOCK_INFO_06		0x4b8
#define AAL_Y2R_00			0x4bc
#define AAL_Y2R_01			0x4c0
#define AAL_Y2R_02			0x4c4
#define AAL_Y2R_03			0x4c8
#define AAL_Y2R_04			0x4cc
#define AAL_Y2R_05			0x4d0
#define AAL_R2Y_00			0x4d4
#define AAL_R2Y_01			0x4d8
#define AAL_R2Y_02			0x4dc
#define AAL_R2Y_03			0x4e0
#define AAL_R2Y_04			0x4e4
#define AAL_R2Y_05			0x4e8
#define AAL_TILE_00			0x4ec
#define AAL_TILE_01			0x4f0
#define AAL_DUAL_PIPE_00		0x500
#define AAL_DUAL_PIPE_01		0x504
#define AAL_DUAL_PIPE_02		0x508
#define AAL_DUAL_PIPE_03		0x50c
#define AAL_DUAL_PIPE_04		0x510
#define AAL_DUAL_PIPE_05		0x514
#define AAL_DUAL_PIPE_06		0x518
#define AAL_DUAL_PIPE_07		0x51c
#define AAL_DRE_ROI_00			0x520
#define AAL_DRE_ROI_01			0x524
#define AAL_DRE_CHROMA_HIST2_00		0x528
#define AAL_DRE_CHROMA_HIST2_01		0x52c
#define AAL_DRE_CHROMA_HIST3_00		0x530
#define AAL_DRE_CHROMA_HIST3_01		0x534
#define AAL_DRE_FLATLINE_DIR		0x538
#define AAL_DRE_BILATERAL		0x53c
#define AAL_DRE_DISP_OUT		0x540
#define AAL_DUAL_PIPE_08		0x544
#define AAL_DUAL_PIPE_09		0x548
#define AAL_DUAL_PIPE_10		0x54c
#define AAL_DUAL_PIPE_11		0x550
#define AAL_DUAL_PIPE_12		0x554
#define AAL_DUAL_PIPE_13		0x558
#define AAL_DUAL_PIPE_14		0x55c
#define AAL_DUAL_PIPE_15		0x560
#define AAL_DRE_BILATERAL_BLENDING	0x564

struct aal_data {
	u32 min_tile_width;
	u32 tile_width;
	u32 min_hist_width;
};

static const struct aal_data mt6893_aal_data = {
	.min_tile_width = 50,
	.tile_width = 560,
	.min_hist_width = 128
};

struct mml_comp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct aal_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct aal_frame_data {
	u8 out_idx;
};

#define aal_frm_data(i)	((struct aal_frame_data *)(i->data))

static inline struct mml_comp_aal *comp_to_aal(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_aal, comp);
}

static s32 aal_prepare(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg)
{
	struct aal_frame_data *aal_frm;

	aal_frm = kzalloc(sizeof(*aal_frm), GFP_KERNEL);
	ccfg->data = aal_frm;
	/* cache out index for easy use */
	aal_frm->out_idx = ccfg->node->out_idx;

	return 0;
}

static s32 aal_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    void *ptr_func, void *tile_data)
{
	TILE_FUNC_BLOCK_STRUCT *func = (TILE_FUNC_BLOCK_STRUCT*)ptr_func;
	struct mml_tile_data *data = (struct mml_tile_data*)tile_data;
	struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_frame_dest *dest = &cfg->info.dest[aal_frm->out_idx];
	struct mml_comp_aal *aal = comp_to_aal(comp);

	data->aal_data.max_width = aal->data->tile_width;
	data->aal_data.min_hist_width = aal->data->min_hist_width;
	data->aal_data.min_width = aal->data->min_tile_width;
	func->func_data = (struct TILE_FUNC_DATA_STRUCT*)(&data->aal_data);

	func->enable_flag = dest->pq_config.en_dre;

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

static const struct mml_comp_tile_ops aal_tile_ops = {
	.prepare = aal_tile_prepare,
};

static s32 aal_init(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_EN, 0x1, 0x00000001);

	return 0;
}

static s32 aal_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	const struct aal_frame_data *aal_frm = aal_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[aal_frm->out_idx];
	const phys_addr_t base_pa = comp->base_pa;

	/* relay mode */
	cmdq_pkt_write(pkt, NULL, base_pa + AAL_CFG, 0x1, 0x00000001);

	if (MML_FMT_10BIT(src->format) ||
	    MML_FMT_10BIT(dest->data.format))
		cmdq_pkt_write(pkt, NULL, base_pa + AAL_CFG_MAIN,
			0, 0x00000080);
	else
		cmdq_pkt_write(pkt, NULL, base_pa + AAL_CFG_MAIN,
			1 << 7, 0x00000080);

	return 0;
}

static s32 aal_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	u32 aal_input_w;
	u32 aal_input_h;
	u32 aal_output_w;
	u32 aal_output_h;
	u32 aal_crop_x_offset;
	u32 aal_crop_y_offset;

	aal_input_w = tile->in.xe - tile->in.xs + 1;
	aal_input_h = tile->in.ye - tile->in.ys + 1;
	aal_output_w = tile->out.xe - tile->out.xs + 1;
	aal_output_h = tile->out.ye - tile->out.ys + 1;
	aal_crop_x_offset = tile->out.xs - tile->in.xs;
	aal_crop_y_offset = tile->out.ys - tile->in.ys;

	cmdq_pkt_write(pkt, NULL, base_pa + AAL_SIZE,
		(aal_input_w << 16) + aal_input_h, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + AAL_OUTPUT_OFFSET,
		(aal_crop_x_offset << 16) + aal_crop_y_offset, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + AAL_OUTPUT_SIZE,
		(aal_output_w << 16) + aal_output_h, U32_MAX);

	return 0;
}

static const struct mml_comp_config_ops aal_cfg_ops = {
	.prepare = aal_prepare,
	.init = aal_init,
	.frame = aal_config_frame,
	.tile = aal_config_tile,
};

static void aal_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[9];

	mml_err("aal component %u dump:", comp->id);

	value[0] = readl(base + AAL_INTSTA);
	value[1] = readl(base + AAL_STATUS);
	value[2] = readl(base + AAL_INPUT_COUNT);
	value[3] = readl(base + AAL_OUTPUT_COUNT);
	value[4] = readl(base + AAL_SIZE);
	value[5] = readl(base + AAL_OUTPUT_SIZE);
	value[6] = readl(base + AAL_OUTPUT_OFFSET);
	value[7] = readl(base + AAL_TILE_00);
	value[8] = readl(base + AAL_TILE_01);

	mml_err("AAL_INTSTA %#010x AAL_STATUS %#010x AAL_INPUT_COUNT %#010x AAL_OUTPUT_COUNT %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("AAL_SIZE %#010x AAL_OUTPUT_SIZE %#010x AAL_OUTPUT_OFFSET %#010x",
		value[4], value[5], value[6]);
	mml_err("AAL_TILE_00 %#010x AAL_TILE_01 %#010x",
		value[7], value[8]);
}

static const struct mml_comp_debug_ops aal_debug_ops = {
	.dump = &aal_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_aal *aal = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 ret = -1, temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		ret = mml_register_comp(master, &aal->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		drm_dev = data;
		ret = mml_ddp_comp_register(drm_dev, &aal->ddp_comp);
		if (ret < 0)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			aal->ddp_bound = true;
	}

	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_aal *aal = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		mml_unregister_comp(master, &aal->comp);
	} else {
		drm_dev = data;
		mml_ddp_comp_unregister(drm_dev, &aal->ddp_comp);
		aal->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_aal *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_aal *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (const struct aal_data *)of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.tile_ops = &aal_tile_ops;
	priv->comp.config_ops = &aal_cfg_ops;
	priv->comp.debug_ops = &aal_debug_ops;

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

const struct of_device_id mtk_mml_aal_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_aal",
		.data = &mt6893_aal_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mml_aal_driver_dt_match);

struct platform_driver mtk_mml_aal_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-aal",
		.owner = THIS_MODULE,
		.of_match_table = mtk_mml_aal_driver_dt_match,
	},
};

//module_platform_driver(mtk_mml_aal_driver);

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
module_param_cb(aal_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(aal_ut_case, "mml aal UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML AAL driver");
MODULE_LICENSE("GPL v2");
