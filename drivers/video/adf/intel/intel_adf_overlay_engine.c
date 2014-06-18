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

#include "intel_adf.h"

int intel_adf_overlay_engine_init(struct intel_adf_overlay_engine *eng,
			struct intel_adf_device *dev,
			struct intel_plane *plane)
{
	if (!eng || !dev || !plane)
		return -EINVAL;

	memset(eng, 0, sizeof(struct intel_adf_overlay_engine));

	INIT_LIST_HEAD(&eng->active_list);

	eng->plane = plane;

	return adf_overlay_engine_init(&eng->base, &dev->base,
		&plane->ops->adf_ops, "intel_ov_eng_%s", plane->base.name);
}

void intel_adf_overlay_engine_destroy(
			struct intel_adf_overlay_engine *eng)
{
	if (eng) {
		eng->plane = NULL;
		adf_overlay_engine_destroy(&eng->base);
	}
}
