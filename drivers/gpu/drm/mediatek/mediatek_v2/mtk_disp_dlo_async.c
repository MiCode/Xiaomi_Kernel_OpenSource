// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"

/**
 * struct mtk_disp_dlo_async - DISP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_dlo_async {
	struct mtk_ddp_comp ddp_comp;
};

static inline struct mtk_disp_dlo_async *comp_to_dlo_async(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_dlo_async, ddp_comp);
}

static void mtk_dlo_async_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
}

void mtk_dlo_async_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== DISP %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	DDPDUMP("0x0F0: 0x%08x\n", readl(baddr + 0x0F0));
	DDPDUMP("0x27C: 0x%08x\n", readl(baddr + 0x27C));
	DDPDUMP("0x2A8: 0x%08x 0x%08x\n", readl(baddr + 0x2A8),
		readl(baddr + 0x2AC));
}

int mtk_dlo_async_analysis(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	return 0;
}

static void mtk_dlo_async_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	// nothig to do
}

static void mtk_dlo_async_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	// nothig to do
}

static void mtk_dlo_async_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_dlo_async_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_dlo_async_funcs = {
	.start = mtk_dlo_async_start,
	.stop = mtk_dlo_async_stop,
	.addon_config = mtk_dlo_async_addon_config,
	.prepare = mtk_dlo_async_prepare,
	.unprepare = mtk_dlo_async_unprepare,
};

static int mtk_disp_dlo_async_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_dlo_async *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s &priv->ddp_comp:0x%lx\n", __func__, (unsigned long)&priv->ddp_comp);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_dlo_async_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_dlo_async *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_dlo_async_component_ops = {
	.bind = mtk_disp_dlo_async_bind, .unbind = mtk_disp_dlo_async_unbind,
};

static int mtk_disp_dlo_async_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dlo_async *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DLO_ASYNC);
	DDPINFO("comp_id:%d", comp_id);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_dlo_async_funcs);

	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	//priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	DDPINFO("&priv->ddp_comp:0x%lx\n", (unsigned long)&priv->ddp_comp);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_dlo_async_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_dlo_async_remove(struct platform_device *pdev)
{
	struct mtk_disp_dlo_async *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_dlo_async_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_disp_dlo_async_driver_dt_match[] = {
	{.compatible = "mediatek,mt6983-disp-dlo-async3",},
	{.compatible = "mediatek,mt6895-disp-dlo-async3",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_dlo_async_driver_dt_match);

struct platform_driver mtk_disp_dlo_async_driver = {
	.probe = mtk_disp_dlo_async_probe,
	.remove = mtk_disp_dlo_async_remove,
	.driver = {
		.name = "mediatek-disp-dlo-async",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_dlo_async_driver_dt_match,
	},
};
