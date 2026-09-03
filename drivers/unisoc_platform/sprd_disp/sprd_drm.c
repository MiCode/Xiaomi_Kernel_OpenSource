// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "sprd_drm.h"
#include "sprd_drm_gsp.h"
#include "sprd_gem.h"
#include "sprd_dpu.h"
#include "sprd_dsi.h"
#include "sysfs/sysfs_display.h"

#include <trace/hooks/drm_atomic.h>
#include <trace/hooks/drm_framebuffer.h>

#include <uapi/drm/sprd_drm_gsp.h>

#define DRIVER_NAME	"sprd"
#define DRIVER_DESC	"Spreadtrum SoCs' DRM Driver"
#define DRIVER_DATE	"20200201"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0

#define SPRD_FENCE_WAIT_TIMEOUT 3000 /* ms */

static void sprd_drm_atomic_check_modeset(void *data, struct drm_atomic_state *state,
				struct drm_crtc *crtc, bool *allow)
{
	if(allow)
		*allow = true;
}

static void sprd_atomic_remove_fb(void *data, struct drm_framebuffer *fb, bool *allow)
{
	if(allow)
		*allow = true;
}

static bool boot_mode_check(const char *str)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int rc;

	cmdline_node = of_find_node_by_path("/chosen");
	rc = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (rc)
		return false;

	if (!strstr(cmd_line, str))
		return false;

	return true;
}

/**
 * sprd_atomic_wait_for_fences - wait for fences stashed in plane state
 * @dev: DRM device
 * @state: atomic state object with old state structures
 * @pre_swap: If true, do an interruptible wait, and @state is the new state.
 * Otherwise @state is the old state.
 *
 * For implicit sync, driver should fish the exclusive fence out from the
 * incoming fb's and stash it in the drm_plane_state.  This is called after
 * drm_atomic_helper_swap_state() so it uses the current plane state (and
 * just uses the atomic state to find the changed planes)
 *
 * Note that @pre_swap is needed since the point where we block for fences moves
 * around depending upon whether an atomic commit is blocking or
 * non-blocking. For non-blocking commit all waiting needs to happen after
 * drm_atomic_helper_swap_state() is called, but for blocking commits we want
 * to wait **before** we do anything that can't be easily rolled back. That is
 * before we call drm_atomic_helper_swap_state().
 *
 * Returns zero if success or < 0 if dma_fence_wait_timeout() fails.
 */
int sprd_atomic_wait_for_fences(struct drm_device *dev,
				      struct drm_atomic_state *state,
				      bool pre_swap)
{
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	int i, ret;

	for_each_new_plane_in_state(state, plane, new_plane_state, i) {
		if (!new_plane_state->fence)
			continue;

		WARN_ON(!new_plane_state->fb);

		/*
		 * If waiting for fences pre-swap (ie: nonblock), userspace can
		 * still interrupt the operation. Instead of blocking until the
		 * timer expires, make the wait interruptible.
		 */
		ret = dma_fence_wait_timeout(new_plane_state->fence,
				pre_swap,
				msecs_to_jiffies(SPRD_FENCE_WAIT_TIMEOUT));
		if (ret == 0) {
			DRM_ERROR("wait fence timed out, index:%d,\n", i);
			return -EBUSY;
		} else if (ret < 0) {
			DRM_ERROR("wait fence failed, index:%d, ret:%d.\n",
				i, ret);
			return ret;
		}

		dma_fence_put(new_plane_state->fence);
		new_plane_state->fence = NULL;
	}

	return 0;
}

static void sprd_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;

	drm_atomic_helper_wait_for_dependencies(old_state);

	sprd_atomic_wait_for_fences(dev, old_state, false);

	drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state,
					DRM_PLANE_COMMIT_ACTIVE_ONLY);

	drm_atomic_helper_commit_hw_done(old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);

	drm_atomic_helper_commit_cleanup_done(old_state);

	drm_atomic_state_put(old_state);
}

static void sprd_commit_work(struct work_struct *work)
{
	struct drm_atomic_state *state = container_of(work,
						      struct drm_atomic_state,
						      commit_work);
	sprd_commit_tail(state);
}

int sprd_atomic_helper_commit(struct drm_device *dev,
			struct drm_atomic_state *state, bool nonblock)
{
	int ret;
	/*
	 * FIXME:
	 * In some extreme scenes, there will be FPS drops or screen freeze,
	 * it's may be due to poor gpu capabilities or rendering heavy loads.
	 * If nonblock flag is true, userspace is not allowed to get ahead of the
	 * previous commit with nonblocking ones, it maybe cause drm atomic_commit
	 * pipeline occur performance issues.
	 *
	 * -EBUSY when userspace schedules nonblocking commits too fast, that's
	 * not the case for us, maybe caused by FPS drops or screen freezes.
	 */
	ret = drm_atomic_helper_setup_commit(state, false);
	if (ret)
		return ret;

	INIT_WORK(&state->commit_work, sprd_commit_work);

	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret)
		return ret;

	ret = drm_atomic_helper_swap_state(state, true);
	if (ret)
		goto err;

	drm_atomic_state_get(state);
	if (nonblock)
		queue_work(system_unbound_wq, &state->commit_work);
	else
		sprd_commit_tail(state);

	return 0;

err:
	drm_atomic_helper_cleanup_planes(dev, state);
	return ret;
}

static const struct drm_mode_config_funcs sprd_drm_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = sprd_atomic_helper_commit,
};

static void sprd_drm_mode_config_init(struct drm_device *drm)
{
	drm_mode_config_init(drm);

	drm->mode_config.min_width = 0;
	drm->mode_config.min_height = 0;
	drm->mode_config.max_width = 8192;
	drm->mode_config.max_height = 8192;
	drm->mode_config.allow_fb_modifiers = false;

	drm->mode_config.funcs = &sprd_drm_mode_config_funcs;
}

static const struct drm_ioctl_desc sprd_ioctls[] = {
	DRM_IOCTL_DEF_DRV(SPRD_GSP_GET_CAPABILITY,
			sprd_gsp_get_capability_ioctl, 0),
	DRM_IOCTL_DEF_DRV(SPRD_GSP_TRIGGER,
			sprd_gsp_trigger_ioctl, 0),
};

static const struct file_operations sprd_drm_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
// #ifdef CONFIG_COMPAT
// 	.compat_ioctl	= sprd_compat_ioctl,
// #endif
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= no_llseek,
	.mmap		= sprd_gem_mmap,
};

static struct drm_driver sprd_drm_drv = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.fops			= &sprd_drm_fops,
	.dumb_create		= sprd_gem_dumb_create,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_import_sg_table = sprd_gem_prime_import_sg_table,

	.ioctls			= sprd_ioctls,
	.num_ioctls		= ARRAY_SIZE(sprd_ioctls),

	.name			= DRIVER_NAME,
	.desc			= DRIVER_DESC,
	.date			= DRIVER_DATE,
	.major			= DRIVER_MAJOR,
	.minor			= DRIVER_MINOR,
};

static int sprd_drm_bind(struct device *dev)
{
	struct drm_device *drm;
	struct sprd_drm *sprd;
	int err;

	DRM_INFO("%s()\n", __func__);

	drm = drm_dev_alloc(&sprd_drm_drv, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	dev_set_drvdata(dev, drm);

	sprd = devm_kzalloc(drm->dev, sizeof(*sprd), GFP_KERNEL);
	if (!sprd) {
		err = -ENOMEM;
		goto err_free_drm;
	}

	drm->dev_private = sprd;

	/* get the optional framebuffer memory resource */
	err = of_reserved_mem_device_init_by_idx(drm->dev,
				drm->dev->of_node, 0);
	if (err && err != -ENODEV) {
		DRM_ERROR("failed to obtain reserved memory\n");
		goto err_free_drm;
	}

	sprd_drm_mode_config_init(drm);

	/* bind and init sub drivers */
	err = component_bind_all(drm->dev, drm);
	if (err) {
		DRM_ERROR("failed to bind all component.\n");
		goto err_dc_cleanup;
	}

	/* vblank init */
	err = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (err) {
		DRM_ERROR("failed to initialize vblank.\n");
		goto err_unbind_all;
	}

	/* reset all the states of crtc/plane/encoder/connector */
	drm_mode_config_reset(drm);

	/* init kms poll for handling hpd */
	drm_kms_helper_poll_init(drm);

	err = drm_dev_register(drm, 0);
	if (err < 0)
		goto err_kms_helper_poll_fini;

	return 0;

err_kms_helper_poll_fini:
	drm_kms_helper_poll_fini(drm);
err_unbind_all:
	component_unbind_all(drm->dev, drm);
err_dc_cleanup:
	drm_mode_config_cleanup(drm);
	of_reserved_mem_device_release(drm->dev);
err_free_drm:
	drm_dev_put(drm);
	return err;
}

static void sprd_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	DRM_INFO("%s()\n", __func__);

	drm_dev_unregister(drm);

	drm_kms_helper_poll_fini(drm);

	drm_mode_config_cleanup(drm);

	component_unbind_all(drm->dev, drm);

	of_reserved_mem_device_release(drm->dev);

	drm_dev_put(drm);
}

static const struct component_master_ops drm_component_ops = {
	.bind = sprd_drm_bind,
	.unbind = sprd_drm_unbind,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int sprd_drm_component_probe(struct device *dev,
			const struct component_master_ops *m_ops)
{
	struct device_node *ep, *port, *remote;
	struct component_match *match = NULL;
	int i;

	if (!dev->of_node)
		return -EINVAL;

	/*
	 * Bind the crtc's ports first, so that drm_of_find_possible_crtcs()
	 * called from encoder's .bind callbacks works as expected
	 */
	for (i = 0; ; i++) {
		port = of_parse_phandle(dev->of_node, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		component_match_add(dev, &match, compare_of, port->parent);
		of_node_put(port);
	}

	if (i == 0) {
		dev_err(dev, "missing 'ports' property\n");
		return -ENODEV;
	}

	if (!match) {
		dev_err(dev, "no available port\n");
		return -ENODEV;
	}

	/*
	 * For bound crtcs, bind the encoders attached to their remote endpoint
	 */
	for (i = 0; ; i++) {
		port = of_parse_phandle(dev->of_node, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		for_each_child_of_node(port, ep) {
			remote = of_graph_get_remote_port_parent(ep);
			if (!remote || !of_device_is_available(remote)) {
				of_node_put(remote);
				continue;
			} else if (!of_device_is_available(remote->parent)) {
				dev_warn(dev, "parent device of %s is not available\n",
					 remote->full_name);
				of_node_put(remote);
				continue;
			}

			component_match_add(dev, &match, compare_of, remote);
			of_node_put(remote);
		}
		of_node_put(port);
	}
	if (IS_ENABLED(CONFIG_UNISOC_GSP)) {
		for (i = 0; ; i++) {
			port = of_parse_phandle(dev->of_node, "gsp", i);
			if (!port)
				break;

			if (!of_device_is_available(port->parent)) {
				of_node_put(port);
				continue;
			}

			component_match_add(dev, &match, compare_of, port);
			of_node_put(port);
		}
	}
	return component_master_add_with_match(dev, m_ops, match);
}

static int sprd_drm_probe(struct platform_device *pdev)
{
	int ret;
	DRM_INFO("%s()\n", __func__);
	ret = dma_set_mask_and_coherent(&pdev->dev, ~0);
	if (ret) {
		DRM_ERROR("dma_set_mask_and_coherent failed (%d)\n", ret);
		return ret;
	}

	// ret = drm_of_component_probe(&pdev->dev, compare_of, &drm_component_ops);
	// if(ret) {
	// 	DRM_ERROR("sprd_drm_probe error: %d\n", ret);
	// 	return ret;
	// }

	return sprd_drm_component_probe(&pdev->dev, &drm_component_ops);
}

static int sprd_drm_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &drm_component_ops);
	return 0;
}

static void sprd_drm_shutdown(struct platform_device *pdev)
{
	struct drm_device *drm = dev_get_drvdata(&pdev->dev);

	if (!drm) {
		DRM_WARN("drm device is not available, no shutdown\n");
		return;
	}

	drm_atomic_helper_shutdown(drm);
}

static int sprd_drm_pm_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct drm_atomic_state *state;
	struct sprd_drm *sprd;
	struct drm_crtc *crtc;
	struct drm_encoder *encoder;
	static bool is_suspend;

	if (!drm) {
		DRM_WARN("drm device is not available, no suspend\n");
		return 0;
	}

	DRM_INFO("%s()\n", __func__);

	if (boot_mode_check("androidboot.mode=autotest")) {
		if (is_suspend)
			return 0;

		drm_for_each_crtc(crtc, drm) {
			if (!crtc->state->active) {
				/* crtc force power down! */
				sprd_dpu_atomic_disable_force(crtc);

				/* encoder force power down! */
				drm_for_each_encoder(encoder, drm) {
					sprd_dsi_encoder_disable_force(encoder);
				}

				is_suspend = true; /* For BBAT deep sleep */
				return 0;
			}
		}
		is_suspend = true; /* For BBAT display test */
	}

	drm_kms_helper_poll_disable(drm);

	state = drm_atomic_helper_suspend(drm);
	if (IS_ERR(state)) {
		drm_kms_helper_poll_enable(drm);
		DRM_WARN("suspend fail\n");
		return PTR_ERR(state);
	}

	sprd = drm->dev_private;
	sprd->state = state;

	return 0;
}

static int sprd_drm_pm_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct sprd_drm *sprd;

	if (!drm) {
		DRM_WARN("drm device is not available, no resume\n");
		return 0;
	}

	if (boot_mode_check("androidboot.mode=autotest")) {
		DRM_WARN("BBAT mode not need resume\n");
		return 0;
	}

	DRM_INFO("%s()\n", __func__);

	sprd = drm->dev_private;
	if (sprd->state) {
		drm_atomic_helper_resume(drm, sprd->state);
		drm_kms_helper_poll_enable(drm);
		sprd->state = NULL;
	}

	return 0;
}

static const struct dev_pm_ops sprd_drm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_drm_pm_suspend, sprd_drm_pm_resume)
};

static const struct of_device_id drm_match_table[] = {
	{ .compatible = "sprd,display-subsystem", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, drm_match_table);

static struct platform_driver sprd_drm_driver = {
	.probe = sprd_drm_probe,
	.remove = sprd_drm_remove,
	.shutdown = sprd_drm_shutdown,
	.driver = {
		.name = "sprd-drm-drv",
		.of_match_table = drm_match_table,
		.pm = &sprd_drm_pm_ops,
	},
};

static struct platform_driver *sprd_drm_drivers[]  = {
#ifdef CONFIG_DRM_SPRD_DUMMY
	&sprd_dummy_crtc_driver,
	&sprd_dummy_connector_driver,
#endif
#ifdef CONFIG_DRM_SPRD_DPU0
	&sprd_dpu_driver,
	&sprd_backlight_driver,
#endif
#ifdef CONFIG_DRM_SPRD_DSI
	&sprd_dsi_driver,
	&sprd_dphy_driver,
#endif
	&sprd_drm_driver,
};

static int __init sprd_drm_init(void)
{
	int ret;
	bool cali_mode;

	cali_mode = boot_mode_check("androidboot.mode=cali");
	if (cali_mode) {
		DRM_WARN("Calibration Mode! Don't register sprd drm driver");
		return 0;
	}

	ret = sprd_display_class_init();
	if (ret)
		return ret;

	ret = platform_register_drivers(sprd_drm_drivers,
					ARRAY_SIZE(sprd_drm_drivers));
	if (ret)
		return ret;

#ifdef CONFIG_DRM_SPRD_DSI
	mipi_dsi_driver_register(&sprd_panel_driver);
#endif

	ret = register_trace_android_vh_drm_atomic_check_modeset(
			sprd_drm_atomic_check_modeset, NULL);
	if (ret)
		return ret;

	ret = register_trace_android_vh_atomic_remove_fb(
			sprd_atomic_remove_fb, NULL);
	if (ret)
		return ret;

	return 0;
}

static void __exit sprd_drm_exit(void)
{
#ifdef CONFIG_DRM_SPRD_DSI
	mipi_dsi_driver_unregister(&sprd_panel_driver);
#endif

	platform_unregister_drivers(sprd_drm_drivers,
				    ARRAY_SIZE(sprd_drm_drivers));
}

module_init(sprd_drm_init);
module_exit(sprd_drm_exit);

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_AUTHOR("Kevin Tang <kevin.tang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc DRM KMS Master Driver");
MODULE_LICENSE("GPL v2");
