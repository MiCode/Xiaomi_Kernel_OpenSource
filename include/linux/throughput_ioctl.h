/*
 * include/linux/throughput_ioctl.h
 *
 * ioctl declarations for throughput miscdev
 *
 * Copyright (c) 2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __TEGRA_THROUGHPUT_IOCTL_H
#define __TEGRA_THROUGHPUT_IOCTL_H

#include <linux/ioctl.h>

#define TEGRA_THROUGHPUT_MAGIC 'g'

struct tegra_throughput_target_fps_args {
	__u32 target_fps;
};

#define TEGRA_THROUGHPUT_IOCTL_TARGET_FPS \
	_IOW(TEGRA_THROUGHPUT_MAGIC, 1, struct tegra_throughput_target_fps_args)
#define TEGRA_THROUGHPUT_IOCTL_MAXNR \
	(_IOC_NR(TEGRA_THROUGHPUT_IOCTL_TARGET_FPS))

#endif /* !defined(__TEGRA_THROUGHPUT_IOCTL_H) */

