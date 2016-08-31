/*
 * arch/arm/mach-tegra/panel-l-720p-5.c
 *
  * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mach/dc.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/mfd/max8831.h>
#include <linux/max8831_backlight.h>
#include <linux/leds.h>
#include <linux/ioport.h>
#include <linux/lm3528.h>
#include <linux/export.h>

#include "gpio-names.h"
#include "board-panel.h"
#include "board.h"

#define DSI_PANEL_RESET         1

#define DC_CTRL_MODE            TEGRA_DC_OUT_CONTINUOUS_MODE

static struct regulator *vdd_lcd_s_1v8;
static struct regulator *vdd_sys_bl_3v7;
static struct regulator *avdd_lcd_3v0_2v8;

static bool dsi_l_720p_5_reg_requested;
static bool dsi_l_720p_5_gpio_requested;
static bool is_bl_powered;

static struct tegra_dc_sd_settings dsi_l_720p_5_sd_settings = {
	.enable = 1, /* enabled by default */
	.use_auto_pwm = false,
	.hw_update_delay = 0,
	.bin_width = -1,
	.aggressiveness = 5,
	.use_vid_luma = false,
	.phase_in_adjustments = 0,
	.k_limit_enable = true,
	.k_limit = 180,
	.sd_window_enable = false,
	.soft_clipping_enable = true,
	/* Low soft clipping threshold to compensate for aggressive k_limit */
	.soft_clipping_threshold = 128,
	.smooth_k_enable = true,
	.smooth_k_incr = 4,
	/* Default video coefficients */
	.coeff = {5, 9, 2},
	.fc = {0, 0},
	/* Immediate backlight changes */
	.blp = {1024, 255},
	/* Gammas: R: 2.2 G: 2.2 B: 2.2 */
	/* Default BL TF */
	.bltf = {
			{
				{57, 65, 73, 82},
				{92, 103, 114, 125},
				{138, 150, 164, 178},
				{193, 208, 224, 241},
			},
		},
	/* Default LUT */
	.lut = {
			{
				{255, 255, 255},
				{199, 199, 199},
				{153, 153, 153},
				{116, 116, 116},
				{85, 85, 85},
				{59, 59, 59},
				{36, 36, 36},
				{17, 17, 17},
				{0, 0, 0},
			},
		},
	.sd_brightness = &sd_brightness,
	.use_vpulse2 = true,
};

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu dsi_l_720p_5_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
		0,    1,    2,    4,    5,    6,    7,    9,
		10,   11,   12,   14,   15,   16,   18,   20,
		21,   23,   25,   27,   29,   31,   33,   35,
		37,   40,   42,   45,   48,   50,   53,   56,
		59,   62,   66,   69,   72,   76,   79,   83,
		87,   91,   95,   99,   103,  107,  112,  116,
		121,  126,  131,  136,  141,  146,  151,  156,
		162,  168,  173,  179,  185,  191,  197,  204,
		210,  216,  223,  230,  237,  244,  251,  258,
		265,  273,  280,  288,  296,  304,  312,  320,
		329,  337,  346,  354,  363,  372,  381,  390,
		400,  409,  419,  428,  438,  448,  458,  469,
		479,  490,  500,  511,  522,  533,  544,  555,
		567,  578,  590,  602,  614,  626,  639,  651,
		664,  676,  689,  702,  715,  728,  742,  755,
		769,  783,  797,  811,  825,  840,  854,  869,
		884,  899,  914,  929,  945,  960,  976,  992,
		1008, 1024, 1041, 1057, 1074, 1091, 1108, 1125,
		1142, 1159, 1177, 1195, 1213, 1231, 1249, 1267,
		1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420,
		1440, 1459, 1480, 1500, 1520, 1541, 1562, 1582,
		1603, 1625, 1646, 1668, 1689, 1711, 1733, 1755,
		1778, 1800, 1823, 1846, 1869, 1892, 1916, 1939,
		1963, 1987, 2011, 2035, 2059, 2084, 2109, 2133,
		2159, 2184, 2209, 2235, 2260, 2286, 2312, 2339,
		2365, 2392, 2419, 2446, 2473, 2500, 2527, 2555,
		2583, 2611, 2639, 2668, 2696, 2725, 2754, 2783,
		2812, 2841, 2871, 2901, 2931, 2961, 2991, 3022,
		3052, 3083, 3114, 3146, 3177, 3209, 3240, 3272,
		3304, 3337, 3369, 3402, 3435, 3468, 3501, 3535,
		3568, 3602, 3636, 3670, 3705, 3739, 3774, 3809,
		3844, 3879, 3915, 3950, 3986, 4022, 4059, 4095,
	},
	/* csc */
	{
		0x10D, 0x3F3, 0x000, /* 1.05036053  -0.05066457 0.00030404 */
		0x000, 0x0FC, 0x003, /* -0.00012137 0.98659651  0.01352485 */
		0x002, 0x001, 0x0FC, /* 0.00722989  0.00559134  0.98717878 */
	},
	/* lut2 maps linear space to sRGB */
	{
		0,    1,    2,    2,    3,    4,    5,    6,
		6,    7,    8,    9,    10,   10,   11,   12,
		13,   13,   14,   15,   15,   16,   16,   17,
		18,   18,   19,   19,   20,   20,   21,   21,
		22,   22,   23,   23,   23,   24,   24,   25,
		25,   25,   26,   26,   27,   27,   27,   28,
		28,   29,   29,   29,   30,   30,   30,   31,
		31,   31,   32,   32,   32,   33,   33,   33,
		34,   34,   34,   34,   35,   35,   35,   36,
		36,   36,   37,   37,   37,   37,   38,   38,
		38,   38,   39,   39,   39,   40,   40,   40,
		40,   41,   41,   41,   41,   42,   42,   42,
		42,   43,   43,   43,   43,   43,   44,   44,
		44,   44,   45,   45,   45,   45,   46,   46,
		46,   46,   46,   47,   47,   47,   47,   48,
		48,   48,   48,   48,   49,   49,   49,   49,
		49,   50,   50,   50,   50,   50,   51,   51,
		51,   51,   51,   52,   52,   52,   52,   52,
		53,   53,   53,   53,   53,   54,   54,   54,
		54,   54,   55,   55,   55,   55,   55,   55,
		56,   56,   56,   56,   56,   57,   57,   57,
		57,   57,   57,   58,   58,   58,   58,   58,
		58,   59,   59,   59,   59,   59,   59,   60,
		60,   60,   60,   60,   60,   61,   61,   61,
		61,   61,   61,   62,   62,   62,   62,   62,
		62,   63,   63,   63,   63,   63,   63,   64,
		64,   64,   64,   64,   64,   64,   65,   65,
		65,   65,   65,   65,   66,   66,   66,   66,
		66,   66,   66,   67,   67,   67,   67,   67,
		67,   67,   68,   68,   68,   68,   68,   68,
		68,   69,   69,   69,   69,   69,   69,   69,
		70,   70,   70,   70,   70,   70,   70,   71,
		71,   71,   71,   71,   71,   71,   72,   72,
		72,   72,   72,   72,   72,   72,   73,   73,
		73,   73,   73,   73,   73,   74,   74,   74,
		74,   74,   74,   74,   74,   75,   75,   75,
		75,   75,   75,   75,   75,   76,   76,   76,
		76,   76,   76,   76,   77,   77,   77,   77,
		77,   77,   77,   77,   78,   78,   78,   78,
		78,   78,   78,   78,   78,   79,   79,   79,
		79,   79,   79,   79,   79,   80,   80,   80,
		80,   80,   80,   80,   80,   81,   81,   81,
		81,   81,   81,   81,   81,   81,   82,   82,
		82,   82,   82,   82,   82,   82,   83,   83,
		83,   83,   83,   83,   83,   83,   83,   84,
		84,   84,   84,   84,   84,   84,   84,   84,
		85,   85,   85,   85,   85,   85,   85,   85,
		85,   86,   86,   86,   86,   86,   86,   86,
		86,   86,   87,   87,   87,   87,   87,   87,
		87,   87,   87,   88,   88,   88,   88,   88,
		88,   88,   88,   88,   88,   89,   89,   89,
		89,   89,   89,   89,   89,   89,   90,   90,
		90,   90,   90,   90,   90,   90,   90,   90,
		91,   91,   91,   91,   91,   91,   91,   91,
		91,   91,   92,   92,   92,   92,   92,   92,
		92,   92,   92,   92,   93,   93,   93,   93,
		93,   93,   93,   93,   93,   93,   94,   94,
		94,   94,   94,   94,   94,   94,   94,   94,
		95,   95,   95,   95,   95,   95,   95,   95,
		95,   95,   96,   96,   96,   96,   96,   96,
		96,   96,   96,   96,   96,   97,   97,   97,
		97,   97,   97,   97,   97,   97,   97,   98,
		98,   98,   98,   98,   98,   98,   98,   98,
		98,   98,   99,   99,   99,   99,   99,   99,
		99,   100,  101,  101,  102,  103,  103,  104,
		105,  105,  106,  107,  107,  108,  109,  109,
		110,  111,  111,  112,  113,  113,  114,  115,
		115,  116,  116,  117,  118,  118,  119,  119,
		120,  120,  121,  122,  122,  123,  123,  124,
		124,  125,  126,  126,  127,  127,  128,  128,
		129,  129,  130,  130,  131,  131,  132,  132,
		133,  133,  134,  134,  135,  135,  136,  136,
		137,  137,  138,  138,  139,  139,  140,  140,
		141,  141,  142,  142,  143,  143,  144,  144,
		145,  145,  145,  146,  146,  147,  147,  148,
		148,  149,  149,  150,  150,  150,  151,  151,
		152,  152,  153,  153,  153,  154,  154,  155,
		155,  156,  156,  156,  157,  157,  158,  158,
		158,  159,  159,  160,  160,  160,  161,  161,
		162,  162,  162,  163,  163,  164,  164,  164,
		165,  165,  166,  166,  166,  167,  167,  167,
		168,  168,  169,  169,  169,  170,  170,  170,
		171,  171,  172,  172,  172,  173,  173,  173,
		174,  174,  174,  175,  175,  176,  176,  176,
		177,  177,  177,  178,  178,  178,  179,  179,
		179,  180,  180,  180,  181,  181,  182,  182,
		182,  183,  183,  183,  184,  184,  184,  185,
		185,  185,  186,  186,  186,  187,  187,  187,
		188,  188,  188,  189,  189,  189,  189,  190,
		190,  190,  191,  191,  191,  192,  192,  192,
		193,  193,  193,  194,  194,  194,  195,  195,
		195,  196,  196,  196,  196,  197,  197,  197,
		198,  198,  198,  199,  199,  199,  200,  200,
		200,  200,  201,  201,  201,  202,  202,  202,
		202,  203,  203,  203,  204,  204,  204,  205,
		205,  205,  205,  206,  206,  206,  207,  207,
		207,  207,  208,  208,  208,  209,  209,  209,
		209,  210,  210,  210,  211,  211,  211,  211,
		212,  212,  212,  213,  213,  213,  213,  214,
		214,  214,  214,  215,  215,  215,  216,  216,
		216,  216,  217,  217,  217,  217,  218,  218,
		218,  219,  219,  219,  219,  220,  220,  220,
		220,  221,  221,  221,  221,  222,  222,  222,
		223,  223,  223,  223,  224,  224,  224,  224,
		225,  225,  225,  225,  226,  226,  226,  226,
		227,  227,  227,  227,  228,  228,  228,  228,
		229,  229,  229,  229,  230,  230,  230,  230,
		231,  231,  231,  231,  232,  232,  232,  232,
		233,  233,  233,  233,  234,  234,  234,  234,
		235,  235,  235,  235,  236,  236,  236,  236,
		237,  237,  237,  237,  238,  238,  238,  238,
		239,  239,  239,  239,  240,  240,  240,  240,
		240,  241,  241,  241,  241,  242,  242,  242,
		242,  243,  243,  243,  243,  244,  244,  244,
		244,  244,  245,  245,  245,  245,  246,  246,
		246,  246,  247,  247,  247,  247,  247,  248,
		248,  248,  248,  249,  249,  249,  249,  249,
		250,  250,  250,  250,  251,  251,  251,  251,
		251,  252,  252,  252,  252,  253,  253,  253,
		253,  253,  254,  254,  254,  254,  255,  255,
	},
};
#endif

static tegra_dc_bl_output dsi_l_720p_5_max8831_bl_response_curve = {
	0, 1, 3, 5, 7, 9, 11, 13,
	15, 17, 19, 21, 22, 23, 25, 26,
	28, 29, 30, 32, 33, 34, 36, 37,
	39, 40, 42, 43, 45, 46, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57,
	58, 59, 60, 61, 62, 63, 64, 65,
	66, 67, 68, 70, 71, 72, 73, 74,
	75, 77, 78, 79, 80, 81, 82, 83,
	84, 85, 86, 87, 88, 89, 90, 91,
	92, 93, 94, 95, 96, 97, 98, 99,
	100, 101, 101, 102, 102, 103, 103, 104,
	105, 105, 106, 107, 108, 108, 109, 110,
	111, 112, 113, 114, 115, 116, 117, 118,
	119, 120, 121, 121, 122, 123, 124, 125,
	126, 127, 128, 129, 130, 131, 132, 133,
	134, 135, 135, 136, 137, 138, 139, 140,
	141, 142, 143, 144, 145, 146, 147, 148,
	149, 150, 151, 152, 153, 154, 155, 156,
	156, 157, 158, 159, 160, 161, 162, 162,
	163, 163, 164, 164, 165, 165, 166, 167,
	167, 168, 169, 170, 171, 172, 173, 173,
	174, 175, 176, 177, 178, 179, 180, 181,
	182, 183, 184, 185, 186, 187, 188, 188,
	189, 190, 191, 192, 193, 194, 194, 195,
	196, 197, 198, 199, 200, 201, 202, 203,
	204, 204, 205, 206, 206, 207, 207, 208,
	209, 209, 210, 211, 212, 213, 214, 215,
	216, 217, 218, 219, 220, 221, 222, 223,
	223, 224, 225, 226, 227, 228, 229, 230,
	231, 232, 233, 234, 235, 236, 237, 238,
	239, 240, 241, 242, 243, 244, 245, 246,
	247, 247, 248, 250, 251, 252, 253, 255
};

static tegra_dc_bl_output dsi_l_720p_5_lm3528_bl_response_curve = {
	1, 33, 61, 77, 88, 97, 105, 111,
	116, 121, 125, 129, 132, 136, 139, 141,
	144, 146, 149, 151, 153, 155, 157, 158,
	160, 162, 163, 165, 166, 168, 169, 170,
	172, 173, 174, 175, 176, 177, 178, 180,
	181, 182, 182, 183, 184, 185, 186, 187,
	188, 189, 189, 190, 191, 192, 193, 193,
	194, 195, 195, 196, 197, 197, 198, 199,
	199, 200, 201, 201, 202, 202, 203, 203,
	204, 205, 205, 206, 206, 207, 207, 208,
	208, 209, 209, 210, 210, 211, 211, 212,
	212, 212, 213, 213, 214, 214, 215, 215,
	216, 216, 216, 217, 217, 218, 218, 218,
	219, 219, 219, 220, 220, 221, 221, 221,
	222, 222, 222, 223, 223, 223, 224, 224,
	224, 225, 225, 225, 226, 226, 226, 227,
	227, 227, 228, 228, 228, 229, 229, 229,
	229, 230, 230, 230, 231, 231, 231, 231,
	232, 232, 232, 233, 233, 233, 233, 234,
	234, 234, 234, 235, 235, 235, 235, 236,
	236, 236, 236, 237, 237, 237, 237, 238,
	238, 238, 238, 239, 239, 239, 239, 240,
	240, 240, 240, 240, 241, 241, 241, 241,
	242, 242, 242, 242, 242, 243, 243, 243,
	243, 243, 244, 244, 244, 244, 244, 245,
	245, 245, 245, 245, 246, 246, 246, 246,
	246, 247, 247, 247, 247, 247, 248, 248,
	248, 248, 248, 248, 249, 249, 249, 249,
	249, 250, 250, 250, 250, 250, 250, 251,
	251, 251, 251, 251, 251, 252, 252, 252,
	252, 252, 252, 253, 253, 253, 253, 253,
	253, 254, 254, 254, 254, 254, 254, 255
};

static p_tegra_dc_bl_output dsi_l_720p_5_bl_response_curve;

static int __maybe_unused dsi_l_720p_5_bl_notify(struct device *unused,
							int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else
		brightness = dsi_l_720p_5_bl_response_curve[brightness];

	return brightness;
}
static bool __maybe_unused dsi_l_720p_5_check_bl_power(void)
{
	return is_bl_powered;
}

/*
	LG uses I2C max8831 blacklight device
*/
static struct led_info dsi_l_720p_5_max8831_leds[] = {
	[MAX8831_ID_LED3] = {
		.name = "max8831:red:pluto",
	},
	[MAX8831_ID_LED4] = {
		.name = "max8831:green:pluto",
	},
	[MAX8831_ID_LED5] = {
		.name = "max8831:blue:pluto",
	},
};

static struct platform_max8831_backlight_data dsi_l_720p_5_max8831_bl_data = {
	.id	= -1,
	.name	= "pluto_display_bl",
	.max_brightness	= MAX8831_BL_LEDS_MAX_CURR,
	.dft_brightness	= 100,
	.notify	= dsi_l_720p_5_bl_notify,
	.is_powered = dsi_l_720p_5_check_bl_power,
};

static struct max8831_subdev_info dsi_l_720p_5_max8831_subdevs[] = {
	{
		.id = MAX8831_ID_LED3,
		.name = "max8831_led_bl",
		.platform_data = &dsi_l_720p_5_max8831_leds[MAX8831_ID_LED3],
		.pdata_size = sizeof(
				dsi_l_720p_5_max8831_leds[MAX8831_ID_LED3]),
	}, {
		.id = MAX8831_ID_LED4,
		.name = "max8831_led_bl",
		.platform_data = &dsi_l_720p_5_max8831_leds[MAX8831_ID_LED4],
		.pdata_size = sizeof(
				dsi_l_720p_5_max8831_leds[MAX8831_ID_LED4]),
	}, {
		.id = MAX8831_ID_LED5,
		.name = "max8831_led_bl",
		.platform_data = &dsi_l_720p_5_max8831_leds[MAX8831_ID_LED5],
		.pdata_size = sizeof(
				dsi_l_720p_5_max8831_leds[MAX8831_ID_LED5]),
	}, {
		.id = MAX8831_BL_LEDS,
		.name = "max8831_display_bl",
		.platform_data = &dsi_l_720p_5_max8831_bl_data,
		.pdata_size = sizeof(dsi_l_720p_5_max8831_bl_data),
	},
};

static struct max8831_platform_data dsi_l_720p_5_max8831 = {
	.num_subdevs = ARRAY_SIZE(dsi_l_720p_5_max8831_subdevs),
	.subdevs = dsi_l_720p_5_max8831_subdevs,
};

static struct i2c_board_info dsi_l_720p_5_i2c_led_info = {
	.type		= "max8831",
	.addr		= 0x4d,
	.platform_data	= &dsi_l_720p_5_max8831,
};

static struct lm3528_platform_data lm3528_pdata = {
	.dft_brightness	= 100,
	.is_powered = dsi_l_720p_5_check_bl_power,
	.notify	= dsi_l_720p_5_bl_notify,
};

static struct i2c_board_info lm3528_dsi_l_720p_5_i2c_led_info = {
	.type		= "lm3528_display_bl",
	.addr		= 0x36,
	.platform_data	= &lm3528_pdata,
};

static int __init dsi_l_720p_5_register_bl_dev(void)
{
	struct i2c_board_info *bl_info;
	struct board_info board_info;
	tegra_get_board_info(&board_info);

	switch (board_info.board_id) {
	case BOARD_E1670: /* Atlantis ERS */
	case BOARD_E1671: /* Atlantis POP Socket */
		bl_info = &lm3528_dsi_l_720p_5_i2c_led_info;
		dsi_l_720p_5_bl_response_curve =
			dsi_l_720p_5_lm3528_bl_response_curve;
		break;
	case BOARD_E1680: /* Ceres ERS */
	case BOARD_E1681: /* Ceres DSC Socket */
		bl_info = &dsi_l_720p_5_i2c_led_info;
		dsi_l_720p_5_bl_response_curve =
				dsi_l_720p_5_max8831_bl_response_curve;
		break;
	case BOARD_E1580: /* Pluto */
	/* fall through */
	default:
		bl_info = &dsi_l_720p_5_i2c_led_info;
		dsi_l_720p_5_bl_response_curve =
				dsi_l_720p_5_max8831_bl_response_curve;
		break;
	}

	return i2c_register_board_info(1, bl_info, 1);
}

struct tegra_dc_mode dsi_l_720p_5_modes[] = {
	{
		.pclk = 66700000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 4,
		.v_sync_width = 4,
		.h_back_porch = 112,
		.v_back_porch = 7,
		.h_active = 720,
		.v_active = 1280,
		.h_front_porch = 12,
		.v_front_porch = 20,
	},
};
static int dsi_l_720p_5_reg_get(void)
{
	int err = 0;

	if (dsi_l_720p_5_reg_requested)
		return 0;

	avdd_lcd_3v0_2v8 = regulator_get(NULL, "avdd_lcd");

	if (IS_ERR(avdd_lcd_3v0_2v8)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0_2v8);
		avdd_lcd_3v0_2v8 = NULL;
		goto fail;
	}

	vdd_lcd_s_1v8 = regulator_get(NULL, "vdd_lcd_1v8_s");
	if (IS_ERR(vdd_lcd_s_1v8)) {
		pr_err("vdd_lcd_1v8_s regulator get failed\n");
		err = PTR_ERR(vdd_lcd_s_1v8);
		vdd_lcd_s_1v8 = NULL;
		goto fail;
	}

	vdd_sys_bl_3v7 = regulator_get(NULL, "vdd_sys_bl");
	if (IS_ERR(vdd_sys_bl_3v7)) {
		pr_err("vdd_sys_bl regulator get failed\n");
		err = PTR_ERR(vdd_sys_bl_3v7);
		vdd_sys_bl_3v7 = NULL;
		goto fail;
	}

	dsi_l_720p_5_reg_requested = true;
	return 0;
fail:
	return err;
}

static struct tegra_dsi_out dsi_l_720p_5_pdata;
static int dsi_l_720p_5_gpio_get(void)
{
	int err = 0;

	if (dsi_l_720p_5_gpio_requested)
		return 0;

	err = gpio_request(dsi_l_720p_5_pdata.dsi_panel_rst_gpio,
		"panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	err = gpio_request(dsi_l_720p_5_pdata.dsi_panel_bl_en_gpio,
		"panel backlight");
	if (err < 0) {
		pr_err("panel backlight gpio request failed\n");
		goto fail;
	}

	err = gpio_request(dsi_l_720p_5_pdata.dsi_panel_bl_pwm_gpio,
		"panel pwm");
	if (err < 0) {
		pr_err("panel backlight pwm gpio request failed\n");
		goto fail;
	}

	dsi_l_720p_5_gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_l_720p_5_enable(struct device *dev)
{
	int err = 0;

	err = dsi_l_720p_5_reg_get();
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = tegra_panel_gpio_get_dt("lg,720p-5", &panel_of);
	if (err < 0) {
		/* try to request gpios from board file */
		err = dsi_l_720p_5_gpio_get();
		if (err < 0) {
			pr_err("dsi gpio request failed\n");
			goto fail;
		}
	}

	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET]))
		gpio_direction_output(
			panel_of.panel_gpio[TEGRA_GPIO_RESET], 0);
	else
		gpio_direction_output(
			dsi_l_720p_5_pdata.dsi_panel_rst_gpio, 0);

	if (avdd_lcd_3v0_2v8) {
		err = regulator_enable(avdd_lcd_3v0_2v8);
		if (err < 0) {
			pr_err("avdd_lcd regulator enable failed\n");
			goto fail;
		}
		regulator_set_voltage(avdd_lcd_3v0_2v8, 2800000, 2800000);
	}

	usleep_range(3000, 5000);

	if (vdd_lcd_s_1v8) {
		err = regulator_enable(vdd_lcd_s_1v8);
		if (err < 0) {
			pr_err("vdd_lcd_1v8_s regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(3000, 5000);

	if (vdd_sys_bl_3v7) {
		err = regulator_enable(vdd_sys_bl_3v7);
		if (err < 0) {
			pr_err("vdd_sys_bl regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(3000, 5000);

#if DSI_PANEL_RESET
	err = tegra_panel_reset(&panel_of, 20);
	if (err < 0) {
		/* use platform data */
		gpio_set_value(dsi_l_720p_5_pdata.dsi_panel_rst_gpio, 1);
		usleep_range(1000, 5000);
		gpio_set_value(dsi_l_720p_5_pdata.dsi_panel_rst_gpio, 0);
		usleep_range(1000, 5000);
		gpio_set_value(dsi_l_720p_5_pdata.dsi_panel_rst_gpio, 1);
		msleep(20);
	}
#endif
	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_BL_ENABLE]))
		gpio_direction_output(
			panel_of.panel_gpio[TEGRA_GPIO_BL_ENABLE], 1);
	else
		gpio_direction_output(
			dsi_l_720p_5_pdata.dsi_panel_bl_en_gpio, 1);

	is_bl_powered = true;
	return 0;
fail:
	return err;
}

static u8 panel_dsi_config[] = {0xe0, 0x43, 0x0, 0x80, 0x0, 0x0};
static u8 panel_disp_ctrl1[] = {0xb5, 0x34, 0x20, 0x40, 0x0, 0x20};
static u8 panel_disp_ctrl2[] = {0xb6, 0x04, 0x74, 0x0f, 0x16, 0x13};
static u8 panel_internal_clk[] = {0xc0, 0x01, 0x08};
static u8 panel_pwr_ctrl3[] = {
	0xc3, 0x0, 0x09, 0x10, 0x02, 0x0, 0x66, 0x20, 0x13, 0x0};
static u8 panel_pwr_ctrl4[] = {0xc4, 0x23, 0x24, 0x17, 0x17, 0x59};
static u8 panel_positive_gamma_red[] = {
	0xd0, 0x21, 0x13, 0x67, 0x37, 0x0c, 0x06, 0x62, 0x23, 0x03};
static u8 panel_negetive_gamma_red[] = {
	0xd1, 0x32, 0x13, 0x66, 0x37, 0x02, 0x06, 0x62, 0x23, 0x03};
static u8 panel_positive_gamma_green[] = {
	0xd2, 0x41, 0x14, 0x56, 0x37, 0x0c, 0x06, 0x62, 0x23, 0x03};
static u8 panel_negetive_gamma_green[] = {
	0xd3, 0x52, 0x14, 0x55, 0x37, 0x02, 0x06, 0x62, 0x23, 0x03};
static u8 panel_positive_gamma_blue[] =	{
	0xd4, 0x41, 0x14, 0x56, 0x37, 0x0c, 0x06, 0x62, 0x23, 0x03};
static u8 panel_negetive_gamma_blue[] =	{
	0xd5, 0x52, 0x14, 0x55, 0x37, 0x02, 0x06, 0x62, 0x23, 0x03};
static u8 panel_ce2[] = {0x71, 0x0, 0x0, 0x01, 0x01};
static u8 panel_ce3[] = {0x72, 0x01, 0x0e};
static u8 panel_ce4[] = {0x73, 0x34, 0x52, 0x0};
static u8 panel_ce5[] = {0x74, 0x05, 0x0, 0x06};
static u8 panel_ce6[] = {0x75, 0x03, 0x0, 0x07};
static u8 panel_ce7[] = {0x76, 0x07, 0x0, 0x06};
static u8 panel_ce8[] = {0x77, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f};
static u8 panel_ce9[] = {0x78, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};
static u8 panel_ce10[] = {
	0x79, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};
static u8 panel_ce11[] = {0x7a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
static u8 panel_ce12[] = {0x7b, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
static u8 panel_ce13[] = {0x7c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

static struct tegra_dsi_cmd dsi_l_720p_5_init_cmd[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_dsi_config),

	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_disp_ctrl1),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_disp_ctrl2),

	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_internal_clk),

	/*  panel power control 1 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc1, 0x0),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_pwr_ctrl3),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_pwr_ctrl4),

	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_positive_gamma_red),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_negetive_gamma_red),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_positive_gamma_green),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_negetive_gamma_green),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_positive_gamma_blue),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_negetive_gamma_blue),

	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, DSI_DCS_SET_ADDR_MODE, 0x08),

	/* panel OTP 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xf9, 0x0),

	/* panel CE 1 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x70, 0x0),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce3),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce4),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce5),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce6),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce7),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce8),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce9),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce10),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce11),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce12),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce13),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x02),
	DSI_DLY_MS(15),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x06),
	DSI_DLY_MS(15),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x4e),
	DSI_DLY_MS(85),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
	DSI_DLY_MS(15),

	/* panel OTP 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xf9, 0x80),
	DSI_DLY_MS(15),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
};

static struct tegra_dsi_out dsi_l_720p_5_pdata = {
	.n_data_lanes = 4,

	.refresh_rate = 60,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END,

	.controller_vs = DSI_VS_1,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.dsi_init_cmd = dsi_l_720p_5_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_l_720p_5_init_cmd),
};

static int dsi_l_720p_5_disable(void)
{
	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_BL_ENABLE]))
		gpio_direction_output(
			panel_of.panel_gpio[TEGRA_GPIO_BL_ENABLE], 0);
	else
		gpio_direction_output(
			dsi_l_720p_5_pdata.dsi_panel_bl_en_gpio, 0);

	is_bl_powered = false;

	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET]))
		gpio_direction_output(
			panel_of.panel_gpio[TEGRA_GPIO_RESET], 0);
	else
		gpio_direction_output(
			dsi_l_720p_5_pdata.dsi_panel_rst_gpio, 0);

	if (vdd_sys_bl_3v7)
		regulator_disable(vdd_sys_bl_3v7);

	if (vdd_lcd_s_1v8)
		regulator_disable(vdd_lcd_s_1v8);

	if (avdd_lcd_3v0_2v8)
		regulator_disable(avdd_lcd_3v0_2v8);

	return 0;
}

static void dsi_l_720p_5_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_l_720p_5_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_l_720p_5_modes;
	dc->n_modes = ARRAY_SIZE(dsi_l_720p_5_modes);
	dc->enable = dsi_l_720p_5_enable;
	dc->disable = dsi_l_720p_5_disable;
	dc->width = 62;
	dc->height = 110;
	dc->flags = DC_CTRL_MODE;
}
static void dsi_l_720p_5_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_l_720p_5_modes[0].h_active;
	fb->yres = dsi_l_720p_5_modes[0].v_active;
}

static void dsi_l_720p_5_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	struct board_info bi;
	tegra_get_display_board_info(&bi);

	*settings = dsi_l_720p_5_sd_settings;

	if (bi.board_id == BOARD_E1563)
		settings->bl_device_name = "lm3528_display_bl";
	else
		settings->bl_device_name = "max8831_display_bl";
}

#ifdef CONFIG_TEGRA_DC_CMU
static void dsi_l_720p_5_cmu_init(struct tegra_dc_platform_data *pdata)
{
	pdata->cmu = &dsi_l_720p_5_cmu;
}
#endif

struct tegra_panel __initdata dsi_l_720p_5 = {
	.init_sd_settings = dsi_l_720p_5_sd_settings_init,
	.init_dc_out = dsi_l_720p_5_dc_out_init,
	.init_fb_data = dsi_l_720p_5_fb_data_init,
	.register_bl_dev = dsi_l_720p_5_register_bl_dev,
#ifdef CONFIG_TEGRA_DC_CMU
	.init_cmu_data = dsi_l_720p_5_cmu_init,
#endif
};
EXPORT_SYMBOL(dsi_l_720p_5);
