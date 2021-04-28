/*
 * Copyright (c) 2015 MediaTek Inc.
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
#include "mtk_dump.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"


#define DISP_REG_MERGE_CTRL (0x000)
	#define FLD_MERGE_EN REG_FLD_MSB_LSB(0, 0)
	#define FLD_MERGE_RST REG_FLD_MSB_LSB(4, 4)
	#define FLD_MERGE_LR_SWAP REG_FLD_MSB_LSB(8, 8)
	#define FLD_MERGE_DCM_DIS REG_FLD_MSB_LSB(12, 12)
#define DISP_REG_MERGE_WIDTH (0x004)
	#define FLD_IN_WIDHT_L REG_FLD_MSB_LSB(15, 0)
	#define FLD_IN_WIDHT_R REG_FLD_MSB_LSB(31, 16)
#define DISP_REG_MERGE_HEIGHT (0x008)
	#define FLD_IN_HEIGHT REG_FLD_MSB_LSB(15, 0)

#define DISP_REG_MERGE_SHADOW_CRTL (0x00C)
#define DISP_REG_MERGE_DGB0 (0x010)
	#define FLD_PIXEL_CNT REG_FLD_MSB_LSB(15, 0)
	#define FLD_MERGE_STATE REG_FLD_MSB_LSB(17, 16)
#define DISP_REG_MERGE_DGB1 (0x014)
	#define FLD_LINE_CNT REG_FLD_MSB_LSB(15, 0)

struct mtk_merge_config_struct {
	unsigned short width_right;
	unsigned short width_left;
	unsigned int height;
};


struct mtk_disp_merge {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
};

static inline struct mtk_disp_merge *comp_to_merge(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_merge, ddp_comp);
}

static void mtk_merge_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	int ret;

	ret = pm_runtime_get_sync(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to enable power domain: %d\n", ret);
	DDPMSG("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_MERGE_CTRL, 0x1, ~0);
}

static void mtk_merge_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	int ret;

	DDPMSG("%s\n", __func__);
	ret = pm_runtime_put(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_MERGE_CTRL, 0x0, ~0);
}

static int mtk_merge_check_params(struct mtk_merge_config_struct *merge_config)
{
	if (!merge_config->height || !merge_config->width_left
		|| !merge_config->width_right) {
		DDPPR_ERR("%s:merge input width l(%u) w(%u) h(%u)\n",
			  __func__, merge_config->width_left,
			  merge_config->width_right, merge_config->height);
		return -EINVAL;
	}
	DDPMSG("%s:merge input width l(%u) r(%u) height(%u)\n",
			  __func__, merge_config->width_left,
			  merge_config->width_right, merge_config->height);
	return 0;
}

static void mtk_merge_config(struct mtk_ddp_comp *comp,
				struct mtk_ddp_config *cfg,
		       struct cmdq_pkt *handle)
{
	struct mtk_merge_config_struct merge_config;

	merge_config.height = cfg->h;
	merge_config.width_left = cfg->w / 2;
	merge_config.width_right = cfg->w / 2;

	mtk_merge_check_params(&merge_config);

	DDPMSG("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MERGE_WIDTH,
		merge_config.width_left | (merge_config.width_right << 16),
		~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_MERGE_HEIGHT,
		       merge_config.height, ~0);
}

void mtk_merge_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i = 0;

	DDPDUMP("== DISP %s REGS ==\n", mtk_dump_comp_str(comp));
	for (i = 0; i < 1; i++) {
		DDPDUMP("0x%03X: 0x%08x 0x%08x 0x%08x 0x%08x\n", i * 0x10,
			readl(baddr + i * 0x10), readl(baddr + i * 0x10 + 0x4),
			readl(baddr + i * 0x10 + 0x8),
			readl(baddr + i * 0x10 + 0xC));
	}
	DDPDUMP("0x010: 0x%08x 0x%08x;\n", readl(baddr + 0x10),
		readl(baddr + 0x14));
}

int mtk_merge_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	u32 width = 0;
	u32 height = 0;
	u32 enable = 0;
	u32 dbg0 = 0;
	u32 dbg1 = 0;

	const int len = 100;
	int ret;
	char msg[len];

	enable = readl(baddr + DISP_REG_MERGE_CTRL);
	width = readl(baddr + DISP_REG_MERGE_WIDTH);
	height = readl(baddr + DISP_REG_MERGE_HEIGHT);
	dbg0 = readl(baddr + DISP_REG_MERGE_DGB0);
	dbg1 = readl(baddr + DISP_REG_MERGE_DGB1);

	DDPDUMP("== DISP %s ANALYSIS ==\n", mtk_dump_comp_str(comp));

	ret = snprintf(msg, len,
		"en:%d,swap:%d,dcm_dis:%d,width_L:%d,width_R:%d,h:%d,pix_cnt:%d,line_cnt:%d\n",
		REG_FLD_VAL_GET(FLD_MERGE_EN, enable),
		REG_FLD_VAL_GET(FLD_MERGE_LR_SWAP, enable),
		REG_FLD_VAL_GET(FLD_MERGE_DCM_DIS, enable),
		REG_FLD_VAL_GET(FLD_IN_WIDHT_L, width),
		REG_FLD_VAL_GET(FLD_IN_WIDHT_R, width),
		REG_FLD_VAL_GET(FLD_IN_HEIGHT, height),
		REG_FLD_VAL_GET(FLD_PIXEL_CNT, dbg0),
		REG_FLD_VAL_GET(FLD_MERGE_STATE, dbg0),
		REG_FLD_VAL_GET(FLD_LINE_CNT, dbg1));

	if (ret >= 0)
		DDPDUMP("%s", msg);

	return 0;
}

static void mtk_merge_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_merge_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_merge_funcs = {
	.start = mtk_merge_start,
	.stop = mtk_merge_stop,
	.config = mtk_merge_config,
	.prepare = mtk_merge_prepare,
	.unprepare = mtk_merge_unprepare,
};

static int mtk_disp_merge_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_merge *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	//struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;

	pr_info("%s\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}
	pr_info("%s end\n", __func__);
	return 0;
}

static void mtk_disp_merge_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_merge *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_merge_component_ops = {
	.bind = mtk_disp_merge_bind, .unbind = mtk_disp_merge_unbind,
};

static int mtk_disp_merge_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_merge *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	pr_info("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_MERGE);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_merge_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	//priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_disp_merge_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	pr_info("%s-\n", __func__);

	return ret;
}

static int mtk_disp_merge_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_merge_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_disp_merge_driver_dt_match[] = {
	{.compatible = "mediatek,mt6779-disp-merge", },
	{.compatible = "mediatek,mt6885-disp-merge", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_merge_driver_dt_match);

struct platform_driver mtk_disp_merge_driver = {
	.probe = mtk_disp_merge_probe,
	.remove = mtk_disp_merge_remove,
	.driver = {

			.name = "mediatek-disp-merge",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_merge_driver_dt_match,
		},
};
