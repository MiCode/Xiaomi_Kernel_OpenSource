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

#define TDSHP_00		0x000
#define TDSHP_01		0x004
#define TDSHP_02		0x008
#define TDSHP_03		0x00c
#define TDSHP_05		0x014
#define TDSHP_06		0x018
#define TDSHP_07		0x01c
#define TDSHP_08		0x020
#define TDSHP_09		0x024
#define PBC_00			0x040
#define PBC_01			0x044
#define PBC_02			0x048
#define PBC_03			0x04c
#define PBC_04			0x050
#define PBC_05			0x054
#define PBC_06			0x058
#define PBC_07			0x05c
#define PBC_08			0x060
#define HIST_CFG_00		0x064
#define HIST_CFG_01		0x068
#define LUMA_HIST_00		0x06c
#define LUMA_HIST_01		0x070
#define LUMA_HIST_02		0x074
#define LUMA_HIST_03		0x078
#define LUMA_HIST_04		0x07c
#define LUMA_HIST_05		0x080
#define LUMA_HIST_06		0x084
#define LUMA_HIST_07		0x08c
#define LUMA_HIST_08		0x090
#define LUMA_HIST_09		0x094
#define LUMA_HIST_10		0x098
#define LUMA_HIST_11		0x09c
#define LUMA_HIST_12		0x0a0
#define LUMA_HIST_13		0x0a4
#define LUMA_HIST_14		0x0a8
#define LUMA_HIST_15		0x0ac
#define LUMA_HIST_16		0x0b0
#define LUMA_SUM		0x0b4
#define Y_FTN_1_0_MAIN		0x0bc
#define Y_FTN_3_2_MAIN		0x0c0
#define Y_FTN_5_4_MAIN		0x0c4
#define Y_FTN_7_6_MAIN		0x0c8
#define Y_FTN_9_8_MAIN		0x0cc
#define Y_FTN_11_10_MAIN	0x0d0
#define Y_FTN_13_12_MAIN	0x0d4
#define Y_FTN_15_14_MAIN	0x0d8
#define Y_FTN_17_16_MAIN	0x0dc
#define C_BOOST_MAIN		0x0e0
#define C_BOOST_MAIN_2		0x0e4
#define TDSHP_C_BOOST_MAIN	0x0e8
#define TDSHP_C_BOOST_MAIN_2	0x0ec
#define TDSHP_ATPG		0x0fc
#define TDSHP_CTRL		0x100
#define TDSHP_INTEN		0x104
#define TDSHP_INTSTA		0x108
#define TDSHP_STATUS		0x10c
#define TDSHP_CFG		0x110
#define TDSHP_INPUT_COUNT	0x114
#define TDSHP_CHKSUM		0x118
#define TDSHP_OUTPUT_COUNT	0x11c
#define TDSHP_INPUT_SIZE	0x120
#define TDSHP_OUTPUT_OFFSET	0x124
#define TDSHP_OUTPUT_SIZE	0x128
#define TDSHP_BLANK_WIDTH	0x12c
#define TDSHP_DEMO_HMASK	0x130
#define TDSHP_DEMO_VMASK	0x134
#define TDSHP_DUMMY_REG		0x14c
#define LUMA_HIST_INIT_00	0x200
#define LUMA_HIST_INIT_01	0x204
#define LUMA_HIST_INIT_02	0x208
#define LUMA_HIST_INIT_03	0x20c
#define LUMA_HIST_INIT_04	0x210
#define LUMA_HIST_INIT_05	0x214
#define LUMA_HIST_INIT_06	0x218
#define LUMA_HIST_INIT_07	0x21c
#define LUMA_HIST_INIT_08	0x220
#define LUMA_HIST_INIT_09	0x224
#define LUMA_HIST_INIT_10	0x228
#define LUMA_HIST_INIT_11	0x22c
#define LUMA_HIST_INIT_12	0x230
#define LUMA_HIST_INIT_13	0x234
#define LUMA_HIST_INIT_14	0x238
#define LUMA_HIST_INIT_15	0x23c
#define LUMA_HIST_INIT_16	0x240
#define LUMA_SUM_INIT		0x244
#define DC_DBG_CFG_MAIN		0x250
#define DC_WIN_X_MAIN		0x254
#define DC_WIN_Y_MAIN		0x258
#define DC_TWO_D_W1		0x25c
#define DC_TWO_D_W1_RESULT_INIT	0x260
#define DC_TWO_D_W1_RESULT	0x264
#define EDF_GAIN_00		0x300
#define EDF_GAIN_01		0x304
#define EDF_GAIN_02		0x308
#define EDF_GAIN_03		0x30c
#define EDF_GAIN_04		0x310
#define EDF_GAIN_05		0x314
#define TDSHP_10		0x320
#define TDSHP_11		0x324
#define TDSHP_12		0x328
#define TDSHP_13		0x32c
#define PAT1_GEN_SET		0x330
#define PAT1_GEN_FRM_SIZE	0x334
#define PAT1_GEN_COLOR0		0x338
#define PAT1_GEN_COLOR1		0x33c
#define PAT1_GEN_COLOR2		0x340
#define PAT1_GEN_POS		0x344
#define PAT1_GEN_TILE_POS	0x354
#define PAT1_GEN_TILE_OV	0x358
#define PAT2_GEN_SET		0x360
#define PAT2_GEN_COLOR0		0x368
#define PAT2_GEN_COLOR1		0x36c
#define PAT2_GEN_POS		0x374
#define PAT2_GEN_CURSOR_RB0	0x378
#define PAT2_GEN_CURSOR_RB1	0x37c
#define PAT2_GEN_TILE_POS	0x384
#define PAT2_GEN_TILE_OV	0x388
#define BITPLUS_00		0x38c
#define BITPLUS_01		0x390
#define BITPLUS_02		0x394
#define DC_SKIN_RANGE0		0x420
#define CONTOUR_HIST_INIT_00	0x398
#define CONTOUR_HIST_INIT_01	0x39c
#define CONTOUR_HIST_INIT_02	0x3a0
#define CONTOUR_HIST_INIT_03	0x3a4
#define CONTOUR_HIST_INIT_04	0x3a8
#define CONTOUR_HIST_INIT_05	0x3ac
#define CONTOUR_HIST_INIT_06	0x3b0
#define CONTOUR_HIST_INIT_07	0x3b4
#define CONTOUR_HIST_INIT_08	0x3b8
#define CONTOUR_HIST_INIT_09	0x3bc
#define CONTOUR_HIST_INIT_10	0x3c0
#define CONTOUR_HIST_INIT_11	0x3c4
#define CONTOUR_HIST_INIT_12	0x3c8
#define CONTOUR_HIST_INIT_13	0x3cc
#define CONTOUR_HIST_INIT_14	0x3d0
#define CONTOUR_HIST_INIT_15	0x3d4
#define CONTOUR_HIST_INIT_16	0x3d8
#define CONTOUR_HIST_00		0x3dc
#define CONTOUR_HIST_01		0x3e0
#define CONTOUR_HIST_02		0x3e4
#define CONTOUR_HIST_03		0x3e8
#define CONTOUR_HIST_04		0x3ec
#define CONTOUR_HIST_05		0x3f0
#define CONTOUR_HIST_06		0x3f4
#define CONTOUR_HIST_07		0x3f8
#define CONTOUR_HIST_08		0x3fc
#define CONTOUR_HIST_09		0x400
#define CONTOUR_HIST_10		0x404
#define CONTOUR_HIST_11		0x408
#define CONTOUR_HIST_12		0x40c
#define CONTOUR_HIST_13		0x410
#define CONTOUR_HIST_14		0x414
#define CONTOUR_HIST_15		0x418
#define CONTOUR_HIST_16		0x41c
#define DC_SKIN_RANGE1		0x424
#define DC_SKIN_RANGE2		0x428
#define DC_SKIN_RANGE3		0x42c
#define DC_SKIN_RANGE4		0x430
#define DC_SKIN_RANGE5		0x434
#define POST_YLEV_00		0x480
#define POST_YLEV_01		0x484
#define POST_YLEV_02		0x488
#define POST_YLEV_03		0x48c
#define POST_YLEV_04		0x490
#define HFG_CTRL		0x500
#define HFG_RAN_0		0x504
#define HFG_RAN_1		0x508
#define HFG_RAN_2		0x50c
#define HFG_RAN_3		0x510
#define HFG_RAN_4		0x514
#define HFG_CROP_X		0x518
#define HFG_CROP_Y		0x51c
#define HFC_CON_0		0x524
#define HFC_LUMA_0		0x528
#define HFC_LUMA_1		0x52c
#define HFC_LUMA_2		0x530
#define HFC_SL2_0		0x534
#define HFC_SL2_1		0x538
#define HFC_SL2_2		0x53c
#define SL2_CEN			0x544
#define SL2_RR_CON0		0x548
#define SL2_RR_CON1		0x54c
#define SL2_GAIN		0x550
#define SL2_RZ			0x554
#define SL2_XOFF		0x558
#define SL2_YOFF		0x55c
#define SL2_SLP_CON0		0x560
#define SL2_SLP_CON1		0x564
#define SL2_SLP_CON2		0x568
#define SL2_SLP_CON3		0x66c
#define SL2_SIZE		0x670
#define HFG_OUTPUT_COUNT	0x678

struct tdshp_data {
};

static const struct tdshp_data mt6893_tdshp_data = {
};

struct mml_tdshp {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct tdshp_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct tdshp_frame_data {
	u8 out_idx;
};

#define tdshp_frm_data(i)	((struct tdshp_frame_data *)(i->data))

static s32 tdshp_prepare(struct mml_comp *comp, struct mml_task *task,
			 struct mml_comp_config *ccfg)
{
	struct tdshp_frame_data *tdshp_frm;

	tdshp_frm = kzalloc(sizeof(*tdshp_frm), GFP_KERNEL);
	ccfg->data = tdshp_frm;
	/* cache out index for easy use */
	tdshp_frm->out_idx = ccfg->node->out_idx;

	return 0;
}

static s32 tdshp_init(struct mml_comp *comp, struct mml_task *task,
		      struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CTRL, 1, 0x00000001);
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CFG, 0x2, 0x00000002);

	/* reset luma hist */
	return 0;
}

static s32 tdshp_config_frame(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	const struct tdshp_frame_data *tdshp_frm = tdshp_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[tdshp_frm->out_idx];

	const phys_addr_t base_pa = comp->base_pa;

	/* relay mode */
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CFG, 0x1, 0x00000001);
	cmdq_pkt_write(pkt, NULL, base_pa + HFG_CTRL, 0, 0x00000101);

	if (MML_FMT_10BIT(src->format) ||
	    MML_FMT_10BIT(dest->data.format))
		cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CTRL, 0, 0x00000004);
	else
		cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_CTRL,
			1 << 2, 0x00000004);

	return 0;
}

static s32 tdshp_config_tile(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 tdshp_input_w;
	u32 tdshp_input_h;
	u32 tdshp_output_w;
	u32 tdshp_output_h;
	u32 tdshp_crop_x_offset;
	u32 tdshp_crop_y_offset;
	u32 tdshp_hist_left;
	u32 tdshp_hist_top;

	tdshp_input_w = tile->in.xe - tile->in.xs + 1;
	tdshp_input_h = tile->in.ye - tile->in.ys + 1;
	tdshp_output_w = tile->out.xe - tile->out.xs + 1;
	tdshp_output_h = tile->out.ye - tile->out.ys + 1;
	tdshp_crop_x_offset = tile->out.xs - tile->in.xs;
	tdshp_crop_y_offset = tile->out.ys - tile->in.ys;
	/* TODO: need official implementation */
	tdshp_hist_left = 0xffff;
	tdshp_hist_top = 0xffff;

	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_INPUT_SIZE,
		(tdshp_input_w << 16) + tdshp_input_h, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_OUTPUT_OFFSET,
		(tdshp_crop_x_offset << 16) + tdshp_crop_y_offset, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + TDSHP_OUTPUT_SIZE,
		(tdshp_output_w << 16) + tdshp_output_h, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + HIST_CFG_00,
		((tile->out.xe - tile->in.xs) << 16) +
		(tdshp_hist_left - tile->in.xs), U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HIST_CFG_01,
		((tile->out.ye - tile->in.ys) << 16) +
		(tdshp_hist_top - tile->in.ys), U32_MAX);

	return 0;
}

static const struct mml_comp_config_ops tdshp_cfg_ops = {
	.prepare = tdshp_prepare,
	.init = tdshp_init,
	.frame = tdshp_config_frame,
	.tile = tdshp_config_tile,
};

static void tdshp_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[10];

	mml_err("tdshp component %u dump:", comp->id);

	value[0] = readl(base + TDSHP_INTEN);
	value[1] = readl(base + TDSHP_INTSTA);
	value[2] = readl(base + TDSHP_STATUS);
	value[3] = readl(base + TDSHP_CFG);
	value[4] = readl(base + TDSHP_INPUT_COUNT);
	value[5] = readl(base + TDSHP_OUTPUT_COUNT);
	value[6] = readl(base + TDSHP_INPUT_SIZE);
	value[7] = readl(base + TDSHP_OUTPUT_OFFSET);
	value[8] = readl(base + TDSHP_OUTPUT_SIZE);
	value[9] = readl(base + TDSHP_BLANK_WIDTH);

	mml_err("TDSHP_INTEN %#010x TDSHP_INTSTA %#010x TDSHP_STATUS %#010x TDSHP_CFG %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("TDSHP_INPUT_COUNT %#010x TDSHP_OUTPUT_COUNT %#010x",
		value[4], value[5]);
	mml_err("TDSHP_INPUT_SIZE %#010x TDSHP_OUTPUT_OFFSET %#010x TDSHP_OUTPUT_SIZE %#010x",
		value[6], value[7], value[8]);
	mml_err("TDSHP_BLANK_WIDTH %#010x", value[9]);
}

static const struct mml_comp_debug_ops tdshp_debug_ops = {
	.dump = &tdshp_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_tdshp *tdshp = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 ret = -1, temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		ret = mml_register_comp(master, &tdshp->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		drm_dev = data;
		ret = mml_ddp_comp_register(drm_dev, &tdshp->ddp_comp);
		if (ret < 0)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			tdshp->ddp_bound = true;
	}

	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_tdshp *tdshp = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		mml_unregister_comp(master, &tdshp->comp);
	} else {
		drm_dev = data;
		mml_ddp_comp_unregister(drm_dev, &tdshp->ddp_comp);
		tdshp->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_tdshp *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_tdshp *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (const struct tdshp_data *)of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.config_ops = &tdshp_cfg_ops;
	priv->comp.debug_ops = &tdshp_debug_ops;

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

const struct of_device_id mtk_mml_tdshp_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_tdshp",
		.data = &mt6893_tdshp_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mml_tdshp_driver_dt_match);

struct platform_driver mtk_mml_tdshp_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-tdshp",
		.owner = THIS_MODULE,
		.of_match_table = mtk_mml_tdshp_driver_dt_match,
	},
};

//module_platform_driver(mtk_mml_tdshp_driver);

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
module_param_cb(tdshp_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(tdshp_ut_case, "mml tdshp UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML TDSHP driver");
MODULE_LICENSE("GPL v2");
