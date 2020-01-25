/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef HID_QVR_H_FILE
#define HID_QVR_H_FILE

#define QVR_EXTERNAL_SENSOR_REPORT_ID 0x1

//CMD IDs
#define QVR_CMD_ID_CALIBRATION_DATA_SIZE      20
#define QVR_CMD_ID_CALIBRATION_BLOCK_DATA     21
#define QVR_CMD_ID_START_CALIBRATION_UPDATE   22
#define QVR_CMD_ID_UPDATE_CALIBRATION_BLOCK   23
#define QVR_CMD_ID_FINISH_CALIBRATION_UPDATE  24
#define QVR_CMD_ID_IMU_CONTROL                25
#define QVR_CMD_ID_IMU_CONTROL_FALLBACK       7

#define QVR_HID_REPORT_ID_CAL                 2
#define QVR_HID_REQUEST_REPORT_SIZE           64

struct external_imu_format {
	u8 reportID;
	u8 padding;
	u16 version;
	u16 numIMUs;
	u16 numSamplesPerImuPacket;
	u16 totalPayloadSize;
	u8 reservedPadding[28];

	s16 imuID;
	s16 sampleID;
	s16 temperature;

	u64 gts0;
	u32 gNumerator;
	u32 gDenominator;
	s32 gx0;
	s32 gy0;
	s32 gz0;

	u64 ats0;
	u32 aNumerator;
	u32 aDenominator;
	s32 ax0;
	s32 ay0;
	s32 az0;

	u64 mts0;
	u32 mNumerator;
	u32 mDenominator;
	s32 mx0;
	s32 my0;
	s32 mz0;
} __packed;

void qvr_clear_def_parmeter(void);
void qvr_init(struct hid_device *hdev);
int qvr_input_init(void);
void qvr_input_remove(void);

#endif
