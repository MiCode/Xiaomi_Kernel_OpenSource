/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: YT SHEN <yt.shen@mediatek.com>
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
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>

#include "mtk_cec.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_debugfs.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"

#define DRIVER_NAME "mediatek"
#define DRIVER_DESC "Mediatek SoC DRM"
#define DRIVER_DATE "20150513"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 0

static void mtk_atomic_schedule(struct mtk_drm_private *private,
				struct drm_atomic_state *state)
{
	private->commit.state = state;
	schedule_work(&private->commit.work);
}

static void mtk_atomic_wait_for_fences(struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	int i;

	for_each_plane_in_state(state, plane, plane_state, i)
		mtk_fb_wait(plane->state->fb);
}

static void mtk_atomic_complete(struct mtk_drm_private *private,
				struct drm_atomic_state *state)
{
	struct drm_device *drm = private->drm;

	mtk_atomic_wait_for_fences(state);

	drm_atomic_helper_commit_modeset_disables(drm, state);
	drm_atomic_helper_commit_planes(drm, state, false);
	drm_atomic_helper_commit_modeset_enables(drm, state);
	drm_atomic_helper_wait_for_vblanks(drm, state);
	drm_atomic_helper_cleanup_planes(drm, state);
	drm_atomic_state_free(state);
}

static void mtk_atomic_work(struct work_struct *work)
{
	struct mtk_drm_private *private = container_of(work,
			struct mtk_drm_private, commit.work);

	mtk_atomic_complete(private, private->commit.state);
}

static int mtk_atomic_commit(struct drm_device *drm,
			     struct drm_atomic_state *state,
			     bool async)
{
	struct mtk_drm_private *private = drm->dev_private;
	int ret;

	ret = drm_atomic_helper_prepare_planes(drm, state);
	if (ret)
		return ret;

	mutex_lock(&private->commit.lock);
	flush_work(&private->commit.work);

	drm_atomic_helper_swap_state(drm, state);

	if (async)
		mtk_atomic_schedule(private, state);
	else
		mtk_atomic_complete(private, state);

	mutex_unlock(&private->commit.lock);

	return 0;
}

static const struct drm_mode_config_funcs mtk_drm_mode_config_funcs = {
	.fb_create = mtk_drm_mode_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = mtk_atomic_commit,
#ifdef CONFIG_DRM_MEDIATEK_FBDEV
	.output_poll_changed = mtk_drm_mode_output_poll_changed,
#endif
};

static const enum mtk_ddp_comp_id mtk_ddp_main_2701[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_DSI0,
};

static const enum mtk_ddp_comp_id mtk_ddp_ext_2701[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_DSI0,
};

static const enum mtk_ddp_comp_id mtk_ddp_main_8173[] = {
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_AAL,
	DDP_COMPONENT_OD,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mtk_ddp_ext_8173[] = {
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_GAMMA,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_DPI0,
};

static struct mtk_mmsys_driver_data mt2701_mmsys_driver_data = {
	.main_path = mtk_ddp_main_2701,
	.main_len = ARRAY_SIZE(mtk_ddp_main_2701),
	.ext_path = mtk_ddp_ext_2701,
	.ext_len = ARRAY_SIZE(mtk_ddp_ext_2701),
};

static struct mtk_mmsys_driver_data mt8173_mmsys_driver_data = {
	.main_path = mtk_ddp_main_8173,
	.main_len = ARRAY_SIZE(mtk_ddp_main_8173),
	.ext_path = mtk_ddp_ext_8173,
	.ext_len = ARRAY_SIZE(mtk_ddp_ext_8173),
};

static int mtk_drm_create_properties(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;

	dev->mode_config.rotation_property =
			drm_mode_create_rotation_property(dev,
			BIT(DRM_ROTATE_0) | BIT(DRM_ROTATE_90) |
			BIT(DRM_ROTATE_180) | BIT(DRM_ROTATE_270) |
			BIT(DRM_REFLECT_X) | BIT(DRM_REFLECT_Y));
	if (!dev->mode_config.rotation_property)
		return -ENOMEM;

	priv->alpha = drm_property_create_range(dev, 0, "alpha", 0, 255);
	if (priv->alpha == NULL)
		return -ENOMEM;

	/* The color key is expressed as an RGB888 triplet stored in a 32-bit
	 * integer in XRGB8888 format. Bit 24 is used as a flag to disable (0)
	 * or enable source color keying (1).
	 */
	priv->colorkey = drm_property_create_range(dev, 0, "colorkey",
						   0, 0x01ffffff);
	if (priv->colorkey == NULL)
		return -ENOMEM;

	priv->zpos = drm_property_create_range(dev, 0, "zpos", 0, 3);
	if (priv->zpos == NULL)
		return -ENOMEM;

	return 0;
}

static int mtk_drm_kms_init(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;
	struct platform_device *pdev;
	struct device_node *np;
	int ret;

	if (!iommu_present(&platform_bus_type))
		return -EPROBE_DEFER;

	pdev = of_find_device_by_node(private->mutex_node);
	if (!pdev) {
		dev_err(drm->dev, "Waiting for disp-mutex device %s\n",
			private->mutex_node->full_name);
		of_node_put(private->mutex_node);
		return -EPROBE_DEFER;
	}
	private->mutex_dev = &pdev->dev;

	drm_mode_config_init(drm);

	drm->mode_config.min_width = 64;
	drm->mode_config.min_height = 64;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;
	drm->mode_config.funcs = &mtk_drm_mode_config_funcs;

	ret = mtk_drm_create_properties(drm);
	if (ret) {
		DRM_ERROR("failed to create properties\n");
		goto err_config_cleanup;
	}

	ret = component_bind_all(drm->dev, drm);
	if (ret)
		goto err_config_cleanup;

	/* Use OVL device for all DMA memory allocations */
	np = private->comp_node[private->data->main_path[0]] ?:
	     private->comp_node[private->data->ext_path[0]];
	pdev = of_find_device_by_node(np);
	if (!pdev) {
		ret = -ENODEV;
		dev_err(drm->dev, "Need at least one OVL device\n");
		goto err_component_unbind;
	}

	private->dma_dev = &pdev->dev;

	/*
	 * We currently support two fixed data streams, each optional,
	 * and each statically assigned to a crtc:
	 * OVL0 -> COLOR0 -> AAL -> OD -> RDMA0 -> UFOE -> DSI0 ...
	 */
	ret = mtk_drm_crtc_create(drm, private->data->main_path,
				  private->data->main_len);
	if (ret < 0)
		goto err_component_unbind;
	/* ... and OVL1 -> COLOR1 -> GAMMA -> RDMA1 -> DPI0. */
	ret = mtk_drm_crtc_create(drm, private->data->ext_path,
				  private->data->ext_len);
	if (ret < 0)
		goto err_component_unbind;

	/*
	 * We don't use the drm_irq_install() helpers provided by the DRM
	 * core, so we need to set this manually in order to allow the
	 * DRM_IOCTL_WAIT_VBLANK to operate correctly.
	 */
	drm->irq_enabled = true;
	ret = drm_vblank_init(drm, MAX_CRTC);
	if (ret < 0)
		goto err_component_unbind;

	drm_kms_helper_poll_init(drm);
	drm_mode_config_reset(drm);

	mtk_drm_debugfs_init(drm, private);

#ifdef CONFIG_DRM_MEDIATEK_FBDEV
	ret = mtk_fbdev_create(drm);
	if (ret) {
		mtk_drm_debugfs_deinit();
		drm_kms_helper_poll_fini(drm);
		goto err_vblank_cleanup;
	}
#endif

	return 0;

err_vblank_cleanup:
	drm_vblank_cleanup(drm);
err_component_unbind:
	component_unbind_all(drm->dev, drm);
err_config_cleanup:
	drm_mode_config_cleanup(drm);

	return ret;
}

static void mtk_drm_kms_deinit(struct drm_device *drm)
{
	mtk_drm_debugfs_deinit();
	drm_kms_helper_poll_fini(drm);

#ifdef CONFIG_DRM_MEDIATEK_FBDEV
	mtk_fbdev_destroy(drm);
#endif

	drm_vblank_cleanup(drm);
	component_unbind_all(drm->dev, drm);
	drm_mode_config_cleanup(drm);
}

static int mtk_drm_unload(struct drm_device *drm)
{
	mtk_drm_kms_deinit(drm);
	drm->dev_private = NULL;

	return 0;
}

static const struct vm_operations_struct mtk_drm_gem_vm_ops = {
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct file_operations mtk_drm_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = mtk_drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
};

static struct drm_driver mtk_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME |
			   DRIVER_ATOMIC,
	.unload = mtk_drm_unload,
	.set_busid = drm_platform_set_busid,

	.get_vblank_counter = drm_vblank_count,
	.enable_vblank = mtk_drm_crtc_enable_vblank,
	.disable_vblank = mtk_drm_crtc_disable_vblank,

	.gem_free_object = mtk_drm_gem_free_object,
	.gem_vm_ops = &mtk_drm_gem_vm_ops,
	.dumb_create = mtk_drm_gem_dumb_create,
	.dumb_map_offset = mtk_drm_gem_dumb_map_offset,
	.dumb_destroy = drm_gem_dumb_destroy,

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = drm_gem_prime_export,
	.gem_prime_import = drm_gem_prime_import,
	.gem_prime_get_sg_table = mtk_gem_prime_get_sg_table,
	.gem_prime_import_sg_table = mtk_gem_prime_import_sg_table,
	.gem_prime_mmap = mtk_drm_gem_mmap_buf,
	.fops = &mtk_drm_fops,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int mtk_drm_bind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm;
	int ret;

	drm = drm_dev_alloc(&mtk_drm_driver, dev);
	if (!drm)
		return -ENOMEM;

	drm_dev_set_unique(drm, dev_name(dev));

	ret = drm_dev_register(drm, 0);
	if (ret < 0)
		goto err_free;

	drm->dev_private = private;
	private->drm = drm;

	ret = mtk_drm_kms_init(drm);
	if (ret < 0)
		goto err_unregister;

	return 0;

err_unregister:
	drm_dev_unregister(drm);
err_free:
	drm_dev_unref(drm);
	return ret;
}

static void mtk_drm_unbind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	drm_put_dev(private->drm);
	private->drm = NULL;
}

static const struct component_master_ops mtk_drm_ops = {
	.bind		= mtk_drm_bind,
	.unbind		= mtk_drm_unbind,
};

static const struct of_device_id mtk_ddp_comp_dt_ids[] = {
	{ .compatible = "mediatek,mt2701-disp-ovl",   .data = (void *)MTK_DISP_OVL },
	{ .compatible = "mediatek,mt8173-disp-ovl",   .data = (void *)MTK_DISP_OVL },
	{ .compatible = "mediatek,mt2701-disp-rdma",  .data = (void *)MTK_DISP_RDMA },
	{ .compatible = "mediatek,mt8173-disp-rdma",  .data = (void *)MTK_DISP_RDMA },
	{ .compatible = "mediatek,mt2701-disp-wdma",  .data = (void *)MTK_DISP_WDMA },
	{ .compatible = "mediatek,mt8173-disp-wdma",  .data = (void *)MTK_DISP_WDMA },
	{ .compatible = "mediatek,mt2701-disp-color", .data = (void *)MTK_DISP_COLOR },
	{ .compatible = "mediatek,mt8173-disp-color", .data = (void *)MTK_DISP_COLOR },
	{ .compatible = "mediatek,mt8173-disp-aal",   .data = (void *)MTK_DISP_AAL},
	{ .compatible = "mediatek,mt8173-disp-gamma", .data = (void *)MTK_DISP_GAMMA, },
	{ .compatible = "mediatek,mt8173-disp-ufoe",  .data = (void *)MTK_DISP_UFOE },
	{ .compatible = "mediatek,mt2701-dsi",	      .data = (void *)MTK_DSI },
	{ .compatible = "mediatek,mt8173-dsi",        .data = (void *)MTK_DSI },
	{ .compatible = "mediatek,mt2701-dpi",	      .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,mt8173-dpi",        .data = (void *)MTK_DPI },
	{ .compatible = "mediatek,mt2701-disp-mutex", .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt8173-disp-mutex", .data = (void *)MTK_DISP_MUTEX },
	{ .compatible = "mediatek,mt8173-disp-pwm",   .data = (void *)MTK_DISP_PWM },
	{ .compatible = "mediatek,mt8173-disp-od",    .data = (void *)MTK_DISP_OD },
	{ .compatible = "mediatek,mt2701-disp-bls",   .data = (void *)MTK_DISP_BLS },
	{ }
};

static const struct of_device_id mtk_drm_of_ids[] = {
	{ .compatible = "mediatek,mt2701-mmsys", .data = &mt2701_mmsys_driver_data},
	{ .compatible = "mediatek,mt8173-mmsys", .data = &mt8173_mmsys_driver_data},
	{ }
};

static inline struct mtk_mmsys_driver_data *mtk_drm_get_driver_data(
	struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(mtk_drm_of_ids, &pdev->dev);

	return (struct mtk_mmsys_driver_data *)of_id->data;
}

static int mtk_drm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_drm_private *private;
	struct resource *mem;
	struct device_node *node;
	struct component_match *match = NULL;
	int ret;
	int i;

	private = devm_kzalloc(dev, sizeof(*private), GFP_KERNEL);
	if (!private)
		return -ENOMEM;

	mutex_init(&private->commit.lock);
	INIT_WORK(&private->commit.work, mtk_atomic_work);
	private->data = mtk_drm_get_driver_data(pdev);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	private->config_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->config_regs)) {
		ret = PTR_ERR(private->config_regs);
		dev_err(dev, "Failed to ioremap mmsys-config resource: %d\n",
			ret);
		return ret;
	}

	/* Iterate over sibling DISP function blocks */
	for_each_child_of_node(dev->of_node->parent, node) {
		const struct of_device_id *of_id;
		enum mtk_ddp_comp_type comp_type;
		int comp_id;

		of_id = of_match_node(mtk_ddp_comp_dt_ids, node);
		if (!of_id)
			continue;

		if (!of_device_is_available(node)) {
			dev_dbg(dev, "Skipping disabled component %s\n",
				node->full_name);
			continue;
		}

		comp_type = (enum mtk_ddp_comp_type)of_id->data;

		if (comp_type == MTK_DISP_MUTEX) {
			private->mutex_node = of_node_get(node);
			continue;
		}

		comp_id = mtk_ddp_comp_get_id(node, comp_type);
		if (comp_id < 0) {
			dev_warn(dev, "Skipping unknown component %s\n",
				 node->full_name);
			continue;
		}

		private->comp_node[comp_id] = of_node_get(node);

		/*
		 * Currently only the OVL, RDMA, DSI, and DPI blocks have
		 * separate component platform drivers and initialize their own
		 * DDP component structure. The others are initialized here.
		 */
		if (comp_type == MTK_DISP_OVL ||
		    comp_type == MTK_DISP_RDMA ||
		    comp_type == MTK_DSI ||
		    comp_type == MTK_DPI) {
			dev_info(dev, "Adding component match for %s\n",
				 node->full_name);
			component_match_add(dev, &match, compare_of, node);
		} else {
			struct mtk_ddp_comp *comp;

			comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
			if (!comp) {
				ret = -ENOMEM;
				goto err_node;
			}

			ret = mtk_ddp_comp_init(dev, node, comp, comp_id, NULL);
			if (ret)
				goto err_node;

			private->ddp_comp[comp_id] = comp;
		}
	}

	if (!private->mutex_node) {
		dev_err(dev, "Failed to find disp-mutex node\n");
		ret = -ENODEV;
		goto err_node;
	}

	pm_runtime_enable(dev);

	platform_set_drvdata(pdev, private);

	ret = component_master_add_with_match(dev, &mtk_drm_ops, match);
	if (ret)
		goto err_pm;

	return 0;

err_pm:
	pm_runtime_disable(dev);
err_node:
	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++)
		of_node_put(private->comp_node[i]);
	return ret;
}

static int mtk_drm_remove(struct platform_device *pdev)
{
	struct mtk_drm_private *private = platform_get_drvdata(pdev);
	int i;

	component_master_del(&pdev->dev, &mtk_drm_ops);
	pm_runtime_disable(&pdev->dev);
	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++)
		of_node_put(private->comp_node[i]);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_drm_sys_suspend(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm = private->drm;
	struct drm_connector *conn;

	drm_kms_helper_poll_disable(drm);

	drm_modeset_lock_all(drm);
	list_for_each_entry(conn, &drm->mode_config.connector_list, head) {
		int old_dpms = conn->dpms;

		if (conn->funcs->dpms)
			conn->funcs->dpms(conn, DRM_MODE_DPMS_OFF);

		/* Set the old mode back to the connector for resume */
		conn->dpms = old_dpms;
	}
	drm_modeset_unlock_all(drm);

	DRM_DEBUG_DRIVER("mtk_drm_sys_suspend\n");
	return 0;
}

static int mtk_drm_sys_resume(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm = private->drm;
	struct drm_connector *conn;

	drm_modeset_lock_all(drm);
	list_for_each_entry(conn, &drm->mode_config.connector_list, head) {
		int desired_mode = conn->dpms;

		/*
		 * at suspend time, we save dpms to connector->dpms,
		 * restore the old_dpms, and at current time, the connector
		 * dpms status must be DRM_MODE_DPMS_OFF.
		 */
		conn->dpms = DRM_MODE_DPMS_OFF;

		/*
		 * If the connector has been disconnected during suspend,
		 * disconnect it from the encoder and leave it off. We'll notify
		 * userspace at the end.
		 */
		if (conn->funcs->dpms)
			conn->funcs->dpms(conn, desired_mode);
	}
	drm_modeset_unlock_all(drm);

	drm_kms_helper_poll_enable(drm);

	DRM_DEBUG_DRIVER("mtk_drm_sys_resume\n");
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_drm_pm_ops, mtk_drm_sys_suspend,
			 mtk_drm_sys_resume);

static struct platform_driver mtk_drm_platform_driver = {
	.probe	= mtk_drm_probe,
	.remove	= mtk_drm_remove,
	.driver	= {
		.name	= "mediatek-drm",
		.of_match_table = mtk_drm_of_ids,
		.pm     = &mtk_drm_pm_ops,
	},
};

static struct platform_driver * const mtk_drm_drivers[] = {
	&mtk_drm_platform_driver,
	&mtk_disp_ovl_driver,
	&mtk_disp_rdma_driver,
	&mtk_dsi_driver,
	&mtk_mipi_tx_driver,
	&mtk_dpi_driver,
};

static int __init mtk_drm_init(void)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_drm_drivers); i++) {
		ret = platform_driver_register(mtk_drm_drivers[i]);
		if (ret < 0) {
			pr_err("Failed to register %s driver: %d\n",
			       mtk_drm_drivers[i]->driver.name, ret);
			goto err;
		}
	}

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_drm_drivers[i]);

	return ret;
}

static void __exit mtk_drm_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_drm_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_drm_drivers[i]);
}

module_init(mtk_drm_init);
module_exit(mtk_drm_exit);

MODULE_AUTHOR("YT SHEN <yt.shen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek SoC DRM driver");
MODULE_LICENSE("GPL v2");
