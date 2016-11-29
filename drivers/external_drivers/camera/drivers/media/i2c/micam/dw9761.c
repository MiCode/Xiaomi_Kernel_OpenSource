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

#include <linux/delay.h>
#include "dw9761.h"

static struct dw9761_device dw9761_dev;

static u16 crc16table[] = {
	0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
	0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
	0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
	0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
	0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
	0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
	0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
	0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
	0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
	0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
	0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
	0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
	0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
	0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
	0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
	0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
	0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
	0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
	0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
	0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
	0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
	0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
	0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
	0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
	0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
	0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
	0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
	0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
	0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
	0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
	0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
	0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};


static int dw9761_i2c_rd8(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2] = { reg };

	msg[0].addr = DW9761_VCM_ADDR;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;

	msg[1].addr = DW9761_VCM_ADDR;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[1];
	*val = 0;

	if (i2c_transfer(client->adapter, msg, 2) != 2)
		return -EIO;
	*val = buf[1];

	return 0;
}

static int dw9761_i2c_wr8(struct i2c_client *client, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2] = { reg, val };

	msg.addr = DW9761_VCM_ADDR;
	msg.flags = 0;
	msg.len = sizeof(buf);
	msg.buf = buf;

	if (i2c_transfer(client->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}

static int dw9761_i2c_wr16(struct i2c_client *client, u8 reg, u16 val)
{
	struct i2c_msg msg;
	u8 buf[3] = { reg, (u8)(val >> 8), (u8)(val & 0xff)};

	msg.addr = DW9761_VCM_ADDR;
	msg.flags = 0;
	msg.len = sizeof(buf);
	msg.buf = buf;

	if (i2c_transfer(client->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}


int dw9761_vcm_power_up(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 value;

	/* Enable power */
	if (dw9761_dev.platform_data) {
		ret = dw9761_dev.platform_data->power_ctrl(sd, 1);
		/* waiting time requested by DW9761B(vcm) */
		if (ret)
			return ret;
	}
	/* Wait for VBAT to stabilize */
	udelay(1);
	/* Detect device */
	ret = dw9761_i2c_rd8(client, DW9761_INFO, &value);
	if (ret < 0)
		goto fail_powerdown;
	if (value != DW9761_ID) {
		ret = -ENXIO;
		goto fail_powerdown;
	}
	/*
	 * Jiggle SCL pin to wake up device.
	 */
	ret = dw9761_i2c_wr8(client, DW9761_CONTROL, 1);
	usleep_range(100, 1000);
	ret = dw9761_i2c_wr8(client, DW9761_CONTROL, 0);
	/* Need 100us to transit from SHUTDOWN to STANDBY*/
	usleep_range(100, 1000);

	/* Enable the ringing compensation */
	ret = dw9761_i2c_wr8(client, DW9761_CONTROL, DW9761_ENABLE_RINGING);
	if (ret < 0)
		goto fail_powerdown;

	/* Use SAC3 mode */
	ret = dw9761_i2c_wr8(client, DW9761_MODE, DW9761_MODE_SAC3);
	if (ret < 0)
		goto fail_powerdown;

	/* Set the resonance frequency */
	ret = dw9761_i2c_wr8(client, DW9761_VCM_FREQ, DW9761_DEFAULT_VCM_FREQ);
	if (ret < 0)
		goto fail_powerdown;

	ret = dw9761_i2c_wr8(client, DW9761_VCM_PRELOAD, 115);
	if (ret < 0)
		goto fail_powerdown;

	dw9761_dev.focus = 230;
	dw9761_dev.initialized = true;

	return 0;

fail_powerdown:
	if (dw9761_dev.platform_data)
		dw9761_dev.platform_data->power_ctrl(sd, 0);
	return ret;
}

int dw9761_vcm_power_down(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 step;

	step = dw9761_dev.focus/4;

	ret = dw9761_i2c_wr16(client,
			DW9761_VCM_CURRENT, dw9761_dev.focus - step);
	msleep(50);
	ret = dw9761_i2c_wr16(client,
			DW9761_VCM_CURRENT, dw9761_dev.focus - 2*step);
	msleep(50);
	ret = dw9761_i2c_wr16(client,
			DW9761_VCM_CURRENT, dw9761_dev.focus - 3*step);
	msleep(50);
	ret = dw9761_i2c_wr16(client,
			DW9761_VCM_CURRENT, dw9761_dev.focus - 4*step);
	msleep(200);
	if (ret < 0)
		return ret;

	ret = dw9761_i2c_wr8(client, DW9761_CONTROL, 1);
	if (ret < 0)
		return ret;

	if (dw9761_dev.platform_data)
		dw9761_dev.platform_data->power_ctrl(sd, 0);
	return 0;
}

int dw9761_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	static const struct timespec move_time = {

		.tv_sec = 0,
		.tv_nsec = 60000000
	};
	struct timespec current_time, finish_time, delta_time;

	getnstimeofday(&current_time);
	finish_time = timespec_add(dw9761_dev.focus_time, move_time);
	delta_time = timespec_sub(current_time, finish_time);
	if (delta_time.tv_sec >= 0 && delta_time.tv_nsec >= 0) {
		*value = ATOMISP_FOCUS_HP_COMPLETE |
			 ATOMISP_FOCUS_STATUS_ACCEPTS_NEW_MOVE;
	} else {
		*value = ATOMISP_FOCUS_STATUS_MOVING |
			 ATOMISP_FOCUS_HP_IN_PROGRESS;
	}
	return 0;
}

int dw9761_t_focus_vcm(struct v4l2_subdev *sd, u16 val)
{
	return -EINVAL;
}

int dw9761_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 VBUSY;

	ret = dw9761_i2c_rd8(client, DW9761_STATUS, &VBUSY);
	VBUSY = VBUSY & 0x01;
	if (VBUSY == 1) {
		msleep(100);
		ret = dw9761_i2c_rd8(client, DW9761_STATUS, &VBUSY);
		VBUSY = VBUSY & 0x01;
		if (VBUSY == 1) {
			dev_err(&client->dev, "dw9761_t_focus_abs error VBUSY is 0x1\n");
			return 1;
		}
	}

	value = clamp(value, 0, DW9761_MAX_FOCUS_POS);
	ret = dw9761_i2c_wr16(client, DW9761_VCM_CURRENT, value);
	if (ret < 0)
		return ret;

	getnstimeofday(&dw9761_dev.focus_time);
	dw9761_dev.focus = value;

	return 0;
}

int dw9761_t_focus_rel(struct v4l2_subdev *sd, s32 value)
{
	return dw9761_t_focus_abs(sd, dw9761_dev.focus + value);
}

int dw9761_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	*value = dw9761_dev.focus;
	return 0;
}
int dw9761_t_vcm_slew(struct v4l2_subdev *sd, s32 value)
{
	return 0;
}

int dw9761_t_vcm_timing(struct v4l2_subdev *sd, s32 value)
{
	return 0;
}

int dw9761_vcm_init(struct v4l2_subdev *sd)
{
	dw9761_dev.platform_data = camera_get_af_platform_data();
	return (NULL == dw9761_dev.platform_data) ? -ENODEV : 0;
}

static int dw9761_otp_CRC16CheckSum(u8 *pData, u32 Size)
{
	u16 crc = 0;
	u32 i;
	u8 index;

	for (i = 0; i < Size; i++) {
		index = (u8)(crc ^ pData[i]);
		crc = (crc >> 8) ^ crc16table[index];
		}

	return crc;
}

static int dw9761_otp_group_checksum(u8 *pData, u32 Size)
{
	u32 i;
	u16 Sum = 0;
	u8 CheckSum;

	for (i = 0; i < Size; i++)
		Sum += pData[i];

	CheckSum = Sum%255;
	return CheckSum;
}
static int dw9761_otp_format(struct v4l2_subdev *sd,
							u8 *pRawOTPData,
							u32 RawOTPSize,
							u8 *pOTPData,
							u32 OTPSize)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 GroupCheckSum;
	u8 size;

	/*module information*/
	GroupCheckSum = dw9761_otp_group_checksum(
			&pRawOTPData[DW9761_OTP_MODULE_INFO_START + 1],
			DW9761_OTP_MODULE_INFO_SIZE - 2);

	if (GroupCheckSum != pRawOTPData[DW9761_OTP_AF_START - 1]) {
		dev_err(&client->dev, "invalid otp MODULE_INFO checksum error\n");
		return 1;
	}

	if (DATA_VALID != pRawOTPData[DW9761_OTP_MODULE_INFO_START]) {
		dev_err(&client->dev, "invalid otp MODULE_INFO\n");
		return 1;
	}

	dw9761_dev.ModuleVendor = pRawOTPData[DW9761_OTP_MODULE_INFO_START + 1];
	dev_info(&client->dev, "module build on 20%2d.%2d.%2d/%2d:%2d:%2d\n",
		pRawOTPData[DW9761_OTP_MODULE_INFO_START + 5],
		pRawOTPData[DW9761_OTP_MODULE_INFO_START + 6],
		pRawOTPData[DW9761_OTP_MODULE_INFO_START + 7],
		pRawOTPData[DW9761_OTP_MODULE_INFO_START + 8],
		pRawOTPData[DW9761_OTP_MODULE_INFO_START + 9],
		pRawOTPData[DW9761_OTP_MODULE_INFO_START + 10]);
	/*AF */
	GroupCheckSum = dw9761_otp_group_checksum(
				&pRawOTPData[DW9761_OTP_AF_START + 1],
				DW9761_OTP_AF_SIZE - 2);

	if (GroupCheckSum != pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START - 1]) {
		dev_err(&client->dev, "invalid otp AF checksum error\n");
		return 1;
	}
	if (DATA_VALID != pRawOTPData[DW9761_OTP_AF_START]) {
		dev_err(&client->dev, "invalid otp AF\n");
		return 1;
	}
	pOTPData[INTEL_OTP_AF_OFFSET] = 1;/*number of position*/
	pOTPData[INTEL_OTP_AF_OFFSET+1] = pRawOTPData[DW9761_OTP_AF_START+1];
	pOTPData[INTEL_OTP_AF_OFFSET+2] = pRawOTPData[DW9761_OTP_AF_START+2];
	/* AF macro*/
	pOTPData[INTEL_OTP_AF_OFFSET+3] = pRawOTPData[DW9761_OTP_AF_START+6];
	pOTPData[INTEL_OTP_AF_OFFSET+4] = pRawOTPData[DW9761_OTP_AF_START+5];
	/* AF infinity*/
	pOTPData[INTEL_OTP_AF_OFFSET+5] = pRawOTPData[DW9761_OTP_AF_START+4];
	pOTPData[INTEL_OTP_AF_OFFSET+6] = pRawOTPData[DW9761_OTP_AF_START+3];
	/* AF start  = inf*/
	pOTPData[INTEL_OTP_AF_OFFSET+7] = pOTPData[INTEL_OTP_AF_OFFSET+5];
	pOTPData[INTEL_OTP_AF_OFFSET+8] = pOTPData[INTEL_OTP_AF_OFFSET+6];
	/* AF end = macro*/
	pOTPData[INTEL_OTP_AF_OFFSET+9] = pOTPData[INTEL_OTP_AF_OFFSET+3];
	pOTPData[INTEL_OTP_AF_OFFSET+10] = pOTPData[INTEL_OTP_AF_OFFSET+4];

	/*LS1_AWB_LSC*/
	GroupCheckSum = dw9761_otp_group_checksum(
				&pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START + 1],
				DW9761_OTP_LS1_AWB_LSC_SIZE - 2);

	if (GroupCheckSum != pRawOTPData[DW9761_OTP_LS2_AWB_LSC_START - 1]) {
		dev_err(&client->dev, "invalid otp LS1_AWB_LSC checksum error\n");
		return 1;
	}
	if (DATA_VALID != pRawOTPData[DW9761_OTP_LS2_AWB_LSC_START]) {
		dev_err(&client->dev, "invalid otp AWB_LSC1\n");
		return 1;
	}
	pOTPData[INTEL_OTP_VER_OFFSET] =
			pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START + 1];
	pOTPData[INTEL_OTP_VER_OFFSET + 1] =
			pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START + 2];
	/* number of lightsource*/
	pOTPData[INTEL_OTP_LS_OFFSET] =
			pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START + 3];
	/* LS coordinate X1 Y1*/
	pOTPData[INTEL_OTP_LS_OFFSET + 1] =
		pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START + 4];
	pOTPData[INTEL_OTP_LS_OFFSET + 3] =
		pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START + 5];
	/* grid size*/
	pOTPData[INTEL_OTP_LSGRID_OFFSET] =
		pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START + 6];
	pOTPData[INTEL_OTP_LSGRID_OFFSET + 1] =
		pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START + 7];
	/*copy LSC table*/
	size = pOTPData[INTEL_OTP_LSGRID_OFFSET] *
		pOTPData[INTEL_OTP_LSGRID_OFFSET+1] * sizeof(u8) * 4 + 1;
	memcpy(&pOTPData[INTEL_OTP_LS1_LSC_OFFSET],
			&pRawOTPData[DW9761_OTP_LS1_AWB_LSC_START + 8], size);
	/*copy AWB LS1*/
	pOTPData[INTEL_OTP_LS1_AWB_OFFSET] =
		pRawOTPData[DW9761_OTP_LS1_AWB_OFFSET];
	pOTPData[INTEL_OTP_LS1_AWB_OFFSET + 1] =
		pRawOTPData[DW9761_OTP_LS1_AWB_OFFSET + 1];
	pOTPData[INTEL_OTP_LS1_AWB_OFFSET + 4] =
		pRawOTPData[DW9761_OTP_LS1_AWB_OFFSET + 2];
	pOTPData[INTEL_OTP_LS1_AWB_OFFSET + 5] =
		pRawOTPData[DW9761_OTP_LS1_AWB_OFFSET + 3];
	pOTPData[INTEL_OTP_LS1_AWB_OFFSET + 8] =
		pRawOTPData[DW9761_OTP_LS1_AWB_OFFSET + 4];
	pOTPData[INTEL_OTP_LS1_AWB_OFFSET + 9] =
		pRawOTPData[DW9761_OTP_LS1_AWB_OFFSET + 5];
	pOTPData[INTEL_OTP_LS1_AWB_OFFSET + 12] =
		pRawOTPData[DW9761_OTP_LS1_AWB_OFFSET + 6];
	pOTPData[INTEL_OTP_LS1_AWB_OFFSET + 13] =
		pRawOTPData[DW9761_OTP_LS1_AWB_OFFSET + 7];

	/*LS2_AWB_LSC*/
	GroupCheckSum = dw9761_otp_group_checksum(
				&pRawOTPData[DW9761_OTP_LS2_AWB_LSC_START + 1],
				DW9761_OTP_LS2_AWB_LSC_SIZE - 2);
	if (GroupCheckSum != pRawOTPData[DW9761_OTP_RAW_SIZE - 1]) {
		dev_err(&client->dev, "invalid otp LS2_AWB_LSC checksum error\n");
		return 1;
	}
	if (DATA_VALID != pRawOTPData[DW9761_OTP_LS2_AWB_LSC_START]) {
		dev_err(&client->dev, "invalid otp AWB_LSC2\n");
		return 1;
	}

	/* LS coordinate X2 Y2*/
	pOTPData[INTEL_OTP_LS_OFFSET + 2] =
		pRawOTPData[DW9761_OTP_LS2_AWB_LSC_START + 4];
	pOTPData[INTEL_OTP_LS_OFFSET + 4] =
		pRawOTPData[DW9761_OTP_LS2_AWB_LSC_START + 5];

	/*copy LSC table*/
	size = pOTPData[INTEL_OTP_LSGRID_OFFSET] *
		pOTPData[INTEL_OTP_LSGRID_OFFSET+1] * sizeof(u8) * 4 + 1;
	memcpy(&pOTPData[INTEL_OTP_LS2_LSC_OFFSET],
		&pRawOTPData[DW9761_OTP_LS2_AWB_LSC_START + 8], size);
	/*copy AWB LS2*/
	pOTPData[INTEL_OTP_LS2_AWB_OFFSET] =
		pRawOTPData[DW9761_OTP_LS2_AWB_OFFSET];
	pOTPData[INTEL_OTP_LS2_AWB_OFFSET + 1] =
		pRawOTPData[DW9761_OTP_LS2_AWB_OFFSET + 1];
	pOTPData[INTEL_OTP_LS2_AWB_OFFSET + 4] =
		pRawOTPData[DW9761_OTP_LS2_AWB_OFFSET + 2];
	pOTPData[INTEL_OTP_LS2_AWB_OFFSET + 5] =
		pRawOTPData[DW9761_OTP_LS2_AWB_OFFSET + 3];
	pOTPData[INTEL_OTP_LS2_AWB_OFFSET + 8] =
		pRawOTPData[DW9761_OTP_LS2_AWB_OFFSET + 4];
	pOTPData[INTEL_OTP_LS2_AWB_OFFSET + 9] =
		pRawOTPData[DW9761_OTP_LS2_AWB_OFFSET + 5];
	pOTPData[INTEL_OTP_LS2_AWB_OFFSET + 12] =
		pRawOTPData[DW9761_OTP_LS2_AWB_OFFSET + 6];
	pOTPData[INTEL_OTP_LS2_AWB_OFFSET + 13] =
		pRawOTPData[DW9761_OTP_LS2_AWB_OFFSET + 7];

	*(u16 *)(&pOTPData[INTEL_OTP_CHECKSUM_OFFSET]) =
		dw9761_otp_CRC16CheckSum(pOTPData, 542);
	dev_info(&client->dev, "OTP check sum = 0x%x\n",
				*(u16 *)(&pOTPData[INTEL_OTP_CHECKSUM_OFFSET]));
	dev_info(&client->dev, "OTP format done\n");

	return 0;
}

void *dw9761_otp_read(struct v4l2_subdev *sd, u8 **rawotp, u8 *vendorid)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int addr = 0x0000;
	unsigned char *buffer;
	unsigned char *parsedbuffer;
	struct i2c_msg msg[2];
	unsigned int i2c_addr = DW9761_OTP_ADDR;
	u32 size = DW9761_OTP_RAW_SIZE;
	u16 addr_buf;
	int r;

	dev_info(&client->dev, "enter dw9761_otp_read\n");

	buffer = devm_kzalloc(&client->dev, size, GFP_KERNEL);
	if (!buffer)
		return NULL;
	parsedbuffer = devm_kzalloc(&client->dev,
					DEFAULT_DW9761_OTP_SIZE, GFP_KERNEL);
	if (!parsedbuffer)
		return NULL;

	msg[0].flags = 0;
	msg[0].addr = i2c_addr;
	addr_buf = cpu_to_be16(addr & 0xFFFF);
	msg[0].len = 2;
	msg[0].buf = (u8 *)&addr_buf;
	msg[1].addr = i2c_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = buffer;

	r = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (r != ARRAY_SIZE(msg)) {
		kfree(buffer);
		dev_err(&client->dev, "read otp failed size = %d\n", r);
		return NULL;
	}

	r = dw9761_otp_format(sd, buffer, DW9761_OTP_RAW_SIZE,
				parsedbuffer, DEFAULT_DW9761_OTP_SIZE);
	if (r != 0) {
		devm_kfree(&client->dev, buffer);
		dev_err(&client->dev, "dw9761_otp_format error\n");
		return NULL;
	}
	*rawotp = buffer;
	*vendorid = dw9761_dev.ModuleVendor;

	return parsedbuffer;
}

int dw9761_otp_save(u8 *pData, u32 size, const u8 *filp_name)
{
	struct file *fp = NULL;
	mm_segment_t fs;
	loff_t pos;

	fp = filp_open(filp_name, O_CREAT|O_RDWR, 0644);
	if (IS_ERR(fp))
		return -EPERM;

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, pData, size, &pos);
	set_fs(fs);

	filp_close(fp, NULL);

	return 0;
}


