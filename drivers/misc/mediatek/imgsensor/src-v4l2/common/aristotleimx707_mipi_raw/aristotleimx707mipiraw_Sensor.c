#define PFX "ARISTOTLEIMX707_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/timer.h>
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
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "aristotleimx707_ana_gain_table.h"
#include "aristotleimx707mipiraw_Sensor.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor_8(...)  subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor(...)    subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor(...)   subdrv_i2c_wr_u16(__VA_ARGS__)
#define aristotleimx707_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)
#define aristotleimx707_burst_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8_burst(__VA_ARGS__)
#define LOG_TAG "[aristotleimx707]"
#define IMX707_LOG_INF(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)
#define IMX707_LOG_DBG(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)

#undef VENDOR_EDIT
#define USE_BURST_MODE 1
#define ByPass 0
#define LONG_EXP 1

static kal_uint8 enable_seamless = 0;
static struct SET_SENSOR_AWB_GAIN last_sensor_awb;
static kal_uint16 previous_exp_cnt;
static kal_uint32 previous_exp[3];
static kal_uint32 previous_fll[3];
static unsigned long long last_sof_timestamps_ns = 0;
#ifdef LONG_EXP
static int longexposure_times = 0;
static int long_exposure_status;
#endif

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = ARISTOTLEIMX707_SENSOR_ID,

	.checksum_value = 0xa4c32546,

	.pre = { /* preview reb_B3 Tline > 8000ns */
		.pclk = 1689600000,
		.linelength = 13536,
		.framelength = 4160,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300,
	},

	.cap = { /*capture*/
		.pclk = 1689600000,
		.linelength = 13536,
		.framelength = 4160,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300,
	},

	.normal_video = { /*normal_video*/
		.pclk = 3456000000,
		.linelength = 13536,
		.framelength = 8508,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300,
	},

	.hs_video = { /* 1920x1080 @240fps */
		.pclk = 3456000000,
		.linelength = 9424,
		.framelength = 1528,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1152,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 2400,
	},

	.slim_video = { /* 4:3 video */
		.pclk = 1689600000,
		.linelength = 13536,
		.framelength = 4160,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300,
	},

	.custom1 = { /*HD preview reg_B1 Tline=7150ns*/
		.pclk = 1891200000,
		.linelength = 13536,
		.framelength = 4656,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300,
	},

	.custom2 = { /*1920x1080 @120fps*/
		.pclk = 3456000000,
		.linelength = 9424,
		.framelength = 3056,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1152,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 1200,
	},

	.custom3 = { /*60fps*/
		.pclk = 3456000000,
		.linelength = 13536,
		.framelength = 4252,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 600,
	},

	.custom4 = { /*fullsize*/
		.pclk = 1891200000,
		.linelength = 19008,
		.framelength = 6632,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 8192,
		.grabwindow_height = 6144,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 150,
	},

	.custom5 = { /*in sensor zoom*/
		.pclk = 1891200000,
		.linelength = 19008,
		.framelength = 3316,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300,
	},

	.custom6 = { /* bokeh */
		.pclk = 1689600000,
		.linelength = 13536,
		.framelength = 5200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 240,
	},

	.custom7 = { /* super night video */
		.pclk = 3456000000,
		.linelength = 13536,
		.framelength = 10634,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 240,
	},

	.custom8 = { /* 2x bokeh */
		.pclk = 2332800000,
		.linelength = 13536,
		.framelength = 7180,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 240,
	},

	.custom9 = { /* pre */
		.pclk = 1689600000,
		.linelength = 13536,
		.framelength = 4160,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 300,
	},

	.custom10 = { /* 8K@24fps */
		.pclk = 3043200000,
		.linelength = 19008,
		.framelength = 6670,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 7680,
		.grabwindow_height = 4320,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 240,
	},

	.custom11 = { /* Leica Classic bokeh */
		.pclk = 1689600000,
		.linelength = 13536,
		.framelength = 5200,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 240,
	},

	.custom12 = { /* 2x bokeh Leica Classic */
		.pclk = 2332800000,
		.linelength = 13536,
		.framelength = 7180,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1711540000,
		.max_framerate = 240,
	},

	.margin           = 96,   /* sensor framelength & shutter margin */
	.min_shutter      = 16,   /* min shutter */
	.min_gain         = 1  * BASEGAIN, //1024*1x
	.max_gain         = 64 * BASEGAIN, //64x
	.min_gain_iso     = 50,
	.exp_step         = 2,
	.gain_step        = 1,
	.gain_type        = 0,
	.max_frame_length = 0xFFFF * 128,

	.ae_shut_delay_frame        = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame     = 2, /* isp gain delay frame for AE cycle */
	.frame_time_delay_frame     = 3,

	.ihdr_support               = 0, /* 1, support; 0,not support */
	.ihdr_le_firstline          = 0, /* 1,le first ; 0, se first */
	.temperature_support        = 1, /* 1, support; 0,not support */

	.sensor_mode_num            = 17,/* support sensor mode num */

	.cap_delay_frame        = 2, /* enter capture delay frame num */
	.pre_delay_frame        = 2, /* enter preview delay frame num */
	.video_delay_frame      = 2, /* enter video delay frame num */
	.hs_video_delay_frame   = 2,
	.slim_video_delay_frame = 2, /* enter slim video delay frame num */
	.custom1_delay_frame    = 2, /* enter custom1 delay frame num */
	.custom2_delay_frame    = 2, /* enter custom2 delay frame num */
	.custom3_delay_frame    = 2, /* enter custom3 delay frame num */
	.custom4_delay_frame    = 2, /* enter custom4 delay frame num */
	.custom5_delay_frame    = 2, /* enter custom5 delay frame num */
	.custom6_delay_frame    = 2, /* enter custom6 delay frame num */
	.custom7_delay_frame    = 2, /* enter custom7 delay frame num */
	.custom8_delay_frame    = 2, /* enter custom8 delay frame num */
	.custom9_delay_frame    = 2, /* enter custom9 delay frame num */
	.custom10_delay_frame   = 2, /* enter custom10 delay frame num */
	.custom11_delay_frame   = 2, /* enter custom11 delay frame num */
	.custom12_delay_frame   = 2, /* enter custom12 delay frame num */

	.isp_driving_current      = ISP_DRIVING_6MA,
	.sensor_interface_type    = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type         = MIPI_CPHY, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode   = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.mclk           = 24,   /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num  = SENSOR_MIPI_3_LANE,
	.i2c_addr_table = {0x20, 0xff},
	.i2c_speed      = 1000, /* i2c read/write speed */
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* Preview */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* capture */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   384, 4096, 2304,  0,  0, 4096, 2304}, /* normal video */
	{8192, 6144, 0,    0,    8192, 6144, 2048, 1536, 0,   192, 2048, 1152,  0,  0, 2048, 1152}, /* hs_video */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* slim video */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* custom1 */
	{8192, 6144, 0,    0,    8192, 6144, 2048, 1536, 0,   192, 2048, 1152,  0,  0, 2048, 1152}, /* custom2 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   384, 4096, 2304,  0,  0, 4096, 2304}, /* custom3 */
	{8192, 6144, 0,    0,    8192, 6144, 8192, 6144, 0,   0,   8192, 6144,  0,  0, 8192, 6144}, /* custom4 */
	{8192, 6144, 2048, 1536, 4096, 3072, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* custom5 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* custom6 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   384, 4096, 2304,  0,  0, 4096, 2304}, /* custom7 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 608, 456, 2880, 2160,  0,  0, 2880, 2160}, /* custom8 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* custom9 */
	{8192, 6144, 256,  912,  7680, 4320, 7680, 4320, 0,   0,   7680, 4320,  0,  0, 7680, 4320}, /* custom10 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* custom11 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 608, 456, 2880, 2160,  0,  0, 2880, 2160}, /* custom12 */
};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[] = {
	{/* preview/cust11*/
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4096, 3072, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 4096, 768,  0x00, 0x00, 0x0000, 0x0000
	},
	{/* normal video */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4096, 2304, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 4096, 576,  0x00, 0x00, 0x0000, 0x0000
	},
	{/* cust2 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 2048, 1152, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 2048, 288,  0x00, 0x00, 0x0000, 0x0000
	},
	{/* cust3 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4096, 2304, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 4096, 576,  0x00, 0x00, 0x0000, 0x0000
	},
	{/* cust4 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 8192, 6144, 0x00, 0x00, 0x0000, 0x0000,
	},
	{/* cust5 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4096, 3072, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 4096, 768,  0x00, 0x00, 0x0000, 0x0000
	},
	{/* cust8 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 2880, 2160, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 2880, 540,  0x00, 0x00, 0x0000, 0x0000
	},
	{/* cust10 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 7680, 4320, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 7680, 1080, 0x00, 0x00, 0x0000, 0x0000
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX  = 0,
	.i4PitchY  = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL    = {{0, 0} },
	.i4PosR    = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst   = 0,
	.i4Crop = {
		{0, 0}, {0,  0  }, {0, 384}, {64, 228}, {0, 0  },
		{0, 0}, {64, 228}, {0, 384}, {0,  0  }, {2048, 1536},
		{0, 0}, {0,  384}, {608, 456}, {0, 0}, {256,912}, {0, 0}
	},
	.iMirrorFlip = IMAGE_NORMAL,
};

static kal_uint16 aristotleimx707_QSC_setting[2304 * 2];
//static kal_uint16 aristotleimx707_LRC_setting[384 * 2];


static void aristotleimx707_get_pdaf_reg_setting(struct subdrv_ctx *ctx, MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(ctx, regDa[idx]);
		IMX707_LOG_INF("%x %x", regDa[idx], regDa[idx+1]);
	}
}
static void aristotleimx707_set_pdaf_reg_setting(struct subdrv_ctx *ctx,
		MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor_8(ctx, regDa[idx], regDa[idx + 1]);
		IMX707_LOG_INF("%x %x", regDa[idx], regDa[idx+1]);
	}
}

static kal_uint16 read_cmos_eeprom_8(struct subdrv_ctx *ctx, kal_uint16 addr)
{
	u8 val;

	adaptor_i2c_rd_u8(ctx->i2c_client, 0xA2 >> 1, addr, &val);

	return (u16)val;
}

static void write_sensor_QSC(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("Entet %s +++\n", __FUNCTION__);

	aristotleimx707_burst_write_cmos_sensor(ctx, aristotleimx707_QSC_setting,
		sizeof(aristotleimx707_QSC_setting) / sizeof(kal_uint16));

	IMX707_LOG_INF("Entet %s ---\n", __FUNCTION__);
}
#if 0
static void write_sensor_LRC(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("Entet %s +++\n", __FUNCTION__);

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_LRC_setting,
		sizeof(aristotleimx707_LRC_setting) / sizeof(kal_uint16));

	IMX707_LOG_INF("Entet %s ---\n", __FUNCTION__);
}
#endif
static void read_sensor_Cali(struct subdrv_ctx *ctx)
{
	kal_uint16 idx          = 0;
	kal_uint16 eeprom_qsc   = 0x2956, sensor_qsc   = 0x6700;
//	kal_uint16 eeprom_lrc_0 = 0x18F6, sensor_lrc_0 = 0x19B6;
//	kal_uint16 eeprom_lrc_1 = 0x7B10, sensor_lrc_1 = 0x7C00;

	IMX707_LOG_INF("Entet %s +++\n", __FUNCTION__);
	if(read_cmos_eeprom_8(ctx,0x000C ) == 0x63){//eeprom sensor id
		for (idx = 0; idx < 2304; idx++) {
				aristotleimx707_QSC_setting[2 * idx]	 = sensor_qsc + idx;
				aristotleimx707_QSC_setting[2 * idx + 1] = read_cmos_eeprom_8(ctx, eeprom_qsc + idx);
		}
		ctx->is_read_preload_eeprom = 1;
	}else{
		IMX707_LOG_INF("%s EEPROM mismatch", __FUNCTION__);
	}

#if 0
	for (idx = 0; idx < 192; idx++) {
		aristotleimx707_LRC_setting[2 * idx]               = sensor_lrc_0 + idx;
		aristotleimx707_LRC_setting[2 * idx + 1]           = read_cmos_eeprom_8(ctx, eeprom_lrc_0 + idx);
		aristotleimx707_LRC_setting[2 * idx + 192 * 2]     = sensor_lrc_1 + idx;
		aristotleimx707_LRC_setting[2 * idx + 1 + 192 * 2] = read_cmos_eeprom_8(ctx, eeprom_lrc_1 + idx);
	}
#endif

	IMX707_LOG_INF("Entet %s ---\n", __FUNCTION__);
}

static void set_dummy(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("dummyline = %d, dummypixels = %d\n",
		ctx->dummy_line, ctx->dummy_pixel);
}	/*	set_dummy  */

static void set_mirror_flip(struct subdrv_ctx *ctx, kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	IMX707_LOG_INF("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(ctx, 0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
	write_cmos_sensor_8(ctx, 0x0101, itemp);
	break;

	case IMAGE_V_MIRROR:
	write_cmos_sensor_8(ctx, 0x0101, itemp | 0x02);
	break;

	case IMAGE_H_MIRROR:
	write_cmos_sensor_8(ctx, 0x0101, itemp | 0x01);
	break;

	case IMAGE_HV_MIRROR:
	write_cmos_sensor_8(ctx, 0x0101, itemp | 0x03);
	break;
	}
}

static void set_max_framerate(struct subdrv_ctx *ctx,
		UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = ctx->frame_length;

	IMX707_LOG_INF(
		"framerate = %d, min framelength should enable %d\n", framerate,
		min_framelength_en);

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
	if (frame_length >= ctx->min_frame_length)
		ctx->frame_length = frame_length;
	else
		ctx->frame_length = ctx->min_frame_length;

	ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;
		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
}	/*	set_max_framerate  */

static void set_max_framerate_video(struct subdrv_ctx *ctx, UINT16 framerate,
					kal_bool min_framelength_en)
{
	set_max_framerate(ctx, framerate, min_framelength_en);
	set_dummy(ctx);
}


static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter, bool hold_en)
{
	kal_uint16 realtime_fps = 0;
	int i = 0;

	if (shutter > ctx->min_frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;
	else
		ctx->frame_length = ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	// Framelength should be an even number
	shutter = (shutter >> 1) << 1;
	ctx->frame_length = (ctx->frame_length >> 1) << 1;
	ctx->min_frame_length = (ctx->min_frame_length >> 1) << 1;

	for (i = 0; i < previous_exp_cnt; i++) {
		previous_exp[i] = 0;
		previous_fll[i] = 0;
	}
	previous_exp[0] = shutter;
	previous_fll[0] = ctx->frame_length;
	previous_exp_cnt = 1;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10
				/ ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
	}

	if (ctx->fast_mode_on) {
		IMX707_LOG_INF("skip : fast_mode_on(%d)", ctx->fast_mode_on);
		return;
	}

	if (hold_en) {
		write_cmos_sensor_8(ctx, 0x0104, 0x01);
	}

#ifdef LONG_EXP
	longexposure_times = 0;
	while (shutter >= 65535) {
		shutter = shutter / 2;
		longexposure_times += 1;
	}
	if (longexposure_times > 0) {
		IMX707_LOG_INF("enter long exposure mode, time is %d",
			longexposure_times);
		long_exposure_status = 1;
		ctx->frame_length = ctx->min_frame_length;
		write_cmos_sensor_8(ctx, 0x3100, longexposure_times & 0x07);
	} else if (long_exposure_status == 1) {
		long_exposure_status = 0;
		write_cmos_sensor_8(ctx, 0x3100, longexposure_times & 0x00);

		IMX707_LOG_INF("exit long exposure mode");
	}
#endif

	// Update Shutter
	// 0x0350 = 1, Set the minimum FLL here and let the sensor calculate the required FLL
	write_cmos_sensor_8(ctx, 0x0350, 0x01);
	write_cmos_sensor_8(ctx, 0x0340, (ctx->min_frame_length >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0341, ctx->min_frame_length & 0xFF);
	write_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, shutter  & 0xFF);

	IMX707_LOG_INF("shutter =%d, framelength =%d, min_frame_length=%d\n",
		shutter, ctx->frame_length, ctx->min_frame_length);
}	/*	write_shutter  */

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
static void set_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter, bool hold_en)
{
	ctx->shutter = shutter;

	write_shutter(ctx, shutter, hold_en);
} /* set_shutter */


static void set_frame_length(struct subdrv_ctx *ctx, kal_uint16 frame_length)
{
	if (frame_length > 1)
		ctx->frame_length = frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (ctx->min_frame_length > ctx->frame_length)
		ctx->frame_length = ctx->min_frame_length;

	/* Extend frame length */
	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
	write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	IMX707_LOG_INF("Framelength: set=%d/input=%d/min=%d, auto_extend=%d\n",
		ctx->frame_length, frame_length, ctx->min_frame_length,
		read_cmos_sensor_8(ctx, 0x0350));
}


static void set_multi_shutter_frame_length(struct subdrv_ctx *ctx,
				kal_uint32 *shutters, kal_uint16 shutter_cnt,
				kal_uint32 frame_length)
{
  	int i;
	if (ctx->fast_mode_on) {
		IMX707_LOG_INF("skip : fast_mode_on(%d)", ctx->fast_mode_on);
		return;
	}

	if (shutter_cnt == 1) {
		ctx->shutter = shutters[0];

		if (shutters[0] > ctx->min_frame_length - imgsensor_info.margin)
			ctx->frame_length = shutters[0] + imgsensor_info.margin;
		else
			ctx->frame_length = ctx->min_frame_length;
		if (frame_length > ctx->frame_length)
			ctx->frame_length = frame_length;
		if (ctx->frame_length > imgsensor_info.max_frame_length)
			ctx->frame_length = imgsensor_info.max_frame_length;
		if (shutters[0] < imgsensor_info.min_shutter)
			shutters[0] = imgsensor_info.min_shutter;

		// Framelength should be an even number
		shutters[0] = (shutters[0] >> 1) << 1;
		ctx->frame_length = (ctx->frame_length >> 1) << 1;
		ctx->min_frame_length = (ctx->min_frame_length >> 1) << 1;

		for (i = 0; i < previous_exp_cnt; i++) {
			previous_exp[i] = 0;
			previous_fll[i] = 0;
		}
		previous_exp[0] = shutters[0];
		previous_fll[0] = ctx->frame_length;
		previous_exp_cnt = 1;

		write_cmos_sensor_8(ctx, 0x0104, 0x01);

#ifdef LONG_EXP
		longexposure_times = 0;
		while (shutters[0] >= 65535) {
			shutters[0] = shutters[0] / 2;
			longexposure_times += 1;
		}
		if (longexposure_times > 0) {
			IMX707_LOG_INF("enter long exposure mode, time is %d",
				longexposure_times);
			long_exposure_status = 1;
			ctx->frame_length = ctx->min_frame_length;
			write_cmos_sensor_8(ctx, 0x3100, longexposure_times & 0x07);
		} else if (long_exposure_status == 1) {
			long_exposure_status = 0;
			write_cmos_sensor_8(ctx, 0x3100, longexposure_times & 0x00);
			IMX707_LOG_INF("exit long exposure mode");
		}
#endif

		write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
		write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
		write_cmos_sensor_8(ctx, 0x0202, (shutters[0] >> 8) & 0xFF);
		write_cmos_sensor_8(ctx, 0x0203, shutters[0]  & 0xFF);

		IMX707_LOG_INF("shutter =%d, framelength =%d\n",
			shutters[0], ctx->frame_length);
	}
}

/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(struct subdrv_ctx *ctx, kal_uint16 shutter,
				     kal_uint16 frame_length,
				     kal_bool auto_extend_en)
{
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	ctx->shutter = shutter;

	/*0x3500, 0x3501, 0x3502 will increase VBLANK to
	 *get exposure larger than frame exposure
	 *AE doesn't update sensor gain at capture mode,
	 *thus extra exposure lines must be updated here.
	 */

	/* OV Recommend Solution */
	/*if shutter bigger than frame_length,
	 *should extend frame length first
	 */
	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;

	ctx->frame_length = ctx->frame_length + dummy_line;

	if (shutter > ctx->frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10 /
				ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
	}

	/* Update Shutter */
	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
	write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
	write_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);

	IMX707_LOG_INF(
		"Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		shutter, ctx->frame_length, frame_length,
		dummy_line, read_cmos_sensor_8(ctx, 0x0350));

}	/* set_shutter_frame_length */

#define BASEGAIN_x_1024 1024 * BASEGAIN
static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
	kal_uint16 reg_gain = 0x0;
	kal_uint16 val = 0x0;

	if ((BASEGAIN_x_1024 % gain) > (gain >> 1)) {
		val = BASEGAIN_x_1024 / gain + 1;
	} else {
		val = BASEGAIN_x_1024 / gain;
	}
	reg_gain = 1024 - val;
	return (kal_uint16) reg_gain;
}

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
static kal_uint32 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain, bool hold_en)
{
	kal_uint16 reg_gain, min_gain;
	kal_uint32 max_gain;

	max_gain = imgsensor_info.max_gain;//setuphere for mode use
	min_gain = imgsensor_info.min_gain;//setuphere for mode use

	if (ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM4 ||//16x for full mode
			ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM5) {
		/* 8K6K */
		max_gain = 16 * BASEGAIN;
	}

	if (gain < min_gain || gain > max_gain) {
		IMX707_LOG_INF("Error max gain setting: %d Should between %d & %d\n",
			gain, min_gain, max_gain);
		if (gain < min_gain)
			gain = min_gain;
		else if (gain > max_gain)
			gain = max_gain;
	}

	if (ctx->fast_mode_on) {
		IMX707_LOG_INF("skip : fast_mode_on(%d)", ctx->fast_mode_on);
		return 0;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;
	IMX707_LOG_INF("gain = %d, reg_gain = 0x%x, max_gain:%d\n ",
		gain, reg_gain, max_gain);

	write_cmos_sensor_8(ctx, 0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0205, reg_gain & 0xFF);

	if (hold_en) {
		write_cmos_sensor_8(ctx, 0x0104, 0x00);
	}


	return gain;
} /* set_gain */


static kal_uint16 aristotleimx707_feedback_awbgain[] = {
	0x0b8e, 0x00,
	0x0b8f, 0x00,
	0x0b90, 0x00,
	0x0b91, 0x00,
	0x0b92, 0x00,
	0x0b93, 0x00,
	0x0b94, 0x00,
	0x0b95, 0x00,
};

static kal_uint32 aristotleimx707_awb_gain(struct subdrv_ctx *ctx,
		struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	UINT32 r_gain_int = 0;
	UINT32 b_gain_int = 0;
	UINT32 gr_gain_int = 0;
	UINT32 gb_gain_int = 0;

	IMX707_LOG_INF("Enter %s, r = %d, gr = %d, gb = %d, b = %d\n",
		__FUNCTION__,
		pSetSensorAWB->ABS_GAIN_R,
		pSetSensorAWB->ABS_GAIN_GR,
		pSetSensorAWB->ABS_GAIN_GB,
		pSetSensorAWB->ABS_GAIN_B);

	if (pSetSensorAWB->ABS_GAIN_R == 0 ||
		pSetSensorAWB->ABS_GAIN_B == 0 ||
		pSetSensorAWB->ABS_GAIN_GR == 0 ||
		pSetSensorAWB->ABS_GAIN_GB == 0)
		return ERROR_NONE;

	r_gain_int = pSetSensorAWB->ABS_GAIN_R / 2;
	b_gain_int = pSetSensorAWB->ABS_GAIN_B / 2;
	gr_gain_int = pSetSensorAWB->ABS_GAIN_GR / 2;
	gb_gain_int = pSetSensorAWB->ABS_GAIN_GB / 2;

	aristotleimx707_feedback_awbgain[1] = (gr_gain_int & 0xFF00) >> 8;
	aristotleimx707_feedback_awbgain[3] = gr_gain_int & 0xFF;
	aristotleimx707_feedback_awbgain[5] = (r_gain_int & 0xFF00) >> 8;
	aristotleimx707_feedback_awbgain[7] = r_gain_int & 0xFF;
	aristotleimx707_feedback_awbgain[9] = (b_gain_int & 0xFF00) >> 8;
	aristotleimx707_feedback_awbgain[11] = b_gain_int & 0xFF;
	aristotleimx707_feedback_awbgain[13] = (gb_gain_int & 0xFF00) >> 8;
	aristotleimx707_feedback_awbgain[15] = gb_gain_int & 0xFF;

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_feedback_awbgain,
		sizeof(aristotleimx707_feedback_awbgain)/sizeof(kal_uint16));

	return ERROR_NONE;
}

static void aristotleimx707_set_lsc_reg_setting(struct subdrv_ctx *ctx,
		kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{
	int i;
	int startAddr[4] = {0x9D88, 0x9CB0, 0x9BD8, 0x9B00};
	/*0:B,1:Gb,2:Gr,3:R*/

	IMX707_LOG_INF("E! index:%d, regNum:%d\n", index, regNum);

	write_cmos_sensor_8(ctx, 0x0B00, 0x01); /*lsc enable*/
	write_cmos_sensor_8(ctx, 0x9014, 0x01);
	write_cmos_sensor_8(ctx, 0x4439, 0x01);
	mdelay(1);
	IMX707_LOG_INF("Addr 0xB870, 0x380D Value:0x%x %x\n",
		read_cmos_sensor_8(ctx, 0xB870),
		read_cmos_sensor_8(ctx, 0x380D));
	/*define Knot point, 2'b01:u3.7*/
	write_cmos_sensor_8(ctx, 0x9750, 0x01);
	write_cmos_sensor_8(ctx, 0x9751, 0x01);
	write_cmos_sensor_8(ctx, 0x9752, 0x01);
	write_cmos_sensor_8(ctx, 0x9753, 0x01);

	for (i = 0; i < regNum; i++)
		write_cmos_sensor(ctx, startAddr[index] + 2*i, regDa[i]);

	write_cmos_sensor_8(ctx, 0x0B00, 0x00); /*lsc disable*/
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
static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	IMX707_LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable) {
		write_cmos_sensor_8(ctx, 0x3010, 0x00);/*Mode transition mode change*/
		write_cmos_sensor_8(ctx, 0x0100, 0X01);
		ctx->is_streaming = true;
	} else {
		write_cmos_sensor_8(ctx, 0x0100, 0x00);
		ctx->is_streaming = false;
	}
	return ERROR_NONE;
}
static void sensor_init(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("[%s] start\n", __func__);

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_init_setting,
		sizeof(aristotleimx707_init_setting)/sizeof(kal_uint16));

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_Image_quality_setting,
                sizeof(aristotleimx707_Image_quality_setting)/sizeof(kal_uint16));

	write_sensor_QSC(ctx);

	if (imgsensor_info.temperature_support) {
		write_cmos_sensor_8(ctx, 0x0138, 0x01); // temperature enable
		IMX707_LOG_INF("temperature enable\n");
	}

	set_mirror_flip(ctx, ctx->mirror);
	IMX707_LOG_INF("[%s] End\n", __func__);
}	/*	  sensor_init  */

static void preview_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("%s +\n", __func__);
	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_preview_setting,
		sizeof(aristotleimx707_preview_setting)/sizeof(kal_uint16));

	IMX707_LOG_INF("%s -\n", __func__);
} /* preview_setting */


static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	IMX707_LOG_INF("%s +\n", __func__);
	preview_setting(ctx);
	IMX707_LOG_INF("%s -\n", __func__);

}

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	IMX707_LOG_INF("%s +\n", __func__);

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_normal_video_setting,
		sizeof(aristotleimx707_normal_video_setting)/sizeof(kal_uint16));

	IMX707_LOG_INF("%s -\n", __func__);

}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("%s +\n", __func__);

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_hs_video_setting,
		sizeof(aristotleimx707_hs_video_setting)/sizeof(kal_uint16));

	IMX707_LOG_INF("%s -\n", __func__);

}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("%s +\n", __func__);

	preview_setting(ctx);

	IMX707_LOG_INF("%s -\n", __func__);

}

static void custom1_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("%s +\n", __func__);

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_custom1_setting,
		sizeof(aristotleimx707_custom1_setting)/sizeof(kal_uint16));

	IMX707_LOG_INF("%s -\n", __func__);

}

static void custom2_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("%s +\n", __func__);

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_custom2_setting,
		sizeof(aristotleimx707_custom2_setting)/sizeof(kal_uint16));

	IMX707_LOG_INF("%s -\n", __func__);

}

static void custom3_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("%s +\n", __func__);

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_custom3_setting,
		sizeof(aristotleimx707_custom3_setting)/sizeof(kal_uint16));

	IMX707_LOG_INF("%s -\n", __func__);

}

static void custom4_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("%s +\n", __func__);

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_custom4_setting,
		sizeof(aristotleimx707_custom4_setting)/sizeof(kal_uint16));

	IMX707_LOG_INF("%s -\n", __func__);

}

static void custom5_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("%s +\n", __func__);

	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_custom5_setting,
		sizeof(aristotleimx707_custom5_setting)/sizeof(kal_uint16));

	IMX707_LOG_INF("%s -\n", __func__);

}

static void custom6_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("%s +\n", __func__);

	preview_setting(ctx);

	IMX707_LOG_INF("%s -\n", __func__);

}

static void custom7_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("+\n");
	normal_video_setting(ctx,ctx->current_fps);
	IMX707_LOG_INF("-\n");
}

static void custom8_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("+\n");
	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_custom8_setting,
		sizeof(aristotleimx707_custom8_setting)/sizeof(kal_uint16));
	IMX707_LOG_INF("-\n");
}

static void custom9_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("+\n");
	preview_setting(ctx);
	IMX707_LOG_INF("-\n");
}

static void custom10_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("+\n");
	aristotleimx707_table_write_cmos_sensor(ctx, aristotleimx707_custom10_setting,
		sizeof(aristotleimx707_custom10_setting)/sizeof(kal_uint16));
	IMX707_LOG_INF("-\n");
}

static void custom11_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("+\n");
	preview_setting(ctx);
	IMX707_LOG_INF("-\n");
}

static void custom12_setting(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("+\n");
	custom8_setting(ctx);
	IMX707_LOG_INF("-\n");
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
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	/*sensor have two i2c address 0x34 & 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id = ((read_cmos_sensor_8(ctx, 0x0016) << 8)
					| read_cmos_sensor_8(ctx, 0x0017));
			IMX707_LOG_INF(
				"read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
				read_cmos_sensor_8(ctx, 0x0016),
				read_cmos_sensor_8(ctx, 0x0017),
				read_cmos_sensor(ctx, 0x0000));
			if (*sensor_id == imgsensor_info.sensor_id) {
				IMX707_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			IMX707_LOG_INF("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
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
static int open(struct subdrv_ctx *ctx)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	IMX707_LOG_INF("%s +\n", __func__);
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = ((read_cmos_sensor_8(ctx, 0x0016) << 8)
					| read_cmos_sensor_8(ctx, 0x0017));
			if (sensor_id == imgsensor_info.sensor_id) {
				IMX707_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			IMX707_LOG_INF("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
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
	sensor_init(ctx);
	last_sensor_awb.ABS_GAIN_GR = 0;
	last_sensor_awb.ABS_GAIN_R  = 0;
	last_sensor_awb.ABS_GAIN_B  = 0;
	last_sensor_awb.ABS_GAIN_GB = 0;


	ctx->autoflicker_en = KAL_FALSE;
	ctx->sensor_mode = IMGSENSOR_MODE_INIT;
	ctx->shutter = 0x3D0;
	ctx->gain = 0x100;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->ihdr_mode = 0;
	ctx->test_pattern = KAL_FALSE;
	ctx->current_fps = imgsensor_info.pre.max_framerate;
	IMX707_LOG_INF("%s -\n", __func__);

	return ERROR_NONE;
} /* open */

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
static int close(struct subdrv_ctx *ctx)
{
	IMX707_LOG_INF("E\n");
	/* No Need to implement this function */
	streaming_control(ctx, KAL_FALSE);
	return ERROR_NONE;
} /* close */


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
static kal_uint32 preview(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	preview_setting(ctx);

	return ERROR_NONE;
} /* preview */

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
static kal_uint32 capture(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("E\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		IMX707_LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			ctx->current_fps,
			imgsensor_info.cap.max_framerate / 10);
	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	capture_setting(ctx, ctx->current_fps);

	return ERROR_NONE;
}	/* capture(ctx) */
static kal_uint32 normal_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	normal_video_setting(ctx, ctx->current_fps);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	/*ctx->video_mode = KAL_TRUE;*/
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/*ctx->current_fps = 300;*/
	ctx->autoflicker_en = KAL_FALSE;
	hs_video_setting(ctx);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("%s. 720P@240FPS\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	/*ctx->video_mode = KAL_TRUE;*/
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/*ctx->current_fps = 300;*/
	ctx->autoflicker_en = KAL_FALSE;
	slim_video_setting(ctx);

	return ERROR_NONE;
}	/* slim_video */


static kal_uint32 custom1(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom1_setting(ctx);

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	ctx->pclk = imgsensor_info.custom2.pclk;
	ctx->line_length = imgsensor_info.custom2.linelength;
	ctx->frame_length = imgsensor_info.custom2.framelength;
	ctx->min_frame_length = imgsensor_info.custom2.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom2_setting(ctx);

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	ctx->pclk = imgsensor_info.custom3.pclk;
	ctx->line_length = imgsensor_info.custom3.linelength;
	ctx->frame_length = imgsensor_info.custom3.framelength;
	ctx->min_frame_length = imgsensor_info.custom3.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom3_setting(ctx);

	return ERROR_NONE;
}	/* custom3 */

static kal_uint32 custom4(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	ctx->pclk = imgsensor_info.custom4.pclk;
	ctx->line_length = imgsensor_info.custom4.linelength;
	ctx->frame_length = imgsensor_info.custom4.framelength;
	ctx->min_frame_length = imgsensor_info.custom4.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom4_setting(ctx);

	if (last_sensor_awb.ABS_GAIN_GR != 0 &&
	    last_sensor_awb.ABS_GAIN_GB != 0 &&
	    last_sensor_awb.ABS_GAIN_R  != 0 &&
	    last_sensor_awb.ABS_GAIN_B  != 0) {
		IMX707_LOG_INF("custom4 write awb gain r:g:b %d:%d:%d \n",
						last_sensor_awb.ABS_GAIN_R,
						last_sensor_awb.ABS_GAIN_GR,
						last_sensor_awb.ABS_GAIN_B);
		aristotleimx707_awb_gain(ctx, &last_sensor_awb);
	}
	return ERROR_NONE;
}	/* custom4 */

static kal_uint32 custom5(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	ctx->pclk = imgsensor_info.custom5.pclk;
	ctx->line_length = imgsensor_info.custom5.linelength;
	ctx->frame_length = imgsensor_info.custom5.framelength;
	ctx->min_frame_length = imgsensor_info.custom5.framelength;
	ctx->autoflicker_en = KAL_FALSE;


	custom5_setting(ctx);

	return ERROR_NONE;
}	/* custom5 */


static kal_uint32 custom6(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	ctx->pclk = imgsensor_info.custom6.pclk;
	ctx->line_length = imgsensor_info.custom6.linelength;
	ctx->frame_length = imgsensor_info.custom6.framelength;
	ctx->min_frame_length = imgsensor_info.custom6.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom6_setting(ctx);

	return ERROR_NONE;
}	/* custom5 */

static kal_uint32 custom7(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM7;
	ctx->pclk = imgsensor_info.custom7.pclk;
	ctx->line_length = imgsensor_info.custom7.linelength;
	ctx->frame_length = imgsensor_info.custom7.framelength;
	ctx->min_frame_length = imgsensor_info.custom7.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom7_setting(ctx);

	return ERROR_NONE;
}

static kal_uint32 custom8(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM8;
	ctx->pclk = imgsensor_info.custom8.pclk;
	ctx->line_length = imgsensor_info.custom8.linelength;
	ctx->frame_length = imgsensor_info.custom8.framelength;
	ctx->min_frame_length = imgsensor_info.custom8.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom8_setting(ctx);

	return ERROR_NONE;
}

static kal_uint32 custom9(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM9;
	ctx->pclk = imgsensor_info.custom9.pclk;
	ctx->line_length = imgsensor_info.custom9.linelength;
	ctx->frame_length = imgsensor_info.custom9.framelength;
	ctx->min_frame_length = imgsensor_info.custom9.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom9_setting(ctx);

	return ERROR_NONE;
}

static kal_uint32 custom10(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM10;
	ctx->pclk = imgsensor_info.custom10.pclk;
	ctx->line_length = imgsensor_info.custom10.linelength;
	ctx->frame_length = imgsensor_info.custom10.framelength;
	ctx->min_frame_length = imgsensor_info.custom10.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom10_setting(ctx);

	return ERROR_NONE;
}

static kal_uint32 custom11(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM11;
	ctx->pclk = imgsensor_info.custom11.pclk;
	ctx->line_length = imgsensor_info.custom11.linelength;
	ctx->frame_length = imgsensor_info.custom11.framelength;
	ctx->min_frame_length = imgsensor_info.custom11.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom11_setting(ctx);

	return ERROR_NONE;
}

static kal_uint32 custom12(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM12;
	ctx->pclk = imgsensor_info.custom12.pclk;
	ctx->line_length = imgsensor_info.custom12.linelength;
	ctx->frame_length = imgsensor_info.custom12.framelength;
	ctx->min_frame_length = imgsensor_info.custom12.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom12_setting(ctx);

	return ERROR_NONE;
}

static int get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	int i = 0;

	for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (i < imgsensor_info.sensor_mode_num) {
			sensor_resolution->SensorWidth[i] = imgsensor_winsize_info[i].w2_tg_size;
			sensor_resolution->SensorHeight[i] = imgsensor_winsize_info[i].h2_tg_size;
		} else {
			sensor_resolution->SensorWidth[i] = 0;
			sensor_resolution->SensorHeight[i] = 0;
		}
	}

	return ERROR_NONE;
} /* get_resolution */

static int get_info(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_PREVIEW] =
		imgsensor_info.pre_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_CAPTURE] =
		imgsensor_info.cap_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_VIDEO] =
		imgsensor_info.video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO] =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_SLIM_VIDEO] =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM1] =
		imgsensor_info.custom1_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM2] =
		imgsensor_info.custom2_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM3] =
		imgsensor_info.custom3_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM4] =
		imgsensor_info.custom4_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM5] =
		imgsensor_info.custom5_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM6] =
		imgsensor_info.custom6_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM7] =
		imgsensor_info.custom7_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM8] =
		imgsensor_info.custom8_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM9] =
		imgsensor_info.custom9_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM10] =
		imgsensor_info.custom10_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM11] =
		imgsensor_info.custom11_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM12] =
		imgsensor_info.custom12_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV_DUALPD;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */


	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

	return ERROR_NONE;
}	/*	get_info  */


static int control(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX707_LOG_INF("scenario_id = %d\n", scenario_id);
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		capture(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		normal_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		hs_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		slim_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		custom1(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		custom2(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		custom3(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		custom4(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		custom5(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		custom6(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM7:
		custom7(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM8:
		custom8(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM9:
		custom9(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM10:
		custom10(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM11:
		custom11(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM12:
		custom12(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		preview(ctx, image_window, sensor_config_data);
		break;
	default:
		IMX707_LOG_INF("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control(ctx) */

static void wait_frame_done(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	unsigned long long timestamps_ns = ktime_get_boottime_ns();
	uint32_t frametime_ms = 0;
	uint32_t timestamps_diff_ms = 0;
	uint32_t frame_length = 0;
	int i;

	for (i = 0; i < previous_exp_cnt; i++) {
		frame_length += previous_fll[i];
	}

	IMX707_LOG_INF("+\n");
	if (timestamps_ns > last_sof_timestamps_ns) {
		timestamps_diff_ms = (timestamps_ns - last_sof_timestamps_ns) / 1000000;
		frametime_ms = 1000 * (unsigned long long)frame_length *
					(unsigned long long)ctx->line_length /
					(unsigned long long)ctx->pclk;
		IMX707_LOG_INF("timestamps_diff_ms : %lu\n", timestamps_diff_ms);
		IMX707_LOG_INF("frametime_ms : %d\n", frametime_ms);
		if (((2*frametime_ms) > timestamps_diff_ms) && (timestamps_diff_ms > 30)) {
			mdelay((2*frametime_ms) - timestamps_diff_ms + 5);
			IMX707_LOG_INF("mdelay : %d\n", ((2*frametime_ms) - timestamps_diff_ms + 5));
		}
	}
	IMX707_LOG_INF("-\n");
}

static kal_uint32 seamless_switch(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, uint32_t *ae_ctrl)
{
	uint32_t gain    = ae_ctrl[5];
	uint32_t shutter = ae_ctrl[0];

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		IMX707_LOG_INF("seamless switch to preview!\n");
		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.pre.pclk;
		ctx->line_length = imgsensor_info.pre.linelength;
		ctx->frame_length = imgsensor_info.pre.framelength;
		ctx->min_frame_length = imgsensor_info.pre.framelength;

		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x3010, 0x02); // Complete mode

		preview_setting(ctx);

		if (shutter != 0) {
			set_shutter(ctx, shutter, false);
		}

		if (gain != 0) {
			set_gain(ctx, gain, false);
		}

		write_cmos_sensor_8(ctx, 0x0104, 0x00);
		//mdelay(40);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		IMX707_LOG_INF("seamless switch to custom1 mode!\n");
		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.custom1.pclk;
		ctx->line_length = imgsensor_info.custom1.linelength;
		ctx->frame_length = imgsensor_info.custom1.framelength;
		ctx->min_frame_length = imgsensor_info.custom1.framelength;

		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x3010, 0x02); // Complete mode

		custom1_setting(ctx);

		if (shutter != 0) {
			set_shutter(ctx, shutter, false);
		}

		if (gain != 0) {
			set_gain(ctx, gain, false);
		}

		write_cmos_sensor_8(ctx, 0x0104, 0x00);
		//mdelay(40);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		IMX707_LOG_INF("seamless switch to fullsize mode!\n");
		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.custom4.pclk;
		ctx->line_length = imgsensor_info.custom4.linelength;
		ctx->frame_length = imgsensor_info.custom4.framelength;
		ctx->min_frame_length = imgsensor_info.custom4.framelength;

		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x3010, 0x02); // Complete mode

		custom4_setting(ctx);

		if (shutter != 0) {
			set_shutter(ctx, shutter, false);
		}

		if (gain != 0) {
			set_gain(ctx, gain, false);
		}

		write_cmos_sensor_8(ctx, 0x0104, 0x00);
		//mdelay(40);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		IMX707_LOG_INF("seamless switch to 2x mode!\n");
		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.custom5.pclk;
		ctx->line_length = imgsensor_info.custom5.linelength;
		ctx->frame_length = imgsensor_info.custom5.framelength;
		ctx->min_frame_length = imgsensor_info.custom5.framelength;

		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x3010, 0x02); // Complete mode

		custom5_setting(ctx);

		if (shutter != 0) {
			set_shutter(ctx, shutter, false);
		}

		if (gain != 0) {
			set_gain(ctx, gain, false);
		}

		write_cmos_sensor_8(ctx, 0x0104, 0x00);

		wait_frame_done(ctx, scenario_id);

		//mdelay(40);
		break;
	default:
		IMX707_LOG_INF(
		"error! wrong setting in set_seamless_switch = %d",
		scenario_id);
		return 0xff;
	}

	ctx->fast_mode_on = KAL_TRUE;
	IMX707_LOG_INF("%s success, scenario is switched to %d", __func__, scenario_id);
	return 0;
}

static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	IMX707_LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	if ((framerate == 300) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 296;
	else if ((framerate == 150) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 146;
	else
		ctx->current_fps = framerate;
	set_max_framerate_video(ctx, ctx->current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx,
		kal_bool enable, UINT16 framerate)
{
	if (enable) /*enable auto flicker*/ {
		//ctx->autoflicker_en = KAL_TRUE;
		IMX707_LOG_INF("enable! fps = %d", framerate);
	} else {
		 /*Cancel Auto flick*/
		ctx->autoflicker_en = KAL_FALSE;
	}

	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	IMX707_LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
		: 0;
		ctx->frame_length =
			imgsensor_info.normal_video.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		IMX707_LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
			, framerate, imgsensor_info.cap.max_framerate/10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;
			ctx->dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			  ? (frame_length - imgsensor_info.cap.framelength) : 0;
			ctx->frame_length =
				imgsensor_info.cap.framelength
				+ ctx->dummy_line;
			ctx->min_frame_length = ctx->frame_length;

		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			  ? (frame_length - imgsensor_info.hs_video.framelength)
			  : 0;
		ctx->frame_length =
			imgsensor_info.hs_video.framelength
				+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.slim_video.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
				/ imgsensor_info.custom1.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom1.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10
				/ imgsensor_info.custom2.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom2.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
				/ imgsensor_info.custom3.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
		? (frame_length - imgsensor_info.custom3.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom3.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10
				/ imgsensor_info.custom4.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom4.framelength)
		? (frame_length - imgsensor_info.custom4.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom4.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10
				/ imgsensor_info.custom5.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom5.framelength)
		? (frame_length - imgsensor_info.custom5.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom5.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		frame_length = imgsensor_info.custom6.pclk / framerate * 10
				/ imgsensor_info.custom6.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom6.framelength)
		? (frame_length - imgsensor_info.custom6.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom6.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM7:
		frame_length = imgsensor_info.custom7.pclk / framerate * 10
				/ imgsensor_info.custom7.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom7.framelength)
		? (frame_length - imgsensor_info.custom7.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom7.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM8:
		frame_length = imgsensor_info.custom8.pclk / framerate * 10
				/ imgsensor_info.custom8.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom8.framelength)
		? (frame_length - imgsensor_info.custom8.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom8.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM9:
		frame_length = imgsensor_info.custom9.pclk / framerate * 10
				/ imgsensor_info.custom9.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom9.framelength)
		? (frame_length - imgsensor_info.custom9.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom9.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM10:
		frame_length = imgsensor_info.custom10.pclk / framerate * 10
				/ imgsensor_info.custom10.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom10.framelength)
		? (frame_length - imgsensor_info.custom10.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom10.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM11:
		frame_length = imgsensor_info.custom11.pclk / framerate * 10
				/ imgsensor_info.custom11.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom11.framelength)
		? (frame_length - imgsensor_info.custom11.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom11.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM12:
		frame_length = imgsensor_info.custom12.pclk / framerate * 10
				/ imgsensor_info.custom12.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom12.framelength)
		? (frame_length - imgsensor_info.custom12.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom12.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
				/ imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		IMX707_LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	IMX707_LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		*framerate = imgsensor_info.custom6.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM7:
		*framerate = imgsensor_info.custom7.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM8:
		*framerate = imgsensor_info.custom8.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM9:
		*framerate = imgsensor_info.custom9.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM10:
		*framerate = imgsensor_info.custom10.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM11:
		*framerate = imgsensor_info.custom11.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM12:
		*framerate = imgsensor_info.custom12.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_uint32 mode)
{
	IMX707_LOG_INF("mode: %d\n", mode);
	switch (mode) {
		case SENSOR_TEST_PATTERN_MODE_BLACK: // Black
			write_cmos_sensor_8(ctx, 0x0600, 0x00);
			write_cmos_sensor_8(ctx, 0x0601, 0x01);
			break;
		default:
			write_cmos_sensor_8(ctx, 0x0601, 0x00); /*No pattern*/
			break;
	}

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	UINT8 temperature;
	INT32 temperature_convert;

	temperature = read_cmos_sensor_8(ctx, 0x013a);

	if (temperature >= 0x0 && temperature <= 0x4F)
		temperature_convert = temperature;
	else if (temperature >= 0x50 && temperature <= 0x7F)
		temperature_convert = 80;
	else if (temperature >= 0x80 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (INT8) temperature;

	IMX707_LOG_INF("temp_c(%d), read_reg(%d)\n",
			temperature_convert, temperature);

	return temperature_convert;
}

static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	kal_uint16 sensor_output_cnt;

	sensor_output_cnt = read_cmos_sensor_8(ctx, 0x0005);
	last_sof_timestamps_ns = ktime_get_boottime_ns();

	if (ctx->fast_mode_on) {
		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x3010, 0x00);
		write_cmos_sensor_8(ctx, 0x0104, 0x00);
		ctx->fast_mode_on = false;
		IMX707_LOG_INF("disable seamless\n");
	}

	IMX707_LOG_INF("sensormode(%d) sof_cnt(%d) sensor_output_cnt(%d)\n",
		ctx->sensor_mode, sof_cnt, sensor_output_cnt);
	return 0;
}

static int feature_control(
		struct subdrv_ctx *ctx,
		MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para,
		UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	/* unsigned long long *feature_return_para
	 *  = (unsigned long long *) feature_para;
	 */
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB = NULL;
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*IMX707_LOG_INF("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM3:	
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				imgsensor_info.sensor_output_dataformat;
			break;
		}
	break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(aristotleimx707_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)aristotleimx707_ana_gain_table,
			sizeof(aristotleimx707_ana_gain_table));
		}
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM10:
			*(feature_data + 1) = imgsensor_info.min_gain;
			*(feature_data + 2) = 16 * BASEGAIN;
			break;
		default:
			*(feature_data + 1) = imgsensor_info.min_gain;
			*(feature_data + 2) = imgsensor_info.max_gain;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom7.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom8.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom9.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM10:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom10.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM11:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom11.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM12:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom12.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom4.framelength << 16)
				+ imgsensor_info.custom4.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom5.framelength << 16)
				+ imgsensor_info.custom5.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom6.framelength << 16)
				+ imgsensor_info.custom6.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom7.framelength << 16)
				+ imgsensor_info.custom7.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom8.framelength << 16)
				+ imgsensor_info.custom8.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom9.framelength << 16)
				+ imgsensor_info.custom9.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM10:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom10.framelength << 16)
				+ imgsensor_info.custom10.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM11:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom11.framelength << 16)
				+ imgsensor_info.custom11.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM12:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom12.framelength << 16)
				+ imgsensor_info.custom12.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = ctx->line_length;
		*feature_return_para_16 = ctx->frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = ctx->pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		 set_shutter(ctx, *feature_data, true);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		 /* night_mode((BOOL) *feature_data); */
		break;
	#ifdef VENDOR_EDIT
	case SENSOR_FEATURE_CHECK_MODULE_ID:
		*feature_return_para_32 = imgsensor_info.module_id;
		break;
	#endif
	case SENSOR_FEATURE_SET_GAIN:
		set_gain(ctx, (UINT32) * feature_data, true);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(ctx, sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(ctx, sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
		 * if EEPROM does not exist in camera module.
		 */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(ctx, feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(ctx, (BOOL)*feature_data_16,
				      *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature(ctx);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		 set_max_framerate_by_scenario(ctx,
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		 get_default_framerate_by_scenario(ctx,
				(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
				(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		IMX707_LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (UINT32)*feature_data_32);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		IMX707_LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
		ctx->current_fps = *feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		IMX707_LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
		ctx->ihdr_mode = *feature_data_32;
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[3],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[4],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[5],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[6],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[7],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[8],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[9],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[10],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM7:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[11],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM8:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[12],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[13],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM10:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[14],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM11:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[15],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM12:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[16],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[0],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		IMX707_LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
			sizeof(struct SET_PD_BLOCK_INFO_T));
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		IMX707_LOG_INF(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
		 *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1666500;
		 break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		IMX707_LOG_INF("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		aristotleimx707_get_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		IMX707_LOG_INF("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		aristotleimx707_set_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		IMX707_LOG_INF("PDAF mode :%d\n", *feature_data_16);
		ctx->pdaf_mode = *feature_data_16;
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		IMX707_LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(ctx, (UINT16) (*feature_data),
					(UINT16) (*(feature_data + 1)),
					(BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length(ctx) support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		IMX707_LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		IMX707_LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		IMX707_LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data, true);
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		default:
			*feature_return_para_32 = 1000; /*BINNING_AVERAGED*/
			break;
		}
		IMX707_LOG_INF("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;

		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM7:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom7.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM8:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom8.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom9.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM10:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom10.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM11:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom11.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM12:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom12.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
break;

	case SENSOR_FEATURE_GET_VC_INFO:
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM7:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM2:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[3],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[4],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[5],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM12:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[6],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM10:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[7],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
	break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		/* modify to separate 3hdr and remosaic */
		if ((ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM4 ) ||
			(ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM5 ) ||
			(ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM10 )) {
			aristotleimx707_awb_gain(ctx,
				(struct SET_SENSOR_AWB_GAIN *) feature_para);
		}
		if (feature_para) {
			last_sensor_awb = *((struct SET_SENSOR_AWB_GAIN *) feature_para);
		}
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		{
		kal_uint8 index =
			*(((kal_uint8 *)feature_para) + (*feature_para_len));

		aristotleimx707_set_lsc_reg_setting(ctx, index, feature_data_16,
					  (*feature_para_len)/sizeof(UINT16));
		}
		break;
	case XIAOMI_FEATURE_ENABLE_SEAMLESS_SWITCH:
		enable_seamless = *feature_data_32;
		IMX707_LOG_INF("enable seamless switch setting : %d", enable_seamless);
		break;
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
		if ((feature_data + 1) != NULL) {
			pAeCtrls =
			(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			IMX707_LOG_INF("warning! no ae_ctrl input");
		}

		if ((feature_data + 3) != NULL) {
			pSetSensorAWB =
			(struct SET_SENSOR_AWB_GAIN *)((uintptr_t)(*(feature_data + 3)));
		} else {
			IMX707_LOG_INF("warning! no awb gain input");
		}

		if (feature_data == NULL) {
			IMX707_LOG_INF("error! input scenario is null!");
			return ERROR_INVALID_SCENARIO_ID;
		}

		if (pSetSensorAWB != NULL) {
			aristotleimx707_awb_gain(ctx, pSetSensorAWB);
			IMX707_LOG_INF("update awb gain by seamless switch");
		}

		seamless_switch(ctx, (*feature_data), pAeCtrls);
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		if ((feature_data + 1) != NULL) {
			pScenarios =
				(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			IMX707_LOG_INF("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			*pScenarios = SENSOR_SCENARIO_ID_CUSTOM5;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*pScenarios = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*pScenarios = SENSOR_SCENARIO_ID_CUSTOM4;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*pScenarios = SENSOR_SCENARIO_ID_CUSTOM1;
			break;
		default:
			*pScenarios = 0xff;
			break;
		}
		IMX707_LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",
			*feature_data, *pScenarios);
		break;
	case SENSOR_FEATURE_SET_FRAMELENGTH:
		set_frame_length(ctx, (UINT16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME:
		set_multi_shutter_frame_length(ctx, (UINT32 *)(*feature_data),
					(UINT32) (*(feature_data + 1)),
					(UINT32) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_PRELOAD_EEPROM_DATA:
		/*get eeprom preloader data*/
		*feature_return_para_32 = ctx->is_read_preload_eeprom;
		*feature_para_len = 4;
		if (ctx->is_read_preload_eeprom != 1)
			read_sensor_Cali(ctx);
		break;
	case SENSOR_FEATURE_GET_CUST_PIXEL_RATE:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 592800000; // 312M * 2 * 0.95
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1037400000; // 546M * 2 * 0.95
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return ERROR_NONE;
} /* feature_control(ctx) */


#ifdef IMGSENSOR_VC_ROUTING
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust2[] = { //2048x1152
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1152,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 288,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 8192,
			.vsize = 6144,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2880,
			.vsize = 2160,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2880,
			.vsize = 540,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 7680,
			.vsize = 4320,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 7680,
			.vsize = 1080,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM7:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_vid);
		memcpy(fd->entry, frame_desc_vid, sizeof(frame_desc_vid));
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM2:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust2);
		memcpy(fd->entry, frame_desc_cust2, sizeof(frame_desc_cust2));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust3);
		memcpy(fd->entry, frame_desc_cust3, sizeof(frame_desc_cust3));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust4);
		memcpy(fd->entry, frame_desc_cust4, sizeof(frame_desc_cust4));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust5);
		memcpy(fd->entry, frame_desc_cust5, sizeof(frame_desc_cust5));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM12:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust8);
		memcpy(fd->entry, frame_desc_cust8, sizeof(frame_desc_cust8));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM10:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust10);
		memcpy(fd->entry, frame_desc_cust10, sizeof(frame_desc_cust10));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM6:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	default:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	}

	return 0;
}
#endif


static const struct subdrv_ctx defctx = {

	.ana_gain_def  = 4 * BASEGAIN,
	.ana_gain_max  = 64 * BASEGAIN,
	.ana_gain_min  = 1 * BASEGAIN,
	.ana_gain_step = 1,
	.exposure_def  = 0x3D0,
	/* support long exposure at most 128 times) */
	.exposure_max  = (0xFFFF * 128) - 96,
	.exposure_min  = 16,
	.exposure_step = 4,
	.frame_time_delay_frame = 3,
	.margin                 = 96,
	.max_frame_length       = 0xffff,
	.is_streaming           = KAL_FALSE,
	.mirror                 = IMAGE_NORMAL, /* mirrorflip information */
	.sensor_mode            = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter                = 0x3D0, /* current shutter */
	.gain                   = 4 * BASEGAIN, /* current gain */
	.dummy_pixel            = 0, /* current dummypixel */
	.dummy_line             = 0, /* current dummyline */
	.current_fps            = 300,
	.autoflicker_en         = KAL_FALSE,
	.test_pattern           = KAL_FALSE,
	.current_scenario_id    = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	.ihdr_mode              = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id           = 0x20, /* record current sensor's i2c write id */
};

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int get_temp(struct subdrv_ctx *ctx, int *temp)
{
	*temp = get_sensor_temperature(ctx) * 1000;
	return 0;
}

static int get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	csi_param->legacy_phy = 0;
	csi_param->not_fixed_trail_settle = 0;

	return 0;
}


static struct subdrv_ops ops = {
	.get_id          = get_imgsensor_id,
	.init_ctx        = init_ctx,
	.open            = open,
	.get_info        = get_info,
	.get_resolution  = get_resolution,
	.control         = control,
	.feature_control = feature_control,
	.close           = close,
	.get_csi_param   = get_csi_param,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc  = get_frame_desc,
#endif
	.get_temp        = get_temp,
	.vsync_notify    = vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST,   0,       0},
	{HW_ID_AFVDD, 3100000, 0},
	{HW_ID_AVDD,  1,       1}, // AVDD 2.9 gpio en
	{HW_ID_AVDD1, 1,       1}, // AVDD 1.8 gpio en
	{HW_ID_DVDD,  1,       1},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_MCLK,  24,      0},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 2},
	{HW_ID_RST,   1,       3},
};

const struct subdrv_entry aristotleimx707_mipi_raw_entry = {
	.name       = "aristotleimx707_mipi_raw",
	.id         = ARISTOTLEIMX707_SENSOR_ID,
	.pw_seq     = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops        = &ops,
};

