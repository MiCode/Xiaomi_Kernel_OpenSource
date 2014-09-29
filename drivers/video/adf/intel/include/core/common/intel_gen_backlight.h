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

#ifndef _INTEL_GEN_BACKLIGHT_H_
#define _INTEL_GEN_BACKLIGHT_H_

#include <core/intel_dc_config.h>

extern int intel_backlight_init(struct intel_pipe *pipe);
extern void intel_disable_backlight(struct intel_pipe *pipe);
extern void intel_enable_backlight(struct intel_pipe *pipe);

#endif /* _INTEL_GEN_BACKLIGHT_H_ */
