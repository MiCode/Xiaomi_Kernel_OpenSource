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

#ifndef INTEL_DRRS_H__
#define INTEL_DRRS_H__

#include <core/common/dsi/intel_dsi_drrs.h>
#include <core/vlv/vlv_dc_config.h>

#define DRRS_IDLENESS_INTERVAL_MS	1000
#define DRRS_MIN_VREFRESH_HZ		55

void intel_disable_idleness_drrs(struct intel_pipeline *pipeline);
void intel_restart_idleness_drrs(struct intel_pipeline *pipeline);
int intel_drrs_init(struct intel_pipeline *pipeline);

#endif /* INTEL_DRRS_H__ */
