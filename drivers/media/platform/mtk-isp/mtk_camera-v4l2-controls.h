/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAMERA_V4l2_CONTROLS_H
#define __MTK_CAMERA_V4l2_CONTROLS_H

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

/**
 * I D  B A S E
 */

/**
 * The base for the mediatek camsys driver controls
 * We reserve 48 controls for this driver.
 */
#define V4L2_CID_USER_MTK_CAM_BASE		(V4L2_CID_USER_BASE + 0x10d0)

/**
 * The base for the mediatek sensor driver controls
 * We reserve 48 controls for this driver.
 */
#define V4L2_CID_USER_MTK_SENSOR_BASE		(V4L2_CID_USER_MTK_CAM_BASE + 0x200)

/**
 * The base for the mediatek seninf driver controls
 * We reserve 48 controls for this driver.
 */
#define V4L2_CID_USER_MTK_SENINF_BASE		(V4L2_CID_USER_MTK_CAM_BASE + 0x300)


/* C A M S Y S */
#define V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT \
	(V4L2_CID_USER_MTK_CAM_BASE + 1)
#define V4L2_CID_MTK_CAM_BIN_LIMIT \
	(V4L2_CID_USER_MTK_CAM_BASE + 2)
#define V4L2_CID_MTK_CAM_FRZ_LIMIT \
	(V4L2_CID_USER_MTK_CAM_BASE + 3)
#define V4L2_CID_MTK_CAM_RESOURCE_PLAN_POLICY \
	(V4L2_CID_USER_MTK_CAM_BASE + 4)
#define V4L2_CID_MTK_CAM_USED_ENGINE \
	(V4L2_CID_USER_MTK_CAM_BASE + 5)
#define V4L2_CID_MTK_CAM_BIN \
	(V4L2_CID_USER_MTK_CAM_BASE + 6)
#define V4L2_CID_MTK_CAM_FRZ \
	(V4L2_CID_USER_MTK_CAM_BASE + 7)
#define V4L2_CID_MTK_CAM_USED_ENGINE_TRY \
	(V4L2_CID_USER_MTK_CAM_BASE + 8)
#define V4L2_CID_MTK_CAM_BIN_TRY \
	(V4L2_CID_USER_MTK_CAM_BASE + 9)
#define V4L2_CID_MTK_CAM_FRZ_TRY \
	(V4L2_CID_USER_MTK_CAM_BASE + 10)
#define V4L2_CID_MTK_CAM_PIXEL_RATE \
	(V4L2_CID_USER_MTK_CAM_BASE + 11)
#define V4L2_CID_MTK_CAM_FEATURE \
	(V4L2_CID_USER_MTK_CAM_BASE + 12)
#define V4L2_CID_MTK_CAM_SYNC_ID \
	(V4L2_CID_USER_MTK_CAM_BASE + 13)
#define V4L2_CID_MTK_CAM_RAW_PATH_SELECT\
	(V4L2_CID_USER_MTK_CAM_BASE + 14)
#define V4L2_CID_MTK_CAM_RAW_RESOURCE \
	(V4L2_CID_USER_MTK_CAM_BASE + 18)
/* Allowed value of V4L2_CID_MTK_CAM_RAW_PATH_SELECT */
#define V4L2_MTK_CAM_RAW_PATH_SELECT_LSC	1


/* I M G S Y S */


/* I M A G E  S E N S O R */
#define V4L2_CID_MTK_TEMPERATURE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 1)

#define V4L2_CID_MTK_ANTI_FLICKER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 2)

#define V4L2_CID_MTK_AWB_GAIN \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 3)

#define V4L2_CID_MTK_SHUTTER_GAIN_SYNC \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 4)

#define V4L2_CID_MTK_DUAL_GAIN \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 5)

#define V4L2_CID_MTK_IHDR_SHUTTER_GAIN \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 6)

#define V4L2_CID_MTK_HDR_SHUTTER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 7)

#define V4L2_CID_MTK_SHUTTER_FRAME_LENGTH \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 8)

#define V4L2_CID_MTK_PDFOCUS_AREA \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 9)

#define V4L2_CID_MTK_HDR_ATR \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 10)

#define V4L2_CID_MTK_HDR_TRI_SHUTTER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 11)

#define V4L2_CID_MTK_HDR_TRI_GAIN \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 12)

#define V4L2_CID_FRAME_SYNC \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 13)

#define V4L2_CID_MTK_MAX_FPS \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 14)

#define V4L2_CID_MTK_STAGGER_AE_CTRL \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 15)

#define V4L2_CID_SEAMLESS_SCENARIOS \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 16)

#define V4L2_CID_MTK_STAGGER_INFO \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 17)

#define V4L2_CID_STAGGER_TARGET_SCENARIO \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 18)

#define V4L2_CID_START_SEAMLESS_SWITCH \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 19)

#define V4L2_CID_MTK_DEBUG_CMD \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 20)

#define V4L2_CID_MAX_EXP_TIME \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 21)

#define V4L2_CID_MTK_SENSOR_PIXEL_RATE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 22)

#define V4L2_CID_MTK_FRAME_DESC \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 23)

#define V4L2_CID_MTK_SENSOR_STATIC_PARAM \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 24)

/* S E N I N F */
#define V4L2_CID_MTK_SENINF_S_STREAM \
		(V4L2_CID_USER_MTK_SENINF_BASE + 1)

#define V4L2_CID_FSYNC_MAP_ID \
	(V4L2_CID_USER_MTK_SENINF_BASE + 2)

#define V4L2_CID_VSYNC_NOTIFY \
	(V4L2_CID_USER_MTK_SENINF_BASE + 3)


#define MTK_CAM_RESOURCE_DEFAULT	0xFFFF

/*
 * struct mtk_cam_resource_sensor - sensor resoruces for format negotiation
 *
 */
struct mtk_cam_resource_sensor {
	struct v4l2_fract interval;
	__u32 hblank;
	__u32 vblank;
	__u64 pixel_rate;
};

/*
 * struct mtk_cam_resource_raw - MTK camsys raw resoruces for format negotiation
 *
 * @feature: value of V4L2_CID_MTK_CAM_FEATURE the user want to check the
 *		  resource with. If it is used in set CTRL, we will apply the value
 *		  to V4L2_CID_MTK_CAM_FEATURE ctrl directly.
 * @strategy: indicate the order of multiple raws, binning or DVFS to be selected
 *	      when doing format negotiation of raw's source pads (output pads).
 *	      Please pass MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to
 *	      determine it.
 * @raw_max: indicate the max number of raw to be used for the raw pipeline.
 *	     Please pass MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to
 *	     determine it.
 * @raw_min: indicate the max number of raw to be used for the raw pipeline.
 *	     Please pass MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to
 *	     determine it.
 * @raw_used: The number of raw used. The used don't need to writ this failed,
 *	      the driver always updates the field.
 * @bin: indicate if the driver should enable the bining or not. The driver
 *	 update the field depanding the hardware supporting status. Please pass
 *	 MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to determine it.
 * @path_sel: indicate the user selected raw path. The driver
 *	      update the field depanding the hardware supporting status. Please
 *	      pass MTK_CAM_RESOURCE_DEFAULT if you want camsys driver to
 *	      determine it.
 * @pixel_mode: the pixel mode driver used in the raw pipeline. It is written by
 *		drive only.
 * @throughput: the throughput be used in the raw pipeline. It is written by
 *		drive only.
 *
 */
struct mtk_cam_resource_raw {
	__u32	feature;
	__u16	strategy;
	__u8	raw_max;
	__u8	raw_min;
	__u8	raw_used;
	__u8	bin;
	__u8	path_sel;
	__u8	pixel_mode;
	__u64	throughput;
};

/*
 * struct mtk_cam_resource - MTK camsys resoruces for format negotiation
 *
 * @sink_fmt: sink_fmt pad's format, it must be return by g_fmt or s_fmt
 *		from driver.
 * @sensor_res: senor information to calculate the required resource, it is
 *		read-only and camsys driver will not change it.
 * @raw_res: user hint and resource negotiation result.
 * @status:	TBC
 *
 */
struct mtk_cam_resource {
	struct v4l2_mbus_framefmt *sink_fmt;
	struct mtk_cam_resource_sensor sensor_res;
	struct mtk_cam_resource_raw raw_res;
	__u8 status;
};
#endif /* __MTK_CAMERA_V4l2_CONTROLS_H */
