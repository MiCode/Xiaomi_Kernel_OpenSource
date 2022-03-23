// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */


/*****************************************************************************
 *
 * Filename:
 * ---------
 *     s5k4h7_mipi_sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/slab.h>
/* #include <asm/system.h> */
/* #include <linux/xlog.h> */

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "s5k4h7_mipi_Sensor.h"

/*WDR auto ration mode*/
/* #define ENABLE_WDR_AUTO_RATION */

/****************************Modify following Strings for debug****************************/
#define PFX "s5k4h7_camera_sensor"
#define LOG_1 LOG_INF("s5k4h7,MIPI 4LANE\n")
/****************************   Modify end    *******************************************/

/**********  S5K4H7 otp begin  ***************************************/
#define MODULE_ID_FLAG1_ADDRESS  0x0A04
#define MODULE_ID_FLAG2_ADDRESS  0x0A0E
#define MODULE_ID_FLAG3_ADDRESS  0x0A18
#define AWB_FLAG1_ADDRESS  0x0A05
#define AWB_FLAG2_ADDRESS  0x0A33
#define AWB_FLAG3_ADDRESS  0x0A41
#define dumpEnable 0
#define INFO_AWB_PAGE  21
#define INFO_AWB_GROUP3_PAGE  22

#define LSC1_PAGE_START  1
#define LSC2_PAGE_START  6
#define LSC1_FLAG1_ADDRESS  0x0A04
#define LSC2_FLAG2_ADDRESS  0x0A2C

struct OTP otp_data_info = {0};

unsigned char g_reg_data[34];

/*********  S5K4H7 otp end  *************************************/
#define LOG_INF(fmt, args...)	pr_debug(PFX "[%s] " fmt, __func__, ##args)
/* static int first_flag = 1; */
static DEFINE_SPINLOCK(imgsensor_drv_lock);


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K4H7_SENSOR_ID,

	.checksum_value = 0xf16e8197,	/*Check by Dream */

	.pre = {
		.pclk = 280000000,	/*record different mode's pclk */
		.linelength = 3688,	/*record different mode's linelength */
		.framelength = 2530,	/*record different mode's framelength */
		.startx = 0,	/*record different mode's startx of grabwindow */
		.starty = 0,	/*record different mode's starty of grabwindow */
		.grabwindow_width = 1632,	/*record different mode's width of grabwindow */
		.grabwindow_height = 1224,	/*record different mode's height of grabwindow */
		/*following for MIPIDataLowPwr2HighS*/
		/*peedSettleDelayCount by different scenario   */
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280000000,
		/*       following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 280000000,	/*record different mode's pclk */
		.linelength = 3688,	/*record different mode's linelength */
		.framelength = 2530,	/*record different mode's framelength */
		.startx = 0,	/*record different mode's startx of grabwindow */
		.starty = 0,	/*record different mode's starty of grabwindow */
		.grabwindow_width = 3264,	/*record different mode's width of grabwindow */
		.grabwindow_height = 2448,	/*record different mode's height of grabwindow */
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount*/
		/*by different scenario   */
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280000000,
		/*following for GetDefaultFramerateByScenario()  */
		.max_framerate = 300,
		},
	.normal_video = {
		.pclk = 280000000,	/*record different mode's pclk */
		.linelength = 3688,	/*record different mode's linelength */
		.framelength = 2530,	/*record different mode's framelength */
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,	/*record different mode's width of grabwindow */
		.grabwindow_height = 2448,	/*record different mode's height of grabwindow */

		.mipi_data_lp2hs_settle_dc = 85,
		/*following for MIPIDataLowPwr2HighSpeedSettleDelayCount*/
			/*by different scenario   */
		.mipi_pixel_rate = 280000000,

		.max_framerate = 300,
		/*following for*/
		/*GetDefaultFramerateByScenario()  */
		},
	.hs_video = {
		.pclk = 280000000,	/*record different mode's pclk */
		.linelength = 3688,	/*record different mode's linelength */
		.framelength = 632,	/*record different mode's framelength */
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 640,	/*record different mode's width of grabwindow */
		.grabwindow_height = 480,	/*record different mode's height of grabwindow */

		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280000000,
		.max_framerate = 1200,
		},
	.slim_video = {
		.pclk = 280000000,	/*record different mode's pclk */
		.linelength = 3688,	/*record different mode's linelength */
		.framelength = 2530,	/*record different mode's framelength */
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,	/*record different mode's width of grabwindow */
		.grabwindow_height = 720,	/*record different mode's height of grabwindow */

		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 280000000,
		.max_framerate = 300,
		},
	.margin = 4,
	.min_shutter = 4,
	.max_frame_length = 0xffff - 5,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num */

	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,

	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 0,	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	/* SENSOR_OUTPUT_FORMAT_RAW_Gr, */
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	/* .i2c_speed = 100, */
	.i2c_addr_table = {0x5A, 0xff},
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 0,	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,
	.test_pattern = 0,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,	/* current scenario id */
	.ihdr_en = 0,		/* sensor need support LE, SE with HDR feature */

	.hdr_mode = KAL_FALSE,	/* HDR Mode : 0: disable HDR, 1:IHDR, 2:HDR, 9:ZHDR */

	.i2c_write_id = 0x5A,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
{3264, 2448, 0, 0, 3264, 2448, 1632, 1224,
	0000, 0000, 1632, 1224, 0, 0, 1632, 1224},	/* Preview */
{3264, 2448, 0, 0, 3264, 2448, 3264, 2448,
	0000, 0000, 3264, 2448, 0, 0, 3264, 2448},	/* capture */
{3264, 2448, 0, 0, 3264, 2448, 3264, 2448,
	0000, 0000, 3264, 2448, 0, 0, 3264, 2448},	/* video */
{3264, 2448, 352, 264, 2560, 1920, 640, 480,
	0000, 0000, 640, 480, 0, 0, 640,  480},	/* hight speed video */
{3264, 2448, 352, 504, 2560, 1440, 1280,
	720, 0000, 0000, 1280, 720, 0, 0, 1280, 720},	/* slim video */

};				/* slim video */

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte << 8) & 0xff00) | ((get_byte >> 8) & 0x00ff);
}


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
	    (char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF) };
	iWriteRegI2CTiming(pusendcmd, 4, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

static kal_uint16 read_cmos_sensor_8(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2CTiming(pusendcmd, 3, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
}

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 4

#endif


static bool s5k4h7_start_read_otp(kal_uint8 pageidx)
{
	kal_uint8 val = 0;
	char i = 0;

	write_cmos_sensor_8(0x0100, 0x01);
	mdelay(50);
	write_cmos_sensor_8(0x0A02, pageidx);
	write_cmos_sensor_8(0x0A00, 0x01);

	mdelay(1);
	for (i = 0; i <= 100; i++) {
		val = (kal_uint8)read_cmos_sensor_8(0x0A01);
		if (val == 0x01)
			break;
		mdelay(1);
	}

	if (i == 100) {
		write_cmos_sensor_8(0x0A00, 0x00);
		return false;
	}

	return true;
}

static bool zte_s5k4h7_127_awb(void)
{
	kal_uint16 flag1;
	kal_uint16 start_address = 0;
	kal_uint8 *buf = NULL;
	unsigned char  checksum = 0;
	bool flag = false;
	bool ret = false;
	unsigned int i = 0, j = 0, k = 0, sum = 0;

	for (i = 0; i < 3; i++) {
		flag = s5k4h7_start_read_otp(INFO_AWB_PAGE + i);
		LOG_INF("valid otp select page:0x%x, i:%d", flag, i);
		if (!flag) {
			LOG_INF("invalid otp select page");
			return false;
		}
		flag1 = read_cmos_sensor_8(MODULE_ID_FLAG1_ADDRESS);
		if ((flag1 >> 6) == 0x01)
			break;
	}
	buf = kmalloc(sizeof(kal_uint8)*31, GFP_KERNEL);
	for (j = AWB_FLAG1_ADDRESS; j <= AWB_FLAG1_ADDRESS + 29; j++) {
		buf[k] = (kal_uint8) read_cmos_sensor_8(j)&0xff;
		sum += buf[k];
		//LOG_INF("group  buf[%d] =%d sum=%d\n", k, buf[k], sum);
		k++;
	}
	buf[k] = (kal_uint8) read_cmos_sensor_8(AWB_FLAG1_ADDRESS+30)&0xff;

	/*LOG_INF("group checksum buf[%d] =%d", k, buf[k]);*/

	if ((sum % 255 + 1) == buf[k]) {

		/*LOG_INF(" Checksum good, j:%d, sum:%d buf: %d", j,sum, buf[j]);*/

		checksum = 1;
		memcpy(g_reg_data, buf, 30*sizeof(kal_uint8));
	} else {
		LOG_INF(" Checksum error!");
	}

	if (checksum == 1) {
		otp_data_info.module_integrator_id = buf[start_address];
		otp_data_info.lens_id = buf[start_address+5];
		otp_data_info.production_year = buf[start_address+7];
		otp_data_info.production_month = buf[start_address+8];
		otp_data_info.production_day = buf[start_address+9];
		otp_data_info.module_integrator_id, otp_data_info.production_year,
		otp_data_info.production_month;
		otp_data_info.r_cur = buf[start_address+12];
		otp_data_info.b_cur = buf[start_address+13];
		otp_data_info.r_golden_cur = buf[start_address+15];
		otp_data_info.b_golden_cur = buf[start_address+16];

		/*LOG_INF("r = %d, b = %d, golden_r = %d, golden_b = %d\n",*/
		/*otp_data_info.r_cur, otp_data_info.b_cur,*/
		/*otp_data_info.r_golden_cur, otp_data_info.b_golden_cur);*/

		ret = true;
	}

	kfree(buf);
	return ret;
}
static void get_4h7_page_data(int pageidx, unsigned char *pdata)
{
	unsigned short get_byte = 0;
	unsigned int addr = 0x0A04;
	int i = 0;

	write_cmos_sensor_8(0x0A02, pageidx);
	write_cmos_sensor_8(0x0A00, 0x01);

	do {
		mdelay(1);
		get_byte = read_cmos_sensor_8(0x0A01);
	} while ((get_byte & 0x01) != 1);

	for (i = 0; i < 64; i++) {
		pdata[i] = read_cmos_sensor_8(addr);
		LOG_INF("pageidx = %d, pdata[%d] = %d\n", pageidx, i, pdata[i]);
		addr++;
	}
	/* write_cmos_sensor_8(0x0A00,0x00); */
}

static bool read_4h7_page(int page_start, int page_end, unsigned char *pdata)
{
	bool bresult = true;
	int st_page_start = page_start;

	if (page_start <= 0 || page_end > 22) {
		bresult = false;
		LOG_INF(" OTP page_end is large!");
		return bresult;
	}
	for (; st_page_start <= page_end; st_page_start++)
		get_4h7_page_data(st_page_start, pdata);

	return bresult;
}
static unsigned int sum_awb_flag_lsc(unsigned int sum_start,
				unsigned int sum_end, unsigned char *pdata)
{
	int i = 0;
	unsigned int start;
	unsigned int re_sum = 0;

	for (start = 0x0A04; i < 64; i++, start++) {
		if ((start >= sum_start) && (start <= sum_end)) {
			LOG_INF("pdata[%d]=%d\n", i, pdata[i]);
			re_sum += pdata[i];
		}
	}
	LOG_INF("re_sum=%d\n", re_sum);
	return re_sum;
}
static void apply_s5k4h7_otp_enb_lsc(void)
{
	LOG_INF("s5k4h7 enable lsc\n");
	write_cmos_sensor_8(0x3400, 0x00);
	write_cmos_sensor_8(0x0B00, 0x01);
}

static bool zte_s5k4h7_127_lsc(void)
{
	kal_uint16 flag1, flag2;
	bool  flag = false;
	int page_start = 21, page_end = 21;
	unsigned char data_p[12][64] = {};
	bool bresult = true;
	unsigned int  sum_slc = 0;
	unsigned char checksum = 0;

	flag = s5k4h7_start_read_otp(LSC1_PAGE_START);
	if (!flag)
		LOG_INF("invalid LSC1 select page\n");

	flag1 = read_cmos_sensor_8(0x0A04);
	LOG_INF("flag=%d, flag1=%d\n", flag, flag1);
	if (flag1 != 0) {
		otp_data_info.lsc_group = 1;
		for (page_start = 1, page_end = 6; page_start <= page_end; page_start++) {
			/* LOG_INF("1-23 page_start=%d\n", page_start); */
			bresult &= read_4h7_page(page_start, page_start, data_p[page_start-1]);
			if (page_start == 6) {
				flag = s5k4h7_start_read_otp(page_start);
				/* LOG_INF("flag =%d\n", flag); */
				if (flag) {
					sum_slc +=
						sum_awb_flag_lsc(0x0A04,
								0x0A2A, data_p[page_start-1]);
					continue;
				}
			}
			sum_slc += sum_awb_flag_lsc(0x0A04, 0X0A43, data_p[page_start-1]);
		}
		if (!bresult)
			LOG_INF("LSC1 read page too large failed\n");

		checksum = (unsigned char)read_cmos_sensor_8(0x0A2B);
		LOG_INF("LSC1 checksum=%d, sum_slc=%d\n", checksum, sum_slc);
		if (checksum != 0 || sum_slc != 0) {
			LOG_INF("LSC1 data ok\n");
			otp_data_info.lsc_check_flag = 1;
		}

	} else {
		flag = s5k4h7_start_read_otp(LSC2_PAGE_START);
		if (!flag)
			LOG_INF("invalid LSC2 select page\n");

		flag2 = read_cmos_sensor_8(0x0A2C);
		/*LOG_INF("flag=%d, flag2=%d\n", flag, flag2);*/
		if (flag2 != 0) {
			otp_data_info.lsc_group = 2;
			for (page_start = 6, page_end = 12; page_start <= page_end; page_start++) {
				bresult &= read_4h7_page(page_start, page_start,
				data_p[page_start-1]);
				if (page_start == 12) {
					flag = s5k4h7_start_read_otp(page_start);
					if (flag) {
						sum_slc += sum_awb_flag_lsc(0x0A04,
							0x0A12,
							 data_p[page_start-1]);
						continue;
					}
				}
				if (page_start == 6) {
					sum_slc += sum_awb_flag_lsc(0x0A2C,
								0x0A43, data_p[page_start-1]);
					continue;
				}
				sum_slc += sum_awb_flag_lsc(0x0A04,
							0X0A43, data_p[page_start-1]);
			}
		}
		if (!bresult)
			LOG_INF("LSC2 read page too large failed\n");

		checksum = (unsigned char)read_cmos_sensor_8(0x0A13);
		LOG_INF("LSC2 checksum=%d, sum_slc=%d\n", checksum, sum_slc);
		if (checksum != 0 || sum_slc != 0) {
			LOG_INF("LSC2 data ok\n");
			otp_data_info.lsc_check_flag = 1;
		}

	}

	if (otp_data_info.lsc_check_flag == 1)
		apply_s5k4h7_otp_enb_lsc();
	else
		bresult &= 0;

	return bresult;

}

static bool zte_s5k4h7_127_af(void)
{

	kal_uint16 flag1;
	kal_uint8 *buf = NULL;
	bool flag = false;
	bool ret = false;
	unsigned int i = 0, j = 0, k = 0, sum = 0;

	for (i = 0; i < 3; i++) {
		flag = s5k4h7_start_read_otp(INFO_AWB_PAGE + i);
		LOG_INF("invalid otp select page:0x%x, i:%d", flag, i);
		if (!flag) {
			LOG_INF("invalid otp select page");
			return false;
		}
		flag1 = read_cmos_sensor_8(0x0a24); //af flag
		if ((flag1 >> 6) == 0x01)
			break;
	}
	buf = kmalloc(sizeof(kal_uint8)*4, GFP_KERNEL);
	for (j = 0x0a25; j < 0x0a25 + 3; j++) {
		buf[k] = (kal_uint8) read_cmos_sensor_8(j)&0xff;
		sum += buf[k];
		LOG_INF("group  buf[%d] =%d sum=%d\n", k, buf[k], sum);
		k++;
	}

	buf[k] = (kal_uint8) read_cmos_sensor_8(j)&0xff;

	LOG_INF("af group checksum buf[%d] =%d", k, buf[k]);
	if ((sum % 255 + 1) == buf[k]) {
		LOG_INF("af Checksum good, j:%d, sum:%d buf: %d", j,
		sum, buf[j]);
		 memcpy(g_reg_data+30, buf, 3*sizeof(kal_uint8));
		ret = true;
	} else {
		LOG_INF("af Checksum error!");
	}
	return ret;
}

static bool update_otp(void)
{
	memset(&otp_data_info, 0, sizeof(struct OTP));
	/*LOG_INF("update_otp start!");*/

	/*if (!zte_s5k4h7_127_read_info()) return false;*/

	if (!zte_s5k4h7_127_awb())
		return false;
	if (!zte_s5k4h7_127_lsc())
		return false;
	if (!zte_s5k4h7_127_af())
		return false;
	g_reg_data[33] = 1;
	return true;
}

unsigned int zte_s5k4h7_read_region(struct i2c_client *client, unsigned int addr,
			unsigned char *data, unsigned int size)
{
	memcpy(data, g_reg_data, 34*sizeof(kal_uint8));
	return 5376;
}

static void set_dummy(void)
{
	LOG_INF("frame_length = %d, line_length = %d\n",
		imgsensor.frame_length,	imgsensor.line_length);

	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);

}				/*      set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));

}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/* kal_int16 dummy_line; */
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	LOG_INF("framerate = %d, min framelength should enable = %d\n",
				framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
	    (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	/* dummy_line = frame_length - imgsensor.min_frame_length; */
	/* if (dummy_line < 0) */
	/* imgsensor.dummy_line = 0; */
	/* else */
	/* imgsensor.dummy_line = dummy_line; */
	/* imgsensor.frame_length = frame_length + imgsensor.dummy_line; */
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*      set_max_framerate  */


static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	/* kal_uint32 frame_length = 0; */


	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter =
	    (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
			? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
	/* write_cmos_sensor(0x0104, 0x01);   //group hold */
	write_cmos_sensor_8(0x0202, shutter >> 8);
	write_cmos_sensor_8(0x0203, shutter & 0xFF);
	/* write_cmos_sensor(0x021E, shutter); */
	/* write_cmos_sensor(0x0104, 0x00);   //group hold */

	LOG_INF("shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

}				/*      write_shutter  */

static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	/* LOG_INF("shutter =%d\n", shutter); */
	write_shutter(shutter);
}				/*      set_shutter */


static void hdr_write_shutter(kal_uint16 le, kal_uint16 se)
{
	unsigned int iRation;
	unsigned long flags;
	/* kal_uint16 realtime_fps = 0; */
	/* kal_uint32 frame_length = 0; */
	/* LOG_INF("enter xxxx  set_shutter, shutter =%d\n", shutter); */
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = le;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	if (!le)
		le = 1;		/*avoid 0 */

	spin_lock(&imgsensor_drv_lock);
	if (le > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = le + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_unlock(&imgsensor_drv_lock);

	le = (le < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : le;
	le = (le > (imgsensor_info.max_frame_length - imgsensor_info.margin))
			? (imgsensor_info.max_frame_length - imgsensor_info.margin) : le;

	/* Frame length :4000 C340 */
	/* write_cmos_sensor(0x6028,0x4000); */
	/* write_cmos_sensor(0x602A,0xC340 ); */
	write_cmos_sensor(0x0340, imgsensor.frame_length);

	/* SET LE/SE ration */
	/* iRation = (((LE + SE/2)/SE) >> 1 ) << 1 ; */
	iRation = ((10 * le / se) + 5) / 10;
	if (iRation < 2)
		iRation = 1;
	else if (iRation < 4)
		iRation = 2;
	else if (iRation < 8)
		iRation = 4;
	else if (iRation < 16)
		iRation = 8;
	else if (iRation < 32)
		iRation = 16;
	else
		iRation = 1;

	/*set ration for auto */
	iRation = 0x100 * iRation;
#if defined(ENABLE_WDR_AUTO_RATION)
	/*LE / SE ration ,  0x218/0x21a =  LE Ration */
	/*0x218 =0x400, 0x21a=0x100, LE/SE = 4x */
	write_cmos_sensor(0x0218, iRation);
	write_cmos_sensor(0x021a, 0x100);
#endif
	/*Short exposure */
	write_cmos_sensor(0x0202, se);
	/*Log exposure ratio */
	write_cmos_sensor(0x021e, le);

	LOG_INF("HDR set shutter LE=%d, SE=%d, iRation=0x%x\n", le, se, iRation);

}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X  */
	/* [4:9] = M meams M X       */
	/* Total gain = M + N /16 X   */

	/*  */
	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
	}

	reg_gain = gain >> 1;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n", gain, reg_gain);

	/* write_cmos_sensor_8(0x0104, 0x01); */
	/* write_cmos_sensor(0x0204, reg_gain); */
	write_cmos_sensor_8(0x0204, (reg_gain>>8));
	write_cmos_sensor_8(0x0205, (reg_gain&0xff));
	/* write_cmos_sensor(0x0220, reg_gain); */

	/* write_cmos_sensor_8(0x0104, 0x00); */

	return gain;
}				/*      set_gain  */


/* defined but not used */
static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {

		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length = imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;
		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;


		/* Extend frame length first */

		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);


		set_gain(gain);
	}

}



static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_8(0x0101, 0x00);	/* Gr */
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_8(0x0101, 0x01);	/* R */
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_8(0x0101, 0x02);	/* B */
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_8(0x0101, 0x03);	/* Gb */
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
	}

}

static void sensor_init(void)
{
	LOG_INF("s5k init");

	write_cmos_sensor_8(0x0100, 0x00);
	write_cmos_sensor_8(0x0000, 0x12);
	write_cmos_sensor_8(0x0000, 0x48);
	write_cmos_sensor_8(0x0A02, 0x15);
	write_cmos_sensor_8(0x0B05, 0x01);
	write_cmos_sensor_8(0x3074, 0x06);
	write_cmos_sensor_8(0x3075, 0x2F);
	write_cmos_sensor_8(0x308A, 0x20);
	write_cmos_sensor_8(0x308B, 0x08);
	write_cmos_sensor_8(0x308C, 0x0B);
	write_cmos_sensor_8(0x3081, 0x07);
	write_cmos_sensor_8(0x307B, 0x85);
	write_cmos_sensor_8(0x307A, 0x0A);
	write_cmos_sensor_8(0x3079, 0x0A);
	write_cmos_sensor_8(0x306E, 0x71);
	write_cmos_sensor_8(0x306F, 0x28);
	write_cmos_sensor_8(0x301F, 0x20);
	write_cmos_sensor_8(0x306B, 0x9A);
	write_cmos_sensor_8(0x3091, 0x1F);
	write_cmos_sensor_8(0x30C4, 0x06);
	write_cmos_sensor_8(0x3200, 0x09);
	write_cmos_sensor_8(0x306A, 0x79);
	write_cmos_sensor_8(0x30B0, 0xFF);
	write_cmos_sensor_8(0x306D, 0x08);
	write_cmos_sensor_8(0x3080, 0x00);
	write_cmos_sensor_8(0x3929, 0x3F);
	write_cmos_sensor_8(0x3084, 0x16);
	write_cmos_sensor_8(0x3070, 0x0F);
	write_cmos_sensor_8(0x3B45, 0x01);
	write_cmos_sensor_8(0x30C2, 0x05);
	write_cmos_sensor_8(0x3069, 0x87);
	write_cmos_sensor_8(0x3924, 0x7F);
	write_cmos_sensor_8(0x3925, 0xFD);
	write_cmos_sensor_8(0x3C08, 0xFF);
	write_cmos_sensor_8(0x3C09, 0xFF);
	write_cmos_sensor_8(0x3C31, 0xFF);
	write_cmos_sensor_8(0x3C32, 0xFF);

	write_cmos_sensor_8(0x0100, 0x00);

}

static void preview_setting(void)
{
	kal_uint16 framecnt;
	int i;

	LOG_INF("preview setting()E\n");
	write_cmos_sensor_8(0X0100, 0X00);
	for (i = 0; i < 100; i++) {
		framecnt = read_cmos_sensor(0x0005);
		/* waiting for sensor to  stop output  then  set the  setting */
		if (framecnt == 0xFF) {
			LOG_INF("stream is off\n");
			break;
		}
			LOG_INF("stream is not off\n");
			mdelay(1);
	}
	LOG_INF("s5k preview");
	write_cmos_sensor_8(0x0136, 0x18);
	write_cmos_sensor_8(0x0137, 0x00);
	write_cmos_sensor_8(0x0305, 0x06);
	write_cmos_sensor_8(0x0306, 0x00);
	write_cmos_sensor_8(0x0307, 0x8C);
	write_cmos_sensor_8(0x030D, 0x06);
	write_cmos_sensor_8(0x030E, 0x00);
	write_cmos_sensor_8(0x030F, 0xAF);
	write_cmos_sensor_8(0x3C1F, 0x00);
	write_cmos_sensor_8(0x3C17, 0x00);
	write_cmos_sensor_8(0x3C1C, 0x05);
	write_cmos_sensor_8(0x3C1D, 0x15);
	write_cmos_sensor_8(0x0301, 0x04);
	write_cmos_sensor_8(0x0820, 0x02);
	write_cmos_sensor_8(0x0821, 0xBC);
	write_cmos_sensor_8(0x0822, 0x00);
	write_cmos_sensor_8(0x0823, 0x00);
	write_cmos_sensor_8(0x0112, 0x0A);
	write_cmos_sensor_8(0x0113, 0x0A);
	write_cmos_sensor_8(0x0114, 0x03);
	write_cmos_sensor_8(0x3906, 0x00);
	write_cmos_sensor_8(0x0344, 0x00);
	write_cmos_sensor_8(0x0345, 0x08);
	write_cmos_sensor_8(0x0346, 0x00);
	write_cmos_sensor_8(0x0347, 0x08);
	write_cmos_sensor_8(0x0348, 0x0C);
	write_cmos_sensor_8(0x0349, 0xC7);
	write_cmos_sensor_8(0x034A, 0x09);
	write_cmos_sensor_8(0x034B, 0x97);
	write_cmos_sensor_8(0x034C, 0x06);
	write_cmos_sensor_8(0x034D, 0x60);
	write_cmos_sensor_8(0x034E, 0x04);
	write_cmos_sensor_8(0x034F, 0xC8);
	write_cmos_sensor_8(0x0900, 0x01);
	write_cmos_sensor_8(0x0901, 0x22);
	write_cmos_sensor_8(0x0381, 0x01);
	write_cmos_sensor_8(0x0383, 0x01);
	write_cmos_sensor_8(0x0385, 0x01);
	write_cmos_sensor_8(0x0387, 0x03);
	write_cmos_sensor_8(0x0101, 0x00);
	write_cmos_sensor_8(0x0340, 0x09);
	write_cmos_sensor_8(0x0341, 0xE2);
	write_cmos_sensor_8(0x0342, 0x0E);
	write_cmos_sensor_8(0x0343, 0x68);
	write_cmos_sensor_8(0x0200, 0x0D);
	write_cmos_sensor_8(0x0201, 0xD8);
	write_cmos_sensor_8(0x0202, 0x02);
	write_cmos_sensor_8(0x0203, 0x08);
	write_cmos_sensor_8(0x0100, 0x01);
}				/*      preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	kal_uint16 framecnt;
	int i;

	LOG_INF("capture setting()E! currefps:%d\n", currefps);

	write_cmos_sensor_8(0X0100, 0X00);

	for (i = 0; i < 100; i++) {
		framecnt = read_cmos_sensor(0x0005);
		if (framecnt == 0xFF) {
			LOG_INF("stream is  off\\n");
			break;
		}
			LOG_INF("stream is not off\\n");
			mdelay(1);
	}
	LOG_INF("s5k4h7 capture_setting123");
	write_cmos_sensor_8(0X0136, 0X18);
	write_cmos_sensor_8(0X0137, 0X00);
	write_cmos_sensor_8(0X0305, 0X06);
	write_cmos_sensor_8(0X0306, 0X00);
	write_cmos_sensor_8(0X0307, 0X8C);
	write_cmos_sensor_8(0X030D, 0X06);
	write_cmos_sensor_8(0X030E, 0X00);
	write_cmos_sensor_8(0X030F, 0XAF);
	write_cmos_sensor_8(0X3C1F, 0X00);
	write_cmos_sensor_8(0X3C17, 0X00);
	write_cmos_sensor_8(0X3C1C, 0X05);
	write_cmos_sensor_8(0X3C1D, 0X15);
	write_cmos_sensor_8(0X0301, 0X04);
	write_cmos_sensor_8(0X0820, 0X02);
	write_cmos_sensor_8(0X0821, 0XBC);
	write_cmos_sensor_8(0X0822, 0X00);
	write_cmos_sensor_8(0X0823, 0X00);
	write_cmos_sensor_8(0X0112, 0X0A);
	write_cmos_sensor_8(0X0113, 0X0A);
	write_cmos_sensor_8(0X0114, 0X03);
	write_cmos_sensor_8(0X3906, 0X04);
	write_cmos_sensor_8(0X0344, 0X00);
	write_cmos_sensor_8(0X0345, 0X08);
	write_cmos_sensor_8(0X0346, 0X00);
	write_cmos_sensor_8(0X0347, 0X08);
	write_cmos_sensor_8(0X0348, 0X0C);
	write_cmos_sensor_8(0X0349, 0XC7);
	write_cmos_sensor_8(0X034A, 0X09);
	write_cmos_sensor_8(0X034B, 0X97);
	write_cmos_sensor_8(0X034C, 0X0C);
	write_cmos_sensor_8(0X034D, 0XC0);
	write_cmos_sensor_8(0X034E, 0X09);
	write_cmos_sensor_8(0X034F, 0X90);
	write_cmos_sensor_8(0X0900, 0X00);
	write_cmos_sensor_8(0X0901, 0X00);
	write_cmos_sensor_8(0X0381, 0X01);
	write_cmos_sensor_8(0X0383, 0X01);
	write_cmos_sensor_8(0X0385, 0X01);
	write_cmos_sensor_8(0X0387, 0X01);
	write_cmos_sensor_8(0X0101, 0X00);

	if (currefps == 300) {
		write_cmos_sensor_8(0X0340, 0X09);
		write_cmos_sensor_8(0X0341, 0XE2);
	} else if (currefps == 240) {
		write_cmos_sensor_8(0X0340, 0X0C);
		write_cmos_sensor_8(0X0341, 0X4E);
	} else {
		write_cmos_sensor_8(0X0340, 0X13);
		write_cmos_sensor_8(0X0341, 0XC4);
	}

	write_cmos_sensor_8(0X0342, 0X0E);
	write_cmos_sensor_8(0X0343, 0X68);
	write_cmos_sensor_8(0X0200, 0X0D);
	write_cmos_sensor_8(0X0201, 0XD8);
	write_cmos_sensor_8(0X0202, 0X02);
	write_cmos_sensor_8(0X0203, 0X08);
	write_cmos_sensor_8(0x0100, 0x01);
}


static void normal_video_setting(kal_uint16 currefps)
{
	kal_uint16 framecnt;
	int i;

	LOG_INF("normal video_setting()E! currefps:%d\n", currefps);

	write_cmos_sensor_8(0x0100, 0x00);

	for (i = 0; i < 100; i++) {
		framecnt = read_cmos_sensor(0x0005);
		if (framecnt == 0xFF) {
			LOG_INF("stream is off\\n");
			break;
		}
			LOG_INF("stream is not off\\n");
			mdelay(1);
	}
	LOG_INF("s5k normal video_setting");
	/* Reset for operation */

	write_cmos_sensor_8(0X0100, 0X00);
	write_cmos_sensor_8(0X0136, 0X18);
	write_cmos_sensor_8(0X0137, 0X00);
	write_cmos_sensor_8(0X0305, 0X06);
	write_cmos_sensor_8(0X0306, 0X00);
	write_cmos_sensor_8(0X0307, 0X8C);
	write_cmos_sensor_8(0X030D, 0X06);
	write_cmos_sensor_8(0X030E, 0X00);
	write_cmos_sensor_8(0X030F, 0XAF);
	write_cmos_sensor_8(0X3C1F, 0X00);
	write_cmos_sensor_8(0X3C17, 0X00);
	write_cmos_sensor_8(0X3C1C, 0X05);
	write_cmos_sensor_8(0X3C1D, 0X15);
	write_cmos_sensor_8(0X0301, 0X04);
	write_cmos_sensor_8(0X0820, 0X02);
	write_cmos_sensor_8(0X0821, 0XBC);
	write_cmos_sensor_8(0X0822, 0X00);
	write_cmos_sensor_8(0X0823, 0X00);
	write_cmos_sensor_8(0X0112, 0X0A);
	write_cmos_sensor_8(0X0113, 0X0A);
	write_cmos_sensor_8(0X0114, 0X03);
	write_cmos_sensor_8(0X3906, 0X04);
	write_cmos_sensor_8(0X0344, 0X00);
	write_cmos_sensor_8(0X0345, 0X08);
	write_cmos_sensor_8(0X0346, 0X00);
	write_cmos_sensor_8(0X0347, 0X08);
	write_cmos_sensor_8(0X0348, 0X0C);
	write_cmos_sensor_8(0X0349, 0XC7);
	write_cmos_sensor_8(0X034A, 0X09);
	write_cmos_sensor_8(0X034B, 0X97);
	write_cmos_sensor_8(0X034C, 0X0C);
	write_cmos_sensor_8(0X034D, 0XC0);
	write_cmos_sensor_8(0X034E, 0X09);
	write_cmos_sensor_8(0X034F, 0X90);
	write_cmos_sensor_8(0X0900, 0X00);
	write_cmos_sensor_8(0X0901, 0X00);
	write_cmos_sensor_8(0X0381, 0X01);
	write_cmos_sensor_8(0X0383, 0X01);
	write_cmos_sensor_8(0X0385, 0X01);
	write_cmos_sensor_8(0X0387, 0X01);
	write_cmos_sensor_8(0X0101, 0X00);
	write_cmos_sensor_8(0X0340, 0X09);
	write_cmos_sensor_8(0X0341, 0XE2);
	write_cmos_sensor_8(0X0342, 0X0E);
	write_cmos_sensor_8(0X0343, 0X68);
	write_cmos_sensor_8(0X0200, 0X0D);
	write_cmos_sensor_8(0X0201, 0XD8);
	write_cmos_sensor_8(0X0202, 0X02);
	write_cmos_sensor_8(0X0203, 0X08);
	write_cmos_sensor_8(0x0100, 0x01);

}

static void hs_video_setting(void)
{
	kal_uint16 framecnt;
	int i;

	LOG_INF("hs video_setting()E\n");

	write_cmos_sensor_8(0X0100, 0X00);
	for (i = 0; i < 100; i++) {
		framecnt = read_cmos_sensor(0x0005);
		if (framecnt == 0xFF) {
			LOG_INF("stream is off\\n");
			break;
		}
		LOG_INF("stream is not off\\n");
		mdelay(1);
	}

	write_cmos_sensor_8(0X0136, 0X18);
	write_cmos_sensor_8(0X0137, 0X00);
	write_cmos_sensor_8(0X0305, 0X06);
	write_cmos_sensor_8(0X0306, 0X00);
	write_cmos_sensor_8(0X0307, 0X8C);
	write_cmos_sensor_8(0X030D, 0X06);
	write_cmos_sensor_8(0X030E, 0X00);
	write_cmos_sensor_8(0X030F, 0XAF);
	write_cmos_sensor_8(0X3C1F, 0X00);
	write_cmos_sensor_8(0X3C17, 0X00);
	write_cmos_sensor_8(0X3C1C, 0X05);
	write_cmos_sensor_8(0X3C1D, 0X15);
	write_cmos_sensor_8(0X0301, 0X04);
	write_cmos_sensor_8(0X0820, 0X02);
	write_cmos_sensor_8(0X0821, 0XBC);
	write_cmos_sensor_8(0X0822, 0X00);
	write_cmos_sensor_8(0X0823, 0X00);
	write_cmos_sensor_8(0X0112, 0X0A);
	write_cmos_sensor_8(0X0113, 0X0A);
	write_cmos_sensor_8(0X0114, 0X03);
	write_cmos_sensor_8(0X3906, 0X00);
	write_cmos_sensor_8(0X0344, 0X01);
	write_cmos_sensor_8(0X0345, 0X68);
	write_cmos_sensor_8(0X0346, 0X01);
	write_cmos_sensor_8(0X0347, 0X10);
	write_cmos_sensor_8(0X0348, 0X0B);
	write_cmos_sensor_8(0X0349, 0X67);
	write_cmos_sensor_8(0X034A, 0X08);
	write_cmos_sensor_8(0X034B, 0X8F);
	write_cmos_sensor_8(0X034C, 0X02);
	write_cmos_sensor_8(0X034D, 0X80);
	write_cmos_sensor_8(0X034E, 0X01);
	write_cmos_sensor_8(0X034F, 0XE0);
	write_cmos_sensor_8(0X0900, 0X01);
	write_cmos_sensor_8(0X0901, 0X44);
	write_cmos_sensor_8(0X0381, 0X01);
	write_cmos_sensor_8(0X0383, 0X01);
	write_cmos_sensor_8(0X0385, 0X01);
	write_cmos_sensor_8(0X0387, 0X07);
	write_cmos_sensor_8(0X0101, 0X00);
	write_cmos_sensor_8(0X0340, 0X02);
	write_cmos_sensor_8(0X0341, 0X78);
	write_cmos_sensor_8(0X0342, 0X0E);
	write_cmos_sensor_8(0X0343, 0X68);
	write_cmos_sensor_8(0X0200, 0X0D);
	write_cmos_sensor_8(0X0201, 0XD8);
	write_cmos_sensor_8(0X0202, 0X02);
	write_cmos_sensor_8(0X0203, 0X08);
	write_cmos_sensor_8(0x0100, 0x01);
}

static void slim_video_setting(void)
{
	kal_uint16 framecnt;
	int i;

	LOG_INF("slim video setting() E\n");

	write_cmos_sensor_8(0X0100, 0X00);

	for (i = 0; i < 100; i++) {
		framecnt = read_cmos_sensor(0x0005);

		/* waiting for sensor to  stop output  then  set the  setting */

		if (framecnt == 0xFF) {
			LOG_INF("stream is  off\\n");
			break;
		}
		LOG_INF("stream is not off\\n");
		mdelay(1);
	}
	write_cmos_sensor_8(0X0136, 0X18);
	write_cmos_sensor_8(0X0137, 0X00);
	write_cmos_sensor_8(0X0305, 0X06);
	write_cmos_sensor_8(0X0306, 0X00);
	write_cmos_sensor_8(0X0307, 0X8C);
	write_cmos_sensor_8(0X030D, 0X06);
	write_cmos_sensor_8(0X030E, 0X00);
	write_cmos_sensor_8(0X030F, 0XAF);
	write_cmos_sensor_8(0X3C1F, 0X00);
	write_cmos_sensor_8(0X3C17, 0X00);
	write_cmos_sensor_8(0X3C1C, 0X05);
	write_cmos_sensor_8(0X3C1D, 0X15);
	write_cmos_sensor_8(0X0301, 0X04);
	write_cmos_sensor_8(0X0820, 0X02);
	write_cmos_sensor_8(0X0821, 0XBC);
	write_cmos_sensor_8(0X0822, 0X00);
	write_cmos_sensor_8(0X0823, 0X00);
	write_cmos_sensor_8(0X0112, 0X0A);
	write_cmos_sensor_8(0X0113, 0X0A);
	write_cmos_sensor_8(0X0114, 0X03);
	write_cmos_sensor_8(0X3906, 0X00);
	write_cmos_sensor_8(0X0344, 0X01);
	write_cmos_sensor_8(0X0345, 0X68);
	write_cmos_sensor_8(0X0346, 0X02);
	write_cmos_sensor_8(0X0347, 0X00);
	write_cmos_sensor_8(0X0348, 0X0B);
	write_cmos_sensor_8(0X0349, 0X67);
	write_cmos_sensor_8(0X034A, 0X07);
	write_cmos_sensor_8(0X034B, 0X9F);
	write_cmos_sensor_8(0X034C, 0X05);
	write_cmos_sensor_8(0X034D, 0X00);
	write_cmos_sensor_8(0X034E, 0X02);
	write_cmos_sensor_8(0X034F, 0XD0);
	write_cmos_sensor_8(0X0900, 0X01);
	write_cmos_sensor_8(0X0901, 0X22);
	write_cmos_sensor_8(0X0381, 0X01);
	write_cmos_sensor_8(0X0383, 0X01);
	write_cmos_sensor_8(0X0385, 0X01);
	write_cmos_sensor_8(0X0387, 0X03);
	write_cmos_sensor_8(0X0101, 0X00);
	write_cmos_sensor_8(0X0340, 0X09);
	write_cmos_sensor_8(0X0341, 0XE2);
	write_cmos_sensor_8(0X0342, 0X0E);
	write_cmos_sensor_8(0X0343, 0X68);
	write_cmos_sensor_8(0X0200, 0X0D);
	write_cmos_sensor_8(0X0201, 0XD8);
	write_cmos_sensor_8(0X0202, 0X02);
	write_cmos_sensor_8(0X0203, 0X08);
	write_cmos_sensor_8(0x0100, 0x01);
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor(0x0100, 0x0100);
	else
		write_cmos_sensor(0x0100, 0x0000);
	return ERROR_NONE;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	/* sensor have two i2c address 0x6c*/
	/*0x6d & 0x21 0x20, we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
			pr_debug("i2c write id: 0x%x, sensor id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, i2c_write_id: 0x%x\n",
				imgsensor.i2c_write_id);
				pr_debug("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	/* const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2}; */
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;
	/* sensor have two i2c address 0x20,0x5a  we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, i2c write id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	if (!(update_otp()))
		LOG_INF("s5k4h7 Demon_otp update_otp error!");
	sensor_init();
	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;	/*  */
	imgsensor.gain = 0x100;	/*  */
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*      open  */

static kal_uint32 close(void)
{

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*      close  */


static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}				/*      preview   */

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	/* imgsensor.current_fps = 240; */
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		/* PIP capture:15fps */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {		/* PIP capture: 24fps */
		imgsensor.pclk = imgsensor_info.cap2.pclk;
		imgsensor.line_length = imgsensor_info.cap2.linelength;
		imgsensor.frame_length = imgsensor_info.cap2.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	set_mirror_flip(imgsensor.mirror);


	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);

	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{


	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}				/*      slim_video       */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{

	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}				/*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/* LOG_INF("scenario_id = %d\n", scenario_id); */



	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;


	/*0: no support, 1: G0,R0.B0, 2: G0,R0.B1, 3: G0,R1.B0, 4: G0,R1.B1 */
	/*                    5: G1,R0.B0, 6: G1,R0.B1, 7: G1,R1.B0, 8: G1,R1.B1 */
	sensor_info->ZHDR_Mode = 5;


	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
		    imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*      get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	/* LOG_INF("framerate = %d\n ", framerate); */
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
						MUINT32 framerate)
{
	kal_uint32 frame_length;

	/* LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate); */

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
		    imgsensor_info.normal_video.pclk / framerate * 10 /
		    imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.normal_video.framelength)
			    ? (frame_length - imgsensor_info.normal_video.framelength) : 0;

		imgsensor.frame_length =
		    imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
			frame_length =
			    imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength)
				? (frame_length - imgsensor_info.cap1.framelength) : 0;

			imgsensor.frame_length =
				imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
			frame_length =
			    imgsensor_info.cap2.pclk / framerate * 10 /
					imgsensor_info.cap2.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap2.framelength)
			    ? (frame_length - imgsensor_info.cap2.framelength) : 0;

			imgsensor.frame_length =
				imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
				LOG_INF("Warning: cur_fps %d fps is not support\n",
				framerate);
				LOG_INF("Warning:use cap's setting: %d fps!\n",
							imgsensor_info.cap.max_framerate / 10);
			}
			frame_length =
				imgsensor_info.cap.pclk / framerate * 10 /
						imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
				? (frame_length - imgsensor_info.cap.framelength) : 0;

			imgsensor.frame_length =
				imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
			imgsensor_info.hs_video.pclk / framerate * 10 /
						imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.hs_video.framelength)
		    ? (frame_length - imgsensor_info.hs_video.framelength) : 0;

		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
			imgsensor_info.slim_video.pclk / framerate * 10 /
					imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength) : 0;

		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:		/* coding with  preview scenario by default */
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
					imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.pre.framelength)
		    ? (frame_length - imgsensor_info.pre.framelength) : 0;

		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
						    MUINT32 *framerate)
{

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_uint32 modes,
	struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	kal_uint16 Color_R, Color_Gr, Color_Gb, Color_B;

	LOG_INF("set_test_pattern modes: %d,\n", modes);

	if (modes) {
		write_cmos_sensor_8(0x0100, 0x00);
		write_cmos_sensor_8(0x0601, modes);
		if (modes == 1 && (pdata != NULL)) { //Solid Color
			LOG_INF("R=0x%x,Gr=0x%x,B=0x%x,Gb=0x%x",
				pdata->COLOR_R, pdata->COLOR_Gr, pdata->COLOR_B, pdata->COLOR_Gb);
			Color_R = (pdata->COLOR_R >> 16) & 0xFFFF;
			Color_Gr = (pdata->COLOR_Gr >> 16) & 0xFFFF;
			Color_B = (pdata->COLOR_B >> 16) & 0xFFFF;
			Color_Gb = (pdata->COLOR_Gb >> 16) & 0xFFFF;
			write_cmos_sensor_8(0x0602, Color_R >> 8);
			write_cmos_sensor_8(0x0603, Color_R & 0xFF);
			write_cmos_sensor_8(0x0604, Color_Gr >> 8);
			write_cmos_sensor_8(0x0605, Color_Gr & 0xFF);
			write_cmos_sensor_8(0x0606, Color_B >> 8);
			write_cmos_sensor_8(0x0607, Color_B & 0xFF);
			write_cmos_sensor_8(0x0608, Color_Gb >> 8);
			write_cmos_sensor_8(0x0609, Color_Gb & 0xFF);
			write_cmos_sensor_8(0x0100, 0x01);
		}
	} else
		write_cmos_sensor(0x0601, 0x00); /*No pattern*/

	write_cmos_sensor_8(0x3200, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	/* unsigned long long *feature_return_para*/
	/*=(unsigned long long *) feature_para; */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/* LOG_INF("feature_id = %d", feature_id); */
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		if ((sensor_reg_data->RegData >> 8) > 0)
			write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		else
			write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL) * feature_data_16,
				*(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM) *feature_data,
					      *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
						  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((UINT32)*feature_data,
		(struct SET_SENSOR_PATTERN_SOLID_COLOR *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (MUINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
		/* zhdr,wdrs */
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("hdr mode :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		/*imgsensor.hdr_mode = (UINT8)*feature_data_32; */
		imgsensor.ihdr_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
		/* imgsensor.hdr_mode = 9;*/
		/*force set hdr_mode to zHDR */
		/* LOG_INF("hdr mode :%d\n", imgsensor.hdr_mode); */
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (MUINT32) *feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
			       sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d,SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data, (UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n", (UINT16) *feature_data,
			(UINT16) *(feature_data + 1));
		hdr_write_shutter((UINT16) *feature_data, (UINT16) *(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME\n");
		streaming_control(KAL_TRUE);
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/*      feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K4H7_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      S5K4H7_MIPI_RAW_SensorInit  */

