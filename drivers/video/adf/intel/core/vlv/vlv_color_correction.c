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
 */

#include <core/vlv/vlv_color_correction.h>

/*
 * Valleyview SOC allows following color correction values:
 *	- CSC(wide gamut) with 3x3 matrix = 9 csc correction values.
 *	- Gamma correction with 128 gamma values + 1 GCMAX value.
 */
const struct color_property vlv_pipe_color_corrections[] = {
	{
		.status = false,
		.prop_id = csc,
		.len = VLV_CSC_VALS,
		.name = "csc-correction",
		.set_property = vlv_set_csc,
		.disable_property = vlv_disable_csc,
		.validate = vlv_validate,
	},

	{
		.status = false,
		.prop_id = gamma,
		.len = VLV_GAMMA_VALS,
		.name = "gamma-correction",
		.validate = vlv_validate,
	}
};

/*
 * Valleyview SOC allows following plane level color correction values:
 *	- contrast: single valued property
 *	- brightness: single valued property
 *	- hue: single valued property
 *	- saturation: single valued property
 */
const struct color_property vlv_plane_color_corrections[] = {
	{
		.status = false,
		.prop_id = contrast,
		.len = VLV_CB_VALS,
		.name = "contrast",
		.validate = vlv_validate,
	},

	{
		.status = false,
		.prop_id = brightness,
		.len = VLV_CB_VALS,
		.name = "brightness",
		.validate = vlv_validate,
	},

	{
		.status = false,
		.prop_id = hue,
		.len = VLV_HS_VALS,
		.name = "hue",
		.validate = vlv_validate
	},

	{
		.status = false,
		.prop_id = saturation,
		.len = VLV_HS_VALS,
		.name = "saturation",
		.validate = vlv_validate
	}
};

/* Core function to program CSC regs */
bool vlv_set_csc(struct color_property *property, u64 *data, u8 pipe_id)
{
	u32 count = 0;
	u32 pipeconf, csc_reg, data_size, pipe;
	u32 c0, c1, c2;

	pipe = pipe_id;
	data_size = property->len;

	/* Validate input */
	if (data_size != VLV_CSC_VALS) {
		pr_err("ADF: CM: Unexpected value count for CSC LUT\n");
		return false;
	}

	pr_info("ADF: CM: Setting CSC on pipe = %d\n", pipe);
	csc_reg = PIPECSC(pipe);

	/* Read CSC matrix, one row at a time */
	while (count < VLV_CSC_VALS) {
		property->lut[count] = data[count];
		c0 = data[count++] & VLV_CSC_VALUE_MASK;

		property->lut[count] = data[count];
		c1 = data[count++] & VLV_CSC_VALUE_MASK;

		property->lut[count] = data[count];
		c2 = data[count++] & VLV_CSC_VALUE_MASK;

		/* C0 is LSB 12bits, C1 is MSB 16-27 */
		REG_WRITE(csc_reg, (c1 << VLV_CSC_COEFF_SHIFT) | c0);
		csc_reg += 4;

		/* C2 is LSB 12 bits */
		REG_WRITE(csc_reg, c2);
		csc_reg += 4;
	}

	/* Enable csc correction */
	pipeconf = (REG_READ(PIPECONF(pipe)) | PIPECONF_CSC_ENABLE);
	REG_WRITE(PIPECONF(pipe), pipeconf);

	property->status = true;
	pr_info("ADF: CM: CSC successfully set on pipe = %d\n", pipe);
	return true;
}

bool vlv_disable_csc(struct color_property *property, u8 pipe_id)
{
	u32 pipeconf, pipe;

	pipe = pipe_id;

	/* Disable csc correction */
	pipeconf = REG_READ(PIPECONF(pipe));
	pipeconf &= ~PIPECONF_CSC_ENABLE;
	REG_WRITE(PIPECONF(pipe), pipeconf);

	property->status = false;

	/* Clear old values */
	memset(property->lut, 0, property->len * sizeof(u64));
	pr_info("ADF: CM: CSC disabled on pipe = %d\n", pipe);
	return true;
}

bool vlv_get_color_correction(void *props_data, int object_type)
{
	u32 count = 0;
	u32 index = 0;
	/* Sanity */
	if (!props_data) {
		pr_err("ADF: CM: VLV: Null input to get_color_correction\n");
		return false;
	}

	/* Allocate pipe color capabilities holder if request is from pipe */
	if (object_type == CLRMGR_REQUEST_FROM_PIPE) {
		struct pipe_properties *pipe_props =
			(struct pipe_properties *) props_data;
		if (!pipe_props) {
			pr_err("ADF: CM: VLV: OOM while loading color correction data\n");
			return false;
		}

		pipe_props->no_of_pipe_props =
			ARRAY_SIZE(vlv_pipe_color_corrections);
		count = ARRAY_SIZE(vlv_pipe_color_corrections);

		for (index = 0; index < count; index++) {
			struct color_property *cp =
				kzalloc(sizeof(struct color_property),
					GFP_KERNEL);

			if (!cp) {
				pr_err("ADF: CM: Out of Memory to create a color property\n");

				while (index--)
					kfree(pipe_props->props[index]);
				return false;
			}
			memcpy((void *) cp, (const void *)
				(&(vlv_pipe_color_corrections[index])),
					sizeof(struct color_property));
			pipe_props->props[index] = cp;
		}

		pr_info("ADF: CM: VLV: Pipe color correction data loading done, details are:\n");
		pr_info("ADF: CM: Properties(pipe=%d)\n",
				(int)pipe_props->no_of_pipe_props);
	}
	/* Allocate plane color capabilities holder if request is from plane */
	else if (object_type == CLRMGR_REQUEST_FROM_PLANE) {
		struct plane_properties *plane_props =
			(struct plane_properties *) props_data;
		if (!plane_props) {
			pr_err("ADF: CM: VLV: OOM while loading color correction data\n");
			return false;
		}

		plane_props->no_of_plane_props =
			ARRAY_SIZE(vlv_plane_color_corrections);
		count = ARRAY_SIZE(vlv_plane_color_corrections);

		for (index = 0; index < count; index++) {
			struct color_property *cp =
				kzalloc(sizeof(struct color_property),
					GFP_KERNEL);

			if (!cp) {
				pr_err("ADF: CM: Out of Memory to create a color property\n");

				while (index--)
					kfree(plane_props->props[index]);
				return false;
			}
			memcpy((void *) cp, (const void *)
				(&(vlv_plane_color_corrections[index])),
					sizeof(struct color_property));
			plane_props->props[index] = cp;
		}

		pr_info("ADF: CM: VLV: Plane color correction data loading done, details are:\n");
		pr_info("ADF: CM: Properties(plane=%d)\n",
				(int)plane_props->no_of_plane_props);
	} else
		return false;

	return true;
}

bool vlv_validate(u8 property)
{
	/* Validate if we support this property */
	if ((int)property < csc || (int)property > saturation) {
		pr_err("ADF: CM: VLV: Invalid input, propery Max=%d, Min=%d\n",
			csc, saturation);
		return false;
	}

	pr_info("ADF: CM: VLV: Input is valid for property\n");
	return true;
}
