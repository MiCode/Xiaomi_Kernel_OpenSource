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
#include <cmdq-util.h>

#define MDP_MUTEX0_EN 0x020
#define MDP_BYPASS_MUX_SHADOW 0xF00
#define MDP_DLI0_SEL_IN 0xF14
#define MDP_RDMA0_MOUT_EN 0xF20
#define MDP_WROT0_SEL_IN 0xF70
#define MDP_DLO0_SOUT_SEL 0xF88
#define MDPSYS_CG_CON0 0x100
#define MDPSYS_CG_SET0 0x104

#define DISP_MUTEX0_EN 0xA0
#define DISP_MUTEX0_CTL 0xAc
#define DISP_MUTEX0_MOD0 0xB0
#define DISP_MUTEX0_MOD1 0xB4

//struct clk* g_clk_arr[4];

/**
 * struct mtk_mmlsys_bypass - DISP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_mmlsys_bypass {
	struct mtk_ddp_comp ddp_comp;
};

static inline struct mtk_mmlsys_bypass *comp_to_mmlsys_bypass(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_mmlsys_bypass, ddp_comp);
}

static void mtk_mmlsys_bypass_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	DDPINFO("%s +\n", __func__);
	cmdq_util_prebuilt_init(CMDQ_PREBUILT_MML);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + MDPSYS_CG_SET0,
		0x00000000, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + MDP_BYPASS_MUX_SHADOW,
		0x00000001, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + 0x0F0,
		0x80000000, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + MDP_DLI0_SEL_IN,
		0x00000001, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + MDP_RDMA0_MOUT_EN,
		0x00000002, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + MDP_WROT0_SEL_IN,
		0x00000000, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + MDP_DLO0_SOUT_SEL,
		0x00000001, ~0);

	// mutex
	{
		DDPINFO("%s cmdq_pkt_write mutex +\n", __func__);
		cmdq_pkt_write(handle, comp->cmdq_base,
			(resource_size_t)(0x1F801000) + 0x000, 0x00010001, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			(resource_size_t)(0x1F801000) + 0x008, 0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			(resource_size_t)(0x1F801000) + DISP_MUTEX0_EN, 0x00000001, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			(resource_size_t)(0x1F801000) + DISP_MUTEX0_CTL, 0x00000041, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			(resource_size_t)(0x1F801000) + DISP_MUTEX0_MOD0, 0x000F0000, ~0);
		DDPINFO("%s cmdq_pkt_write mutex -\n", __func__);
	}
}

void mtk_mmlsys_bypass_dump(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	DDPDUMP("== DISP %s REGS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("MDP_DLI0_SEL_IN 0x%08x: 0x%08x\n",
		MDP_DLI0_SEL_IN, readl(comp->regs + MDP_DLI0_SEL_IN));
	DDPDUMP("MDP_RDMA0_MOUT_EN 0x%08x: 0x%08x\n",
		MDP_RDMA0_MOUT_EN, readl(comp->regs + MDP_RDMA0_MOUT_EN));
	DDPDUMP("MDP_WROT0_SEL_IN 0x%08x: 0x%08x\n",
		MDP_WROT0_SEL_IN, readl(comp->regs + MDP_WROT0_SEL_IN));
	DDPDUMP("MDP_DLO0_SOUT_SEL 0x%08x: 0x%08x\n",
		MDP_DLO0_SOUT_SEL, readl(comp->regs + MDP_DLO0_SOUT_SEL));
	DDPDUMP("MDPSYS_CG_CON0 0x%08x: 0x%08x\n",
		MDPSYS_CG_CON0, readl(comp->regs + MDPSYS_CG_CON0));
	DDPDUMP("0xFE0: 0x%08x 0x%08x 0x%08x\n", readl(comp->regs + 0xFE0),
		readl(comp->regs + 0xFE4), readl(comp->regs + 0xFE8));
	DDPDUMP("0xFF0: 0x%08x 0x%08x 0x%08x\n", readl(comp->regs + 0xFF0),
		readl(comp->regs + 0xFF4), readl(comp->regs + 0xFF8));
	DDPDUMP("== DISP %s REGS ==\n", mtk_dump_comp_str(comp));
}

int mtk_mmlsys_bypass_analysis(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	return 0;
}

static void mtk_mmlsys_bypass_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
}

static void mtk_mmlsys_bypass_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
}

static void mtk_mmlsys_bypass_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s +\n", __func__);
}

static void mtk_mmlsys_bypass_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_mmlsys_bypass_funcs = {
	.start = mtk_mmlsys_bypass_start,
	.stop = mtk_mmlsys_bypass_stop,
	.addon_config = mtk_mmlsys_bypass_addon_config,
	.prepare = mtk_mmlsys_bypass_prepare,
	.unprepare = mtk_mmlsys_bypass_unprepare,
};

static int mtk_mmlsys_bypass_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_mmlsys_bypass *priv = dev_get_drvdata(dev);
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

static void mtk_mmlsys_bypass_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_mmlsys_bypass *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_mmlsys_bypass_component_ops = {
	.bind = mtk_mmlsys_bypass_bind, .unbind = mtk_mmlsys_bypass_unbind,
};

static int mtk_mmlsys_bypass_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_mmlsys_bypass *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_MMLSYS_BYPASS);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_mmlsys_bypass_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_mmlsys_bypass_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_mmlsys_bypass_remove(struct platform_device *pdev)
{
	struct mtk_mmlsys_bypass *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_mmlsys_bypass_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_mmlsys_bypass_driver_dt_match[] = {
	{.compatible = "mediatek,mt6983-mmlsys-bypass",},
	{.compatible = "mediatek,mt6895-mmlsys-bypass",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mmlsys_bypass_driver_dt_match);

struct platform_driver mtk_mmlsys_bypass_driver = {
	.probe = mtk_mmlsys_bypass_probe,
	.remove = mtk_mmlsys_bypass_remove,
	.driver = {
		.name = "mediatek-mmlsys-bypass",
		.owner = THIS_MODULE,
		.of_match_table = mtk_mmlsys_bypass_driver_dt_match,
	},
};
