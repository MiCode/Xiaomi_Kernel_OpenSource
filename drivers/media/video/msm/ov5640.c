/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

/* #define DEBUG */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "ov5640.h"

#define FALSE 0
#define TRUE 1

struct ov5640_work {
	struct work_struct work;
};

struct __ov5640_ctrl {
	const struct msm_camera_sensor_info *sensordata;
	int sensormode;
	uint fps_divider; /* init to 1 * 0x00000400 */
	uint pict_fps_divider; /* init to 1 * 0x00000400 */
	u16 curr_step_pos;
	u16 curr_lens_pos;
	u16 init_curr_lens_pos;
	u16 my_reg_gain;
	u16 my_reg_line_count;
	enum msm_s_resolution prev_res;
	enum msm_s_resolution pict_res;
	enum msm_s_resolution curr_res;
	enum msm_s_test_mode  set_test;
};

static DECLARE_WAIT_QUEUE_HEAD(ov5640_wait_queue);
DEFINE_MUTEX(ov5640_mutex);

static int ov5640_pwdn_gpio;
static int ov5640_reset_gpio;
static int ov5640_driver_pwdn_gpio;
static int OV5640_CSI_CONFIG;
static struct ov5640_work *ov5640_sensorw;
static struct i2c_client    *ov5640_client;
static u8 ov5640_i2c_buf[4];
static u8 ov5640_counter;
static int16_t ov5640_effect;
static int is_autoflash;
static int effect_value;
unsigned int ov5640_SAT_U = 0x40;
unsigned int ov5640_SAT_V = 0x40;

static struct __ov5640_ctrl *ov5640_ctrl;
static int ov5640_afinit = 1;

struct rw_semaphore ov_leds_list_lock;
struct list_head ov_leds_list;

static int ov5640_i2c_remove(struct i2c_client *client);
static int ov5640_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id);

static int ov5640_i2c_txdata(u16 saddr, u8 *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr	= saddr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

	if (i2c_transfer(ov5640_client->adapter, msg, 1) < 0)
		return -EIO;
	else
		return 0;
}

static int ov5640_i2c_write(unsigned short saddr, unsigned int waddr,
		unsigned short bdata, u8 trytimes)
{
	int rc = -EIO;

	ov5640_counter = 0;
	ov5640_i2c_buf[0] = (waddr & 0xFF00) >> 8;
	ov5640_i2c_buf[1] = (waddr & 0x00FF);
	ov5640_i2c_buf[2] = (bdata & 0x00FF);

	while ((ov5640_counter < trytimes) && (rc != 0)) {
		rc = ov5640_i2c_txdata(saddr, ov5640_i2c_buf, 3);

		if (rc < 0) {
			ov5640_counter++;
			CDBG("***--CAMERA i2c_write_w failed,i2c addr=0x%x,"
				"command addr = 0x%x, val = 0x%x,s=%d,"
					"rc=%d!\n", saddr, waddr, bdata,
					ov5640_counter, rc);
			msleep(20);
		}
	}
	return rc;
}

static int ov5640_i2c_rxdata(unsigned short saddr, unsigned char *rxdata,
		int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr	= saddr,
			.flags	= 0,
			.len	= 2,
			.buf	= rxdata,
		},
		{
			.addr	= saddr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	if (i2c_transfer(ov5640_client->adapter, msgs, 2) < 0) {
		CDBG("ov5640_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t ov5640_i2c_read_byte(unsigned short  saddr,
		unsigned int raddr, unsigned int *rdata)
{
	int rc = 0;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = ov5640_i2c_rxdata(saddr, buf, 1);
	if (rc < 0) {
		CDBG("ov5640_i2c_read_byte failed!\n");
		return rc;
	}

	*rdata = buf[0];

	return rc;
}

static int32_t ov5640_writepregs(struct ov5640_sensor *ptb, int32_t len)
{
	int32_t i, ret = 0;
	uint32_t regv;

	for (i = 0; i < len; i++) {
		if (0 == ptb[i].mask) {
			ov5640_i2c_write(ov5640_client->addr, ptb[i].addr,
					ptb[i].data, 10);
		} else {
			ov5640_i2c_read_byte(ov5640_client->addr, ptb[i].addr,
					&regv);
			regv &= ptb[i].mask;
			regv |= (ptb[i].data & (~ptb[i].mask));
			ov5640_i2c_write(ov5640_client->addr, ptb[i].addr,
					regv, 10);
		}
	}
	return ret;
}

static void camera_sw_power_onoff(int v)
{
	if (v == 0) {
		CDBG("camera_sw_power_onoff: down\n");
		ov5640_i2c_write(ov5640_client->addr, 0x3008, 0x42, 10);
	} else {
		CDBG("camera_sw_power_onoff: on\n");
		ov5640_i2c_write(ov5640_client->addr, 0x3008, 0x02, 10);
	}
}

static void ov5640_power_off(void)
{
	CDBG("--CAMERA-- %s ... (Start...)\n", __func__);
	gpio_set_value(ov5640_pwdn_gpio, 1);
	CDBG("--CAMERA-- %s ... (End...)\n", __func__);
}

static void ov5640_power_on(void)
{
	CDBG("--CAMERA-- %s ... (Start...)\n", __func__);
	gpio_set_value(ov5640_pwdn_gpio, 0);
	CDBG("--CAMERA-- %s ... (End...)\n", __func__);
}

static void ov5640_power_reset(void)
{
	CDBG("--CAMERA-- %s ... (Start...)\n", __func__);
	gpio_set_value(ov5640_reset_gpio, 1);   /* reset camera reset pin */
	msleep(20);
	gpio_set_value(ov5640_reset_gpio, 0);
	msleep(20);
	gpio_set_value(ov5640_reset_gpio, 1);
	msleep(20);

	CDBG("--CAMERA-- %s ... (End...)\n", __func__);
}

static int ov5640_probe_readID(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	u32 device_id_high = 0;
	u32 device_id_low = 0;

	CDBG("--CAMERA-- %s (Start...)\n", __func__);
	CDBG("--CAMERA-- %s sensor poweron,begin to read ID!\n", __func__);

	/* 0x300A ,sensor ID register */
	rc = ov5640_i2c_read_byte(ov5640_client->addr, 0x300A,
			&device_id_high);

	if (rc < 0) {
		CDBG("--CAMERA-- %s ok , readI2C failed, rc = 0x%x\r\n",
				__func__, rc);
		return rc;
	}
	CDBG("--CAMERA-- %s  readID high byte, data = 0x%x\r\n",
			__func__, device_id_high);

	/* 0x300B ,sensor ID register */
	rc = ov5640_i2c_read_byte(ov5640_client->addr, 0x300B,
			&device_id_low);
	if (rc < 0) {
		CDBG("--CAMERA-- %s ok , readI2C failed,rc = 0x%x\r\n",
				__func__, rc);
		return rc;
	}

	CDBG("--CAMERA-- %s  readID low byte, data = 0x%x\r\n",
			__func__, device_id_low);
	CDBG("--CAMERA-- %s return ID :0x%x\n", __func__,
			(device_id_high << 8) + device_id_low);

	/* 0x5640, ov5640 chip id */
	if ((device_id_high << 8) + device_id_low != OV5640_SENSOR_ID) {
		CDBG("--CAMERA-- %s ok , device id error, should be 0x%x\r\n",
				__func__, OV5640_SENSOR_ID);
		return -EINVAL;
	} else {
		CDBG("--CAMERA-- %s ok , device id=0x%x\n", __func__,
				OV5640_SENSOR_ID);
		return 0;
	}
}

static int ov5640_af_setting(void)
{
	int rc = 0;
	int lens = sizeof(ov5640_afinit_tbl) / sizeof(ov5640_afinit_tbl[0]);

	CDBG("--CAMERA-- ov5640_af_setting\n");

	ov5640_i2c_write(ov5640_client->addr, 0x3000, 0x20, 10);

	rc = ov5640_i2c_txdata(ov5640_client->addr, ov5640_afinit_tbl, lens);
	if (rc < 0) {
		CDBG("--CAMERA-- AF_init failed\n");
		return rc;
	}

	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_MAIN, 0x00, 10);
	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_ACK, 0x00, 10);
	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_PARA0, 0x00, 10);
	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_PARA1, 0x00, 10);
	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_PARA2, 0x00, 10);
	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_PARA3, 0x00, 10);
	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_PARA4, 0x00, 10);
	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_FW_STATUS, 0x7f, 10);
	ov5640_i2c_write(ov5640_client->addr, 0x3000, 0x00, 10);

	return rc;
}

static int ov5640_set_flash_light(enum led_brightness brightness)
{
	struct led_classdev *led_cdev;

	CDBG("ov5640_set_flash_light brightness = %d\n", brightness);

	down_read(&ov_leds_list_lock);
	list_for_each_entry(led_cdev, &ov_leds_list, node) {
		if (!strncmp(led_cdev->name, "flashlight", 10))
			break;
	}
	up_read(&ov_leds_list_lock);

	if (led_cdev) {
		led_brightness_set(led_cdev, brightness);
	} else {
		CDBG("get flashlight device failed\n");
		return -EINVAL;
	}

	return 0;
}

static int ov5640_video_config(void)
{
	int rc = 0;

	CDBG("--CAMERA-- ov5640_video_config\n");
	CDBG("--CAMERA-- preview in, is_autoflash - 0x%x\n", is_autoflash);

	/* autoflash setting */
	if (is_autoflash == 1)
		ov5640_set_flash_light(LED_OFF);

	/* preview setting */
	rc = OV5640CORE_WRITEPREG(ov5640_preview_tbl);
	return rc;
}

static int ov5640_snapshot_config(void)
{
	int rc = 0;
	unsigned int tmp;

	CDBG("--CAMERA-- SENSOR_SNAPSHOT_MODE\n");
	CDBG("--CAMERA-- %s, snapshot in, is_autoflash - 0x%x\n", __func__,
			is_autoflash);

	if (is_autoflash == 1) {
		ov5640_i2c_read_byte(ov5640_client->addr, 0x350b, &tmp);
		CDBG("--CAMERA-- GAIN VALUE : %x\n", tmp);
		if ((tmp & 0x80) == 0)
			ov5640_set_flash_light(LED_OFF);
		else
			ov5640_set_flash_light(LED_FULL);
	}

	rc = OV5640CORE_WRITEPREG(ov5640_capture_tbl);

	return rc;
}

static int ov5640_setting(enum msm_s_reg_update rupdate,
		enum msm_s_setting rt)
{
	int rc = -EINVAL, tmp;
	struct msm_camera_csi_params ov5640_csi_params;

	CDBG("--CAMERA-- %s (Start...), rupdate=%d\n", __func__, rupdate);

	switch (rupdate) {
	case S_UPDATE_PERIODIC:
		if (!OV5640_CSI_CONFIG) {
			camera_sw_power_onoff(0); /* standby */
			msleep(20);

			ov5640_csi_params.lane_cnt = 2;
			ov5640_csi_params.data_format = CSI_8BIT;
			ov5640_csi_params.lane_assign = 0xe4;
			ov5640_csi_params.dpcm_scheme = 0;
			ov5640_csi_params.settle_cnt = 0x6;

			CDBG("%s: msm_camio_csi_config\n", __func__);

			rc = msm_camio_csi_config(&ov5640_csi_params);
			msleep(20);
			camera_sw_power_onoff(1); /* on */
			msleep(20);

			OV5640_CSI_CONFIG = 1;

		} else {
			rc = 0;
		}

		if (S_RES_PREVIEW == rt)
			rc = ov5640_video_config();
		else if (S_RES_CAPTURE == rt)
			rc = ov5640_snapshot_config();

		break; /* UPDATE_PERIODIC */

	case S_REG_INIT:
		CDBG("--CAMERA-- S_REG_INIT (Start)\n");

		rc = ov5640_i2c_write(ov5640_client->addr, 0x3103, 0x11, 10);
		rc = ov5640_i2c_write(ov5640_client->addr, 0x3008, 0x82, 10);
		msleep(20);

		/* set sensor init setting */
		CDBG("set sensor init setting\n");
		rc = OV5640CORE_WRITEPREG(ov5640_init_tbl);
		if (rc < 0) {
			CDBG("sensor init setting failed\n");
			break;
		}

		/* set image quality setting */
		rc = OV5640CORE_WRITEPREG(ov5640_init_iq_tbl);
		rc = ov5640_i2c_read_byte(ov5640_client->addr, 0x4740, &tmp);
		CDBG("--CAMERA-- init 0x4740 value=0x%x\n", tmp);

		if (tmp != 0x21) {
			rc = ov5640_i2c_write(ov5640_client->addr, 0x4740,
					0x21, 10);
			msleep(20);
			rc = ov5640_i2c_read_byte(ov5640_client->addr,
					0x4740, &tmp);
			CDBG("--CAMERA-- WG 0x4740 value=0x%x\n", tmp);
		}

		CDBG("--CAMERA-- AF_init: ov5640_afinit = %d\n",
				ov5640_afinit);
		if (ov5640_afinit == 1) {
			rc = ov5640_af_setting();
			if (rc < 0) {
				CDBG("--CAMERA-- ov5640_af_setting failed\n");
				break;
			}
			ov5640_afinit = 0;
		}

		/* reset fps_divider */
		ov5640_ctrl->fps_divider = 1 * 0x0400;
		CDBG("--CAMERA-- S_REG_INIT (End)\n");
		break; /* case REG_INIT: */

	default:
		break;
	} /* switch (rupdate) */

	CDBG("--CAMERA-- %s (End), rupdate=%d\n", __func__, rupdate);

	return rc;
}

static int ov5640_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int rc = -ENOMEM;

	CDBG("--CAMERA-- %s\n", __func__);
	ov5640_ctrl = kzalloc(sizeof(struct __ov5640_ctrl), GFP_KERNEL);
	if (!ov5640_ctrl) {
		CDBG("--CAMERA-- kzalloc ov5640_ctrl error !!\n");
		kfree(ov5640_ctrl);
		return rc;
	}

	ov5640_ctrl->fps_divider = 1 * 0x00000400;
	ov5640_ctrl->pict_fps_divider = 1 * 0x00000400;
	ov5640_ctrl->set_test = S_TEST_OFF;
	ov5640_ctrl->prev_res = S_QTR_SIZE;
	ov5640_ctrl->pict_res = S_FULL_SIZE;

	if (data)
		ov5640_ctrl->sensordata = data;

	ov5640_power_off();

	CDBG("%s: msm_camio_clk_rate_set\n", __func__);

	msm_camio_clk_rate_set(24000000);
	msleep(20);

	ov5640_power_on();
	ov5640_power_reset();

	CDBG("%s: init sequence\n", __func__);

	if (ov5640_ctrl->prev_res == S_QTR_SIZE)
		rc = ov5640_setting(S_REG_INIT, S_RES_PREVIEW);
	else
		rc = ov5640_setting(S_REG_INIT, S_RES_CAPTURE);

	if (rc < 0) {
		CDBG("--CAMERA-- %s : ov5640_setting failed. rc = %d\n",
				__func__, rc);
		kfree(ov5640_ctrl);
		return rc;
	}

	OV5640_CSI_CONFIG = 0;

	CDBG("--CAMERA--re_init_sensor ok!!\n");
	return rc;
}

static int ov5640_sensor_release(void)
{
	CDBG("--CAMERA--ov5640_sensor_release!!\n");

	mutex_lock(&ov5640_mutex);

	ov5640_power_off();

	kfree(ov5640_ctrl);
	ov5640_ctrl = NULL;

	OV5640_CSI_CONFIG = 0;

	mutex_unlock(&ov5640_mutex);
	return 0;
}

static const struct i2c_device_id ov5640_i2c_id[] = {
	{"ov5640",  0}, {}
};

static int ov5640_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static int ov5640_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&ov5640_wait_queue);
	return 0;
}

static long ov5640_set_effect(int mode, int effect)
{
	int rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		/* Context A Special Effects */
		CDBG("--CAMERA-- %s ...SENSOR_PREVIEW_MODE\n", __func__);
		break;

	case SENSOR_SNAPSHOT_MODE:
		/* Context B Special Effects */
		CDBG("--CAMERA-- %s ...SENSOR_SNAPSHOT_MODE\n", __func__);
		break;

	default:
		break;
	}

	effect_value = effect;

	switch (effect)	{
	case CAMERA_EFFECT_OFF:
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_OFF\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_effect_normal_tbl);
		/* for recover saturation level when change special effect */
		ov5640_i2c_write(ov5640_client->addr, 0x5583, ov5640_SAT_U,
				10);
		/* for recover saturation level when change special effect */
		ov5640_i2c_write(ov5640_client->addr, 0x5584, ov5640_SAT_V,
				10);
		break;

	case CAMERA_EFFECT_MONO:
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_MONO\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_effect_mono_tbl);
		break;

	case CAMERA_EFFECT_BW:
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_BW\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_effect_bw_tbl);
		break;

	case CAMERA_EFFECT_BLUISH:
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_BLUISH\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_effect_bluish_tbl);
		break;

	case CAMERA_EFFECT_SOLARIZE:
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_NEGATIVE\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_effect_solarize_tbl);
		break;

	case CAMERA_EFFECT_SEPIA:
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_SEPIA\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_effect_sepia_tbl);
		break;

	case CAMERA_EFFECT_REDDISH:
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_REDDISH\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_effect_reddish_tbl);
		break;

	case CAMERA_EFFECT_GREENISH:
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_GREENISH\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_effect_greenish_tbl);
		break;

	case CAMERA_EFFECT_NEGATIVE:
		CDBG("--CAMERA-- %s ...CAMERA_EFFECT_NEGATIVE\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_effect_negative_tbl);
		break;

	default:
		CDBG("--CAMERA-- %s ...Default(Not Support)\n", __func__);
	}

	ov5640_effect = effect;
	/* Refresh Sequencer */
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int ov5640_set_brightness(int8_t brightness)
{
	int rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...brightness = %d\n", __func__ , brightness);

	switch (brightness) {
	case CAMERA_BRIGHTNESS_LV0:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_LV0\n");
		rc = OV5640CORE_WRITEPREG(ov5640_brightness_lv0_tbl);
		break;

	case CAMERA_BRIGHTNESS_LV1:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_LV1\n");
		rc = OV5640CORE_WRITEPREG(ov5640_brightness_lv1_tbl);
		break;

	case CAMERA_BRIGHTNESS_LV2:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_LV2\n");
		rc = OV5640CORE_WRITEPREG(ov5640_brightness_lv2_tbl);
		break;

	case CAMERA_BRIGHTNESS_LV3:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_LV3\n");
		rc = OV5640CORE_WRITEPREG(ov5640_brightness_lv3_tbl);
		break;

	case CAMERA_BRIGHTNESS_LV4:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_LV4\n");
		rc = OV5640CORE_WRITEPREG(ov5640_brightness_default_lv4_tbl);
		break;

	case CAMERA_BRIGHTNESS_LV5:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_LV5\n");
		rc = OV5640CORE_WRITEPREG(ov5640_brightness_lv5_tbl);
		break;

	case CAMERA_BRIGHTNESS_LV6:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_LV6\n");
		rc = OV5640CORE_WRITEPREG(ov5640_brightness_lv6_tbl);
		break;

	case CAMERA_BRIGHTNESS_LV7:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_LV7\n");
		rc = OV5640CORE_WRITEPREG(ov5640_brightness_lv7_tbl);
		break;

	case CAMERA_BRIGHTNESS_LV8:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_LV8\n");
		rc = OV5640CORE_WRITEPREG(ov5640_brightness_lv8_tbl);
		break;

	default:
		CDBG("--CAMERA--CAMERA_BRIGHTNESS_ERROR COMMAND\n");
		break;
	}

	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int ov5640_set_contrast(int contrast)
{
	int rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...contrast = %d\n", __func__ , contrast);

	if (effect_value == CAMERA_EFFECT_OFF) {
		switch (contrast) {
		case CAMERA_CONTRAST_LV0:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV0\n");
			rc = OV5640CORE_WRITEPREG(ov5640_contrast_lv0_tbl);
			break;

		case CAMERA_CONTRAST_LV1:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV1\n");
			rc = OV5640CORE_WRITEPREG(ov5640_contrast_lv1_tbl);
			break;

		case CAMERA_CONTRAST_LV2:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV2\n");
			rc = OV5640CORE_WRITEPREG(ov5640_contrast_lv2_tbl);
			break;

		case CAMERA_CONTRAST_LV3:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV3\n");
			rc = OV5640CORE_WRITEPREG(ov5640_contrast_lv3_tbl);
			break;

		case CAMERA_CONTRAST_LV4:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV4\n");
			rc = OV5640CORE_WRITEPREG(
					ov5640_contrast_default_lv4_tbl);
			break;

		case CAMERA_CONTRAST_LV5:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV5\n");
			rc = OV5640CORE_WRITEPREG(ov5640_contrast_lv5_tbl);
			break;

		case CAMERA_CONTRAST_LV6:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV6\n");
			rc = OV5640CORE_WRITEPREG(ov5640_contrast_lv6_tbl);
			break;

		case CAMERA_CONTRAST_LV7:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV7\n");
			rc = OV5640CORE_WRITEPREG(ov5640_contrast_lv7_tbl);
			break;

		case CAMERA_CONTRAST_LV8:
			CDBG("--CAMERA--CAMERA_CONTRAST_LV8\n");
			rc = OV5640CORE_WRITEPREG(ov5640_contrast_lv8_tbl);
			break;

		default:
			CDBG("--CAMERA--CAMERA_CONTRAST_ERROR COMMAND\n");
			break;
		}
	}

	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int ov5640_set_sharpness(int sharpness)
{
	int rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...sharpness = %d\n", __func__ , sharpness);

	if (effect_value == CAMERA_EFFECT_OFF) {
		switch (sharpness) {
		case CAMERA_SHARPNESS_LV0:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV0\n");
			rc = OV5640CORE_WRITEPREG(ov5640_sharpness_lv0_tbl);
			break;

		case CAMERA_SHARPNESS_LV1:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV1\n");
			rc = OV5640CORE_WRITEPREG(ov5640_sharpness_lv1_tbl);
			break;

		case CAMERA_SHARPNESS_LV2:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV2\n");
			rc = OV5640CORE_WRITEPREG(
					ov5640_sharpness_default_lv2_tbl);
			break;

		case CAMERA_SHARPNESS_LV3:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV3\n");
			rc = OV5640CORE_WRITEPREG(ov5640_sharpness_lv3_tbl);
			break;

		case CAMERA_SHARPNESS_LV4:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV4\n");
			rc = OV5640CORE_WRITEPREG(ov5640_sharpness_lv4_tbl);
			break;

		case CAMERA_SHARPNESS_LV5:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV5\n");
			rc = OV5640CORE_WRITEPREG(ov5640_sharpness_lv5_tbl);
			break;

		case CAMERA_SHARPNESS_LV6:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV6\n");
			rc = OV5640CORE_WRITEPREG(ov5640_sharpness_lv6_tbl);
			break;

		case CAMERA_SHARPNESS_LV7:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV7\n");
			rc = OV5640CORE_WRITEPREG(ov5640_sharpness_lv7_tbl);
			break;

		case CAMERA_SHARPNESS_LV8:
			CDBG("--CAMERA--CAMERA_SHARPNESS_LV8\n");
			rc = OV5640CORE_WRITEPREG(ov5640_sharpness_lv8_tbl);
			break;

		default:
			CDBG("--CAMERA--CAMERA_SHARPNESS_ERROR COMMAND\n");
			break;
		}
	}

	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int ov5640_set_saturation(int saturation)
{
	long rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...saturation = %d\n", __func__ , saturation);

	if (effect_value == CAMERA_EFFECT_OFF) {
		switch (saturation) {
		case CAMERA_SATURATION_LV0:
			CDBG("--CAMERA--CAMERA_SATURATION_LV0\n");
			rc = OV5640CORE_WRITEPREG(ov5640_saturation_lv0_tbl);
			break;

		case CAMERA_SATURATION_LV1:
			CDBG("--CAMERA--CAMERA_SATURATION_LV1\n");
			rc = OV5640CORE_WRITEPREG(ov5640_saturation_lv1_tbl);
			break;

		case CAMERA_SATURATION_LV2:
			CDBG("--CAMERA--CAMERA_SATURATION_LV2\n");
			rc = OV5640CORE_WRITEPREG(ov5640_saturation_lv2_tbl);
			break;

		case CAMERA_SATURATION_LV3:
			CDBG("--CAMERA--CAMERA_SATURATION_LV3\n");
			rc = OV5640CORE_WRITEPREG(ov5640_saturation_lv3_tbl);
			break;

		case CAMERA_SATURATION_LV4:
			CDBG("--CAMERA--CAMERA_SATURATION_LV4\n");
			rc = OV5640CORE_WRITEPREG(
					ov5640_saturation_default_lv4_tbl);
			break;

		case CAMERA_SATURATION_LV5:
			CDBG("--CAMERA--CAMERA_SATURATION_LV5\n");
			rc = OV5640CORE_WRITEPREG(ov5640_saturation_lv5_tbl);
			break;

		case CAMERA_SATURATION_LV6:
			CDBG("--CAMERA--CAMERA_SATURATION_LV6\n");
			rc = OV5640CORE_WRITEPREG(ov5640_saturation_lv6_tbl);
			break;

		case CAMERA_SATURATION_LV7:
			CDBG("--CAMERA--CAMERA_SATURATION_LV7\n");
			rc = OV5640CORE_WRITEPREG(ov5640_saturation_lv7_tbl);
			break;

		case CAMERA_SATURATION_LV8:
			CDBG("--CAMERA--CAMERA_SATURATION_LV8\n");
			rc = OV5640CORE_WRITEPREG(ov5640_saturation_lv8_tbl);
			break;

		default:
			CDBG("--CAMERA--CAMERA_SATURATION_ERROR COMMAND\n");
			break;
		}
	}

	/* for recover saturation level when change special effect */
	switch (saturation) {
	case CAMERA_SATURATION_LV0:
		CDBG("--CAMERA--CAMERA_SATURATION_LV0\n");
		ov5640_SAT_U = 0x00;
		ov5640_SAT_V = 0x00;
		break;
	case CAMERA_SATURATION_LV1:
		CDBG("--CAMERA--CAMERA_SATURATION_LV1\n");
		ov5640_SAT_U = 0x10;
		ov5640_SAT_V = 0x10;
		break;
	case CAMERA_SATURATION_LV2:
		CDBG("--CAMERA--CAMERA_SATURATION_LV2\n");
		ov5640_SAT_U = 0x20;
		ov5640_SAT_V = 0x20;
		break;
	case CAMERA_SATURATION_LV3:
		CDBG("--CAMERA--CAMERA_SATURATION_LV3\n");
		ov5640_SAT_U = 0x30;
		ov5640_SAT_V = 0x30;
		break;
	case CAMERA_SATURATION_LV4:
		CDBG("--CAMERA--CAMERA_SATURATION_LV4\n");
		ov5640_SAT_U = 0x40;
		ov5640_SAT_V = 0x40;            break;
	case CAMERA_SATURATION_LV5:
		CDBG("--CAMERA--CAMERA_SATURATION_LV5\n");
		ov5640_SAT_U = 0x50;
		ov5640_SAT_V = 0x50;            break;
	case CAMERA_SATURATION_LV6:
		CDBG("--CAMERA--CAMERA_SATURATION_LV6\n");
		ov5640_SAT_U = 0x60;
		ov5640_SAT_V = 0x60;
		break;
	case CAMERA_SATURATION_LV7:
		CDBG("--CAMERA--CAMERA_SATURATION_LV7\n");
		ov5640_SAT_U = 0x70;
		ov5640_SAT_V = 0x70;            break;
	case CAMERA_SATURATION_LV8:
		CDBG("--CAMERA--CAMERA_SATURATION_LV8\n");
		ov5640_SAT_U = 0x80;
		ov5640_SAT_V = 0x80;
		break;
	default:
		CDBG("--CAMERA--CAMERA_SATURATION_ERROR COMMAND\n");
		break;
	}

	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static long ov5640_set_antibanding(int antibanding)
{
	long rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n",  __func__);
	CDBG("--CAMERA-- %s ...antibanding = %d\n",  __func__, antibanding);

	switch (antibanding) {
	case CAMERA_ANTIBANDING_OFF:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_OFF\n");
		break;

	case CAMERA_ANTIBANDING_60HZ:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_60HZ\n");
		rc = OV5640CORE_WRITEPREG(ov5640_antibanding_60z_tbl);
		break;

	case CAMERA_ANTIBANDING_50HZ:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_50HZ\n");
		rc = OV5640CORE_WRITEPREG(ov5640_antibanding_50z_tbl);
		break;

	case CAMERA_ANTIBANDING_AUTO:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_AUTO\n");
		rc = OV5640CORE_WRITEPREG(ov5640_antibanding_auto_tbl);
		break;

	default:
		CDBG("--CAMERA--CAMERA_ANTIBANDING_ERROR COMMAND\n");
		break;
	}

	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static long ov5640_set_exposure_mode(int mode)
{
	long rc = 0;
	CDBG("--CAMERA-- %s ...(Start)\n", __func__);
	CDBG("--CAMERA-- %s ...mode = %d\n", __func__ , mode);
	CDBG("--CAMERA-- %s ...(End)\n", __func__);
	return rc;
}

static int32_t ov5640_lens_shading_enable(uint8_t is_enable)
{
	int32_t rc = 0;
	CDBG("--CAMERA--%s: ...(Start). enable = %d\n",  __func__, is_enable);

	if (is_enable) {
		CDBG("%s: enable~!!\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_lens_shading_on_tbl);
	} else {
		CDBG("%s: disable~!!\n", __func__);
		rc = OV5640CORE_WRITEPREG(ov5640_lens_shading_off_tbl);
	}
	CDBG("--CAMERA--%s: ...(End). rc = %d\n", __func__, rc);
	return rc;
}

static int ov5640_set_sensor_mode(int mode, int res)
{
	int rc = 0;

	CDBG("--CAMERA-- ov5640_set_sensor_mode mode = %d, res = %d\n",
			mode, res);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		CDBG("--CAMERA-- SENSOR_PREVIEW_MODE\n");
		rc = ov5640_setting(S_UPDATE_PERIODIC, S_RES_PREVIEW);
		break;

	case SENSOR_SNAPSHOT_MODE:
		CDBG("--CAMERA-- SENSOR_SNAPSHOT_MODE\n");
		rc = ov5640_setting(S_UPDATE_PERIODIC, S_RES_CAPTURE);
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		CDBG("--CAMERA-- SENSOR_RAW_SNAPSHOT_MODE\n");
		rc = ov5640_setting(S_UPDATE_PERIODIC, S_RES_CAPTURE);
		break;

	default:
		CDBG("--CAMERA--ov5640_set_sensor_mode no support\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int ov5640_set_wb_oem(uint8_t param)
{
	int rc = 0;
	unsigned int tmp2;

	CDBG("[kylin] %s \r\n", __func__);

	ov5640_i2c_read_byte(ov5640_client->addr, 0x350b, &tmp2);
	CDBG("--CAMERA-- GAIN VALUE : %x\n", tmp2);

	switch (param) {
	case CAMERA_WB_AUTO:

		CDBG("--CAMERA--CAMERA_WB_AUTO\n");
		rc = OV5640CORE_WRITEPREG(ov5640_wb_def);
		break;

	case CAMERA_WB_CUSTOM:
		CDBG("--CAMERA--CAMERA_WB_CUSTOM\n");
		rc = OV5640CORE_WRITEPREG(ov5640_wb_custom);
		break;
	case CAMERA_WB_INCANDESCENT:
		CDBG("--CAMERA--CAMERA_WB_INCANDESCENT\n");
		rc = OV5640CORE_WRITEPREG(ov5640_wb_inc);
		break;
	case CAMERA_WB_DAYLIGHT:
		CDBG("--CAMERA--CAMERA_WB_DAYLIGHT\n");
		rc = OV5640CORE_WRITEPREG(ov5640_wb_daylight);
		break;
	case CAMERA_WB_CLOUDY_DAYLIGHT:
		CDBG("--CAMERA--CAMERA_WB_CLOUDY_DAYLIGHT\n");
		rc = OV5640CORE_WRITEPREG(ov5640_wb_cloudy);
		break;
	default:
		break;
	}
	return rc;
}

static int ov5640_set_touchaec(uint32_t x, uint32_t y)
{
	uint8_t aec_arr[8] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
	int idx = 0;
	int i;

	CDBG("[kylin] %s x: %d ,y: %d\r\n", __func__ , x, y);
	idx = x / 2 + y * 2;
	CDBG("[kylin] idx: %d\r\n", idx);

	if (x % 2 == 0)
		aec_arr[idx] = 0x10 | 0x0a;
	else
		aec_arr[idx] = 0x01 | 0xa0;

	for (i = 0; i < 8; i++) {
		CDBG("write : %x val : %x ", 0x5688 + i, aec_arr[i]);
		ov5640_i2c_write(ov5640_client->addr, 0x5688 + i,
				aec_arr[i], 10);
	}

	return 1;
}

static int ov5640_set_exposure_compensation(int compensation)
{
	long rc = 0;

	CDBG("--CAMERA-- %s ...(Start)\n", __func__);

	CDBG("--CAMERA-- %s ...exposure_compensation = %d\n", __func__ ,
			    compensation);

	switch (compensation) {
	case CAMERA_EXPOSURE_COMPENSATION_LV0:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV0\n");
		rc = OV5640CORE_WRITEPREG(
				ov5640_exposure_compensation_lv0_tbl);
		break;

	case CAMERA_EXPOSURE_COMPENSATION_LV1:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV1\n");
		rc = OV5640CORE_WRITEPREG(
				ov5640_exposure_compensation_lv1_tbl);
		break;

	case CAMERA_EXPOSURE_COMPENSATION_LV2:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV2\n");
		rc = OV5640CORE_WRITEPREG(
			    ov5640_exposure_compensation_lv2_default_tbl);
		break;

	case CAMERA_EXPOSURE_COMPENSATION_LV3:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
		rc = OV5640CORE_WRITEPREG(
				ov5640_exposure_compensation_lv3_tbl);
		break;

	case CAMERA_EXPOSURE_COMPENSATION_LV4:
		CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
		rc = OV5640CORE_WRITEPREG(
				ov5640_exposure_compensation_lv4_tbl);
		break;

	default:
		CDBG("--CAMERA--ERROR CAMERA_EXPOSURE_COMPENSATION\n");
		break;
	}

	CDBG("--CAMERA-- %s ...(End)\n", __func__);

	return rc;
}

static int ov5640_sensor_start_af(void)
{
	int i;
	unsigned int af_st = 0;
	unsigned int af_ack = 0;
	unsigned int tmp = 0;
	int rc = 0;

	CDBG("--CAMERA-- %s (Start...)\n", __func__);

	ov5640_i2c_read_byte(ov5640_client->addr,
			OV5640_CMD_FW_STATUS, &af_st);
	CDBG("--CAMERA-- %s af_st = %d\n", __func__, af_st);

	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_ACK, 0x01, 10);
	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_MAIN, 0x03, 10);

	for (i = 0; i < 50; i++) {
		ov5640_i2c_read_byte(ov5640_client->addr,
				OV5640_CMD_ACK, &af_ack);
		if (af_ack == 0)
			break;
		msleep(50);
	}
	CDBG("--CAMERA-- %s af_ack = 0x%x\n", __func__, af_ack);

	ov5640_i2c_read_byte(ov5640_client->addr, OV5640_CMD_FW_STATUS,
			&af_st);
	CDBG("--CAMERA-- %s af_st = %d\n", __func__, af_st);

	if (af_st == 0x10) {
		CDBG("--CAMERA-- %s AF ok and release AF setting~!!\n",
				__func__);
	} else {
		CDBG("--CAMERA-- %s AF not ready!!\n", __func__);
	}

	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_ACK, 0x01, 10);
	ov5640_i2c_write(ov5640_client->addr, OV5640_CMD_MAIN, 0x07, 10);

	for (i = 0; i < 70; i++) {
		ov5640_i2c_read_byte(ov5640_client->addr, OV5640_CMD_ACK,
				&af_ack);
		if (af_ack == 0)
			break;
		msleep(25);
	}

	ov5640_i2c_read_byte(ov5640_client->addr, OV5640_CMD_PARA0, &tmp);
	CDBG("0x3024 = %x\n", tmp);
	rc = ((tmp == 0) ? 1 : 0);

	ov5640_i2c_read_byte(ov5640_client->addr, OV5640_CMD_PARA1, &tmp);
	CDBG("0x3025 = %x\n", tmp);
	rc = ((tmp == 0) ? 1 : 0);

	ov5640_i2c_read_byte(ov5640_client->addr, OV5640_CMD_PARA2, &tmp);
	CDBG("0x3026 = %x\n", tmp);
	rc = ((tmp == 0) ? 1 : 0);

	ov5640_i2c_read_byte(ov5640_client->addr, OV5640_CMD_PARA3, &tmp);
	CDBG("0x3027 = %x\n", tmp);
	rc = ((tmp == 0) ? 1 : 0) ;

	ov5640_i2c_read_byte(ov5640_client->addr, OV5640_CMD_PARA4, &tmp);
	CDBG("0x3028 = %x\n", tmp);
	rc = ((tmp == 0) ? 1 : 0) ;

	CDBG("--CAMERA-- %s rc = %d(End...)\n", __func__, rc);
	return rc;
}

static int ov5640_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long rc = 0;

	if (copy_from_user(&cdata, (void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	CDBG("--CAMERA-- %s %d\n", __func__, cdata.cfgtype);

	mutex_lock(&ov5640_mutex);

	switch (cdata.cfgtype) {
	case CFG_SET_MODE:
		rc = ov5640_set_sensor_mode(cdata.mode, cdata.rs);
		break;

	case CFG_SET_EFFECT:
		CDBG("--CAMERA-- CFG_SET_EFFECT mode=%d,"
				"effect = %d !!\n", cdata.mode,
				cdata.cfg.effect);
		rc = ov5640_set_effect(cdata.mode, cdata.cfg.effect);
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
		CDBG("--CAMERA-- CFG_PWR_DOWN (Not Support)\n");
		ov5640_power_off();
		break;

	case CFG_SET_DEFAULT_FOCUS:
		CDBG("--CAMERA-- CFG_SET_DEFAULT_FOCUS (Not Implement) !!\n");
		break;

	case CFG_MOVE_FOCUS:
		CDBG("--CAMERA-- CFG_MOVE_FOCUS (Not Implement) !!\n");
		break;

	case CFG_SET_BRIGHTNESS:
		CDBG("--CAMERA-- CFG_SET_BRIGHTNESS  !!\n");
		rc = ov5640_set_brightness(cdata.cfg.brightness);
		break;

	case CFG_SET_CONTRAST:
		CDBG("--CAMERA-- CFG_SET_CONTRAST  !!\n");
		rc = ov5640_set_contrast(cdata.cfg.contrast);
		break;

	case CFG_SET_EXPOSURE_MODE:
		CDBG("--CAMERA-- CFG_SET_EXPOSURE_MODE !!\n");
		rc = ov5640_set_exposure_mode(cdata.cfg.ae_mode);
		break;

	case CFG_SET_ANTIBANDING:
		CDBG("--CAMERA-- CFG_SET_ANTIBANDING antibanding = %d!!\n",
				cdata.cfg.antibanding);
		rc = ov5640_set_antibanding(cdata.cfg.antibanding);
		break;

	case CFG_SET_LENS_SHADING:
		CDBG("--CAMERA-- CFG_SET_LENS_SHADING !!\n");
		rc = ov5640_lens_shading_enable(
				cdata.cfg.lens_shading);
		break;

	case CFG_SET_SATURATION:
		CDBG("--CAMERA-- CFG_SET_SATURATION !!\n");
		rc = ov5640_set_saturation(cdata.cfg.saturation);
		break;

	case CFG_SET_SHARPNESS:
		CDBG("--CAMERA-- CFG_SET_SHARPNESS !!\n");
		rc = ov5640_set_sharpness(cdata.cfg.sharpness);
		break;

	case CFG_SET_WB:
		CDBG("--CAMERA-- CFG_SET_WB!!\n");
		ov5640_set_wb_oem(cdata.cfg.wb_val);
		rc = 0 ;
		break;

	case CFG_SET_TOUCHAEC:
		CDBG("--CAMERA-- CFG_SET_TOUCHAEC!!\n");
		ov5640_set_touchaec(cdata.cfg.aec_cord.x,
				cdata.cfg.aec_cord.y);
		rc = 0 ;
		break;

	case CFG_SET_AUTO_FOCUS:
		CDBG("--CAMERA-- CFG_SET_AUTO_FOCUS !\n");
		rc = ov5640_sensor_start_af();
		break;

	case CFG_SET_AUTOFLASH:
		CDBG("--CAMERA-- CFG_SET_AUTOFLASH !\n");
		is_autoflash = cdata.cfg.is_autoflash;
		CDBG("[kylin] is autoflash %d\r\n", is_autoflash);
		rc = 0;
		break;

	case CFG_SET_EXPOSURE_COMPENSATION:
		CDBG("--CAMERA-- CFG_SET_EXPOSURE_COMPENSATION !\n");
		rc = ov5640_set_exposure_compensation(
				cdata.cfg.exp_compensation);
		break;

	default:
		CDBG("%s: Command=%d (Not Implement)!!\n", __func__,
				cdata.cfgtype);
		rc = -EINVAL;
		break;
	}

	mutex_unlock(&ov5640_mutex);
	return rc;
}

static struct i2c_driver ov5640_i2c_driver = {
	.id_table = ov5640_i2c_id,
	.probe  = ov5640_i2c_probe,
	.remove = ov5640_i2c_remove,
	.driver = {
		.name = "ov5640",
	},
};

static int ov5640_probe_init_gpio(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	CDBG("--CAMERA-- %s\n", __func__);

	ov5640_pwdn_gpio = data->sensor_pwd;
	ov5640_reset_gpio = data->sensor_reset;
	ov5640_driver_pwdn_gpio = data->vcm_pwd ;

	if (data->vcm_enable)
		gpio_direction_output(data->vcm_pwd, 1);

	gpio_direction_output(data->sensor_reset, 1);
	gpio_direction_output(data->sensor_pwd, 1);

	return rc;

}

static void ov5640_probe_free_gpio(const struct msm_camera_sensor_info *data)
{
	gpio_free(ov5640_pwdn_gpio);
	gpio_free(ov5640_reset_gpio);

	if (data->vcm_enable) {
		gpio_free(ov5640_driver_pwdn_gpio);
		ov5640_driver_pwdn_gpio = 0xFF ;
	}

	ov5640_pwdn_gpio	= 0xFF;
	ov5640_reset_gpio	= 0xFF;
}

static int ov5640_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = -ENOTSUPP;

	CDBG("--CAMERA-- %s (Start...)\n", __func__);
	rc = i2c_add_driver(&ov5640_i2c_driver);
	CDBG("--CAMERA-- i2c_add_driver ret:0x%x,ov5640_client=0x%x\n",
			rc, (unsigned int)ov5640_client);
	if ((rc < 0) || (ov5640_client == NULL)) {
		CDBG("--CAMERA-- i2c_add_driver FAILS!!\n");
		return rc;
	}

	rc = ov5640_probe_init_gpio(info);
	if (rc < 0)
		return rc;

	ov5640_power_off();

	/* SENSOR NEED MCLK TO DO I2C COMMUNICTION, OPEN CLK FIRST*/
	msm_camio_clk_rate_set(24000000);

	msleep(20);

	ov5640_power_on();
	ov5640_power_reset();

	rc = ov5640_probe_readID(info);

	if (rc < 0) {
		CDBG("--CAMERA--ov5640_probe_readID Fail !!~~~~!!\n");
		CDBG("--CAMERA-- %s, unregister\n", __func__);
		i2c_del_driver(&ov5640_i2c_driver);
		ov5640_power_off();
		ov5640_probe_free_gpio(info);
		return rc;
	}

	s->s_init		= ov5640_sensor_open_init;
	s->s_release		= ov5640_sensor_release;
	s->s_config		= ov5640_sensor_config;
	s->s_camera_type	= BACK_CAMERA_2D;
	s->s_mount_angle	= info->sensor_platform_info->mount_angle;

	ov5640_power_off();

	CDBG("--CAMERA-- %s (End...)\n", __func__);
	return rc;
}

static int ov5640_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	CDBG("--CAMERA-- %s ... (Start...)\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("--CAMERA--i2c_check_functionality failed\n");
		return -ENOMEM;
	}

	ov5640_sensorw = kzalloc(sizeof(struct ov5640_work), GFP_KERNEL);
	if (!ov5640_sensorw) {
		CDBG("--CAMERA--kzalloc failed\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, ov5640_sensorw);
	ov5640_init_client(client);
	ov5640_client = client;

	CDBG("--CAMERA-- %s ... (End...)\n", __func__);
	return 0;
}

static int __ov5640_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, ov5640_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe	= __ov5640_probe,
	.driver	= {
		.name	= "msm_camera_ov5640",
		.owner	= THIS_MODULE,
	},
};

static int __init ov5640_init(void)
{
	ov5640_i2c_buf[0] = 0x5A;
	return platform_driver_register(&msm_camera_driver);
}

module_init(ov5640_init);

MODULE_DESCRIPTION("OV5640 YUV MIPI sensor driver");
MODULE_LICENSE("GPL v2");
