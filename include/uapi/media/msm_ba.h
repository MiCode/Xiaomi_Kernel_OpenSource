/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __UAPI_MSM_BA_H__
#define __UAPI_MSM_BA_H__

#include <linux/videodev2.h>
#include <linux/types.h>

/* CSI control params */
struct csi_ctrl_params {
	uint32_t settle_count;
	uint32_t lane_count;
};

/* Field info params */
struct field_info_params {
	bool even_field;
	struct timeval field_ts;
};

/* private ioctl structure */
struct msm_ba_v4l2_ioctl_t {
	size_t len;
	void __user *ptr;
};

/* ADV7481 private ioctls for CSI control params */
#define VIDIOC_G_CSI_PARAMS \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct msm_ba_v4l2_ioctl_t)
/* ADV7481 private ioctls for field info query */
#define VIDIOC_G_FIELD_INFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 40, struct msm_ba_v4l2_ioctl_t)

#endif
