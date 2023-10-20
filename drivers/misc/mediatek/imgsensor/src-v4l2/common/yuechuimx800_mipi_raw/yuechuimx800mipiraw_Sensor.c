#define PFX "YUECHUIMX800_camera_sensor"
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

#include "yuechuimx800_ana_gain_table.h"
#include "yuechuimx800mipiraw_Sensor.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor_8(...)  subdrv_i2c_rd_u8(__VA_ARGS__)
#define read_cmos_sensor(...)    subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define write_cmos_sensor(...)   subdrv_i2c_wr_u16(__VA_ARGS__)
#define yuechuimx800_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)
#define yuechuimx800_burst_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8_burst(__VA_ARGS__)
#define LOG_TAG "[yuechuimx800]"
#define IMX800_LOG_INF(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)
#define IMX800_LOG_DBG(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)

#undef VENDOR_EDIT
#define USE_BURST_MODE 1
#define ByPass         0
#define LONG_EXP       1
#define ENABLE_PDAF    1

static kal_uint8 enable_seamless = 0;
static struct SET_SENSOR_AWB_GAIN last_sensor_awb;

static kal_uint32 previous_exp[3];
static kal_uint32 previous_fll[3];
static kal_uint16 previous_exp_cnt;

static unsigned long long last_sof_timestamps_ns = 0;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = YUECHUIMX800_SENSOR_ID,

	.checksum_value = 0xa4c32546,

	.pre = { /* preview */
		.pclk = 1932000000,
		.linelength = 9776,
		.framelength = 6584,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 300,
	},

	.cap = { /*capture*/
		.pclk = 1932000000,
		.linelength = 9776,
		.framelength = 6584,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 300,
	},

	.normal_video = { /*normal_video*/
		.pclk = 2184000000,
		.linelength = 9776,
		.framelength = 7444,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 300,
	},

	.hs_video = { /* SMVR 1920x1080 @240fps */
		.pclk = 1644000000,
		.linelength = 5520,
		.framelength = 1240,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1152,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 2400,
	},

	.slim_video = { /* 4:3 video */
		.pclk = 1932000000,
		.linelength = 9776,
		.framelength = 6584,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 300,
	},

	.custom1 = { /*sat*/
		.pclk = 1932000000,
		.linelength = 9776,
		.framelength = 6584,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 300,
	},

	.custom2 = { /*SMVR 1920x1080 @120fps*/
		.pclk = 1440000000,
		.linelength = 5520,
		.framelength = 2172,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1152,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 1200,
	},

	.custom3 = { /*60fps*/
		.pclk = 2184000000,
		.linelength = 9776,
		.framelength = 3720,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 600,
	},

	.custom4 = { /*fullsize*/
		.pclk = 1932000000,
		.linelength = 23072,
		.framelength = 6248,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 8192,
		.grabwindow_height = 6144,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 130,
	},

	.custom5 = { /*in sensor zoom*/
		.pclk = 1932000000,
		.linelength = 11536,
		.framelength = 5580,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 300,
	},

	.custom6 = { /* bokeh */
		.pclk = 1212000000,
		.linelength = 9776,
		.framelength = 5164,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 3072,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 240,
	},

	.custom7 = { /* super night video */
		.pclk = 2184000000,
		.linelength = 9776,
		.framelength = 9304,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 240,
	},

	.custom8 = { /* 2x bokeh */
		.pclk = 2196000000,
		.linelength = 9776,
		.framelength = 7484,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2048,
		.grabwindow_height = 1536,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 300,
	},

	.custom9 = { /* 2exp stagger hdr */
		.pclk = 2184000000,
		.linelength = 9776,
		.framelength = 7440,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4096,
		.grabwindow_height = 2304,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 300,
		/* See IMX800 SRM Table 11-4 Readout length of each DOL mode */
		/* (20d + (Y_ADD_END-Y_ADD_STA+1) / 4) x 2 */
		.readout_length = 2344 * 2,
		.read_margin = 24 * 2,
	},

	.custom10 = { /* 8K@30fps */
		.pclk = 1596000000,
		.linelength = 11536,
		.framelength = 4608,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 7680,
		.grabwindow_height = 4320,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 1714290000,
		.max_framerate = 300,
	},

	.margin           = 48,   /* sensor framelength & shutter margin */
	.min_shutter      = 16,   /* min shutter */
	.min_gain         = 1463, //1.428671085f  * BASEGAIN
	.max_gain         = 65281, //63.7509 * BASEGAIN
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
	.temperature_support        = 0, /* 1, support; 0,not support */

	.sensor_mode_num            = 15,/* support sensor mode num */

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
	{8192, 6144, 0,    768,  8192, 4608, 2048, 1152, 0,   0,   2048, 1152,  0,  0, 2048, 1152}, /* hs_video */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* slim video */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* custom1 */
	{8192, 6144, 0,    768,  8192, 4608, 2048, 1152, 0,   0,   2048, 1152,  0,  0, 2048, 1152}, /* custom2 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   384, 4096, 2304,  0,  0, 4096, 2304}, /* custom3 */
	{8192, 6144, 0,    0,    8192, 6144, 8192, 6144, 0,   0,   8192, 6144,  0,  0, 8192, 6144}, /* custom4 */
	{8192, 6144, 2048, 1536, 4096, 3072, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* custom5 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   0,   4096, 3072,  0,  0, 4096, 3072}, /* custom6 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   384, 4096, 2304,  0,  0, 4096, 2304}, /* custom7 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 1024,768, 2048, 1536,  0,  0, 2048, 1536}, /* custom8 */
	{8192, 6144, 0,    0,    8192, 6144, 4096, 3072, 0,   384, 4096, 2304,  0,  0, 4096, 2304}, /* custom9 */
	{8192, 6144, 256,912,    7680, 4320, 7680, 4320, 0,   0,   7680, 4320,  0,  0, 7680, 4320}, /* custom10 */
};

#if ENABLE_PDAF
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[] = {
	{/* preview/capture/slim_video/custom1/custom6 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4096, 3072, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 4096, 768, 0x00, 0x00, 0x0000, 0x0000
	},
	{/* normal video /custom7*/
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4096, 2304, 0x00, 0x00, 0x0000, 0x0000,
		0x02, 0x2b, 4096, 576,  0x00, 0x00, 0x0000, 0x0000
	},
	{/* custom3 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4096, 2304, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 4096, 576,  0x00, 0x00, 0x0000, 0x0000
	},
	{/* hs_video/custom2 */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 2048, 1152, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 2048, 288, 0x00, 0x00, 0x0000, 0x0000
	},
	{/* cust4,fullsize */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 8192, 6144, 0x00, 0x00, 0x0000, 0x0000,
	},
	{/* cust5, in sensor zoom */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 4096, 3072, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 2048, 1536, 0x00, 0x00, 0x0000, 0x0000
	},
	{/* cust8, 2x bokeh */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 2048, 1536, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 2048, 384, 0x00, 0x00, 0x0000, 0x0000
	},
	{/* cust9 */
		0x04, 0x0a, 0x00, 0x08, 0x40, 0x00, //custom4
		{
			{VC_STAGGER_NE, 0x00, 0x2b, 4096, 2304},
			{VC_STAGGER_ME, 0x01, 0x2b, 4096, 2304},
			{VC_PDAF_STATS_NE_PIX_1, 0x03, 0x2b, 4096, 576},
			{VC_PDAF_STATS_ME_PIX_1, 0x04, 0x2b, 4096, 576},
		},
		1
	},
	{/* cust10, 8k 30fps */
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 7680, 4320, 0x00, 0x00, 0x0000, 0x0000,
		0x01, 0x2b, 3840, 2160, 0x00, 0x00, 0x0000, 0x0000
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
		{0, 0}, {0, 0}, {0, 384}, {0, 192}, {0, 0 },
		{0, 0}, {0, 192}, {0, 384}, {0,  0 }, {2048, 1536},
		{0, 0}, {0, 384}, {1024, 958}, {0, 0}, {256,912}
	},
	.iMirrorFlip = IMAGE_NORMAL,
};

static void yuechuimx800_get_pdaf_reg_setting(struct subdrv_ctx *ctx, MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(ctx, regDa[idx]);
		IMX800_LOG_INF("%x %x", regDa[idx], regDa[idx+1]);
	}
}
static void yuechuimx800_set_pdaf_reg_setting(struct subdrv_ctx *ctx,
		MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor_8(ctx, regDa[idx], regDa[idx + 1]);
		IMX800_LOG_INF("%x %x", regDa[idx], regDa[idx+1]);
	}
}
#endif
#define EEPROM_QSC_SIZE 3456
static kal_uint16 yuechuimx800_QSC_setting[EEPROM_QSC_SIZE * 2];
static kal_uint16 yuechuimx800_LRC_setting[432 * 2];

static kal_uint16 read_cmos_eeprom_8(struct subdrv_ctx *ctx, kal_uint16 addr)
{
	u8 val;

	adaptor_i2c_rd_u8(ctx->i2c_client, 0xA2 >> 1, addr, &val);

	return (u16)val;
}

static void write_sensor_QSC(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("Entet %s +++\n", __FUNCTION__);

	yuechuimx800_burst_write_cmos_sensor(ctx, yuechuimx800_QSC_setting,
		sizeof(yuechuimx800_QSC_setting) / sizeof(kal_uint16));

	IMX800_LOG_INF("Entet %s ---\n", __FUNCTION__);
}

static void write_sensor_LRC(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("Entet %s +++\n", __FUNCTION__);

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_LRC_setting,
		sizeof(yuechuimx800_LRC_setting) / sizeof(kal_uint16));

	IMX800_LOG_INF("Entet %s ---\n", __FUNCTION__);
}

static void read_sensor_Cali(struct subdrv_ctx *ctx)
{
	kal_uint16 idx          = 0;
	kal_uint16 eeprom_qsc   = 0x1DC0, sensor_qsc   = 0x1000;
	kal_uint16 eeprom_lrc_0 = 0x1BBB, sensor_lrc_0 = 0x7E00;
	kal_uint16 eeprom_lrc_1 = 0x1C92, sensor_lrc_1 = 0x7F00;

	IMX800_LOG_INF("Entet %s +++\n", __FUNCTION__);

	for (idx = 0; idx < EEPROM_QSC_SIZE; idx++) {
		yuechuimx800_QSC_setting[2 * idx]     = sensor_qsc + idx;
		yuechuimx800_QSC_setting[2 * idx + 1] = read_cmos_eeprom_8(ctx, eeprom_qsc + idx);
	}

	for (idx = 0; idx < 216; idx++) {
		yuechuimx800_LRC_setting[2 * idx]               = sensor_lrc_0 + idx;
		yuechuimx800_LRC_setting[2 * idx + 1]           = read_cmos_eeprom_8(ctx, eeprom_lrc_0 + idx);
		yuechuimx800_LRC_setting[2 * idx + 192 * 2]     = sensor_lrc_1 + idx;
		yuechuimx800_LRC_setting[2 * idx + 1 + 192 * 2] = read_cmos_eeprom_8(ctx, eeprom_lrc_1 + idx);
	}

	ctx->is_read_preload_eeprom = 1;

	IMX800_LOG_INF("Entet %s ---\n", __FUNCTION__);
}

static void set_dummy(struct subdrv_ctx *ctx)
{
	IMX800_LOG_DBG("dummyline = %d, dummypixels = %d\n",
		ctx->dummy_line, ctx->dummy_pixel);
}	/*	set_dummy  */

static void set_mirror_flip(struct subdrv_ctx *ctx, kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	IMX800_LOG_INF("image_mirror = %d\n", image_mirror);
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

	IMX800_LOG_INF(
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


static kal_uint32 get_cur_exp_cnt(struct subdrv_ctx *ctx)
{
	kal_uint32 exp_cnt = 1;

	if (ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM9) {
		exp_cnt = 2;
	}
#if 0
	if (0x1 == (read_cmos_sensor_8(ctx, 0x3170) & 0x1)) { // DOL_EN
		if (0x1 == (read_cmos_sensor_8(ctx, 0x3171) & 0x3)) { // DOL_MODE
			exp_cnt = 3;
		} else {
			exp_cnt = 2;
		}
	}
#endif
	return exp_cnt;
}

static void write_frame_len(struct subdrv_ctx *ctx, kal_uint32 fll)
{
	// write_frame_len should be called inside GRP_PARAM_HOLD (0x0104)
	// FRM_LENGTH_LINES must be multiple of 4
	kal_uint32 exp_cnt = get_cur_exp_cnt(ctx);

	ctx->frame_length = round_up(fll / exp_cnt, 4) * exp_cnt;

	if (ctx->extend_frame_length_en == KAL_FALSE) {
		IMX800_LOG_INF("fll %d exp_cnt %d\n", ctx->frame_length, exp_cnt);
		write_cmos_sensor_8(ctx, 0x0340, (ctx->frame_length / exp_cnt) >> 8);
		write_cmos_sensor_8(ctx, 0x0341, (ctx->frame_length / exp_cnt) & 0xFF);
	}
}

static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter, bool hold_en)
{
	kal_uint16 realtime_fps = 0;
	int i;

#ifdef LONG_EXP
	int longexposure_times = 0;
	static int long_exposure_status;
#endif

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

	/* restore current shutter value */
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

	if (hold_en) {
		write_cmos_sensor_8(ctx, 0x0104, 0x01);
	}

#ifdef LONG_EXP
	while (shutter >= 65535 - 48) {
		shutter = shutter / 2;
		longexposure_times += 1;
	}
	if (longexposure_times > 0) {
		IMX800_LOG_INF("enter long exposure mode, time is %d",
			longexposure_times);
		long_exposure_status = 1;
		ctx->frame_length = shutter + 48;
		write_cmos_sensor_8(ctx, 0x3150, longexposure_times & 0x07);
	} else if (long_exposure_status == 1) {
		long_exposure_status = 0;
		write_cmos_sensor_8(ctx, 0x3150, 0x00);
		IMX800_LOG_INF("exit long exposure mode");
	}
#endif

	// Update Shutter
	// 0x0350 = 1, Set the minimum FLL here and let the sensor calculate the required FLL
	write_cmos_sensor_8(ctx, 0x0350, 0x01);
	write_cmos_sensor_8(ctx, 0x0340, (ctx->min_frame_length >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0341, ctx->min_frame_length & 0xFF);
	write_cmos_sensor_8(ctx, 0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, shutter  & 0xFF);

	IMX800_LOG_DBG("shutter =%d, framelength =%d, min_frame_length=%d\n",
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

	IMX800_LOG_INF("Framelength: set=%d/input=%d/min=%d, auto_extend=%d\n",
		ctx->frame_length, frame_length, ctx->min_frame_length,
		read_cmos_sensor_8(ctx, 0x0350));
}


static void set_multi_shutter_frame_length(struct subdrv_ctx *ctx,
				kal_uint16 *shutters, kal_uint16 shutter_cnt,
				kal_uint16 frame_length)
{
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

		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x0340, ctx->frame_length >> 8);
		write_cmos_sensor_8(ctx, 0x0341, ctx->frame_length & 0xFF);
		write_cmos_sensor_8(ctx, 0x0202, (shutters[0] >> 8) & 0xFF);
		write_cmos_sensor_8(ctx, 0x0203, shutters[0]  & 0xFF);

		IMX800_LOG_DBG("shutter =%d, framelength =%d\n",
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

	IMX800_LOG_INF(
		"Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		shutter, ctx->frame_length, frame_length,
		dummy_line, read_cmos_sensor_8(ctx, 0x0350));

}	/* set_shutter_frame_length */

#define BASEGAIN_x_16384 16384 * BASEGAIN
static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
	kal_uint16 reg_gain = 0x0;
	kal_uint16 val = 0x0;

	if ((BASEGAIN_x_16384 % gain) > (gain >> 1)) {
		val = BASEGAIN_x_16384 / gain + 1;
	} else {
		val = BASEGAIN_x_16384 / gain;
	}
	reg_gain = 16384 - val;
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
		max_gain = 16 * BASEGAIN;
	}

	if (gain < min_gain || gain > max_gain) {
		IMX800_LOG_INF("Error max gain setting: %d Should between %d & %d\n",
			gain, min_gain, max_gain);
		if (gain < min_gain)
			gain = min_gain;
		else if (gain > max_gain)
			gain = max_gain;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;
	IMX800_LOG_DBG("gain = %d, reg_gain = 0x%x, max_gain:%d\n ",
		gain, reg_gain, max_gain);

	write_cmos_sensor_8(ctx, 0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0205, reg_gain & 0xFF);

	if (hold_en) {
		write_cmos_sensor_8(ctx, 0x0104, 0x00);
	}

	return gain;
} /* set_gain */


static void hdr_write_tri_shutter_w_gph(struct subdrv_ctx *ctx,
		kal_uint32 le, kal_uint32 me, kal_uint32 se, kal_bool gph)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 exposure_cnt = 0;
	kal_uint32 fineIntegTime = 0;//fine_integ_line_table[ctx->current_scenario_id];
	int i;

	le = FINE_INTEG_CONVERT(le, fineIntegTime);
	me = FINE_INTEG_CONVERT(me, fineIntegTime);
	se = FINE_INTEG_CONVERT(se, fineIntegTime);

	exposure_cnt = get_cur_exp_cnt(ctx);

	if (le) {
		le = (kal_uint16)max(imgsensor_info.min_shutter, (kal_uint32)le);
		le = round_up((le) / exposure_cnt, 4) * exposure_cnt;
	}
	if (me) {
		me = (kal_uint16)max(imgsensor_info.min_shutter, (kal_uint32)me);
		me = round_up((me) / exposure_cnt, 4) * exposure_cnt;
	}
	if (se) {
		se = (kal_uint16)max(imgsensor_info.min_shutter, (kal_uint32)se);
		se = round_up((se) / exposure_cnt, 4) * exposure_cnt;
	}

	ctx->frame_length =
		max((kal_uint32)(le + me + se + imgsensor_info.margin*exposure_cnt),
		ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, imgsensor_info.max_frame_length);

	for (i = 0; i < previous_exp_cnt; i++)
		previous_exp[i] = 0;
	previous_exp[0] = le;
	switch (exposure_cnt) {
	case 3:
		previous_exp[1] = me;
		previous_exp[2] = se;
		break;
	case 2:
		previous_exp[1] = se;
		previous_exp[2] = 0;
		break;
	case 1:
	default:
		previous_exp[1] = 0;
		previous_exp[2] = 0;
		break;
	}
	previous_exp_cnt = exposure_cnt;

	//see IMX800 SRM 11.6.3 Exposure time calculation formula of DOL mode
	if (le)
		le = le / exposure_cnt;
	if (me)
		me = me / exposure_cnt;
	if (se)
		se = se / exposure_cnt;

	if (ctx->autoflicker_en) {
		realtime_fps =
			ctx->pclk / ctx->line_length * 10 /
			ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
	}

	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x01);

	/* Long exposure */
	write_cmos_sensor_8(ctx, 0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0203, le & 0xFF);

	/* Muddle exposure */
	if (me) {
		/* Valid only in 3exp DOL and QDOL4 */
		write_cmos_sensor_8(ctx, 0x3162, (me >> 8) & 0xFF);
		write_cmos_sensor_8(ctx, 0x3163, me & 0xFF);
	}

	/* Short exposure */
	write_cmos_sensor_8(ctx, 0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0225, se & 0xFF);

	write_frame_len(ctx, ctx->frame_length);

	if (gph && !ctx->ae_ctrl_gph_en) {
		write_cmos_sensor_8(ctx, 0x0104, 0x00);
	}

	IMX800_LOG_DBG("X! exposure_cnt %d le:0x%x, me:0x%x, se:0x%x autoflicker_en %d frame_length %d ae_ctrl_gph_en %d\n",
		exposure_cnt, le, me, se, ctx->autoflicker_en, ctx->frame_length, ctx->ae_ctrl_gph_en);
}

static void hdr_write_tri_shutter(struct subdrv_ctx *ctx,
		kal_uint32 le, kal_uint32 me, kal_uint32 se)
{
	hdr_write_tri_shutter_w_gph(ctx, le, me, se, KAL_TRUE);
}


static void hdr_write_tri_gain_w_gph(struct subdrv_ctx *ctx,
		kal_uint32 lg, kal_uint32 mg, kal_uint32 sg, kal_bool gph)
{
	kal_uint16 reg_lg, reg_mg, reg_sg;

	reg_lg = gain2reg(ctx, lg);
	reg_mg = mg ? gain2reg(ctx, mg) : 0;
	reg_sg = gain2reg(ctx, sg);

	ctx->gain = reg_lg;
	if (gph && !ctx->ae_ctrl_gph_en)
		write_cmos_sensor_8(ctx, 0x0104, 0x01);
	/* Long Gian */
	write_cmos_sensor_8(ctx, 0x0204, (reg_lg>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0205, reg_lg & 0xFF);
	/* Middle Gian */
	if (mg != 0) {
		/* Valid only in 3exp DOL and QDOL4 */
		write_cmos_sensor_8(ctx, 0x3164, (reg_mg>>8) & 0xFF);
		write_cmos_sensor_8(ctx, 0x3165, reg_mg & 0xFF);
	}
	/* Short Gian */
	write_cmos_sensor_8(ctx, 0x0216, (reg_sg>>8) & 0xFF);
	write_cmos_sensor_8(ctx, 0x0217, reg_sg & 0xFF);
	if (gph)
		write_cmos_sensor_8(ctx, 0x0104, 0x00);

	IMX800_LOG_DBG(
		"lg:0x%x, reg_lg:0x%x, mg:0x%x, reg_mg:0x%x, sg:0x%x, reg_sg:0x%x, ae_ctrl_gph_en:%d\n",
		lg, reg_lg, mg, reg_mg, sg, reg_sg, ctx->ae_ctrl_gph_en);
}

static void hdr_write_tri_gain(struct subdrv_ctx *ctx,
		kal_uint32 lg, kal_uint32 mg, kal_uint32 sg)
{
	hdr_write_tri_gain_w_gph(ctx, lg, mg, sg, KAL_TRUE);
}

static kal_uint16 yuechuimx800_feedback_awbgain[] = {
	0x0B8E, 0x00,
	0x0B8F, 0x00,
	0x0B90, 0x00,
	0x0B91, 0x00,
	0x0B92, 0x00,
	0x0B93, 0x00,
	0x0B94, 0x00,
	0x0B95, 0x00,
};

static kal_uint32 yuechuimx800_awb_gain(struct subdrv_ctx *ctx,
		struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	UINT32 r_gain_int  = 0;
	UINT32 b_gain_int  = 0;
	UINT32 gr_gain_int = 0;
	UINT32 gb_gain_int = 0;

	//IMX800_LOG_INF("Enter %s, r = %d, gr = %d, gb = %d, b = %d\n", __FUNCTION__, pSetSensorAWB->ABS_GAIN_R, pSetSensorAWB->ABS_GAIN_GR ,pSetSensorAWB->ABS_GAIN_GB, pSetSensorAWB->ABS_GAIN_B);

	if (pSetSensorAWB->ABS_GAIN_R  == 0 ||
		pSetSensorAWB->ABS_GAIN_B  == 0 ||
		pSetSensorAWB->ABS_GAIN_GR == 0 ||
		pSetSensorAWB->ABS_GAIN_GB == 0)
		return ERROR_NONE;

	r_gain_int  = pSetSensorAWB->ABS_GAIN_R  / 2;
	b_gain_int  = pSetSensorAWB->ABS_GAIN_B  / 2;
	gr_gain_int = pSetSensorAWB->ABS_GAIN_GR / 2;
	gb_gain_int = pSetSensorAWB->ABS_GAIN_GB / 2;

	yuechuimx800_feedback_awbgain[1]  = (gr_gain_int & 0xFF00) >> 8;
	yuechuimx800_feedback_awbgain[3]  = gr_gain_int & 0xFF;
	yuechuimx800_feedback_awbgain[5]  = (r_gain_int & 0xFF00) >> 8;
	yuechuimx800_feedback_awbgain[7]  = r_gain_int & 0xFF;
	yuechuimx800_feedback_awbgain[9]  = (b_gain_int & 0xFF00) >> 8;
	yuechuimx800_feedback_awbgain[11] = b_gain_int & 0xFF;
	yuechuimx800_feedback_awbgain[13] = (gb_gain_int & 0xFF00) >> 8;
	yuechuimx800_feedback_awbgain[15] = gb_gain_int & 0xFF;

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_feedback_awbgain,
		sizeof(yuechuimx800_feedback_awbgain)/sizeof(kal_uint16));

	return ERROR_NONE;
}

static void yuechuimx800_set_lsc_reg_setting(struct subdrv_ctx *ctx,
		kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{
	int i;
	int startAddr[4] = {0x9D88, 0x9CB0, 0x9BD8, 0x9B00};
	/*0:B,1:Gb,2:Gr,3:R*/

	IMX800_LOG_INF("E! index:%d, regNum:%d\n", index, regNum);

	write_cmos_sensor_8(ctx, 0x0B00, 0x01); /*lsc enable*/
	write_cmos_sensor_8(ctx, 0x9014, 0x01);
	write_cmos_sensor_8(ctx, 0x4439, 0x01);
	mdelay(1);
	IMX800_LOG_INF("Addr 0xB870, 0x380D Value:0x%x %x\n",
		read_cmos_sensor_8(ctx, 0xB870),
		read_cmos_sensor_8(ctx, 0x380D));
	/*define Knot point, 2'b01:u3.7*/
	write_cmos_sensor_8(ctx, 0x1F52, 0x01);
	write_cmos_sensor_8(ctx, 0x1F53, 0x01);
	write_cmos_sensor_8(ctx, 0x1F54, 0x01);
	write_cmos_sensor_8(ctx, 0x1F55, 0x01);

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
	IMX800_LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
		enable);
	if (enable) {
		if (read_cmos_sensor_8(ctx, 0x0350) != 0x01) {
			IMX800_LOG_INF("single cam scenario enable auto-extend");
			write_cmos_sensor_8(ctx, 0x0350, 0x01);
		}
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
	IMX800_LOG_INF("[%s] start\n", __func__);

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_init_setting,
		sizeof(yuechuimx800_init_setting)/sizeof(kal_uint16));

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_Image_quality_setting,
                sizeof(yuechuimx800_Image_quality_setting)/sizeof(kal_uint16));

	write_sensor_LRC(ctx);
	write_sensor_QSC(ctx);

	if (imgsensor_info.temperature_support) {
		write_cmos_sensor_8(ctx, 0x0138, 0x01); // temperature enable
		IMX800_LOG_INF("temperature enable\n");
	}

	set_mirror_flip(ctx, ctx->mirror);
	IMX800_LOG_INF("[%s] End\n", __func__);
}	/*	  sensor_init  */

static void preview_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("%s +\n", __func__);
	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_preview_setting,
		sizeof(yuechuimx800_preview_setting)/sizeof(kal_uint16));

	IMX800_LOG_INF("%s -\n", __func__);
} /* preview_setting */


static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	IMX800_LOG_INF("%s +\n", __func__);
	preview_setting(ctx);
	IMX800_LOG_INF("%s -\n", __func__);

}

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	IMX800_LOG_INF("%s +\n", __func__);

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_normal_video_setting,
		sizeof(yuechuimx800_normal_video_setting)/sizeof(kal_uint16));

	IMX800_LOG_INF("%s -\n", __func__);

}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("%s +\n", __func__);

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_hs_video_setting,
		sizeof(yuechuimx800_hs_video_setting)/sizeof(kal_uint16));

	IMX800_LOG_INF("%s -\n", __func__);

}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("%s +\n", __func__);

	preview_setting(ctx);

	IMX800_LOG_INF("%s -\n", __func__);

}

static void custom1_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("%s +\n", __func__);

	preview_setting(ctx);

	IMX800_LOG_INF("%s -\n", __func__);

}

static void custom2_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("%s +\n", __func__);

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_custom2_setting,
		sizeof(yuechuimx800_custom2_setting)/sizeof(kal_uint16));

	IMX800_LOG_INF("%s -\n", __func__);

}

static void custom3_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("%s +\n", __func__);

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_custom3_setting,
		sizeof(yuechuimx800_custom3_setting)/sizeof(kal_uint16));

	IMX800_LOG_INF("%s -\n", __func__);

}

static void custom4_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("%s +\n", __func__);

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_custom4_setting,
		sizeof(yuechuimx800_custom4_setting)/sizeof(kal_uint16));

	IMX800_LOG_INF("%s -\n", __func__);

}

static void custom5_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("%s +\n", __func__);

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_custom5_setting,
		sizeof(yuechuimx800_custom5_setting)/sizeof(kal_uint16));

	IMX800_LOG_INF("%s -\n", __func__);

}

static void custom6_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("%s +\n", __func__);

	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_custom6_setting,
		sizeof(yuechuimx800_custom6_setting)/sizeof(kal_uint16));

	IMX800_LOG_INF("%s -\n", __func__);

}

static void custom7_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("+\n");
	normal_video_setting(ctx,ctx->current_fps);
	IMX800_LOG_INF("-\n");
}

static void custom8_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("+\n");
	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_custom8_setting,
		sizeof(yuechuimx800_custom8_setting)/sizeof(kal_uint16));
	IMX800_LOG_INF("-\n");
}

static void custom9_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("+\n");
	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_custom9_setting,
		sizeof(yuechuimx800_custom9_setting)/sizeof(kal_uint16));
	IMX800_LOG_INF("-\n");
}

static void custom10_setting(struct subdrv_ctx *ctx)
{
	IMX800_LOG_INF("+\n");
	yuechuimx800_table_write_cmos_sensor(ctx, yuechuimx800_custom10_setting,
		sizeof(yuechuimx800_custom10_setting)/sizeof(kal_uint16));
	IMX800_LOG_INF("-\n");
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
			IMX800_LOG_INF(
				"read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
				read_cmos_sensor_8(ctx, 0x0016),
				read_cmos_sensor_8(ctx, 0x0017),
				read_cmos_sensor(ctx, 0x0000));
			if (*sensor_id == imgsensor_info.sensor_id) {
				IMX800_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			IMX800_LOG_INF("Read sensor id fail, id: 0x%x\n",
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

	IMX800_LOG_INF("%s +\n", __func__);
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = ((read_cmos_sensor_8(ctx, 0x0016) << 8)
					| read_cmos_sensor_8(ctx, 0x0017));
			if (sensor_id == imgsensor_info.sensor_id) {
				IMX800_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			IMX800_LOG_INF("Read sensor id fail, id: 0x%x\n",
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
	IMX800_LOG_INF("%s -\n", __func__);

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
	IMX800_LOG_INF("E\n");
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
	IMX800_LOG_INF("%s E\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->readout_length = imgsensor_info.pre.readout_length;
	ctx->read_margin = imgsensor_info.pre.read_margin;
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
	IMX800_LOG_INF("E\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		IMX800_LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			ctx->current_fps,
			imgsensor_info.cap.max_framerate / 10);
	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->readout_length = imgsensor_info.cap.readout_length;
	ctx->read_margin = imgsensor_info.cap.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	capture_setting(ctx, ctx->current_fps);

	return ERROR_NONE;
}	/* capture(ctx) */
static kal_uint32 normal_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->readout_length = imgsensor_info.normal_video.readout_length;
	ctx->read_margin = imgsensor_info.normal_video.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	normal_video_setting(ctx, ctx->current_fps);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	/*ctx->video_mode = KAL_TRUE;*/
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->readout_length = imgsensor_info.hs_video.readout_length;
	ctx->read_margin = imgsensor_info.hs_video.read_margin;
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
	IMX800_LOG_INF("%s. 720P@240FPS\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	/*ctx->video_mode = KAL_TRUE;*/
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->readout_length = imgsensor_info.slim_video.readout_length;
	ctx->read_margin = imgsensor_info.slim_video.read_margin;
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
	IMX800_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->readout_length = imgsensor_info.custom1.readout_length;
	ctx->read_margin = imgsensor_info.custom1.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	custom1_setting(ctx);

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	ctx->pclk = imgsensor_info.custom2.pclk;
	ctx->line_length = imgsensor_info.custom2.linelength;
	ctx->frame_length = imgsensor_info.custom2.framelength;
	ctx->min_frame_length = imgsensor_info.custom2.framelength;
	ctx->readout_length = imgsensor_info.custom2.readout_length;
	ctx->read_margin = imgsensor_info.custom2.read_margin;
	ctx->autoflicker_en = KAL_FALSE;
	custom2_setting(ctx);

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	ctx->pclk = imgsensor_info.custom3.pclk;
	ctx->line_length = imgsensor_info.custom3.linelength;
	ctx->frame_length = imgsensor_info.custom3.framelength;
	ctx->min_frame_length = imgsensor_info.custom3.framelength;
	ctx->readout_length = imgsensor_info.custom3.readout_length;
	ctx->read_margin = imgsensor_info.custom3.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	custom3_setting(ctx);

	return ERROR_NONE;
}	/* custom3 */

static kal_uint32 custom4(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	ctx->pclk = imgsensor_info.custom4.pclk;
	ctx->line_length = imgsensor_info.custom4.linelength;
	ctx->frame_length = imgsensor_info.custom4.framelength;
	ctx->min_frame_length = imgsensor_info.custom4.framelength;
	ctx->readout_length = imgsensor_info.custom4.readout_length;
	ctx->read_margin = imgsensor_info.custom4.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	custom4_setting(ctx);

	if (last_sensor_awb.ABS_GAIN_GR != 0 &&
	    last_sensor_awb.ABS_GAIN_GB != 0 &&
	    last_sensor_awb.ABS_GAIN_R  != 0 &&
	    last_sensor_awb.ABS_GAIN_B  != 0) {
		IMX800_LOG_INF("custom4 write awb gain r:g:b %d:%d:%d \n",
						last_sensor_awb.ABS_GAIN_R,
						last_sensor_awb.ABS_GAIN_GR,
						last_sensor_awb.ABS_GAIN_B);
		yuechuimx800_awb_gain(ctx, &last_sensor_awb);
	}
	return ERROR_NONE;
}	/* custom4 */

static kal_uint32 custom5(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	ctx->pclk = imgsensor_info.custom5.pclk;
	ctx->line_length = imgsensor_info.custom5.linelength;
	ctx->frame_length = imgsensor_info.custom5.framelength;
	ctx->min_frame_length = imgsensor_info.custom5.framelength;
	ctx->readout_length = imgsensor_info.custom5.readout_length;
	ctx->read_margin = imgsensor_info.custom5.read_margin;
	ctx->autoflicker_en = KAL_FALSE;


	custom5_setting(ctx);

	return ERROR_NONE;
}	/* custom5 */


static kal_uint32 custom6(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("%s.\n", __func__);

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	ctx->pclk = imgsensor_info.custom6.pclk;
	ctx->line_length = imgsensor_info.custom6.linelength;
	ctx->frame_length = imgsensor_info.custom6.framelength;
	ctx->min_frame_length = imgsensor_info.custom6.framelength;
	ctx->readout_length = imgsensor_info.custom6.readout_length;
	ctx->read_margin = imgsensor_info.custom6.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	custom6_setting(ctx);

	return ERROR_NONE;
}	/* custom5 */

static kal_uint32 custom7(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM7;
	ctx->pclk = imgsensor_info.custom7.pclk;
	ctx->line_length = imgsensor_info.custom7.linelength;
	ctx->frame_length = imgsensor_info.custom7.framelength;
	ctx->min_frame_length = imgsensor_info.custom7.framelength;
	ctx->readout_length = imgsensor_info.custom7.readout_length;
	ctx->read_margin = imgsensor_info.custom7.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	custom7_setting(ctx);

	return ERROR_NONE;
}

static kal_uint32 custom8(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM8;
	ctx->pclk = imgsensor_info.custom8.pclk;
	ctx->line_length = imgsensor_info.custom8.linelength;
	ctx->frame_length = imgsensor_info.custom8.framelength;
	ctx->min_frame_length = imgsensor_info.custom8.framelength;
	ctx->readout_length = imgsensor_info.custom8.readout_length;
	ctx->read_margin = imgsensor_info.custom8.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	custom8_setting(ctx);

	return ERROR_NONE;
}

static kal_uint32 custom9(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM9;
	ctx->pclk = imgsensor_info.custom9.pclk;
	ctx->line_length = imgsensor_info.custom9.linelength;
	ctx->frame_length = imgsensor_info.custom9.framelength;
	ctx->min_frame_length = imgsensor_info.custom9.framelength;
	ctx->readout_length = imgsensor_info.custom9.readout_length;
	ctx->read_margin = imgsensor_info.custom9.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	custom9_setting(ctx);

	return ERROR_NONE;
}

static kal_uint32 custom10(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	IMX800_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM10;
	ctx->pclk = imgsensor_info.custom10.pclk;
	ctx->line_length = imgsensor_info.custom10.linelength;
	ctx->frame_length = imgsensor_info.custom10.framelength;
	ctx->min_frame_length = imgsensor_info.custom10.framelength;
	ctx->readout_length = imgsensor_info.custom10.readout_length;
	ctx->read_margin = imgsensor_info.custom10.read_margin;
	ctx->autoflicker_en = KAL_FALSE;

	custom10_setting(ctx);

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
	IMX800_LOG_INF("scenario_id = %d\n", scenario_id);

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
#if ENABLE_PDAF
	sensor_info->PDAF_Support = PDAF_SUPPORT_CAMSV_QPD;
#endif
	sensor_info->HDR_Support = HDR_SUPPORT_STAGGER_FDOL;
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
	IMX800_LOG_INF("scenario_id = %d\n", scenario_id);
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		preview(ctx, image_window, sensor_config_data);
		break;
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
	default:
		IMX800_LOG_INF("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control(ctx) */

static void extend_frame_length(struct subdrv_ctx *ctx, kal_uint32 ns)
{
	int i;
	kal_uint32 old_fl = ctx->frame_length;
	kal_uint32 calc_fl = 0;
	kal_uint32 readoutLength = ctx->readout_length;
	kal_uint32 readMargin = ctx->read_margin;
	kal_uint32 per_frame_ns = (kal_uint32)(((unsigned long long)ctx->frame_length *
		(unsigned long long)ctx->line_length * 1000000000) / (unsigned long long)ctx->pclk);

	/* NEED TO FIX start: support 1exp-2exp only; 3exp-?exp instead */
	if (previous_exp_cnt == 1)
		ns = 10000000;

	if (ns)
		ctx->frame_length = (kal_uint32)(((unsigned long long)(per_frame_ns + ns)) *
			ctx->frame_length / per_frame_ns);

	/* fl constraint: normal DOL behavior while stagger seamless switch */
	if (previous_exp_cnt > 1) {
		calc_fl = (readoutLength + readMargin);
		for (i = 1; i < previous_exp_cnt; i++)
			calc_fl += (previous_exp[i] + imgsensor_info.margin);

		ctx->frame_length = max(calc_fl, ctx->frame_length);
	}
	/* NEED TO FIX end */

	write_cmos_sensor_8(ctx, 0x0104, 0x01);
	write_frame_len(ctx, ctx->frame_length);
	write_cmos_sensor_8(ctx, 0x0104, 0x00);


	ctx->extend_frame_length_en = KAL_TRUE;

	ns = (kal_uint32)(((unsigned long long)(ctx->frame_length - old_fl) *
		(unsigned long long)ctx->line_length * 1000000000) / (unsigned long long)ctx->pclk);
	IMX800_LOG_INF("new frame len = %d, old frame len = %d, per_frame_ns = %d, add more %d ns",
		ctx->frame_length, old_fl, per_frame_ns, ns);
}

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

	IMX800_LOG_INF("+\n");
	if (timestamps_ns > last_sof_timestamps_ns) {
		timestamps_diff_ms = (timestamps_ns - last_sof_timestamps_ns) / 1000000;
		frametime_ms = 1000 * (unsigned long long)frame_length *
					(unsigned long long)ctx->line_length /
					(unsigned long long)ctx->pclk;
		IMX800_LOG_INF("timestamps_diff_ms : %lu\n", timestamps_diff_ms);
		IMX800_LOG_INF("frametime_ms : %d\n", frametime_ms);
		if (((2*frametime_ms) > timestamps_diff_ms) && (timestamps_diff_ms > 30)) {
			mdelay((2*frametime_ms) - timestamps_diff_ms + 5);
			IMX800_LOG_INF("mdelay : %d\n", ((2*frametime_ms) - timestamps_diff_ms + 5));
		}
	}
	IMX800_LOG_INF("-\n");
}

static kal_uint32 seamless_switch(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, uint32_t *ae_ctrl)
{
	uint32_t gain    = ae_ctrl[5];
	uint32_t shutter = ae_ctrl[0];

	ctx->extend_frame_length_en = KAL_FALSE;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		IMX800_LOG_INF("seamless switch to preview!\n");
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
	case SENSOR_SCENARIO_ID_CUSTOM4:
		IMX800_LOG_INF("seamless switch to fullsize mode!\n");
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
		IMX800_LOG_INF("seamless switch to 2x mode!\n");
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
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		IMX800_LOG_INF("seamless switch to normal video!\n");
		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.normal_video.pclk;
		ctx->line_length = imgsensor_info.normal_video.linelength;
		ctx->frame_length = imgsensor_info.normal_video.framelength;
		ctx->min_frame_length = imgsensor_info.normal_video.framelength;
		ctx->readout_length = imgsensor_info.normal_video.readout_length;
		ctx->read_margin = imgsensor_info.normal_video.read_margin;

		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x3010, 0x02); // Complete mode

		normal_video_setting(ctx, ctx->current_fps);

		if (shutter != 0) {
			set_shutter(ctx, shutter, false);
		}

		if (gain != 0) {
			set_gain(ctx, gain, false);
		}

		write_cmos_sensor_8(ctx, 0x0104, 0x00);
		//mdelay(40);
		break;

	case SENSOR_SCENARIO_ID_CUSTOM9:
		IMX800_LOG_INF("seamless switch to 2exp stagger mode!\n");
		ctx->sensor_mode = scenario_id;
		ctx->autoflicker_en = KAL_FALSE;
		ctx->pclk = imgsensor_info.custom9.pclk;
		ctx->line_length = imgsensor_info.custom9.linelength;
		ctx->frame_length = imgsensor_info.custom9.framelength;
		ctx->min_frame_length = imgsensor_info.custom9.framelength;
		ctx->readout_length = imgsensor_info.custom9.readout_length;
		ctx->read_margin = imgsensor_info.custom9.read_margin;

		write_cmos_sensor_8(ctx, 0x0104, 0x01);
		write_cmos_sensor_8(ctx, 0x3010, 0x02); // Complete mode

		custom9_setting(ctx);

		if (ae_ctrl) {
			hdr_write_tri_shutter_w_gph(ctx,
				ae_ctrl[0],	0, ae_ctrl[1], KAL_FALSE);
			hdr_write_tri_gain_w_gph(ctx,
				ae_ctrl[5],	0, ae_ctrl[6], KAL_FALSE);
		}

		write_cmos_sensor_8(ctx, 0x0104, 0x00);
		//mdelay(40);
		break;

	default:
		IMX800_LOG_INF(
		"error! wrong setting in set_seamless_switch = %d",
		scenario_id);
		return 0xff;
	}

	ctx->fast_mode_on = KAL_TRUE;
	IMX800_LOG_INF("%s success, scenario is switched to %d", __func__, scenario_id);
	return 0;
}

static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	IMX800_LOG_INF("framerate = %d\n ", framerate);
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
		IMX800_LOG_DBG("enable! fps = %d", framerate);
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

	IMX800_LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
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
		IMX800_LOG_INF(
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
		IMX800_LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	IMX800_LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
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
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_uint32 mode)
{
	IMX800_LOG_INF("mode: %d\n", mode);
	switch (mode) {
		case 0: // OFF
		case 1: // Solid Color
		case 2: // Color bar
		case 3: // Color bar Fade To Gray
		case 4: // PN9
			write_cmos_sensor_8(ctx, 0x0601, mode);
			break;
		case 5: // Black
			write_cmos_sensor_8(ctx, 0x0602, 0x00);
			write_cmos_sensor_8(ctx, 0x0603, 0x00);
			write_cmos_sensor_8(ctx, 0x0604, 0x00);
			write_cmos_sensor_8(ctx, 0x0605, 0x00);
			write_cmos_sensor_8(ctx, 0x0606, 0x00);
			write_cmos_sensor_8(ctx, 0x0607, 0x00);
			write_cmos_sensor_8(ctx, 0x0608, 0x00);
			write_cmos_sensor_8(ctx, 0x0609, 0x00);
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

	temperature = read_cmos_sensor_8(ctx, 0x013A);

	if (temperature <= 0x54)
		temperature_convert = temperature;
	else if (temperature >= 0x55 && temperature <= 0x7F)
		temperature_convert = 85;
	else if (temperature >= 0x81 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (char)temperature | 0xFFFFFF0;

	IMX800_LOG_INF("temp_c(%d), read_reg(%d)\n", temperature_convert, temperature);

	return temperature_convert;
}

static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	kal_uint16 sensor_output_cnt;

	last_sof_timestamps_ns = ktime_get_boottime_ns();
	sensor_output_cnt = read_cmos_sensor_8(ctx, 0x0005);

	if (ctx->fast_mode_on) {
		write_cmos_sensor_8(ctx, 0x3010, 0x00);
		ctx->fast_mode_on = false;
		IMX800_LOG_INF("disable seamless\n");
	}

	IMX800_LOG_INF("sensormode(%d) sof_cnt(%d) sensor_output_cnt(%d)\n",
		ctx->sensor_mode, sof_cnt, sensor_output_cnt);
	return 0;
};

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
#if ENABLE_PDAF
	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB = NULL;
	uint32_t *pAeCtrls;
	uint32_t *pScenarios;
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*IMX800_LOG_INF("feature_id = %d\n", feature_id);*/
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
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(yuechuimx800_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)yuechuimx800_ana_gain_table,
			sizeof(yuechuimx800_ana_gain_table));
		}
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
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

		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(feature_data + 2) = 1;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*(feature_data + 2) = 8;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(feature_data + 2) = imgsensor_info.exp_step;
			break;
		}

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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
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
		IMX800_LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
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
		IMX800_LOG_INF("current fps :%d\n", (UINT32)*feature_data_32);
		ctx->current_fps = *feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		IMX800_LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[0],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
#if ENABLE_PDAF
	case SENSOR_FEATURE_GET_PDAF_INFO:
		IMX800_LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
			(UINT16) *feature_data);
		PDAFinfo =
		  (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
			sizeof(struct SET_PD_BLOCK_INFO_T));
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		IMX800_LOG_INF(
		"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
			(UINT16) *feature_data);
		switch (*feature_data) {
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
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
			break;

		}
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		IMX800_LOG_INF("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		yuechuimx800_get_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		IMX800_LOG_INF("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
			(*feature_para_len));
		yuechuimx800_set_pdaf_reg_setting(ctx,
				(*feature_para_len) / sizeof(UINT32),
				feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		IMX800_LOG_INF("PDAF mode :%d\n", *feature_data_16);
		ctx->pdaf_mode = *feature_data_16;
		break;
#endif
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		IMX800_LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
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
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		IMX800_LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		IMX800_LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
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
		IMX800_LOG_INF("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
break;

#if ENABLE_PDAF
	case SENSOR_FEATURE_GET_VC_INFO:
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM7:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM2:
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
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[6],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[7],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM10:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[8],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
			break;
		}
	break;
#endif
	case SENSOR_FEATURE_SET_AWB_GAIN:
		/* modify to separate 3hdr and remosaic */
		if ((ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM4 ) ||
			(ctx->sensor_mode == IMGSENSOR_MODE_CUSTOM5 )) {
			yuechuimx800_awb_gain(ctx,
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

		yuechuimx800_set_lsc_reg_setting(ctx, index, feature_data_16,
					  (*feature_para_len)/sizeof(UINT16));
		}
		break;
	case XIAOMI_FEATURE_ENABLE_SEAMLESS_SWITCH:
		enable_seamless = *feature_data_32;
		IMX800_LOG_INF("enable seamless switch setting : %d", enable_seamless);
		break;
	case SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH:
		//extend_frame_length(ctx, (MUINT32) *feature_data);
		break;
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
		if ((feature_data + 1) != NULL) {
			pAeCtrls =
			(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			IMX800_LOG_INF("warning! no ae_ctrl input");
		}

		if ((feature_data + 3) != NULL) {
			pSetSensorAWB =
			(struct SET_SENSOR_AWB_GAIN *)((uintptr_t)(*(feature_data + 3)));
		} else {
			IMX800_LOG_INF("warning! no awb gain input");
		}

		if (feature_data == NULL) {
			IMX800_LOG_INF("error! input scenario is null!");
			return ERROR_INVALID_SCENARIO_ID;
		}

		if (pSetSensorAWB != NULL) {
			yuechuimx800_awb_gain(ctx, pSetSensorAWB);
			IMX800_LOG_INF("update awb gain by seamless switch");
		}

		seamless_switch(ctx, (*feature_data), pAeCtrls);
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		if ((feature_data + 1) != NULL) {
			pScenarios =
				(MUINT32 *)((uintptr_t)(*(feature_data + 1)));
		} else {
			IMX800_LOG_INF("input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
			*pScenarios       = SENSOR_SCENARIO_ID_CUSTOM5;
			*(pScenarios + 1) = SENSOR_SCENARIO_ID_CUSTOM4;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*pScenarios = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*pScenarios = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*pScenarios = SENSOR_SCENARIO_ID_CUSTOM9;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*pScenarios = SENSOR_SCENARIO_ID_NORMAL_VIDEO;
			break;
		default:
			*pScenarios = 0xff;
			break;
		}
		IMX800_LOG_INF("SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS %d %d\n",
			*feature_data, *pScenarios);
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM9:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = HDR_RAW_STAGGER_2EXP;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		default:
			*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = HDR_NONE;
			break;
		}
		IMX800_LOG_INF(
			"SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY, scenarioId:%llu, HDR:%llu\n",
			*feature_data, *(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO:
		if (*feature_data == SENSOR_SCENARIO_ID_NORMAL_VIDEO) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM9;
				break;
			default:
				break;
			}
		} else if (*feature_data == SENSOR_SCENARIO_ID_CUSTOM9) {
			switch (*(feature_data + 1)) {
			case HDR_NONE:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_NORMAL_VIDEO;
				break;
			default:
				break;
			}
		}
		IMX800_LOG_INF("SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO %d %d %d\n",
				(UINT16) *feature_data,
				(UINT16) *(feature_data + 1),
				(UINT16) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME:
		if (*feature_data == SENSOR_SCENARIO_ID_CUSTOM9) {
			// see IMX800 SRM, table 5-27 constraints of COARSE_INTEG_TIME
			switch (*(feature_data + 1)) {
			case VC_STAGGER_NE:
			case VC_STAGGER_ME:
			case VC_STAGGER_SE:
			default:
				*(feature_data + 2) = 65532 - imgsensor_info.margin*2;
				break;
			}
		} else {
			*(feature_data + 2) = 0; //No limit
		}
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER://for 2EXP
		IMX800_LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER, LE=%d, SE=%d\n",
				(UINT32) *feature_data, (UINT32) *(feature_data + 1));
		hdr_write_tri_shutter(ctx, (UINT32)*feature_data,
					0,
					(UINT32)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN://for 2EXP
		IMX800_LOG_INF("SENSOR_FEATURE_SET_DUAL_GAIN, LE=%d, SE=%d\n",
				(UINT32)*feature_data, (UINT32)*(feature_data + 1));
		hdr_write_tri_gain(ctx,
				(UINT32)*feature_data,
				0,
				(UINT32)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER://for 3EXP
		IMX800_LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_SHUTTER, LE=%d, ME=%d, SE=%d\n",
				(UINT32) *feature_data,
				(UINT32) *(feature_data + 1),
				(UINT32) *(feature_data + 2));
		hdr_write_tri_shutter(ctx,
				(UINT32) *feature_data,
				(UINT32) *(feature_data + 1),
				(UINT32) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_GAIN://for 3EXP
		IMX800_LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_GAIN, LG=%d, SG=%d, MG=%d\n",
				(UINT32) *feature_data,
				(UINT32) *(feature_data + 1),
				(UINT32) *(feature_data + 2));
		hdr_write_tri_gain(ctx,
				(UINT32) *feature_data,
				(UINT32) *(feature_data + 1),
				(UINT32) *(feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_FRAMELENGTH:
		set_frame_length(ctx, (UINT16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME:
		set_multi_shutter_frame_length(ctx, (UINT16 *)(*feature_data),
					(UINT16) (*(feature_data + 1)),
					(UINT16) (*(feature_data + 2)));
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
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
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
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust2[] = {
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
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
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
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
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
			.hsize = 2048,
			.vsize = 1536,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},

};

static struct mtk_mbus_frame_desc_entry frame_desc_cust8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1536,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 384,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cust9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},

	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_ME,
		},
	},

	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},

	{
		.bus.csi2 = {
			.channel = 3,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
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
			.hsize = 3840,
			.vsize = 2160,
			//.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM6:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
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
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust8);
		memcpy(fd->entry, frame_desc_cust8, sizeof(frame_desc_cust8));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM9:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust9);
		memcpy(fd->entry, frame_desc_cust9, sizeof(frame_desc_cust9));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM10:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cust10);
		memcpy(fd->entry, frame_desc_cust10, sizeof(frame_desc_cust10));
		break;
	default:
		return -1;
	}

	return 0;
}
#endif


static const struct subdrv_ctx defctx = {

	.ana_gain_def  = 4 * BASEGAIN,
	.ana_gain_max  = 64 * BASEGAIN,
	.ana_gain_min  = 1463,//1.428671085f  * BASEGAIN
	.ana_gain_step = 1,
	.exposure_def  = 0x3D0,
	/* support long exposure at most 128 times) */
	.exposure_max  = (0xffff * 128) - 48,
	.exposure_min  = 16,
	.exposure_step = 1,
	.frame_time_delay_frame = 3,
	.margin                 = 48,
	.max_frame_length       = 0xffff * 128,
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
	.readout_length         = 0,
	.read_margin            = 24,
	.current_ae_effective_frame = 2,
	.extend_frame_length_en     = KAL_FALSE,
	.ae_ctrl_gph_en             = KAL_FALSE,
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
	{HW_ID_AFVDD, 2900000, 0},
	{HW_ID_AVDD1, 1200000, 1}, // VCAM_LDO
	{HW_ID_AVDD,  1,       1}, // AVDD 2.9 gpio en
	{HW_ID_AVDD2, 1,       1}, // AVDD 1.8 gpio en
	{HW_ID_DVDD,  1,       1},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_MCLK,  24,      0},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 1},
	{HW_ID_RST,   1,       3},
};

const struct subdrv_entry yuechuimx800_mipi_raw_entry = {
	.name       = "yuechuimx800_mipi_raw",
	.id         = YUECHUIMX800_SENSOR_ID,
	.pw_seq     = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops        = &ops,
};

