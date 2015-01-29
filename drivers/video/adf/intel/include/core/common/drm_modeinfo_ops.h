/*
 * Copyright (C) 2015, Intel Corporation.
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
 * Author:
 * Ramalingam C <ramalingam.c@intel.com>
 */

#ifndef DRM_MODEINFO_OPS_H_
#define DRM_MODEINFO_OPS_H_
#include <drm/drm_mode.h>

int drm_modeinfo_vrefresh(const struct drm_mode_modeinfo *mode);
bool drm_modeinfo_equal_no_clocks(const struct drm_mode_modeinfo *mode1,
					const struct drm_mode_modeinfo *mode2);
bool drm_modeinfo_equal(const struct drm_mode_modeinfo *mode1,
				const struct drm_mode_modeinfo *mode2);
void drm_modeinfo_copy(struct drm_mode_modeinfo *dst,
					const struct drm_mode_modeinfo *src);
struct drm_mode_modeinfo *drm_modeinfo_create(void);
inline void drm_modeinfo_destroy(struct drm_mode_modeinfo *mode);
struct drm_mode_modeinfo *
		drm_modeinfo_duplicate(const struct drm_mode_modeinfo *mode);
void drm_modeinfo_debug_printmodeline(struct drm_mode_modeinfo *mode);

#endif		/* DRM_MODEINFO_OPS_H_ */
