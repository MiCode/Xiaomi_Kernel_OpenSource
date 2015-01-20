/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <drm/i915_adf.h>
#include <intel_adf.h>

static const struct intel_adf_context *g_adf_context;

static struct intel_adf_interface *create_interfaces(
	struct intel_adf_device *dev,
	struct intel_pipe **pipes, size_t n_pipes)
{
	struct intel_adf_interface *intfs;
	int err;
	size_t i;
	u32 intf_idx = 0;

	intfs = kzalloc(n_pipes * sizeof(*intfs), GFP_KERNEL);
	if (!intfs)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < n_pipes; i++) {
		err = intel_adf_interface_init(&intfs[i], dev, pipes[i],
						intf_idx++);
		if (err)
			goto out_err0;
	}
	return intfs;
out_err0:
	for (; i >= 0; i--)
		intel_adf_interface_destroy(&intfs[i]);
	kfree(intfs);
	return ERR_PTR(err);
}

static void destroy_interfaces(struct intel_adf_interface *intfs,
	size_t n_intfs)
{
	size_t i;

	for (i = 0; i < n_intfs; i++)
		intel_adf_interface_destroy(&intfs[i]);
	kfree(intfs);
}

static struct intel_adf_overlay_engine *create_overlay_engines(
	struct intel_adf_device *dev,
	struct intel_plane **planes, size_t n_planes)
{
	struct intel_adf_overlay_engine *engs;
	int err;
	size_t i;

	engs = kzalloc(n_planes * sizeof(*engs), GFP_KERNEL);
	if (!engs)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < n_planes; i++) {
		err = intel_adf_overlay_engine_init(&engs[i], dev, planes[i]);
		if (err)
			goto out_err0;
	}
	return engs;
out_err0:
	for (; i >= 0; i--)
		intel_adf_overlay_engine_destroy(&engs[i]);
	kfree(engs);
	return ERR_PTR(err);
}

static void destroy_overlay_engines(struct intel_adf_overlay_engine *engs,
	size_t n_engs)
{
	int i;

	for (i = 0; i < n_engs; i++)
		intel_adf_overlay_engine_destroy(&engs[i]);
	kfree(engs);
}

static int create_attachments(struct intel_adf_device *dev,
	struct intel_adf_interface *intfs, size_t n_intfs,
	struct intel_adf_overlay_engine *engs, size_t n_engs,
	const struct intel_dc_attachment *allowed_attachments,
	size_t n_allowed_attachments)
{
	u8 pipe_id, plane_id;
	int err;
	u8 max_overlay_eng_per_intf;

	pipe_id = 0;
	plane_id = 0;

	/* for VLV & CHV, each interface has max of 3 overlay engines */
	max_overlay_eng_per_intf = INTEL_ADF_MAX_OVERLAY_ENG_PER_INTF;

	while (n_intfs--) {
		while (max_overlay_eng_per_intf-- && n_engs--) {
			err = adf_attachment_allow(&dev->base,
						   &engs[plane_id++].base,
						   &intfs[pipe_id].base);
			if (err) {
				pr_err("ADF:%s error: %d\n", __func__, err);
				return err;
			}
		}
		pipe_id++;
		/* Initialised again to 3 for the other interface if active */
		max_overlay_eng_per_intf = INTEL_ADF_MAX_OVERLAY_ENG_PER_INTF;
	}
	return 0;
}

#if defined(CONFIG_ADF_FBDEV) && defined(CONFIG_ADF_INTEL_FBDEV)
static struct adf_fbdev *create_fbdevs(struct intel_adf_context *ctx)
{
	struct intel_adf_device *dev = ctx->dev;
	struct intel_adf_interface *intfs = ctx->intfs;
	struct intel_adf_overlay_engine *engs = ctx->engs;
	size_t n_intfs = ctx->n_intfs;
	size_t n_engs = ctx->n_engs;

	struct adf_fbdev *fbdevs;
	struct intel_pipe *pipe;
	const struct intel_plane *primary_plane;
	int err;
	int i;

	fbdevs = kzalloc(n_intfs * sizeof(*fbdevs), GFP_KERNEL);
	if (!fbdevs)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < n_intfs; i++) {
		pipe = intfs[i].pipe;
		if (!pipe || !pipe->ops || !pipe->ops->is_screen_connected ||
			!pipe->primary_plane) {
			dev_err(dev->base.dev, "%s: invalid pipe\n", __func__);
			err = -EINVAL;
			goto out_err0;
		}

		/*if screen was disconnected, don't create fbdev for this intf*/
		if (!pipe->ops->is_screen_connected(pipe))
			continue;

		primary_plane = pipe->primary_plane;
		if (primary_plane->base.idx >= n_engs) {
			dev_err(dev->base.dev, "%s: invalid plane\n", __func__);
			err = -EINVAL;
			goto out_err0;
		}

		err = intel_adf_fbdev_init(&fbdevs[i], &intfs[i],
				&engs[primary_plane->base.idx]);
		if (err) {
			dev_err(dev->base.dev, "%s: failed to init fbdev %d\n",
				__func__, i);
			goto out_err0;
		}
	}
	ctx->fbdevs = fbdevs;
	ctx->n_fbdevs = n_intfs;
	return fbdevs;
out_err0:
	for (; i >= 0; i--)
		intel_adf_fbdev_destroy(&fbdevs[i]);
	kfree(fbdevs);
	return ERR_PTR(err);
}

static void destroy_fbdevs(struct adf_fbdev *fbdevs, size_t n_fbdevs)
{
	size_t i;
	for (i = 0; i < n_fbdevs; i++)
		intel_adf_fbdev_destroy(&fbdevs[i]);
	kfree(fbdevs);
}
#endif

void intel_adf_context_destroy(struct intel_adf_context *ctx)
{

	if (!ctx)
		return;
#if defined(CONFIG_ADF_FBDEV) && defined(CONFIG_ADF_INTEL_FBDEV)
	if (ctx->fbdevs)
		destroy_fbdevs(ctx->fbdevs, ctx->n_fbdevs);
#endif
	if (ctx->engs)
		destroy_overlay_engines(ctx->engs, ctx->n_engs);
	if (ctx->intfs)
		destroy_interfaces(ctx->intfs, ctx->n_intfs);
	if (ctx->dev)
		intel_adf_device_destroy(ctx->dev);
	if (ctx->dc_config)
		intel_adf_destroy_config(ctx->dc_config);

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	if (ctx->bl_dev)
		backlight_exit(ctx->bl_dev);
#endif

	kfree(ctx);
	g_adf_context = NULL;
}

struct intel_adf_context *intel_adf_context_create(struct pci_dev *pdev)
{
	struct intel_adf_context *ctx;
	struct intel_adf_device *dev;
	struct intel_dc_config *config;
	struct intel_adf_interface *intfs;
	struct intel_adf_overlay_engine *engs;
	int n_intfs, n_engs;
#if defined(CONFIG_ADF_FBDEV) && defined(CONFIG_ADF_INTEL_FBDEV)
	struct adf_fbdev *fbdevs;
#endif
	u32 platform_id;
	int err;

	platform_id = (u32) intel_adf_get_platform_id();
	if (!pdev)
		return ERR_PTR(-EINVAL);

	/*create ADF context*/
	ctx = kzalloc(sizeof(struct intel_adf_context), GFP_KERNEL);
	if (!ctx) {
		err = -ENOMEM;
		goto err;
	}

	/*get display controller configure of this platform*/
	config = intel_adf_get_dc_config(pdev, platform_id);
	if (IS_ERR(config)) {
		dev_err(&pdev->dev, "%s: failed to get DC config\n", __func__);
		err = PTR_ERR(config);
		goto err;
	}

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	err = backlight_init(ctx);
	if (err)
		goto err;
#endif

	/*create ADF device*/
	dev = intel_adf_device_create(pdev, config->memory);
	if (IS_ERR(dev)) {
		dev_err(&pdev->dev, "%s: failed to create adf device\n",
			__func__);
		err = PTR_ERR(dev);
		goto err;
	}

	/*create ADF interfaces*/
	n_intfs = config->n_pipes;
	intfs = create_interfaces(dev, config->pipes, n_intfs);
	if (IS_ERR(intfs)) {
		dev_err(&pdev->dev, "%s: failed to create interfaces\n",
			__func__);
		err = PTR_ERR(intfs);
		goto err;
	}

	dev_info(&pdev->dev, "%s: created interfaces %d\n", __func__, n_intfs);

	/*create ADF overlay engines*/
	n_engs = config->n_planes;
	engs = create_overlay_engines(dev, config->planes, n_engs);
	if (IS_ERR(engs)) {
		dev_err(&pdev->dev, "%s: failed to create overlay engines\n",
			__func__);
		err = PTR_ERR(engs);
		goto err;
	}

	dev_info(&pdev->dev, "%s: created engines %d\n", __func__, n_engs);

	/*create allowed attachements*/
	err = create_attachments(dev, intfs, n_intfs, engs, n_engs,
			config->allowed_attachments,
			config->n_allowed_attachments);
	if (err) {
		dev_err(&pdev->dev, "%s: failed to init allowed attachments\n",
			__func__);
		goto err;
	}

	ctx->dc_config = config;
	ctx->dev = dev;
	ctx->engs = engs;
	ctx->n_engs = n_engs;
	ctx->intfs = intfs;
	ctx->n_intfs = n_intfs;

	g_adf_context = ctx;

#if defined(CONFIG_ADF_FBDEV) && defined(CONFIG_ADF_INTEL_FBDEV)
	fbdevs = create_fbdevs(ctx);
	if (IS_ERR(fbdevs)) {
		dev_err(&pdev->dev, "%s: failed to create FB devices\n",
			__func__);
		err = PTR_ERR(fbdevs);
		goto err;
	}
#endif

	return ctx;
err:
	intel_adf_context_destroy(ctx);
	return ERR_PTR(err);
}

int intel_adf_context_on_event(void)
{
	struct intel_adf_interface *intf;
	bool handled;
	size_t i;
	int ret;

	if (!g_adf_context)
		return IRQ_NONE;

	/*return intel_adf_handle_event(g_adf_context->event_handler);*/

	pr_debug("%s\n", __func__);

	handled = false;

	for (i = 0; i < g_adf_context->n_intfs; i++) {
		intf = &g_adf_context->intfs[i];
		ret = intel_adf_interface_handle_event(intf);
		if (ret == IRQ_HANDLED)
			handled = true;
	}

	/*TODO: handle fbdev create/destroy*/

	return handled ? IRQ_HANDLED : IRQ_NONE;
}
EXPORT_SYMBOL(intel_adf_context_on_event);
