// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"

#define RSZ_ENABLE (0x000)
#define RSZ_INPUT_IMAGE (0x010)
#define RSZ_OUTPUT_IMAGE (0x014)

struct mtk_disp_mdp_rsz_data {
	unsigned int tile_length;
	unsigned int in_max_height;
	bool support_shadow;
	bool need_bypass_shadow;
};


/**
 * struct mtk_disp_rsz - DISP_MDP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_mdp_rsz {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_mdp_rsz_data *data;
};

static inline struct mtk_disp_mdp_rsz *comp_to_mdp_rsz(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_mdp_rsz, ddp_comp);
}

static void mtk_mdp_rsz_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_mdp_rsz_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_mdp_rsz_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	unsigned int width, height;
	struct drm_crtc *crtc = &(comp->mtk_crtc->base);

	width = crtc->mode.hdisplay;
	height = crtc->mode.vdisplay;

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_ENABLE, 0x00, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_INPUT_IMAGE,
		 width | (height << 16), ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + RSZ_OUTPUT_IMAGE,
		width | (height << 16), ~0);
}

static const struct mtk_ddp_comp_funcs mtk_disp_mdp_rsz_funcs = {
	.addon_config = mtk_mdp_rsz_addon_config,
	.prepare = mtk_mdp_rsz_prepare,
	.unprepare = mtk_mdp_rsz_unprepare,
};

static int mtk_disp_mdp_rsz_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_mdp_rsz *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPFUNC();
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		DDPPR_ERR("Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_mdp_rsz_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_mdp_rsz *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_mdp_rsz_component_ops = {
	.bind = mtk_disp_mdp_rsz_bind,
	.unbind = mtk_disp_mdp_rsz_unbind,
};

static int mtk_disp_mdp_rsz_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_mdp_rsz *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_MDP_RSZ);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_mdp_rsz_funcs);
	if (ret) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_mdp_rsz_component_ops);
	if (ret != 0) {
		DDPPR_ERR("Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_mdp_rsz_remove(struct platform_device *pdev)
{
	struct mtk_disp_mdp_rsz *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_mdp_rsz_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

int mtk_mdp_rsz_dump(struct mtk_ddp_comp *comp)
{
	return 0;
}

int mtk_mdp_rsz_analysis(struct mtk_ddp_comp *comp)
{
	return 0;
}

static const struct mtk_disp_mdp_rsz_data mt6985_mdp_rsz_driver_data = {
	.tile_length = 1660, .in_max_height = 4096,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_mdp_rsz_driver_dt_match[] = {
	{.compatible = "mediatek,mt6985-disp-mdp-rsz",
	 .data = &mt6985_mdp_rsz_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_mdp_rsz_driver_dt_match);

struct platform_driver mtk_disp_mdp_rsz_driver = {
	.probe = mtk_disp_mdp_rsz_probe,
	.remove = mtk_disp_mdp_rsz_remove,
	.driver = {
			.name = "mediatek-disp-mdp-rsz",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_mdp_rsz_driver_dt_match,
		},
};
