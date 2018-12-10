/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI__MSM_ROTATOR_H__
#define _UAPI__MSM_ROTATOR_H__

#include <linux/types.h>
#include <linux/msm_mdp.h>

#define MSM_ROTATOR_IOCTL_MAGIC 'R'

#define MSM_ROTATOR_IOCTL_START   \
		_IOWR(MSM_ROTATOR_IOCTL_MAGIC, 1, struct msm_rotator_img_info)
#define MSM_ROTATOR_IOCTL_ROTATE   \
		_IOW(MSM_ROTATOR_IOCTL_MAGIC, 2, struct msm_rotator_data_info)
#define MSM_ROTATOR_IOCTL_FINISH   \
		_IOW(MSM_ROTATOR_IOCTL_MAGIC, 3, int)

#define ROTATOR_VERSION_01	0xA5B4C301

enum rotator_clk_type {
	ROTATOR_CORE_CLK,
	ROTATOR_PCLK,
	ROTATOR_IMEM_CLK
};

struct msm_rotator_img_info {
	unsigned int session_id;
	struct msmfb_img  src;
	struct msmfb_img  dst;
	struct mdp_rect src_rect;
	unsigned int    dst_x;
	unsigned int    dst_y;
	unsigned char   rotations;
	int enable;
	unsigned int	downscale_ratio;
	unsigned int secure;
};

struct msm_rotator_data_info {
	int session_id;
	struct msmfb_data src;
	struct msmfb_data dst;
	unsigned int version_key;
	struct msmfb_data src_chroma;
	struct msmfb_data dst_chroma;
};

struct msm_rot_clocks {
	const char *clk_name;
	enum rotator_clk_type clk_type;
	unsigned int clk_rate;
};

struct msm_rotator_platform_data {
	unsigned int number_of_clocks;
	unsigned int hardware_version_number;
	struct msm_rot_clocks *rotator_clks;
	struct msm_bus_scale_pdata *bus_scale_table;
	char rot_iommu_split_domain;
};
#endif

