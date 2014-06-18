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
#ifndef INTEL_ADF_FBDEV_H_
#define INTEL_ADF_FBDEV_H_

#include <video/adf_fbdev.h>

extern int intel_adf_fbdev_init(struct adf_fbdev *fbdev,
	struct intel_adf_interface *intf,
	struct intel_adf_overlay_engine *eng);
extern void intel_adf_fbdev_destroy(struct adf_fbdev *fbdev);

#endif /* INTEL_ADF_FBDEV_H_ */
