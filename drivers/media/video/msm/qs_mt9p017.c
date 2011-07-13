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
 *
 */

#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <media/v4l2-subdev.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "qs_mt9p017.h"
#include "msm.h"
/*
* =============================================================
*	SENSOR REGISTER DEFINES
* ==============================================================
*/
#define REG_GROUPED_PARAMETER_HOLD		0x0104
#define GROUPED_PARAMETER_HOLD_OFF		0x00
#define GROUPED_PARAMETER_HOLD			0x01
/* Integration Time */
#define REG_COARSE_INTEGRATION_TIME		0x3012
/* Gain */
#define REG_GLOBAL_GAIN					0x305E
#define REG_GR_GAIN						0x3056
#define REG_R_GAIN						0x3058
#define REG_B_GAIN						0x305A
#define REG_GB_GAIN						0x305C

/* PLL registers */
#define REG_FRAME_LENGTH_LINES			0x300A
#define REG_LINE_LENGTH_PCK				0x300C
/* Test Pattern */
#define REG_TEST_PATTERN_MODE			0x0601
#define REG_VCM_NEW_CODE				0x30F2
#define AF_ADDR							0x18
#define BRIDGE_ADDR						0x80
/*
* ============================================================================
*							 TYPE DECLARATIONS
* ============================================================================
*/

/* 16bit address - 8 bit context register structure */
#define Q8  0x00000100
#define Q10 0x00000400
#define QS_MT9P017_MASTER_CLK_RATE 24000000
#define QS_MT9P017_OFFSET			8

/* AF Total steps parameters */
#define QS_MT9P017_TOTAL_STEPS_NEAR_TO_FAR    32
#define QS_MT9P017_NL_REGION_BOUNDARY 3
#define QS_MT9P017_NL_REGION_CODE_PER_STEP 30
#define QS_MT9P017_L_REGION_CODE_PER_STEP 4

#define QS_MT9P017_BRIDGE_DBG 0

static struct i2c_client *qs_mt9p017_client;

struct qs_mt9p017_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	u16 fmt;
	u16 order;
};

struct qs_mt9p017_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;

	uint32_t sensormode;
	uint32_t fps_divider;/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;/* init to 1 * 0x00000400 */
	uint16_t fps;

	uint16_t curr_lens_pos;
	uint16_t curr_step_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint16_t total_lines_per_frame;

	enum qs_mt9p017_resolution_t prev_res;
	enum qs_mt9p017_resolution_t pict_res;
	enum qs_mt9p017_resolution_t curr_res;
	enum qs_mt9p017_cam_mode_t cam_mode;
	struct v4l2_subdev *sensor_dev;
	struct qs_mt9p017_format *fmt;
	uint16_t qs_mt9p017_step_position_table
		[QS_MT9P017_TOTAL_STEPS_NEAR_TO_FAR+1];
	uint16_t prev_line_length_pck;
	uint16_t prev_frame_length_lines;
	uint16_t snap_line_length_pck;
	uint16_t snap_frame_length_lines;
};

static bool CSI_CONFIG;
static struct qs_mt9p017_ctrl_t *qs_mt9p017_ctrl;
DEFINE_MUTEX(qs_mt9p017_mut);
/*=============================================================*/

static int qs_mt9p017_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = length,
			.buf   = rxdata,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxdata,
		},
	};
	if (i2c_transfer(qs_mt9p017_client->adapter, msgs, 2) < 0) {
		CDBG("qs_mt9p017_i2c_rxdata faild 0x%x\n", saddr);
		return -EIO;
	}
	return 0;
}

static int32_t qs_mt9p017_i2c_txdata(unsigned short saddr,
				unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		 },
	};
	if (i2c_transfer(qs_mt9p017_client->adapter, msg, 1) < 0) {
		CDBG("qs_mt9p017_i2c_txdata faild 0x%x\n", saddr);
		return -EIO;
	}

	return 0;
}

static int32_t qs_mt9p017_i2c_read(unsigned short raddr,
	unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[] = {
		raddr >> BITS_PER_BYTE,
		raddr,
	};
	if (!rdata)
		return -EIO;
	rc = qs_mt9p017_i2c_rxdata(qs_mt9p017_client->addr, buf, rlen);
	if (rc < 0) {
		CDBG("qs_mt9p017_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = (rlen == 2 ? buf[0] << BITS_PER_BYTE | buf[1] : buf[0]);
	CDBG("qs_mt9p017_i2c_read 0x%x val = 0x%x!\n", raddr, *rdata);
	return rc;
}

static int32_t qs_mt9p017_i2c_write_w_sensor(unsigned short waddr,
	uint16_t wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[] = {
		waddr >> BITS_PER_BYTE,
		waddr,
		wdata >> BITS_PER_BYTE,
		wdata,
	};
	CDBG("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, wdata);
	rc = qs_mt9p017_i2c_txdata(qs_mt9p017_client->addr, buf, 4);
	if (rc < 0) {
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			waddr, wdata);
	}
	return rc;
}

static int32_t qs_mt9p017_i2c_write_b_sensor(unsigned short waddr,
	uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[] = {
		waddr >> BITS_PER_BYTE,
		waddr,
		bdata,
	};
	CDBG("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, bdata);
	rc = qs_mt9p017_i2c_txdata(qs_mt9p017_client->addr, buf, 3);
	if (rc < 0) {
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			waddr, bdata);
	}
	return rc;
}

static int32_t qs_mt9p017_i2c_write_seq_sensor(unsigned short waddr,
		unsigned char *seq_data, int len)
{
	int32_t rc = -EFAULT;
	unsigned char buf[len+2];
	int i = 0;
	buf[0] = waddr >> BITS_PER_BYTE;
	buf[1] = waddr;
	for (i = 0; i < len; i++)
		buf[i+2] = seq_data[i];
	rc = qs_mt9p017_i2c_txdata(qs_mt9p017_client->addr, buf, len+2);
	return rc;
}

static int32_t qs_mt9p017_i2c_write_w_table(struct qs_mt9p017_i2c_reg_conf const
					 *reg_conf_tbl, int num)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < num; i++) {
		rc = qs_mt9p017_i2c_write_w_sensor(reg_conf_tbl->waddr,
			reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

static int32_t bridge_i2c_write_w(unsigned short waddr, uint16_t wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[] = {
		waddr >> BITS_PER_BYTE,
		waddr,
		wdata >> BITS_PER_BYTE,
		wdata,
	};
	CDBG("bridge_i2c_write_w addr = 0x%x, val = 0x%x\n", waddr, wdata);
	rc = qs_mt9p017_i2c_txdata(BRIDGE_ADDR>>1, buf, 4);
	if (rc < 0) {
		CDBG("bridge_i2c_write_w failed, addr = 0x%x, val = 0x%x!\n",
			waddr, wdata);
	}
	return rc;
}

static int32_t bridge_i2c_read(unsigned short raddr,
	unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[] = {
		raddr >> BITS_PER_BYTE,
		raddr,
	};
	if (!rdata)
		return -EIO;
	rc = qs_mt9p017_i2c_rxdata(BRIDGE_ADDR>>1, buf, rlen);
	if (rc < 0) {
		CDBG("bridge_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = (rlen == 2 ? buf[0] << BITS_PER_BYTE | buf[1] : buf[0]);
	CDBG("bridge_i2c_read 0x%x val = 0x%x!\n", raddr, *rdata);
	return rc;
}

static int32_t qs_mt9p017_eeprom_i2c_read(unsigned short raddr,
	unsigned char *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned short i2caddr = 0xA0 >> 1;
	uint8_t block_num = 0;
	unsigned char buf[rlen+2];
	int i = 0;
	if (!rdata)
		return -EIO;

	block_num = raddr >> BITS_PER_BYTE;
	i2caddr |= block_num;

	buf[0] = raddr >> BITS_PER_BYTE;
	buf[1] = raddr;
	rc = qs_mt9p017_i2c_rxdata(i2caddr, buf, rlen);
	if (rc < 0) {
		CDBG("qs_mt9p017_eeprom_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	for (i = 0; i < rlen; i++) {
		rdata[i] = buf[i];
		CDBG("qs_mt9p017_eeprom_i2c_read 0x%x index: %d val = 0x%x!\n",
			raddr, i, buf[i]);
	}
	return rc;
}

static int32_t qs_mt9p017_eeprom_i2c_read_b(unsigned short raddr,
	unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[2];
	rc = qs_mt9p017_eeprom_i2c_read(raddr, &buf[0], rlen);
	*rdata = (rlen == 2 ? buf[0] << BITS_PER_BYTE | buf[1] : buf[0]);
	CDBG("qs_mt9p017_eeprom_i2c_read 0x%x val = 0x%x!\n", raddr, *rdata);
	return rc;
}

static int32_t qs_mt9p017_get_calibration_data(
	struct sensor_3d_cali_data_t *cdata)
{
	struct qs_mt9p017_i2c_read_seq_t eeprom_read_seq_tbl[] = {
		{0x0, &(cdata->left_p_matrix[0][0][0]), 96},
		{0x60, &(cdata->right_p_matrix[0][0][0]), 96},
		{0xC0, &(cdata->square_len[0]), 8},
		{0xC8, &(cdata->focal_len[0]), 8},
		{0xD0, &(cdata->pixel_pitch[0]), 8},
	};

	struct qs_mt9p017_i2c_read_t eeprom_read_tbl[] = {
		{0x100, &(cdata->left_r), 1},
		{0x101, &(cdata->right_r), 1},
		{0x102, &(cdata->left_b), 1},
		{0x103, &(cdata->right_b), 1},
		{0x104, &(cdata->left_gb), 1},
		{0x105, &(cdata->right_gb), 1},
		{0x110, &(cdata->left_af_far), 2},
		{0x112, &(cdata->right_af_far), 2},
		{0x114, &(cdata->left_af_mid), 2},
		{0x116, &(cdata->right_af_mid), 2},
		{0x118, &(cdata->left_af_short), 2},
		{0x11A, &(cdata->right_af_short), 2},
		{0x11C, &(cdata->left_af_5um), 2},
		{0x11E, &(cdata->right_af_5um), 2},
		{0x120, &(cdata->left_af_50up), 2},
		{0x122, &(cdata->right_af_50up), 2},
		{0x124, &(cdata->left_af_50down), 2},
		{0x126, &(cdata->right_af_50down), 2},
	};

	int i;
	for (i = 0; i < ARRAY_SIZE(eeprom_read_seq_tbl); i++) {
		qs_mt9p017_eeprom_i2c_read(
			eeprom_read_seq_tbl[i].raddr,
			eeprom_read_seq_tbl[i].rdata,
			eeprom_read_seq_tbl[i].rlen);
	}

	for (i = 0; i < ARRAY_SIZE(eeprom_read_tbl); i++) {
		qs_mt9p017_eeprom_i2c_read_b(
			eeprom_read_tbl[i].raddr,
			eeprom_read_tbl[i].rdata,
			eeprom_read_tbl[i].rlen);
	}
	return 0;
}

static int32_t qs_mt9p017_load_left_lsc(void)
{
	int i;
	unsigned char left_lsc1[210];
	unsigned short left_origin_c, left_origin_r;
	struct qs_mt9p017_i2c_reg_conf lsc_conf[] = {
		{0x37C0, 0x0000},
		{0x37C2, 0x0000},
		{0x37C4, 0x0000},
		{0x37C6, 0x0000},
		{0x3780, 0x8000},
	};
	qs_mt9p017_eeprom_i2c_read(0x200, &left_lsc1[0], 126);
	qs_mt9p017_eeprom_i2c_read_b(0x27E, &left_origin_c, 2);
	qs_mt9p017_eeprom_i2c_read_b(0x280, &left_origin_r, 2);
	bridge_i2c_write_w(0x06, 0x01);
	qs_mt9p017_i2c_write_seq_sensor(0x3600, left_lsc1, 126);
	qs_mt9p017_i2c_write_w_sensor(0x3782, left_origin_c);
	qs_mt9p017_i2c_write_w_sensor(0x3784, left_origin_r);
	for (i = 0; i < ARRAY_SIZE(lsc_conf); i++) {
		qs_mt9p017_i2c_write_w_sensor(
			lsc_conf[i].waddr, lsc_conf[i].wdata);
	}
	return 0;
}

static int32_t qs_mt9p017_load_right_lsc(void)
{
	int i;
	unsigned char right_lsc1[210];
	unsigned short right_origin_c, right_origin_r;
	struct qs_mt9p017_i2c_reg_conf lsc_conf[] = {
		{0x37C0, 0x0000},
		{0x37C2, 0x0000},
		{0x37C4, 0x0000},
		{0x37C6, 0x0000},
		{0x3780, 0x8000},
	};
	qs_mt9p017_eeprom_i2c_read(0x2D2, &right_lsc1[0], 126);
	qs_mt9p017_eeprom_i2c_read_b(0x350, &right_origin_c, 2);
	qs_mt9p017_eeprom_i2c_read_b(0x352, &right_origin_r, 2);
	bridge_i2c_write_w(0x06, 0x02);
	qs_mt9p017_i2c_write_seq_sensor(0x3600, right_lsc1, 126);
	qs_mt9p017_i2c_write_w_sensor(0x3782, right_origin_c);
	qs_mt9p017_i2c_write_w_sensor(0x3784, right_origin_r);

	for (i = 0; i < ARRAY_SIZE(lsc_conf); i++) {
		qs_mt9p017_i2c_write_w_sensor(
			lsc_conf[i].waddr, lsc_conf[i].wdata);
	}
	return 0;
}

static int32_t qs_mt9p017_load_lsc(void)
{
	qs_mt9p017_load_left_lsc();
	qs_mt9p017_load_right_lsc();
	return 0;
}

static int32_t af_i2c_write_b_sensor(unsigned short baddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[2];
	buf[0] = baddr;
	buf[1] = bdata;
	CDBG("af i2c_write_b addr = 0x%x, val = 0x%x\n", baddr, bdata);
	rc = qs_mt9p017_i2c_txdata(AF_ADDR>>1, buf, 2);
	if (rc < 0) {
		CDBG("af i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			baddr, bdata);
	}
	return rc;
}

static void qs_mt9p017_bridge_reset(void){
	unsigned short rstl_state = 0, gpio_state = 0;
	bridge_i2c_write_w(0x50, 0x00);
	bridge_i2c_write_w(0x53, 0x00);
	msleep(30);
	bridge_i2c_write_w(0x53, 0x01);
	msleep(30);
	bridge_i2c_write_w(0x14, 0x0C);
	bridge_i2c_write_w(0x0E, 0xFFFF);

	bridge_i2c_read(0x54, &rstl_state, 2);
	bridge_i2c_write_w(0x54, (rstl_state | 0x1));
	msleep(30);
	bridge_i2c_write_w(0x54, (rstl_state | 0x3));
	bridge_i2c_read(0x54, &rstl_state, 2);
	bridge_i2c_write_w(0x54, (rstl_state | 0x4));
	bridge_i2c_write_w(0x54, (rstl_state | 0xC));

	bridge_i2c_read(0x55, &gpio_state, 2);
	bridge_i2c_write_w(0x55, (gpio_state | 0x1));
	msleep(30);
	bridge_i2c_write_w(0x55, (gpio_state | 0x3));

	bridge_i2c_read(0x55, &gpio_state, 2);
	bridge_i2c_write_w(0x55, (gpio_state | 0x4));
	msleep(30);
	bridge_i2c_write_w(0x55, (gpio_state | 0xC));
	bridge_i2c_read(0x55, &gpio_state, 2);
}

static void qs_mt9p017_bridge_config(int mode, int rt)
{
	if (mode == MODE_3D) {
		bridge_i2c_write_w(0x16, 0x00);
		bridge_i2c_write_w(0x51, 0x3);
		bridge_i2c_write_w(0x52, 0x1);
		bridge_i2c_write_w(0x06, 0x03);
		bridge_i2c_write_w(0x04, 0x6C18);
		bridge_i2c_write_w(0x50, 0x00);
	} else if (mode == MODE_2D_LEFT) {
		bridge_i2c_write_w(0x06, 0x01);
		bridge_i2c_write_w(0x04, 0x6C18);
		bridge_i2c_write_w(0x50, 0x01);
	} else if (mode == MODE_2D_RIGHT) {
		bridge_i2c_write_w(0x06, 0x02);
		bridge_i2c_write_w(0x04, 0x6C18);
		bridge_i2c_write_w(0x50, 0x02);
	}
}

static void qs_mt9p017_group_hold_on(void)
{
	qs_mt9p017_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
						GROUPED_PARAMETER_HOLD);
}

static void qs_mt9p017_group_hold_off(void)
{
	qs_mt9p017_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
						GROUPED_PARAMETER_HOLD_OFF);
}

static void qs_mt9p017_start_stream(void)
{
	qs_mt9p017_i2c_write_w_sensor(0x301A, 0x065C|0x2);
}

static void qs_mt9p017_stop_stream(void)
{
	qs_mt9p017_i2c_write_b_sensor(0x0100, 0x00);
}

static void qs_mt9p017_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint32_t divider, d1, d2;

	d1 = qs_mt9p017_ctrl->prev_frame_length_lines * 0x400
		/ qs_mt9p017_ctrl->snap_frame_length_lines;
	d2 = qs_mt9p017_ctrl->prev_line_length_pck * 0x400
		/ qs_mt9p017_ctrl->snap_line_length_pck;
	divider = d1 * d2 / 0x400;

	/*Verify PCLK settings and frame sizes.*/
	*pfps = (uint16_t) (fps * divider / 0x400);
	/* 2 is the ratio of no.of snapshot channels
	to number of preview channels */
}

static int32_t qs_mt9p017_set_fps(struct fps_cfg   *fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;
	total_lines_per_frame = (uint16_t)
		((qs_mt9p017_ctrl->prev_frame_length_lines) *
		qs_mt9p017_ctrl->fps_divider/0x400);
	qs_mt9p017_ctrl->fps_divider = fps->fps_div;
	qs_mt9p017_ctrl->pict_fps_divider = fps->pict_fps_div;

	qs_mt9p017_group_hold_on();
	rc = qs_mt9p017_i2c_write_w_sensor(REG_FRAME_LENGTH_LINES,
							total_lines_per_frame);
	qs_mt9p017_group_hold_off();
	return rc;
}

static int32_t qs_mt9p017_write_exp_gain(struct sensor_3d_exp_cfg exp_cfg)
{
	uint16_t max_legal_gain = 0xE7F;
	uint16_t gain = exp_cfg.gain;
	uint32_t line = exp_cfg.line;
	int32_t rc = 0;
	if (gain > max_legal_gain) {
		CDBG("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}
	qs_mt9p017_group_hold_on();
	rc = qs_mt9p017_i2c_write_w_sensor(REG_GLOBAL_GAIN, gain|0x1000);
	rc = qs_mt9p017_i2c_write_w_sensor(REG_COARSE_INTEGRATION_TIME, line);
	if (qs_mt9p017_ctrl->cam_mode == MODE_3D) {
		bridge_i2c_write_w(0x06, 0x02);
		rc = qs_mt9p017_i2c_write_w_sensor(REG_GR_GAIN, gain|0x1000);
		rc = qs_mt9p017_i2c_write_w_sensor(REG_R_GAIN,
				exp_cfg.r_gain|0x1000);
		rc = qs_mt9p017_i2c_write_w_sensor(REG_B_GAIN,
				exp_cfg.b_gain|0x1000);
		rc = qs_mt9p017_i2c_write_w_sensor(REG_GB_GAIN,
				exp_cfg.gb_gain|0x1000);
		bridge_i2c_write_w(0x06, 0x03);
	}
	qs_mt9p017_group_hold_off();
	return rc;
}

static int32_t qs_mt9p017_set_pict_exp_gain(struct sensor_3d_exp_cfg exp_cfg)
{
	int32_t rc = 0;
	rc = qs_mt9p017_write_exp_gain(exp_cfg);
	qs_mt9p017_i2c_write_w_sensor(0x301A, 0x065C|0x2);
	return rc;
}

static int32_t qs_mt9p017_move_focus(int direction,
	int32_t num_steps)
{
	int16_t step_direction, actual_step, next_position;
	uint8_t code_val_msb, code_val_lsb;
	if (direction == MOVE_NEAR)
		step_direction = 16;
	else
		step_direction = -16;

	actual_step = (int16_t) (step_direction * (int16_t) num_steps);
	next_position = (int16_t) (qs_mt9p017_ctrl->curr_lens_pos+actual_step);

	if (next_position > 1023)
		next_position = 1023;
	else if (next_position < 0)
		next_position = 0;

	code_val_msb = next_position >> 8;
	code_val_lsb = (next_position & 0x00FF);
	af_i2c_write_b_sensor(0x4, code_val_msb);
	af_i2c_write_b_sensor(0x5, code_val_lsb);

	qs_mt9p017_ctrl->curr_lens_pos = next_position;
	return 0;
}

static int32_t qs_mt9p017_set_default_focus(uint8_t af_step)
{
	int32_t rc = 0;
	if (qs_mt9p017_ctrl->curr_step_pos != 0) {
		rc = qs_mt9p017_move_focus(MOVE_FAR,
		qs_mt9p017_ctrl->curr_step_pos);
	} else {
		af_i2c_write_b_sensor(0x4, 0);
		af_i2c_write_b_sensor(0x5, 0);
	}

	qs_mt9p017_ctrl->curr_lens_pos = 0;
	qs_mt9p017_ctrl->curr_step_pos = 0;

	return rc;
}

static void qs_mt9p017_init_focus(void)
{
	uint8_t i;
	qs_mt9p017_ctrl->qs_mt9p017_step_position_table[0] = 0;
	for (i = 1; i <= QS_MT9P017_TOTAL_STEPS_NEAR_TO_FAR; i++) {
		if (i <= QS_MT9P017_NL_REGION_BOUNDARY) {
			qs_mt9p017_ctrl->qs_mt9p017_step_position_table[i] =
			qs_mt9p017_ctrl->qs_mt9p017_step_position_table[i-1]
				+ QS_MT9P017_NL_REGION_CODE_PER_STEP;
		} else {
			qs_mt9p017_ctrl->qs_mt9p017_step_position_table[i] =
			qs_mt9p017_ctrl->qs_mt9p017_step_position_table[i-1]
				+ QS_MT9P017_L_REGION_CODE_PER_STEP;
		}

		if (qs_mt9p017_ctrl->qs_mt9p017_step_position_table[i] > 255)
			qs_mt9p017_ctrl->
			qs_mt9p017_step_position_table[i] = 255;
	}
}

static int32_t qs_mt9p017_sensor_setting(int update_type, int rt)
{

	int32_t rc = 0, i;
	uint16_t read_data;
	struct msm_camera_csid_params qs_mt9p017_csid_params;
	struct msm_camera_csiphy_params qs_mt9p017_csiphy_params;
	qs_mt9p017_stop_stream();
	msleep(30);
	bridge_i2c_write_w(0x53, 0x00);
	msleep(30);
	if (update_type == REG_INIT) {
		CSI_CONFIG = 0;
		qs_mt9p017_bridge_config(qs_mt9p017_ctrl->cam_mode, rt);
		qs_mt9p017_i2c_write_w_table(qs_mt9p017_regs.rec_settings,
			qs_mt9p017_regs.rec_size);
		if (qs_mt9p017_ctrl->cam_mode == MODE_3D)
			qs_mt9p017_i2c_write_w_table(qs_mt9p017_regs.reg_3d_pll,
			qs_mt9p017_regs.reg_3d_pll_size);
		else
			qs_mt9p017_i2c_write_w_table(qs_mt9p017_regs.reg_pll,
			qs_mt9p017_regs.reg_pll_size);
		qs_mt9p017_i2c_write_w_table(qs_mt9p017_regs.reg_lens,
			qs_mt9p017_regs.reg_lens_size);
		qs_mt9p017_i2c_read(0x31BE, &read_data, 2);
		qs_mt9p017_i2c_write_w_sensor(0x31BE, read_data | 0x4);
	} else if (update_type == UPDATE_PERIODIC) {
		qs_mt9p017_i2c_write_w_table(
			qs_mt9p017_regs.conf_array[rt].conf,
			qs_mt9p017_regs.conf_array[rt].size);

		msleep(20);
		bridge_i2c_write_w(0x53, 0x01);
		msleep(30);
		if (!CSI_CONFIG) {
			if (qs_mt9p017_ctrl->cam_mode == MODE_3D) {
				struct msm_camera_csid_vc_cfg
					qs_mt9p017_vccfg[] = {
					{0, CSI_RAW10, CSI_DECODE_10BIT},
					{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
				};
				qs_mt9p017_csid_params.lane_cnt = 4;
				qs_mt9p017_csiphy_params.lane_cnt = 4;
				qs_mt9p017_csid_params.lut_params.num_cid =
					ARRAY_SIZE(qs_mt9p017_vccfg);
				qs_mt9p017_csid_params.lut_params.vc_cfg =
					&qs_mt9p017_vccfg[0];
			} else {
				struct msm_camera_csid_vc_cfg
					qs_mt9p017_vccfg[] = {
					{0, CSI_RAW10, CSI_DECODE_10BIT},
					{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
				};
				qs_mt9p017_csid_params.lane_cnt = 2;
				qs_mt9p017_csiphy_params.lane_cnt = 2;
				qs_mt9p017_csid_params.lut_params.num_cid =
					ARRAY_SIZE(qs_mt9p017_vccfg);
				qs_mt9p017_csid_params.lut_params.vc_cfg =
					&qs_mt9p017_vccfg[0];
			}
			qs_mt9p017_csid_params.lane_assign = 0xe4;
			qs_mt9p017_csiphy_params.settle_cnt = 0x18;
			rc = msm_camio_csid_config(&qs_mt9p017_csid_params);
			rc = msm_camio_csiphy_config
				(&qs_mt9p017_csiphy_params);
			v4l2_subdev_notify(qs_mt9p017_ctrl->sensor_dev,
					NOTIFY_CID_CHANGE, NULL);
			msleep(100);
			CSI_CONFIG = 1;
		}
		qs_mt9p017_start_stream();
		msleep(30);

		for (i = 0; i < QS_MT9P017_BRIDGE_DBG; i++) {
			bridge_i2c_read(0x10, &read_data, 2);
			CDBG("IRQ Status: 0x%x\n", read_data);
			bridge_i2c_read(0x0A, &read_data, 2);
			CDBG("Skew Value: 0x%x\n", read_data);
		}
	}
	return rc;
}

static int32_t qs_mt9p017_video_config(int mode)
{

	int32_t rc = 0;
	/* change sensor resolution if needed */
	rc = qs_mt9p017_sensor_setting(UPDATE_PERIODIC,
		qs_mt9p017_ctrl->prev_res);
	if (rc < 0)
		return rc;

	qs_mt9p017_ctrl->curr_res = qs_mt9p017_ctrl->prev_res;
	qs_mt9p017_ctrl->sensormode = mode;
	return rc;
}

static int32_t qs_mt9p017_snapshot_config(int mode)
{
	int32_t rc = 0;
	/*change sensor resolution if needed */
	if (qs_mt9p017_ctrl->curr_res != qs_mt9p017_ctrl->pict_res) {
		rc = qs_mt9p017_sensor_setting(UPDATE_PERIODIC,
					qs_mt9p017_ctrl->pict_res);
		if (rc < 0)
			return rc;
	}

	qs_mt9p017_ctrl->curr_res = qs_mt9p017_ctrl->pict_res;
	qs_mt9p017_ctrl->sensormode = mode;
	return rc;
} /*end of qs_mt9p017_snapshot_config*/

static int32_t qs_mt9p017_raw_snapshot_config(int mode)
{
	int32_t rc = 0;
	/* change sensor resolution if needed */
	if (qs_mt9p017_ctrl->curr_res != qs_mt9p017_ctrl->pict_res) {
		rc = qs_mt9p017_sensor_setting(UPDATE_PERIODIC,
					qs_mt9p017_ctrl->pict_res);
		if (rc < 0)
			return rc;
	}

	qs_mt9p017_ctrl->curr_res = qs_mt9p017_ctrl->pict_res;
	qs_mt9p017_ctrl->sensormode = mode;
	return rc;
} /*end of qs_mt9p017_raw_snapshot_config*/

static int32_t qs_mt9p017_mode_init(int mode, struct sensor_init_cfg init_info)
{
	int32_t rc = 0;
	if (mode == qs_mt9p017_ctrl->cam_mode)
		return rc;

	qs_mt9p017_ctrl->prev_res = init_info.prev_res;
	qs_mt9p017_ctrl->pict_res = init_info.pict_res;
	qs_mt9p017_ctrl->cam_mode = mode;

	qs_mt9p017_ctrl->prev_frame_length_lines =
		qs_mt9p017_regs.conf_array[qs_mt9p017_ctrl->prev_res].
		conf[QS_MT9P017_FRAME_LENGTH_LINES].wdata;
	qs_mt9p017_ctrl->prev_line_length_pck =
		qs_mt9p017_regs.conf_array[qs_mt9p017_ctrl->prev_res].
		conf[QS_MT9P017_LINE_LENGTH_PCK].wdata;
	qs_mt9p017_ctrl->snap_frame_length_lines =
		qs_mt9p017_regs.conf_array[qs_mt9p017_ctrl->pict_res].
		conf[QS_MT9P017_FRAME_LENGTH_LINES].wdata;
	qs_mt9p017_ctrl->snap_line_length_pck =
		qs_mt9p017_regs.conf_array[qs_mt9p017_ctrl->pict_res].
		conf[QS_MT9P017_LINE_LENGTH_PCK].wdata;

	if (mode == MODE_2D_LEFT)
		qs_mt9p017_load_left_lsc();
	else if (mode == MODE_2D_RIGHT)
		qs_mt9p017_load_right_lsc();
	else
		qs_mt9p017_load_lsc();

	rc = qs_mt9p017_sensor_setting(REG_INIT,
		qs_mt9p017_ctrl->prev_res);
	return rc;
}

static int32_t qs_mt9p017_set_sensor_mode(int mode, int res)
{
	int32_t rc = 0;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		qs_mt9p017_ctrl->prev_res = res;
		rc = qs_mt9p017_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
		qs_mt9p017_ctrl->pict_res = res;
		rc = qs_mt9p017_snapshot_config(mode);
		break;
	case SENSOR_RAW_SNAPSHOT_MODE:
		qs_mt9p017_ctrl->pict_res = res;
		rc = qs_mt9p017_raw_snapshot_config(mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int32_t qs_mt9p017_check_id(void){
	int rc;
	uint16_t chipid = 0x0;

	rc = qs_mt9p017_i2c_read(0x0000, &chipid, 2);
	if (rc < 0)
		return rc;

	CDBG(KERN_ERR "qs_mt9p017 model_id = 0x%x\n", chipid);
	if (chipid != 0x4800) {
		rc = -ENODEV;
		CDBG("qs_mt9p017 fail chip id doesnot match\n");
	}
	return rc;
}

static int32_t qs_mt9p017_power_up(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	rc = gpio_request(data->sensor_reset, "qs_mt9p017");
	CDBG("%s\n", __func__);
	if (!rc) {
		CDBG("sensor_reset = %d\n", rc);
		gpio_direction_output(data->sensor_reset, 0);
		msleep(50);
		gpio_set_value_cansleep(data->sensor_reset, 1);
		msleep(20);
	} else {
		CDBG("sensor reset fail");
	}
	qs_mt9p017_bridge_reset();
	qs_mt9p017_bridge_config(MODE_3D, RES_PREVIEW);
	msleep(30);
	return rc;
}

static int32_t qs_mt9p017_power_down(const struct msm_camera_sensor_info *data)
{
	gpio_set_value_cansleep(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);
	return 0;
}

static int qs_mt9p017_sensor_open_init
	(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	CDBG("%s: %d\n", __func__, __LINE__);
	qs_mt9p017_ctrl->fps_divider = 1 * 0x00000400;
	qs_mt9p017_ctrl->pict_fps_divider = 1 * 0x00000400;
	qs_mt9p017_ctrl->cam_mode = MODE_INVALID;
	qs_mt9p017_ctrl->fps = 30*Q8;

	if (data)
		qs_mt9p017_ctrl->sensordata = data;

	msm_camio_clk_rate_set(QS_MT9P017_MASTER_CLK_RATE);
	msleep(20);

	rc = qs_mt9p017_power_up(data);
	if (rc < 0) {
		CDBG("Calling qs_mt9p017_sensor_open_init fail\n");
		return rc;
	}

	qs_mt9p017_init_focus();
	return rc;
}

static const struct i2c_device_id qs_mt9p017_i2c_id[] = {
	{"qs_mt9p017", 0},
	{ }
};

static int qs_mt9p017_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("qs_mt9p017_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		rc = -1;
		goto probe_failure;
	}

	qs_mt9p017_client = client;
	CDBG("qs_mt9p017_probe successed! rc = %d\n", rc);
	return rc;

probe_failure:
	CDBG("qs_mt9p017_probe failed! rc = %d\n", rc);
	return rc;
}

static int __exit qs_mt9p017_i2c_remove(struct i2c_client *client)
{
	qs_mt9p017_client = NULL;
	return 0;
}

static struct i2c_driver qs_mt9p017_i2c_driver = {
	.id_table = qs_mt9p017_i2c_id,
	.probe  = qs_mt9p017_i2c_probe,
	.remove = __exit_p(qs_mt9p017_i2c_remove),
	.driver = {
		.name = "qs_mt9p017",
	},
};

static int qs_mt9p017_3D_sensor_config(void __user *argp)
{
	struct sensor_large_data cdata;
	int rc;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_large_data)))
		return -EFAULT;
	mutex_lock(&qs_mt9p017_mut);
	rc = qs_mt9p017_get_calibration_data
		(&cdata.data.sensor_3d_cali_data);
	if (copy_to_user((void *)argp,
		&cdata,
		sizeof(struct sensor_large_data)))
		rc = -EFAULT;
	mutex_unlock(&qs_mt9p017_mut);
	return rc;
}

static int qs_mt9p017_2D_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&qs_mt9p017_mut);
	CDBG("qs_mt9p017_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_GET_PICT_FPS:
		qs_mt9p017_get_pict_fps(
			cdata.cfg.gfps.prevfps,
			&(cdata.cfg.gfps.pictfps));

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PREV_L_PF:
		cdata.cfg.prevl_pf =
			qs_mt9p017_ctrl->prev_frame_length_lines;

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PREV_P_PL:
		cdata.cfg.prevp_pl =
			qs_mt9p017_ctrl->prev_line_length_pck;

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_L_PF:
		cdata.cfg.pictl_pf =
			qs_mt9p017_ctrl->snap_frame_length_lines;

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_P_PL:
		cdata.cfg.pictp_pl =
			qs_mt9p017_ctrl->snap_line_length_pck;

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_MAX_EXP_LC:
		cdata.cfg.pict_max_exp_lc =
			qs_mt9p017_ctrl->snap_frame_length_lines * 24;

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_SET_FPS:
	case CFG_SET_PICT_FPS:
		rc = qs_mt9p017_set_fps(&(cdata.cfg.fps));
		break;

	case CFG_SET_EXP_GAIN:
		rc =
			qs_mt9p017_write_exp_gain(
				cdata.cfg.sensor_3d_exp);
		break;

	case CFG_SET_PICT_EXP_GAIN:
		rc =
			qs_mt9p017_set_pict_exp_gain(
			cdata.cfg.sensor_3d_exp);
		break;

	case CFG_SET_MODE:
		rc = qs_mt9p017_set_sensor_mode(cdata.mode,
				cdata.rs);
		break;

	case CFG_PWR_DOWN:
		rc = qs_mt9p017_power_down(qs_mt9p017_ctrl->sensordata);
		break;

	case CFG_MOVE_FOCUS:
		rc =
			qs_mt9p017_move_focus(
			cdata.cfg.focus.dir,
			cdata.cfg.focus.steps);
		break;

	case CFG_SET_DEFAULT_FOCUS:
		rc =
			qs_mt9p017_set_default_focus(
			cdata.cfg.focus.steps);
		break;

	case CFG_GET_AF_MAX_STEPS:
		cdata.max_steps = QS_MT9P017_TOTAL_STEPS_NEAR_TO_FAR;
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_SET_EFFECT:
		rc = qs_mt9p017_set_default_focus(
			cdata.cfg.effect);
		break;

	case CFG_SENSOR_INIT:
		rc = qs_mt9p017_mode_init(cdata.mode,
				cdata.cfg.init_info);
		break;

	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(&qs_mt9p017_mut);

	return rc;
}

static int qs_mt9p017_sensor_config(void __user *argp)
{
	int cfgtype;
	if (copy_from_user(&cfgtype,
		(void *)argp,
		sizeof(int)))
		return -EFAULT;
	if (cfgtype != CFG_GET_3D_CALI_DATA)
		qs_mt9p017_2D_sensor_config(argp);
	else
		qs_mt9p017_3D_sensor_config(argp);
	return 0;
}

static int qs_mt9p017_sensor_release(void)
{
	int rc = -EBADF;
	mutex_lock(&qs_mt9p017_mut);
	qs_mt9p017_power_down(qs_mt9p017_ctrl->sensordata);
	mutex_unlock(&qs_mt9p017_mut);
	return rc;
}

static int qs_mt9p017_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = i2c_add_driver(&qs_mt9p017_i2c_driver);
	if (rc < 0 || qs_mt9p017_client == NULL) {
		rc = -ENOTSUPP;
		CDBG("I2C add driver failed");
		goto i2c_probe_fail;
	}
	msm_camio_clk_rate_set(QS_MT9P017_MASTER_CLK_RATE);
	rc = qs_mt9p017_power_up(info);
	if (rc < 0)
		goto gpio_request_fail;

	rc = qs_mt9p017_check_id();
	if (rc < 0) {
		qs_mt9p017_power_down(info);
		goto i2c_probe_fail;
	}
	s->s_init = qs_mt9p017_sensor_open_init;
	s->s_release = qs_mt9p017_sensor_release;
	s->s_config  = qs_mt9p017_sensor_config;
	s->s_mount_angle = 270;

	qs_mt9p017_power_down(info);
	return rc;

gpio_request_fail:
	pr_err("%s: gpio request fail\n", __func__);
i2c_probe_fail:
	CDBG("qs_mt9p017_sensor_probe: probe failed!\n");
	i2c_del_driver(&qs_mt9p017_i2c_driver);
	return rc;
}

static struct qs_mt9p017_format qs_mt9p017_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static int qs_mt9p017_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	CDBG(KERN_DEBUG "Index is %d\n", index);
	if (index >= ARRAY_SIZE(qs_mt9p017_subdev_info))
		return -EINVAL;

	*code = qs_mt9p017_subdev_info[index].code;
	return 0;
}

static struct v4l2_subdev_core_ops qs_mt9p017_subdev_core_ops;
static struct v4l2_subdev_video_ops qs_mt9p017_subdev_video_ops = {
	.enum_mbus_fmt = qs_mt9p017_enum_fmt,
};

static struct v4l2_subdev_ops qs_mt9p017_subdev_ops = {
	.core = &qs_mt9p017_subdev_core_ops,
	.video  = &qs_mt9p017_subdev_video_ops,
};


static int qs_mt9p017_sensor_probe_cb(const struct msm_camera_sensor_info *info,
	struct v4l2_subdev *sdev, struct msm_sensor_ctrl *s)
{
	int rc = 0;
	qs_mt9p017_ctrl = kzalloc(sizeof(struct qs_mt9p017_ctrl_t), GFP_KERNEL);
	if (!qs_mt9p017_ctrl) {
		CDBG("qs_mt9p017_sensor_probe failed!\n");
		return -ENOMEM;
	}

	rc = qs_mt9p017_sensor_probe(info, s);
	if (rc < 0) {
		kfree(qs_mt9p017_ctrl);
		return rc;
	}

	/* probe is successful, init a v4l2 subdevice */
	CDBG(KERN_DEBUG "going into v4l2_i2c_subdev_init\n");
	if (sdev) {
		v4l2_i2c_subdev_init(sdev, qs_mt9p017_client,
						&qs_mt9p017_subdev_ops);
		qs_mt9p017_ctrl->sensor_dev = sdev;
	}
	return rc;
}

static int __qs_mt9p017_probe(struct platform_device *pdev)
{
	return msm_sensor_register(pdev, qs_mt9p017_sensor_probe_cb);
}

static struct platform_driver msm_camera_driver = {
	.probe = __qs_mt9p017_probe,
	.driver = {
		.name = "msm_camera_qs_mt9p017",
		.owner = THIS_MODULE,
	},
};

static int __init qs_mt9p017_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

static void __exit qs_mt9p017_exit(void)
{
	platform_driver_unregister(&msm_camera_driver);
}
module_init(qs_mt9p017_init);
module_exit(qs_mt9p017_exit);
MODULE_DESCRIPTION("Aptina 8 MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");

