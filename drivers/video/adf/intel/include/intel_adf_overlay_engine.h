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

#ifndef INTEL_ADF_OVERLAY_ENGINE_H_
#define INTEL_ADF_OVERLAY_ENGINE_H_

#include <video/adf.h>

#include "core/intel_dc_config.h"
#include "intel_adf_device.h"

struct intel_adf_overlay_engine {
	struct adf_overlay_engine base;
	struct intel_plane *plane;
	struct list_head active_list;
};

static inline struct intel_adf_overlay_engine *to_intel_eng(
	struct adf_overlay_engine *eng)
{
	return container_of(eng, struct intel_adf_overlay_engine, base);
}

extern int intel_adf_overlay_engine_init(
	struct intel_adf_overlay_engine *eng, struct intel_adf_device *dev,
	struct intel_plane *plane);
extern void intel_adf_overlay_engine_destroy(
	struct intel_adf_overlay_engine *eng);

#endif /* INTEL_ADF_OVERLAY_ENGINE_H_ */
