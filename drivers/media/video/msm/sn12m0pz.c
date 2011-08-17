/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include <linux/slab.h>
#include "sn12m0pz.h"


#define	Q8					0x00000100
#define	REG_GROUPED_PARAMETER_HOLD		0x0104
#define	GROUPED_PARAMETER_HOLD_OFF		0x00
#define	GROUPED_PARAMETER_HOLD			0x01
#define	REG_MODE_SELECT				0x0100
#define	MODE_SELECT_STANDBY_MODE		0x00
#define	MODE_SELECT_STREAM			0x01

/* Integration Time */
#define	REG_COARSE_INTEGRATION_TIME_MSB		0x0202
#define	REG_COARSE_INTEGRATION_TIME_LSB		0x0203

/* Gain */
#define	REG_ANALOGUE_GAIN_CODE_GLOBAL_MSB	0x0204
#define	REG_ANALOGUE_GAIN_CODE_GLOBAL_LSB	0x0205

/* PLL Register Defines */
#define	REG_PLL_MULTIPLIER			0x0307
#define	REG_0x302B				0x302B

/* MIPI Enable Settings */
#define	REG_0x30E5				0x30E5
#define	REG_0x3300				0x3300

/* Global Setting */
#define	REG_IMAGE_ORIENTATION			0x0101

#define	REG_0x300A				0x300A
#define	REG_0x3014				0x3014
#define	REG_0x3015				0x3015
#define	REG_0x3017				0x3017
#define	REG_0x301C				0x301C
#define	REG_0x3031				0x3031
#define	REG_0x3040				0x3040
#define	REG_0x3041				0x3041
#define	REG_0x3051				0x3051
#define	REG_0x3053				0x3053
#define	REG_0x3055				0x3055
#define	REG_0x3057				0x3057
#define	REG_0x3060				0x3060
#define	REG_0x3065				0x3065
#define	REG_0x30AA				0x30AA
#define	REG_0x30AB				0x30AB
#define	REG_0x30B0				0x30B0
#define	REG_0x30B2				0x30B2
#define	REG_0x30D3				0x30D3

#define	REG_0x3106				0x3106
#define	REG_0x3108				0x3108
#define	REG_0x310A				0x310A
#define	REG_0x310C				0x310C
#define	REG_0x310E				0x310E
#define	REG_0x3126				0x3126
#define	REG_0x312E				0x312E
#define	REG_0x313C				0x313C
#define	REG_0x313E				0x313E
#define	REG_0x3140				0x3140
#define	REG_0x3142				0x3142
#define	REG_0x3144				0x3144
#define	REG_0x3148				0x3148
#define	REG_0x314A				0x314A
#define	REG_0x3166				0x3166
#define	REG_0x3168				0x3168
#define	REG_0x316F				0x316F
#define	REG_0x3171				0x3171
#define	REG_0x3173				0x3173
#define	REG_0x3175				0x3175
#define	REG_0x3177				0x3177
#define	REG_0x3179				0x3179
#define	REG_0x317B				0x317B
#define	REG_0x317D				0x317D
#define	REG_0x317F			0x317F
#define	REG_0x3181			0x3181
#define	REG_0x3184			0x3184
#define	REG_0x3185			0x3185
#define	REG_0x3187			0x3187

#define	REG_0x31A4			0x31A4
#define	REG_0x31A6			0x31A6
#define	REG_0x31AC			0x31AC
#define	REG_0x31AE			0x31AE
#define	REG_0x31B4			0x31B4
#define	REG_0x31B6			0x31B6

#define	REG_0x3254			0x3254
#define	REG_0x3256			0x3256
#define	REG_0x3258			0x3258
#define	REG_0x325A			0x325A
#define	REG_0x3260			0x3260
#define	REG_0x3262			0x3262


#define	REG_0x3304			0x3304
#define	REG_0x3305			0x3305
#define	REG_0x3306			0x3306
#define	REG_0x3307			0x3307
#define	REG_0x3308			0x3308
#define	REG_0x3309			0x3309
#define	REG_0x330A			0x330A
#define	REG_0x330B			0x330B
#define	REG_0x330C			0x330C
#define	REG_0x330D			0x330D

/* Mode Setting */
#define	REG_FRAME_LENGTH_LINES_MSB	0x0340
#define	REG_FRAME_LENGTH_LINES_LSB	0x0341
#define	REG_LINE_LENGTH_PCK_MSB		0x0342
#define	REG_LINE_LENGTH_PCK_LSB		0x0343
#define	REG_X_OUTPUT_SIZE_MSB		0x034C
#define	REG_X_OUTPUT_SIZE_LSB		0x034D
#define	REG_Y_OUTPUT_SIZE_MSB		0x034E
#define	REG_Y_OUTPUT_SIZE_LSB		0x034F
#define	REG_X_EVEN_INC_LSB		0x0381
#define	REG_X_ODD_INC_LSB		0x0383
#define	REG_Y_EVEN_INC_LSB		0x0385
#define	REG_Y_ODD_INC_LSB		0x0387
#define	REG_0x3016			0x3016
#define	REG_0x30E8			0x30E8
#define	REG_0x3301			0x3301
/* for 120fps support */
#define	REG_0x0344			0x0344
#define	REG_0x0345			0x0345
#define	REG_0x0346			0x0346
#define	REG_0x0347			0x0347
#define	REG_0x0348			0x0348
#define	REG_0x0349			0x0349
#define	REG_0x034A			0x034A
#define	REG_0x034B			0x034B

/* Test Pattern */
#define	REG_0x30D8			0x30D8
#define	REG_TEST_PATTERN_MODE		0x0601

/* Solid Color Test Pattern */
#define	REG_TEST_DATA_RED_MSB		0x0603
#define	REG_TEST_DATA_RED_LSB		0x0603
#define	REG_TEST_DATA_GREENR_MSB	0x0604
#define	REG_TEST_DATA_GREENR_LSB	0x0605
#define	REG_TEST_DATA_BLUE_MSB		0x0606
#define	REG_TEST_DATA_BLUE_LSB		0x0607
#define	REG_TEST_DATA_GREENB_MSB	0x0608
#define	REG_TEST_DATA_GREENB_LSB	0x0609
#define	SN12M0PZ_AF_I2C_SLAVE_ID	0xE4
#define	SN12M0PZ_STEPS_NEAR_TO_CLOSEST_INF	42
#define	SN12M0PZ_TOTAL_STEPS_NEAR_TO_FAR	42


/* TYPE DECLARATIONS */


enum mipi_config_type {
	IU060F_SN12M0PZ_STMIPID01,
	IU060F_SN12M0PZ_STMIPID02
};

enum sn12m0pz_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum sn12m0pz_resolution_t {
	QTR_SIZE,
	FULL_SIZE,
	INVALID_SIZE,
	QVGA_SIZE,
};

enum sn12m0pz_setting {
	RES_PREVIEW,
	RES_CAPTURE,
	RES_VIDEO_120FPS,
};

enum mt9p012_reg_update {
	/* Sensor egisters that need to be updated during initialization */
	REG_INIT,
	/* Sensor egisters that needs periodic I2C writes */
	UPDATE_PERIODIC,
	/* All the sensor Registers will be updated */
	UPDATE_ALL,
	/* Not valid update */
	UPDATE_INVALID
};

/* 816x612, 24MHz MCLK 96MHz PCLK */
#define	IU060F_SN12M0PZ_OFFSET			3
/* Time in milisecs for waiting for the sensor to reset.*/
#define	SN12M0PZ_RESET_DELAY_MSECS		66
#define	SN12M0PZ_WIDTH				4032
#define	SN12M0PZ_HEIGHT				3024
#define	SN12M0PZ_FULL_SIZE_WIDTH		4032
#define	SN12M0PZ_FULL_SIZE_HEIGHT		3024
#define	SN12M0PZ_HRZ_FULL_BLK_PIXELS		176
#define	SN12M0PZ_VER_FULL_BLK_LINES		50
#define	SN12M0PZ_QTR_SIZE_WIDTH			2016
#define	SN12M0PZ_QTR_SIZE_HEIGHT		1512
#define	SN12M0PZ_HRZ_QTR_BLK_PIXELS		2192
#define	SN12M0PZ_VER_QTR_BLK_LINES		26

/* 120fps mode */
#define	SN12M0PZ_QVGA_SIZE_WIDTH		4032
#define	SN12M0PZ_QVGA_SIZE_HEIGHT		249
#define	SN12M0PZ_HRZ_QVGA_BLK_PIXELS		176
#define	SN12M0PZ_VER_QVGA_BLK_LINES		9
#define	SN12M0PZ_DEFAULT_CLOCK_RATE		24000000

static uint32_t IU060F_SN12M0PZ_DELAY_MSECS = 30;
static enum mipi_config_type mipi_config = IU060F_SN12M0PZ_STMIPID02;
/* AF Tuning Parameters */
static int16_t enable_single_D02_lane;
static int16_t fullsize_cropped_at_8mp;

struct sn12m0pz_work_t {
	struct work_struct work;
};

static struct sn12m0pz_work_t *sn12m0pz_sensorw;
static struct i2c_client *sn12m0pz_client;

struct sn12m0pz_ctrl_t {
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
	enum sn12m0pz_resolution_t prev_res;
	enum sn12m0pz_resolution_t pict_res;
	enum sn12m0pz_resolution_t curr_res;
	enum sn12m0pz_test_mode_t  set_test;
	unsigned short imgaddr;
};

static struct sn12m0pz_ctrl_t *sn12m0pz_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(sn12m0pz_wait_queue);
DEFINE_MUTEX(sn12m0pz_mut);


static int sn12m0pz_i2c_rxdata(unsigned short saddr,
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

	if (i2c_transfer(sn12m0pz_client->adapter, msgs, 2) < 0) {
		CDBG("sn12m0pz_i2c_rxdata failed!");
		return -EIO;
	}

	return 0;
}
static int32_t sn12m0pz_i2c_txdata(unsigned short saddr,
				unsigned char *txdata, int length)
{

	struct i2c_msg msg[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len	 = length,
			.buf	 = txdata,
		},
	};

	if (i2c_transfer(sn12m0pz_client->adapter, msg, 1) < 0) {
		CDBG("sn12m0pz_i2c_txdata faild 0x%x", sn12m0pz_client->addr);
		return -EIO;
	}

	return 0;
}

static int32_t sn12m0pz_i2c_read(unsigned short raddr,
				unsigned short *rdata, int rlen)
{
	int32_t rc;
	unsigned char buf[2];
	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00) >> 8;
	buf[1] = (raddr & 0x00FF);

	rc = sn12m0pz_i2c_rxdata(sn12m0pz_client->addr, buf, rlen);

	if (rc < 0) {
		CDBG("sn12m0pz_i2c_read 0x%x failed!", raddr);
		return rc;
	}

	*rdata = (rlen == 2 ? buf[0] << 8 | buf[1] : buf[0]);

	return rc;
}

static int32_t sn12m0pz_i2c_write_b_sensor(unsigned short waddr, uint8_t bdata)
{
	int32_t rc;
	unsigned char buf[3];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;
	udelay(90);
	CDBG("i2c_write_b addr = %x, val = %x\n", waddr, bdata);
	rc = sn12m0pz_i2c_txdata(sn12m0pz_client->addr, buf, 3);

	if (rc < 0) {
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!",
			waddr, bdata);
	}

	return rc;
}

static int16_t sn12m0pz_i2c_write_b_af(unsigned short saddr,
				unsigned short baddr, unsigned short bdata)
{
	int16_t rc;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));
	buf[0] = baddr;
	buf[1] = bdata;
	rc = sn12m0pz_i2c_txdata(saddr, buf, 2);

	if (rc < 0)
		CDBG("i2c_write failed, saddr = 0x%x addr = 0x%x, val =0x%x!",
			saddr, baddr, bdata);

	return rc;
}

static int32_t sn12m0pz_i2c_write_byte_bridge(unsigned short saddr,
				unsigned short waddr, uint8_t bdata)
{
	int32_t rc;
	unsigned char buf[3];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;

	CDBG("i2c_write_b addr = %x, val = %x", waddr, bdata);
	rc = sn12m0pz_i2c_txdata(saddr, buf, 3);

	if (rc < 0)
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!",
			waddr, bdata);

	return rc;
}

static int32_t sn12m0pz_stmipid01_config(void)
{
	int32_t rc = 0;
	/* Initiate I2C for D01: */
	/* MIPI Bridge configuration */
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0002, 0x19) < 0)
		return rc; /* enable clock lane*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0003, 0x00) < 0)
		return rc;
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0004, 0x3E) < 0)
		return rc; /* mipi mode clock*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0005, 0x01) < 0)
		return rc; /* enable data line*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0006, 0x0F) < 0)
		return rc; /* mipi mode data 0x01*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0009, 0x00) < 0)
		return rc; /* Data_Lane1_Reg1*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x000D, 0x92) < 0)
		return rc; /* CCPRxRegisters*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x000E, 0x28) < 0)
		return rc; /* 10 bits for pixel width input for CCP rx.*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0014, 0xC0) < 0)
		return rc; /* no bypass, no decomp, 1Lane System,CSIstreaming*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0015, 0x48) < 0)
		return rc; /* ModeControlRegisters-- Don't reset error flag*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0017, 0x2B) < 0)
		return rc; /* Data_ID_Rreg*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0018, 0x2B) < 0)
		return rc; /* Data_ID_Rreg_emb*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0019, 0x0C) < 0)
		return rc;
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x001E, 0x0A) < 0)
		return rc;
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x001F, 0x0A) < 0)
		return rc;

	return rc;
}
static int32_t sn12m0pz_stmipid02_config(void)
{
	int32_t rc = 0;

	/* Main Camera Clock Lane 1 (CLHP1, CLKN1)*/
	/* Enable Clock Lane 1 (CLHP1, CLKN1), 0x15 for 400MHz */
	if (enable_single_D02_lane) {
		if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0002, 0x19) < 0)
			return rc;
		/* Main Camera Data Lane 1.1 (DATA2P1, DATA2N1) */
		if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0009, 0x00) < 0)
			return rc;/* Enable Data Lane 1.2 (DATA2P1, DATA2N1) */
		if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x000A, 0x00) < 0)
			return rc; /*CSIMode on Data Lane1.2(DATA2P1,DATA2N1)*/
		/* Mode Control */
		/* Enable single lane for qtr preview */
		if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0014, 0xC0) < 0)
			return rc; /*set 0xC0 - left justified on upper bits)*/
		/* bit 1 set to 0 i.e. 1 lane system for qtr size preview */
	} else {
		if (sn12m0pz_ctrl->prev_res == QVGA_SIZE) {
			if (sn12m0pz_i2c_write_byte_bridge(0x28>>1,
				0x0002, 0x19) < 0)
				return rc;
		} else {
			if (sn12m0pz_i2c_write_byte_bridge(0x28>>1,
				0x0002, 0x21) < 0)
				return rc;
		}
		/* Main Camera Data Lane 1.1 (DATA2P1, DATA2N1) */
		if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0009, 0x01) < 0)
			return rc; /* Enable Data Lane 1.2 (DATA2P1, DATA2N1) */
		if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x000A, 0x01) < 0)
			return rc; /* CSI Mode Data Lane1.2(DATA2P1, DATA2N1)*/

		/* Mode Control */
		/* Enable two lanes for full size preview/ snapshot */
		if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0014, 0xC2) < 0)
			return rc; /* No decompression, CSI dual lane */
	}

	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0004, 0x1E) < 0)
		return rc;

	/* Main Camera Data Lane 1.1 (DATA1P1, DATA1N1) */
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0005, 0x03) < 0)
		return rc; /* Enable Data Lane 1.1 (DATA1P1, DATA1N1) */
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0006, 0x0f) < 0)
		return rc; /* CSI Mode on Data Lane 1.1 (DATA1P1, DATA1N1) */

	/* Tristated Output, continuous clock, */
	/*polarity of clock is inverted and sync signals not inverted*/
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0015, 0x08) < 0)
		return rc;
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0036, 0x20) < 0)
		return rc; /* Enable compensation macro, main camera */

	/* Data type: 0x2B Raw 10 */
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0017, 0x2B) < 0)
		return rc;
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0018, 0x2B) < 0)
		return rc; /* Data type of embedded data: 0x2B Raw 10 */
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x0019, 0x0C) < 0)
		return rc; /* Data type and pixel width programmed 0x0C*/

	/* Decompression Mode */

	/* Pixel Width and Decompression ON/OFF */
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x001E, 0x0A) < 0)
		return rc; /* Image data not compressed: 0x0A for 10 bits */
	if (sn12m0pz_i2c_write_byte_bridge(0x28>>1, 0x001F, 0x0A) < 0)
		return rc; /* Embedded data not compressed: 0x0A for 10 bits */
	return rc;
}

static int16_t sn12m0pz_af_init(void)
{
	int16_t rc;
	/* Initialize waveform */
	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x01, 0xA9);

	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x02, 0xD2);

	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x03, 0x0C);

	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x04, 0x14);

	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x05, 0xB6);

	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x06, 0x4F);

	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x07, 0x00);

	return rc;
}

static int32_t sn12m0pz_move_focus(int direction,
	int32_t num_steps)
{
	int8_t step_direction, dest_step_position, bit_mask;
	int32_t rc = 0;
	uint16_t sn12m0pz_l_region_code_per_step = 3;

	if (num_steps == 0)
		return rc;

	if (direction == MOVE_NEAR) {
		step_direction = 1;
		bit_mask = 0x80;
	} else if (direction == MOVE_FAR) {
		step_direction = -1;
		bit_mask = 0x00;
	} else {
		CDBG("sn12m0pz_move_focus: Illegal focus direction");
		return -EINVAL;
	}

	dest_step_position = sn12m0pz_ctrl->curr_step_pos +
		(step_direction * num_steps);

	if (dest_step_position < 0)
		dest_step_position = 0;
	else if (dest_step_position > SN12M0PZ_TOTAL_STEPS_NEAR_TO_FAR)
		dest_step_position = SN12M0PZ_TOTAL_STEPS_NEAR_TO_FAR;

	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x00,
		((num_steps * sn12m0pz_l_region_code_per_step) | bit_mask));

	sn12m0pz_ctrl->curr_step_pos = dest_step_position;

	return rc;
}
static int32_t sn12m0pz_set_default_focus(uint8_t af_step)
{
	int32_t rc;

	/* Initialize to infinity */

	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x00, 0x7F);

	rc = sn12m0pz_i2c_write_b_af(SN12M0PZ_AF_I2C_SLAVE_ID >> 1, 0x00, 0x7F);

	sn12m0pz_ctrl->curr_step_pos = 0;

	return rc;
}
static void sn12m0pz_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint16_t preview_frame_length_lines, snapshot_frame_length_lines;
	uint16_t preview_line_length_pck, snapshot_line_length_pck;
	uint32_t divider, pclk_mult, d1, d2;

	/* Total frame_length_lines and line_length_pck for preview */
	CDBG("sn12m0pz_get_pict_fps prev_res %d", sn12m0pz_ctrl->prev_res);
	if (sn12m0pz_ctrl->prev_res == QVGA_SIZE) {
		preview_frame_length_lines = SN12M0PZ_QVGA_SIZE_HEIGHT +
			SN12M0PZ_VER_QVGA_BLK_LINES;
		preview_line_length_pck = SN12M0PZ_QVGA_SIZE_WIDTH +
			SN12M0PZ_HRZ_QVGA_BLK_PIXELS;
	} else {
		preview_frame_length_lines = SN12M0PZ_QTR_SIZE_HEIGHT +
			SN12M0PZ_VER_QTR_BLK_LINES;
		preview_line_length_pck = SN12M0PZ_QTR_SIZE_WIDTH +
			SN12M0PZ_HRZ_QTR_BLK_PIXELS;
	}
	/* Total frame_length_lines and line_length_pck for snapshot */
	snapshot_frame_length_lines = SN12M0PZ_FULL_SIZE_HEIGHT
				+ SN12M0PZ_HRZ_FULL_BLK_PIXELS;
	snapshot_line_length_pck = SN12M0PZ_FULL_SIZE_WIDTH
				+ SN12M0PZ_HRZ_FULL_BLK_PIXELS;
	d1 = preview_frame_length_lines *
				0x00000400 / snapshot_frame_length_lines;
	d2 = preview_line_length_pck *
				0x00000400/snapshot_line_length_pck;
	divider = d1 * d2 / 0x400;
	pclk_mult =
		(uint32_t)
		(sn12m0pz_regs.reg_pat[RES_CAPTURE].pll_multiplier_lsb *
		0x400) / (uint32_t)
		sn12m0pz_regs.reg_pat[RES_PREVIEW].pll_multiplier_lsb;
	*pfps = (uint16_t) (((fps * divider) / 0x400 * pclk_mult) / 0x400);
}

static uint16_t sn12m0pz_get_prev_lines_pf(void)
{
	if (sn12m0pz_ctrl->prev_res == QTR_SIZE)
		return SN12M0PZ_QTR_SIZE_HEIGHT +
			SN12M0PZ_VER_QTR_BLK_LINES;
	else if (sn12m0pz_ctrl->prev_res == QVGA_SIZE)
		return SN12M0PZ_QVGA_SIZE_HEIGHT +
			SN12M0PZ_VER_QVGA_BLK_LINES;

	else
		return SN12M0PZ_FULL_SIZE_HEIGHT +
			SN12M0PZ_VER_FULL_BLK_LINES;
}

static uint16_t sn12m0pz_get_prev_pixels_pl(void)
{
	if (sn12m0pz_ctrl->prev_res == QTR_SIZE)
		return SN12M0PZ_QTR_SIZE_WIDTH +
			SN12M0PZ_HRZ_QTR_BLK_PIXELS;
	else
		return SN12M0PZ_FULL_SIZE_WIDTH +
			SN12M0PZ_HRZ_FULL_BLK_PIXELS;
}

static uint16_t sn12m0pz_get_pict_lines_pf(void)
{
	if (sn12m0pz_ctrl->pict_res == QTR_SIZE)
		return SN12M0PZ_QTR_SIZE_HEIGHT +
			SN12M0PZ_VER_QTR_BLK_LINES;
	else
		return SN12M0PZ_FULL_SIZE_HEIGHT +
			SN12M0PZ_VER_FULL_BLK_LINES;
}

static uint16_t sn12m0pz_get_pict_pixels_pl(void)
{
	if (sn12m0pz_ctrl->pict_res == QTR_SIZE)
		return SN12M0PZ_QTR_SIZE_WIDTH +
			SN12M0PZ_HRZ_QTR_BLK_PIXELS;
	else
		return SN12M0PZ_FULL_SIZE_WIDTH +
			SN12M0PZ_HRZ_FULL_BLK_PIXELS;
}

static uint32_t sn12m0pz_get_pict_max_exp_lc(void)
{
	if (sn12m0pz_ctrl->pict_res == QTR_SIZE)
		return (SN12M0PZ_QTR_SIZE_HEIGHT +
			SN12M0PZ_VER_QTR_BLK_LINES) * 24;
	else
		return (SN12M0PZ_FULL_SIZE_HEIGHT +
			SN12M0PZ_VER_FULL_BLK_LINES) * 24;
}

static int32_t sn12m0pz_set_fps(struct fps_cfg	*fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;

	total_lines_per_frame = (uint16_t)((SN12M0PZ_QTR_SIZE_HEIGHT +
				SN12M0PZ_VER_QTR_BLK_LINES) *
				sn12m0pz_ctrl->fps_divider / 0x400);

	if (sn12m0pz_i2c_write_b_sensor(REG_FRAME_LENGTH_LINES_MSB,
				((total_lines_per_frame & 0xFF00) >> 8)) < 0)
		return rc;

	if (sn12m0pz_i2c_write_b_sensor(REG_FRAME_LENGTH_LINES_LSB,
				(total_lines_per_frame & 0x00FF)) < 0)
		return rc;

	return rc;
}

static int32_t sn12m0pz_write_exp_gain(uint16_t gain, uint32_t line)
{
	static uint16_t max_legal_gain = 0x00E0;
	uint8_t gain_msb, gain_lsb;
	uint8_t intg_time_msb, intg_time_lsb;
	uint8_t line_length_pck_msb, line_length_pck_lsb;
	uint16_t line_length_pck, frame_length_lines, temp_lines;
	uint32_t line_length_ratio = 1 * Q8;
	int32_t rc = 0;
	CDBG("sn12m0pz_write_exp_gain : gain = %d line = %d", gain, line);

	if (sn12m0pz_ctrl->sensormode != SENSOR_SNAPSHOT_MODE) {
		if (sn12m0pz_ctrl->prev_res == QVGA_SIZE) {
			frame_length_lines = SN12M0PZ_QVGA_SIZE_HEIGHT +
						SN12M0PZ_VER_QVGA_BLK_LINES;
			line_length_pck = SN12M0PZ_QVGA_SIZE_WIDTH +
						SN12M0PZ_HRZ_QVGA_BLK_PIXELS;
			if (line > (frame_length_lines -
					IU060F_SN12M0PZ_OFFSET))
				line = frame_length_lines -
						IU060F_SN12M0PZ_OFFSET;
			sn12m0pz_ctrl->fps = (uint16_t) (120 * Q8);
		} else {
			if (sn12m0pz_ctrl->curr_res  == QTR_SIZE) {
				frame_length_lines = SN12M0PZ_QTR_SIZE_HEIGHT +
						SN12M0PZ_VER_QTR_BLK_LINES;
				line_length_pck = SN12M0PZ_QTR_SIZE_WIDTH +
						SN12M0PZ_HRZ_QTR_BLK_PIXELS;
			} else {
				frame_length_lines = SN12M0PZ_HEIGHT +
						SN12M0PZ_VER_FULL_BLK_LINES;
				line_length_pck = SN12M0PZ_WIDTH +
						SN12M0PZ_HRZ_FULL_BLK_PIXELS;
			}
			if (line > (frame_length_lines -
						IU060F_SN12M0PZ_OFFSET))
				sn12m0pz_ctrl->fps = (uint16_t) (30 * Q8 *
			(frame_length_lines - IU060F_SN12M0PZ_OFFSET) / line);
			else
				sn12m0pz_ctrl->fps = (uint16_t) (30 * Q8);
		}
	} else {
		if (sn12m0pz_ctrl->curr_res  == QTR_SIZE) {
			frame_length_lines = SN12M0PZ_QTR_SIZE_HEIGHT +
						SN12M0PZ_VER_QTR_BLK_LINES;
			line_length_pck = SN12M0PZ_QTR_SIZE_WIDTH +
						SN12M0PZ_HRZ_QTR_BLK_PIXELS;
		} else {
			frame_length_lines = SN12M0PZ_HEIGHT +
						SN12M0PZ_VER_FULL_BLK_LINES;
			line_length_pck = SN12M0PZ_WIDTH +
						SN12M0PZ_HRZ_FULL_BLK_PIXELS;
		}
	}
	if (gain > max_legal_gain)
		/* range: 0 to 224 */
		gain = max_legal_gain;
	temp_lines = line;
	/* calculate line_length_ratio */
	if (line > (frame_length_lines - IU060F_SN12M0PZ_OFFSET)) {
		line_length_ratio = (line * Q8) / (frame_length_lines -
					IU060F_SN12M0PZ_OFFSET);
		temp_lines = frame_length_lines - IU060F_SN12M0PZ_OFFSET;
		if (line_length_ratio == 0)
			line_length_ratio = 1 * Q8;
	} else
		line_length_ratio = 1 * Q8;

	line = (uint32_t) (line * sn12m0pz_ctrl->fps_divider/0x400);

	/* update gain registers */
	gain_msb = (uint8_t) ((gain & 0xFF00) >> 8);
	gain_lsb = (uint8_t) (gain & 0x00FF);

	/* linear AFR horizontal stretch */
	line_length_pck = (uint16_t) (line_length_pck * line_length_ratio / Q8);
	line_length_pck_msb = (uint8_t) ((line_length_pck & 0xFF00) >> 8);
	line_length_pck_lsb = (uint8_t) (line_length_pck & 0x00FF);

	/* update line count registers */
	intg_time_msb = (uint8_t) ((temp_lines & 0xFF00) >> 8);
	intg_time_lsb = (uint8_t) (temp_lines & 0x00FF);


	if (sn12m0pz_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_HOLD) < 0)
		return rc;

	if (sn12m0pz_i2c_write_b_sensor(REG_ANALOGUE_GAIN_CODE_GLOBAL_MSB,
			gain_msb) < 0)
		return rc;

	if (sn12m0pz_i2c_write_b_sensor(REG_ANALOGUE_GAIN_CODE_GLOBAL_LSB,
			gain_lsb) < 0)
		return rc;

	if (sn12m0pz_i2c_write_b_sensor(REG_LINE_LENGTH_PCK_MSB,
			line_length_pck_msb) < 0)
		return rc;

	if (sn12m0pz_i2c_write_b_sensor(REG_LINE_LENGTH_PCK_LSB,
			line_length_pck_lsb) < 0)
		return rc;

	if (sn12m0pz_i2c_write_b_sensor(REG_COARSE_INTEGRATION_TIME_MSB,
			intg_time_msb) < 0)
		return rc;

	if (sn12m0pz_i2c_write_b_sensor(REG_COARSE_INTEGRATION_TIME_LSB,
			intg_time_lsb) < 0)
		return rc;

	if (sn12m0pz_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_HOLD_OFF) < 0)
		return rc;

	return rc;
}


static int32_t sn12m0pz_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc;
	rc = sn12m0pz_write_exp_gain(gain, line);
	return rc;
}

static int32_t sn12m0pz_test(enum sn12m0pz_test_mode_t mo)
{
	uint8_t test_data_val_msb = 0x07;
	uint8_t test_data_val_lsb = 0xFF;
	int32_t rc = 0;
	if (mo == TEST_OFF)
		return rc;
	else {
		/* REG_0x30D8[4] is TESBYPEN: 0: Normal Operation,
		 1: Bypass Signal Processing. REG_0x30D8[5] is EBDMASK:
		 0: Output Embedded data, 1: No output embedded data */

		if (sn12m0pz_i2c_write_b_sensor(REG_0x30D8, 0x10) < 0)
			return rc;

		if (sn12m0pz_i2c_write_b_sensor(REG_TEST_PATTERN_MODE,
			(uint8_t) mo) < 0)
			return rc;

		/* Solid Color Test Pattern */

		if (mo == TEST_1) {
			if (sn12m0pz_i2c_write_b_sensor(REG_TEST_DATA_RED_MSB,
				test_data_val_msb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_TEST_DATA_RED_LSB,
				test_data_val_lsb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(
						REG_TEST_DATA_GREENR_MSB,
						test_data_val_msb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(
						REG_TEST_DATA_GREENR_LSB,
						test_data_val_lsb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_TEST_DATA_BLUE_MSB,
				test_data_val_msb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_TEST_DATA_BLUE_LSB,
				test_data_val_lsb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(
						REG_TEST_DATA_GREENB_MSB,
						test_data_val_msb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(
						REG_TEST_DATA_GREENB_LSB,
						test_data_val_lsb) < 0)
				return rc;
		}

	}

	return rc;
}

static int32_t sn12m0pz_reset(void)
{
	int32_t rc = 0;
	/* register 0x0002 is Port 2, CAM_XCLRO */
	gpio_direction_output(sn12m0pz_ctrl->
		sensordata->sensor_reset,
		0);
	msleep(50);
	gpio_direction_output(sn12m0pz_ctrl->
		sensordata->sensor_reset,
		1);
	msleep(13);
	return rc;
}

static int32_t sn12m0pz_sensor_setting(int update_type, int rt)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;

	switch (update_type) {
	case UPDATE_PERIODIC:
		/* Put Sensor into sofware standby mode	*/
		if (sn12m0pz_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STANDBY_MODE) <  0)
			return rc;
		msleep(5);
		/* Hardware reset D02, lane config between full size/qtr size*/
		rc = sn12m0pz_reset();
		if (rc < 0)
			return rc;

		if (sn12m0pz_stmipid02_config() < 0)
			return rc;
	case REG_INIT:
		if (rt == RES_PREVIEW || rt == RES_CAPTURE
				|| rt == RES_VIDEO_120FPS) {
			/* reset fps_divider */
			sn12m0pz_ctrl->fps_divider = 1 * 0x400;

			/* PLL settings */
			if (sn12m0pz_i2c_write_b_sensor(REG_PLL_MULTIPLIER,
			sn12m0pz_regs.reg_pat[rt].pll_multiplier_lsb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x302B,
				sn12m0pz_regs.reg_pat_init[0].reg_0x302B) < 0)
				return rc;

			/* MIPI Enable Settings */
			if (sn12m0pz_i2c_write_b_sensor(REG_0x30E5,
				sn12m0pz_regs.reg_pat_init[0].reg_0x30E5) < 0)
				return rc;

			if (sn12m0pz_i2c_write_b_sensor(REG_0x3300,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3300) < 0)
				return rc;

			/* Global Setting */
			if (
				sn12m0pz_i2c_write_b_sensor(
				REG_IMAGE_ORIENTATION,
				sn12m0pz_regs.reg_pat_init[0].image_orient) < 0)
				return rc;
			if (
				sn12m0pz_i2c_write_b_sensor(
				REG_COARSE_INTEGRATION_TIME_MSB,
				sn12m0pz_regs.reg_pat[rt].coarse_integ_time_msb)
				< 0)
				return rc;
			if (
				sn12m0pz_i2c_write_b_sensor(
				REG_COARSE_INTEGRATION_TIME_LSB,
				sn12m0pz_regs.reg_pat[rt].coarse_integ_time_lsb)
				 < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x300A,
				sn12m0pz_regs.reg_pat_init[0].reg_0x300A) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3014,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3014) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3015,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3015) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3017,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3017) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x301C,
				sn12m0pz_regs.reg_pat_init[0].reg_0x301C) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3031,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3031) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3040,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3040) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3041,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3041) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3051,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3051) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3053,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3053) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3055,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3055) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3057,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3057) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3060,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3060) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3065,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3065) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x30AA,
				sn12m0pz_regs.reg_pat_init[0].reg_0x30AA) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x30AB,
				sn12m0pz_regs.reg_pat_init[0].reg_0x30AB) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x30B0,
				sn12m0pz_regs.reg_pat_init[0].reg_0x30B0) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x30B2,
				sn12m0pz_regs.reg_pat_init[0].reg_0x30B2) < 0)
				return rc;

			if (sn12m0pz_i2c_write_b_sensor(REG_0x30D3,
				sn12m0pz_regs.reg_pat_init[0].reg_0x30D3) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x30D8,
				sn12m0pz_regs.reg_pat_init[0].reg_0x30D8) < 0)
				return rc;

			if (sn12m0pz_i2c_write_b_sensor(REG_0x3106,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3106) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3108,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3108) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x310A,
				sn12m0pz_regs.reg_pat_init[0].reg_0x310A) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x310C,
				sn12m0pz_regs.reg_pat_init[0].reg_0x310C) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x310E,
				sn12m0pz_regs.reg_pat_init[0].reg_0x310E) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3126,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3126) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x312E,
				sn12m0pz_regs.reg_pat_init[0].reg_0x312E) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x313C,
				sn12m0pz_regs.reg_pat_init[0].reg_0x313C) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x313E,
				sn12m0pz_regs.reg_pat_init[0].reg_0x313E) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3140,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3140) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3142,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3142) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3144,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3144) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3148,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3148) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x314A,
				sn12m0pz_regs.reg_pat_init[0].reg_0x314A) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3166,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3166) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3168,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3168) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x316F,
				sn12m0pz_regs.reg_pat_init[0].reg_0x316F) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3171,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3171) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3173,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3173) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3175,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3175) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3177,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3177) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3179,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3179) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x317B,
				sn12m0pz_regs.reg_pat_init[0].reg_0x317B) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x317D,
				sn12m0pz_regs.reg_pat_init[0].reg_0x317D) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x317F,
				sn12m0pz_regs.reg_pat_init[0].reg_0x317F) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3181,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3181) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3184,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3184) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3185,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3185) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3187,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3187) < 0)
				return rc;

			if (sn12m0pz_i2c_write_b_sensor(REG_0x31A4,
				sn12m0pz_regs.reg_pat_init[0].reg_0x31A4) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x31A6,
				sn12m0pz_regs.reg_pat_init[0].reg_0x31A6) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x31AC,
				sn12m0pz_regs.reg_pat_init[0].reg_0x31AC) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x31AE,
				sn12m0pz_regs.reg_pat_init[0].reg_0x31AE) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x31B4,
				sn12m0pz_regs.reg_pat_init[0].reg_0x31B4) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x31B6,
				sn12m0pz_regs.reg_pat_init[0].reg_0x31B6) < 0)
				return rc;

			if (sn12m0pz_i2c_write_b_sensor(REG_0x3254,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3254) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3256,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3256) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3258,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3258) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x325A,
				sn12m0pz_regs.reg_pat_init[0].reg_0x325A) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3260,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3260) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3262,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3262) < 0)
				return rc;


			if (sn12m0pz_i2c_write_b_sensor(REG_0x3304,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3304) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3305,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3305) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3306,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3306) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3307,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3307) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3308,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3308) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3309,
				sn12m0pz_regs.reg_pat_init[0].reg_0x3309) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x330A,
				sn12m0pz_regs.reg_pat_init[0].reg_0x330A) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x330B,
				sn12m0pz_regs.reg_pat_init[0].reg_0x330B) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x330C,
				sn12m0pz_regs.reg_pat_init[0].reg_0x330C) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x330D,
				sn12m0pz_regs.reg_pat_init[0].reg_0x330D) < 0)
				return rc;

			/* Mode setting */
			/* Update registers with correct
				 frame_length_line value for AFR */
			total_lines_per_frame = (uint16_t)(
			(sn12m0pz_regs.reg_pat[rt].frame_length_lines_msb << 8)
			& 0xFF00) +
			sn12m0pz_regs.reg_pat[rt].frame_length_lines_lsb;
			total_lines_per_frame = total_lines_per_frame *
					sn12m0pz_ctrl->fps_divider / 0x400;

			if (sn12m0pz_i2c_write_b_sensor(
					REG_FRAME_LENGTH_LINES_MSB,
					(total_lines_per_frame & 0xFF00) >> 8)
					< 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(
					REG_FRAME_LENGTH_LINES_LSB,
					(total_lines_per_frame & 0x00FF)) < 0)
				return rc;

			if (sn12m0pz_i2c_write_b_sensor(REG_LINE_LENGTH_PCK_MSB,
				sn12m0pz_regs.reg_pat[rt].line_length_pck_msb) <
				0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_LINE_LENGTH_PCK_LSB,
				sn12m0pz_regs.reg_pat[rt].line_length_pck_lsb) <
				0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_X_OUTPUT_SIZE_MSB,
				sn12m0pz_regs.reg_pat[rt].x_output_size_msb) <
				0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_X_OUTPUT_SIZE_LSB,
				sn12m0pz_regs.reg_pat[rt].x_output_size_lsb) <
				0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_Y_OUTPUT_SIZE_MSB,
				sn12m0pz_regs.reg_pat[rt].y_output_size_msb) <
				0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_Y_OUTPUT_SIZE_LSB,
				sn12m0pz_regs.reg_pat[rt].y_output_size_lsb) <
				0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_X_EVEN_INC_LSB,
				sn12m0pz_regs.reg_pat[rt].x_even_inc_lsb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_X_ODD_INC_LSB,
				sn12m0pz_regs.reg_pat[rt].x_odd_inc_lsb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_Y_EVEN_INC_LSB,
				sn12m0pz_regs.reg_pat[rt].y_even_inc_lsb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_Y_ODD_INC_LSB,
				sn12m0pz_regs.reg_pat[rt].y_odd_inc_lsb) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3016,
				sn12m0pz_regs.reg_pat[rt].reg_0x3016) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x30E8,
				sn12m0pz_regs.reg_pat[rt].reg_0x30E8) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x3301,
				sn12m0pz_regs.reg_pat[rt].reg_0x3301) < 0)
				return rc;

			if (sn12m0pz_i2c_write_b_sensor(REG_0x0344,
				sn12m0pz_regs.reg_pat[rt].reg_0x0344) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x0345,
				sn12m0pz_regs.reg_pat[rt].reg_0x0345) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x0346,
				sn12m0pz_regs.reg_pat[rt].reg_0x0346) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x0347,
				sn12m0pz_regs.reg_pat[rt].reg_0x0347) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x0348,
				sn12m0pz_regs.reg_pat[rt].reg_0x0348) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x0349,
				sn12m0pz_regs.reg_pat[rt].reg_0x0349) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x034A,
				sn12m0pz_regs.reg_pat[rt].reg_0x034A) < 0)
				return rc;
			if (sn12m0pz_i2c_write_b_sensor(REG_0x034B,
				sn12m0pz_regs.reg_pat[rt].reg_0x034B) < 0)
				return rc;

			if ((rt == RES_CAPTURE) && fullsize_cropped_at_8mp) {
				/* x address end */
				if (sn12m0pz_i2c_write_b_sensor(0x0348,
								0x0C) < 0)
					return rc;
				if (sn12m0pz_i2c_write_b_sensor(0x0349,
								0x0CF) < 0)
					return rc;
				/* y address end */
				if (sn12m0pz_i2c_write_b_sensor(0x034A,
								0x09) < 0)
					return rc;
				if (sn12m0pz_i2c_write_b_sensor(0x034B,
								0x9F) < 0)
					return rc;
			}

			if (mipi_config == IU060F_SN12M0PZ_STMIPID01) {
				if (sn12m0pz_i2c_write_b_sensor(
						REG_PLL_MULTIPLIER, 0x43) < 0)
					return rc;
				if (rt == RES_CAPTURE) {
					if (sn12m0pz_i2c_write_b_sensor(
						REG_0x3301, 0x01) < 0)
						return rc;
				if (sn12m0pz_i2c_write_b_sensor(
						REG_0x3017, 0xE0) < 0)
					return rc;
				}
			}

			if (sn12m0pz_i2c_write_b_sensor(REG_MODE_SELECT,
						MODE_SELECT_STREAM) < 0)
				return rc;

			msleep(IU060F_SN12M0PZ_DELAY_MSECS);

			if (sn12m0pz_test(sn12m0pz_ctrl->set_test) < 0)
				return rc;

			if (mipi_config == IU060F_SN12M0PZ_STMIPID02)
				CDBG("%s,%d", __func__, __LINE__);
			return rc;
		}
	default:
		return rc;
		}
}


static int32_t sn12m0pz_video_config(int mode)
{

	int32_t rc = 0;
	int rt;


	if (mode == SENSOR_HFR_120FPS_MODE)
		sn12m0pz_ctrl->prev_res = QVGA_SIZE;

	/* change sensor resolution if needed */
	if (sn12m0pz_ctrl->curr_res != sn12m0pz_ctrl->prev_res) {
		if (sn12m0pz_ctrl->prev_res == QTR_SIZE) {
			rt = RES_PREVIEW;
			IU060F_SN12M0PZ_DELAY_MSECS = 35; /*measured on scope*/
			enable_single_D02_lane = 1;
		} else if (sn12m0pz_ctrl->prev_res == QVGA_SIZE) {
			rt = RES_VIDEO_120FPS;
			IU060F_SN12M0PZ_DELAY_MSECS = 35; /*measured on scope*/
			enable_single_D02_lane = 0;
		} else {
			rt = RES_CAPTURE;
			enable_single_D02_lane = 0;
		}

		if (sn12m0pz_sensor_setting(UPDATE_PERIODIC, rt) < 0)
			return rc;
	}

	sn12m0pz_ctrl->curr_res = sn12m0pz_ctrl->prev_res;
	sn12m0pz_ctrl->sensormode = mode;

	return rc;
}
static int32_t sn12m0pz_snapshot_config(int mode)
{
	int32_t rc = 0;
	int rt;
	/* change sensor resolution if needed */
	if (sn12m0pz_ctrl->curr_res != sn12m0pz_ctrl->pict_res) {
		if (sn12m0pz_ctrl->pict_res == QTR_SIZE) {
			rt = RES_PREVIEW;
			enable_single_D02_lane = 1;
		} else {
			rt = RES_CAPTURE;
			IU060F_SN12M0PZ_DELAY_MSECS = 100;/*measured on scope*/
			enable_single_D02_lane = 0;
		}

		if (sn12m0pz_sensor_setting(UPDATE_PERIODIC, rt) < 0)
			return rc;
	}

	sn12m0pz_ctrl->curr_res = sn12m0pz_ctrl->pict_res;
	sn12m0pz_ctrl->sensormode = mode;
	return rc;
}

static int32_t sn12m0pz_raw_snapshot_config(int mode)
{
	int32_t rc = 0;
	int rt;
	/* change sensor resolution if needed */
	if (sn12m0pz_ctrl->curr_res != sn12m0pz_ctrl->pict_res) {
		if (sn12m0pz_ctrl->pict_res == QTR_SIZE) {
			rt = RES_PREVIEW;
			enable_single_D02_lane = 1;
		} else {
			rt = RES_CAPTURE;
			IU060F_SN12M0PZ_DELAY_MSECS = 100;/*measured on scope*/
			enable_single_D02_lane = 0;
		}
		if (sn12m0pz_sensor_setting(UPDATE_PERIODIC, rt) < 0)
			return rc;
		}
	sn12m0pz_ctrl->curr_res = sn12m0pz_ctrl->pict_res;
	sn12m0pz_ctrl->sensormode = mode;
	return rc;
}
static int32_t sn12m0pz_set_sensor_mode(int  mode,
	int  res)
{
	int32_t rc;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
	case SENSOR_HFR_120FPS_MODE:
		rc = sn12m0pz_video_config(mode);
		break;

	case SENSOR_SNAPSHOT_MODE:
		rc = sn12m0pz_snapshot_config(mode);
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = sn12m0pz_raw_snapshot_config(mode);
		break;

	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int32_t sn12m0pz_power_down(void)
{
	return 0;
}


static int sn12m0pz_probe_init_done(const struct msm_camera_sensor_info *data)
{

	gpio_direction_output(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);
	gpio_direction_output(data->vcm_pwd, 0);
	gpio_free(data->vcm_pwd);
	return 0;
}

static int sn12m0pz_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int32_t rc;
	unsigned short chipidl, chipidh;
	CDBG("Requesting gpio");
	rc = gpio_request(data->sensor_reset, "sn12m0pz");
	CDBG(" sn12m0pz_probe_init_sensor");
	if (!rc) {
		gpio_direction_output(data->sensor_reset, 0);
		msleep(20);
		gpio_direction_output(data->sensor_reset, 1);
		msleep(13);
	} else {
		goto init_probe_done;
	}
	CDBG("Requestion gpio");
	rc = gpio_request(data->vcm_pwd, "sn12m0pz");
	CDBG(" sn12m0pz_probe_init_sensor");

	if (!rc) {
		gpio_direction_output(data->vcm_pwd, 0);
		msleep(20);
		gpio_direction_output(data->vcm_pwd, 1);
		msleep(13);
	} else {
		gpio_direction_output(data->sensor_reset, 0);
		gpio_free(data->sensor_reset);
		goto init_probe_done;
	}

	msleep(20);

	/* 3. Read sensor Model ID: */
	rc = sn12m0pz_i2c_read(0x0000, &chipidh, 1);
	if (rc < 0) {
		CDBG(" sn12m0pz_probe_init_sensor3");
		goto init_probe_fail;
	}
	rc = sn12m0pz_i2c_read(0x0001, &chipidl, 1);
	if (rc < 0) {
		CDBG(" sn12m0pz_probe_init_sensor4");
		goto init_probe_fail;
	}

	/* 4. Compare sensor ID to SN12M0PZ ID: */
	if (chipidh != 0x00 || chipidl != 0x60) {
		rc = -ENODEV;
		CDBG("sn12m0pz_probe_init_sensor fail chip id doesnot match");
		goto init_probe_fail;
	}

	msleep(SN12M0PZ_RESET_DELAY_MSECS);

	goto init_probe_done;

init_probe_fail:
	CDBG(" sn12m0pz_probe_init_sensor fails");
	sn12m0pz_probe_init_done(data);

init_probe_done:
	CDBG(" sn12m0pz_probe_init_sensor finishes");
	return rc;
}

int sn12m0pz_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	CDBG("Calling sn12m0pz_sensor_open_init");

	sn12m0pz_ctrl = kzalloc(sizeof(struct sn12m0pz_ctrl_t), GFP_KERNEL);
	if (!sn12m0pz_ctrl) {
		CDBG("sn12m0pz_init failed!");
		rc = -ENOMEM;
		goto init_done;
	}

	sn12m0pz_ctrl->fps_divider      = 1 * 0x00000400;
	sn12m0pz_ctrl->pict_fps_divider = 1 * 0x00000400;
	sn12m0pz_ctrl->set_test = TEST_OFF;
	sn12m0pz_ctrl->prev_res = QTR_SIZE;
	sn12m0pz_ctrl->pict_res = FULL_SIZE;
	sn12m0pz_ctrl->curr_res = INVALID_SIZE;
	if (data)
		sn12m0pz_ctrl->sensordata = data;

	if (rc < 0)
		return rc;

	/* enable mclk first */
	msm_camio_clk_rate_set(SN12M0PZ_DEFAULT_CLOCK_RATE);
	msleep(20);
	msm_camio_camif_pad_reg_reset();
	msleep(20);
	CDBG("Calling sn12m0pz_sensor_open_init");
	rc = sn12m0pz_probe_init_sensor(data);

	if (rc < 0)
		goto init_fail;
	/* send reset signal */
	if (mipi_config == IU060F_SN12M0PZ_STMIPID01) {
		if (sn12m0pz_stmipid01_config() < 0) {
			CDBG("Calling sn12m0pz_sensor_open_init fail");
			return rc;
		}
	} else {
		if (sn12m0pz_ctrl->prev_res  == QTR_SIZE)
			enable_single_D02_lane = 1;
		else /* FULL_SIZE */
			enable_single_D02_lane = 0;

		if (sn12m0pz_stmipid02_config() < 0) {
			CDBG("Calling sn12m0pz_sensor_open_init fail");
			return rc;
		}
	}


	if (sn12m0pz_ctrl->prev_res == QTR_SIZE) {
		if (sn12m0pz_sensor_setting(REG_INIT, RES_PREVIEW) < 0)
			return rc;
	} else if (sn12m0pz_ctrl->prev_res == QVGA_SIZE) {
		if (sn12m0pz_sensor_setting(REG_INIT, RES_VIDEO_120FPS) < 0)
			return rc;
	} else {
		if (sn12m0pz_sensor_setting(REG_INIT, RES_CAPTURE) < 0)
			return rc;
	}

	if (sn12m0pz_af_init() < 0)
		return rc;
	sn12m0pz_ctrl->fps = 30*Q8;
	if (rc < 0)
		goto init_fail;
	else
		goto init_done;
init_fail:
	CDBG(" init_fail");
	sn12m0pz_probe_init_done(data);
	kfree(sn12m0pz_ctrl);
init_done:
	CDBG("init_done");
	return rc;
}
static int __init sn12m0pz_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&sn12m0pz_wait_queue);
	return 0;
}

static const struct i2c_device_id sn12m0pz_i2c_id[] = {
	{ "sn12m0pz", 0},
	{ }
};

static int sn12m0pz_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("sn12m0pz_probe called!");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed");
		goto probe_failure;
	}

	sn12m0pz_sensorw = kzalloc(sizeof(struct sn12m0pz_work_t), GFP_KERNEL);
	if (!sn12m0pz_sensorw) {
		CDBG("kzalloc failed");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, sn12m0pz_sensorw);
	sn12m0pz_init_client(client);
	sn12m0pz_client = client;

	msleep(50);

	CDBG("sn12m0pz_probe successed! rc = %d", rc);
	return 0;

probe_failure:
	CDBG("sn12m0pz_probe failed! rc = %d", rc);
	return rc;
}

static int __exit sn12m0pz_remove(struct i2c_client *client)
{
	struct sn12m0pz_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	sn12m0pz_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver sn12m0pz_i2c_driver = {
	.id_table = sn12m0pz_i2c_id,
	.probe	= sn12m0pz_i2c_probe,
	.remove = __exit_p(sn12m0pz_i2c_remove),
	.driver = {
		.name = "sn12m0pz",
	},
};

int sn12m0pz_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	int32_t rc = 0;
	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	mutex_lock(&sn12m0pz_mut);

	CDBG("sn12m0pz_sensor_config: cfgtype = %d",
		cdata.cfgtype);
	switch (cdata.cfgtype) {
	case CFG_GET_PICT_FPS:
		sn12m0pz_get_pict_fps(cdata.cfg.gfps.prevfps,
					&(cdata.cfg.gfps.pictfps));

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PREV_L_PF:
		cdata.cfg.prevl_pf =
			sn12m0pz_get_prev_lines_pf();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PREV_P_PL:
		cdata.cfg.prevp_pl =
			sn12m0pz_get_prev_pixels_pl();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_L_PF:
		cdata.cfg.pictl_pf =
			sn12m0pz_get_pict_lines_pf();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_P_PL:
		cdata.cfg.pictp_pl =
			sn12m0pz_get_pict_pixels_pl();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_GET_PICT_MAX_EXP_LC:
		cdata.cfg.pict_max_exp_lc =
			sn12m0pz_get_pict_max_exp_lc();

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;

	case CFG_SET_FPS:
	case CFG_SET_PICT_FPS:
		rc = sn12m0pz_set_fps(&(cdata.cfg.fps));
		break;

	case CFG_SET_EXP_GAIN:
		rc =
			sn12m0pz_write_exp_gain(
				cdata.cfg.exp_gain.gain,
				cdata.cfg.exp_gain.line);
		break;
	case CFG_SET_PICT_EXP_GAIN:
		rc =
			sn12m0pz_set_pict_exp_gain(
				cdata.cfg.exp_gain.gain,
				cdata.cfg.exp_gain.line);
		break;

	case CFG_SET_MODE:
		rc = sn12m0pz_set_sensor_mode(cdata.mode,
					cdata.rs);
		break;

	case CFG_PWR_DOWN:
		rc = sn12m0pz_power_down();
		break;

	case CFG_MOVE_FOCUS:
		rc = sn12m0pz_move_focus(cdata.cfg.focus.dir,
					cdata.cfg.focus.steps);
		break;

	case CFG_SET_DEFAULT_FOCUS:
		rc = sn12m0pz_set_default_focus(cdata.cfg.focus.steps);
		break;

	case CFG_SET_EFFECT:
		rc = 0;
		break;
	case CFG_SET_LENS_SHADING:
		rc = 0;
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(&sn12m0pz_mut);

	return rc;
}

static int sn12m0pz_sensor_release(void)
{
	int rc = -EBADF;

	mutex_lock(&sn12m0pz_mut);

	sn12m0pz_power_down();

	gpio_direction_output(sn12m0pz_ctrl->sensordata->sensor_reset,
		0);
	gpio_free(sn12m0pz_ctrl->sensordata->sensor_reset);

	gpio_direction_output(sn12m0pz_ctrl->sensordata->vcm_pwd,
		0);
	gpio_free(sn12m0pz_ctrl->sensordata->vcm_pwd);

	kfree(sn12m0pz_ctrl);
	sn12m0pz_ctrl = NULL;

	CDBG("sn12m0pz_release completed");


	mutex_unlock(&sn12m0pz_mut);

	return rc;
}

static int sn12m0pz_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc;

	rc = i2c_add_driver(&sn12m0pz_i2c_driver);
	if (rc < 0 || sn12m0pz_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_fail;
	}

	msm_camio_clk_rate_set(SN12M0PZ_DEFAULT_CLOCK_RATE);
	msleep(20);

	rc = sn12m0pz_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;

	s->s_init = sn12m0pz_sensor_open_init;
	s->s_release = sn12m0pz_sensor_release;
	s->s_config  = sn12m0pz_sensor_config;
	s->s_mount_angle  = 0;
	sn12m0pz_probe_init_done(info);

	return rc;

probe_fail:
	CDBG("SENSOR PROBE FAILS!");
	i2c_del_driver(&sn12m0pz_i2c_driver);
	return rc;
}

static int __sn12m0pz_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, sn12m0pz_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __sn12m0pz_probe,
	.driver = {
		.name = "msm_camera_sn12m0pz",
		.owner = THIS_MODULE,
	},
};

static int __init sn12m0pz_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(sn12m0pz_init);

MODULE_DESCRIPTION("Sony 12M MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
