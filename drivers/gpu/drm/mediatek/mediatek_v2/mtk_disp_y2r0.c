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

#define DISP_REG_DISP_Y2R0_EN 0x250
#define DISP_REG_DISP_Y2R0_RST 0x254
#define DISP_REG_DISP_Y2R0_CON0 0x258
	#define DISP_REG_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT709_RGB 0x4 //Bit 2 always on
	#define DISP_REG_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT601_RGB 0x5
	#define DISP_REG_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT709_RGB 0x6
	#define DISP_REG_DISP_Y2R0_MATRIX_SEL_LIMIT_RANGE_BT601_RGB 0x7
	#define DISP_REG_DISP_Y2R0_CLAMP BIT(4)
	#define DISP_REG_DISP_Y2R0_BYPASS_DATA_PROCESS_NOBYPASS BIT(5)
#define DISP_REG_DISP_DL_IN_RELAY3_SIZE 0x26C
#define DISP_REG_DISP_DL_OUT_RELAY3_SIZE 0x27C

/**
 * struct mtk_disp_y2r - DISP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_y2r {
	struct mtk_ddp_comp ddp_comp;
};

static inline struct mtk_disp_y2r *comp_to_y2r(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_y2r, ddp_comp);
}

static void mtk_y2r_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	if (!mtk_crtc->is_force_mml_scen)
		return;

	if (addon_config->config_type.type == ADDON_DISCONNECT)
		return;

	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
			comp->regs_pa
			+ DISP_REG_DISP_Y2R0_EN, 0x1, ~0);
	cmdq_pkt_write(handle, mtk_crtc->gce_obj.base,
			comp->regs_pa
			+ DISP_REG_DISP_Y2R0_CON0,
			DISP_REG_DISP_Y2R0_MATRIX_SEL_FULL_RANGE_BT601_RGB, ~0);
}

void mtk_y2r_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== DISP %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	DDPDUMP("0x250: 0x%08x 0x%08x 0x%08x\n", readl(baddr + 0x250),
		readl(baddr + 0x254), readl(baddr + 0x258));
}

int mtk_y2r_analysis(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	return 0;
}

static void mtk_y2r_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
}

static void mtk_y2r_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
}

static void mtk_y2r_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_y2r_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_y2r_funcs = {
	.start = mtk_y2r_start,
	.stop = mtk_y2r_stop,
	.addon_config = mtk_y2r_addon_config,
	.prepare = mtk_y2r_prepare,
	.unprepare = mtk_y2r_unprepare,
};

static int mtk_disp_y2r_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_y2r *priv = dev_get_drvdata(dev);
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

static void mtk_disp_y2r_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_y2r *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_y2r_component_ops = {
	.bind = mtk_disp_y2r_bind, .unbind = mtk_disp_y2r_unbind,
};

static int mtk_disp_y2r_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_y2r *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_Y2R);
	DDPINFO("comp_id:%d", comp_id);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_y2r_funcs);

	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	//priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	DDPINFO("&priv->ddp_comp:0x%lx\n", (unsigned long)&priv->ddp_comp);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_y2r_component_ops);

	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_y2r_remove(struct platform_device *pdev)
{
	struct mtk_disp_y2r *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_y2r_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_disp_y2r_driver_dt_match[] = {
	{.compatible = "mediatek,mt6983-disp-y2r",},
	{.compatible = "mediatek,mt6895-disp-y2r",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_y2r_driver_dt_match);

struct platform_driver mtk_disp_y2r_driver = {
	.probe = mtk_disp_y2r_probe,
	.remove = mtk_disp_y2r_remove,
	.driver = {
		.name = "mediatek-disp-y2r",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_y2r_driver_dt_match,
	},
};
