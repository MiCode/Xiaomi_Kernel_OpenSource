/*
 * Copyright © 2008 Intel Corporation
 *
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
 * Authors:
 *Shashank Sharma <shashank.sharma@intel.com>
 *Uma Shankar <uma.shankar@intel.com>
 *Shobhit Kumar <skumar40@intel.com>
 */

#include "drmP.h"
#include "intel_drv.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_clrmgr.h"

/* Gamma lookup table for Sprite planes */
u32 gamma_sprite_softlut[GAMMA_SP_MAX_COUNT] = {
	0, 0, 0, 0, 0, 1023
};

/* Gamma soft lookup table for default gamma =1.0 */
u32 gamma_softlut[MAX_PIPES_VLV][GAMMA_CORRECT_MAX_COUNT] =  {
	{0x000000, 0x0, 0x020202, 0x0, 0x040404, 0x0, 0x060606, 0x0,
	 0x080808, 0x0, 0x0A0A0A, 0x0, 0x0C0C0C, 0x0, 0x0E0E0E, 0x0,
	 0x101010, 0x0, 0x121212, 0x0, 0x141414, 0x0, 0x161616, 0x0,
	 0x181818, 0x0, 0x1A1A1A, 0x0, 0x1C1C1C, 0x0, 0x1E1E1E, 0x0,
	 0x202020, 0x0, 0x222222, 0x0, 0x242424, 0x0, 0x262626, 0x0,
	 0x282828, 0x0, 0x2A2A2A, 0x0, 0x2C2C2C, 0x0, 0x2E2E2E, 0x0,
	 0x303030, 0x0, 0x323232, 0x0, 0x343434, 0x0, 0x363636, 0x0,
	 0x383838, 0x0, 0x3A3A3A, 0x0, 0x3C3C3C, 0x0, 0x3E3E3E, 0x0,
	 0x404040, 0x0, 0x424242, 0x0, 0x444444, 0x0, 0x464646, 0x0,
	 0x484848, 0x0, 0x4A4A4A, 0x0, 0x4C4C4C, 0x0, 0x4E4E4E, 0x0,
	 0x505050, 0x0, 0x525252, 0x0, 0x545454, 0x0, 0x565656, 0x0,
	 0x585858, 0x0, 0x5A5A5A, 0x0, 0x5C5C5C, 0x0, 0x5E5E5E, 0x0,
	 0x606060, 0x0, 0x626262, 0x0, 0x646464, 0x0, 0x666666, 0x0,
	 0x686868, 0x0, 0x6A6A6A, 0x0, 0x6C6C6C, 0x0, 0x6E6E6E, 0x0,
	 0x707070, 0x0, 0x727272, 0x0, 0x747474, 0x0, 0x767676, 0x0,
	 0x787878, 0x0, 0x7A7A7A, 0x0, 0x7C7C7C, 0x0, 0x7E7E7E, 0x0,
	 0x808080, 0x0, 0x828282, 0x0, 0x848484, 0x0, 0x868686, 0x0,
	 0x888888, 0x0, 0x8A8A8A, 0x0, 0x8C8C8C, 0x0, 0x8E8E8E, 0x0,
	 0x909090, 0x0, 0x929292, 0x0, 0x949494, 0x0, 0x969696, 0x0,
	 0x989898, 0x0, 0x9A9A9A, 0x0, 0x9C9C9C, 0x0, 0x9E9E9E, 0x0,
	 0xA0A0A0, 0x0, 0xA2A2A2, 0x0, 0xA4A4A4, 0x0, 0xA6A6A6, 0x0,
	 0xA8A8A8, 0x0, 0xAAAAAA, 0x0, 0xACACAC, 0x0, 0xAEAEAE, 0x0,
	 0xB0B0B0, 0x0, 0xB2B2B2, 0x0, 0xB4B4B4, 0x0, 0xB6B6B6, 0x0,
	 0xB8B8B8, 0x0, 0xBABABA, 0x0, 0xBCBCBC, 0x0, 0xBEBEBE, 0x0,
	 0xC0C0C0, 0x0, 0xC2C2C2, 0x0, 0xC4C4C4, 0x0, 0xC6C6C6, 0x0,
	 0xC8C8C8, 0x0, 0xCACACA, 0x0, 0xCCCCCC, 0x0, 0xCECECE, 0x0,
	 0xD0D0D0, 0x0, 0xD2D2D2, 0x0, 0xD4D4D4, 0x0, 0xD6D6D6, 0x0,
	 0xD8D8D8, 0x0, 0xDADADA, 0x0, 0xDCDCDC, 0x0, 0xDEDEDE, 0x0,
	 0xE0E0E0, 0x0, 0xE2E2E2, 0x0, 0xE4E4E4, 0x0, 0xE6E6E6, 0x0,
	 0xE8E8E8, 0x0, 0xEAEAEA, 0x0, 0xECECEC, 0x0, 0xEEEEEE, 0x0,
	 0xF0F0F0, 0x0, 0xF2F2F2, 0x0, 0xF4F4F4, 0x0, 0xF6F6F6, 0x0,
	 0xF8F8F8, 0x0, 0xFAFAFA, 0x0, 0xFCFCFC, 0x0, 0xFEFEFE, 0x0},
	{0x000000, 0x0, 0x020202, 0x0, 0x040404, 0x0, 0x060606, 0x0,
	 0x080808, 0x0, 0x0A0A0A, 0x0, 0x0C0C0C, 0x0, 0x0E0E0E, 0x0,
	 0x101010, 0x0, 0x121212, 0x0, 0x141414, 0x0, 0x161616, 0x0,
	 0x181818, 0x0, 0x1A1A1A, 0x0, 0x1C1C1C, 0x0, 0x1E1E1E, 0x0,
	 0x202020, 0x0, 0x222222, 0x0, 0x242424, 0x0, 0x262626, 0x0,
	 0x282828, 0x0, 0x2A2A2A, 0x0, 0x2C2C2C, 0x0, 0x2E2E2E, 0x0,
	 0x303030, 0x0, 0x323232, 0x0, 0x343434, 0x0, 0x363636, 0x0,
	 0x383838, 0x0, 0x3A3A3A, 0x0, 0x3C3C3C, 0x0, 0x3E3E3E, 0x0,
	 0x404040, 0x0, 0x424242, 0x0, 0x444444, 0x0, 0x464646, 0x0,
	 0x484848, 0x0, 0x4A4A4A, 0x0, 0x4C4C4C, 0x0, 0x4E4E4E, 0x0,
	 0x505050, 0x0, 0x525252, 0x0, 0x545454, 0x0, 0x565656, 0x0,
	 0x585858, 0x0, 0x5A5A5A, 0x0, 0x5C5C5C, 0x0, 0x5E5E5E, 0x0,
	 0x606060, 0x0, 0x626262, 0x0, 0x646464, 0x0, 0x666666, 0x0,
	 0x686868, 0x0, 0x6A6A6A, 0x0, 0x6C6C6C, 0x0, 0x6E6E6E, 0x0,
	 0x707070, 0x0, 0x727272, 0x0, 0x747474, 0x0, 0x767676, 0x0,
	 0x787878, 0x0, 0x7A7A7A, 0x0, 0x7C7C7C, 0x0, 0x7E7E7E, 0x0,
	 0x808080, 0x0, 0x828282, 0x0, 0x848484, 0x0, 0x868686, 0x0,
	 0x888888, 0x0, 0x8A8A8A, 0x0, 0x8C8C8C, 0x0, 0x8E8E8E, 0x0,
	 0x909090, 0x0, 0x929292, 0x0, 0x949494, 0x0, 0x969696, 0x0,
	 0x989898, 0x0, 0x9A9A9A, 0x0, 0x9C9C9C, 0x0, 0x9E9E9E, 0x0,
	 0xA0A0A0, 0x0, 0xA2A2A2, 0x0, 0xA4A4A4, 0x0, 0xA6A6A6, 0x0,
	 0xA8A8A8, 0x0, 0xAAAAAA, 0x0, 0xACACAC, 0x0, 0xAEAEAE, 0x0,
	 0xB0B0B0, 0x0, 0xB2B2B2, 0x0, 0xB4B4B4, 0x0, 0xB6B6B6, 0x0,
	 0xB8B8B8, 0x0, 0xBABABA, 0x0, 0xBCBCBC, 0x0, 0xBEBEBE, 0x0,
	 0xC0C0C0, 0x0, 0xC2C2C2, 0x0, 0xC4C4C4, 0x0, 0xC6C6C6, 0x0,
	 0xC8C8C8, 0x0, 0xCACACA, 0x0, 0xCCCCCC, 0x0, 0xCECECE, 0x0,
	 0xD0D0D0, 0x0, 0xD2D2D2, 0x0, 0xD4D4D4, 0x0, 0xD6D6D6, 0x0,
	 0xD8D8D8, 0x0, 0xDADADA, 0x0, 0xDCDCDC, 0x0, 0xDEDEDE, 0x0,
	 0xE0E0E0, 0x0, 0xE2E2E2, 0x0, 0xE4E4E4, 0x0, 0xE6E6E6, 0x0,
	 0xE8E8E8, 0x0, 0xEAEAEA, 0x0, 0xECECEC, 0x0, 0xEEEEEE, 0x0,
	 0xF0F0F0, 0x0, 0xF2F2F2, 0x0, 0xF4F4F4, 0x0, 0xF6F6F6, 0x0,
	 0xF8F8F8, 0x0, 0xFAFAFA, 0x0, 0xFCFCFC, 0x0, 0xFEFEFE, 0x0}
};

/* GCMAX soft lookup table */
u32 gcmax_softlut[MAX_PIPES_VLV][GC_MAX_COUNT] =  {
	{0x10000, 0x10000, 0x10000},
	{0x10000, 0x10000, 0x10000}
};

/* Hue Saturation defaults */
struct hue_saturationlut savedhsvalues[NO_SPRITE_REG] = {
	{SPRITEA, 0x1000000},
	{SPRITEB, 0x1000000},
	{SPRITEC, 0x1000000},
	{SPRITED, 0x1000000}
};

/* Contrast brightness defaults */
struct cont_brightlut savedcbvalues[NO_SPRITE_REG] = {
	{SPRITEA, 0x80},
	{SPRITEB, 0x80},
	{SPRITEC, 0x80},
	{SPRITED, 0x80}
};

/* Color space conversion coff's */
u32 csc_softlut[MAX_PIPES_VLV][CSC_MAX_COEFF_COUNT] = {
	{ 1024,	 0, 67108864, 0, 0, 1024 },
	{ 1024,	 0, 67108864, 0, 0, 1024 }
};

/*
 * Gen 6 SOC allows following color correction values:
 *     - CSC(wide gamut) with 3x3 matrix = 9 csc correction values.
 *     - Gamma correction with 128 gamma values.
 */
struct clrmgr_property gen6_pipe_color_corrections[] = {
	{
		.tweak_id = cgm_csc,
		.type = DRM_MODE_PROP_BLOB,
		.len = CHV_CGM_CSC_MATRIX_MAX_VALS,
		.name = "cgm-csc-correction",
		.set_property = intel_clrmgr_set_cgm_csc,
	},
	{
		.tweak_id = cgm_gamma,
		.type = DRM_MODE_PROP_BLOB,
		.len = CHV_CGM_GAMMA_MATRIX_MAX_VALS,
		.name = "cgm-gamma-correction",
		.set_property = intel_clrmgr_set_cgm_gamma,
	},
	{
		.tweak_id = cgm_degamma,
		.type = DRM_MODE_PROP_BLOB,
		.len = CHV_CGM_DEGAMMA_MATRIX_MAX_VALS,
		.name = "cgm-degamma-correction",
		.set_property = intel_clrmgr_set_cgm_degamma,
	},
};

static u32 cgm_ctrl[] = {
	PIPEA_CGM_CTRL,
	PIPEB_CGM_CTRL,
	PIPEC_CGM_CTRL
};

static u32 cgm_degamma_st[] = {
	PIPEA_CGM_DEGAMMA_ST,
	PIPEB_CGM_DEGAMMA_ST,
	PIPEC_CGM_DEGAMMA_ST
};

static u32 cgm_csc_st[] = {
	PIPEA_CGM_CSC_ST,
	PIPEB_CGM_CSC_ST,
	PIPEC_CGM_CSC_ST
};

static u32 cgm_gamma_st[] = {
	PIPEA_CGM_GAMMA_ST,
	PIPEB_CGM_GAMMA_ST,
	PIPEC_CGM_GAMMA_ST
};

/* Enable color space conversion on PIPE */
int
do_intel_enable_csc(struct drm_device *dev, void *data, struct drm_crtc *crtc)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = NULL;
	u32 pipeconf = 0;
	int pipe = 0;
	u32 csc_reg = 0;
	int i = 0, j = 0;

	if (!data) {
		DRM_ERROR("NULL input to enable CSC");
		return -EINVAL;
	}

	intel_crtc = to_intel_crtc(crtc);
	pipe = intel_crtc->pipe;
	DRM_DEBUG_DRIVER("pipe = %d\n", pipe);
	pipeconf = I915_READ(PIPECONF(pipe));
	pipeconf |= PIPECONF_CSC_ENABLE;

	if (pipe == 0)
		csc_reg = _PIPEACSC;
	else if (pipe == 1)
		csc_reg = _PIPEBCSC;
	else {
		DRM_ERROR("Invalid pipe input");
		return -EINVAL;
	}

	/* Enable csc correction */
	I915_WRITE(PIPECONF(pipe), pipeconf);
	dev_priv->csc_enabled[pipe] = true;
	POSTING_READ(PIPECONF(pipe));

	/* Write csc coeff to csc regs */
	for (i = 0; i < 6; i++) {
		I915_WRITE(csc_reg + j, ((u32 *)data)[i]);
		j = j + 0x4;
	}
	return 0;
}

/* Disable color space conversion on PIPE */
void
do_intel_disable_csc(struct drm_device *dev, struct drm_crtc *crtc)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = NULL;
	u32 pipeconf = 0;
	int pipe = 0;

	intel_crtc = to_intel_crtc(crtc);
	pipe = intel_crtc->pipe;
	pipeconf = I915_READ(PIPECONF(pipe));
	pipeconf &= ~(PIPECONF_CSC_ENABLE);

	/* Disable CSC on PIPE */
	I915_WRITE(PIPECONF(pipe), pipeconf);
	dev_priv->csc_enabled[pipe] = false;
	POSTING_READ(PIPECONF(pipe));
	return;
}

/* Parse userspace input coming from dev node*/
int parse_clrmgr_input(uint *dest, char *src, int max, int *num_bytes)
{
	int size = 0;
	int bytes = 0;
	char *populate = NULL;

	/* Check for trailing comma or \n */
	if (!dest || !src || *src == ',' || *src == '\n' || !(*num_bytes)) {
		DRM_ERROR("Invalid input to parse");
		return -EINVAL;
	}

	/* limit check */
	if (*num_bytes < max) {
		DRM_ERROR("Invalid input to parse");
		return -EINVAL;
	}

	/* Extract values from buffer */
	while ((size < max) && (*src != '\n')) {
		populate = strsep(&src, ",");
		if (!populate)
			break;

		bytes += (strlen(populate)+1);
		if (kstrtouint((const char *)populate, CLRMGR_BASE,
					&dest[size++])) {
			DRM_ERROR("Parse: Invalid limit");
			return -EINVAL;
		}
		if (src == NULL || *src == '\0')
			break;
	}
	/* Fill num_bytes with number of bytes read */
	*num_bytes = bytes;

	/* Return number of tokens parsed */
	return size;
}

/* Gamma correction for sprite planes on External display */
int intel_enable_external_sprite_gamma(struct drm_crtc *crtc, int planeid)
{
	DRM_ERROR("This functionality is not implemented yet\n");
	return -ENOSYS;
}

/* Gamma correction for External display plane*/
int intel_enable_external_gamma(struct drm_crtc *crtc)
{
	DRM_ERROR("This functionality is not implemented yet\n");
	return -ENOSYS;
}

/* Gamma correction for External pipe */
int intel_enable_external_pipe_gamma(struct drm_crtc *crtc)
{
	DRM_ERROR("This functionality is not implemented yet\n");
	return -ENOSYS;
}

/* Gamma correction for sprite planes on Primary display */
int intel_enable_sprite_gamma(struct drm_crtc *crtc, int planeid)
{
	u32 count = 0;
	u32 status = 0;
	u32 controlreg = 0;
	u32 correctreg = 0;

	struct drm_device *dev = crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	switch (planeid) {
	case SPRITEA:
		correctreg = GAMMA_SPA_GAMC0;
		controlreg = GAMMA_SPA_CNTRL;
		break;

	case SPRITEB:
		correctreg = GAMMA_SPB_GAMC0;
		controlreg = GAMMA_SPB_CNTRL;
		break;

	case SPRITEC:
	case SPRITED:
		return intel_enable_external_sprite_gamma(crtc, planeid);

	default:
		DRM_ERROR("Invalid sprite object gamma enable\n");
		return -EINVAL;
	}

	/* Write gamma cofficients in gamma regs*/
	while (count < GAMMA_SP_MAX_COUNT) {
		/* Write and read */
		I915_WRITE(correctreg - 4 * count, gamma_sprite_softlut[count]);
		status = I915_READ(correctreg - 4 * count++);
	}

	/* Enable gamma on plane */
	status = I915_READ(controlreg);
	status |= GAMMA_ENABLE_SPR;
	I915_WRITE(controlreg, status);

	DRM_DEBUG("Gamma applied on plane sprite%c\n",
		(planeid == SPRITEA) ? 'A' : 'B');

	return 0;
}

/* Gamma correction at Plane level */
int intel_enable_primary_gamma(struct drm_crtc *crtc)
{
	u32 odd = 0;
	u32 even = 0;
	u32 count = 0;
	u32 palreg = 0;
	u32 status = 0;
	u32 pipe = 0;
	struct intel_crtc *intel_crtc;
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;

	/* Validate input */
	if (!crtc) {
		DRM_ERROR("Invalid CRTC object input to gamma enable\n");
		return -EINVAL;
	}

	intel_crtc = to_intel_crtc(crtc);
	pipe = intel_crtc->pipe;
	dev = crtc->dev;
	dev_priv = dev->dev_private;

	palreg = PALETTE(pipe);
	 /* 10.6 mode Gamma Implementation */
	while (count < GAMMA_CORRECT_MAX_COUNT) {
		/* Get the gamma corrected value from table */
		odd = gamma_softlut[pipe][count];
		even = gamma_softlut[pipe][count + 1];

		/* Write even and odd parts in palette regs*/
		I915_WRITE(palreg + 4 * count, even);
		I915_WRITE(palreg + 4 * ++count, odd);
		count++;
	}

	/* Write max values in 11.6 format */
	I915_WRITE(PIPEA_GAMMA_MAX_BLUE, gcmax_softlut[pipe][0]);
	I915_WRITE(PIPEA_GAMMA_MAX_GREEN, gcmax_softlut[pipe][1]);
	I915_WRITE(PIPEA_GAMMA_MAX_RED, gcmax_softlut[pipe][2]);

	/* Enable gamma on PIPE  */
	status = I915_READ(PIPECONF(pipe));
	status |= PIPECONF_GAMMA;
	I915_WRITE(PIPECONF(pipe), status);
	DRM_DEBUG("Gamma enabled on Plane A\n");

	return 0;
}


/*
 * chv_set_cgm_csc
 * Cherryview specific csc correction method on PIPE.
 * inputs:
 * - intel_crtc*
 * - color manager registered property for cgm_csc_correction
 * - data: pointer to correction values to be applied
 */
bool chv_set_cgm_csc(struct intel_crtc *intel_crtc,
	const struct clrmgr_regd_prop *cgm_csc, const int *data, bool enable)
{
	u32 cgm_csc_reg, cgm_ctrl_reg, data_size, i;
	struct drm_device *dev = intel_crtc->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_property *property;

	property = cgm_csc->property;
	data_size = property->num_values;

	/* Validate input */
	if (data_size != CHV_CGM_CSC_MATRIX_MAX_VALS) {
		DRM_ERROR("Unexpected value count for CSC LUT\n");
		return false;
	}
	cgm_ctrl_reg = dev_priv->info.display_mmio_offset +
			cgm_ctrl[intel_crtc->pipe];

	if (enable) {
		DRM_DEBUG_DRIVER("Setting CSC on pipe = %d\n",
						intel_crtc->pipe);

		/* program CGM CSC values */
		cgm_csc_reg = dev_priv->info.display_mmio_offset +
					cgm_csc_st[intel_crtc->pipe];

		/*
		 * the input data is 32 bit signed int array
		 * of 9 coefficients and for 5 registers
		 * C0 - 16 bit (LSB)
		 * C1 - 16 bit (HSB)
		 * C8 - 16 bit (LSB) and HSB- reserved.
		 */
		for (i = 0; i < CGM_CSC_MAX_REGS; i++) {
			I915_WRITE(cgm_csc_reg + i*4,
					(data[EVEN(i)] >> 16) |
					((data[ODD(i)] >> 16) << 16));

			/* The last register has only valid LSB */
			if (i == 4)
				I915_WRITE(cgm_csc_reg + i*4,
					(data[EVEN(i)] >> 16));
		}

		/* enable csc if not enabled */
		if (!(I915_READ(cgm_ctrl_reg) & CGM_CSC_EN))
			I915_WRITE(cgm_ctrl_reg,
					I915_READ(cgm_ctrl_reg) | CGM_CSC_EN);
	} else {
		I915_WRITE(cgm_ctrl_reg,
			I915_READ(cgm_ctrl_reg) & ~CGM_CSC_EN);
	}
	return true;
}

/*
 * chv_set_cgm_gamma
 * Cherryview specific u0.10 cgm gamma correction method on PIPE.
 * inputs:
 * - intel_crtc*
 * - color manager registered property for cgm_csc_correction
 * - data: pointer to correction values to be applied
 */
bool chv_set_cgm_gamma(struct intel_crtc *intel_crtc,
			const struct clrmgr_regd_prop *cgm_gamma,
			const struct gamma_lut_data *data, bool enable)
{
	u32 i = 0;
	u32 cgm_gamma_reg = 0;
	u32 cgm_ctrl_reg = 0;

	struct drm_device *dev = intel_crtc->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_property *property;

	property = cgm_gamma->property;

	/* Validate input */
	if (!intel_crtc) {
		DRM_ERROR("Invalid CRTC object input to CGM gamma enable\n");
		return false;
	}
	cgm_ctrl_reg = dev_priv->info.display_mmio_offset +
			cgm_ctrl[intel_crtc->pipe];
	if (enable) {
		/*
		 * program CGM Gamma values is in
		 * u0.10 while i/p is 16 bit
		 */
		cgm_gamma_reg = dev_priv->info.display_mmio_offset +
				cgm_gamma_st[intel_crtc->pipe];

		for (i = 0; i < CHV_CGM_GAMMA_MATRIX_MAX_VALS; i++) {

			/* Red coefficent needs to be updated in D1 registers*/
			I915_WRITE(cgm_gamma_reg + 4 * ODD(i),
						(data[i].red) >> 6);

			/*
			 * green and blue coefficients
			 * need to be updated in D0 registers
			 */
			I915_WRITE(cgm_gamma_reg + 4 * EVEN(i),
					(((data[i].green) >> 6) << 16) |
					((data[i].blue) >> 6));
		}

		if (!(I915_READ(cgm_ctrl_reg) & CGM_GAMMA_EN)) {
			I915_WRITE(cgm_ctrl_reg,
				I915_READ(cgm_ctrl_reg) | CGM_GAMMA_EN);
			DRM_DEBUG("CGM Gamma enabled on Pipe %d\n",
							intel_crtc->pipe);
		}

	} else {
		I915_WRITE(cgm_ctrl_reg,
				I915_READ(cgm_ctrl_reg) & ~CGM_GAMMA_EN);
	}
	return true;
}

/*
 * chv_set_cgm_degamma
 * Cherryview specific cgm degamma correction method on PIPE.
 *  inputs:
 * - intel_crtc*
 * - color manager registered property for cgm_csc_correction
 * - data: pointer to correction values to be applied.
 */
bool chv_set_cgm_degamma(struct intel_crtc *intel_crtc,
			const struct clrmgr_regd_prop *cgm_degamma,
			const struct gamma_lut_data *data, bool enable)
{
	struct drm_device *dev = intel_crtc->base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_property *property;
	u32 i = 0;
	u32 cgm_degamma_reg = 0;
	u32 cgm_ctrl_reg = 0;
	property = cgm_degamma->property;

	/* Validate input */
	if (!intel_crtc) {
		DRM_ERROR("Invalid CRTC object i/p to CGM degamma enable\n");
		return -EINVAL;
	}
	cgm_ctrl_reg = dev_priv->info.display_mmio_offset +
			cgm_ctrl[intel_crtc->pipe];

	if (enable) {
		/* program CGM Gamma values is in u0.10 */
		cgm_degamma_reg = dev_priv->info.display_mmio_offset +
					cgm_degamma_st[intel_crtc->pipe];

		for (i = 0; i < CHV_CGM_DEGAMMA_MATRIX_MAX_VALS; i++) {
			/* Red coefficent needs to be updated in D1 registers*/
			I915_WRITE(cgm_degamma_reg + 4 * ODD(i),
						((data[i].red) >> 2));

			/*
			 * green and blue coefficients
			 * need to be updated in D0 registers
			 */
			I915_WRITE(cgm_degamma_reg + 4 * EVEN(i),
					(((data[i].green) >> 2) << 16) |
						((data[i].blue) >> 2));
		}

		/* If already enabled, do not enable again */
		if (!(I915_READ(cgm_ctrl_reg) & CGM_DEGAMMA_EN)) {
			I915_WRITE(cgm_ctrl_reg,
				I915_READ(cgm_ctrl_reg) | CGM_DEGAMMA_EN);
			DRM_DEBUG("CGM Degamma enabled on Pipe %d\n",
							intel_crtc->pipe);
		}

	 } else {
		I915_WRITE(cgm_ctrl_reg,
			I915_READ(cgm_ctrl_reg) & ~CGM_DEGAMMA_EN);
	}
	return true;
}

/*
 * intel_clrmgr_set_csc
 * CSC correction method is different across various
 * gen devices. c
 * inputs:
 * - intel_crtc *
 * - color manager registered property for csc correction
 * - data: pointer to correction values to be applied
 */
bool intel_clrmgr_set_cgm_csc(void *crtc,
	const struct clrmgr_regd_prop *cgm_csc, const struct lut_info *info)
{
	struct intel_crtc *intel_crtc = crtc;
	struct drm_device *dev = intel_crtc->base.dev;
	int *data;
	int ret = false;

	/* Validate input */
	if (!info || !info->data || !cgm_csc || !cgm_csc->property) {
		DRM_ERROR("Invalid input to set cgm_csc\n");
		return ret;
	}

#ifdef CLRMGR_DEBUG
	DRM_DEBUG_DRIVER("Clrmgr: Set csc: data len=%d\n",
			cgm_csc->property->num_values);
#endif
	data = kmalloc(sizeof(int) * (cgm_csc->property->num_values),
							GFP_KERNEL);
	if (!data) {
		DRM_ERROR("Out of memory\n");
		return ret;
	}

	if (copy_from_user(data, (const int __user *)info->data,
			cgm_csc->property->num_values * sizeof(int))) {
		DRM_ERROR("Failed to copy all data\n");
		goto free;
	}

	/* CHV CGM CSC color correction */
	if (IS_CHERRYVIEW(dev)) {
		if (chv_set_cgm_csc(intel_crtc, cgm_csc,
					data, info->enable))
			ret = true;
		goto free;
	}

	/* Todo: Support other gen devices */
	DRM_ERROR("CGM correction is supported only on CHV\n");

free:	kfree(data);
	return ret;
}

/*
* Gamma correction at PIPE level:
* This function applies gamma correction Primary as well as Sprite planes
* assosiated with this PIPE. Assumptions are:
* Plane A is internal display primary panel.
* Sprite A and B are interal display's sprite planes.
*/
int intel_enable_pipe_gamma(struct drm_crtc *crtc)
{
	u32 odd = 0;
	u32 even = 0;
	u32 count = 0;
	u32 palreg = 0;
	u32 status = 0;
	u32 pipe = 0;
	struct intel_crtc *intel_crtc;
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;

	/* Validate input */
	if (!crtc) {
		DRM_ERROR("Invalid CRTC object input to gamma enable\n");
		return -EINVAL;
	}

	intel_crtc = to_intel_crtc(crtc);
	pipe = intel_crtc->pipe;
	dev = crtc->dev;
	dev_priv = dev->dev_private;
	dev_priv->gamma_enabled[pipe] = true;

	palreg = PALETTE(pipe);
	 /* 10.6 mode Gamma Implementation */
	while (count < GAMMA_CORRECT_MAX_COUNT) {
		/* Get the gamma corrected value from table */
		odd = gamma_softlut[pipe][count];
		even = gamma_softlut[pipe][count + 1];

		/* Write even and odd parts in palette regs*/
		I915_WRITE(palreg + 4 * count, even);
		I915_WRITE(palreg + 4 * ++count, odd);
		count++;
	}

	/* Write max values in 11.6 format */
	I915_WRITE(PIPE_GAMMA_MAX_BLUE(pipe), gcmax_softlut[pipe][0]);
	I915_WRITE(PIPE_GAMMA_MAX_GREEN(pipe), gcmax_softlut[pipe][1]);
	I915_WRITE(PIPE_GAMMA_MAX_RED(pipe), gcmax_softlut[pipe][2]);

	/* Enable gamma for Plane A  */
	status = I915_READ(PIPECONF(pipe));
	status |= PIPECONF_GAMMA;
	I915_WRITE(PIPECONF(pipe), status);

	/* Enable gamma on Sprite plane A*/
	status = I915_READ(GAMMA_SP1_CNTRL(pipe));
	status |= GAMMA_ENABLE_SPR;
	I915_WRITE(GAMMA_SP1_CNTRL(pipe), status);

	/* Enable gamma on Sprite plane B*/
	status = I915_READ(GAMMA_SP2_CNTRL(pipe));
	status |= GAMMA_ENABLE_SPR;
	I915_WRITE(GAMMA_SP2_CNTRL(pipe), status);

	DRM_DEBUG("Gamma enabled on Pipe A\n");
	return 0;
}

/* Load gamma correction values corresponding to supplied
gamma and program palette accordingly */
int intel_crtc_enable_gamma(struct drm_crtc *crtc, u32 identifier)
{
	switch (identifier) {
	/* Whole pipe level correction */
	case PIPEA:
	case PIPEB:
		return intel_enable_pipe_gamma(crtc);

	/* Primary display planes */
	case PLANEA:
		return intel_enable_primary_gamma(crtc);
	case PLANEB:
		return intel_enable_external_gamma(crtc);

	/* Sprite planes */
	case SPRITEA:
	case SPRITEB:
		return intel_enable_sprite_gamma(crtc, identifier);
	case SPRITEC:
	case SPRITED:
		return intel_enable_external_sprite_gamma(crtc, identifier);

	default:
		DRM_ERROR("Invalid panel ID to Gamma enabled\n");
		return -EINVAL;
	}
}

int intel_disable_external_sprite_gamma(struct drm_crtc *crtc, u32 planeid)
{
	DRM_ERROR("This functionality is not implemented yet\n");
	return -EINVAL;
}

/* Disable Gamma correction on external display */
int intel_disable_external_gamma(struct drm_crtc *crtc)
{
	DRM_ERROR("This functionality is not implemented yet\n");
	return -EINVAL;
}

/*
* intel_clrmgr_set_gamma
* Gamma correction method is different across various
* gen devices. This is a wrapper function which will call
* the platform specific gamma set function
* inputs:
* - intel_crtc*
* - color manager registered property for gamma correction
* - data: pointer to correction values to be applied
*/
bool intel_clrmgr_set_cgm_gamma(void *crtc,
		const struct clrmgr_regd_prop *cgm_gamma,
				const struct lut_info  *info)
{
	struct intel_crtc *intel_crtc = crtc;
	struct drm_device *dev = intel_crtc->base.dev;
	struct gamma_lut_data *data;
	int ret = false;

	/* Validate input */
	if (!info->data || !cgm_gamma || !cgm_gamma->property) {
		DRM_ERROR("Invalid input to set_gamma\n");
		return ret;
	}

	DRM_DEBUG_DRIVER("Setting gamma correction, len=%d\n",
		cgm_gamma->property->num_values);
#ifdef CLRMGR_DEBUG
	DRM_DEBUG_DRIVER("Clrmgr: Set gamma: len=%d\n",
				cgm_gamma->property->num_values);
#endif
	data = kmalloc(sizeof(struct gamma_lut_data) *
				(cgm_gamma->property->num_values),
				GFP_KERNEL);
	if (!data) {
		DRM_ERROR("Out of memory\n");
		return ret;
	}

	if (copy_from_user(data,
		(const struct gamma_lut_data __user *) info->data,
		cgm_gamma->property->num_values *
		sizeof(struct gamma_lut_data))) {

		DRM_ERROR("Failed to copy all data\n");
		goto free;
	}

	/* CHV has CGM gamma correction */
	if (IS_CHERRYVIEW(dev)) {
		if (chv_set_cgm_gamma(intel_crtc,
				cgm_gamma, data, info->enable))
			ret = true;
		goto free;
	}

	/* Todo: Support other gen devices */
	DRM_ERROR("Color correction is supported only on VLV for now\n");
free:	kfree(data);
	return ret;
}

/*
* intel_clrmgr_set_cgm_degamma
* Gamma correction method is different across various
* gen devices. This is a wrapper function which will call
* the platform specific gamma set function
* inputs:
* - intel_crtc*
* - color manager registered property for gamma correction
* - data: pointer to correction values to be applied
*/
bool intel_clrmgr_set_cgm_degamma(void *crtc,
		const struct clrmgr_regd_prop *cgm_degamma,
				const struct lut_info *info)
{
	struct intel_crtc *intel_crtc = crtc;
	struct drm_device *dev = intel_crtc->base.dev;
	struct gamma_lut_data *data;
	int ret = false;

	/* Validate input */
	if (!info->data || !cgm_degamma || !cgm_degamma->property) {
		DRM_ERROR("Invalid input to set_gamma\n");
		return ret;
	}

	DRM_DEBUG_DRIVER("Setting gamma correction, len=%d\n",
		cgm_degamma->property->num_values);
#ifdef CLRMGR_DEBUG
	DRM_DEBUG_DRIVER("Clrmgr: Set gamma: len=%d\n",
			cgm_degamma->property->num_values);
#endif
	data = kmalloc(sizeof(struct gamma_lut_data) *
				(cgm_degamma->property->num_values),
				GFP_KERNEL);
	if (!data) {
		DRM_ERROR("Out of memory\n");
		goto free;
	}

	if (copy_from_user(data,
			(const struct gamma_lut_data __user *) info->data,
			cgm_degamma->property->num_values *
			sizeof(struct gamma_lut_data))) {
		DRM_ERROR("Failed to copy all data\n");
		goto free;
	}

	/* CHV has CGM degamma correction */
	if (IS_CHERRYVIEW(dev)) {
		if (chv_set_cgm_degamma(intel_crtc,
			cgm_degamma, data, info->enable))
			ret = true;
		goto free;
	}
	/* Todo: Support other gen devices */
	DRM_ERROR("Color correction is supported only on VLV for now\n");

free:	kfree(data);
	return ret;
}


/*
 * intel_clrmgr_set_property
 * Set the value of a DRM color correction property
 * and program the corresponding registers
 * Inputs:
 *  - intel_crtc *
 *  - color manager registered property * which encapsulates
 *    drm_property and additional data.
 * - value is the new value to be set
 */
bool intel_clrmgr_set_pipe_property(struct intel_crtc *intel_crtc,
		struct clrmgr_regd_prop *cp, uint64_t value)
{
	bool ret = false;
	struct lut_info *info;

	/* Sanity */
	if (!cp || !cp->property || !value) {
		DRM_ERROR("NULL input to set_property\n");
		return false;
	}

	DRM_DEBUG_DRIVER("Property %s len:%d\n",
				cp->property->name, cp->property->num_values);

	info = kmalloc(sizeof(struct lut_info), GFP_KERNEL);
	if (!info) {
		DRM_ERROR("Out of memory\n");
		return false;
	}

	info = (struct lut_info *) (uintptr_t) value;

	/* call the corresponding set property */
	if (cp->set_property) {
		if (!cp->set_property((void *)intel_crtc, cp, info)) {
			DRM_ERROR("Set property for %s failed\n",
						cp->property->name);
			return ret;
		} else {
			ret = true;
			cp->enabled = true;
			DRM_DEBUG_DRIVER("Set property %s successful\n",
				cp->property->name);
		}
	}

	return ret;
}

/* Disable gamma correction for sprite planes on primary display */
int intel_disable_sprite_gamma(struct drm_crtc *crtc, u32 planeid)
{
	u32 status = 0;
	u32 controlreg = 0;

	struct drm_device *dev = crtc->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	switch (planeid) {
	case SPRITEA:
		controlreg = GAMMA_SPA_CNTRL;
		break;

	case SPRITEB:
		controlreg = GAMMA_SPB_CNTRL;
		break;

	default:
		DRM_ERROR("Invalid sprite object gamma enable\n");
		return -EINVAL;
	}

	/* Reset pal regs */
	intel_crtc_load_lut(crtc);

	/* Disable gamma on PIPE config  */
	status = I915_READ(controlreg);
	status &= ~(GAMMA_ENABLE_SPR);
	I915_WRITE(controlreg, status);

	/* TODO: Reset gamma table default */
	DRM_DEBUG("Gamma on Sprite %c disabled\n",
		(planeid == SPRITEA) ? 'A' : 'B');

	return 0;
}

/* Disable gamma correction on Primary display */
int intel_disable_primary_gamma(struct drm_crtc *crtc)
{
	u32 status = 0;
	struct drm_device *dev = crtc->dev;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	struct drm_i915_private *dev_priv = dev->dev_private;

	/* Reset pal regs */
	intel_crtc_load_lut(crtc);

	/* Disable gamma on PIPE config  */
	status = I915_READ(PIPECONF(intel_crtc->pipe));
	status &= ~(PIPECONF_GAMMA);
	I915_WRITE(PIPECONF(intel_crtc->pipe), status);

	/* TODO: Reset gamma table default */
	DRM_DEBUG("Gamma disabled on Pipe\n");
	return 0;
}

/*
 * intel_clrmgr_register:
 * Register color correction properties as DRM propeties
 */
struct drm_property *intel_clrmgr_register(struct drm_device *dev,
	struct drm_mode_object *obj, const struct clrmgr_property *cp)
{
	struct drm_property *property;

	/* Create drm property */
	switch (cp->type) {

	case DRM_MODE_PROP_BLOB:
		property = drm_property_create(dev,
				DRM_MODE_PROP_BLOB,
					cp->name, cp->len);
		if (!property) {
			DRM_ERROR("Failed to create property %s\n",
							cp->name);
			return NULL;
		}

		/* Attach property to object */
		drm_object_attach_property(obj, property, 0);
		break;

	case DRM_MODE_PROP_RANGE:
		property = drm_property_create_range(dev,
				DRM_MODE_PROP_RANGE, cp->name,
						cp->min, cp->max);
		if (!property) {
			DRM_ERROR("Failed to create property %s\n",
							cp->name);
			return NULL;
		}
		drm_object_attach_property(obj, property, 0);
		break;

	default:
		DRM_ERROR("Unsupported type for property %s\n",
							cp->name);
		return NULL;
	}

	DRM_DEBUG_DRIVER("Registered property %s\n", property->name);
	return property;
}

bool intel_clrmgr_register_pipe_property(struct intel_crtc *intel_crtc,
		struct clrmgr_reg_request *features)
{
	u32 count = 0;
	struct clrmgr_property *cp;
	struct clrmgr_regd_prop *regd_property;
	struct drm_property *property;
	struct drm_device *dev = intel_crtc->base.dev;
	struct drm_mode_object *obj = &intel_crtc->base.base;
	struct clrmgr_status *status = intel_crtc->color_status;

	/* Color manager initialized? */
	if (!status) {
		DRM_ERROR("Request wihout pipe init\n");
		return false;
	}

	/* Validate input */
	if (!features || !features->no_of_properties) {
		DRM_ERROR("Invalid input to color manager register\n");
		return false;
	}

	/* Create drm property */
	while (count < features->no_of_properties) {
		cp = &features->cp[count++];
		property = intel_clrmgr_register(dev, obj, cp);
		if (!property) {
			DRM_ERROR("Failed to register property %s\n",
							property->name);
			goto error;
		}

		/* Add the property in global pipe status */
		regd_property = kzalloc(sizeof(struct clrmgr_regd_prop),
								GFP_KERNEL);
		regd_property->property = property;
		regd_property->enabled = false;
		regd_property->set_property = cp->set_property;
		status->cp[status->no_of_properties++] = regd_property;
	}
	/* Successfully registered all */
	DRM_DEBUG_DRIVER("Registered color properties on pipe %c\n",
		pipe_name(intel_crtc->pipe));
	return true;

error:
	if (--count) {
		DRM_ERROR("Can only register following properties\n");
		while (count--)
			DRM_ERROR("%s", status->cp[count]->property->name);
	} else
		DRM_ERROR("Can not register any property\n");
	return false;
}

/* Disable gamma correction on Primary display */
int intel_disable_pipe_gamma(struct drm_crtc *crtc)
{
	u32 status = 0;
	struct drm_device *dev = crtc->dev;
	struct intel_crtc *intel_crtc = to_intel_crtc(crtc);
	struct drm_i915_private *dev_priv = dev->dev_private;

	dev_priv->gamma_enabled[intel_crtc->pipe] = false;

	/* Reset pal regs */
	intel_crtc_load_lut(crtc);

	/* Disable gamma on PIPE config  */
	status = I915_READ(PIPECONF(intel_crtc->pipe));
	status &= ~(PIPECONF_GAMMA);
	I915_WRITE(PIPECONF(intel_crtc->pipe), status);

	/* Disable gamma on SpriteA  */
	status = I915_READ(GAMMA_SP1_CNTRL(intel_crtc->pipe));
	status &= ~(GAMMA_ENABLE_SPR);
	I915_WRITE(GAMMA_SP1_CNTRL(intel_crtc->pipe), status);

	/* Disable gamma on SpriteB  */
	status = I915_READ(GAMMA_SP2_CNTRL(intel_crtc->pipe));
	status &= ~(GAMMA_ENABLE_SPR);
	I915_WRITE(GAMMA_SP2_CNTRL(intel_crtc->pipe), status);

	/* TODO: Reset gamma table default */
	DRM_DEBUG("Gamma disabled on Pipe %d\n", intel_crtc->pipe);
	return 0;
}

/*
* intel_clrmgr_deregister
* De register color manager properties
* destroy the DRM property and cleanup
* Should be called from CRTC/Plane .destroy function
* input:
* - struct drm device *dev
* - status: attached colot status
*/
void intel_clrmgr_deregister(struct drm_device *dev,
	struct clrmgr_status *status)
{
	u32 count = 0;
	struct clrmgr_regd_prop *cp;

	/* Free drm property */
	while (count < status->no_of_properties) {
		cp = status->cp[count++];

		/* Destroy property */
		drm_property_destroy(dev, cp->property);

		/* Release the color property */
		kfree(status->cp[count]);
		status->cp[count] = NULL;
	}

	/* Successfully deregistered all */
	DRM_DEBUG_DRIVER("De-registered all color properties\n");
}

/* Load gamma correction values corresponding to supplied
gamma and program palette accordingly */
int intel_crtc_disable_gamma(struct drm_crtc *crtc, u32 identifier)
{
	switch (identifier) {
	/* Whole pipe level correction */
	case PIPEA:
	case PIPEB:
		return intel_disable_pipe_gamma(crtc);
	/* Primary planes */
	case PLANEA:
		return intel_disable_primary_gamma(crtc);
	case PLANEB:
		return intel_disable_external_gamma(crtc);
	/* Sprite plane */
	case SPRITEA:
	case SPRITEB:
		return intel_disable_sprite_gamma(crtc, identifier);
	case SPRITEC:
	case SPRITED:
		return intel_disable_external_sprite_gamma(crtc, identifier);
	default:
		DRM_ERROR("Invalid panel ID to Gamma enabled\n");
		return -EINVAL;
	}
	return 0;
}

/* Tune Contrast Brightness Value for Sprite */
int intel_sprite_cb_adjust(struct drm_i915_private *dev_priv,
		struct cont_brightlut *cb_ptr)
{
	if (!dev_priv || !cb_ptr) {
		DRM_ERROR("Contrast Brightness: Invalid Arguments\n");
		return -EINVAL;
	}

	switch (cb_ptr->sprite_no) {
	/* Sprite plane */
	case SPRITEA:
		if (is_sprite_enabled(dev_priv, 0, 0) || dev_priv->is_resuming)
			I915_WRITE(SPRITEA_CB_REG, cb_ptr->val);
	break;
	case SPRITEB:
		if (is_sprite_enabled(dev_priv, 0, 1) || dev_priv->is_resuming)
			I915_WRITE(SPRITEB_CB_REG, cb_ptr->val);
	break;
	case SPRITEC:
		if (is_sprite_enabled(dev_priv, 1, 0) || dev_priv->is_resuming)
			I915_WRITE(SPRITEC_CB_REG, cb_ptr->val);
	break;
	case SPRITED:
		if (is_sprite_enabled(dev_priv, 1, 1) || dev_priv->is_resuming)
			I915_WRITE(SPRITED_CB_REG, cb_ptr->val);
	break;
	default:
		DRM_ERROR("Invalid Sprite Number\n");
		return -EINVAL;
	}
	return 0;
}

/*
* intel_attach_pipe_color_correction:
* register color correction properties as DRM CRTC properties
* for a particular device
* input:
* - intel_crtc : CRTC to attach color correcection with
*/
void
intel_attach_pipe_color_correction(struct intel_crtc *intel_crtc)
{
	struct clrmgr_reg_request *features;

	/* Color manager initialized? */
	if (!intel_crtc->color_status) {
		DRM_ERROR("Color manager not initialized for PIPE %d\n",
			intel_crtc->pipe);
		return;
	}

	features = kzalloc(sizeof(struct clrmgr_reg_request), GFP_KERNEL);
	if (!features) {
		DRM_ERROR("kzalloc failed: pipe color features\n");
		return;
	}

	features->no_of_properties = ARRAY_SIZE(gen6_pipe_color_corrections);
	memcpy(features->cp, gen6_pipe_color_corrections,
			features->no_of_properties
				* sizeof(struct clrmgr_property));

	/* Register pipe level color properties */
	if (!intel_clrmgr_register_pipe_property(intel_crtc, features))
		DRM_ERROR("Register pipe color property failed\n");
	else
		DRM_DEBUG_DRIVER("Attached colot corrections for pipe %d\n",
		intel_crtc->pipe);
	kfree(features);
}


/*
* intel_clrmgr_init:
* allocate memory to save color correction status
* input: struct drm_device
*/
struct clrmgr_status *intel_clrmgr_init(struct drm_device *dev)
{
	struct clrmgr_status *status;

	/* Sanity */
	if (!IS_VALLEYVIEW(dev)) {
		DRM_ERROR("Color manager is supported for VLV for now\n");
		return NULL;
	}

	/* Allocate and attach color status tracker */
	status = kzalloc(sizeof(struct clrmgr_status), GFP_KERNEL);
	if (!status) {
		DRM_ERROR("Out of memory, cant init color manager\n");
		return NULL;
	}
	DRM_DEBUG_DRIVER("\n");
	return status;
}

/*
* intel_clrmgr_exit
* Free allocated memory for color status
* Should be called from CRTC/Plane .destroy function
* input: color status
*/
void intel_clrmgr_exit(struct drm_device *dev, struct clrmgr_status *status)
{
	/* First free the DRM property, then status */
	if (status) {
		intel_clrmgr_deregister(dev, status);
		kfree(status);
	}
}

/* Tune Hue Saturation Value for Sprite */
int intel_sprite_hs_adjust(struct drm_i915_private *dev_priv,
		struct hue_saturationlut *hs_ptr)
{
	if (!dev_priv || !hs_ptr) {
		DRM_ERROR("Hue Saturation: Invalid Arguments\n");
		return -EINVAL;
	}

	switch (hs_ptr->sprite_no) {
	/* Sprite plane */
	case SPRITEA:
		if (is_sprite_enabled(dev_priv, 0, 0) || dev_priv->is_resuming)
			I915_WRITE(SPRITEA_HS_REG, hs_ptr->val);
	break;
	case SPRITEB:
		if (is_sprite_enabled(dev_priv, 0, 1) || dev_priv->is_resuming)
			I915_WRITE(SPRITEB_HS_REG, hs_ptr->val);
	break;
	case SPRITEC:
		if (is_sprite_enabled(dev_priv, 1, 0) || dev_priv->is_resuming)
			I915_WRITE(SPRITEC_HS_REG, hs_ptr->val);
	break;
	case SPRITED:
		if (is_sprite_enabled(dev_priv, 1, 1) || dev_priv->is_resuming)
			I915_WRITE(SPRITED_HS_REG, hs_ptr->val);
	break;
	default:
		DRM_ERROR("Invalid Sprite Number\n");
		return -EINVAL;
	}
	return 0;
}

static bool intel_restore_cb(struct drm_device *dev)
{
	int count = 0;
	struct drm_i915_private *dev_priv = dev->dev_private;

	while (count < NO_SPRITE_REG) {
		if (intel_sprite_cb_adjust(dev_priv, &savedcbvalues[count++])) {
			DRM_ERROR("Color Restore: Error restoring CB\n");
			return false;
		}
	}

	return true;
}

static bool intel_restore_hs(struct drm_device *dev)
{
	int count = 0;
	struct drm_i915_private *dev_priv = dev->dev_private;
	while (count < NO_SPRITE_REG) {
		if (intel_sprite_hs_adjust(dev_priv, &savedhsvalues[count++])) {
			DRM_ERROR("Color Restore: Error restoring HS\n");
			return false;
		}
	}

	return true;
}

bool intel_restore_clr_mgr_status(struct drm_device *dev)
{
	struct drm_crtc *crtc = NULL;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int pipe = 0;

	/* Validate input */
	if (!dev_priv) {
		DRM_ERROR("Color Restore: Invalid input\n");
		return false;
	}

	/* Search for a CRTC */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		pipe = to_intel_crtc(crtc)->pipe;

		/* If gamma enabled, restore gamma */
		if (dev_priv->gamma_enabled[pipe]) {
			if (intel_crtc_enable_gamma(crtc,
						pipe ? PIPEB : PIPEA)) {
				DRM_ERROR("Color Restore: gamma failed\n");
				return false;
			}
		}

		/* If csc enabled, restore csc */
		if (dev_priv->csc_enabled[pipe]) {
			if (do_intel_enable_csc(dev,
					(void *) csc_softlut[pipe], crtc)) {
				DRM_ERROR("Color Restore: CSC failed\n");
				return false;
			}
			return false;
		}
	}

	if (!intel_restore_hs(dev)) {
		DRM_ERROR("Color Restore: Restore hue/sat failed\n");
		return false;
	}

	if (!intel_restore_cb(dev)) {
		DRM_ERROR("Color Restore: Restore CB failed\n");
		return false;
	}

	DRM_DEBUG("Color Restore: Restore success\n");
	return true;
}
EXPORT_SYMBOL(intel_restore_clr_mgr_status);

void intel_save_cb_status(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	savedcbvalues[0].val = I915_READ(SPRITEA_CB_REG);
	savedcbvalues[1].val = I915_READ(SPRITEB_CB_REG);
	savedcbvalues[2].val = I915_READ(SPRITEC_CB_REG);
	savedcbvalues[3].val = I915_READ(SPRITED_CB_REG);
}

void intel_save_hs_status(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	savedhsvalues[0].val = I915_READ(SPRITEA_HS_REG);
	savedhsvalues[1].val = I915_READ(SPRITEB_HS_REG);
	savedhsvalues[2].val = I915_READ(SPRITEC_HS_REG);
	savedhsvalues[3].val = I915_READ(SPRITED_HS_REG);
}

void intel_save_clr_mgr_status(struct drm_device *dev)
{
	intel_save_hs_status(dev);
	intel_save_cb_status(dev);
}
EXPORT_SYMBOL(intel_save_clr_mgr_status);
