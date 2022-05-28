/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_JOB_UTILS_H
#define __MTK_CAM_JOB_UTILS_H

#include "mtk_cam-ipi_7_1.h"

struct mtk_cam_job;
struct mtk_cam_buffer;
struct mtk_cam_video_device;

/*For state analysis and controlling for request*/
enum MTK_CAMSYS_STATE_IDX {
	E_STATE_READY = 0x0,
	E_STATE_SENINF,
	E_STATE_SENSOR,
	E_STATE_CQ,
	E_STATE_OUTER,
	E_STATE_CAMMUX_OUTER_CFG,
	E_STATE_CAMMUX_OUTER,
	E_STATE_INNER,
	E_STATE_DONE_NORMAL,
	E_STATE_CQ_SCQ_DELAY,
	E_STATE_CAMMUX_OUTER_CFG_DELAY,
	E_STATE_OUTER_HW_DELAY,
	E_STATE_INNER_HW_DELAY,
	E_STATE_DONE_MISMATCH,
	E_STATE_SUBSPL_READY = 0x10,
	E_STATE_SUBSPL_SCQ,
	E_STATE_SUBSPL_OUTER,
	E_STATE_SUBSPL_SENSOR,
	E_STATE_SUBSPL_INNER,
	E_STATE_SUBSPL_DONE_NORMAL,
	E_STATE_SUBSPL_SCQ_DELAY,
	E_STATE_TS_READY = 0x20,
	E_STATE_TS_SENSOR,
	E_STATE_TS_SV,
	E_STATE_TS_MEM,
	E_STATE_TS_CQ,
	E_STATE_TS_INNER,
	E_STATE_TS_DONE_NORMAL,
	E_STATE_EXTISP_READY = 0x30,
	E_STATE_EXTISP_SENSOR,
	E_STATE_EXTISP_SV_OUTER,
	E_STATE_EXTISP_SV_INNER,
	E_STATE_EXTISP_CQ,
	E_STATE_EXTISP_OUTER,
	E_STATE_EXTISP_INNER,
	E_STATE_EXTISP_DONE_NORMAL,
};

struct req_buffer_helper {
	struct mtk_cam_job *job;
	struct mtkcam_ipi_frame_param *fp;

	int ii_idx; /* image in */
	int io_idx; /* imgae out */
	int mi_idx; /* meta in */
	int mo_idx; /* meta out */

	/* for stagger case */
	bool filled_hdr_buffer;
};
struct pack_job_ops_helper {
	/* pack job ops */
	int (*pack_job)(struct mtk_cam_job *job,
		struct pack_job_ops_helper *job_helper);
	int (*update_imgo_to_ipi)(struct req_buffer_helper *helper,
			struct mtk_cam_buffer *buf, struct mtk_cam_video_device *node);
	int (*update_yuvo_to_ipi)(struct req_buffer_helper *helper,
			struct mtk_cam_buffer *buf, struct mtk_cam_video_device *node);
	int (*pack_job_check_ipi_buffer)(struct req_buffer_helper *helper);
};

void _state_trans(struct mtk_cam_job *job,
	enum MTK_CAMSYS_STATE_IDX from, enum MTK_CAMSYS_STATE_IDX to);
void _set_timestamp(struct mtk_cam_job *job,
	u64 time_boot, u64 time_mono);
int get_apply_sensor_margin_ms(struct mtk_cam_job *job);
int get_subsample_ratio(int feature);
int get_raw_subdev_idx(int used_pipe);
unsigned int _get_master_raw_id(unsigned int num_raw,
	unsigned int enabled_raw);
int fill_img_out(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node);
int fill_img_out_hdr(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node);
int fill_img_in_hdr(struct mtkcam_ipi_img_input *ii,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node);
int fill_imgo_out_subsample(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node,
			int sub_ratio);
int fill_yuvo_out_subsample(struct mtkcam_ipi_img_output *io,
			struct mtk_cam_buffer *buf,
			struct mtk_cam_video_device *node,
			int sub_ratio);
int update_work_buffer_to_ipi_frame(struct req_buffer_helper *helper);
struct mtkcam_ipi_crop v4l2_rect_to_ipi_crop(const struct v4l2_rect *r);



#endif //__MTK_CAM_JOB_UTILS_H

