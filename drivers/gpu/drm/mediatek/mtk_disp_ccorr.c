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

#define DISP_REG_CCORR_EN (0x000)
#define DISP_REG_CCORR_CFG (0x020)
#define DISP_REG_CCORR_SIZE (0x030)

struct mtk_disp_ccorr {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
};

static void mtk_ccorr_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_CCORR_SIZE,
		       (cfg->w << 16) | cfg->h, ~0);
}

static void mtk_ccorr_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_CCORR_EN, 0x1, 0x1);
}

static void mtk_ccorr_bypass(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_CCORR_CFG, 0x1, 0x1);
}

static void mtk_ccorr_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_ccorr_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_ccorr_funcs = {
	.config = mtk_ccorr_config,
	.start = mtk_ccorr_start,
	.bypass = mtk_ccorr_bypass,
	.prepare = mtk_ccorr_prepare,
	.unprepare = mtk_ccorr_unprepare,
};

static int mtk_disp_ccorr_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_ccorr *priv = dev_get_drvdata(dev);
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

static void mtk_disp_ccorr_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_ccorr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ccorr_component_ops = {
	.bind = mtk_disp_ccorr_bind, .unbind = mtk_disp_ccorr_unbind,
};

void mtk_ccorr_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, -1);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, -1, -1);
}

static int mtk_disp_ccorr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ccorr *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPPR_ERR("%s\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_CCORR);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ccorr_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_disp_ccorr_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	return ret;
}

static int mtk_disp_ccorr_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_ccorr_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_disp_ccorr_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6779-disp-ccorr",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_ccorr_driver_dt_match);

struct platform_driver mtk_disp_ccorr_driver = {
	.probe = mtk_disp_ccorr_probe,
	.remove = mtk_disp_ccorr_remove,
	.driver = {

			.name = "mediatek-disp-ccorr",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_ccorr_driver_dt_match,
		},
};
