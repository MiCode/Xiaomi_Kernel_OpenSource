/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2020 MediaTek Inc. */

#ifndef __ADAPTOR_SUBDRV_H__
#define __ADAPTOR_SUBDRV_H__

#include <media/v4l2-subdev.h>

#include "kd_imgsensor_define.h"

#ifdef V4L2_MBUS_CSI2_IS_USER_DEFINED_DATA
#define IMGSENSOR_VC_ROUTING
#endif

enum {
	HW_ID_AVDD = 0,
	HW_ID_DVDD,
	HW_ID_DOVDD,
	HW_ID_AFVDD,
	HW_ID_PDN,
	HW_ID_RST,
	HW_ID_MCLK,
	HW_ID_MCLK_DRIVING_CURRENT,
	HW_ID_MIPI_SWITCH,
	HW_ID_MAXCNT,
};

struct imgsensor_pw_seq_entry {
	int id;
	int val;
	int delay;
};

#define HDR_CAP_IHDR 0x1
#define HDR_CAP_MVHDR 0x2
#define HDR_CAP_ZHDR 0x4
#define HDR_CAP_3HDR 0x8
#define HDR_CAP_ATR 0x10

#define PDAF_CAP_PIXEL_DATA_IN_RAW 0x1
#define PDAF_CAP_PIXEL_DATA_IN_VC 0x2
#define PDAF_CAP_DIFF_DATA_IN_VC 0x4
#define PDAF_CAP_PDFOCUS_AREA 0x10

struct imgsensor_subdrv_entry {
	const char *name;
	unsigned int id;
	struct SENSOR_FUNCTION_STRUCT *ops;
	struct imgsensor_pw_seq_entry *pw_seq;
	unsigned int is_hflip:1;
	unsigned int is_vflip:1;
	unsigned int hdr_cap;
	unsigned int pdaf_cap;
	int pw_seq_cnt;
	int max_frame_length;
	int ana_gain_min;
	int ana_gain_max;
	int ana_gain_step;
	int ana_gain_def;
	int exposure_min;
	int exposure_max;
	int exposure_step;
	int exposure_def;
	int le_shutter_def;
	int me_shutter_def;
	int se_shutter_def;
	int le_gain_def;
	int me_gain_def;
	int se_gain_def;
	int (*get_frame_desc)(int scenario_id, struct v4l2_mbus_frame_desc *fd);
};

#endif
