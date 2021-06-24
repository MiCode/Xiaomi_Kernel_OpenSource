/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/clk.h>
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

#define RSZ_ENABLE			0x000
#define RSZ_CON_1			0x004
#define RSZ_CON_2			0x008
#define RSZ_INT_FLAG			0x00c
#define RSZ_INPUT_IMAGE			0x010
#define RSZ_OUTPUT_IMAGE		0x014
#define RSZ_HOR_COEFF_STEP		0x018
#define RSZ_VER_COEFF_STEP		0x01c
#define RSZ_LUMA_HOR_INT_OFFSET		0x020
#define RSZ_LUMA_HOR_SUB_OFFSET		0x024
#define RSZ_LUMA_VER_INT_OFFSET		0x028
#define RSZ_LUMA_VER_SUB_OFFSET		0x02c
#define RSZ_CHROMA_HOR_INT_OFFSET	0x030
#define RSZ_CHROMA_HOR_SUB_OFFSET	0x034
#define RSZ_RSV				0x040
#define RSZ_DEBUG_SEL			0x044
#define RSZ_DEBUG			0x048
#define RSZ_TAP_ADAPT			0x04c
#define RSZ_IBSE_SOFTCLIP		0x050
#define RSZ_IBSE_YLEVEL_1		0x054
#define RSZ_IBSE_YLEVEL_2		0x058
#define RSZ_IBSE_YLEVEL_3		0x05c
#define RSZ_IBSE_YLEVEL_4		0x060
#define RSZ_IBSE_YLEVEL_5		0x064
#define RSZ_IBSE_GAINCON_1		0x068
#define RSZ_IBSE_GAINCON_2		0x06c
#define RSZ_DEMO_IN_HMASK		0x070
#define RSZ_DEMO_IN_VMASK		0x074
#define RSZ_DEMO_OUT_HMASK		0x078
#define RSZ_DEMO_OUT_VMASK		0x07c
#define RSZ_ATPG			0x0fc
#define RSZ_PAT1_GEN_SET		0x100
#define RSZ_PAT1_GEN_FRM_SIZE		0x104
#define RSZ_PAT1_GEN_COLOR0		0x108
#define RSZ_PAT1_GEN_COLOR1		0x10c
#define RSZ_PAT1_GEN_COLOR2		0x110
#define RSZ_PAT1_GEN_POS		0x114
#define RSZ_PAT1_GEN_TILE_POS		0x124
#define RSZ_PAT1_GEN_TILE_OV		0x128
#define RSZ_PAT2_GEN_SET		0x200
#define RSZ_PAT2_GEN_COLOR0		0x208
#define RSZ_PAT2_GEN_COLOR1		0x20c
#define RSZ_PAT2_GEN_POS		0x214
#define RSZ_PAT2_GEN_CURSOR_RB0		0x218
#define RSZ_PAT2_GEN_CURSOR_RB1		0x21c
#define RSZ_PAT2_GEN_TILE_POS		0x224
#define RSZ_PAT2_GEN_TILE_OV		0x228
#define RSZ_ETC_CONTROL			0x22c
#define RSZ_ETC_SWITCH_MAX_MIN_1	0x230
#define RSZ_ETC_SWITCH_MAX_MIN_2	0x234
#define RSZ_ETC_RING			0x238
#define RSZ_ETC_RING_GAINCON_1		0x23c
#define RSZ_ETC_RING_GAINCON_2		0x240
#define RSZ_ETC_RING_GAINCON_3		0x244
#define RSZ_ETC_SIM_PROT_GAINCON_1	0x248
#define RSZ_ETC_SIM_PROT_GAINCON_2	0x24c
#define RSZ_ETC_SIM_PROT_GAINCON_3	0x250
#define RSZ_ETC_BLEND			0x254

struct rsz_data {
};

static const struct rsz_data mt6893_rsz_data = {
};

struct mml_rsz {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct rsz_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct rsz_frame_data {
	/* 0 or 1 for 1st or 2nd out port */
	u8 out_idx;

	bool use121filter:1;
};

#define rsz_frm_data(i)	((struct rsz_frame_data *)(i->data))

static s32 rsz_config_scale(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct rsz_frame_data *rsz_frm;

	rsz_frm = kzalloc(sizeof(*rsz_frm), GFP_KERNEL);
	ccfg->data = rsz_frm;
	/* cache out index for easy use */
	rsz_frm->out_idx = ccfg->node->out_idx;

	return 0;
}

static s32 rsz_init(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0x10000, 0x00010000);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0, 0x00010000);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0x1, 0x00000001);
	return 0;
}

static s32 rsz_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_info *frame_info = &cfg->info;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct rsz_frame_data *rsz_frm = rsz_frm_data(ccfg);
	const struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[rsz_frm->out_idx];
	u32 out_width, out_height;

	if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270) {
		out_width = dest->data.height;
		out_height = dest->data.width;
	} else {
		out_width = dest->data.width;
		out_height = dest->data.height;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ETC_CONTROL, 0x0, U32_MAX);

	if (frame_info->dest_cnt == 1 &&
	    dest->crop.r.width == src->width &&
	    src->width == out_width &&
	    dest->crop.r.height == src->height &&
	    src->height == out_height &&
	    dest->crop.x_sub_px == 0 && dest->crop.y_sub_px == 0) {
		/* relay mode */
		cmdq_pkt_write(pkt, NULL, base_pa + RSZ_ENABLE, 0, 0x00000001);
		return 0;
	}

	rsz_frm->use121filter = !MML_FMT_H_SUBSAMPLE(src->format);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_CON_1,
		       rsz_frm->use121filter << 26, 0x04000000);
	return 0;
}

static s32 rsz_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	/* frame data should not change between each tile */
	const struct rsz_frame_data *rsz_frm = rsz_frm_data(ccfg);
	const struct mml_frame_dest *dest = &cfg->info.dest[rsz_frm->out_idx];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u32 drs_lclip_en;
	u32 drs_padding_dis;
	u32 urs_clip_en;
	u32 rsz_input_w;
	u32 rsz_input_h;
	u32 rsz_output_w;
	u32 rsz_output_h;

	if (!(tile->in.xe & 0x1))
		/* Odd coordinate, should pad 1 column */
		drs_padding_dis = 0;
	else
		/* Even coordinate, no padding required */
		drs_padding_dis = 1;

	if (rsz_frm->use121filter && tile->in.xs)
		drs_lclip_en = 1;
	else
		drs_lclip_en = 0;

	rsz_input_w = tile->in.xe - tile->in.xs + 1;
	rsz_input_h = tile->in.ye - tile->in.ys + 1;
	rsz_output_w = tile->out.xe - tile->out.xs + 1;
	rsz_output_h = tile->out.ye - tile->out.ys + 1;

	/* YUV422 to YUV444 upsampler */
	if (dest->rotate == MML_ROT_90 ||
	    dest->rotate == MML_ROT_270) {
		if (tile->out.xe >= dest->data.height- 1)
			urs_clip_en = 0;
		else
			urs_clip_en = 1;
	} else {
		if (tile->out.xe >= dest->data.width - 1)
			urs_clip_en = 0;
		else
			urs_clip_en = 1;
	}

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_CON_2,
		       (drs_lclip_en << 11) +
		       (drs_padding_dis << 12) +
		       (urs_clip_en << 13), 0x00003800);

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_INPUT_IMAGE,
		       (rsz_input_h << 16) + rsz_input_w, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_LUMA_HOR_INT_OFFSET,
		       tile->luma.x, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_LUMA_HOR_SUB_OFFSET,
		       tile->luma.x_sub, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_LUMA_VER_INT_OFFSET,
		       tile->luma.y, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_LUMA_VER_SUB_OFFSET,
		       tile->luma.y_sub, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_CHROMA_HOR_INT_OFFSET,
		       tile->chroma.x, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_CHROMA_HOR_SUB_OFFSET,
		       tile->chroma.x_sub, U32_MAX);

	cmdq_pkt_write(pkt, NULL, base_pa + RSZ_OUTPUT_IMAGE,
		       (rsz_output_h << 16) + rsz_output_w, U32_MAX);

	return 0;
}

static const struct mml_comp_config_ops rsz_cfg_ops = {
	.prepare = rsz_config_scale,
	.init = rsz_init,
	.frame = rsz_config_frame,
	.tile = rsz_config_tile,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_rsz *rsz = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 ret = -1, temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		ret = mml_register_comp(master, &rsz->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		drm_dev = data;
		ret = mml_ddp_comp_register(drm_dev, &rsz->ddp_comp);
		if (ret < 0)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			rsz->ddp_bound = true;
	}

	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_rsz *rsz = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		mml_unregister_comp(master, &rsz->comp);
	} else {
		drm_dev = data;
		mml_ddp_comp_unregister(drm_dev, &rsz->ddp_comp);
		rsz->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_rsz *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_rsz *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (const struct rsz_data *)of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.config_ops = &rsz_cfg_ops;

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

const struct of_device_id mtk_mml_rsz_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_rsz",
		.data = &mt6893_rsz_data
	},
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
module_param_cb(rsz_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(rsz_ut_case, "mml rsz UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML RSZ driver");
MODULE_LICENSE("GPL v2");
