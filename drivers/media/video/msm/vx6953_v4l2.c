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
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/v4l2-subdev.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include <linux/slab.h>
#include "vx6953_v4l2.h"
#include "msm.h"

#define V4L2_IDENT_VX6953  50000

/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/

#define REG_GROUPED_PARAMETER_HOLD			0x0104
#define GROUPED_PARAMETER_HOLD_OFF			0x00
#define GROUPED_PARAMETER_HOLD				0x01
#define REG_MODE_SELECT					0x0100
#define MODE_SELECT_STANDBY_MODE			0x00
#define MODE_SELECT_STREAM				0x01
/* Integration Time */
#define REG_COARSE_INTEGRATION_TIME_HI			0x0202
#define REG_COARSE_INTEGRATION_TIME_LO			0x0203
/* Gain */
#define REG_ANALOGUE_GAIN_CODE_GLOBAL_HI		0x0204
#define REG_ANALOGUE_GAIN_CODE_GLOBAL_LO		0x0205
/* Digital Gain */
#define REG_DIGITAL_GAIN_GREEN_R_HI			0x020E
#define REG_DIGITAL_GAIN_GREEN_R_LO			0x020F
#define REG_DIGITAL_GAIN_RED_HI				0x0210
#define REG_DIGITAL_GAIN_RED_LO				0x0211
#define REG_DIGITAL_GAIN_BLUE_HI			0x0212
#define REG_DIGITAL_GAIN_BLUE_LO			0x0213
#define REG_DIGITAL_GAIN_GREEN_B_HI			0x0214
#define REG_DIGITAL_GAIN_GREEN_B_LO			0x0215
/* output bits setting */
#define REG_0x0112					0x0112
#define REG_0x0113					0x0113
/* PLL registers */
#define REG_VT_PIX_CLK_DIV				0x0301
#define REG_PRE_PLL_CLK_DIV				0x0305
#define REG_PLL_MULTIPLIER				0x0307
#define REG_OP_PIX_CLK_DIV				0x0309
#define REG_0x034c					0x034c
#define REG_0x034d					0x034d
#define REG_0x034e					0x034e
#define REG_0x034f					0x034f
#define REG_0x0387					0x0387
#define REG_0x0383					0x0383
#define REG_FRAME_LENGTH_LINES_HI			0x0340
#define REG_FRAME_LENGTH_LINES_LO			0x0341
#define REG_LINE_LENGTH_PCK_HI				0x0342
#define REG_LINE_LENGTH_PCK_LO				0x0343
#define REG_0x3030					0x3030
#define REG_0x0111					0x0111
#define REG_0x0136					0x0136
#define REG_0x0137					0x0137
#define REG_0x0b00					0x0b00
#define REG_0x3001					0x3001
#define REG_0x3004					0x3004
#define REG_0x3007					0x3007
#define REG_0x301a					0x301a
#define REG_0x3101					0x3101
#define REG_0x3364					0x3364
#define REG_0x3365					0x3365
#define REG_0x0b83					0x0b83
#define REG_0x0b84					0x0b84
#define REG_0x0b85					0x0b85
#define REG_0x0b88					0x0b88
#define REG_0x0b89					0x0b89
#define REG_0x0b8a					0x0b8a
#define REG_0x3005					0x3005
#define REG_0x3010					0x3010
#define REG_0x3036					0x3036
#define REG_0x3041					0x3041
#define REG_0x0b80					0x0b80
#define REG_0x0900					0x0900
#define REG_0x0901					0x0901
#define REG_0x0902					0x0902
#define REG_0x3016					0x3016
#define REG_0x301d					0x301d
#define REG_0x317e					0x317e
#define REG_0x317f					0x317f
#define REG_0x3400					0x3400
#define REG_0x303a					0x303a
#define REG_0x1716					0x1716
#define REG_0x1717					0x1717
#define REG_0x1718					0x1718
#define REG_0x1719					0x1719
#define REG_0x3006					0x3006
#define REG_0x301b					0x301b
#define REG_0x3098					0x3098
#define REG_0x309d					0x309d
#define REG_0x3011					0x3011
#define REG_0x3035					0x3035
#define REG_0x3045					0x3045
#define REG_0x3210					0x3210
#define	REG_0x0111					0x0111
#define REG_0x3410					0x3410
/* Test Pattern */
#define REG_TEST_PATTERN_MODE				0x0601

/*============================================================================
							 TYPE DECLARATIONS
============================================================================*/

/* 16bit address - 8 bit context register structure */
#define	VX6953_STM5M0EDOF_OFFSET	9
#define	Q8		0x00000100
#define	Q10		0x00000400
#define	VX6953_STM5M0EDOF_MAX_SNAPSHOT_EXPOSURE_LINE_COUNT	2922
#define	VX6953_STM5M0EDOF_DEFAULT_MASTER_CLK_RATE	24000000
#define	VX6953_STM5M0EDOF_OP_PIXEL_CLOCK_RATE	79800000
#define	VX6953_STM5M0EDOF_VT_PIXEL_CLOCK_RATE	88670000
/* Full	Size */
#define	VX6953_FULL_SIZE_WIDTH	2608
#define	VX6953_FULL_SIZE_HEIGHT		1960
#define	VX6953_FULL_SIZE_DUMMY_PIXELS	1
#define	VX6953_FULL_SIZE_DUMMY_LINES	0
/* Quarter Size	*/
#define	VX6953_QTR_SIZE_WIDTH	1304
#define	VX6953_QTR_SIZE_HEIGHT		980
#define	VX6953_QTR_SIZE_DUMMY_PIXELS	1
#define	VX6953_QTR_SIZE_DUMMY_LINES		0
/* Blanking	as measured	on the scope */
/* Full	Size */
#define	VX6953_HRZ_FULL_BLK_PIXELS	348
#define	VX6953_VER_FULL_BLK_LINES	40
/* Quarter Size	*/
#define	VX6953_HRZ_QTR_BLK_PIXELS	1628
#define	VX6953_VER_QTR_BLK_LINES	28
#define	MAX_LINE_LENGTH_PCK		8190
#define	VX6953_REVISION_NUMBER_CUT2	0x10/*revision number	for	Cut2.0*/
#define	VX6953_REVISION_NUMBER_CUT3	0x20/*revision number	for	Cut3.0*/
/* FIXME: Changes from here */
struct vx6953_work_t {
	struct work_struct work;
};

static struct vx6953_work_t *vx6953_sensorw;
static struct i2c_client *vx6953_client;

struct vx6953_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;

	uint32_t sensormode;
	uint32_t fps_divider;  /* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;  /* init to 1 * 0x00000400 */
	uint16_t fps;

	int16_t curr_lens_pos;
	uint16_t curr_step_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint16_t total_lines_per_frame;

	enum vx6953_resolution_t prev_res;
	enum vx6953_resolution_t pict_res;
	enum vx6953_resolution_t curr_res;
	enum vx6953_test_mode_t  set_test;
	enum sensor_revision_t sensor_type;

	enum edof_mode_t edof_mode;

	unsigned short imgaddr;

	struct v4l2_subdev *sensor_dev;
	struct vx6953_format *fmt;
};


static uint8_t vx6953_stm5m0edof_delay_msecs_stdby;
static uint16_t vx6953_stm5m0edof_delay_msecs_stream = 20;

static struct vx6953_ctrl_t *vx6953_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(vx6953_wait_queue);
DEFINE_MUTEX(vx6953_mut);
static struct vx6953_i2c_reg_conf patch_tbl_cut2[] = {
	{0xFB94, 0},	/*intialise Data Xfer Status reg*/
	{0xFB95, 0},	/*gain 1	  (0x00)*/
	{0xFB96, 0},	/*gain 1.07   (0x10)*/
	{0xFB97, 0},	/*gain 1.14   (0x20)*/
	{0xFB98, 0},	/*gain 1.23   (0x30)*/
	{0xFB99, 0},	/*gain 1.33   (0x40)*/
	{0xFB9A, 0},	/*gain 1.45   (0x50)*/
	{0xFB9B, 0},	/*gain 1.6    (0x60)*/
	{0xFB9C, 0},	/*gain 1.78   (0x70)*/
	{0xFB9D, 2},	/*gain 2	  (0x80)*/
	{0xFB9E, 2},	/*gain 2.29   (0x90)*/
	{0xFB9F, 3},	/*gain 2.67   (0xA0)*/
	{0xFBA0, 3},	/*gain 3.2    (0xB0)*/
	{0xFBA1, 4},	/*gain 4	  (0xC0)*/
	{0xFBA2, 7},	/*gain 5.33   (0xD0)*/
	{0xFBA3, 10},	/*gain 8	  (0xE0)*/
	{0xFBA4, 11},	/*gain 9.14   (0xE4)*/
	{0xFBA5, 13},	/*gain 10.67  (0xE8)*/
	{0xFBA6, 15},	/*gain 12.8   (0xEC)*/
	{0xFBA7, 19},	/*gain 16     (0xF0)*/
	{0xF800, 0x12},
	{0xF801, 0x06},
	{0xF802, 0xf7},
	{0xF803, 0x90},
	{0xF804, 0x02},
	{0xF805, 0x05},
	{0xF806, 0xe0},
	{0xF807, 0xff},
	{0xF808, 0x65},
	{0xF809, 0x7d},
	{0xF80A, 0x70},
	{0xF80B, 0x03},
	{0xF80C, 0x02},
	{0xF80D, 0xf9},
	{0xF80E, 0x1c},
	{0xF80F, 0x8f},
	{0xF810, 0x7d},
	{0xF811, 0xe4},
	{0xF812, 0xf5},
	{0xF813, 0x7a},
	{0xF814, 0x75},
	{0xF815, 0x78},
	{0xF816, 0x30},
	{0xF817, 0x75},
	{0xF818, 0x79},
	{0xF819, 0x53},
	{0xF81A, 0x85},
	{0xF81B, 0x79},
	{0xF81C, 0x82},
	{0xF81D, 0x85},
	{0xF81E, 0x78},
	{0xF81F, 0x83},
	{0xF820, 0xe0},
	{0xF821, 0xc3},
	{0xF822, 0x95},
	{0xF823, 0x7b},
	{0xF824, 0xf0},
	{0xF825, 0x74},
	{0xF826, 0x02},
	{0xF827, 0x25},
	{0xF828, 0x79},
	{0xF829, 0xf5},
	{0xF82A, 0x79},
	{0xF82B, 0xe4},
	{0xF82C, 0x35},
	{0xF82D, 0x78},
	{0xF82E, 0xf5},
	{0xF82F, 0x78},
	{0xF830, 0x05},
	{0xF831, 0x7a},
	{0xF832, 0xe5},
	{0xF833, 0x7a},
	{0xF834, 0xb4},
	{0xF835, 0x08},
	{0xF836, 0xe3},
	{0xF837, 0xe5},
	{0xF838, 0x7d},
	{0xF839, 0x70},
	{0xF83A, 0x04},
	{0xF83B, 0xff},
	{0xF83C, 0x02},
	{0xF83D, 0xf8},
	{0xF83E, 0xe4},
	{0xF83F, 0xe5},
	{0xF840, 0x7d},
	{0xF841, 0xb4},
	{0xF842, 0x10},
	{0xF843, 0x05},
	{0xF844, 0x7f},
	{0xF845, 0x01},
	{0xF846, 0x02},
	{0xF847, 0xf8},
	{0xF848, 0xe4},
	{0xF849, 0xe5},
	{0xF84A, 0x7d},
	{0xF84B, 0xb4},
	{0xF84C, 0x20},
	{0xF84D, 0x05},
	{0xF84E, 0x7f},
	{0xF84F, 0x02},
	{0xF850, 0x02},
	{0xF851, 0xf8},
	{0xF852, 0xe4},
	{0xF853, 0xe5},
	{0xF854, 0x7d},
	{0xF855, 0xb4},
	{0xF856, 0x30},
	{0xF857, 0x05},
	{0xF858, 0x7f},
	{0xF859, 0x03},
	{0xF85A, 0x02},
	{0xF85B, 0xf8},
	{0xF85C, 0xe4},
	{0xF85D, 0xe5},
	{0xF85E, 0x7d},
	{0xF85F, 0xb4},
	{0xF860, 0x40},
	{0xF861, 0x04},
	{0xF862, 0x7f},
	{0xF863, 0x04},
	{0xF864, 0x80},
	{0xF865, 0x7e},
	{0xF866, 0xe5},
	{0xF867, 0x7d},
	{0xF868, 0xb4},
	{0xF869, 0x50},
	{0xF86A, 0x04},
	{0xF86B, 0x7f},
	{0xF86C, 0x05},
	{0xF86D, 0x80},
	{0xF86E, 0x75},
	{0xF86F, 0xe5},
	{0xF870, 0x7d},
	{0xF871, 0xb4},
	{0xF872, 0x60},
	{0xF873, 0x04},
	{0xF874, 0x7f},
	{0xF875, 0x06},
	{0xF876, 0x80},
	{0xF877, 0x6c},
	{0xF878, 0xe5},
	{0xF879, 0x7d},
	{0xF87A, 0xb4},
	{0xF87B, 0x70},
	{0xF87C, 0x04},
	{0xF87D, 0x7f},
	{0xF87E, 0x07},
	{0xF87F, 0x80},
	{0xF880, 0x63},
	{0xF881, 0xe5},
	{0xF882, 0x7d},
	{0xF883, 0xb4},
	{0xF884, 0x80},
	{0xF885, 0x04},
	{0xF886, 0x7f},
	{0xF887, 0x08},
	{0xF888, 0x80},
	{0xF889, 0x5a},
	{0xF88A, 0xe5},
	{0xF88B, 0x7d},
	{0xF88C, 0xb4},
	{0xF88D, 0x90},
	{0xF88E, 0x04},
	{0xF88F, 0x7f},
	{0xF890, 0x09},
	{0xF891, 0x80},
	{0xF892, 0x51},
	{0xF893, 0xe5},
	{0xF894, 0x7d},
	{0xF895, 0xb4},
	{0xF896, 0xa0},
	{0xF897, 0x04},
	{0xF898, 0x7f},
	{0xF899, 0x0a},
	{0xF89A, 0x80},
	{0xF89B, 0x48},
	{0xF89C, 0xe5},
	{0xF89D, 0x7d},
	{0xF89E, 0xb4},
	{0xF89F, 0xb0},
	{0xF8A0, 0x04},
	{0xF8A1, 0x7f},
	{0xF8A2, 0x0b},
	{0xF8A3, 0x80},
	{0xF8A4, 0x3f},
	{0xF8A5, 0xe5},
	{0xF8A6, 0x7d},
	{0xF8A7, 0xb4},
	{0xF8A8, 0xc0},
	{0xF8A9, 0x04},
	{0xF8AA, 0x7f},
	{0xF8AB, 0x0c},
	{0xF8AC, 0x80},
	{0xF8AD, 0x36},
	{0xF8AE, 0xe5},
	{0xF8AF, 0x7d},
	{0xF8B0, 0xb4},
	{0xF8B1, 0xd0},
	{0xF8B2, 0x04},
	{0xF8B3, 0x7f},
	{0xF8B4, 0x0d},
	{0xF8B5, 0x80},
	{0xF8B6, 0x2d},
	{0xF8B7, 0xe5},
	{0xF8B8, 0x7d},
	{0xF8B9, 0xb4},
	{0xF8BA, 0xe0},
	{0xF8BB, 0x04},
	{0xF8BC, 0x7f},
	{0xF8BD, 0x0e},
	{0xF8BE, 0x80},
	{0xF8BF, 0x24},
	{0xF8C0, 0xe5},
	{0xF8C1, 0x7d},
	{0xF8C2, 0xb4},
	{0xF8C3, 0xe4},
	{0xF8C4, 0x04},
	{0xF8C5, 0x7f},
	{0xF8C6, 0x0f},
	{0xF8C7, 0x80},
	{0xF8C8, 0x1b},
	{0xF8C9, 0xe5},
	{0xF8CA, 0x7d},
	{0xF8CB, 0xb4},
	{0xF8CC, 0xe8},
	{0xF8CD, 0x04},
	{0xF8CE, 0x7f},
	{0xF8CF, 0x10},
	{0xF8D0, 0x80},
	{0xF8D1, 0x12},
	{0xF8D2, 0xe5},
	{0xF8D3, 0x7d},
	{0xF8D4, 0xb4},
	{0xF8D5, 0xec},
	{0xF8D6, 0x04},
	{0xF8D7, 0x7f},
	{0xF8D8, 0x11},
	{0xF8D9, 0x80},
	{0xF8DA, 0x09},
	{0xF8DB, 0xe5},
	{0xF8DC, 0x7d},
	{0xF8DD, 0x7f},
	{0xF8DE, 0x00},
	{0xF8DF, 0xb4},
	{0xF8E0, 0xf0},
	{0xF8E1, 0x02},
	{0xF8E2, 0x7f},
	{0xF8E3, 0x12},
	{0xF8E4, 0x8f},
	{0xF8E5, 0x7c},
	{0xF8E6, 0xef},
	{0xF8E7, 0x24},
	{0xF8E8, 0x95},
	{0xF8E9, 0xff},
	{0xF8EA, 0xe4},
	{0xF8EB, 0x34},
	{0xF8EC, 0xfb},
	{0xF8ED, 0x8f},
	{0xF8EE, 0x82},
	{0xF8EF, 0xf5},
	{0xF8F0, 0x83},
	{0xF8F1, 0xe4},
	{0xF8F2, 0x93},
	{0xF8F3, 0xf5},
	{0xF8F4, 0x7c},
	{0xF8F5, 0xf5},
	{0xF8F6, 0x7b},
	{0xF8F7, 0xe4},
	{0xF8F8, 0xf5},
	{0xF8F9, 0x7a},
	{0xF8FA, 0x75},
	{0xF8FB, 0x78},
	{0xF8FC, 0x30},
	{0xF8FD, 0x75},
	{0xF8FE, 0x79},
	{0xF8FF, 0x53},
	{0xF900, 0x85},
	{0xF901, 0x79},
	{0xF902, 0x82},
	{0xF903, 0x85},
	{0xF904, 0x78},
	{0xF905, 0x83},
	{0xF906, 0xe0},
	{0xF907, 0x25},
	{0xF908, 0x7c},
	{0xF909, 0xf0},
	{0xF90A, 0x74},
	{0xF90B, 0x02},
	{0xF90C, 0x25},
	{0xF90D, 0x79},
	{0xF90E, 0xf5},
	{0xF90F, 0x79},
	{0xF910, 0xe4},
	{0xF911, 0x35},
	{0xF912, 0x78},
	{0xF913, 0xf5},
	{0xF914, 0x78},
	{0xF915, 0x05},
	{0xF916, 0x7a},
	{0xF917, 0xe5},
	{0xF918, 0x7a},
	{0xF919, 0xb4},
	{0xF91A, 0x08},
	{0xF91B, 0xe4},
	{0xF91C, 0x02},
	{0xF91D, 0x18},
	{0xF91E, 0x32},
	{0xF91F, 0x22},
	{0xF920, 0xf0},
	{0xF921, 0x90},
	{0xF922, 0xa0},
	{0xF923, 0xf8},
	{0xF924, 0xe0},
	{0xF925, 0x70},
	{0xF926, 0x02},
	{0xF927, 0xa3},
	{0xF928, 0xe0},
	{0xF929, 0x70},
	{0xF92A, 0x0a},
	{0xF92B, 0x90},
	{0xF92C, 0xa1},
	{0xF92D, 0x10},
	{0xF92E, 0xe0},
	{0xF92F, 0xfe},
	{0xF930, 0xa3},
	{0xF931, 0xe0},
	{0xF932, 0xff},
	{0xF933, 0x80},
	{0xF934, 0x04},
	{0xF935, 0x7e},
	{0xF936, 0x00},
	{0xF937, 0x7f},
	{0xF938, 0x00},
	{0xF939, 0x8e},
	{0xF93A, 0x7e},
	{0xF93B, 0x8f},
	{0xF93C, 0x7f},
	{0xF93D, 0x90},
	{0xF93E, 0x36},
	{0xF93F, 0x0d},
	{0xF940, 0xe0},
	{0xF941, 0x44},
	{0xF942, 0x02},
	{0xF943, 0xf0},
	{0xF944, 0x90},
	{0xF945, 0x36},
	{0xF946, 0x0e},
	{0xF947, 0xe5},
	{0xF948, 0x7e},
	{0xF949, 0xf0},
	{0xF94A, 0xa3},
	{0xF94B, 0xe5},
	{0xF94C, 0x7f},
	{0xF94D, 0xf0},
	{0xF94E, 0xe5},
	{0xF94F, 0x3a},
	{0xF950, 0x60},
	{0xF951, 0x0c},
	{0xF952, 0x90},
	{0xF953, 0x36},
	{0xF954, 0x09},
	{0xF955, 0xe0},
	{0xF956, 0x70},
	{0xF957, 0x06},
	{0xF958, 0x90},
	{0xF959, 0x36},
	{0xF95A, 0x08},
	{0xF95B, 0xf0},
	{0xF95C, 0xf5},
	{0xF95D, 0x3a},
	{0xF95E, 0x02},
	{0xF95F, 0x03},
	{0xF960, 0x94},
	{0xF961, 0x22},
	{0xF962, 0x78},
	{0xF963, 0x07},
	{0xF964, 0xe6},
	{0xF965, 0xd3},
	{0xF966, 0x94},
	{0xF967, 0x00},
	{0xF968, 0x40},
	{0xF969, 0x16},
	{0xF96A, 0x16},
	{0xF96B, 0xe6},
	{0xF96C, 0x90},
	{0xF96D, 0x30},
	{0xF96E, 0xa1},
	{0xF96F, 0xf0},
	{0xF970, 0x90},
	{0xF971, 0x43},
	{0xF972, 0x83},
	{0xF973, 0xe0},
	{0xF974, 0xb4},
	{0xF975, 0x01},
	{0xF976, 0x0f},
	{0xF977, 0x90},
	{0xF978, 0x43},
	{0xF979, 0x87},
	{0xF97A, 0xe0},
	{0xF97B, 0xb4},
	{0xF97C, 0x01},
	{0xF97D, 0x08},
	{0xF97E, 0x80},
	{0xF97F, 0x00},
	{0xF980, 0x90},
	{0xF981, 0x30},
	{0xF982, 0xa0},
	{0xF983, 0x74},
	{0xF984, 0x01},
	{0xF985, 0xf0},
	{0xF986, 0x22},
	{0xF987, 0xf0},
	{0xF988, 0x90},
	{0xF989, 0x35},
	{0xF98A, 0xba},
	{0xF98B, 0xe0},
	{0xF98C, 0xb4},
	{0xF98D, 0x0a},
	{0xF98E, 0x0d},
	{0xF98F, 0xa3},
	{0xF990, 0xe0},
	{0xF991, 0xb4},
	{0xF992, 0x01},
	{0xF993, 0x08},
	{0xF994, 0x90},
	{0xF995, 0xfb},
	{0xF996, 0x94},
	{0xF997, 0xe0},
	{0xF998, 0x90},
	{0xF999, 0x35},
	{0xF99A, 0xb8},
	{0xF99B, 0xf0},
	{0xF99C, 0xd0},
	{0xF99D, 0xd0},
	{0xF99E, 0xd0},
	{0xF99F, 0x82},
	{0xF9A0, 0xd0},
	{0xF9A1, 0x83},
	{0xF9A2, 0xd0},
	{0xF9A3, 0xe0},
	{0xF9A4, 0x32},
	{0xF9A5, 0x22},
	{0xF9A6, 0xe5},
	{0xF9A7, 0x7f},
	{0xF9A8, 0x45},
	{0xF9A9, 0x7e},
	{0xF9AA, 0x60},
	{0xF9AB, 0x15},
	{0xF9AC, 0x90},
	{0xF9AD, 0x01},
	{0xF9AE, 0x00},
	{0xF9AF, 0xe0},
	{0xF9B0, 0x70},
	{0xF9B1, 0x0f},
	{0xF9B2, 0x90},
	{0xF9B3, 0xa0},
	{0xF9B4, 0xf8},
	{0xF9B5, 0xe5},
	{0xF9B6, 0x7e},
	{0xF9B7, 0xf0},
	{0xF9B8, 0xa3},
	{0xF9B9, 0xe5},
	{0xF9BA, 0x7f},
	{0xF9BB, 0xf0},
	{0xF9BC, 0xe4},
	{0xF9BD, 0xf5},
	{0xF9BE, 0x7e},
	{0xF9BF, 0xf5},
	{0xF9C0, 0x7f},
	{0xF9C1, 0x22},
	{0xF9C2, 0x02},
	{0xF9C3, 0x0e},
	{0xF9C4, 0x79},
	{0xF9C5, 0x22},
	/* Offsets:*/
	{0x35C6, 0x00},/* FIDDLEDARKCAL*/
	{0x35C7, 0x00},
	{0x35C8, 0x01},/*STOREDISTANCEATSTOPSTREAMING*/
	{0x35C9, 0x20},
	{0x35CA, 0x01},/*BRUCEFIX*/
	{0x35CB, 0x62},
	{0x35CC, 0x01},/*FIXDATAXFERSTATUSREG*/
	{0x35CD, 0x87},
	{0x35CE, 0x01},/*FOCUSDISTANCEUPDATE*/
	{0x35CF, 0xA6},
	{0x35D0, 0x01},/*SKIPEDOFRESET*/
	{0x35D1, 0xC2},
	{0x35D2, 0x00},
	{0x35D3, 0xFB},
	{0x35D4, 0x00},
	{0x35D5, 0x94},
	{0x35D6, 0x00},
	{0x35D7, 0xFB},
	{0x35D8, 0x00},
	{0x35D9, 0x94},
	{0x35DA, 0x00},
	{0x35DB, 0xFB},
	{0x35DC, 0x00},
	{0x35DD, 0x94},
	{0x35DE, 0x00},
	{0x35DF, 0xFB},
	{0x35E0, 0x00},
	{0x35E1, 0x94},
	{0x35E6, 0x18},/* FIDDLEDARKCAL*/
	{0x35E7, 0x2F},
	{0x35E8, 0x03},/* STOREDISTANCEATSTOPSTREAMING*/
	{0x35E9, 0x93},
	{0x35EA, 0x18},/* BRUCEFIX*/
	{0x35EB, 0x99},
	{0x35EC, 0x00},/* FIXDATAXFERSTATUSREG*/
	{0x35ED, 0xA3},
	{0x35EE, 0x21},/* FOCUSDISTANCEUPDATE*/
	{0x35EF, 0x5B},
	{0x35F0, 0x0E},/* SKIPEDOFRESET*/
	{0x35F1, 0x74},
	{0x35F2, 0x04},
	{0x35F3, 0x64},
	{0x35F4, 0x04},
	{0x35F5, 0x65},
	{0x35F6, 0x04},
	{0x35F7, 0x7B},
	{0x35F8, 0x04},
	{0x35F9, 0x7C},
	{0x35FA, 0x04},
	{0x35FB, 0xDD},
	{0x35FC, 0x04},
	{0x35FD, 0xDE},
	{0x35FE, 0x04},
	{0x35FF, 0xEF},
	{0x3600, 0x04},
	{0x3601, 0xF0},
	/*Jump/Data:*/
	{0x35C2, 0x3F},/* Jump Reg*/
	{0x35C3, 0xFF},/* Jump Reg*/
	{0x35C4, 0x3F},/* Data Reg*/
	{0x35C5, 0xC0},/* Data Reg*/
	{0x35C0, 0x01},/* Enable*/

};

static struct vx6953_i2c_reg_conf edof_tbl[] = {
	{0xa098, 0x02},
	{0xa099, 0x87},
	{0xa09c, 0x00},
	{0xa09d, 0xc5},
	{0xa4ec, 0x05},
	{0xa4ed, 0x05},
	{0xa4f0, 0x04},
	{0xa4f1, 0x04},
	{0xa4f4, 0x04},
	{0xa4f5, 0x05},
	{0xa4f8, 0x05},
	{0xa4f9, 0x07},
	{0xa4fc, 0x07},
	{0xa4fd, 0x07},
	{0xa500, 0x07},
	{0xa501, 0x07},
	{0xa504, 0x08},
	{0xa505, 0x08},
	{0xa518, 0x01},
	{0xa519, 0x02},
	{0xa51c, 0x01},
	{0xa51d, 0x00},
	{0xa534, 0x00},
	{0xa535, 0x04},
	{0xa538, 0x04},
	{0xa539, 0x03},
	{0xa53c, 0x05},
	{0xa53d, 0x07},
	{0xa540, 0x07},
	{0xa541, 0x06},
	{0xa544, 0x07},
	{0xa545, 0x06},
	{0xa548, 0x05},
	{0xa549, 0x06},
	{0xa54c, 0x06},
	{0xa54d, 0x07},
	{0xa550, 0x07},
	{0xa551, 0x04},
	{0xa554, 0x04},
	{0xa555, 0x04},
	{0xa558, 0x05},
	{0xa559, 0x06},
	{0xa55c, 0x07},
	{0xa55d, 0x07},
	{0xa56c, 0x00},
	{0xa56d, 0x0a},
	{0xa570, 0x08},
	{0xa571, 0x05},
	{0xa574, 0x04},
	{0xa575, 0x03},
	{0xa578, 0x04},
	{0xa579, 0x04},
	{0xa58c, 0x1f},
	{0xa58d, 0x1b},
	{0xa590, 0x17},
	{0xa591, 0x13},
	{0xa594, 0x10},
	{0xa595, 0x0d},
	{0xa598, 0x0f},
	{0xa599, 0x11},
	{0xa59c, 0x03},
	{0xa59d, 0x03},
	{0xa5a0, 0x03},
	{0xa5a1, 0x03},
	{0xa5a4, 0x03},
	{0xa5a5, 0x04},
	{0xa5a8, 0x05},
	{0xa5a9, 0x00},
	{0xa5ac, 0x00},
	{0xa5ad, 0x00},
	{0xa5b0, 0x00},
	{0xa5b1, 0x00},
	{0xa5b4, 0x00},
	{0xa5b5, 0x00},
	{0xa5c4, 0x1f},
	{0xa5c5, 0x13},
	{0xa5c8, 0x14},
	{0xa5c9, 0x14},
	{0xa5cc, 0x14},
	{0xa5cd, 0x13},
	{0xa5d0, 0x17},
	{0xa5d1, 0x1a},
	{0xa5f4, 0x05},
	{0xa5f5, 0x05},
	{0xa5f8, 0x05},
	{0xa5f9, 0x06},
	{0xa5fc, 0x06},
	{0xa5fd, 0x06},
	{0xa600, 0x06},
	{0xa601, 0x06},
	{0xa608, 0x07},
	{0xa609, 0x08},
	{0xa60c, 0x08},
	{0xa60d, 0x07},
	{0xa63c, 0x00},
	{0xa63d, 0x02},
	{0xa640, 0x02},
	{0xa641, 0x02},
	{0xa644, 0x02},
	{0xa645, 0x02},
	{0xa648, 0x03},
	{0xa649, 0x04},
	{0xa64c, 0x0a},
	{0xa64d, 0x09},
	{0xa650, 0x08},
	{0xa651, 0x09},
	{0xa654, 0x09},
	{0xa655, 0x0a},
	{0xa658, 0x0a},
	{0xa659, 0x0a},
	{0xa65c, 0x0a},
	{0xa65d, 0x09},
	{0xa660, 0x09},
	{0xa661, 0x09},
	{0xa664, 0x09},
	{0xa665, 0x08},
	{0xa680, 0x01},
	{0xa681, 0x02},
	{0xa694, 0x1f},
	{0xa695, 0x10},
	{0xa698, 0x0e},
	{0xa699, 0x0c},
	{0xa69c, 0x0d},
	{0xa69d, 0x0d},
	{0xa6a0, 0x0f},
	{0xa6a1, 0x11},
	{0xa6a4, 0x00},
	{0xa6a5, 0x00},
	{0xa6a8, 0x00},
	{0xa6a9, 0x00},
	{0xa6ac, 0x00},
	{0xa6ad, 0x00},
	{0xa6b0, 0x00},
	{0xa6b1, 0x04},
	{0xa6b4, 0x04},
	{0xa6b5, 0x04},
	{0xa6b8, 0x04},
	{0xa6b9, 0x04},
	{0xa6bc, 0x05},
	{0xa6bd, 0x05},
	{0xa6c0, 0x1f},
	{0xa6c1, 0x1f},
	{0xa6c4, 0x1f},
	{0xa6c5, 0x1f},
	{0xa6c8, 0x1f},
	{0xa6c9, 0x1f},
	{0xa6cc, 0x1f},
	{0xa6cd, 0x0b},
	{0xa6d0, 0x0c},
	{0xa6d1, 0x0d},
	{0xa6d4, 0x0d},
	{0xa6d5, 0x0d},
	{0xa6d8, 0x11},
	{0xa6d9, 0x14},
	{0xa6fc, 0x02},
	{0xa6fd, 0x03},
	{0xa700, 0x03},
	{0xa701, 0x03},
	{0xa704, 0x03},
	{0xa705, 0x04},
	{0xa708, 0x05},
	{0xa709, 0x02},
	{0xa70c, 0x02},
	{0xa70d, 0x02},
	{0xa710, 0x03},
	{0xa711, 0x04},
	{0xa714, 0x04},
	{0xa715, 0x04},
	{0xa744, 0x00},
	{0xa745, 0x03},
	{0xa748, 0x04},
	{0xa749, 0x04},
	{0xa74c, 0x05},
	{0xa74d, 0x06},
	{0xa750, 0x07},
	{0xa751, 0x07},
	{0xa754, 0x05},
	{0xa755, 0x05},
	{0xa758, 0x05},
	{0xa759, 0x05},
	{0xa75c, 0x05},
	{0xa75d, 0x06},
	{0xa760, 0x07},
	{0xa761, 0x07},
	{0xa764, 0x06},
	{0xa765, 0x05},
	{0xa768, 0x05},
	{0xa769, 0x05},
	{0xa76c, 0x06},
	{0xa76d, 0x07},
	{0xa77c, 0x00},
	{0xa77d, 0x05},
	{0xa780, 0x05},
	{0xa781, 0x05},
	{0xa784, 0x05},
	{0xa785, 0x04},
	{0xa788, 0x05},
	{0xa789, 0x06},
	{0xa79c, 0x1f},
	{0xa79d, 0x15},
	{0xa7a0, 0x13},
	{0xa7a1, 0x10},
	{0xa7a4, 0x0f},
	{0xa7a5, 0x0d},
	{0xa7a8, 0x11},
	{0xa7a9, 0x14},
	{0xa7ac, 0x02},
	{0xa7ad, 0x02},
	{0xa7b0, 0x02},
	{0xa7b1, 0x02},
	{0xa7b4, 0x02},
	{0xa7b5, 0x03},
	{0xa7b8, 0x03},
	{0xa7b9, 0x00},
	{0xa7bc, 0x00},
	{0xa7bd, 0x00},
	{0xa7c0, 0x00},
	{0xa7c1, 0x00},
	{0xa7c4, 0x00},
	{0xa7c5, 0x00},
	{0xa7d4, 0x1f},
	{0xa7d5, 0x0d},
	{0xa7d8, 0x0f},
	{0xa7d9, 0x10},
	{0xa7dc, 0x10},
	{0xa7dd, 0x10},
	{0xa7e0, 0x13},
	{0xa7e1, 0x16},
	{0xa7f4, 0x00},
	{0xa7f5, 0x03},
	{0xa7f8, 0x04},
	{0xa7f9, 0x04},
	{0xa7fc, 0x04},
	{0xa7fd, 0x03},
	{0xa800, 0x03},
	{0xa801, 0x03},
	{0xa804, 0x03},
	{0xa805, 0x03},
	{0xa808, 0x03},
	{0xa809, 0x03},
	{0xa80c, 0x03},
	{0xa80d, 0x04},
	{0xa810, 0x04},
	{0xa811, 0x0a},
	{0xa814, 0x0a},
	{0xa815, 0x0a},
	{0xa818, 0x0f},
	{0xa819, 0x14},
	{0xa81c, 0x14},
	{0xa81d, 0x14},
	{0xa82c, 0x00},
	{0xa82d, 0x04},
	{0xa830, 0x02},
	{0xa831, 0x00},
	{0xa834, 0x00},
	{0xa835, 0x00},
	{0xa838, 0x00},
	{0xa839, 0x00},
	{0xa840, 0x1f},
	{0xa841, 0x1f},
	{0xa848, 0x1f},
	{0xa849, 0x1f},
	{0xa84c, 0x1f},
	{0xa84d, 0x0c},
	{0xa850, 0x0c},
	{0xa851, 0x0c},
	{0xa854, 0x0c},
	{0xa855, 0x0c},
	{0xa858, 0x0c},
	{0xa859, 0x0c},
	{0xa85c, 0x0c},
	{0xa85d, 0x0c},
	{0xa860, 0x0c},
	{0xa861, 0x0c},
	{0xa864, 0x0c},
	{0xa865, 0x0c},
	{0xa868, 0x0c},
	{0xa869, 0x0c},
	{0xa86c, 0x0c},
	{0xa86d, 0x0c},
	{0xa870, 0x0c},
	{0xa871, 0x0c},
	{0xa874, 0x0c},
	{0xa875, 0x0c},
	{0xa878, 0x1f},
	{0xa879, 0x1f},
	{0xa87c, 0x1f},
	{0xa87d, 0x1f},
	{0xa880, 0x1f},
	{0xa881, 0x1f},
	{0xa884, 0x1f},
	{0xa885, 0x0c},
	{0xa888, 0x0c},
	{0xa889, 0x0c},
	{0xa88c, 0x0c},
	{0xa88d, 0x0c},
	{0xa890, 0x0c},
	{0xa891, 0x0c},
	{0xa898, 0x1f},
	{0xa899, 0x1f},
	{0xa8a0, 0x1f},
	{0xa8a1, 0x1f},
	{0xa8a4, 0x1f},
	{0xa8a5, 0x0c},
	{0xa8a8, 0x0c},
	{0xa8a9, 0x0c},
	{0xa8ac, 0x0c},
	{0xa8ad, 0x0c},
	{0xa8b0, 0x0c},
	{0xa8b1, 0x0c},
	{0xa8b4, 0x0c},
	{0xa8b5, 0x0c},
	{0xa8b8, 0x0c},
	{0xa8b9, 0x0c},
	{0xa8bc, 0x0c},
	{0xa8bd, 0x0c},
	{0xa8c0, 0x0c},
	{0xa8c1, 0x0c},
	{0xa8c4, 0x0c},
	{0xa8c5, 0x0c},
	{0xa8c8, 0x0c},
	{0xa8c9, 0x0c},
	{0xa8cc, 0x0c},
	{0xa8cd, 0x0c},
	{0xa8d0, 0x1f},
	{0xa8d1, 0x1f},
	{0xa8d4, 0x1f},
	{0xa8d5, 0x1f},
	{0xa8d8, 0x1f},
	{0xa8d9, 0x1f},
	{0xa8dc, 0x1f},
	{0xa8dd, 0x0c},
	{0xa8e0, 0x0c},
	{0xa8e1, 0x0c},
	{0xa8e4, 0x0c},
	{0xa8e5, 0x0c},
	{0xa8e8, 0x0c},
	{0xa8e9, 0x0c},
	{0xa8f0, 0x1f},
	{0xa8f1, 0x1f},
	{0xa8f8, 0x1f},
	{0xa8f9, 0x1f},
	{0xa8fc, 0x1f},
	{0xa8fd, 0x0c},
	{0xa900, 0x0c},
	{0xa901, 0x0c},
	{0xa904, 0x0c},
	{0xa905, 0x0c},
	{0xa908, 0x0c},
	{0xa909, 0x0c},
	{0xa90c, 0x0c},
	{0xa90d, 0x0c},
	{0xa910, 0x0c},
	{0xa911, 0x0c},
	{0xa914, 0x0c},
	{0xa915, 0x0c},
	{0xa918, 0x0c},
	{0xa919, 0x0c},
	{0xa91c, 0x0c},
	{0xa91d, 0x0c},
	{0xa920, 0x0c},
	{0xa921, 0x0c},
	{0xa924, 0x0c},
	{0xa925, 0x0c},
	{0xa928, 0x1f},
	{0xa929, 0x1f},
	{0xa92c, 0x1f},
	{0xa92d, 0x1f},
	{0xa930, 0x1f},
	{0xa931, 0x1f},
	{0xa934, 0x1f},
	{0xa935, 0x0c},
	{0xa938, 0x0c},
	{0xa939, 0x0c},
	{0xa93c, 0x0c},
	{0xa93d, 0x0c},
	{0xa940, 0x0c},
	{0xa941, 0x0c},
	{0xa96c, 0x0d},
	{0xa96d, 0x16},
	{0xa970, 0x19},
	{0xa971, 0x0e},
	{0xa974, 0x16},
	{0xa975, 0x1a},
	{0xa978, 0x0d},
	{0xa979, 0x15},
	{0xa97c, 0x19},
	{0xa97d, 0x0d},
	{0xa980, 0x15},
	{0xa981, 0x1a},
	{0xa984, 0x0d},
	{0xa985, 0x15},
	{0xa988, 0x1a},
	{0xa989, 0x0d},
	{0xa98c, 0x15},
	{0xa98d, 0x1a},
	{0xa990, 0x0b},
	{0xa991, 0x11},
	{0xa994, 0x02},
	{0xa995, 0x0e},
	{0xa998, 0x16},
	{0xa999, 0x02},
	{0xa99c, 0x0c},
	{0xa99d, 0x13},
	{0xa9a0, 0x02},
	{0xa9a1, 0x0c},
	{0xa9a4, 0x12},
	{0xa9a5, 0x02},
	{0xa9a8, 0x0c},
	{0xa9a9, 0x12},
	{0xa9ac, 0x02},
	{0xa9ad, 0x0c},
	{0xa9b0, 0x12},
	{0xa9b1, 0x02},
	{0xa9b4, 0x10},
	{0xa9b5, 0x1e},
	{0xa9b8, 0x0f},
	{0xa9b9, 0x13},
	{0xa9bc, 0x20},
	{0xa9bd, 0x10},
	{0xa9c0, 0x11},
	{0xa9c1, 0x1e},
	{0xa9c4, 0x10},
	{0xa9c5, 0x11},
	{0xa9c8, 0x1e},
	{0xa9c9, 0x10},
	{0xa9cc, 0x11},
	{0xa9cd, 0x20},
	{0xa9d0, 0x10},
	{0xa9d1, 0x13},
	{0xa9d4, 0x24},
	{0xa9d5, 0x10},
	{0xa9f0, 0x02},
	{0xa9f1, 0x01},
	{0xa9f8, 0x19},
	{0xa9f9, 0x0b},
	{0xa9fc, 0x0a},
	{0xa9fd, 0x07},
	{0xaa00, 0x0c},
	{0xaa01, 0x0e},
	{0xaa08, 0x0c},
	{0xaa09, 0x06},
	{0xaa0c, 0x0c},
	{0xaa0d, 0x0a},
	{0xaa24, 0x10},
	{0xaa25, 0x12},
	{0xaa28, 0x0b},
	{0xaa29, 0x07},
	{0xaa2c, 0x10},
	{0xaa2d, 0x14},
	{0xaa34, 0x0e},
	{0xaa35, 0x0e},
	{0xaa38, 0x07},
	{0xaa39, 0x07},
	{0xaa3c, 0x0e},
	{0xaa3d, 0x0c},
	{0xaa48, 0x09},
	{0xaa49, 0x0c},
	{0xaa4c, 0x0c},
	{0xaa4d, 0x07},
	{0xaa54, 0x08},
	{0xaa55, 0x06},
	{0xaa58, 0x04},
	{0xaa59, 0x05},
	{0xaa5c, 0x06},
	{0xaa5d, 0x06},
	{0xaa68, 0x05},
	{0xaa69, 0x05},
	{0xaa6c, 0x04},
	{0xaa6d, 0x05},
	{0xaa74, 0x06},
	{0xaa75, 0x04},
	{0xaa78, 0x05},
	{0xaa79, 0x05},
	{0xaa7c, 0x04},
	{0xaa7d, 0x06},
	{0xac18, 0x14},
	{0xac19, 0x00},
	{0xac1c, 0x14},
	{0xac1d, 0x00},
	{0xac20, 0x14},
	{0xac21, 0x00},
	{0xac24, 0x14},
	{0xac25, 0x00},
	{0xac28, 0x14},
	{0xac29, 0x00},
	{0xac2c, 0x14},
	{0xac2d, 0x00},
	{0xac34, 0x16},
	{0xac35, 0x00},
	{0xac38, 0x16},
	{0xac39, 0x00},
	{0xac3c, 0x16},
	{0xac3d, 0x00},
	{0xac40, 0x16},
	{0xac41, 0x00},
	{0xac44, 0x16},
	{0xac45, 0x00},
	{0xac48, 0x16},
	{0xac49, 0x00},
	{0xac50, 0x1b},
	{0xac51, 0x00},
	{0xac54, 0x1b},
	{0xac55, 0x00},
	{0xac58, 0x1b},
	{0xac59, 0x00},
	{0xac5c, 0x1b},
	{0xac5d, 0x00},
	{0xac60, 0x1b},
	{0xac61, 0x00},
	{0xac64, 0x1b},
	{0xac65, 0x00},
	{0xac74, 0x09},
	{0xac75, 0x0c},
	{0xac78, 0x0f},
	{0xac79, 0x11},
	{0xac7c, 0x12},
	{0xac7d, 0x14},
	{0xac80, 0x09},
	{0xac81, 0x0c},
	{0xac84, 0x0f},
	{0xac85, 0x11},
	{0xac88, 0x12},
	{0xac89, 0x14},
	{0xac8c, 0x09},
	{0xac8d, 0x0c},
	{0xac90, 0x0f},
	{0xac91, 0x11},
	{0xac94, 0x12},
	{0xac95, 0x14},
	{0xac98, 0x09},
	{0xac99, 0x0c},
	{0xac9c, 0x0f},
	{0xac9d, 0x11},
	{0xaca0, 0x12},
	{0xaca1, 0x14},
	{0xaca4, 0x09},
	{0xaca5, 0x0c},
	{0xaca8, 0x0f},
	{0xaca9, 0x11},
	{0xacac, 0x12},
	{0xacad, 0x14},
	{0xacb0, 0x07},
	{0xacb1, 0x09},
	{0xacb4, 0x0c},
	{0xacb5, 0x0d},
	{0xacb8, 0x0d},
	{0xacb9, 0x0e},
	{0xacbc, 0x05},
	{0xacbd, 0x07},
	{0xacc0, 0x0a},
	{0xacc1, 0x0b},
	{0xacc4, 0x0b},
	{0xacc5, 0x0c},
	{0xacc8, 0x03},
	{0xacc9, 0x04},
	{0xaccc, 0x07},
	{0xaccd, 0x08},
	{0xacd0, 0x09},
	{0xacd1, 0x09}
};

static struct vx6953_i2c_reg_conf patch_tbl_cut3[] = {
	{0xF800, 0x90},
	{0xF801, 0x30},
	{0xF802, 0x31},
	{0xF803, 0xe0},
	{0xF804, 0xf5},
	{0xF805, 0x7d},
	{0xF806, 0xb4},
	{0xF807, 0x01},
	{0xF808, 0x06},
	{0xF809, 0x75},
	{0xF80A, 0x7d},
	{0xF80B, 0x03},
	{0xF80C, 0x74},
	{0xF80D, 0x03},
	{0xF80E, 0xf0},
	{0xF80F, 0x90},
	{0xF810, 0x30},
	{0xF811, 0x04},
	{0xF812, 0x74},
	{0xF813, 0x33},
	{0xF814, 0xf0},
	{0xF815, 0x90},
	{0xF816, 0x30},
	{0xF817, 0x06},
	{0xF818, 0xe4},
	{0xF819, 0xf0},
	{0xF81A, 0xa3},
	{0xF81B, 0x74},
	{0xF81C, 0x09},
	{0xF81D, 0xf0},
	{0xF81E, 0x90},
	{0xF81F, 0x30},
	{0xF820, 0x10},
	{0xF821, 0xe4},
	{0xF822, 0xf0},
	{0xF823, 0xa3},
	{0xF824, 0xf0},
	{0xF825, 0x90},
	{0xF826, 0x30},
	{0xF827, 0x16},
	{0xF828, 0x74},
	{0xF829, 0x1e},
	{0xF82A, 0xf0},
	{0xF82B, 0x90},
	{0xF82C, 0x30},
	{0xF82D, 0x1a},
	{0xF82E, 0x74},
	{0xF82F, 0x6a},
	{0xF830, 0xf0},
	{0xF831, 0xa3},
	{0xF832, 0x74},
	{0xF833, 0x29},
	{0xF834, 0xf0},
	{0xF835, 0x90},
	{0xF836, 0x30},
	{0xF837, 0x30},
	{0xF838, 0x74},
	{0xF839, 0x08},
	{0xF83A, 0xf0},
	{0xF83B, 0x90},
	{0xF83C, 0x30},
	{0xF83D, 0x36},
	{0xF83E, 0x74},
	{0xF83F, 0x2c},
	{0xF840, 0xf0},
	{0xF841, 0x90},
	{0xF842, 0x30},
	{0xF843, 0x41},
	{0xF844, 0xe4},
	{0xF845, 0xf0},
	{0xF846, 0xa3},
	{0xF847, 0x74},
	{0xF848, 0x24},
	{0xF849, 0xf0},
	{0xF84A, 0x90},
	{0xF84B, 0x30},
	{0xF84C, 0x45},
	{0xF84D, 0x74},
	{0xF84E, 0x81},
	{0xF84F, 0xf0},
	{0xF850, 0x90},
	{0xF851, 0x30},
	{0xF852, 0x98},
	{0xF853, 0x74},
	{0xF854, 0x01},
	{0xF855, 0xf0},
	{0xF856, 0x90},
	{0xF857, 0x30},
	{0xF858, 0x9d},
	{0xF859, 0x74},
	{0xF85A, 0x05},
	{0xF85B, 0xf0},
	{0xF85C, 0xe5},
	{0xF85D, 0x7d},
	{0xF85E, 0x70},
	{0xF85F, 0x10},
	{0xF860, 0x90},
	{0xF861, 0x30},
	{0xF862, 0x05},
	{0xF863, 0x04},
	{0xF864, 0xf0},
	{0xF865, 0x90},
	{0xF866, 0x30},
	{0xF867, 0x30},
	{0xF868, 0xe4},
	{0xF869, 0xf0},
	{0xF86A, 0x90},
	{0xF86B, 0x30},
	{0xF86C, 0x35},
	{0xF86D, 0x04},
	{0xF86E, 0xf0},
	{0xF86F, 0x22},
	{0xF870, 0xe5},
	{0xF871, 0x7d},
	{0xF872, 0x64},
	{0xF873, 0x02},
	{0xF874, 0x70},
	{0xF875, 0x2d},
	{0xF876, 0x90},
	{0xF877, 0x30},
	{0xF878, 0x04},
	{0xF879, 0x74},
	{0xF87A, 0x34},
	{0xF87B, 0xf0},
	{0xF87C, 0xa3},
	{0xF87D, 0x74},
	{0xF87E, 0x07},
	{0xF87F, 0xf0},
	{0xF880, 0x90},
	{0xF881, 0x30},
	{0xF882, 0x10},
	{0xF883, 0x74},
	{0xF884, 0x10},
	{0xF885, 0xf0},
	{0xF886, 0x90},
	{0xF887, 0x30},
	{0xF888, 0x16},
	{0xF889, 0x74},
	{0xF88A, 0x1f},
	{0xF88B, 0xf0},
	{0xF88C, 0x90},
	{0xF88D, 0x30},
	{0xF88E, 0x1a},
	{0xF88F, 0x74},
	{0xF890, 0x62},
	{0xF891, 0xf0},
	{0xF892, 0x90},
	{0xF893, 0x30},
	{0xF894, 0x35},
	{0xF895, 0x74},
	{0xF896, 0x04},
	{0xF897, 0xf0},
	{0xF898, 0x90},
	{0xF899, 0x30},
	{0xF89A, 0x41},
	{0xF89B, 0x74},
	{0xF89C, 0x60},
	{0xF89D, 0xf0},
	{0xF89E, 0xa3},
	{0xF89F, 0x74},
	{0xF8A0, 0x64},
	{0xF8A1, 0xf0},
	{0xF8A2, 0x22},
	{0xF8A3, 0xe5},
	{0xF8A4, 0x7d},
	{0xF8A5, 0xb4},
	{0xF8A6, 0x03},
	{0xF8A7, 0x12},
	{0xF8A8, 0x90},
	{0xF8A9, 0x30},
	{0xF8AA, 0x05},
	{0xF8AB, 0x74},
	{0xF8AC, 0x03},
	{0xF8AD, 0xf0},
	{0xF8AE, 0x90},
	{0xF8AF, 0x30},
	{0xF8B0, 0x11},
	{0xF8B1, 0x74},
	{0xF8B2, 0x01},
	{0xF8B3, 0xf0},
	{0xF8B4, 0x90},
	{0xF8B5, 0x30},
	{0xF8B6, 0x35},
	{0xF8B7, 0x74},
	{0xF8B8, 0x03},
	{0xF8B9, 0xf0},
	{0xF8BA, 0x22},
	{0xF8BB, 0xc3},
	{0xF8BC, 0x90},
	{0xF8BD, 0x0b},
	{0xF8BE, 0x89},
	{0xF8BF, 0xe0},
	{0xF8C0, 0x94},
	{0xF8C1, 0x1e},
	{0xF8C2, 0x90},
	{0xF8C3, 0x0b},
	{0xF8C4, 0x88},
	{0xF8C5, 0xe0},
	{0xF8C6, 0x94},
	{0xF8C7, 0x00},
	{0xF8C8, 0x50},
	{0xF8C9, 0x06},
	{0xF8CA, 0x7e},
	{0xF8CB, 0x00},
	{0xF8CC, 0x7f},
	{0xF8CD, 0x01},
	{0xF8CE, 0x80},
	{0xF8CF, 0x3d},
	{0xF8D0, 0xc3},
	{0xF8D1, 0x90},
	{0xF8D2, 0x0b},
	{0xF8D3, 0x89},
	{0xF8D4, 0xe0},
	{0xF8D5, 0x94},
	{0xF8D6, 0x3c},
	{0xF8D7, 0x90},
	{0xF8D8, 0x0b},
	{0xF8D9, 0x88},
	{0xF8DA, 0xe0},
	{0xF8DB, 0x94},
	{0xF8DC, 0x00},
	{0xF8DD, 0x50},
	{0xF8DE, 0x06},
	{0xF8DF, 0x7e},
	{0xF8E0, 0x00},
	{0xF8E1, 0x7f},
	{0xF8E2, 0x02},
	{0xF8E3, 0x80},
	{0xF8E4, 0x28},
	{0xF8E5, 0xc3},
	{0xF8E6, 0x90},
	{0xF8E7, 0x0b},
	{0xF8E8, 0x89},
	{0xF8E9, 0xe0},
	{0xF8EA, 0x94},
	{0xF8EB, 0xfa},
	{0xF8EC, 0x90},
	{0xF8ED, 0x0b},
	{0xF8EE, 0x88},
	{0xF8EF, 0xe0},
	{0xF8F0, 0x94},
	{0xF8F1, 0x00},
	{0xF8F2, 0x50},
	{0xF8F3, 0x06},
	{0xF8F4, 0x7e},
	{0xF8F5, 0x00},
	{0xF8F6, 0x7f},
	{0xF8F7, 0x03},
	{0xF8F8, 0x80},
	{0xF8F9, 0x13},
	{0xF8FA, 0xc3},
	{0xF8FB, 0x90},
	{0xF8FC, 0x0b},
	{0xF8FD, 0x88},
	{0xF8FE, 0xe0},
	{0xF8FF, 0x94},
	{0xF900, 0x80},
	{0xF901, 0x50},
	{0xF902, 0x06},
	{0xF903, 0x7e},
	{0xF904, 0x00},
	{0xF905, 0x7f},
	{0xF906, 0x04},
	{0xF907, 0x80},
	{0xF908, 0x04},
	{0xF909, 0xae},
	{0xF90A, 0x7e},
	{0xF90B, 0xaf},
	{0xF90C, 0x7f},
	{0xF90D, 0x90},
	{0xF90E, 0xa0},
	{0xF90F, 0xf8},
	{0xF910, 0xee},
	{0xF911, 0xf0},
	{0xF912, 0xa3},
	{0xF913, 0xef},
	{0xF914, 0xf0},
	{0xF915, 0x22},
	{0xF916, 0x90},
	{0xF917, 0x33},
	{0xF918, 0x82},
	{0xF919, 0xe0},
	{0xF91A, 0xff},
	{0xF91B, 0x64},
	{0xF91C, 0x01},
	{0xF91D, 0x70},
	{0xF91E, 0x30},
	{0xF91F, 0xe5},
	{0xF920, 0x7f},
	{0xF921, 0x64},
	{0xF922, 0x02},
	{0xF923, 0x45},
	{0xF924, 0x7e},
	{0xF925, 0x70},
	{0xF926, 0x04},
	{0xF927, 0x7d},
	{0xF928, 0x1e},
	{0xF929, 0x80},
	{0xF92A, 0x1d},
	{0xF92B, 0xe5},
	{0xF92C, 0x7f},
	{0xF92D, 0x64},
	{0xF92E, 0x03},
	{0xF92F, 0x45},
	{0xF930, 0x7e},
	{0xF931, 0x70},
	{0xF932, 0x04},
	{0xF933, 0x7d},
	{0xF934, 0x3c},
	{0xF935, 0x80},
	{0xF936, 0x11},
	{0xF937, 0xe5},
	{0xF938, 0x7f},
	{0xF939, 0x64},
	{0xF93A, 0x04},
	{0xF93B, 0x45},
	{0xF93C, 0x7e},
	{0xF93D, 0x70},
	{0xF93E, 0x04},
	{0xF93F, 0x7d},
	{0xF940, 0xfa},
	{0xF941, 0x80},
	{0xF942, 0x05},
	{0xF943, 0x90},
	{0xF944, 0x33},
	{0xF945, 0x81},
	{0xF946, 0xe0},
	{0xF947, 0xfd},
	{0xF948, 0xae},
	{0xF949, 0x05},
	{0xF94A, 0x90},
	{0xF94B, 0x33},
	{0xF94C, 0x81},
	{0xF94D, 0xed},
	{0xF94E, 0xf0},
	{0xF94F, 0xef},
	{0xF950, 0xb4},
	{0xF951, 0x01},
	{0xF952, 0x10},
	{0xF953, 0x90},
	{0xF954, 0x01},
	{0xF955, 0x00},
	{0xF956, 0xe0},
	{0xF957, 0x60},
	{0xF958, 0x0a},
	{0xF959, 0x90},
	{0xF95A, 0xa1},
	{0xF95B, 0x10},
	{0xF95C, 0xe0},
	{0xF95D, 0xf5},
	{0xF95E, 0x7e},
	{0xF95F, 0xa3},
	{0xF960, 0xe0},
	{0xF961, 0xf5},
	{0xF962, 0x7f},
	{0xF963, 0x22},
	{0xF964, 0x12},
	{0xF965, 0x2f},
	{0xF966, 0x4d},
	{0xF967, 0x90},
	{0xF968, 0x35},
	{0xF969, 0x38},
	{0xF96A, 0xe0},
	{0xF96B, 0x70},
	{0xF96C, 0x05},
	{0xF96D, 0x12},
	{0xF96E, 0x00},
	{0xF96F, 0x0e},
	{0xF970, 0x80},
	{0xF971, 0x03},
	{0xF972, 0x12},
	{0xF973, 0x07},
	{0xF974, 0xc9},
	{0xF975, 0x90},
	{0xF976, 0x40},
	{0xF977, 0x06},
	{0xF978, 0xe0},
	{0xF979, 0xf4},
	{0xF97A, 0x54},
	{0xF97B, 0x02},
	{0xF97C, 0xff},
	{0xF97D, 0xe0},
	{0xF97E, 0x54},
	{0xF97F, 0x01},
	{0xF980, 0x4f},
	{0xF981, 0x90},
	{0xF982, 0x31},
	{0xF983, 0x32},
	{0xF984, 0xf0},
	{0xF985, 0x90},
	{0xF986, 0xfa},
	{0xF987, 0x9d},
	{0xF988, 0xe0},
	{0xF989, 0x70},
	{0xF98A, 0x03},
	{0xF98B, 0x12},
	{0xF98C, 0x27},
	{0xF98D, 0x27},
	{0xF98E, 0x02},
	{0xF98F, 0x05},
	{0xF990, 0xac},
	{0xF991, 0x22},
	{0xF992, 0x78},
	{0xF993, 0x07},
	{0xF994, 0xe6},
	{0xF995, 0xf5},
	{0xF996, 0x7c},
	{0xF997, 0xe5},
	{0xF998, 0x7c},
	{0xF999, 0x60},
	{0xF99A, 0x1d},
	{0xF99B, 0x90},
	{0xF99C, 0x43},
	{0xF99D, 0x83},
	{0xF99E, 0xe0},
	{0xF99F, 0xb4},
	{0xF9A0, 0x01},
	{0xF9A1, 0x16},
	{0xF9A2, 0x90},
	{0xF9A3, 0x43},
	{0xF9A4, 0x87},
	{0xF9A5, 0xe0},
	{0xF9A6, 0xb4},
	{0xF9A7, 0x01},
	{0xF9A8, 0x0f},
	{0xF9A9, 0x15},
	{0xF9AA, 0x7c},
	{0xF9AB, 0x90},
	{0xF9AC, 0x30},
	{0xF9AD, 0xa1},
	{0xF9AE, 0xe5},
	{0xF9AF, 0x7c},
	{0xF9B0, 0xf0},
	{0xF9B1, 0x90},
	{0xF9B2, 0x30},
	{0xF9B3, 0xa0},
	{0xF9B4, 0x74},
	{0xF9B5, 0x01},
	{0xF9B6, 0xf0},
	{0xF9B7, 0x22},
	{0xF9B8, 0xe4},
	{0xF9B9, 0x90},
	{0xF9BA, 0x30},
	{0xF9BB, 0xa0},
	{0xF9BC, 0xf0},
	{0xF9BD, 0x22},
	{0xF9BE, 0xf0},
	{0xF9BF, 0xe5},
	{0xF9C0, 0x3a},
	{0xF9C1, 0xb4},
	{0xF9C2, 0x06},
	{0xF9C3, 0x06},
	{0xF9C4, 0x63},
	{0xF9C5, 0x3e},
	{0xF9C6, 0x02},
	{0xF9C7, 0x12},
	{0xF9C8, 0x03},
	{0xF9C9, 0xea},
	{0xF9CA, 0x02},
	{0xF9CB, 0x17},
	{0xF9CC, 0x4a},
	{0xF9CD, 0x22},
	{0x35C9, 0xBB},
	{0x35CA, 0x01},
	{0x35CB, 0x16},
	{0x35CC, 0x01},
	{0x35CD, 0x64},
	{0x35CE, 0x01},
	{0x35CF, 0x92},
	{0x35D0, 0x01},
	{0x35D1, 0xBE},
	{0x35D3, 0xF6},
	{0x35D5, 0x07},
	{0x35D7, 0xA3},
	{0x35DB, 0x02},
	{0x35DD, 0x06},
	{0x35DF, 0x1B},
	{0x35E6, 0x28},
	{0x35E7, 0x76},
	{0x35E8, 0x2D},
	{0x35E9, 0x07},
	{0x35EA, 0x04},
	{0x35EB, 0x43},
	{0x35EC, 0x05},
	{0x35ED, 0xA9},
	{0x35EE, 0x2A},
	{0x35EF, 0x15},
	{0x35F0, 0x17},
	{0x35F1, 0x41},
	{0x35F2, 0x24},
	{0x35F3, 0x88},
	{0x35F4, 0x01},
	{0x35F5, 0x54},
	{0x35F6, 0x01},
	{0x35F7, 0x55},
	{0x35F8, 0x2E},
	{0x35F9, 0xF2},
	{0x35FA, 0x06},
	{0x35FB, 0x02},
	{0x35FC, 0x06},
	{0x35FD, 0x03},
	{0x35FE, 0x06},
	{0x35FF, 0x04},
	{0x35C2, 0x1F},
	{0x35C3, 0xFF},
	{0x35C4, 0x1F},
	{0x35C5, 0xC0},
	{0x35C0, 0x01},
};

struct vx6953_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	u16 fmt;
	u16 order;
};

static const struct vx6953_format vx6953_cfmts[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	}
	/* more can be supported, to be added later */
};


/*=============================================================*/

static int vx6953_i2c_rxdata(unsigned short saddr,
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
	if (i2c_transfer(vx6953_client->adapter, msgs, 2) < 0) {
		CDBG("vx6953_i2c_rxdata failed!\n");
		return -EIO;
	}
	return 0;
}
static int32_t vx6953_i2c_txdata(unsigned short saddr,
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
	if (i2c_transfer(vx6953_client->adapter, msg, 1) < 0) {
		CDBG("vx6953_i2c_txdata faild 0x%x\n", vx6953_client->addr);
		return -EIO;
	}

	return 0;
}


static int32_t vx6953_i2c_read(unsigned short raddr,
	unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[2];
	if (!rdata)
		return -EIO;
	memset(buf, 0, sizeof(buf));
	buf[0] = (raddr & 0xFF00) >> 8;
	buf[1] = (raddr & 0x00FF);
	rc = vx6953_i2c_rxdata(vx6953_client->addr>>1, buf, rlen);
	if (rc < 0) {
		CDBG("vx6953_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = (rlen == 2 ? buf[0] << 8 | buf[1] : buf[0]);
	return rc;
}
static int32_t vx6953_i2c_write_b_sensor(unsigned short waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[3];
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;
	CDBG("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, bdata);
	rc = vx6953_i2c_txdata(vx6953_client->addr>>1, buf, 3);
	if (rc < 0) {
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			waddr, bdata);
	}
	return rc;
}
static int32_t vx6953_i2c_write_seq_sensor(unsigned short waddr,
	uint8_t *bdata, uint16_t len)
{
	int32_t rc = -EFAULT;
	unsigned char buf[len+2];
	int i;
	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	for (i = 2; i < len+2; i++)
		buf[i] = *bdata++;
	rc = vx6953_i2c_txdata(vx6953_client->addr>>1, buf, len+2);
	if (rc < 0) {
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			 waddr, bdata[0]);
	}
	return rc;
}

static int32_t vx6953_i2c_write_w_table(struct vx6953_i2c_reg_conf const
					 *reg_conf_tbl, int num)
{
	int i;
	int32_t rc = -EIO;
	for (i = 0; i < num; i++) {
		rc = vx6953_i2c_write_b_sensor(reg_conf_tbl->waddr,
			reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

static void vx6953_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint16_t preview_frame_length_lines, snapshot_frame_length_lines;
	uint16_t preview_line_length_pck, snapshot_line_length_pck;
	uint32_t divider, d1, d2;
	/* Total frame_length_lines and line_length_pck for preview */
	preview_frame_length_lines = VX6953_QTR_SIZE_HEIGHT +
		VX6953_VER_QTR_BLK_LINES;
	preview_line_length_pck = VX6953_QTR_SIZE_WIDTH +
		VX6953_HRZ_QTR_BLK_PIXELS;
	/* Total frame_length_lines and line_length_pck for snapshot */
	snapshot_frame_length_lines = VX6953_FULL_SIZE_HEIGHT +
		VX6953_VER_FULL_BLK_LINES;
	snapshot_line_length_pck = VX6953_FULL_SIZE_WIDTH +
		VX6953_HRZ_FULL_BLK_PIXELS;
	d1 = preview_frame_length_lines * 0x00000400/
		snapshot_frame_length_lines;
	d2 = preview_line_length_pck * 0x00000400/
		snapshot_line_length_pck;
	divider = d1 * d2 / 0x400;
	/*Verify PCLK settings and frame sizes.*/
	*pfps = (uint16_t) (fps * divider / 0x400);
	/* 2 is the ratio of no.of snapshot channels
	to number of preview channels */

}

static uint16_t vx6953_get_prev_lines_pf(void)
{
	if (vx6953_ctrl->prev_res == QTR_SIZE)
		return VX6953_QTR_SIZE_HEIGHT + VX6953_VER_QTR_BLK_LINES;
	else
		return VX6953_FULL_SIZE_HEIGHT + VX6953_VER_FULL_BLK_LINES;

}

static uint16_t vx6953_get_prev_pixels_pl(void)
{
	if (vx6953_ctrl->prev_res == QTR_SIZE)
		return VX6953_QTR_SIZE_WIDTH + VX6953_HRZ_QTR_BLK_PIXELS;
	else
		return VX6953_FULL_SIZE_WIDTH + VX6953_HRZ_FULL_BLK_PIXELS;
}

static uint16_t vx6953_get_pict_lines_pf(void)
{
		if (vx6953_ctrl->pict_res == QTR_SIZE)
			return VX6953_QTR_SIZE_HEIGHT +
				VX6953_VER_QTR_BLK_LINES;
		else
			return VX6953_FULL_SIZE_HEIGHT +
				VX6953_VER_FULL_BLK_LINES;
}

static uint16_t vx6953_get_pict_pixels_pl(void)
{
	if (vx6953_ctrl->pict_res == QTR_SIZE)
		return VX6953_QTR_SIZE_WIDTH +
			VX6953_HRZ_QTR_BLK_PIXELS;
	else
		return VX6953_FULL_SIZE_WIDTH +
			VX6953_HRZ_FULL_BLK_PIXELS;
}

static uint32_t vx6953_get_pict_max_exp_lc(void)
{
	if (vx6953_ctrl->pict_res == QTR_SIZE)
		return (VX6953_QTR_SIZE_HEIGHT +
			VX6953_VER_QTR_BLK_LINES)*24;
	else
		return (VX6953_FULL_SIZE_HEIGHT +
			VX6953_VER_FULL_BLK_LINES)*24;
}

static int32_t vx6953_set_fps(struct fps_cfg	*fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;
	total_lines_per_frame = (uint16_t)((VX6953_QTR_SIZE_HEIGHT +
		VX6953_VER_QTR_BLK_LINES) * vx6953_ctrl->fps_divider/0x400);
	if (vx6953_i2c_write_b_sensor(REG_FRAME_LENGTH_LINES_HI,
		((total_lines_per_frame & 0xFF00) >> 8)) < 0)
		return rc;
	if (vx6953_i2c_write_b_sensor(REG_FRAME_LENGTH_LINES_LO,
		(total_lines_per_frame & 0x00FF)) < 0)
		return rc;
	return rc;
}

static int32_t vx6953_write_exp_gain(uint16_t gain, uint32_t line)
{
	uint16_t line_length_pck, frame_length_lines;
	uint8_t gain_hi, gain_lo;
	uint8_t intg_time_hi, intg_time_lo;
	uint8_t line_length_pck_hi = 0, line_length_pck_lo = 0;
	uint16_t line_length_ratio = 1 * Q8;
	int32_t rc = 0;
	if (vx6953_ctrl->sensormode != SENSOR_SNAPSHOT_MODE) {
		frame_length_lines = VX6953_QTR_SIZE_HEIGHT +
		VX6953_VER_QTR_BLK_LINES;
		line_length_pck = VX6953_QTR_SIZE_WIDTH +
			VX6953_HRZ_QTR_BLK_PIXELS;
		if (line > (frame_length_lines -
			VX6953_STM5M0EDOF_OFFSET)) {
			vx6953_ctrl->fps = (uint16_t) (30 * Q8 *
			(frame_length_lines - VX6953_STM5M0EDOF_OFFSET)/
			line);
		} else {
			vx6953_ctrl->fps = (uint16_t) (30 * Q8);
		}
	} else {
		frame_length_lines = VX6953_FULL_SIZE_HEIGHT +
				VX6953_VER_FULL_BLK_LINES;
		line_length_pck = VX6953_FULL_SIZE_WIDTH +
				VX6953_HRZ_FULL_BLK_PIXELS;
	}
	/* calculate line_length_ratio */
	if ((frame_length_lines - VX6953_STM5M0EDOF_OFFSET) < line) {
		line_length_ratio = (line*Q8) /
			(frame_length_lines - VX6953_STM5M0EDOF_OFFSET);
		line = frame_length_lines - VX6953_STM5M0EDOF_OFFSET;
	} else {
		line_length_ratio = 1*Q8;
	}
	vx6953_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
		GROUPED_PARAMETER_HOLD);
	line_length_pck = (line_length_pck >
		MAX_LINE_LENGTH_PCK) ?
		MAX_LINE_LENGTH_PCK : line_length_pck;
	line_length_pck = (uint16_t) (line_length_pck *
		line_length_ratio/Q8);
	line_length_pck_hi = (uint8_t) ((line_length_pck &
		0xFF00) >> 8);
	line_length_pck_lo = (uint8_t) (line_length_pck &
		0x00FF);
	vx6953_i2c_write_b_sensor(REG_LINE_LENGTH_PCK_HI,
		line_length_pck_hi);
	vx6953_i2c_write_b_sensor(REG_LINE_LENGTH_PCK_LO,
		line_length_pck_lo);
	/* update analogue gain registers */
	gain_hi = (uint8_t) ((gain & 0xFF00) >> 8);
	gain_lo = (uint8_t) (gain & 0x00FF);
	vx6953_i2c_write_b_sensor(REG_ANALOGUE_GAIN_CODE_GLOBAL_LO,
		gain_lo);
	vx6953_i2c_write_b_sensor(REG_DIGITAL_GAIN_GREEN_R_LO, gain_hi);
	vx6953_i2c_write_b_sensor(REG_DIGITAL_GAIN_RED_LO, gain_hi);
	vx6953_i2c_write_b_sensor(REG_DIGITAL_GAIN_BLUE_LO, gain_hi);
	vx6953_i2c_write_b_sensor(REG_DIGITAL_GAIN_GREEN_B_LO, gain_hi);
	CDBG("%s, gain_hi 0x%x, gain_lo 0x%x\n", __func__,
		gain_hi, gain_lo);
	/* update line count registers */
	intg_time_hi = (uint8_t) (((uint16_t)line & 0xFF00) >> 8);
	intg_time_lo = (uint8_t) ((uint16_t)line & 0x00FF);
	vx6953_i2c_write_b_sensor(REG_COARSE_INTEGRATION_TIME_HI,
		intg_time_hi);
	vx6953_i2c_write_b_sensor(REG_COARSE_INTEGRATION_TIME_LO,
		intg_time_lo);
	vx6953_i2c_write_b_sensor(REG_GROUPED_PARAMETER_HOLD,
		GROUPED_PARAMETER_HOLD_OFF);

	return rc;
}

static int32_t vx6953_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;
	rc = vx6953_write_exp_gain(gain, line);
	return rc;
} /* endof vx6953_set_pict_exp_gain*/

static int32_t vx6953_move_focus(int direction,
	int32_t num_steps)
{
	return 0;
}


static int32_t vx6953_set_default_focus(uint8_t af_step)
{
	return 0;
}

static int32_t vx6953_test(enum vx6953_test_mode_t mo)
{
	int32_t rc = 0;
	if (mo == TEST_OFF)
		return rc;
	else {
		/* REG_0x30D8[4] is TESBYPEN: 0: Normal Operation,
		1: Bypass Signal Processing
		REG_0x30D8[5] is EBDMASK: 0:
		Output Embedded data, 1: No output embedded data */
		if (vx6953_i2c_write_b_sensor(REG_TEST_PATTERN_MODE,
			(uint8_t) mo) < 0) {
			return rc;
		}
	}
	return rc;
}

static int vx6953_enable_edof(enum edof_mode_t edof_mode)
{
	int rc = 0;
	if (edof_mode == VX6953_EDOF_ESTIMATION) {
		/* EDof Estimation mode for preview */
		if (vx6953_i2c_write_b_sensor(REG_0x0b80, 0x02) < 0)
			return rc;
		CDBG("VX6953_EDOF_ESTIMATION");
	} else if (edof_mode == VX6953_EDOF_APPLICATION) {
		/* EDof Application mode for Capture */
		if (vx6953_i2c_write_b_sensor(REG_0x0b80, 0x01) < 0)
			return rc;
		CDBG("VX6953_EDOF_APPLICATION");
	} else {
		/* EDOF disabled */
		if (vx6953_i2c_write_b_sensor(REG_0x0b80, 0x00) < 0)
			return rc;
		CDBG("VX6953_EDOF_DISABLE");
	}
	return rc;
}

static int32_t vx6953_patch_for_cut2(void)
{
	int32_t rc = 0;
	rc = vx6953_i2c_write_w_table(patch_tbl_cut2,
		ARRAY_SIZE(patch_tbl_cut2));
	if (rc < 0)
		return rc;

	return rc;
}
static int32_t vx6953_patch_for_cut3(void)
{
	int32_t rc = 0;
	rc = vx6953_i2c_write_w_table(patch_tbl_cut3,
		ARRAY_SIZE(patch_tbl_cut3));
	if (rc < 0)
		return rc;

	return rc;
}
static int32_t vx6953_sensor_setting(int update_type, int rt)
{

	int32_t rc = 0;
	unsigned short frame_cnt;
	struct msm_camera_csi_params vx6953_csi_params;
	if (vx6953_ctrl->sensor_type != VX6953_STM5M0EDOF_CUT_2) {
		switch (update_type) {
		case REG_INIT:
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			struct vx6953_i2c_reg_conf init_tbl[] = {
			{REG_0x0112,
				vx6953_regs.reg_pat_init[0].reg_0x0112},
			{0x6003, 0x01},
			{REG_0x0113,
				vx6953_regs.reg_pat_init[0].reg_0x0113},
			{REG_VT_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				vt_pix_clk_div},
			{REG_PRE_PLL_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				pre_pll_clk_div},
			{REG_PLL_MULTIPLIER,
				vx6953_regs.reg_pat_init[0].
				pll_multiplier},
			{REG_OP_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				op_pix_clk_div},
			{REG_COARSE_INTEGRATION_TIME_HI,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_hi},
			{REG_COARSE_INTEGRATION_TIME_LO,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_lo},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_LO,
				vx6953_regs.reg_pat[rt].
				analogue_gain_code_global},
			{REG_0x3030,
				vx6953_regs.reg_pat_init[0].reg_0x3030},
			/* 953 specific registers */
			{REG_0x0111,
				vx6953_regs.reg_pat_init[0].reg_0x0111},
			{REG_0x0b00,
				vx6953_regs.reg_pat_init[0].reg_0x0b00},
			{REG_0x3001,
				vx6953_regs.reg_pat_init[0].reg_0x3001},
			{REG_0x3004,
				vx6953_regs.reg_pat_init[0].reg_0x3004},
			{0x3006, 0x00},
			{REG_0x3007,
				vx6953_regs.reg_pat_init[0].reg_0x3007},
			{0x301b, 0x29},
			/* DEFCOR settings */
			/*Single Defect Correction Weight DISABLE*/
			{0x0b06,
				vx6953_regs.reg_pat_init[0].reg_0x0b06},
			/*Single_defect_correct_weight = auto*/
			{0x0b07,
				vx6953_regs.reg_pat_init[0].reg_0x0b07},
			/*Dynamic couplet correction ENABLED*/
			{0x0b08,
				vx6953_regs.reg_pat_init[0].reg_0x0b08},
			/*Dynamic couplet correction weight*/
			{0x0b09,
				vx6953_regs.reg_pat_init[0].reg_0x0b09},
			/* Clock Setup */
			/* Tell sensor ext clk is 24MHz*/
			{REG_0x0136,
				vx6953_regs.reg_pat_init[0].reg_0x0136},
			{REG_0x0137,
				vx6953_regs.reg_pat_init[0].reg_0x0137},
			/* The white balance gains must be written
			to the sensor every frame. */
			/* Edof */
			{REG_0x0b83,
				vx6953_regs.reg_pat_init[0].reg_0x0b83},
			{REG_0x0b84,
				vx6953_regs.reg_pat_init[0].reg_0x0b84},
			{REG_0x0b85,
				vx6953_regs.reg_pat_init[0].reg_0x0b85},
			{REG_0x0b88,
				vx6953_regs.reg_pat_init[0].reg_0x0b88},
			{REG_0x0b89,
				vx6953_regs.reg_pat_init[0].reg_0x0b89},
			{REG_0x0b8a,
				vx6953_regs.reg_pat_init[0].reg_0x0b8a},
			/* Mode specific regieters */
			{REG_FRAME_LENGTH_LINES_HI,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_hi},
			{REG_FRAME_LENGTH_LINES_LO,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_lo},
			{REG_LINE_LENGTH_PCK_HI,
				vx6953_regs.reg_pat[rt].
				line_length_pck_hi},
			{REG_LINE_LENGTH_PCK_LO,
				vx6953_regs.reg_pat[rt].
				line_length_pck_lo},
			{REG_0x3005,
				vx6953_regs.reg_pat[rt].reg_0x3005},
			{0x3010,
				vx6953_regs.reg_pat[rt].reg_0x3010},
			{REG_0x3011,
				vx6953_regs.reg_pat[rt].reg_0x3011},
			{REG_0x301a,
				vx6953_regs.reg_pat[rt].reg_0x301a},
			{REG_0x3035,
				vx6953_regs.reg_pat[rt].reg_0x3035},
			{REG_0x3036,
				vx6953_regs.reg_pat[rt].reg_0x3036},
			{REG_0x3041,
				vx6953_regs.reg_pat[rt].reg_0x3041},
			{0x3042,
				vx6953_regs.reg_pat[rt].reg_0x3042},
			{REG_0x3045,
				vx6953_regs.reg_pat[rt].reg_0x3045},
			/*EDOF: Estimation settings for Preview mode
			Application settings for capture mode
			(standard settings - Not tuned) */
			{REG_0x0b80,
				vx6953_regs.reg_pat[rt].reg_0x0b80},
			{REG_0x0900,
				vx6953_regs.reg_pat[rt].reg_0x0900},
			{REG_0x0901,
				vx6953_regs.reg_pat[rt].reg_0x0901},
			{REG_0x0902,
				vx6953_regs.reg_pat[rt].reg_0x0902},
			{REG_0x0383,
				vx6953_regs.reg_pat[rt].reg_0x0383},
			{REG_0x0387,
				vx6953_regs.reg_pat[rt].reg_0x0387},
			/* Change output size / frame rate */
			{REG_0x034c,
				vx6953_regs.reg_pat[rt].reg_0x034c},
			{REG_0x034d,
				vx6953_regs.reg_pat[rt].reg_0x034d},
			{REG_0x034e,
				vx6953_regs.reg_pat[rt].reg_0x034e},
			{REG_0x034f,
				vx6953_regs.reg_pat[rt].reg_0x034f},
			};
			/* reset fps_divider */
			vx6953_ctrl->fps = 30 * Q8;
			/* stop streaming */

			/* Reset everything first */
			if (vx6953_i2c_write_b_sensor(0x103, 0x01) < 0) {
				CDBG("S/W reset failed\n");
				return rc;
			} else
				CDBG("S/W reset successful\n");

			msleep(10);

			CDBG("Init vx6953_sensor_setting standby\n");
			if (vx6953_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STANDBY_MODE) < 0)
				return rc;
			/*vx6953_stm5m0edof_delay_msecs_stdby*/
			msleep(vx6953_stm5m0edof_delay_msecs_stdby);



			vx6953_patch_for_cut3();
			rc = vx6953_i2c_write_w_table(&init_tbl[0],
				ARRAY_SIZE(init_tbl));
			if (rc < 0)
				return rc;

			msleep(vx6953_stm5m0edof_delay_msecs_stdby);

			vx6953_i2c_write_b_sensor(0x0b80, 0x00);
			vx6953_i2c_write_b_sensor(0x3388, 0x03);
			vx6953_i2c_write_b_sensor(0x3640, 0x00);

			rc = vx6953_i2c_write_w_table(&edof_tbl[0],
				ARRAY_SIZE(edof_tbl));
			vx6953_i2c_write_b_sensor(0x3388, 0x00);

		}
		return rc;
		case UPDATE_PERIODIC:
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			struct vx6953_i2c_reg_conf preview_mode_tbl[] = {
			{REG_0x0112,
				vx6953_regs.reg_pat_init[0].reg_0x0112},
			{0x6003, 0x01},
			{REG_0x0113,
				vx6953_regs.reg_pat_init[0].reg_0x0113},
			{REG_VT_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				vt_pix_clk_div},
			{REG_PRE_PLL_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				pre_pll_clk_div},
			{REG_PLL_MULTIPLIER,
				vx6953_regs.reg_pat_init[0].
				pll_multiplier},
			{REG_OP_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				op_pix_clk_div},

			{REG_COARSE_INTEGRATION_TIME_HI,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_hi},
			{REG_COARSE_INTEGRATION_TIME_LO,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_lo},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_LO,
				vx6953_regs.reg_pat[rt].
				analogue_gain_code_global},

			{REG_0x3210, vx6953_regs.reg_pat[rt].reg_0x3210},
			{REG_0x0111, vx6953_regs.reg_pat[rt].reg_0x111},
			{REG_0x3410, vx6953_regs.reg_pat[rt].reg_0x3410},

			{REG_0x3004,
				vx6953_regs.reg_pat_init[0].reg_0x3004},
			{REG_0x3006, 0x00},
			{REG_0x3007,
				vx6953_regs.reg_pat_init[0].reg_0x3007},
			{REG_0x301b, 0x29},
			{REG_0x3036,
				vx6953_regs.reg_pat[rt].reg_0x3036},
			{REG_0x3045, vx6953_regs.reg_pat[rt].reg_0x3045},
			{REG_0x3098, vx6953_regs.reg_pat[rt].reg_0x3098},
			{REG_0x309d, vx6953_regs.reg_pat[rt].reg_0x309D},

			{REG_0x0900, vx6953_regs.reg_pat[rt].reg_0x0900},
			{REG_0x0901, vx6953_regs.reg_pat[rt].reg_0x0901},
			{REG_0x0902, vx6953_regs.reg_pat[rt].reg_0x0902},
			{REG_0x0383, vx6953_regs.reg_pat[rt].reg_0x0383},
			{REG_0x0387, vx6953_regs.reg_pat[rt].reg_0x0387},

			{REG_FRAME_LENGTH_LINES_HI,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_hi},
			{REG_FRAME_LENGTH_LINES_LO,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_lo},
			{REG_LINE_LENGTH_PCK_HI,
				vx6953_regs.reg_pat[rt].
				line_length_pck_hi},
			{REG_LINE_LENGTH_PCK_LO,
				vx6953_regs.reg_pat[rt].
				line_length_pck_lo},
			{REG_0x034c,
				vx6953_regs.reg_pat[rt].reg_0x034c},
			{REG_0x034d,
				vx6953_regs.reg_pat[rt].reg_0x034d},
			{REG_0x034e,
				vx6953_regs.reg_pat[rt].reg_0x034e},
			{REG_0x034f,
				vx6953_regs.reg_pat[rt].reg_0x034f},

			{REG_0x3005, vx6953_regs.reg_pat[rt].reg_0x3005},
			{REG_0x3010, vx6953_regs.reg_pat[rt].reg_0x3010},
			{REG_0x3011, vx6953_regs.reg_pat[rt].reg_0x3011},
			{REG_0x301a, vx6953_regs.reg_pat[rt].reg_0x301a},
			{REG_0x3030, 0x08},
			{REG_0x3035, vx6953_regs.reg_pat[rt].reg_0x3035},
			{REG_0x3041, vx6953_regs.reg_pat[rt].reg_0x3041},
			{0x3042, vx6953_regs.reg_pat[rt].reg_0x3042},

			{0x200, vx6953_regs.reg_pat[rt].reg_0x0200},
			{0x201, vx6953_regs.reg_pat[rt].reg_0x0201},

			{0x0b06,
				vx6953_regs.reg_pat_init[0].reg_0x0b06},
			/*Single_defect_correct_weight = auto*/
			{0x0b07,
				vx6953_regs.reg_pat_init[0].reg_0x0b07},
			/*Dynamic couplet correction ENABLED*/
			{0x0b08,
				vx6953_regs.reg_pat_init[0].reg_0x0b08},
			/*Dynamic couplet correction weight*/
			{0x0b09,
				vx6953_regs.reg_pat_init[0].reg_0x0b09},

			{REG_0x0136,
				vx6953_regs.reg_pat_init[0].reg_0x0136},
			{REG_0x0137,
				vx6953_regs.reg_pat_init[0].reg_0x0137},

			/*EDOF: Estimation settings for Preview mode
			Application settings for capture
			mode(standard settings - Not tuned) */
			{REG_0x0b80, vx6953_regs.reg_pat[rt].reg_0x0b80},
			{REG_0x0b83,
				vx6953_regs.reg_pat_init[0].reg_0x0b83},
			{REG_0x0b84,
				vx6953_regs.reg_pat_init[0].reg_0x0b84},
			{REG_0x0b85,
				vx6953_regs.reg_pat_init[0].reg_0x0b85},
			{REG_0x0b88,
				vx6953_regs.reg_pat_init[0].reg_0x0b88},
			{REG_0x0b89,
				vx6953_regs.reg_pat_init[0].reg_0x0b89},
			{REG_0x0b8a,
				vx6953_regs.reg_pat_init[0].reg_0x0b8a},
			{0x3393, 0x06}, /* man_spec_edof_ctrl_edof*/
			{0x3394, 0x07}, /* man_spec_edof_ctrl_edof*/
			};

			struct vx6953_i2c_reg_conf snapshot_mode_tbl[] = {
			{REG_MODE_SELECT,	MODE_SELECT_STANDBY_MODE},
			{REG_0x0112,
				vx6953_regs.reg_pat_init[0].reg_0x0112},
			{0x6003, 0x01},
			{REG_0x0113,
				vx6953_regs.reg_pat_init[0].reg_0x0113},
			{REG_VT_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				vt_pix_clk_div},
			{0x303,	1}, /* VT_SYS_CLK_DIV */
			{REG_PRE_PLL_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				pre_pll_clk_div},
			{REG_PLL_MULTIPLIER,
				vx6953_regs.reg_pat_init[0].
				pll_multiplier},
			{REG_OP_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				op_pix_clk_div},
			{0x30b,	1},
			{REG_COARSE_INTEGRATION_TIME_HI,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_hi},
			{REG_COARSE_INTEGRATION_TIME_LO,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_lo},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_LO,
				vx6953_regs.reg_pat[rt].
				analogue_gain_code_global},
			{REG_LINE_LENGTH_PCK_HI,
				vx6953_regs.reg_pat[rt].
				line_length_pck_hi},
			{REG_LINE_LENGTH_PCK_LO,
				vx6953_regs.reg_pat[rt].
				line_length_pck_lo},
			{REG_FRAME_LENGTH_LINES_HI,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_hi},
			{REG_FRAME_LENGTH_LINES_LO,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_lo},
			{REG_0x3210, vx6953_regs.reg_pat[rt].reg_0x3210},
			{REG_0x0111, vx6953_regs.reg_pat[rt].reg_0x111},

			{REG_0x0b00,
				vx6953_regs.reg_pat_init[0].reg_0x0b00},
			{0x3140, 0x01},  /* AV2X2 block enabled */
			{REG_0x3410, vx6953_regs.reg_pat[rt].reg_0x3410},
			{0x0b06,
				vx6953_regs.reg_pat_init[0].reg_0x0b06},
			/*Single_defect_correct_weight = auto*/
			{0x0b07,
				vx6953_regs.reg_pat_init[0].reg_0x0b07},
			/*Dynamic couplet correction ENABLED*/
			{0x0b08,
				vx6953_regs.reg_pat_init[0].reg_0x0b08},
			/*Dynamic couplet correction weight*/
			{0x0b09,
				vx6953_regs.reg_pat_init[0].reg_0x0b09},


			{REG_0x3004,
				vx6953_regs.reg_pat_init[0].reg_0x3004},
			{REG_0x3006, 0x00},
			{REG_0x3007,
				vx6953_regs.reg_pat_init[0].reg_0x3007},
			{0x301A, 0x6A},
			{REG_0x301b, 0x29},
			{REG_0x3036,
				vx6953_regs.reg_pat[rt].reg_0x3036},
			{REG_0x3045, vx6953_regs.reg_pat[rt].reg_0x3045},
			{REG_0x3098, vx6953_regs.reg_pat[rt].reg_0x3098},
			{REG_0x309d, vx6953_regs.reg_pat[rt].reg_0x309D},

			{REG_0x0136,
				vx6953_regs.reg_pat_init[0].reg_0x0136},
			{REG_0x0137,
				vx6953_regs.reg_pat_init[0].reg_0x0137},

			{REG_0x0b80, vx6953_regs.reg_pat[rt].reg_0x0b80},
			{REG_0x0b83,
				vx6953_regs.reg_pat_init[0].reg_0x0b83},
			{REG_0x0b84,
				vx6953_regs.reg_pat_init[0].reg_0x0b84},
			{REG_0x0b85,
				vx6953_regs.reg_pat_init[0].reg_0x0b85},
			{REG_0x0b88,
				vx6953_regs.reg_pat_init[0].reg_0x0b88},
			{REG_0x0b89,
				vx6953_regs.reg_pat_init[0].reg_0x0b89},
			{REG_0x0b8a,
				vx6953_regs.reg_pat_init[0].reg_0x0b8a},
			{0x3393, 0x06}, /* man_spec_edof_ctrl*/
			{0x3394, 0x07}, /* man_spec_edof_ctrl*/
			};
			/* stop streaming */
			msleep(5);

			/* Reset everything first */

			if (vx6953_i2c_write_b_sensor(0x103, 0x01) < 0) {
				CDBG("S/W reset failed\n");
				return rc;
			} else
				CDBG("S/W reset successful\n");

			msleep(10);

			if (vx6953_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STANDBY_MODE) < 0)
				return rc;
			/*vx6953_stm5m0edof_delay_msecs_stdby*/
			msleep(vx6953_stm5m0edof_delay_msecs_stdby);

			vx6953_csi_params.data_format = CSI_8BIT;
			vx6953_csi_params.lane_cnt = 1;
			vx6953_csi_params.lane_assign = 0xe4;
			vx6953_csi_params.dpcm_scheme = 0;
			vx6953_csi_params.settle_cnt = 7;
			rc = msm_camio_csi_config(&vx6953_csi_params);
			if (rc < 0)
				CDBG(" config csi controller failed\n");

			msleep(vx6953_stm5m0edof_delay_msecs_stdby);

			vx6953_patch_for_cut3();

			msleep(vx6953_stm5m0edof_delay_msecs_stdby);

			if (rt == RES_PREVIEW) {
				rc = vx6953_i2c_write_w_table(
					&preview_mode_tbl[0],
					ARRAY_SIZE(preview_mode_tbl));
				if (rc < 0)
					return rc;
			}
			if (rt == RES_CAPTURE) {
				rc = vx6953_i2c_write_w_table(
					&snapshot_mode_tbl[0],
					ARRAY_SIZE(snapshot_mode_tbl));
				if (rc < 0)
					return rc;
			}
			msleep(vx6953_stm5m0edof_delay_msecs_stdby);

			/* Start sensor streaming */
			if (vx6953_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STREAM) < 0)
				return rc;
			msleep(vx6953_stm5m0edof_delay_msecs_stream);
			/* man_spec_edof_ctrl_tune_smooth_lowlight*/
			vx6953_i2c_write_b_sensor(0x338d, 0x08);
			/* man_spec_edof_ctrl_tune_smooth_indoor*/
			vx6953_i2c_write_b_sensor(0x338e, 0x08);
			/* man_spec_edof_ctrl_tune_smooth_outdoor*/
			vx6953_i2c_write_b_sensor(0x338f, 0x00);
			/*Apply Capture FPGA state machine reset*/
			vx6953_i2c_write_b_sensor(0x16, 0x00);
			msleep(100);
			vx6953_i2c_write_b_sensor(0x16, 0x01);

			if (vx6953_i2c_read(0x0005, &frame_cnt, 1) < 0)
				return rc;

			while (frame_cnt == 0xFF) {
				if (vx6953_i2c_read(0x0005, &frame_cnt, 1) < 0)
					return rc;
				CDBG("frame_cnt=%d", frame_cnt);
				msleep(10);
			}
		}
		return rc;
		default:
			return rc;
		}
	} else {
		switch (update_type) {
		case REG_INIT:
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			struct vx6953_i2c_reg_conf init_tbl[] = {
			{REG_0x0112,
				vx6953_regs.reg_pat_init[0].reg_0x0112},
			{REG_0x0113,
				vx6953_regs.reg_pat_init[0].reg_0x0113},
			{REG_VT_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				vt_pix_clk_div},
			{REG_PRE_PLL_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				pre_pll_clk_div},
			{REG_PLL_MULTIPLIER,
				vx6953_regs.reg_pat_init[0].
				pll_multiplier},
			{REG_OP_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				op_pix_clk_div},
			{REG_COARSE_INTEGRATION_TIME_HI,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_hi},
			{REG_COARSE_INTEGRATION_TIME_LO,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_lo},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_LO,
				vx6953_regs.reg_pat[rt].
				analogue_gain_code_global},
			{REG_0x3030,
				vx6953_regs.reg_pat_init[0].reg_0x3030},
			/* 953 specific registers */
			{REG_0x0111,
				vx6953_regs.reg_pat_init[0].reg_0x0111},
			{REG_0x0b00,
				vx6953_regs.reg_pat_init[0].reg_0x0b00},
			{REG_0x3001,
				vx6953_regs.reg_pat_init[0].reg_0x3001},
			{REG_0x3004,
				vx6953_regs.reg_pat_init[0].reg_0x3004},
			{REG_0x3007,
				vx6953_regs.reg_pat_init[0].reg_0x3007},
			{REG_0x3016,
				vx6953_regs.reg_pat_init[0].reg_0x3016},
			{REG_0x301d,
				vx6953_regs.reg_pat_init[0].reg_0x301d},
			{REG_0x317e,
				vx6953_regs.reg_pat_init[0].reg_0x317e},
			{REG_0x317f,
				vx6953_regs.reg_pat_init[0].reg_0x317f},
			{REG_0x3400,
				vx6953_regs.reg_pat_init[0].reg_0x3400},
			/* DEFCOR settings */
			/*Single Defect Correction Weight DISABLE*/
			{0x0b06,
				vx6953_regs.reg_pat_init[0].reg_0x0b06},
			/*Single_defect_correct_weight = auto*/
			{0x0b07,
				vx6953_regs.reg_pat_init[0].reg_0x0b07},
			/*Dynamic couplet correction ENABLED*/
			{0x0b08,
				vx6953_regs.reg_pat_init[0].reg_0x0b08},
			/*Dynamic couplet correction weight*/
			{0x0b09,
				vx6953_regs.reg_pat_init[0].reg_0x0b09},
			/* Clock Setup */
			/* Tell sensor ext clk is 24MHz*/
			{0x0136,
				vx6953_regs.reg_pat_init[0].reg_0x0136},
			{0x0137,
				vx6953_regs.reg_pat_init[0].reg_0x0137},
			/* The white balance gains must be written
			to the sensor every frame. */
			/* Edof */
			{REG_0x0b83,
				vx6953_regs.reg_pat_init[0].reg_0x0b83},
			{REG_0x0b84,
				vx6953_regs.reg_pat_init[0].reg_0x0b84},
			{0x0b85,
				vx6953_regs.reg_pat_init[0].reg_0x0b85},
			{0x0b88,
				vx6953_regs.reg_pat_init[0].reg_0x0b88},
			{0x0b89,
				vx6953_regs.reg_pat_init[0].reg_0x0b89},
			{REG_0x0b8a,
				vx6953_regs.reg_pat_init[0].reg_0x0b8a},
			/* Mode specific regieters */
			{REG_FRAME_LENGTH_LINES_HI,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_hi},
			{REG_FRAME_LENGTH_LINES_LO,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_lo},
			{REG_LINE_LENGTH_PCK_HI,
				vx6953_regs.reg_pat[rt].
				line_length_pck_hi},
			{REG_LINE_LENGTH_PCK_LO,
				vx6953_regs.reg_pat[rt].
				line_length_pck_lo},
			{REG_0x3005,
				vx6953_regs.reg_pat[rt].reg_0x3005},
			{0x3010,
				vx6953_regs.reg_pat[rt].reg_0x3010},
			{REG_0x3011,
				vx6953_regs.reg_pat[rt].reg_0x3011},
			{REG_0x301a,
				vx6953_regs.reg_pat[rt].reg_0x301a},
			{REG_0x3035,
				vx6953_regs.reg_pat[rt].reg_0x3035},
			{REG_0x3036,
				vx6953_regs.reg_pat[rt].reg_0x3036},
			{REG_0x3041,
				vx6953_regs.reg_pat[rt].reg_0x3041},
			{0x3042,
				vx6953_regs.reg_pat[rt].reg_0x3042},
			{REG_0x3045,
				vx6953_regs.reg_pat[rt].reg_0x3045},
			/*EDOF: Estimation settings for Preview mode
			Application settings for capture mode
			(standard settings - Not tuned) */
			{REG_0x0b80,
				vx6953_regs.reg_pat[rt].reg_0x0b80},
			{REG_0x0900,
				vx6953_regs.reg_pat[rt].reg_0x0900},
			{REG_0x0901,
				vx6953_regs.reg_pat[rt].reg_0x0901},
			{REG_0x0902,
				vx6953_regs.reg_pat[rt].reg_0x0902},
			{REG_0x0383,
				vx6953_regs.reg_pat[rt].reg_0x0383},
			{REG_0x0387,
				vx6953_regs.reg_pat[rt].reg_0x0387},
			/* Change output size / frame rate */
			{REG_0x034c,
				vx6953_regs.reg_pat[rt].reg_0x034c},
			{REG_0x034d,
				vx6953_regs.reg_pat[rt].reg_0x034d},
			{REG_0x034e,
				vx6953_regs.reg_pat[rt].reg_0x034e},
			{REG_0x034f,
				vx6953_regs.reg_pat[rt].reg_0x034f},
			{REG_0x1716,
				vx6953_regs.reg_pat[rt].reg_0x1716},
			{REG_0x1717,
				vx6953_regs.reg_pat[rt].reg_0x1717},
			{REG_0x1718,
				vx6953_regs.reg_pat[rt].reg_0x1718},
			{REG_0x1719,
				vx6953_regs.reg_pat[rt].reg_0x1719},
			};
			/* reset fps_divider */
			vx6953_ctrl->fps = 30 * Q8;
			/* stop streaming */

			/* Reset everything first */
			if (vx6953_i2c_write_b_sensor(0x103, 0x01) < 0) {
				CDBG("S/W reset failed\n");
				return rc;
			} else
				CDBG("S/W reset successful\n");

			msleep(10);

			CDBG("Init vx6953_sensor_setting standby\n");
			if (vx6953_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STANDBY_MODE) < 0)
				return rc;
				/*vx6953_stm5m0edof_delay_msecs_stdby*/
			msleep(vx6953_stm5m0edof_delay_msecs_stdby);
			vx6953_patch_for_cut2();
			rc = vx6953_i2c_write_w_table(&init_tbl[0],
				ARRAY_SIZE(init_tbl));
			if (rc < 0)
				return rc;
				msleep(vx6953_stm5m0edof_delay_msecs_stdby);
		}
		return rc;
		case UPDATE_PERIODIC:
		if (rt == RES_PREVIEW || rt == RES_CAPTURE) {
			struct vx6953_i2c_reg_conf init_mode_tbl[] =  {
			{REG_0x0112,
				vx6953_regs.reg_pat_init[0].reg_0x0112},
			{REG_0x0113,
				vx6953_regs.reg_pat_init[0].reg_0x0113},
			{REG_VT_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				vt_pix_clk_div},
			{REG_PRE_PLL_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				pre_pll_clk_div},
			{REG_PLL_MULTIPLIER,
				vx6953_regs.reg_pat_init[0].
				pll_multiplier},
			{REG_OP_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				op_pix_clk_div},
			{REG_COARSE_INTEGRATION_TIME_HI,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_hi},
			{REG_COARSE_INTEGRATION_TIME_LO,
				vx6953_regs.reg_pat[rt].
				coarse_integration_time_lo},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_LO,
				vx6953_regs.reg_pat[rt].
				analogue_gain_code_global},
			{REG_0x3030,
				vx6953_regs.reg_pat_init[0].reg_0x3030},
			/* 953 specific registers */
			{REG_0x0111,
				vx6953_regs.reg_pat_init[0].reg_0x0111},
			{REG_0x0b00,
				vx6953_regs.reg_pat_init[0].reg_0x0b00},
			{REG_0x3001,
				vx6953_regs.reg_pat_init[0].reg_0x3001},
			{REG_0x3004,
				vx6953_regs.reg_pat_init[0].reg_0x3004},
			{REG_0x3007,
				vx6953_regs.reg_pat_init[0].reg_0x3007},
			{REG_0x3016,
				vx6953_regs.reg_pat_init[0].reg_0x3016},
			{REG_0x301d,
				vx6953_regs.reg_pat_init[0].reg_0x301d},
			{REG_0x317e,
				vx6953_regs.reg_pat_init[0].reg_0x317e},
			{REG_0x317f,
				vx6953_regs.reg_pat_init[0].reg_0x317f},
			{REG_0x3400,
				vx6953_regs.reg_pat_init[0].reg_0x3400},
			{0x0b06,
				vx6953_regs.reg_pat_init[0].reg_0x0b06},
			/*Single_defect_correct_weight = auto*/
			{0x0b07,
				vx6953_regs.reg_pat_init[0].reg_0x0b07},
			/*Dynamic couplet correction ENABLED*/
			{0x0b08,
				vx6953_regs.reg_pat_init[0].reg_0x0b08},
			/*Dynamic couplet correction weight*/
			{0x0b09,
				vx6953_regs.reg_pat_init[0].reg_0x0b09},
			/* Clock Setup */
			/* Tell sensor ext clk is 24MHz*/
			{0x0136,
				vx6953_regs.reg_pat_init[0].reg_0x0136},
			{0x0137,
				vx6953_regs.reg_pat_init[0].reg_0x0137},
			/* The white balance gains must be written
			to the sensor every frame. */
			/* Edof */
			{REG_0x0b83,
				vx6953_regs.reg_pat_init[0].reg_0x0b83},
			{REG_0x0b84,
				vx6953_regs.reg_pat_init[0].reg_0x0b84},
			{0x0b85,
				vx6953_regs.reg_pat_init[0].reg_0x0b85},
			{0x0b88,
				vx6953_regs.reg_pat_init[0].reg_0x0b88},
			{0x0b89,
				vx6953_regs.reg_pat_init[0].reg_0x0b89},
			{REG_0x0b8a,
				vx6953_regs.reg_pat_init[0].reg_0x0b8a},
			/* Mode specific regieters */
			{REG_FRAME_LENGTH_LINES_HI,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_hi},
			{REG_FRAME_LENGTH_LINES_LO,
				vx6953_regs.reg_pat[rt].
				frame_length_lines_lo},
			{REG_LINE_LENGTH_PCK_HI,
				vx6953_regs.reg_pat[rt].
				line_length_pck_hi},
			{REG_LINE_LENGTH_PCK_LO,
				vx6953_regs.reg_pat[rt].
				line_length_pck_lo},
			{REG_0x3005,
				vx6953_regs.reg_pat[rt].reg_0x3005},
			{0x3010,
				vx6953_regs.reg_pat[rt].reg_0x3010},
			{REG_0x3011,
				vx6953_regs.reg_pat[rt].reg_0x3011},
			{REG_0x301a,
				vx6953_regs.reg_pat[rt].reg_0x301a},
			{REG_0x3035,
				vx6953_regs.reg_pat[rt].reg_0x3035},
			{REG_0x3036,
				vx6953_regs.reg_pat[rt].reg_0x3036},
			{REG_0x3041,
				vx6953_regs.reg_pat[rt].reg_0x3041},
			{0x3042,
				vx6953_regs.reg_pat[rt].reg_0x3042},
			{REG_0x3045,
				vx6953_regs.reg_pat[rt].reg_0x3045},
			/*EDOF: Estimation settings for Preview mode
			Application settings for capture mode
			(standard settings - Not tuned) */
			{REG_0x0b80,
				vx6953_regs.reg_pat[rt].reg_0x0b80},
			{REG_0x0900,
				vx6953_regs.reg_pat[rt].reg_0x0900},
			{REG_0x0901,
				vx6953_regs.reg_pat[rt].reg_0x0901},
			{REG_0x0902,
				vx6953_regs.reg_pat[rt].reg_0x0902},
			{REG_0x0383,
				vx6953_regs.reg_pat[rt].reg_0x0383},
			{REG_0x0387,
				vx6953_regs.reg_pat[rt].reg_0x0387},
			/* Change output size / frame rate */
			{REG_0x034c,
				vx6953_regs.reg_pat[rt].reg_0x034c},
			{REG_0x034d,
				vx6953_regs.reg_pat[rt].reg_0x034d},
			{REG_0x034e,
				vx6953_regs.reg_pat[rt].reg_0x034e},
			{REG_0x034f,
				vx6953_regs.reg_pat[rt].reg_0x034f},
			{REG_0x1716,
				vx6953_regs.reg_pat[rt].reg_0x1716},
			{REG_0x1717,
				vx6953_regs.reg_pat[rt].reg_0x1717},
			{REG_0x1718,
				vx6953_regs.reg_pat[rt].reg_0x1718},
			{REG_0x1719,
				vx6953_regs.reg_pat[rt].reg_0x1719},
			};
			struct vx6953_i2c_reg_conf mode_tbl[] = {
			{REG_0x0112,
				vx6953_regs.reg_pat_init[0].reg_0x0112},
			{REG_0x0113,
				vx6953_regs.reg_pat_init[0].reg_0x0113},
			{REG_VT_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				vt_pix_clk_div},
			{REG_PRE_PLL_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				pre_pll_clk_div},
			{REG_PLL_MULTIPLIER,
				vx6953_regs.reg_pat_init[0].
				pll_multiplier},
			{REG_OP_PIX_CLK_DIV,
				vx6953_regs.reg_pat_init[0].
				op_pix_clk_div},
		/* Mode specific regieters */
			{REG_FRAME_LENGTH_LINES_HI,
				vx6953_regs.reg_pat[rt].frame_length_lines_hi},
			{REG_FRAME_LENGTH_LINES_LO,
				vx6953_regs.reg_pat[rt].frame_length_lines_lo},
			{REG_LINE_LENGTH_PCK_HI,
				vx6953_regs.reg_pat[rt].line_length_pck_hi},
			{REG_LINE_LENGTH_PCK_LO,
				vx6953_regs.reg_pat[rt].line_length_pck_lo},
			{REG_0x3005, vx6953_regs.reg_pat[rt].reg_0x3005},
			{0x3010, vx6953_regs.reg_pat[rt].reg_0x3010},
			{REG_0x3011, vx6953_regs.reg_pat[rt].reg_0x3011},
			{REG_0x301a, vx6953_regs.reg_pat[rt].reg_0x301a},
			{REG_0x3035, vx6953_regs.reg_pat[rt].reg_0x3035},
			{REG_0x3036, vx6953_regs.reg_pat[rt].reg_0x3036},
			{REG_0x3041, vx6953_regs.reg_pat[rt].reg_0x3041},
			{0x3042, vx6953_regs.reg_pat[rt].reg_0x3042},
			{REG_0x3045, vx6953_regs.reg_pat[rt].reg_0x3045},
			/*EDOF: Estimation settings for Preview mode
			Application settings for capture
			mode(standard settings - Not tuned) */
			{REG_0x0b80, vx6953_regs.reg_pat[rt].reg_0x0b80},
			{REG_0x0900, vx6953_regs.reg_pat[rt].reg_0x0900},
			{REG_0x0901, vx6953_regs.reg_pat[rt].reg_0x0901},
			{REG_0x0902, vx6953_regs.reg_pat[rt].reg_0x0902},
			{REG_0x0383, vx6953_regs.reg_pat[rt].reg_0x0383},
			{REG_0x0387, vx6953_regs.reg_pat[rt].reg_0x0387},
			/* Change output size / frame rate */
			{REG_0x034c, vx6953_regs.reg_pat[rt].reg_0x034c},
			{REG_0x034d, vx6953_regs.reg_pat[rt].reg_0x034d},
			{REG_0x034e, vx6953_regs.reg_pat[rt].reg_0x034e},
			{REG_0x034f, vx6953_regs.reg_pat[rt].reg_0x034f},
			/*{0x200, vx6953_regs.reg_pat[rt].reg_0x0200},
			{0x201, vx6953_regs.reg_pat[rt].reg_0x0201},*/
			{REG_0x1716, vx6953_regs.reg_pat[rt].reg_0x1716},
			{REG_0x1717, vx6953_regs.reg_pat[rt].reg_0x1717},
			{REG_0x1718, vx6953_regs.reg_pat[rt].reg_0x1718},
			{REG_0x1719, vx6953_regs.reg_pat[rt].reg_0x1719},
			};
			/* stop streaming */
			msleep(5);

			/* Reset everything first */
			if (vx6953_i2c_write_b_sensor(0x103, 0x01) < 0) {
				CDBG("S/W reset failed\n");
				return rc;
			} else
				CDBG("S/W reset successful\n");

			msleep(10);

			if (vx6953_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STANDBY_MODE) < 0)
				return rc;
			/*vx6953_stm5m0edof_delay_msecs_stdby*/
			msleep(vx6953_stm5m0edof_delay_msecs_stdby);

			vx6953_csi_params.data_format = CSI_8BIT;
			vx6953_csi_params.lane_cnt = 1;
			vx6953_csi_params.lane_assign = 0xe4;
			vx6953_csi_params.dpcm_scheme = 0;
			vx6953_csi_params.settle_cnt = 7;
			rc = msm_camio_csi_config(&vx6953_csi_params);
			if (rc < 0)
				CDBG(" config csi controller failed\n");

			msleep(vx6953_stm5m0edof_delay_msecs_stdby);

			vx6953_patch_for_cut2();
			rc = vx6953_i2c_write_w_table(&init_mode_tbl[0],
				ARRAY_SIZE(init_mode_tbl));
			if (rc < 0)
				return rc;

			msleep(vx6953_stm5m0edof_delay_msecs_stdby);

			rc = vx6953_i2c_write_w_table(&mode_tbl[0],
				ARRAY_SIZE(mode_tbl));
			if (rc < 0)
				return rc;

			msleep(vx6953_stm5m0edof_delay_msecs_stdby);

			/* Start sensor streaming */
			if (vx6953_i2c_write_b_sensor(REG_MODE_SELECT,
				MODE_SELECT_STREAM) < 0)
				return rc;
			msleep(vx6953_stm5m0edof_delay_msecs_stream);

			if (vx6953_i2c_read(0x0005, &frame_cnt, 1) < 0)
				return rc;

			while (frame_cnt == 0xFF) {
				if (vx6953_i2c_read(0x0005, &frame_cnt, 1) < 0)
					return rc;
				CDBG("frame_cnt=%d", frame_cnt);
				msleep(10);
			}
		}
		return rc;
		default:
		return rc;
	}
	}
	return rc;
}


static int32_t vx6953_video_config(int mode)
{

	int32_t	rc = 0;
	int	rt;
	/* change sensor resolution	if needed */
	if (vx6953_ctrl->prev_res == QTR_SIZE) {
		rt = RES_PREVIEW;
		vx6953_stm5m0edof_delay_msecs_stdby	=
			((((2 * 1000 * vx6953_ctrl->fps_divider) /
			vx6953_ctrl->fps) * Q8) / Q10) + 1;
	} else {
		rt = RES_CAPTURE;
		vx6953_stm5m0edof_delay_msecs_stdby	=
			((((1000 * vx6953_ctrl->fps_divider) /
			vx6953_ctrl->fps) * Q8) / Q10) + 1;
	}
	if (vx6953_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	if (vx6953_ctrl->set_test) {
		if (vx6953_test(vx6953_ctrl->set_test) < 0)
			return	rc;
	}
	vx6953_ctrl->edof_mode = VX6953_EDOF_ESTIMATION;
	rc = vx6953_enable_edof(vx6953_ctrl->edof_mode);
	if (rc < 0)
		return rc;
	vx6953_ctrl->curr_res = vx6953_ctrl->prev_res;
	vx6953_ctrl->sensormode = mode;
	return rc;
}

static int32_t vx6953_snapshot_config(int mode)
{
	int32_t rc = 0;
	int rt;
	/*change sensor resolution if needed */
	if (vx6953_ctrl->curr_res != vx6953_ctrl->pict_res) {
		if (vx6953_ctrl->pict_res == QTR_SIZE) {
			rt = RES_PREVIEW;
			vx6953_stm5m0edof_delay_msecs_stdby =
				((((2 * 1000 * vx6953_ctrl->fps_divider) /
				vx6953_ctrl->fps) * Q8) / Q10) + 1;
		} else {
			rt = RES_CAPTURE;
			vx6953_stm5m0edof_delay_msecs_stdby =
				((((1000 * vx6953_ctrl->fps_divider) /
				vx6953_ctrl->fps) * Q8) / Q10) + 1;
		}
	if (vx6953_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	}

	vx6953_ctrl->edof_mode = VX6953_EDOF_APPLICATION;
	if (vx6953_enable_edof(vx6953_ctrl->edof_mode) < 0)
		return rc;
	vx6953_ctrl->curr_res = vx6953_ctrl->pict_res;
	vx6953_ctrl->sensormode = mode;
	return rc;
} /*end of vx6953_snapshot_config*/

static int32_t vx6953_raw_snapshot_config(int mode)
{
	int32_t rc = 0;
	int rt;
	/* change sensor resolution if needed */
	if (vx6953_ctrl->curr_res != vx6953_ctrl->pict_res) {
		if (vx6953_ctrl->pict_res == QTR_SIZE) {
			rt = RES_PREVIEW;
			vx6953_stm5m0edof_delay_msecs_stdby =
				((((2 * 1000 * vx6953_ctrl->fps_divider)/
				vx6953_ctrl->fps) * Q8) / Q10) + 1;
		} else {
			rt = RES_CAPTURE;
			vx6953_stm5m0edof_delay_msecs_stdby =
				((((1000 * vx6953_ctrl->fps_divider)/
				vx6953_ctrl->fps) * Q8) / Q10) + 1;
		}
		if (vx6953_sensor_setting(UPDATE_PERIODIC, rt) < 0)
			return rc;
	}
	vx6953_ctrl->edof_mode = VX6953_EDOF_APPLICATION;
	if (vx6953_enable_edof(vx6953_ctrl->edof_mode) < 0)
		return rc;
	vx6953_ctrl->curr_res = vx6953_ctrl->pict_res;
	vx6953_ctrl->sensormode = mode;
	return rc;
} /*end of vx6953_raw_snapshot_config*/
static int32_t vx6953_set_sensor_mode(int mode,
	int res)
{
	int32_t rc = 0;
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = vx6953_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
		rc = vx6953_snapshot_config(mode);
		break;
	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = vx6953_raw_snapshot_config(mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}
static int32_t vx6953_power_down(void)
{
	vx6953_i2c_write_b_sensor(REG_MODE_SELECT,
	MODE_SELECT_STANDBY_MODE);
	return 0;
}


static int vx6953_probe_init_done(const struct msm_camera_sensor_info *data)
{
	gpio_free(data->sensor_reset);
	kfree(vx6953_ctrl);
	vx6953_ctrl = NULL;
	return 0;
}
static int vx6953_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	unsigned short revision_number;
	int32_t rc = 0;
	unsigned short chipidl, chipidh;
	CDBG("%s: %d\n", __func__, __LINE__);
	rc = gpio_request(data->sensor_reset, "vx6953");
	CDBG(" vx6953_probe_init_sensor\n");
	if (!rc) {
		CDBG("sensor_reset = %d\n", rc);
		CDBG(" vx6953_probe_init_sensor 1\n");
		gpio_direction_output(data->sensor_reset, 0);
		msleep(50);
		CDBG(" vx6953_probe_init_sensor 1\n");
		gpio_direction_output(data->sensor_reset, 1);
		msleep(13);
	} else {
		CDBG(" vx6953_probe_init_sensor 2\n");
		goto init_probe_done;
	}
	msleep(20);
	CDBG(" vx6953_probe_init_sensor is called\n");
	/* 3. Read sensor Model ID: */
	rc = vx6953_i2c_read(0x0000, &chipidh, 1);
	if (rc < 0) {
		CDBG(" vx6953_probe_init_sensor 3\n");
		goto init_probe_fail;
	}
	rc = vx6953_i2c_read(0x0001, &chipidl, 1);
	if (rc < 0) {
		CDBG(" vx6953_probe_init_sensor4\n");
		goto init_probe_fail;
	}
	CDBG("vx6953 model_id = 0x%x  0x%x\n", chipidh, chipidl);
	/* 4. Compare sensor ID to VX6953 ID: */
	if (chipidh != 0x03 || chipidl != 0xB9) {
		rc = -ENODEV;
		CDBG("vx6953_probe_init_sensor fail chip id doesnot match\n");
		goto init_probe_fail;
	}

	vx6953_ctrl = kzalloc(sizeof(struct vx6953_ctrl_t), GFP_KERNEL);
	if (!vx6953_ctrl) {
		CDBG("vx6953_init failed!\n");
		rc = -ENOMEM;
	}
	vx6953_ctrl->fps_divider = 1 * 0x00000400;
	vx6953_ctrl->pict_fps_divider = 1 * 0x00000400;
	vx6953_ctrl->set_test = TEST_OFF;
	vx6953_ctrl->prev_res = QTR_SIZE;
	vx6953_ctrl->pict_res = FULL_SIZE;
	vx6953_ctrl->curr_res = INVALID_SIZE;
	vx6953_ctrl->sensor_type = VX6953_STM5M0EDOF_CUT_2;
	vx6953_ctrl->edof_mode = VX6953_EDOF_ESTIMATION;

	if (data)
		vx6953_ctrl->sensordata = data;

	if (vx6953_i2c_read(0x0002, &revision_number, 1) < 0)
		return rc;
		CDBG("sensor revision number major = 0x%x\n", revision_number);
	if (vx6953_i2c_read(0x0018, &revision_number, 1) < 0)
		return rc;
		CDBG("sensor revision number = 0x%x\n", revision_number);
	if (revision_number == VX6953_REVISION_NUMBER_CUT3) {
		vx6953_ctrl->sensor_type = VX6953_STM5M0EDOF_CUT_3;
		CDBG("VX6953 EDof Cut 3.0 sensor\n ");
	} else if (revision_number == VX6953_REVISION_NUMBER_CUT2) {
		vx6953_ctrl->sensor_type = VX6953_STM5M0EDOF_CUT_2;
		CDBG("VX6953 EDof Cut 2.0 sensor\n ");
	} else {/* Cut1.0 reads 0x00 for register 0x0018*/
		vx6953_ctrl->sensor_type = VX6953_STM5M0EDOF_CUT_1;
		CDBG("VX6953 EDof Cut 1.0 sensor\n ");
	}

	if (vx6953_ctrl->prev_res == QTR_SIZE) {
		if (vx6953_sensor_setting(REG_INIT, RES_PREVIEW) < 0)
			goto init_probe_fail;
	} else {
		if (vx6953_sensor_setting(REG_INIT, RES_CAPTURE) < 0)
			goto init_probe_fail;
	}

	goto init_probe_done;
init_probe_fail:
	CDBG(" vx6953_probe_init_sensor fails\n");
	gpio_direction_output(data->sensor_reset, 0);
	vx6953_probe_init_done(data);
init_probe_done:
	CDBG(" vx6953_probe_init_sensor finishes\n");
	return rc;
	}
/* camsensor_iu060f_vx6953_reset */
int vx6953_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	unsigned short revision_number;
	int32_t rc = 0;

	CDBG("%s: %d\n", __func__, __LINE__);
	CDBG("Calling vx6953_sensor_open_init\n");
	rc = gpio_request(data->sensor_reset, "vx6953");
	if (!rc)
		CDBG("vx6953 gpio_request fail\n");

	vx6953_ctrl = kzalloc(sizeof(struct vx6953_ctrl_t), GFP_KERNEL);
	if (!vx6953_ctrl) {
		CDBG("vx6953_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	vx6953_ctrl->fps_divider = 1 * 0x00000400;
	vx6953_ctrl->pict_fps_divider = 1 * 0x00000400;
	vx6953_ctrl->set_test = TEST_OFF;
	vx6953_ctrl->prev_res = QTR_SIZE;
	vx6953_ctrl->pict_res = FULL_SIZE;
	vx6953_ctrl->curr_res = INVALID_SIZE;
	vx6953_ctrl->sensor_type = VX6953_STM5M0EDOF_CUT_2;
	vx6953_ctrl->edof_mode = VX6953_EDOF_ESTIMATION;
	if (data)
		vx6953_ctrl->sensordata = data;
	if (rc < 0) {
		CDBG("Calling vx6953_sensor_open_init fail1\n");
		return rc;
	}
	CDBG("%s: %d\n", __func__, __LINE__);
	/* enable mclk first */
	msm_camio_clk_rate_set(VX6953_STM5M0EDOF_DEFAULT_MASTER_CLK_RATE);
	CDBG("%s: %d\n", __func__, __LINE__);
	if (vx6953_i2c_read(0x0002, &revision_number, 1) < 0)
		return rc;
		CDBG("sensor revision number major = 0x%x\n", revision_number);
	if (vx6953_i2c_read(0x0018, &revision_number, 1) < 0)
		return rc;
		CDBG("sensor revision number = 0x%x\n", revision_number);
	if (revision_number == VX6953_REVISION_NUMBER_CUT3) {
		vx6953_ctrl->sensor_type = VX6953_STM5M0EDOF_CUT_3;
		CDBG("VX6953 EDof Cut 3.0 sensor\n ");
	} else if (revision_number == VX6953_REVISION_NUMBER_CUT2) {
		vx6953_ctrl->sensor_type = VX6953_STM5M0EDOF_CUT_2;
		CDBG("VX6953 EDof Cut 2.0 sensor\n ");
	} else {/* Cut1.0 reads 0x00 for register 0x0018*/
		vx6953_ctrl->sensor_type = VX6953_STM5M0EDOF_CUT_1;
		CDBG("VX6953 EDof Cut 1.0 sensor\n ");
	}

	vx6953_ctrl->fps = 30*Q8;
	if (rc < 0)
		goto init_fail;
	else
		goto init_done;
init_fail:
	CDBG("init_fail\n");
	gpio_direction_output(data->sensor_reset, 0);
	vx6953_probe_init_done(data);
init_done:
	CDBG("init_done\n");
	return rc;
} /*endof vx6953_sensor_open_init*/

static int vx6953_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&vx6953_wait_queue);
	return 0;
}

static const struct i2c_device_id vx6953_i2c_id[] = {
	{"vx6953", 0},
	{ }
};

static int vx6953_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("vx6953_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	vx6953_sensorw = kzalloc(sizeof(struct vx6953_work_t), GFP_KERNEL);
	if (!vx6953_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, vx6953_sensorw);
	vx6953_init_client(client);
	vx6953_client = client;

	msleep(50);

	CDBG("vx6953_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	CDBG("vx6953_probe failed! rc = %d\n", rc);
	return rc;
}

static int vx6953_send_wb_info(struct wb_info_cfg *wb)
{
	unsigned short read_data;
	uint8_t temp[8];
	int rc = 0;
	int i = 0;

	/* red_gain */
	temp[2] = wb->red_gain >> 8;
	temp[3] = wb->red_gain & 0xFF;

	/* green_gain */
	temp[0] = wb->green_gain >> 8;
	temp[1] = wb->green_gain & 0xFF;
	temp[6] = temp[0];
	temp[7] = temp[1];

	/* blue_gain */
	temp[4] = wb->blue_gain >> 8;
	temp[5] = wb->blue_gain & 0xFF;
	rc = vx6953_i2c_write_seq_sensor(0x0B8E, &temp[0], 8);

	for (i = 0; i < 6; i++) {
		rc = vx6953_i2c_read(0x0B8E + i, &read_data, 1);
		CDBG("%s addr 0x%x val %d\n", __func__, 0x0B8E + i, read_data);
	}
	rc = vx6953_i2c_read(0x0B82, &read_data, 1);
	CDBG("%s addr 0x%x val %d\n", __func__, 0x0B82, read_data);
	if (rc < 0)
		return rc;
	return rc;
} /*end of vx6953_snapshot_config*/

static int __exit vx6953_remove(struct i2c_client *client)
{
	struct vx6953_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	vx6953_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver vx6953_i2c_driver = {
	.id_table = vx6953_i2c_id,
	.probe  = vx6953_i2c_probe,
	.remove = __exit_p(vx6953_i2c_remove),
	.driver = {
		.name = "vx6953",
	},
};

static int vx6953_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
		(void *)argp,
		sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&vx6953_mut);
	CDBG("vx6953_sensor_config: cfgtype = %d\n",
	cdata.cfgtype);
		switch (cdata.cfgtype) {
		case CFG_GET_PICT_FPS:
			vx6953_get_pict_fps(
				cdata.cfg.gfps.prevfps,
				&(cdata.cfg.gfps.pictfps));

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_L_PF:
			cdata.cfg.prevl_pf =
			vx6953_get_prev_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_P_PL:
			cdata.cfg.prevp_pl =
				vx6953_get_prev_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_L_PF:
			cdata.cfg.pictl_pf =
				vx6953_get_pict_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_P_PL:
			cdata.cfg.pictp_pl =
				vx6953_get_pict_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_MAX_EXP_LC:
			cdata.cfg.pict_max_exp_lc =
				vx6953_get_pict_max_exp_lc();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			rc = vx6953_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				vx6953_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				vx6953_set_pict_exp_gain(
				cdata.cfg.exp_gain.gain,
				cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = vx6953_set_sensor_mode(cdata.mode,
					cdata.rs);
			break;

		case CFG_PWR_DOWN:
			rc = vx6953_power_down();
			break;

		case CFG_MOVE_FOCUS:
			rc =
				vx6953_move_focus(
				cdata.cfg.focus.dir,
				cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				vx6953_set_default_focus(
				cdata.cfg.focus.steps);
			break;

		case CFG_SET_EFFECT:
			rc = vx6953_set_default_focus(
				cdata.cfg.effect);
			break;


		case CFG_SEND_WB_INFO:
			rc = vx6953_send_wb_info(
				&(cdata.cfg.wb_info));
			break;

		default:
			rc = -EFAULT;
			break;
		}

	mutex_unlock(&vx6953_mut);

	return rc;
}




static int vx6953_sensor_release(void)
{
	int rc = -EBADF;
	mutex_lock(&vx6953_mut);
	vx6953_power_down();
	gpio_free(vx6953_ctrl->sensordata->sensor_reset);
	kfree(vx6953_ctrl);
	vx6953_ctrl = NULL;
	CDBG("vx6953_release completed\n");
	mutex_unlock(&vx6953_mut);

	return rc;
}

static int vx6953_g_chip_ident(struct v4l2_subdev *sd,
			struct v4l2_dbg_chip_ident *id)
{
	/* TODO: Need to add this ID in v4l2-chip-ident.h */
	id->ident    = V4L2_IDENT_VX6953;
	id->revision = 0;

	return 0;
}

static int vx6953_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	int ret = 0;
	/* return current mode value */
	param->parm.capture.capturemode = vx6953_ctrl->sensormode;
	return ret;
}

static int vx6953_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	/* set the desired mode */
	/* right now, the only purpose is to set the desired mode -
	 preview or snapshot */
	vx6953_ctrl->sensormode = param->parm.capture.capturemode;
	return 0;
}

static int vx6953_s_stream(struct v4l2_subdev *sd, int enable)
{
	long rc = 0;
	int mode = vx6953_ctrl->sensormode;
	int rt = RES_PREVIEW;
	unsigned short frame_cnt;
	struct msm_camera_csi_params vx6953_csi_params;

	CDBG("mode = %d, enable = %d\n", mode, enable);

	if (!enable) {
		/* turn off streaming */
		/* TODO: Make call to I2C write to turn streaming off */
		/* rc = vx6953_i2c_write_b_sensor(); */

		struct vx6953_i2c_reg_conf init_tbl[] = {
			{REG_0x0112,
			vx6953_regs.reg_pat_init[0].reg_0x0112},
			{0x6003, 0x01},
			{REG_0x0113,
			vx6953_regs.reg_pat_init[0].reg_0x0113},
			{REG_VT_PIX_CLK_DIV,
			vx6953_regs.reg_pat_init[0].
			vt_pix_clk_div},
			{REG_PRE_PLL_CLK_DIV,
			vx6953_regs.reg_pat_init[0].
			pre_pll_clk_div},
			{REG_PLL_MULTIPLIER,
			vx6953_regs.reg_pat_init[0].
			pll_multiplier},
			{REG_OP_PIX_CLK_DIV,
			vx6953_regs.reg_pat_init[0].
			op_pix_clk_div},
			{REG_COARSE_INTEGRATION_TIME_HI,
			vx6953_regs.reg_pat[rt].
			coarse_integration_time_hi},
			{REG_COARSE_INTEGRATION_TIME_LO,
			vx6953_regs.reg_pat[rt].
			coarse_integration_time_lo},
			{REG_ANALOGUE_GAIN_CODE_GLOBAL_LO,
			vx6953_regs.reg_pat[rt].
			analogue_gain_code_global},
			{REG_0x3030,
			vx6953_regs.reg_pat_init[0].reg_0x3030},
			/* 953 specific registers */
			{REG_0x0111,
			vx6953_regs.reg_pat_init[0].reg_0x0111},
			{REG_0x0b00,
			vx6953_regs.reg_pat_init[0].reg_0x0b00},
			{REG_0x3001,
			vx6953_regs.reg_pat_init[0].reg_0x3001},
			{REG_0x3004,
			vx6953_regs.reg_pat_init[0].reg_0x3004},
			{0x3006, 0x00},
			{REG_0x3007,
			vx6953_regs.reg_pat_init[0].reg_0x3007},
			{0x301b, 0x29},
			/* DEFCOR settings */
			/*Single Defect Correction Weight DISABLE*/
			{0x0b06,
			vx6953_regs.reg_pat_init[0].reg_0x0b06},
			/*Single_defect_correct_weight = auto*/
			{0x0b07,
			vx6953_regs.reg_pat_init[0].reg_0x0b07},
			/*Dynamic couplet correction ENABLED*/
			{0x0b08,
			vx6953_regs.reg_pat_init[0].reg_0x0b08},
			/*Dynamic couplet correction weight*/
			{0x0b09,
			vx6953_regs.reg_pat_init[0].reg_0x0b09},
			/* Clock Setup */
			/* Tell sensor ext clk is 24MHz*/
			{REG_0x0136,
			vx6953_regs.reg_pat_init[0].reg_0x0136},
			{REG_0x0137,
			vx6953_regs.reg_pat_init[0].reg_0x0137},
			/* The white balance gains must be written
			 to the sensor every frame. */
			/* Edof */
			{REG_0x0b83,
			vx6953_regs.reg_pat_init[0].reg_0x0b83},
			{REG_0x0b84,
			vx6953_regs.reg_pat_init[0].reg_0x0b84},
			{REG_0x0b85,
			vx6953_regs.reg_pat_init[0].reg_0x0b85},
			{REG_0x0b88,
			vx6953_regs.reg_pat_init[0].reg_0x0b88},
			{REG_0x0b89,
			vx6953_regs.reg_pat_init[0].reg_0x0b89},
			{REG_0x0b8a,
			vx6953_regs.reg_pat_init[0].reg_0x0b8a},
			/* Mode specific regieters */
			{REG_FRAME_LENGTH_LINES_HI,
			vx6953_regs.reg_pat[rt].
			frame_length_lines_hi},
			{REG_FRAME_LENGTH_LINES_LO,
			vx6953_regs.reg_pat[rt].
			frame_length_lines_lo},
			{REG_LINE_LENGTH_PCK_HI,
			vx6953_regs.reg_pat[rt].
			line_length_pck_hi},
			{REG_LINE_LENGTH_PCK_LO,
			vx6953_regs.reg_pat[rt].
			line_length_pck_lo},
			{REG_0x3005,
			vx6953_regs.reg_pat[rt].reg_0x3005},
			{0x3010,
			vx6953_regs.reg_pat[rt].reg_0x3010},
			{REG_0x3011,
			vx6953_regs.reg_pat[rt].reg_0x3011},
			{REG_0x301a,
			vx6953_regs.reg_pat[rt].reg_0x301a},
			{REG_0x3035,
			vx6953_regs.reg_pat[rt].reg_0x3035},
			{REG_0x3036,
			vx6953_regs.reg_pat[rt].reg_0x3036},
			{REG_0x3041,
			vx6953_regs.reg_pat[rt].reg_0x3041},
			{0x3042,
			vx6953_regs.reg_pat[rt].reg_0x3042},
			{REG_0x3045,
			vx6953_regs.reg_pat[rt].reg_0x3045},
			/*EDOF: Estimation settings for Preview mode
			  Application settings for capture mode
			  (standard settings - Not tuned) */
			{REG_0x0b80,
			vx6953_regs.reg_pat[rt].reg_0x0b80},
			{REG_0x0900,
			vx6953_regs.reg_pat[rt].reg_0x0900},
			{REG_0x0901,
			vx6953_regs.reg_pat[rt].reg_0x0901},
			{REG_0x0902,
			vx6953_regs.reg_pat[rt].reg_0x0902},
			{REG_0x0383,
			vx6953_regs.reg_pat[rt].reg_0x0383},
			{REG_0x0387,
			vx6953_regs.reg_pat[rt].reg_0x0387},
			/* Change output size / frame rate */
			{REG_0x034c,
			vx6953_regs.reg_pat[rt].reg_0x034c},
			{REG_0x034d,
			vx6953_regs.reg_pat[rt].reg_0x034d},
			{REG_0x034e,
			vx6953_regs.reg_pat[rt].reg_0x034e},
			{REG_0x034f,
			vx6953_regs.reg_pat[rt].reg_0x034f},
		};
		/* reset fps_divider */
		vx6953_ctrl->fps = 30 * Q8;
		/* stop streaming */

		/* Reset everything first */
		if (vx6953_i2c_write_b_sensor(0x103, 0x01) < 0) {
			CDBG("S/W reset failed\n");
			return rc;
		} else
			CDBG("S/W reset successful\n");

		msleep(10);

		CDBG("Init vx6953_sensor_setting standby\n");
		if (vx6953_i2c_write_b_sensor(REG_MODE_SELECT,
				    MODE_SELECT_STANDBY_MODE) < 0)
			return rc;

		/*vx6953_stm5m0edof_delay_msecs_stdby*/
		msleep(vx6953_stm5m0edof_delay_msecs_stdby);

		vx6953_csi_params.data_format = CSI_8BIT;
		vx6953_csi_params.lane_cnt = 1;
		vx6953_csi_params.lane_assign = 0xe4;
		vx6953_csi_params.dpcm_scheme = 0;
		vx6953_csi_params.settle_cnt = 7;
		rc = msm_camio_csi_config(&vx6953_csi_params);
		if (rc < 0)
			CDBG(" config csi controller failed\n");
		msleep(vx6953_stm5m0edof_delay_msecs_stdby);

		vx6953_patch_for_cut3();
		rc = vx6953_i2c_write_w_table(&init_tbl[0],
					    ARRAY_SIZE(init_tbl));
		if (rc < 0)
			return rc;

		msleep(vx6953_stm5m0edof_delay_msecs_stdby);

		vx6953_i2c_write_b_sensor(0x0b80, 0x00);
		vx6953_i2c_write_b_sensor(0x3388, 0x03);
		vx6953_i2c_write_b_sensor(0x3640, 0x00);
		return rc;
	} else {
		/* Start sensor streaming */
		if (vx6953_i2c_write_b_sensor(REG_MODE_SELECT,
					    MODE_SELECT_STREAM) < 0)
			return rc;
		CDBG("Init vx6953_sensor_setting stream\n");
		msleep(vx6953_stm5m0edof_delay_msecs_stream);
		if (vx6953_i2c_read(0x0005, &frame_cnt, 1) < 0)
			return rc;

		rc = vx6953_i2c_write_w_table(&edof_tbl[0],
					    ARRAY_SIZE(edof_tbl));
		vx6953_i2c_write_b_sensor(0x3388, 0x00);

		while (frame_cnt == 0xFF) {
			if (vx6953_i2c_read(0x0005, &frame_cnt, 1) < 0)
				return rc;
			CDBG("frame_cnt=%d", frame_cnt);
			msleep(10);
		}

		/* set desired mode */
		switch (mode) {
		case SENSOR_PREVIEW_MODE:
			CDBG("SENSOR_PREVIEW_MODE\n");
			rc = vx6953_video_config(mode);
			break;
		case SENSOR_SNAPSHOT_MODE:
			CDBG("SENSOR_SNAPSHOT_MODE\n");
			rc = vx6953_snapshot_config(mode);
			break;
		case SENSOR_RAW_SNAPSHOT_MODE:
			CDBG("SENSOR_RAW_SNAPSHOT_MODE\n");
			rc = vx6953_raw_snapshot_config(mode);
			break;
		default:
			CDBG("default\n");
			return -EINVAL;
		}
	}

	return 0;
}

static void vx6953_frame_check(u32 *width, u32 *height)
{
	/* get mode first */
	int mode = vx6953_ctrl->sensormode;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		if (*width > VX6953_QTR_SIZE_WIDTH)
			*width = VX6953_QTR_SIZE_WIDTH;

		if (*height > VX6953_QTR_SIZE_HEIGHT)
			*height = VX6953_QTR_SIZE_HEIGHT;
		break;
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		if (*width > VX6953_HRZ_FULL_BLK_PIXELS)
			*width = VX6953_HRZ_FULL_BLK_PIXELS;

		if (*height > VX6953_VER_FULL_BLK_LINES)
			*height = VX6953_VER_FULL_BLK_LINES;
		break;
	default:
		break;
	}
}


static int vx6953_set_params(struct i2c_client *client, u32 width, u32 height,
			     enum v4l2_mbus_pixelcode code)
{
	int i;
	vx6953_ctrl->fmt = NULL;

	/*
	 * frame size check
	 */
	vx6953_frame_check(&width, &height);

	/*
	 * get color format
	 */
	for (i = 0; i < ARRAY_SIZE(vx6953_cfmts); i++)
		if (vx6953_cfmts[i].code == code)
			break;
	if (i == ARRAY_SIZE(vx6953_cfmts))
		return -EINVAL;

	/* sensor supports one fixed size depending upon the mode */
	switch (vx6953_ctrl->sensormode) {
	case SENSOR_PREVIEW_MODE:
		vx6953_video_config(vx6953_ctrl->sensormode);
		break;
	case SENSOR_SNAPSHOT_MODE:
		vx6953_snapshot_config(vx6953_ctrl->sensormode);
		break;
	case SENSOR_RAW_SNAPSHOT_MODE:
		vx6953_raw_snapshot_config(vx6953_ctrl->sensormode);
		break;
	default:
		return -EINVAL;
	}

	/* why need this ? vx6953_ctrl->fmt = &(vx6953_cfmts[i]); */

	return 0;
}

static int vx6953_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	/* right now we are not supporting, probably vfe can take care */
	return -EINVAL;
}

static int vx6953_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	return -EINVAL;
}

static int vx6953_s_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	return -EINVAL;
}

static int vx6953_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	/* by this time vx6953_client should already be set */
	struct i2c_client *client = vx6953_client;

	/* currently sensor supports fixed dimensions only
	 * depending upon the mode*/
	if (!vx6953_ctrl->fmt) {
		int ret = vx6953_set_params(client, VX6953_QTR_SIZE_WIDTH,
						VX6953_QTR_SIZE_HEIGHT,
						V4L2_MBUS_FMT_YUYV8_2X8);
		if (ret < 0)
			return ret;
	}

	mf->width = vx6953_get_pict_pixels_pl();
	mf->height  = vx6953_get_pict_lines_pf();
	/* TODO: set colorspace */
	mf->code  = vx6953_ctrl->fmt->code;
	mf->field = V4L2_FIELD_NONE;

	return 0;
}

static int vx6953_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	/* by this time vx6953_client should already be set */
	struct i2c_client *client = vx6953_client;

	/* TODO: We need to define this function */
	/* TODO: set colorspace */
	return vx6953_set_params(client, mf->width, mf->height, mf->code);
}

static int vx6953_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vx6953_cfmts); i++)
		if (mf->code == vx6953_cfmts[i].code)
			break;

	if (i == ARRAY_SIZE(vx6953_cfmts))
		return -EINVAL;

	/* check that frame is within max sensor supported frame size */
	vx6953_frame_check(&mf->width, &mf->height);

	/* TODO: set colorspace */
	mf->field = V4L2_FIELD_NONE;

	return 0;
}

static int vx6953_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	printk(KERN_DEBUG "Index is %d\n", index);
	if ((unsigned int)index >= ARRAY_SIZE(vx6953_cfmts))
		return -EINVAL;

	*code = vx6953_cfmts[index].code;
	return 0;
}

static struct v4l2_subdev_core_ops vx6953_subdev_core_ops = {
	.g_chip_ident = vx6953_g_chip_ident,
};

static struct v4l2_subdev_video_ops vx6953_subdev_video_ops = {
	.g_parm			   = vx6953_g_parm,
	.s_parm			   = vx6953_s_parm,
	.s_stream = vx6953_s_stream,
	.g_mbus_fmt = vx6953_g_fmt,
	.s_mbus_fmt = vx6953_s_fmt,
	.try_mbus_fmt = vx6953_try_fmt,
	.cropcap  = vx6953_cropcap,
	.g_crop   = vx6953_g_crop,
	.s_crop   = vx6953_s_crop,
	.enum_mbus_fmt  = vx6953_enum_fmt,
};

static struct v4l2_subdev_ops vx6953_subdev_ops = {
	.core = &vx6953_subdev_core_ops,
	.video  = &vx6953_subdev_video_ops,
};

static int vx6953_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = i2c_add_driver(&vx6953_i2c_driver);
	if (rc < 0 || vx6953_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_fail;
	}
	msm_camio_clk_rate_set(24000000);
	rc = vx6953_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;
	s->s_init = vx6953_sensor_open_init;
	s->s_release = vx6953_sensor_release;
	s->s_config  = vx6953_sensor_config;
	vx6953_probe_init_done(info);
	return rc;

probe_fail:
	CDBG("vx6953_sensor_probe: SENSOR PROBE FAILS!\n");
	return rc;
}


static int vx6953_sensor_probe_cb(const struct msm_camera_sensor_info *info,
	struct v4l2_subdev *sdev, struct msm_sensor_ctrl *s)
{
	int rc = 0;
	rc = vx6953_sensor_probe(info, s);
	if (rc < 0)
		return rc;

	vx6953_ctrl = kzalloc(sizeof(struct vx6953_ctrl_t), GFP_KERNEL);
	if (!vx6953_ctrl) {
		CDBG("vx6953_sensor_probe failed!\n");
		return -ENOMEM;
	}

	/* probe is successful, init a v4l2 subdevice */
	printk(KERN_DEBUG "going into v4l2_i2c_subdev_init\n");
	if (sdev) {
		v4l2_i2c_subdev_init(sdev, vx6953_client,
						&vx6953_subdev_ops);
		vx6953_ctrl->sensor_dev = sdev;
	}
	return rc;
}

static int __vx6953_probe(struct platform_device *pdev)
{
	return msm_sensor_register(pdev, vx6953_sensor_probe_cb);
}

static struct platform_driver msm_camera_driver = {
	.probe = __vx6953_probe,
	.driver = {
		.name = "msm_camera_vx6953",
		.owner = THIS_MODULE,
	},
};

static int __init vx6953_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(vx6953_init);
void vx6953_exit(void)
{
	i2c_del_driver(&vx6953_i2c_driver);
}


