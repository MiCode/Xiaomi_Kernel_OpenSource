/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_IPI_H__
#define __MTK_CAM_IPI_H__

#define MTK_CAM_IPI_VERSION_MAJOR 0
#define MTK_CAM_IPI_VERSION_MINOR 1

#include <linux/types.h>
#include "mtk_cam-defs.h"

#define MTK_CAM_MAX_RUNNING_JOBS (3)
#define CAM_MAX_PLANENUM (4)
#define CAM_MAX_SUBSAMPLE (32)

/*
 * struct mtkcam_ipi_point - Point
 *
 * @x: x-coordinate of the point (zero-based).
 * @y: y-coordinate of the point (zero-based).
 */
struct mtkcam_ipi_point {
	__u16 x;
	__u16 y;
} __packed;

/*
 * struct mtkcam_ipi_size - Size
 *
 * @w: width (in pixels).
 * @h: height (in pixels).
 */
struct mtkcam_ipi_size {
	__u16 w;
	__u16 h;
} __packed;

/*
 * struct mtkcam_ipi_buffer
 *	- Shared buffer between cam-device and co-processor.
 *
 * @iova: DMA address for CAM DMA device.
 * @ccd_fd: fd to ccd
 * @scp_addr: SCP  address for external co-processor unit.
 * @size: buffer size.
 */
struct mtkcam_ipi_buffer {
	__u64 iova;
	union {
		__u32 ccd_fd;
		__u32 scp_addr;
	};
	__u32 size;
} __packed;

struct mtkcam_ipi_pix_fmt {
	__u32			format;
	struct mtkcam_ipi_size	s;
	__u16			stride[CAM_MAX_PLANENUM];
} __packed;

struct mtkcam_ipi_crop {
	struct mtkcam_ipi_point p;
	struct mtkcam_ipi_size s;
} __packed;

struct mtkcam_ipi_uid {
	__u8 pipe_id;
	__u8 id;
} __packed;

struct mtkcam_ipi_img_input {
	struct mtkcam_ipi_uid		uid;
	struct mtkcam_ipi_pix_fmt	fmt;
	struct mtkcam_ipi_buffer	buf[CAM_MAX_PLANENUM];
} __packed;

struct mtkcam_ipi_img_output {
	struct mtkcam_ipi_uid		uid;
	struct mtkcam_ipi_pix_fmt	fmt;
	struct mtkcam_ipi_buffer	buf[CAM_MAX_SUBSAMPLE][CAM_MAX_PLANENUM];
	struct mtkcam_ipi_crop		crop;
} __packed;

struct mtkcam_ipi_meta_input {
	struct mtkcam_ipi_uid		uid;
	struct mtkcam_ipi_buffer	buf;
} __packed;

struct mtkcam_ipi_meta_output {
	struct mtkcam_ipi_uid		uid;
	struct mtkcam_ipi_buffer	buf;
} __packed;

struct mtkcam_ipi_input_param {
	__u32	fmt;
	__u8	raw_pixel_id;
	__u8	data_pattern;
	__u8	pixel_mode;
	__u8	pixel_mode_before_raw;
	__u8	subsample;
	/* u8 continuous; */ /* always 1 */
	/* u16 tg_fps; */ /* not used yet */
	struct mtkcam_ipi_crop in_crop;
} __packed;

struct mtkcam_ipi_sv_input_param {
	__u32	pipe_id;
	__u8	tag_id;
	__u8	tag_order;
	struct mtkcam_ipi_input_param input;
} __packed;

struct mtkcam_ipi_mraw_input_param {
	__u32	pipe_id;
	struct mtkcam_ipi_input_param input;
} __packed;

struct mtkcam_ipi_raw_frame_param {
	__u8	imgo_path_sel; /* mtkcam_ipi_raw_path_control */
	__u32	hardware_scenario;
	__u32	bin_flag;
	__u8    exposure_num;
	__u8    previous_exposure_num;

	/* blahblah */
} __packed;

struct mtkcam_ipi_adl_frame_param {
	__u8 vpu_i_point;
	__u8 vpu_o_point;
	__u8 sysram_en;
	__u32 block_y_size;
	__u64 slb_addr;
	__u32 slb_size;
} __packed;

/* TODO: support CAMSV */
struct cam_camsv_params {
	/* sth here */
} __packed;

#define MRAW_MAX_IMAGE_OUTPUT (3)
#define CAMSV_MAX_IMAGE_OUTPUT (1)

struct mtkcam_ipi_camsv_frame_param {
	__u32	pipe_id;
	__u8	tag_id;
	__u32	hardware_scenario;

	struct mtkcam_ipi_img_output camsv_img_outputs[CAMSV_MAX_IMAGE_OUTPUT];
} __packed;

struct mtkcam_ipi_mraw_frame_param {
	__u32 pipe_id;

	struct mtkcam_ipi_meta_input mraw_meta_inputs;
	struct mtkcam_ipi_img_output mraw_img_outputs[MRAW_MAX_IMAGE_OUTPUT];
} __packed;

struct mtkcam_ipi_session_cookie {
	__u8 session_id;
	__u32 frame_no;
} __packed;

struct mtkcam_ipi_session_param {
	struct mtkcam_ipi_buffer workbuf; /* TODO: rename cqbuf? */
	struct mtkcam_ipi_buffer msg_buf;
} __packed;

struct mtkcam_ipi_hw_mapping {
	__u8 pipe_id; /* ref. to mtkcam_pipe_subdev */
	__u16 dev_mask; /* ref. to mtkcam_pipe_dev */
} __packed;

struct mtkcam_ipi_bw_info {
	/* TBD */
	/* TODO: define ports in defs.h */
	__u32 xsize;
	__u32 ysize;
} __packed;

struct mtkcam_ipi_timeshared_msg {
	__u8	session_id;
	__u32	cur_msgbuf_offset;
	__u32	cur_msgbuf_size;
} __packed;

/*  Control flags of CAM_CMD_CONFIG */
#define MTK_CAM_IPI_CONFIG_TYPE_INIT			0x0001
#define MTK_CAM_IPI_CONFIG_TYPE_INPUT_CHANGE		0x0002
#define MTK_CAM_IPI_CONFIG_TYPE_EXEC_TWICE		0x0004
#define MTK_CAM_IPI_CONFIG_TYPE_SMVR_PREVIEW		0x0008

/* TODO: update number */
#define CAM_MAX_INPUT_PAD           (3)
#define CAM_MAX_W_PATH_INPUT_SIZE   (2)
#define CAM_MAX_IMAGE_INPUT  (CAM_MAX_INPUT_PAD + CAM_MAX_W_PATH_INPUT_SIZE)
#define CAM_MAX_OUTPUT_PAD          (15)
#define CAM_MAX_W_PATH_OUT_SIZE     (1)
#define CAM_MAX_IMAGE_OUTPUT (CAM_MAX_OUTPUT_PAD + CAM_MAX_W_PATH_OUT_SIZE)
#define CAM_MAX_META_OUTPUT	 (4)
#define CAM_MAX_PIPE_USED	 (4)
#define MRAW_MAX_PIPE_USED   (4)
#define CAMSV_MAX_PIPE_USED  (2)
#define CAMSV_MAX_TAGS       (8)

struct mtkcam_ipi_config_param {
	__u8 flags;
	struct mtkcam_ipi_input_param	input;
	struct mtkcam_ipi_sv_input_param sv_input[CAMSV_MAX_PIPE_USED][CAMSV_MAX_TAGS];
	struct mtkcam_ipi_mraw_input_param mraw_input[MRAW_MAX_PIPE_USED];
	__u8 n_maps; /* maximum # of subdevs per stream */
	struct mtkcam_ipi_hw_mapping maps[6];
	__u8	sw_feature;
	__u32	exp_order : 4;
	__u32	frame_order : 4;
	__u32	vsync_order : 4;
	struct mtkcam_ipi_buffer w_cac_table; /* for rgbw's empty cac table */
} __packed;

struct mtkcam_ipi_frame_param {
	__u32 cur_workbuf_offset;
	__u32 cur_workbuf_size;

	struct mtkcam_ipi_raw_frame_param raw_param;
	struct mtkcam_ipi_mraw_frame_param mraw_param[MRAW_MAX_PIPE_USED];
	struct mtkcam_ipi_camsv_frame_param camsv_param[CAMSV_MAX_PIPE_USED][CAMSV_MAX_TAGS];
	struct mtkcam_ipi_adl_frame_param adl_param;

	struct mtkcam_ipi_timeshared_msg	timeshared_param;

	struct mtkcam_ipi_img_input img_ins[CAM_MAX_IMAGE_INPUT];
	struct mtkcam_ipi_img_output img_outs[CAM_MAX_IMAGE_OUTPUT];
	struct mtkcam_ipi_meta_output meta_outputs[CAM_MAX_META_OUTPUT];
	struct mtkcam_ipi_meta_input meta_inputs[CAM_MAX_PIPE_USED];

	/* following will be modified */
	//struct mtkcam_ipi_bw_info	bw_infos[10*3]; //ports * num_raw
} __packed;

struct mtkcam_ipi_frame_info {
	__u32	cur_msgbuf_offset;
	__u32	cur_msgbuf_size;
} __packed;

struct mtkcam_ipi_cq_desc_entry {
	__u32 offset;
	__u32 size;
} __packed;

struct mtkcam_ipi_frame_ack_result {
	struct mtkcam_ipi_cq_desc_entry		main;
	struct mtkcam_ipi_cq_desc_entry		sub;
	struct mtkcam_ipi_cq_desc_entry		mraw[MRAW_MAX_PIPE_USED];
	struct mtkcam_ipi_cq_desc_entry		camsv[CAMSV_MAX_PIPE_USED];
} __packed;

struct mtkcam_ipi_ack_info {
	__u8 ack_cmd_id;
	__s32 ret;
	struct mtkcam_ipi_frame_ack_result frame_result;
} __packed;

/*
 * The IPI command enumeration.
 */
enum mtkcam_ipi_cmds {
	/* request for a new streaming: mtkcam_ipi_session_param */
	CAM_CMD_CREATE_SESSION,

	/* config the stream: mtkcam_ipi_config_param */
	CAM_CMD_CONFIG,

	/* per-frame: mtkcam_ipi_frame_param */
	CAM_CMD_FRAME,

	/* release certain streaming: mtkcam_ipi_session_param */
	CAM_CMD_DESTROY_SESSION,

	/* ack: mtkcam_ipi_ack_info */
	CAM_CMD_ACK,

	CAM_CMD_RESERVED,
};

struct mtkcam_ipi_event  {
	struct mtkcam_ipi_session_cookie cookie;
	__u8 cmd_id;
	union {
		struct mtkcam_ipi_session_param	session_data;
		struct mtkcam_ipi_config_param	config_data;
		struct mtkcam_ipi_frame_info	frame_data;
		struct mtkcam_ipi_ack_info	ack_data;
	};
} __packed;

#endif /* __MTK_CAM_IPI_H__ */
