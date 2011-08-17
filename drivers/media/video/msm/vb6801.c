/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "vb6801.h"

/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/
enum {
	REG_HOLD = 0x0104,
	RELEASE_HOLD = 0x0000,
	HOLD = 0x0001,
	STANDBY_MODE = 0x0000,
	REG_COARSE_INTEGRATION_TIME = 0x0202,
	REG_ANALOGUE_GAIN_CODE_GLOBAL = 0x0204,
	REG_RAMP_SCALE = 0x3116,
	REG_POWER_MAN_ENABLE_3 = 0x3142,
	REG_POWER_MAN_ENABLE_4 = 0x3143,
	REG_POWER_MAN_ENABLE_5 = 0x3144,
	REG_CCP2_DATA_FORMAT = 0x0112,
	REG_PRE_PLL_CLK_DIV = 0x0304,
	REG_PLL_MULTIPLIER = 0x0306,
	REG_VT_SYS_CLK_DIV = 0x0302,
	REG_VT_PIX_CLK_DIV = 0x0300,
	REG_OP_SYS_CLK_DIV = 0x030A,
	REG_OP_PIX_CLK_DIV = 0x0308,
	REG_VT_LINE_LENGTH_PCK = 0x0342,
	REG_X_OUTPUT_SIZE = 0x034C,
	REG_Y_OUTPUT_SIZE = 0x034E,
	REG_X_ODD_INC = 0x0382,
	REG_Y_ODD_INC = 0x0386,
	REG_VT_FRAME_LENGTH_LINES = 0x0340,
	REG_ANALOG_TIMING_MODES_2 = 0x3113,
	REG_BRUCE_ENABLE = 0x37B0,
	REG_OP_CODER_SYNC_CLK_SETUP = 0x3400,
	REG_OP_CODER_ENABLE = 0x3401,
	REG_OP_CODER_SLOW_PAD_EN = 0x3402,
	REG_OP_CODER_AUTO_STARTUP = 0x3414,
	REG_SCYTHE_ENABLE = 0x3204,
	REG_SCYTHE_WEIGHT = 0x3206,
	REG_FRAME_COUNT = 0x0005,
	REG_MODE_SELECT = 0x0100,
	REG_CCP2_CHANNEL_IDENTIFIER = 0x0110,
	REG_CCP2_SIGNALLING_MODE = 0x0111,
	REG_BTL_LEVEL_SETUP = 0x311B,
	REG_OP_CODER_AUTOMATIC_MODE_ENABLE = 0x3403,
	REG_PLL_CTRL = 0x3801,
	REG_VCM_DAC_CODE = 0x3860,
	REG_VCM_DAC_STROBE = 0x3868,
	REG_VCM_DAC_ENABLE = 0x386C,
	REG_NVM_T1_ADDR_00 = 0x3600,
	REG_NVM_T1_ADDR_01 = 0x3601,
	REG_NVM_T1_ADDR_02 = 0x3602,
	REG_NVM_T1_ADDR_03 = 0x3603,
	REG_NVM_T1_ADDR_04 = 0x3604,
	REG_NVM_T1_ADDR_05 = 0x3605,
	REG_NVM_T1_ADDR_06 = 0x3606,
	REG_NVM_T1_ADDR_07 = 0x3607,
	REG_NVM_T1_ADDR_08 = 0x3608,
	REG_NVM_T1_ADDR_09 = 0x3609,
	REG_NVM_T1_ADDR_0A = 0x360A,
	REG_NVM_T1_ADDR_0B = 0x360B,
	REG_NVM_T1_ADDR_0C = 0x360C,
	REG_NVM_T1_ADDR_0D = 0x360D,
	REG_NVM_T1_ADDR_0E = 0x360E,
	REG_NVM_T1_ADDR_0F = 0x360F,
	REG_NVM_T1_ADDR_10 = 0x3610,
	REG_NVM_T1_ADDR_11 = 0x3611,
	REG_NVM_T1_ADDR_12 = 0x3612,
	REG_NVM_T1_ADDR_13 = 0x3613,
	REG_NVM_CTRL = 0x3680,
	REG_NVM_PDN = 0x3681,
	REG_NVM_PULSE_WIDTH = 0x368B,
};

#define VB6801_LINES_PER_FRAME_PREVIEW   800
#define VB6801_LINES_PER_FRAME_SNAPSHOT 1600
#define VB6801_PIXELS_PER_LINE_PREVIEW  2500
#define VB6801_PIXELS_PER_LINE_SNAPSHOT 2500

/* AF constant */
#define VB6801_TOTAL_STEPS_NEAR_TO_FAR    25
#define VB6801_STEPS_NEAR_TO_CLOSEST_INF  25

/* for 30 fps preview */
#define VB6801_DEFAULT_CLOCK_RATE    12000000

enum vb6801_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum vb6801_resolution_t {
	QTR_SIZE,
	FULL_SIZE,
	INVALID_SIZE
};

enum vb6801_setting_t {
	RES_PREVIEW,
	RES_CAPTURE
};

struct vb6801_work_t {
	struct work_struct work;
};

struct sensor_dynamic_params_t {
	uint16_t preview_pixelsPerLine;
	uint16_t preview_linesPerFrame;
	uint16_t snapshot_pixelsPerLine;
	uint16_t snapshot_linesPerFrame;
	uint8_t snapshot_changed_fps;
	uint32_t pclk;
};

struct vb6801_sensor_info {
	/* Sensor Configuration Input Parameters */
	uint32_t ext_clk_freq_mhz;
	uint32_t target_frame_rate_fps;
	uint32_t target_vt_pix_clk_freq_mhz;
	uint32_t sub_sampling_factor;
	uint32_t analog_binning_allowed;
	uint32_t raw_mode;
	uint32_t capture_mode;

	/* Image Readout Registers */
	uint32_t x_odd_inc;	/* x pixel array addressing odd increment */
	uint32_t y_odd_inc;	/* y pixel array addressing odd increment */
	uint32_t x_output_size;	/* width of output image  */
	uint32_t y_output_size;	/* height of output image */

	/* Declare data format */
	uint32_t ccp2_data_format;

	/* Clock Tree Registers */
	uint32_t pre_pll_clk_div;
	uint32_t pll_multiplier;
	uint32_t vt_sys_clk_div;
	uint32_t vt_pix_clk_div;
	uint32_t op_sys_clk_div;
	uint32_t op_pix_clk_div;

	/* Video Timing Registers */
	uint32_t vt_line_length_pck;
	uint32_t vt_frame_length_lines;

	/* Analogue Binning Registers */
	uint8_t vtiming_major;
	uint8_t analog_timing_modes_4;

	/* Fine (pixel) Integration Time Registers */
	uint32_t fine_integration_time;

	/* Coarse (lines) Integration Time Limit Registers */
	uint32_t coarse_integration_time_max;

	/* Coarse (lines) Integration Timit Register (16-bit) */
	uint32_t coarse_integration_time;

	/* Analogue Gain Code Global Registers */
	uint32_t analogue_gain_code_global;

	/* Digital Gain Code Registers */
	uint32_t digital_gain_code;

	/* Overall gain (analogue & digital) code
	 * Note that this is not a real register but just
	 * an abstraction for the combination of analogue
	 * and digital gain */
	uint32_t gain_code;

	/* FMT Test Information */
	uint32_t pass_fail;
	uint32_t day;
	uint32_t month;
	uint32_t year;
	uint32_t tester;
	uint32_t part_number;

	/* Autofocus controls */
	uint32_t vcm_dac_code;
	int vcm_max_dac_code_step;
	int vcm_proportional_factor;
	int vcm_dac_code_spacing_ms;

	/* VCM NVM Characterisation Information */
	uint32_t vcm_dac_code_infinity_dn;
	uint32_t vcm_dac_code_macro_up;
	uint32_t vcm_dac_code_up_dn_delta;

	/* Internal Variables */
	uint32_t min_vt_frame_length_lines;
};

struct vb6801_work_t *vb6801_sensorw;
struct i2c_client *vb6801_client;

struct vb6801_ctrl_t {
	const struct msm_camera_sensor_info *sensordata;

	int sensormode;
	uint32_t factor_fps;	/* init to 1 * 0x00000400 */
	uint16_t curr_fps;
	uint16_t max_fps;
	int8_t pict_exp_update;
	int8_t reducel;
	uint16_t curr_lens_pos;
	uint16_t init_curr_lens_pos;
	enum vb6801_resolution_t prev_res;
	enum vb6801_resolution_t pict_res;
	enum vb6801_resolution_t curr_res;
	enum vb6801_test_mode_t set_test;

	struct vb6801_sensor_info s_info;
	struct sensor_dynamic_params_t s_dynamic_params;
};

static struct vb6801_ctrl_t *vb6801_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(vb6801_wait_queue);
DEFINE_MUTEX(vb6801_mut);

static int vb6801_i2c_rxdata(unsigned short saddr,
			     unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = 2,
			.buf = rxdata,
		},
		{
			.addr = saddr,
			.flags = I2C_M_RD,
			.len = 2,
			.buf = rxdata,
		},
	};

	if (i2c_transfer(vb6801_client->adapter, msgs, 2) < 0) {
		CDBG("vb6801_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t vb6801_i2c_read(unsigned short raddr,
			       unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[2];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00) >> 8;
	buf[1] = (raddr & 0x00FF);

	rc = vb6801_i2c_rxdata(vb6801_client->addr, buf, rlen);

	if (rc < 0) {
		CDBG("vb6801_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}

	*rdata = (rlen == 2 ? buf[0] << 8 | buf[1] : buf[0]);

	return rc;
}

static int32_t vb6801_i2c_read_table(struct vb6801_i2c_reg_conf_t *regs,
				     int items)
{
	int i;
	int32_t rc = -EFAULT;

	for (i = 0; i < items; i++) {
		unsigned short *buf =
		    regs->dlen == D_LEN_BYTE ?
		    (unsigned short *)&regs->bdata :
		    (unsigned short *)&regs->wdata;
		rc = vb6801_i2c_read(regs->waddr, buf, regs->dlen + 1);

		if (rc < 0) {
			CDBG("vb6801_i2c_read_table Failed!!!\n");
			break;
		}

		regs++;
	}

	return rc;
}

static int32_t vb6801_i2c_txdata(unsigned short saddr,
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

	if (i2c_transfer(vb6801_client->adapter, msg, 1) < 0) {
		CDBG("vb6801_i2c_txdata faild 0x%x\n", vb6801_client->addr);
		return -EIO;
	}

	return 0;
}

static int32_t vb6801_i2c_write_b(unsigned short waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[3];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;

	CDBG("i2c_write_b addr = %d, val = %d\n", waddr, bdata);
	rc = vb6801_i2c_txdata(vb6801_client->addr, buf, 3);

	if (rc < 0) {
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
		     waddr, bdata);
	}

	return rc;
}

static int32_t vb6801_i2c_write_w(unsigned short waddr, unsigned short wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00) >> 8;
	buf[3] = (wdata & 0x00FF);

	CDBG("i2c_write_w addr = %d, val = %d, buf[2] = 0x%x, buf[3] = 0x%x\n",
	     waddr, wdata, buf[2], buf[3]);

	rc = vb6801_i2c_txdata(vb6801_client->addr, buf, 4);
	if (rc < 0) {
		CDBG("i2c_write_w failed, addr = 0x%x, val = 0x%x!\n",
		     waddr, wdata);
	}

	return rc;
}

static int32_t vb6801_i2c_write_table(struct vb6801_i2c_reg_conf_t *regs,
				      int items)
{
	int i;
	int32_t rc = -EFAULT;

	for (i = 0; i < items; i++) {
		rc = ((regs->dlen == D_LEN_BYTE) ?
		      vb6801_i2c_write_b(regs->waddr, regs->bdata) :
		      vb6801_i2c_write_w(regs->waddr, regs->wdata));

		if (rc < 0) {
			CDBG("vb6801_i2c_write_table Failed!!!\n");
			break;
		}

		regs++;
	}

	return rc;
}

static int32_t vb6801_reset(const struct msm_camera_sensor_info *data)
{
	int rc;

	rc = gpio_request(data->sensor_reset, "vb6801");
	if (!rc) {
		CDBG("sensor_reset SUcceeded\n");
		gpio_direction_output(data->sensor_reset, 0);
		mdelay(50);
		gpio_direction_output(data->sensor_reset, 1);
		mdelay(13);
	} else
		CDBG("sensor_reset FAiled\n");

	return rc;
}

static int32_t vb6801_set_default_focus(void)
{
	int32_t rc = 0;

	/* FIXME: Default focus not supported */

	return rc;
}

static void vb6801_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint32_t divider; /*Q10 */
	uint32_t pclk_mult; /*Q10 */
	uint32_t d1;
	uint32_t d2;

	d1 =
		(uint32_t)(
		(vb6801_ctrl->s_dynamic_params.preview_linesPerFrame *
		0x00000400) /
		vb6801_ctrl->s_dynamic_params.snapshot_linesPerFrame);

	d2 =
		(uint32_t)(
		(vb6801_ctrl->s_dynamic_params.preview_pixelsPerLine *
		0x00000400) /
		vb6801_ctrl->s_dynamic_params.snapshot_pixelsPerLine);


	divider = (uint32_t) (d1 * d2) / 0x00000400;

	pclk_mult = (48 * 0x400) / 60;

	/* Verify PCLK settings and frame sizes. */
	*pfps = (uint16_t)((((fps * pclk_mult) / 0x00000400) * divider)/
				0x00000400);
}

static uint16_t vb6801_get_prev_lines_pf(void)
{
	if (vb6801_ctrl->prev_res == QTR_SIZE)
		return vb6801_ctrl->s_dynamic_params.preview_linesPerFrame;
	else
		return vb6801_ctrl->s_dynamic_params.snapshot_linesPerFrame;
}

static uint16_t vb6801_get_prev_pixels_pl(void)
{
	if (vb6801_ctrl->prev_res == QTR_SIZE)
		return vb6801_ctrl->s_dynamic_params.preview_pixelsPerLine;
	else
		return vb6801_ctrl->s_dynamic_params.snapshot_pixelsPerLine;
}

static uint16_t vb6801_get_pict_lines_pf(void)
{
	return vb6801_ctrl->s_dynamic_params.snapshot_linesPerFrame;
}

static uint16_t vb6801_get_pict_pixels_pl(void)
{
	return vb6801_ctrl->s_dynamic_params.snapshot_pixelsPerLine;
}

static uint32_t vb6801_get_pict_max_exp_lc(void)
{
	uint16_t snapshot_lines_per_frame;

	if (vb6801_ctrl->pict_res == QTR_SIZE) {
		snapshot_lines_per_frame =
		    vb6801_ctrl->s_dynamic_params.preview_linesPerFrame - 3;
	} else {
		snapshot_lines_per_frame =
		    vb6801_ctrl->s_dynamic_params.snapshot_linesPerFrame - 3;
	}

	return snapshot_lines_per_frame;
}

static int32_t vb6801_set_fps(struct fps_cfg *fps)
{
	int32_t rc = 0;

	/* input is new fps in Q8 format */
	switch (fps->fps_div) {
	case 7680:		/* 30 * Q8 */
		vb6801_ctrl->factor_fps = 1;
		break;

	case 3840:		/* 15 * Q8 */
		vb6801_ctrl->factor_fps = 2;
		break;

	case 2560:		/* 10 * Q8 */
		vb6801_ctrl->factor_fps = 3;
		break;

	case 1920:		/* 7.5 * Q8 */
		vb6801_ctrl->factor_fps = 4;
		break;

	default:
		rc = -ENODEV;
		break;
	}

	return rc;
}

static int32_t vb6801_write_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;
	uint16_t lpf;

	if (vb6801_ctrl->curr_res == SENSOR_FULL_SIZE)
		lpf = VB6801_LINES_PER_FRAME_SNAPSHOT;
	else
		lpf = VB6801_LINES_PER_FRAME_PREVIEW;

	/* hold */
	rc = vb6801_i2c_write_w(REG_HOLD, HOLD);
	if (rc < 0)
		goto exp_gain_done;

	if ((vb6801_ctrl->curr_fps <
	     vb6801_ctrl->max_fps / vb6801_ctrl->factor_fps) &&
	    (!vb6801_ctrl->pict_exp_update)) {

		if (vb6801_ctrl->reducel) {

			rc = vb6801_i2c_write_w(REG_VT_FRAME_LENGTH_LINES,
						lpf * vb6801_ctrl->factor_fps);

			vb6801_ctrl->curr_fps =
			    vb6801_ctrl->max_fps / vb6801_ctrl->factor_fps;

		} else if (!vb6801_ctrl->reducel) {

			rc = vb6801_i2c_write_w(REG_COARSE_INTEGRATION_TIME,
						line * vb6801_ctrl->factor_fps);

			vb6801_ctrl->reducel = 1;
		}
	} else if ((vb6801_ctrl->curr_fps >
		    vb6801_ctrl->max_fps / vb6801_ctrl->factor_fps) &&
		   (!vb6801_ctrl->pict_exp_update)) {

		rc = vb6801_i2c_write_w(REG_VT_FRAME_LENGTH_LINES,
					lpf * vb6801_ctrl->factor_fps);

		vb6801_ctrl->curr_fps =
		    vb6801_ctrl->max_fps / vb6801_ctrl->factor_fps;

	} else {
		/* analogue_gain_code_global */
		rc = vb6801_i2c_write_w(REG_ANALOGUE_GAIN_CODE_GLOBAL, gain);
		if (rc < 0)
			goto exp_gain_done;

		/* coarse_integration_time */
		rc = vb6801_i2c_write_w(REG_COARSE_INTEGRATION_TIME,
					line * vb6801_ctrl->factor_fps);
		if (rc < 0)
			goto exp_gain_done;

		vb6801_ctrl->pict_exp_update = 1;
	}

	rc = vb6801_i2c_write_w(REG_HOLD, RELEASE_HOLD);

exp_gain_done:
	return rc;
}

static int32_t vb6801_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	vb6801_ctrl->pict_exp_update = 1;
	return vb6801_write_exp_gain(gain, line);
}

static int32_t vb6801_power_down(void)
{
	int32_t rc = 0;
	rc = vb6801_i2c_write_b(REG_NVM_PDN, 0);

	mdelay(5);
	return rc;
}

static int32_t vb6801_go_to_position(uint32_t target_vcm_dac_code,
				     struct vb6801_sensor_info *ps)
{
	/* Prior to running this function the following values must
	 * be initialised in the sensor data structure, PS
	 * ps->vcm_dac_code
	 * ps->vcm_max_dac_code_step
	 * ps->vcm_dac_code_spacing_ms */

	int32_t rc = 0;

	ps->vcm_dac_code = target_vcm_dac_code;

	/* Restore Strobe to zero state */
	rc = vb6801_i2c_write_b(REG_VCM_DAC_STROBE, 0x00);
	if (rc < 0)
		return rc;

	/* Write 9-bit VCM DAC Code */
	rc = vb6801_i2c_write_w(REG_VCM_DAC_CODE, ps->vcm_dac_code);
	if (rc < 0)
		return rc;

	/* Generate a rising edge on the dac_strobe to latch
	 * new DAC value */

	rc = vb6801_i2c_write_w(REG_VCM_DAC_STROBE, 0x01);

	return rc;
}

static int32_t vb6801_move_focus(int direction, int32_t num_steps)
{
	int16_t step_direction;
	int16_t actual_step;
	int16_t next_position;
	uint32_t step_size;
	int16_t small_move[4];
	uint16_t i;
	int32_t rc = 0;

	step_size = (vb6801_ctrl->s_info.vcm_dac_code_macro_up -
		     vb6801_ctrl->s_info.vcm_dac_code_infinity_dn) /
	    VB6801_TOTAL_STEPS_NEAR_TO_FAR;

	if (num_steps > VB6801_TOTAL_STEPS_NEAR_TO_FAR)
		num_steps = VB6801_TOTAL_STEPS_NEAR_TO_FAR;
	else if (num_steps == 0)
		return -EINVAL;

	if (direction == MOVE_NEAR)
		step_direction = 4;
	else if (direction == MOVE_FAR)
		step_direction = -4;
	else
		return -EINVAL;

	/* need to decide about default position and power supplied
	 * at start up and reset */
	if (vb6801_ctrl->curr_lens_pos < vb6801_ctrl->init_curr_lens_pos)
		vb6801_ctrl->curr_lens_pos = vb6801_ctrl->init_curr_lens_pos;

	actual_step = (step_direction * num_steps);

	next_position = vb6801_ctrl->curr_lens_pos;

	for (i = 0; i < 4; i++) {
		if (actual_step >= 0)
			small_move[i] =
			    (i + 1) * actual_step / 4 - i * actual_step / 4;

		if (actual_step < 0)
			small_move[i] =
			    (i + 1) * actual_step / 4 - i * actual_step / 4;
	}

	if (next_position > 511)
		next_position = 511;
	else if (next_position < 0)
		next_position = 0;

	/* for damping */
	for (i = 0; i < 4; i++) {
		next_position =
		    (int16_t) (vb6801_ctrl->curr_lens_pos + small_move[i]);

		/* Writing the digital code for current to the actuator */
		CDBG("next_position in damping mode = %d\n", next_position);

		rc = vb6801_go_to_position(next_position, &vb6801_ctrl->s_info);
		if (rc < 0) {
			CDBG("go_to_position Failed!!!\n");
			return rc;
		}

		vb6801_ctrl->curr_lens_pos = next_position;
		if (i < 3)
			mdelay(5);
	}

	return rc;
}

static int vb6801_read_nvm_data(struct vb6801_sensor_info *ps)
{
	/* +--------+------+------+----------------+---------------+
	 * | Index | NVM | NVM | Name | Description |
	 * | | Addr | Byte | | |
	 * +--------+------+------+----------------+---------------+
	 * | 0x3600 | 0 | 3 | nvm_t1_addr_00 | {PF[2:0]:Day[4:0]} |
	 * | 0x3601 | 0 | 2 | nvm_t1_addr_01 | {Month[3:0]:Year[3:0]} |
	 * | 0x3602 | 0 | 1 | nvm_t1_addr_02 | Tester[7:0] |
	 * | 0x3603 | 0 | 0 | nvm_t1_addr_03 | Part[15:8] |
	 * +--------+------+------+----------------+---------------+
	 * | 0x3604 | 1 | 3 | nvm_t1_addr_04 | Part[7:0] |
	 * | 0x3605 | 1 | 2 | nvm_t1_addr_05 | StartWPM[7:0] |
	 * | 0x3606 | 1 | 1 | nvm_t1_addr_06 | Infinity[7:0] |
	 * | 0x3607 | 1 | 0 | nvm_t1_addr_07 | Macro[7:0] |
	 * +--------+------+------+----------------+---------------+
	 * | 0x3608 | 2 | 3 | nvm_t1_addr_08 | Reserved |
	 * | 0x3609 | 2 | 2 | nvm_t1_addr_09 | Reserved |
	 * | 0x360A | 2 | 1 | nvm_t1_addr_0A | UpDown[7:0] |
	 * | 0x360B | 2 | 0 | nvm_t1_addr_0B | Reserved |
	 * +--------+------+------+----------------+---------------+
	 * | 0x360C | 3 | 3 | nvm_t1_addr_0C | Reserved |
	 * | 0x360D | 3 | 2 | nvm_t1_addr_0D | Reserved |
	 * | 0x360E | 3 | 1 | nvm_t1_addr_0E | Reserved |
	 * | 0x360F | 3 | 0 | nvm_t1_addr_0F | Reserved |
	 * +--------+------+------+----------------+---------------+
	 * | 0x3610 | 4 | 3 | nvm_t1_addr_10 | Reserved |
	 * | 0x3611 | 4 | 2 | nvm_t1_addr_11 | Reserved |
	 * | 0x3612 | 4 | 1 | nvm_t1_addr_12 | Reserved |
	 * | 0x3613 | 4 | 0 | nvm_t1_addr_13 | Reserved |
	 * +--------+------+------+----------------+---------------+*/

	int32_t rc;
	struct vb6801_i2c_reg_conf_t rreg[] = {
		{REG_NVM_T1_ADDR_00, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_01, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_02, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_03, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_04, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_05, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_06, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_07, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_08, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_09, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_0A, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_0B, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_0C, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_0D, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_0E, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_0F, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_10, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_11, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_12, 0, 0, D_LEN_BYTE},
		{REG_NVM_T1_ADDR_13, 0, 0, D_LEN_BYTE},
	};

	struct vb6801_i2c_reg_conf_t wreg[] = {
		/* Enable NVM for Direct Reading */
		{REG_NVM_CTRL, 0, 2, D_LEN_BYTE},

		/* Power up NVM */
		{REG_NVM_PDN, 0, 1, D_LEN_BYTE},
	};

	rc = vb6801_i2c_write_table(wreg, ARRAY_SIZE(wreg));
	if (rc < 0) {
		CDBG("I2C Write Table FAILED!!!\n");
		return rc;
	}

	/* NVM Read Pulse Width
	 * ====================
	 * nvm_pulse_width_us = nvm_pulse_width_ext_clk / ext_clk_freq_mhz
	 * Valid Range for Read Pulse Width = 400ns -> 3.0us
	 * Min ext_clk_freq_mhz = 6MHz  => 3.0 *  6  = 18
	 * Max ext_clk_freq_mhz = 27MHz => 0.4 * 27 = 10.8
	 * Choose 15 as a common value
	 *  - 15 /  6.0 = 2.5000us
	 *  - 15 / 12.0 = 1.2500us
	 *  - 15 / 27.0 = 0.5555us */
	rc = vb6801_i2c_write_w(REG_NVM_PULSE_WIDTH, 15);
	if (rc < 0) {
		rc = -EBUSY;
		goto nv_shutdown;
	}

	rc = vb6801_i2c_read_table(rreg, ARRAY_SIZE(rreg));
	if (rc < 0) {
		CDBG("I2C Read Table FAILED!!!\n");
		rc = -EBUSY;
		goto nv_shutdown;
	}

	/* Decode and Save FMT Info */
	ps->pass_fail = (rreg[0].bdata & 0x00E0) >> 5;
	ps->day = (rreg[0].bdata & 0x001F);
	ps->month = (rreg[1].bdata & 0x00F0) >> 4;
	ps->year = (rreg[1].bdata & 0x000F) + 2000;
	ps->tester = rreg[2].bdata;
	ps->part_number = (rreg[3].bdata << 8) + rreg[4].bdata;

	/* Decode and Save VCM Dac Values in data structure */
	ps->vcm_dac_code_infinity_dn = rreg[6].bdata;
	ps->vcm_dac_code_macro_up = rreg[7].bdata << 1;
	ps->vcm_dac_code_up_dn_delta = rreg[10].bdata;

nv_shutdown:
	/* Power Down NVM to extend life time */
	rc = vb6801_i2c_write_b(REG_NVM_PDN, 0);

	return rc;
}

static int vb6801_config_sensor(int32_t ext_clk_freq_mhz,
				int32_t target_frame_rate_fps,
				int32_t target_vt_pix_clk_freq_mhz,
				uint32_t sub_sampling_factor,
				uint32_t analog_binning_allowed,
				uint32_t raw_mode, int capture_mode,
				enum vb6801_resolution_t res)
{
	uint32_t rc;
	/* ext_clk_freq_mhz      = 6.0 -> 27.0 MHz
	 * target_frame_rate_fps  = 15 fps
	 * target_vt_pix_clk_freq_mhz = 24.0 -> 64.0MHz
	 * sub_sampling factor   = 1, 2, 3, or 4
	 * raw_mode factor       = 10
	 *
	 * capture_mode, 0 = CCP1
	 * capture_mode, 1 = CCP2
	 * capture_mode, 2 = 10-bit parallel + hsync + vsync */

	/* Declare data format */
	uint32_t ccp2_data_format = 0x0A0A;

	/*  Declare clock tree variables */
	int32_t min_pll_ip_freq_mhz = 6;
	int32_t max_pll_op_freq_mhz = 640;
	uint32_t pre_pll_clk_div = 1;
	int32_t pll_ip_freq_mhz = 6;
	uint32_t pll_multiplier = 100;
	int32_t pll_op_freq_mhz = 600;
	uint32_t vt_sys_clk_div = 1;
	int32_t vt_sys_clk_freq_mhz = 600;
	uint32_t vt_pix_clk_div = 10;
	int32_t vt_pix_clk_freq_mhz = 60;
	uint32_t op_sys_clk_div = 1;
	int32_t op_sys_clk_freq_mhz = 60;
	uint32_t op_pix_clk_div = 10;
	int32_t op_pix_clk_freq_mhz = 60;

	/* Declare pixel array and frame timing variables */
	uint32_t x_pixel_array = 2064;
	uint32_t y_pixel_array = 1544;
	uint32_t x_even_inc = 1;
	uint32_t x_odd_inc = 1;
	uint32_t y_even_inc = 1;
	uint32_t y_odd_inc = 1;
	uint32_t x_output_size = 2064;
	uint32_t y_output_size = 1544;
	uint32_t additional_rows = 2;
	uint32_t min_vt_frame_blanking_lines = 16;
	uint32_t vt_line_length_pck = 2500;
	uint32_t vt_line_length_us = 0;
	uint32_t min_vt_frame_length_lines = 1562;
	uint32_t vt_frame_length_lines = 1600;
	uint32_t target_vt_frame_length_ms;	/* 200 * 0x0001000 / 3; */
	uint32_t vt_frame_length_ms;	/* 200 * 0x0001000 / 3; */
	uint32_t frame_rate_fps = 15;

	/* Coarse intergration time */
	uint32_t coarse_integration_time = 1597;
	uint32_t coarse_integration_time_max_margin = 3;
	uint16_t frame_count;
	int timeout;

	struct vb6801_sensor_info *pinfo = &vb6801_ctrl->s_info;

	struct vb6801_i2c_reg_conf_t rreg[] = {
		{REG_PRE_PLL_CLK_DIV, 0, 0, D_LEN_WORD},
		{REG_PLL_MULTIPLIER, 0, 0, D_LEN_WORD},
		{REG_VT_SYS_CLK_DIV, 0, 0, D_LEN_WORD},
		{REG_VT_PIX_CLK_DIV, 0, 0, D_LEN_WORD},
		{REG_OP_SYS_CLK_DIV, 0, 0, D_LEN_WORD},
		{REG_OP_PIX_CLK_DIV, 0, 0, D_LEN_WORD},
		{REG_FRAME_COUNT, 0, 0, D_LEN_BYTE},
	};

	struct vb6801_i2c_reg_conf_t wreg2[] = {
		{REG_POWER_MAN_ENABLE_3, 0, 95, D_LEN_BYTE},
		{REG_POWER_MAN_ENABLE_4, 0, 142, D_LEN_BYTE},
		{REG_POWER_MAN_ENABLE_5, 0, 7, D_LEN_BYTE},
	};

	/* VIDEO TIMING CALCULATIONS
	 * ========================= */

	/* Pixel Array Size */
	x_pixel_array = 2064;
	y_pixel_array = 1544;

	/* set current resolution */
	vb6801_ctrl->curr_res = res;

	/* Analogue binning setup */
	if (pinfo->analog_binning_allowed > 0 &&
	    pinfo->sub_sampling_factor == 4) {

		pinfo->vtiming_major = 1;
		pinfo->analog_timing_modes_4 = 32;
	} else if (pinfo->analog_binning_allowed > 0 &&
		   pinfo->sub_sampling_factor == 2) {

		pinfo->vtiming_major = 1;
		pinfo->analog_timing_modes_4 = 0;
	} else {

		pinfo->vtiming_major = 0;
		pinfo->analog_timing_modes_4 = 0;
	}

	/* Sub-Sampling X & Y Odd Increments: valid values 1, 3, 5, 7 */
	x_even_inc = 1;
	y_even_inc = 1;
	x_odd_inc = (sub_sampling_factor << 1) - x_even_inc;
	y_odd_inc = (sub_sampling_factor << 1) - y_even_inc;

	/* Output image size
	 * Must always be a multiple of 2 - round down */
	x_output_size = ((x_pixel_array / sub_sampling_factor) >> 1) << 1;
	y_output_size = ((y_pixel_array / sub_sampling_factor) >> 1) << 1;

	/* Output data format */
	ccp2_data_format = (raw_mode << 8) + raw_mode;

	/* Pre PLL clock divider : valid values 1, 2 or 4
	 * The 1st step is to ensure that PLL input frequency is as close
	 * as possible to the min allowed PLL input frequency.
	 * This yields the smallest step size in the PLL output frequency. */
	pre_pll_clk_div =
	    ((int)(ext_clk_freq_mhz / min_pll_ip_freq_mhz) >> 1) << 1;
	if (pre_pll_clk_div < 2)
		pre_pll_clk_div = 1;

	pll_ip_freq_mhz = ext_clk_freq_mhz / pre_pll_clk_div;

	/* Video Timing System Clock divider: valid values 1, 2, 4
	 * Now need to work backwards through the clock tree to determine the
	 * 1st pass estimates for vt_sys_clk_freq_mhz and then the PLL output
	 * frequency.*/
	vt_sys_clk_freq_mhz = vt_pix_clk_div * target_vt_pix_clk_freq_mhz;
	vt_sys_clk_div = max_pll_op_freq_mhz / vt_sys_clk_freq_mhz;
	if (vt_sys_clk_div < 2)
		vt_sys_clk_div = 1;

	/* PLL Mulitplier: min , max 106 */
	pll_op_freq_mhz = vt_sys_clk_div * vt_sys_clk_freq_mhz;
	pll_multiplier = (pll_op_freq_mhz * 0x0001000) / pll_ip_freq_mhz;

	/* Calculate the acutal pll output frequency
	 * - the pll_multiplier calculation introduces a quantisation error
	 *   due the integer nature of the pll multiplier */
	pll_op_freq_mhz = (pll_ip_freq_mhz * pll_multiplier) / 0x0001000;

	/* Re-calculate video timing clock frequencies based
	 * on actual PLL freq */
	vt_sys_clk_freq_mhz = pll_op_freq_mhz / vt_sys_clk_div;
	vt_pix_clk_freq_mhz = ((vt_sys_clk_freq_mhz * 0x0001000) /
				vt_pix_clk_div)/0x0001000;

	/* Output System Clock Divider: valid value 1, 2, 4, 6, 8
	 * op_sys_clk_div = vt_sys_clk_div;*/
	op_sys_clk_div = (vt_sys_clk_div * sub_sampling_factor);
	if (op_sys_clk_div < 2)
		op_sys_clk_div = 1;

	/* Calculate output timing clock frequencies */
	op_sys_clk_freq_mhz = pll_op_freq_mhz / op_sys_clk_div;
	op_pix_clk_freq_mhz =
	    (op_sys_clk_freq_mhz * 0x0001000) / (op_pix_clk_div * 0x0001000);

	/* Line length in pixels and us */
	vt_line_length_pck = 2500;
	vt_line_length_us =
	    vt_line_length_pck * 0x0001000 / vt_pix_clk_freq_mhz;

	/* Target vt_frame_length_ms */
	target_vt_frame_length_ms = (1000 * 0x0001000 / target_frame_rate_fps);

	/* Frame length in lines */
	min_vt_frame_length_lines =
	    additional_rows + y_output_size + min_vt_frame_blanking_lines;

	vt_frame_length_lines =
	    ((1000 * target_vt_frame_length_ms) / vt_line_length_us);

	if (vt_frame_length_lines <= min_vt_frame_length_lines)
		vt_frame_length_lines = min_vt_frame_length_lines;

	/* Calcuate the actual frame length in ms */
	vt_frame_length_ms = (vt_frame_length_lines * vt_line_length_us / 1000);

	/* Frame Rate in fps */
	frame_rate_fps = (1000 * 0x0001000 / vt_frame_length_ms);

	/* Set coarse integration to max */
	coarse_integration_time =
	    vt_frame_length_lines - coarse_integration_time_max_margin;

	CDBG("SENSOR VIDEO TIMING SUMMARY:\n");
	CDBG(" ============================\n");
	CDBG("ext_clk_freq_mhz      = %d\n", ext_clk_freq_mhz);
	CDBG("pre_pll_clk_div       = %d\n", pre_pll_clk_div);
	CDBG("pll_ip_freq_mhz       = %d\n", pll_ip_freq_mhz);
	CDBG("pll_multiplier        = %d\n", pll_multiplier);
	CDBG("pll_op_freq_mhz       = %d\n", pll_op_freq_mhz);
	CDBG("vt_sys_clk_div        = %d\n", vt_sys_clk_div);
	CDBG("vt_sys_clk_freq_mhz   = %d\n", vt_sys_clk_freq_mhz);
	CDBG("vt_pix_clk_div        = %d\n", vt_pix_clk_div);
	CDBG("vt_pix_clk_freq_mhz   = %d\n", vt_pix_clk_freq_mhz);
	CDBG("op_sys_clk_div        = %d\n", op_sys_clk_div);
	CDBG("op_sys_clk_freq_mhz   = %d\n", op_sys_clk_freq_mhz);
	CDBG("op_pix_clk_div        = %d\n", op_pix_clk_div);
	CDBG("op_pix_clk_freq_mhz   = %d\n", op_pix_clk_freq_mhz);
	CDBG("vt_line_length_pck    = %d\n", vt_line_length_pck);
	CDBG("vt_line_length_us     = %d\n", vt_line_length_us/0x0001000);
	CDBG("vt_frame_length_lines = %d\n", vt_frame_length_lines);
	CDBG("vt_frame_length_ms    = %d\n", vt_frame_length_ms/0x0001000);
	CDBG("frame_rate_fps        = %d\n", frame_rate_fps);
	CDBG("ccp2_data_format = %d\n", ccp2_data_format);
	CDBG("x_output_size = %d\n", x_output_size);
	CDBG("y_output_size = %d\n", y_output_size);
	CDBG("x_odd_inc = %d\n", x_odd_inc);
	CDBG("y_odd_inc = %d\n", y_odd_inc);
	CDBG("(vt_frame_length_lines * frame_rate_factor ) = %d\n",
	    (vt_frame_length_lines * vb6801_ctrl->factor_fps));
	CDBG("coarse_integration_time = %d\n", coarse_integration_time);
	CDBG("pinfo->vcm_dac_code = %d\n", pinfo->vcm_dac_code);
	CDBG("capture_mode = %d\n", capture_mode);

	/* RE-CONFIGURE SENSOR WITH NEW TIMINGS
	 * ====================================
	 * Enter Software Standby Mode */
	rc = vb6801_i2c_write_b(REG_MODE_SELECT, 0);
	if (rc < 0) {
		CDBG("I2C vb6801_i2c_write_b FAILED!!!\n");
		return rc;
	}

	/* Wait 100ms */
	mdelay(100);

	if (capture_mode == 0) {

		rc = vb6801_i2c_write_b(REG_CCP2_CHANNEL_IDENTIFIER, 0);
		rc = vb6801_i2c_write_b(REG_CCP2_SIGNALLING_MODE, 0);
	} else if (capture_mode == 1) {

		rc = vb6801_i2c_write_b(REG_CCP2_CHANNEL_IDENTIFIER, 0);
		rc = vb6801_i2c_write_b(REG_CCP2_SIGNALLING_MODE, 1);
	}

	{
		struct vb6801_i2c_reg_conf_t wreg[] = {
			/* Re-configure Sensor */
			{REG_CCP2_DATA_FORMAT, ccp2_data_format, 0,
			 D_LEN_WORD},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL, 128, 0, D_LEN_WORD},
			{REG_PRE_PLL_CLK_DIV, pre_pll_clk_div, 0, D_LEN_WORD},
			{REG_VT_SYS_CLK_DIV, vt_sys_clk_div, 0, D_LEN_WORD},
			{REG_VT_PIX_CLK_DIV, vt_pix_clk_div, 0, D_LEN_WORD},
			{REG_OP_SYS_CLK_DIV, vt_sys_clk_div, 0, D_LEN_WORD},
			{REG_OP_PIX_CLK_DIV, vt_pix_clk_div, 0, D_LEN_WORD},
			{REG_VT_LINE_LENGTH_PCK, vt_line_length_pck, 0,
			 D_LEN_WORD},
			{REG_X_OUTPUT_SIZE, x_output_size, 0, D_LEN_WORD},
			{REG_Y_OUTPUT_SIZE, y_output_size, 0, D_LEN_WORD},
			{REG_X_ODD_INC, x_odd_inc, 0, D_LEN_WORD},
			{REG_Y_ODD_INC, y_odd_inc, 0, D_LEN_WORD},
			{REG_VT_FRAME_LENGTH_LINES,
			 vt_frame_length_lines * vb6801_ctrl->factor_fps, 0,
			 D_LEN_WORD},
			{REG_COARSE_INTEGRATION_TIME,
			 coarse_integration_time, 0, D_LEN_WORD},
			/* Analogue Settings */
			{REG_ANALOG_TIMING_MODES_2, 0, 132, D_LEN_BYTE},
			{REG_RAMP_SCALE, 0, 5, D_LEN_BYTE},
			{REG_BTL_LEVEL_SETUP, 0, 11, D_LEN_BYTE},
			/* Enable Defect Correction */
			{REG_SCYTHE_ENABLE, 0, 1, D_LEN_BYTE},
			{REG_SCYTHE_WEIGHT, 0, 16, D_LEN_BYTE},
			{REG_BRUCE_ENABLE, 0, 1, D_LEN_BYTE},
			/* Auto Focus Configuration
			 * Please note that the DAC Code is a written as a
			 * 16-bit value 0 = infinity (no DAC current) */
			{REG_VCM_DAC_CODE, pinfo->vcm_dac_code, 0, D_LEN_WORD},
			{REG_VCM_DAC_STROBE, 0, 0, D_LEN_BYTE},
			{REG_VCM_DAC_ENABLE, 0, 1, D_LEN_BYTE},
		};

		rc = vb6801_i2c_write_table(wreg, ARRAY_SIZE(wreg));
		if (rc < 0) {
			CDBG("I2C Write Table FAILED!!!\n");
			return rc;
		}
	}
	/* Parallel Interface Configuration */
	if (capture_mode >= 2) {
		struct vb6801_i2c_reg_conf_t wreg1[] = {
			{REG_OP_CODER_SYNC_CLK_SETUP, 0, 15, D_LEN_BYTE},
			{REG_OP_CODER_ENABLE, 0, 3, D_LEN_BYTE},
			{REG_OP_CODER_SLOW_PAD_EN, 0, 1, D_LEN_BYTE},
			{REG_OP_CODER_AUTOMATIC_MODE_ENABLE, 0, 3, D_LEN_BYTE},
			{REG_OP_CODER_AUTO_STARTUP, 0, 2, D_LEN_BYTE},
		};

		rc = vb6801_i2c_write_table(wreg1, ARRAY_SIZE(wreg1));
		if (rc < 0) {
			CDBG("I2C Write Table FAILED!!!\n");
			return rc;
		}
	}

	/* Enter Streaming Mode */
	rc = vb6801_i2c_write_b(REG_MODE_SELECT, 1);
	if (rc < 0) {
		CDBG("I2C Write Table FAILED!!!\n");
		return rc;
	}

	/* Wait until the sensor starts streaming
	 * Poll until the reported frame_count value is != 0xFF */
	frame_count = 0xFF;
	timeout = 2000;
	while (frame_count == 0xFF && timeout > 0) {
		rc = vb6801_i2c_read(REG_FRAME_COUNT, &frame_count, 1);
		if (rc < 0)
			return rc;

		CDBG("REG_FRAME_COUNT  = 0x%x\n", frame_count);
		timeout--;
	}

	/* Post Streaming Configuration */

	rc = vb6801_i2c_write_table(wreg2, ARRAY_SIZE(wreg2));
	if (rc < 0) {
		CDBG("I2C Write Table FAILED!!!\n");
		return rc;
	}

	rc = vb6801_i2c_read_table(rreg, ARRAY_SIZE(rreg));
	if (rc < 0) {
		CDBG("I2C Read Table FAILED!!!\n");
		return rc;
	}

	CDBG("REG_PRE_PLL_CLK_DIV = 0x%x\n", rreg[0].wdata);
	CDBG("REG_PLL_MULTIPLIER  = 0x%x\n", rreg[1].wdata);
	CDBG("REG_VT_SYS_CLK_DIV  = 0x%x\n", rreg[2].wdata);
	CDBG("REG_VT_PIX_CLK_DIV  = 0x%x\n", rreg[3].wdata);
	CDBG("REG_OP_SYS_CLK_DIV  = 0x%x\n", rreg[4].wdata);
	CDBG("REG_OP_PIX_CLK_DIV  = 0x%x\n", rreg[5].wdata);
	CDBG("REG_FRAME_COUNT  = 0x%x\n", rreg[6].bdata);

	mdelay(50);
	frame_count = 0;
	rc = vb6801_i2c_read(REG_FRAME_COUNT, &frame_count, 1);
	CDBG("REG_FRAME_COUNT1  = 0x%x\n", frame_count);

	mdelay(150);
	frame_count = 0;
	rc = vb6801_i2c_read(REG_FRAME_COUNT, &frame_count, 1);
	CDBG("REG_FRAME_COUNT2  = 0x%x\n", frame_count);

	mdelay(100);
	frame_count = 0;
	rc = vb6801_i2c_read(REG_FRAME_COUNT, &frame_count, 1);
	CDBG("REG_FRAME_COUNT3  = 0x%x\n", frame_count);

	mdelay(250);
	frame_count = 0;
	rc = vb6801_i2c_read(REG_FRAME_COUNT, &frame_count, 1);
	CDBG("REG_FRAME_COUNT4  = 0x%x\n", frame_count);

	return rc;
}

static int vb6801_sensor_init_done(const struct msm_camera_sensor_info *data)
{
	gpio_direction_output(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);
	return 0;
}

static int vb6801_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&vb6801_wait_queue);
	return 0;
}

static int32_t vb6801_video_config(int mode, int res)
{
	int32_t rc = 0;

	vb6801_ctrl->prev_res = res;
	vb6801_ctrl->curr_res = res;
	vb6801_ctrl->sensormode = mode;

	rc = vb6801_config_sensor(12, 30, 60, 2, 1, 10, 2, RES_PREVIEW);
	if (rc < 0)
		return rc;

	rc = vb6801_i2c_read(REG_VT_LINE_LENGTH_PCK,
			     &vb6801_ctrl->s_dynamic_params.
			     preview_pixelsPerLine, 2);
	if (rc < 0)
		return rc;

	rc = vb6801_i2c_read(REG_VT_LINE_LENGTH_PCK,
			     &vb6801_ctrl->s_dynamic_params.
			     preview_linesPerFrame, 2);

	return rc;
}

static int32_t vb6801_snapshot_config(int mode, int res)
{
	int32_t rc = 0;

	vb6801_ctrl->curr_res = vb6801_ctrl->pict_res;
	vb6801_ctrl->sensormode = mode;

	rc = vb6801_config_sensor(12, 12, 48, 1, 1, 10, 2, RES_CAPTURE);
	if (rc < 0)
		return rc;

	rc = vb6801_i2c_read(REG_VT_LINE_LENGTH_PCK,
			     &vb6801_ctrl->s_dynamic_params.
			     snapshot_pixelsPerLine, 2);
	if (rc < 0)
		return rc;

	rc = vb6801_i2c_read(REG_VT_LINE_LENGTH_PCK,
			     &vb6801_ctrl->s_dynamic_params.
			     snapshot_linesPerFrame, 2);

	return rc;
}

static int32_t vb6801_set_sensor_mode(int mode, int res)
{
	int32_t rc = 0;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = vb6801_video_config(mode, res);
		break;

	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = vb6801_snapshot_config(mode, res);
		break;

	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

int vb6801_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long rc = 0;

	if (copy_from_user(&cdata,
			   (void *)argp, sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	mutex_lock(&vb6801_mut);

	CDBG("vb6801_sensor_config, cfgtype = %d\n", cdata.cfgtype);

	switch (cdata.cfgtype) {
	case CFG_GET_PICT_FPS:
		vb6801_get_pict_fps(cdata.cfg.gfps.prevfps,
				    &(cdata.cfg.gfps.pictfps));

		if (copy_to_user((void *)argp,
				 &cdata, sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PREV_L_PF:
		cdata.cfg.prevl_pf = vb6801_get_prev_lines_pf();

		if (copy_to_user((void *)argp,
				 &cdata, sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PREV_P_PL:
		cdata.cfg.prevp_pl = vb6801_get_prev_pixels_pl();

		if (copy_to_user((void *)argp,
				 &cdata, sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_L_PF:
		cdata.cfg.pictl_pf = vb6801_get_pict_lines_pf();

		if (copy_to_user((void *)argp,
				 &cdata, sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_P_PL:
		cdata.cfg.pictp_pl = vb6801_get_pict_pixels_pl();

		if (copy_to_user((void *)argp,
				 &cdata, sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_MAX_EXP_LC:
		cdata.cfg.pict_max_exp_lc = vb6801_get_pict_max_exp_lc();

		if (copy_to_user((void *)argp,
				 &cdata, sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_SET_FPS:
	case CFG_SET_PICT_FPS:
		rc = vb6801_set_fps(&(cdata.cfg.fps));
		break;

	case CFG_SET_EXP_GAIN:
		rc = vb6801_write_exp_gain(cdata.cfg.exp_gain.gain,
					   cdata.cfg.exp_gain.line);
		break;

	case CFG_SET_PICT_EXP_GAIN:
		rc = vb6801_set_pict_exp_gain(cdata.cfg.exp_gain.gain,
					      cdata.cfg.exp_gain.line);
		break;

	case CFG_SET_MODE:
		rc = vb6801_set_sensor_mode(cdata.mode, cdata.rs);
		break;

	case CFG_PWR_DOWN:
		rc = vb6801_power_down();
		break;

	case CFG_MOVE_FOCUS:
		rc = vb6801_move_focus(cdata.cfg.focus.dir,
				       cdata.cfg.focus.steps);
		break;

	case CFG_SET_DEFAULT_FOCUS:
		rc = vb6801_set_default_focus();
		break;

	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(&vb6801_mut);

	return rc;
}

static int vb6801_sensor_release(void)
{
	int rc = -EBADF;

	mutex_lock(&vb6801_mut);

	vb6801_power_down();
	vb6801_sensor_init_done(vb6801_ctrl->sensordata);
	kfree(vb6801_ctrl);
	mutex_unlock(&vb6801_mut);

	return rc;
}

static int vb6801_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	vb6801_sensorw = kzalloc(sizeof(struct vb6801_work_t), GFP_KERNEL);

	if (!vb6801_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, vb6801_sensorw);
	vb6801_init_client(client);
	vb6801_client = client;
	vb6801_client->addr = vb6801_client->addr >> 1;

	return 0;

probe_failure:
	if (vb6801_sensorw != NULL) {
		kfree(vb6801_sensorw);
		vb6801_sensorw = NULL;
	}
	return rc;
}

static int __exit vb6801_i2c_remove(struct i2c_client *client)
{
	struct vb6801_work_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	vb6801_client = NULL;
	kfree(sensorw);
	return 0;
}

static const struct i2c_device_id vb6801_i2c_id[] = {
	{"vb6801", 0},
	{}
};

static struct i2c_driver vb6801_i2c_driver = {
	.id_table = vb6801_i2c_id,
	.probe = vb6801_i2c_probe,
	.remove = __exit_p(vb6801_i2c_remove),
	.driver = {
		   .name = "vb6801",
		   },
};

static int vb6801_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int rc;

	struct vb6801_i2c_reg_conf_t rreg[] = {
		{0x0000, 0, 0, D_LEN_BYTE},
		{0x0001, 0, 0, D_LEN_BYTE},
	};

	rc = vb6801_reset(data);
	if (rc < 0)
		goto init_probe_done;

	mdelay(20);

	rc = vb6801_i2c_read_table(rreg, ARRAY_SIZE(rreg));
	if (rc < 0) {
		CDBG("I2C Read Table FAILED!!!\n");
		goto init_probe_fail;
	}

	/* 4. Compare sensor ID to VB6801 ID: */
	if (rreg[0].bdata != 0x03 || rreg[1].bdata != 0x53) {
		CDBG("vb6801_sensor_init: sensor ID don't match!\n");
		goto init_probe_fail;
	}

	goto init_probe_done;

init_probe_fail:
	vb6801_sensor_init_done(data);
init_probe_done:
	return rc;
}

int vb6801_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc;
	struct vb6801_i2c_reg_conf_t wreg[] = {
		{REG_MODE_SELECT, 0, STANDBY_MODE, D_LEN_BYTE},
		{0x0113, 0, 0x0A, D_LEN_BYTE},
	};

	vb6801_ctrl = kzalloc(sizeof(struct vb6801_ctrl_t), GFP_KERNEL);
	if (!vb6801_ctrl) {
		rc = -ENOMEM;
		goto open_init_fail1;
	}

	vb6801_ctrl->factor_fps = 1 /** 0x00000400*/ ;
	vb6801_ctrl->curr_fps = 7680; /* 30 * Q8 */ ;
	vb6801_ctrl->max_fps = 7680; /* 30 * Q8 */ ;
	vb6801_ctrl->pict_exp_update = 0; /* 30 * Q8 */ ;
	vb6801_ctrl->reducel = 0; /* 30 * Q8 */ ;

	vb6801_ctrl->set_test = TEST_OFF;
	vb6801_ctrl->prev_res = QTR_SIZE;
	vb6801_ctrl->pict_res = FULL_SIZE;

	vb6801_ctrl->s_dynamic_params.preview_linesPerFrame =
	    VB6801_LINES_PER_FRAME_PREVIEW;
	vb6801_ctrl->s_dynamic_params.preview_pixelsPerLine =
	    VB6801_PIXELS_PER_LINE_PREVIEW;
	vb6801_ctrl->s_dynamic_params.snapshot_linesPerFrame =
	    VB6801_LINES_PER_FRAME_SNAPSHOT;
	vb6801_ctrl->s_dynamic_params.snapshot_pixelsPerLine =
	    VB6801_PIXELS_PER_LINE_SNAPSHOT;

	if (data)
		vb6801_ctrl->sensordata = data;

	/* enable mclk first */
	msm_camio_clk_rate_set(VB6801_DEFAULT_CLOCK_RATE);
	mdelay(20);

	rc = vb6801_reset(data);
	if (rc < 0)
		goto open_init_fail1;

	rc = vb6801_i2c_write_table(wreg, ARRAY_SIZE(wreg));
	if (rc < 0) {
		CDBG("I2C Write Table FAILED!!!\n");
		goto open_init_fail2;
	}

	rc = vb6801_read_nvm_data(&vb6801_ctrl->s_info);
	if (rc < 0) {
		CDBG("vb6801_read_nvm_data FAILED!!!\n");
		goto open_init_fail2;
	}
	mdelay(66);

	rc = vb6801_config_sensor(12, 30, 60, 2, 1, 10, 2, RES_PREVIEW);
	if (rc < 0)
		goto open_init_fail2;

	goto open_init_done;

open_init_fail2:
	vb6801_sensor_init_done(data);
open_init_fail1:
	kfree(vb6801_ctrl);
open_init_done:
	return rc;
}

static int vb6801_sensor_probe(const struct msm_camera_sensor_info *info,
			       struct msm_sensor_ctrl *s)
{
	int rc = i2c_add_driver(&vb6801_i2c_driver);
	if (rc < 0 || vb6801_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	/* enable mclk first */
	msm_camio_clk_rate_set(VB6801_DEFAULT_CLOCK_RATE);
	mdelay(20);

	rc = vb6801_probe_init_sensor(info);
	if (rc < 0)
		goto probe_done;

	s->s_init = vb6801_sensor_open_init;
	s->s_release = vb6801_sensor_release;
	s->s_config = vb6801_sensor_config;
	s->s_mount_angle  = 0;
	vb6801_sensor_init_done(info);

probe_done:
	return rc;
}

static int __vb6801_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, vb6801_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __vb6801_probe,
	.driver = {
		   .name = "msm_camera_vb6801",
		   .owner = THIS_MODULE,
		   },
};

static int __init vb6801_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(vb6801_init);
void vb6801_exit(void)
{
	i2c_del_driver(&vb6801_i2c_driver);
}
