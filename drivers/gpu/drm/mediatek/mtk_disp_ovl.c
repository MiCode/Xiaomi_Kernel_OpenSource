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

#ifdef CONFIG_ARM
#include <asm/dma-iommu.h>
#endif
#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_debugfs.h"

#define DISP_REG_OVL_INTEN			0x0004
#define OVL_FME_CPL_INT					BIT(1)
#define DISP_REG_OVL_INTSTA			0x0008
#define DISP_REG_OVL_EN				0x000c
#define DISP_REG_OVL_RST			0x0014
#define DISP_REG_OVL_ROI_SIZE			0x0020
#define DISP_REG_OVL_ROI_BGCLR			0x0028
#define DISP_REG_OVL_SRC_CON			0x002c
#define DISP_REG_OVL_CON(n)			(0x0030 + 0x20 * (n))
#define DISP_REG_OVL_SRC_KEY(n)			(0x0034 + 0x20 * (n))
#define DISP_REG_OVL_SRC_SIZE(n)		(0x0038 + 0x20 * (n))
#define DISP_REG_OVL_OFFSET(n)			(0x003c + 0x20 * (n))
#define DISP_REG_OVL_PITCH(n)			(0x0044 + 0x20 * (n))
#define DISP_REG_OVL_RDMA_CTRL(n)		(0x00c0 + 0x20 * (n))
#define DISP_REG_OVL_RDMA_GMC(n)		(0x00c8 + 0x20 * (n))
#define DISP_REG_OVL_ADDR(n)			(0x0f40 + 0x20 * (n))

#define	OVL_RDMA_MEM_GMC	0x40402020

#define OVL_CON_SKEN		BIT(30)
#define OVL_CON_BYTE_SWAP	BIT(24)
#define OVL_CON_CLRFMT_RGBA8888	(2 << 12)
#define OVL_CON_CLRFMT_ARGB8888	(3 << 12)
#define	OVL_CON_AEN		BIT(8)
#define	OVL_CON_ALPHA		0xff

#define	OVL_SURFL_EN		BIT(31)
#define	OVL_DST_BLENDING	BIT(23)
#define	OVL_SRC_BLENDING	BIT(18)

/**
 * struct mtk_disp_ovl - DISP_OVL driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report vblank events to
 */
struct mtk_disp_ovl {
	struct mtk_ddp_comp		ddp_comp;
	struct drm_crtc			*crtc;
};

static irqreturn_t mtk_disp_ovl_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_ovl *priv = dev_id;
	struct mtk_ddp_comp *ovl = &priv->ddp_comp;

	/* Clear frame completion interrupt */
	writel(0x0, ovl->regs + DISP_REG_OVL_INTSTA);

	if (!priv->crtc)
		return IRQ_NONE;

	mtk_crtc_ddp_irq(priv->crtc, ovl);

	return IRQ_HANDLED;
}

static void mtk_ovl_enable_vblank(struct mtk_ddp_comp *comp,
				  struct drm_crtc *crtc)
{
	struct mtk_disp_ovl *priv = container_of(comp, struct mtk_disp_ovl,
						 ddp_comp);

	priv->crtc = crtc;
	writel_relaxed(OVL_FME_CPL_INT, comp->regs + DISP_REG_OVL_INTEN);
}

static void mtk_ovl_disable_vblank(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ovl *priv = container_of(comp, struct mtk_disp_ovl,
						 ddp_comp);

	priv->crtc = NULL;
	writel_relaxed(0x0, comp->regs + DISP_REG_OVL_INTEN);
}

static void mtk_ovl_start(struct mtk_ddp_comp *comp)
{
	writel_relaxed(0x1, comp->regs + DISP_REG_OVL_EN);
}

static void mtk_ovl_stop(struct mtk_ddp_comp *comp)
{
	writel_relaxed(0x0, comp->regs + DISP_REG_OVL_EN);
}

static void mtk_ovl_config(struct mtk_ddp_comp *comp, unsigned int w,
			   unsigned int h, unsigned int vrefresh)
{
	if (w != 0 && h != 0)
		writel_relaxed(h << 16 | w, comp->regs + DISP_REG_OVL_ROI_SIZE);
	writel_relaxed(0x0, comp->regs + DISP_REG_OVL_ROI_BGCLR);

	writel(0x1, comp->regs + DISP_REG_OVL_RST);
	writel(0x0, comp->regs + DISP_REG_OVL_RST);
}

static void mtk_ovl_layer_on(struct mtk_ddp_comp *comp, unsigned int idx)
{
	unsigned int reg;

	writel(0x1, comp->regs + DISP_REG_OVL_RDMA_CTRL(idx));
	writel(OVL_RDMA_MEM_GMC, comp->regs + DISP_REG_OVL_RDMA_GMC(idx));

	reg = readl(comp->regs + DISP_REG_OVL_SRC_CON);
	reg = reg | BIT(idx);
	writel(reg, comp->regs + DISP_REG_OVL_SRC_CON);
}

static void mtk_ovl_layer_off(struct mtk_ddp_comp *comp, unsigned int idx)
{
	unsigned int reg;

	reg = readl(comp->regs + DISP_REG_OVL_SRC_CON);
	reg = reg & ~BIT(idx);
	writel(reg, comp->regs + DISP_REG_OVL_SRC_CON);

	writel(0x0, comp->regs + DISP_REG_OVL_RDMA_CTRL(idx));
}

static unsigned int ovl_fmt_convert(struct mtk_ddp_comp *comp, unsigned int fmt)
{
	switch (fmt) {
	default: return 0x7; /* not support */
	case DRM_FORMAT_RGB565:
		return comp->data->ovl.fmt_rgb565;
	case DRM_FORMAT_BGR565:
		return comp->data->ovl.fmt_rgb565 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_RGB888:
		return comp->data->ovl.fmt_rgb888;
	case DRM_FORMAT_BGR888:
		return comp->data->ovl.fmt_rgb888 | OVL_CON_BYTE_SWAP;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_AEN | OVL_CON_ALPHA;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		return OVL_CON_CLRFMT_ARGB8888 | OVL_CON_BYTE_SWAP | OVL_CON_AEN
					       | OVL_CON_ALPHA;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		return OVL_CON_CLRFMT_RGBA8888 | OVL_CON_AEN | OVL_CON_ALPHA;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		return OVL_CON_CLRFMT_RGBA8888 | OVL_CON_BYTE_SWAP | OVL_CON_AEN
					       | OVL_CON_ALPHA;
	}
}

static void mtk_ovl_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				 struct mtk_plane_state *state)
{
	struct mtk_plane_pending_state *pending = &state->pending;
	unsigned int addr = pending->addr;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int fmt = pending->format;
	unsigned int offset = (pending->y << 16) | pending->x;
	unsigned int src_size = (pending->height << 16) | pending->width;
	unsigned int con;

	if (!pending->enable)
		mtk_ovl_layer_off(comp, idx);

	con = ovl_fmt_convert(comp, fmt);
	if (force_alpha())
		pitch |= OVL_SURFL_EN | OVL_DST_BLENDING; /* pre-multiplied */
	else
		pitch |= OVL_SURFL_EN | OVL_DST_BLENDING | OVL_SRC_BLENDING;

	con = (con & 0xffffff00) | (pending->alpha & 0xff);
	if (pending->colorkey & BIT(24)) /* default is disable */
		con |= OVL_CON_SKEN;

	writel_relaxed(pending->colorkey & 0xffffff, comp->regs + DISP_REG_OVL_SRC_KEY(idx));
	writel_relaxed(con, comp->regs + DISP_REG_OVL_CON(idx));
	writel_relaxed(pitch, comp->regs + DISP_REG_OVL_PITCH(idx));
	writel_relaxed(src_size, comp->regs + DISP_REG_OVL_SRC_SIZE(idx));
	writel_relaxed(offset, comp->regs + DISP_REG_OVL_OFFSET(idx));
	writel_relaxed(addr, comp->regs + (comp->data->ovl.addr_offset + idx * 0x20));

	if (pending->enable)
		mtk_ovl_layer_on(comp, idx);
}

static const struct mtk_ddp_comp_funcs mtk_disp_ovl_funcs = {
	.config = mtk_ovl_config,
	.start = mtk_ovl_start,
	.stop = mtk_ovl_stop,
	.enable_vblank = mtk_ovl_enable_vblank,
	.disable_vblank = mtk_ovl_disable_vblank,
	.layer_on = mtk_ovl_layer_on,
	.layer_off = mtk_ovl_layer_off,
	.layer_config = mtk_ovl_layer_config,
};

static int mtk_disp_ovl_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;
	int i;

#ifdef CONFIG_ARM
	struct device_node *np;
	struct platform_device *pdev;
	struct dma_iommu_mapping *dma_mapping;

	np = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!np)
		return 0;

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (WARN_ON(!pdev))
		return -EINVAL;

	dma_mapping = pdev->dev.archdata.iommu;
	arm_iommu_attach_device(dev, dma_mapping);
#endif

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	for (i = 0 ; i < OVL_LAYER_NR; i++)
		mtk_ddp_comp_layer_off(&priv->ddp_comp, i);

	return 0;
}

static void mtk_disp_ovl_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_ovl *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ovl_component_ops = {
	.bind	= mtk_disp_ovl_bind,
	.unbind = mtk_disp_ovl_unbind,
};

static struct mtk_ddp_comp_driver_data mt2701_ovl_driver_data = {
	.ovl = {0x0040, 1, 0}
};

static struct mtk_ddp_comp_driver_data mt8173_ovl_driver_data = {
	.ovl = {0x0f40, 0, 1}
};

static const struct of_device_id mtk_disp_ovl_driver_dt_match[] = {
	{ .compatible = "mediatek,mt2701-disp-ovl",
	  .data = &mt2701_ovl_driver_data},
	{ .compatible = "mediatek,mt8173-disp-ovl",
	  .data = &mt8173_ovl_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_ovl_driver_dt_match);

static inline struct mtk_ddp_comp_driver_data *mtk_ovl_get_driver_data(
	struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(mtk_disp_ovl_driver_dt_match, &pdev->dev);

	return (struct mtk_ddp_comp_driver_data *)of_id->data;
}

static int mtk_disp_ovl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ovl *priv;
	int comp_id;
	int irq;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(dev, irq, mtk_disp_ovl_irq_handler,
			       IRQF_TRIGGER_NONE, dev_name(dev), priv);
	if (ret < 0) {
		dev_err(dev, "Failed to request irq %d: %d\n", irq, ret);
		return ret;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_OVL);
	if (comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ovl_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->ddp_comp.data = mtk_ovl_get_driver_data(pdev);
	priv->ddp_comp.data->do_shadow_reg = of_property_read_bool(dev->of_node,
							"shadow_register");

	platform_set_drvdata(pdev, priv);

	ret = component_add(dev, &mtk_disp_ovl_component_ops);
	if (ret)
		dev_err(dev, "Failed to add component: %d\n", ret);

	return ret;
}

static int mtk_disp_ovl_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_ovl_component_ops);

	return 0;
}


struct platform_driver mtk_disp_ovl_driver = {
	.probe		= mtk_disp_ovl_probe,
	.remove		= mtk_disp_ovl_remove,
	.driver		= {
		.name	= "mediatek-disp-ovl",
		.owner	= THIS_MODULE,
		.of_match_table = mtk_disp_ovl_driver_dt_match,
	},
};
