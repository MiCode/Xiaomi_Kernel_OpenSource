/*
 * Support for dw9761 vcm driver.
 *
 * Copyright (c) 2015 Intel Corporation. All Rights Reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DW9761_H__
#define __DW9761_H__

#include <linux/atomisp_platform.h>
#include <linux/types.h>

#define DW9761_VCM_ADDR	 (0x18 >> 1)

/* dw9761 device structure */
struct dw9761_device {
	struct timespec timestamp_t_focus_abs;
	s16 number_of_steps;
	bool initialized;		/* true if dw9761 is detected */
	s32 focus;			/* Current focus value */
	struct timespec focus_time;	/* Time when focus was last time set */
	__u8 buffer[4];			/* Used for i2c transactions */
	const struct camera_af_platform_data *platform_data;
	u8 ModuleVendor;
};

#define DW9761_INVALID_CONFIG	0xffffffff
#define DW9761_MAX_FOCUS_POS	1023
#define DELAY_PER_STEP_NS	1000000
#define DELAY_MAX_PER_STEP_NS	(1000000 * 1023)

#define DW9761_INFO			0
#define DW9761_ID			0xF4
#define DW9761_CONTROL			2
#define DW9761_VCM_CURRENT		3
#define DW9761_STATUS			5
#define DW9761_MODE				6
#define DW9761_VCM_FREQ			7
#define DW9761_VCM_PRELOAD		8

#define DW9761_MODE_SAC3		0x61
#define DW9761_DEFAULT_VCM_FREQ		0x3E
#define DW9761_ENABLE_RINGING		0x02

#define DW9761_OTP_ADDR				(0xB0 >> 1)
#define DEFAULT_DW9761_OTP_SIZE		544
#define DW9761_OTP_RAW_SIZE 578

#define DW9761_OTP_MODULE_INFO_START	0
#define DW9761_OTP_MODULE_INFO_SIZE		0x10
#define DW9761_OTP_AF_START	(DW9761_OTP_MODULE_INFO_START +\
		DW9761_OTP_MODULE_INFO_SIZE)
#define DW9761_OTP_AF_SIZE				0x10
#define DW9761_OTP_LS1_AWB_LSC_START	(DW9761_OTP_AF_START +\
		DW9761_OTP_AF_SIZE)
#define DW9761_OTP_LS1_AWB_LSC_SIZE		0x110
#define DW9761_OTP_LS1_AWB_OFFSET		0x125
#define DW9761_OTP_LS2_AWB_LSC_START	(DW9761_OTP_LS1_AWB_LSC_START +\
		DW9761_OTP_LS1_AWB_LSC_SIZE)
#define DW9761_OTP_LS2_AWB_LSC_SIZE		0x112
#define DW9761_OTP_LS2_AWB_OFFSET		0x235
#define DATA_VALID						0x01


#define INTEL_OTP_VER_OFFSET			0
#define INTEL_OTP_AF_OFFSET				2
#define INTEL_OTP_LS_OFFSET				13
#define INTEL_OTP_LSGRID_OFFSET			18
#define INTEL_OTP_LS1_LSC_OFFSET		20
#define INTEL_OTP_LS1_AWB_OFFSET		526
#define INTEL_OTP_LS2_LSC_OFFSET		273
#define INTEL_OTP_LS2_AWB_OFFSET		528
#define INTEL_OTP_CHECKSUM_OFFSET		542

#define DW9761_SAVE_RAW_OTP	"/data/misc/media/dw9761raw.otp"
#define DW9761_SAVE_PARSED_OTP	"/data/misc/media/dw9761parsed.otp"

int dw9761_vcm_power_down(struct v4l2_subdev *sd);
int dw9761_vcm_power_up(struct v4l2_subdev *sd);
int dw9761_vcm_init(struct v4l2_subdev *sd);
int dw9761_t_focus_abs(struct v4l2_subdev *sd, s32 value);
int dw9761_t_focus_vcm(struct v4l2_subdev *sd, u16 value);
int dw9761_t_focus_rel(struct v4l2_subdev *sd, s32 value);
int dw9761_q_focus_abs(struct v4l2_subdev *sd, s32 *value);
int dw9761_q_focus_status(struct v4l2_subdev *sd, s32 *value);
void *dw9761_otp_read(struct v4l2_subdev *sd, u8 **rawotp, u8 *vendorid);
int dw9761_otp_save(u8 *pData, u32 size, const u8 *filp_name);

#endif
