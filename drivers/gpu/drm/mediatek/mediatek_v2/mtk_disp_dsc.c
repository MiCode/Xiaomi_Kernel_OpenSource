// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"

#define DISP_REG_DSC_CON			0x0000
	#define DSC_EN BIT(0)
	#define DSC_DUAL_INOUT BIT(2)
	#define DSC_IN_SRC_SEL BIT(3)
	#define DSC_BYPASS BIT(4)
	#define DSC_RELAY BIT(5)
	#define DSC_EMPTY_FLAG_SEL		0xC000
	#define DSC_UFOE_SEL BIT(16)
	#define CON_FLD_DSC_EN		REG_FLD_MSB_LSB(0, 0)
	#define CON_FLD_DISP_DSC_BYPASS		REG_FLD_MSB_LSB(4, 4)

#define DISP_REG_DSC_PIC_W			0x0018
	#define CFG_FLD_PIC_WIDTH	REG_FLD_MSB_LSB(15, 0)
	#define CFG_FLD_PIC_HEIGHT_M1	REG_FLD_MSB_LSB(31, 16)

#define DISP_REG_DSC_PIC_H			0x001C

#define DISP_REG_DSC_SLICE_W		0x0020
	#define CFG_FLD_SLICE_WIDTH	REG_FLD_MSB_LSB(15, 0)

#define DISP_REG_DSC_SLICE_H		0x0024

#define DISP_REG_DSC_CHUNK_SIZE		0x0028

#define DISP_REG_DSC_BUF_SIZE		0x002C

#define DISP_REG_DSC_MODE			0x0030
	#define DSC_SLICE_MODE BIT(0)
	#define DSC_RGB_SWAP BIT(2)
#define DISP_REG_DSC_CFG			0x0034

#define DISP_REG_DSC_PAD			0x0038

#define DISP_REG_DSC_DBG_CON		0x0060
	#define DSC_CKSM_CAL_EN BIT(9)
#define DISP_REG_DSC_OBUF			0x0070
#define DISP_REG_DSC_PPS0			0x0080
#define DISP_REG_DSC_PPS1			0x0084
#define DISP_REG_DSC_PPS2			0x0088
#define DISP_REG_DSC_PPS3			0x008C
#define DISP_REG_DSC_PPS4			0x0090
#define DISP_REG_DSC_PPS5			0x0094
#define DISP_REG_DSC_PPS6			0x0098
#define DISP_REG_DSC_PPS7			0x009C
#define DISP_REG_DSC_PPS8			0x00A0
#define DISP_REG_DSC_PPS9			0x00A4
#define DISP_REG_DSC_PPS10			0x00A8
#define DISP_REG_DSC_PPS11			0x00AC
#define DISP_REG_DSC_PPS12			0x00B0
#define DISP_REG_DSC_PPS13			0x00B4
#define DISP_REG_DSC_PPS14			0x00B8
#define DISP_REG_DSC_PPS15			0x00BC
#define DISP_REG_DSC_PPS16			0x00C0
#define DISP_REG_DSC_PPS17			0x00C4
#define DISP_REG_DSC_PPS18			0x00C8
#define DISP_REG_DSC_PPS19			0x00CC

#define DISP_REG_DSC_SHADOW			0x0200
#define DSC_FORCE_COMMIT(module)	BIT((module)->data->force_commit_bit)
#define DSC_BYPASS_SHADOW	BIT(1)
#define DSC_READ_WORKING	BIT(2)

struct mtk_disp_dsc_data {
	bool need_bypass_shadow;
	unsigned int force_commit_bit;
};


/**
 * struct mtk_disp_dsc - DISP_DSC driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_dsc {
	struct mtk_ddp_comp	 ddp_comp;
	const struct mtk_disp_dsc_data *data;
	int enable;
};

static inline struct mtk_disp_dsc *comp_to_dsc(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_dsc, ddp_comp);
}

static void mtk_dsc_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	void __iomem *baddr = comp->regs;
	struct mtk_disp_dsc *dsc = comp_to_dsc(comp);

	mtk_ddp_write_mask(comp, DSC_FORCE_COMMIT(dsc),
		DISP_REG_DSC_SHADOW, DSC_FORCE_COMMIT(dsc), handle);

	if (dsc->enable) {
		mtk_ddp_write_mask(comp, DSC_EN, DISP_REG_DSC_CON,
				DSC_EN, handle);

		/* DSC Empty flag always high */
		mtk_ddp_write_mask(comp, 0x4000, DISP_REG_DSC_CON,
				DSC_EMPTY_FLAG_SEL, handle);

		/* DSC output buffer as FHD(plus) */
		mtk_ddp_write_mask(comp, 0x800002C2, DISP_REG_DSC_OBUF,
				0xFFFFFFFF, handle);
	}

	DDPINFO("%s, dsc_start:0x%x\n",
		mtk_dump_comp_str(comp), readl(baddr + DISP_REG_DSC_CON));
}

static void mtk_dsc_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	void __iomem *baddr = comp->regs;

	mtk_ddp_write_mask(comp, 0x0, DISP_REG_DSC_CON, DSC_EN, handle);
	DDPINFO("%s, dsc_stop:0x%x\n",
		mtk_dump_comp_str(comp), readl(baddr + DISP_REG_DSC_CON));
}

static void mtk_dsc_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_dsc *dsc = comp_to_dsc(comp);

	mtk_ddp_comp_clk_prepare(comp);

	/* Bypass shadow register and read shadow register */
	if (dsc->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, DSC_BYPASS_SHADOW,
			DISP_REG_DSC_SHADOW, DSC_BYPASS_SHADOW);
}

static void mtk_dsc_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_dsc_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	u32 reg_val;
	struct mtk_disp_dsc *dsc = comp_to_dsc(comp);
	unsigned int pic_group_width, slice_width, slice_height;
	unsigned int pic_height_ext_num, slice_group_width;
	unsigned int bit_per_pixel, chrunk_size, pad_num;
	unsigned int init_delay_limit, init_delay_height_min;
	unsigned int init_delay_height;
	struct mtk_panel_dsc_params *dsc_params;

	if (!comp->mtk_crtc || !comp->mtk_crtc->panel_ext)
		return;

	dsc_params = &comp->mtk_crtc->panel_ext->params->dsc_params;
	if (dsc_params->enable == 1) {
		DDPINFO("%s, w:0x%x, h:0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			mtk_dump_comp_str(comp), cfg->w, cfg->h,
			dsc_params->slice_mode,	dsc_params->slice_width,
			dsc_params->slice_height, dsc_params->bit_per_pixel);

		pic_group_width = (cfg->w + 2)/3;
		slice_width = dsc_params->slice_width;
		slice_height = dsc_params->slice_height;
		pic_height_ext_num = (cfg->h + slice_height - 1) / slice_height;
		slice_group_width = (slice_width + 2)/3;
		/* 128=1/3, 196=1/2 */
		bit_per_pixel = dsc_params->bit_per_pixel;
		chrunk_size = (slice_width*bit_per_pixel/8/16);
		pad_num = (chrunk_size + 2)/3*3 - chrunk_size;

		mtk_ddp_write_mask(comp, 0, DISP_REG_DSC_CON,
				DSC_DUAL_INOUT, handle);
		/* TODO: for dual pipe */
		/* mtk_ddp_write_mask(comp, pConfig->is_dual ?
		 * DSC_IN_SRC_SEL : 0,
		 * DISP_REG_DSC_CON, DSC_IN_SRC_SEL, handle);
		 */

		mtk_ddp_write_mask(comp, DSC_UFOE_SEL,
				DISP_REG_DSC_CON, DSC_UFOE_SEL, handle);
		mtk_ddp_write_mask(comp, 0, DISP_REG_DSC_CON,
				DSC_BYPASS, handle);

		mtk_ddp_write_relaxed(comp,
			(pic_group_width - 1) << 16 | cfg->w,
			DISP_REG_DSC_PIC_W, handle);

		mtk_ddp_write_relaxed(comp,
			(pic_height_ext_num * slice_height - 1) << 16 |
			(cfg->h - 1),
			DISP_REG_DSC_PIC_H, handle);

		mtk_ddp_write_relaxed(comp,
			(slice_group_width - 1) << 16 | slice_width,
			DISP_REG_DSC_SLICE_W, handle);

		mtk_ddp_write_relaxed(comp,
			(slice_group_width - 1) << 16 | slice_width,
			DISP_REG_DSC_SLICE_W, handle);

		mtk_ddp_write_relaxed(comp,
			(slice_width % 3) << 30 |
			(pic_height_ext_num - 1) << 16 |
			(slice_height - 1),
			DISP_REG_DSC_SLICE_H, handle);

		mtk_ddp_write_relaxed(comp, chrunk_size,
			DISP_REG_DSC_CHUNK_SIZE, handle);

		mtk_ddp_write_relaxed(comp,	pad_num,
			DISP_REG_DSC_PAD, handle);

		mtk_ddp_write_relaxed(comp,	chrunk_size * slice_height,
			DISP_REG_DSC_BUF_SIZE, handle);

		init_delay_limit =
			((128 + (dsc_params->xmit_delay + 2) / 3) * 3 +
			dsc_params->slice_width-1) / dsc_params->slice_width;
		init_delay_height_min =
			(init_delay_limit > 15) ? 15 : init_delay_limit;
		init_delay_height = init_delay_height_min;

		reg_val = (!!dsc_params->slice_mode) |
					(!!dsc_params->rgb_swap << 2) |
					(init_delay_height << 8);
		mtk_ddp_write_mask(comp, reg_val,
					DISP_REG_DSC_MODE, 0xFFFF, handle);

		DDPINFO("%s, init delay:0x%x\n",
			mtk_dump_comp_str(comp), reg_val);

		mtk_ddp_write_relaxed(comp,
			(dsc_params->dsc_cfg == 0) ? 0x22 : dsc_params->dsc_cfg,
			DISP_REG_DSC_CFG, handle);

		mtk_ddp_write_relaxed(comp,
			(dsc_params->dsc_cfg == 0) ? 0x22 : dsc_params->dsc_cfg,
			DISP_REG_DSC_CFG, handle);

		mtk_ddp_write_mask(comp, DSC_CKSM_CAL_EN,
					DISP_REG_DSC_DBG_CON, DSC_CKSM_CAL_EN,
					handle);

		mtk_ddp_write_mask(comp,
			(((dsc_params->ver & 0xf) == 2) ? 0x40 : 0x20),
			0x200, 0x60, handle);

		if (dsc_params->dsc_line_buf_depth == 0)
			reg_val = 0x9;
		else
			reg_val = dsc_params->dsc_line_buf_depth;
		if (dsc_params->bit_per_channel == 0)
			reg_val |= (0x8 << 4);
		else
			reg_val |= (dsc_params->bit_per_channel << 4);
		if (dsc_params->bit_per_pixel == 0)
			reg_val |= (0x80 << 8);
		else
			reg_val |= (dsc_params->bit_per_pixel << 8);
		if (dsc_params->rct_on == 0)
			reg_val |= (0x1 << 18);
		else
			reg_val |= (dsc_params->rct_on << 18);
		reg_val |= (dsc_params->bp_enable << 19);
		mtk_ddp_write_relaxed(comp,	reg_val,
			DISP_REG_DSC_PPS0, handle);

		if (dsc_params->xmit_delay == 0)
			reg_val = 0x200;
		else
			reg_val = (dsc_params->xmit_delay);
		if (dsc_params->dec_delay == 0)
			reg_val |= (0x268 << 16);
		else
			reg_val |= (dsc_params->dec_delay << 16);
		mtk_ddp_write_relaxed(comp,	reg_val,
			DISP_REG_DSC_PPS1, handle);

		reg_val = ((dsc_params->scale_value == 0) ?
			0x20 : dsc_params->scale_value);
		reg_val |= ((dsc_params->increment_interval == 0) ?
			0x387 : dsc_params->increment_interval) << 16;
		mtk_ddp_write_relaxed(comp,	reg_val,
			DISP_REG_DSC_PPS2, handle);

		reg_val = ((dsc_params->decrement_interval == 0) ?
			0xa : dsc_params->decrement_interval);
		reg_val |= ((dsc_params->line_bpg_offset == 0) ?
			0xc : dsc_params->line_bpg_offset) << 16;
		mtk_ddp_write_relaxed(comp,	reg_val,
			DISP_REG_DSC_PPS3, handle);

		reg_val = ((dsc_params->nfl_bpg_offset == 0) ?
			0x319 : dsc_params->nfl_bpg_offset);
		reg_val |= ((dsc_params->slice_bpg_offset == 0) ?
			0x263 : dsc_params->slice_bpg_offset) << 16;
		mtk_ddp_write_relaxed(comp,	reg_val,
			DISP_REG_DSC_PPS4, handle);

		reg_val = ((dsc_params->initial_offset == 0) ?
			0x1800 : dsc_params->initial_offset);
		reg_val |= ((dsc_params->final_offset == 0) ?
			0x10f0 : dsc_params->final_offset) << 16;
		mtk_ddp_write_relaxed(comp,	reg_val,
			DISP_REG_DSC_PPS5, handle);

		reg_val = ((dsc_params->flatness_minqp == 0) ?
			0x3 : dsc_params->flatness_minqp);
		reg_val |= ((dsc_params->flatness_maxqp == 0) ?
			0xc : dsc_params->flatness_maxqp) << 8;
		reg_val |= ((dsc_params->rc_model_size == 0) ?
			0x2000 : dsc_params->rc_model_size) << 16;
		mtk_ddp_write_relaxed(comp,	reg_val,
			DISP_REG_DSC_PPS6, handle);

		mtk_ddp_write(comp, 0x20000c03, DISP_REG_DSC_PPS6, handle);
		mtk_ddp_write(comp, 0x330b0b06, DISP_REG_DSC_PPS7, handle);
		mtk_ddp_write(comp, 0x382a1c0e, DISP_REG_DSC_PPS8, handle);
		mtk_ddp_write(comp, 0x69625446, DISP_REG_DSC_PPS9, handle);
		mtk_ddp_write(comp, 0x7b797770, DISP_REG_DSC_PPS10, handle);
		mtk_ddp_write(comp, 0x00007e7d, DISP_REG_DSC_PPS11, handle);
		mtk_ddp_write(comp, 0x00800880, DISP_REG_DSC_PPS12, handle);
		mtk_ddp_write(comp, 0xf8c100a1, DISP_REG_DSC_PPS13, handle);
		mtk_ddp_write(comp, 0xe8e3f0e3, DISP_REG_DSC_PPS14, handle);
		mtk_ddp_write(comp, 0xe103e0e3, DISP_REG_DSC_PPS15, handle);
		mtk_ddp_write(comp, 0xd943e123, DISP_REG_DSC_PPS16, handle);
		mtk_ddp_write(comp, 0xd185d965, DISP_REG_DSC_PPS17, handle);
		mtk_ddp_write(comp, 0xd1a7d1a5, DISP_REG_DSC_PPS18, handle);
		mtk_ddp_write(comp, 0x0000d1ed, DISP_REG_DSC_PPS19, handle);

		dsc->enable = true;
	} else {
		/*enable dsc relay mode*/
		mtk_ddp_write_mask(comp, DSC_RELAY, DISP_REG_DSC_CON,
				DSC_RELAY, handle);
		dsc->enable = false;
	}
}

void mtk_dsc_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));

	DDPDUMP("(0x000)DSC_START=0x%x\n", readl(baddr + DISP_REG_DSC_CON));
	DDPDUMP("(0x000)DSC_SLICE_WIDTH=0x%x\n",
		readl(baddr + DISP_REG_DSC_SLICE_W));
	DDPDUMP("(0x000)DSC_SLICE_HIGHT=0x%x\n",
		readl(baddr + DISP_REG_DSC_SLICE_H));
	DDPDUMP("(0x000)DSC_WIDTH=0x%x\n", readl(baddr + DISP_REG_DSC_PIC_W));
	DDPDUMP("(0x000)DSC_HEIGHT=0x%x\n", readl(baddr + DISP_REG_DSC_PIC_H));
	DDPDUMP("(0x000)DSC_SHADOW=0x%x\n",
		readl(baddr + DISP_REG_DSC_SHADOW));
	DDPDUMP("-- Start dump dsc registers --\n");
	for (i = 0; i < 204; i += 16) {
		DDPDUMP("DSC+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(baddr + i),
			 readl(baddr + i + 0x4), readl(baddr + i + 0x8),
			 readl(baddr + i + 0xc));
	}
}

int mtk_dsc_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("en=%d, pic_w=%d, pic_h=%d, slice_w=%d, bypass=%d\n",
		 DISP_REG_GET_FIELD(CON_FLD_DSC_EN,
				baddr + DISP_REG_DSC_CON),
		 DISP_REG_GET_FIELD(CFG_FLD_PIC_WIDTH,
				baddr + DISP_REG_DSC_PIC_W),
		 DISP_REG_GET_FIELD(CFG_FLD_PIC_HEIGHT_M1,
				baddr + DISP_REG_DSC_PIC_H),
		 DISP_REG_GET_FIELD(CFG_FLD_SLICE_WIDTH,
				baddr + DISP_REG_DSC_SLICE_W),
		 DISP_REG_GET_FIELD(CON_FLD_DISP_DSC_BYPASS,
				baddr + DISP_REG_DSC_CON));

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_dsc_funcs = {
	.config = mtk_dsc_config,
	.start = mtk_dsc_start,
	.stop = mtk_dsc_stop,
	.prepare = mtk_dsc_prepare,
	.unprepare = mtk_dsc_unprepare,
};

static int mtk_disp_dsc_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_dsc *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_dsc_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_disp_dsc *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_dsc_component_ops = {
	.bind = mtk_disp_dsc_bind,
	.unbind = mtk_disp_dsc_unbind,
};

static int mtk_disp_dsc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dsc *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq;
	int ret;

	DDPMSG("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DSC);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_dsc_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_dsc_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPMSG("%s-\n", __func__);
	return ret;
}

static int mtk_disp_dsc_remove(struct platform_device *pdev)
{
	struct mtk_disp_dsc *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_dsc_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_dsc_data mt6885_dsc_driver_data = {
	.need_bypass_shadow = false,
	.force_commit_bit = 1,
};

static const struct mtk_disp_dsc_data mt6873_dsc_driver_data = {
	.need_bypass_shadow = true,
	.force_commit_bit = 0,
};

static const struct mtk_disp_dsc_data mt6853_dsc_driver_data = {
	.need_bypass_shadow = true,
	.force_commit_bit = 0,
};

static const struct of_device_id mtk_disp_dsc_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-disp-dsc",
	  .data = &mt6885_dsc_driver_data},
	{ .compatible = "mediatek,mt6873-disp-dsc",
	  .data = &mt6873_dsc_driver_data},
	{ .compatible = "mediatek,mt6853-disp-dsc",
	  .data = &mt6853_dsc_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_dsc_driver_dt_match);

struct platform_driver mtk_disp_dsc_driver = {
	.probe = mtk_disp_dsc_probe,
	.remove = mtk_disp_dsc_remove,
	.driver = {
		.name = "mediatek-disp-dsc",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_dsc_driver_dt_match,
	},
};
