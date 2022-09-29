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

struct mtk_disp_inlinerotate_data {
	unsigned int (*ovl_sel_mapping)(struct mtk_ddp_comp *comp);
};

struct mtk_disp_inlinerotate {
	struct mtk_ddp_comp ddp_comp;
	const struct mtk_disp_inlinerotate_data *data;
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
	struct mtk_ddp_comp *first_ovl = NULL;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_inlinerotate *ir = comp_to_inlinerotate(comp);
	unsigned int ovl_sel = 0;

	/* config inlinerot only when the first IR frame, bypass addon_connect */
	if (addon_config)
		return;

	if (!mtk_crtc)
		return;

	if (mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].ovl_comp_nr[0] != 0)
		first_ovl = mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].ovl_comp[DDP_FIRST_PATH][0];
	else
		first_ovl = mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].ddp_comp[DDP_FIRST_PATH][0];
	ovl_sel = ir->data->ovl_sel_mapping(first_ovl);

	/* TODO: dynamic OVLSEL */
#ifdef IF_ZERO
	if (addon_config && addon_config->config_type.type == ADDON_DISCONNECT)
		return;

	if (addon_config && !addon_config->addon_mml_config.is_entering)
		return;
#endif

	DDPINFO("%s handle:0x%x, comp->regs_pa:0x%x, ovl_sel:%u\n",
		__func__, handle, comp->regs_pa, ovl_sel);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_DISPSYS_SHADOW_CTRL,
		       0x00000002, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_INLINEROT_OVLSEL,
		       ovl_sel, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_INLINEROT_HEIGHT0,
		       64, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_REG_INLINEROT_HEIGHT1,
		       64, ~0);
}

void mtk_inlinerotate_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}
	DDPDUMP("== DISP %s REGS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
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
}

static void mtk_inlinerotate_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
}

static void mtk_inlinerotate_reset(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s+ %s\n", __func__, mtk_dump_comp_str(comp));
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_INLINEROT_SWRST, 1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_INLINEROT_SWRST, 0, ~0);
	DDPINFO("%s-\n", __func__);
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
	.reset = mtk_inlinerotate_reset,
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

	priv->data = of_device_get_match_data(dev);

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

static unsigned int mt6983_ovl_sel_mapping(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
		return 0;
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
		return 12;
	default:
		DDPPR_ERR("%s not support comp id %d\n", __func__, comp->id);
		return 0;
	}
}

static unsigned int ovl_sel_mapping(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0_2L:
		return 0;
	case DDP_COMPONENT_OVL1_2L:
		return 5;
	case DDP_COMPONENT_OVL2_2L:
		return 10;
	case DDP_COMPONENT_OVL3_2L:
		return 15;
	default:
		DDPPR_ERR("%s not support comp id %d\n", __func__, comp->id);
		return 0;
	}
}

static const struct mtk_disp_inlinerotate_data mt6983_inlinerotate_driver_data = {
	.ovl_sel_mapping = &mt6983_ovl_sel_mapping,
};

static const struct mtk_disp_inlinerotate_data inlinerotate_driver_data = {
	.ovl_sel_mapping = &ovl_sel_mapping,
};

static const struct of_device_id mtk_disp_inlinerotate_driver_dt_match[] = {
	{.compatible = "mediatek,mt6983-disp-inlinerotate",
	 .data = &mt6983_inlinerotate_driver_data},
	{.compatible = "mediatek,mt6895-disp-inlinerotate",
	 .data = &mt6983_inlinerotate_driver_data}, /* same as 6983 */
	{.compatible = "mediatek,mt6985-disp-inlinerotate",
	 .data = &inlinerotate_driver_data},
	{.compatible = "mediatek,mt6886-disp-inlinerotate",
	 .data = &mt6983_inlinerotate_driver_data}, /* same as 6983 */
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
