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
#define V4L2_CID_MTK_CAM_RAW_PATH_SELECT \
	(V4L2_CID_USER_MTK_CAM_BASE + 14)
#define V4L2_CID_MTK_CAM_HSF_EN \
	(V4L2_CID_USER_MTK_CAM_BASE + 15)
#define V4L2_CID_MTK_CAM_PDE_INFO \
	(V4L2_CID_USER_MTK_CAM_BASE + 16)
#define V4L2_CID_MTK_CAM_MSTREAM_EXPOSURE \
	(V4L2_CID_USER_MTK_CAM_BASE + 17)
#define V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC \
	(V4L2_CID_USER_MTK_CAM_BASE + 18)
#define V4L2_CID_MTK_CAM_TG_FLASH_CFG \
	(V4L2_CID_USER_MTK_CAM_BASE + 19)
#define V4L2_CID_MTK_CAM_RAW_RESOURCE_UPDATE \
	(V4L2_CID_USER_MTK_CAM_BASE + 20)
#define V4L2_CID_MTK_CAM_CAMSYS_HW_MODE \
	(V4L2_CID_USER_MTK_CAM_BASE + 21)
#define V4L2_CID_MTK_CAM_FRAME_SYNC \
	(V4L2_CID_USER_MTK_CAM_BASE + 22)

/* Allowed value of V4L2_CID_MTK_CAM_RAW_PATH_SELECT */
#define V4L2_MTK_CAM_RAW_PATH_SELECT_BPC	1
#define V4L2_MTK_CAM_RAW_PATH_SELECT_FUS	3
#define V4L2_MTK_CAM_RAW_PATH_SELECT_DGN	4
#define V4L2_MTK_CAM_RAW_PATH_SELECT_LSC	5
#define V4L2_MTK_CAM_RAW_PATH_SELECT_LTM	7

#define V4L2_MTK_CAM_TG_FALSH_ID_MAX		4
#define V4L2_MTK_CAM_TG_FLASH_MODE_SINGLE	0
#define V4L2_MTK_CAM_TG_FLASH_MODE_CONTINUOUS	1
#define V4L2_MTK_CAM_TG_FLASH_MODE_MULTIPLE	2

/* store the tg flush setting from user */
struct mtk_cam_tg_flash_config {
	__u32 flash_enable;
	__u32 flash_mode;
	__u32 flash_pluse_num;
	__u32 flash_offset;
	__u32 flash_high_width;
	__u32 flash_low_width;
	__u32 flash_light_id;
};

#define V4L2_MBUS_FRAMEFMT_PAD_ENABLE  BIT(1)

#define MEDIA_BUS_FMT_MTISP_SBGGR10_1X10		0x8001
#define MEDIA_BUS_FMT_MTISP_SBGGR12_1X12		0x8002
#define MEDIA_BUS_FMT_MTISP_SBGGR14_1X14		0x8003
#define MEDIA_BUS_FMT_MTISP_SGBRG10_1X10		0x8004
#define MEDIA_BUS_FMT_MTISP_SGBRG12_1X12		0x8005
#define MEDIA_BUS_FMT_MTISP_SGBRG14_1X14		0x8006
#define MEDIA_BUS_FMT_MTISP_SGRBG10_1X10		0x8007
#define MEDIA_BUS_FMT_MTISP_SGRBG12_1X12		0x8008
#define MEDIA_BUS_FMT_MTISP_SGRBG14_1X14		0x8009
#define MEDIA_BUS_FMT_MTISP_SRGGB10_1X10		0x800a
#define MEDIA_BUS_FMT_MTISP_SRGGB12_1X12		0x800b
#define MEDIA_BUS_FMT_MTISP_SRGGB14_1X14		0x800c
#define MEDIA_BUS_FMT_MTISP_BAYER8_UFBC			0x800d
#define MEDIA_BUS_FMT_MTISP_BAYER10_UFBC		0x800e
#define MEDIA_BUS_FMT_MTISP_BAYER12_UFBC		0x8010
#define MEDIA_BUS_FMT_MTISP_BAYER14_UFBC		0x8011
#define MEDIA_BUS_FMT_MTISP_BAYER16_UFBC		0x8012
#define MEDIA_BUS_FMT_MTISP_NV12			0x8013
#define MEDIA_BUS_FMT_MTISP_NV21			0x8014
#define MEDIA_BUS_FMT_MTISP_NV12_10			0x8015
#define MEDIA_BUS_FMT_MTISP_NV21_10			0x8016
#define MEDIA_BUS_FMT_MTISP_NV12_10P			0x8017
#define MEDIA_BUS_FMT_MTISP_NV21_10P			0x8018
#define MEDIA_BUS_FMT_MTISP_NV12_12			0x8019
#define MEDIA_BUS_FMT_MTISP_NV21_12			0x801a
#define MEDIA_BUS_FMT_MTISP_NV12_12P			0x801b
#define MEDIA_BUS_FMT_MTISP_NV21_12P			0x801c
#define MEDIA_BUS_FMT_MTISP_YUV420			0x801d
#define MEDIA_BUS_FMT_MTISP_NV12_UFBC			0x801e
#define MEDIA_BUS_FMT_MTISP_NV21_UFBC			0x8020
#define MEDIA_BUS_FMT_MTISP_NV12_10_UFBC		0x8021
#define MEDIA_BUS_FMT_MTISP_NV21_10_UFBC		0x8022
#define MEDIA_BUS_FMT_MTISP_NV12_12_UFBC		0x8023
#define MEDIA_BUS_FMT_MTISP_NV21_12_UFBC		0x8024
#define MEDIA_BUS_FMT_MTISP_NV16			0x8025
#define MEDIA_BUS_FMT_MTISP_NV61			0x8026
#define MEDIA_BUS_FMT_MTISP_NV16_10			0x8027
#define MEDIA_BUS_FMT_MTISP_NV61_10			0x8028
#define MEDIA_BUS_FMT_MTISP_NV16_10P			0x8029
#define MEDIA_BUS_FMT_MTISP_NV61_10P			0x802a

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
	__u64 cust_pixel_rate;
	__u64 driver_buffered_pixel_rate;
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
 *		driver only.
 * @throughput: the throughput be used in the raw pipeline. It is written by
 *		driver only.
 *
 */
struct mtk_cam_resource_raw {
	__s64	feature;
	__u16	strategy;
	__u8	raw_max;
	__u8	raw_min;
	__u8	raw_used;
	__u8	bin;
	__u8	path_sel;
	__u8	pixel_mode;
	__u64	throughput;
	__s64	hw_mode;
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

enum mtk_cam_ctrl_type {
	CAM_SET_CTRL = 0,
	CAM_TRY_CTRL,
	CAM_CTRL_NUM,
};

/**
 * struct mtk_cam_pde_info - PDE module information for raw
 *
 * @pdo_max_size: the max pdo size of pde sensor.
 * @pdi_max_size: the max pdi size of pde sensor or max pd table size.
 * @pd_table_offset: the offest of meta config for pd table content.
 * @meta_cfg_size: the enlarged meta config size.
 * @meta_0_size: the enlarged meta 0 size.
 */
struct mtk_cam_pde_info {
	__u32 pdo_max_size;
	__u32 pdi_max_size;
	__u32 pd_table_offset;
	__u32 meta_cfg_size;
	__u32 meta_0_size;
};

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

#define V4L2_CID_MTK_SENSOR_POWER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 25)

#define V4L2_CID_MTK_MSTREAM_MODE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 26)

#define V4L2_CID_MTK_N_1_MODE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 27)

#define V4L2_CID_MTK_CUST_SENSOR_PIXEL_RATE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 28)

#define V4L2_CID_MTK_CSI_PARAM \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 29)

#define V4L2_CID_MTK_SENSOR_TEST_PATTERN_DATA \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 30)

#define V4L2_CID_MTK_SENSOR_RESET \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 31)

#define V4L2_CID_MTK_SENSOR_INIT \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 32)

#define V4L2_CID_MTK_SOF_TIMEOUT_VALUE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 33)

#define V4L2_CID_MTK_SENSOR_RESET_S_STREAM \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 34)

#define V4L2_CID_MTK_SENSOR_RESET_BY_USER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 35)



/* S E N I N F */
#define V4L2_CID_MTK_SENINF_S_STREAM \
		(V4L2_CID_USER_MTK_SENINF_BASE + 1)

#define V4L2_CID_FSYNC_MAP_ID \
	(V4L2_CID_USER_MTK_SENINF_BASE + 2)

#define V4L2_CID_VSYNC_NOTIFY \
	(V4L2_CID_USER_MTK_SENINF_BASE + 3)

#define V4L2_CID_UPDATE_SOF_CNT \
	(V4L2_CID_USER_MTK_SENINF_BASE + 4)

#endif /* __MTK_CAMERA_V4l2_CONTROLS_H */
