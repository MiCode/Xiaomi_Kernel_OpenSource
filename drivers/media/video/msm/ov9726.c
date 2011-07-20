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

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "ov9726.h"

/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/
#define OV9726_Q8				0x00000100
#define OV9726_Q8Shift				8
#define OV9726_Q10				0x00000400
#define OV9726_Q10Shift				10

/* Omnivision8810 product ID register address */
#define	OV9726_PIDH_REG				0x0000
#define	OV9726_PIDL_REG				0x0001
/* Omnivision8810 product ID */
#define	OV9726_PID				0x97
/* Omnivision8810 version */
#define	OV9726_VER				0x26
/* Time in milisecs for waiting for the sensor to reset */
#define	OV9726_RESET_DELAY_MSECS		66
#define	OV9726_DEFAULT_CLOCK_RATE		24000000
/* Registers*/
#define	OV9726_GAIN				0x3000
#define	OV9726_AEC_MSB				0x3002
#define	OV9726_AEC_LSB				0x3003

/* Color bar pattern selection */
#define OV9726_COLOR_BAR_PATTERN_SEL_REG	0x600
/* Color bar enabling control */
#define OV9726_COLOR_BAR_ENABLE_REG		0x601
/* Time in milisecs for waiting for the sensor to reset*/
#define OV9726_RESET_DELAY_MSECS		66
/* I2C Address of the Sensor */
/*============================================================================
		DATA DECLARATIONS
============================================================================*/
#define OV9726_FULL_SIZE_DUMMY_PIXELS		0
#define OV9726_FULL_SIZE_DUMMY_LINES		0
#define OV9726_QTR_SIZE_DUMMY_PIXELS		0
#define OV9726_QTR_SIZE_DUMMY_LINES		0

#define OV9726_FULL_SIZE_WIDTH			1296
#define OV9726_FULL_SIZE_HEIGHT			808

#define OV9726_QTR_SIZE_WIDTH			1296
#define OV9726_QTR_SIZE_HEIGHT			808

#define OV9726_HRZ_FULL_BLK_PIXELS		368
#define OV9726_VER_FULL_BLK_LINES		32
#define OV9726_HRZ_QTR_BLK_PIXELS		368
#define OV9726_VER_QTR_BLK_LINES		32

#define OV9726_MSB_MASK			0xFF00
#define OV9726_LSB_MASK			0x00FF

struct ov9726_work_t {
	struct work_struct work;
};
static struct ov9726_work_t *ov9726_sensorw;
static struct i2c_client *ov9726_client;
struct ov9726_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;
	uint32_t sensormode;
	uint32_t fps_divider;		/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;	/* init to 1 * 0x00000400 */
	uint16_t fps;
	int16_t curr_lens_pos;
	uint16_t curr_step_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint16_t total_lines_per_frame;
	enum ov9726_resolution_t prev_res;
	enum ov9726_resolution_t pict_res;
	enum ov9726_resolution_t curr_res;
	enum ov9726_test_mode_t  set_test;
	unsigned short imgaddr;
};
static struct ov9726_ctrl_t *ov9726_ctrl;
static int8_t config_not_set = 1;
static DECLARE_WAIT_QUEUE_HEAD(ov9726_wait_queue);
DEFINE_MUTEX(ov9726_mut);

/*=============================================================*/
static int ov9726_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr  = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr  = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

	if (i2c_transfer(ov9726_client->adapter, msgs, 2) < 0) {
		CDBG("ov9726_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t ov9726_i2c_txdata(unsigned short saddr,
				unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
		 .addr = saddr ,
		 .flags = 0,
		 .len = length,
		 .buf = txdata,
		 },
	};

	if (i2c_transfer(ov9726_client->adapter, msg, 1) < 0) {
		CDBG("ov9726_i2c_txdata faild 0x%x\n", ov9726_client->addr);
		return -EIO;
	}

	return 0;
}

static int32_t ov9726_i2c_read(unsigned short raddr,
				unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[2];

	if (!rdata)
		return -EIO;

	buf[0] = (raddr & OV9726_MSB_MASK) >> 8;
	buf[1] = (raddr & OV9726_LSB_MASK);

	rc = ov9726_i2c_rxdata(ov9726_client->addr, buf, rlen);

	if (rc < 0) {
		CDBG("ov9726_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}

	*rdata = (rlen == 2 ? buf[0] << 8 | buf[1] : buf[0]);
	return rc;
}

static int32_t ov9726_i2c_write_b(unsigned short saddr,
	unsigned short waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[3];

	buf[0] = (waddr & OV9726_MSB_MASK) >> 8;
	buf[1] = (waddr & OV9726_LSB_MASK);
	buf[2] = bdata;

	CDBG("i2c_write_b addr = 0x%x, val = 0x%xd\n", waddr, bdata);
	rc = ov9726_i2c_txdata(saddr, buf, 3);

	if (rc < 0) {
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			 waddr, bdata);
	}

	return rc;
}

static void ov9726_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	uint32_t divider;	/*Q10 */
	uint32_t d1;
	uint32_t d2;
	uint16_t snapshot_height, preview_height, preview_width, snapshot_width;
	if (ov9726_ctrl->prev_res == QTR_SIZE) {
		preview_width = OV9726_QTR_SIZE_WIDTH +
			OV9726_HRZ_QTR_BLK_PIXELS ;
		preview_height = OV9726_QTR_SIZE_HEIGHT +
			OV9726_VER_QTR_BLK_LINES ;
	} else {
		/* full size resolution used for preview. */
		preview_width = OV9726_FULL_SIZE_WIDTH +
			OV9726_HRZ_FULL_BLK_PIXELS ;
		preview_height = OV9726_FULL_SIZE_HEIGHT +
			OV9726_VER_FULL_BLK_LINES ;
	}
	if (ov9726_ctrl->pict_res == QTR_SIZE) {
		snapshot_width  = OV9726_QTR_SIZE_WIDTH +
			OV9726_HRZ_QTR_BLK_PIXELS ;
		snapshot_height = OV9726_QTR_SIZE_HEIGHT +
			OV9726_VER_QTR_BLK_LINES ;
	} else {
		snapshot_width  = OV9726_FULL_SIZE_WIDTH +
			OV9726_HRZ_FULL_BLK_PIXELS;
		snapshot_height = OV9726_FULL_SIZE_HEIGHT +
			OV9726_VER_FULL_BLK_LINES;
	}

	d1 = (uint32_t)(((uint32_t)preview_height <<
		OV9726_Q10Shift) /
		snapshot_height);

	d2 = (uint32_t)(((uint32_t)preview_width <<
		OV9726_Q10Shift) /
		 snapshot_width);

	divider = (uint32_t) (d1 * d2) >> OV9726_Q10Shift;
	*pfps = (uint16_t)((uint32_t)(fps * divider) >> OV9726_Q10Shift);
}

static uint16_t ov9726_get_prev_lines_pf(void)
{
	if (ov9726_ctrl->prev_res == QTR_SIZE)
		return OV9726_QTR_SIZE_HEIGHT + OV9726_VER_QTR_BLK_LINES;
	else
		return OV9726_FULL_SIZE_HEIGHT + OV9726_VER_FULL_BLK_LINES;
}

static uint16_t ov9726_get_prev_pixels_pl(void)
{
	if (ov9726_ctrl->prev_res == QTR_SIZE)
		return OV9726_QTR_SIZE_WIDTH + OV9726_HRZ_QTR_BLK_PIXELS;
	else
		return OV9726_FULL_SIZE_WIDTH + OV9726_HRZ_FULL_BLK_PIXELS;
}

static uint16_t ov9726_get_pict_lines_pf(void)
{
	if (ov9726_ctrl->pict_res == QTR_SIZE)
		return OV9726_QTR_SIZE_HEIGHT + OV9726_VER_QTR_BLK_LINES;
	else
		return OV9726_FULL_SIZE_HEIGHT + OV9726_VER_FULL_BLK_LINES;
}

static uint16_t ov9726_get_pict_pixels_pl(void)
{
	if (ov9726_ctrl->pict_res == QTR_SIZE)
		return OV9726_QTR_SIZE_WIDTH + OV9726_HRZ_QTR_BLK_PIXELS;
	else
		return OV9726_FULL_SIZE_WIDTH + OV9726_HRZ_FULL_BLK_PIXELS;
}

static uint32_t ov9726_get_pict_max_exp_lc(void)
{
	if (ov9726_ctrl->pict_res == QTR_SIZE)
		return (OV9726_QTR_SIZE_HEIGHT + OV9726_VER_QTR_BLK_LINES)*24;
	else
		return (OV9726_FULL_SIZE_HEIGHT + OV9726_VER_FULL_BLK_LINES)*24;
}

static int32_t ov9726_set_fps(struct fps_cfg	*fps)
{
	int32_t rc = 0;
	CDBG("%s: fps->fps_div = %d\n", __func__, fps->fps_div);
	/* TODO: Passing of fps_divider from user space has issues. */
	/* ov9726_ctrl->fps_divider = fps->fps_div; */
	ov9726_ctrl->fps_divider = 1 * 0x400;
	CDBG("%s: ov9726_ctrl->fps_divider = %d\n", __func__,
		ov9726_ctrl->fps_divider);
	ov9726_ctrl->pict_fps_divider = fps->pict_fps_div;
	ov9726_ctrl->fps = fps->f_mult;
	return rc;
}

static int32_t ov9726_write_exp_gain(uint16_t gain, uint32_t line)
{
	static uint16_t max_legal_gain = 0x00FF;
	uint8_t gain_msb, gain_lsb;
	uint8_t intg_time_msb, intg_time_lsb;
	uint8_t ov9726_offset = 6;
	uint8_t line_length_pck_msb, line_length_pck_lsb;
	uint16_t line_length_pck, frame_length_lines;
	uint32_t line_length_ratio = 1 << OV9726_Q8Shift;
	int32_t rc = -1;
	CDBG("%s: gain = %d	line = %d", __func__, gain, line);

	if (ov9726_ctrl->sensormode != SENSOR_SNAPSHOT_MODE) {
		if (ov9726_ctrl->curr_res == QTR_SIZE) {
			frame_length_lines = OV9726_QTR_SIZE_HEIGHT +
			 OV9726_VER_QTR_BLK_LINES;
			line_length_pck = OV9726_QTR_SIZE_WIDTH	+
			 OV9726_HRZ_QTR_BLK_PIXELS;
		} else {
			frame_length_lines = OV9726_FULL_SIZE_HEIGHT +
				OV9726_VER_FULL_BLK_LINES;
			line_length_pck = OV9726_FULL_SIZE_WIDTH +
				OV9726_HRZ_FULL_BLK_PIXELS;
		}
		if (line > (frame_length_lines - ov9726_offset))
			ov9726_ctrl->fps = (uint16_t) (((uint32_t)30 <<
				OV9726_Q8Shift) *
				(frame_length_lines - ov9726_offset) / line);
		else
			ov9726_ctrl->fps = (uint16_t) ((uint32_t)30 <<
				OV9726_Q8Shift);
	} else {
		frame_length_lines = OV9726_FULL_SIZE_HEIGHT +
			OV9726_VER_FULL_BLK_LINES;
		line_length_pck = OV9726_FULL_SIZE_WIDTH +
			OV9726_HRZ_FULL_BLK_PIXELS;
	}

	if (ov9726_ctrl->sensormode != SENSOR_SNAPSHOT_MODE) {
		line = (uint32_t) (line * ov9726_ctrl->fps_divider) >>
			OV9726_Q10Shift;
	} else {
		line = (uint32_t) (line * ov9726_ctrl->pict_fps_divider) >>
			OV9726_Q10Shift;
	}

	/* calculate line_length_ratio */
	if (line > (frame_length_lines - ov9726_offset)) {
		line_length_ratio = (line << OV9726_Q8Shift) /
			(frame_length_lines - ov9726_offset);
		line = frame_length_lines - ov9726_offset;
	} else
		line_length_ratio = (uint32_t)1 << OV9726_Q8Shift;

	if (gain > max_legal_gain) {
		/* range:	0	to 224 */
		gain = max_legal_gain;
	}
	/* update	gain registers */
	gain_msb = (uint8_t) ((gain & 0xFF00) >> 8);
	gain_lsb = (uint8_t) (gain & 0x00FF);
	/* linear	AFR	horizontal stretch */
	line_length_pck = (uint16_t) ((line_length_pck *
		line_length_ratio) >> OV9726_Q8Shift);
	line_length_pck_msb = (uint8_t) ((line_length_pck & 0xFF00) >> 8);
	line_length_pck_lsb = (uint8_t) (line_length_pck & 0x00FF);
	/* update	line count registers */
	intg_time_msb = (uint8_t) ((line & 0xFF00) >> 8);
	intg_time_lsb = (uint8_t) (line	& 0x00FF);

	rc = ov9726_i2c_write_b(ov9726_client->addr, 0x104, 0x1);
	if (rc < 0)
		return rc;

	rc = ov9726_i2c_write_b(ov9726_client->addr, 0x204, gain_msb);
	if (rc < 0)
		return rc;

	rc = ov9726_i2c_write_b(ov9726_client->addr, 0x205, gain_lsb);
	if (rc < 0)
		return rc;

	rc = ov9726_i2c_write_b(ov9726_client->addr, 0x342,
		line_length_pck_msb);
	if (rc < 0)
		return rc;

	rc = ov9726_i2c_write_b(ov9726_client->addr, 0x343,
		line_length_pck_lsb);
	if (rc < 0)
		return rc;

	rc = ov9726_i2c_write_b(ov9726_client->addr, 0x0202, intg_time_msb);
	if (rc < 0)
		return rc;

	rc = ov9726_i2c_write_b(ov9726_client->addr, 0x0203, intg_time_lsb);
	if (rc < 0)
		return rc;

	rc = ov9726_i2c_write_b(ov9726_client->addr, 0x104, 0x0);
	if (rc < 0)
		return rc;

	return rc;
}

static int32_t ov9726_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;
	rc = ov9726_write_exp_gain(gain, line);
	return rc;
}

static int32_t initialize_ov9726_registers(void)
{
	int32_t i;
	int32_t rc = 0;
	ov9726_ctrl->sensormode = SENSOR_PREVIEW_MODE ;
	/* Configure sensor for Preview mode and Snapshot mode */
	CDBG("Initialize_ov9726_registers\n");
	for (i = 0; i < ov9726_array_length; i++) {
		rc = ov9726_i2c_write_b(ov9726_client->addr,
			ov9726_init_settings_array[i].reg_addr,
			ov9726_init_settings_array[i].reg_val);
	if (rc < 0)
		return rc;
	}
	return rc;
}

static int32_t ov9726_video_config(int mode)
{
	int32_t rc = 0;

	ov9726_ctrl->sensormode = mode;

	if (config_not_set) {
		struct msm_camera_csi_params ov9726_csi_params;

		/* sensor in standby */
		ov9726_i2c_write_b(ov9726_client->addr, 0x100, 0);
		msleep(5);
		/* Initialize Sensor registers */
		ov9726_csi_params.data_format = CSI_10BIT;
		ov9726_csi_params.lane_cnt = 1;
		ov9726_csi_params.lane_assign = 0xe4;
		ov9726_csi_params.dpcm_scheme = 0;
		ov9726_csi_params.settle_cnt = 7;

		rc = msm_camio_csi_config(&ov9726_csi_params);
		rc = initialize_ov9726_registers();
		config_not_set = 0;
	}
	return rc;
}

static int32_t ov9726_snapshot_config(int mode)
{
	int32_t rc = 0;
	ov9726_ctrl->sensormode = mode;
	return rc;
}

static int32_t ov9726_raw_snapshot_config(int mode)
{
	int32_t rc = 0;
	ov9726_ctrl->sensormode = mode;
	return rc;
}

static int32_t ov9726_set_sensor_mode(int  mode,
			int  res)
{
	int32_t rc = 0;
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = ov9726_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
		rc = ov9726_snapshot_config(mode);
		break;
	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = ov9726_raw_snapshot_config(mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int ov9726_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	uint16_t  chipidl, chipidh;

	if (data->sensor_reset_enable) {
		rc = gpio_request(data->sensor_reset, "ov9726");
		if (!rc) {
			gpio_direction_output(data->sensor_reset, 0);
			gpio_set_value_cansleep(data->sensor_reset, 1);
			msleep(20);
		} else
			goto init_probe_done;
	}
	/* 3. Read sensor Model ID: */
	rc = ov9726_i2c_read(OV9726_PIDH_REG, &chipidh, 1);
	if (rc < 0)
		goto init_probe_fail;
	rc = ov9726_i2c_read(OV9726_PIDL_REG, &chipidl, 1);
	if (rc < 0)
		goto init_probe_fail;
	CDBG("kov9726 model_id = 0x%x  0x%x\n", chipidh, chipidl);
	/* 4. Compare sensor ID to OV9726 ID: */
	if (chipidh != OV9726_PID) {
		rc = -ENODEV;
		printk(KERN_INFO "Probeinit fail\n");
		goto init_probe_fail;
	}
	CDBG("chipidh == OV9726_PID\n");
	msleep(OV9726_RESET_DELAY_MSECS);
	CDBG("after delay\n");
	goto init_probe_done;

init_probe_fail:
	if (data->sensor_reset_enable) {
		gpio_direction_output(data->sensor_reset, 0);
		gpio_free(data->sensor_reset);
	}
init_probe_done:
	printk(KERN_INFO " ov9726_probe_init_sensor finishes\n");
	return rc;
}

int ov9726_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t  rc;

	CDBG("Calling ov9726_sensor_open_init\n");
	ov9726_ctrl = kzalloc(sizeof(struct ov9726_ctrl_t), GFP_KERNEL);
	if (!ov9726_ctrl) {
		CDBG("ov9726_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	ov9726_ctrl->curr_lens_pos = -1;
	ov9726_ctrl->fps_divider = 1 << OV9726_Q10Shift;
	ov9726_ctrl->pict_fps_divider = 1 << OV9726_Q10Shift;
	ov9726_ctrl->set_test = TEST_OFF;
	ov9726_ctrl->prev_res = FULL_SIZE;
	ov9726_ctrl->pict_res = FULL_SIZE;
	ov9726_ctrl->curr_res = INVALID_SIZE;
	config_not_set = 1;
	if (data)
		ov9726_ctrl->sensordata = data;
	/* enable mclk first */
	msm_camio_clk_rate_set(OV9726_DEFAULT_CLOCK_RATE);
	msleep(20);
	rc = ov9726_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;

	ov9726_ctrl->fps = (uint16_t)(30 << OV9726_Q8Shift);
	/* generate test pattern */
	if (rc < 0)
		goto init_fail;
	else
		goto init_done;
	/* reset the driver state */
init_fail:
	CDBG(" init_fail\n");
	kfree(ov9726_ctrl);
init_done:
	CDBG("init_done\n");
	return rc;
}

static int ov9726_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&ov9726_wait_queue);
	return 0;
}

static const struct i2c_device_id ov9726_i2c_id[] = {
	{ "ov9726", 0},
	{ }
};

static int ov9726_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("ov9726_probe called!\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}
	ov9726_sensorw = kzalloc(sizeof(struct ov9726_work_t), GFP_KERNEL);
	if (!ov9726_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}
	i2c_set_clientdata(client, ov9726_sensorw);
	ov9726_init_client(client);
	ov9726_client = client;
	msleep(50);
	CDBG("ov9726_probe successed! rc = %d\n", rc);
	return 0;
probe_failure:
	CDBG("ov9726_probe failed! rc = %d\n", rc);
	return rc;
}

static int __exit ov9726_remove(struct i2c_client *client)
{
	struct ov9726_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	ov9726_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver ov9726_i2c_driver = {
	.id_table = ov9726_i2c_id,
	.probe	= ov9726_i2c_probe,
	.remove = __exit_p(ov9726_i2c_remove),
	.driver = {
		.name = "ov9726",
	},
};

int ov9726_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&ov9726_mut);
	CDBG("ov9726_sensor_config: cfgtype = %d\n",
		cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_GET_PICT_FPS:
		ov9726_get_pict_fps(cdata.cfg.gfps.prevfps,
				&(cdata.cfg.gfps.pictfps));
		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
			break;
	case CFG_GET_PREV_L_PF:
		cdata.cfg.prevl_pf = ov9726_get_prev_lines_pf();
		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PREV_P_PL:
		cdata.cfg.prevp_pl = ov9726_get_prev_pixels_pl();
		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PICT_L_PF:
		cdata.cfg.pictl_pf = ov9726_get_pict_lines_pf();
		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PICT_P_PL:
		cdata.cfg.pictp_pl =
				ov9726_get_pict_pixels_pl();
		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PICT_MAX_EXP_LC:
		cdata.cfg.pict_max_exp_lc = ov9726_get_pict_max_exp_lc();
		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_SET_FPS:
	case CFG_SET_PICT_FPS:
		rc = ov9726_set_fps(&(cdata.cfg.fps));
		break;
	case CFG_SET_EXP_GAIN:
		rc = ov9726_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
		break;
	case CFG_SET_PICT_EXP_GAIN:
		rc = ov9726_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
		break;
	case CFG_SET_MODE:
		rc = ov9726_set_sensor_mode(cdata.mode,
						cdata.rs);
		break;
	case CFG_PWR_DOWN:
	case CFG_MOVE_FOCUS:
	case CFG_SET_DEFAULT_FOCUS:
		rc = 0;
		break;
	case CFG_SET_EFFECT:
	default:
		rc = -EFAULT;
		break;
	}
	mutex_unlock(&ov9726_mut);
	return rc;
}

static int ov9726_probe_init_done(const struct msm_camera_sensor_info *data)
{
	if (data->sensor_reset_enable) {
		gpio_direction_output(data->sensor_reset, 0);
		gpio_free(data->sensor_reset);
	}
	return 0;
}

static int ov9726_sensor_release(void)
{
	int rc = -EBADF;
	mutex_lock(&ov9726_mut);
	if (ov9726_ctrl->sensordata->sensor_reset_enable) {
		gpio_direction_output(
			ov9726_ctrl->sensordata->sensor_reset, 0);
		gpio_free(ov9726_ctrl->sensordata->sensor_reset);
	}
	kfree(ov9726_ctrl);
	ov9726_ctrl = NULL;
	CDBG("ov9726_release completed\n");
	mutex_unlock(&ov9726_mut);
	return rc;
}

static int ov9726_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;

	rc = i2c_add_driver(&ov9726_i2c_driver);
	if (rc < 0 || ov9726_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_fail;
	}
	msm_camio_clk_rate_set(24000000);
	msleep(20);
	rc = ov9726_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;

	s->s_init = ov9726_sensor_open_init;
	s->s_release = ov9726_sensor_release;
	s->s_config  = ov9726_sensor_config;
	s->s_camera_type = FRONT_CAMERA_2D;
	s->s_mount_angle = info->sensor_platform_info->mount_angle;
	ov9726_probe_init_done(info);

	return rc;

probe_fail:
	CDBG("SENSOR PROBE FAILS!\n");
	return rc;
}

static int __ov9726_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, ov9726_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov9726_probe,
	.driver = {
		.name = "msm_camera_ov9726",
		.owner = THIS_MODULE,
	},
};

static int __init ov9726_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(ov9726_init);
void ov9726_exit(void)
{
	i2c_del_driver(&ov9726_i2c_driver);
}

MODULE_DESCRIPTION("OMNI VGA Bayer sensor driver");
MODULE_LICENSE("GPL v2");

