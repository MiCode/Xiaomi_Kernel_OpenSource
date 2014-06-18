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

#include <video/adf_client.h>
#include <video/adf_fbdev.h>
#include <video/adf_format.h>

#include "intel_adf.h"

static struct fb_ops intel_fbdev_ops = {
	.owner = THIS_MODULE,
	.fb_open = adf_fbdev_open,
	.fb_release = adf_fbdev_release,
	.fb_check_var = adf_fbdev_check_var,
	.fb_set_par = adf_fbdev_set_par,
	.fb_blank = adf_fbdev_blank,
	.fb_pan_display = adf_fbdev_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_mmap = adf_fbdev_mmap,
};

int intel_adf_fbdev_init(struct adf_fbdev *fbdev,
			struct intel_adf_interface *intf,
			struct intel_adf_overlay_engine *eng)
{
	struct drm_mode_modeinfo mode;
	struct adf_device *parent;
	struct device *dev;
	int err;

	if (!fbdev || !intf || !eng)
		return -EINVAL;

	parent = adf_interface_parent(&intf->base);
	dev = &parent->base.dev;

	/*get current mode*/
	adf_interface_current_mode(&intf->base, &mode);

	err = adf_fbdev_init(fbdev, &intf->base, &eng->base,
			mode.hdisplay, mode.vdisplay,
			DRM_FORMAT_XRGB8888,
			&intel_fbdev_ops, "intel_fbdev");
	if (err) {
		dev_err(dev, "%s: failed to init fbdev\n", __func__);
		goto out_err;
	}

	return 0;
out_err:
	return err;
}

void intel_adf_fbdev_destroy(struct adf_fbdev *fbdev)
{
	if (fbdev)
		adf_fbdev_destroy(fbdev);
}

