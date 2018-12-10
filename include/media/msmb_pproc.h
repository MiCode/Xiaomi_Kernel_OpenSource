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
 */

#ifndef __MSMB_PPROC_H
#define __MSMB_PPROC_H

#include <uapi/media/msmb_pproc.h>

#include <linux/compat.h>

#define MSM_OUTPUT_BUF_CNT 8

#ifdef CONFIG_COMPAT
struct msm_cpp_frame_info32_t {
	int32_t frame_id;
	struct compat_timeval timestamp;
	uint32_t inst_id;
	uint32_t identity;
	uint32_t client_id;
	enum msm_cpp_frame_type frame_type;
	uint32_t num_strips;
	uint32_t msg_len;
	compat_uint_t cpp_cmd_msg;
	int src_fd;
	int dst_fd;
	struct compat_timeval in_time, out_time;
	compat_caddr_t cookie;
	compat_int_t status;
	int32_t duplicate_output;
	uint32_t duplicate_identity;
	uint32_t feature_mask;
	uint8_t we_disable;
	struct msm_cpp_buffer_info_t input_buffer_info;
	struct msm_cpp_buffer_info_t output_buffer_info[MSM_OUTPUT_BUF_CNT];
	struct msm_cpp_buffer_info_t duplicate_buffer_info;
	struct msm_cpp_buffer_info_t tnr_scratch_buffer_info[2];
	uint32_t reserved;
	uint8_t partial_frame_indicator;
	/* the following are used only for partial_frame type
	 * and is only used for offline frame processing and
	 * only if payload big enough and need to be split into partial_frame
	 * if first_payload, kernel acquires output buffer
	 * first payload must have the last stripe
	 * buffer addresses from 0 to last_stripe_index are updated.
	 * kernel updates payload with msg_len and stripe_info
	 * kernel sends top level, plane level, then only stripes
	 * starting with first_stripe_index and
	 * ends with last_stripe_index
	 * kernel then sends trailing flag at frame done,
	 * if last payload, kernel queues the output buffer to HAL
	 */
	uint8_t first_payload;
	uint8_t last_payload;
	uint32_t first_stripe_index;
	uint32_t last_stripe_index;
	uint32_t stripe_info_offset;
	uint32_t stripe_info;
	struct msm_cpp_batch_info_t  batch_info;
};

struct msm_cpp_clock_settings32_t {
	compat_long_t clock_rate;
	uint64_t avg;
	uint64_t inst;
};

struct msm_cpp_stream_buff_info32_t {
	uint32_t identity;
	uint32_t num_buffs;
	compat_caddr_t buffer_info;
};

struct msm_pproc_queue_buf_info32_t {
	struct msm_buf_mngr_info32_t buff_mgr_info;
	uint8_t is_buf_dirty;
};

struct cpp_hw_info_32_t {
	uint32_t cpp_hw_version;
	uint32_t cpp_hw_caps;
	compat_long_t freq_tbl[MAX_FREQ_TBL];
	uint32_t freq_tbl_count;
};


#define VIDIOC_MSM_CPP_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_GET_EVENTPAYLOAD32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_GET_INST_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_LOAD_FIRMWARE32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_GET_HW_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_FLUSH_QUEUE32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_ENQUEUE_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_DEQUEUE_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 7, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_CFG32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_TRANSACTION_SETUP32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_GET_EVENTPAYLOAD32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_GET_INST_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_ENQUEUE_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_VPE_DEQUEUE_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_QUEUE_BUF32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 14, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_APPEND_STREAM_BUFF_INFO32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_SET_CLOCK32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 16, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_POP_STREAM_BUFFER32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 17, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_IOMMU_ATTACH32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 18, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_IOMMU_DETACH32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 19, struct msm_camera_v4l2_ioctl32_t)

#define VIDIOC_MSM_CPP_DELETE_STREAM_BUFF32\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 20, struct msm_camera_v4l2_ioctl32_t)

struct msm_camera_v4l2_ioctl32_t {
	uint32_t id;
	uint32_t len;
	int32_t trans_code;
	compat_caddr_t ioctl_ptr;
};
#endif

#endif
