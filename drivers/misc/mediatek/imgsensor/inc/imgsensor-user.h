/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2020 MediaTek Inc. */

#ifndef __IMGSENSOR_USER_H__
#define __IMGSENSOR_USER_H__

#include <linux/videodev2.h>

#include "kd_imgsensor_define_v4l2.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"

#define DEFAULT_WIDTH 1600
#define DEFAULT_HEIGHT 1200

enum {
	PAD_SINK = 0,
	PAD_SRC_RAW0,
	PAD_SRC_RAW1,
	PAD_SRC_RAW2,
	PAD_SRC_RAW_W0,
	PAD_SRC_RAW_EXT0,
	PAD_SRC_PDAF0,
	PAD_SRC_PDAF1,
	PAD_SRC_PDAF2,
	PAD_SRC_PDAF3,
	PAD_SRC_PDAF4,
	PAD_SRC_PDAF5,
	PAD_SRC_PDAF6,
	PAD_SRC_HDR0,
	PAD_SRC_HDR1,
	PAD_SRC_HDR2,
	PAD_SRC_GENERAL0,
	PAD_MAXCNT,
	PAD_ERR = 0xffff,
};

struct mtk_awb_gain {
	__u32 abs_gain_gr;
	__u32 abs_gain_r;
	__u32 abs_gain_b;
	__u32 abs_gain_gb;
};

struct mtk_shutter_gain_sync {
	__u32 shutter;
	__u32 gain;
};

struct mtk_dual_gain {
	__u32 le_gain;
	__u32 se_gain;
};

struct mtk_ihdr_shutter_gain {
	__u32 le_shutter;
	__u32 se_shutter;
	__u32 gain;
};

struct mtk_pixel_mode {
	__u32 pixel_mode;
	__u32 pad_id;
};


struct mtk_hdr_shutter {
	__u32 le_shutter;
	__u32 se_shutter;
};

struct mtk_shutter_frame_length {
	__u32 shutter;
	__u32 frame_length;
	__u32 auto_extend_en;
};

struct mtk_fps_by_scenario {
	__u32 scenario_id;
	__u32 fps;
};

struct mtk_pclk_by_scenario {
	__u32 scenario_id;
	__u32 pclk;
};

struct mtk_llp_fll_by_scenario {
	__u32 scenario_id;
	__u32 llp;
	__u32 fll;
};

struct mtk_gain_range_by_scenario {
	__u32 scenario_id;
	__u32 min_gain;
	__u32 max_gain;
};

struct mtk_min_shutter_by_scenario {
	__u32 scenario_id;
	__u32 min_shutter;
	__u32 shutter_step;
};

struct mtk_base_gain_iso_n_step {
	__u32 min_gain_iso;
	__u32 gain_step;
	__u32 gain_type;
};

struct mtk_crop_by_scenario {
	__u32 scenario_id;
	struct SENSOR_WINSIZE_INFO_STRUCT *p_winsize;
};

struct mtk_vcinfo_by_scenario {
	__u32 scenario_id;
	struct SENSOR_VC_INFO2_STRUCT *p_vcinfo;
};

struct mtk_pdaf_info_by_scenario {
	__u32 scenario_id;
	struct SET_PD_BLOCK_INFO_T *p_pd;
};

struct mtk_cap {
	__u32 scenario_id;
	__u32 cap;
};

struct mtk_binning_type {
	__u32 scenario_id;
	__u32 HDRMode;
	__u32 binning_type;
};

struct mtk_ana_gain_table {
	int size;
	__u32 *p_buf;
};

struct mtk_llp_fll {
	__u32 llp;
	__u32 fll;
};

struct mtk_pdaf_data {
	int offset;
	int size;
	__u8 *p_buf;
};

struct mtk_pdfocus_area {
	__u32 pos;
	__u32 size;
};

struct mtk_mipi_pixel_rate {
	__u32 scenario_id;
	__u32 mipi_pixel_rate;
};

struct mtk_4cell_data {
	int type;
	int size;
	__u8 *p_buf;
};

struct mtk_hdr_atr {
	__u32 limit_gain;
	__u32 ltc_rate;
	__u32 post_gain;
};

struct mtk_hdr_exposure {
	union {
		struct {
			__u32 le_exposure;
			__u32 me_exposure;
			__u32 se_exposure;
			__u32 sse_exposure;
			__u32 ssse_exposure;
		};

		__u32 arr[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	};

};

struct mtk_hdr_gain {
	union {
		struct {
			__u32 le_gain;
			__u32 me_gain;
			__u32 se_gain;
			__u32 sse_gain;
			__u32 ssse_gain;
		};

		__u32 arr[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	};

};

struct mtk_hdr_ae {
	struct mtk_hdr_exposure exposure;
	struct mtk_hdr_gain gain;
	__u32 actions;
	__u32 subsample_tags;
};

struct mtk_seamless_switch_param {
	struct mtk_hdr_ae ae_ctrl[2];
	__u32 frame_length[2];
	__u32 target_scenario_id;
};

/* struct mtk_regs
 * @size:
	the size of buffer in bytes.
 * @p_buf:
	addr, val, addr, val, ... in that order
	in unit of __u16
 */
struct mtk_regs {
	int size;
	__u16 *p_buf;
};

struct mtk_sensor_info {
	char name[64];
	__u32 id;
	__u32 dir;
	__u32 bitOrder;
	__u32 orientation;
	__u32 horizontalFov;
	__u32 verticalFov;
};

struct mtk_scenario_timing {
	__u32 llp;
	__u32 fll;
	__u32 width;
	__u32 height;
	__u32 mipi_pixel_rate;
	__u32 max_framerate;
	__u32 pclk;
	__u64 linetime_in_ns;
};

struct mtk_scenario_combo_info {
	__u32 scenario_id;
	struct mtk_scenario_timing *p_timing;
	struct SET_PD_BLOCK_INFO_T *p_pd;
	struct SENSOR_WINSIZE_INFO_STRUCT *p_winsize;
	struct SENSOR_VC_INFO2_STRUCT *p_vcinfo;
};

struct mtk_min_max_fps {
	__u32 min_fps;
	__u32 max_fps;
};

struct mtk_feature_info {
	__u32 scenario_id;
	struct ACDK_SENSOR_INFO_STRUCT *p_info;
	struct ACDK_SENSOR_CONFIG_STRUCT *p_config;
	struct ACDK_SENSOR_RESOLUTION_INFO_STRUCT *p_resolution;
};

struct mtk_lsc_tbl {
	int index;
	int size;
	__u16 *p_buf;
};

struct mtk_sensor_control {
	int scenario_id;
	struct ACDK_SENSOR_EXPOSURE_WINDOW_STRUCT *p_window;
	struct ACDK_SENSOR_CONFIG_STRUCT *p_config;
};

struct mtk_stagger_info {
	__u32 scenario_id;
	__u32 count;
	int order[IMGSENSOR_STAGGER_EXPOSURE_CNT];
};

struct mtk_stagger_target_scenario {
	__u32 scenario_id;
	__u32 exposure_num;
	__u32 target_scenario_id;
};

struct mtk_seamless_target_scenarios {
	__u32 scenario_id;
	__u32 count;
	__u32 *target_scenario_ids;
};

struct mtk_stagger_max_exp_time {
	__u32 scenario_id;
	__u32 exposure;
	__u32 max_exp_time;
};

struct mtk_max_exp_line {
	__u32 scenario_id;
	__u32 exposure;
	__u32 max_exp_line;
};

struct mtk_exp_margin {
	__u32 scenario_id;
	__u32 margin;
};

struct mtk_sensor_value {
	__u32 scenario_id;
	__u32 value;
};

struct mtk_sensor_static_param {
	__u32 scenario_id;
	__u32 fps;
	__u32 vblank;
	__u32 hblank;
	__u32 pixelrate;
	__u32 cust_pixelrate;
};
struct mtk_mbus_frame_desc_entry_csi2 {
	u8 channel;
	u8 data_type;
	u8 enable;
	u16 hsize;
	u16 vsize;
	u16 user_data_desc;
};


struct mtk_mbus_frame_desc_entry {
	//enum v4l2_mbus_frame_desc_flags flags;
	//u32 pixelcode;
	//u32 length;
	union {
		struct mtk_mbus_frame_desc_entry_csi2 csi2;
	} bus;
};
#define MTK_FRAME_DESC_ENTRY_MAX 8


enum mtk_mbus_frame_desc_type {
	MTK_MBUS_FRAME_DESC_TYPE_PLATFORM,
	MTK_MBUS_FRAME_DESC_TYPE_PARALLEL,
	MTK_MBUS_FRAME_DESC_TYPE_CCP2,
	MTK_MBUS_FRAME_DESC_TYPE_CSI2,
};



struct mtk_mbus_frame_desc {
	enum mtk_mbus_frame_desc_type type;
	struct mtk_mbus_frame_desc_entry entry[MTK_FRAME_DESC_ENTRY_MAX];
	unsigned short num_entries;
};

struct mtk_csi_param {
	__u8 dphy_trail;
	__u8 dphy_data_settle;
	__u8 dphy_clk_settle;
	__u8 cphy_settle;
	__u8 legacy_phy;
	__u8 not_fixed_trail_settle;
};


struct mtk_n_1_mode {
	__u32 n;
	__u8 en;
};

struct mtk_test_pattern_data {
	__u32 Channel_R;
	__u32 Channel_Gr;
	__u32 Channel_Gb;
	__u32 Channel_B;
};


struct mtk_fine_integ_line {
	__u32 scenario_id;
	__u32 fine_integ_line;
};

/* GET */

#define VIDIOC_MTK_G_DEF_FPS_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 1, struct mtk_fps_by_scenario)

#define VIDIOC_MTK_G_PCLK_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 2, struct mtk_pclk_by_scenario)

#define VIDIOC_MTK_G_LLP_FLL_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 3, struct mtk_llp_fll_by_scenario)

#define VIDIOC_MTK_G_GAIN_RANGE_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 4, struct mtk_gain_range_by_scenario)

#define VIDIOC_MTK_G_MIN_SHUTTER_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 5, struct mtk_min_shutter_by_scenario)

#define VIDIOC_MTK_G_CROP_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 6, struct mtk_crop_by_scenario)

#define VIDIOC_MTK_G_VCINFO_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 7, struct mtk_vcinfo_by_scenario)

#define VIDIOC_MTK_G_SCENARIO_COMBO_INFO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 8, struct mtk_scenario_combo_info)

#define VIDIOC_MTK_G_BINNING_TYPE \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 10, struct mtk_binning_type)

#define VIDIOC_MTK_G_PDAF_INFO_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 11, struct mtk_pdaf_info_by_scenario)

#define VIDIOC_MTK_G_LLP_FLL \
	_IOR('M', BASE_VIDIOC_PRIVATE + 12, struct mtk_llp_fll)

#define VIDIOC_MTK_G_PCLK \
	_IOR('M', BASE_VIDIOC_PRIVATE + 13, unsigned int)

#define VIDIOC_MTK_G_PDAF_DATA \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 14, struct mtk_pdaf_data)

#define VIDIOC_MTK_G_PDAF_CAP \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 15, struct mtk_cap)

#define VIDIOC_MTK_G_PDAF_REGS \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 16, struct mtk_regs)

#define VIDIOC_MTK_G_MIPI_PIXEL_RATE \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 17, struct mtk_mipi_pixel_rate)

#define VIDIOC_MTK_G_HDR_CAP \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 18, struct mtk_cap)

#define VIDIOC_MTK_G_TEST_PATTERN_CHECKSUM \
	_IOR('M', BASE_VIDIOC_PRIVATE + 20, __u32)

#define VIDIOC_MTK_G_BASE_GAIN_ISO_N_STEP \
	_IOR('M', BASE_VIDIOC_PRIVATE + 21, struct mtk_base_gain_iso_n_step)

#define VIDIOC_MTK_G_OFFSET_TO_START_OF_EXPOSURE \
	_IOR('M', BASE_VIDIOC_PRIVATE + 22, unsigned int)

#define VIDIOC_MTK_G_ANA_GAIN_TABLE_SIZE \
	_IOR('M', BASE_VIDIOC_PRIVATE + 23, struct mtk_ana_gain_table)

#define VIDIOC_MTK_G_ANA_GAIN_TABLE \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 24, struct mtk_ana_gain_table)

#define VIDIOC_MTK_G_DELAY_INFO \
	_IOR('M', BASE_VIDIOC_PRIVATE + 25, struct SENSOR_DELAY_INFO_STRUCT)

#define VIDIOC_MTK_G_FEATURE_INFO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 26, struct mtk_feature_info)

#define VIDIOC_MTK_G_4CELL_DATA \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 27, struct mtk_4cell_data)

#define VIDIOC_MTK_G_AE_FRAME_MODE_FOR_LE \
	_IOR('M', BASE_VIDIOC_PRIVATE + 28, struct IMGSENSOR_AE_FRM_MODE)

#define VIDIOC_MTK_G_AE_EFFECTIVE_FRAME_FOR_LE \
	_IOR('M', BASE_VIDIOC_PRIVATE + 29, unsigned int)

#define VIDIOC_MTK_G_SENSOR_INFO \
	_IOR('M', BASE_VIDIOC_PRIVATE + 30, struct mtk_sensor_info)

#define VIDIOC_MTK_G_EXPOSURE_MARGIN_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 31, struct mtk_exp_margin)

#define VIDIOC_MTK_G_SEAMLESS_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 32, struct mtk_seamless_target_scenarios)

#define VIDIOC_MTK_G_CUSTOM_READOUT_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 33, struct mtk_sensor_value)

#define VIDIOC_MTK_G_STAGGER_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 34, struct mtk_stagger_target_scenario)

#define VIDIOC_MTK_G_MAX_EXPOSURE \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 35, struct mtk_stagger_max_exp_time)

#define VIDIOC_MTK_G_PRELOAD_EEPROM_DATA \
	_IOR('M', BASE_VIDIOC_PRIVATE + 36, unsigned int)

#define VIDIOC_MTK_G_OUTPUT_FORMAT_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 37, struct mtk_sensor_value)

#define VIDIOC_MTK_G_FINE_INTEG_LINE_BY_SCENARIO \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 38, struct mtk_fine_integ_line)

#define VIDIOC_MTK_G_MAX_EXPOSURE_LINE \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 39, struct mtk_max_exp_line)

/* SET */

#define VIDIOC_MTK_S_VIDEO_FRAMERATE \
	_IOW('M', BASE_VIDIOC_PRIVATE + 101, __u32)

#define VIDIOC_MTK_S_MAX_FPS_BY_SCENARIO \
	_IOW('M', BASE_VIDIOC_PRIVATE + 102, struct mtk_fps_by_scenario)

#define VIDIOC_MTK_S_FRAMERATE \
	_IOW('M', BASE_VIDIOC_PRIVATE + 103, __u32)

#define VIDIOC_MTK_S_HDR \
	_IOW('M', BASE_VIDIOC_PRIVATE + 104, int)

#define VIDIOC_MTK_S_PDAF_REGS \
	_IOW('M', BASE_VIDIOC_PRIVATE + 105, struct mtk_regs)

#define VIDIOC_MTK_S_PDAF \
	_IOW('M', BASE_VIDIOC_PRIVATE + 106, int)

#define VIDIOC_MTK_S_MIN_MAX_FPS \
	_IOW('M', BASE_VIDIOC_PRIVATE + 107, struct mtk_min_max_fps)

#define VIDIOC_MTK_S_LSC_TBL \
	_IOW('M', BASE_VIDIOC_PRIVATE + 108, struct mtk_lsc_tbl)

#define VIDIOC_MTK_S_CONTROL \
	_IOW('M', BASE_VIDIOC_PRIVATE + 109, struct mtk_sensor_control)

#define VIDIOC_MTK_S_TG \
	_IOW('M', BASE_VIDIOC_PRIVATE + 110, int)

#endif
