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

/* Custom IOCTL */
long intel_overlay_engine_obj_ioctl(struct adf_obj *obj,
	unsigned int cmd, unsigned long arg)
{
	struct intel_adf_overlay_engine *overlay_engine;
	struct intel_plane *plane;
	struct adf_overlay_engine *adf_oe;
	struct intel_pipe *pipe;
	u8 plane_id;
	u8 pipe_id;

	long err = 0;

	adf_oe = container_of(obj, struct adf_overlay_engine, base);
	overlay_engine = container_of(adf_oe,
		struct intel_adf_overlay_engine, base);

	plane = overlay_engine->plane;
	plane_id = plane->base.idx;

	pipe = plane->pipe;
	pipe_id = pipe->base.idx;

	switch (cmd) {
	case INTEL_ADF_COLOR_MANAGER_SET:
		pr_info("ADF: Calling apply to set Color Property on the Overlay Engine\n");

		/*
		 * Sprite registers are continuously placed at
		 * an offset of 0x100. But the Plane Enum adds
		 * primary planes also into account.
		 * To compensate for that, alter the plane id,
		 * so that a direct sprite offset can be used
		 * in applying color correction.
		 */
		if (!intel_color_manager_apply(plane->color_ctx,
				(struct color_cmd *) arg, plane_id - pipe_id)) {
			pr_err("%s Error: Set color correction failed\n",
								__func__);
			return -EFAULT;
		}

		pr_info("%s: Set color correction success\n", __func__);

		err = 0;
		break;

	case INTEL_ADF_COLOR_MANAGER_GET:
		pr_info("ADF: Calling get Color Property on the Overlay Engine\n");

		/*
		 * Sprite registers are continuously placed at
		 * an offset of 0x100. But the Plane Enum adds
		 * primary planes also into account.
		 * To compensate for that, alter the plane id,
		 * so that a direct sprite offset can be used
		 * in applying color correction.
		 */
		if (!intel_color_manager_get(plane->color_ctx,
				(struct color_cmd *) arg, plane_id - pipe_id)) {
			pr_err("%s Error: Get color correction failed\n",
								__func__);
			return -EFAULT;
		}

		pr_info("%s: Get color correction success\n", __func__);

		err = 0;
		break;

	default:
		pr_err("%s: ADF: Error: Invalid custom IOCTL\n", __func__);
	}

	return err;
}

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
		&plane->ops->adf_ops,
			"intel_ov_eng_%s", plane->base.name);
}

void intel_adf_overlay_engine_destroy(
			struct intel_adf_overlay_engine *eng)
{
	if (eng) {
		eng->plane = NULL;
		adf_overlay_engine_destroy(&eng->base);
	}
}
