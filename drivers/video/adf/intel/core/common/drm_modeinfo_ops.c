/*
 * Copyright (C) 2015, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Author:
 * Ramalingam C <ramalingam.c@intel.com>
 */

#include <linux/fb.h>
#include <linux/slab.h>

#include <core/common/drm_modeinfo_ops.h>

/**
 * drm_modeinfo_vrefresh - get the vrefresh of a mode
 * @mode: mode
 *
 * Returns:
 * @modes's vrefresh rate in Hz, rounded to the nearest integer. Calculates the
 * value first if it is not yet set.
 */
int drm_modeinfo_vrefresh(const struct drm_mode_modeinfo *mode)
{
	int refresh = 0;
	int vtotal = mode->vtotal;
	unsigned int calc_val;

	if (mode->vrefresh > 0)
		refresh = mode->vrefresh;
	else if (mode->htotal > 0 && mode->vtotal > 0) {

		/* work out vrefresh the value will be x1000 */
		calc_val = (mode->clock * 1000);
		calc_val /= mode->htotal;
		refresh = (calc_val + vtotal / 2) / vtotal;

		if (mode->flags & DRM_MODE_FLAG_INTERLACE)
			refresh *= 2;
		if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
			refresh /= 2;
		if (mode->vscan > 1)
			refresh /= mode->vscan;
	}
	return refresh;
}

/**
 * drm_modeinfo_equal_no_clocks - test modes for equality
 * @mode1: first mode
 * @mode2: second mode
 *
 * Check to see if @mode1 and @mode2 are equivalent, but
 * don't check the pixel clocks.
 *
 * RETURNS:
 * True if the modes are equal, false otherwise.
 */
bool drm_modeinfo_equal_no_clocks(const struct drm_mode_modeinfo *mode1,
					const struct drm_mode_modeinfo *mode2)
{
	if (!mode1 || !mode2) {
		pr_err("ADF: %s: Either of the input mode is NULL\n", __func__);
		return false;
	}

	if (mode1->hdisplay == mode2->hdisplay &&
		mode1->hsync_start == mode2->hsync_start &&
		mode1->hsync_end == mode2->hsync_end &&
		mode1->htotal == mode2->htotal &&
		mode1->hskew == mode2->hskew &&
		mode1->vdisplay == mode2->vdisplay &&
		mode1->vsync_start == mode2->vsync_start &&
		mode1->vsync_end == mode2->vsync_end &&
		mode1->vtotal == mode2->vtotal &&
		mode1->vscan == mode2->vscan &&
		mode1->flags == mode2->flags)
		return true;

	return false;
}

/**
 * drm_modeinfo_equal - test modes for equality
 * @mode1: first mode
 * @mode2: second mode
 *
 * Check to see if @mode1 and @mode2 are equivalent.
 *
 * RETURNS:
 * True if the modes are equal, false otherwise.
 */
bool drm_modeinfo_equal(const struct drm_mode_modeinfo *mode1,
					const struct drm_mode_modeinfo *mode2)
{
	if (!mode1 || !mode2) {
		pr_err("ADF: %s: Either of the input mode is NULL\n", __func__);
		return false;
	}

	/*
	 * do clock check convert to PICOS so fb modes get matched
	 * the same
	 */
	if (mode1->clock && mode2->clock) {
		if (KHZ2PICOS(mode1->clock) != KHZ2PICOS(mode2->clock))
			return false;
	} else if (mode1->clock != mode2->clock)
		return false;

	return drm_modeinfo_equal_no_clocks(mode1, mode2);
}

/**
 * drm_modeinfo_debug_printmodeline - print a mode to dmesg
 * @mode: mode to print
 *
 * Describe @mode using DRM_DEBUG.
 */
void drm_modeinfo_debug_printmodeline(struct drm_mode_modeinfo *mode)
{
	pr_debug("ADF: Modeline: \"%s\" %d %d %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x\n",
		mode->name, mode->vrefresh, mode->clock,
		mode->hdisplay, mode->hsync_start,
		mode->hsync_end, mode->htotal, mode->hskew,
		mode->vdisplay, mode->vsync_start,
		mode->vsync_end, mode->vtotal, mode->vscan,
		mode->type, mode->flags);
}

/**
 * drm_modeinfo_copy - copy the mode
 * @dst: mode to overwrite
 * @src: mode to copy
 *
 * Copy an existing mode into another mode
 */
void drm_modeinfo_copy(struct drm_mode_modeinfo *dst,
					const struct drm_mode_modeinfo *src)
{
	if (!dst || !src) {
		pr_err("ADF: %s: Either of the input mode is NULL\n", __func__);
		return;
	}
	*dst = *src;
}

/**
 * drm_modeinfo_create - create a new display mode
 *
 * Create a new drm_mode_modeinfo, give it an ID, and return it.
 *
 * RETURNS:
 * Pointer to new mode on success, NULL on error.
 */

struct drm_mode_modeinfo *drm_modeinfo_create(void)
{
	struct drm_mode_modeinfo *nmode;

	nmode = kzalloc(sizeof(struct drm_mode_modeinfo), GFP_KERNEL);
	if (!nmode) {
		pr_err("ADF: %s: Mem allocation failed\n", __func__);
		return NULL;
	}

	return nmode;
}

/**
 * drm_modeinfo_destroy - remove a mode
 * @mode: mode to remove
 *
 * Free @mode's unique identifier, then free it.
 */
inline void drm_modeinfo_destroy(struct drm_mode_modeinfo *mode)
{
	kfree(mode);
}

/**
 * drm_modeinfo_duplicate - allocate and duplicate an existing mode
 * @m: mode to duplicate
 *
 * Just allocate a new mode, copy the existing mode into it, and return
 * a pointer to it. Used to create new instances of established modes.
 */
struct drm_mode_modeinfo *
		drm_modeinfo_duplicate(const struct drm_mode_modeinfo *mode)
{
	struct drm_mode_modeinfo *nmode;

	if (!mode) {
		pr_err("ADF: %s: Source Pointer is NULL\n", __func__);
		return NULL;
	}

	nmode = drm_modeinfo_create();
	if (!nmode) {
		pr_err("ADF: %s: Mode creation failed\n", __func__);
		return NULL;
	}

	drm_modeinfo_copy(nmode, mode);

	return nmode;
}

