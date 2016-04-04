/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#ifndef __MSM_FD__
#define __MSM_FD__

#include <uapi/media/msm_fd.h>
#include <linux/compat.h>

#ifdef CONFIG_COMPAT
/*
 * struct msm_fd_result32 - Compat structure contain detected faces result.
 * @frame_id: Frame id of requested result.
 * @face_cnt: Number of result faces, driver can modify this value (to smaller)
 * @face_data: Pointer to array of face data structures.
 *  Array size should not be smaller then face_cnt.
 */
struct msm_fd_result32 {
	__u32 frame_id;
	__u32 face_cnt;
	compat_uptr_t face_data;
};

/* MSM FD compat private ioctl ID */
#define VIDIOC_MSM_FD_GET_RESULT32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct msm_fd_result32)
#endif

#endif /* __MSM_FD__ */
