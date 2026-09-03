// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/component.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/time.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_vblank.h>

#include "sprd_drm.h"

struct dummy_crtc {
	struct drm_crtc crtc;
	struct drm_pending_vblank_event *event;
	struct hrtimer vsync_timer;
};

static inline struct dummy_crtc *crtc_to_dummy(struct drm_crtc *crtc)
{
	return crtc ? container_of(crtc, struct dummy_crtc, crtc) : NULL;
}

static enum hrtimer_restart vsync_timer_func(struct hrtimer *timer)
{
	struct dummy_crtc *dummy = container_of(timer, struct dummy_crtc,
						vsync_timer);
	drm_crtc_handle_vblank(&dummy->crtc);

	hrtimer_forward_now(timer, ns_to_ktime(16666666));

	return HRTIMER_RESTART;
}

static void sprd_dummy_plane_atomic_update(struct drm_plane *plane,
				    struct drm_atomic_state *state)
{
	DRM_DEBUG("\n");
}

static const struct drm_plane_helper_funcs sprd_dummy_plane_helper_funcs = {
	.atomic_update = sprd_dummy_plane_atomic_update,
};

static const struct drm_plane_funcs sprd_dummy_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static struct drm_plane *sprd_dummy_plane_init(struct drm_device *drm,
					struct dummy_crtc *dummy)
{
	const u32 primary_fmts[] = {
		DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
		DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	};
	struct drm_plane *plane;
	int err;

	plane = kzalloc(sizeof(*plane), GFP_KERNEL);
	if (!plane)
		return ERR_PTR(-ENOMEM);

	err = drm_universal_plane_init(drm, plane, 1,
				       &sprd_dummy_plane_funcs, primary_fmts,
				       ARRAY_SIZE(primary_fmts), NULL,
				       DRM_PLANE_TYPE_PRIMARY, NULL);
	if (err) {
		kfree(plane);
		DRM_ERROR("fail to init primary plane\n");
		return ERR_PTR(err);
	}

	drm_plane_helper_add(plane, &sprd_dummy_plane_helper_funcs);

	return plane;
}

static void sprd_dummy_crtc_atomic_enable(struct drm_crtc *crtc,
				   struct drm_atomic_state *old_state)
{
	DRM_INFO("%s()\n", __func__);
}

static void sprd_dummy_crtc_atomic_disable(struct drm_crtc *crtc,
				   struct drm_atomic_state *state)
{
	spin_lock_irq(&crtc->dev->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&crtc->dev->event_lock);

	DRM_INFO("%s()\n", __func__);
}

static void sprd_dummy_crtc_atomic_flush(struct drm_crtc *crtc,
				  struct drm_atomic_state *old_state)

{
	struct dummy_crtc *dummy = crtc_to_dummy(crtc);
	struct drm_device *drm = dummy->crtc.dev;

	DRM_DEBUG("%s()\n", __func__);

	spin_lock_irq(&drm->event_lock);
	if (crtc->state->event) {
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
	}
	spin_unlock_irq(&drm->event_lock);
}

static int sprd_dummy_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct dummy_crtc *dummy = crtc_to_dummy(crtc);

	DRM_INFO("%s()\n", __func__);

	hrtimer_start(&dummy->vsync_timer, ns_to_ktime(16666666),
		      HRTIMER_MODE_REL);

	return 0;
}

static void sprd_dummy_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct dummy_crtc *dummy = crtc_to_dummy(crtc);

	DRM_INFO("%s()\n", __func__);

	hrtimer_cancel(&dummy->vsync_timer);
}

static const struct drm_crtc_helper_funcs sprd_dummy_crtc_helper_funcs = {
	.atomic_enable	= sprd_dummy_crtc_atomic_enable,
	.atomic_disable	= sprd_dummy_crtc_atomic_disable,
	.atomic_flush = sprd_dummy_crtc_atomic_flush,
};

static const struct drm_crtc_funcs sprd_dummy_crtc_funcs = {
	.destroy	= drm_crtc_cleanup,
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.reset		= drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state	= drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_crtc_destroy_state,
	.enable_vblank	= sprd_dummy_crtc_enable_vblank,
	.disable_vblank	= sprd_dummy_crtc_disable_vblank,
};

static int sprd_dummy_crtc_init(struct drm_device *drm, struct drm_crtc *crtc,
			 struct drm_plane *primary)
{
	struct device_node *port;
	int err;

	/*
	 * set crtc port so that drm_of_find_possible_crtcs call works
	 */
	port = of_parse_phandle(drm->dev->of_node, "ports", 0);
	if (!port) {
		DRM_ERROR("find 'ports' phandle of %s failed\n",
			  drm->dev->of_node->full_name);
		return -EINVAL;
	}
	of_node_put(port);
	crtc->port = port;

	err = drm_crtc_init_with_planes(drm, crtc, primary, NULL,
					&sprd_dummy_crtc_funcs, "dummy-crtc");
	if (err) {
		DRM_ERROR("failed to init crtc.\n");
		return err;
	}

	drm_crtc_helper_add(crtc, &sprd_dummy_crtc_helper_funcs);

	return 0;
}

static int sprd_dummy_crtc_bind(struct device *dev, struct device *master,
				void *data)
{
	struct drm_device *drm = data;
	struct dummy_crtc *dummy = dev_get_drvdata(dev);
	struct drm_plane *primary;
	int err;

	primary = sprd_dummy_plane_init(drm, dummy);
	if (IS_ERR_OR_NULL(primary)) {
		err = PTR_ERR(primary);
		return err;
	}

	err = sprd_dummy_crtc_init(drm, &dummy->crtc, primary);
	if (err)
		return err;

	return 0;
}

static void sprd_dummy_crtc_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct dummy_crtc *dummy = dev_get_drvdata(dev);

	drm_crtc_cleanup(&dummy->crtc);
}

static const struct component_ops dummy_component_ops = {
	.bind = sprd_dummy_crtc_bind,
	.unbind = sprd_dummy_crtc_unbind,
};

static int sprd_dummy_crtc_probe(struct platform_device *pdev)
{
	struct dummy_crtc *dummy;

	dummy = devm_kzalloc(&pdev->dev, sizeof(*dummy), GFP_KERNEL);
	if (!dummy)
		return -ENOMEM;

	hrtimer_init(&dummy->vsync_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	dummy->vsync_timer.function = vsync_timer_func;

	platform_set_drvdata(pdev, dummy);

	return component_add(&pdev->dev, &dummy_component_ops);
}

static int sprd_dummy_crtc_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dummy_component_ops);
	return 0;
}

static const struct of_device_id dummy_crtc_match_table[] = {
	{ .compatible = "sprd,dummy-crtc", },
	{ /* sentinel */ },
};

struct platform_driver sprd_dummy_crtc_driver = {
	.probe = sprd_dummy_crtc_probe,
	.remove = sprd_dummy_crtc_remove,
	.driver = {
		.name = "sprd-dummy-crtc-drv",
		.of_match_table = dummy_crtc_match_table,
	},
};

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Dummy CRTC Driver for Unisoc");
MODULE_LICENSE("GPL v2");
