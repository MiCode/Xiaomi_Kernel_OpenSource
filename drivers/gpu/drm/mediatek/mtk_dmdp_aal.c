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
#define DMDP_AAL_SHADOW_CTRL    0x0F0
#define AAL_BYPASS_SHADOW	BIT(0)
#define AAL_READ_WRK_REG	BIT(2)
#define DMDP_AAL_DRE_BITPLUS_00 0x048C
#define DMDP_AAL_DRE_BILATERAL	0x053C
#define DMDP_AAL_Y2R_00		0x04BC
#define DMDP_AAL_R2Y_00		0x04D4

#define AAL_EN BIT(0)

struct mtk_dmdp_aal_data {
	bool support_shadow;
	u32 block_info_00_mask;
};

struct mtk_dmdp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_dmdp_aal_data *data;
};

static inline struct mtk_dmdp_aal *comp_to_dmdp_aal(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_dmdp_aal, ddp_comp);
}

static void mtk_aal_write_mask(void __iomem *address, u32 data, u32 mask)
{
	u32 value = data;

	if (mask != ~0) {
		value = readl(address);
		value &= ~mask;
		data &= mask;
		value |= data;
	}
	writel(value, address);
}

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
	DDPINFO("%s\n", __func__);
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

	DDPINFO("%s: 0x%08x\n", __func__, val);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_CFG,
			0, 1);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_SIZE,
			val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_OUTPUT_SIZE, val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_BILATERAL, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_BITPLUS_00, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_Y2R_00, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_R2Y_00, 0, ~0);
}

void mtk_dmdp_aal_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_CFG,
			0x00400022, ~0);
	mtk_dmdp_aal_config(comp, cfg, handle);
	mtk_dmdp_aal_start(comp, handle);
}

static atomic_t g_aal_initialed = ATOMIC_INIT(0);
struct aal_backup { /* structure for backup AAL register value */
	unsigned int DRE_MAPPING;
	unsigned int DRE_BLOCK_INFO_00;
	unsigned int DRE_BLOCK_INFO_01;
	unsigned int DRE_BLOCK_INFO_02;
	unsigned int DRE_BLOCK_INFO_04;
	unsigned int DRE_BLOCK_INFO_05;
	unsigned int DRE_BLOCK_INFO_06;
	unsigned int DRE_BLOCK_INFO_07;
	unsigned int DRE_CHROMA_HIST_00;
	unsigned int DRE_CHROMA_HIST_01;
	unsigned int DRE_ALPHA_BLEND_00;
	unsigned int SRAM_CFG;
	unsigned int DUAL_PIPE_INFO_00;
	unsigned int DUAL_PIPE_INFO_01;
	unsigned int TILE_00;
	unsigned int TILE_01;
	unsigned int TILE_02;
};
static struct aal_backup g_aal_backup;

#define DMDP_AAL_SRAM_CFG                       (0x0c4)
#define DMDP_AAL_TILE_02			(0x0F4)
#define DMDP_AAL_DRE_BLOCK_INFO_07              (0x0f8)
#define DMDP_AAL_DRE_MAPPING_00                 (0x3b4)
#define DMDP_AAL_DRE_BLOCK_INFO_00              (0x468)
#define DMDP_AAL_DRE_BLOCK_INFO_01              (0x46c)
#define DMDP_AAL_DRE_BLOCK_INFO_02              (0x470)
#define DMDP_AAL_DRE_BLOCK_INFO_03              (0x474)
#define DMDP_AAL_DRE_BLOCK_INFO_04              (0x478)
#define DMDP_AAL_DRE_CHROMA_HIST_00             (0x480)
#define DMDP_AAL_DRE_CHROMA_HIST_01             (0x484)
#define DMDP_AAL_DRE_ALPHA_BLEND_00             (0x488)
#define DMDP_AAL_DRE_BLOCK_INFO_05              (0x4b4)
#define DMDP_AAL_DRE_BLOCK_INFO_06              (0x4b8)
#define DMDP_AAL_DUAL_PIPE_INFO_00              (0x4d0)
#define DMDP_AAL_DUAL_PIPE_INFO_01              (0x4d4)
#define DMDP_AAL_TILE_00			(0x4EC)
#define DMDP_AAL_TILE_01			(0x4F0)

static void ddp_aal_dre3_backup(struct mtk_ddp_comp *comp)
{
	g_aal_backup.DRE_BLOCK_INFO_00 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_00);
	g_aal_backup.DRE_BLOCK_INFO_01 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_01);
	g_aal_backup.DRE_BLOCK_INFO_02 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_02);
	g_aal_backup.DRE_BLOCK_INFO_04 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_04);
	g_aal_backup.DRE_CHROMA_HIST_00 =
		readl(comp->regs + DMDP_AAL_DRE_CHROMA_HIST_00);
	g_aal_backup.DRE_CHROMA_HIST_01 =
		readl(comp->regs + DMDP_AAL_DRE_CHROMA_HIST_01);
	g_aal_backup.DRE_ALPHA_BLEND_00 =
		readl(comp->regs + DMDP_AAL_DRE_ALPHA_BLEND_00);
	g_aal_backup.DRE_BLOCK_INFO_05 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_05);
	g_aal_backup.DRE_BLOCK_INFO_06 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_06);
	g_aal_backup.DRE_BLOCK_INFO_07 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_07);
	g_aal_backup.SRAM_CFG =
		readl(comp->regs + DMDP_AAL_SRAM_CFG);
	g_aal_backup.DUAL_PIPE_INFO_00 =
		readl(comp->regs + DMDP_AAL_DUAL_PIPE_INFO_00);
	g_aal_backup.DUAL_PIPE_INFO_01 =
		readl(comp->regs + DMDP_AAL_DUAL_PIPE_INFO_01);
	g_aal_backup.TILE_00 =
		readl(comp->regs + DMDP_AAL_TILE_00);
	g_aal_backup.TILE_01 =
		readl(comp->regs + DMDP_AAL_TILE_01);
	g_aal_backup.TILE_02 =
		readl(comp->regs + DMDP_AAL_TILE_02);
}

static void ddp_aal_dre_backup(struct mtk_ddp_comp *comp)
{
	g_aal_backup.DRE_MAPPING =
		readl(comp->regs + DMDP_AAL_DRE_MAPPING_00);
}

static void mtk_dmdp_aal_backup(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	ddp_aal_dre_backup(comp);
	ddp_aal_dre3_backup(comp);
	atomic_set(&g_aal_initialed, 1);
}

static void ddp_aal_dre3_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_dmdp_aal *dmdp_aal = comp_to_dmdp_aal(comp);

	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_00,
		g_aal_backup.DRE_BLOCK_INFO_00 &
		(dmdp_aal->data->block_info_00_mask),
		dmdp_aal->data->block_info_00_mask);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_01,
		g_aal_backup.DRE_BLOCK_INFO_01, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_02,
		g_aal_backup.DRE_BLOCK_INFO_02, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_04,
		g_aal_backup.DRE_BLOCK_INFO_04 & (0x3FF << 13), 0x3FF << 13);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_CHROMA_HIST_00,
		g_aal_backup.DRE_CHROMA_HIST_00, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_CHROMA_HIST_01,
		g_aal_backup.DRE_CHROMA_HIST_01 & 0x1FFFFFFF, 0x1FFFFFFF);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_ALPHA_BLEND_00,
		g_aal_backup.DRE_ALPHA_BLEND_00, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_05,
		g_aal_backup.DRE_BLOCK_INFO_05, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_06,
		g_aal_backup.DRE_BLOCK_INFO_06, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_07,
		g_aal_backup.DRE_BLOCK_INFO_07, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_SRAM_CFG,
		g_aal_backup.SRAM_CFG, 0x1);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DUAL_PIPE_INFO_00,
		g_aal_backup.DUAL_PIPE_INFO_00, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DUAL_PIPE_INFO_01,
		g_aal_backup.DUAL_PIPE_INFO_01, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_00,
		g_aal_backup.TILE_00, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_01,
		g_aal_backup.TILE_01, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_02,
		g_aal_backup.TILE_02, ~0);
}

static void ddp_aal_dre_restore(struct mtk_ddp_comp *comp)
{
	writel(g_aal_backup.DRE_MAPPING,
		comp->regs + DMDP_AAL_DRE_MAPPING_00);
}

static void mtk_dmdp_aal_restore(struct mtk_ddp_comp *comp)
{
	if (atomic_read(&g_aal_initialed) != 1)
		return;

	DDPINFO("%s\n", __func__);
	ddp_aal_dre_restore(comp);
	ddp_aal_dre3_restore(comp);
}

static void mtk_dmdp_aal_prepare(struct mtk_ddp_comp *comp)
{
#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	struct mtk_dmdp_aal *dmdp_aal = comp_to_dmdp_aal(comp);
#endif

	pr_notice("%s\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);

#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	if (dmdp_aal->data->support_shadow) {
		/* Enable shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, 0x0,
			DMDP_AAL_SHADOW_CTRL, AAL_BYPASS_SHADOW);
	} else {
		/* Bypass shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, AAL_BYPASS_SHADOW,
			DMDP_AAL_SHADOW_CTRL, AAL_BYPASS_SHADOW);
	}
#else
#if defined(CONFIG_MACH_MT6873)
	/* Bypass shadow register and read shadow register */
	mtk_ddp_write_mask_cpu(comp, AAL_BYPASS_SHADOW,
		DMDP_AAL_SHADOW_CTRL, AAL_BYPASS_SHADOW);
#endif
#endif

	mtk_dmdp_aal_restore(comp);
}

static void mtk_dmdp_aal_unprepare(struct mtk_ddp_comp *comp)
{
	pr_notice("%s\n", __func__);
	mtk_dmdp_aal_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_dmdp_aal_funcs = {
	.config = mtk_dmdp_aal_config,
	.first_cfg = mtk_dmdp_aal_first_cfg,
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
	mtk_cust_dump_reg(baddr, 0x200, 0xf4, 0xf8, 0x468);
	mtk_cust_dump_reg(baddr, 0x46c, 0x470, 0x474, 0x478);
	mtk_cust_dump_reg(baddr, 0x4ec, 0x4f0, 0x528, 0x52c);
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

	priv->data = of_device_get_match_data(dev);
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

static const struct mtk_dmdp_aal_data mt6885_dmdp_aal_driver_data = {
	.support_shadow = false,
	.block_info_00_mask = 0x3FFFFFF,
};

static const struct mtk_dmdp_aal_data mt6873_dmdp_aal_driver_data = {
	.support_shadow = false,
	.block_info_00_mask = 0x3FFF3FFF,
};

static const struct of_device_id mtk_dmdp_aal_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-dmdp-aal",
	  .data = &mt6885_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6873-dmdp-aal",
	  .data = &mt6873_dmdp_aal_driver_data},
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
