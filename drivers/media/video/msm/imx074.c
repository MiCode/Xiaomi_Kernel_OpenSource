/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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
#include <asm/mach-types.h>
#include "imx074.h"

/*SENSOR REGISTER DEFINES*/
#define	IMX074_EEPROM_SLAVE_ADDR			0x52
#define REG_GROUPED_PARAMETER_HOLD			0x0104
#define GROUPED_PARAMETER_HOLD_OFF			0x00
#define GROUPED_PARAMETER_HOLD				0x01
#define REG_MODE_SELECT					0x100
#define MODE_SELECT_STANDBY_MODE			0x00
#define MODE_SELECT_STREAM				0x01
/* Integration Time */
#define REG_COARSE_INTEGRATION_TIME_HI			0x0202
#define REG_COARSE_INTEGRATION_TIME_LO			0x0203
/* Gain */
#define REG_ANALOGUE_GAIN_CODE_GLOBAL_HI		0x0204
#define REG_ANALOGUE_GAIN_CODE_GLOBAL_LO		0x0205
/* PLL registers */
#define REG_PLL_MULTIPLIER				0x0307
#define REG_PRE_PLL_CLK_DIV				0x0305
#define REG_PLSTATIM					0x302b
#define REG_3024					0x3024
#define REG_IMAGE_ORIENTATION				0x0101
#define REG_VNDMY_ABLMGSHLMT				0x300a
#define REG_Y_OPBADDR_START_DI				0x3014
#define REG_3015					0x3015
#define REG_301C					0x301C
#define REG_302C					0x302C
#define REG_3031					0x3031
#define REG_3041					0x3041
#define REG_3051					0x3051
#define REG_3053					0x3053
#define REG_3057					0x3057
#define REG_305C					0x305C
#define REG_305D					0x305D
#define REG_3060					0x3060
#define REG_3065					0x3065
#define REG_30AA					0x30AA
#define REG_30AB					0x30AB
#define REG_30B0					0x30B0
#define REG_30B2					0x30B2
#define REG_30D3					0x30D3
#define REG_3106					0x3106
#define REG_310C					0x310C
#define REG_3304					0x3304
#define REG_3305					0x3305
#define REG_3306					0x3306
#define REG_3307					0x3307
#define REG_3308					0x3308
#define REG_3309					0x3309
#define REG_330A					0x330A
#define REG_330B					0x330B
#define REG_330C					0x330C
#define REG_330D					0x330D
#define REG_330F					0x330F
#define REG_3381					0x3381

/* mode setting */
#define REG_FRAME_LENGTH_LINES_HI			0x0340
#define REG_FRAME_LENGTH_LINES_LO			0x0341
#define REG_YADDR_START					0x0347
#define REG_YAAAR_END					0x034b
#define REG_X_OUTPUT_SIZE_MSB				0x034c
#define REG_X_OUTPUT_SIZE_LSB				0x034d
#define REG_Y_OUTPUT_SIZE_MSB				0x034e
#define REG_Y_OUTPUT_SIZE_LSB				0x034f
#define REG_X_EVEN_INC					0x0381
#define REG_X_ODD_INC					0x0383
#define REG_Y_EVEN_INC					0x0385
#define REG_Y_ODD_INC					0x0387
#define REG_HMODEADD					0x3001
#define REG_VMODEADD					0x3016
#define REG_VAPPLINE_START				0x3069
#define REG_VAPPLINE_END				0x306b
#define REG_SHUTTER					0x3086
#define REG_HADDAVE					0x30e8
#define REG_LANESEL					0x3301
/* Test Pattern */
#define REG_TEST_PATTERN_MODE				0x0601

#define REG_LINE_LENGTH_PCK_HI				0x0342
#define REG_LINE_LENGTH_PCK_LO				0x0343
/*..... TYPE DECLARATIONS.....*/
#define	IMX074_OFFSET					3
#define	IMX074_DEFAULT_MASTER_CLK_RATE			24000000
/* Full	Size */
#define	IMX074_FULL_SIZE_WIDTH				4208
#define	IMX074_FULL_SIZE_HEIGHT				3120
#define	IMX074_FULL_SIZE_DUMMY_PIXELS			0
#define	IMX074_FULL_SIZE_DUMMY_LINES			0
/* Quarter Size	*/
#define	IMX074_QTR_SIZE_WIDTH				2104
#define	IMX074_QTR_SIZE_HEIGHT				1560
#define	IMX074_QTR_SIZE_DUMMY_PIXELS			0
#define	IMX074_QTR_SIZE_DUMMY_LINES			0
/* Blanking as measured	on the scope */
/* Full	Size */
#define	IMX074_HRZ_FULL_BLK_PIXELS			264
#define	IMX074_VER_FULL_BLK_LINES			96
/* Quarter Size	*/
#define	IMX074_HRZ_QTR_BLK_PIXELS			2368
#define	IMX074_VER_QTR_BLK_LINES			21
#define	Q8						0x100
#define	Q10						0x400
#define	IMX074_AF_I2C_SLAVE_ID				0x72
#define	IMX074_STEPS_NEAR_TO_CLOSEST_INF		52
#define	IMX074_TOTAL_STEPS_NEAR_TO_FAR			52
static uint32_t imx074_l_region_code_per_step = 2;

struct imx074_work_t {
	struct work_struct work;
};

static struct imx074_work_t *imx074_sensorw;
static struct i2c_client *imx074_client;

struct imx074_ctrl_t {
	const struct msm_camera_sensor_info *sensordata;
	uint32_t sensormode;
	uint32_t fps_divider;/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;/* init to 1 * 0x00000400 */
	uint16_t fps;
	int16_t curr_lens_pos;
	uint16_t curr_step_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint16_t total_lines_per_frame;
	enum imx074_resolution_t prev_res;
	enum imx074_resolution_t pict_res;
	enum imx074_resolution_t curr_res;
	enum imx074_test_mode_t set_test;
	unsigned short imgaddr;
};
static uint8_t imx074_delay_msecs_stdby = 5;
static uint16_t imx074_delay_msecs_stream = 5;
static int32_t config_csi;

static struct imx074_ctrl_t *imx074_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(imx074_wait_queue);
DEFINE_MUTEX(imx074_mut);

/*=============================================================*/

static int imx074_i2c_rxdata(unsigned short saddr,
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
			.len   = 2,
			.buf   = rxdata,
		},
	};
	if (i2c_transfer(imx074_client->adapter, msgs, 2) < 0) {
		CDBG("imx074_i2c_rxdata failed!\n");
		return -EIO;
	}
	return 0;
}
static int32_t imx074_i2c_txdata(unsigned short saddr,
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
	if (i2c_transfer(imx074_client->adapter, msg, 1) < 0) {
		CDBG("imx074_i2c_txdata faild 0x%x\n", imx074_client->addr);
		return -EIO;
	}

	return 0;
}


static int32_t imx074_i2c_read(unsigned short raddr,
	unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[2];
	if (!rdata)
		return -EIO;
	memset(buf, 0, sizeof(buf));
	buf[0] = (raddr & 0xFF00) >> 8;
	buf[1] = (raddr & 0x00FF);
	rc = imx074_i2c_rxdata(imx074_client->addr, buf, rlen);
	if (rc < 0) {
		CDBG("imx074_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = (rlen == 2 ? buf[0] << 8 | buf[1] : buf[0]);
	return rc;
}

static int imx074_af_i2c_rxdata_b(unsigned short saddr,
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

	if (i2c_transfer(imx074_client->adapter, msgs, 2) < 0) {
		CDBG("imx074_i2c_rxdata_b failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t imx074_i2c_read_w_eeprom(unsigned short raddr,
	unsigned short *rdata)
{
	int32_t rc;
	unsigned char buf;
	if (!rdata)
		return -EIO;
	/* Read 2 bytes in sequence */
	buf = (raddr & 0x00FF);
	rc = imx074_af_i2c_rxdata_b(IMX074_EEPROM_SLAVE_ADDR, &buf, 1);
	if (rc < 0) {
		CDBG("imx074_i2c_read_eeprom 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = buf<<8;

	/* Read Second byte of data */
	buf = (raddr & 0x00FF) + 1;
	rc = imx074_af_i2c_rxdata_b(IMX074_EEPROM_SLAVE_ADDR, &buf, 1);
	if (rc < 0) {
		CDBG("imx074_i2c_read_eeprom 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata |= buf;
	return rc;
}

static int32_t imx074_i2c_write_b_sensor(unsigned short waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[3];
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;
	CDBG("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, bdata);
	rc = imx074_i2c_txdata(imx074_client->addr, buf, 3);
	if (rc < 0) {
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			waddr, bdata);
	}
	return rc;
}
static int16_t imx074_i2c_write_b_af(unsigned short saddr,
	unsigned short baddr, unsigned short bdata)
{
	int32_t rc;
	unsigned char buf[2];
	memset(buf, 0, sizeof(buf));
	buf[0] = baddr;
	buf[1] = bdata;
	rc = imx074_i2c_txdata(saddr, buf, 2);
	if (rc < 0)
		CDBG("AFi2c_write failed, saddr = 0x%x addr = 0x%x, val =0x%x!",
			saddr, baddr, bdata);
	return rc;
}

static int32_t imx074_i2c_write_w_table(struct imx074_i2c_reg_conf const
					 *reg_conf_tbl, int num)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < num; i++) {
		rc = imx074_i2c_write_b_sensor(reg_conf_tbl->waddr,
			reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}
static int16_t imx074_af_init(void)
{
	int32_t rc;
	/* Initialize waveform */
	rc = imx074_i2c_write_b_af(IMX074_AF_I2C_SLAVE_ID, 0x01, 0xA9);
	rc = imx074_i2c_write_b_af(IMX074_AF_I2C_SLAVE_ID, 0x02, 0xD2);
	rc = imx074_i2c_write_b_af(IMX074_AF_I2C_SLAVE_ID, 0x03, 0x0C);
	rc = imx074_i2c_write_b_af(IMX074_AF_I2C_SLAVE_ID, 0x04, 0x14);
	rc = imx074_i2c_write_b_af(IMX074_AF_I2C_SLAVE_ID, 0x05, 0xB6);
	rc = imx074_i2c_write_b_af(IMX074_AF_I2C_SLAVE_ID, 0x06, 0x4F);
	return rc;
}

static void imx074_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint16_t preview_frame_length_lines, snapshot_frame_length_lines;
	uint32_t divider, d1;
	uint32_t pclk_mult;/*Q10 */
	/* Total frame_length_lines and line_length_pck for preview */
	preview_frame_length_lines = IMX074_QTR_SIZE_HEIGHT +
		IMX074_VER_QTR_BLK_LINES;
	/* Total frame_length_lines and line_length_pck for snapshot */
	snapshot_frame_length_lines = IMX074_FULL_SIZE_HEIGHT +
		IMX074_VER_FULL_BLK_LINES;
	d1 = preview_frame_length_lines * 0x00010000 /
		snapshot_frame_length_lines;
	pclk_mult =
		(uint32_t) ((imx074_regs.reg_pat[RES_CAPTURE].pll_multiplier *
		0x00010000) /
		(imx074_regs.reg_pat[RES_PREVIEW].pll_multiplier));
	divider = d1 * pclk_mult / 0x00010000;
	*pfps = (uint16_t) (fps * divider / 0x00010000);
}

static uint16_t imx074_get_prev_lines_pf(void)
{
	if (imx074_ctrl->prev_res == QTR_SIZE)
		return IMX074_QTR_SIZE_HEIGHT + IMX074_VER_QTR_BLK_LINES;
	else
		return IMX074_FULL_SIZE_HEIGHT + IMX074_VER_FULL_BLK_LINES;

}

static uint16_t imx074_get_prev_pixels_pl(void)
{
	if (imx074_ctrl->prev_res == QTR_SIZE)
		return IMX074_QTR_SIZE_WIDTH + IMX074_HRZ_QTR_BLK_PIXELS;
	else
		return IMX074_FULL_SIZE_WIDTH + IMX074_HRZ_FULL_BLK_PIXELS;
}

static uint16_t imx074_get_pict_lines_pf(void)
{
		if (imx074_ctrl->pict_res == QTR_SIZE)
			return IMX074_QTR_SIZE_HEIGHT +
				IMX074_VER_QTR_BLK_LINES;
		else
			return IMX074_FULL_SIZE_HEIGHT +
				IMX074_VER_FULL_BLK_LINES;
}

static uint16_t imx074_get_pict_pixels_pl(void)
{
	if (imx074_ctrl->pict_res == QTR_SIZE)
		return IMX074_QTR_SIZE_WIDTH +
			IMX074_HRZ_QTR_BLK_PIXELS;
	else
		return IMX074_FULL_SIZE_WIDTH +
			IMX074_HRZ_FULL_BLK_PIXELS;
}

static uint32_t imx074_get_pict_max_exp_lc(void)
{
	if (imx074_ctrl->pict_res == QTR_SIZE)
		return (IMX074_QTR_SIZE_HEIGHT +
			IMX074_VER_QTR_BLK_LINES)*24;
	else
		return (IMX074_FULL_SIZE_HEIGHT +
			IMX074_VER_FULL_BLK_LINES)*24;
}

static int32_t imx074_set_fps(struct fps_cfg	*fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;
	imx074_ctrl->fps_divider = fps->fps_div;
	imx074_ctrl->pict_fps_divider = fps->pict_fps_div;
	if (imx074_ctrl->curr_res  == QTR_SIZE) {
		total_lines_per_frame = (uint16_t)(((IMX074_QTR_SIZE_HEIGHT +
			IMX074_VER_QTR_BLK_LINES) *
			imx074_ctrl->fps_divider) / 0x400);
	} else {
		total_lines_per_frame = (uint16_t)(((IMX074_FULL_SIZE_HEIGHT +
			IMX074_VER_FULL_BLK_LINES) *
			imx074_ctrl->pict_fps_divider) / 0x400);
	}
	if (imx074_i2c_write_b_sensor(REG_FRAME_LENGTH_LINES_HI,
		((total_lines_per_frame & 0xFF00) >> 8)) < 0)
		return rc;
	if (imx074_i2c_write_b_sensor(REG_FRAME_LENGTH_LINES_LO,
		(total_lines_per_frame & 0x00FF)) < 0)
		return rc;
	return rc;
}

static int32_t imx074_write_exp_gain(uint16_t gain, uint32_t line)
{
	static uint16_t max_legal_gain = 0x00E0;
	uint8_t gain_msb, gain_lsb;
	uint8_t intg_time_msb, intg_time_lsb;
	uint8_t frame_length_line_msb, frame_length_line_lsb;
	uint16_t frame_length_lines;
	int32_t rc = -1;

	CDBG("imx074_write_exp_gain : gain = %d line = %d", gain, line);
	if (imx074_ctrl->curr_res  == QTR_SIZE) {
		frame_length_lines = IMX074_QTR_SIZE_HEIGHT +
			IMX074_VER_QTR_BLK_LINES;
		frame_length_lines = frame_length_lines *
			imx074_ctrl->fps_divider / 0x400;
	} else {
		frame_length_lines = IMX074_FULL_SIZE_HEIGHT +
			IMX074_VER_FULL_BLK_LINES;
		frame_length_lines = frame_length_lines *
			imx074_ctrl->pict_fps_divider / 0x400;
	}
	if (line > (frame_length_lines - IMX074_OFFSET))
		frame_length_lines = line + IMX074_OFFSET;

	CDBG("imx074 setting line = %d\n", line);


	CDBG("imx074 setting frame_length_lines = %d\n",
					frame_length_lines);

	if (gain > max_legal_gain)
		/* range: 0 to 224 */
		gain = max_legal_gain;

	/* update gain registers */
	gain_msb = (uint8_t) ((gain & 0xFF00) >> 8);
	gain_lsb = (uint8_t) (gain & 0x00FF);

	frame_length_line_msb = (uint8_t) ((frame_length_lines & 0xFF00) >> 8);
	frame_length_line_lsb = (uint8_t) (frame_length_lines & 0x00FF);

	/* update line count registers */
	intg_time_msb = (uint8_t) ((line & 0xFF00) >> 8);
	intg_time_lsb = (uint8_t) (line & 0x00FF);

	rc = imx074_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
					GROUPED_PARAMETER_HOLD);
	if (rc < 0)
		return rc;
	CDBG("imx074 setting REG_ANALOGUE_GAIN_CODE_GLOBAL_HI = 0x%X\n",
					gain_msb);
	rc = imx074_i2c_write_b_sensor(REG_ANALOGUE_GAIN_CODE_GLOBAL_HI,
					gain_msb);
	if (rc < 0)
		return rc;
	CDBG("imx074 setting REG_ANALOGUE_GAIN_CODE_GLOBAL_LO = 0x%X\n",
					gain_lsb);
	rc = imx074_i2c_write_b_sensor(REG_ANALOGUE_GAIN_CODE_GLOBAL_LO,
					gain_lsb);
	if (rc < 0)
		return rc;

	CDBG("imx074 setting REG_FRAME_LENGTH_LINES_HI = 0x%X\n",
					frame_length_line_msb);
	rc = imx074_i2c_write_b_sensor(REG_FRAME_LENGTH_LINES_HI,
			frame_length_line_msb);
	if (rc < 0)
		return rc;

	CDBG("imx074 setting REG_FRAME_LENGTH_LINES_LO = 0x%X\n",
			frame_length_line_lsb);
	rc = imx074_i2c_write_b_sensor(REG_FRAME_LENGTH_LINES_LO,
			frame_length_line_lsb);
	if (rc < 0)
		return rc;

	CDBG("imx074 setting REG_COARSE_INTEGRATION_TIME_HI = 0x%X\n",
					intg_time_msb);
	rc = imx074_i2c_write_b_sensor(REG_COARSE_INTEGRATION_TIME_HI,
					intg_time_msb);
	if (rc < 0)
		return rc;

	CDBG("imx074 setting REG_COARSE_INTEGRATION_TIME_LO = 0x%X\n",
					intg_time_lsb);
	rc = imx074_i2c_write_b_sensor(REG_COARSE_INTEGRATION_TIME_LO,
					intg_time_lsb);
	if (rc < 0)
		return rc;

	rc = imx074_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
					GROUPED_PARAMETER_HOLD_OFF);
	if (rc < 0)
		return rc;

	return rc;
}

static int32_t imx074_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;
	rc = imx074_write_exp_gain(gain, line);
	return rc;
}

static int32_t imx074_move_focus(int direction,
	int32_t num_steps)
{
	int32_t step_direction, dest_step_position, bit_mask;
	int32_t rc = 0;

	if (num_steps == 0)
		return rc;

	if (direction == MOVE_NEAR) {
		step_direction = 1;
		bit_mask = 0x80;
	} else if (direction == MOVE_FAR) {
		step_direction = -1;
		bit_mask = 0x00;
	} else {
		CDBG("imx074_move_focus: Illegal focus direction");
		return -EINVAL;
	}
	dest_step_position = imx074_ctrl->curr_step_pos +
		(step_direction * num_steps);
	if (dest_step_position < 0)
		dest_step_position = 0;
	else if (dest_step_position > IMX074_TOTAL_STEPS_NEAR_TO_FAR)
		dest_step_position = IMX074_TOTAL_STEPS_NEAR_TO_FAR;
	rc = imx074_i2c_write_b_af(IMX074_AF_I2C_SLAVE_ID, 0x00,
		((num_steps * imx074_l_region_code_per_step) | bit_mask));
	CDBG("%s: Index: %d\n", __func__, dest_step_position);
	imx074_ctrl->curr_step_pos = dest_step_position;
	return rc;
}


static int32_t imx074_set_default_focus(uint8_t af_step)
{
	int32_t rc;
	/* Initialize to infinity */
	rc = imx074_i2c_write_b_af(IMX074_AF_I2C_SLAVE_ID, 0x00, 0x7F);
	rc = imx074_i2c_write_b_af(IMX074_AF_I2C_SLAVE_ID, 0x00, 0x7F);
	imx074_ctrl->curr_step_pos = 0;
	return rc;
}
static int32_t imx074_test(enum imx074_test_mode_t mo)
{
	int32_t rc = 0;
	if (mo == TEST_OFF)
		return rc;
	else {
		/* Set mo to 2 inorder to enable test pattern*/
		if (imx074_i2c_write_b_sensor(REG_TEST_PATTERN_MODE,
			(uint8_t) mo) < 0) {
			return rc;
		}
	}
	return rc;
}
static int32_t imx074_sensor_setting(int update_type, int rt)
{
	int32_t rc = 0;
	struct msm_camera_csi_params imx074_csi_params;
	switch (update_type) {
	case REG_INIT:
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			struct imx074_i2c_reg_conf init_tbl[] = {
				{REG_PRE_PLL_CLK_DIV,
					imx074_regs.reg_pat_init[0].
					pre_pll_clk_div},
				{REG_PLSTATIM,
					imx074_regs.reg_pat_init[0].
					plstatim},
				{REG_3024,
					imx074_regs.reg_pat_init[0].
					reg_3024},
				{REG_IMAGE_ORIENTATION,
					imx074_regs.reg_pat_init[0].
					image_orientation},
				{REG_VNDMY_ABLMGSHLMT,
					imx074_regs.reg_pat_init[0].
					vndmy_ablmgshlmt},
				{REG_Y_OPBADDR_START_DI,
					imx074_regs.reg_pat_init[0].
					y_opbaddr_start_di},
				{REG_3015,
					imx074_regs.reg_pat_init[0].
					reg_0x3015},
				{REG_301C,
					imx074_regs.reg_pat_init[0].
					reg_0x301c},
				{REG_302C,
					imx074_regs.reg_pat_init[0].
					reg_0x302c},
				{REG_3031,
					imx074_regs.reg_pat_init[0].reg_0x3031},
				{REG_3041,
					imx074_regs.reg_pat_init[0].reg_0x3041},
				{REG_3051,
					imx074_regs.reg_pat_init[0].reg_0x3051},
				{REG_3053,
					imx074_regs.reg_pat_init[0].reg_0x3053},
				{REG_3057,
					imx074_regs.reg_pat_init[0].reg_0x3057},
				{REG_305C,
					imx074_regs.reg_pat_init[0].reg_0x305c},
				{REG_305D,
					imx074_regs.reg_pat_init[0].reg_0x305d},
				{REG_3060,
					imx074_regs.reg_pat_init[0].reg_0x3060},
				{REG_3065,
					imx074_regs.reg_pat_init[0].reg_0x3065},
				{REG_30AA,
					imx074_regs.reg_pat_init[0].reg_0x30aa},
				{REG_30AB,
					imx074_regs.reg_pat_init[0].reg_0x30ab},
				{REG_30B0,
					imx074_regs.reg_pat_init[0].reg_0x30b0},
				{REG_30B2,
					imx074_regs.reg_pat_init[0].reg_0x30b2},
				{REG_30D3,
					imx074_regs.reg_pat_init[0].reg_0x30d3},
				{REG_3106,
					imx074_regs.reg_pat_init[0].reg_0x3106},
				{REG_310C,
					imx074_regs.reg_pat_init[0].reg_0x310c},
				{REG_3304,
					imx074_regs.reg_pat_init[0].reg_0x3304},
				{REG_3305,
					imx074_regs.reg_pat_init[0].reg_0x3305},
				{REG_3306,
					imx074_regs.reg_pat_init[0].reg_0x3306},
				{REG_3307,
					imx074_regs.reg_pat_init[0].reg_0x3307},
				{REG_3308,
					imx074_regs.reg_pat_init[0].reg_0x3308},
				{REG_3309,
					imx074_regs.reg_pat_init[0].reg_0x3309},
				{REG_330A,
					imx074_regs.reg_pat_init[0].reg_0x330a},
				{REG_330B,
					imx074_regs.reg_pat_init[0].reg_0x330b},
				{REG_330C,
					imx074_regs.reg_pat_init[0].reg_0x330c},
				{REG_330D,
					imx074_regs.reg_pat_init[0].reg_0x330d},
				{REG_330F,
					imx074_regs.reg_pat_init[0].reg_0x330f},
				{REG_3381,
					imx074_regs.reg_pat_init[0].reg_0x3381},
			};
			struct imx074_i2c_reg_conf init_mode_tbl[] = {
				{REG_GROUPED_PARAMETER_HOLD,
					GROUPED_PARAMETER_HOLD},
				{REG_PLL_MULTIPLIER,
					imx074_regs.reg_pat[rt].
					pll_multiplier},
				{REG_FRAME_LENGTH_LINES_HI,
					imx074_regs.reg_pat[rt].
					frame_length_lines_hi},
				{REG_FRAME_LENGTH_LINES_LO,
					imx074_regs.reg_pat[rt].
					frame_length_lines_lo},
				{REG_YADDR_START ,
					imx074_regs.reg_pat[rt].
					y_addr_start},
				{REG_YAAAR_END,
					imx074_regs.reg_pat[rt].
					y_add_end},
				{REG_X_OUTPUT_SIZE_MSB,
					imx074_regs.reg_pat[rt].
					x_output_size_msb},
				{REG_X_OUTPUT_SIZE_LSB,
					imx074_regs.reg_pat[rt].
					x_output_size_lsb},
				{REG_Y_OUTPUT_SIZE_MSB,
					imx074_regs.reg_pat[rt].
					y_output_size_msb},
				{REG_Y_OUTPUT_SIZE_LSB ,
					imx074_regs.reg_pat[rt].
					y_output_size_lsb},
				{REG_X_EVEN_INC,
					imx074_regs.reg_pat[rt].
					x_even_inc},
				{REG_X_ODD_INC,
					imx074_regs.reg_pat[rt].
					x_odd_inc},
				{REG_Y_EVEN_INC,
					imx074_regs.reg_pat[rt].
					y_even_inc},
				{REG_Y_ODD_INC,
					imx074_regs.reg_pat[rt].
					y_odd_inc},
				{REG_HMODEADD,
					imx074_regs.reg_pat[rt].
					hmodeadd},
				{REG_VMODEADD,
					imx074_regs.reg_pat[rt].
					vmodeadd},
				{REG_VAPPLINE_START,
					imx074_regs.reg_pat[rt].
					vapplinepos_start},
				{REG_VAPPLINE_END,
					imx074_regs.reg_pat[rt].
					vapplinepos_end},
				{REG_SHUTTER,
					imx074_regs.reg_pat[rt].
					shutter},
				{REG_HADDAVE,
					imx074_regs.reg_pat[rt].
					haddave},
				{REG_LANESEL,
					imx074_regs.reg_pat[rt].
					lanesel},
				{REG_GROUPED_PARAMETER_HOLD,
					GROUPED_PARAMETER_HOLD_OFF},

			};
			/* reset fps_divider */
			imx074_ctrl->fps = 30 * Q8;
			imx074_ctrl->fps_divider = 1 * 0x400;
			/* stop streaming */
			rc = imx074_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STANDBY_MODE);
			if (rc < 0)
				return rc;
			msleep(imx074_delay_msecs_stdby);
			rc = imx074_i2c_write_w_table(&init_tbl[0],
				ARRAY_SIZE(init_tbl));
			if (rc < 0)
				return rc;
			rc = imx074_i2c_write_w_table(&init_mode_tbl[0],
				ARRAY_SIZE(init_mode_tbl));
			if (rc < 0)
				return rc;
			rc = imx074_test(imx074_ctrl->set_test);
			return rc;
		}
		break;
	case UPDATE_PERIODIC:
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			struct imx074_i2c_reg_conf mode_tbl[] = {
				{REG_GROUPED_PARAMETER_HOLD,
					GROUPED_PARAMETER_HOLD},
				{REG_PLL_MULTIPLIER,
					imx074_regs.reg_pat[rt].
					pll_multiplier},
				{REG_FRAME_LENGTH_LINES_HI,
					imx074_regs.reg_pat[rt].
					frame_length_lines_hi},
				{REG_FRAME_LENGTH_LINES_LO,
					imx074_regs.reg_pat[rt].
					frame_length_lines_lo},
				{REG_YADDR_START ,
					imx074_regs.reg_pat[rt].
					y_addr_start},
				{REG_YAAAR_END,
					imx074_regs.reg_pat[rt].
					y_add_end},
				{REG_X_OUTPUT_SIZE_MSB,
					imx074_regs.reg_pat[rt].
					x_output_size_msb},
				{REG_X_OUTPUT_SIZE_LSB,
					imx074_regs.reg_pat[rt].
					x_output_size_lsb},
				{REG_Y_OUTPUT_SIZE_MSB,
					imx074_regs.reg_pat[rt].
					y_output_size_msb},
				{REG_Y_OUTPUT_SIZE_LSB ,
					imx074_regs.reg_pat[rt].
					y_output_size_lsb},
				{REG_X_EVEN_INC,
					imx074_regs.reg_pat[rt].
					x_even_inc},
				{REG_X_ODD_INC,
					imx074_regs.reg_pat[rt].
					x_odd_inc},
				{REG_Y_EVEN_INC,
					imx074_regs.reg_pat[rt].
					y_even_inc},
				{REG_Y_ODD_INC,
					imx074_regs.reg_pat[rt].
					y_odd_inc},
				{REG_HMODEADD,
					imx074_regs.reg_pat[rt].
					hmodeadd},
				{REG_VMODEADD,
					imx074_regs.reg_pat[rt].
					vmodeadd},
				{REG_VAPPLINE_START,
					imx074_regs.reg_pat[rt].
					vapplinepos_start},
				{REG_VAPPLINE_END,
					imx074_regs.reg_pat[rt].
					vapplinepos_end},
				{REG_SHUTTER,
					imx074_regs.reg_pat[rt].
					shutter},
				{REG_HADDAVE,
					imx074_regs.reg_pat[rt].
					haddave},
				{REG_LANESEL,
					imx074_regs.reg_pat[rt].
					lanesel},
				{REG_GROUPED_PARAMETER_HOLD,
					GROUPED_PARAMETER_HOLD_OFF},
			};

			/* stop streaming */
			rc = imx074_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STANDBY_MODE);
			msleep(imx074_delay_msecs_stdby);
			if (config_csi == 0) {
				imx074_csi_params.lane_cnt = 4;
				imx074_csi_params.data_format = CSI_10BIT;
				imx074_csi_params.lane_assign = 0xe4;
				imx074_csi_params.dpcm_scheme = 0;
				imx074_csi_params.settle_cnt = 0x14;
				rc = msm_camio_csi_config(&imx074_csi_params);
				/*imx074_delay_msecs_stdby*/
				msleep(imx074_delay_msecs_stream);
				config_csi = 1;
			}
			rc = imx074_i2c_write_w_table(&mode_tbl[0],
				ARRAY_SIZE(mode_tbl));
			if (rc < 0)
				return rc;
			rc = imx074_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STREAM);
			if (rc < 0)
				return rc;
			msleep(imx074_delay_msecs_stream);
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}


static int32_t imx074_video_config(int mode)
{

	int32_t	rc = 0;
	int	rt;
	/* change sensor resolution	if needed */
	if (imx074_ctrl->prev_res == QTR_SIZE) {
		rt = RES_PREVIEW;
	} else {
		rt = RES_CAPTURE;
	}
	if (imx074_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	imx074_ctrl->curr_res = imx074_ctrl->prev_res;
	imx074_ctrl->sensormode = mode;
	return rc;
}

static int32_t imx074_snapshot_config(int mode)
{
	int32_t rc = 0;
	int rt = RES_PREVIEW; /* TODO: Used without initialization, guessing. */
	/* change sensor resolution if needed */
	if (imx074_ctrl->curr_res != imx074_ctrl->pict_res) {
		if (imx074_ctrl->pict_res == QTR_SIZE) {
			rt = RES_PREVIEW;
		} else {
			rt = RES_CAPTURE;
		}
	}
	if (imx074_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	imx074_ctrl->curr_res = imx074_ctrl->pict_res;
	imx074_ctrl->sensormode = mode;
	return rc;
}
static int32_t imx074_raw_snapshot_config(int mode)
{
	int32_t rc = 0;
	int rt = RES_PREVIEW; /* TODO: Used without initialization, guessing. */
	/* change sensor resolution if needed */
	if (imx074_ctrl->curr_res != imx074_ctrl->pict_res) {
		if (imx074_ctrl->pict_res == QTR_SIZE) {
			rt = RES_PREVIEW;
		} else {
			rt = RES_CAPTURE;
		}
	}
	if (imx074_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	imx074_ctrl->curr_res = imx074_ctrl->pict_res;
	imx074_ctrl->sensormode = mode;
	return rc;
}
static int32_t imx074_set_sensor_mode(int mode,
	int res)
{
	int32_t rc = 0;
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = imx074_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
		rc = imx074_snapshot_config(mode);
		break;
	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = imx074_raw_snapshot_config(mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}
static int32_t imx074_power_down(void)
{
	imx074_i2c_write_b_sensor(REG_MODE_SELECT,
		MODE_SELECT_STANDBY_MODE);
	msleep(imx074_delay_msecs_stdby);
	return 0;
}
static int imx074_probe_init_done(const struct msm_camera_sensor_info *data)
{
	gpio_set_value_cansleep(data->sensor_reset, 0);
	gpio_direction_input(data->sensor_reset);
	gpio_free(data->sensor_reset);
	return 0;
}

static int imx074_read_eeprom_data(struct sensor_cfg_data *cfg)
{
	int32_t rc = 0;
	uint16_t eepromdata = 0;
	uint8_t addr = 0;

	addr = 0x10;
	rc = imx074_i2c_read_w_eeprom(addr, &eepromdata);
	if (rc < 0) {
		CDBG("%s: Error Reading EEPROM @ 0x%x\n", __func__, addr);
		return rc;
	}
	cfg->cfg.calib_info.r_over_g = eepromdata;

	addr = 0x12;
	rc = imx074_i2c_read_w_eeprom(addr, &eepromdata);
	if (rc < 0) {
		CDBG("%s: Error Reading EEPROM @ 0x%x\n", __func__, addr);
		return rc;
	}
	cfg->cfg.calib_info.b_over_g = eepromdata;

	addr = 0x14;
	rc = imx074_i2c_read_w_eeprom(addr, &eepromdata);
	if (rc < 0) {
		CDBG("%s: Error Reading EEPROM @ 0x%x\n", __func__, addr);
		return rc;
	}
	cfg->cfg.calib_info.gr_over_gb = eepromdata;

	addr = 0x1A;
	rc = imx074_i2c_read_w_eeprom(addr, &eepromdata);
	if (rc < 0) {
		CDBG("%s: Error Reading EEPROM @ 0x%x\n", __func__, addr);
		return rc;
	}
	cfg->cfg.calib_info.macro_2_inf = eepromdata;

	addr = 0x1C;
	rc = imx074_i2c_read_w_eeprom(addr, &eepromdata);
	if (rc < 0) {
		CDBG("%s: Error Reading EEPROM @ 0x%x\n", __func__, addr);
		return rc;
	}
	cfg->cfg.calib_info.inf_2_macro = eepromdata;

	addr = 0x1E;
	rc = imx074_i2c_read_w_eeprom(addr, &eepromdata);
	if (rc < 0) {
		CDBG("%s: Error Reading EEPROM @ 0x%x\n", __func__, addr);
		return rc;
	}
	cfg->cfg.calib_info.stroke_amt = eepromdata;

	addr = 0x20;
	rc = imx074_i2c_read_w_eeprom(addr, &eepromdata);
	if (rc < 0) {
		CDBG("%s: Error Reading EEPROM @ 0x%x\n", __func__, addr);
		return rc;
	}
	cfg->cfg.calib_info.af_pos_1m = eepromdata;

	addr = 0x22;
	rc = imx074_i2c_read_w_eeprom(addr, &eepromdata);
	if (rc < 0) {
		CDBG("%s: Error Reading EEPROM @ 0x%x\n", __func__, addr);
		return rc;
	}
	cfg->cfg.calib_info.af_pos_inf = eepromdata;

	return rc;
}

static int imx074_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	unsigned short chipidl, chipidh;
	CDBG("%s: %d\n", __func__, __LINE__);
	rc = gpio_request(data->sensor_reset, "imx074");
	CDBG(" imx074_probe_init_sensor \n");
	if (!rc) {
		CDBG("sensor_reset = %d\n", rc);
		gpio_direction_output(data->sensor_reset, 0);
		usleep_range(5000, 6000);
		gpio_set_value_cansleep(data->sensor_reset, 1);
		usleep_range(5000, 6000);
	} else {
		CDBG("gpio reset fail");
		goto init_probe_done;
	}
	CDBG("imx074_probe_init_sensor is called\n");
	/* 3. Read sensor Model ID: */
	rc = imx074_i2c_read(0x0000, &chipidh, 1);
	if (rc < 0) {
		CDBG("Model read failed\n");
		goto init_probe_fail;
	}
	rc = imx074_i2c_read(0x0001, &chipidl, 1);
	if (rc < 0) {
		CDBG("Model read failed\n");
		goto init_probe_fail;
	}
	CDBG("imx074 model_id = 0x%x  0x%x\n", chipidh, chipidl);
	/* 4. Compare sensor ID to IMX074 ID: */
	if (chipidh != 0x00 || chipidl != 0x74) {
		rc = -ENODEV;
		CDBG("imx074_probe_init_sensor fail chip id doesnot match\n");
		goto init_probe_fail;
	}
	goto init_probe_done;
init_probe_fail:
	CDBG("imx074_probe_init_sensor fails\n");
	imx074_probe_init_done(data);
init_probe_done:
	CDBG(" imx074_probe_init_sensor finishes\n");
	return rc;
	}
static int32_t imx074_poweron_af(void)
{
	int32_t rc = 0;
	CDBG("imx074 enable AF actuator, gpio = %d\n",
			imx074_ctrl->sensordata->vcm_pwd);
	rc = gpio_request(imx074_ctrl->sensordata->vcm_pwd, "imx074");
	if (!rc) {
		gpio_direction_output(imx074_ctrl->sensordata->vcm_pwd, 1);
		msleep(20);
		rc = imx074_af_init();
		if (rc < 0)
			CDBG("imx074 AF initialisation failed\n");
	} else {
		CDBG("%s: AF PowerON gpio_request failed %d\n", __func__, rc);
	 }
	return rc;
}
static void imx074_poweroff_af(void)
{
	gpio_set_value_cansleep(imx074_ctrl->sensordata->vcm_pwd, 0);
	gpio_free(imx074_ctrl->sensordata->vcm_pwd);
}
/* camsensor_iu060f_imx074_reset */
int imx074_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	CDBG("%s: %d\n", __func__, __LINE__);
	CDBG("Calling imx074_sensor_open_init\n");
	imx074_ctrl = kzalloc(sizeof(struct imx074_ctrl_t), GFP_KERNEL);
	if (!imx074_ctrl) {
		CDBG("imx074_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	imx074_ctrl->fps_divider = 1 * 0x00000400;
	imx074_ctrl->pict_fps_divider = 1 * 0x00000400;
	imx074_ctrl->fps = 30 * Q8;
	imx074_ctrl->set_test = TEST_OFF;
	imx074_ctrl->prev_res = QTR_SIZE;
	imx074_ctrl->pict_res = FULL_SIZE;
	imx074_ctrl->curr_res = INVALID_SIZE;
	config_csi = 0;

	if (data)
		imx074_ctrl->sensordata = data;

	/* enable mclk first */
	msm_camio_clk_rate_set(IMX074_DEFAULT_MASTER_CLK_RATE);
	usleep_range(1000, 2000);
	rc = imx074_probe_init_sensor(data);
	if (rc < 0) {
		CDBG("Calling imx074_sensor_open_init fail\n");
		goto probe_fail;
	}

	rc = imx074_sensor_setting(REG_INIT, RES_PREVIEW);
	if (rc < 0) {
		CDBG("imx074_sensor_setting failed\n");
		goto init_fail;
	}
	if (machine_is_msm8x60_fluid())
		rc = imx074_poweron_af();
	else
		rc = imx074_af_init();
	if (rc < 0) {
		CDBG("AF initialisation failed\n");
		goto init_fail;
	} else
		goto init_done;
probe_fail:
	CDBG(" imx074_sensor_open_init probe fail\n");
	kfree(imx074_ctrl);
	return rc;
init_fail:
	CDBG(" imx074_sensor_open_init fail\n");
	imx074_probe_init_done(data);
	kfree(imx074_ctrl);
init_done:
	CDBG("imx074_sensor_open_init done\n");
	return rc;
}
static int imx074_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&imx074_wait_queue);
	return 0;
}

static const struct i2c_device_id imx074_i2c_id[] = {
	{"imx074", 0},
	{ }
};

static int imx074_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("imx074_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	imx074_sensorw = kzalloc(sizeof(struct imx074_work_t), GFP_KERNEL);
	if (!imx074_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, imx074_sensorw);
	imx074_init_client(client);
	imx074_client = client;


	CDBG("imx074_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	CDBG("imx074_probe failed! rc = %d\n", rc);
	return rc;
}

static int __exit imx074_remove(struct i2c_client *client)
{
	struct imx074_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	imx074_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver imx074_i2c_driver = {
	.id_table = imx074_i2c_id,
	.probe  = imx074_i2c_probe,
	.remove = __exit_p(imx074_i2c_remove),
	.driver = {
		.name = "imx074",
	},
};

int imx074_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&imx074_mut);
	CDBG("imx074_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_GET_PICT_FPS:
		imx074_get_pict_fps(
			cdata.cfg.gfps.prevfps,
			&(cdata.cfg.gfps.pictfps));
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
			break;
	case CFG_GET_PREV_L_PF:
		cdata.cfg.prevl_pf =
			imx074_get_prev_lines_pf();
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
			break;
	case CFG_GET_PREV_P_PL:
		cdata.cfg.prevp_pl =
			imx074_get_prev_pixels_pl();
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
			break;

	case CFG_GET_PICT_L_PF:
		cdata.cfg.pictl_pf =
			imx074_get_pict_lines_pf();
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
			break;
	case CFG_GET_PICT_P_PL:
		cdata.cfg.pictp_pl =
			imx074_get_pict_pixels_pl();
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
			break;
	case CFG_GET_PICT_MAX_EXP_LC:
		cdata.cfg.pict_max_exp_lc =
			imx074_get_pict_max_exp_lc();
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
			break;
	case CFG_SET_FPS:
	case CFG_SET_PICT_FPS:
		rc = imx074_set_fps(&(cdata.cfg.fps));
		break;
	case CFG_SET_EXP_GAIN:
		rc =
			imx074_write_exp_gain(
			cdata.cfg.exp_gain.gain,
			cdata.cfg.exp_gain.line);
			break;
	case CFG_SET_PICT_EXP_GAIN:
		rc =
			imx074_set_pict_exp_gain(
			cdata.cfg.exp_gain.gain,
			cdata.cfg.exp_gain.line);
			break;
	case CFG_SET_MODE:
		rc = imx074_set_sensor_mode(cdata.mode,
			cdata.rs);
			break;
	case CFG_PWR_DOWN:
		rc = imx074_power_down();
			break;
	case CFG_GET_CALIB_DATA:
		rc = imx074_read_eeprom_data(&cdata);
		if (rc < 0)
			break;
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(cdata)))
			rc = -EFAULT;
		break;
	case CFG_MOVE_FOCUS:
		rc =
			imx074_move_focus(
			cdata.cfg.focus.dir,
			cdata.cfg.focus.steps);
			break;
	case CFG_SET_DEFAULT_FOCUS:
		rc =
			imx074_set_default_focus(
			cdata.cfg.focus.steps);
			break;
	case CFG_GET_AF_MAX_STEPS:
		cdata.max_steps = IMX074_STEPS_NEAR_TO_CLOSEST_INF;
		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
			break;
	case CFG_SET_EFFECT:
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(&imx074_mut);

	return rc;
}
static int imx074_sensor_release(void)
{
	int rc = -EBADF;
	mutex_lock(&imx074_mut);
	if (machine_is_msm8x60_fluid())
		imx074_poweroff_af();
	imx074_power_down();
	gpio_set_value_cansleep(imx074_ctrl->sensordata->sensor_reset, 0);
	msleep(5);
	gpio_direction_input(imx074_ctrl->sensordata->sensor_reset);
	gpio_free(imx074_ctrl->sensordata->sensor_reset);
	kfree(imx074_ctrl);
	imx074_ctrl = NULL;
	CDBG("imx074_release completed\n");
	mutex_unlock(&imx074_mut);

	return rc;
}

static int imx074_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = i2c_add_driver(&imx074_i2c_driver);
	if (rc < 0 || imx074_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_fail;
	}
	msm_camio_clk_rate_set(IMX074_DEFAULT_MASTER_CLK_RATE);
	rc = imx074_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;
	s->s_init = imx074_sensor_open_init;
	s->s_release = imx074_sensor_release;
	s->s_config  = imx074_sensor_config;
	s->s_mount_angle = info->sensor_platform_info->mount_angle;
	imx074_probe_init_done(info);
	return rc;

probe_fail:
	CDBG("imx074_sensor_probe: SENSOR PROBE FAILS!\n");
	i2c_del_driver(&imx074_i2c_driver);
	return rc;
}

static int __imx074_probe(struct platform_device *pdev)
{

	return msm_camera_drv_start(pdev, imx074_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __imx074_probe,
	.driver = {
		.name = "msm_camera_imx074",
		.owner = THIS_MODULE,
	},
};

static int __init imx074_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(imx074_init);

MODULE_DESCRIPTION("Sony 13 MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");

