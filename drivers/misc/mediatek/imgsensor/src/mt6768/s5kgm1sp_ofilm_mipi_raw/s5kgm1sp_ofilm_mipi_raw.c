/*****************************************************************************
 *
 *Filename:
 *---------
 *   s5kgm1sp_ofilm_mipi_raw.c
 *
 *Project:
 *--------
 *   ALPS MT6768
 *
 *Description:
 *------------
 *   Source code of Sensor driver
 *------------------------------------------------------------------------------
 *Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
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

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"
#include "s5kgm1sp_ofilm_mipi_raw.h"

#define FPTPDAFSUPPORT

#define MULTI_WRITE 1

#define PFX "s5kgm1sp_ofilm_mipi_raw"

#define LOG_INF(format, args...)    pr_err(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5KGM1SP_OFILM_SENSOR_ID,
	.checksum_value = 0x42d95d37,
	.pre = {
		.pclk = 482000000,
		.linelength = 5024,
		.framelength = 3194,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 460800000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 482000000,
		.linelength = 5024,
		.framelength = 3194,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 460800000,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 482000000,
		.linelength = 5024,
		.framelength = 3194,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 460800000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 482000000,
		.linelength = 5024,
		.framelength = 3194,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4000,
		.grabwindow_height = 3000,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 460800000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 482000000,
		.linelength = 2512,
		.framelength = 1598,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 476800000,
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 492000000,
		.linelength = 5024,
		.framelength = 816,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 145600000,
		.max_framerate = 1200,
	},
	.custom1 = {
		.pclk = 482000000,
		.linelength = 2514,
		.framelength = 6388,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2000,
		.grabwindow_height = 1500,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 476800000,
		.max_framerate = 300,
	},
	.custom2 = {
		.pclk = 492000000,
		.linelength = 5024,
		.framelength = 816,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 145600000,
		.max_framerate = 1200,
	},
	.margin = 5,
	.min_shutter = 5,
	.max_frame_length = 0xFFFF,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.frame_time_delay_frame = 1,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 7,
	.cap_delay_frame = 3,
	.pre_delay_frame = 3,
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	.custom1_delay_frame = 3,
	.custom2_delay_frame = 3,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = 1,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x5A, 0xff},
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x0200,
	.gain = 0x0100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 0,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = KAL_FALSE,
	.i2c_write_id = 0x5A,
//cxc long exposure >
	.current_ae_effective_frame = 2,
//cxc long exposure <
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {

	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x07D0, 0x05DC, 0x01, 0x00, 0x0000, 0x0000,
		0x01, 0x30, 0x026C, 0x02E0, 0x03, 0x00, 0x0000, 0x0000
	},

	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x07D0, 0x05DC, 0x01, 0x00, 0x0000, 0x0000,
		0x01, 0x30, 0x026C, 0x02E0, 0x03, 0x00, 0x0000, 0x0000
	},

	{
		0x02, 0x0A, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2B, 0x0FA0, 0x0BB8, 0x01, 0x00, 0x0000, 0x0000,
		0x01, 0x30, 0x026C, 0x02E0, 0x03, 0x00, 0x0000, 0x0000
	}
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[7] = {
	{
		4000, 3000, 0, 0, 4000, 3000, 4000, 3000, 0, 0, 4000, 3000,
		0, 0, 4000, 3000
	},
	{
		4000, 3000, 0, 0, 4000, 3000, 4000, 3000, 0, 0, 4000, 3000,
		0, 0, 4000, 3000
	},
	{
		4000, 3000, 0, 0, 4000, 3000, 4000, 3000, 0, 0, 4000, 3000,
		0, 0, 4000, 3000
	},
	{
		4000, 3000, 80, 420, 3840, 2160, 1920, 1080, 0, 0, 1920, 1080,
		0, 0, 1920, 1080
	},
	{
		4000, 3000, 80, 420, 3840, 2160, 1280, 720, 0, 0, 1280, 720,
		0, 0, 1280, 720
	},
	{
		4000, 3000, 0, 0, 4000, 3000, 2000, 1500, 0, 0, 2000, 1500,
		0, 0, 2000, 1500
	},
	{
		4000, 3000, 80, 420, 3840, 2160, 1280, 720, 0, 0, 1280, 720,
		0, 0, 1280, 720
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 16,
	.i4OffsetY = 28,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum = 16,
	.i4SubBlkW = 8,
	.i4SubBlkH = 8,
	.i4PosL = {{18, 29}, {26, 29}, {34, 29}, {42, 29}, {22, 41}, {30, 41}, {
			38,
			41
		},
		{46, 41}, {18, 49}, {26, 49}, {34, 49}, {42, 49}, {
			22,
			53
		}, {
			30,
			53
		},
		{38, 53}, {46, 53}
	},
	.i4PosR = {{18, 33}, {26, 33}, {34, 33}, {42, 33}, {22, 37}, {30, 37}, {
			38,
			37
		},
		{46, 37}, {18, 45}, {26, 45}, {34, 45}, {42, 45}, {
			22,
			57
		}, {
			30,
			57
		},
		{38, 57}, {46, 57}
	},
	.iMirrorFlip = 0,
	.i4BlockNumX = 124,
	.i4BlockNumY = 92,
};


#if MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3
#endif
static kal_uint16 s5kgm1sp_ofilm_table_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;
	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE
		if ((I2C_BUFFER_LEN - tosend) < 3 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				3, imgsensor_info.i2c_speed);

			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}


static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1,
				imgsensor.i2c_write_id);
	return get_byte;
}

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1,
				imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_byte(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
	 (char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8),
		(char)(para & 0xFF)
	};

	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line,
			imgsensor.dummy_pixel);

	write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	write_cmos_sensor(0x0342, imgsensor.line_length & 0xFFFF);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable(%d)\n",
			framerate, min_framelength_en);
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length =
		(frame_length >
		 imgsensor.min_frame_length)
		  ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line =
		imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line =
			imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

#if 1
static void check_streamoff(void)
{
	unsigned int i = 0;
	int timeout = (10000 / imgsensor.current_fps) + 1;

	mdelay(3);
	for (i = 0; i < timeout; i++) {
		if (read_cmos_sensor_8(0x0005) != 0xFF)
			mdelay(1);
		else
			break;
	}
	LOG_INF(" check_streamoff exit!\n");
}

static kal_uint32 streaming_control(kal_bool enable)
{
	unsigned int i = 0;

	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {

		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor_byte(0x0100, 0X01);
		for (i = 0; i < 5; i++) {
			pr_debug("%s streaming check is %d", __func__,
					 read_cmos_sensor_8(0x0005));
			pr_debug("%s streaming check 0x0100 is %d", __func__,
					 read_cmos_sensor_8(0x0100));
			pr_debug("%s streaming check 0x6028 is %d", __func__,
					 read_cmos_sensor(0x6028));
			mdelay(1);
		}
	} else {
		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor_byte(0x0100, 0x00);
		check_streamoff();
	}
	return ERROR_NONE;
}
#endif
#if 0
static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;

	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter =
		(shutter < imgsensor_info.min_shutter)
		  ? imgsensor_info.min_shutter : shutter;
	shutter =
		(shutter >
		 (imgsensor_info.max_frame_length -
		 imgsensor_info.margin)) ? (imgsensor_info.max_frame_length
				 - imgsensor_info.margin) : shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps =
			imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
	} else {

		write_cmos_sensor(0x0340, imgsensor.frame_length & 0xFFFF);
	}

	write_cmos_sensor(0x0202, (shutter) & 0xFFFF);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,
			imgsensor.frame_length);

}
#endif

//cxc long exposure >
static bool bNeedSetNormalMode = KAL_FALSE;
#define SHUTTER_1		95932//30396
#define SHUTTER_2		191864//60792
#define SHUTTER_4		383729//56049
#define SHUTTER_8		767459//46563
#define SHUTTER_16		1534919//27591
#define SHUTTER_32		3069838

static void check_output_stream_off(void)
{
	kal_uint16 read_count = 0, read_register0005_value = 0;

	for (read_count = 0; read_count <= 4; read_count++) {
		read_register0005_value = read_cmos_sensor_8(0x0005);

		if (read_register0005_value == 0xff)
			break;
		mdelay(50);

		if (read_count == 4)
			LOG_INF("cxc stream off error\n");
	}

}
//cxc long exposure <

/*************************************************************************
*FUNCTION
*  set_shutter
*
*DESCRIPTION
*  This function set e-shutter of sensor to change exposure time.
*
*PARAMETERS
*  iShutter : exposured lines
*
*RETURNS
*  None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_uint32 pre_shutter = 2877;

	LOG_INF("cxc enter  shutter = %d\n", shutter);

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	if (shutter < 95932) {
		if (bNeedSetNormalMode) {
			LOG_INF("exit long shutter\n");
			write_cmos_sensor(0x6028, 0x4000);
			write_cmos_sensor(0x0100, 0x0000); //stream off
			write_cmos_sensor(0x0334, 0x0000);
			write_cmos_sensor(0x0E0A, 0x0000);
			write_cmos_sensor(0x0E0C, 0x0000);
			write_cmos_sensor(0x0E0E, 0x0000);
			write_cmos_sensor(0x0E10, 0x0000);
			write_cmos_sensor(0x0E12, 0x0000);
			write_cmos_sensor(0x0E14, 0x0000);
			write_cmos_sensor(0x0E16, 0x0000);
			write_cmos_sensor(0x0704, 0x0000);
			write_cmos_sensor(0x0100, 0x0100); //stream on
			bNeedSetNormalMode = KAL_FALSE;
			imgsensor.current_ae_effective_frame = 2;
		}
		pre_shutter = shutter;

		spin_lock(&imgsensor_drv_lock);
		if (shutter > imgsensor.min_frame_length -
				imgsensor_info.margin)
			imgsensor.frame_length = shutter +
				imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
				imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		shutter =
			(shutter < imgsensor_info.min_shutter)
			 ? imgsensor_info.min_shutter : shutter;
		shutter =
			(shutter >
			 (imgsensor_info.max_frame_length -
			 imgsensor_info.margin)) ?
			 (imgsensor_info.max_frame_length -
			 imgsensor_info.margin) : shutter;
		if (imgsensor.autoflicker_en) {
			realtime_fps =
				imgsensor.pclk / imgsensor.line_length * 10 /
				imgsensor.frame_length;
			if (realtime_fps >= 297 && realtime_fps <= 305)
				set_max_framerate(296, 0);
			else if (realtime_fps >= 147 && realtime_fps <= 150)
				set_max_framerate(146, 0);
			else {
				write_cmos_sensor(0x0340,
					  imgsensor.frame_length & 0xFFFF);
			}
		} else {

			write_cmos_sensor(0x0340,
					imgsensor.frame_length & 0xFFFF);
		}

		write_cmos_sensor(0X0202, shutter & 0xFFFF);
		LOG_INF("cxc 2 enter  shutter = %d\n", shutter);

	} else {

		LOG_INF("cxc enter long shutter\n");
		bNeedSetNormalMode = KAL_TRUE;
		imgsensor.ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		imgsensor.ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		imgsensor.current_ae_effective_frame = 2;

		write_cmos_sensor(0x6028, 0x4000);
		write_cmos_sensor(0x0100, 0x0000); //stream off
		check_output_stream_off();
		write_cmos_sensor(0x0334, 0x0001);
		write_cmos_sensor(0x0E0A, 0x0002);
		write_cmos_sensor(0x0E0C, 0x0100);
		write_cmos_sensor(0x0E0E, 0x0003);

		write_cmos_sensor(0x0E10, pre_shutter); //1st frame
		write_cmos_sensor(0x0E12, imgsensor.gain); //aGain 1st frame

		switch (shutter) {
		case SHUTTER_1:
			LOG_INF("cxc shutter 1\n");
			write_cmos_sensor(0x0E14, 0x05DB); //2nd frame
			break;
		case SHUTTER_2:
			LOG_INF("cxc shutter 2\n");
			write_cmos_sensor(0x0E14, 0x0BB6); //2nd frame
			break;
		case SHUTTER_4:
			LOG_INF("cxc shutter 4\n");
			write_cmos_sensor(0x0E14, 0x176C); //2nd frame
			break;
		case SHUTTER_8:
			LOG_INF("cxc shutter 8\n");
			write_cmos_sensor(0x0E14, 0x2ED8); //2nd frame
			break;
		case SHUTTER_16:
			LOG_INF("cxc shutter 16\n");
			write_cmos_sensor(0x0E14, 0x5DB0); //2nd frame
			break;
		case SHUTTER_32:
			LOG_INF("cxc shutter 32\n");
			write_cmos_sensor(0x0E14, 0xBB61); //2nd frame
			break;
		default:
			LOG_INF("cxc shutter > 1\n");
			write_cmos_sensor(0x0E14, ((shutter * 0x05DB) /
				SHUTTER_1)); //2nd frame
			break;
		}
		write_cmos_sensor(0x0E16, 0x0050); //aGain 2nd frame
		write_cmos_sensor(0x0704, 0x0600); //shifter for shutter

		write_cmos_sensor(0x0100, 0x0100); //stream on
		pre_shutter = 0;
	}

	//write_cmos_sensor(0X0202, shutter & 0xFFFF);

	LOG_INF("cxc  Exit! shutter =%d, framelength =%d\n", shutter,
			imgsensor.frame_length);
}

/*************************************************************************
*FUNCTION
*  set_shutter_frame_length
*
*DESCRIPTION
*  for frame &3A sync
*
*************************************************************************/

static void
set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	kal_bool autoflicker_closed = KAL_FALSE;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	spin_lock(&imgsensor_drv_lock);

	dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter =
		(shutter < imgsensor_info.min_shutter)
		 ? imgsensor_info.min_shutter : shutter;
	shutter =
		(shutter >
		 (imgsensor_info.max_frame_length -
		  imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
			 imgsensor_info.margin) : shutter;

	if (autoflicker_closed) {
		realtime_fps =
			imgsensor.pclk / imgsensor.line_length * 10 /
			imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else
			write_cmos_sensor(0x0340, imgsensor.frame_length);
	} else
		write_cmos_sensor(0x0340, imgsensor.frame_length);


	write_cmos_sensor(0x0202, imgsensor.shutter);
	LOG_INF
	("Exit! shutter %d framelength %d/%d dummy_line=%d auto_extend=%d\n",
	 shutter, imgsensor.frame_length,
	  frame_length, dummy_line, read_cmos_sensor(0x0350));

}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = 0x0000;

	reg_gain = gain / 2;

	return (kal_uint16) reg_gain;
}

/*************************************************************************
*FUNCTION
*  set_gain
*
*DESCRIPTION
*  This function is to set global gain to sensor.
*
*PARAMETERS
*  iGain : sensor global gain(base: 0x40)
*
*RETURNS
*  the actually gain set to sensor.
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{

	kal_uint16 reg_gain;

	LOG_INF("set_gain %d\n", gain);
	if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");
		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 16 * BASEGAIN)
			gain = 16 * BASEGAIN;
	}
	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	write_cmos_sensor(0x0204, (reg_gain & 0xFFFF));
	return gain;
}

#if 0
static void
ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {
		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
				imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;
		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;

		write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);
		write_cmos_sensor(0x3512, (se << 4) & 0xFF);
		write_cmos_sensor(0x3511, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3510, (se >> 12) & 0x0F);
		set_gain(gain);
	}
}
#endif
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.mirror = image_mirror;
	spin_unlock(&imgsensor_drv_lock);
	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor_byte(0x0101, 0x00);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor_byte(0x0101, 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor_byte(0x0101, 0x02);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor_byte(0x0101, 0x03);
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
		break;
	}
}

/*************************************************************************
*FUNCTION
*  night_mode
*
*DESCRIPTION
*  This function night mode of sensor.
*
*PARAMETERS
*  bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
*RETURNS
*  None
*
*GLOBALS AFFECTED
*
*************************************************************************/
#if 0
static void night_mode(kal_bool enable)
{

}
#endif

#if MULTI_WRITE
kal_uint16 addr_data_pair_init_s5kgm1sp_ofilm[] = {
	0x6028, 0x4000,
	0x0000, 0x0009,
	0x0000, 0x08D1,
	0x6010, 0x0001,
	0x6214, 0x7971,
	0x6218, 0x7150,
	0x0A70, 0x0001,
	0x0A72, 0x0100,
	0x0A02, 0x0074,
	0x6028, 0x2000,
	0x602A, 0x106A,
	0x6F12, 0x0003,
	0x602A, 0x2BC2,
	0x6F12, 0x0003,
	0x602A, 0x3F5C,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0549,
	0x6F12, 0x0448,
	0x6F12, 0x054A,
	0x6F12, 0xC1F8,
	0x6F12, 0x5005,
	0x6F12, 0x101A,
	0x6F12, 0xA1F8,
	0x6F12, 0x5405,
	0x6F12, 0x00F0,
	0x6F12, 0xCFB9,
	0x6F12, 0x2000,
	0x6F12, 0x4470,
	0x6F12, 0x2000,
	0x6F12, 0x2E30,
	0x6F12, 0x2000,
	0x6F12, 0x6E00,
	0x6F12, 0x2DE9,
	0x6F12, 0xFF5F,
	0x6F12, 0xF848,
	0x6F12, 0x8B46,
	0x6F12, 0x1746,
	0x6F12, 0x0068,
	0x6F12, 0x9A46,
	0x6F12, 0x4FEA,
	0x6F12, 0x1049,
	0x6F12, 0x80B2,
	0x6F12, 0x8046,
	0x6F12, 0x0146,
	0x6F12, 0x0022,
	0x6F12, 0x4846,
	0x6F12, 0x00F0,
	0x6F12, 0x02FA,
	0x6F12, 0xF24D,
	0x6F12, 0x95F8,
	0x6F12, 0x6D00,
	0x6F12, 0x0228,
	0x6F12, 0x35D0,
	0x6F12, 0x0224,
	0x6F12, 0xF04E,
	0x6F12, 0x5346,
	0x6F12, 0xB6F8,
	0x6F12, 0xB802,
	0x6F12, 0xB0FB,
	0x6F12, 0xF4F0,
	0x6F12, 0xA6F8,
	0x6F12, 0xB802,
	0x6F12, 0xD5F8,
	0x6F12, 0x1411,
	0x6F12, 0x06F5,
	0x6F12, 0x2E76,
	0x6F12, 0x6143,
	0x6F12, 0xC5F8,
	0x6F12, 0x1411,
	0x6F12, 0xB5F8,
	0x6F12, 0x8C11,
	0x6F12, 0x411A,
	0x6F12, 0x89B2,
	0x6F12, 0x25F8,
	0x6F12, 0x981B,
	0x6F12, 0x35F8,
	0x6F12, 0x142C,
	0x6F12, 0x6243,
	0x6F12, 0x521E,
	0x6F12, 0x00FB,
	0x6F12, 0x0210,
	0x6F12, 0xB5F8,
	0x6F12, 0xF210,
	0x6F12, 0x07FB,
	0x6F12, 0x04F2,
	0x6F12, 0x0844,
	0x6F12, 0xC5F8,
	0x6F12, 0xF800,
	0x6F12, 0x5946,
	0x6F12, 0x0098,
	0x6F12, 0x00F0,
	0x6F12, 0xDBF9,
	0x6F12, 0x3088,
	0x6F12, 0x4146,
	0x6F12, 0x6043,
	0x6F12, 0x3080,
	0x6F12, 0xE86F,
	0x6F12, 0x0122,
	0x6F12, 0xB0FB,
	0x6F12, 0xF4F0,
	0x6F12, 0xE867,
	0x6F12, 0x04B0,
	0x6F12, 0x4846,
	0x6F12, 0xBDE8,
	0x6F12, 0xF05F,
	0x6F12, 0x00F0,
	0x6F12, 0xC7B9,
	0x6F12, 0x0124,
	0x6F12, 0xC8E7,
	0x6F12, 0x2DE9,
	0x6F12, 0xF041,
	0x6F12, 0x8046,
	0x6F12, 0xD148,
	0x6F12, 0x0022,
	0x6F12, 0x4168,
	0x6F12, 0x0D0C,
	0x6F12, 0x8EB2,
	0x6F12, 0x3146,
	0x6F12, 0x2846,
	0x6F12, 0x00F0,
	0x6F12, 0xB9F9,
	0x6F12, 0xD04C,
	0x6F12, 0xCE4F,
	0x6F12, 0x2078,
	0x6F12, 0x97F8,
	0x6F12, 0x8B12,
	0x6F12, 0x10FB,
	0x6F12, 0x01F0,
	0x6F12, 0x2070,
	0x6F12, 0x4046,
	0x6F12, 0x00F0,
	0x6F12, 0xB8F9,
	0x6F12, 0x2078,
	0x6F12, 0x97F8,
	0x6F12, 0x8B12,
	0x6F12, 0x0122,
	0x6F12, 0xB0FB,
	0x6F12, 0xF1F0,
	0x6F12, 0x2070,
	0x6F12, 0x3146,
	0x6F12, 0x2846,
	0x6F12, 0xBDE8,
	0x6F12, 0xF041,
	0x6F12, 0x00F0,
	0x6F12, 0xA1B9,
	0x6F12, 0x2DE9,
	0x6F12, 0xFF47,
	0x6F12, 0x8146,
	0x6F12, 0xBF48,
	0x6F12, 0x1746,
	0x6F12, 0x8846,
	0x6F12, 0x8068,
	0x6F12, 0x1C46,
	0x6F12, 0x85B2,
	0x6F12, 0x060C,
	0x6F12, 0x0022,
	0x6F12, 0x2946,
	0x6F12, 0x3046,
	0x6F12, 0x00F0,
	0x6F12, 0x92F9,
	0x6F12, 0x2346,
	0x6F12, 0x3A46,
	0x6F12, 0x4146,
	0x6F12, 0x4846,
	0x6F12, 0x00F0,
	0x6F12, 0x9BF9,
	0x6F12, 0xBA4A,
	0x6F12, 0x9088,
	0x6F12, 0xF0B3,
	0x6F12, 0xB748,
	0x6F12, 0x90F8,
	0x6F12, 0xBA10,
	0x6F12, 0xD1B3,
	0x6F12, 0xD0F8,
	0x6F12, 0x2801,
	0x6F12, 0x1168,
	0x6F12, 0x8842,
	0x6F12, 0x00D3,
	0x6F12, 0x0846,
	0x6F12, 0x010A,
	0x6F12, 0xB1FA,
	0x6F12, 0x81F0,
	0x6F12, 0xC0F1,
	0x6F12, 0x1700,
	0x6F12, 0xC140,
	0x6F12, 0x02EB,
	0x6F12, 0x4000,
	0x6F12, 0xC9B2,
	0x6F12, 0x0389,
	0x6F12, 0xC288,
	0x6F12, 0x9B1A,
	0x6F12, 0x4B43,
	0x6F12, 0x8033,
	0x6F12, 0x02EB,
	0x6F12, 0x2322,
	0x6F12, 0x0092,
	0x6F12, 0x438A,
	0x6F12, 0x028A,
	0x6F12, 0x9B1A,
	0x6F12, 0x4B43,
	0x6F12, 0x8033,
	0x6F12, 0x02EB,
	0x6F12, 0x2322,
	0x6F12, 0x0192,
	0x6F12, 0x838B,
	0x6F12, 0x428B,
	0x6F12, 0x9B1A,
	0x6F12, 0x4B43,
	0x6F12, 0x8033,
	0x6F12, 0x02EB,
	0x6F12, 0x2322,
	0x6F12, 0x0292,
	0x6F12, 0xC28C,
	0x6F12, 0x808C,
	0x6F12, 0x121A,
	0x6F12, 0x4A43,
	0x6F12, 0x8032,
	0x6F12, 0x00EB,
	0x6F12, 0x2220,
	0x6F12, 0x0390,
	0x6F12, 0x0022,
	0x6F12, 0x6846,
	0x6F12, 0x54F8,
	0x6F12, 0x2210,
	0x6F12, 0x50F8,
	0x6F12, 0x2230,
	0x6F12, 0x5943,
	0x6F12, 0x090B,
	0x6F12, 0x44F8,
	0x6F12, 0x2210,
	0x6F12, 0x521C,
	0x6F12, 0x00E0,
	0x6F12, 0x01E0,
	0x6F12, 0x042A,
	0x6F12, 0xF2D3,
	0x6F12, 0x04B0,
	0x6F12, 0x2946,
	0x6F12, 0x3046,
	0x6F12, 0xBDE8,
	0x6F12, 0xF047,
	0x6F12, 0x0122,
	0x6F12, 0x00F0,
	0x6F12, 0x3FB9,
	0x6F12, 0x2DE9,
	0x6F12, 0xF041,
	0x6F12, 0x954C,
	0x6F12, 0x9349,
	0x6F12, 0x0646,
	0x6F12, 0x94F8,
	0x6F12, 0x6970,
	0x6F12, 0x8988,
	0x6F12, 0x94F8,
	0x6F12, 0x8120,
	0x6F12, 0x0020,
	0x6F12, 0xC1B1,
	0x6F12, 0x2146,
	0x6F12, 0xD1F8,
	0x6F12, 0x9410,
	0x6F12, 0x72B1,
	0x6F12, 0x8FB1,
	0x6F12, 0x0846,
	0x6F12, 0x00F0,
	0x6F12, 0x3FF9,
	0x6F12, 0x0546,
	0x6F12, 0xE06F,
	0x6F12, 0x00F0,
	0x6F12, 0x3BF9,
	0x6F12, 0x8542,
	0x6F12, 0x02D2,
	0x6F12, 0xD4F8,
	0x6F12, 0x9400,
	0x6F12, 0x26E0,
	0x6F12, 0xE06F,
	0x6F12, 0x24E0,
	0x6F12, 0x002F,
	0x6F12, 0xFBD1,
	0x6F12, 0x002A,
	0x6F12, 0x24D0,
	0x6F12, 0x0846,
	0x6F12, 0x1EE0,
	0x6F12, 0x8149,
	0x6F12, 0x0D8E,
	0x6F12, 0x496B,
	0x6F12, 0x4B42,
	0x6F12, 0x77B1,
	0x6F12, 0x8148,
	0x6F12, 0x806F,
	0x6F12, 0x10E0,
	0x6F12, 0x4242,
	0x6F12, 0x00E0,
	0x6F12, 0x0246,
	0x6F12, 0x0029,
	0x6F12, 0x0FDB,
	0x6F12, 0x8A42,
	0x6F12, 0x0FDD,
	0x6F12, 0x3046,
	0x6F12, 0xBDE8,
	0x6F12, 0xF041,
	0x6F12, 0x00F0,
	0x6F12, 0x1FB9,
	0x6F12, 0x002A,
	0x6F12, 0x0CD0,
	0x6F12, 0x7848,
	0x6F12, 0xD0F8,
	0x6F12, 0x8C00,
	0x6F12, 0x25B1,
	0x6F12, 0x0028,
	0x6F12, 0xEDDA,
	0x6F12, 0xEAE7,
	0x6F12, 0x1946,
	0x6F12, 0xEDE7,
	0x6F12, 0x00F0,
	0x6F12, 0x17F9,
	0x6F12, 0xE060,
	0x6F12, 0x0120,
	0x6F12, 0xBDE8,
	0x6F12, 0xF081,
	0x6F12, 0x2DE9,
	0x6F12, 0xF35F,
	0x6F12, 0xDFF8,
	0x6F12, 0xB0A1,
	0x6F12, 0x0C46,
	0x6F12, 0xBAF8,
	0x6F12, 0xBE04,
	0x6F12, 0x08B1,
	0x6F12, 0x00F0,
	0x6F12, 0x0EF9,
	0x6F12, 0x6C4E,
	0x6F12, 0x3088,
	0x6F12, 0x0128,
	0x6F12, 0x06D1,
	0x6F12, 0x002C,
	0x6F12, 0x04D1,
	0x6F12, 0x684D,
	0x6F12, 0x2889,
	0x6F12, 0x18B1,
	0x6F12, 0x401E,
	0x6F12, 0x2881,
	0x6F12, 0xBDE8,
	0x6F12, 0xFC9F,
	0x6F12, 0xDFF8,
	0x6F12, 0x9891,
	0x6F12, 0xD9F8,
	0x6F12, 0x0000,
	0x6F12, 0xB0F8,
	0x6F12, 0xD602,
	0x6F12, 0x38B1,
	0x6F12, 0x3089,
	0x6F12, 0x401C,
	0x6F12, 0x80B2,
	0x6F12, 0x3081,
	0x6F12, 0xFF28,
	0x6F12, 0x01D9,
	0x6F12, 0xE889,
	0x6F12, 0x3081,
	0x6F12, 0x6048,
	0x6F12, 0x4FF0,
	0x6F12, 0x0008,
	0x6F12, 0xC6F8,
	0x6F12, 0x0C80,
	0x6F12, 0xB0F8,
	0x6F12, 0x5EB0,
	0x6F12, 0x40F2,
	0x6F12, 0xFF31,
	0x6F12, 0x0B20,
	0x6F12, 0x00F0,
	0x6F12, 0xEBF8,
	0x6F12, 0xD9F8,
	0x6F12, 0x0000,
	0x6F12, 0x0027,
	0x6F12, 0x3C46,
	0x6F12, 0xB0F8,
	0x6F12, 0xD412,
	0x6F12, 0x21B1,
	0x6F12, 0x0098,
	0x6F12, 0x00F0,
	0x6F12, 0xD2F8,
	0x6F12, 0x0746,
	0x6F12, 0x0BE0,
	0x6F12, 0xB0F8,
	0x6F12, 0xD602,
	0x6F12, 0x40B1,
	0x6F12, 0x3089,
	0x6F12, 0xE989,
	0x6F12, 0x8842,
	0x6F12, 0x04D3,
	0x6F12, 0x0098,
	0x6F12, 0xFFF7,
	0x6F12, 0x6EFF,
	0x6F12, 0x0746,
	0x6F12, 0x0124,
	0x6F12, 0x3846,
	0x6F12, 0x00F0,
	0x6F12, 0xD5F8,
	0x6F12, 0xD9F8,
	0x6F12, 0x0000,
	0x6F12, 0xB0F8,
	0x6F12, 0xD602,
	0x6F12, 0x08B9,
	0x6F12, 0xA6F8,
	0x6F12, 0x0280,
	0x6F12, 0xC7B3,
	0x6F12, 0x4746,
	0x6F12, 0xA6F8,
	0x6F12, 0x0880,
	0x6F12, 0x00F0,
	0x6F12, 0xCDF8,
	0x6F12, 0xF068,
	0x6F12, 0x3061,
	0x6F12, 0x688D,
	0x6F12, 0x50B3,
	0x6F12, 0xA88D,
	0x6F12, 0x50BB,
	0x6F12, 0x00F0,
	0x6F12, 0xCAF8,
	0x6F12, 0xA889,
	0x6F12, 0x20B3,
	0x6F12, 0x1CB3,
	0x6F12, 0x706B,
	0x6F12, 0xAA88,
	0x6F12, 0xDAF8,
	0x6F12, 0x0815,
	0x6F12, 0xCAB1,
	0x6F12, 0x8842,
	0x6F12, 0x0CDB,
	0x6F12, 0x90FB,
	0x6F12, 0xF1F3,
	0x6F12, 0x90FB,
	0x6F12, 0xF1F2,
	0x6F12, 0x01FB,
	0x6F12, 0x1303,
	0x6F12, 0xB3EB,
	0x6F12, 0x610F,
	0x6F12, 0x00DD,
	0x6F12, 0x521C,
	0x6F12, 0x01FB,
	0x6F12, 0x1200,
	0x6F12, 0x0BE0,
	0x6F12, 0x91FB,
	0x6F12, 0xF0F3,
	0x6F12, 0x91FB,
	0x6F12, 0xF0F2,
	0x6F12, 0x00FB,
	0x6F12, 0x1313,
	0x6F12, 0xB3EB,
	0x6F12, 0x600F,
	0x6F12, 0x00DD,
	0x6F12, 0x521C,
	0x6F12, 0x5043,
	0x6F12, 0x401A,
	0x6F12, 0xF168,
	0x6F12, 0x01EB,
	0x6F12, 0x4000,
	0x6F12, 0xF060,
	0x6F12, 0xA88D,
	0x6F12, 0x10B1,
	0x6F12, 0xF089,
	0x6F12, 0x3087,
	0x6F12, 0xAF85,
	0x6F12, 0x5846,
	0x6F12, 0xBDE8,
	0x6F12, 0xFC5F,
	0x6F12, 0x00F0,
	0x6F12, 0x9EB8,
	0x6F12, 0x70B5,
	0x6F12, 0x2349,
	0x6F12, 0x0446,
	0x6F12, 0x0020,
	0x6F12, 0xC1F8,
	0x6F12, 0x3005,
	0x6F12, 0x1E48,
	0x6F12, 0x0022,
	0x6F12, 0xC168,
	0x6F12, 0x0D0C,
	0x6F12, 0x8EB2,
	0x6F12, 0x3146,
	0x6F12, 0x2846,
	0x6F12, 0x00F0,
	0x6F12, 0x53F8,
	0x6F12, 0x2046,
	0x6F12, 0x00F0,
	0x6F12, 0x91F8,
	0x6F12, 0x3146,
	0x6F12, 0x2846,
	0x6F12, 0xBDE8,
	0x6F12, 0x7040,
	0x6F12, 0x0122,
	0x6F12, 0x00F0,
	0x6F12, 0x49B8,
	0x6F12, 0x10B5,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x9731,
	0x6F12, 0x1C48,
	0x6F12, 0x00F0,
	0x6F12, 0x88F8,
	0x6F12, 0x114C,
	0x6F12, 0x0122,
	0x6F12, 0xAFF2,
	0x6F12, 0x0D31,
	0x6F12, 0x2060,
	0x6F12, 0x1948,
	0x6F12, 0x00F0,
	0x6F12, 0x80F8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0xD121,
	0x6F12, 0x6060,
	0x6F12, 0x1648,
	0x6F12, 0x00F0,
	0x6F12, 0x79F8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x1D21,
	0x6F12, 0xA060,
	0x6F12, 0x1448,
	0x6F12, 0x00F0,
	0x6F12, 0x72F8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x9511,
	0x6F12, 0x1248,
	0x6F12, 0x00F0,
	0x6F12, 0x6CF8,
	0x6F12, 0x0022,
	0x6F12, 0xAFF2,
	0x6F12, 0x7B01,
	0x6F12, 0x1048,
	0x6F12, 0x00F0,
	0x6F12, 0x66F8,
	0x6F12, 0xE060,
	0x6F12, 0x10BD,
	0x6F12, 0x2000,
	0x6F12, 0x4460,
	0x6F12, 0x2000,
	0x6F12, 0x2C30,
	0x6F12, 0x2000,
	0x6F12, 0x2E30,
	0x6F12, 0x2000,
	0x6F12, 0x2580,
	0x6F12, 0x2000,
	0x6F12, 0x6000,
	0x6F12, 0x2000,
	0x6F12, 0x2BA0,
	0x6F12, 0x2000,
	0x6F12, 0x3600,
	0x6F12, 0x2000,
	0x6F12, 0x0890,
	0x6F12, 0x4000,
	0x6F12, 0x7000,
	0x6F12, 0x0000,
	0x6F12, 0x24A7,
	0x6F12, 0x0001,
	0x6F12, 0x1AF3,
	0x6F12, 0x0001,
	0x6F12, 0x09BD,
	0x6F12, 0x0000,
	0x6F12, 0x576B,
	0x6F12, 0x0000,
	0x6F12, 0x57ED,
	0x6F12, 0x0000,
	0x6F12, 0xBF8D,
	0x6F12, 0x4AF6,
	0x6F12, 0x293C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x42F2,
	0x6F12, 0xA74C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x41F6,
	0x6F12, 0xF32C,
	0x6F12, 0xC0F2,
	0x6F12, 0x010C,
	0x6F12, 0x6047,
	0x6F12, 0x40F6,
	0x6F12, 0xBD1C,
	0x6F12, 0xC0F2,
	0x6F12, 0x010C,
	0x6F12, 0x6047,
	0x6F12, 0x4AF6,
	0x6F12, 0x532C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0x377C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0xD56C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0xC91C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0xAB2C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x44F6,
	0x6F12, 0x897C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0xA56C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x45F2,
	0x6F12, 0xEF6C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x40F2,
	0x6F12, 0x6D7C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4BF6,
	0x6F12, 0x8D7C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x4BF2,
	0x6F12, 0xAB4C,
	0x6F12, 0xC0F2,
	0x6F12, 0x000C,
	0x6F12, 0x6047,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x0000,
	0x6F12, 0x08D1,
	0x6F12, 0x008B,
	0x6F12, 0x0000,
	0x6F12, 0x0067,
};
#endif


static void sensor_init(void)
{
	LOG_INF("E\n");
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0000, 0x0009);
	write_cmos_sensor(0x0000, 0x08D1);
	write_cmos_sensor(0x6010, 0x0001);
	mdelay(3);
#if MULTI_WRITE
//	LOG_INF("cxc s5kgm1sp ofilm_sensor_init MULTI_WRITE\n");
	s5kgm1sp_ofilm_table_write_cmos_sensor(
		addr_data_pair_init_s5kgm1sp_ofilm,
		sizeof(addr_data_pair_init_s5kgm1sp_ofilm) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0A70, 0x0001);
	write_cmos_sensor(0x0A72, 0x0100);
	write_cmos_sensor(0x0A02, 0x0074);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x106A);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x2BC2);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x3F5C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0549);
	write_cmos_sensor(0x6F12, 0x0448);
	write_cmos_sensor(0x6F12, 0x054A);
	write_cmos_sensor(0x6F12, 0xC1F8);
	write_cmos_sensor(0x6F12, 0x5005);
	write_cmos_sensor(0x6F12, 0x101A);
	write_cmos_sensor(0x6F12, 0xA1F8);
	write_cmos_sensor(0x6F12, 0x5405);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xCFB9);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x4470);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2E30);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x6E00);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xFF5F);
	write_cmos_sensor(0x6F12, 0xF848);
	write_cmos_sensor(0x6F12, 0x8B46);
	write_cmos_sensor(0x6F12, 0x1746);
	write_cmos_sensor(0x6F12, 0x0068);
	write_cmos_sensor(0x6F12, 0x9A46);
	write_cmos_sensor(0x6F12, 0x4FEA);
	write_cmos_sensor(0x6F12, 0x1049);
	write_cmos_sensor(0x6F12, 0x80B2);
	write_cmos_sensor(0x6F12, 0x8046);
	write_cmos_sensor(0x6F12, 0x0146);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x4846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x02FA);
	write_cmos_sensor(0x6F12, 0xF24D);
	write_cmos_sensor(0x6F12, 0x95F8);
	write_cmos_sensor(0x6F12, 0x6D00);
	write_cmos_sensor(0x6F12, 0x0228);
	write_cmos_sensor(0x6F12, 0x35D0);
	write_cmos_sensor(0x6F12, 0x0224);
	write_cmos_sensor(0x6F12, 0xF04E);
	write_cmos_sensor(0x6F12, 0x5346);
	write_cmos_sensor(0x6F12, 0xB6F8);
	write_cmos_sensor(0x6F12, 0xB802);
	write_cmos_sensor(0x6F12, 0xB0FB);
	write_cmos_sensor(0x6F12, 0xF4F0);
	write_cmos_sensor(0x6F12, 0xA6F8);
	write_cmos_sensor(0x6F12, 0xB802);
	write_cmos_sensor(0x6F12, 0xD5F8);
	write_cmos_sensor(0x6F12, 0x1411);
	write_cmos_sensor(0x6F12, 0x06F5);
	write_cmos_sensor(0x6F12, 0x2E76);
	write_cmos_sensor(0x6F12, 0x6143);
	write_cmos_sensor(0x6F12, 0xC5F8);
	write_cmos_sensor(0x6F12, 0x1411);
	write_cmos_sensor(0x6F12, 0xB5F8);
	write_cmos_sensor(0x6F12, 0x8C11);
	write_cmos_sensor(0x6F12, 0x411A);
	write_cmos_sensor(0x6F12, 0x89B2);
	write_cmos_sensor(0x6F12, 0x25F8);
	write_cmos_sensor(0x6F12, 0x981B);
	write_cmos_sensor(0x6F12, 0x35F8);
	write_cmos_sensor(0x6F12, 0x142C);
	write_cmos_sensor(0x6F12, 0x6243);
	write_cmos_sensor(0x6F12, 0x521E);
	write_cmos_sensor(0x6F12, 0x00FB);
	write_cmos_sensor(0x6F12, 0x0210);
	write_cmos_sensor(0x6F12, 0xB5F8);
	write_cmos_sensor(0x6F12, 0xF210);
	write_cmos_sensor(0x6F12, 0x07FB);
	write_cmos_sensor(0x6F12, 0x04F2);
	write_cmos_sensor(0x6F12, 0x0844);
	write_cmos_sensor(0x6F12, 0xC5F8);
	write_cmos_sensor(0x6F12, 0xF800);
	write_cmos_sensor(0x6F12, 0x5946);
	write_cmos_sensor(0x6F12, 0x0098);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xDBF9);
	write_cmos_sensor(0x6F12, 0x3088);
	write_cmos_sensor(0x6F12, 0x4146);
	write_cmos_sensor(0x6F12, 0x6043);
	write_cmos_sensor(0x6F12, 0x3080);
	write_cmos_sensor(0x6F12, 0xE86F);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0xB0FB);
	write_cmos_sensor(0x6F12, 0xF4F0);
	write_cmos_sensor(0x6F12, 0xE867);
	write_cmos_sensor(0x6F12, 0x04B0);
	write_cmos_sensor(0x6F12, 0x4846);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF05F);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xC7B9);
	write_cmos_sensor(0x6F12, 0x0124);
	write_cmos_sensor(0x6F12, 0xC8E7);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x8046);
	write_cmos_sensor(0x6F12, 0xD148);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x4168);
	write_cmos_sensor(0x6F12, 0x0D0C);
	write_cmos_sensor(0x6F12, 0x8EB2);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xB9F9);
	write_cmos_sensor(0x6F12, 0xD04C);
	write_cmos_sensor(0x6F12, 0xCE4F);
	write_cmos_sensor(0x6F12, 0x2078);
	write_cmos_sensor(0x6F12, 0x97F8);
	write_cmos_sensor(0x6F12, 0x8B12);
	write_cmos_sensor(0x6F12, 0x10FB);
	write_cmos_sensor(0x6F12, 0x01F0);
	write_cmos_sensor(0x6F12, 0x2070);
	write_cmos_sensor(0x6F12, 0x4046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xB8F9);
	write_cmos_sensor(0x6F12, 0x2078);
	write_cmos_sensor(0x6F12, 0x97F8);
	write_cmos_sensor(0x6F12, 0x8B12);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0xB0FB);
	write_cmos_sensor(0x6F12, 0xF1F0);
	write_cmos_sensor(0x6F12, 0x2070);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xA1B9);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xFF47);
	write_cmos_sensor(0x6F12, 0x8146);
	write_cmos_sensor(0x6F12, 0xBF48);
	write_cmos_sensor(0x6F12, 0x1746);
	write_cmos_sensor(0x6F12, 0x8846);
	write_cmos_sensor(0x6F12, 0x8068);
	write_cmos_sensor(0x6F12, 0x1C46);
	write_cmos_sensor(0x6F12, 0x85B2);
	write_cmos_sensor(0x6F12, 0x060C);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x92F9);
	write_cmos_sensor(0x6F12, 0x2346);
	write_cmos_sensor(0x6F12, 0x3A46);
	write_cmos_sensor(0x6F12, 0x4146);
	write_cmos_sensor(0x6F12, 0x4846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x9BF9);
	write_cmos_sensor(0x6F12, 0xBA4A);
	write_cmos_sensor(0x6F12, 0x9088);
	write_cmos_sensor(0x6F12, 0xF0B3);
	write_cmos_sensor(0x6F12, 0xB748);
	write_cmos_sensor(0x6F12, 0x90F8);
	write_cmos_sensor(0x6F12, 0xBA10);
	write_cmos_sensor(0x6F12, 0xD1B3);
	write_cmos_sensor(0x6F12, 0xD0F8);
	write_cmos_sensor(0x6F12, 0x2801);
	write_cmos_sensor(0x6F12, 0x1168);
	write_cmos_sensor(0x6F12, 0x8842);
	write_cmos_sensor(0x6F12, 0x00D3);
	write_cmos_sensor(0x6F12, 0x0846);
	write_cmos_sensor(0x6F12, 0x010A);
	write_cmos_sensor(0x6F12, 0xB1FA);
	write_cmos_sensor(0x6F12, 0x81F0);
	write_cmos_sensor(0x6F12, 0xC0F1);
	write_cmos_sensor(0x6F12, 0x1700);
	write_cmos_sensor(0x6F12, 0xC140);
	write_cmos_sensor(0x6F12, 0x02EB);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xC9B2);
	write_cmos_sensor(0x6F12, 0x0389);
	write_cmos_sensor(0x6F12, 0xC288);
	write_cmos_sensor(0x6F12, 0x9B1A);
	write_cmos_sensor(0x6F12, 0x4B43);
	write_cmos_sensor(0x6F12, 0x8033);
	write_cmos_sensor(0x6F12, 0x02EB);
	write_cmos_sensor(0x6F12, 0x2322);
	write_cmos_sensor(0x6F12, 0x0092);
	write_cmos_sensor(0x6F12, 0x438A);
	write_cmos_sensor(0x6F12, 0x028A);
	write_cmos_sensor(0x6F12, 0x9B1A);
	write_cmos_sensor(0x6F12, 0x4B43);
	write_cmos_sensor(0x6F12, 0x8033);
	write_cmos_sensor(0x6F12, 0x02EB);
	write_cmos_sensor(0x6F12, 0x2322);
	write_cmos_sensor(0x6F12, 0x0192);
	write_cmos_sensor(0x6F12, 0x838B);
	write_cmos_sensor(0x6F12, 0x428B);
	write_cmos_sensor(0x6F12, 0x9B1A);
	write_cmos_sensor(0x6F12, 0x4B43);
	write_cmos_sensor(0x6F12, 0x8033);
	write_cmos_sensor(0x6F12, 0x02EB);
	write_cmos_sensor(0x6F12, 0x2322);
	write_cmos_sensor(0x6F12, 0x0292);
	write_cmos_sensor(0x6F12, 0xC28C);
	write_cmos_sensor(0x6F12, 0x808C);
	write_cmos_sensor(0x6F12, 0x121A);
	write_cmos_sensor(0x6F12, 0x4A43);
	write_cmos_sensor(0x6F12, 0x8032);
	write_cmos_sensor(0x6F12, 0x00EB);
	write_cmos_sensor(0x6F12, 0x2220);
	write_cmos_sensor(0x6F12, 0x0390);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0x6846);
	write_cmos_sensor(0x6F12, 0x54F8);
	write_cmos_sensor(0x6F12, 0x2210);
	write_cmos_sensor(0x6F12, 0x50F8);
	write_cmos_sensor(0x6F12, 0x2230);
	write_cmos_sensor(0x6F12, 0x5943);
	write_cmos_sensor(0x6F12, 0x090B);
	write_cmos_sensor(0x6F12, 0x44F8);
	write_cmos_sensor(0x6F12, 0x2210);
	write_cmos_sensor(0x6F12, 0x521C);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x01E0);
	write_cmos_sensor(0x6F12, 0x042A);
	write_cmos_sensor(0x6F12, 0xF2D3);
	write_cmos_sensor(0x6F12, 0x04B0);
	write_cmos_sensor(0x6F12, 0x2946);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF047);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x3FB9);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x954C);
	write_cmos_sensor(0x6F12, 0x9349);
	write_cmos_sensor(0x6F12, 0x0646);
	write_cmos_sensor(0x6F12, 0x94F8);
	write_cmos_sensor(0x6F12, 0x6970);
	write_cmos_sensor(0x6F12, 0x8988);
	write_cmos_sensor(0x6F12, 0x94F8);
	write_cmos_sensor(0x6F12, 0x8120);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xC1B1);
	write_cmos_sensor(0x6F12, 0x2146);
	write_cmos_sensor(0x6F12, 0xD1F8);
	write_cmos_sensor(0x6F12, 0x9410);
	write_cmos_sensor(0x6F12, 0x72B1);
	write_cmos_sensor(0x6F12, 0x8FB1);
	write_cmos_sensor(0x6F12, 0x0846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x3FF9);
	write_cmos_sensor(0x6F12, 0x0546);
	write_cmos_sensor(0x6F12, 0xE06F);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x3BF9);
	write_cmos_sensor(0x6F12, 0x8542);
	write_cmos_sensor(0x6F12, 0x02D2);
	write_cmos_sensor(0x6F12, 0xD4F8);
	write_cmos_sensor(0x6F12, 0x9400);
	write_cmos_sensor(0x6F12, 0x26E0);
	write_cmos_sensor(0x6F12, 0xE06F);
	write_cmos_sensor(0x6F12, 0x24E0);
	write_cmos_sensor(0x6F12, 0x002F);
	write_cmos_sensor(0x6F12, 0xFBD1);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x6F12, 0x24D0);
	write_cmos_sensor(0x6F12, 0x0846);
	write_cmos_sensor(0x6F12, 0x1EE0);
	write_cmos_sensor(0x6F12, 0x8149);
	write_cmos_sensor(0x6F12, 0x0D8E);
	write_cmos_sensor(0x6F12, 0x496B);
	write_cmos_sensor(0x6F12, 0x4B42);
	write_cmos_sensor(0x6F12, 0x77B1);
	write_cmos_sensor(0x6F12, 0x8148);
	write_cmos_sensor(0x6F12, 0x806F);
	write_cmos_sensor(0x6F12, 0x10E0);
	write_cmos_sensor(0x6F12, 0x4242);
	write_cmos_sensor(0x6F12, 0x00E0);
	write_cmos_sensor(0x6F12, 0x0246);
	write_cmos_sensor(0x6F12, 0x0029);
	write_cmos_sensor(0x6F12, 0x0FDB);
	write_cmos_sensor(0x6F12, 0x8A42);
	write_cmos_sensor(0x6F12, 0x0FDD);
	write_cmos_sensor(0x6F12, 0x3046);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF041);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x1FB9);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x6F12, 0x0CD0);
	write_cmos_sensor(0x6F12, 0x7848);
	write_cmos_sensor(0x6F12, 0xD0F8);
	write_cmos_sensor(0x6F12, 0x8C00);
	write_cmos_sensor(0x6F12, 0x25B1);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x6F12, 0xEDDA);
	write_cmos_sensor(0x6F12, 0xEAE7);
	write_cmos_sensor(0x6F12, 0x1946);
	write_cmos_sensor(0x6F12, 0xEDE7);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x17F9);
	write_cmos_sensor(0x6F12, 0xE060);
	write_cmos_sensor(0x6F12, 0x0120);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xF081);
	write_cmos_sensor(0x6F12, 0x2DE9);
	write_cmos_sensor(0x6F12, 0xF35F);
	write_cmos_sensor(0x6F12, 0xDFF8);
	write_cmos_sensor(0x6F12, 0xB0A1);
	write_cmos_sensor(0x6F12, 0x0C46);
	write_cmos_sensor(0x6F12, 0xBAF8);
	write_cmos_sensor(0x6F12, 0xBE04);
	write_cmos_sensor(0x6F12, 0x08B1);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x0EF9);
	write_cmos_sensor(0x6F12, 0x6C4E);
	write_cmos_sensor(0x6F12, 0x3088);
	write_cmos_sensor(0x6F12, 0x0128);
	write_cmos_sensor(0x6F12, 0x06D1);
	write_cmos_sensor(0x6F12, 0x002C);
	write_cmos_sensor(0x6F12, 0x04D1);
	write_cmos_sensor(0x6F12, 0x684D);
	write_cmos_sensor(0x6F12, 0x2889);
	write_cmos_sensor(0x6F12, 0x18B1);
	write_cmos_sensor(0x6F12, 0x401E);
	write_cmos_sensor(0x6F12, 0x2881);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xFC9F);
	write_cmos_sensor(0x6F12, 0xDFF8);
	write_cmos_sensor(0x6F12, 0x9891);
	write_cmos_sensor(0x6F12, 0xD9F8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0xD602);
	write_cmos_sensor(0x6F12, 0x38B1);
	write_cmos_sensor(0x6F12, 0x3089);
	write_cmos_sensor(0x6F12, 0x401C);
	write_cmos_sensor(0x6F12, 0x80B2);
	write_cmos_sensor(0x6F12, 0x3081);
	write_cmos_sensor(0x6F12, 0xFF28);
	write_cmos_sensor(0x6F12, 0x01D9);
	write_cmos_sensor(0x6F12, 0xE889);
	write_cmos_sensor(0x6F12, 0x3081);
	write_cmos_sensor(0x6F12, 0x6048);
	write_cmos_sensor(0x6F12, 0x4FF0);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x6F12, 0xC6F8);
	write_cmos_sensor(0x6F12, 0x0C80);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0x5EB0);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0xFF31);
	write_cmos_sensor(0x6F12, 0x0B20);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xEBF8);
	write_cmos_sensor(0x6F12, 0xD9F8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0027);
	write_cmos_sensor(0x6F12, 0x3C46);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0xD412);
	write_cmos_sensor(0x6F12, 0x21B1);
	write_cmos_sensor(0x6F12, 0x0098);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xD2F8);
	write_cmos_sensor(0x6F12, 0x0746);
	write_cmos_sensor(0x6F12, 0x0BE0);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0xD602);
	write_cmos_sensor(0x6F12, 0x40B1);
	write_cmos_sensor(0x6F12, 0x3089);
	write_cmos_sensor(0x6F12, 0xE989);
	write_cmos_sensor(0x6F12, 0x8842);
	write_cmos_sensor(0x6F12, 0x04D3);
	write_cmos_sensor(0x6F12, 0x0098);
	write_cmos_sensor(0x6F12, 0xFFF7);
	write_cmos_sensor(0x6F12, 0x6EFF);
	write_cmos_sensor(0x6F12, 0x0746);
	write_cmos_sensor(0x6F12, 0x0124);
	write_cmos_sensor(0x6F12, 0x3846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xD5F8);
	write_cmos_sensor(0x6F12, 0xD9F8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0xB0F8);
	write_cmos_sensor(0x6F12, 0xD602);
	write_cmos_sensor(0x6F12, 0x08B9);
	write_cmos_sensor(0x6F12, 0xA6F8);
	write_cmos_sensor(0x6F12, 0x0280);
	write_cmos_sensor(0x6F12, 0xC7B3);
	write_cmos_sensor(0x6F12, 0x4746);
	write_cmos_sensor(0x6F12, 0xA6F8);
	write_cmos_sensor(0x6F12, 0x0880);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xCDF8);
	write_cmos_sensor(0x6F12, 0xF068);
	write_cmos_sensor(0x6F12, 0x3061);
	write_cmos_sensor(0x6F12, 0x688D);
	write_cmos_sensor(0x6F12, 0x50B3);
	write_cmos_sensor(0x6F12, 0xA88D);
	write_cmos_sensor(0x6F12, 0x50BB);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0xCAF8);
	write_cmos_sensor(0x6F12, 0xA889);
	write_cmos_sensor(0x6F12, 0x20B3);
	write_cmos_sensor(0x6F12, 0x1CB3);
	write_cmos_sensor(0x6F12, 0x706B);
	write_cmos_sensor(0x6F12, 0xAA88);
	write_cmos_sensor(0x6F12, 0xDAF8);
	write_cmos_sensor(0x6F12, 0x0815);
	write_cmos_sensor(0x6F12, 0xCAB1);
	write_cmos_sensor(0x6F12, 0x8842);
	write_cmos_sensor(0x6F12, 0x0CDB);
	write_cmos_sensor(0x6F12, 0x90FB);
	write_cmos_sensor(0x6F12, 0xF1F3);
	write_cmos_sensor(0x6F12, 0x90FB);
	write_cmos_sensor(0x6F12, 0xF1F2);
	write_cmos_sensor(0x6F12, 0x01FB);
	write_cmos_sensor(0x6F12, 0x1303);
	write_cmos_sensor(0x6F12, 0xB3EB);
	write_cmos_sensor(0x6F12, 0x610F);
	write_cmos_sensor(0x6F12, 0x00DD);
	write_cmos_sensor(0x6F12, 0x521C);
	write_cmos_sensor(0x6F12, 0x01FB);
	write_cmos_sensor(0x6F12, 0x1200);
	write_cmos_sensor(0x6F12, 0x0BE0);
	write_cmos_sensor(0x6F12, 0x91FB);
	write_cmos_sensor(0x6F12, 0xF0F3);
	write_cmos_sensor(0x6F12, 0x91FB);
	write_cmos_sensor(0x6F12, 0xF0F2);
	write_cmos_sensor(0x6F12, 0x00FB);
	write_cmos_sensor(0x6F12, 0x1313);
	write_cmos_sensor(0x6F12, 0xB3EB);
	write_cmos_sensor(0x6F12, 0x600F);
	write_cmos_sensor(0x6F12, 0x00DD);
	write_cmos_sensor(0x6F12, 0x521C);
	write_cmos_sensor(0x6F12, 0x5043);
	write_cmos_sensor(0x6F12, 0x401A);
	write_cmos_sensor(0x6F12, 0xF168);
	write_cmos_sensor(0x6F12, 0x01EB);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0xF060);
	write_cmos_sensor(0x6F12, 0xA88D);
	write_cmos_sensor(0x6F12, 0x10B1);
	write_cmos_sensor(0x6F12, 0xF089);
	write_cmos_sensor(0x6F12, 0x3087);
	write_cmos_sensor(0x6F12, 0xAF85);
	write_cmos_sensor(0x6F12, 0x5846);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0xFC5F);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x9EB8);
	write_cmos_sensor(0x6F12, 0x70B5);
	write_cmos_sensor(0x6F12, 0x2349);
	write_cmos_sensor(0x6F12, 0x0446);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x6F12, 0xC1F8);
	write_cmos_sensor(0x6F12, 0x3005);
	write_cmos_sensor(0x6F12, 0x1E48);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xC168);
	write_cmos_sensor(0x6F12, 0x0D0C);
	write_cmos_sensor(0x6F12, 0x8EB2);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x53F8);
	write_cmos_sensor(0x6F12, 0x2046);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x91F8);
	write_cmos_sensor(0x6F12, 0x3146);
	write_cmos_sensor(0x6F12, 0x2846);
	write_cmos_sensor(0x6F12, 0xBDE8);
	write_cmos_sensor(0x6F12, 0x7040);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x49B8);
	write_cmos_sensor(0x6F12, 0x10B5);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x9731);
	write_cmos_sensor(0x6F12, 0x1C48);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x88F8);
	write_cmos_sensor(0x6F12, 0x114C);
	write_cmos_sensor(0x6F12, 0x0122);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x0D31);
	write_cmos_sensor(0x6F12, 0x2060);
	write_cmos_sensor(0x6F12, 0x1948);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x80F8);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0xD121);
	write_cmos_sensor(0x6F12, 0x6060);
	write_cmos_sensor(0x6F12, 0x1648);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x79F8);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x1D21);
	write_cmos_sensor(0x6F12, 0xA060);
	write_cmos_sensor(0x6F12, 0x1448);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x72F8);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x9511);
	write_cmos_sensor(0x6F12, 0x1248);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x6CF8);
	write_cmos_sensor(0x6F12, 0x0022);
	write_cmos_sensor(0x6F12, 0xAFF2);
	write_cmos_sensor(0x6F12, 0x7B01);
	write_cmos_sensor(0x6F12, 0x1048);
	write_cmos_sensor(0x6F12, 0x00F0);
	write_cmos_sensor(0x6F12, 0x66F8);
	write_cmos_sensor(0x6F12, 0xE060);
	write_cmos_sensor(0x6F12, 0x10BD);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x4460);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2C30);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2E30);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2580);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x6000);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x2BA0);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x3600);
	write_cmos_sensor(0x6F12, 0x2000);
	write_cmos_sensor(0x6F12, 0x0890);
	write_cmos_sensor(0x6F12, 0x4000);
	write_cmos_sensor(0x6F12, 0x7000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x24A7);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x1AF3);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x6F12, 0x09BD);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x576B);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x57ED);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0xBF8D);
	write_cmos_sensor(0x6F12, 0x4AF6);
	write_cmos_sensor(0x6F12, 0x293C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x42F2);
	write_cmos_sensor(0x6F12, 0xA74C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x41F6);
	write_cmos_sensor(0x6F12, 0xF32C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F6);
	write_cmos_sensor(0x6F12, 0xBD1C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x010C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4AF6);
	write_cmos_sensor(0x6F12, 0x532C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x45F2);
	write_cmos_sensor(0x6F12, 0x377C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x45F2);
	write_cmos_sensor(0x6F12, 0xD56C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x45F2);
	write_cmos_sensor(0x6F12, 0xC91C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0xAB2C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x44F6);
	write_cmos_sensor(0x6F12, 0x897C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x45F2);
	write_cmos_sensor(0x6F12, 0xA56C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x45F2);
	write_cmos_sensor(0x6F12, 0xEF6C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x40F2);
	write_cmos_sensor(0x6F12, 0x6D7C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4BF6);
	write_cmos_sensor(0x6F12, 0x8D7C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x4BF2);
	write_cmos_sensor(0x6F12, 0xAB4C);
	write_cmos_sensor(0x6F12, 0xC0F2);
	write_cmos_sensor(0x6F12, 0x000C);
	write_cmos_sensor(0x6F12, 0x6047);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x08D1);
	write_cmos_sensor(0x6F12, 0x008B);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x6F12, 0x0067);
#endif
}

#if 0
static void preview_setting(void)
{

	LOG_INF("E\n");
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x0FA7);
	write_cmos_sensor(0x034A, 0x0BBF);
	write_cmos_sensor(0x034C, 0x07D0);
	write_cmos_sensor(0x034E, 0x05DC);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0000);
	write_cmos_sensor(0x0340, 0x18F4);
	write_cmos_sensor(0x0342, 0x09D0);
	write_cmos_sensor(0x0900, 0x0121);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0003);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0404, 0x1000);
	write_cmos_sensor(0x0402, 0x1020);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0306, 0x00F1);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0008);
	write_cmos_sensor(0x030E, 0x0003);
	write_cmos_sensor(0x0312, 0x0001);
	write_cmos_sensor(0x0310, 0x0095);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1492);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x0E4E);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0118, 0x0004);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2126);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1168);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x602A, 0x2DB6);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1668);
	write_cmos_sensor(0x6F12, 0xFF00);
	write_cmos_sensor(0x602A, 0x166A);
	write_cmos_sensor(0x6F12, 0xFF00);
	write_cmos_sensor(0x602A, 0x118A);
	write_cmos_sensor(0x6F12, 0x0802);
	write_cmos_sensor(0x602A, 0x151E);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x217E);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1520);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x2522);
	write_cmos_sensor(0x6F12, 0x0804);
	write_cmos_sensor(0x602A, 0x2524);
	write_cmos_sensor(0x6F12, 0x0400);
	write_cmos_sensor(0x602A, 0x2568);
	write_cmos_sensor(0x6F12, 0x5500);
	write_cmos_sensor(0x602A, 0x2588);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x258C);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x25A6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x252C);
	write_cmos_sensor(0x6F12, 0x0601);
	write_cmos_sensor(0x602A, 0x252E);
	write_cmos_sensor(0x6F12, 0x0605);
	write_cmos_sensor(0x602A, 0x25A8);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25AC);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x25B0);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25B4);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x15A4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15A6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15A8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15AA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15AC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15AE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15B0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15B2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15B4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15B6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15B8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15BA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15BC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15BE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15C0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15C2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x15C4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15C6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15C8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15CA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15CC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15CE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15D0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15D2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15D4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15D6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15D8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15DA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15DC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15DE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15E0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15E2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x1A50);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1A54);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0D00, 0x0101);
	write_cmos_sensor(0x0D02, 0x0101);
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0xF486, 0x0641);
	write_cmos_sensor(0xF488, 0x0A45);
	write_cmos_sensor(0xF48A, 0x0A49);
	write_cmos_sensor(0xF48C, 0x064D);
	write_cmos_sensor(0xF48E, 0x0651);
	write_cmos_sensor(0xF490, 0x0A55);
	write_cmos_sensor(0xF492, 0x0A59);
	write_cmos_sensor(0xF494, 0x065D);
	write_cmos_sensor(0xF496, 0x0661);
	write_cmos_sensor(0xF498, 0x0A65);
	write_cmos_sensor(0xF49A, 0x0A69);
	write_cmos_sensor(0xF49C, 0x066D);
	write_cmos_sensor(0xF49E, 0x0671);
	write_cmos_sensor(0xF4A0, 0x0A75);
	write_cmos_sensor(0xF4A2, 0x0A79);
	write_cmos_sensor(0xF4A4, 0x067D);
	write_cmos_sensor(0xF4A6, 0x0641);
	write_cmos_sensor(0xF4A8, 0x0A45);
	write_cmos_sensor(0xF4AA, 0x0A49);
	write_cmos_sensor(0xF4AC, 0x064D);
	write_cmos_sensor(0xF4AE, 0x0651);
	write_cmos_sensor(0xF4B0, 0x0A55);
	write_cmos_sensor(0xF4B2, 0x0A59);
	write_cmos_sensor(0xF4B4, 0x065D);
	write_cmos_sensor(0xF4B6, 0x0661);
	write_cmos_sensor(0xF4B8, 0x0A65);
	write_cmos_sensor(0xF4BA, 0x0A69);
	write_cmos_sensor(0xF4BC, 0x066D);
	write_cmos_sensor(0xF4BE, 0x0671);
	write_cmos_sensor(0xF4C0, 0x0A75);
	write_cmos_sensor(0xF4C2, 0x0A79);
	write_cmos_sensor(0xF4C4, 0x067D);
	write_cmos_sensor(0x0202, 0x0010);
	write_cmos_sensor(0x0226, 0x0010);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x107A);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x1074);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x0E7C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1120);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1122);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x602A, 0x1128);
	write_cmos_sensor(0x6F12, 0x0601);
	write_cmos_sensor(0x602A, 0x1AC0);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1AC2);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x1494);
	write_cmos_sensor(0x6F12, 0x3D68);
	write_cmos_sensor(0x602A, 0x1498);
	write_cmos_sensor(0x6F12, 0xF10D);
	write_cmos_sensor(0x602A, 0x1488);
	write_cmos_sensor(0x6F12, 0x0904);
	write_cmos_sensor(0x602A, 0x148A);
	write_cmos_sensor(0x6F12, 0x170B);
	write_cmos_sensor(0x602A, 0x150E);
	write_cmos_sensor(0x6F12, 0x00C2);
	write_cmos_sensor(0x602A, 0x1510);
	write_cmos_sensor(0x6F12, 0xC0AF);
	write_cmos_sensor(0x602A, 0x1512);
	write_cmos_sensor(0x6F12, 0x0080);
	write_cmos_sensor(0x602A, 0x1486);
	write_cmos_sensor(0x6F12, 0x1430);
	write_cmos_sensor(0x602A, 0x1490);
	write_cmos_sensor(0x6F12, 0x4D09);
	write_cmos_sensor(0x602A, 0x149E);
	write_cmos_sensor(0x6F12, 0x01C4);
	write_cmos_sensor(0x602A, 0x11CC);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x602A, 0x11CE);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x602A, 0x11D0);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x11DA);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x602A, 0x11E6);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x602A, 0x125E);
	write_cmos_sensor(0x6F12, 0x0048);
	write_cmos_sensor(0x602A, 0x11F4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x11F8);
	write_cmos_sensor(0x6F12, 0x0016);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0xF444, 0x05BF);
	write_cmos_sensor(0xF44A, 0x0008);
	write_cmos_sensor(0xF44E, 0x0012);
	write_cmos_sensor(0xF46E, 0x90C0);
	write_cmos_sensor(0xF470, 0x2809);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1CAA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6000);
	write_cmos_sensor(0x6F12, 0x000F);
	write_cmos_sensor(0x602A, 0x6002);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x602A, 0x6004);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6006);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6008);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6010);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6012);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6014);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6016);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6018);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6020);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6022);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6024);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6026);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6028);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x1144);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1146);
	write_cmos_sensor(0x6F12, 0x1B00);
	write_cmos_sensor(0x602A, 0x1080);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1084);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x108A);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x1090);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1092);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1094);
	write_cmos_sensor(0x6F12, 0xA32E);
}
#endif

static void hs_video_setting(void)
{

	LOG_INF("E\n");
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0058);
	write_cmos_sensor(0x0346, 0x01AC);
	write_cmos_sensor(0x0348, 0x0F57);
	write_cmos_sensor(0x034A, 0x0A1B);
	write_cmos_sensor(0x034C, 0x0780);
	write_cmos_sensor(0x034E, 0x0438);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0000);
	write_cmos_sensor(0x0340, 0x063E);
	write_cmos_sensor(0x0342, 0x09D0);
	write_cmos_sensor(0x0900, 0x0122);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0003);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0003);
	write_cmos_sensor(0x0404, 0x1000);
	write_cmos_sensor(0x0402, 0x1010);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0306, 0x00F1);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0008);
	write_cmos_sensor(0x030E, 0x0003);
	write_cmos_sensor(0x0312, 0x0001);
	write_cmos_sensor(0x0310, 0x0095);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1492);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x0E4E);
	write_cmos_sensor(0x6F12, 0x0069);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0118, 0x0004);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2126);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1168);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x602A, 0x2DB6);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1668);
	write_cmos_sensor(0x6F12, 0xFF00);
	write_cmos_sensor(0x602A, 0x166A);
	write_cmos_sensor(0x6F12, 0xFF00);
	write_cmos_sensor(0x602A, 0x118A);
	write_cmos_sensor(0x6F12, 0x0402);
	write_cmos_sensor(0x602A, 0x151E);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x217E);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1520);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x2522);
	write_cmos_sensor(0x6F12, 0x1004);
	write_cmos_sensor(0x602A, 0x2524);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x2568);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x2588);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x258C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x25A6);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x252C);
	write_cmos_sensor(0x6F12, 0x0601);
	write_cmos_sensor(0x602A, 0x252E);
	write_cmos_sensor(0x6F12, 0x0605);
	write_cmos_sensor(0x602A, 0x25A8);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25AC);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x25B0);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25B4);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x15A4);
	write_cmos_sensor(0x6F12, 0x0641);
	write_cmos_sensor(0x602A, 0x15A6);
	write_cmos_sensor(0x6F12, 0x0145);
	write_cmos_sensor(0x602A, 0x15A8);
	write_cmos_sensor(0x6F12, 0x0149);
	write_cmos_sensor(0x602A, 0x15AA);
	write_cmos_sensor(0x6F12, 0x064D);
	write_cmos_sensor(0x602A, 0x15AC);
	write_cmos_sensor(0x6F12, 0x0651);
	write_cmos_sensor(0x602A, 0x15AE);
	write_cmos_sensor(0x6F12, 0x0155);
	write_cmos_sensor(0x602A, 0x15B0);
	write_cmos_sensor(0x6F12, 0x0159);
	write_cmos_sensor(0x602A, 0x15B2);
	write_cmos_sensor(0x6F12, 0x065D);
	write_cmos_sensor(0x602A, 0x15B4);
	write_cmos_sensor(0x6F12, 0x0661);
	write_cmos_sensor(0x602A, 0x15B6);
	write_cmos_sensor(0x6F12, 0x0165);
	write_cmos_sensor(0x602A, 0x15B8);
	write_cmos_sensor(0x6F12, 0x0169);
	write_cmos_sensor(0x602A, 0x15BA);
	write_cmos_sensor(0x6F12, 0x066D);
	write_cmos_sensor(0x602A, 0x15BC);
	write_cmos_sensor(0x6F12, 0x0671);
	write_cmos_sensor(0x602A, 0x15BE);
	write_cmos_sensor(0x6F12, 0x0175);
	write_cmos_sensor(0x602A, 0x15C0);
	write_cmos_sensor(0x6F12, 0x0179);
	write_cmos_sensor(0x602A, 0x15C2);
	write_cmos_sensor(0x6F12, 0x067D);
	write_cmos_sensor(0x602A, 0x15C4);
	write_cmos_sensor(0x6F12, 0x0641);
	write_cmos_sensor(0x602A, 0x15C6);
	write_cmos_sensor(0x6F12, 0x0145);
	write_cmos_sensor(0x602A, 0x15C8);
	write_cmos_sensor(0x6F12, 0x0149);
	write_cmos_sensor(0x602A, 0x15CA);
	write_cmos_sensor(0x6F12, 0x064D);
	write_cmos_sensor(0x602A, 0x15CC);
	write_cmos_sensor(0x6F12, 0x0651);
	write_cmos_sensor(0x602A, 0x15CE);
	write_cmos_sensor(0x6F12, 0x0155);
	write_cmos_sensor(0x602A, 0x15D0);
	write_cmos_sensor(0x6F12, 0x0159);
	write_cmos_sensor(0x602A, 0x15D2);
	write_cmos_sensor(0x6F12, 0x065D);
	write_cmos_sensor(0x602A, 0x15D4);
	write_cmos_sensor(0x6F12, 0x0661);
	write_cmos_sensor(0x602A, 0x15D6);
	write_cmos_sensor(0x6F12, 0x0165);
	write_cmos_sensor(0x602A, 0x15D8);
	write_cmos_sensor(0x6F12, 0x0169);
	write_cmos_sensor(0x602A, 0x15DA);
	write_cmos_sensor(0x6F12, 0x066D);
	write_cmos_sensor(0x602A, 0x15DC);
	write_cmos_sensor(0x6F12, 0x0671);
	write_cmos_sensor(0x602A, 0x15DE);
	write_cmos_sensor(0x6F12, 0x0175);
	write_cmos_sensor(0x602A, 0x15E0);
	write_cmos_sensor(0x6F12, 0x0179);
	write_cmos_sensor(0x602A, 0x15E2);
	write_cmos_sensor(0x6F12, 0x067D);
	write_cmos_sensor(0x602A, 0x1A50);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1A54);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0D00, 0x0101);
	write_cmos_sensor(0x0D02, 0x0101);
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0xF486, 0x0000);
	write_cmos_sensor(0xF488, 0x0000);
	write_cmos_sensor(0xF48A, 0x0000);
	write_cmos_sensor(0xF48C, 0x0000);
	write_cmos_sensor(0xF48E, 0x0000);
	write_cmos_sensor(0xF490, 0x0000);
	write_cmos_sensor(0xF492, 0x0000);
	write_cmos_sensor(0xF494, 0x0000);
	write_cmos_sensor(0xF496, 0x0000);
	write_cmos_sensor(0xF498, 0x0000);
	write_cmos_sensor(0xF49A, 0x0000);
	write_cmos_sensor(0xF49C, 0x0000);
	write_cmos_sensor(0xF49E, 0x0000);
	write_cmos_sensor(0xF4A0, 0x0000);
	write_cmos_sensor(0xF4A2, 0x0000);
	write_cmos_sensor(0xF4A4, 0x0000);
	write_cmos_sensor(0xF4A6, 0x0000);
	write_cmos_sensor(0xF4A8, 0x0000);
	write_cmos_sensor(0xF4AA, 0x0000);
	write_cmos_sensor(0xF4AC, 0x0000);
	write_cmos_sensor(0xF4AE, 0x0000);
	write_cmos_sensor(0xF4B0, 0x0000);
	write_cmos_sensor(0xF4B2, 0x0000);
	write_cmos_sensor(0xF4B4, 0x0000);
	write_cmos_sensor(0xF4B6, 0x0000);
	write_cmos_sensor(0xF4B8, 0x0000);
	write_cmos_sensor(0xF4BA, 0x0000);
	write_cmos_sensor(0xF4BC, 0x0000);
	write_cmos_sensor(0xF4BE, 0x0000);
	write_cmos_sensor(0xF4C0, 0x0000);
	write_cmos_sensor(0xF4C2, 0x0000);
	write_cmos_sensor(0xF4C4, 0x0000);
	write_cmos_sensor(0x0202, 0x0010);
	write_cmos_sensor(0x0226, 0x0010);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x107A);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x1074);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x0E7C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1120);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1122);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x1128);
	write_cmos_sensor(0x6F12, 0x0604);
	write_cmos_sensor(0x602A, 0x1AC0);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1AC2);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x1494);
	write_cmos_sensor(0x6F12, 0x3D68);
	write_cmos_sensor(0x602A, 0x1498);
	write_cmos_sensor(0x6F12, 0xF10D);
	write_cmos_sensor(0x602A, 0x1488);
	write_cmos_sensor(0x6F12, 0x0904);
	write_cmos_sensor(0x602A, 0x148A);
	write_cmos_sensor(0x6F12, 0x170B);
	write_cmos_sensor(0x602A, 0x150E);
	write_cmos_sensor(0x6F12, 0x00C2);
	write_cmos_sensor(0x602A, 0x1510);
	write_cmos_sensor(0x6F12, 0xC0AF);
	write_cmos_sensor(0x602A, 0x1512);
	write_cmos_sensor(0x6F12, 0x0080);
	write_cmos_sensor(0x602A, 0x1486);
	write_cmos_sensor(0x6F12, 0x1430);
	write_cmos_sensor(0x602A, 0x1490);
	write_cmos_sensor(0x6F12, 0x4D09);
	write_cmos_sensor(0x602A, 0x149E);
	write_cmos_sensor(0x6F12, 0x01C4);
	write_cmos_sensor(0x602A, 0x11CC);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x602A, 0x11CE);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x602A, 0x11D0);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x11DA);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x602A, 0x11E6);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x602A, 0x125E);
	write_cmos_sensor(0x6F12, 0x0048);
	write_cmos_sensor(0x602A, 0x11F4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x11F8);
	write_cmos_sensor(0x6F12, 0x0016);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0xF444, 0x05BF);
	write_cmos_sensor(0xF44A, 0x0008);
	write_cmos_sensor(0xF44E, 0x0012);
	write_cmos_sensor(0xF46E, 0x74C0);
	write_cmos_sensor(0xF470, 0x2809);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1CAA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6000);
	write_cmos_sensor(0x6F12, 0x000F);
	write_cmos_sensor(0x602A, 0x6002);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x602A, 0x6004);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6006);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6008);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6010);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6012);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6014);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6016);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6018);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6020);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6022);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6024);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6026);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6028);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x1144);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1146);
	write_cmos_sensor(0x6F12, 0x1B00);
	write_cmos_sensor(0x602A, 0x1080);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1084);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x108A);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x1090);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1092);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1094);
	write_cmos_sensor(0x6F12, 0xA32E);
}

static void capture_setting(kal_uint16 currefps)
{

	LOG_INF("start\n");
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x0FA7);
	write_cmos_sensor(0x034A, 0x0BBF);
	write_cmos_sensor(0x034C, 0x0FA0);
	write_cmos_sensor(0x034E, 0x0BB8);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0000);
	write_cmos_sensor(0x0340, 0x0C7A);
	write_cmos_sensor(0x0342, 0x13A0);
	write_cmos_sensor(0x0900, 0x0111);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0001);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0404, 0x1000);
	write_cmos_sensor(0x0402, 0x1010);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0306, 0x00F1);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0008);
	write_cmos_sensor(0x030E, 0x0003);
	write_cmos_sensor(0x0312, 0x0001);
	write_cmos_sensor(0x0310, 0x0090);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1492);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x0E4E);
	write_cmos_sensor(0x6F12, 0x007A);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0118, 0x0004);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2126);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1168);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x602A, 0x2DB6);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1668);
	write_cmos_sensor(0x6F12, 0xF0F0);
	write_cmos_sensor(0x602A, 0x166A);
	write_cmos_sensor(0x6F12, 0xF0F0);
	write_cmos_sensor(0x602A, 0x118A);
	write_cmos_sensor(0x6F12, 0x0802);
	write_cmos_sensor(0x602A, 0x151E);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x217E);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1520);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x602A, 0x2522);
	write_cmos_sensor(0x6F12, 0x0804);
	write_cmos_sensor(0x602A, 0x2524);
	write_cmos_sensor(0x6F12, 0x0400);
	write_cmos_sensor(0x602A, 0x2568);
	write_cmos_sensor(0x6F12, 0x5500);
	write_cmos_sensor(0x602A, 0x2588);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x258C);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x25A6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x252C);
	write_cmos_sensor(0x6F12, 0x0601);
	write_cmos_sensor(0x602A, 0x252E);
	write_cmos_sensor(0x6F12, 0x0605);
	write_cmos_sensor(0x602A, 0x25A8);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25AC);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x25B0);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25B4);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x15A4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15A6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15A8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15AA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15AC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15AE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15B0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15B2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15B4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15B6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15B8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15BA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15BC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15BE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15C0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15C2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x15C4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15C6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15C8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15CA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15CC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15CE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15D0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15D2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15D4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15D6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15D8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15DA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15DC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15DE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15E0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15E2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x1A50);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1A54);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0D00, 0x0101);
	write_cmos_sensor(0x0D02, 0x0101);
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0xF486, 0x0000);
	write_cmos_sensor(0xF488, 0x0000);
	write_cmos_sensor(0xF48A, 0x0000);
	write_cmos_sensor(0xF48C, 0x0000);
	write_cmos_sensor(0xF48E, 0x0000);
	write_cmos_sensor(0xF490, 0x0000);
	write_cmos_sensor(0xF492, 0x0000);
	write_cmos_sensor(0xF494, 0x0000);
	write_cmos_sensor(0xF496, 0x0000);
	write_cmos_sensor(0xF498, 0x0000);
	write_cmos_sensor(0xF49A, 0x0000);
	write_cmos_sensor(0xF49C, 0x0000);
	write_cmos_sensor(0xF49E, 0x0000);
	write_cmos_sensor(0xF4A0, 0x0000);
	write_cmos_sensor(0xF4A2, 0x0000);
	write_cmos_sensor(0xF4A4, 0x0000);
	write_cmos_sensor(0xF4A6, 0x0000);
	write_cmos_sensor(0xF4A8, 0x0000);
	write_cmos_sensor(0xF4AA, 0x0000);
	write_cmos_sensor(0xF4AC, 0x0000);
	write_cmos_sensor(0xF4AE, 0x0000);
	write_cmos_sensor(0xF4B0, 0x0000);
	write_cmos_sensor(0xF4B2, 0x0000);
	write_cmos_sensor(0xF4B4, 0x0000);
	write_cmos_sensor(0xF4B6, 0x0000);
	write_cmos_sensor(0xF4B8, 0x0000);
	write_cmos_sensor(0xF4BA, 0x0000);
	write_cmos_sensor(0xF4BC, 0x0000);
	write_cmos_sensor(0xF4BE, 0x0000);
	write_cmos_sensor(0xF4C0, 0x0000);
	write_cmos_sensor(0xF4C2, 0x0000);
	write_cmos_sensor(0xF4C4, 0x0000);
	write_cmos_sensor(0x0202, 0x0010);
	write_cmos_sensor(0x0226, 0x0010);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x107A);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x1074);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x0E7C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1120);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1122);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x602A, 0x1128);
	write_cmos_sensor(0x6F12, 0x0604);
	write_cmos_sensor(0x602A, 0x1AC0);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1AC2);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x1494);
	write_cmos_sensor(0x6F12, 0x3D68);
	write_cmos_sensor(0x602A, 0x1498);
	write_cmos_sensor(0x6F12, 0xF10D);
	write_cmos_sensor(0x602A, 0x1488);
	write_cmos_sensor(0x6F12, 0x0F0F);
	write_cmos_sensor(0x602A, 0x148A);
	write_cmos_sensor(0x6F12, 0x170F);
	write_cmos_sensor(0x602A, 0x150E);
	write_cmos_sensor(0x6F12, 0x00C2);
	write_cmos_sensor(0x602A, 0x1510);
	write_cmos_sensor(0x6F12, 0xC0AF);
	write_cmos_sensor(0x602A, 0x1512);
	write_cmos_sensor(0x6F12, 0x00A0);
	write_cmos_sensor(0x602A, 0x1486);
	write_cmos_sensor(0x6F12, 0x1430);
	write_cmos_sensor(0x602A, 0x1490);
	write_cmos_sensor(0x6F12, 0x4D09);
	write_cmos_sensor(0x602A, 0x149E);
	write_cmos_sensor(0x6F12, 0x01C4);
	write_cmos_sensor(0x602A, 0x11CC);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x602A, 0x11CE);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x602A, 0x11D0);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x11DA);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x602A, 0x11E6);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x602A, 0x125E);
	write_cmos_sensor(0x6F12, 0x0048);
	write_cmos_sensor(0x602A, 0x11F4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x11F8);
	write_cmos_sensor(0x6F12, 0x0016);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0xF444, 0x05BF);
	write_cmos_sensor(0xF44A, 0x0008);
	write_cmos_sensor(0xF44E, 0x0012);
	write_cmos_sensor(0xF46E, 0x40C0);
	write_cmos_sensor(0xF470, 0x7809);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1CAA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6000);
	write_cmos_sensor(0x6F12, 0x000F);
	write_cmos_sensor(0x602A, 0x6002);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x602A, 0x6004);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6006);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6008);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6010);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6012);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6014);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6016);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6018);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6020);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6022);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6024);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6026);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6028);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x1144);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1146);
	write_cmos_sensor(0x6F12, 0x1B00);
	write_cmos_sensor(0x602A, 0x1080);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1084);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x108A);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x1090);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1092);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1094);
	write_cmos_sensor(0x6F12, 0xA32E);
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("start\n");
//	preview_setting();
	capture_setting(currefps);
}

static void slim_video_setting(void)
{

	LOG_INF("E\n");
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0058);
	write_cmos_sensor(0x0346, 0x01AC);
	write_cmos_sensor(0x0348, 0x0F57);
	write_cmos_sensor(0x034A, 0x0A1B);
	write_cmos_sensor(0x034C, 0x0500);
	write_cmos_sensor(0x034E, 0x02D0);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0000);
	write_cmos_sensor(0x0340, 0x0330);
	write_cmos_sensor(0x0342, 0x13A0);
	write_cmos_sensor(0x0900, 0x0123);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0002);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0005);
	write_cmos_sensor(0x0404, 0x1000);
	write_cmos_sensor(0x0402, 0x1810);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0306, 0x00F6);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0008);
	write_cmos_sensor(0x030E, 0x0003);
	write_cmos_sensor(0x0312, 0x0002);
	write_cmos_sensor(0x0310, 0x005B);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1492);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x0E4E);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0118, 0x0104);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2126);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1168);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x602A, 0x2DB6);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1668);
	write_cmos_sensor(0x6F12, 0xF0F0);
	write_cmos_sensor(0x602A, 0x166A);
	write_cmos_sensor(0x6F12, 0xF0F0);
	write_cmos_sensor(0x602A, 0x118A);
	write_cmos_sensor(0x6F12, 0x0802);
	write_cmos_sensor(0x602A, 0x151E);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x217E);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1520);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x2522);
	write_cmos_sensor(0x6F12, 0x0804);
	write_cmos_sensor(0x602A, 0x2524);
	write_cmos_sensor(0x6F12, 0x0400);
	write_cmos_sensor(0x602A, 0x2568);
	write_cmos_sensor(0x6F12, 0x5500);
	write_cmos_sensor(0x602A, 0x2588);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x258C);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x25A6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x252C);
	write_cmos_sensor(0x6F12, 0x0601);
	write_cmos_sensor(0x602A, 0x252E);
	write_cmos_sensor(0x6F12, 0x0605);
	write_cmos_sensor(0x602A, 0x25A8);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25AC);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x25B0);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25B4);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x15A4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15A6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15A8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15AA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15AC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15AE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15B0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15B2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15B4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15B6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15B8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15BA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15BC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15BE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15C0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15C2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x15C4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15C6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15C8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15CA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15CC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15CE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15D0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15D2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15D4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15D6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15D8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15DA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15DC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15DE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15E0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15E2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x1A50);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1A54);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0D00, 0x0100);
	write_cmos_sensor(0x0D02, 0x0101);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0xF486, 0x0000);
	write_cmos_sensor(0xF488, 0x0000);
	write_cmos_sensor(0xF48A, 0x0000);
	write_cmos_sensor(0xF48C, 0x0000);
	write_cmos_sensor(0xF48E, 0x0000);
	write_cmos_sensor(0xF490, 0x0000);
	write_cmos_sensor(0xF492, 0x0000);
	write_cmos_sensor(0xF494, 0x0000);
	write_cmos_sensor(0xF496, 0x0000);
	write_cmos_sensor(0xF498, 0x0000);
	write_cmos_sensor(0xF49A, 0x0000);
	write_cmos_sensor(0xF49C, 0x0000);
	write_cmos_sensor(0xF49E, 0x0000);
	write_cmos_sensor(0xF4A0, 0x0000);
	write_cmos_sensor(0xF4A2, 0x0000);
	write_cmos_sensor(0xF4A4, 0x0000);
	write_cmos_sensor(0xF4A6, 0x0000);
	write_cmos_sensor(0xF4A8, 0x0000);
	write_cmos_sensor(0xF4AA, 0x0000);
	write_cmos_sensor(0xF4AC, 0x0000);
	write_cmos_sensor(0xF4AE, 0x0000);
	write_cmos_sensor(0xF4B0, 0x0000);
	write_cmos_sensor(0xF4B2, 0x0000);
	write_cmos_sensor(0xF4B4, 0x0000);
	write_cmos_sensor(0xF4B6, 0x0000);
	write_cmos_sensor(0xF4B8, 0x0000);
	write_cmos_sensor(0xF4BA, 0x0000);
	write_cmos_sensor(0xF4BC, 0x0000);
	write_cmos_sensor(0xF4BE, 0x0000);
	write_cmos_sensor(0xF4C0, 0x0000);
	write_cmos_sensor(0xF4C2, 0x0000);
	write_cmos_sensor(0xF4C4, 0x0000);
	write_cmos_sensor(0x0202, 0x0010);
	write_cmos_sensor(0x0226, 0x0010);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x107A);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x1074);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x0E7C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1120);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1122);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x602A, 0x1128);
	write_cmos_sensor(0x6F12, 0x0604);
	write_cmos_sensor(0x602A, 0x1AC0);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1AC2);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x1494);
	write_cmos_sensor(0x6F12, 0x3D68);
	write_cmos_sensor(0x602A, 0x1498);
	write_cmos_sensor(0x6F12, 0xF10D);
	write_cmos_sensor(0x602A, 0x1488);
	write_cmos_sensor(0x6F12, 0x0F0F);
	write_cmos_sensor(0x602A, 0x148A);
	write_cmos_sensor(0x6F12, 0x170F);
	write_cmos_sensor(0x602A, 0x150E);
	write_cmos_sensor(0x6F12, 0x00C2);
	write_cmos_sensor(0x602A, 0x1510);
	write_cmos_sensor(0x6F12, 0xC0AF);
	write_cmos_sensor(0x602A, 0x1512);
	write_cmos_sensor(0x6F12, 0x0080);
	write_cmos_sensor(0x602A, 0x1486);
	write_cmos_sensor(0x6F12, 0x1430);
	write_cmos_sensor(0x602A, 0x1490);
	write_cmos_sensor(0x6F12, 0x4D09);
	write_cmos_sensor(0x602A, 0x149E);
	write_cmos_sensor(0x6F12, 0x01C4);
	write_cmos_sensor(0x602A, 0x11CC);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x602A, 0x11CE);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x602A, 0x11D0);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x11DA);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x602A, 0x11E6);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x602A, 0x125E);
	write_cmos_sensor(0x6F12, 0x0048);
	write_cmos_sensor(0x602A, 0x11F4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x11F8);
	write_cmos_sensor(0x6F12, 0x0016);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0xF444, 0x05BF);
	write_cmos_sensor(0xF44A, 0x0008);
	write_cmos_sensor(0xF44E, 0x0012);
	write_cmos_sensor(0xF46E, 0x6CC0);
	write_cmos_sensor(0xF470, 0x7809);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1CAA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6000);
	write_cmos_sensor(0x6F12, 0x000F);
	write_cmos_sensor(0x602A, 0x6002);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x602A, 0x6004);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6006);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6008);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6010);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6012);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6014);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6016);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6018);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6020);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6022);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6024);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6026);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6028);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x1144);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1146);
	write_cmos_sensor(0x6F12, 0x1B00);
	write_cmos_sensor(0x602A, 0x1080);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1084);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x108A);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x1090);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1092);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1094);
	write_cmos_sensor(0x6F12, 0xA32E);
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001));
}
/*
static kal_uint16 get_vendor_id(void)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(0x01 >> 8), (char)(0x01 & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, 0xA0);

	return get_byte;
}
*/
static void custom1_setting(void)
{

	LOG_INF("E\n");
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0008);
	write_cmos_sensor(0x0346, 0x0008);
	write_cmos_sensor(0x0348, 0x0FA7);
	write_cmos_sensor(0x034A, 0x0BBF);
	write_cmos_sensor(0x034C, 0x07D0);
	write_cmos_sensor(0x034E, 0x05DC);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0000);
	write_cmos_sensor(0x0340, 0x18F4);
	write_cmos_sensor(0x0342, 0x09D2);
	write_cmos_sensor(0x0900, 0x0121);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0003);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0001);
	write_cmos_sensor(0x0404, 0x1000);
	write_cmos_sensor(0x0402, 0x1020);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0306, 0x00F1);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0008);
	write_cmos_sensor(0x030E, 0x0003);
	write_cmos_sensor(0x0312, 0x0001);
	write_cmos_sensor(0x0310, 0x0095);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1492);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x0E4E);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0118, 0x0004);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2126);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1168);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x602A, 0x2DB6);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1668);
	write_cmos_sensor(0x6F12, 0xFF00);
	write_cmos_sensor(0x602A, 0x166A);
	write_cmos_sensor(0x6F12, 0xFF00);
	write_cmos_sensor(0x602A, 0x118A);
	write_cmos_sensor(0x6F12, 0x0802);
	write_cmos_sensor(0x602A, 0x151E);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x217E);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1520);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x2522);
	write_cmos_sensor(0x6F12, 0x0804);
	write_cmos_sensor(0x602A, 0x2524);
	write_cmos_sensor(0x6F12, 0x0400);
	write_cmos_sensor(0x602A, 0x2568);
	write_cmos_sensor(0x6F12, 0x5500);
	write_cmos_sensor(0x602A, 0x2588);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x258C);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x25A6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x252C);
	write_cmos_sensor(0x6F12, 0x0601);
	write_cmos_sensor(0x602A, 0x252E);
	write_cmos_sensor(0x6F12, 0x0605);
	write_cmos_sensor(0x602A, 0x25A8);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25AC);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x25B0);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25B4);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x15A4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15A6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15A8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15AA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15AC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15AE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15B0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15B2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15B4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15B6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15B8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15BA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15BC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15BE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15C0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15C2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x15C4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15C6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15C8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15CA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15CC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15CE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15D0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15D2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15D4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15D6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15D8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15DA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15DC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15DE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15E0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15E2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x1A50);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1A54);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0D00, 0x0101);
	write_cmos_sensor(0x0D02, 0x0101);
	write_cmos_sensor(0x0114, 0x0301);
	write_cmos_sensor(0xF486, 0x0641);
	write_cmos_sensor(0xF488, 0x0A45);
	write_cmos_sensor(0xF48A, 0x0A49);
	write_cmos_sensor(0xF48C, 0x064D);
	write_cmos_sensor(0xF48E, 0x0651);
	write_cmos_sensor(0xF490, 0x0A55);
	write_cmos_sensor(0xF492, 0x0A59);
	write_cmos_sensor(0xF494, 0x065D);
	write_cmos_sensor(0xF496, 0x0661);
	write_cmos_sensor(0xF498, 0x0A65);
	write_cmos_sensor(0xF49A, 0x0A69);
	write_cmos_sensor(0xF49C, 0x066D);
	write_cmos_sensor(0xF49E, 0x0671);
	write_cmos_sensor(0xF4A0, 0x0A75);
	write_cmos_sensor(0xF4A2, 0x0A79);
	write_cmos_sensor(0xF4A4, 0x067D);
	write_cmos_sensor(0xF4A6, 0x0641);
	write_cmos_sensor(0xF4A8, 0x0A45);
	write_cmos_sensor(0xF4AA, 0x0A49);
	write_cmos_sensor(0xF4AC, 0x064D);
	write_cmos_sensor(0xF4AE, 0x0651);
	write_cmos_sensor(0xF4B0, 0x0A55);
	write_cmos_sensor(0xF4B2, 0x0A59);
	write_cmos_sensor(0xF4B4, 0x065D);
	write_cmos_sensor(0xF4B6, 0x0661);
	write_cmos_sensor(0xF4B8, 0x0A65);
	write_cmos_sensor(0xF4BA, 0x0A69);
	write_cmos_sensor(0xF4BC, 0x066D);
	write_cmos_sensor(0xF4BE, 0x0671);
	write_cmos_sensor(0xF4C0, 0x0A75);
	write_cmos_sensor(0xF4C2, 0x0A79);
	write_cmos_sensor(0xF4C4, 0x067D);
	write_cmos_sensor(0x0202, 0x0010);
	write_cmos_sensor(0x0226, 0x0010);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x107A);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x1074);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x0E7C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1120);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1122);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x602A, 0x1128);
	write_cmos_sensor(0x6F12, 0x0601);
	write_cmos_sensor(0x602A, 0x1AC0);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1AC2);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x1494);
	write_cmos_sensor(0x6F12, 0x3D68);
	write_cmos_sensor(0x602A, 0x1498);
	write_cmos_sensor(0x6F12, 0xF10D);
	write_cmos_sensor(0x602A, 0x1488);
	write_cmos_sensor(0x6F12, 0x0904);
	write_cmos_sensor(0x602A, 0x148A);
	write_cmos_sensor(0x6F12, 0x170B);
	write_cmos_sensor(0x602A, 0x150E);
	write_cmos_sensor(0x6F12, 0x00C2);
	write_cmos_sensor(0x602A, 0x1510);
	write_cmos_sensor(0x6F12, 0xC0AF);
	write_cmos_sensor(0x602A, 0x1512);
	write_cmos_sensor(0x6F12, 0x0080);
	write_cmos_sensor(0x602A, 0x1486);
	write_cmos_sensor(0x6F12, 0x1430);
	write_cmos_sensor(0x602A, 0x1490);
	write_cmos_sensor(0x6F12, 0x4D09);
	write_cmos_sensor(0x602A, 0x149E);
	write_cmos_sensor(0x6F12, 0x01C4);
	write_cmos_sensor(0x602A, 0x11CC);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x602A, 0x11CE);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x602A, 0x11D0);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x11DA);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x602A, 0x11E6);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x602A, 0x125E);
	write_cmos_sensor(0x6F12, 0x0048);
	write_cmos_sensor(0x602A, 0x11F4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x11F8);
	write_cmos_sensor(0x6F12, 0x0016);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0xF444, 0x05BF);
	write_cmos_sensor(0xF44A, 0x0008);
	write_cmos_sensor(0xF44E, 0x0012);
	write_cmos_sensor(0xF46E, 0x90C0);
	write_cmos_sensor(0xF470, 0x2809);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1CAA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6000);
	write_cmos_sensor(0x6F12, 0x000F);
	write_cmos_sensor(0x602A, 0x6002);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x602A, 0x6004);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6006);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6008);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6010);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6012);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6014);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6016);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6018);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6020);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6022);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6024);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6026);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6028);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x1144);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1146);
	write_cmos_sensor(0x6F12, 0x1B00);
	write_cmos_sensor(0x602A, 0x1080);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1084);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x108A);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x1090);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1092);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1094);
	write_cmos_sensor(0x6F12, 0xA32E);
}

static void custom2_setting(void)
{

	LOG_INF("E\n");
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x6214, 0x7971);
	write_cmos_sensor(0x6218, 0x7150);
	write_cmos_sensor(0x0344, 0x0058);
	write_cmos_sensor(0x0346, 0x01AC);
	write_cmos_sensor(0x0348, 0x0F57);
	write_cmos_sensor(0x034A, 0x0A1B);
	write_cmos_sensor(0x034C, 0x0500);
	write_cmos_sensor(0x034E, 0x02D0);
	write_cmos_sensor(0x0350, 0x0000);
	write_cmos_sensor(0x0352, 0x0000);
	write_cmos_sensor(0x0340, 0x0330);
	write_cmos_sensor(0x0342, 0x13A0);
	write_cmos_sensor(0x0900, 0x0123);
	write_cmos_sensor(0x0380, 0x0001);
	write_cmos_sensor(0x0382, 0x0002);
	write_cmos_sensor(0x0384, 0x0001);
	write_cmos_sensor(0x0386, 0x0005);
	write_cmos_sensor(0x0404, 0x1000);
	write_cmos_sensor(0x0402, 0x1810);
	write_cmos_sensor(0x0136, 0x1800);
	write_cmos_sensor(0x0304, 0x0006);
	write_cmos_sensor(0x030C, 0x0000);
	write_cmos_sensor(0x0306, 0x00F6);
	write_cmos_sensor(0x0302, 0x0001);
	write_cmos_sensor(0x0300, 0x0008);
	write_cmos_sensor(0x030E, 0x0003);
	write_cmos_sensor(0x0312, 0x0002);
	write_cmos_sensor(0x0310, 0x005B);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1492);
	write_cmos_sensor(0x6F12, 0x0078);
	write_cmos_sensor(0x602A, 0x0E4E);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0118, 0x0104);
	write_cmos_sensor(0x021E, 0x0000);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x2126);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1168);
	write_cmos_sensor(0x6F12, 0x0020);
	write_cmos_sensor(0x602A, 0x2DB6);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1668);
	write_cmos_sensor(0x6F12, 0xF0F0);
	write_cmos_sensor(0x602A, 0x166A);
	write_cmos_sensor(0x6F12, 0xF0F0);
	write_cmos_sensor(0x602A, 0x118A);
	write_cmos_sensor(0x6F12, 0x0802);
	write_cmos_sensor(0x602A, 0x151E);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x217E);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1520);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x2522);
	write_cmos_sensor(0x6F12, 0x0804);
	write_cmos_sensor(0x602A, 0x2524);
	write_cmos_sensor(0x6F12, 0x0400);
	write_cmos_sensor(0x602A, 0x2568);
	write_cmos_sensor(0x6F12, 0x5500);
	write_cmos_sensor(0x602A, 0x2588);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x258C);
	write_cmos_sensor(0x6F12, 0x1111);
	write_cmos_sensor(0x602A, 0x25A6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x252C);
	write_cmos_sensor(0x6F12, 0x0601);
	write_cmos_sensor(0x602A, 0x252E);
	write_cmos_sensor(0x6F12, 0x0605);
	write_cmos_sensor(0x602A, 0x25A8);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25AC);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x25B0);
	write_cmos_sensor(0x6F12, 0x1100);
	write_cmos_sensor(0x602A, 0x25B4);
	write_cmos_sensor(0x6F12, 0x0011);
	write_cmos_sensor(0x602A, 0x15A4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15A6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15A8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15AA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15AC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15AE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15B0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15B2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15B4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15B6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15B8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15BA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15BC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15BE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15C0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15C2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x15C4);
	write_cmos_sensor(0x6F12, 0x0141);
	write_cmos_sensor(0x602A, 0x15C6);
	write_cmos_sensor(0x6F12, 0x0545);
	write_cmos_sensor(0x602A, 0x15C8);
	write_cmos_sensor(0x6F12, 0x0649);
	write_cmos_sensor(0x602A, 0x15CA);
	write_cmos_sensor(0x6F12, 0x024D);
	write_cmos_sensor(0x602A, 0x15CC);
	write_cmos_sensor(0x6F12, 0x0151);
	write_cmos_sensor(0x602A, 0x15CE);
	write_cmos_sensor(0x6F12, 0x0555);
	write_cmos_sensor(0x602A, 0x15D0);
	write_cmos_sensor(0x6F12, 0x0659);
	write_cmos_sensor(0x602A, 0x15D2);
	write_cmos_sensor(0x6F12, 0x025D);
	write_cmos_sensor(0x602A, 0x15D4);
	write_cmos_sensor(0x6F12, 0x0161);
	write_cmos_sensor(0x602A, 0x15D6);
	write_cmos_sensor(0x6F12, 0x0565);
	write_cmos_sensor(0x602A, 0x15D8);
	write_cmos_sensor(0x6F12, 0x0669);
	write_cmos_sensor(0x602A, 0x15DA);
	write_cmos_sensor(0x6F12, 0x026D);
	write_cmos_sensor(0x602A, 0x15DC);
	write_cmos_sensor(0x6F12, 0x0171);
	write_cmos_sensor(0x602A, 0x15DE);
	write_cmos_sensor(0x6F12, 0x0575);
	write_cmos_sensor(0x602A, 0x15E0);
	write_cmos_sensor(0x6F12, 0x0679);
	write_cmos_sensor(0x602A, 0x15E2);
	write_cmos_sensor(0x6F12, 0x027D);
	write_cmos_sensor(0x602A, 0x1A50);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1A54);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0x0D00, 0x0100);
	write_cmos_sensor(0x0D02, 0x0101);
	write_cmos_sensor(0x0114, 0x0300);
	write_cmos_sensor(0xF486, 0x0000);
	write_cmos_sensor(0xF488, 0x0000);
	write_cmos_sensor(0xF48A, 0x0000);
	write_cmos_sensor(0xF48C, 0x0000);
	write_cmos_sensor(0xF48E, 0x0000);
	write_cmos_sensor(0xF490, 0x0000);
	write_cmos_sensor(0xF492, 0x0000);
	write_cmos_sensor(0xF494, 0x0000);
	write_cmos_sensor(0xF496, 0x0000);
	write_cmos_sensor(0xF498, 0x0000);
	write_cmos_sensor(0xF49A, 0x0000);
	write_cmos_sensor(0xF49C, 0x0000);
	write_cmos_sensor(0xF49E, 0x0000);
	write_cmos_sensor(0xF4A0, 0x0000);
	write_cmos_sensor(0xF4A2, 0x0000);
	write_cmos_sensor(0xF4A4, 0x0000);
	write_cmos_sensor(0xF4A6, 0x0000);
	write_cmos_sensor(0xF4A8, 0x0000);
	write_cmos_sensor(0xF4AA, 0x0000);
	write_cmos_sensor(0xF4AC, 0x0000);
	write_cmos_sensor(0xF4AE, 0x0000);
	write_cmos_sensor(0xF4B0, 0x0000);
	write_cmos_sensor(0xF4B2, 0x0000);
	write_cmos_sensor(0xF4B4, 0x0000);
	write_cmos_sensor(0xF4B6, 0x0000);
	write_cmos_sensor(0xF4B8, 0x0000);
	write_cmos_sensor(0xF4BA, 0x0000);
	write_cmos_sensor(0xF4BC, 0x0000);
	write_cmos_sensor(0xF4BE, 0x0000);
	write_cmos_sensor(0xF4C0, 0x0000);
	write_cmos_sensor(0xF4C2, 0x0000);
	write_cmos_sensor(0xF4C4, 0x0000);
	write_cmos_sensor(0x0202, 0x0010);
	write_cmos_sensor(0x0226, 0x0010);
	write_cmos_sensor(0x0204, 0x0020);
	write_cmos_sensor(0x0B06, 0x0101);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x107A);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x1074);
	write_cmos_sensor(0x6F12, 0x1D00);
	write_cmos_sensor(0x602A, 0x0E7C);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1120);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1122);
	write_cmos_sensor(0x6F12, 0x0028);
	write_cmos_sensor(0x602A, 0x1128);
	write_cmos_sensor(0x6F12, 0x0604);
	write_cmos_sensor(0x602A, 0x1AC0);
	write_cmos_sensor(0x6F12, 0x0200);
	write_cmos_sensor(0x602A, 0x1AC2);
	write_cmos_sensor(0x6F12, 0x0002);
	write_cmos_sensor(0x602A, 0x1494);
	write_cmos_sensor(0x6F12, 0x3D68);
	write_cmos_sensor(0x602A, 0x1498);
	write_cmos_sensor(0x6F12, 0xF10D);
	write_cmos_sensor(0x602A, 0x1488);
	write_cmos_sensor(0x6F12, 0x0F0F);
	write_cmos_sensor(0x602A, 0x148A);
	write_cmos_sensor(0x6F12, 0x170F);
	write_cmos_sensor(0x602A, 0x150E);
	write_cmos_sensor(0x6F12, 0x00C2);
	write_cmos_sensor(0x602A, 0x1510);
	write_cmos_sensor(0x6F12, 0xC0AF);
	write_cmos_sensor(0x602A, 0x1512);
	write_cmos_sensor(0x6F12, 0x0080);
	write_cmos_sensor(0x602A, 0x1486);
	write_cmos_sensor(0x6F12, 0x1430);
	write_cmos_sensor(0x602A, 0x1490);
	write_cmos_sensor(0x6F12, 0x4D09);
	write_cmos_sensor(0x602A, 0x149E);
	write_cmos_sensor(0x6F12, 0x01C4);
	write_cmos_sensor(0x602A, 0x11CC);
	write_cmos_sensor(0x6F12, 0x0008);
	write_cmos_sensor(0x602A, 0x11CE);
	write_cmos_sensor(0x6F12, 0x000B);
	write_cmos_sensor(0x602A, 0x11D0);
	write_cmos_sensor(0x6F12, 0x0003);
	write_cmos_sensor(0x602A, 0x11DA);
	write_cmos_sensor(0x6F12, 0x0012);
	write_cmos_sensor(0x602A, 0x11E6);
	write_cmos_sensor(0x6F12, 0x002A);
	write_cmos_sensor(0x602A, 0x125E);
	write_cmos_sensor(0x6F12, 0x0048);
	write_cmos_sensor(0x602A, 0x11F4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x11F8);
	write_cmos_sensor(0x6F12, 0x0016);
	write_cmos_sensor(0x6028, 0x4000);
	write_cmos_sensor(0xF444, 0x05BF);
	write_cmos_sensor(0xF44A, 0x0008);
	write_cmos_sensor(0xF44E, 0x0012);
	write_cmos_sensor(0xF46E, 0x6CC0);
	write_cmos_sensor(0xF470, 0x7809);
	write_cmos_sensor(0x6028, 0x2000);
	write_cmos_sensor(0x602A, 0x1CAA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CAE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CB8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBA);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBC);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CBE);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC0);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC2);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC4);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC6);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1CC8);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6000);
	write_cmos_sensor(0x6F12, 0x000F);
	write_cmos_sensor(0x602A, 0x6002);
	write_cmos_sensor(0x6F12, 0xFFFF);
	write_cmos_sensor(0x602A, 0x6004);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x6006);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6008);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x600E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6010);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6012);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6014);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6016);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6018);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x601E);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6020);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6022);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6024);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6026);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x6028);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602A);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x602C);
	write_cmos_sensor(0x6F12, 0x1000);
	write_cmos_sensor(0x602A, 0x1144);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1146);
	write_cmos_sensor(0x6F12, 0x1B00);
	write_cmos_sensor(0x602A, 0x1080);
	write_cmos_sensor(0x6F12, 0x0100);
	write_cmos_sensor(0x602A, 0x1084);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x108A);
	write_cmos_sensor(0x6F12, 0x00C0);
	write_cmos_sensor(0x602A, 0x1090);
	write_cmos_sensor(0x6F12, 0x0001);
	write_cmos_sensor(0x602A, 0x1092);
	write_cmos_sensor(0x6F12, 0x0000);
	write_cmos_sensor(0x602A, 0x1094);
	write_cmos_sensor(0x6F12, 0xA32E);
}

/*************************************************************************
*FUNCTION
*  get_imgsensor_id
*
*DESCRIPTION
*  This function get the sensor ID
*
*PARAMETERS
*  *sensorID : return the sensor ID
*
*RETURNS
*  None
*
*GLOBALS AFFECTED
*
*************************************************************************/
extern int hbb_flag;
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();

			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			LOG_INF("Read sensor id fail, id: 0x%x\n",
				imgsensor.i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct,
		 *Must set *sensor_id to 0xFFFFFFFF
		 */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
*FUNCTION
*  open
*
*DESCRIPTION
*  This function initialize the registers of CMOS sensor
*
*PARAMETERS
*  None
*
*RETURNS
*  None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{

	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

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
			LOG_INF("Read sensor id fail, id: 0x%x, sensor id: 0x%x\n",
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

	sensor_init();
	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

/*************************************************************************
*FUNCTION
*  close
*
*DESCRIPTION
*
*
*PARAMETERS
*  None
*
*RETURNS
*  None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	return ERROR_NONE;
}

/*************************************************************************
*FUNCTION
*preview
*
*DESCRIPTION
*  This function start the sensor preview.
*
*PARAMETERS
*  *image_window : address pointer of pixel numbers in one period of HSYNC
**sensor_config_data : address pointer of line numbers in one period of VSYNC
*
*RETURNS
*  None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32
preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;

	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);   //preview use capture setting
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}

/*************************************************************************
*FUNCTION
*  capture
*
*DESCRIPTION
*  This function setup the CMOS sensor in capture MY_OUTPUT mode
*
*PARAMETERS
*
*RETURNS
*  None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32
capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
/*
#ifdef FANPENGTAO
	int i;
#endif
*/
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
		LOG_INF("capture30fps: use cap30FPS's setting: %d fps!\n",
				imgsensor.current_fps / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {

		LOG_INF("cap115fps: use cap1's setting: %d fps!\n",
				imgsensor.current_fps / 10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		LOG_INF
		("Warning:current_fps %d is not support, use cap1\n",
		 imgsensor.current_fps / 10);
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	set_mirror_flip(IMAGE_NORMAL);
	mdelay(10);
/*
#ifdef FANPENGTAO
	for (i = 0; i < 10; i++) {
		LOG_INF("delay time = %d, the frame no = %d\n", i * 10,
				read_cmos_sensor(0x0005));
		mdelay(10);
	}
#endif
*/
	return ERROR_NONE;
}

static kal_uint32
normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
	 image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;

	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}

static kal_uint32
hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		 image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;

	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;

	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}

static kal_uint32
slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		   image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
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
	set_mirror_flip(IMAGE_NORMAL);
	return ERROR_NONE;
}

/*************************************************************************
*FUNCTION
*Custom1
*
*DESCRIPTION
* This function start the sensor Custom1.
*
*PARAMETERS
* *image_window : address pointer of pixel numbers in one period of HSYNC
**sensor_config_data : address pointer of line numbers in one period of VSYNC
*
*RETURNS
* None
*
*GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32
Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;

	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	return ERROR_NONE;
}

static kal_uint32
Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;

	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	return ERROR_NONE;
}

#if 0
static kal_uint32
Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;

	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}

static kal_uint32
Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;

	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}

static kal_uint32
Custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *
		image_window, MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;

	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}
#endif
static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
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
	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;
	sensor_resolution->SensorCustom2Width =
		imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height =
		imgsensor_info.custom2.grabwindow_height;
	return ERROR_NONE;
}

static kal_uint32
get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		 MSDK_SENSOR_INFO_STRUCT *sensor_info,
		 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_HIGH;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_HIGH;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_HIGH;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_HIGH;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;
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
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;
	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;
	sensor_info->SensorPixelClockCount = 3;
	sensor_info->SensorDataLatchCount = 2;
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
	sensor_info->SensorPacketECCOrder = 1;
#ifdef FPTPDAFSUPPORT
	sensor_info->PDAF_Support = 2;
#else
	sensor_info->PDAF_Support = 0;
#endif
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;

	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}
	return ERROR_NONE;
}

static kal_uint32
control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);
		break;

	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);

	if (framerate == 0)

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
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32
set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM
	  scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			 imgsensor_info.pre.framelength) ? (frame_length -
			imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);

		if (imgsensor.frame_length > imgsensor.shutter)
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
		  ? (frame_length -
		   imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
			 imgsensor_info.cap1.max_framerate) {
			frame_length =
				imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
			(frame_length > imgsensor_info.cap1.framelength)
			 ? (frame_length - imgsensor_info.cap1.framelength)
			  : 0;
			imgsensor.frame_length =
				imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
					imgsensor_info.cap.max_framerate)
				LOG_INF
		("current_fps %d is not support, so use cap' %d fps!\n",
				 framerate,
				 imgsensor_info.cap.max_framerate / 10);
			frame_length =
				imgsensor_info.cap.pclk / framerate * 10 /
				imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line =
				(frame_length > imgsensor_info.cap.framelength)
				 ? (frame_length -
				 imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length =
			imgsensor_info.hs_video.pclk / framerate * 10 /
			imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
		 ? (frame_length - imgsensor_info.hs_video.framelength)
		 : 0;
		imgsensor.frame_length =
			imgsensor_info.hs_video.framelength
			 + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length =
			imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			  ? (frame_length -
			 imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		if (imgsensor.current_fps !=
				imgsensor_info.custom1.max_framerate)
			LOG_INF
			("%d fps is not support, so use cap: %d fps!\n",
			 framerate,
			 imgsensor_info.custom1.max_framerate / 10);
		frame_length =
			imgsensor_info.custom1.pclk / framerate * 10 /
			imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			 imgsensor_info.custom1.framelength) ? (frame_length -
			 imgsensor_info.custom1.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
			imgsensor_info.custom1.framelength
			 + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length =
			imgsensor_info.custom2.pclk / framerate * 10 /
			imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			 imgsensor_info.custom2.framelength) ? (frame_length -
			 imgsensor_info.custom2.framelength) : 0;
		if (imgsensor.dummy_line < 0)
			imgsensor.dummy_line = 0;
		imgsensor.frame_length =
			imgsensor_info.custom2.framelength
			 + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
		frame_length =
			imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line =
			(frame_length >
			 imgsensor_info.pre.framelength) ? (frame_length -
			imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length =
			imgsensor_info.pre.framelength
			 + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
				scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum
		MSDK_SCENARIO_ID_ENUM
		scenario_id,
		MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);
	if (enable) {

		write_cmos_sensor(0x0200, 0x0002);
		write_cmos_sensor(0x0202, 0x0002);
		write_cmos_sensor(0x0204, 0x0020);
		write_cmos_sensor(0x020E, 0x0100);
		write_cmos_sensor(0x0210, 0x0100);
		write_cmos_sensor(0x0212, 0x0100);
		write_cmos_sensor(0x0214, 0x0100);
		write_cmos_sensor(0x0600, 0x0002);
	} else {

		write_cmos_sensor(0x0600, 0x0000);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32
feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	UINT32 fps = 0;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	pr_debug("feature_id = %d, len=%d\n", feature_id, *feature_para_len);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		pr_debug("imgsensor.pclk = %d,current_fps = %d\n",
				 imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter((kal_uint32)*feature_data);
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
		write_cmos_sensor(sensor_reg_data->RegAddr,
						  sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM or
		 *just return LENS_DRIVER_ID_DO_NOT_CARE
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
		set_auto_flicker_mode((BOOL) * feature_data_16,
			  *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
			  *feature_data, *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)
			  *(feature_data), (MUINT32 *) (uintptr_t) (*
			  (feature_data + 1)));
		break;

	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) * feature_data);
		break;

	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;

	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_SET_HDR:
		pr_debug("hdr enable :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = (UINT8) *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;

	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
				 (UINT32) * feature_data_32);

		wininfo =
			(struct SENSOR_WINSIZE_INFO_STRUCT
			 *)(uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				   (void *)&imgsensor_winsize_info[1],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				   (void *)&imgsensor_winsize_info[2],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
				   (void *)&imgsensor_winsize_info[3],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				   (void *)&imgsensor_winsize_info[4],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				   (void *)&imgsensor_winsize_info[5],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
				   (void *)&imgsensor_winsize_info[6],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				   (void *)&imgsensor_winsize_info[0],
				   sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		/*pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
				 (UINT16) *feature_data,
				 (UINT16) *(feature_data + 1),
				 (UINT16) *(feature_data + 2));*/

		/*ihdr_write_shutter_gain((UINT16)*feature_data,
		 *(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
		 */
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data),
			 (UINT16) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
				 (UINT16) *feature_data,
				 (UINT16) *(feature_data + 1));
		/*ihdr_write_shutter(
		 *(UINT16)*feature_data,(UINT16)*(feature_data+1));
		 */
		break;

	case SENSOR_FEATURE_GET_PDAF_INFO:
		pr_debug("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
				 (UINT16) *feature_data);
		PDAFinfo =
			(struct SET_PD_BLOCK_INFO_T
			 *)(uintptr_t) (*(feature_data + 1));
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_CUSTOM1:
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
				   sizeof(struct SET_PD_BLOCK_INFO_T));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pr_debug("SENSOR_FEATURE_GET_VC_INFO %d\n",
				 (UINT16) *feature_data);
		pvcinfo =
			(struct SENSOR_VC_INFO_STRUCT
			 *)(uintptr_t) (*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)pvcinfo,
				   (void *)&SENSOR_VC_INFO[2],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
		default:
			memcpy((void *)pvcinfo,
				   (void *)&SENSOR_VC_INFO[0],
				   sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		pr_debug
		("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
		 (UINT16) *feature_data);

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:

			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:

			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
			break;
		}
		break;
	case SENSOR_FEATURE_SET_PDAF:
		pr_debug("PDAF mode :%d\n", *feature_data_16);
		imgsensor.pdaf_mode = *feature_data_16;
		break;

	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
				 *feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.cap.pclk /
				 (imgsensor_info.cap.linelength - 80)) *
				imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.normal_video.pclk /
				 (imgsensor_info.normal_video.linelength - 80))
				 *imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.hs_video.pclk /
				 (imgsensor_info.hs_video.linelength - 80)) *
				imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.slim_video.pclk /
				 (imgsensor_info.slim_video.linelength - 80)) *
				imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.slim_video.pclk /
				 (imgsensor_info.slim_video.linelength - 80)) *
				imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				(imgsensor_info.pre.pclk /
				 (imgsensor_info.pre.linelength - 80)) *
				imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		fps = (MUINT32) (*(feature_data + 2));

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) =
				imgsensor_info.pre.mipi_pixel_rate;
			break;
		}

		break;

//cxc long exposure >
		case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
			*feature_return_para_32 =
				imgsensor.current_ae_effective_frame;
			break;
		case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
			memcpy(feature_return_para_32, &imgsensor.ae_frm_mode,
				sizeof(struct IMGSENSOR_AE_FRM_MODE));
			break;
//cxc long exposure <

	default:
		break;
	}
	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32
S5KGM1SP_OFILM_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{

	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
