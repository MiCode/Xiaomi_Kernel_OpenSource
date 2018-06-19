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

/* AVI Infoframe params */
enum picture_aspect_ratio {
	PICTURE_ASPECT_RATIO_NONE,
	PICTURE_ASPECT_RATIO_4_3,
	PICTURE_ASPECT_RATIO_16_9,
	PICTURE_ASPECT_RATIO_64_27,
	PICTURE_ASPECT_RATIO_256_135,
	PICTURE_ASPECT_RATIO_RESERVED,
};

enum active_format_aspect_ratio {
	ACTIVE_ASPECT_RATIO_16_9_TOP = 2,
	ACTIVE_ASPECT_RATIO_14_9_TOP = 3,
	ACTIVE_ASPECT_RATIO_16_9_CENTER = 4,
	ACTIVE_ASPECT_RATIO_PICTURE = 8,
	ACTIVE_ASPECT_RATIO_4_3 = 9,
	ACTIVE_ASPECT_RATIO_16_9 = 10,
	ACTIVE_ASPECT_RATIO_14_9 = 11,
	ACTIVE_ASPECT_RATIO_4_3_SP_14_9 = 13,
	ACTIVE_ASPECT_RATIO_16_9_SP_14_9 = 14,
	ACTIVE_ASPECT_RATIO_16_9_SP_4_3 = 15,
};

struct avi_infoframe_params {
	enum picture_aspect_ratio picture_aspect;
	enum active_format_aspect_ratio active_aspect;
	unsigned char video_code;
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
/* ADV7481 private ioctl for AVI Infoframe query */
#define VIDIOC_G_AVI_INFOFRAME \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 41, struct msm_ba_v4l2_ioctl_t)

#endif
