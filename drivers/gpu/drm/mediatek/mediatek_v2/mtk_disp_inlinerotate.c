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

#define DISP_REG_DISPSYS_SHADOW_CTRL	0x10
#define DISP_REG_INLINEROT_SWRST		0x20
#define DISP_REG_INLINEROT_OVLSEL		0x30
#define DISP_REG_INLINEROT_HEIGHT0		0x34
#define DISP_REG_INLINEROT_HEIGHT1		0x38
#define DISP_REG_INLINEROT_WDONE		0x3C

/**
 * struct mtk_disp_inlinerotate - DISP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_inlinerotate {
	struct mtk_ddp_comp ddp_comp;
};

static inline struct mtk_disp_inlinerotate *comp_to_inlinerotate(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_inlinerotate, ddp_comp);
}

static void mtk_inlinerotate_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	/* config inlinerot only when the first IR frame, bypass addon_connect */
	if (addon_config)
		return;

	DDPINFO("%s+ handle:0x%x, comp->regs_pa:0x%x\n",
		__func__, handle, comp->regs_pa);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_DISPSYS_SHADOW_CTRL,
		0x00000002, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_INLINEROT_OVLSEL,
		12, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_INLINEROT_HEIGHT0,
		64, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_INLINEROT_HEIGHT1,
		64, ~0);

	DDPINFO("comp->regs_pa:%llx", comp->regs_pa);
	DDPINFO("%s -\n", __func__);
}

void mtk_inlinerotate_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== DISP %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	DDPDUMP("DISP_REG_DISPSYS_SHADOW_CTRL  0x%08x: 0x%08x\n",
		DISP_REG_DISPSYS_SHADOW_CTRL,
		readl(baddr + DISP_REG_DISPSYS_SHADOW_CTRL));
	DDPDUMP("DISP_REG_INLINEROT_SWRST  0x%08x: 0x%08x\n",
		DISP_REG_INLINEROT_SWRST,
		readl(baddr + DISP_REG_INLINEROT_SWRST));
	DDPDUMP("DISP_REG_INLINEROT_OVLSEL  0x%08x: 0x%08x\n",
		DISP_REG_INLINEROT_OVLSEL,
		readl(baddr + DISP_REG_INLINEROT_OVLSEL));
	DDPDUMP("DISP_REG_INLINEROT_HEIGHT0  0x%08x: 0x%08x\n",
		DISP_REG_INLINEROT_HEIGHT0,
		readl(baddr + DISP_REG_INLINEROT_HEIGHT0));
	DDPDUMP("DISP_REG_INLINEROT_HEIGHT1  0x%08x: 0x%08x\n",
		DISP_REG_INLINEROT_HEIGHT1,
		readl(baddr + DISP_REG_INLINEROT_HEIGHT1));
	DDPDUMP("DISP_REG_INLINEROT_WDONE  0x%08x: 0x%08x\n",
		DISP_REG_INLINEROT_WDONE,
		readl(baddr + DISP_REG_INLINEROT_WDONE));
}

int mtk_inlinerotate_analysis(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	return 0;
}

static void mtk_inlinerotate_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	/* cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_INLINEROT_SWRST,
		0, ~0);
	*/
}

static void mtk_inlinerotate_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	/* cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_INLINEROT_SWRST,
		1, ~0);
	*/
}

static void mtk_inlinerotate_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_inlinerotate_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_inlinerotate_funcs = {
	.start = mtk_inlinerotate_start,
	.stop = mtk_inlinerotate_stop,
	.addon_config = mtk_inlinerotate_addon_config,
	.prepare = mtk_inlinerotate_prepare,
	.unprepare = mtk_inlinerotate_unprepare,
};

static int mtk_disp_inlinerotate_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_inlinerotate *priv = dev_get_drvdata(dev);
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

static void mtk_disp_inlinerotate_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_inlinerotate *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_inlinerotate_component_ops = {
	.bind = mtk_disp_inlinerotate_bind, .unbind = mtk_disp_inlinerotate_unbind,
};

static int mtk_disp_inlinerotate_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_inlinerotate *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_INLINE_ROTATE);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_inlinerotate_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	//priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_inlinerotate_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_inlinerotate_remove(struct platform_device *pdev)
{
	struct mtk_disp_inlinerotate *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_inlinerotate_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_disp_inlinerotate_driver_dt_match[] = {
	{.compatible = "mediatek,mt6983-disp-inlinerotate",},
	{.compatible = "mediatek,mt6895-disp-inlinerotate",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_inlinerotate_driver_dt_match);

struct platform_driver mtk_disp_inlinerotate_driver = {
	.probe = mtk_disp_inlinerotate_probe,
	.remove = mtk_disp_inlinerotate_remove,
	.driver = {
		.name = "mediatek-disp-inlinerotate",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_inlinerotate_driver_dt_match,
	},
};
