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

#define HDR_TOP			0x000
#define HDR_RELAY		0x004
#define HDR_INTERR		0x008
#define HDR_INTSTA		0x00c
#define HDR_ENGSTA		0x010
#define HDR_SIZE_0		0x014
#define HDR_SIZE_1		0x018
#define HDR_SIZE_2		0x01c
#define HDR_HIST_CTRL_0		0x020
#define HDR_HIST_CTRL_1		0x024
#define HDR_DEMO_CTRL_0		0x028
#define HDR_DEMO_CTRL_1		0x02c
#define HDR_3x3_COEF_0		0x030
#define HDR_3x3_COEF_1		0x034
#define HDR_3x3_COEF_2		0x038
#define HDR_3x3_COEF_3		0x03c
#define HDR_3x3_COEF_4		0x040
#define HDR_3x3_COEF_5		0x044
#define HDR_3x3_COEF_6		0x048
#define HDR_3x3_COEF_7		0x04c
#define HDR_3x3_COEF_8		0x050
#define HDR_3x3_COEF_9		0x054
#define HDR_3x3_COEF_10		0x058
#define HDR_3x3_COEF_11		0x05c
#define HDR_3x3_COEF_12		0x060
#define HDR_3x3_COEF_13		0x064
#define HDR_3x3_COEF_14		0x068
#define HDR_3x3_COEF_15		0x06c
#define HDR_TONE_MAP_P01	0x070
#define HDR_TONE_MAP_P02	0x074
#define HDR_TONE_MAP_P03	0x078
#define HDR_TONE_MAP_P04	0x07c
#define HDR_TONE_MAP_P05	0x080
#define HDR_TONE_MAP_P06	0x084
#define HDR_TONE_MAP_P07	0x088
#define HDR_TONE_MAP_P08	0x08c
#define HDR_TONE_MAP_S00	0x090
#define HDR_TONE_MAP_S01	0x094
#define HDR_TONE_MAP_S02	0x098
#define HDR_TONE_MAP_S03	0x09c
#define HDR_TONE_MAP_S04	0x0a0
#define HDR_TONE_MAP_S05	0x0a4
#define HDR_TONE_MAP_S06	0x0a8
#define HDR_TONE_MAP_S07	0x0ac
#define HDR_TONE_MAP_S08	0x0b0
#define HDR_TONE_MAP_S09	0x0b4
#define HDR_TONE_MAP_S10	0x0b8
#define HDR_TONE_MAP_S11	0x0bc
#define HDR_TONE_MAP_S12	0x0c0
#define HDR_TONE_MAP_S13	0x0c4
#define HDR_TONE_MAP_S14	0x0c8
#define HDR_TONE_MAP_S15	0x0cc
#define HDR_B_CHANNEL_NR	0x0d0
#define HDR_HIST_ADDR		0x0d4
#define HDR_HIST_DATA		0x0d8
#define HDR_A_LUMINANCE		0x0dc
#define HDR_GAIN_TABLE_0	0x0e0
#define HDR_GAIN_TABLE_1	0x0e4
#define HDR_GAIN_TABLE_2	0x0e8
#define HDR_LBOX_DET_1		0x0f0
#define HDR_LBOX_DET_2		0x0f4
#define HDR_LBOX_DET_3		0x0f8
#define HDR_LBOX_DET_4		0x0fc
#define HDR_CURSOR_CTRL		0x100
#define HDR_CURSOR_POS		0x104
#define HDR_CURSOR_COLOR	0x108
#define HDR_TILE_POS		0x10c
#define HDR_CURSOR_BUF0		0x110
#define HDR_CURSOR_BUF1		0x114
#define HDR_CURSOR_BUF2		0x118
#define HDR_R2Y_00		0x11c
#define HDR_R2Y_01		0x120
#define HDR_R2Y_02		0x124
#define HDR_R2Y_03		0x128
#define HDR_R2Y_04		0x12c
#define HDR_R2Y_05		0x130
#define HDR_R2Y_06		0x134
#define HDR_R2Y_07		0x138
#define HDR_R2Y_08		0x13c
#define HDR_R2Y_09		0x140
#define HDR_Y2R_00		0x144
#define HDR_Y2R_01		0x148
#define HDR_Y2R_02		0x14c
#define HDR_Y2R_03		0x150
#define HDR_Y2R_04		0x154
#define HDR_Y2R_05		0x15c
#define HDR_Y2R_06		0x160
#define HDR_Y2R_07		0x164
#define HDR_Y2R_08		0x168
#define HDR_Y2R_09		0x16c
#define HDR_Y2R_10		0x170
#define HDR_PROG_EOTF_0		0x174
#define HDR_PROG_EOTF_1		0x178
#define HDR_PROG_EOTF_2		0x17c
#define HDR_PROG_EOTF_3		0x180
#define HDR_PROG_EOTF_4		0x184
#define HDR_PROG_EOTF_5		0x188
#define HDR_EOTF_TABLE_0	0x18c
#define HDR_EOTF_TABLE_1	0x190
#define HDR_EOTF_TABLE_2	0x194
#define HDR_OETF_TABLE_0	0x19c
#define HDR_OETF_TABLE_1	0x1a0
#define TONE_MAP_TOP		0x1a4
#define HDR_EOTF_ACCURACY_0	0x1a8
#define HDR_EOTF_ACCURACY_1	0x1ac
#define HDR_EOTF_ACCURACY_2	0x1b0
#define HDR_L_MIX_0		0x1b4
#define HDR_L_MIX_1		0x1b8
#define HDR_L_MIX_2		0x1bc
#define HDR_Y_GAIN_IDX_0	0x1c0
#define HDR_Y_GAIN_IDX_1	0x1c4
#define HDR_DUMMY0		0x1c8
#define HDR_DUMMY1		0x1cc
#define HDR_DUMMY2		0x1d0
#define HDR_HLG_SG		0x1d4

struct hdr_data {
};

static const struct hdr_data mt6893_hdr_data = {
};

struct mml_hdr {
	struct mtk_ddp_comp ddp_comp;
	struct mml_comp comp;
	const struct hdr_data *data;
	bool ddp_bound;
};

/* meta data for each different frame config */
struct hdr_frame_data {
	u8 out_idx;
};

#define hdr_frm_data(i)	((struct hdr_frame_data *)(i->data))

static s32 hdr_prepare(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg)
{
	struct hdr_frame_data *hdr_frm;

	hdr_frm = kzalloc(sizeof(*hdr_frm), GFP_KERNEL);
	ccfg->data = hdr_frm;
	/* cache out index for easy use */
	hdr_frm->out_idx = ccfg->node->out_idx;

	return 0;
}

static s32 hdr_init(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg)
{
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP, 0x1, 0x00000001);

	return 0;
}

static s32 hdr_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	const struct hdr_frame_data *hdr_frm = hdr_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest = &cfg->info.dest[hdr_frm->out_idx];
	const phys_addr_t base_pa = comp->base_pa;

	if (MML_FMT_10BIT(src->format) ||
	    MML_FMT_10BIT(dest->data.format))
		cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP,
			3 << 28, 0x30000000);
	else
		cmdq_pkt_write(pkt, NULL, base_pa + HDR_TOP,
			1 << 28, 0x30000000);

	/* relay mode */
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_RELAY, 0x1, 0x00000001);

	return 0;
}

static s32 hdr_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);
	u32 hdr_input_w;
	u32 hdr_input_h;
	u32 hdr_crop_xs;
	u32 hdr_crop_xe;
	u32 hdr_crop_ye;

	hdr_input_w = tile->in.xe - tile->in.xs + 1;
	hdr_input_h = tile->in.ye - tile->in.ys + 1;
	hdr_crop_xs = tile->out.xs - tile->in.xs;
	hdr_crop_xe = tile->out.xe - tile->in.xs;
	hdr_crop_ye = tile->in.ye - tile->in.ys;

	cmdq_pkt_write(pkt, NULL, base_pa + HDR_TILE_POS,
		(tile->out.ys << 16) + tile->out.xs, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_SIZE_0,
		(hdr_input_h << 16) + hdr_input_w, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_SIZE_1,
		(hdr_crop_xe << 16) + hdr_crop_xs, U32_MAX);
	cmdq_pkt_write(pkt, NULL, base_pa + HDR_SIZE_2,
		hdr_crop_ye << 16, U32_MAX);

	return 0;
}

static const struct mml_comp_config_ops hdr_cfg_ops = {
	.prepare = hdr_prepare,
	.init = hdr_init,
	.frame = hdr_config_frame,
	.tile = hdr_config_tile,
};

static void hdr_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	u32 value[16];

	mml_err("hdr component %u dump:", comp->id);

	value[0] = readl(base + HDR_TOP);
	value[1] = readl(base + HDR_RELAY);
	value[2] = readl(base + HDR_INTSTA);
	value[3] = readl(base + HDR_ENGSTA);
	value[4] = readl(base + HDR_SIZE_0);
	value[5] = readl(base + HDR_SIZE_1);
	value[6] = readl(base + HDR_SIZE_2);
	value[7] = readl(base + HDR_HIST_CTRL_0);
	value[8] = readl(base + HDR_HIST_CTRL_1);
	value[9] = readl(base + HDR_CURSOR_CTRL);
	value[10] = readl(base + HDR_CURSOR_POS);
	value[11] = readl(base + HDR_CURSOR_COLOR);
	value[12] = readl(base + HDR_TILE_POS);
	value[13] = readl(base + HDR_CURSOR_BUF0);
	value[14] = readl(base + HDR_CURSOR_BUF1);
	value[15] = readl(base + HDR_CURSOR_BUF2);

	mml_err("HDR_TOP %#010x HDR_RELAY %#010x HDR_INTSTA %#010x HDR_ENGSTA %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("HDR_SIZE_0 %#010x HDR_SIZE_1 %#010x HDR_SIZE_2 %#010x",
		value[4], value[5], value[6]);
	mml_err("HDR_HIST_CTRL_0 %#010x HDR_HIST_CTRL_1 %#010x",
		value[7], value[8]);
	mml_err("HDR_CURSOR_CTRL %#010x HDR_CURSOR_POS %#010x HDR_CURSOR_COLOR %#010x",
		value[9], value[10], value[11]);
	mml_err("HDR_TILE_POS %#010x", value[12]);
	mml_err("HDR_CURSOR_BUF0 %#010x HDR_CURSOR_BUF1 %#010x HDR_CURSOR_BUF2 %#010x",
		value[13], value[14], value[15]);
}

static const struct mml_comp_debug_ops hdr_debug_ops = {
	.dump = &hdr_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_hdr *hdr = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 ret = -1, temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		ret = mml_register_comp(master, &hdr->comp);
		if (ret)
			dev_err(dev, "Failed to register mml component %s: %d\n",
				dev->of_node->full_name, ret);
	} else {
		drm_dev = data;
		ret = mml_ddp_comp_register(drm_dev, &hdr->ddp_comp);
		if (ret < 0)
			dev_err(dev, "Failed to register ddp component %s: %d\n",
				dev->of_node->full_name, ret);
		else
			hdr->ddp_bound = true;
	}

	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_hdr *hdr = dev_get_drvdata(dev);
	struct drm_device *drm_dev = NULL;
	bool mml_master = false;
	s32 temp;

	if (!of_property_read_u32(master->of_node, "comp-count", &temp))
		mml_master = true;

	if (mml_master) {
		mml_unregister_comp(master, &hdr->comp);
	} else {
		drm_dev = data;
		mml_ddp_comp_unregister(drm_dev, &hdr->ddp_comp);
		hdr->ddp_bound = false;
	}
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_hdr *dbg_probed_components[2];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_hdr *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = (const struct hdr_data *)of_device_get_match_data(dev);

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* assign ops */
	priv->comp.config_ops = &hdr_cfg_ops;
	priv->comp.debug_ops = &hdr_debug_ops;

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

const struct of_device_id mtk_mml_hdr_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6893-mml_hdr",
		.data = &mt6893_hdr_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mml_hdr_driver_dt_match);

struct platform_driver mtk_mml_hdr_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-hdr",
		.owner = THIS_MODULE,
		.of_match_table = mtk_mml_hdr_driver_dt_match,
	},
};

//module_platform_driver(mtk_mml_hdr_driver);

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
module_param_cb(hdr_ut_case, &up_param_ops, NULL, 0644);
MODULE_PARM_DESC(hdr_ut_case, "mml hdr UT test case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML HDR driver");
MODULE_LICENSE("GPL v2");
