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

#define DMDP_AAL_EN		0x0000
#define DMDP_AAL_CFG		0x0020
#define DMDP_AAL_CFG_MAIN	0x0200
#define DMDP_AAL_SIZE		0x0030
#define DMDP_AAL_OUTPUT_SIZE	0x0034
#define DMDP_AAL_DRE_BILATERAL	0x053C

#define AAL_EN BIT(0)

struct mtk_dmdp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
};

static void mtk_dmdp_aal_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_EN,
		       AAL_EN, ~0);
}

static void mtk_dmdp_aal_stop(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_EN,
		       0x0, ~0);
}

static void mtk_dmdp_aal_bypass(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_EN,
		       AAL_EN, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_CFG,
		       0x400003, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_CFG_MAIN, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_BILATERAL, 0, ~0);
}

static void mtk_dmdp_aal_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int val = (cfg->w << 16) | (cfg->h);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_SIZE,
		       val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DMDP_AAL_OUTPUT_SIZE, val, ~0);

	/* TODO: remove this */
	mtk_dmdp_aal_bypass(comp, handle);
}

static void mtk_dmdp_aal_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_dmdp_aal_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_dmdp_aal_funcs = {
	.config = mtk_dmdp_aal_config,
	.start = mtk_dmdp_aal_start,
	.stop = mtk_dmdp_aal_stop,
	.bypass = mtk_dmdp_aal_bypass,
	.prepare = mtk_dmdp_aal_prepare,
	.unprepare = mtk_dmdp_aal_unprepare,
};

static int mtk_dmdp_aal_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_dmdp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		DDPMSG("Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_dmdp_aal_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_dmdp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_dmdp_aal_component_ops = {
	.bind = mtk_dmdp_aal_bind, .unbind = mtk_dmdp_aal_unbind,
};

void mtk_dmdp_aal_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, 0x4D8);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, -1, -1);
}

static int mtk_dmdp_aal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dmdp_aal *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPMSG("%s\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DMDP_AAL);
	if ((int)comp_id < 0) {
		DDPMSG("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_dmdp_aal_funcs);
	if (ret != 0) {
		DDPMSG("Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_dmdp_aal_component_ops);
	if (ret != 0) {
		DDPMSG("Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	return ret;
}

static int mtk_dmdp_aal_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_dmdp_aal_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_dmdp_aal_driver_dt_match[] = {
	{.compatible = "mediatek,mt6885-dmdp-aal",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_dmdp_aal_driver_dt_match);

struct platform_driver mtk_dmdp_aal_driver = {
	.probe = mtk_dmdp_aal_probe,
	.remove = mtk_dmdp_aal_remove,
	.driver = {

			.name = "mediatek-dmdp-aal",
			.owner = THIS_MODULE,
			.of_match_table = mtk_dmdp_aal_driver_dt_match,
		},
};
