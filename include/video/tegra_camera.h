/*
 * include/linux/tegra_camera.h
 *
 * Copyright (C) 2010 Google, Inc.
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
#ifndef TEGRA_CAMERA_H
#define TEGRA_CAMERA_H

/* this is to enable VI pattern generator (Null Sensor) */
#define TEGRA_CAMERA_ENABLE_PD2VI_CLK 0x1

enum {
	TEGRA_CAMERA_MODULE_ISP = 0,
	TEGRA_CAMERA_MODULE_VI,
	TEGRA_CAMERA_MODULE_CSI,
	TEGRA_CAMERA_MODULE_EMC,
	TEGRA_CAMERA_MODULE_MAX
};

enum {
	TEGRA_CAMERA_VI_CLK,
	TEGRA_CAMERA_VI_SENSOR_CLK,
	TEGRA_CAMERA_EMC_CLK
};

struct tegra_camera_clk_info {
	uint id;
	uint clk_id;
	unsigned long rate;
	uint flag;	/* to inform if any special bits need to enabled/disabled */
};

enum StereoCameraMode {
	Main = 0x0,		/* Sets the default camera to Main */
	StereoCameraMode_Left = 0x01,	/* the left camera is on. */
	StereoCameraMode_Right = 0x02,	/* the right camera is on. */
	StereoCameraMode_Stereo = 0x03,	/* both cameras are on. */
	StereoCameraMode_Force32 = 0x7FFFFFFF
};


#define TEGRA_CAMERA_IOCTL_ENABLE		_IOWR('i', 1, uint)
#define TEGRA_CAMERA_IOCTL_DISABLE		_IOWR('i', 2, uint)
#define TEGRA_CAMERA_IOCTL_CLK_SET_RATE		\
	_IOWR('i', 3, struct tegra_camera_clk_info)
#define TEGRA_CAMERA_IOCTL_RESET		_IOWR('i', 4, uint)

#endif
