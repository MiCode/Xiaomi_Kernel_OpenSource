/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_RAW_PIPELINE_H
#define __MTK_CAM_RAW_PIPELINE_H

#include <media/v4l2-subdev.h>
#include "mtk_cam-video.h"
#include "mtk_cam-tg-flash.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_cam-raw_nodes.h"


#define MTK_CAMSYS_RES_STEP_NUM	8 //TODO: remove


/* FIXME: dynamic config image max/min w/h */
#define IMG_MAX_WIDTH		12000
#define IMG_MAX_HEIGHT		9000
#define IMG_MIN_WIDTH		80
#define IMG_MIN_HEIGHT		60
#define YUV_GROUP1_MAX_WIDTH	8160
#define YUV_GROUP1_MAX_HEIGHT	3896
#define YUV_GROUP2_MAX_WIDTH	3060
#define YUV_GROUP2_MAX_HEIGHT	1145
#define YUV1_MAX_WIDTH		8160
#define YUV1_MAX_HEIGHT		2290
#define YUV2_MAX_WIDTH		3060
#define YUV2_MAX_HEIGHT		1145
#define YUV3_MAX_WIDTH		7794
#define YUV3_MAX_HEIGHT		3896
#define YUV4_MAX_WIDTH		1530
#define YUV4_MAX_HEIGHT		572
#define YUV5_MAX_WIDTH		1530
#define YUV5_MAX_HEIGHT		572
#define DRZS4NO1_MAX_WIDTH	2400
#define DRZS4NO1_MAX_HEIGHT	1080
#define DRZS4NO2_MAX_WIDTH	2400
#define DRZS4NO2_MAX_HEIGHT	1080
#define DRZS4NO3_MAX_WIDTH	576
#define DRZS4NO3_MAX_HEIGHT	432
#define RZH1N2TO1_MAX_WIDTH	1280
#define RZH1N2TO1_MAX_HEIGHT	600
#define RZH1N2TO2_MAX_WIDTH	512
#define RZH1N2TO2_MAX_HEIGHT	512
#define RZH1N2TO3_MAX_WIDTH	1280
#define RZH1N2TO3_MAX_HEIGHT	600

#define IMG_PIX_ALIGN		2


#define PREISP
/* feature mask to categorize all raw functions */
#define MTK_CAM_FEATURE_HDR_MASK		0x0000000F
#define MTK_CAM_FEATURE_SUBSAMPLE_MASK		0x000000F0
#define MTK_CAM_FEATURE_OFFLINE_M2M_MASK	0x00000100
#define MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK	0x00000200
#define MTK_CAM_FEATURE_TIMESHARE_MASK		0x00001000
#define MTK_CAM_FEATURE_HDR_MEMORY_SAVING_MASK		0x00002000
#define MTK_CAM_FEATURE_EXT_ISP_MASK	0x0000C000
/* flags common to features */
#define MTK_CAM_FEATURE_SEAMLESS_SWITCH_MASK	BIT(31)


enum raw_function_id {
	/* hdr */
	STAGGER_2_EXPOSURE_LE_SE	= (1 << 0),
	STAGGER_2_EXPOSURE_SE_LE	= (2 << 0),
	STAGGER_3_EXPOSURE_LE_NE_SE	= (3 << 0),
	STAGGER_3_EXPOSURE_SE_NE_LE	= (4 << 0),
	MSTREAM_NE_SE			= (5 << 0),
	MSTREAM_SE_NE			= (6 << 0),
	/* high fps (subsample) */
	HIGHFPS_2_SUBSAMPLE		= (1 << 4),
	HIGHFPS_4_SUBSAMPLE		= (2 << 4),
	HIGHFPS_8_SUBSAMPLE		= (3 << 4),
	HIGHFPS_16_SUBSAMPLE		= (4 << 4),
	HIGHFPS_32_SUBSAMPLE		= (5 << 4),
	OFFLINE_M2M			= (1 << 8),
	PURE_OFFLINE_M2M		= (1 << 9),
	TIMESHARE_1_GROUP		= (1 << 12),
	HDR_MEMORY_SAVING		= (1 << 13),
	EXT_ISP_CUS_1			= (1 << 14),
	EXT_ISP_CUS_2			= (2 << 14),
	EXT_ISP_CUS_3			= (3 << 14),
	WITH_W_CHANNEL			= (1 << 16),
	RAW_FUNCTION_END		= 0xF0000000,
};

enum hdr_scenario_id {
	STAGGER_ON_THE_FLY	= (1 << 0),
	STAGGER_OFFLINE		= (1 << 1),
	STAGGER_DCIF		= (1 << 2),
	STAGGER_M2M		= (1 << 3),
	MSTREAM			= (1 << 4),
	MSTREAM_M2M		= (1 << 5),
};

enum hardware_mode_id {
	HW_MODE_DEFAULT			= 0,
	HW_MODE_ON_THE_FLY		= 1,
	HW_MODE_DIRECT_COUPLED	= 2,
	HW_MODE_OFFLINE			= 3,
	HW_MODE_M2M				= 4,
};

/* max(pdi_table1, pdi_table2, ...) */
#define RAW_STATS_CFG_VARIOUS_SIZE ALIGN(0x7500, SZ_1K)

#define MTK_RAW_TOTAL_NODES (MTK_RAW_PIPELINE_PADS_NUM - MTK_RAW_SINK_NUM)

struct mtk_raw_pde_config {
	struct mtk_cam_pde_info pde_info;
};

struct mtk_cam_resource_config {
	struct v4l2_subdev *seninf;
	struct mutex resource_lock;
	struct v4l2_fract interval;
	s64 pixel_rate;
	u32 bin_limit;
	u32 frz_limit;
	u32 hwn_limit_max;
	u32 hwn_limit_min;
	s64 hblank;
	s64 vblank;
	s64 sensor_pixel_rate;
	u32 res_plan;
	u32 raw_feature;
	u32 res_strategy[MTK_CAMSYS_RES_STEP_NUM];
	u32 clk_target;
	u32 raw_num_used;
	u32 bin_enable;
	u32 frz_enable;
	u32 frz_ratio;
	u32 tgo_pxl_mode;
	u32 tgo_pxl_mode_before_raw;
	u32 raw_path;
	/* sink fmt adjusted according resource used*/
	struct v4l2_mbus_framefmt sink_fmt;
	u32 enable_hsf_raw;
	u32 hw_mode;
};

/* exposure for m-stream */
struct mtk_cam_shutter_gain {
	__u32 shutter;
	__u32 gain;
};

/* multiple exposure for m-stream(2 exposures) */
struct mtk_cam_mstream_exposure {
	struct mtk_cam_shutter_gain exposure[2];
	unsigned int valid;
	int req_id;
};

struct mtk_raw_pad_config {
	struct v4l2_mbus_framefmt mbus_fmt;
	struct v4l2_rect crop;
};

struct mtk_raw_stagger_select {
	int hw_mode;
	int enabled_raw;
};

/*
 * struct mtk_raw_pipeline - sub dev to use raws.
 *
 * @feature_pending: keep the user value of S_CTRL V4L2_CID_MTK_CAM_FEATURE.
 *		     It it safe save to be used in mtk_cam_vidioc_s_fmt,
 *		     mtk_cam_vb2_queue_setup and mtk_cam_vb2_buf_queue
 *		     But considering that we can't when the user calls S_CTRL,
 *		     please use mtk_cam_request_stream_data's
 *		     feature.raw_feature field
 *		     to avoid the CTRL value change tming issue.
 * @feature_active: The active feature during streaming. It can't be changed
 *		    during streaming and can only be used after streaming on.
 *
 */
struct mtk_raw_pipeline {
	unsigned int id;
	struct v4l2_subdev subdev;
	struct media_pad pads[MTK_RAW_PIPELINE_PADS_NUM];
	struct mtk_cam_video_device vdev_nodes[MTK_RAW_TOTAL_NODES];
	struct mtk_raw_pad_config pad_cfg[MTK_RAW_PIPELINE_PADS_NUM];
	struct v4l2_ctrl_handler ctrl_handler;

	/* v4l2 ctrl related data */
	s64 feature_pending;
	s64 feature_active;
	int dynamic_exposure_num_max;
	bool enqueued_tg_flash_req; /* need a better way to collect the request */
	struct mtk_cam_tg_flash_config tg_flash_config;
	/* TODO: merge or integrate with mtk_cam_resource_config */
	struct mtk_cam_resource user_res;
	struct mtk_cam_resource_config res_config;
	int sensor_mode_update;
	s64 sync_id;
	/* mstream */
	struct mtk_cam_mstream_exposure mstream_exposure;
	/* pde module */
	struct mtk_raw_pde_config pde_config;
	s64 hw_mode;
	s64 hw_mode_pending;
};

static inline struct mtk_raw_pipeline*
mtk_cam_ctrl_handler_to_raw_pipeline(struct v4l2_ctrl_handler *handler)
{
	return container_of(handler, struct mtk_raw_pipeline, ctrl_handler);
};

struct mtk_raw_pipeline *mtk_raw_pipeline_create(struct device *dev, int n);

struct mtk_cam_engines;
int mtk_raw_setup_dependencies(struct mtk_cam_engines *eng);

int mtk_raw_register_entities(struct mtk_raw_pipeline *arr_pipe, int num,
			      struct v4l2_device *v4l2_dev);
void mtk_raw_unregister_entities(struct mtk_raw_pipeline *arr_pipe, int num);

struct mtk_cam_ctx;
int mtk_cam_raw_select(struct mtk_cam_ctx *ctx,
		       struct mtkcam_ipi_input_param *cfg_in_param);

int mtk_cam_get_subsample_ratio(int raw_feature);

int mtk_raw_call_pending_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_format *fmt);

struct v4l2_subdev *mtk_cam_find_sensor(struct mtk_cam_ctx *ctx,
					struct media_entity *entity);

void mtk_cam_update_sensor(struct mtk_cam_ctx *ctx,
			   struct v4l2_subdev *sensor);

bool
mtk_raw_sel_get_res(struct v4l2_subdev *sd, struct v4l2_subdev_selection *sel,
		    struct mtk_cam_resource *res);
bool
mtk_raw_fmt_get_res(struct v4l2_subdev *sd, struct v4l2_subdev_format *fmt,
		    struct mtk_cam_resource *res);

unsigned int mtk_raw_get_hdr_scen_id(struct mtk_cam_ctx *ctx);

struct mtk_raw_pipeline*
mtk_cam_get_link_enabled_raw(struct v4l2_subdev *seninf);

struct v4l2_mbus_framefmt*
mtk_raw_pipeline_get_fmt(struct mtk_raw_pipeline *pipe,
			 struct v4l2_subdev_state *state,
			 int padid, int which);
struct v4l2_rect*
mtk_raw_pipeline_get_selection(struct mtk_raw_pipeline *pipe,
			       struct v4l2_subdev_state *state,
			       int pad, int which);
int
mtk_cam_raw_try_res_ctrl(struct mtk_raw_pipeline *pipeline,
			 struct mtk_cam_resource *res_user,
			 struct mtk_cam_resource_config *res_cfg,
			 struct v4l2_mbus_framefmt *sink_fmt,
			 char *dbg_str, bool log);
int
mtk_cam_res_copy_fmt_from_user(struct mtk_raw_pipeline *pipeline,
			       struct mtk_cam_resource *res_user,
			       struct v4l2_mbus_framefmt *dest);

int
mtk_cam_res_copy_fmt_to_user(struct mtk_raw_pipeline *pipeline,
			     struct mtk_cam_resource *res_user,
			     struct v4l2_mbus_framefmt *src);

bool mtk_raw_resource_calc(struct mtk_cam_device *cam,
			   struct mtk_cam_resource_config *res,
			   s64 pixel_rate, int res_plan,
			   int in_w, int in_h, int *out_w, int *out_h);

#endif /*__MTK_CAM_RAW_PIPELINE_H*/
