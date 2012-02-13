/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <mach/camera.h>
#include <mach/gpio.h>
#include "ov7692.h"

/*=============================================================
    SENSOR REGISTER DEFINES
==============================================================*/
#define Q8    0x00000100

/* Omnivision8810 product ID register address */
#define REG_OV7692_MODEL_ID_MSB                       0x0A
#define REG_OV7692_MODEL_ID_LSB                       0x0B

#define OV7692_MODEL_ID                       0x7692
/* Omnivision8810 product ID */

/* Time in milisecs for waiting for the sensor to reset */
#define OV7692_RESET_DELAY_MSECS    66
#define OV7692_DEFAULT_CLOCK_RATE   24000000
/* Registers*/

/* Color bar pattern selection */
#define OV7692_COLOR_BAR_PATTERN_SEL_REG     0x82
/* Color bar enabling control */
#define OV7692_COLOR_BAR_ENABLE_REG           0x601
/* Time in milisecs for waiting for the sensor to reset*/
#define OV7692_RESET_DELAY_MSECS    66

static int ov7692_pwdn_gpio;
static int ov7692_reset_gpio;


/*============================================================================
			DATA DECLARATIONS
============================================================================*/


static bool OV7692_CSI_CONFIG;
/* 816x612, 24MHz MCLK 96MHz PCLK */
uint32_t OV7692_FULL_SIZE_WIDTH        = 640;
uint32_t OV7692_FULL_SIZE_HEIGHT       = 480;

uint32_t OV7692_QTR_SIZE_WIDTH         = 640;
uint32_t OV7692_QTR_SIZE_HEIGHT        = 480;

uint32_t OV7692_HRZ_FULL_BLK_PIXELS    = 16;
uint32_t OV7692_VER_FULL_BLK_LINES     = 12;
uint32_t OV7692_HRZ_QTR_BLK_PIXELS     = 16;
uint32_t OV7692_VER_QTR_BLK_LINES      = 12;

struct ov7692_work_t {
	struct work_struct work;
};
static struct  ov7692_work_t *ov7692_sensorw;
static struct  i2c_client *ov7692_client;
struct ov7692_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;
	uint32_t sensormode;
	uint32_t fps_divider;        /* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;    /* init to 1 * 0x00000400 */
	uint32_t fps;
	int32_t  curr_lens_pos;
	uint32_t curr_step_pos;
	uint32_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint32_t total_lines_per_frame;
	enum ov7692_resolution_t prev_res;
	enum ov7692_resolution_t pict_res;
	enum ov7692_resolution_t curr_res;
	enum ov7692_test_mode_t  set_test;
	unsigned short imgaddr;
};
static struct ov7692_ctrl_t *ov7692_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(ov7692_wait_queue);
DEFINE_MUTEX(ov7692_mut);
static int effect_value;
static int16_t ov7692_effect = CAMERA_EFFECT_OFF;
static unsigned int SAT_U = 0x80;
static unsigned int SAT_V = 0x80;

/*=============================================================*/

static int ov7692_i2c_rxdata(unsigned short saddr,
		unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = 1,
			.buf   = rxdata,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = 1,
			.buf   = rxdata,
		},
	};
	if (i2c_transfer(ov7692_client->adapter, msgs, 2) < 0) {
		CDBG("ov7692_i2c_rxdata failed!\n");
		return -EIO;
	}
	return 0;
}
static int32_t ov7692_i2c_txdata(unsigned short saddr,
		unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = 2,
			.buf = txdata,
		},
	};
	if (i2c_transfer(ov7692_client->adapter, msg, 1) < 0) {
		CDBG("ov7692_i2c_txdata faild 0x%x\n", ov7692_client->addr);
		return -EIO;
	}

	return 0;
}

static int32_t ov7692_i2c_read(uint8_t raddr,
		uint8_t *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[1];
	if (!rdata)
		return -EIO;
	memset(buf, 0, sizeof(buf));
	buf[0] = raddr;
	rc = ov7692_i2c_rxdata(ov7692_client->addr >> 1, buf, rlen);
	if (rc < 0) {
		CDBG("ov7692_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = buf[0];
	return rc;
}
static int32_t ov7692_i2c_write_b_sensor(uint8_t waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));
	buf[0] = waddr;
	buf[1] = bdata;
	CDBG("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, bdata);
	rc = ov7692_i2c_txdata(ov7692_client->addr >> 1, buf, 2);
	if (rc < 0)
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
		waddr, bdata);

	return rc;
}

static int32_t OV7692_WritePRegs(struct OV7692_WREG *pTb, int32_t len)
{
	int32_t i, ret = 0;
	uint8_t regv;

	for (i = 0; i < len; i++) {
		if (pTb[i].mask == 0) {
			ov7692_i2c_write_b_sensor(pTb[i].addr, pTb[i].data);
		} else {
			ov7692_i2c_read(pTb[i].addr, &regv, 1);
			regv &= pTb[i].mask;
			regv |= (pTb[i].data & (~pTb[i].mask));
			ov7692_i2c_write_b_sensor(pTb[i].addr, regv);
		}
	}
	return ret;
}

static int32_t ov7692_sensor_setting(int update_type, int rt)
{
	int32_t i, array_length;
	int32_t rc = 0;
	struct msm_camera_csi_params ov7692_csi_params;

	CDBG("%s: rt = %d\n", __func__, rt);

	switch (update_type) {
	case REG_INIT:
		OV7692_CSI_CONFIG = 0;
		ov7692_i2c_write_b_sensor(0x0e, 0x08);
		return rc;
		break;
	case UPDATE_PERIODIC:
		if (!OV7692_CSI_CONFIG) {
			ov7692_csi_params.lane_cnt = 1;
			ov7692_csi_params.data_format = CSI_8BIT;
			ov7692_csi_params.lane_assign = 0xe4;
			ov7692_csi_params.dpcm_scheme = 0;
			ov7692_csi_params.settle_cnt = 0x14;

			array_length = sizeof(ov7692_init_settings_array) /
				sizeof(ov7692_init_settings_array[0]);
			for (i = 0; i < array_length; i++) {
				rc = ov7692_i2c_write_b_sensor(
				ov7692_init_settings_array[i].reg_addr,
				ov7692_init_settings_array[i].reg_val);
				if (rc < 0)
					return rc;
			}
			usleep_range(10000, 11000);
			rc = msm_camio_csi_config(&ov7692_csi_params);
			usleep_range(10000, 11000);
			ov7692_i2c_write_b_sensor(0x0e, 0x00);
			OV7692_CSI_CONFIG = 1;
			msleep(20);
			return rc;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int32_t ov7692_video_config(int mode)
{
	int32_t rc = 0;
	int rt;
	/* change sensor resolution if needed */
	rt = RES_PREVIEW;

	CDBG("%s\n", __func__);

	if (ov7692_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	ov7692_ctrl->curr_res = ov7692_ctrl->prev_res;
	ov7692_ctrl->sensormode = mode;
	return rc;
}

static int32_t ov7692_set_sensor_mode(int mode,
		int res)
{
	int32_t rc = 0;
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = ov7692_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int ov7692_set_exposure_compensation(int compensation)
{
	long rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...exposure_compensation = %d\n",
		 __func__ , compensation);
	switch (compensation) {
	case CAMERA_EXPOSURE_COMPENSATION_LV0:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV0\n");
		rc = OV7692Core_WritePREG(
			ov7692_exposure_compensation_lv0_tbl);
		break;
	case CAMERA_EXPOSURE_COMPENSATION_LV1:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV1\n");
		rc = OV7692Core_WritePREG(
			ov7692_exposure_compensation_lv1_tbl);
		break;
	case CAMERA_EXPOSURE_COMPENSATION_LV2:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV2\n");
		rc = OV7692Core_WritePREG(
			ov7692_exposure_compensation_lv2_default_tbl);
		break;
	case CAMERA_EXPOSURE_COMPENSATION_LV3:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
		rc = OV7692Core_WritePREG(
			ov7692_exposure_compensation_lv3_tbl);
		break;
	case CAMERA_EXPOSURE_COMPENSATION_LV4:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
		rc = OV7692Core_WritePREG(
			ov7692_exposure_compensation_lv4_tbl);
		break;
	default:
		CDBG("--CAMERA--ERROR CAMERA_EXPOSURE_COMPENSATION\n");
		break;
	}
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static long ov7692_set_antibanding(int antibanding)
{
	long rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...antibanding = %d\n", __func__, antibanding);
	switch (antibanding) {
	case CAMERA_ANTIBANDING_OFF:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_OFF\n");
		break;
	case CAMERA_ANTIBANDING_60HZ:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_60HZ\n");
		rc = OV7692Core_WritePREG(ov7692_antibanding_60z_tbl);
		break;
	case CAMERA_ANTIBANDING_50HZ:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_50HZ\n");
		rc = OV7692Core_WritePREG(ov7692_antibanding_50z_tbl);
		break;
	case CAMERA_ANTIBANDING_AUTO:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_AUTO\n");
		rc = OV7692Core_WritePREG(ov7692_antibanding_auto_tbl);
		break;
	default:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_ERROR COMMAND\n");
		break;
	}
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int ov7692_set_saturation(int saturation)
{
	long rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...saturation = %d\n", __func__ , saturation);

	if (effect_value == CAMERA_EFFECT_OFF) {
		switch (saturation) {
		case CAMERA_SATURATION_LV0:
			CDBG("--CAMERA--CAMERA_SATURATION_LV0\n");
			rc = OV7692Core_WritePREG(ov7692_saturation_lv0_tbl);
			break;
		case CAMERA_SATURATION_LV1:
			CDBG("--CAMERA--CAMERA_SATURATION_LV1\n");
			rc = OV7692Core_WritePREG(ov7692_saturation_lv1_tbl);
			break;
		case CAMERA_SATURATION_LV2:
			CDBG("--CAMERA--CAMERA_SATURATION_LV2\n");
			rc = OV7692Core_WritePREG(ov7692_saturation_lv2_tbl);
			break;
		case CAMERA_SATURATION_LV3:
			CDBG("--CAMERA--CAMERA_SATURATION_LV3\n");
			rc = OV7692Core_WritePREG(ov7692_saturation_lv3_tbl);
			break;
		case CAMERA_SATURATION_LV4:
			CDBG("--CAMERA--CAMERA_SATURATION_LV4\n");
			rc = OV7692Core_WritePREG(
				ov7692_saturation_default_lv4_tbl);
			break;
		case CAMERA_SATURATION_LV5:
			CDBG("--CAMERA--CAMERA_SATURATION_LV5\n");
			rc = OV7692Core_WritePREG(ov7692_saturation_lv5_tbl);
			break;
		case CAMERA_SATURATION_LV6:
			CDBG("--CAMERA--CAMERA_SATURATION_LV6\n");
			rc = OV7692Core_WritePREG(ov7692_saturation_lv6_tbl);
			break;
		case CAMERA_SATURATION_LV7:
			CDBG("--CAMERA--CAMERA_SATURATION_LV7\n");
			rc = OV7692Core_WritePREG(ov7692_saturation_lv7_tbl);
			break;
		case CAMERA_SATURATION_LV8:
			CDBG("--CAMERA--CAMERA_SATURATION_LV8\n");
			rc = OV7692Core_WritePREG(ov7692_saturation_lv8_tbl);
			break;
		default:
			CDBG("--CAMERA--CAMERA_SATURATION_ERROR COMMAND\n");
			break;
		}
	}

	/*for recover saturation level when change special effect*/
	switch (saturation) {
	case CAMERA_SATURATION_LV0:
		CDBG("--CAMERA--CAMERA_SATURATION_LV0\n");
		SAT_U = 0x00;
		SAT_V = 0x00;
		break;
	case CAMERA_SATURATION_LV1:
		CDBG("--CAMERA--CAMERA_SATURATION_LV1\n");
		SAT_U = 0x10;
		SAT_V = 0x10;
		break;
	case CAMERA_SATURATION_LV2:
		CDBG("--CAMERA--CAMERA_SATURATION_LV2\n");
		SAT_U = 0x20;
		SAT_V = 0x20;
		break;
	case CAMERA_SATURATION_LV3:
		CDBG("--CAMERA--CAMERA_SATURATION_LV3\n");
		SAT_U = 0x30;
		SAT_V = 0x30;
		break;
	case CAMERA_SATURATION_LV4:
		CDBG("--CAMERA--CAMERA_SATURATION_LV4\n");
		SAT_U = 0x40;
		SAT_V = 0x40;
		break;
	case CAMERA_SATURATION_LV5:
		CDBG("--CAMERA--CAMERA_SATURATION_LV5\n");
		SAT_U = 0x50;
		SAT_V = 0x50;
		break;
	case CAMERA_SATURATION_LV6:
		CDBG("--CAMERA--CAMERA_SATURATION_LV6\n");
		SAT_U = 0x60;
		SAT_V = 0x60;
		break;
	case CAMERA_SATURATION_LV7:
		CDBG("--CAMERA--CAMERA_SATURATION_LV7\n");
		SAT_U = 0x70;
		SAT_V = 0x70;
		break;
	case CAMERA_SATURATION_LV8:
		CDBG("--CAMERA--CAMERA_SATURATION_LV8\n");
		SAT_U = 0x80;
		SAT_V = 0x80;
		break;
	default:
		CDBG("--CAMERA--CAMERA_SATURATION_ERROR COMMAND\n");
		break;
	}
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static long ov7692_set_effect(int mode, int effect)
{
	int rc = 0;
	CDBG("--CAMERA-- %s ...(Start)\n", __func__);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		break;
	case SENSOR_HFR_60FPS_MODE:
		break;
	case SENSOR_HFR_90FPS_MODE:
		/* Context A Special Effects */
		CDBG("-CAMERA- %s ...SENSOR_PREVIEW_MODE\n", __func__);
		break;
	case SENSOR_SNAPSHOT_MODE:
		/* Context B Special Effects */
		CDBG("-CAMERA- %s ...SENSOR_SNAPSHOT_MODE\n", __func__);
		break;
	default:
		break;
	}
	effect_value = effect;
	switch (effect) {
	case CAMERA_EFFECT_OFF: {
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_OFF\n", __func__);
		rc = OV7692Core_WritePREG(ov7692_effect_normal_tbl);
		/* for recover saturation level
		 when change special effect*/
		ov7692_i2c_write_b_sensor(0xda, SAT_U);
		/* for recover saturation level
		when change special effect*/
		ov7692_i2c_write_b_sensor(0xdb, SAT_V);
		break;
	}
	case CAMERA_EFFECT_MONO: {
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_MONO\n", __func__);
		rc = OV7692Core_WritePREG(ov7692_effect_mono_tbl);
		break;
	}
	case CAMERA_EFFECT_BW: {
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_BW\n", __func__);
		rc = OV7692Core_WritePREG(ov7692_effect_bw_tbl);
		break;
	}
	case CAMERA_EFFECT_BLUISH: {
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_BLUISH\n", __func__);
		rc = OV7692Core_WritePREG(ov7692_effect_bluish_tbl);
		break;
	}
	case CAMERA_EFFECT_SOLARIZE: {
		CDBG("%s ...CAMERA_EFFECT_NEGATIVE(No Support)!\n", __func__);
		break;
	}
	case CAMERA_EFFECT_SEPIA: {
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_SEPIA\n", __func__);
		rc = OV7692Core_WritePREG(ov7692_effect_sepia_tbl);
		break;
	}
	case CAMERA_EFFECT_REDDISH: {
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_REDDISH\n", __func__);
		rc = OV7692Core_WritePREG(ov7692_effect_reddish_tbl);
		break;
	}
	case CAMERA_EFFECT_GREENISH: {
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_GREENISH\n", __func__);
		rc = OV7692Core_WritePREG(ov7692_effect_greenish_tbl);
		break;
	}
	case CAMERA_EFFECT_NEGATIVE: {
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_NEGATIVE\n", __func__);
		rc = OV7692Core_WritePREG(ov7692_effect_negative_tbl);
		break;
	}
	default: {
		CDBG("--CAMERA-- %s ...Default(Not Support)\n", __func__);
	}
	}
	ov7692_effect = effect;
	/*Refresh Sequencer */
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int ov7692_set_contrast(int contrast)
{
	int rc = 0;
	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...contrast = %d\n", __func__ , contrast);

	if (effect_value == CAMERA_EFFECT_OFF) {
		switch (contrast) {
		case CAMERA_CONTRAST_LV0:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV0\n");
			rc = OV7692Core_WritePREG(ov7692_contrast_lv0_tbl);
			break;
		case CAMERA_CONTRAST_LV1:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV1\n");
			rc = OV7692Core_WritePREG(ov7692_contrast_lv1_tbl);
			break;
		case CAMERA_CONTRAST_LV2:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV2\n");
			rc = OV7692Core_WritePREG(ov7692_contrast_lv2_tbl);
			break;
		case CAMERA_CONTRAST_LV3:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV3\n");
			rc = OV7692Core_WritePREG(ov7692_contrast_lv3_tbl);
			break;
		case CAMERA_CONTRAST_LV4:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV4\n");
			rc = OV7692Core_WritePREG(
				ov7692_contrast_default_lv4_tbl);
			break;
		case CAMERA_CONTRAST_LV5:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV5\n");
			rc = OV7692Core_WritePREG(ov7692_contrast_lv5_tbl);
			break;
		case CAMERA_CONTRAST_LV6:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV6\n");
			rc = OV7692Core_WritePREG(ov7692_contrast_lv6_tbl);
			break;
		case CAMERA_CONTRAST_LV7:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV7\n");
			rc = OV7692Core_WritePREG(ov7692_contrast_lv7_tbl);
			break;
		case CAMERA_CONTRAST_LV8:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV8\n");
			rc = OV7692Core_WritePREG(ov7692_contrast_lv8_tbl);
			break;
		default:
			CDBG("--CAMERA--CAMERA_CONTRAST_ERROR COMMAND\n");
			break;
		}
	}
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int ov7692_set_sharpness(int sharpness)
{
	int rc = 0;
	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...sharpness = %d\n", __func__ , sharpness);

	if (effect_value == CAMERA_EFFECT_OFF) {
		switch (sharpness) {
		case CAMERA_SHARPNESS_LV0:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV0\n");
			rc = OV7692Core_WritePREG(ov7692_sharpness_lv0_tbl);
			break;
		case CAMERA_SHARPNESS_LV1:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV1\n");
			rc = OV7692Core_WritePREG(ov7692_sharpness_lv1_tbl);
			break;
		case CAMERA_SHARPNESS_LV2:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV2\n");
			rc = OV7692Core_WritePREG(
				ov7692_sharpness_default_lv2_tbl);
			break;
		case CAMERA_SHARPNESS_LV3:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV3\n");
			rc = OV7692Core_WritePREG(ov7692_sharpness_lv3_tbl);
			break;
		case CAMERA_SHARPNESS_LV4:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV4\n");
			rc = OV7692Core_WritePREG(ov7692_sharpness_lv4_tbl);
			break;
		case CAMERA_SHARPNESS_LV5:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV5\n");
			rc = OV7692Core_WritePREG(ov7692_sharpness_lv5_tbl);
			break;
		case CAMERA_SHARPNESS_LV6:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV6\n");
			rc = OV7692Core_WritePREG(ov7692_sharpness_lv6_tbl);
			break;
		default:
			CDBG("--CAMERA--CAMERA_SHARPNESS_ERROR COMMAND\n");
			break;
		}
	}
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int ov7692_set_iso(int8_t iso_type)
{
	long rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...iso_type = %d\n", __func__ , iso_type);
	switch (iso_type) {
	case CAMERA_ISO_TYPE_AUTO:
		CDBG("--CAMERA--CAMERA_ISO_TYPE_AUTO\n");
		rc = OV7692Core_WritePREG(ov7692_iso_type_auto);
		break;
	case CAMEAR_ISO_TYPE_HJR:
		CDBG("--CAMERA--CAMEAR_ISO_TYPE_HJR\n");
		rc = OV7692Core_WritePREG(ov7692_iso_type_auto);
		break;
	case CAMEAR_ISO_TYPE_100:
		CDBG("--CAMERA--CAMEAR_ISO_TYPE_100\n");
		rc = OV7692Core_WritePREG(ov7692_iso_type_100);
		break;
	case CAMERA_ISO_TYPE_200:
		CDBG("--CAMERA--CAMERA_ISO_TYPE_200\n");
		rc = OV7692Core_WritePREG(ov7692_iso_type_200);
		break;
	case CAMERA_ISO_TYPE_400:
		CDBG("--CAMERA--CAMERA_ISO_TYPE_400\n");
		rc = OV7692Core_WritePREG(ov7692_iso_type_400);
		break;
	case CAMEAR_ISO_TYPE_800:
		CDBG("--CAMERA--CAMEAR_ISO_TYPE_800\n");
		rc = OV7692Core_WritePREG(ov7692_iso_type_800);
		break;
	case CAMERA_ISO_TYPE_1600:
		CDBG("--CAMERA--CAMERA_ISO_TYPE_1600\n");
		rc = OV7692Core_WritePREG(ov7692_iso_type_1600);
		break;
	default:
		CDBG("--CAMERA--ERROR ISO TYPE\n");
		break;
	}
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int ov7692_set_wb_oem(uint8_t param)
{
	int rc = 0;
	CDBG("--CAMERA--%s runs\r\n", __func__);

	switch (param) {
	case CAMERA_WB_AUTO:
		CDBG("--CAMERA--CAMERA_WB_AUTO\n");
		rc = OV7692Core_WritePREG(ov7692_wb_def);
		break;
	case CAMERA_WB_CUSTOM:
		CDBG("--CAMERA--CAMERA_WB_CUSTOM\n");
		rc = OV7692Core_WritePREG(ov7692_wb_custom);
		break;
	case CAMERA_WB_INCANDESCENT:
		CDBG("--CAMERA--CAMERA_WB_INCANDESCENT\n");
		rc = OV7692Core_WritePREG(ov7692_wb_inc);
		break;
	case CAMERA_WB_DAYLIGHT:
		CDBG("--CAMERA--CAMERA_WB_DAYLIGHT\n");
		rc = OV7692Core_WritePREG(ov7692_wb_daylight);
		break;
	case CAMERA_WB_CLOUDY_DAYLIGHT:
		CDBG("--CAMERA--CAMERA_WB_CLOUDY_DAYLIGHT\n");
		rc = OV7692Core_WritePREG(ov7692_wb_cloudy);
		break;
	default:
		break;
	}
	return rc;
}

static void ov7692_power_on(void)
{
	CDBG("%s\n", __func__);
	gpio_set_value(ov7692_pwdn_gpio, 0);
}

static void ov7692_power_down(void)
{
	CDBG("%s\n", __func__);
	gpio_set_value(ov7692_pwdn_gpio, 1);
}

static void ov7692_sw_reset(void)
{
	CDBG("%s\n", __func__);
	ov7692_i2c_write_b_sensor(0x12, 0x80);
}

static void ov7692_hw_reset(void)
{
	CDBG("--CAMERA-- %s ... (Start...)\n", __func__);
	gpio_set_value(ov7692_reset_gpio, 1);   /*reset camera reset pin*/
	usleep_range(5000, 5100);
	gpio_set_value(ov7692_reset_gpio, 0);
	usleep_range(5000, 5100);
	gpio_set_value(ov7692_reset_gpio, 1);
	usleep_range(1000, 1100);
	CDBG("--CAMERA-- %s ... (End...)\n", __func__);
}



static int ov7692_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	uint8_t model_id_msb, model_id_lsb = 0;
	uint16_t model_id = 0;
	int32_t rc = 0;
	/*The reset pin is not physically connected to the sensor.
	  The standby pin will do the reset hence there is no need
	  to request the gpio reset*/

	/* Read sensor Model ID: */
	rc = ov7692_i2c_read(REG_OV7692_MODEL_ID_MSB, &model_id_msb, 1);
	if (rc < 0)
		goto init_probe_fail;
	rc = ov7692_i2c_read(REG_OV7692_MODEL_ID_LSB, &model_id_lsb, 1);
	if (rc < 0)
		goto init_probe_fail;
	model_id = (model_id_msb << 8) | ((model_id_lsb & 0x00FF)) ;
	CDBG("ov7692 model_id = 0x%x, 0x%x, 0x%x\n",
			model_id, model_id_msb, model_id_lsb);
	/* 4. Compare sensor ID to OV7692 ID: */
	if (model_id != OV7692_MODEL_ID) {
		rc = -ENODEV;
		goto init_probe_fail;
	}
	goto init_probe_done;
init_probe_fail:
	pr_warning(" ov7692_probe_init_sensor fails\n");
init_probe_done:
	CDBG(" ov7692_probe_init_sensor finishes\n");
	return rc;
}

int ov7692_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;

	CDBG("%s: %d\n", __func__, __LINE__);
	CDBG("Calling ov7692_sensor_open_init\n");
	ov7692_ctrl = kzalloc(sizeof(struct ov7692_ctrl_t), GFP_KERNEL);
	if (!ov7692_ctrl) {
		CDBG("ov7692_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	ov7692_ctrl->fps_divider = 1 * 0x00000400;
	ov7692_ctrl->pict_fps_divider = 1 * 0x00000400;
	ov7692_ctrl->fps = 30 * Q8;
	ov7692_ctrl->set_test = TEST_OFF;
	ov7692_ctrl->prev_res = QTR_SIZE;
	ov7692_ctrl->pict_res = FULL_SIZE;
	ov7692_ctrl->curr_res = INVALID_SIZE;

	if (data)
		ov7692_ctrl->sensordata = data;
	/* turn on LDO for PVT */
	if (data->pmic_gpio_enable)
		lcd_camera_power_onoff(1);

	/* enable mclk first */

	msm_camio_clk_rate_set(24000000);
	msleep(20);

	ov7692_power_on();
	usleep_range(5000, 5100);

	rc = ov7692_probe_init_sensor(data);
	if (rc < 0) {
		CDBG("Calling ov7692_sensor_open_init fail\n");
		goto init_fail;
	}

	rc = ov7692_sensor_setting(REG_INIT, RES_PREVIEW);
	if (rc < 0)
		goto init_fail;
	else
		goto init_done;

init_fail:
	CDBG(" ov7692_sensor_open_init fail\n");
	if (data->pmic_gpio_enable)
		lcd_camera_power_onoff(0);
	kfree(ov7692_ctrl);
init_done:
	CDBG("ov7692_sensor_open_init done\n");
	return rc;
}

static int ov7692_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&ov7692_wait_queue);
	return 0;
}

static const struct i2c_device_id ov7692_i2c_id[] = {
	{"ov7692", 0},
	{ }
};

static int ov7692_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("ov7692_i2c_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	ov7692_sensorw = kzalloc(sizeof(struct ov7692_work_t), GFP_KERNEL);
	if (!ov7692_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, ov7692_sensorw);
	ov7692_init_client(client);
	ov7692_client = client;

	CDBG("ov7692_i2c_probe success! rc = %d\n", rc);
	return 0;

probe_failure:
	CDBG("ov7692_i2c_probe failed! rc = %d\n", rc);
	return rc;
}

static int __exit ov7692_remove(struct i2c_client *client)
{
	struct ov7692_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	ov7692_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver ov7692_i2c_driver = {
	.id_table = ov7692_i2c_id,
	.probe  = ov7692_i2c_probe,
	.remove = __exit_p(ov7692_i2c_remove),
	.driver = {
		.name = "ov7692",
	},
};

int ov7692_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&ov7692_mut);
	CDBG("ov7692_sensor_config: cfgtype = %d\n", cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_SET_MODE:
		rc = ov7692_set_sensor_mode(cdata.mode, cdata.rs);
		break;
	case CFG_SET_EFFECT:
		CDBG("--CAMERA-- CFG_SET_EFFECT mode=%d, effect = %d !!\n",
			 cdata.mode, cdata.cfg.effect);
		rc = ov7692_set_effect(cdata.mode, cdata.cfg.effect);
		break;
	case CFG_START:
		CDBG("--CAMERA-- CFG_START (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_PWR_UP:
		CDBG("--CAMERA-- CFG_PWR_UP (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_PWR_DOWN:
		CDBG("--CAMERA-- CFG_PWR_DOWN !!\n");
		ov7692_power_down();
		break;
	case CFG_WRITE_EXPOSURE_GAIN:
		CDBG("--CAMERA-- CFG_WRITE_EXPOSURE_GAIN (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_DEFAULT_FOCUS:
		CDBG("--CAMERA-- CFG_SET_DEFAULT_FOCUS (Not Implement) !!\n");
		break;
	case CFG_MOVE_FOCUS:
		CDBG("--CAMERA-- CFG_MOVE_FOCUS (Not Implement) !!\n");
		break;
	case CFG_REGISTER_TO_REAL_GAIN:
		CDBG("--CAMERA-- CFG_REGISTER_TO_REAL_GAIN (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_REAL_TO_REGISTER_GAIN:
		CDBG("--CAMERA-- CFG_REAL_TO_REGISTER_GAIN (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_FPS:
		CDBG("--CAMERA-- CFG_SET_FPS (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_PICT_FPS:
		CDBG("--CAMERA-- CFG_SET_PICT_FPS (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_BRIGHTNESS:
		CDBG("--CAMERA-- CFG_SET_BRIGHTNESS  !!\n");
		/* rc = ov7692_set_brightness(cdata.cfg.brightness); */
		break;
	case CFG_SET_CONTRAST:
		CDBG("--CAMERA-- CFG_SET_CONTRAST  !!\n");
		rc = ov7692_set_contrast(cdata.cfg.contrast);
		break;
	case CFG_SET_ZOOM:
		CDBG("--CAMERA-- CFG_SET_ZOOM (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_EXPOSURE_MODE:
		CDBG("--CAMERA-- CFG_SET_EXPOSURE_MODE !!\n");
		/* rc = ov7692_set_exposure_mode(cdata.cfg.ae_mode); */
		break;
	case CFG_SET_WB:
		CDBG("--CAMERA-- CFG_SET_WB!!\n");
		ov7692_set_wb_oem(cdata.cfg.wb_val);
		rc = 0 ;
		break;
	case CFG_SET_ANTIBANDING:
		CDBG("--CAMERA-- CFG_SET_ANTIBANDING antibanding = %d !!\n",
			 cdata.cfg.antibanding);
		rc = ov7692_set_antibanding(cdata.cfg.antibanding);
		break;
	case CFG_SET_EXP_GAIN:
		CDBG("--CAMERA-- CFG_SET_EXP_GAIN (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_PICT_EXP_GAIN:
		CDBG("--CAMERA-- CFG_SET_PICT_EXP_GAIN (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_LENS_SHADING:
		CDBG("--CAMERA-- CFG_SET_LENS_SHADING !!\n");
		/* rc = ov7692_lens_shading_enable(cdata.cfg.lens_shading); */
		break;
	case CFG_GET_PICT_FPS:
		CDBG("--CAMERA-- CFG_GET_PICT_FPS (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_GET_PREV_L_PF:
		CDBG("--CAMERA-- CFG_GET_PREV_L_PF (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_GET_PREV_P_PL:
		CDBG("--CAMERA-- CFG_GET_PREV_P_PL (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_GET_PICT_L_PF:
		CDBG("--CAMERA-- CFG_GET_PICT_L_PF (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_GET_PICT_P_PL:
		CDBG("--CAMERA-- CFG_GET_PICT_P_PL (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_GET_AF_MAX_STEPS:
		CDBG("--CAMERA-- CFG_GET_AF_MAX_STEPS (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_GET_PICT_MAX_EXP_LC:
		CDBG("--CAMERA-- CFG_GET_PICT_MAX_EXP_LC (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SEND_WB_INFO:
		CDBG("--CAMERA-- CFG_SEND_WB_INFO (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SENSOR_INIT:
		CDBG("--CAMERA-- CFG_SENSOR_INIT (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_SATURATION:
		CDBG("--CAMERA-- CFG_SET_SATURATION !!\n");
		rc = ov7692_set_saturation(cdata.cfg.saturation);
		break;
	case CFG_SET_SHARPNESS:
		CDBG("--CAMERA-- CFG_SET_SHARPNESS !!\n");
		rc = ov7692_set_sharpness(cdata.cfg.sharpness);
		break;
	case CFG_SET_TOUCHAEC:
		CDBG("--CAMERA-- CFG_SET_TOUCHAEC!!\n");
		/* ov7692_set_touchaec(cdata.cfg.aec_cord.x,
			 cdata.cfg.aec_cord.y); */
		rc = 0 ;
		break;
	case CFG_SET_AUTO_FOCUS:
		CDBG("--CAMERA-- CFG_SET_AUTO_FOCUS (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_AUTOFLASH:
		CDBG("--CAMERA-- CFG_SET_AUTOFLASH (Not Support) !!\n");
		/* Not Support */
		break;
	case CFG_SET_EXPOSURE_COMPENSATION:
		CDBG("--CAMERA-- CFG_SET_EXPOSURE_COMPENSATION !\n");
		rc = ov7692_set_exposure_compensation(
			cdata.cfg.exp_compensation);
		break;
	case CFG_SET_ISO:
		CDBG("--CAMERA-- CFG_SET_ISO !\n");
		rc = ov7692_set_iso(cdata.cfg.iso_type);
		break;
	default:
		CDBG("--CAMERA-- %s: Command=%d (Not Implement) !!\n",
			 __func__, cdata.cfgtype);
		rc = -EINVAL;
		break;
	}

	mutex_unlock(&ov7692_mut);

	return rc;
}
static int ov7692_sensor_release(void)
{
	int rc = -EBADF;

	mutex_lock(&ov7692_mut);
	ov7692_sw_reset();
	ov7692_power_down();
	kfree(ov7692_ctrl);
	ov7692_ctrl = NULL;
	CDBG("ov7692_release completed\n");
	mutex_unlock(&ov7692_mut);

	return rc;
}

static int ov7692_probe_init_gpio(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	ov7692_pwdn_gpio = data->sensor_pwd;
	ov7692_reset_gpio = data->sensor_reset ;

	if (data->sensor_reset_enable)
		gpio_direction_output(data->sensor_reset, 1);

	gpio_direction_output(data->sensor_pwd, 1);

	return rc;

}


static int ov7692_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = i2c_add_driver(&ov7692_i2c_driver);
	if (rc < 0 || ov7692_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_fail;
	}
	pr_debug("%s: %d Entered\n", __func__, __LINE__);
	rc = ov7692_probe_init_gpio(info);
	if (rc < 0) {
		CDBG("%s: gpio init failed\n", __func__);
		goto probe_fail;
	}
	/* turn on LDO for PVT */
	if (info->pmic_gpio_enable)
		lcd_camera_power_onoff(1);

	ov7692_power_down();

	msm_camio_clk_rate_set(24000000);
	usleep_range(5000, 5100);

	ov7692_power_on();
	usleep_range(5000, 5100);

	if (info->sensor_reset_enable)
		ov7692_hw_reset();
	else
		ov7692_sw_reset();

	rc = ov7692_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;


	s->s_init = ov7692_sensor_open_init;
	s->s_release = ov7692_sensor_release;
	s->s_config  = ov7692_sensor_config;
	s->s_camera_type = FRONT_CAMERA_2D;
	s->s_mount_angle = info->sensor_platform_info->mount_angle;

	/* ov7692_sw_reset(); */
	ov7692_power_down();

	if (info->pmic_gpio_enable)
		lcd_camera_power_onoff(0);

	return rc;

probe_fail:
	CDBG("ov7692_sensor_probe: SENSOR PROBE FAILS!\n");
	if (info->pmic_gpio_enable)
		lcd_camera_power_onoff(0);
	i2c_del_driver(&ov7692_i2c_driver);
	return rc;
}

static int __ov7692_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, ov7692_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov7692_probe,
	.driver = {
		.name = "msm_camera_ov7692",
		.owner = THIS_MODULE,
	},
};

static int __init ov7692_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(ov7692_init);

MODULE_DESCRIPTION("OMNI VGA YUV sensor driver");
MODULE_LICENSE("GPL v2");
