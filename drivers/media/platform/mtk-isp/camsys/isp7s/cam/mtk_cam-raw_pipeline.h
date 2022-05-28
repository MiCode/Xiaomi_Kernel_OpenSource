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
#include "mtk_cam-raw_pads.h"

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

struct mtk_cam_resource_driver {

	/* expose to userspace, v4l2_ctrl */
	struct mtk_cam_resource		user_data;

	/* driver internally cached */
	unsigned int clk_target; /* Hz */
};


struct mtk_raw_ctrl_data {
	s64 feature;

	struct mtk_cam_resource_driver resource;
	int raw_path;

	bool enqueued_tg_flash_req; /* need a better way to collect the request */
	struct mtk_cam_tg_flash_config tg_flash_config;

	bool sensor_mode_update;
	s64 sync_id;
	struct mtk_cam_mstream_exposure mstream_exp;
};

struct mtk_raw_sink_data {
	unsigned int width;
	unsigned int height;
	unsigned int mbus_code;
	struct v4l2_rect crop;
};

struct mtk_raw_request_data {
	struct mtk_raw_sink_data sink;
	struct mtk_raw_ctrl_data ctrl;
};

/*
 * struct mtk_raw_pipeline - sub dev to use raws.
 */
struct mtk_raw_pipeline {
	unsigned int id;
	struct v4l2_subdev subdev;
	struct mtk_cam_video_device vdev_nodes[MTK_RAW_TOTAL_NODES];
	struct media_pad pads[MTK_RAW_PIPELINE_PADS_NUM];
	struct mtk_raw_pad_config pad_cfg[MTK_RAW_PIPELINE_PADS_NUM];
	struct v4l2_ctrl_handler ctrl_handler;


	/*** v4l2 ctrl related data ***/
	/* changed with request */
	struct mtk_raw_ctrl_data ctrl_data;
	/* pde module */
	struct mtk_raw_pde_config pde_config;
};

static inline struct mtk_raw_pipeline *
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

/* TODO: move to cam */
struct v4l2_subdev *mtk_cam_find_sensor(struct mtk_cam_ctx *ctx,
					struct media_entity *entity);

/* helper function */
static inline struct mtk_cam_video_device *
mtk_raw_get_node(struct mtk_raw_pipeline *pipe, int pad)
{
	int idx;

	if (WARN_ON(pad < MTK_RAW_SINK_NUM))
		return NULL;

	idx = raw_pad_to_node_idx(pad);
	if (WARN_ON(idx < 0 || idx >= ARRAY_SIZE(pipe->vdev_nodes)))
		return NULL;

	return &pipe->vdev_nodes[idx];
}

#endif /*__MTK_CAM_RAW_PIPELINE_H*/
