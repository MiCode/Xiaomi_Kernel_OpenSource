// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/component.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#include <linux/sprd_iommu.h>
#include <linux/mm.h>
#include <linux/memblock.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_modes.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_vblank.h>

#include "sprd_crtc.h"
#include "sprd_dpu.h"
#include "sprd_drm.h"
#include "sprd_gem.h"
#include "sprd_iommu.h"
#include "sprd_plane.h"
#include "sysfs/sysfs_display.h"

static void sprd_dpu_enable(struct sprd_dpu *dpu);
static void sprd_dpu_disable(struct sprd_dpu *dpu);

static void sprd_dpu_prepare_fb(struct sprd_crtc *crtc,
				struct drm_plane_state *new_state)
{
	struct drm_gem_object *obj;
	struct sprd_gem_obj *sprd_gem;
	struct sprd_dpu *dpu = crtc->priv;
	int i;

	if (!dpu->ctx.enabled) {
		DRM_WARN("dpu has already powered off\n");
		return;
	}

	for (i = 0; i < new_state->fb->format->num_planes; i++) {
		obj = drm_gem_fb_get_obj(new_state->fb, i);
		sprd_gem = to_sprd_gem_obj(obj);
		if (sprd_gem->need_iommu)
			sprd_crtc_iommu_map(&dpu->dev, sprd_gem);
	}
}

static unsigned long sprd_free_reserved_area(void *start, void *end, int poison, const char *s)
{
	void *pos;
	unsigned long pages = 0;

	start = (void *)PAGE_ALIGN((unsigned long)start);
	end = (void *)((unsigned long)end & PAGE_MASK);
	for (pos = start; pos < end; pos += PAGE_SIZE, pages++) {
		struct page *page = virt_to_page(pos);
		void *direct_map_addr;

		/*
		 * 'direct_map_addr' might be different from 'pos'
		 * because some architectures' virt_to_page()
		 * work with aliases.  Getting the direct map
		 * address ensures that we get a _writeable_
		 * alias for the memset().
		 */
		direct_map_addr = page_address(page);
		if ((unsigned int)poison <= 0xFF)
			memset(direct_map_addr, poison, PAGE_SIZE);

		free_reserved_page(page);
	}

	if (pages && s)
		pr_info("Freeing %s memory: %ldK\n",
			s, pages << (PAGE_SHIFT - 10));

	return pages;
}

static void sprd_dpu_cleanup_fb(struct sprd_crtc *crtc,
				struct drm_plane_state *old_state)
{
	struct drm_gem_object *obj;
	struct sprd_gem_obj *sprd_gem;
	struct sprd_dpu *dpu = crtc->priv;
	static atomic_t logo2animation = { -1 };
	int i;

	if (!dpu->ctx.enabled) {
		DRM_WARN("dpu has already powered off\n");
		return;
	}

	for (i = 0; i < old_state->fb->format->num_planes; i++) {
		obj = drm_gem_fb_get_obj(old_state->fb, i);
		sprd_gem = to_sprd_gem_obj(obj);
		if (sprd_gem->need_iommu)
			sprd_crtc_iommu_unmap(&dpu->dev, sprd_gem);
	}

	if (unlikely(atomic_inc_not_zero(&logo2animation)) &&
		dpu->ctx.logo_addr) {
		DRM_INFO("free logo memory addr:0x%lx size:0x%lx\n",
			dpu->ctx.logo_addr, dpu->ctx.logo_size);
		sprd_free_reserved_area(phys_to_virt(dpu->ctx.logo_addr),
			phys_to_virt(dpu->ctx.logo_addr + dpu->ctx.logo_size),
			-1, "logo");
	}
}

static void sprd_dpu_mode_set_nofb(struct sprd_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc->priv;
	struct drm_display_mode *mode = &crtc->base.state->adjusted_mode;

	DRM_INFO("%s() set mode: %s\n", __func__, dpu->mode->name);

	if (dpu->dsi->ctx.work_mode == DSI_MODE_VIDEO)
		dpu->ctx.if_type = SPRD_DPU_IF_DPI;
	else
		dpu->ctx.if_type = SPRD_DPU_IF_EDPI;

	if (dpu->core->modeset && crtc->base.state->mode_changed)
		dpu->core->modeset(&dpu->ctx, mode);
}

static enum drm_mode_status sprd_dpu_mode_valid(struct sprd_crtc *crtc,
					const struct drm_display_mode *mode)
{
	struct sprd_dpu *dpu = crtc->priv;

	DRM_INFO("%s() mode: "DRM_MODE_FMT"\n", __func__, DRM_MODE_ARG(mode));

	if (mode->type == DRM_MODE_TYPE_DRIVER)
		dpu->mode = (struct drm_display_mode *)mode;

	if (mode->type & DRM_MODE_TYPE_PREFERRED) {
		dpu->mode = (struct drm_display_mode *)mode;
		drm_display_mode_to_videomode(dpu->mode, &dpu->ctx.vm);
	}

	return MODE_OK;
}

static void sprd_dpu_atomic_enable(struct sprd_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc->priv;
	static bool is_enable = true;

	DRM_INFO("%s()\n", __func__);
	if (is_enable) {
		/* workaround:
		 * dpu r6p0 need resume after dsi resume on div6 scences
		 * for dsi core and dpi clk depends on dphy clk
		 */
		if (!strcmp(dpu->ctx.version, "dpu-r6p0")) {
			sprd_dpu_resume(dpu);
		}
		is_enable = false;
	}
	else
		pm_runtime_get_sync(dpu->dev.parent);

	if (strcmp(dpu->ctx.version, "dpu-r6p0")) {
		sprd_dpu_resume(dpu);
	}
}

static void sprd_dpu_atomic_disable(struct sprd_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc->priv;

	DRM_INFO("%s()\n", __func__);

	sprd_crtc_wait_last_commit_complete(&crtc->base);

	disable_irq(dpu->ctx.irq);

	sprd_dpu_disable(dpu);

	pm_runtime_put(dpu->dev.parent);
}

void sprd_dpu_atomic_disable_force(struct drm_crtc *crtc)
{
	struct sprd_crtc *sprd_crtc = container_of(crtc, struct sprd_crtc, base);
	struct sprd_dpu *dpu = sprd_crtc->priv;

	DRM_INFO("%s()\n", __func__);

	/* dpu is not initialized,it should enable first! */
	sprd_dpu_enable(dpu);
	enable_irq(dpu->ctx.irq);

	disable_irq(dpu->ctx.irq);
	sprd_dpu_disable(dpu);
}

static void sprd_dpu_atomic_begin(struct sprd_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc->priv;

	DRM_DEBUG("%s()\n", __func__);

	down(&dpu->ctx.lock);

	crtc->pending_planes = 0;
}

static void sprd_dpu_atomic_flush(struct sprd_crtc *crtc)

{
	struct sprd_dpu *dpu = crtc->priv;

	DRM_DEBUG("%s()\n", __func__);

	if (crtc->pending_planes && !dpu->ctx.flip_pending) {
		dpu->core->flip(&dpu->ctx, crtc->planes, crtc->pending_planes);
		dpu->ctx.frame_count++;
	}
	up(&dpu->ctx.lock);
}

static int sprd_dpu_enable_vblank(struct sprd_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc->priv;

	DRM_INFO("%s()\n", __func__);

	if (dpu->core->enable_vsync)
		dpu->core->enable_vsync(&dpu->ctx);

	return 0;
}

static void sprd_dpu_disable_vblank(struct sprd_crtc *crtc)
{
	struct sprd_dpu *dpu = crtc->priv;

	DRM_INFO("%s()\n", __func__);

	if (dpu->core->disable_vsync)
		dpu->core->disable_vsync(&dpu->ctx);
}

static const struct sprd_crtc_ops sprd_dpu_ops = {
	.mode_set_nofb	= sprd_dpu_mode_set_nofb,
	.mode_valid	= sprd_dpu_mode_valid,
	.atomic_begin	= sprd_dpu_atomic_begin,
	.atomic_flush	= sprd_dpu_atomic_flush,
	.atomic_enable	= sprd_dpu_atomic_enable,
	.atomic_disable	= sprd_dpu_atomic_disable,
	.enable_vblank	= sprd_dpu_enable_vblank,
	.disable_vblank	= sprd_dpu_disable_vblank,
	.prepare_fb = sprd_dpu_prepare_fb,
	.cleanup_fb = sprd_dpu_cleanup_fb,
};

void sprd_dpu_run(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	down(&ctx->lock);
	if (!ctx->enabled) {
		DRM_ERROR("dpu is not initialized\n");
		up(&ctx->lock);
		return;
	}

	if (!ctx->stopped) {
		up(&ctx->lock);
		return;
	}

	if (dpu->core->run)
		dpu->core->run(ctx);

	up(&ctx->lock);

	drm_crtc_vblank_on(&dpu->crtc->base);
}

void sprd_dpu_stop(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	down(&ctx->lock);

	if (!ctx->enabled) {
		DRM_ERROR("dpu is not initialized\n");
		up(&ctx->lock);
		return;
	}

	if (ctx->stopped) {
		up(&ctx->lock);
		return;
	}

	if (dpu->core->stop)
		dpu->core->stop(ctx);

	up(&ctx->lock);

	drm_crtc_handle_vblank(&dpu->crtc->base);
	drm_crtc_vblank_off(&dpu->crtc->base);
}

static void sprd_dpu_enable(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	down(&ctx->lock);

	if (ctx->enabled) {
		up(&ctx->lock);
		return;
	}

	if (dpu->glb->power)
		dpu->glb->power(ctx, true);
	if (dpu->glb->enable)
		dpu->glb->enable(ctx);

	if (ctx->stopped && dpu->glb->reset)
		dpu->glb->reset(ctx);

	if (dpu->clk->init)
		dpu->clk->init(ctx);
	if (dpu->clk->enable)
		dpu->clk->enable(ctx);

	if (dpu->core->init)
		dpu->core->init(ctx);
	if (dpu->core->ifconfig)
		dpu->core->ifconfig(ctx);

	ctx->enabled = true;

	up(&ctx->lock);
}

void sprd_dpu_resume(struct sprd_dpu *dpu)
{
	sprd_dpu_enable(dpu);
	enable_irq(dpu->ctx.irq);
	sprd_iommu_restore(&dpu->dev);
	DRM_INFO("dpu resume OK\n");
}

static void sprd_dpu_disable(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;

	down(&ctx->lock);
	down(&ctx->cabc_lock);
	if (!ctx->enabled) {
		up(&ctx->lock);
		up(&ctx->cabc_lock);
		return;
	}

	if (dpu->core->fini)
		dpu->core->fini(ctx);
	if (dpu->clk->disable)
		dpu->clk->disable(ctx);
	if (dpu->glb->disable)
		dpu->glb->disable(ctx);
	if (dpu->glb->power)
		dpu->glb->power(ctx, false);

	ctx->enabled = false;
	up(&ctx->cabc_lock);
	up(&ctx->lock);
}

static irqreturn_t sprd_dpu_isr(int irq, void *data)
{
	struct sprd_dpu *dpu = data;
	struct dpu_context *ctx = &dpu->ctx;
	u32 int_mask = 0;
	int_mask = dpu->core->isr(ctx);

	if (int_mask & BIT_DPU_INT_TE) {
		if (ctx->te_check_en) {
			ctx->evt_te = true;
			wake_up_interruptible_all(&ctx->te_wq);
		}
		if (ctx->if_type == SPRD_DPU_IF_EDPI)
			drm_crtc_handle_vblank(&dpu->crtc->base);
	}

	if (int_mask & BIT_DPU_INT_ERR)
		DRM_WARN("Warning: dpu underflow!\n");

	if (int_mask & BIT_DPU_INT_VSYNC)
		drm_crtc_handle_vblank(&dpu->crtc->base);

	return IRQ_HANDLED;
}

static int sprd_dpu_irq_request(struct sprd_dpu *dpu)
{
	struct dpu_context *ctx = &dpu->ctx;
	int irq_num;
	int ret;

	irq_num = irq_of_parse_and_map(dpu->dev.of_node, 0);
	if (!irq_num) {
		DRM_ERROR("error: dpu parse irq num failed\n");
		return -EINVAL;
	}
	DRM_INFO("dpu irq_num = %d\n", irq_num);

	irq_set_status_flags(irq_num, IRQ_NOAUTOEN);
	ret = devm_request_irq(&dpu->dev, irq_num, sprd_dpu_isr,
					0, "DISPC", dpu);
	if (ret) {
		DRM_ERROR("error: dpu request irq failed\n");
		return -EINVAL;
	}
	ctx->irq = irq_num;
	ctx->dpu_isr = sprd_dpu_isr;

	return 0;
}

static struct sprd_dsi *sprd_dpu_dsi_attach(struct sprd_dpu *dpu)
{
	struct device *dev;
	struct sprd_dsi *dsi;

	DRM_INFO("dpu attach dsi\n");
	dev = sprd_disp_pipe_get_output(&dpu->dev);
	if (!dev) {
		DRM_ERROR("dpu pipe get output failed\n");
		return NULL;
	}

	dsi = dev_get_drvdata(dev);
	if (!dsi) {
		DRM_ERROR("dpu attach dsi failed\n");
		return NULL;
	}

	return dsi;
}

static int sprd_dpu_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct sprd_dpu *dpu = dev_get_drvdata(dev);
	struct sprd_crtc_capability cap = {};
	struct sprd_plane *planes;

	DRM_INFO("%s()\n", __func__);

	dpu->core->version(&dpu->ctx);
	dpu->core->capability(&dpu->ctx, &cap);

	planes = sprd_plane_init(drm, &cap);
	if (IS_ERR_OR_NULL(planes))
		return PTR_ERR(planes);

	dpu->crtc = sprd_crtc_init(drm, planes, SPRD_DISPLAY_TYPE_LCD,
				&sprd_dpu_ops, dpu->ctx.version, dpu->ctx.corner_size, dpu);
	if (IS_ERR(dpu->crtc))
		return PTR_ERR(dpu->crtc);

	sprd_dpu_irq_request(dpu);

	dpu->dsi = sprd_dpu_dsi_attach(dpu);

	return 0;
}

static void sprd_dpu_unbind(struct device *dev, struct device *master,
	void *data)
{
	struct sprd_dpu *dpu = dev_get_drvdata(dev);

	DRM_INFO("%s()\n", __func__);

	drm_crtc_cleanup(&dpu->crtc->base);
}

static const struct component_ops dpu_component_ops = {
	.bind = sprd_dpu_bind,
	.unbind = sprd_dpu_unbind,
};

static int sprd_dpu_device_create(struct sprd_dpu *dpu,
				struct device *parent)
{
	int ret;

	dpu->dev.class = display_class;
	dpu->dev.parent = parent;
	dpu->dev.of_node = parent->of_node;
	dev_set_name(&dpu->dev, "dispc0");
	dev_set_drvdata(&dpu->dev, dpu);

	ret = device_register(&dpu->dev);
	if (ret) {
		DRM_ERROR("dpu device register failed\n");
		return ret;
	}

	return 0;
}

static int of_get_logo_memory_info(struct sprd_dpu *dpu,
	struct device_node *np)
{
	struct device_node *node;
	struct resource r;
	int ret;
	struct dpu_context *ctx = &dpu->ctx;

	node = of_parse_phandle(np, "sprd,logo-memory", 0);
	if (!node) {
		DRM_INFO("no sprd,logo-memory specified\n");
		return 0;
	}

	ret = of_address_to_resource(node, 0, &r);
	of_node_put(node);
	if (ret) {
		DRM_ERROR("invalid logo reserved memory node!\n");
		return -EINVAL;
	}

	ctx->logo_addr = r.start;
	ctx->logo_size = resource_size(&r);

	return 0;
}

static int sprd_dpu_context_init(struct sprd_dpu *dpu,
				struct device_node *np)
{
	struct resource r;
	struct dpu_context *ctx = &dpu->ctx;
	int ret;

	if (dpu->core->context_init) {
		ret = dpu->core->context_init(ctx, np);
		if (ret)
			return ret;
	}

	if (dpu->clk->parse_dt)
		dpu->clk->parse_dt(ctx, np);
	if (dpu->glb->parse_dt)
		dpu->glb->parse_dt(ctx, np);

	if (of_address_to_resource(np, 0, &r)) {
		DRM_ERROR("parse dt base address failed\n");
		return -ENODEV;
	}
	ctx->base = ioremap(r.start, resource_size(&r));
	if (!ctx->base) {
		DRM_ERROR("ioremap base address failed\n");
		return -EFAULT;
	}

	of_get_logo_memory_info(dpu, np);

	sema_init(&ctx->lock, 1);
	sema_init(&ctx->cabc_lock, 1);
	init_waitqueue_head(&ctx->wait_queue);

	ctx->panel_ready = true;
	ctx->time = 5000;
	ctx->secure_debug = false;

	init_waitqueue_head(&dpu->ctx.te_wq);

	return 0;
}

static const struct sprd_dpu_ops sharkle_dpu = {
	.core = &dpu_lite_r1p0_core_ops,
	.clk = &sharkle_dpu_clk_ops,
	.glb = &sharkle_dpu_glb_ops,
};

static const struct sprd_dpu_ops pike2_dpu = {
	.core = &dpu_lite_r1p0_core_ops,
	.clk = &pike2_dpu_clk_ops,
	.glb = &pike2_dpu_glb_ops,
};

static const struct sprd_dpu_ops sharkl3_dpu = {
	.core = &dpu_r2p0_core_ops,
	.clk = &sharkl3_dpu_clk_ops,
	.glb = &sharkl3_dpu_glb_ops,
};

static const struct sprd_dpu_ops sharkl5_dpu = {
	.core = &dpu_lite_r2p0_core_ops,
	.clk = &sharkl5_dpu_clk_ops,
	.glb = &sharkl5_dpu_glb_ops,
};

static const struct sprd_dpu_ops sharkl5pro_dpu = {
	.core = &dpu_r4p0_core_ops,
	.clk = &sharkl5pro_dpu_clk_ops,
	.glb = &sharkl5pro_dpu_glb_ops,
};

static const struct sprd_dpu_ops qogirl6_dpu = {
	.core = &dpu_r5p0_core_ops,
	.clk = &qogirl6_dpu_clk_ops,
	.glb = &qogirl6_dpu_glb_ops,
};

static const struct sprd_dpu_ops qogirn6pro_dpu = {
	.core = &dpu_r6p0_core_ops ,
	.clk = &qogirn6pro_dpu_clk_ops,
	.glb = &qogirn6pro_dpu_glb_ops,
};

static const struct of_device_id dpu_match_table[] = {
	{ .compatible = "sprd,sharkle-dpu",
	  .data = &sharkle_dpu },
	{ .compatible = "sprd,pike2-dpu",
	  .data = &pike2_dpu },
	{ .compatible = "sprd,sharkl3-dpu",
	  .data = &sharkl3_dpu },
	{ .compatible = "sprd,sharkl5-dpu",
	  .data = &sharkl5_dpu },
	{ .compatible = "sprd,sharkl5pro-dpu",
	  .data = &sharkl5pro_dpu },
	{ .compatible = "sprd,qogirl6-dpu",
	  .data = &qogirl6_dpu },
	{ .compatible = "sprd,qogirn6pro-dpu",
	  .data = &qogirn6pro_dpu },
	{ /* sentinel */ },
};

static int sprd_dpu_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct sprd_dpu_ops *pdata;
	struct sprd_dpu *dpu;
	int ret;

	dpu = devm_kzalloc(&pdev->dev, sizeof(*dpu), GFP_KERNEL);
	if (!dpu)
		return -ENOMEM;

	pdata = of_device_get_match_data(&pdev->dev);
	if (pdata) {
		dpu->core = pdata->core;
		dpu->clk = pdata->clk;
		dpu->glb = pdata->glb;
	} else {
		DRM_ERROR("No matching driver data found\n");
		return -EINVAL;
	}

	ret = sprd_dpu_context_init(dpu, np);
	if (ret)
		return ret;

	ret = sprd_dpu_device_create(dpu, &pdev->dev);
	if (ret)
		return ret;

	ret = sprd_dpu_sysfs_init(&dpu->dev);
	if (ret)
	 	return ret;

	platform_set_drvdata(pdev, dpu);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return component_add(&pdev->dev, &dpu_component_ops);
}

static int sprd_dpu_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dpu_component_ops);
	return 0;
}

struct platform_driver sprd_dpu_driver = {
	.probe = sprd_dpu_probe,
	.remove = sprd_dpu_remove,
	.driver = {
		.name = "sprd-dpu-drv",
		.of_match_table = dpu_match_table,
	},
};

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_AUTHOR("Kevin Tang <kevin.tang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc Display Controller Driver");
MODULE_LICENSE("GPL v2");
