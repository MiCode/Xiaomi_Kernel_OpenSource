/*
 * Copyright © 2014 Intel Corporation
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Author:
 * Shashank Sharma <shashank.sharma@intel.com>
 * Ramalingam C <Ramalingam.c@intel.com>
 * Kausal Malladi <Kausal.Malladi@intel.com>
 */

#include <core/vlv/chv_color_correction.h>

/*
 * Cherryview SOC allows following color correction values:
 *	- CSC(wide gamut) with 3x3 matrix = 9 csc correction values.
 *	- Gamma correction with 128 gamma values + 1 GCMAX value.
 */
const struct color_property chv_pipe_color_corrections[] = {
	{
		.status = false,
		.prop_id = csc,
		.len = CHV_CSC_VALS,
		.name = "csc-correction",
		.validate = chv_validate,
	},

	{
		.status = false,
		.prop_id = gamma,
		.len = CHV_GAMMA_VALS,
		.name = "gamma-correction",
		.validate = chv_validate,
	},

	{
		.status = false,
		.prop_id = degamma,
		.len = CHV_DEGAMMA_VALS,
		.name = "degamma-enable-disable",
		.validate = chv_validate,
	}
};

/*
 * Cherryview SOC allows following plane level color correction values:
 *	- contrast: single valued property
 *	- brightness: single valued property
 *	- hue: single valued property
 *	- saturation: single valued property
 */
const struct color_property chv_plane_color_corrections[] = {
	{
		.status = false,
		.prop_id = contrast,
		.len = CHV_CB_VALS,
		.name = "contrast",
		.validate = chv_validate,
	},

	{
		.status = false,
		.prop_id = brightness,
		.len = CHV_CB_VALS,
		.name = "brightness",
		.validate = chv_validate,
	},

	{
		.status = false,
		.prop_id = hue,
		.len = CHV_HS_VALS,
		.name = "hue",
		.validate = chv_validate
	},

	{
		.status = false,
		.prop_id = saturation,
		.len = CHV_HS_VALS,
		.name = "saturation",
		.validate = chv_validate
	}
};

bool chv_get_color_correction(void *props_data, int object_type)
{
	u32 count = 0;
	u32 index = 0;

	/* Sanity */
	if (!props_data) {
		pr_err("ADF: CM: CHV: Null input to get_color_correction\n");
		return false;
	}

	/* Allocate pipe color capabilities holder if request is from pipe */
	if (object_type == CLRMGR_REQUEST_FROM_PIPE) {
		struct pipe_properties *pipe_props =
			(struct pipe_properties *) props_data;
		if (!pipe_props) {
			pr_err("ADF: CM: CHV: OOM while loading color correction data\n");
			return false;
		}

		pipe_props->no_of_pipe_props =
			ARRAY_SIZE(chv_pipe_color_corrections);
		count = ARRAY_SIZE(chv_pipe_color_corrections);

		for (index = 0; index < count; index++) {
			struct color_property *cp =
				kzalloc(sizeof(struct color_property),
					GFP_KERNEL);
			if (!cp) {
				pr_err("ADF: CM: Out of Memory for creating a color property\n");

				while (index--)
					kfree(pipe_props->props[index]);
				return false;
			}
			memcpy((void *) cp, (const void *)
				(&(chv_pipe_color_corrections[index])),
					sizeof(struct color_property));
			pipe_props->props[index] = cp;
		}

		pr_info("ADF: CM: CHV: Pipe color correction data loading done, details are:\n");
		pr_info("ADF: CM: Properties(pipe=%d)\n",
				(int)pipe_props->no_of_pipe_props);
	}
	/* Allocate plane color capabilities holder if request is from plane */
	else if (object_type == CLRMGR_REQUEST_FROM_PLANE) {
		struct plane_properties *plane_props =
			(struct plane_properties *) props_data;
		if (!plane_props) {
			pr_err("ADF: CM: CHV: OOM while loading color correction data\n");
			return false;
		}

		plane_props->no_of_plane_props =
			ARRAY_SIZE(chv_plane_color_corrections);
		count = ARRAY_SIZE(chv_plane_color_corrections);

		for (index = 0; index < count; index++) {
			struct color_property *cp =
				kzalloc(sizeof(struct color_property),
					GFP_KERNEL);
			if (!cp) {
				pr_err("ADF: CM: Out of Memory for creating a color property\n");

				while (index--)
					kfree(plane_props->props[index]);
				return false;
			}
			memcpy((void *) cp, (const void *)
				(&(chv_plane_color_corrections[index])),
					sizeof(struct color_property));
			plane_props->props[index] = cp;
		}

		pr_info("ADF: CM: CHV: Plane color correction data loading done, details are:\n");
		pr_info("ADF: CM: Properties(plane=%d)\n",
				(int)plane_props->no_of_plane_props);
	} else
		return false;

	return true;
}

bool chv_validate(u8 property)
{
	/* Validate if we support this property */
	if ((int)property < csc || (int)property > saturation) {
		pr_err("ADF: CM: CHV: Invalid input, propery Max=%d, Min=%d\n",
			csc, saturation);
		return false;
	}

	pr_info("ADF: CM: CHV: Input is valid for property\n");
	return true;
}
