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
		.set_property = chv_set_csc,
		.disable_property = chv_disable_csc,
		.validate = chv_validate,
	},

	{
		.status = false,
		.prop_id = gamma,
		.len = CHV_GAMMA_VALS,
		.name = "gamma-correction",
		.set_property = chv_set_gamma,
		.disable_property = chv_disable_gamma,
		.validate = chv_validate,
	},

	{
		.status = false,
		.prop_id = degamma,
		.len = CHV_DEGAMMA_VALS,
		.name = "degamma-enable-disable",
		.set_property = chv_set_degamma,
		.disable_property = chv_disable_degamma,
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
		.set_property = chv_set_contrast,
		.disable_property = chv_disable_contrast,
		.validate = chv_validate,
	},

	{
		.status = false,
		.prop_id = brightness,
		.len = CHV_CB_VALS,
		.name = "brightness",
		.set_property = chv_set_brightness,
		.disable_property = chv_disable_brightness,
		.validate = chv_validate,
	},

	{
		.status = false,
		.prop_id = hue,
		.len = CHV_HS_VALS,
		.name = "hue",
		.set_property = chv_set_hue,
		.disable_property = chv_disable_hue,
		.validate = chv_validate
	},

	{
		.status = false,
		.prop_id = saturation,
		.len = CHV_HS_VALS,
		.name = "saturation",
		.set_property = chv_set_saturation,
		.disable_property = chv_disable_saturation,
		.validate = chv_validate
	}
};

/* DeGamma LUT for R G and B */
const u16 deGamma_LUT_R[CHV_DEGAMMA_VALS] = {
	0, 20, 40, 60, 85,
	114, 149, 189, 235, 288,
	346, 410, 482, 559, 644,
	736, 835, 942, 1055, 1177,
	1306, 1444, 1589, 1743, 1905,
	2075, 2254, 2442, 2638, 2843,
	3058, 3281, 3514, 3756, 4008,
	4269, 4540, 4821, 5111, 5411,
	5722, 6043, 6374, 6715, 7066,
	7429, 7801, 8185, 8579, 8984,
	9400, 9827, 10266, 10715, 11176,
	11648, 12131, 12626, 13133, 13651,
	14181, 14723, 15276, 15842, 16383
};

const u16 deGamma_LUT_G[CHV_DEGAMMA_VALS] = {
	0, 20, 40, 60, 85,
	114, 149, 189, 235, 288,
	346, 410, 482, 559, 644,
	736, 835, 942, 1055, 1177,
	1306, 1444, 1589, 1743, 1905,
	2075, 2254, 2442, 2638, 2843,
	3058, 3281, 3514, 3756, 4008,
	4269, 4540, 4821, 5111, 5411,
	5722, 6043, 6374, 6715, 7066,
	7429, 7801, 8185, 8579, 8984,
	9400, 9827, 10266, 10715, 11176,
	11648, 12131, 12626, 13133, 13651,
	14181, 14723, 15276, 15842, 16383
};

const u16 deGamma_LUT_B[CHV_DEGAMMA_VALS] = {
	0, 20, 40, 60, 85,
	114, 149, 189, 235, 288,
	346, 410, 482, 559, 644,
	736, 835, 942, 1055, 1177,
	1306, 1444, 1589, 1743, 1905,
	2075, 2254, 2442, 2638, 2843,
	3058, 3281, 3514, 3756, 4008,
	4269, 4540, 4821, 5111, 5411,
	5722, 6043, 6374, 6715, 7066,
	7429, 7801, 8185, 8579, 8984,
	9400, 9827, 10266, 10715, 11176,
	11648, 12131, 12626, 13133, 13651,
	14181, 14723, 15276, 15842, 16383
};

bool chv_set_saturation(struct color_property *property, u64 *data, u8 plane_id)
{
	u32 reg, sprite, val, new_val;

	/* If sprite plane enabled */
	sprite = plane_id;
	if (!(REG_READ(CHV_CLRMGR_SPCNTR(sprite)) &
			SP_ENABLE)) {
		pr_err("ADF: CM: Sprite plane %d not enabled\n", sprite);
		return false;
	}

	reg = CHV_CLRMGR_SPHS(sprite);

	/* Clear current values */
	val = REG_READ(reg) & ~(CHV_SATURATION_MASK);

	/* Get new values */
	new_val = *data & CHV_SATURATION_MASK;
	val |= new_val;

	/* Update */
	REG_WRITE(reg, val);
	property->lut[0] = new_val;

	/* Set status */
	if (new_val == CHV_SATURATION_DEFAULT)
		property->status = false;
	else
		property->status = true;

	pr_info("ADF: CM: Set Saturation to 0x%x successful\n", new_val);
	return true;

}
bool chv_disable_saturation(struct color_property *property, u8 plane_id)
{
	u64 data = CHV_SATURATION_DEFAULT;
	return chv_set_saturation(property, &data, plane_id);
}

bool chv_set_hue(struct color_property *property, u64 *data, u8 plane_id)
{
	u32 reg, sprite, val, new_val;

	/* If sprite plane enabled */
	sprite = plane_id;
	if (!(REG_READ(CHV_CLRMGR_SPCNTR(sprite)) &
			SP_ENABLE)) {
		pr_err("ADF: CM: Sprite plane %d not enabled\n", sprite);
		return false;
	}

	reg = CHV_CLRMGR_SPHS(sprite);

	/* Clear current hue values */
	val = REG_READ(reg) & ~(CHV_HUE_MASK << CHV_HUE_SHIFT);

	/* Get the new values */
	new_val = *data & CHV_HUE_MASK;
	val |= (new_val << CHV_HUE_SHIFT);

	/* Update */
	REG_WRITE(reg, new_val);
	property->lut[0] = new_val;

	/* Set status */
	if (new_val == CHV_HUE_DEFAULT)
		property->status = false;
	else
		property->status = true;
	pr_info("ADF: CM: Set Hue to 0x%x successful\n", new_val);
	return true;
}

bool chv_disable_hue(struct color_property *property, u8 plane_id)
{
	u64 data = CHV_HUE_DEFAULT;
	return chv_set_hue(property, &data, plane_id);
}

bool chv_set_brightness(struct color_property *property, u64 *data, u8 plane_id)
{
	u32 val, new_val, reg, sprite;

	/* If sprite plane enabled */
	sprite = plane_id;
	if (!(REG_READ(CHV_CLRMGR_SPCNTR(sprite)) &
			SP_ENABLE)) {
		pr_err("ADF: CM: Sprite plane %d not enabled\n", sprite);
		return false;
	}

	reg = CHV_CLRMGR_SPCB(sprite);

	/* Clear current values */
	val = REG_READ(reg) & ~(CHV_BRIGHTNESS_MASK);

	/*Get new values */
	new_val = *data & CHV_BRIGHTNESS_MASK;

	/* Update */
	val |= new_val;
	REG_WRITE(reg, val);
	property->lut[0] = new_val;

	/* Set status */
	if (new_val == CHV_BRIGHTNESS_DEFAULT)
		property->status = false;
	else
		property->status = true;
	pr_info("ADF: CM: Set Brightness correction to %d successful\n",
				new_val);
	return true;
}

bool chv_disable_brightness(struct color_property *property, u8 plane_id)
{
	u64 data = CHV_BRIGHTNESS_DEFAULT;
	return chv_set_brightness(property, &data, plane_id);
}

bool chv_set_contrast(struct color_property *property, u64 *data, u8 plane_id)
{
	u32 val, new_val, reg, sprite;

	/* If sprite plane enabled */
	sprite = plane_id;
	if (!(REG_READ(CHV_CLRMGR_SPCNTR(sprite)) &
			SP_ENABLE)) {
		pr_err("ADF: CM: Sprite plane %d not enabled\n", sprite);
		return false;
	}

	reg = CHV_CLRMGR_SPCB(sprite);

	/* Clear current value. Contrast correction position is bit [26:18] */
	val = REG_READ(reg) &
		~(CHV_CONTRAST_MASK << CHV_CONTRAST_SHIFT);

	/* Get new value */
	new_val = *data & CHV_CONTRAST_MASK;

	/* Update */
	val |= (new_val << CHV_CONTRAST_SHIFT);
	REG_WRITE(reg, val);
	property->lut[0] = new_val;

	/* Set status */
	if (new_val == CHV_CONTRAST_DEFAULT)
		property->status = false;
	else
		property->status = true;
	pr_info("ADF CM: Set Contrast to 0x%x successful\n", new_val);
	return true;
}

bool chv_disable_contrast(struct color_property *property, u8 plane_id)
{
	u64 data = CHV_CONTRAST_DEFAULT;
	return chv_set_contrast(property, &data, plane_id);
}

/* Core function to enable degamma block */
bool chv_set_degamma(struct color_property *property, u64 *data, u8 pipe_id)
{
	u32 count = 0;
	u32 cgm_deGamma_reg, data_size, pipe;
	u32 word0, word1;

	pipe = pipe_id;
	data_size = property->len;

	/* Validate input */
	if (data_size != CHV_DEGAMMA_VALS) {
		pr_err("ADF: CM: Unexpected value count for DEGAMMA Set/Reset\n");
		return false;
	}


	cgm_deGamma_reg = _PIPE_DEGAMMA_BASE(pipe);

	while (count < CHV_DEGAMMA_VALS) {
		/* Green (29:16) and Blue (13:0) to be written to DWORD1 */
		word0 = deGamma_LUT_G[count];
		word0 = word0 << DEGAMMA_GREEN_LEFT_SHIFT;
		word0 = word0 | deGamma_LUT_B[count];
		REG_WRITE(cgm_deGamma_reg, word0);

		cgm_deGamma_reg += 4;

		/* Red (13:0) to be written to DWORD2 */
		word1 = deGamma_LUT_R[count];
		REG_WRITE(cgm_deGamma_reg, word1);

		cgm_deGamma_reg += 4;

		count++;
	}

	/* Enable DeGamma on CGM_CONTROL register on respective pipe */
	REG_WRITE(_PIPE_CGM_CONTROL(pipe),
		REG_READ(_PIPE_CGM_CONTROL(pipe)) | CGM_DEGAMMA_EN);

	property->status = true;
	pr_info("ADF: CM: DEGAMMA successfully enabled on pipe = %d\n", pipe);

	return true;
}

bool chv_disable_degamma(struct color_property *property, u8 pipe_id)
{
	u32 cgm_control_reg, pipe;

	pipe = pipe_id;

	/* Disable DeGamma*/
	cgm_control_reg = REG_READ(_PIPE_CGM_CONTROL(pipe));
	cgm_control_reg &= ~CGM_DEGAMMA_EN;
	REG_WRITE(_PIPE_CGM_CONTROL(pipe), cgm_control_reg);

	property->status = false;

	/* Clear old values */
	memset(property->lut, 0, property->len * sizeof(u64));
	pr_info("ADF: CM: DEGAMMA successfully disabled on pipe = %d\n", pipe);
	return true;
}

/* Core function to apply 10-bit gamma correction */
bool chv_set_gamma(struct color_property *property, u64 *data, u8 pipe_id)
{
	u16 red, green, blue;
	u64 correct_rgb;
	u32 cgm_gamma_reg;
	u32 count = 0;
	u32 pipe = pipe_id;
	u64 data_size = property->len;

	u32 word0, word1;

	/* Validate input */
	if (data_size != CHV_10BIT_GAMMA_MAX_VALS) {
		pr_err("ADF CM: Unexpected value count for GAMMA LUT\n");
		return false;
	}

	cgm_gamma_reg = _PIPE_GAMMA_BASE(pipe);

	while (count < CHV_GAMMA_VALS) {
		correct_rgb = data[count];
		property->lut[count] = correct_rgb;

		blue = correct_rgb >> CLRMGR_GAMMA_PARSER_SHIFT_BLUE;
		green = correct_rgb >> CLRMGR_GAMMA_PARSER_SHIFT_GREEN;
		red = correct_rgb >> CLRMGR_GAMMA_PARSER_SHIFT_RED;

		blue = blue >> CHV_GAMMA_MSB_SHIFT;
		green = green >> CHV_GAMMA_MSB_SHIFT;
		red = red >> CHV_GAMMA_MSB_SHIFT;

		/* Green (25:16) and Blue (9:0) to be written to DWORD1 */
		word0 = green;
		word0 = word0 << CHV_GAMMA_SHIFT_GREEN;
		word0 = word0 | blue;
		REG_WRITE(cgm_gamma_reg, word0);

		cgm_gamma_reg += 4;

		/* Red (9:0) to be written to DWORD2 */
		word1 = red;
		REG_WRITE(cgm_gamma_reg, word1);

		cgm_gamma_reg += 4;

		count++;
	}

	/* Enable Gamma on CGM_CONTROL register on respective pipe */
	REG_WRITE(_PIPE_CGM_CONTROL(pipe),
		REG_READ(_PIPE_CGM_CONTROL(pipe)) | CGM_GAMMA_EN);

	property->status = true;
	pr_info("ADF: CM: 10bit gamma correction successfully applied\n");
	return true;
}

bool chv_disable_gamma(struct color_property *property, u8 pipe_id)
{
	u32 cgm_control_reg, pipe;

	pipe = pipe_id;

	/* Disable DeGamma*/
	cgm_control_reg = REG_READ(_PIPE_CGM_CONTROL(pipe));
	cgm_control_reg &= ~CGM_GAMMA_EN;
	REG_WRITE(_PIPE_CGM_CONTROL(pipe), cgm_control_reg);

	property->status = false;

	/* Clear old values */
	memset(property->lut, 0, property->len * sizeof(u64));
	return true;
}

/* Core function to program CSC regs */
bool chv_set_csc(struct color_property *property, u64 *data, u8 pipe_id)
{
	u32 count = 0;
	u32 csc_reg, data_size, pipe;
	u32 c0, c1, c2;

	pipe = pipe_id;
	data_size = property->len;

	/* Validate input */
	if (data_size != CHV_CSC_VALS) {
		pr_err("ADF: CM: Unexpected value count for CSC LUT\n");
		return false;
	}

	csc_reg = _PIPE_CSC_BASE(pipe);

	while (count < CHV_CSC_VALS) {
		property->lut[count] = data[count];
		c0 = data[count++] & CHV_CSC_VALUE_MASK;

		property->lut[count] = data[count];
		c1 = data[count++] & CHV_CSC_VALUE_MASK;

		REG_WRITE(csc_reg, (c1 << CHV_CSC_COEFF_SHIFT) | c0);
		csc_reg += 4;

		/*
		 * Last register has only one 16 bit value (C8)
		 * to be programmed, other bits are Reserved
		 */
		if (count == 8) {
			property->lut[count] = data[count];
			c2 = data[count++] & CHV_CSC_VALUE_MASK;

			REG_WRITE(csc_reg, c2);
		}

	}

	/* Enable CSC on CGM_CONTROL register on respective pipe */
	REG_WRITE(_PIPE_CGM_CONTROL(pipe),
		REG_READ(_PIPE_CGM_CONTROL(pipe)) | CGM_CSC_EN);

	pr_info("ADF: CM: Setting CSC on pipe = %d\n", pipe);

	property->status = true;
	pr_info("ADF: CM: CSC successfully set on pipe = %d\n", pipe);
	return true;
}

bool chv_disable_csc(struct color_property *property, u8 pipe_id)
{
	u32 pipe, cgm_control_reg;

	pipe = pipe_id;

	/* Disable csc correction */
	cgm_control_reg = REG_READ(_PIPE_CGM_CONTROL(pipe));
	cgm_control_reg &= ~CGM_CSC_EN;
	REG_WRITE(_PIPE_CGM_CONTROL(pipe), cgm_control_reg);

	property->status = false;

	/* Clear old values */
	memset(property->lut, 0, property->len * sizeof(u64));
	pr_info("ADF: CM: CSC disabled on pipe = %d\n", pipe);
	return true;
}

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
