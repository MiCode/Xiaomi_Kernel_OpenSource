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
#define V4L2_CID_MTK_CAM_INTERNAL_MEM_CTRL \
	(V4L2_CID_USER_MTK_CAM_BASE + 24)
#define V4L2_CID_MTK_CAM_CAMSYS_HDR_TIMESTAMP \
	(V4L2_CID_USER_MTK_CAM_BASE + 25)
#define V4L2_CID_MTK_CAM_APU_INFO \
	(V4L2_CID_USER_MTK_CAM_BASE + 26)
#define V4L2_CID_MTK_CAM_CAMSYS_VF_RESET \
	(V4L2_CID_USER_MTK_CAM_BASE + 27)

/* used for v2 resoruce struct testing */
#define V4L2_CID_MTK_CAM_RAW_RESOURCE_CALC_TEST \
	(V4L2_CID_USER_MTK_CAM_BASE + 47)

/* Allowed value of V4L2_CID_MTK_CAM_RAW_PATH_SELECT */
#define V4L2_MTK_CAM_RAW_PATH_SELECT_BPC	1
#define V4L2_MTK_CAM_RAW_PATH_SELECT_FUS	3
#define V4L2_MTK_CAM_RAW_PATH_SELECT_DGN	4
#define V4L2_MTK_CAM_RAW_PATH_SELECT_LSC	5
#define V4L2_MTK_CAM_RAW_PATH_SELECT_HLR	6
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

struct mtk_cam_hdr_timestamp_info {
	__u64 le;
	__u64 le_mono;
	__u64 ne;
	__u64 ne_mono;
	__u64 se;
	__u64 se_mono;
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

#define MEDIA_BUS_FMT_MTISP_SBGGR22_1X22		0x8100
#define MEDIA_BUS_FMT_MTISP_SGBRG22_1X22		0x8101
#define MEDIA_BUS_FMT_MTISP_SGRBG22_1X22		0x8102
#define MEDIA_BUS_FMT_MTISP_SRGGB22_1X22		0x8103

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
 * @img_wbuf_size: the img working buffer size considering chaning sensor after
 *		   streaming on, it is required in VHDR SAT scenarios.
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
	__u32	img_wbuf_size;
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

/**
 *  mtk cam V4l2 ctrl structures V2
 */

/**
 * struct mtk_cam_resource_sensor_v2
 *
 * @no_bufferd_prate_calc: notify driver don't use buffered pixel rate
 *			   as the thrpughput requriement; use
 *			   pixel_rate passed directly. If the pixel_rate is coming
 *			   from sensor driver's custom pixel rate, please set it
 *			   true since we can't trust the hblank value in such
 *			   case;
 * @driver_buffered_pixel_rate: only used in legacy driver and could be phased-out.
 */
struct mtk_cam_resource_sensor_v2 {
	__u32	width;
	__u32	height;
	__u32	code;
	struct	v4l2_fract interval;
	__u32	hblank;
	__u32	vblank;
	__u64	pixel_rate;
	__u8	no_bufferd_prate_calc;
	__u64	driver_buffered_pixel_rate;
};

/**
 *  enum mtk_cam_scen_id - camsys hardware scenario ids
 *
 * @MTK_CAM_SCEN_NORMAL: The default scenario
 * MTK_CAM_SCEN_M2M_NORMAL: the m2m scenario
 */
enum mtk_cam_scen_id {
	MTK_CAM_SCEN_NORMAL,
	MTK_CAM_SCEN_MSTREAM,
	MTK_CAM_SCEN_SMVR,
	MTK_CAM_SCEN_ODT_NORMAL,
	MTK_CAM_SCEN_ODT_MSTREAM,
	MTK_CAM_SCEN_M2M_NORMAL,
	MTK_CAM_SCEN_TIMESHARE,
	MTK_CAM_SCEN_CAMSV_RGBW, // for ISP7.1, output W chn via CAMSV
	MTK_CAM_SCEN_EXT_ISP,
};

enum mtk_cam_exp_order {
	MTK_CAM_EXP_SE_LE,
	MTK_CAM_EXP_LE_SE,
};

enum mtk_cam_frame_order {
	MTK_CAM_FRAME_BAYER_W,
	MTK_CAM_FRAME_W_BAYER,
};

/**
 * struct mtk_cam_scen_normal - common properties
 *         in different scenario
 * @max_exp_num: max number of exposure
 * @exp_num: current number of exposure
 * @exp_order: order of exposure readout,
 *         see mtk_cam_exp_order
 * @w_chn_supported: support W channel
 * @w_chn_enabled: w/ or w/o W channel
 * @frame_order: order of bayer-w, see mtk_cam_frame_order
 * @mem_saving: memory saving
 */
struct mtk_cam_scen_normal {
	__u8 max_exp_num:4;
	__u8 exp_num:4;
	__u8 exp_order:4;
	__u8 w_chn_supported:4;
	__u8 w_chn_enabled:4;
	__u8 frame_order:4;
	__u8 mem_saving:4;
};

enum mtk_cam_mstream_type {
	MTK_CAM_MSTREAM_1_EXPOSURE		= 0,
	MTK_CAM_MSTREAM_NE_SE			= 5,
	MTK_CAM_MSTREAM_SE_NE			= 6,
};

/**
 * struct mtk_cam_scen_mstream - mstream scenario user hints
 *
 * @type: the hardware scenario of the frame, please check
 *	       mtk_cam_mstream_type for the allowed value
 * @mem_saving: 1 means enable mem_saving
 */
struct mtk_cam_scen_mstream {
	__u32	type;
	__u8	mem_saving;
};

enum mtk_cam_subsample_num_allowed {
	MTK_CAM_SMVR_2_SUBSAMPLE		= 2,
	MTK_CAM_SMVR_4_SUBSAMPLE		= 4,
	MTK_CAM_SMVR_8_SUBSAMPLE		= 8,
	MTK_CAM_SMVR_16_SUBSAMPLE		= 16,
	MTK_CAM_SMVR_32_SUBSAMPLE		= 32,
};

/**
 * struct mtk_cam_scen_smvr - smvr scenario user hints
 *
 * @subsample_num: the subsample number of the frame, please check
 *	       mtk_cam_subsample_num_allowed for the allowed value
 * @output_first_frame_only: set it true when the user donesn't need
 *		     the images except the first one in SMVR scenario.
 */
struct mtk_cam_scen_smvr {
	__u8 subsample_num;
	__u8 output_first_frame_only;
};

enum mtk_cam_extisp_type {
	MTK_CAM_EXTISP_CUS_1			= 1,
	MTK_CAM_EXTISP_CUS_2			= 2,
	MTK_CAM_EXTISP_CUS_3			= 3,
};

/**
 * struct mtk_cam_scen_extisp - smvr scenario user hints
 *
 * @subsampletype_num: the ext isp type of the frame, please check
 *	       mtk_cam_extisp_type for the allowed value
 */
struct mtk_cam_scen_extisp {
	enum mtk_cam_extisp_type type;
};

enum mtk_cam_timeshare_group {
	MTK_CAM_TIMESHARE_GROUP_1 = 1,
};

/**
 * struct mtk_cam_scen_timeshare - smvr scenario user hints
 *
 * @group: the time group of the frame, please check
 *	       mtk_cam_timeshare_group for the allowed value
 */
struct mtk_cam_scen_timeshare {
	__u8 group;
};

/**
 * struct mtk_cam_scen - hardware scenario user hints
 *
 * @id: the id of the hardware scenario.
 * @scen: union of struct of diff scenario:
 * MTK_CAM_SCEN_NORMAL, MTK_CAM_SCEN_ODT_NORMAL,
 * MTK_CAM_SCEN_M2M_NORMAL=> normal
 * MTK_CAM_SCEN_MSTREAM => mstream
 * MTK_CAM_SCEN_SMVR => smvr
 * MTK_CAM_SCEN_EXT_ISP => extisp
 * MTK_CAM_SCEN_TIMESHARE => timeshare
 */

struct mtk_cam_scen {
	enum mtk_cam_scen_id id;
	union {
		struct mtk_cam_scen_normal normal;
		struct mtk_cam_scen_mstream	mstream;
		struct mtk_cam_scen_smvr	smvr;
		struct mtk_cam_scen_extisp	extisp;
		struct mtk_cam_scen_timeshare	timeshare;
	} scen;
	char dbg_str[16];
};

#define MTK_CAM_RAW_A	0x0001
#define MTK_CAM_RAW_B	0x0002
#define MTK_CAM_RAW_C	0x0004

/* to be refined, not to use bit mask */
enum mtk_cam_bin {
	MTK_CAM_BIN_OFF	=	0,
	MTK_CAM_BIN_ON =	(1 << 0),
	MTK_CAM_CBN_2X2_ON =	(1 << 4),
	MTK_CAM_CBN_3X3_ON =	(1 << 5),
	MTK_CAM_CBN_4X4_ON =	(1 << 6),
	MTK_CAM_QBND_ON	=	(1 << 8)
};

/**
 * struct mtk_cam_resource_raw_v2
 *
 * @raws_max_num: Max number of raws can be used. This is only
 *		  used when user let driver select raws.
 *		  (raws = 0, and raws_must = 0)
 */
struct mtk_cam_resource_raw_v2 {
	struct mtk_cam_scen scen;
	__u8	raws;
	__u8	raws_must;
	__u8	raws_max_num;
	__u8	bin;
	__u8	raw_pixel_mode;
	__u8	hw_mode;
	__u32	img_wbuf_size;
	__u32	img_wbuf_num;
};

struct mtk_cam_resource_v2 {
	struct mtk_cam_resource_sensor_v2 sensor_res;
	struct mtk_cam_resource_raw_v2 raw_res;
};

#define MTK_CAM_INTERNAL_MEM_MAX 8

struct mtk_cam_internal_buf {
	__s32 fd;
	__u32 length;
};

struct mtk_cam_internal_mem {
	__u32 num;
	struct mtk_cam_internal_buf bufs[MTK_CAM_INTERNAL_MEM_MAX];
};

/**
 * struct mtk_cam_apu_info - apu related information
 *  @is_update: kernel control only
 *
 */

enum mtk_cam_apu_tap_point {
	AFTER_SEP_R1,
	AFTER_BPC,
	AFTER_LTM,
};

enum mtk_cam_apu_path {
	APU_NONE,
	APU_FRAME_MODE,
	APU_DC_RAW,
	RAW_DC_APU,
	RAW_DC_APU_DC_RAW,
};

struct mtk_cam_apu_info {
	__u8 is_update;
	__u8 apu_path;
	__u8 vpu_i_point;
	__u8 vpu_o_point;
	__u8 sysram_en;
	__u8 opp_index;
	__u32 block_y_size;
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

#define V4L2_CID_MTK_SENSOR_IDX \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 36)

#define V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SCL_AUX \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 37)

#define V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SDA_AUX \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 38)

#define V4L2_CID_FSYNC_ASYNC_MASTER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 39)

#define V4L2_CID_MTK_AOV_SWITCH_RX_PARAM \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 40)

#define V4L2_CID_MTK_AOV_SWITCH_PM_OPS \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 41)

/**
 * enum scl for imgsensor gpio aux function.
 */
enum mtk_cam_sensor_i2c_bus_scl {
	SCL0 = 0,
	SCL1,
	SCL2,
	SCL3,
	SCL4,
	SCL5,
	SCL6,
	SCL7,
	SCL8,
	SCL9,
	SCL10,
	SCL_MAXCNT,
	SCL_ERR = 0xffff,
};

/**
 * enum sda for imgsensor gpio aux function.
 */
enum mtk_cam_sensor_i2c_bus_sda {
	SDA0 = 0,
	SDA1,
	SDA2,
	SDA3,
	SDA4,
	SDA5,
	SDA6,
	SDA7,
	SDA8,
	SDA9,
	SDA10,
	SDA_MAXCNT,
	SDA_ERR = 0xffff,
};

enum stream_mode {
	NORMAL_CAMERA = 0,
	AOV_REAL_SENSOR,
	AOV_TEST_MODEL,
};

struct mtk_seninf_s_stream {
	enum stream_mode stream_mode;
	int enable;
};

enum mtk_cam_seninf_csi_clk_for_param {
	CSI_CLK_52 = 0,
	CSI_CLK_65,
	CSI_CLK_104,
	CSI_CLK_130,
	CSI_CLK_242,
	CSI_CLK_260,
	CSI_CLK_312,
	CSI_CLK_416,
	CSI_CLK_499,
};

enum mtk_cam_sensor_pm_ops {
	AOV_PM_RELAX = 0,
	AOV_PM_STAY_AWAKE,
	AOV_ABNORMAL_FORCE_SENSOR_PWR_OFF,
	AOV_ABNORMAL_FORCE_SENSOR_PWR_ON,
};

/* S E N I N F */
#define V4L2_CID_MTK_SENINF_S_STREAM \
		(V4L2_CID_USER_MTK_SENINF_BASE + 1)

#define V4L2_CID_FSYNC_MAP_ID \
	(V4L2_CID_USER_MTK_SENINF_BASE + 2)

#define V4L2_CID_VSYNC_NOTIFY \
	(V4L2_CID_USER_MTK_SENINF_BASE + 3)

#define V4L2_CID_TEST_PATTERN_FOR_AOV_PARAM \
	(V4L2_CID_USER_MTK_SENINF_BASE + 4)

#define V4L2_CID_FSYNC_LISTEN_TARGET \
	(V4L2_CID_USER_MTK_SENINF_BASE + 5)

#define V4L2_CID_REAL_SENSOR_FOR_AOV_PARAM \
	(V4L2_CID_USER_MTK_SENINF_BASE + 6)

#define V4L2_CID_UPDATE_SOF_CNT \
	(V4L2_CID_USER_MTK_SENINF_BASE + 7)

#endif /* __MTK_CAMERA_V4l2_CONTROLS_H */
