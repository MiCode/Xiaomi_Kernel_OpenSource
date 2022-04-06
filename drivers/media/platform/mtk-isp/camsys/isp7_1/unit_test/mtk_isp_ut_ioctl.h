/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Bibby Hsieh <bibby.hsieh@mediatek.com>
 */

#ifndef __MTK_ISP_UT_IOCTL_H__
#define __MTK_ISP_UT_IOCTL_H__

#include <linux/types.h>

#define ISP_UT_IOCTL_TYPE 'K'

struct cam_ioctl_set_testmdl {
	__u16 width;
	__u16 height;
	__u8 pattern;
	__u8 pixmode_lg2;
	__u8 mode;
	__u8 hwScenario;
	__u16 dev_mask;
};

#define ISP_UT_IOCTL_SET_TESTMDL \
	_IOW(ISP_UT_IOCTL_TYPE, 0, struct cam_ioctl_set_testmdl)

struct cam_ioctl_create_session {
	struct mtkcam_ipi_session_cookie cookie;
	uint32_t cq_buffer_size;
};

#define ISP_UT_IOCTL_CREATE_SESSION \
	_IOW(ISP_UT_IOCTL_TYPE, 1, struct cam_ioctl_create_session)

#define ISP_UT_IOCTL_DESTROY_SESSION \
	_IOW(ISP_UT_IOCTL_TYPE, 2, struct mtkcam_ipi_session_cookie)

struct cam_ioctl_config {
	struct mtkcam_ipi_session_cookie cookie;
	struct mtkcam_ipi_config_param config_param;
};

#define ISP_UT_IOCTL_CONFIG \
	_IOW(ISP_UT_IOCTL_TYPE, 3, struct cam_ioctl_config)

struct cam_ioctl_enque {
	struct mtkcam_ipi_session_cookie cookie;
	struct mtkcam_ipi_frame_param frame_param;

	uint32_t img_output_fd[CAM_MAX_IMAGE_OUTPUT * CAM_MAX_PLANENUM];
	uint32_t meta_output_fd[CAM_MAX_META_OUTPUT];
	uint32_t meta_input_fd[CAM_MAX_PIPE_USED];
};

#define ISP_UT_IOCTL_ENQUE \
	_IOW(ISP_UT_IOCTL_TYPE, 4, struct cam_ioctl_enque *)

#define ISP_UT_IOCTL_DEQUE \
	_IOW(ISP_UT_IOCTL_TYPE, 5, unsigned int)

struct cam_ioctl_dmabuf_param {
	uint32_t size;
	uint32_t ccd_fd;
	uint64_t iova;
	void     *kva;
};

#define ISP_UT_IOCTL_ALLOC_DMABUF \
	_IOW(ISP_UT_IOCTL_TYPE, 6, struct cam_ioctl_dmabuf_param)

#define ISP_UT_IOCTL_FREE_DMABUF \
	_IOW(ISP_UT_IOCTL_TYPE, 7, struct cam_ioctl_dmabuf_param)

#endif
