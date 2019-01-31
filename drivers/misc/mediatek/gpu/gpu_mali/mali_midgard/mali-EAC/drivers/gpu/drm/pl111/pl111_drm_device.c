/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * pl111_drm_device.c
 * Implementation of the Linux device driver entrypoints for PL111 DRM
 */
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/version.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>

#include "pl111_drm.h"

struct pl111_drm_dev_private priv;

#ifdef CONFIG_DMA_SHARED_BUFFER_USES_KDS
static void initial_kds_obtained(void *cb1, void *cb2)
{
	wait_queue_head_t *wait = (wait_queue_head_t *) cb1;
	bool *cb_has_called = (bool *) cb2;

	*cb_has_called = true;
	wake_up(wait);
}

/* Must be called from within current_displaying_lock spinlock */
void release_kds_resource_and_display(struct pl111_drm_flip_resource *flip_res)
{
	struct pl111_drm_crtc *pl111_crtc = to_pl111_crtc(flip_res->crtc);
	pl111_crtc->displaying_fb = flip_res->fb;

	/* Release the previous buffer */
	if (pl111_crtc->old_kds_res_set != NULL) {
		/*
		 * Can flip to the same buffer, but must not release the current
		 * resource set
		 */
		BUG_ON(pl111_crtc->old_kds_res_set == flip_res->kds_res_set);
		kds_resource_set_release(&pl111_crtc->old_kds_res_set);
	}
	/* Record the current buffer, to release on the next buffer flip */
	pl111_crtc->old_kds_res_set = flip_res->kds_res_set;
}
#endif

void pl111_drm_preclose(struct drm_device *dev, struct drm_file *file_priv)
{
	DRM_DEBUG_KMS("DRM %s on dev=%p\n", __func__, dev);
}

void pl111_drm_lastclose(struct drm_device *dev)
{
	DRM_DEBUG_KMS("DRM %s on dev=%p\n", __func__, dev);
}

/*
 * pl111 does not have a proper HW counter for vblank IRQs so enable_vblank
 * and disable_vblank are just no op callbacks.
 */
static int pl111_enable_vblank(struct drm_device *dev, int crtc)
{
	DRM_DEBUG_KMS("%s: dev=%p, crtc=%d", __func__, dev, crtc);
	return 0;
}

static void pl111_disable_vblank(struct drm_device *dev, int crtc)
{
	DRM_DEBUG_KMS("%s: dev=%p, crtc=%d", __func__, dev, crtc);
}

struct drm_mode_config_funcs mode_config_funcs = {
	.fb_create = pl111_fb_create,
};

static int pl111_modeset_init(struct drm_device *dev)
{
	struct drm_mode_config *mode_config;
	struct pl111_drm_dev_private *priv = dev->dev_private;
	struct pl111_drm_connector *pl111_connector;
	struct pl111_drm_encoder *pl111_encoder;
	int ret = 0;

	if (priv == NULL)
		return -EINVAL;

	drm_mode_config_init(dev);
	mode_config = &dev->mode_config;
	mode_config->funcs = &mode_config_funcs;
	mode_config->min_width = 1;
	mode_config->max_width = 1024;
	mode_config->min_height = 1;
	mode_config->max_height = 768;

	priv->pl111_crtc = pl111_crtc_create(dev);
	if (priv->pl111_crtc == NULL) {
		pr_err("Failed to create pl111_drm_crtc\n");
		ret = -ENOMEM;
		goto out_config;
	}

	priv->number_crtcs = 1;

	pl111_connector = pl111_connector_create(dev);
	if (pl111_connector == NULL) {
		pr_err("Failed to create pl111_drm_connector\n");
		ret = -ENOMEM;
		goto out_config;
	}

	pl111_encoder = pl111_encoder_create(dev, 1);
	if (pl111_encoder == NULL) {
		pr_err("Failed to create pl111_drm_encoder\n");
		ret = -ENOMEM;
		goto out_config;
	}

	ret = drm_mode_connector_attach_encoder(&pl111_connector->connector,
						&pl111_encoder->encoder);
	if (ret != 0) {
		DRM_ERROR("Failed to attach encoder\n");
		goto out_config;
	}

	pl111_connector->connector.encoder = &pl111_encoder->encoder;

	goto finish;

out_config:
	drm_mode_config_cleanup(dev);
finish:
	DRM_DEBUG("%s returned %d\n", __func__, ret);
	return ret;
}

static void pl111_modeset_fini(struct drm_device *dev)
{
	drm_mode_config_cleanup(dev);
}

static int pl111_drm_load(struct drm_device *dev, unsigned long chipset)
{
	int ret = 0;

	pr_debug("DRM %s\n", __func__);

	mutex_init(&priv.export_dma_buf_lock);
	atomic_set(&priv.nr_flips_in_flight, 0);
	init_waitqueue_head(&priv.wait_for_flips);
#ifdef CONFIG_DMA_SHARED_BUFFER_USES_KDS
	ret = kds_callback_init(&priv.kds_cb, 1, show_framebuffer_on_crtc_cb);
	if (ret != 0) {
		pr_err("Failed to initialise KDS callback\n");
		goto finish;
	}

	ret = kds_callback_init(&priv.kds_obtain_current_cb, 1,
				initial_kds_obtained);
	if (ret != 0) {
		pr_err("Failed to init KDS obtain callback\n");
		kds_callback_term(&priv.kds_cb);
		goto finish;
	}
#endif

	/* Create a cache for page flips */
	priv.page_flip_slab = kmem_cache_create("page flip slab",
			sizeof(struct pl111_drm_flip_resource), 0, 0, NULL);
	if (priv.page_flip_slab == NULL) {
		DRM_ERROR("Failed to create slab\n");
		ret = -ENOMEM;
		goto out_kds_callbacks;
	}

	dev->dev_private = &priv;

	ret = pl111_modeset_init(dev);
	if (ret != 0) {
		pr_err("Failed to init modeset\n");
		goto out_slab;
	}

	ret = pl111_device_init(dev);
	if (ret != 0) {
		DRM_ERROR("Failed to init MMIO and IRQ\n");
		goto out_modeset;
	}

	ret = drm_vblank_init(dev, 1);
	if (ret != 0) {
		DRM_ERROR("Failed to init vblank\n");
		goto out_vblank;
	}

	goto finish;

out_vblank:
	pl111_device_fini(dev);
out_modeset:
	pl111_modeset_fini(dev);
out_slab:
	kmem_cache_destroy(priv.page_flip_slab);
out_kds_callbacks:
#ifdef CONFIG_DMA_SHARED_BUFFER_USES_KDS
	kds_callback_term(&priv.kds_obtain_current_cb);
	kds_callback_term(&priv.kds_cb);
#endif
finish:
	DRM_DEBUG_KMS("pl111_drm_load returned %d\n", ret);
	return ret;
}

static int pl111_drm_unload(struct drm_device *dev)
{
	pr_debug("DRM %s\n", __func__);

	kmem_cache_destroy(priv.page_flip_slab);

	drm_vblank_cleanup(dev);
	pl111_modeset_fini(dev);
	pl111_device_fini(dev);

#ifdef CONFIG_DMA_SHARED_BUFFER_USES_KDS
	kds_callback_term(&priv.kds_obtain_current_cb);
	kds_callback_term(&priv.kds_cb);
#endif
	return 0;
}

static struct vm_operations_struct pl111_gem_vm_ops = {
	.fault = pl111_gem_fault,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0))
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
#else
	.open = pl111_gem_vm_open,
	.close = pl111_gem_vm_close,
#endif
};

static const struct file_operations drm_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = pl111_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
	.fasync = drm_fasync,
};

static struct drm_ioctl_desc pl111_ioctls[] = {
	DRM_IOCTL_DEF_DRV(PL111_GEM_CREATE, pl111_drm_gem_create_ioctl,
		DRM_CONTROL_ALLOW | DRM_UNLOCKED),
};

static struct drm_driver driver = {
	.driver_features =
		DRIVER_MODESET | DRIVER_FB_DMA | DRIVER_GEM | DRIVER_PRIME,
	.load = pl111_drm_load,
	.unload = pl111_drm_unload,
	.context_dtor = NULL,
	.preclose = pl111_drm_preclose,
	.lastclose = pl111_drm_lastclose,
	.suspend = pl111_drm_suspend,
	.resume = pl111_drm_resume,
	.get_vblank_counter = drm_vblank_count,
	.enable_vblank = pl111_enable_vblank,
	.disable_vblank = pl111_disable_vblank,
	.ioctls = pl111_ioctls,
	.fops = &drm_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
	.dumb_create = pl111_dumb_create,
	.dumb_destroy = pl111_dumb_destroy,
	.dumb_map_offset = pl111_dumb_map_offset,
	.gem_free_object = pl111_gem_free_object,
	.gem_vm_ops = &pl111_gem_vm_ops,
	.prime_handle_to_fd = &pl111_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = &pl111_gem_prime_export,
	.gem_prime_import = &pl111_gem_prime_import,
};

int pl111_drm_init(struct platform_device *dev)
{
	int ret;
	pr_debug("DRM %s\n", __func__);
	pr_debug("PL111 DRM initialize, driver name: %s, version %d.%d\n",
		DRIVER_NAME, DRIVER_MAJOR, DRIVER_MINOR);
	driver.num_ioctls = DRM_ARRAY_SIZE(pl111_ioctls);
	ret = 0;
	driver.kdriver.platform_device = dev;
	return drm_platform_init(&driver, dev);

}

void pl111_drm_exit(struct platform_device *dev)
{
	pr_debug("DRM %s\n", __func__);
	drm_platform_exit(&driver, dev);
}
