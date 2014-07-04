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
