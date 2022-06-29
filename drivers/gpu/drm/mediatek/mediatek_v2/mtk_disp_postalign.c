// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"

struct mtk_disp_postalign_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

struct mtk_disp_postalign {
	struct mtk_ddp_comp	 ddp_comp;
	const struct mtk_disp_postalign_data *data;
	int enable;
};

static inline struct mtk_disp_postalign *comp_to_postalign(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_postalign, ddp_comp);
}

static void mtk_postalign_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s start\n", mtk_dump_comp_str(comp));
}

static void mtk_postalign_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s stop\n", mtk_dump_comp_str(comp));
}

static void mtk_postalign_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s prepare\n", mtk_dump_comp_str(comp));

	mtk_ddp_comp_clk_prepare(comp);

	/* Bypass shadow register and read shadow register */
}

static void mtk_postalign_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s unprepare\n", mtk_dump_comp_str(comp));

	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_postalign_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	DDPINFO("%s config\n", mtk_dump_comp_str(comp));
}

void mtk_postalign_dump(struct mtk_ddp_comp *comp)
{
	if (!comp->regs) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	if (!comp->regs_pa) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
}

int mtk_postalign_analysis(struct mtk_ddp_comp *comp)
{
	if (!comp->regs) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	if (!comp->regs_pa) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}

	DDPDUMP("== %s ANALYSIS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_postalign_funcs = {
	.config = mtk_postalign_config,
	.start = mtk_postalign_start,
	.stop = mtk_postalign_stop,
	.prepare = mtk_postalign_prepare,
	.unprepare = mtk_postalign_unprepare,
};

static int mtk_disp_postalign_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_postalign *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_postalign_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_disp_postalign *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_postalign_component_ops = {
	.bind = mtk_disp_postalign_bind,
	.unbind = mtk_disp_postalign_unbind,
};

static int mtk_disp_postalign_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_postalign *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_POSTALIGN);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_postalign_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_postalign_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);
	return ret;
}

static int mtk_disp_postalign_remove(struct platform_device *pdev)
{
	struct mtk_disp_postalign *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_postalign_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_postalign_data mt6985_postalign_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_postalign_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6985-disp-postalign",
	  .data = &mt6985_postalign_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_postalign_driver_dt_match);

struct platform_driver mtk_disp_postalign_driver = {
	.probe = mtk_disp_postalign_probe,
	.remove = mtk_disp_postalign_remove,
	.driver = {
		.name = "mediatek-disp-postalign",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_postalign_driver_dt_match,
	},
};
