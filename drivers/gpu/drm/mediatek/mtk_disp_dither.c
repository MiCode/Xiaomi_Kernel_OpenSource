/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_dump.h"

#define DISP_DITHER_EN 0x0
#define DISP_REG_DITHER_CFG 0x20
#define DISP_REG_DITHER_SIZE 0x30
#define DISP_DITHER_5 0x0114
#define DISP_DITHER_7 0x011c
#define DISP_DITHER_15 0x013c
#define DISP_DITHER_16 0x0140

#define DITHER_REG(idx) (0x100 + (idx)*4)

#define DISP_DITHERING BIT(2)
#define DITHER_LSB_ERR_SHIFT_R(x) (((x)&0x7) << 28)
#define DITHER_OVFLW_BIT_R(x) (((x)&0x7) << 24)
#define DITHER_ADD_LSHIFT_R(x) (((x)&0x7) << 20)
#define DITHER_ADD_RSHIFT_R(x) (((x)&0x7) << 16)
#define DITHER_NEW_BIT_MODE BIT(0)
#define DITHER_LSB_ERR_SHIFT_B(x) (((x)&0x7) << 28)
#define DITHER_OVFLW_BIT_B(x) (((x)&0x7) << 24)
#define DITHER_ADD_LSHIFT_B(x) (((x)&0x7) << 20)
#define DITHER_ADD_RSHIFT_B(x) (((x)&0x7) << 16)
#define DITHER_LSB_ERR_SHIFT_G(x) (((x)&0x7) << 12)
#define DITHER_OVFLW_BIT_G(x) (((x)&0x7) << 8)
#define DITHER_ADD_LSHIFT_G(x) (((x)&0x7) << 4)
#define DITHER_ADD_RSHIFT_G(x) (((x)&0x7) << 0)

struct mtk_disp_dither {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	int pwr_sta;
	unsigned int cfg_reg;
};

static void mtk_dither_config(struct mtk_ddp_comp *comp,
			      struct mtk_ddp_config *cfg,
			      struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);

	unsigned int enable = 1;

	DDPINFO("%s: %u\n", __func__, cfg->bpc);

	/* skip redundant config */
	if (priv->pwr_sta != 0)
		return;

	priv->pwr_sta = 1;

	if (cfg->bpc == 8) { /* 888 */
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(15), 0x20200001, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(16), 0x20202020, ~0);
	} else if (cfg->bpc == 5) { /* 565 */
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(15), 0x50500001, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(16), 0x50504040, ~0);
	} else if (cfg->bpc == 6) { /* 666 */
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(15), 0x40400001, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(16), 0x40404040, ~0);
	} else if (cfg->bpc > 8) {
		/* High depth LCM, no need dither */
		;
	} else {
		/* Invalid dither bpp, bypass dither */
		/* FIXME: this case would cause dither hang */
		enable = 0;
	}

	if (enable == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(5), 0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(6), 0x00003002, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(7), 0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(8), 0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(9), 0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(10), 0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(11), 0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(12), 0x00000011, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(13), 0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(14), 0x00000000, ~0);
	}

	priv->cfg_reg = enable << 1 | (priv->cfg_reg & ~(0x1 << 1));

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_DITHER_EN,
		       enable, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_DITHER_CFG, priv->cfg_reg, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_DITHER_SIZE,
		       cfg->w << 16 | cfg->h, ~0);
}

static void mtk_dither_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);

	DDPINFO("%s\n", __func__);

	priv->pwr_sta = 0;
}

static void mtk_dither_bypass(struct mtk_ddp_comp *comp,
			      struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);

	DDPINFO("%s\n", __func__);

	priv->cfg_reg = 0x1 | (priv->cfg_reg & ~0x1);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_DITHER_CFG, priv->cfg_reg, ~0);
}

static void mtk_dither_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_dither_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_dither_funcs = {
	.config = mtk_dither_config,
	.stop = mtk_dither_stop,
	.bypass = mtk_dither_bypass,
	.prepare = mtk_dither_prepare,
	.unprepare = mtk_dither_unprepare,
};

static int mtk_disp_dither_bind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(dev);
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

static void mtk_disp_dither_unbind(struct device *dev, struct device *master,
				   void *data)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_dither_component_ops = {
	.bind = mtk_disp_dither_bind, .unbind = mtk_disp_dither_unbind,
};

void mtk_dither_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, -1);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, -1, -1);
}

static int mtk_disp_dither_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dither *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPPR_ERR("%s\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DITHER);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_dither_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->pwr_sta = 0;
	priv->cfg_reg = 0x80000100;

	platform_set_drvdata(pdev, priv);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_disp_dither_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	return ret;
}

static int mtk_disp_dither_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_dither_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_disp_dither_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6779-disp-dither",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_dither_driver_dt_match);

struct platform_driver mtk_disp_dither_driver = {
	.probe = mtk_disp_dither_probe,
	.remove = mtk_disp_dither_remove,
	.driver = {

			.name = "mediatek-disp-dither",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_dither_driver_dt_match,
		},
};
