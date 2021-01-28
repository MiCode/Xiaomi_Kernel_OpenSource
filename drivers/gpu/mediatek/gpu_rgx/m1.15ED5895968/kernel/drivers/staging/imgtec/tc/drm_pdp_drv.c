/*
 * @File
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this file
 * under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/component.h>
#include <linux/of_platform.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_prime.h>
#include <drm/drm_print.h>
#include <drm/drm_vblank.h>
#else
#include <drm/drmP.h>
#endif

#include "tc_drv.h"
#include "pvrversion.h"

#include "drm_pdp_drv.h"
#include "drm_pdp_gem.h"
#include "pdp_drm.h"

#include "odin_defs.h"

#if defined(SUPPORT_PLATO_DISPLAY)
#include "plato_drv.h"
#include "pdp2_regs.h"
#include "pdp2_mmu_regs.h"
#endif

#define DRIVER_NAME "pdp"
#define DRIVER_DESC "Imagination Technologies PDP DRM Display Driver"
#define DRIVER_DATE "20150612"

#if defined(PDP_USE_ATOMIC)
#include <drm/drm_atomic_helper.h>

#define PVR_DRIVER_ATOMIC DRIVER_ATOMIC
#else
#define PVR_DRIVER_ATOMIC 0
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#define	PVR_DRIVER_PRIME 0
#else
#define	PVR_DRIVER_PRIME DRIVER_PRIME
#endif

/* This header must always be included last */
#include "kernel_compatibility.h"

static bool display_enable = true;
static unsigned int output_device = 1;

module_param(display_enable, bool, 0444);
MODULE_PARM_DESC(display_enable, "Enable all displays (default: Y)");

module_param(output_device, uint, 0444);
MODULE_PARM_DESC(output_device, "PDP output device (default: PDP1)");

static void pdp_irq_handler(void *data)
{
	struct drm_device *dev = data;
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		pdp_crtc_irq_handler(crtc);
}

static int pdp_early_load(struct drm_device *dev)
{
	struct pdp_drm_private *dev_priv;
	int err;

	DRM_DEBUG("loading %s device\n", to_platform_device(dev->dev)->name);

	platform_set_drvdata(to_platform_device(dev->dev), dev);

	dev_priv = kzalloc(sizeof(*dev_priv), GFP_KERNEL);
	if (!dev_priv)
		return -ENOMEM;

	dev->dev_private = dev_priv;
	dev_priv->dev = dev;
	dev_priv->version = (enum pdp_version)
		to_platform_device(dev->dev)->id_entry->driver_data;
	dev_priv->display_enabled = display_enable;

#if !defined(SUPPORT_PLATO_DISPLAY)
	/* PDP output device selection  */
	dev_priv->outdev = (enum pdp_output_device)output_device;
	if (dev_priv->outdev == PDP_OUTPUT_PDP2 &&
	    !tc_pdp2_compatible(dev->dev->parent)) {
		DRM_ERROR("TC doesn't support PDP2\n");
		err = -ENODEV;
		goto err_dev_priv_free;
	}

	if (dev_priv->outdev == PDP_OUTPUT_PDP1) {
		dev_priv->pdp_interrupt = TC_INTERRUPT_PDP;
	} else if (dev_priv->outdev == PDP_OUTPUT_PDP2) {
		dev_priv->pdp_interrupt = TC_INTERRUPT_PDP2;
	} else {
		DRM_ERROR("wrong PDP device number (outdev=%u)\n",
			  dev_priv->outdev);
		err = -ENODEV;
		goto err_dev_priv_free;
	}

	/* PDP FBC module support detection */
	dev_priv->pfim_capable = (dev_priv->outdev == PDP_OUTPUT_PDP2 &&
				  tc_pfim_capable(dev->dev->parent));
#endif

	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		err = tc_enable(dev->dev->parent);
		if (err) {
			DRM_ERROR("failed to enable parent device (err=%d)\n", err);
			goto err_dev_priv_free;
		}

		/*
		 * check whether it's Orion PDP for picking
		 * the right display mode list later on
		 */
		if (dev_priv->version == PDP_VERSION_ODIN)
			dev_priv->subversion = (enum pdp_odin_subversion)
				tc_odin_subvers(dev->dev->parent);
#endif
	}

#if defined(SUPPORT_PLATO_DISPLAY)
	else if (dev_priv->version == PDP_VERSION_PLATO) {
// XXX do we we need to do this? Plato driver has already enabled device.
		err = plato_enable(dev->dev->parent);
		if (err) {
			DRM_ERROR("failed to enable parent device (err=%d)\n", err);
			goto err_dev_priv_free;
		}
	}
#endif

	dev_priv->gem_priv = pdp_gem_init(dev);
	if (!dev_priv->gem_priv) {
		DRM_ERROR("gem initialisation failed\n");
		err = -ENOMEM;
		goto err_disable_parent_device;
	}

	err = pdp_modeset_early_init(dev_priv);
	if (err) {
		DRM_ERROR("early modeset initialisation failed (err=%d)\n",
			  err);
		goto err_gem_cleanup;
	}

	err = drm_vblank_init(dev_priv->dev, 1);
	if (err) {
		DRM_ERROR("failed to complete vblank init (err=%d)\n", err);
		goto err_modeset_late_cleanup;
	}

	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		err = tc_set_interrupt_handler(dev->dev->parent,
					   dev_priv->pdp_interrupt,
					   pdp_irq_handler,
					   dev);
		if (err) {
			DRM_ERROR("failed to set interrupt handler (err=%d)\n",
				  err);
			goto err_vblank_cleanup;
		}

		err = tc_enable_interrupt(dev->dev->parent,
					  dev_priv->pdp_interrupt);
		if (err) {
			DRM_ERROR("failed to enable pdp interrupts (err=%d)\n",
				  err);
			goto err_uninstall_interrupt_handle;
		}
#endif
	}
#if defined(SUPPORT_PLATO_DISPLAY)
	else if (dev_priv->version == PDP_VERSION_PLATO) {
		err = plato_set_interrupt_handler(dev->dev->parent,
							PLATO_INTERRUPT_PDP,
							pdp_irq_handler,
							dev);
		if (err) {
			DRM_ERROR("failed to set interrupt handler (err=%d)\n",
				  err);
			goto err_vblank_cleanup;
		}

		err = plato_enable_interrupt(dev->dev->parent, PLATO_INTERRUPT_PDP);
		if (err) {
			DRM_ERROR("failed to enable pdp interrupts (err=%d)\n",
				  err);
			goto err_uninstall_interrupt_handle;
		}
	}
#endif

	dev->irq_enabled = true;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0))
	dev->vblank_disable_allowed = 1;
#endif

	return 0;

err_uninstall_interrupt_handle:
	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		tc_set_interrupt_handler(dev->dev->parent,
					     dev_priv->pdp_interrupt,
					     NULL,
					     NULL);
#endif
	}
#if defined(SUPPORT_PLATO_DISPLAY)
	else if (dev_priv->version == PDP_VERSION_PLATO) {
		plato_set_interrupt_handler(dev->dev->parent,
				PLATO_INTERRUPT_PDP,
				NULL,
				NULL);
	}
#endif
err_vblank_cleanup:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	/* Called by drm_dev_fini in Linux 4.11.0 and later */
	drm_vblank_cleanup(dev_priv->dev);
#endif
err_modeset_late_cleanup:
	pdp_modeset_late_cleanup(dev_priv);
err_gem_cleanup:
	pdp_gem_cleanup(dev_priv->gem_priv);
err_disable_parent_device:
	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		tc_disable(dev->dev->parent);
#endif
	}
#if defined(SUPPORT_PLATO_DISPLAY)
	else if (dev_priv->version == PDP_VERSION_PLATO)
		plato_disable(dev->dev->parent);
#endif
err_dev_priv_free:
	kfree(dev_priv);
	return err;
}

static int pdp_late_load(struct drm_device *dev)
{
	struct pdp_drm_private *dev_priv = dev->dev_private;
	int err;

	err = pdp_modeset_late_init(dev_priv);
	if (err) {
		DRM_ERROR("late modeset initialisation failed (err=%d)\n",
			  err);
		return err;
	}

	return 0;
}

static void pdp_early_unload(struct drm_device *dev)
{
	struct pdp_drm_private *dev_priv = dev->dev_private;

#if defined(CONFIG_DRM_FBDEV_EMULATION) && defined(PDP_USE_ATOMIC)
	drm_atomic_helper_shutdown(dev);
#endif
	pdp_modeset_early_cleanup(dev_priv);
}

static void pdp_late_unload(struct drm_device *dev)
{
	struct pdp_drm_private *dev_priv = dev->dev_private;

	DRM_INFO("unloading %s device.\n", to_platform_device(dev->dev)->name);
	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		tc_disable_interrupt(dev->dev->parent, dev_priv->pdp_interrupt);
		tc_set_interrupt_handler(dev->dev->parent,
					     dev_priv->pdp_interrupt,
					     NULL,
					     NULL);
#endif
	}
#if defined(SUPPORT_PLATO_DISPLAY)
	else if (dev_priv->version == PDP_VERSION_PLATO) {
		plato_disable_interrupt(dev->dev->parent, PLATO_INTERRUPT_PDP);
		plato_set_interrupt_handler(dev->dev->parent,
						PLATO_INTERRUPT_PDP,
						NULL,
						NULL);
	}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	/* Called by drm_dev_fini in Linux 4.11.0 and later */
	drm_vblank_cleanup(dev_priv->dev);
#endif
	pdp_modeset_late_cleanup(dev_priv);
	pdp_gem_cleanup(dev_priv->gem_priv);

	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		tc_disable(dev->dev->parent);
#endif
	}
#if defined(SUPPORT_PLATO_DISPLAY)
	else if (dev_priv->version == PDP_VERSION_PLATO)
		plato_disable(dev->dev->parent);
#endif

	kfree(dev_priv);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
static int pdp_load(struct drm_device *dev, unsigned long flags)
{
	int err;

	err = pdp_early_load(dev);
	if (err)
		return err;

	err = pdp_late_load(dev);
	if (err) {
		pdp_late_unload(dev);
		return err;
	}

	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
static int pdp_unload(struct drm_device *dev)
#else
static void pdp_unload(struct drm_device *dev)
#endif
{
	pdp_early_unload(dev);
	pdp_late_unload(dev);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
	return 0;
#endif
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
static void pdp_preclose(struct drm_device *dev, struct drm_file *file)
{
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		pdp_crtc_flip_event_cancel(crtc, file);
}
#endif

#if !defined(CONFIG_DRM_FBDEV_EMULATION)
static inline void pdp_teardown_drm_config(struct drm_device *dev)
{
#if defined(PDP_USE_ATOMIC)
	drm_atomic_helper_shutdown(dev);
#else
	struct drm_crtc *crtc;

	DRM_INFO("%s: %s device\n", __func__, to_platform_device(dev->dev)->name);

	/*
	 * When non atomic driver is in use, manually trigger ->set_config
	 * with an empty mode set associated to this crtc.
	 */
	drm_modeset_lock_all(dev);
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (crtc->primary->fb) {
			struct drm_mode_set mode_set = { .crtc = crtc };
			int err;

			err = drm_mode_set_config_internal(&mode_set);
			if (err)
				DRM_ERROR("failed to disable crtc %p (err=%d)\n",
					  crtc, err);
		}
	}
	drm_modeset_unlock_all(dev);
#endif
}
#endif /* !defined(CONFIG_DRM_FBDEV_EMULATION) */

static void pdp_lastclose(struct drm_device *dev)
{
#if defined(CONFIG_DRM_FBDEV_EMULATION)
	struct pdp_drm_private *dev_priv = dev->dev_private;
	struct pdp_fbdev *fbdev = dev_priv->fbdev;
	int err;

	if (fbdev) {
		/*
		 * This is a fbdev driver, therefore never attempt to shutdown
		 * on a client disconnecting.
		 */
		err = drm_fb_helper_restore_fbdev_mode_unlocked(&fbdev->helper);
		if (err)
			DRM_ERROR("failed to restore mode (err=%d)\n", err);
	}
#else
	pdp_teardown_drm_config(dev);
#endif
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
int pdp_enable_vblank(struct drm_crtc *crtc)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
static int pdp_enable_vblank(struct drm_device *dev, unsigned int pipe)
#else
static int pdp_enable_vblank(struct drm_device *dev, int pipe)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
	struct drm_device *dev = crtc->dev;
	unsigned int pipe      = drm_crtc_index(crtc);
#endif
	struct pdp_drm_private *dev_priv = dev->dev_private;

	switch (pipe) {
	case 0:
		pdp_crtc_set_vblank_enabled(dev_priv->crtc, true);
		break;
	default:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		DRM_ERROR("invalid crtc %u\n", pipe);
#else
		DRM_ERROR("invalid crtc %d\n", pipe);
#endif
		return -EINVAL;
	}

	DRM_DEBUG("vblank interrupts enabled for crtc %d\n", pipe);

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
void pdp_disable_vblank(struct drm_crtc *crtc)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
static void pdp_disable_vblank(struct drm_device *dev, unsigned int pipe)
#else
static void pdp_disable_vblank(struct drm_device *dev, int pipe)
#endif
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
	struct drm_device *dev = crtc->dev;
	unsigned int pipe      = drm_crtc_index(crtc);
#endif
	struct pdp_drm_private *dev_priv = dev->dev_private;

	switch (pipe) {
	case 0:
		pdp_crtc_set_vblank_enabled(dev_priv->crtc, false);
		break;
	default:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		DRM_ERROR("invalid crtc %u\n", pipe);
#else
		DRM_ERROR("invalid crtc %d\n", pipe);
#endif
		return;
	}

	DRM_DEBUG("vblank interrupts disabled for crtc %d\n", pipe);
}

static int pdp_gem_object_create_ioctl(struct drm_device *dev,
				       void *data,
				       struct drm_file *file)
{
	struct pdp_drm_private *dev_priv = dev->dev_private;

	return pdp_gem_object_create_ioctl_priv(dev,
						dev_priv->gem_priv,
						data,
						file);
}

static int pdp_gem_dumb_create(struct drm_file *file,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args)
{
	struct pdp_drm_private *dev_priv = dev->dev_private;

	return pdp_gem_dumb_create_priv(file,
					dev,
					dev_priv->gem_priv,
					args);
}

static void pdp_gem_object_free(struct drm_gem_object *obj)
{
	struct pdp_drm_private *dev_priv = obj->dev->dev_private;

	pdp_gem_object_free_priv(dev_priv->gem_priv, obj);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0))
static void pdp_gem_object_free_unlocked(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;

	mutex_lock(&dev->struct_mutex);
	pdp_gem_object_free(obj);
	mutex_unlock(&dev->struct_mutex);
}
#endif

static const struct vm_operations_struct pdp_gem_vm_ops = {
	.fault	= pdp_gem_object_vm_fault,
	.open	= drm_gem_vm_open,
	.close	= drm_gem_vm_close,
};

static const struct drm_ioctl_desc pdp_ioctls[] = {
	DRM_IOCTL_DEF_DRV(PDP_GEM_CREATE, pdp_gem_object_create_ioctl,
				DRM_AUTH | DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(PDP_GEM_MMAP, pdp_gem_object_mmap_ioctl,
				DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PDP_GEM_CPU_PREP, pdp_gem_object_cpu_prep_ioctl,
				DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PDP_GEM_CPU_FINI, pdp_gem_object_cpu_fini_ioctl,
				DRM_AUTH | DRM_UNLOCKED),
};

static const struct file_operations pdp_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
	.mmap		= drm_gem_mmap,
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= noop_llseek,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= drm_compat_ioctl,
#endif
};

static struct drm_driver pdp_drm_driver = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	.load				= NULL,
	.unload				= NULL,
#else
	.load				= pdp_load,
	.unload				= pdp_unload,
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
	.preclose			= pdp_preclose,
#endif
	.lastclose			= pdp_lastclose,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
	.set_busid			= drm_platform_set_busid,
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
	.get_vblank_counter		= NULL,
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	.get_vblank_counter		= drm_vblank_no_hw_counter,
#else
	.get_vblank_counter		= drm_vblank_count,
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0))
	.enable_vblank			= pdp_enable_vblank,
	.disable_vblank			= pdp_disable_vblank,
#endif

	.debugfs_init			= pdp_debugfs_init,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	.debugfs_cleanup		= pdp_debugfs_cleanup,
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0))
	.gem_free_object		= pdp_gem_object_free,
#else
	.gem_free_object_unlocked	= pdp_gem_object_free_unlocked,
#endif

	.prime_handle_to_fd		= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle		= drm_gem_prime_fd_to_handle,
	.gem_prime_export		= pdp_gem_prime_export,
	.gem_prime_import		= pdp_gem_prime_import,
	.gem_prime_import_sg_table	= pdp_gem_prime_import_sg_table,

    // Set dumb_create to NULL to avoid xorg owning the display (if xorg is running).
	.dumb_create			= pdp_gem_dumb_create,
	.dumb_map_offset		= pdp_gem_dumb_map_offset,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	.dumb_destroy			= drm_gem_dumb_destroy,
#endif

	.gem_vm_ops			= &pdp_gem_vm_ops,

	.name				= DRIVER_NAME,
	.desc				= DRIVER_DESC,
	.date				= DRIVER_DATE,
	.major				= PVRVERSION_MAJ,
	.minor				= PVRVERSION_MIN,
	.patchlevel			= PVRVERSION_BUILD,

	.driver_features		= DRIVER_GEM |
					  DRIVER_MODESET |
					  PVR_DRIVER_PRIME |
					  PVR_DRIVER_ATOMIC,
	.ioctls				= pdp_ioctls,
	.num_ioctls			= ARRAY_SIZE(pdp_ioctls),
	.fops				= &pdp_driver_fops,
};

#if defined(SUPPORT_PLATO_DISPLAY)

static int compare_parent_dev(struct device *dev, void *data)
{
	struct device *pdp_dev = data;

	return dev->parent && dev->parent == pdp_dev->parent;
}

static int pdp_component_bind(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *ddev;
	int ret;

	dev_info(dev, "Loading platform device\n");
	ddev = drm_dev_alloc(&pdp_drm_driver, &pdev->dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	if (IS_ERR(ddev))
		return PTR_ERR(ddev);
#else
	if (!ddev)
		return -ENOMEM;
#endif

	// XXX no need to do this as happens in pdp_early_load
	platform_set_drvdata(pdev, ddev);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
	/* Needed by drm_platform_set_busid */
	ddev->platformdev = pdev;
#endif
	BUG_ON(pdp_drm_driver.load != NULL);

	ret = pdp_early_load(ddev);
	if (ret)
		goto err_drm_dev_put;

	DRM_DEBUG_DRIVER("Binding other components\n");
	/* Bind other components, including HDMI encoder/connector */
	ret = component_bind_all(dev, ddev);
	if (ret) {
		DRM_ERROR("Failed to bind other components (ret=%d)\n", ret);
		goto err_drm_dev_late_unload;
	}

	ret = drm_dev_register(ddev, 0);
	if (ret)
		goto err_drm_dev_late_unload;

	ret = pdp_late_load(ddev);
	if (ret)
		goto err_drm_dev_unregister;

	return 0;

err_drm_dev_unregister:
	drm_dev_unregister(ddev);
err_drm_dev_late_unload:
	pdp_late_unload(ddev);
err_drm_dev_put:
	drm_dev_put(ddev);
	return	ret;
}

static void pdp_component_unbind(struct device *dev)
{
	struct drm_device *ddev = dev_get_drvdata(dev);

	dev_info(dev, "Unloading platform device\n");
	BUG_ON(pdp_drm_driver.unload != NULL);
	pdp_early_unload(ddev);
	drm_dev_unregister(ddev);
	pdp_late_unload(ddev);
	component_unbind_all(dev, ddev);
	drm_dev_put(ddev);
}

static const struct component_master_ops pdp_component_ops = {
	.bind	= pdp_component_bind,
	.unbind = pdp_component_unbind,
};


static int pdp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct component_match *match = NULL;

	component_match_add(dev, &match, compare_parent_dev, dev);
	return component_master_add_with_match(dev, &pdp_component_ops, match);
}

static int pdp_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &pdp_component_ops);
	return 0;
}

#else  // !SUPPORT_PLATO_DISPLAY

static int pdp_probe(struct platform_device *pdev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	struct drm_device *ddev;
	int ret;

	ddev = drm_dev_alloc(&pdp_drm_driver, &pdev->dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	if (IS_ERR(ddev))
		return PTR_ERR(ddev);
#else
	if (!ddev)
		return -ENOMEM;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
	/* Needed by drm_platform_set_busid */
	ddev->platformdev = pdev;
#endif
	/*
	 * The load callback, called from drm_dev_register, is deprecated,
	 * because of potential race conditions.
	 */
	BUG_ON(pdp_drm_driver.load != NULL);

	ret = pdp_early_load(ddev);
	if (ret)
		goto err_drm_dev_put;

	ret = drm_dev_register(ddev, 0);
	if (ret)
		goto err_drm_dev_late_unload;

	ret = pdp_late_load(ddev);
	if (ret)
		goto err_drm_dev_unregister;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		pdp_drm_driver.name,
		pdp_drm_driver.major,
		pdp_drm_driver.minor,
		pdp_drm_driver.patchlevel,
		pdp_drm_driver.date,
		ddev->primary->index);
#endif
	return 0;

err_drm_dev_unregister:
	drm_dev_unregister(ddev);
err_drm_dev_late_unload:
	pdp_late_unload(ddev);
err_drm_dev_put:
	drm_dev_put(ddev);
	return	ret;
#else
	return drm_platform_init(&pdp_drm_driver, pdev);
#endif
}

static int pdp_remove(struct platform_device *pdev)
{
	struct drm_device *ddev = platform_get_drvdata(pdev);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	/*
	 * The unload callback, called from drm_dev_unregister, is
	 * deprecated.
	 */
	BUG_ON(pdp_drm_driver.unload != NULL);

	pdp_early_unload(ddev);

	drm_dev_unregister(ddev);

	pdp_late_unload(ddev);

	drm_dev_put(ddev);
#else
	drm_put_dev(ddev);
#endif
	return 0;
}

#endif  // SUPPORT_PLATO_DISPLAY

static void pdp_shutdown(struct platform_device *pdev)
{
}

static struct platform_device_id pdp_platform_device_id_table[] = {
	{ .name = APOLLO_DEVICE_NAME_PDP, .driver_data = PDP_VERSION_APOLLO },
	{ .name = ODN_DEVICE_NAME_PDP, .driver_data = PDP_VERSION_ODIN },
#if defined(SUPPORT_PLATO_DISPLAY)
	{ .name = PLATO_DEVICE_NAME_PDP, .driver_data = PDP_VERSION_PLATO },
#endif  // SUPPORT_PLATO_DISPLAY
	{ },
};

static struct platform_driver pdp_platform_driver = {
	.probe		= pdp_probe,
	.remove		= pdp_remove,
	.shutdown	= pdp_shutdown,
	.driver		= {
		.owner  = THIS_MODULE,
		.name	= DRIVER_NAME,
	},
	.id_table	= pdp_platform_device_id_table,
};

module_platform_driver(pdp_platform_driver);

MODULE_AUTHOR("Imagination Technologies Ltd. <gpl-support@imgtec.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_DEVICE_TABLE(platform, pdp_platform_device_id_table);
MODULE_LICENSE("Dual MIT/GPL");
