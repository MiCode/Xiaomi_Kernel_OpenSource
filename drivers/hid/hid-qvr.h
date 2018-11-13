/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

struct external_imu_format {
	s16 temp0;
	s16 temp1;
	s16 temp2;
	s16 temp3;
	u64 gts0;
	u64 gts1;
	u64 gts2;
	u64 gts3;
	s16 gx0;
	s16 gx1;
	s16 gx2;
	s16 gx3;
	s16 gx4;
	s16 gx5;
	s16 gx6;
	s16 gx7;
	s16 gx8;
	s16 gx9;
	s16 gx10;
	s16 gx11;
	s16 gx12;
	s16 gx13;
	s16 gx14;
	s16 gx15;
	s16 gx16;
	s16 gx17;
	s16 gx18;
	s16 gx19;
	s16 gx20;
	s16 gx21;
	s16 gx22;
	s16 gx23;
	s16 gx24;
	s16 gx25;
	s16 gx26;
	s16 gx27;
	s16 gx28;
	s16 gx29;
	s16 gx30;
	s16 gx31;
	s16 gy0;
	s16 gy1;
	s16 gy2;
	s16 gy3;
	s16 gy4;
	s16 gy5;
	s16 gy6;
	s16 gy7;
	s16 gy8;
	s16 gy9;
	s16 gy10;
	s16 gy11;
	s16 gy12;
	s16 gy13;
	s16 gy14;
	s16 gy15;
	s16 gy16;
	s16 gy17;
	s16 gy18;
	s16 gy19;
	s16 gy20;
	s16 gy21;
	s16 gy22;
	s16 gy23;
	s16 gy24;
	s16 gy25;
	s16 gy26;
	s16 gy27;
	s16 gy28;
	s16 gy29;
	s16 gy30;
	s16 gy31;
	s16 gz0;
	s16 gz1;
	s16 gz2;
	s16 gz3;
	s16 gz4;
	s16 gz5;
	s16 gz6;
	s16 gz7;
	s16 gz8;
	s16 gz9;
	s16 gz10;
	s16 gz11;
	s16 gz12;
	s16 gz13;
	s16 gz14;
	s16 gz15;
	s16 gz16;
	s16 gz17;
	s16 gz18;
	s16 gz19;
	s16 gz20;
	s16 gz21;
	s16 gz22;
	s16 gz23;
	s16 gz24;
	s16 gz25;
	s16 gz26;
	s16 gz27;
	s16 gz28;
	s16 gz29;
	s16 gz30;
	s16 gz31;
	u64 ats0;
	u64 ats1;
	u64 ats2;
	u64 ats3;
	s32 ax0;
	s32 ax1;
	s32 ax2;
	s32 ax3;
	s32 ay0;
	s32 ay1;
	s32 ay2;
	s32 ay3;
	s32 az0;
	s32 az1;
	s32 az2;
	s32 az3;
	u64 mts0;
	u64 mts1;
	u64 mts2;
	u64 mts3;
	s16 mx0;
	s16 mx1;
	s16 mx2;
	s16 mx3;
	s16 my0;
	s16 my1;
	s16 my2;
	s16 my3;
	s16 mz0;
	s16 mz1;
	s16 mz2;
	s16 mz3;//368 bytes
};

int qvr_send_package_wrap(u8 *message, int msize, struct hid_device *hid);
void qvr_clear_def_parmeter(void);
void qvr_init(struct hid_device *hdev);
int qvr_input_init(void);
void qvr_input_remove(void);

#endif
