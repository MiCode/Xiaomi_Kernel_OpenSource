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

#include "core/intel_dc_config.h"

int intel_adf_plane_init(struct intel_plane *plane, struct device *dev,
	u8 idx, const struct intel_plane_capabilities *caps,
	const struct intel_plane_ops *ops, const char *name)
{
	int platform_color_id;

	/* If platform is CHV, ID is 1 else 0 (for VLV) */
	if (IS_CHERRYVIEW())
		platform_color_id = CHV_COLOR_ID;
	else if (IS_VALLEYVIEW())
		platform_color_id = VLV_COLOR_ID;
	else
		platform_color_id = INVALID_PLATFORM_COLOR_ID;

	if (!plane || !caps || !ops)
		return -EINVAL;

	plane->caps = caps;
	plane->ops = ops;

	if (!intel_color_manager_plane_init(plane, platform_color_id))
		pr_err("ADF: CM: Error registering plane color properties");

	return intel_dc_component_init(&plane->base, dev, idx, name);
}


void intel_plane_destroy(struct intel_plane *plane)
{
	if (plane) {
		intel_dc_component_destroy(&plane->base);
		plane->caps = NULL;
		plane->ops = NULL;
	}
}

