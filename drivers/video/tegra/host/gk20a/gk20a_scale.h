/*
 * gk20a clock scaling profile
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GK20A_SCALE_H
#define GK20A_SCALE_H

#include <linux/nvhost.h>
#include <linux/devfreq.h>

#include "nvhost_scale.h"

struct platform_device;
struct host1x_actmon;
struct clk;

/* Initialization and de-initialization for module */
void nvhost_gk20a_scale_init(struct platform_device *);
void nvhost_gk20a_scale_deinit(struct platform_device *);

/*
 * call when performing submit to notify scaling mechanism that the module is
 * in use
 */
void nvhost_gk20a_scale_notify_busy(struct platform_device *);
void nvhost_gk20a_scale_notify_idle(struct platform_device *);

void nvhost_gk20a_scale_hw_init(struct platform_device *);

void nvhost_gk20a_scale_callback(struct nvhost_device_profile *profile,
				 unsigned long freq);
#endif
