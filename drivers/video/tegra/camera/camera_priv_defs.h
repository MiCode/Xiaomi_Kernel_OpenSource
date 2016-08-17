/*
 * drivers/video/tegra/camera/camera_priv_defs.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_PRIV_DEFS_H
#define __DRIVERS_VIDEO_TEGRA_CAMERA_CAMERA_PRIV_DEFS_H

#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/slab.h>

#include <mach/powergate.h>
#include <mach/clk.h>
#include <mach/mc.h>
#include <mach/iomap.h>
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
#include <mach/isomgr.h>
#endif

#include <video/tegra_camera.h>


/*
 * CAMERA_*_CLK is only for internal driver use.
 * TEGRA_CAMERA_*_CLK is enum used between driver and user space.
 * TEGRA_CAMERA_*_CLK is defined in tegra_camera.h
 */
enum {
	CAMERA_VI_CLK,
	CAMERA_VI_SENSOR_CLK,
	CAMERA_EMC_CLK,
	CAMERA_ISP_CLK,
	CAMERA_CSUS_CLK,
	CAMERA_CSI_CLK,
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	CAMERA_CILAB_CLK,
	CAMERA_CILCD_CLK,
	CAMERA_CILE_CLK,
	CAMERA_PLL_D2_CLK,
#endif
	CAMERA_SCLK,
	CAMERA_CLK_MAX,
};

struct clock {
	struct clk *clk;
	bool on;
};

struct tegra_camera {
	struct device *dev;
	struct miscdevice misc_dev;
	struct clock clock[CAMERA_CLK_MAX];
	struct regulator *reg;
	struct tegra_camera_clk_info info;
	struct mutex tegra_camera_lock;
	atomic_t in_use;
	int power_on;
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
	tegra_isomgr_handle isomgr_handle;
#endif
};

/*
 * index: clock enum value
 * name:  clock name
 * init:  default clock state when camera is opened.
 * freq:  initial clock frequency to set when camera is opened. If it is 0,
 *        then no need to set clock freq.
 */
struct clock_data {
	int index;
	char *name;
	bool init;
	unsigned long freq;
};

struct tegra_camera *tegra_camera_register(struct platform_device *ndev);
int tegra_camera_unregister(struct tegra_camera *camera);
#ifdef CONFIG_PM
int tegra_camera_suspend(struct tegra_camera *camera);
int tegra_camera_resume(struct tegra_camera *camera);
#endif

#endif
