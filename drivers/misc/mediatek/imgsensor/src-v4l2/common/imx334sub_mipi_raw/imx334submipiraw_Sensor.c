// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     IMX334mipi_Sensor.c
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
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "imx334submipiraw_Sensor.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor(...) subdrv_i2c_rd_u8(__VA_ARGS__)
#define write_cmos_sensor(...) subdrv_i2c_wr_u8(__VA_ARGS__)
#define imx334sub_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)

/************************Modify Following Strings for Debug********************/
#define PFX "IMX334SUB_camera_sensor"
#define LOG_1 LOG_INF("IMX334SUB,MIPI 4LANE\n")
/************************   Modify end    *************************************/
#define HDR_raw12 0

#undef IMX334_24_FPS

#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX334SUB_SENSOR_ID,
	.checksum_value = 0xD1EFF68B,
	.pre = {
		.pclk = 475200000,
		.linelength = 7040,
		.framelength = 2250,
		.startx = 12,
		.starty = 10,
		.grabwindow_width = 3840,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 475200000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 475200000,
		.linelength = 7040,
		.framelength = 2250,
		.startx = 12,
		.starty = 10,
		.grabwindow_width = 3840,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 475200000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 475200000,
		.linelength = 7040,
		.framelength = 2250,
		.startx = 12,
		.starty = 10,
		.grabwindow_width = 3840,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 475200000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 475200000,
		.linelength = 7040,
		.framelength = 2250,
		.startx = 12,
		.starty = 10,
		.grabwindow_width = 3840,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 475200000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 475200000,
		.linelength = 7040,
		.framelength = 2250,
		.startx = 12,
		.starty = 10,
		.grabwindow_width = 3840,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 475200000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 712800000,
		.linelength = 6336,
		.framelength = 2500,
		.startx = 12,
		.starty = 10,
		.grabwindow_width = 3840,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 475200000,
		.max_framerate = 150,
	},
	.custom2 = {
		.pclk = 712800000,
		.linelength = 5280,
		.framelength = 1125,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1076,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 712800000,
		.max_framerate = 300,
	},
#if HDR_raw12
	.custom2 = {
		.pclk = 712800000,
		.linelength = 4795,
		.framelength = 1239,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1076,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 712800000,
		.max_framerate = 300,
	},
#endif
	.margin = 10,
	.min_shutter = 3,
	.min_gain = BASEGAIN,
	.max_gain = 254788,
	.min_gain_iso = 100,
	.exp_step = 2,
	.gain_step = 1,
	.gain_type = 0,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 7,	/* support sensor mode num */
	.frame_time_delay_frame = 3,
	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2, /* enter high speed video  delay frame num */
	.custom1_delay_frame = 2,
	.custom2_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_8MA,	/* mclk driving current */

	/* sensor_interface_type */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	/* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */

	/* sensor output first pixel color */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24,	/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x34, 0xff},
/* record sensor support all write id addr, only supprt 4must end with 0xff */
	.i2c_speed = 400,	/* i2c read/write speed */
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	{3864, 2180, 0, 0, 3864, 2180, 3864, 2180,
	 0000, 0000, 3864, 2180, 12, 10, 3840, 2160},
	{3864, 2180, 0, 0, 3864, 2180, 3864, 2180,
	 0000, 0000, 3864, 2180, 12, 10, 3840, 2160},
	{3864, 2180, 0, 0, 3864, 2180, 3864, 2180,
	 0000, 0000, 3864, 2180, 12, 10, 3840, 2160},
	{3864, 2180, 0, 0, 3864, 2180, 3864, 2180,
	 0000, 0000, 3864, 2180, 12, 10, 3840, 2160},
	{3864, 2180, 0, 0, 3864, 2180, 3864, 2180,
	 0000, 0000, 3864, 2180, 12, 10, 3840, 2160},
	{3864, 2180, 0, 0, 3864, 2180, 3864, 2180,
	 0000, 0000, 3864, 2180, 12, 10, 3840, 2160},
	{3864, 2180, 0, 0, 3864, 2180, 3864, 2180,
	 1020, 1280, 1920, 1076, 0, 0, 1920, 1076},
};

#ifdef IMGSENSOR_VC_ROUTING
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0xF00,
			.vsize = 0x870,
			.user_data_desc = VC_STAGGER_NE,
		},
	}
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0xF00,
			.vsize = 0x870,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0xF00,
			.vsize = 0x870,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};


static struct v4l2_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0xF00,
			.vsize = 0x870,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0xF00,
			.vsize = 0x870,
			.user_data_desc = VC_STAGGER_SE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x780,
			.vsize = 0x434,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x780,
			.vsize = 0x434,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 0x780,
			.vsize = 0x434,
			.user_data_desc = VC_STAGGER_SE,
		},
	},
};

static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cap);
		memcpy(fd->entry, frame_desc_cap, sizeof(frame_desc_cap));
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_vid);
		memcpy(fd->entry, frame_desc_vid, sizeof(frame_desc_vid));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cus1);
		memcpy(fd->entry, frame_desc_cus1, sizeof(frame_desc_cus1));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_cus2);
		memcpy(fd->entry, frame_desc_cus2, sizeof(frame_desc_cus2));
		break;
	default:
		return -1;
	}

	return 0;
}
#endif

#define IMX334SUBMIPI_MaxGainIndex (241)
kal_uint32 imx334SUBMIPI_sensorGainMapping[IMX334SUBMIPI_MaxGainIndex][2] = {
	{64, 0},
	{66, 1},
	{68, 2},
	{70, 3},
	{73, 4},
	{76, 5},
	{78, 6},
	{81, 7},
	{84, 8},
	{87, 9},
	{90, 10},
	{93, 11},
	{96, 12},
	{100, 13},
	{103, 14},
	{107, 15},
	{111, 16},
	{115, 17},
	{119, 18},
	{123, 19},
	{127, 20},
	{132, 21},
	{136, 22},
	{141, 23},
	{146, 24},
	{151, 25},
	{157, 26},
	{162, 27},
	{168, 28},
	{174, 29},
	{180, 30},
	{186, 31},
	{193, 32},
	{200, 33},
	{207, 34},
	{214, 35},
	{221, 36},
	{229, 37},
	{237, 38},
	{246, 39},
	{254, 40},
	{263, 41},
	{273, 42},
	{282, 43},
	{292, 44},
	{302, 45},
	{313, 46},
	{324, 47},
	{335, 48},
	{347, 49},
	{359, 50},
	{372, 51},
	{385, 52},
	{399, 53},
	{413, 54},
	{427, 55},
	{442, 56},
	{458, 57},
	{474, 58},
	{491, 59},
	{508, 60},
	{526, 61},
	{544, 62},
	{563, 63},
	{583, 64},
	{604, 65},
	{625, 66},
	{647, 67},
	{670, 68},
	{693, 69},
	{718, 70},
	{743, 71},
	{769, 72},
	{796, 73},
	{824, 74},
	{853, 75},
	{883, 76},
	{914, 77},
	{946, 78},
	{979, 79},
	{1014, 80},
	{1049, 81},
	{1086, 82},
	{1125, 83},
	{1164, 84},
	{1205, 85},
	{1247, 86},
	{1291, 87},
	{1337, 88},
	{1384, 89},
	{1432, 90},
	{1483, 91},
	{1535, 92},
	{1589, 93},
	{1645, 94},
	{1702, 95},
	{1762, 96},
	{1824, 97},
	{1888, 98},
	{1955, 99},
	{2023, 100},
	{2094, 101},
	{2168, 102},
	{2244, 103},
	{2323, 104},
	{2405, 105},
	{2489, 106},
	{2577, 107},
	{2667, 108},
	{2761, 109},
	{2858, 110},
	{2959, 111},
	{3063, 112},
	{3170, 113},
	{3282, 114},
	{3397, 115},
	{3517, 116},
	{3640, 117},
	{3768, 118},
	{3901, 119},
	{4038, 120},
	{4180, 121},
	{4326, 122},
	{4478, 123},
	{4636, 124},
	{4799, 125},
	{4967, 126},
	{5142, 127},
	{5323, 128},
	{5510, 129},
	{5704, 130},
	{5904, 131},
	{6111, 132},
	{6326, 133},
	{6549, 134},
	{6779, 135},
	{7017, 136},
	{7264, 137},
	{7519, 138},
	{7783, 139},
	{8057, 140},
	{8340, 141},
	{8633, 142},
	{8936, 143},
	{9250, 144},
	{9575, 145},
	{9912, 146},
	{10260, 147},
	{10621, 148},
	{10994, 149},
	{11380, 150},
	{11780, 151},
	{12194, 152},
	{12623, 153},
	{13067, 154},
	{13526, 155},
	{14001, 156},
	{14493, 157},
	{15003, 158},
	{15530, 159},
	{16076, 160},
	{16641, 161},
	{17225, 162},
	{17831, 163},
	{18457, 164},
	{19106, 165},
	{19777, 166},
	{20472, 167},
	{21192, 168},
	{21937, 169},
	{22708, 170},
	{23506, 171},
	{24332, 172},
	{25187, 173},
	{26072, 174},
	{26988, 175},
	{27937, 176},
	{28918, 177},
	{29935, 178},
	{30987, 179},
	{32075, 180},
	{33203, 181},
	{34370, 182},
	{35577, 183},
	{36828, 184},
	{38122, 185},
	{39462, 186},
	{40848, 187},
	{42284, 188},
	{43770, 189},
	{45308, 190},
	{46900, 191},
	{48548, 192},
	{50255, 193},
	{52021, 194},
	{53849, 195},
	{55741, 196},
	{57700, 197},
	{59728, 198},
	{61827, 199},
	{64000, 200},
	{66249, 201},
	{68577, 202},
	{70987, 203},
	{73481, 204},
	{76064, 205},
	{78737, 206},
	{81504, 207},
	{84368, 208},
	{87333, 209},
	{90402, 210},
	{93579, 211},
	{96867, 212},
	{100272, 213},
	{103795, 214},
	{107443, 215},
	{111219, 216},
	{115127, 217},
	{119173, 218},
	{123361, 219},
	{127696, 220},
	{132184, 221},
	{136829, 222},
	{141638, 223},
	{146615, 224},
	{151767, 225},
	{157101, 226},
	{162622, 227},
	{168337, 228},
	{174252, 229},
	{180376, 230},
	{186715, 231},
	{193276, 232},
	{200069, 233},
	{207099, 234},
	{214377, 235},
	{221911, 236},
	{229710, 237},
	{237782, 238},
	{246138, 239},
	{254788, 240},
};

kal_uint32 imx334MIPIsub_sensorGain[IMX334MIPI_MaxGainIndex] = {
	64*16, 66*16, 68*16, 70*16, 73*16, 76*16, 78*16, 81*16, 84*16, 87*16,
	90*16, 93*16, 96*16, 100*16, 103*16, 107*16, 111*16, 115*16, 119*16,
	123*16, 127*16, 132*16, 136*16, 141*16, 146*16, 151*16, 157*16, 162*16,
	168*16, 174*16, 180*16, 186*16, 193*16, 200*16, 207*16, 214*16, 221*16,
	229*16, 237*16, 246*16, 254*16, 263*16, 273*16, 282*16, 292*16, 302*16,
	313*16, 324*16, 335*16, 347*16, 359*16, 372*16, 385*16, 399*16, 413*16,
	427*16, 442*16, 458*16, 474*16, 491*16, 508*16, 526*16, 544*16, 563*16,
	583*16, 604*16, 625*16, 647*16, 670*16, 693*16, 718*16, 743*16, 769*16,
	796*16, 824*16, 853*16, 883*16, 914*16, 946*16, 979*16, 1014*16,
	1049*16, 1086*16, 1125*16, 1164*16, 1205*16, 1247*16, 1291*16, 1337*16,
	1384*16, 1432*16, 1483*16, 1535*16, 1589*16, 1645*16, 1702*16, 1762*16,
	1824*16, 1888*16, 1955*16, 2023*16, 2094*16, 2168*16, 2244*16, 2323*16,
	2405*16, 2489*16, 2577*16, 2667*16, 2761*16, 2858*16, 2959*16, 3063*16,
	3170*16, 3282*16, 3397*16, 3517*16, 3640*16, 3768*16, 3901*16, 4038*16,
	4180*16, 4326*16, 4478*16, 4636*16, 4799*16, 4967*16, 5142*16, 5323*16,
	5510*16, 5704*16, 5904*16, 6111*16, 6326*16, 6549*16, 6779*16, 7017*16,
	7264*16, 7519*16, 7783*16, 8057*16, 8340*16, 8633*16, 8936*16, 9250*16,
	9575*16, 9912*16, 10260*16, 10621*16, 10994*16, 11380*16, 11780*16,
	12194*16, 12623*16, 13067*16, 13526*16, 14001*16, 14493*16, 15003*16,
	15530*16, 16076*16, 16641*16, 17225*16, 17831*16, 18457*16, 19106*16,
	19777*16, 20472*16, 21192*16, 21937*16, 22708*16, 23506*16, 24332*16,
	25187*16, 26072*16, 26988*16, 27937*16, 28918*16, 29935*16, 30987*16,
	32075*16, 33203*16, 34370*16, 35577*16, 36828*16, 38122*16, 39462*16,
	40848*16, 42284*16, 43770*16, 45308*16, 46900*16, 48548*16, 50255*16,
	52021*16, 53849*16, 55741*16, 57700*16, 59728*16, 61827*16, 64000*16,
	66249*16, 68577*16, 70987*16, 73481*16, 76064*16, 78737*16, 81504*16,
	84368*16, 87333*16, 90402*16, 93579*16, 96867*16, 100272*16, 103795*16,
	107443*16, 111219*16, 115127*16, 119173*16, 123361*16, 127696*16,
	132184*16, 136829*16, 141638*16, 146615*16, 151767*16, 157101*16,
	162622*16, 168337*16, 174252*16, 180376*16, 186715*16, 193276*16,
	200069*16, 207099*16, 214377*16, 221911*16, 229710*16, 237782*16,
	246138*16, 254788*16,
};
static void set_dummy(struct subdrv_ctx *ctx)
{
	LOG_INF("frame_length = %d, line_length = %d\n",
				ctx->frame_length,
				ctx->line_length);

	write_cmos_sensor(ctx, 0x3031, ctx->frame_length >> 8);
	write_cmos_sensor(ctx, 0x3030, ctx->frame_length & 0xFF);


}				/*    set_dummy  */

static kal_uint32 return_sensor_id(struct subdrv_ctx *ctx)
{
	kal_uint32 ret = 0x00;

	ret = ((read_cmos_sensor(ctx, 0x3034) << 8) | read_cmos_sensor(ctx, 0x3035));
	if (ret != 0 && ret != 0xffff)
		return IMX334SUB_SENSOR_ID;

	return 0;
}

static void set_max_framerate(struct subdrv_ctx *ctx, UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = ctx->frame_length;
	/* unsigned long flags; */

	LOG_INF("framerate = %d, min framelength should enable %d\n",
			framerate,
			min_framelength_en);

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
	ctx->frame_length =
	    (frame_length > ctx->min_frame_length)
	    ? frame_length : ctx->min_frame_length;

	ctx->dummy_line =
		ctx->frame_length - ctx->min_frame_length;
	/* dummy_line = frame_length - ctx->min_frame_length; */
	/* if (dummy_line < 0) */
	/* ctx->dummy_line = 0; */
	/* else */
	/* ctx->dummy_line = dummy_line; */
	/* ctx->frame_length = frame_length + ctx->dummy_line; */
	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;
		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	set_dummy(ctx);
}				/*    set_max_framerate  */

static void set_2exp_max_framerate(struct subdrv_ctx *ctx,
			UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = ctx->frame_length;
	/* unsigned long flags; */

	LOG_INF("framerate = %d, min framelength should enable %d\n",
			framerate,
			min_framelength_en);

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length / 2;
	ctx->frame_length =
	    (frame_length > ctx->min_frame_length)
	    ? frame_length : ctx->min_frame_length;

	ctx->dummy_line =
		ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;
		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	set_dummy(ctx);
}				/*    set_max_framerate  */


static void set_3exp_max_framerate(struct subdrv_ctx *ctx,
			UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = ctx->frame_length;
	/* unsigned long flags; */

	LOG_INF("framerate = %d, min framelength should enable %d\n",
			framerate,
			min_framelength_en);

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length / 4;
	ctx->frame_length =
	    (frame_length > ctx->min_frame_length)
	    ? frame_length : ctx->min_frame_length;

	ctx->dummy_line =
		ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;
		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	set_dummy(ctx);
}				/*    set_max_framerate  */


static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;
	kal_uint32 SHR0 = 0;

	if (shutter > ctx->min_frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;
	else
		ctx->frame_length = ctx->min_frame_length;
	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk /
			ctx->line_length * 10 / ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			// Extend frame length
			//write_cmos_sensor_8(ctx, 0x3001, 1);
			write_cmos_sensor(ctx, 0x3031,
				ctx->frame_length >> 8);
			write_cmos_sensor(ctx, 0x3030,
				ctx->frame_length & 0xFF);
			//write_cmos_sensor_8(ctx, 0x3001, 0);
		}
	} else {
		// Extend frame length
		//write_cmos_sensor_8(ctx, 0x3001, 1);
		write_cmos_sensor(ctx, 0x3031, ctx->frame_length >> 8);
		write_cmos_sensor(ctx, 0x3030, ctx->frame_length & 0xFF);
		//write_cmos_sensor_8(ctx, 0x3001, 0);
	}

	// Update Shutter
	//write_cmos_sensor_8(ctx, 0x3001, 1);
	SHR0 = ctx->frame_length - shutter;
	write_cmos_sensor(ctx, 0x3059, (SHR0 >> 8) & 0xFF);
	write_cmos_sensor(ctx, 0x3058, SHR0  & 0xFF);

	LOG_INF("shutter =%d, SHR0 = %d, framelength =%d\n",
		shutter, SHR0, ctx->frame_length);
}	/*	write_shutter  */


/*************************************************************************
 * FUNCTION
 *    set_shutter
 *
 * DESCRIPTION
 *    This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *    iShutter : exposured lines
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	ctx->shutter = shutter;

	write_shutter(ctx, shutter);
} /*    set_shutter */

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
	kal_uint16 iI;

	for (iI = 0; iI < (IMX334SUBMIPI_MaxGainIndex-1); iI++) {
		if (gain <= imx334SUBMIPI_sensorGainMapping[iI][0])
			break;
	}

	return imx334SUBMIPI_sensorGainMapping[iI][1];
}

/*************************************************************************
 * FUNCTION
 *    set_gain
 *
 * DESCRIPTION
 *    This function is to set global gain to sensor.
 *
 * PARAMETERS
 *    iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *    the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain)
{

	kal_uint16 reg_gain;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X    */
	/* [4:9] = M meams M X         */
	/* Total gain = M + N /16 X   */

	/*  */
	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_INF("Error gain setting");

		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(ctx, gain);
	ctx->gain = reg_gain;
	LOG_INF("gain = %d, reg_gain = 0x%x\n ", gain, reg_gain);
	/* Global analog Gain for Long expo */
	write_cmos_sensor(ctx, 0x30E9, (reg_gain >> 8) & 0xFF);
	write_cmos_sensor(ctx, 0x30E8, reg_gain & 0xFF);

	return gain;

}				/*    set_gain  */

static void hdr_write_shutter(struct subdrv_ctx *ctx, kal_uint16 LE, kal_uint16 ME, kal_uint16 SE)
{
	kal_uint16 TE;
	kal_uint16 FSC;
	kal_uint16 SHR0;
	kal_uint16 SHR1 = 13;
	kal_uint16 RHS1;
	kal_uint16 realtime_fps;

	LOG_INF("E! le:0x%x, me:0x%x, se:0x%x\n", LE, ME, SE);
	TE = LE + SE + /*offset*/26 + imgsensor_info.margin;
	FSC = ctx->frame_length * 2;
	if (FSC < TE/* FSC */) {
		ctx->frame_length = TE/2 + 1;
		FSC = ctx->frame_length * 2;
	}
	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / 2 /
			ctx->line_length * 10 / ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_2exp_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_2exp_max_framerate(ctx, 146, 0);
		else {
			// Extend frame length
			write_cmos_sensor(ctx, 0x3031,
				ctx->frame_length >> 8);
			write_cmos_sensor(ctx, 0x3030,
				ctx->frame_length & 0xFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(ctx, 0x3031, ctx->frame_length >> 8);
		write_cmos_sensor(ctx, 0x3030, ctx->frame_length & 0xFF);
	}

	SHR0 = FSC - LE;
	RHS1 = SHR1 + SE;

	write_cmos_sensor(ctx, 0x3001, 0x01);
	/* LE */
	write_cmos_sensor(ctx, 0x3058, SHR0 & 0xFF);
	write_cmos_sensor(ctx, 0x3059, SHR0 >> 8 & 0xFF);
	write_cmos_sensor(ctx, 0x305A, SHR0 >> 12 & 0xF);
	/* SE */
	write_cmos_sensor(ctx, 0x3068, RHS1 & 0xFF);
	write_cmos_sensor(ctx, 0x3069, RHS1 >> 8 & 0xFF);
	write_cmos_sensor(ctx, 0x306A, RHS1 >> 12 & 0xF);

	write_cmos_sensor(ctx, 0x3001, 0x00);

}

static void hdr_write_gain(struct subdrv_ctx *ctx, kal_uint16 lgain, kal_uint16 sgain)
{
	kal_uint16 reg_lgain;
	kal_uint16 reg_sgain;

	LOG_INF("E! lgain:0x%x, sgain:0x%x, se:0x%x\n", lgain, sgain);

	if (lgain < imgsensor_info.min_gain || lgain > imgsensor_info.max_gain) {
		LOG_INF("Error lgain setting");

		if (lgain < imgsensor_info.min_gain)
			lgain = imgsensor_info.min_gain;
		else
			lgain = imgsensor_info.max_gain;
	}
	if (sgain < imgsensor_info.min_gain || sgain > imgsensor_info.max_gain) {
		LOG_INF("Error sgain setting");

		if (sgain < imgsensor_info.min_gain)
			sgain = imgsensor_info.min_gain;
		else
			sgain = imgsensor_info.max_gain;
	}

	reg_lgain = gain2reg(ctx, lgain);
	reg_sgain = gain2reg(ctx, sgain);

	LOG_INF("lgain:0x%x, mg:0x%x, sg:0x%x\n", reg_lgain, reg_sgain);
	write_cmos_sensor(ctx, 0x30E8, reg_lgain & 0xFF);
	write_cmos_sensor(ctx, 0x30E9, reg_lgain >> 8 & 0x07);
	write_cmos_sensor(ctx, 0x30EA, reg_sgain & 0xFF);
	write_cmos_sensor(ctx, 0x30EB, reg_sgain >> 8 & 0x07);

}

static void hdr_write_tri_shutter(struct subdrv_ctx *ctx,
			kal_uint32 LE, kal_uint32 ME, kal_uint32 SE)
{
	kal_uint16 TE;
	kal_uint16 FSC;
	kal_uint16 SHR0;
	kal_uint16 SHR1 = 13;
	kal_uint16 RHS1;
	kal_uint16 SHR2;
	kal_uint16 RHS2;
	kal_uint16 realtime_fps;

	LOG_INF("E! le:0x%x, me:0x%x, se:0x%x\n", LE, ME, SE);

	if (1)
		return;

	TE = LE + ME + SE + 39 + imgsensor_info.margin;
	FSC = ctx->frame_length * 4;
	if (FSC < TE/* FSC */) {
		ctx->frame_length = TE/4 + 3;
		FSC = ctx->frame_length * 4;
	}
	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / 4 /
			ctx->line_length * 10 / ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_3exp_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_3exp_max_framerate(ctx, 146, 0);
		else {
			// Extend frame length
			write_cmos_sensor(ctx, 0x3031,
				ctx->frame_length >> 8);
			write_cmos_sensor(ctx, 0x3030,
				ctx->frame_length & 0xFF);
		}
	} else {
		// Extend frame length
		write_cmos_sensor(ctx, 0x3031, ctx->frame_length >> 8);
		write_cmos_sensor(ctx, 0x3030, ctx->frame_length & 0xFF);
	}

	SHR0 = FSC - LE;
	RHS1 = SHR1 + ME;
	SHR2 = RHS1 + SHR1;  /*SHR1 => min_offset */
	RHS2 = SHR2 + SE;

	write_cmos_sensor(ctx, 0x3001, 0x01);
	/* LE */
	write_cmos_sensor(ctx, 0x3058, SHR0 & 0xFF);
	write_cmos_sensor(ctx, 0x3059, SHR0 >> 8 & 0xFF);
	write_cmos_sensor(ctx, 0x305A, SHR0 >> 12 & 0xF);
	/* ME */
	write_cmos_sensor(ctx, 0x3068, RHS1 & 0xFF);
	write_cmos_sensor(ctx, 0x3069, RHS1 >> 8 & 0xFF);
	write_cmos_sensor(ctx, 0x306A, RHS1 >> 12 & 0xF);
	/* SE */
	write_cmos_sensor(ctx, 0x3060, SHR2 & 0xFF);
	write_cmos_sensor(ctx, 0x3061, SHR2 >> 8 & 0xFF);
	write_cmos_sensor(ctx, 0x3062, SHR2 >> 12 & 0xF);
	write_cmos_sensor(ctx, 0x306C, RHS2 & 0xFF);
	write_cmos_sensor(ctx, 0x306D, RHS2 >> 8 & 0xFF);
	write_cmos_sensor(ctx, 0x306E, RHS2 >> 12 & 0xF);
	write_cmos_sensor(ctx, 0x3001, 0x00);

}

static void hdr_write_tri_gain(struct subdrv_ctx *ctx,
			kal_uint16 lgain, kal_uint16 mgain, kal_uint16 sgain)
{
	kal_uint16 reg_lgain;
	kal_uint16 reg_mgain;
	kal_uint16 reg_sgain;

	if (1)
		return;

	if (lgain < imgsensor_info.min_gain || lgain > imgsensor_info.max_gain) {
		LOG_INF("Error lgain setting");

		if (lgain < imgsensor_info.min_gain)
			lgain = imgsensor_info.min_gain;
		else
			lgain = imgsensor_info.max_gain;
	}
	if (mgain < imgsensor_info.min_gain || mgain > imgsensor_info.max_gain) {
		LOG_INF("Error mgain setting");

		if (mgain < imgsensor_info.min_gain)
			mgain = imgsensor_info.min_gain;
		else
			mgain = imgsensor_info.max_gain;
	}
	if (sgain < imgsensor_info.min_gain || sgain > imgsensor_info.max_gain) {
		LOG_INF("Error sgain setting");

		if (sgain < imgsensor_info.min_gain)
			sgain = imgsensor_info.min_gain;
		else
			sgain = imgsensor_info.max_gain;
	}

	reg_lgain = gain2reg(ctx, lgain);
	reg_mgain = gain2reg(ctx, mgain);
	reg_sgain = gain2reg(ctx, sgain);

	LOG_INF("lgain:0x%x, mg:0x%x, sg:0x%x\n", reg_lgain, reg_mgain, reg_sgain);
	write_cmos_sensor(ctx, 0x30E8, reg_lgain & 0xFF);
	write_cmos_sensor(ctx, 0x30E9, reg_lgain >> 8 & 0x07);
	write_cmos_sensor(ctx, 0x30EA, reg_mgain & 0xFF);
	write_cmos_sensor(ctx, 0x30EB, reg_mgain >> 8 & 0x07);
	write_cmos_sensor(ctx, 0x30EC, reg_sgain & 0xFF);
	write_cmos_sensor(ctx, 0x30ED, reg_sgain >> 8 & 0x07);
}

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3
#endif

kal_uint16 addr_data_pair_init_imx334sub[] = {
	/* INCK setting */
	0x300c, 0x3B,
	0x300D, 0x2A,
	/* vmax */
	0x3030, 0xCA,
	0x3031, 0x08,
	0x3032, 0x00,
	/* hmax */
	0x3034, 0x4C,
	0x3035, 0x04,
	/* ADbit */
	0x3050, 0x00,
	/* INCK setting */
	0x314C, 0xC6,
	0x314D, 0x00,
	0x315A, 0x02,
	0x3168, 0xA0,
	0x316A, 0x7E,
	/* MDbit */
	0x319D, 0x00,
	0x319E, 0x01,

	0x31A1, 0x00,
	0x3288, 0x21,
	0x328A, 0x02,
	0x3414, 0x05,
	0x3416, 0x18,
	0x341C, 0xFF,
	0x341D, 0x01,
	0x35AC, 0x0E,
	0x3648, 0x01,
	0x364A, 0x04,
	0x364C, 0x04,
	0x3678, 0x01,
	0x367C, 0x31,

	/* global setting */
	0x367E, 0x31,
	0x3708, 0x02,
	0x3714, 0x01,
	0x3715, 0x02,
	0x3716, 0x02,
	0x3717, 0x02,
	0x371C, 0x3D,
	0x371D, 0x3F,
	0x372C, 0x00,
	0x372D, 0x00,
	0x372E, 0x46,
	0x372F, 0x00,
	0x3730, 0x89,
	0x3731, 0x00,
	0x3732, 0x08,
	0x3733, 0x01,
	0x3734, 0xFE,
	0x3735, 0x05,
	0x375D, 0x00,
	0x375E, 0x00,
	0x375F, 0x61,
	0x3760, 0x06,
	0x3768, 0x1B,
	0x3769, 0x1B,
	0x376A, 0x1A,
	0x376B, 0x19,
	0x376C, 0x18,
	0x376D, 0x14,
	0x376E, 0x0F,
	0x3776, 0x00,
	0x3777, 0x00,
	0x3778, 0x46,
	0x3779, 0x00,
	0x377A, 0x08,
	0x377B, 0x01,
	0x377C, 0x45,
	0x377D, 0x01,
	0x377E, 0x23,
	0x377F, 0x02,
	0x3780, 0xD9,
	0x3781, 0x03,
	0x3782, 0xF5,
	0x3783, 0x06,
	0x3784, 0xA5,
	0x3788, 0x0F,
	0x378A, 0xD9,
	0x378B, 0x03,
	0x378C, 0xEB,
	0x378D, 0x05,
	0x378E, 0x87,
	0x378F, 0x06,
	0x3790, 0xF5,
	0x3792, 0x43,
	0x3794, 0x7A,
	0x3796, 0xA1,
	0x3A18, 0x8F,
	0x3A1A, 0x4F,
	0x3A1C, 0x47,
	0x3A1E, 0x37,
	0x3A20, 0x4F,
	0x3A22, 0x87,
	0x3A24, 0x4F,
	0x3A26, 0x7F,
	0x3A28, 0x3F,
	0x3E04, 0x0E,
};

static void sensor_init(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");
	write_cmos_sensor(ctx, 0x3000, 0x01);
	write_cmos_sensor(ctx, 0x3001, 0x00);
	write_cmos_sensor(ctx, 0x3002, 0x01);
	write_cmos_sensor(ctx, 0x3003, 0x00);
	/* INCK setting */
	write_cmos_sensor(ctx, 0x300c, 0x3B);
	write_cmos_sensor(ctx, 0x300D, 0x2A);
	write_cmos_sensor(ctx, 0x3018, 0x00);
	write_cmos_sensor(ctx, 0x302C, 0x30);
	write_cmos_sensor(ctx, 0x302D, 0x00);
	write_cmos_sensor(ctx, 0x302E, 0x18);
	write_cmos_sensor(ctx, 0x302F, 0x0F);
	/* vmax */
	write_cmos_sensor(ctx, 0x3030, 0xCA);
	write_cmos_sensor(ctx, 0x3031, 0x08);
	write_cmos_sensor(ctx, 0x3032, 0x00);
	 /* hmax */
	write_cmos_sensor(ctx, 0x3034, 0x4C);
	write_cmos_sensor(ctx, 0x3035, 0x04);
	//write_cmos_sensor(ctx, 0x3048, 0x00);
	//write_cmos_sensor(ctx, 0x3049, 0x00);
	//write_cmos_sensor(ctx, 0x304A, 0x00);
	//write_cmos_sensor(ctx, 0x304B, 0x00);
	//write_cmos_sensor(ctx, 0x304C, 0x14);
	write_cmos_sensor(ctx, 0x304E, 0x00);
	write_cmos_sensor(ctx, 0x304F, 0x00);
	 /* ADbit */
	write_cmos_sensor(ctx, 0x3050, 0x00);
	 /* shutter setting */
	write_cmos_sensor(ctx, 0x3058, 0x05);
	write_cmos_sensor(ctx, 0x3059, 0x00);
	write_cmos_sensor(ctx, 0x305A, 0x00);
	write_cmos_sensor(ctx, 0x305C, 0x09);
	write_cmos_sensor(ctx, 0x305D, 0x00);
	write_cmos_sensor(ctx, 0x305E, 0x00);
	write_cmos_sensor(ctx, 0x3060, 0x74);
	write_cmos_sensor(ctx, 0x3061, 0x00);
	write_cmos_sensor(ctx, 0x3062, 0x00);
	write_cmos_sensor(ctx, 0x3064, 0x09);
	write_cmos_sensor(ctx, 0x3065, 0x00);
	write_cmos_sensor(ctx, 0x3066, 0x00);
	write_cmos_sensor(ctx, 0x3068, 0x8B);
	write_cmos_sensor(ctx, 0x3069, 0x00);
	write_cmos_sensor(ctx, 0x306A, 0x00);
	write_cmos_sensor(ctx, 0x306C, 0x44);
	write_cmos_sensor(ctx, 0x306D, 0x03);
	write_cmos_sensor(ctx, 0x306E, 0x00);
	write_cmos_sensor(ctx, 0x3074, 0xB0);
	write_cmos_sensor(ctx, 0x3075, 0x00);
	write_cmos_sensor(ctx, 0x3076, 0x84);
	write_cmos_sensor(ctx, 0x3077, 0x08);
	write_cmos_sensor(ctx, 0x3078, 0x02);
	write_cmos_sensor(ctx, 0x3079, 0x00);
	write_cmos_sensor(ctx, 0x307A, 0x00);
	write_cmos_sensor(ctx, 0x307B, 0x00);
	write_cmos_sensor(ctx, 0x3080, 0x02);
	write_cmos_sensor(ctx, 0x3081, 0x00);
	write_cmos_sensor(ctx, 0x3082, 0x00);
	write_cmos_sensor(ctx, 0x3083, 0x00);
	write_cmos_sensor(ctx, 0x3088, 0x02);
	write_cmos_sensor(ctx, 0x308E, 0xB1);
	write_cmos_sensor(ctx, 0x308F, 0x00);
	write_cmos_sensor(ctx, 0x3090, 0x84);
	write_cmos_sensor(ctx, 0x3091, 0x08);
	write_cmos_sensor(ctx, 0x3094, 0x00);
	write_cmos_sensor(ctx, 0x3095, 0x00);
	write_cmos_sensor(ctx, 0x3096, 0x00);
	write_cmos_sensor(ctx, 0x309B, 0x02);
	write_cmos_sensor(ctx, 0x309C, 0x00);
	write_cmos_sensor(ctx, 0x309D, 0x00);
	write_cmos_sensor(ctx, 0x309E, 0x00);
	write_cmos_sensor(ctx, 0x30A4, 0x00);
	write_cmos_sensor(ctx, 0x30A5, 0x00);
	write_cmos_sensor(ctx, 0x30B6, 0x00);
	write_cmos_sensor(ctx, 0x30B7, 0x00);
	write_cmos_sensor(ctx, 0x30C6, 0x00);
	write_cmos_sensor(ctx, 0x30C7, 0x00);
	write_cmos_sensor(ctx, 0x30CE, 0x00);
	write_cmos_sensor(ctx, 0x30CF, 0x00);
	write_cmos_sensor(ctx, 0x30D8, 0xF8);
	write_cmos_sensor(ctx, 0x30D9, 0x11);
	write_cmos_sensor(ctx, 0x30E8, 0x00);
	write_cmos_sensor(ctx, 0x30E9, 0x00);
	write_cmos_sensor(ctx, 0x30EA, 0x00);
	write_cmos_sensor(ctx, 0x30EB, 0x00);
	write_cmos_sensor(ctx, 0x30EC, 0x00);
	write_cmos_sensor(ctx, 0x30ED, 0x00);
	write_cmos_sensor(ctx, 0x30EE, 0x00);
	write_cmos_sensor(ctx, 0x30EF, 0x00);
	write_cmos_sensor(ctx, 0x3116, 0x08);
	write_cmos_sensor(ctx, 0x3117, 0x00);
	/* INCK setting */
	write_cmos_sensor(ctx, 0x314C, 0xC6);
	write_cmos_sensor(ctx, 0x314D, 0x00);
	write_cmos_sensor(ctx, 0x315A, 0x02);
	write_cmos_sensor(ctx, 0x3168, 0xA0);
	write_cmos_sensor(ctx, 0x316A, 0x7E);
	write_cmos_sensor(ctx, 0x3199, 0x00);
	/* MDbit */
	write_cmos_sensor(ctx, 0x319D, 0x00);
	write_cmos_sensor(ctx, 0x319E, 0x01);
	write_cmos_sensor(ctx, 0x319F, 0x03);
	write_cmos_sensor(ctx, 0x31A0, 0x2A);
	write_cmos_sensor(ctx, 0x31A1, 0x00);

	write_cmos_sensor(ctx, 0x31A4, 0x00);
	write_cmos_sensor(ctx, 0x31A5, 0x00);
	write_cmos_sensor(ctx, 0x31A6, 0x00);
	write_cmos_sensor(ctx, 0x31A8, 0x00);
	write_cmos_sensor(ctx, 0x31AC, 0x00);
	write_cmos_sensor(ctx, 0x31AD, 0x00);
	write_cmos_sensor(ctx, 0x31AE, 0x00);
	write_cmos_sensor(ctx, 0x31D4, 0x00);
	write_cmos_sensor(ctx, 0x31D5, 0x00);
	write_cmos_sensor(ctx, 0x31D7, 0x00);
	write_cmos_sensor(ctx, 0x31DD, 0x03);
	write_cmos_sensor(ctx, 0x31E4, 0x01);
	write_cmos_sensor(ctx, 0x31E8, 0x00);
	write_cmos_sensor(ctx, 0x31F3, 0x00);
	write_cmos_sensor(ctx, 0x3200, 0x11);

	write_cmos_sensor(ctx, 0x3288, 0x21);
	write_cmos_sensor(ctx, 0x328A, 0x02);
	write_cmos_sensor(ctx, 0x3300, 0x00);
	write_cmos_sensor(ctx, 0x3302, 0x32);
	write_cmos_sensor(ctx, 0x3303, 0x00);
	write_cmos_sensor(ctx, 0x3308, 0x84);
	write_cmos_sensor(ctx, 0x3309, 0x08);
	write_cmos_sensor(ctx, 0x3414, 0x05);
	write_cmos_sensor(ctx, 0x3416, 0x18);
	write_cmos_sensor(ctx, 0x341C, 0xFF);
	write_cmos_sensor(ctx, 0x341D, 0x01);
	write_cmos_sensor(ctx, 0x35AC, 0x0E);
	write_cmos_sensor(ctx, 0x3648, 0x01);
	write_cmos_sensor(ctx, 0x364A, 0x04);
	write_cmos_sensor(ctx, 0x364C, 0x04);
	write_cmos_sensor(ctx, 0x3678, 0x01);
	write_cmos_sensor(ctx, 0x367C, 0x31);

	/* global setting */
	write_cmos_sensor(ctx, 0x367E, 0x31);
	write_cmos_sensor(ctx, 0x3708, 0x02);
	write_cmos_sensor(ctx, 0x3714, 0x01);
	write_cmos_sensor(ctx, 0x3715, 0x02);
	write_cmos_sensor(ctx, 0x3716, 0x02);
	write_cmos_sensor(ctx, 0x3717, 0x02);
	write_cmos_sensor(ctx, 0x371C, 0x3D);
	write_cmos_sensor(ctx, 0x371D, 0x3F);
	write_cmos_sensor(ctx, 0x372C, 0x00);
	write_cmos_sensor(ctx, 0x372D, 0x00);
	write_cmos_sensor(ctx, 0x372E, 0x46);
	write_cmos_sensor(ctx, 0x372F, 0x00);
	write_cmos_sensor(ctx, 0x3730, 0x89);
	write_cmos_sensor(ctx, 0x3731, 0x00);
	write_cmos_sensor(ctx, 0x3732, 0x08);
	write_cmos_sensor(ctx, 0x3733, 0x01);
	write_cmos_sensor(ctx, 0x3734, 0xFE);
	write_cmos_sensor(ctx, 0x3735, 0x05);
	write_cmos_sensor(ctx, 0x375D, 0x00);
	write_cmos_sensor(ctx, 0x375E, 0x00);
	write_cmos_sensor(ctx, 0x375F, 0x61);
	write_cmos_sensor(ctx, 0x3760, 0x06);
	write_cmos_sensor(ctx, 0x3768, 0x1B);
	write_cmos_sensor(ctx, 0x3769, 0x1B);
	write_cmos_sensor(ctx, 0x376A, 0x1A);
	write_cmos_sensor(ctx, 0x376B, 0x19);
	write_cmos_sensor(ctx, 0x376C, 0x18);
	write_cmos_sensor(ctx, 0x376D, 0x14);
	write_cmos_sensor(ctx, 0x376E, 0x0F);
	write_cmos_sensor(ctx, 0x3776, 0x00);
	write_cmos_sensor(ctx, 0x3777, 0x00);
	write_cmos_sensor(ctx, 0x3778, 0x46);
	write_cmos_sensor(ctx, 0x3779, 0x00);
	write_cmos_sensor(ctx, 0x377A, 0x08);
	write_cmos_sensor(ctx, 0x377B, 0x01);
	write_cmos_sensor(ctx, 0x377C, 0x45);
	write_cmos_sensor(ctx, 0x377D, 0x01);
	write_cmos_sensor(ctx, 0x377E, 0x23);
	write_cmos_sensor(ctx, 0x377F, 0x02);
	write_cmos_sensor(ctx, 0x3780, 0xD9);
	write_cmos_sensor(ctx, 0x3781, 0x03);
	write_cmos_sensor(ctx, 0x3782, 0xF5);
	write_cmos_sensor(ctx, 0x3783, 0x06);
	write_cmos_sensor(ctx, 0x3784, 0xA5);
	write_cmos_sensor(ctx, 0x3788, 0x0F);
	write_cmos_sensor(ctx, 0x378A, 0xD9);
	write_cmos_sensor(ctx, 0x378B, 0x03);
	write_cmos_sensor(ctx, 0x378C, 0xEB);
	write_cmos_sensor(ctx, 0x378D, 0x05);
	write_cmos_sensor(ctx, 0x378E, 0x87);
	write_cmos_sensor(ctx, 0x378F, 0x06);
	write_cmos_sensor(ctx, 0x3790, 0xF5);
	write_cmos_sensor(ctx, 0x3792, 0x43);
	write_cmos_sensor(ctx, 0x3794, 0x7A);
	write_cmos_sensor(ctx, 0x3796, 0xA1);
	write_cmos_sensor(ctx, 0x37B0, 0x36);
	write_cmos_sensor(ctx, 0x3A01, 0x03);
	write_cmos_sensor(ctx, 0x3A04, 0x90);
	write_cmos_sensor(ctx, 0x3A05, 0x12);
	write_cmos_sensor(ctx, 0x3A18, 0x8F);
	write_cmos_sensor(ctx, 0x3A19, 0x00);
	write_cmos_sensor(ctx, 0x3A1A, 0x4F);
	write_cmos_sensor(ctx, 0x3A1B, 0x00);
	write_cmos_sensor(ctx, 0x3A1C, 0x47);
	write_cmos_sensor(ctx, 0x3A1D, 0x00);
	write_cmos_sensor(ctx, 0x3A1E, 0x37);
	write_cmos_sensor(ctx, 0x3A1F, 0x00);
	write_cmos_sensor(ctx, 0x3A20, 0x4F);
	write_cmos_sensor(ctx, 0x3A21, 0x00);
	write_cmos_sensor(ctx, 0x3A22, 0x87);
	write_cmos_sensor(ctx, 0x3A23, 0x00);
	write_cmos_sensor(ctx, 0x3A24, 0x4F);
	write_cmos_sensor(ctx, 0x3A25, 0x00);
	write_cmos_sensor(ctx, 0x3A26, 0x7F);
	write_cmos_sensor(ctx, 0x3A27, 0x00);
	write_cmos_sensor(ctx, 0x3A28, 0x3F);
	write_cmos_sensor(ctx, 0x3A29, 0x00);
	write_cmos_sensor(ctx, 0x3E04, 0x0E);
}    /*    sensor_init  */

static void capture_write_register(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");
	write_cmos_sensor(ctx, 0x3000, 0x01);
	write_cmos_sensor(ctx, 0x3001, 0x00);
	write_cmos_sensor(ctx, 0x3002, 0x01);
	write_cmos_sensor(ctx, 0x3003, 0x00);
	/* INCK setting */
	write_cmos_sensor(ctx, 0x300c, 0x3B);
	write_cmos_sensor(ctx, 0x300D, 0x2A);
	write_cmos_sensor(ctx, 0x3018, 0x00);
	write_cmos_sensor(ctx, 0x302C, 0x30);
	write_cmos_sensor(ctx, 0x302D, 0x00);
	write_cmos_sensor(ctx, 0x302E, 0x18);
	write_cmos_sensor(ctx, 0x302F, 0x0F);
	/* vmax */
	write_cmos_sensor(ctx, 0x3030, 0xCA);
	write_cmos_sensor(ctx, 0x3031, 0x08);
	write_cmos_sensor(ctx, 0x3032, 0x00);
	 /* hmax */
	write_cmos_sensor(ctx, 0x3034, 0x4C);
	write_cmos_sensor(ctx, 0x3035, 0x04);
	//write_cmos_sensor(ctx, 0x3048, 0x00);
	//write_cmos_sensor(ctx, 0x3049, 0x00);
	//write_cmos_sensor(ctx, 0x304A, 0x00);
	//write_cmos_sensor(ctx, 0x304B, 0x00);
	//write_cmos_sensor(ctx, 0x304C, 0x14);
	write_cmos_sensor(ctx, 0x304E, 0x00);
	write_cmos_sensor(ctx, 0x304F, 0x00);
	 /* ADbit */
	write_cmos_sensor(ctx, 0x3050, 0x00);
	 /* shutter setting */
	write_cmos_sensor(ctx, 0x3058, 0x05);
	write_cmos_sensor(ctx, 0x3059, 0x00);
	write_cmos_sensor(ctx, 0x305A, 0x00);
	write_cmos_sensor(ctx, 0x305C, 0x09);
	write_cmos_sensor(ctx, 0x305D, 0x00);
	write_cmos_sensor(ctx, 0x305E, 0x00);
	write_cmos_sensor(ctx, 0x3060, 0x74);
	write_cmos_sensor(ctx, 0x3061, 0x00);
	write_cmos_sensor(ctx, 0x3062, 0x00);
	write_cmos_sensor(ctx, 0x3064, 0x09);
	write_cmos_sensor(ctx, 0x3065, 0x00);
	write_cmos_sensor(ctx, 0x3066, 0x00);
	write_cmos_sensor(ctx, 0x3068, 0x8B);
	write_cmos_sensor(ctx, 0x3069, 0x00);
	write_cmos_sensor(ctx, 0x306A, 0x00);
	write_cmos_sensor(ctx, 0x306C, 0x44);
	write_cmos_sensor(ctx, 0x306D, 0x03);
	write_cmos_sensor(ctx, 0x306E, 0x00);
	write_cmos_sensor(ctx, 0x3074, 0xB0);
	write_cmos_sensor(ctx, 0x3075, 0x00);
	write_cmos_sensor(ctx, 0x3076, 0x84);
	write_cmos_sensor(ctx, 0x3077, 0x08);
	write_cmos_sensor(ctx, 0x3078, 0x02);
	write_cmos_sensor(ctx, 0x3079, 0x00);
	write_cmos_sensor(ctx, 0x307A, 0x00);
	write_cmos_sensor(ctx, 0x307B, 0x00);
	write_cmos_sensor(ctx, 0x3080, 0x02);
	write_cmos_sensor(ctx, 0x3081, 0x00);
	write_cmos_sensor(ctx, 0x3082, 0x00);
	write_cmos_sensor(ctx, 0x3083, 0x00);
	write_cmos_sensor(ctx, 0x3088, 0x02);
	write_cmos_sensor(ctx, 0x308E, 0xB1);
	write_cmos_sensor(ctx, 0x308F, 0x00);
	write_cmos_sensor(ctx, 0x3090, 0x84);
	write_cmos_sensor(ctx, 0x3091, 0x08);
	write_cmos_sensor(ctx, 0x3094, 0x00);
	write_cmos_sensor(ctx, 0x3095, 0x00);
	write_cmos_sensor(ctx, 0x3096, 0x00);
	write_cmos_sensor(ctx, 0x309B, 0x02);
	write_cmos_sensor(ctx, 0x309C, 0x00);
	write_cmos_sensor(ctx, 0x309D, 0x00);
	write_cmos_sensor(ctx, 0x309E, 0x00);
	write_cmos_sensor(ctx, 0x30A4, 0x00);
	write_cmos_sensor(ctx, 0x30A5, 0x00);
	write_cmos_sensor(ctx, 0x30B6, 0x00);
	write_cmos_sensor(ctx, 0x30B7, 0x00);
	write_cmos_sensor(ctx, 0x30C6, 0x00);
	write_cmos_sensor(ctx, 0x30C7, 0x00);
	write_cmos_sensor(ctx, 0x30CE, 0x00);
	write_cmos_sensor(ctx, 0x30CF, 0x00);
	write_cmos_sensor(ctx, 0x30D8, 0xF8);
	write_cmos_sensor(ctx, 0x30D9, 0x11);
	write_cmos_sensor(ctx, 0x30E8, 0x00);
	write_cmos_sensor(ctx, 0x30E9, 0x00);
	write_cmos_sensor(ctx, 0x30EA, 0x00);
	write_cmos_sensor(ctx, 0x30EB, 0x00);
	write_cmos_sensor(ctx, 0x30EC, 0x00);
	write_cmos_sensor(ctx, 0x30ED, 0x00);
	write_cmos_sensor(ctx, 0x30EE, 0x00);
	write_cmos_sensor(ctx, 0x30EF, 0x00);
	write_cmos_sensor(ctx, 0x3116, 0x08);
	write_cmos_sensor(ctx, 0x3117, 0x00);
	/* INCK setting */
	write_cmos_sensor(ctx, 0x314C, 0xC6);
	write_cmos_sensor(ctx, 0x314D, 0x00);
	write_cmos_sensor(ctx, 0x315A, 0x02);
	write_cmos_sensor(ctx, 0x3168, 0xA0);
	write_cmos_sensor(ctx, 0x316A, 0x7E);
	write_cmos_sensor(ctx, 0x3199, 0x00);
	/* MDbit */
	write_cmos_sensor(ctx, 0x319D, 0x00);
	write_cmos_sensor(ctx, 0x319E, 0x01);
	write_cmos_sensor(ctx, 0x319F, 0x03);
	write_cmos_sensor(ctx, 0x31A0, 0x2A);
	write_cmos_sensor(ctx, 0x31A1, 0x00);

	write_cmos_sensor(ctx, 0x31A4, 0x00);
	write_cmos_sensor(ctx, 0x31A5, 0x00);
	write_cmos_sensor(ctx, 0x31A6, 0x00);
	write_cmos_sensor(ctx, 0x31A8, 0x00);
	write_cmos_sensor(ctx, 0x31AC, 0x00);
	write_cmos_sensor(ctx, 0x31AD, 0x00);
	write_cmos_sensor(ctx, 0x31AE, 0x00);
	write_cmos_sensor(ctx, 0x31D4, 0x00);
	write_cmos_sensor(ctx, 0x31D5, 0x00);
	write_cmos_sensor(ctx, 0x31D7, 0x00);
	write_cmos_sensor(ctx, 0x31DD, 0x03);
	write_cmos_sensor(ctx, 0x31E4, 0x01);
	write_cmos_sensor(ctx, 0x31E8, 0x00);
	write_cmos_sensor(ctx, 0x31F3, 0x00);
	write_cmos_sensor(ctx, 0x3200, 0x11);

	write_cmos_sensor(ctx, 0x3288, 0x21);
	write_cmos_sensor(ctx, 0x328A, 0x02);
	write_cmos_sensor(ctx, 0x3300, 0x00);
	write_cmos_sensor(ctx, 0x3302, 0x32);
	write_cmos_sensor(ctx, 0x3303, 0x00);
	write_cmos_sensor(ctx, 0x3308, 0x84);
	write_cmos_sensor(ctx, 0x3309, 0x08);
	write_cmos_sensor(ctx, 0x3414, 0x05);
	write_cmos_sensor(ctx, 0x3416, 0x18);
	write_cmos_sensor(ctx, 0x341C, 0xFF);
	write_cmos_sensor(ctx, 0x341D, 0x01);
	write_cmos_sensor(ctx, 0x35AC, 0x0E);
	write_cmos_sensor(ctx, 0x3648, 0x01);
	write_cmos_sensor(ctx, 0x364A, 0x04);
	write_cmos_sensor(ctx, 0x364C, 0x04);
	write_cmos_sensor(ctx, 0x3678, 0x01);
	write_cmos_sensor(ctx, 0x367C, 0x31);

	/* global setting */
	write_cmos_sensor(ctx, 0x367E, 0x31);
	write_cmos_sensor(ctx, 0x3708, 0x02);
	write_cmos_sensor(ctx, 0x3714, 0x01);
	write_cmos_sensor(ctx, 0x3715, 0x02);
	write_cmos_sensor(ctx, 0x3716, 0x02);
	write_cmos_sensor(ctx, 0x3717, 0x02);
	write_cmos_sensor(ctx, 0x371C, 0x3D);
	write_cmos_sensor(ctx, 0x371D, 0x3F);
	write_cmos_sensor(ctx, 0x372C, 0x00);
	write_cmos_sensor(ctx, 0x372D, 0x00);
	write_cmos_sensor(ctx, 0x372E, 0x46);
	write_cmos_sensor(ctx, 0x372F, 0x00);
	write_cmos_sensor(ctx, 0x3730, 0x89);
	write_cmos_sensor(ctx, 0x3731, 0x00);
	write_cmos_sensor(ctx, 0x3732, 0x08);
	write_cmos_sensor(ctx, 0x3733, 0x01);
	write_cmos_sensor(ctx, 0x3734, 0xFE);
	write_cmos_sensor(ctx, 0x3735, 0x05);
	write_cmos_sensor(ctx, 0x375D, 0x00);
	write_cmos_sensor(ctx, 0x375E, 0x00);
	write_cmos_sensor(ctx, 0x375F, 0x61);
	write_cmos_sensor(ctx, 0x3760, 0x06);
	write_cmos_sensor(ctx, 0x3768, 0x1B);
	write_cmos_sensor(ctx, 0x3769, 0x1B);
	write_cmos_sensor(ctx, 0x376A, 0x1A);
	write_cmos_sensor(ctx, 0x376B, 0x19);
	write_cmos_sensor(ctx, 0x376C, 0x18);
	write_cmos_sensor(ctx, 0x376D, 0x14);
	write_cmos_sensor(ctx, 0x376E, 0x0F);
	write_cmos_sensor(ctx, 0x3776, 0x00);
	write_cmos_sensor(ctx, 0x3777, 0x00);
	write_cmos_sensor(ctx, 0x3778, 0x46);
	write_cmos_sensor(ctx, 0x3779, 0x00);
	write_cmos_sensor(ctx, 0x377A, 0x08);
	write_cmos_sensor(ctx, 0x377B, 0x01);
	write_cmos_sensor(ctx, 0x377C, 0x45);
	write_cmos_sensor(ctx, 0x377D, 0x01);
	write_cmos_sensor(ctx, 0x377E, 0x23);
	write_cmos_sensor(ctx, 0x377F, 0x02);
	write_cmos_sensor(ctx, 0x3780, 0xD9);
	write_cmos_sensor(ctx, 0x3781, 0x03);
	write_cmos_sensor(ctx, 0x3782, 0xF5);
	write_cmos_sensor(ctx, 0x3783, 0x06);
	write_cmos_sensor(ctx, 0x3784, 0xA5);
	write_cmos_sensor(ctx, 0x3788, 0x0F);
	write_cmos_sensor(ctx, 0x378A, 0xD9);
	write_cmos_sensor(ctx, 0x378B, 0x03);
	write_cmos_sensor(ctx, 0x378C, 0xEB);
	write_cmos_sensor(ctx, 0x378D, 0x05);
	write_cmos_sensor(ctx, 0x378E, 0x87);
	write_cmos_sensor(ctx, 0x378F, 0x06);
	write_cmos_sensor(ctx, 0x3790, 0xF5);
	write_cmos_sensor(ctx, 0x3792, 0x43);
	write_cmos_sensor(ctx, 0x3794, 0x7A);
	write_cmos_sensor(ctx, 0x3796, 0xA1);
	write_cmos_sensor(ctx, 0x37B0, 0x36);
	write_cmos_sensor(ctx, 0x3A01, 0x03);
	write_cmos_sensor(ctx, 0x3A04, 0x90);
	write_cmos_sensor(ctx, 0x3A05, 0x12);
	write_cmos_sensor(ctx, 0x3A18, 0x8F);
	write_cmos_sensor(ctx, 0x3A19, 0x00);
	write_cmos_sensor(ctx, 0x3A1A, 0x4F);
	write_cmos_sensor(ctx, 0x3A1B, 0x00);
	write_cmos_sensor(ctx, 0x3A1C, 0x47);
	write_cmos_sensor(ctx, 0x3A1D, 0x00);
	write_cmos_sensor(ctx, 0x3A1E, 0x37);
	write_cmos_sensor(ctx, 0x3A1F, 0x00);
	write_cmos_sensor(ctx, 0x3A20, 0x4F);
	write_cmos_sensor(ctx, 0x3A21, 0x00);
	write_cmos_sensor(ctx, 0x3A22, 0x87);
	write_cmos_sensor(ctx, 0x3A23, 0x00);
	write_cmos_sensor(ctx, 0x3A24, 0x4F);
	write_cmos_sensor(ctx, 0x3A25, 0x00);
	write_cmos_sensor(ctx, 0x3A26, 0x7F);
	write_cmos_sensor(ctx, 0x3A27, 0x00);
	write_cmos_sensor(ctx, 0x3A28, 0x3F);
	write_cmos_sensor(ctx, 0x3A29, 0x00);
	write_cmos_sensor(ctx, 0x3E04, 0x0E);
}

static void custom2_write_register(struct subdrv_ctx *ctx)
{
	write_cmos_sensor(ctx, 0x3000, 0x01);
	write_cmos_sensor(ctx, 0x3001, 0x00);
	write_cmos_sensor(ctx, 0x3002, 0x01);
	write_cmos_sensor(ctx, 0x3003, 0x00);
	write_cmos_sensor(ctx, 0x300C, 0x3B);
	write_cmos_sensor(ctx, 0x300D, 0x2A);
	write_cmos_sensor(ctx, 0x3018, 0x04);
	write_cmos_sensor(ctx, 0x302C, 0xFC);
	write_cmos_sensor(ctx, 0x302D, 0x03);
	write_cmos_sensor(ctx, 0x302E, 0x80);
	write_cmos_sensor(ctx, 0x302F, 0x07);
	write_cmos_sensor(ctx, 0x3030, 0xD7);
	write_cmos_sensor(ctx, 0x3031, 0x04);
	write_cmos_sensor(ctx, 0x3032, 0x00);
	write_cmos_sensor(ctx, 0x3034, 0xF4);
	write_cmos_sensor(ctx, 0x3035, 0x01);
	write_cmos_sensor(ctx, 0x3048, 0x01);
	write_cmos_sensor(ctx, 0x3049, 0x02);
	write_cmos_sensor(ctx, 0x304A, 0x02);
	write_cmos_sensor(ctx, 0x304B, 0x02);
	write_cmos_sensor(ctx, 0x304C, 0x13);
	write_cmos_sensor(ctx, 0x304E, 0x00);
	write_cmos_sensor(ctx, 0x304F, 0x00);
	write_cmos_sensor(ctx, 0x3050, 0x00);
	write_cmos_sensor(ctx, 0x3058, 0xC9);
	write_cmos_sensor(ctx, 0x3059, 0x12);
	write_cmos_sensor(ctx, 0x305A, 0x00);
	write_cmos_sensor(ctx, 0x305C, 0x0D);
	write_cmos_sensor(ctx, 0x305D, 0x00);
	write_cmos_sensor(ctx, 0x305E, 0x00);
	write_cmos_sensor(ctx, 0x3060, 0x2C);
	write_cmos_sensor(ctx, 0x3061, 0x00);
	write_cmos_sensor(ctx, 0x3062, 0x00);
	write_cmos_sensor(ctx, 0x3064, 0x09);
	write_cmos_sensor(ctx, 0x3065, 0x00);
	write_cmos_sensor(ctx, 0x3066, 0x00);
	write_cmos_sensor(ctx, 0x3068, 0x1F);
	write_cmos_sensor(ctx, 0x3069, 0x00);
	write_cmos_sensor(ctx, 0x306A, 0x00);
	write_cmos_sensor(ctx, 0x306C, 0x32);
	write_cmos_sensor(ctx, 0x306D, 0x00);
	write_cmos_sensor(ctx, 0x306E, 0x00);
	write_cmos_sensor(ctx, 0x3074, 0x00);
	write_cmos_sensor(ctx, 0x3075, 0x05);
	write_cmos_sensor(ctx, 0x3076, 0x34);
	write_cmos_sensor(ctx, 0x3077, 0x04);
	write_cmos_sensor(ctx, 0x3078, 0x02);
	write_cmos_sensor(ctx, 0x3079, 0x00);
	write_cmos_sensor(ctx, 0x307A, 0x00);
	write_cmos_sensor(ctx, 0x307B, 0x00);
	write_cmos_sensor(ctx, 0x3080, 0x02);
	write_cmos_sensor(ctx, 0x3081, 0x00);
	write_cmos_sensor(ctx, 0x3082, 0x00);
	write_cmos_sensor(ctx, 0x3083, 0x00);
	write_cmos_sensor(ctx, 0x3088, 0x02);
	write_cmos_sensor(ctx, 0x308E, 0x01);
	write_cmos_sensor(ctx, 0x308F, 0x05);
	write_cmos_sensor(ctx, 0x3090, 0x34);
	write_cmos_sensor(ctx, 0x3091, 0x04);
	write_cmos_sensor(ctx, 0x3094, 0x00);
	write_cmos_sensor(ctx, 0x3095, 0x00);
	write_cmos_sensor(ctx, 0x3096, 0x00);
	write_cmos_sensor(ctx, 0x309B, 0x02);
	write_cmos_sensor(ctx, 0x309C, 0x00);
	write_cmos_sensor(ctx, 0x309D, 0x00);
	write_cmos_sensor(ctx, 0x309E, 0x00);
	write_cmos_sensor(ctx, 0x30A4, 0x00);
	write_cmos_sensor(ctx, 0x30A5, 0x00);
	write_cmos_sensor(ctx, 0x30B6, 0x00);
	write_cmos_sensor(ctx, 0x30B7, 0x00);
	write_cmos_sensor(ctx, 0x30C6, 0x12);
	write_cmos_sensor(ctx, 0x30C7, 0x00);
	write_cmos_sensor(ctx, 0x30CE, 0x64);
	write_cmos_sensor(ctx, 0x30CF, 0x00);
	write_cmos_sensor(ctx, 0x30D8, 0x38);
	write_cmos_sensor(ctx, 0x30D9, 0x0E);
	write_cmos_sensor(ctx, 0x30E8, 0x00);
	write_cmos_sensor(ctx, 0x30E9, 0x00);
	write_cmos_sensor(ctx, 0x30EA, 0x00);
	write_cmos_sensor(ctx, 0x30EB, 0x00);
	write_cmos_sensor(ctx, 0x30EC, 0x00);
	write_cmos_sensor(ctx, 0x30ED, 0x00);
	write_cmos_sensor(ctx, 0x30EE, 0x00);
	write_cmos_sensor(ctx, 0x30EF, 0x00);
	write_cmos_sensor(ctx, 0x3116, 0x08);
	write_cmos_sensor(ctx, 0x3117, 0x00);
	write_cmos_sensor(ctx, 0x314C, 0x29);
	write_cmos_sensor(ctx, 0x314D, 0x01);
	write_cmos_sensor(ctx, 0x315A, 0x02);
	write_cmos_sensor(ctx, 0x3168, 0xA0);
	write_cmos_sensor(ctx, 0x316A, 0x7E);
	write_cmos_sensor(ctx, 0x3199, 0x00);
	write_cmos_sensor(ctx, 0x319D, 0x00);
	write_cmos_sensor(ctx, 0x319E, 0x00);
	write_cmos_sensor(ctx, 0x319F, 0x03);
	write_cmos_sensor(ctx, 0x31A0, 0x2A);
	write_cmos_sensor(ctx, 0x31A1, 0x00);
	write_cmos_sensor(ctx, 0x31A4, 0x00);
	write_cmos_sensor(ctx, 0x31A5, 0x00);
	write_cmos_sensor(ctx, 0x31A6, 0x00);
	write_cmos_sensor(ctx, 0x31A8, 0x00);
	write_cmos_sensor(ctx, 0x31AC, 0x00);
	write_cmos_sensor(ctx, 0x31AD, 0x00);
	write_cmos_sensor(ctx, 0x31AE, 0x00);
	write_cmos_sensor(ctx, 0x31D4, 0x00);
	write_cmos_sensor(ctx, 0x31D5, 0x00);
	write_cmos_sensor(ctx, 0x31D7, 0x03);
	write_cmos_sensor(ctx, 0x31DD, 0x03);
	write_cmos_sensor(ctx, 0x31E4, 0x01);
	write_cmos_sensor(ctx, 0x31E8, 0x00);
	write_cmos_sensor(ctx, 0x31F3, 0x00);
	write_cmos_sensor(ctx, 0x3200, 0x10);
	write_cmos_sensor(ctx, 0x3288, 0x21);
	write_cmos_sensor(ctx, 0x328A, 0x02);
	write_cmos_sensor(ctx, 0x3300, 0x00);
	write_cmos_sensor(ctx, 0x3302, 0x32);
	write_cmos_sensor(ctx, 0x3303, 0x00);
	write_cmos_sensor(ctx, 0x3308, 0x34);
	write_cmos_sensor(ctx, 0x3309, 0x04);
	write_cmos_sensor(ctx, 0x3414, 0x05);
	write_cmos_sensor(ctx, 0x3416, 0x18);
	write_cmos_sensor(ctx, 0x341C, 0xFF);
	write_cmos_sensor(ctx, 0x341D, 0x01);
	write_cmos_sensor(ctx, 0x35AC, 0x0E);
	write_cmos_sensor(ctx, 0x3648, 0x01);
	write_cmos_sensor(ctx, 0x364A, 0x04);
	write_cmos_sensor(ctx, 0x364C, 0x04);
	write_cmos_sensor(ctx, 0x3678, 0x01);
	write_cmos_sensor(ctx, 0x367C, 0x31);
	write_cmos_sensor(ctx, 0x367E, 0x31);
	write_cmos_sensor(ctx, 0x3708, 0x02);
	write_cmos_sensor(ctx, 0x3714, 0x01);
	write_cmos_sensor(ctx, 0x3715, 0x02);
	write_cmos_sensor(ctx, 0x3716, 0x02);
	write_cmos_sensor(ctx, 0x3717, 0x02);
	write_cmos_sensor(ctx, 0x371C, 0x3D);
	write_cmos_sensor(ctx, 0x371D, 0x3F);
	write_cmos_sensor(ctx, 0x372C, 0x00);
	write_cmos_sensor(ctx, 0x372D, 0x00);
	write_cmos_sensor(ctx, 0x372E, 0x46);
	write_cmos_sensor(ctx, 0x372F, 0x00);
	write_cmos_sensor(ctx, 0x3730, 0x89);
	write_cmos_sensor(ctx, 0x3731, 0x00);
	write_cmos_sensor(ctx, 0x3732, 0x08);
	write_cmos_sensor(ctx, 0x3733, 0x01);
	write_cmos_sensor(ctx, 0x3734, 0xFE);
	write_cmos_sensor(ctx, 0x3735, 0x05);
	write_cmos_sensor(ctx, 0x375D, 0x00);
	write_cmos_sensor(ctx, 0x375E, 0x00);
	write_cmos_sensor(ctx, 0x375F, 0x61);
	write_cmos_sensor(ctx, 0x3760, 0x06);
	write_cmos_sensor(ctx, 0x3768, 0x1B);
	write_cmos_sensor(ctx, 0x3769, 0x1B);
	write_cmos_sensor(ctx, 0x376A, 0x1A);
	write_cmos_sensor(ctx, 0x376B, 0x19);
	write_cmos_sensor(ctx, 0x376C, 0x18);
	write_cmos_sensor(ctx, 0x376D, 0x14);
	write_cmos_sensor(ctx, 0x376E, 0x0F);
	write_cmos_sensor(ctx, 0x3776, 0x00);
	write_cmos_sensor(ctx, 0x3777, 0x00);
	write_cmos_sensor(ctx, 0x3778, 0x46);
	write_cmos_sensor(ctx, 0x3779, 0x00);
	write_cmos_sensor(ctx, 0x377A, 0x08);
	write_cmos_sensor(ctx, 0x377B, 0x01);
	write_cmos_sensor(ctx, 0x377C, 0x45);
	write_cmos_sensor(ctx, 0x377D, 0x01);
	write_cmos_sensor(ctx, 0x377E, 0x23);
	write_cmos_sensor(ctx, 0x377F, 0x02);
	write_cmos_sensor(ctx, 0x3780, 0xD9);
	write_cmos_sensor(ctx, 0x3781, 0x03);
	write_cmos_sensor(ctx, 0x3782, 0xF5);
	write_cmos_sensor(ctx, 0x3783, 0x06);
	write_cmos_sensor(ctx, 0x3784, 0xA5);
	write_cmos_sensor(ctx, 0x3788, 0x0F);
	write_cmos_sensor(ctx, 0x378A, 0xD9);
	write_cmos_sensor(ctx, 0x378B, 0x03);
	write_cmos_sensor(ctx, 0x378C, 0xEB);
	write_cmos_sensor(ctx, 0x378D, 0x05);
	write_cmos_sensor(ctx, 0x378E, 0x87);
	write_cmos_sensor(ctx, 0x378F, 0x06);
	write_cmos_sensor(ctx, 0x3790, 0xF5);
	write_cmos_sensor(ctx, 0x3792, 0x43);
	write_cmos_sensor(ctx, 0x3794, 0x7A);
	write_cmos_sensor(ctx, 0x3796, 0xA1);
	write_cmos_sensor(ctx, 0x37B0, 0x36);
	write_cmos_sensor(ctx, 0x3A01, 0x03);
	write_cmos_sensor(ctx, 0x3A04, 0x90);
	write_cmos_sensor(ctx, 0x3A05, 0x12);
	write_cmos_sensor(ctx, 0x3A18, 0xB7);
	write_cmos_sensor(ctx, 0x3A19, 0x00);
	write_cmos_sensor(ctx, 0x3A1A, 0x67);
	write_cmos_sensor(ctx, 0x3A1B, 0x00);
	write_cmos_sensor(ctx, 0x3A1C, 0x6F);
	write_cmos_sensor(ctx, 0x3A1D, 0x00);
	write_cmos_sensor(ctx, 0x3A1E, 0xDF);
	write_cmos_sensor(ctx, 0x3A1F, 0x01);
	write_cmos_sensor(ctx, 0x3A20, 0x6F);
	write_cmos_sensor(ctx, 0x3A21, 0x00);
	write_cmos_sensor(ctx, 0x3A22, 0xCF);
	write_cmos_sensor(ctx, 0x3A23, 0x00);
	write_cmos_sensor(ctx, 0x3A24, 0x6F);
	write_cmos_sensor(ctx, 0x3A25, 0x00);
	write_cmos_sensor(ctx, 0x3A26, 0xB7);
	write_cmos_sensor(ctx, 0x3A27, 0x00);
	write_cmos_sensor(ctx, 0x3A28, 0x5F);
	write_cmos_sensor(ctx, 0x3A29, 0x00);
	write_cmos_sensor(ctx, 0x3E04, 0x0E);
}

kal_uint16 addr_data_pair_preview_imx334sub[] = {
0x3000, 0x01,
0x3001, 0x00,
0x3002, 0x01,
0x3003, 0x00,
0x300c, 0x3B,
0x300D, 0x2A,
0x3030, 0xCA,
0x3031, 0x08,
0x3032, 0x00,
0x3034, 0x4C,
0x3035, 0x04,
0x3050, 0x00,
0x3058, 0x05,
0x3059, 0x00,
0x305A, 0x00,
0x314C, 0xC6,
0x314D, 0x00,
0x315A, 0x02,
0x3168, 0xA0,
0x316A, 0x7E,
0x319D, 0x00,
0x319E, 0x01,
0x31A1, 0x00,
0x3288, 0x21,
0x328A, 0x02,
0x3414, 0x05,
0x3416, 0x18,
0x341C, 0xFF,
0x341D, 0x01,
0x35AC, 0x0E,
0x3648, 0x01,
0x364A, 0x04,
0x364C, 0x04,
0x3678, 0x01,
0x367C, 0x31,
0x367E, 0x31,
0x3708, 0x02,
0x3714, 0x01,
0x3715, 0x02,
0x3716, 0x02,
0x3717, 0x02,
0x371C, 0x3D,
0x371D, 0x3F,
0x372C, 0x00,
0x372D, 0x00,
0x372E, 0x46,
0x372F, 0x00,
0x3730, 0x89,
0x3731, 0x00,
0x3732, 0x08,
0x3733, 0x01,
0x3734, 0xFE,
0x3735, 0x05,
0x375D, 0x00,
0x375E, 0x00,
0x375F, 0x61,
0x3760, 0x06,
0x3768, 0x1B,
0x3769, 0x1B,
0x376A, 0x1A,
0x376B, 0x19,
0x376C, 0x18,
0x376D, 0x14,
0x376E, 0x0F,
0x3776, 0x00,
0x3777, 0x00,
0x3778, 0x46,
0x3779, 0x00,
0x377A, 0x08,
0x377B, 0x01,
0x377C, 0x45,
0x377D, 0x01,
0x377E, 0x23,
0x377F, 0x02,
0x3780, 0xD9,
0x3781, 0x03,
0x3782, 0xF5,
0x3783, 0x06,
0x3784, 0xA5,
0x3788, 0x0F,
0x378A, 0xD9,
0x378B, 0x03,
0x378C, 0xEB,
0x378D, 0x05,
0x378E, 0x87,
0x378F, 0x06,
0x3790, 0xF5,
0x3792, 0x43,
0x3794, 0x7A,
0x3796, 0xA1,
0x3A18, 0x8F,
0x3A1A, 0x4F,
0x3A1C, 0x47,
0x3A1E, 0x37,
0x3A20, 0x4F,
0x3A22, 0x87,
0x3A24, 0x4F,
0x3A26, 0x7F,
0x3A28, 0x3F,
0x3E04, 0x0E,
};

static void preview_setting(struct subdrv_ctx *ctx)
{
	LOG_INF("E\n");
	//imx334sub_table_write_cmos_sensor(ctx, addr_data_pair_preview_imx334sub,
	//sizeof(addr_data_pair_preview_imx334sub) / sizeof(kal_uint16));
	sensor_init(ctx);
	/* zvhdr_setting(ctx); */
}				/*    preview_setting  */

kal_uint16 addr_data_pair_custom1_imx334sub[] = {
0x300C, 0x3B,
0x300D, 0x2A,
0x3048, 0x01,
0x3049, 0x01,
0x304A, 0x01,
0x304B, 0x02,
0x304C, 0x13,
0x3058, 0x0E,
0x3059, 0x11,
0x3068, 0x19,
0x314C, 0x29,
0x314D, 0x01,
0x315A, 0x02,
0x3168, 0xA0,
0x316A, 0x7E,
0x31A1, 0x00,
0x31D7, 0x01,
0x3200, 0x10,
0x3288, 0x21,
0x328A, 0x02,
0x3414, 0x05,
0x3416, 0x18,
0x35AC, 0x0E,
0x3648, 0x01,
0x364A, 0x04,
0x364C, 0x04,
0x3678, 0x01,
0x367C, 0x31,
0x367E, 0x31,
0x3708, 0x02,
0x3714, 0x01,
0x3715, 0x02,
0x3716, 0x02,
0x3717, 0x02,
0x371C, 0x3D,
0x371D, 0x3F,
0x372C, 0x00,
0x372D, 0x00,
0x372E, 0x46,
0x372F, 0x00,
0x3730, 0x89,
0x3731, 0x00,
0x3732, 0x08,
0x3733, 0x01,
0x3734, 0xFE,
0x3735, 0x05,
0x375D, 0x00,
0x375E, 0x00,
0x375F, 0x61,
0x3760, 0x06,
0x3768, 0x1B,
0x3769, 0x1B,
0x376A, 0x1A,
0x376B, 0x19,
0x376C, 0x18,
0x376D, 0x14,
0x376E, 0x0F,
0x3776, 0x00,
0x3777, 0x00,
0x3778, 0x46,
0x3779, 0x00,
0x377A, 0x08,
0x377B, 0x01,
0x377C, 0x45,
0x377D, 0x01,
0x377E, 0x23,
0x377F, 0x02,
0x3780, 0xD9,
0x3781, 0x03,
0x3782, 0xF5,
0x3783, 0x06,
0x3784, 0xA5,
0x3788, 0x0F,
0x378A, 0xD9,
0x378B, 0x03,
0x378C, 0xEB,
0x378D, 0x05,
0x378E, 0x87,
0x378F, 0x06,
0x3790, 0xF5,
0x3792, 0x43,
0x3794, 0x7A,
0x3796, 0xA1,
0x3E04, 0x0E,
};
#if HDR_raw12
kal_uint16 addr_data_pair_custom2_imx334sub[] = {
0x300C, 0x3B,
0x300D, 0x2A,
0x3018, 0x04,
0x302C, 0xFC,
0x302D, 0x03,
0x302E, 0x80,
0x302F, 0x07,
0x3030, 0x65,
0x3031, 0x04,
0x3048, 0x01,
0x3049, 0x02,
0x304A, 0x02,
0x304B, 0x02,
0x304C, 0x13,
0x3058, 0x0D,
0x3059, 0x11,
0x305C, 0x0D,
0x3060, 0x26,
0x3068, 0x19,
0x306C, 0x2C,
0x306D, 0x00,
0x3074, 0x00,
0x3075, 0x05,
0x3076, 0x34,
0x3077, 0x04,
0x308E, 0x01,
0x308F, 0x05,
0x3090, 0x34,
0x3091, 0x04,
0x30C6, 0x12,
0x30CE, 0x64,
0x30D8, 0x38,
0x30D9, 0x0E,
0x314C, 0x29,
0x314D, 0x01,
0x315A, 0x02,
0x3168, 0xA0,
0x316A, 0x7E,
0x31A1, 0x00,
0x31D7, 0x03,
0x3200, 0x10,
0x3288, 0x21,
0x328A, 0x02,
0x3308, 0x34,
0x3309, 0x04,
0x3414, 0x05,
0x3416, 0x18,
0x35AC, 0x0E,
0x3648, 0x01,
0x364A, 0x04,
0x364C, 0x04,
0x3678, 0x01,
0x367C, 0x31,
0x367E, 0x31,
0x3708, 0x02,
0x3714, 0x01,
0x3715, 0x02,
0x3716, 0x02,
0x3717, 0x02,
0x371C, 0x3D,
0x371D, 0x3F,
0x372C, 0x00,
0x372D, 0x00,
0x372E, 0x46,
0x372F, 0x00,
0x3730, 0x89,
0x3731, 0x00,
0x3732, 0x08,
0x3733, 0x01,
0x3734, 0xFE,
0x3735, 0x05,
0x375D, 0x00,
0x375E, 0x00,
0x375F, 0x61,
0x3760, 0x06,
0x3768, 0x1B,
0x3769, 0x1B,
0x376A, 0x1A,
0x376B, 0x19,
0x376C, 0x18,
0x376D, 0x14,
0x376E, 0x0F,
0x3776, 0x00,
0x3777, 0x00,
0x3778, 0x46,
0x3779, 0x00,
0x377A, 0x08,
0x377B, 0x01,
0x377C, 0x45,
0x377D, 0x01,
0x377E, 0x23,
0x377F, 0x02,
0x3780, 0xD9,
0x3781, 0x03,
0x3782, 0xF5,
0x3783, 0x06,
0x3784, 0xA5,
0x3788, 0x0F,
0x378A, 0xD9,
0x378B, 0x03,
0x378C, 0xEB,
0x378D, 0x05,
0x378E, 0x87,
0x378F, 0x06,
0x3790, 0xF5,
0x3792, 0x43,
0x3794, 0x7A,
0x3796, 0xA1,
0x3E04, 0x0E,
/* above is raw10 setting */
0x3000, 0x01,
0x3001, 0x00,
0x3002, 0x01,
0x3003, 0x00,
0x300C, 0x3B,
0x300D, 0x2A,
0x3018, 0x04,
0x302C, 0xFC,
0x302D, 0x03,
0x302E, 0x80,
0x302F, 0x07,
0x3030, 0xD7,
0x3031, 0x04,
0x3032, 0x00,
0x3034, 0xF4,
0x3035, 0x01,
0x3048, 0x01,
0x3049, 0x02,
0x304A, 0x02,
0x304B, 0x02,
0x304C, 0x13,
0x304E, 0x00,
0x304F, 0x00,
0x3050, 0x00,
0x3058, 0xC9,
0x3059, 0x12,
0x305A, 0x00,
0x305C, 0x0D,
0x305D, 0x00,
0x305E, 0x00,
0x3060, 0x2C,
0x3061, 0x00,
0x3062, 0x00,
0x3064, 0x09,
0x3065, 0x00,
0x3066, 0x00,
0x3068, 0x1F,
0x3069, 0x00,
0x306A, 0x00,
0x306C, 0x32,
0x306D, 0x00,
0x306E, 0x00,
0x3074, 0x00,
0x3075, 0x05,
0x3076, 0x34,
0x3077, 0x04,
0x3078, 0x02,
0x3079, 0x00,
0x307A, 0x00,
0x307B, 0x00,
0x3080, 0x02,
0x3081, 0x00,
0x3082, 0x00,
0x3083, 0x00,
0x3088, 0x02,
0x308E, 0x01,
0x308F, 0x05,
0x3090, 0x34,
0x3091, 0x04,
0x3094, 0x00,
0x3095, 0x00,
0x3096, 0x00,
0x309B, 0x02,
0x309C, 0x00,
0x309D, 0x00,
0x309E, 0x00,
0x30A4, 0x00,
0x30A5, 0x00,
0x30B6, 0x00,
0x30B7, 0x00,
0x30C6, 0x12,
0x30C7, 0x00,
0x30CE, 0x64,
0x30CF, 0x00,
0x30D8, 0x38,
0x30D9, 0x0E,
0x30E8, 0x00,
0x30E9, 0x00,
0x30EA, 0x00,
0x30EB, 0x00,
0x30EC, 0x00,
0x30ED, 0x00,
0x30EE, 0x00,
0x30EF, 0x00,
0x3116, 0x08,
0x3117, 0x00,
0x314C, 0x29,
0x314D, 0x01,
0x315A, 0x02,
0x3168, 0xA0,
0x316A, 0x7E,
0x3199, 0x00,
0x319D, 0x00,
0x319E, 0x00,
0x319F, 0x03,
0x31A0, 0x2A,
0x31A1, 0x00,
0x31A4, 0x00,
0x31A5, 0x00,
0x31A6, 0x00,
0x31A8, 0x00,
0x31AC, 0x00,
0x31AD, 0x00,
0x31AE, 0x00,
0x31D4, 0x00,
0x31D5, 0x00,
0x31D7, 0x03,
0x31DD, 0x03,
0x31E4, 0x01,
0x31E8, 0x00,
0x31F3, 0x00,
0x3200, 0x10,
0x3288, 0x21,
0x328A, 0x02,
0x3300, 0x00,
0x3302, 0x32,
0x3303, 0x00,
0x3308, 0x34,
0x3309, 0x04,
0x3414, 0x05,
0x3416, 0x18,
0x341C, 0xFF,
0x341D, 0x01,
0x35AC, 0x0E,
0x3648, 0x01,
0x364A, 0x04,
0x364C, 0x04,
0x3678, 0x01,
0x367C, 0x31,
0x367E, 0x31,
0x3708, 0x02,
0x3714, 0x01,
0x3715, 0x02,
0x3716, 0x02,
0x3717, 0x02,
0x371C, 0x3D,
0x371D, 0x3F,
0x372C, 0x00,
0x372D, 0x00,
0x372E, 0x46,
0x372F, 0x00,
0x3730, 0x89,
0x3731, 0x00,
0x3732, 0x08,
0x3733, 0x01,
0x3734, 0xFE,
0x3735, 0x05,
0x375D, 0x00,
0x375E, 0x00,
0x375F, 0x61,
0x3760, 0x06,
0x3768, 0x1B,
0x3769, 0x1B,
0x376A, 0x1A,
0x376B, 0x19,
0x376C, 0x18,
0x376D, 0x14,
0x376E, 0x0F,
0x3776, 0x00,
0x3777, 0x00,
0x3778, 0x46,
0x3779, 0x00,
0x377A, 0x08,
0x377B, 0x01,
0x377C, 0x45,
0x377D, 0x01,
0x377E, 0x23,
0x377F, 0x02,
0x3780, 0xD9,
0x3781, 0x03,
0x3782, 0xF5,
0x3783, 0x06,
0x3784, 0xA5,
0x3788, 0x0F,
0x378A, 0xD9,
0x378B, 0x03,
0x378C, 0xEB,
0x378D, 0x05,
0x378E, 0x87,
0x378F, 0x06,
0x3790, 0xF5,
0x3792, 0x43,
0x3794, 0x7A,
0x3796, 0xA1,
0x37B0, 0x36,
0x3A01, 0x03,
0x3A04, 0x90,
0x3A05, 0x12,
0x3A18, 0xB7,
0x3A19, 0x00,
0x3A1A, 0x67,
0x3A1B, 0x00,
0x3A1C, 0x6F,
0x3A1D, 0x00,
0x3A1E, 0xDF,
0x3A1F, 0x01,
0x3A20, 0x6F,
0x3A21, 0x00,
0x3A22, 0xCF,
0x3A23, 0x00,
0x3A24, 0x6F,
0x3A25, 0x00,
0x3A26, 0xB7,
0x3A27, 0x00,
0x3A28, 0x5F,
0x3A29, 0x00,
0x3E04, 0x0E,
/* above is raw12 setting */
};
#endif
static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable) {
		write_cmos_sensor(ctx, 0x3000, 0x00);
		/* sony spec:  wait for internal regulator stabilization */
		mDELAY(20);
		write_cmos_sensor(ctx, 0x3002, 0x00);
	} else {
		/* for debug */
		write_cmos_sensor(ctx, 0x3000, 0x01);
		write_cmos_sensor(ctx, 0x3002, 0x01);
		write_cmos_sensor(ctx, 0x3004, 0x04);
		write_cmos_sensor(ctx, 0x3004, 0x00);
		mDELAY(20);
	}
	return ERROR_NONE;
}

static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d hdr:%d\n",
		currefps, ctx->hdr_mode);
	capture_write_register(ctx);
/*
 *	imx334sub_table_write_cmos_sensor(ctx, addr_data_pair_capture_imx334,
 *	sizeof(addr_data_pair_capture_imx334) / sizeof(kal_uint16));
 */
}

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
	LOG_INF("E! %s:%d\n", __func__, currefps);
	capture_write_register(ctx);
}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	capture_write_register(ctx);
	imx334sub_table_write_cmos_sensor(ctx, addr_data_pair_preview_imx334sub,
	sizeof(addr_data_pair_preview_imx334sub) / sizeof(kal_uint16));
}

static void custom1_setting(struct subdrv_ctx *ctx)
{
	imx334sub_table_write_cmos_sensor(ctx, addr_data_pair_custom1_imx334sub,
	sizeof(addr_data_pair_custom1_imx334sub) / sizeof(kal_uint16));
}

static void custom2_setting(struct subdrv_ctx *ctx)
{
	custom2_write_register(ctx);
	//imx334_table_write_cmos_sensor(ctx, addr_data_pair_custom2_imx334sub,
	//sizeof(addr_data_pair_custom2_imx334sub) / sizeof(kal_uint16));
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(ctx, 0x3148, 0x10);
		write_cmos_sensor(ctx, 0x3280, 0x00);
		write_cmos_sensor(ctx, 0x329C, 0x01);
		write_cmos_sensor(ctx, 0x329E, 0x0A); /* Horizontal color-bar */
		write_cmos_sensor(ctx, 0x32A0, 0x11); /* 240 pixel */
		write_cmos_sensor(ctx, 0x3302, 0x01); /* black level offset */
		write_cmos_sensor(ctx, 0x336C, 0x00);
	} else {
		write_cmos_sensor(ctx, 0x3148, 0x00);
		write_cmos_sensor(ctx, 0x3280, 0x01);
		write_cmos_sensor(ctx, 0x329C, 0x00);
		write_cmos_sensor(ctx, 0x329E, 0x00); /* Horizontal color-bar */
		write_cmos_sensor(ctx, 0x32A0, 0x10);
		write_cmos_sensor(ctx, 0x3302, 0x32); /* black level offset */
		write_cmos_sensor(ctx, 0x336C, 0x01);
	}
	ctx->test_pattern = enable;
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *    get_imgsensor_id
 *
 * DESCRIPTION
 *    This function get the sensor ID
 *
 * PARAMETERS
 *    *sensorID : return the sensor ID
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id = return_sensor_id(ctx);
			/* return_sensor_id(); */
			if (*sensor_id == imgsensor_info.sensor_id) {
				//imx334_read_SPC(ctx, imx334_SPC_data);
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF(
				"Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				ctx->i2c_write_id, *sensor_id);
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


/*************************************************************************
 * FUNCTION
 *    open
 *
 * DESCRIPTION
 *    This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int open(struct subdrv_ctx *ctx)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 * we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = return_sensor_id(ctx);
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF(
					"i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			LOG_INF(
				"Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				ctx->i2c_write_id, sensor_id);
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
	//sensor_init(ctx);

	ctx->autoflicker_en = KAL_FALSE;
	ctx->sensor_mode = IMGSENSOR_MODE_INIT;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->hdr_mode = 0;
	ctx->test_pattern = KAL_FALSE;
	ctx->current_fps = imgsensor_info.pre.max_framerate;


	return ERROR_NONE;
} /* open */

/*************************************************************************
 * FUNCTION
 *    close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int close(struct subdrv_ctx *ctx)
{
	write_cmos_sensor(ctx, 0x3000, 0x01);	/*stream off */
	return ERROR_NONE;
}				/*    close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *    This function start the sensor preview.
 *
 * PARAMETERS
 *    *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	/* ctx->video_mode = KAL_FALSE; */
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	preview_setting(ctx);

	return ERROR_NONE;
}				/*    preview   */

/*************************************************************************
 * FUNCTION
 *    capture
 *
 * DESCRIPTION
 *    This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;
	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	capture_setting(ctx, ctx->current_fps);	/*Full mode */


	return ERROR_NONE;
}				/* capture(ctx) */

static kal_uint32 normal_video(struct subdrv_ctx *ctx,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{


	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	/* ctx->current_fps = 300; */
	ctx->autoflicker_en = KAL_FALSE;

	normal_video_setting(ctx, ctx->current_fps);


	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
}				/*    normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	/* ctx->video_mode = KAL_TRUE; */
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;

	hs_video_setting(ctx);
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    hs_video   */

static kal_uint32 custom1(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;
	custom1_setting(ctx);

	return ERROR_NONE;
}				/*    Custom1  staggered HDR    */

static kal_uint32 custom2(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	ctx->pclk = imgsensor_info.custom2.pclk;
	ctx->line_length = imgsensor_info.custom2.linelength;
	ctx->frame_length = imgsensor_info.custom2.framelength;
	ctx->min_frame_length = imgsensor_info.custom2.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	ctx->autoflicker_en = KAL_FALSE;

	custom2_setting(ctx);

	return ERROR_NONE;
}				/*    Custom2  staggered HDR    */

static int get_resolution(
		struct subdrv_ctx *ctx,
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
}				/*    get_resolution    */

static int get_info(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	/*LOG_INF("scenario_id = %d\n", scenario_id); */

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

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;

	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

/* PDAF_SUPPORT_CAMSV; */
/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode */
	sensor_info->PDAF_Support = 0;
#if defined(imx334_ZHDR)
	/* 3; */ /*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR */
	sensor_info->HDR_Support = 2;
	/*0: no support, 1: G0,R0.B0, 2: G0,R0.B1, 3: G0,R1.B0, 4: G0,R1.B1 */
	/*5: G1,R0.B0, 6: G1,R0.B1, 7: G1,R1.B0, 8: G1,R1.B1 */
	sensor_info->ZHDR_Mode = 8;
#endif
	sensor_info->HDR_Support = 6;	/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR */
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	return ERROR_NONE;
}				/*    get_info  */

static int control(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {
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
	case SENSOR_SCENARIO_ID_CUSTOM1:
		custom1(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		custom2(ctx, image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control(ctx) */

static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{				/* This Function not used after ROME */
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;

	ctx->current_fps = framerate;
	set_max_framerate(ctx, ctx->current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx,
	kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	if (enable)		/* enable auto flicker */
		ctx->autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		ctx->autoflicker_en = KAL_FALSE;
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		frame_length =
		    imgsensor_info.pre.pclk /
		    framerate * 10 / imgsensor_info.pre.linelength;
		ctx->dummy_line =
		    (frame_length > imgsensor_info.pre.framelength)
		    ? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
		    imgsensor_info.normal_video.pclk /
		    framerate * 10 / imgsensor_info.normal_video.linelength;

		ctx->dummy_line =
		(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength) : 0;

		ctx->frame_length =
		 imgsensor_info.normal_video.framelength + ctx->dummy_line;

		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		if (ctx->current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			     framerate, imgsensor_info.cap.max_framerate / 10);

		frame_length = imgsensor_info.cap.pclk /
			framerate * 10 / imgsensor_info.cap.linelength;
		ctx->dummy_line =
		    (frame_length > imgsensor_info.cap.framelength)
		    ? (frame_length - imgsensor_info.cap.framelength) : 0;
		ctx->frame_length =
		    imgsensor_info.cap.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk /
		    framerate * 10 / imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
		    (frame_length > imgsensor_info.hs_video.framelength)
		    ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		ctx->frame_length =
		    imgsensor_info.hs_video.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / 2 /
			framerate * 10 / imgsensor_info.custom1.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom1.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / 4 /
			framerate * 10 / imgsensor_info.custom2.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom2.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		break;
	default:		/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk /
		    framerate * 10 / imgsensor_info.pre.linelength;
		ctx->dummy_line =
		    (frame_length > imgsensor_info.pre.framelength)
		    ? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		set_dummy(ctx);
		LOG_INF("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	/*LOG_INF("scenario_id = %d\n", scenario_id); */

	switch (scenario_id) {
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
	case SENSOR_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;

	default:
		break;
	}

	return ERROR_NONE;
}

static int feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	//unsigned long long *feature_return_para =
	//(unsigned long long *) feature_para;
	kal_uint32 rate;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	//struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	//SET_SENSOR_AWB_GAIN *pSetSensorAWB =
	//(SET_SENSOR_AWB_GAIN *)feature_para;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);

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
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				imgsensor_info.sensor_output_dataformat;
			break;
		}
	break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
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
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = ctx->line_length;
		*feature_return_para_16 = ctx->frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		LOG_INF("feature_Control ctx->pclk = %d, ctx->current_fps = %d\n",
			ctx->pclk, ctx->current_fps);
		*feature_return_para_32 = ctx->pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain(ctx, (UINT32)*feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(ctx, sensor_reg_data->RegAddr,
			sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(ctx, sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		// get the lens driver ID from EEPROM or just
		// return LENS_DRIVER_ID_DO_NOT_CARE
		// if EEPROM does not exist in camera module.
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
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		//for factory mode auto testing
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32)*feature_data);
		ctx->current_fps = *feature_data;
		break;
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
		ctx->ihdr_mode = *feature_data;
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));

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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
			(UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			rate = imgsensor_info.cap.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			rate = imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			rate = imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			rate = imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			rate = imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			rate = imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			rate = imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data);
		streaming_control(ctx, KAL_TRUE);
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
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
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t) (*(feature_data + 1))
			= HDR_RAW_STAGGER_3EXP;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t) (*(feature_data + 1))
			= HDR_RAW_STAGGER_2EXP;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		default:
			*(MUINT32 *)(uintptr_t) (*(feature_data + 1))
			= HDR_NONE;
			// other scenario do not support HDR
			break;
		}
		LOG_INF(
			"SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu, HDR:%llu\n",
			*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		(*(feature_data + 1)) = 1;
		(*(feature_data + 2)) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME:
		if (*feature_data == SENSOR_SCENARIO_ID_CUSTOM1) {
			switch (*(feature_data + 1)) {
			case VC_STAGGER_NE:
					*(feature_data + 2) = 0xffff;
					break;
			case VC_STAGGER_ME:
					*(feature_data + 2) = 0xffff;
					break;
			case VC_STAGGER_SE:
					*(feature_data + 2) = 0xffff;
					break;
			default:
					*(feature_data + 2) = 0xffff;
					break;
			}
		} else if (*feature_data == SENSOR_SCENARIO_ID_CUSTOM2) {
			switch (*(feature_data + 1)) {
			case VC_STAGGER_NE:
					*(feature_data + 2) = 0xffff;
					break;
			case VC_STAGGER_ME:
					*(feature_data + 2) = 0xffff;
					break;
			case VC_STAGGER_SE:
					*(feature_data + 2) = 0xffff;
					break;
			default:
					*(feature_data + 2) = 0xffff;
					break;
			}
		} else {
			*(feature_data + 2) = 0;
		}
		break;
	case SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO:
		if (*feature_data == SENSOR_SCENARIO_ID_NORMAL_PREVIEW) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM2;
				break;
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM1;
				break;
			default:
				break;
			}
		}
		if (*feature_data == SENSOR_SCENARIO_ID_NORMAL_VIDEO) {
			switch (*(feature_data + 1)) {
			case HDR_RAW_STAGGER_3EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM2;
				break;
			case HDR_RAW_STAGGER_2EXP:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_CUSTOM1;
				break;
			default:
				break;
			}
		}
		if (*feature_data == SENSOR_SCENARIO_ID_CUSTOM1) {
			switch (*(feature_data + 1)) {
			case HDR_NONE:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
				break;
			default:
				break;
			}
		}
		if (*feature_data == SENSOR_SCENARIO_ID_CUSTOM2) {
			switch (*(feature_data + 1)) {
			case HDR_NONE:
				*(feature_data + 2) = SENSOR_SCENARIO_ID_NORMAL_VIDEO;
				break;
			default:
				break;
			}
		}
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER: // for 2EXP
		LOG_INF(
			"SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, ME=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		// implement write shutter for NE/ME/SE
		hdr_write_shutter(ctx, (UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN:
		LOG_INF(
			"SENSOR_FEATURE_SET_DUAL_GAIN LG=%d, SG=%d\n",
			(UINT16)*(feature_data), (UINT16) *(feature_data + 1));
		hdr_write_gain(ctx, (UINT16)*feature_data,
				(UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER: // for 3EXP
		LOG_INF(
			"SENSOR_FEATURE_SET_HDR_TRI_SHUTTER LE=%d, ME=%d, SE=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		//NE ME SE feature_data_check
		if (((UINT16)*(feature_data+2) > 1) &&
			((UINT16)*(feature_data+1) > 1) &&
			((UINT16)*(feature_data) > 1))
			hdr_write_tri_shutter(ctx, (UINT16)*feature_data,
				(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
		else
			LOG_INF("Value Violation : feature_data<1");
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_GAIN: // for 3EXP
		LOG_INF(
			"SENSOR_FEATURE_SET_HDR_TRI_GAIN LGain=%d, SGain=%d, MGain=%d\n",
			(UINT16) *feature_data,
			(UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
		//LGain SGain MGain feature_data_check
		if (((UINT16)*(feature_data+2) > 1) &&
			((UINT16)*(feature_data+1) > 1) &&
			((UINT16)*(feature_data) > 1))
			hdr_write_tri_gain(ctx, (UINT16)*feature_data,
				(UINT16)*(feature_data+1),
				(UINT16)*(feature_data+2));
		else
			LOG_INF("Value Violation : feature_data<1");
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(imx334MIPIsub_sensorGain);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)imx334MIPIsub_sensorGain,
			sizeof(imx334MIPIsub_sensorGain));
		}
		break;
		break;
	default:
		break;
	}

	return ERROR_NONE;
} /* feature_control(ctx)  */


static const struct subdrv_ctx defctx = {

	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_max = 254788,
	.ana_gain_min = BASEGAIN,
	.ana_gain_step = 1,
	.exposure_def = 0x3D0,
	.exposure_max = 0xfff0,
	.exposure_min = 8,
	.exposure_step = 1,
	.max_frame_length = 0xfff0,

	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,	/* current shutter */
	.gain = BASEGAIN * 4,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	/* auto flicker enable: KAL_FALSE for disable auto flicker,
	 * KAL_TRUE for enable auto flicker
	 */
	.test_pattern = KAL_FALSE,

	/* current scenario id */
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	.hdr_mode = 0,	/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x34,	/* record current sensor's i2c write id */
};

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static struct subdrv_ops ops = {
	.get_id = get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = get_info,
	.get_resolution = get_resolution,
	.control = control,
	.feature_control = feature_control,
	.close = close,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc = get_frame_desc,
#endif
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, 24, 0},
	{HW_ID_DOVDD, 1800000, 1}, // vcamio(pmic supply) always on
	{HW_ID_RST, 0, 0},
	{HW_ID_DVDD, 1200000, 0},
	{HW_ID_PDN, 1, 1}, // pdn replace vcamio
	{HW_ID_AVDD, 2900000, 10},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 5},
	{HW_ID_RST, 1, 10},
};

const struct subdrv_entry imx334sub_mipi_raw_entry = {
	.name = "imx334sub_mipi_raw",
	.id = IMX334SUB_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};
