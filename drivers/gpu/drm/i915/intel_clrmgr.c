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
#include "intel_clrmgr.h"

/* Gamma lookup table for Sprite planes */
u32 gamma_sprite_softlut[GAMMA_SP_MAX_COUNT] = {
	0, 0, 0, 0, 0, 1023
};

/* Gamma soft lookup table for default gamma =1.0 */
u32 gamma_softlut[GAMMA_CORRECT_MAX_COUNT] =  {
	0x000000, 0x0, 0x020202, 0x0, 0x040404, 0x0, 0x060606, 0x0,
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
	0xF8F8F8, 0x0, 0xFAFAFA, 0x0, 0xFCFCFC, 0x0, 0xFEFEFE, 0x0
};



/* Color space conversion coff's */
u32 csc_softlut[CSC_MAX_COEFF_COUNT] = {
	1024,	 0, 67108864, 0, 0, 1024
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
	dev_priv->csc_enabled = 0;

	intel_crtc = to_intel_crtc(crtc);
	pipe = intel_crtc->pipe;
	pipeconf = I915_READ(PIPECONF(pipe));
	pipeconf &= ~(PIPECONF_CSC_ENABLE);

	/* Disable CSC on PIPE */
	I915_WRITE(PIPECONF(pipe), pipeconf);
	POSTING_READ(PIPECONF(pipe));
	return;
}

/* Parse userspace input coming from dev node*/
int parse_clrmgr_input(uint *dest, char *src, int max, int read)
{
	int size = 0;
	int bytes = 0;
	char *populate = NULL;

	/*Check for trailing comma or \n */
	if (!dest || !src || *src == ',' || *src == '\n' || !read) {
		DRM_ERROR("Invalid input to parse");
		return -EINVAL;
	}

	/* limit check */
	if (read < max) {
		DRM_ERROR("Invalid input to parse");
		return -EINVAL;
	}

	/* Extract values from buffer */
	while ((size < max) && (*src != '\n')) {
		populate = strsep(&src, ",");
		if (!populate)
			break;

		bytes += (strlen(populate)+1);
		if (kstrtouint((const char *)populate, 16,
			&dest[size++])) {
			DRM_ERROR("Parse: Invalid limit\n");
			return -EINVAL;
		}
		if (src == NULL || *src == '\0')
			break;
	}
	return read;
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

/*
* Gamma correction at Plane level */
int intel_enable_primary_gamma(struct drm_crtc *crtc)
{
	u32 odd = 0;
	u32 even = 0;
	u32 count = 0;
	u32 palreg = 0;
	u32 status = 0;
	struct intel_crtc *intel_crtc;
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;

	/* Validate input */
	if (!crtc) {
		DRM_ERROR("Invalid CRTC object input to gamma enable\n");
		return -EINVAL;
	}

	intel_crtc = to_intel_crtc(crtc);
	dev = crtc->dev;
	dev_priv = dev->dev_private;

	palreg = PALETTE(intel_crtc->pipe);
	 /* 10.6 mode Gamma Implementation */
	while (count < GAMMA_CORRECT_MAX_COUNT) {
		/* Get the gamma corrected value from table */
		odd = gamma_softlut[count];
		even = gamma_softlut[count + 1];

		/* Write even and odd parts in palette regs*/
		I915_WRITE(palreg + 4 * count, even);
		I915_WRITE(palreg + 4 * ++count, odd);
		count++;
	}

	/* Write max values in 11.6 format */
	I915_WRITE(PIPEA_GAMMA_MAX_BLUE, SHIFTBY6(GAMMA_MAX_VAL));
	I915_WRITE(PIPEA_GAMMA_MAX_GREEN, SHIFTBY6(GAMMA_MAX_VAL));
	I915_WRITE(PIPEA_GAMMA_MAX_RED, SHIFTBY6(GAMMA_MAX_VAL));

	/* Enable gamma on PIPE  */
	status = I915_READ(PIPECONF(intel_crtc->pipe));
	status |= PIPECONF_GAMMA;
	I915_WRITE(PIPECONF(intel_crtc->pipe), status);
	DRM_DEBUG("Gamma enabled on Plane A\n");

	return 0;
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
	struct intel_crtc *intel_crtc;
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;

	/* Validate input */
	if (!crtc) {
		DRM_ERROR("Invalid CRTC object input to gamma enable\n");
		return -EINVAL;
	}

	intel_crtc = to_intel_crtc(crtc);
	dev = crtc->dev;
	dev_priv = dev->dev_private;

	palreg = PALETTE(intel_crtc->pipe);
	 /* 10.6 mode Gamma Implementation */
	while (count < GAMMA_CORRECT_MAX_COUNT) {
		/* Get the gamma corrected value from table */
		odd = gamma_softlut[count];
		even = gamma_softlut[count + 1];

		/* Write even and odd parts in palette regs*/
		I915_WRITE(palreg + 4 * count, even);
		I915_WRITE(palreg + 4 * ++count, odd);
		count++;
	}

	/* Write max values in 11.6 format */
	I915_WRITE(PIPEA_GAMMA_MAX_BLUE, SHIFTBY6(GAMMA_MAX_VAL));
	I915_WRITE(PIPEA_GAMMA_MAX_GREEN, SHIFTBY6(GAMMA_MAX_VAL));
	I915_WRITE(PIPEA_GAMMA_MAX_RED, SHIFTBY6(GAMMA_MAX_VAL));

	/* Enable gamma for Plane A  */
	status = I915_READ(PIPECONF(intel_crtc->pipe));
	status |= PIPECONF_GAMMA;
	I915_WRITE(PIPECONF(intel_crtc->pipe), status);

	/* Enable gamma on Sprite plane A*/
	status = I915_READ(GAMMA_SPA_CNTRL);
	status |= GAMMA_ENABLE_SPR;
	I915_WRITE(GAMMA_SPA_CNTRL, status);

	/* Enable gamma on Sprite plane B*/
	status = I915_READ(GAMMA_SPB_CNTRL);
	status |= GAMMA_ENABLE_SPR;
	I915_WRITE(GAMMA_SPB_CNTRL, status);

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
		return intel_enable_pipe_gamma(crtc);
	case PIPEB:
		return intel_enable_external_pipe_gamma(crtc);

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

/* Disable gamma correction on Primary display */
int intel_disable_external_pipe_gamma(struct drm_crtc *crtc)
{
	DRM_ERROR("This functionality is not implemented yet\n");
	return -EINVAL;
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


/* Disable gamma correction on Primary display */
int intel_disable_pipe_gamma(struct drm_crtc *crtc)
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

	/* Disable gamma on SpriteA  */
	status = I915_READ(GAMMA_SPA_CNTRL);
	status &= ~(GAMMA_ENABLE_SPR);
	I915_WRITE(GAMMA_SPA_CNTRL, status);

	/* Disable gamma on SpriteB  */
	status = I915_READ(GAMMA_SPB_CNTRL);
	status &= ~(GAMMA_ENABLE_SPR);
	I915_WRITE(GAMMA_SPB_CNTRL, status);

	/* TODO: Reset gamma table default */
	DRM_DEBUG("Gamma disabled on Pipe\n");
	return 0;
}

/* Load gamma correction values corresponding to supplied
gamma and program palette accordingly */
int intel_crtc_disable_gamma(struct drm_crtc *crtc, u32 identifier)
{
	switch (identifier) {
	/* Whole pipe level correction */
	case PIPEA:
		return intel_disable_pipe_gamma(crtc);
	case PIPEB:
		return intel_disable_external_pipe_gamma(crtc);
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
