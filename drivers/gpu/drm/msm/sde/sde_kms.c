/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drm_crtc.h>
#include "msm_drv.h"
#include "sde_kms.h"
#include "sde_hw_mdss.h"

static int modeset_init_intf(struct sde_kms *sde_kms, int intf_num)
{
	struct sde_mdss_cfg *catalog = sde_kms->catalog;
	u32 intf_type = catalog->intf[intf_num].type;

	switch (intf_type) {
	case INTF_NONE:
		break;
	case INTF_DSI:
		break;
	case INTF_LCDC:
		break;
	case INTF_HDMI:
		break;
	case INTF_EDP:
	default:
		break;
	}

	return 0;
}

static int modeset_init(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv = sde_kms->dev->dev_private;
	int i;
	int ret;
	struct sde_mdss_cfg *catalog = sde_kms->catalog;
	struct drm_device *dev = sde_kms->dev;
	struct drm_plane *primary_planes[MAX_PLANES];
	int primary_planes_idx = 0;

	int num_private_planes = catalog->mixer_count;

	ret = sde_irq_domain_init(sde_kms);
	if (ret)
		goto fail;

	/* Create the planes */
	for (i = 0; i < catalog->sspp_count; i++) {
		struct drm_plane *plane;
		bool primary = true;

		if (catalog->sspp[i].features & BIT(SDE_SSPP_CURSOR)
			|| !num_private_planes)
			primary = false;

		plane = sde_plane_init(dev, primary);
		if (IS_ERR(plane)) {
			ret = PTR_ERR(plane);
			goto fail;
		}
		priv->planes[priv->num_planes++] = plane;

		if (primary)
			primary_planes[primary_planes_idx++] = plane;
		if (num_private_planes)
			num_private_planes--;
	}

	/* Need enough primary planes to assign one per mixer (CRTC) */
	if (primary_planes_idx < catalog->mixer_count) {
		ret = -EINVAL;
		goto fail;
	}

	/* Create one CRTC per mixer */
	for (i = 0; i < catalog->mixer_count; i++) {
		/*
		 * Each mixer receives a private plane. We start
		 * with first RGB, and then DMA and then VIG.
		 */
		struct drm_crtc *crtc;

		crtc = sde_crtc_init(dev, NULL, primary_planes[i], i);
		if (IS_ERR(crtc)) {
			ret = PTR_ERR(crtc);
			goto fail;
		}
		priv->crtcs[priv->num_crtcs++] = crtc;
	}

	for (i = 0; i < catalog->intf_count; i++) {
		ret = modeset_init_intf(sde_kms, i);
		if (ret)
			goto fail;
	}
	return 0;
fail:
	return ret;
}

static int sde_hw_init(struct msm_kms *kms)
{
	return 0;
}

static long sde_round_pixclk(struct msm_kms *kms, unsigned long rate,
		struct drm_encoder *encoder)
{
	return rate;
}

static void sde_preclose(struct msm_kms *kms, struct drm_file *file)
{
}

static void sde_destroy(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(to_mdp_kms(kms));

	sde_irq_domain_fini(sde_kms);
	kfree(sde_kms);
}

static const struct mdp_kms_funcs kms_funcs = {
	.base = {
		.hw_init         = sde_hw_init,
		.irq_preinstall  = sde_irq_preinstall,
		.irq_postinstall = sde_irq_postinstall,
		.irq_uninstall   = sde_irq_uninstall,
		.irq             = sde_irq,
		.enable_vblank   = sde_enable_vblank,
		.disable_vblank  = sde_disable_vblank,
		.get_format      = mdp_get_format,
		.round_pixclk    = sde_round_pixclk,
		.preclose        = sde_preclose,
		.destroy         = sde_destroy,
	},
	.set_irqmask         = sde_set_irqmask,
};

static int get_clk(struct platform_device *pdev, struct clk **clkp,
		const char *name, bool mandatory)
{
	struct device *dev = &pdev->dev;
	struct clk *clk = devm_clk_get(dev, name);

	if (IS_ERR(clk) && mandatory) {
		dev_err(dev, "failed to get %s (%ld)\n", name, PTR_ERR(clk));
		return PTR_ERR(clk);
	}
	if (IS_ERR(clk))
		DBG("skipping %s", name);
	else
		*clkp = clk;

	return 0;
}

struct sde_kms *sde_hw_setup(struct platform_device *pdev)
{
	struct sde_kms *sde_kms;
	struct msm_kms *kms = NULL;
	int ret;

	sde_kms = kzalloc(sizeof(*sde_kms), GFP_KERNEL);
	if (!sde_kms)
		return NULL;

	mdp_kms_init(&sde_kms->base, &kms_funcs);

	kms = &sde_kms->base.base;

	sde_kms->mmio = msm_ioremap(pdev, "mdp_phys", "SDE");
	if (IS_ERR(sde_kms->mmio)) {
		ret = PTR_ERR(sde_kms->mmio);
		goto fail;
	}

	sde_kms->vbif = msm_ioremap(pdev, "vbif_phys", "VBIF");
	if (IS_ERR(sde_kms->vbif)) {
		ret = PTR_ERR(sde_kms->vbif);
		goto fail;
	}

	sde_kms->venus = devm_regulator_get_optional(&pdev->dev, "gdsc-venus");
	if (IS_ERR(sde_kms->venus)) {
		ret = PTR_ERR(sde_kms->venus);
		DBG("failed to get Venus GDSC regulator: %d\n", ret);
		sde_kms->venus = NULL;
	}

	if (sde_kms->venus) {
		ret = regulator_enable(sde_kms->venus);
		if (ret) {
			DBG("failed to enable venus GDSC: %d\n", ret);
			goto fail;
		}
	}

	sde_kms->vdd = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(sde_kms->vdd)) {
		ret = PTR_ERR(sde_kms->vdd);
		goto fail;
	}

	ret = regulator_enable(sde_kms->vdd);
	if (ret) {
		DBG("failed to enable regulator vdd: %d\n", ret);
		goto fail;
	}

	sde_kms->mmagic = devm_regulator_get_optional(&pdev->dev, "mmagic");
	if (IS_ERR(sde_kms->mmagic)) {
		ret = PTR_ERR(sde_kms->mmagic);
		DBG("failed to get mmagic GDSC regulator: %d\n", ret);
		sde_kms->mmagic = NULL;
	}

	/* mandatory clocks: */
	ret = get_clk(pdev, &sde_kms->axi_clk, "bus_clk", true);
	if (ret)
		goto fail;
	ret = get_clk(pdev, &sde_kms->ahb_clk, "iface_clk", true);
	if (ret)
		goto fail;
	ret = get_clk(pdev, &sde_kms->src_clk, "core_clk_src", true);
	if (ret)
		goto fail;
	ret = get_clk(pdev, &sde_kms->core_clk, "core_clk", true);
	if (ret)
		goto fail;
	ret = get_clk(pdev, &sde_kms->vsync_clk, "vsync_clk", true);
	if (ret)
		goto fail;

	/* optional clocks: */
	get_clk(pdev, &sde_kms->lut_clk, "lut_clk", false);
	get_clk(pdev, &sde_kms->mmagic_clk, "mmagic_clk", false);
	get_clk(pdev, &sde_kms->iommu_clk, "iommu_clk", false);

	return sde_kms;

fail:
	if (kms)
		sde_destroy(kms);

	return ERR_PTR(ret);
}

struct msm_kms *sde_kms_init(struct drm_device *dev)
{
	struct platform_device *pdev = dev->platformdev;
	struct sde_mdss_cfg *catalog;
	struct sde_kms *sde_kms;
	struct msm_kms *msm_kms;
	int ret = 0;

	sde_kms = sde_hw_setup(pdev);
	if (IS_ERR(sde_kms)) {
		ret = PTR_ERR(sde_kms);
		goto fail;
	}

	sde_kms->dev = dev;
	msm_kms = &sde_kms->base.base;

	/*
	 * Currently hardcoding to MDSS version 1.7.0 (8996)
	 */
	catalog = sde_hw_catalog_init(1, 7, 0);
	if (!catalog)
		goto fail;

	sde_kms->catalog = catalog;

	/*
	 * Now we need to read the HW catalog and initialize resources such as
	 * clocks, regulators, GDSC/MMAGIC, ioremap the register ranges etc
	 */

	/*
	 * modeset_init should create the DRM related objects i.e. CRTCs,
	 * planes, encoders, connectors and so forth
	 */
	modeset_init(sde_kms);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	return msm_kms;

fail:
	if (msm_kms)
		sde_destroy(msm_kms);

	return ERR_PTR(ret);
}
