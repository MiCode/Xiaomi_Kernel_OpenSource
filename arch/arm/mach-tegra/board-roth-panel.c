/*
 * arch/arm/mach-tegra/board-roth-panel.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/ioport.h>
#include <linux/fb.h>
#include <linux/nvmap.h>
#include <linux/nvhost.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/of.h>

#include <mach/irqs.h>
#include <mach/dc.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t11.h>
#include <asm/mach-types.h>

#include "board.h"
#include "board-panel.h"
#include "devices.h"
#include "gpio-names.h"
#include "iomap.h"
#include "tegra11_host1x_devices.h"

struct platform_device * __init roth_host1x_init(void)
{
	struct platform_device *pdev = NULL;

#ifdef CONFIG_TEGRA_GRHOST
	if (!of_have_populated_dt())
		pdev = tegra11_register_host1x_devices();
	else
		pdev = to_platform_device(bus_find_device_by_name(
			&platform_bus_type, NULL, "host1x"));
#endif
	return pdev;
}

#define IS_EXTERNAL_PWM		1

#define DSI_PANEL_RESET		1

#define DSI_PANEL_RST_GPIO	TEGRA_GPIO_PH3
#define DSI_PANEL_BL_PWM	TEGRA_GPIO_PH1

#define DSI_PANEL_CE		0

#define DC_CTRL_MODE	TEGRA_DC_OUT_CONTINUOUS_MODE

/* HDMI Hotplug detection pin */
#define roth_hdmi_hpd	TEGRA_GPIO_PN7

static bool reg_requested;
static bool gpio_requested;

static struct regulator *vdd_lcd_s_1v8;
static struct regulator *vdd_lcd_bl;
static struct regulator *vdd_lcd_bl_en;
static struct regulator *avdd_lcd_3v0_2v8;

static struct regulator *roth_hdmi_reg;
static struct regulator *roth_hdmi_pll;
static struct regulator *roth_hdmi_vddio;

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu roth_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
		0,    1,    2,	  4,	5,    6,    7,	  9,
		10,   11,   12,   14,	15,   16,   18,   20,
		21,   23,   25,   27,	29,   31,   33,   35,
		37,   40,   42,   45,	48,   50,   53,   56,
		59,   62,   66,   69,	72,   76,   79,   83,
		87,   91,   95,   99,	103,  107,  112,  116,
		121,  126,  131,  136,	141,  146,  151,  156,
		162,  168,  173,  179,	185,  191,  197,  204,
		210,  216,  223,  230,	237,  244,  251,  258,
		265,  273,  280,  288,	296,  304,  312,  320,
		329,  337,  346,  354,	363,  372,  381,  390,
		400,  409,  419,  428,	438,  448,  458,  469,
		479,  490,  500,  511,	522,  533,  544,  555,
		567,  578,  590,  602,	614,  626,  639,  651,
		664,  676,  689,  702,	715,  728,  742,  755,
		769,  783,  797,  811,	825,  840,  854,  869,
		884,  899,  914,  929,	945,  960,  976,  992,
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
		0x100, 0x0,   0x0,
		0x0,   0x100, 0x0,
		0x0,   0x0,   0x100,
	},
	/* lut2 maps linear space to sRGB*/
	{
		0, 0, 1, 2, 3, 3, 4, 5,
		6, 6, 7, 8, 8, 9, 10, 10,
		11, 12, 12, 13, 13, 14, 14, 15,
		16, 16, 17, 17, 18, 18, 19, 19,
		19, 20, 20, 21, 21, 22, 22, 22,
		23, 23, 24, 24, 24, 25, 25, 25,
		26, 26, 27, 27, 27, 28, 28, 28,
		28, 29, 29, 29, 30, 30, 30, 31,
		31, 31, 31, 32, 32, 32, 33, 33,
		33, 33, 34, 34, 34, 35, 35, 35,
		35, 36, 36, 36, 36, 37, 37, 37,
		38, 38, 38, 38, 39, 39, 39, 39,
		40, 40, 40, 40, 40, 41, 41, 41,
		41, 42, 42, 42, 42, 43, 43, 43,
		43, 43, 44, 44, 44, 44, 45, 45,
		45, 45, 45, 46, 46, 46, 46, 46,
		47, 47, 47, 47, 47, 48, 48, 48,
		48, 48, 49, 49, 49, 49, 49, 49,
		50, 50, 50, 50, 50, 50, 51, 51,
		51, 51, 51, 51, 52, 52, 52, 52,
		52, 52, 53, 53, 53, 53, 53, 53,
		54, 54, 54, 54, 54, 54, 54, 55,
		55, 55, 55, 55, 55, 55, 55, 56,
		56, 56, 56, 56, 56, 56, 57, 57,
		57, 57, 57, 57, 57, 57, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 59,
		59, 59, 59, 59, 59, 59, 59, 59,
		60, 60, 60, 60, 60, 60, 60, 60,
		60, 61, 61, 61, 61, 61, 61, 61,
		61, 61, 61, 62, 62, 62, 62, 62,
		62, 62, 62, 62, 62, 63, 63, 63,
		63, 63, 63, 63, 63, 63, 63, 63,
		64, 64, 64, 64, 64, 64, 64, 64,
		64, 64, 64, 65, 65, 65, 65, 65,
		65, 65, 65, 65, 65, 65, 66, 66,
		66, 66, 66, 66, 66, 66, 66, 66,
		66, 66, 67, 67, 67, 67, 67, 67,
		67, 67, 67, 67, 67, 67, 68, 68,
		68, 68, 68, 68, 68, 68, 68, 68,
		68, 68, 69, 69, 69, 69, 69, 69,
		69, 69, 69, 69, 69, 69, 70, 70,
		70, 70, 70, 70, 70, 70, 70, 70,
		70, 70, 70, 71, 71, 71, 71, 71,
		71, 71, 71, 71, 71, 71, 71, 71,
		72, 72, 72, 72, 72, 72, 72, 72,
		72, 72, 72, 72, 72, 73, 73, 73,
		73, 73, 73, 73, 73, 73, 73, 73,
		73, 73, 73, 74, 74, 74, 74, 74,
		74, 74, 74, 74, 74, 74, 74, 74,
		75, 75, 75, 75, 75, 75, 75, 75,
		75, 75, 75, 75, 75, 75, 76, 76,
		76, 76, 76, 76, 76, 76, 76, 76,
		76, 76, 76, 76, 77, 77, 77, 77,
		77, 77, 77, 77, 77, 77, 77, 77,
		77, 77, 78, 78, 78, 78, 78, 78,
		78, 78, 78, 78, 78, 78, 78, 78,
		79, 79, 79, 79, 79, 79, 79, 79,
		79, 79, 79, 79, 79, 79, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 81, 81, 81, 81,
		81, 81, 81, 81, 81, 81, 81, 81,
		81, 81, 82, 82, 82, 82, 82, 82,
		82, 82, 82, 82, 82, 82, 82, 82,
		83, 83, 83, 83, 83, 83, 83, 83,
		84, 84, 85, 85, 86, 86, 87, 88,
		88, 89, 89, 90, 90, 91, 92, 92,
		93, 93, 94, 94, 95, 95, 96, 96,
		97, 97, 98, 98, 99, 99, 100, 100,
		101, 101, 102, 102, 103, 103, 104, 104,
		105, 105, 106, 106, 107, 107, 107, 108,
		108, 109, 109, 110, 110, 111, 111, 111,
		112, 112, 113, 113, 114, 114, 114, 115,
		115, 116, 116, 117, 117, 117, 118, 118,
		119, 119, 119, 120, 120, 121, 121, 121,
		122, 122, 123, 123, 123, 124, 124, 125,
		125, 126, 126, 126, 127, 127, 128, 128,
		128, 129, 129, 129, 130, 130, 131, 131,
		131, 132, 132, 133, 133, 133, 134, 134,
		135, 135, 135, 136, 136, 137, 137, 137,
		138, 138, 138, 139, 139, 140, 140, 140,
		141, 141, 142, 142, 142, 143, 143, 143,
		144, 144, 145, 145, 145, 146, 146, 146,
		147, 147, 147, 148, 148, 149, 149, 149,
		150, 150, 150, 151, 151, 151, 152, 152,
		153, 153, 153, 154, 154, 154, 155, 155,
		156, 156, 156, 157, 157, 157, 158, 158,
		159, 159, 159, 160, 160, 160, 161, 161,
		162, 162, 162, 163, 163, 164, 164, 164,
		165, 165, 166, 166, 166, 167, 167, 168,
		168, 168, 169, 169, 170, 170, 170, 171,
		171, 172, 172, 172, 173, 173, 173, 174,
		174, 175, 175, 175, 176, 176, 176, 177,
		177, 177, 178, 178, 178, 179, 179, 179,
		180, 180, 180, 180, 181, 181, 181, 182,
		182, 182, 182, 183, 183, 183, 184, 184,
		184, 184, 185, 185, 185, 185, 186, 186,
		186, 186, 187, 187, 187, 187, 188, 188,
		188, 188, 189, 189, 189, 190, 190, 190,
		190, 191, 191, 191, 191, 192, 192, 192,
		193, 193, 193, 193, 194, 194, 194, 195,
		195, 195, 195, 196, 196, 196, 197, 197,
		197, 198, 198, 198, 198, 199, 199, 199,
		200, 200, 200, 201, 201, 201, 202, 202,
		202, 203, 203, 204, 204, 204, 205, 205,
		205, 206, 206, 206, 207, 207, 208, 208,
		208, 209, 209, 209, 210, 210, 211, 211,
		211, 212, 212, 213, 213, 213, 214, 214,
		215, 215, 215, 216, 216, 217, 217, 217,
		218, 218, 218, 219, 219, 220, 220, 220,
		221, 221, 221, 222, 222, 222, 223, 223,
		223, 224, 224, 224, 225, 225, 225, 225,
		226, 226, 226, 226, 227, 227, 227, 227,
		228, 228, 228, 228, 229, 229, 229, 229,
		229, 230, 230, 230, 230, 231, 231, 231,
		231, 232, 232, 232, 233, 233, 233, 233,
		234, 234, 234, 235, 235, 235, 236, 236,
		236, 237, 237, 238, 238, 239, 239, 239,
		240, 240, 241, 241, 242, 242, 243, 244,
		244, 245, 245, 246, 247, 247, 248, 249,
		250, 250, 251, 252, 253, 254, 254, 255,
	},
};
#endif

static struct resource roth_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0, /* Filled in by roth_panel_init() */
		.end	= 0, /* Filled in by roth_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "mipi_cal",
		.start	= TEGRA_MIPI_CAL_BASE,
		.end	= TEGRA_MIPI_CAL_BASE + TEGRA_MIPI_CAL_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource roth_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0, /* Filled in by roth_panel_init() */
		.end	= 0, /* Filled in by roth_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static u8 panel_dsi_config[] = {0xe0, 0x43, 0x0, 0x80, 0x0, 0x0};
static u8 panel_disp_ctrl1[] = {0xb5, 0x34, 0x20, 0x40, 0x0, 0x20};
static u8 panel_disp_ctrl2[] = {0xb6, 0x04, 0x74, 0x0f, 0x16, 0x13};
static u8 panel_internal_clk[] = {0xc0, 0x01, 0x08};
static u8 panel_pwr_ctrl3[] = {
	0xc3, 0x0, 0x09, 0x10, 0x02, 0x0, 0x66, 0x00, 0x13, 0x0};
static u8 panel_pwr_ctrl4[] = {0xc4, 0x23, 0x24, 0x12, 0x12, 0x60};
static u8 panel_positive_gamma_red[] = {
	0xd0, 0x21, 0x25, 0x67, 0x36, 0x0a, 0x06, 0x61, 0x23, 0x03};
static u8 panel_negetive_gamma_red[] = {
	0xd1, 0x31, 0x25, 0x66, 0x36, 0x05, 0x06, 0x61, 0x23, 0x03};
static u8 panel_positive_gamma_green[] = {
	0xd2, 0x41, 0x26, 0x56, 0x36, 0x0a, 0x06, 0x61, 0x23, 0x03};
static u8 panel_negetive_gamma_green[] = {
	0xd3, 0x51, 0x26, 0x55, 0x36, 0x05, 0x06, 0x61, 0x23, 0x03};
static u8 panel_positive_gamma_blue[] = {
	0xd4, 0x41, 0x26, 0x56, 0x36, 0x0a, 0x06, 0x61, 0x23, 0x03};
static u8 panel_negetive_gamma_blue[] = {
	0xd5, 0x51, 0x26, 0x55, 0x36, 0x05, 0x06, 0x61, 0x23, 0x03};

#if DSI_PANEL_CE
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
#endif

static struct tegra_dsi_cmd dsi_init_cmd[] = {
	DSI_DLY_MS(20),
	DSI_GPIO_SET(DSI_PANEL_RST_GPIO, 1),

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

	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, DSI_DCS_SET_ADDR_MODE, 0x0B),

	/* panel OTP 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xf9, 0x0),

#if DSI_PANEL_CE
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
#endif
	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x02),
	DSI_DLY_MS(20),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x06),
	DSI_DLY_MS(20),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x4e),
	DSI_DLY_MS(100),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
	DSI_DLY_MS(140),

	/* panel OTP 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xf9, 0x80),
	DSI_DLY_MS(20),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
};

static u8 panel_suspend_pwr_ctrl4[] = {0xc4, 0x0, 0x0, 0x0, 0x0, 0x0};

static struct tegra_dsi_cmd dsi_suspend_cmd[] = {
	DSI_DLY_MS(40),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_OFF, 0x0),
	DSI_DLY_MS(20),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_ENTER_SLEEP_MODE, 0x0),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x0),

	/* panel power control 4 */
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_suspend_pwr_ctrl4),

	/* panel power control 1 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc1, 0x2),
	DSI_DLY_MS(20),

	/* panel power control 1 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc1, 0x3),
	DSI_DLY_MS(20),
};

static struct tegra_dsi_out roth_dsi = {
	.n_data_lanes = 4,
	.controller_vs = DSI_VS_1,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.refresh_rate = 60,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.dsi_instance = DSI_INSTANCE_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END,
	.dsi_init_cmd = dsi_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_init_cmd),
	.dsi_suspend_cmd = dsi_suspend_cmd,
	.n_suspend_cmd = ARRAY_SIZE(dsi_suspend_cmd),
};

static int roth_dsi_regulator_get(struct device *dev)
{
	int err = 0;

	if (reg_requested)
		return 0;

	avdd_lcd_3v0_2v8 = regulator_get(dev, "avdd_lcd");
	if (IS_ERR(avdd_lcd_3v0_2v8)) {
		pr_err("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0_2v8);
		avdd_lcd_3v0_2v8 = NULL;
		goto fail;
	}
	vdd_lcd_s_1v8 = regulator_get(dev, "dvdd_lcd");
	if (IS_ERR(vdd_lcd_s_1v8)) {
		pr_err("vdd_lcd_1v8_s regulator get failed\n");
		err = PTR_ERR(vdd_lcd_s_1v8);
		vdd_lcd_s_1v8 = NULL;
		goto fail;
	}

	if (machine_is_dalmore()) {
		vdd_lcd_bl = regulator_get(dev, "vdd_lcd_bl");
		if (IS_ERR(vdd_lcd_bl)) {
			pr_err("vdd_lcd_bl regulator get failed\n");
			err = PTR_ERR(vdd_lcd_bl);
			vdd_lcd_bl = NULL;
			goto fail;
		}
	}

	vdd_lcd_bl_en = regulator_get(dev, "vdd_lcd_bl_en");
	if (IS_ERR(vdd_lcd_bl_en)) {
		pr_err("vdd_lcd_bl_en regulator get failed\n");
		err = PTR_ERR(vdd_lcd_bl_en);
		vdd_lcd_bl_en = NULL;
		goto fail;
	}
	reg_requested = true;
	return 0;
fail:
	return err;
}

static int roth_dsi_gpio_get(void)
{
	int err = 0;

	if (gpio_requested)
		return 0;

	err = gpio_request(DSI_PANEL_RST_GPIO, "panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	gpio_requested = true;
	return 0;
fail:
	return err;
}

static struct tegra_dc_out roth_disp1_out;

static int roth_dsi_panel_enable(struct device *dev)
{
	int err = 0;

	err = roth_dsi_regulator_get(dev);
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}
	err = roth_dsi_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}

	/* Skip panel programming if in initialized mode */
	if (!(roth_disp1_out.flags & TEGRA_DC_OUT_INITIALIZED_MODE)) {
		roth_dsi.dsi_init_cmd = dsi_init_cmd;
		gpio_set_value(DSI_PANEL_RST_GPIO, 0);
	} else {
		roth_dsi.dsi_init_cmd = dsi_init_cmd + 2;
	}

	if (vdd_lcd_s_1v8) {
		err = regulator_enable(vdd_lcd_s_1v8);
		if (err < 0) {
			pr_err("vdd_lcd_1v8_s regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(3000, 5000);

	if (avdd_lcd_3v0_2v8) {
		err = regulator_enable(avdd_lcd_3v0_2v8);
		if (err < 0) {
			pr_err("avdd_lcd_3v0_2v8 regulator enable failed\n");
			goto fail;
		}
		regulator_set_voltage(avdd_lcd_3v0_2v8, 2800000, 2800000);
	}
	usleep_range(3000, 5000);

	if (vdd_lcd_bl) {
		err = regulator_enable(vdd_lcd_bl);
		if (err < 0) {
			pr_err("vdd_lcd_bl regulator enable failed\n");
			goto fail;
		}
	}

	if (vdd_lcd_bl_en) {
		err = regulator_enable(vdd_lcd_bl_en);
		if (err < 0) {
			pr_err("vdd_lcd_bl_en regulator enable failed\n");
			goto fail;
		}
	}

	return 0;
fail:
	return err;
}

static int roth_dsi_panel_disable(void)
{
	if (vdd_lcd_bl)
		regulator_disable(vdd_lcd_bl);

	if (vdd_lcd_bl_en)
		regulator_disable(vdd_lcd_bl_en);

	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
	mdelay(20);

	if (vdd_lcd_s_1v8)
		regulator_disable(vdd_lcd_s_1v8);

	if (avdd_lcd_3v0_2v8)
		regulator_disable(avdd_lcd_3v0_2v8);

	return 0;
}

static int roth_dsi_panel_postsuspend(void)
{
	/* TODO */
	return 0;
}

static struct tegra_dc_mode roth_dsi_modes[] = {
	{
		.pclk = 66700000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 4,
		.v_sync_width = 4,
		.h_back_porch = 112,
		.v_back_porch = 12,
		.h_active = 720,
		.v_active = 1280,
		.h_front_porch = 12,
		.v_front_porch = 8,
	},
};

static struct tegra_dc_sd_settings sd_settings;

static struct tegra_dc_out roth_disp1_out = {
	.type		= TEGRA_DC_OUT_DSI,
	.dsi		= &roth_dsi,

	.flags		= DC_CTRL_MODE,
	.sd_settings	= &sd_settings,

	.modes		= roth_dsi_modes,
	.n_modes	= ARRAY_SIZE(roth_dsi_modes),

	.enable		= roth_dsi_panel_enable,
	.disable	= roth_dsi_panel_disable,
	.postsuspend	= roth_dsi_panel_postsuspend,
	.width		= 62,
	.height		= 110,
};

static int roth_hdmi_enable(struct device *dev)
{
	int ret;
	if (!roth_hdmi_reg) {
		roth_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR(roth_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			roth_hdmi_reg = NULL;
			return PTR_ERR(roth_hdmi_reg);
		}
	}
	ret = regulator_enable(roth_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!roth_hdmi_pll) {
		roth_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR(roth_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			roth_hdmi_pll = NULL;
			regulator_put(roth_hdmi_reg);
			roth_hdmi_reg = NULL;
			return PTR_ERR(roth_hdmi_pll);
		}
	}
	ret = regulator_enable(roth_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int roth_hdmi_disable(void)
{
	if (roth_hdmi_reg) {
		regulator_disable(roth_hdmi_reg);
		regulator_put(roth_hdmi_reg);
		roth_hdmi_reg = NULL;
	}

	if (roth_hdmi_pll) {
		regulator_disable(roth_hdmi_pll);
		regulator_put(roth_hdmi_pll);
		roth_hdmi_pll = NULL;
	}

	return 0;
}

static int roth_hdmi_postsuspend(void)
{
	if (roth_hdmi_vddio) {
		regulator_disable(roth_hdmi_vddio);
		regulator_put(roth_hdmi_vddio);
		roth_hdmi_vddio = NULL;
	}
	return 0;
}

static int roth_hdmi_hotplug_init(struct device *dev)
{
	int e = 0;

	if (!roth_hdmi_vddio) {
		roth_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
		if (WARN_ON(IS_ERR(roth_hdmi_vddio))) {
			e = PTR_ERR(roth_hdmi_vddio);
			pr_err("%s: couldn't get regulator vdd_hdmi_5v0: %d\n",
					__func__, e);
			roth_hdmi_vddio = NULL;
		} else
			e = regulator_enable(roth_hdmi_vddio);
	}
	return e;
}

static void roth_hdmi_hotplug_report(bool state)
{
	if (state) {
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SDA,
						TEGRA_PUPD_PULL_DOWN);
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SCL,
						TEGRA_PUPD_PULL_DOWN);
	} else {
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SDA,
						TEGRA_PUPD_NORMAL);
		tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_DDC_SCL,
						TEGRA_PUPD_NORMAL);
	}
}

static struct tegra_dc_out roth_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,
	.parent_clk	= "pll_d2_out0",

	.dcc_bus	= 3,
	.hotplug_gpio	= roth_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(297000),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= roth_hdmi_enable,
	.disable	= roth_hdmi_disable,
	.postsuspend	= roth_hdmi_postsuspend,
	.hotplug_init	= roth_hdmi_hotplug_init,
	.hotplug_report = roth_hdmi_hotplug_report,
};

static struct tegra_fb_data roth_disp1_fb_data = {
	.win		= 0,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
	.xres		= 720,
	.yres		= 1280,
};

static struct tegra_dc_platform_data roth_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &roth_disp1_out,
	.fb		= &roth_disp1_fb_data,
	.emc_clk_rate	= 204000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
	.cmu		= &roth_cmu,
#endif
};

static struct tegra_fb_data roth_disp2_fb_data = {
	.win		= 0,
	.xres		= 1024,
	.yres		= 600,
	.bits_per_pixel = 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_platform_data roth_disp2_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &roth_disp2_out,
	.fb		= &roth_disp2_fb_data,
	.emc_clk_rate	= 300000000,
#ifdef CONFIG_TEGRA_DC_CMU
	.cmu_enable	= 1,
#endif
};

static struct platform_device roth_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= roth_disp2_resources,
	.num_resources	= ARRAY_SIZE(roth_disp2_resources),
	.dev = {
		.platform_data = &roth_disp2_pdata,
	},
};

static struct platform_device roth_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= roth_disp1_resources,
	.num_resources	= ARRAY_SIZE(roth_disp1_resources),
	.dev = {
		.platform_data = &roth_disp1_pdata,
	},
};

static struct nvmap_platform_carveout roth_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE,
		.size		= TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE,
		.buddy_size	= 0, /* no buddy allocation for IRAM */
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0, /* Filled in by roth_panel_init() */
		.size		= 0, /* Filled in by roth_panel_init() */
		.buddy_size	= SZ_32K,
	},
	[2] = {
		.name		= "vpr",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_VPR,
		.base		= 0, /* Filled in by roth_panel_init() */
		.size		= 0, /* Filled in by roth_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data roth_nvmap_data = {
	.carveouts	= roth_carveouts,
	.nr_carveouts	= ARRAY_SIZE(roth_carveouts),
};

static struct platform_device roth_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &roth_nvmap_data,
	},
};

static int roth_disp1_bl_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");

	return brightness;
}

static int roth_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &roth_disp1_device.dev;
}

static struct platform_pwm_backlight_data roth_disp1_bl_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 77,
	.pwm_period_ns	= 40000,
	.pwm_gpio	= DSI_PANEL_BL_PWM,
	.notify		= roth_disp1_bl_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= roth_disp1_check_fb,
};

static struct platform_device __maybe_unused
		roth_disp1_bl_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &roth_disp1_bl_data,
	},
};

static struct platform_device __maybe_unused
			*roth_bl_device[] __initdata = {
	&tegra_pwfm_device,
	&roth_disp1_bl_device,
};

int __init roth_panel_init(int board_id)
{
	int err = 0;
	struct resource __maybe_unused *res;
	struct platform_device *phost1x = NULL;

#ifdef CONFIG_TEGRA_NVMAP
	roth_carveouts[1].base = tegra_carveout_start;
	roth_carveouts[1].size = tegra_carveout_size;
	roth_carveouts[2].base = tegra_vpr_start;
	roth_carveouts[2].size = tegra_vpr_size;

	err = platform_device_register(&roth_nvmap_device);
	if (err) {
		pr_err("nvmap device registration failed\n");
		return err;
	}
#endif

	phost1x = roth_host1x_init();
	if (!phost1x) {
		pr_err("host1x devices registration failed\n");
		return -EINVAL;
	}

	res = platform_get_resource_byname(&roth_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	/* Copy the bootloader fb to the fb. */
	__tegra_move_framebuffer(&roth_nvmap_device,
		tegra_fb_start, tegra_bootloader_fb_start,
			min(tegra_fb_size, tegra_bootloader_fb_size));

	/*
	 * only roth supports initialized mode.
	 */
	if (!board_id)
		roth_disp1_out.flags |= TEGRA_DC_OUT_INITIALIZED_MODE;

	roth_disp1_device.dev.parent = &phost1x->dev;
	err = platform_device_register(&roth_disp1_device);
	if (err) {
		pr_err("disp1 device registration failed\n");
		return err;
	}

	err = tegra_init_hdmi(&roth_disp2_device, phost1x);
	if (err)
		return err;

#if IS_EXTERNAL_PWM
	err = platform_add_devices(roth_bl_device,
				ARRAY_SIZE(roth_bl_device));
	if (err) {
		pr_err("disp1 bl device registration failed");
		return err;
	}
#endif

#ifdef CONFIG_TEGRA_NVAVP
	nvavp_device.dev.parent = &phost1x->dev;
	err = platform_device_register(&nvavp_device);
	if (err) {
		pr_err("nvavp device registration failed\n");
		return err;
	}
#endif
	return err;
}
