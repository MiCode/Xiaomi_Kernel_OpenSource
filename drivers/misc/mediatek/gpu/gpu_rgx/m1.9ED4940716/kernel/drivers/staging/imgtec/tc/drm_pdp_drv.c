/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "pvr_linux_fence.h"
#include "pvr_sw_fence.h"

#include <linux/module.h>
#include <linux/reservation.h>
#include <linux/version.h>

#include <drm/drmP.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
#include <drm/drm_gem.h>
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

static bool display_enable = true;

module_param(display_enable, bool, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(display_enable, "Enable all displays (default: Y)");


static void pdp_irq_handler(void *data)
{
	struct drm_device *dev = data;
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		pdp_crtc_irq_handler(crtc);
}

static int pdp_load(struct drm_device *dev, unsigned long flags)
{
	struct pdp_drm_private *dev_priv;
	int err;

	DRM_INFO("loading %s device\n", dev->platformdev->name);

	platform_set_drvdata(dev->platformdev, dev);

	dev_priv = kzalloc(sizeof(*dev_priv), GFP_KERNEL);
	if (!dev_priv)
		return -ENOMEM;

	dev->dev_private = dev_priv;
	dev_priv->dev = dev;
	dev_priv->version =
		(enum pdp_version) dev->platformdev->id_entry->driver_data;
	dev_priv->display_enabled = display_enable;

	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		err = tc_enable(dev->dev->parent);
		if (err) {
			DRM_ERROR("failed to enable parent device (err=%d)\n", err);
			goto err_dev_priv_free;
		}
#endif
	}
#if defined(SUPPORT_PLATO_DISPLAY)
	else if (dev_priv->version == PDP_VERSION_PLATO) {
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

	dev_priv->dev_fctx = pvr_sw_fence_context_create("pdp-hw", "pdp");
	if (!dev_priv->dev_fctx) {
		err = -ENOMEM;
		goto err_gem_cleanup;
	}

	err = pdp_modeset_init(dev_priv);
	if (err) {
		DRM_ERROR("modeset initialisation failed (err=%d)\n", err);
		goto err_dev_fence_context_destroy;
	}

	err = drm_vblank_init(dev_priv->dev, 1);
	if (err) {
		DRM_ERROR("failed to complete vblank init (err=%d)\n", err);
		goto err_modeset_deinit;
	}

	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		err = tc_set_interrupt_handler(dev->dev->parent,
					   TC_INTERRUPT_PDP,
					   pdp_irq_handler,
					   dev);
		if (err) {
			DRM_ERROR("failed to set interrupt handler (err=%d)\n",
				  err);
			goto err_vblank_cleanup;
		}

		err = tc_enable_interrupt(dev->dev->parent, TC_INTERRUPT_PDP);
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
					     TC_INTERRUPT_PDP,
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
	drm_vblank_cleanup(dev_priv->dev);
err_modeset_deinit:
	pdp_modeset_cleanup(dev_priv);
err_dev_fence_context_destroy:
	pvr_sw_fence_context_destroy(dev_priv->dev_fctx);
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
	else if (dev_priv->version == PDP_VERSION_PLATO) {
		plato_disable(dev->dev->parent);
	}
#endif
err_dev_priv_free:
	kfree(dev_priv);
	return err;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
static int pdp_unload(struct drm_device *dev)
#else
static void pdp_unload(struct drm_device *dev)
#endif
{
	struct pdp_drm_private *dev_priv = dev->dev_private;

	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		tc_disable_interrupt(dev->dev->parent, TC_INTERRUPT_PDP);
		tc_set_interrupt_handler(dev->dev->parent,
					     TC_INTERRUPT_PDP,
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

	drm_vblank_cleanup(dev_priv->dev);

	pdp_modeset_cleanup(dev_priv);
	pdp_gem_cleanup(dev_priv->gem_priv);
	pvr_sw_fence_context_destroy(dev_priv->dev_fctx);

	if (dev_priv->version == PDP_VERSION_APOLLO ||
		dev_priv->version == PDP_VERSION_ODIN) {
#if !defined(SUPPORT_PLATO_DISPLAY)
		tc_disable(dev->dev->parent);
#endif
	}
#if defined(SUPPORT_PLATO_DISPLAY)
	else if (dev_priv->version == PDP_VERSION_PLATO) {
		plato_disable(dev->dev->parent);
	}
#endif

	kfree(dev_priv);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
	return 0;
#endif
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
static void pdp_preclose(struct drm_device *dev, struct drm_file *file)
{
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		pdp_crtc_flip_event_cancel(crtc, file);
}
#endif

static void pdp_lastclose(struct drm_device *dev)
{
	struct drm_crtc *crtc;

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
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
static int pdp_enable_vblank(struct drm_device *dev, unsigned int crtc)
#else
static int pdp_enable_vblank(struct drm_device *dev, int crtc)
#endif
{
	struct pdp_drm_private *dev_priv = dev->dev_private;

	switch (crtc) {
	case 0:
		pdp_crtc_set_vblank_enabled(dev_priv->crtc, true);
		break;
	default:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		DRM_ERROR("invalid crtc %u\n", crtc);
#else
		DRM_ERROR("invalid crtc %d\n", crtc);
#endif
		return -EINVAL;
	}

	DRM_DEBUG_DRIVER("vblank interrupts enabled for crtc %d\n", crtc);

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
static void pdp_disable_vblank(struct drm_device *dev, unsigned int crtc)
#else
static void pdp_disable_vblank(struct drm_device *dev, int crtc)
#endif
{
	struct pdp_drm_private *dev_priv = dev->dev_private;

	switch (crtc) {
	case 0:
		pdp_crtc_set_vblank_enabled(dev_priv->crtc, false);
		break;
	default:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
		DRM_ERROR("invalid crtc %u\n", crtc);
#else
		DRM_ERROR("invalid crtc %d\n", crtc);
#endif
		return;
	}

	DRM_DEBUG_DRIVER("vblank interrupts disabled for crtc %d\n", crtc);
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

static const struct vm_operations_struct pdp_gem_vm_ops = {
	.fault	= pdp_gem_object_vm_fault,
	.open	= drm_gem_vm_open,
	.close	= drm_gem_vm_close,
};

static const struct drm_ioctl_desc pdp_ioctls[] = {
	DRM_IOCTL_DEF_DRV(PDP_GEM_CREATE, pdp_gem_object_create_ioctl, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PDP_GEM_MMAP, pdp_gem_object_mmap_ioctl, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PDP_GEM_CPU_PREP, pdp_gem_object_cpu_prep_ioctl, DRM_AUTH | DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(PDP_GEM_CPU_FINI, pdp_gem_object_cpu_fini_ioctl, DRM_AUTH | DRM_UNLOCKED),
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
	.load				= pdp_load,
	.unload				= pdp_unload,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
	.preclose			= pdp_preclose,
#endif
	.lastclose			= pdp_lastclose,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
	.set_busid			= drm_platform_set_busid,
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	.get_vblank_counter		= drm_vblank_no_hw_counter,
#else
	.get_vblank_counter		= drm_vblank_count,
#endif
	.enable_vblank			= pdp_enable_vblank,
	.disable_vblank			= pdp_disable_vblank,

	.debugfs_init			= pdp_debugfs_init,
	.debugfs_cleanup		= pdp_debugfs_cleanup,

	.gem_free_object		= pdp_gem_object_free,

	.prime_handle_to_fd		= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle		= drm_gem_prime_fd_to_handle,
	.gem_prime_export		= pdp_gem_prime_export,
	.gem_prime_import		= pdp_gem_prime_import,
	.gem_prime_import_sg_table	= pdp_gem_prime_import_sg_table,

	.dumb_create			= pdp_gem_dumb_create,
	.dumb_map_offset		= pdp_gem_dumb_map_offset,
	.dumb_destroy			= drm_gem_dumb_destroy,

	.gem_vm_ops			= &pdp_gem_vm_ops,

	.name				= DRIVER_NAME,
	.desc				= DRIVER_DESC,
	.date				= DRIVER_DATE,
	.major				= PVRVERSION_MAJ,
	.minor				= PVRVERSION_MIN,
	.patchlevel			= PVRVERSION_BUILD,

	.driver_features		= DRIVER_GEM |
					  DRIVER_MODESET |
					  DRIVER_PRIME,
	.ioctls				= pdp_ioctls,
	.num_ioctls			= ARRAY_SIZE(pdp_ioctls),
	.fops				= &pdp_driver_fops,
};


static int pdp_probe(struct platform_device *pdev)
{
	return drm_platform_init(&pdp_drm_driver, pdev);
}

static int pdp_remove(struct platform_device *pdev)
{
	struct drm_device *dev = platform_get_drvdata(pdev);

	drm_put_dev(dev);

	return 0;
}

static void pdp_shutdown(struct platform_device *pdev)
{
}

static struct platform_device_id pdp_platform_device_id_table[] = {
	{ .name = APOLLO_DEVICE_NAME_PDP, .driver_data = PDP_VERSION_APOLLO },
	{ .name = ODN_DEVICE_NAME_PDP, .driver_data = PDP_VERSION_ODIN },
//	{ .name = PLATO_DEVICE_NAME_PDP, .driver_data = PDP_VERSION_PLATO },
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
