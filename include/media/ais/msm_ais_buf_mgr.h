/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MEDIA_MSM_AIS_BUF_MGR_H__
#define __MEDIA_MSM_AIS_BUF_MGR_H__

#include <uapi/media/ais/msm_ais_buf_mgr.h>
#include <linux/compat.h>

struct v4l2_subdev *msm_buf_mngr_get_subdev(void);

#ifdef CONFIG_COMPAT

struct msm_buf_mngr_info32_t {
	uint32_t session_id;
	uint32_t stream_id;
	uint32_t frame_id;
	struct compat_timeval timestamp;
	uint32_t index;
	uint32_t reserved;
	enum msm_camera_buf_mngr_buf_type type;
	struct msm_camera_user_buf_cont_t user_buf;
};

#define VIDIOC_MSM_BUF_MNGR_GET_BUF32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 33, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_PUT_BUF32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 34, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_BUF_DONE32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 35, struct msm_buf_mngr_info32_t)

#define VIDIOC_MSM_BUF_MNGR_FLUSH32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 39, struct msm_buf_mngr_info32_t)

#endif

#endif

