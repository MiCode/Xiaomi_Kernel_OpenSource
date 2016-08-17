/*
 * drivers/video/tegra/host/gr2d/gr2d_t114.h
 *
 * Tegra Graphics Host 2D Tegra11 specific parts
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

#ifndef __NVHOST_2D_T114_H
#define __NVHOST_2D_T114_H

struct platform_device;

void nvhost_gr2d_t114_finalize_poweron(struct platform_device *dev);

#endif
