/*
 * drivers/video/tegra/host/gr3d/pod_scaling.h
 *
 * Tegra Graphics Host Power-On-Demand Scaling
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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

#ifndef POD_SCALING_H
#define POD_SCALING_H

struct platform_device;
struct dentry;

#define GET_TARGET_FREQ_DONTSCALE	1

/* Suspend is called when powering down module */
void nvhost_scale3d_suspend(struct platform_device *);

extern const struct devfreq_governor nvhost_podgov;

#endif
