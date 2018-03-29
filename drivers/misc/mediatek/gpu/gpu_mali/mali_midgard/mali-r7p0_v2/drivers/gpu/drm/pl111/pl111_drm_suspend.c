/*
 *
 * (C) COPYRIGHT 2012-2013 ARM Limited. All rights reserved.
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
 * pl111_drm_suspend.c
 * Implementation of the suspend/resume functions for PL111 DRM
 */

#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/version.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include "pl111_drm.h"

int pl111_drm_suspend(struct drm_device *dev, pm_message_t state)
{
	pr_info("DRM %s\n", __func__);
	return 0;
}

int pl111_drm_resume(struct drm_device *dev)
{
	pr_info("DRM %s\n", __func__);
	return 0;
}
