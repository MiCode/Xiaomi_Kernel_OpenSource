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
		.set_property = vlv_set_gamma,
		.disable_property = vlv_disable_gamma,
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
		.set_property = vlv_set_contrast,
		.disable_property = vlv_disable_contrast,
		.validate = vlv_validate,
	},

	{
		.status = false,
		.prop_id = brightness,
		.len = VLV_CB_VALS,
		.name = "brightness",
		.set_property = vlv_set_brightness,
		.disable_property = vlv_disable_brightness,
		.validate = vlv_validate,
	},

	{
		.status = false,
		.prop_id = hue,
		.len = VLV_HS_VALS,
		.name = "hue",
		.set_property = vlv_set_hue,
		.disable_property = vlv_disable_hue,
		.validate = vlv_validate
	},

	{
		.status = false,
		.prop_id = saturation,
		.len = VLV_HS_VALS,
		.name = "saturation",
		.set_property = vlv_set_saturation,
		.disable_property = vlv_disable_saturation,
		.validate = vlv_validate
	}
};

bool vlv_set_saturation(struct color_property *property, u64 *data, u8 plane_id)
{
	u32 reg, sprite, val, new_val;

	/* If sprite plane enabled */
	sprite = plane_id;
	if (!(REG_READ(VLV_CLRMGR_SPCNTR(sprite)) &
			SP_ENABLE)) {
		pr_err("ADF: CM: Sprite plane %d not enabled\n", sprite);
		return false;
	}

	reg = VLV_CLRMGR_SPHS(sprite);

	/* Clear current values */
	val = REG_READ(reg) & ~(VLV_SATURATION_MASK);

	/* Get new values */
	new_val = *data & VLV_SATURATION_MASK;
	val |= new_val;

	/* Update */
	REG_WRITE(reg, val);
	property->lut[0] = new_val;

	/* Set status */
	if (new_val == VLV_SATURATION_DEFAULT)
		property->status = false;
	else
		property->status = true;

	pr_info("ADF: CM: Set Saturation to 0x%x successful\n", new_val);
	return true;

}
bool vlv_disable_saturation(struct color_property *property, u8 plane_id)
{
	u64 data = VLV_SATURATION_DEFAULT;
	return vlv_set_saturation(property, &data, plane_id);
}

bool vlv_set_hue(struct color_property *property, u64 *data, u8 plane_id)
{
	u32 reg, sprite, val, new_val;

	/* If sprite plane enabled */
	sprite = plane_id;
	if (!(REG_READ(VLV_CLRMGR_SPCNTR(sprite)) &
			SP_ENABLE)) {
		pr_err("ADF: CM: Sprite plane %d not enabled\n", sprite);
		return false;
	}

	reg = VLV_CLRMGR_SPHS(sprite);

	/* Clear current hue values */
	val = REG_READ(reg) & ~(VLV_HUE_MASK << VLV_HUE_SHIFT);

	/* Get the new values */
	new_val = *data & VLV_HUE_MASK;
	val |= (new_val << VLV_HUE_SHIFT);

	/* Update */
	REG_WRITE(reg, new_val);
	property->lut[0] = new_val;

	/* Set status */
	if (new_val == VLV_HUE_DEFAULT)
		property->status = false;
	else
		property->status = true;
	pr_info("ADF: CM: Set Hue to 0x%x successful\n", new_val);
	return true;
}

bool vlv_disable_hue(struct color_property *property, u8 plane_id)
{
	u64 data = VLV_HUE_DEFAULT;
	return vlv_set_hue(property, &data, plane_id);
}

bool vlv_set_brightness(struct color_property *property, u64 *data, u8 plane_id)
{
	u32 val, new_val, reg, sprite;

	/* If sprite plane enabled */
	sprite = plane_id;
	if (!(REG_READ(VLV_CLRMGR_SPCNTR(sprite)) &
			SP_ENABLE)) {
		pr_err("ADF: CM: Sprite plane %d not enabled\n", sprite);
		return false;
	}

	reg = VLV_CLRMGR_SPCB(sprite);

	/* Clear current values */
	val = REG_READ(reg) & ~(VLV_BRIGHTNESS_MASK);

	/*Get new values */
	new_val = *data & VLV_BRIGHTNESS_MASK;

	/* Update */
	val |= new_val;
	REG_WRITE(reg, val);
	property->lut[0] = new_val;

	/* Set status */
	if (new_val == VLV_BRIGHTNESS_DEFAULT)
		property->status = false;
	else
		property->status = true;
	pr_info("ADF: CM: Set Brightness correction to %d successful\n",
								new_val);
	return true;
}

bool vlv_disable_brightness(struct color_property *property, u8 plane_id)
{
	u64 data = VLV_BRIGHTNESS_DEFAULT;
	return vlv_set_brightness(property, &data, plane_id);
}

bool vlv_set_contrast(struct color_property *property, u64 *data, u8 plane_id)
{
	u32 val, new_val, reg, sprite;

	/* If sprite plane enabled */
	sprite = plane_id;
	if (!(REG_READ(VLV_CLRMGR_SPCNTR(sprite)) &
			SP_ENABLE)) {
		pr_err("ADF: CM: Sprite plane %d not enabled\n", sprite);
		return false;
	}

	reg = VLV_CLRMGR_SPCB(sprite);

	/* Clear current value. Contrast correction position is bit [26:18] */
	val = REG_READ(reg) &
		~(VLV_CONTRAST_MASK << VLV_CONTRAST_SHIFT);

	/* Get new value */
	new_val = *data & VLV_CONTRAST_MASK;

	/* Update */
	val |= (new_val << VLV_CONTRAST_SHIFT);
	REG_WRITE(reg, val);
	property->lut[0] = new_val;

	/* Set status */
	if (new_val == VLV_CONTRAST_DEFAULT)
		property->status = false;
	else
		property->status = true;
	pr_info("ADF: CM: Set Contrast to 0x%x successful\n", new_val);
	return true;
}

bool vlv_disable_contrast(struct color_property *property, u8 plane_id)
{
	u64 data = VLV_CONTRAST_DEFAULT;
	return vlv_set_contrast(property, &data, plane_id);
}

/* Core function to apply 10-bit gamma correction */
bool vlv_set_gamma(struct color_property *property, u64 *data, u8 pipe_id)
{
	u16 red, green, blue;
	u64 correct_rgb;
	u32 val, even, odd;
	u32 count = 0;
	u32 reg = 0;
	u32 pipe = pipe_id;
	u32 palette = PALETTE(pipe);
	u64 data_size = property->len;

	/* Validate input */
	if (data_size != VLV_10BIT_GAMMA_MAX_VALS) {
		pr_err("ADF: CM: Unexpected value count for GAMMA LUT\n");
		return false;
	}

	/*
	 * 128, 64 bit values, coming in <0><R16><G16><B16>
	 * format containing
	 * only 10 integer and 6fraction correction values
	 */
	while (count < (data_size - CLRMGR_GAMMA_GCMAX_VAL)) {
		correct_rgb = data[count];
		property->lut[count] = correct_rgb;

		blue = correct_rgb >> CLRMGR_GAMMA_PARSER_SHIFT_BLUE;
		green = correct_rgb >> CLRMGR_GAMMA_PARSER_SHIFT_GREEN;
		red = correct_rgb >> CLRMGR_GAMMA_PARSER_SHIFT_RED;

		/*
		 * Prepare even and odd regs. Even register contains 6
		 * fractional and 2 integer base bits, so lower 8 bits
		 */
		even = ((blue & VLV_GAMMA_EVEN_MASK) <<
				VLV_GAMMA_SHIFT_BLUE_REG) |
			((green & VLV_GAMMA_EVEN_MASK) <<
				VLV_GAMMA_SHIFT_GREEN_REG) |
			((red & VLV_GAMMA_EVEN_MASK) <<
				VLV_GAMMA_SHIFT_RED_REG);

		/* Odd register contains upper 8 (integer) bits */
		odd = ((blue >> VLV_GAMMA_ODD_SHIFT) <<
				VLV_GAMMA_SHIFT_BLUE_REG) |
			((green >> VLV_GAMMA_ODD_SHIFT) <<
				VLV_GAMMA_SHIFT_GREEN_REG) |
			((red >> VLV_GAMMA_ODD_SHIFT) <<
				VLV_GAMMA_SHIFT_RED_REG);

		/* Writing fraction part first, then integer part */
		REG_WRITE(palette, even);
		palette += 4;
		REG_WRITE(palette, odd);
		palette += 4;
		count++;
	}

	/*
	 * Last 64bit values is in 11.6 format for GCmax,
	 * RGB sequence
	 */
	correct_rgb = data[count];
	property->lut[count] = correct_rgb;

	count = CLRMGR_GAMMA_TOTAL_GCMAX_REGS;
	reg = VLV_PIPE_GCMAX(pipe);
	while (count--) {
		val = (correct_rgb >> (count * VLV_CLRMGR_GAMMA_GCMAX_SHIFT)) &
			VLV_GAMMA_GCMAX_MASK;
		/* GCMAX value must be <= 1024 */
		if (val > VLV_CLRMGR_GAMMA_GCMAX_MAX)
			val = VLV_CLRMGR_GAMMA_GCMAX_MAX;

		/* Write in 11.6 format */
		REG_WRITE(reg, (val << 6));
		reg += 4;
	}

	/* Enable gamma for PIPE */
	reg = PIPECONF(pipe);
	val = REG_READ(reg) | PIPECONF_GAMMA;
	REG_WRITE(reg, val);

	property->status = true;
	pr_info("ADF: CM: 10bit gamma correction successfully applied\n");
	return true;
}

bool vlv_disable_gamma(struct color_property *property, u8 pipe_id)
{
	u32 pipe, reg, val;

	pipe = pipe_id;

	/* Disable gamma for PIPE */
	reg = PIPECONF(pipe);
	val = REG_READ(reg) & (~PIPECONF_GAMMA);
	REG_WRITE(reg, val);

	property->status = false;

	/* Clear old values in LUT */
	memset(property->lut, 0, property->len * sizeof(u64));
	pr_info("ADF: CM: 10bit gamma correction successfully applied");
	return true;
}

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
