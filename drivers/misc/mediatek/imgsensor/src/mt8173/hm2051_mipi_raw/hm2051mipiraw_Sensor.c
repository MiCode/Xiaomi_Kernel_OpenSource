/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 HM2051mipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
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
#include <linux/types.h>

#include "kd_camera_typedef.h"
/* #include "kd_camera_hw.h" */
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "hm2051mipiraw_Sensor.h"

#define PFX "hm2051_camera_sensor"
#define LOG_DBG(format, args...) \
	pr_debug(PFX ":%s()" format, __func__, ##args)
 #define LOG_MUSTSHOW(format, args...) \
	pr_err(PFX ":%s()" format, __func__, ##args)

 /* serialize sensor's ioctl commands */
static DEFINE_MUTEX(input_lock);

#define MIPI_SETTLEDELAY_AUTO 0
#define MIPI_SETTLEDELAY_MANNUAL 1

static imgsensor_info_struct imgsensor_info = {
	.sensor_id = HM2051MIPI_SENSOR_ID,

	.checksum_value = 0x523c51f6,

	.pre = {
		.pclk = 88000000,
		.linelength = 2310,
		.framelength = 1266,
		.startx = 0,	/* startx of grabwindow */
		.starty = 0,	/* starty of grabwindow */
		.grabwindow_width = 1616,
		.grabwindow_height = 1216,
		/* for MIPIDataLowPwr2HighSpeedSettleDelayCount */
		.mipi_data_lp2hs_settle_dc = 60, /*11,*/	/*unit , ns */
		/* for GetDefaultFramerateByScenario() */
		.max_framerate = 300,
		},
	.cap = {
		.pclk = 88000000,
		.linelength = 2310,
		.framelength = 1266,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1616,
		.grabwindow_height = 1216,
		.mipi_data_lp2hs_settle_dc = 60, /*11,*/
		.max_framerate = 300,
		},
	/* capture for PIP 24fps relative information,
	 * capture1 mode must use same framelength, linelength
	 * with Capture mode for shutter calculate
	 */
	.cap1 = {
		 .pclk = 88000000,
		 .linelength = 2310,
		 .framelength = 1266,
		 .startx = 0,
		 .starty = 0,
		 .grabwindow_width = 1616,
		 .grabwindow_height = 1216,
		 .mipi_data_lp2hs_settle_dc = 60, /*11,*/
		/*less than 13M(include 13M),cap1 max framerate is 24fps,
		 * 16M max framerate is 20fps, 20M max framerate is 15fps
		 */
		 .max_framerate = 300,
		 },
	.normal_video = {
			 .pclk = 88000000,
			 .linelength = 2310,
			 .framelength = 1266,
			 .startx = 1,
			 .starty = 0,
			 .grabwindow_width = 1616,
			 .grabwindow_height = 1216,
			 .mipi_data_lp2hs_settle_dc = 60, /*11,*/
			 .max_framerate = 300,
			 },
	.hs_video = {
		     .pclk = 88000000,
		     .linelength = 2310,
		     .framelength = 1266,
		     .startx = 1,
		     .starty = 0,
		     .grabwindow_width = 1616,
		     .grabwindow_height = 1216,
		     .mipi_data_lp2hs_settle_dc = 60, /*11,*/
		     .max_framerate = 300,
		     },
	.slim_video = {
		       .pclk = 88000000,
		       .linelength = 2310,
		       .framelength = 1266,
		       .startx = 1,
		       .starty = 0,
		       .grabwindow_width = 1616,
		       .grabwindow_height = 1216,
		       .mipi_data_lp2hs_settle_dc = 60, /*11,*/
		       .max_framerate = 300,
		       },
	.margin = 5,		/* shutter margin */
	.min_shutter = 4,	/* min shutter */
	.max_frame_length = 0x7fff,	/* sensor register's limitation */
	/* shutter delay frame for AE cycle,
	 * 2 frame with ispGain_delay-shut_delay=2-0=2
	 */
	.ae_shut_delay_frame = 0,
	/* sensor gain delay frame for AE cycle,
	 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	 */
	.ae_sensor_gain_delay_frame = 0,
	/* isp gain delay frame for AE cycle */
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 5,	/* support sensor mode num */

	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,	/*enter high speed video  delay */
	.slim_video_delay_frame = 2,	/*enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_8MA,	/*mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	/*0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	/*0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANNUAL, /*MIPI_SETTLEDELAY_AUTO,*/
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,		/* suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_1_LANE,	/* mipi lane num */
	/*record sensor support all write id addr,
	 * only supprt 4must end with 0xff
	 */
	.i2c_addr_table = {0x48, 0xff},
};

static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x300,	/* current shutter */
	.gain = 0x0040,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 0,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	/*sensor need support LE, SE with HDR feature */
	.ihdr_en = 0,
	.i2c_write_id = 0x48,	/*record current sensor's i2c write id */
};

/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
{1616, 1216, 0, 0, 1616, 1216, 1296, 972, 0000, 0000, 1296, 972, 1, 0, 1280, 960},
{1616, 1216, 0, 0, 1616, 1216, 1616, 1216, 0000, 0000, 1616, 1216, 1, 0, 1600, 1200},
{1616, 1216, 0, 0, 1616, 1216, 1296, 972, 0000, 0000, 1296, 972, 1, 0, 1280, 960},
{1616, 1216, 0, 0, 1616, 1216, 640, 480, 0000, 0000, 640, 480, 1, 0, 640, 480},
{1616, 1216, 0, 0, 1616, 1216, 1296, 972, 0000, 0000, 1296, 972, 1, 0, 1280, 960}
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint8 get_byte = 0;
	__be16 pu_send_cmd1 = cpu_to_be16(addr);

	iReadRegI2C((u8 *)&pu_send_cmd1, sizeof(pu_send_cmd1), (u8 *)&get_byte,
		sizeof(get_byte), imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	struct __packed {
		__be16 addr;
		u8 para;
	} pu_send_cmd;

	pu_send_cmd.addr = cpu_to_be16(addr);
	pu_send_cmd.para = para & 0xFF;
	iWriteRegI2C((u8 *)&pu_send_cmd, sizeof(pu_send_cmd),
		imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_DBG("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line,
		imgsensor.dummy_pixel);
	/* set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel,
	 * or set dummy by imgsensor.frame_length and imgsensor.line_length
	 */

}				/* set_dummy */

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_DBG("framerate = %d, min framelength should enable? %d\n",
		framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	imgsensor.frame_length =
	    (frame_length > imgsensor.min_frame_length) ?
	    frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line =
	    imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line =
		    imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	set_dummy();
}				/* set_max_framerate */

/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	/* if shutter bigger than frame_length, should extend frame length
	 * first
	 */
	imgsensor.shutter = shutter;

	if (shutter > imgsensor.min_frame_length)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	imgsensor.frame_length &= ~1;
	shutter = (shutter < imgsensor_info.min_shutter) ?
	    imgsensor_info.min_shutter : shutter;
	shutter =
	    (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
	    (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
	shutter &= ~1;

	/* Update Shutter */
	write_cmos_sensor(0x0015, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0016, shutter & 0xFF);
	LOG_DBG("shutter =%d, framelength =%d\n", shutter,
		imgsensor.frame_length);

	/* HM2051_COMMAND_UPDATE */
	write_cmos_sensor(0x0100, 1);
}				/* set_shutter */

/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	/* set analog gain only, isp would handle digital gain */

	/*max AgainX100 (3100) * max Dgain (iMaxDgainX100) */
	int gain_base = 32, maxgain_x100 = 6169;
	int gain_x100 = (gain * 100) / gain_base;
	int again_reg = 0, fine_again_reg = 0, fine_again_base = 16;
	/*max_again_reg and max_again_reg_pow_x100   */
	int again_reg_pow_x100 = 100, max_again_reg = 3;
	int max_again_reg_pow_x100 = 800;
	int iTemp;

	if (gain < 0)
		return -EINVAL;

	/* gain = Analog gain * Digitgain */
	/* [8*(1+15/16)]*[(3.98)] = [31/2]*[3.98] */
	/* Analog gain */
	/* get the approach Analog gain base on gain */
	/* Adgain = 2^again_reg*(1+fine_again_reg/fine_again_base) */
	LOG_DBG("analog gain 0x%x, gain_x100 %d\n", gain,
		gain_x100);
	gain_x100 = gain_x100 > maxgain_x100 ? maxgain_x100 :
		(gain_x100 < 100 ? 100 : gain_x100);

	iTemp = gain_x100 / 200;
	while (iTemp > 0) {
		again_reg += 1;
		iTemp >>= 1;
		again_reg_pow_x100 *= 2;
	}

	again_reg = again_reg > max_again_reg ? max_again_reg : again_reg;
	again_reg_pow_x100 =
		again_reg_pow_x100 > max_again_reg_pow_x100 ?
		max_again_reg_pow_x100 : again_reg_pow_x100;
	iTemp = (fine_again_base * gain_x100) /
		again_reg_pow_x100 - fine_again_base;

	fine_again_reg =
	    iTemp < 0 ? 0 : (iTemp >=
			     fine_again_base ? fine_again_base - 1 : iTemp);
	gain = (again_reg) + (fine_again_reg << 4);
	LOG_DBG("again=0x%X, again_reg=%d, fine_again_reg=%d\n",
		gain, again_reg, fine_again_reg);

	imgsensor.gain = gain;

	write_cmos_sensor(0x0018, gain);	/*gain & 0xff */

	/* HM2051_COMMAND_UPDATE */
	write_cmos_sensor(0x0100, 1);

	return gain;
}				/*      set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se,
				    kal_uint16 gain)
{
}

/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}				/* night_mode */

static void sensor_init(void)
{
	LOG_MUSTSHOW("HM2051_Sensor_Init +\n");

	/*
	 * hm2051 basic preview procedure
	 * 1. stop stream(reg 0x0005 = 0x02)
	 * 2. delay one frame
	 * 3. download preview settings
	 * 4. streaming ( reg 0x0005 = 0x01)
	 */

	write_cmos_sensor(0x0022, 0x00);
	write_cmos_sensor(0x0000, 0x00);
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x0101, 0x00);
	write_cmos_sensor(0x0005, 0x02);
	write_cmos_sensor(0x0024, 0x40);
	/* 1 frame time is ~33ms in 30fps stream */
	msleep(34);
	write_cmos_sensor(0x0026, 0x0B);
	write_cmos_sensor(0x002A, 0x57);
	write_cmos_sensor(0x0006, 0x00);
	write_cmos_sensor(0x000F, 0x00);
	write_cmos_sensor(0x0027, 0x23);
	write_cmos_sensor(0x0065, 0x01);
	write_cmos_sensor(0x0074, 0x13);
	write_cmos_sensor(0x002B, 0x00);
	write_cmos_sensor(0x002C, 0x06);
	write_cmos_sensor(0x0040, 0x0A);
	write_cmos_sensor(0x0044, 0x03);
	write_cmos_sensor(0x0045, 0x63);
	write_cmos_sensor(0x0046, 0x5F);
	write_cmos_sensor(0x0049, 0xC0);
	write_cmos_sensor(0x004B, 0x03);
	write_cmos_sensor(0x0070, 0x2F);
	write_cmos_sensor(0x0072, 0xFB);
	write_cmos_sensor(0x0073, 0x77);
	write_cmos_sensor(0x0075, 0x40);
	write_cmos_sensor(0x0078, 0x65);
	write_cmos_sensor(0x0080, 0x98);
	write_cmos_sensor(0x0082, 0x09);
	write_cmos_sensor(0x0083, 0x3C);
	write_cmos_sensor(0x0087, 0x41);
	write_cmos_sensor(0x008D, 0x20);
	write_cmos_sensor(0x008E, 0x30);
	write_cmos_sensor(0x009D, 0x11);
	write_cmos_sensor(0x009E, 0x12);
	write_cmos_sensor(0x0090, 0x00);
	write_cmos_sensor(0x0091, 0x01);
	write_cmos_sensor(0x0092, 0x02);
	write_cmos_sensor(0x0093, 0x03);
	write_cmos_sensor(0x00C0, 0x64);
	write_cmos_sensor(0x00C1, 0x15);
	write_cmos_sensor(0x00C2, 0x00);
	write_cmos_sensor(0x00C3, 0x02);
	write_cmos_sensor(0x00C4, 0x0B);
	write_cmos_sensor(0x00C6, 0x83);
	write_cmos_sensor(0x00C7, 0x02);
	write_cmos_sensor(0x00CC, 0x00);
	write_cmos_sensor(0x4B3B, 0x12);
	write_cmos_sensor(0x4B41, 0x10);
	write_cmos_sensor(0x0165, 0x03);
	write_cmos_sensor(0x018C, 0x00);
	write_cmos_sensor(0x0195, 0x06);
	write_cmos_sensor(0x0196, 0x4F);
	write_cmos_sensor(0x0197, 0x04);
	write_cmos_sensor(0x0198, 0xBF);
	write_cmos_sensor(0x0144, 0x12);
	write_cmos_sensor(0x0140, 0x20);
	write_cmos_sensor(0x015A, 0x80);
	write_cmos_sensor(0x015D, 0x20);
	write_cmos_sensor(0x0160, 0x65);
	write_cmos_sensor(0x0123, 0xC5);
	write_cmos_sensor(0x4B50, 0x08);
	write_cmos_sensor(0x4B51, 0xE2);
	write_cmos_sensor(0x4B0A, 0x06);
	write_cmos_sensor(0x4B0B, 0x50);
	write_cmos_sensor(0x4B20, 0x9E);
	write_cmos_sensor(0x4B07, 0xBD);
	write_cmos_sensor(0x4B30, 0x0E);
	write_cmos_sensor(0x4B30, 0x0F);
	write_cmos_sensor(0x0000, 0x00);
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x0101, 0x00);
	/*write_cmos_sensor(0x0005, 0x03);*/
	write_cmos_sensor(0x0005, 0x02); /* only power on, not streaming */
	write_cmos_sensor(0x0025, 0x00);
	LOG_MUSTSHOW("HM2051_Sensor_Init -\n");

}				/* sensor_init */

static void preview_setting(void)
{
	LOG_DBG("enter\n");
	write_cmos_sensor(0x0005, 0x03); /* start streaming */
}				/* preview_setting */

static void capture_setting(kal_uint16 currefps)
{
	LOG_DBG("enter! currefps:%d\n", currefps);
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_DBG("enter! currefps:%d\n", currefps);
}

static void hs_video_setting(void)
{
	LOG_DBG("enter!\n");
}

static void slim_video_setting(void)
{
	LOG_DBG("enter!\n");
}

/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID
*
* PARAMETERS
*	*sensorID : return the sensor ID
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id =
			    ((read_cmos_sensor(0x0001) << 8) |
			     read_cmos_sensor(0x0002));
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_MUSTSHOW
				    ("addr: 0x%x, sensor id: 0x%x\n",
				     imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_MUSTSHOW
			    ("fail, addr: 0x%x, sensor id: 0x%x\n",
			     imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to
		 * 0xFFFFFFFF
		 */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	LOG_DBG("enter\n");

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id =
			    ((read_cmos_sensor(0x0001) << 8) |
			     read_cmos_sensor(0x0002));
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_MUSTSHOW
				    ("addr: 0x%x, sensor id: 0x%x\n",
				     imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_MUSTSHOW
			    ("fail, Addr: 0x%x, sensor id: 0x%x\n",
			     imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init();

	imgsensor.shutter = 0x300;
	imgsensor.gain = 0x0040;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.ihdr_en = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;

	return ERROR_NONE;
}				/*open */

/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_DBG("enter\n");

	/* No Need to implement this function */

	return ERROR_NONE;
}				/* close */

/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("enter\n");

	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	preview_setting();
	return ERROR_NONE;
}				/* preview */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("enter\n");
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_DBG
			("Warning: current_fps %d is not support, so use %d!\n",
			     imgsensor.current_fps,
			     imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}

	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
			       image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("enter\n");

	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	normal_video_setting(imgsensor.current_fps);

	return ERROR_NONE;
}				/* normal_video */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("enter\n");

	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	hs_video_setting();

	return ERROR_NONE;
}				/* hs_video */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("enter\n");

	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	slim_video_setting();

	return ERROR_NONE;
}				/* slim_video */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *
				 sensor_resolution)
{
	LOG_DBG("enter\n");
	sensor_resolution->SensorFullWidth =
	    imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
	    imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
	    imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
	    imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
	    imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
	    imgsensor_info.normal_video.grabwindow_height;

	sensor_resolution->SensorHighSpeedVideoWidth =
	    imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
	    imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
	    imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
	    imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}				/* get_resolution */

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	/* not use */
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	/* inverse with datasheet */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;

	sensor_info->SensorOutputDataFormat =
	    imgsensor_info.sensor_output_dataformat;
	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
	    imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
	    imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
	    imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
	    imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

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
		sensor_info->SensorGrabStartX =
		    imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY =
		    imgsensor_info.normal_video.starty;
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
		sensor_info->SensorGrabStartX =
		    imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY =
		    imgsensor_info.slim_video.starty;
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

static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_DBG("scenario_id = %d\n", scenario_id);
	imgsensor.current_scenario_id = scenario_id;
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
		LOG_DBG("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_DBG("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_DBG("enable = %d, framerate = %d\n", enable, framerate);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM
						scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 /
		    imgsensor_info.pre.linelength;
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.pre.framelength) ?
		    (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
		    imgsensor_info.normal_video.pclk / framerate * 10 /
		    imgsensor_info.normal_video.linelength;
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.normal_video.framelength) ?
		    (frame_length - imgsensor_info.normal_video.framelength) :
		    0;
		imgsensor.frame_length =
		    imgsensor_info.normal_video.framelength +
		    imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			imgsensor_info.cap1.max_framerate) {
			frame_length =
			    imgsensor_info.cap1.pclk / framerate * 10 /
			    imgsensor_info.cap1.linelength;
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap1.framelength) ?
			    (frame_length - imgsensor_info.cap1.framelength) :
			    0;
			imgsensor.frame_length =
			    imgsensor_info.cap1.framelength +
			    imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
		} else {
			if (imgsensor.current_fps !=
			    imgsensor_info.cap.max_framerate)
				LOG_DBG
				("Warning: not support %d, use %d!\n",
				     framerate,
				     imgsensor_info.cap.max_framerate / 10);
			frame_length =
			    imgsensor_info.cap.pclk / framerate * 10 /
			    imgsensor_info.cap.linelength;
			imgsensor.dummy_line =
			    (frame_length > imgsensor_info.cap.framelength) ?
			    (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
			    imgsensor_info.cap.framelength +
			    imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
		}
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
		    imgsensor_info.hs_video.pclk / framerate * 10 /
		    imgsensor_info.hs_video.linelength;
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.hs_video.framelength) ?
		    (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		/* set_dummy(); */
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
		    imgsensor_info.slim_video.pclk / framerate * 10 /
		    imgsensor_info.slim_video.linelength;
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.slim_video.framelength) ?
		    (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.slim_video.framelength +
		    imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		/* set_dummy(); */
		break;
	default:		/* coding with  preview scenario by default */
		frame_length =
		    imgsensor_info.pre.pclk / framerate * 10 /
		    imgsensor_info.pre.linelength;
		imgsensor.dummy_line =
		    (frame_length > imgsensor_info.pre.framelength) ?
		    (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
		    imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		/* set_dummy(); */
		LOG_DBG("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM
						    scenario_id,
						    MUINT32 *framerate)
{
	LOG_DBG("scenario_id = %d\n", scenario_id);

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

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_DBG("enable: %d\n", enable);

	/* TODO: set sensor register */
	imgsensor.test_pattern = enable;
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para,
				  UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
	    (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_DBG("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_DBG("pclk=%d, current_fps=%d\n",
		     imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) *feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr,
				  sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
		    read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return
		 * LENS_DRIVER_ID_DO_NOT_CARE if EEPROM does not
		 * exist in camera module.
		 */
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
		set_auto_flicker_mode((BOOL) *feature_data_16,
				      *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(MSDK_SCENARIO_ID_ENUM) *feature_data,
			*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(MSDK_SCENARIO_ID_ENUM) *(feature_data),
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) *feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_DBG("current fps :%d\n", (UINT32) *feature_data);
		imgsensor.current_fps = *feature_data;
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_DBG("ihdr enable :%d\n", (BOOL) *feature_data);
		imgsensor.ihdr_en = (BOOL) *feature_data;
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_DBG("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);
		wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)
				(uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[1],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[2],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[3],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[4],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
			       (void *)&imgsensor_winsize_info[0],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_DBG("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain((UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
		break;
	default:
		break;
	}

	return ERROR_NONE;
}				/* feature_control() */

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 HM2051_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do: Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*      HM2051_MIPI_RAW_SensorInit      */
