/*
 * arch/arm/mach-tegra/panel-s-1080p-5-mi3.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <mach/iomap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/ioport.h>

#include "gpio-names.h"
#include "board-panel.h"
#include "board-pisces.h"

#define DSI_PANEL_RESET         1
#define DSI_PANEL_RST_GPIO      TEGRA_GPIO_PH5

#define LCD_VSN_EN TEGRA_GPIO_PS0
#define LCD_VSP_EN TEGRA_GPIO_PI4

#define DC_CTRL_MODE            (TEGRA_DC_OUT_CONTINUOUS_MODE | \
	TEGRA_DC_OUT_INITIALIZED_MODE)

static struct regulator *vdd_lcd_s_1v8;

static bool dsi_s_mi3_1080p_5_reg_requested;
static bool dsi_s_mi3_1080p_5_gpio_requested;

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu dsi_s_mi3_1080p_5_cmu = {
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
		0x0FE, 0x001, 0x3FF,
		0x3FF, 0x0E3, 0x004,
		0x000, 0x003, 0x0D9,
	},
	/* lut2 maps linear space to sRGB */
	{
		0, 3, 6, 8, 11, 13, 15, 17,
		19, 21, 22, 24, 25, 26, 27, 28,
		29, 29, 30, 30, 31, 31, 31, 32,
		32, 32, 32, 32, 32, 32, 32, 32,
		32, 32, 31, 31, 32, 32, 32, 32,
		32, 32, 32, 33, 33, 33, 33, 34,
		34, 34, 35, 35, 36, 36, 36, 37,
		37, 38, 38, 39, 39, 40, 40, 41,
		41, 42, 42, 43, 43, 44, 45, 45,
		46, 46, 47, 47, 47, 48, 48, 49,
		49, 50, 50, 51, 51, 51, 52, 52,
		52, 53, 53, 53, 54, 54, 54, 55,
		55, 55, 55, 56, 56, 56, 56, 57,
		57, 57, 57, 58, 58, 58, 58, 58,
		59, 59, 59, 59, 59, 60, 60, 60,
		60, 60, 60, 61, 61, 61, 61, 61,
		61, 62, 62, 62, 62, 62, 62, 63,
		63, 63, 63, 63, 63, 64, 64, 64,
		64, 64, 65, 65, 65, 65, 65, 66,
		66, 66, 66, 66, 67, 67, 67, 67,
		68, 68, 68, 68, 68, 69, 69, 69,
		69, 70, 70, 70, 70, 71, 71, 71,
		71, 72, 72, 72, 72, 72, 73, 73,
		73, 73, 74, 74, 74, 74, 75, 75,
		75, 75, 76, 76, 76, 76, 77, 77,
		77, 77, 78, 78, 78, 78, 78, 79,
		79, 79, 79, 79, 80, 80, 80, 80,
		81, 81, 81, 81, 81, 82, 82, 82,
		82, 82, 82, 83, 83, 83, 83, 83,
		83, 84, 84, 84, 84, 84, 84, 85,
		85, 85, 85, 85, 85, 85, 86, 86,
		86, 86, 86, 86, 86, 87, 87, 87,
		87, 87, 87, 87, 87, 88, 88, 88,
		88, 88, 88, 88, 88, 88, 89, 89,
		89, 89, 89, 89, 89, 89, 89, 90,
		90, 90, 90, 90, 90, 90, 90, 90,
		90, 90, 90, 91, 91, 91, 91, 91,
		91, 91, 91, 91, 91, 91, 91, 92,
		92, 92, 92, 92, 92, 92, 92, 92,
		92, 92, 92, 92, 92, 93, 93, 93,
		93, 93, 93, 93, 93, 93, 93, 93,
		93, 93, 93, 93, 93, 94, 94, 94,
		94, 94, 94, 94, 94, 94, 94, 94,
		94, 94, 94, 94, 94, 94, 95, 95,
		95, 95, 95, 95, 95, 95, 95, 95,
		95, 95, 95, 95, 95, 96, 96, 96,
		96, 96, 96, 96, 96, 96, 96, 96,
		96, 96, 96, 96, 97, 97, 97, 97,
		97, 97, 97, 97, 97, 97, 97, 97,
		97, 98, 98, 98, 98, 98, 98, 98,
		98, 98, 98, 98, 98, 98, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 100, 100, 100, 100, 100, 100, 100,
		100, 100, 100, 100, 100, 101, 101, 101,
		101, 101, 101, 101, 101, 101, 101, 101,
		102, 102, 102, 102, 102, 102, 102, 102,
		102, 102, 102, 103, 103, 103, 103, 103,
		103, 103, 103, 103, 103, 104, 104, 104,
		104, 104, 104, 104, 104, 104, 104, 104,
		105, 105, 105, 105, 105, 105, 105, 105,
		105, 105, 105, 106, 106, 106, 106, 106,
		106, 106, 106, 106, 106, 107, 107, 107,
		107, 107, 107, 107, 107, 107, 107, 107,
		108, 108, 108, 108, 108, 108, 108, 108,
		109, 110, 110, 111, 112, 113, 113, 114,
		114, 115, 116, 116, 117, 117, 118, 119,
		119, 120, 120, 121, 121, 122, 122, 123,
		123, 123, 124, 124, 125, 125, 126, 126,
		127, 127, 128, 128, 129, 129, 130, 130,
		131, 131, 132, 132, 133, 133, 134, 134,
		135, 135, 136, 137, 137, 138, 138, 139,
		139, 140, 140, 141, 141, 142, 142, 143,
		143, 143, 144, 144, 145, 145, 146, 146,
		146, 147, 147, 148, 148, 148, 149, 149,
		150, 150, 150, 151, 151, 151, 152, 152,
		152, 153, 153, 153, 154, 154, 155, 155,
		155, 156, 156, 156, 157, 157, 158, 158,
		158, 159, 159, 160, 160, 160, 161, 161,
		162, 162, 163, 163, 164, 164, 164, 165,
		165, 166, 166, 167, 167, 168, 168, 169,
		169, 170, 170, 171, 171, 172, 172, 173,
		173, 174, 174, 175, 175, 176, 176, 176,
		177, 177, 178, 178, 179, 179, 180, 180,
		180, 181, 181, 182, 182, 182, 183, 183,
		184, 184, 184, 185, 185, 185, 186, 186,
		187, 187, 187, 188, 188, 188, 189, 189,
		189, 190, 190, 190, 190, 191, 191, 191,
		192, 192, 192, 193, 193, 193, 193, 194,
		194, 194, 195, 195, 195, 195, 196, 196,
		196, 196, 197, 197, 197, 197, 198, 198,
		198, 198, 199, 199, 199, 199, 199, 200,
		200, 200, 200, 201, 201, 201, 201, 202,
		202, 202, 202, 202, 203, 203, 203, 203,
		204, 204, 204, 204, 204, 205, 205, 205,
		205, 205, 206, 206, 206, 206, 207, 207,
		207, 207, 208, 208, 208, 208, 208, 209,
		209, 209, 209, 210, 210, 210, 210, 211,
		211, 211, 211, 212, 212, 212, 212, 213,
		213, 213, 213, 214, 214, 214, 214, 215,
		215, 215, 215, 216, 216, 216, 216, 217,
		217, 217, 218, 218, 218, 218, 219, 219,
		219, 219, 220, 220, 220, 220, 221, 221,
		221, 221, 222, 222, 222, 223, 223, 223,
		223, 224, 224, 224, 224, 225, 225, 225,
		225, 226, 226, 226, 227, 227, 227, 227,
		228, 228, 228, 228, 229, 229, 229, 229,
		230, 230, 230, 230, 231, 231, 231, 231,
		232, 232, 232, 232, 233, 233, 233, 233,
		234, 234, 234, 234, 235, 235, 235, 235,
		236, 236, 236, 236, 237, 237, 237, 237,
		238, 238, 238, 238, 239, 239, 239, 239,
		240, 240, 240, 240, 241, 241, 241, 241,
		241, 242, 242, 242, 242, 243, 243, 243,
		243, 244, 244, 244, 244, 245, 245, 245,
		245, 245, 246, 246, 246, 246, 247, 247,
		247, 247, 248, 248, 248, 248, 248, 249,
		249, 249, 249, 250, 250, 250, 250, 250,
		251, 251, 251, 251, 252, 252, 252, 252,
		252, 253, 253, 253, 253, 253, 254, 254,
		254, 254, 254, 255, 255, 255, 255, 255,
	},
};
#endif
struct tegra_dc_mode dsi_s_mi3_1080p_5_modes[] = {
	/* 1080x1920@60Hz */
	{
		.pclk = 143700000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 10,
		.v_sync_width = 2,
		.h_back_porch = 50,
		.v_back_porch = 5,
		.h_active = 1080,
		.v_active = 1920,
		.h_front_porch = 100,
		.v_front_porch = 3,
	},
};
static int dsi_s_mi3_1080p_5_reg_get(void)
{
	int err = 0;

	if (dsi_s_mi3_1080p_5_reg_requested)
		return 0;

	vdd_lcd_s_1v8 = regulator_get(NULL, "vdd_lcd_1v8_s");
	if (IS_ERR_OR_NULL(vdd_lcd_s_1v8)) {
		pr_err("vdd_lcd_1v8_s regulator get failed\n");
		err = PTR_ERR(vdd_lcd_s_1v8);
		vdd_lcd_s_1v8 = NULL;
		goto fail;
	}

	dsi_s_mi3_1080p_5_reg_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_s_mi3_1080p_5_gpio_get(void)
{
	int err = 0;

	if (dsi_s_mi3_1080p_5_gpio_requested)
		return 0;

	err = gpio_request(DSI_PANEL_RST_GPIO, "panel rst");
	if (err < 0) {
		pr_err("panel reset gpio request failed\n");
		goto fail;
	}

	err = gpio_request(LCD_VSN_EN, "lcd vsn en");
	if (err < 0) {
		pr_err("lcd vsn en gpio request failed, err=%d\n", err);
		goto fail;
	}

	err = gpio_request(LCD_VSP_EN, "lcd vsp en");
	if (err < 0) {
		pr_err("lcd vsp en gpio request failed, err=%d\n", err);
		goto fail;
	}

	dsi_s_mi3_1080p_5_gpio_requested = true;
	return 0;
fail:
	return err;
}

static int dsi_s_mi3_1080p_5_enable(struct device *dev)
{
	int err = 0;
	struct tegra_dc_out *pdata = ((struct tegra_dc_platform_data *)
				(dev->platform_data))->default_out;

	if (pdata->flags & TEGRA_DC_OUT_INITIALIZED_MODE)
		return 0;

	err = dsi_s_mi3_1080p_5_reg_get();
	if (err < 0) {
		pr_err("dsi regulator get failed\n");
		goto fail;
	}

	err = dsi_s_mi3_1080p_5_gpio_get();
	if (err < 0) {
		pr_err("dsi gpio request failed\n");
		goto fail;
	}

	gpio_direction_output(DSI_PANEL_RST_GPIO, 0);
	gpio_direction_output(LCD_VSP_EN, 0);
	gpio_direction_output(LCD_VSN_EN, 0);

	if (vdd_lcd_s_1v8) {
		err = regulator_enable(vdd_lcd_s_1v8);
		if (err < 0) {
			pr_err("vdd_lcd_1v8_s regulator enable failed\n");
			goto fail;
		}
	}
	usleep_range(1000, 3000);

	gpio_set_value(LCD_VSP_EN, 1);
	usleep_range(1000, 3000);
	gpio_set_value(LCD_VSN_EN, 1);
	msleep(5);

#if DSI_PANEL_RESET
	pr_info("panel: %s (reset signal DSI_PANEL_RST_GPIO)\n", __func__);
	gpio_set_value(DSI_PANEL_RST_GPIO, 1);
	msleep(5);
#endif

	return 0;
fail:
	return err;
}

static u8 panel_internal[] = {0x51, 0x0e, 0xff};
static u8 __maybe_unused MCAP_off[] = {0xb0, 0x04};
static u8 __maybe_unused MCAP_on[] = {0xb0, 0x03};
static u8 __maybe_unused ce_on[] = {0xca, 0x01};
static u8 __maybe_unused ce_off[] = {0xca, 0x00};
static u8 __maybe_unused cabc_on[] = {0x55, 0x01};
static u8 __maybe_unused cabc_off[] = {0x55, 0x00};

static u8 __maybe_unused panel_gamma24_ev[] = {
	0xd3, 0x1b, 0x33, 0xbb, 0xcc, 0x80, 0x33, 0x33, 0x33,
	0x00, 0x01, 0x00, 0xa0, 0xd8, 0xa0, 0x00, 0x47,
	0x33, 0x33, 0x22, 0x70, 0x02, 0x47, 0x53, 0x3d,
	0xbf, 0x22};

static u8 __maybe_unused panel_gamma24w_vrc[] = {
	0xc7, 0x01, 0x0d, 0x17, 0x21, 0x31, 0x4e, 0x42,
	0x52, 0x60, 0x69, 0x72, 0x77, 0x17, 0x21, 0x29,
	0x31, 0x3d, 0x4e, 0x40, 0x56, 0x62, 0x69, 0x72,
	0x77};
static u8 __maybe_unused panel_gamma24_vrc[] = {
	0xc7, 0x01, 0x0d, 0x17, 0x21, 0x31, 0x4e, 0x42,
	0x52, 0x60, 0x69, 0x72, 0x77, 0x17, 0x21, 0x29,
	0x31, 0x3d, 0x4e, 0x40, 0x56, 0x62, 0x69, 0x72,
	0x77};
static u8 __maybe_unused panel_gamma24c_vrc[] = {
	0xc7, 0x17, 0x1d, 0x22, 0x29, 0x36, 0x50, 0x43,
	0x52, 0x60, 0x69, 0x72, 0x77, 0x2d, 0x31, 0x34,
	0x39, 0x42, 0x50, 0x41, 0x56, 0x62, 0x69, 0x72,
	0x77};

static u8 __maybe_unused panel_gamma24w_vrc_2[] = {
	0xC7, 0x0B, 0x17, 0x1F, 0x28, 0x36, 0x52, 0x44,
	0x52, 0x60, 0x67, 0x6F, 0x77, 0x1D, 0x29, 0x2F,
	0x36, 0x40, 0x50, 0x40, 0x56, 0x62, 0x69, 0x6F,
	0x77};
static u8 __maybe_unused panel_gamma24_vrc_2[] = {
	0xC7, 0x0B, 0x18, 0x20, 0x29, 0x37, 0x53, 0x45,
	0x52, 0x60, 0x67, 0x6E, 0x77, 0x1D, 0x2A, 0x30,
	0x37, 0x41, 0x51, 0x41, 0x56, 0x62, 0x69, 0x6E,
	0x77};
static u8 __maybe_unused panel_gamma24c_vrc_2[] = {
	0xC7, 0x0F, 0x18, 0x21, 0x2B, 0x39, 0x54, 0x45,
	0x52, 0x5F, 0x68, 0x71, 0x77, 0x1F, 0x28, 0x2F,
	0x37, 0x41, 0x52, 0x41, 0x56, 0x61, 0x6A, 0x71,
	0x77};

static u8 __maybe_unused panel_gamma24w_vgc[] = {
	0xc8, 0x13, 0x1a, 0x20, 0x27, 0x34, 0x4f, 0x42,
	0x52, 0x60, 0x69, 0x72, 0x77, 0x29, 0x2e, 0x32,
	0x37, 0x40, 0x4f, 0x40, 0x56, 0x62, 0x69, 0x72,
	0x77};
static u8 __maybe_unused panel_gamma24_vgc[] = {
	0xc8, 0x01, 0x0d, 0x17, 0x21, 0x31, 0x4e, 0x42,
	0x52, 0x60, 0x69, 0x72, 0x77, 0x17, 0x21, 0x29,
	0x31, 0x3d, 0x4e, 0x40, 0x56, 0x62, 0x69, 0x72,
	0x77};
static u8 __maybe_unused panel_gamma24c_vgc[] = {
	0xc8, 0x11, 0x18, 0x1e, 0x26, 0x33, 0x4f, 0x42,
	0x52, 0x60, 0x69, 0x72, 0x77, 0x27, 0x2c, 0x30,
	0x36, 0x3f, 0x4f, 0x40, 0x56, 0x62, 0x69, 0x72,
	0x77};

static u8 __maybe_unused panel_gamma24w_vgc_2[] = {
	0xC8, 0x24, 0x2A, 0x2C, 0x33, 0x3E, 0x55, 0x46,
	0x54, 0x61, 0x69, 0x6F, 0x77, 0x32, 0x36, 0x38,
	0x3D, 0x44, 0x53, 0x42, 0x58, 0x63, 0x6B, 0x6F,
	0x77};
static u8 __maybe_unused panel_gamma24_vgc_2[] = {
	0xC8, 0x20, 0x24, 0x2A, 0x30, 0x3B, 0x54, 0x45,
	0x54, 0x62, 0x69, 0x70, 0x77, 0x2E, 0x32, 0x36,
	0x3A, 0x43, 0x52, 0x41, 0x58, 0x64, 0x6B, 0x70,
	0x77};
static u8 __maybe_unused panel_gamma24c_vgc_2[] = {
	0xC8, 0x1A, 0x20, 0x25, 0x2C, 0x39, 0x52, 0x46,
	0x53, 0x60, 0x69, 0x72, 0x77, 0x28, 0x2E, 0x33,
	0x38, 0x41, 0x50, 0x42, 0x57, 0x62, 0x6B, 0x72,
	0x77};

static u8 __maybe_unused panel_gamma24w_vbc[] = {
	0xc9, 0x27, 0x2b, 0x2e, 0x33, 0x3d, 0x53, 0x44,
	0x55, 0x60, 0x69, 0x72, 0x77, 0x3d, 0x3f, 0x40,
	0x43, 0x49, 0x53, 0x42, 0x59, 0x62, 0x69, 0x72,
	0x77};
static u8 __maybe_unused panel_gamma24_vbc[] = {
	0xc9, 0x01, 0x0d, 0x17, 0x21, 0x31, 0x4e, 0x42,
	0x52, 0x60, 0x69, 0x72, 0x77, 0x17, 0x21, 0x29,
	0x31, 0x3d, 0x4e, 0x40, 0x56, 0x62, 0x69, 0x72,
	0x77};
static u8 __maybe_unused panel_gamma24c_vbc[] = {
	0xc9, 0x01, 0x0d, 0x17, 0x21, 0x31, 0x4e, 0x42,
	0x52, 0x60, 0x69, 0x72, 0x77, 0x17, 0x21, 0x29,
	0x31, 0x3d, 0x4e, 0x40, 0x56, 0x62, 0x69, 0x72,
	0x77};

static u8 __maybe_unused panel_gamma24w_vbc_2[] = {
	0xC9, 0x36, 0x39, 0x3C, 0x3E, 0x46, 0x59, 0x47,
	0x57, 0x5F, 0x6A, 0x73, 0x77, 0x40, 0x43, 0x46,
	0x46, 0x4C, 0x55, 0x43, 0x5B, 0x61, 0x6C, 0x73,
	0x77};
static u8 __maybe_unused panel_gamma24_vbc_2[] = {
	0xC9, 0x20, 0x24, 0x2A, 0x30, 0x3B, 0x54, 0x45,
	0x54, 0x62, 0x69, 0x70, 0x77, 0x2E, 0x32, 0x36,
	0x3A, 0x43, 0x52, 0x41, 0x58, 0x64, 0x6B, 0x70,
	0x77};
static u8 __maybe_unused panel_gamma24c_vbc_2[] = {
	0xC9, 0x0B, 0x18, 0x20, 0x28, 0x36, 0x52, 0x45,
	0x52, 0x60, 0x67, 0x72, 0x77, 0x1D, 0x2A, 0x30,
	0x36, 0x40, 0x50, 0x41, 0x56, 0x62, 0x69, 0x72,
	0x77};

static struct tegra_dsi_cmd __maybe_unused s_24_cmds[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_off),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xd6, 0x01),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_ev),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_vrc),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_vgc),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_vbc),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_on),
};
static struct tegra_dsi_cmd __maybe_unused s_24_cmds_2[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_off),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xd6, 0x01),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_ev),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_vrc_2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_vgc_2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_vbc_2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_on),
};

static struct tegra_dsi_cmd __maybe_unused s_24warm_cmds[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_off),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xd6, 0x01),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_ev),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24w_vrc),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24w_vgc),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24w_vbc),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_on),
};
static struct tegra_dsi_cmd __maybe_unused s_24warm_cmds_2[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_off),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xd6, 0x01),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_ev),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24w_vrc_2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24w_vgc_2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24w_vbc_2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_on),
};

static struct tegra_dsi_cmd __maybe_unused s_24cold_cmds[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_off),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xd6, 0x01),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_ev),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24c_vrc),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24c_vgc),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24c_vbc),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_on),
};
static struct tegra_dsi_cmd __maybe_unused s_24cold_cmds_2[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_off),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xd6, 0x01),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24_ev),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24c_vrc_2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24c_vgc_2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_gamma24c_vbc_2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_on),
};

static struct tegra_dsi_cmd __maybe_unused dsi_s_ce_on_cmds[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_off),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xd6, 0x01),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, ce_on),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_on),
};

static struct tegra_dsi_cmd __maybe_unused dsi_s_ce_off_cmds[] = {
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_off),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xd6, 0x01),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, ce_off),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, MCAP_on),
};

static struct tegra_dsi_cmd __maybe_unused dsi_s_cabc_on_cmds[] = {
	DSI_CMD_LONG(DSI_DCS_LWRITE, cabc_on),
};

static struct tegra_dsi_cmd __maybe_unused dsi_s_cabc_off_cmds[] = {
	DSI_CMD_LONG(DSI_DCS_LWRITE, cabc_off),
};

static struct tegra_dsi_cmd __maybe_unused dsi_s_recovery_cmds[] = {
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
};

static struct tegra_dsi_cmd __maybe_unused dsi_s_recoverypwm_cmds[] = {
	DSI_CMD_LONG(DSI_DCS_LWRITE, panel_internal),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x53, 0x2c),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x55, 0x01),
};

static struct tegra_dsi_cmd __maybe_unused dsi_s_recoverypwm1_cmds[] = {
	DSI_CMD_LONG(DSI_DCS_LWRITE, panel_internal),
};

static struct tegra_dsi_cmd __maybe_unused dsi_s_recoverypwm2_cmds[] = {
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x53, 0x2c),
};

static struct tegra_dsi_cmd __maybe_unused dsi_s_recoverypwm3_cmds[] = {
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x55, 0x01),
};

static struct tegra_dsi_cmd dsi_s_mi3_1080p_5_init_cmd[] = {
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x34, 0x00),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x53, 0x2c),
	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, 0x55, 0x01),
	DSI_CMD_LONG(DSI_DCS_LWRITE, panel_internal),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
};

static struct tegra_dsi_cmd dsi_s_mi3_1080p_5_suspend_cmd[] = {
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_OFF, 0x0),
	DSI_DLY_MS(50),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_ENTER_SLEEP_MODE, 0x0),
};

static struct tegra_dsi_out dsi_s_mi3_1080p_5_pdata = {
	.n_data_lanes = 4,

	.dsi_instance = DSI_INSTANCE_0,

	.refresh_rate = 60,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END,
	.controller_vs = DSI_VS_1,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.dsi_init_cmd = dsi_s_mi3_1080p_5_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_s_mi3_1080p_5_init_cmd),

	.dsi_suspend_cmd = dsi_s_mi3_1080p_5_suspend_cmd,
	.n_suspend_cmd = ARRAY_SIZE(dsi_s_mi3_1080p_5_suspend_cmd),
};

static int dsi_s_mi3_1080p_5_disable(void)
{
	int err = 0;
	pr_info("panel: %s\n", __func__);
	gpio_set_value(DSI_PANEL_RST_GPIO, 0);
	msleep(2);
	gpio_set_value(LCD_VSN_EN, 0);
	msleep(2);
	gpio_set_value(LCD_VSP_EN, 0);
	msleep(2);
	if (vdd_lcd_s_1v8) {
		err = regulator_disable(vdd_lcd_s_1v8);
		if (err < 0) {
			pr_err("vdd_lcd_1v8_s regulator enable failed\n");
			goto fail;
		}
	}
	return 0;
fail:
	return err;
}

static void dsi_s_mi3_1080p_5_resources_init(struct resource *resources,
				int n_resources)
{
	int i;
	for (i = 0; i < n_resources; i++) {
		struct resource *r = &resources[i];
		if (resource_type(r) == IORESOURCE_MEM &&
				!strcmp(r->name, "dsi_regs")) {
			r->start = TEGRA_DSI_BASE;
			r->end = TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1;
		}
	}
}

static void dsi_s_mi3_1080p_5_dc_out_init(struct tegra_dc_out *dc)
{
	dsi_s_mi3_1080p_5_pdata.phy_timing.t_clkprepare_ns = 40;
	dsi_s_mi3_1080p_5_pdata.phy_timing.t_hsprepare_ns = 44;

	dc->dsi = &dsi_s_mi3_1080p_5_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_s_mi3_1080p_5_modes;
	dc->n_modes = ARRAY_SIZE(dsi_s_mi3_1080p_5_modes);
	dc->enable = dsi_s_mi3_1080p_5_enable;
	dc->disable = dsi_s_mi3_1080p_5_disable;
	dc->width = 62;
	dc->height = 110;
	dc->flags = DC_CTRL_MODE;
}
static void dsi_s_mi3_1080p_5_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_s_mi3_1080p_5_modes[0].h_active;
	fb->yres = dsi_s_mi3_1080p_5_modes[0].v_active;
}

static void dsi_s_mi3_1080p_5_sd_settings_init
	(struct tegra_dc_sd_settings *settings) {
	settings->bl_device_name = "lm3533-backlight0";
	settings->bl_device_name2 = "lm3533-backlight1";
}
#ifdef CONFIG_TEGRA_DC_CMU
static void dsi_s_mi3_1080p_5_cmu_init(struct tegra_dc_platform_data *pdata)
{
	pdata->cmu = &dsi_s_mi3_1080p_5_cmu;
}
#endif

extern void tegra_dsi_set_dispparam(struct tegra_dsi_cmd *cmds, int cmds_cnt);
extern void tegra_dsi_send_cmds_hs(struct tegra_dsi_cmd *cmds, int cmds_cnt);
extern u16 tegra_dsi_read_manufacture_id(void);

struct tegra_dsi_cmd *p_dsi_gamma_cmds_mi3 = NULL;
u32 gamma_cmds_cnt_mi3 = 0;
struct tegra_dsi_cmd *p_dsi_ce_cmds_mi3 = NULL;
u32 ce_cmds_cnt_mi3 = 0;
EXPORT_SYMBOL(p_dsi_gamma_cmds_mi3);
EXPORT_SYMBOL(gamma_cmds_cnt_mi3);
EXPORT_SYMBOL(p_dsi_ce_cmds_mi3);
EXPORT_SYMBOL(ce_cmds_cnt_mi3);

static u8 got_mfe_id = 0;
static u8 mfe_id = 0;

static void dsi_s_mi3_panel_gamma_select(void)
{
	if (got_mfe_id == 0) {
		mfe_id = tegra_dsi_read_manufacture_id();
		pr_info("panel: %s   line:%d  mfe_id is %d\n", __func__, __LINE__, mfe_id);
		got_mfe_id = 1;
	}
}

static void dsi_s_mi3_set_dispparam(int param)
{
	unsigned int temp;
	u16 manufacture_id = 0;

	if (!got_mfe_id) {
		pr_info("panel: warning:%s(): sharp manufacture id is not defined.\n", __func__);

		dsi_s_mi3_panel_gamma_select();
	}
	temp = param & 0x0000000F;
	switch (temp) {		/* Gamma */
	case 0x1:	/* 22 */
		if (mfe_id == 0x10) {
			tegra_dsi_set_dispparam(s_24warm_cmds_2, ARRAY_SIZE(s_24warm_cmds_2));
			p_dsi_gamma_cmds_mi3 = s_24warm_cmds_2;
			gamma_cmds_cnt_mi3 = ARRAY_SIZE(s_24warm_cmds_2);
			pr_info("panel: %s: set gamma_2 (Color Fix)\n", __func__);
		} else {
			tegra_dsi_set_dispparam(s_24warm_cmds, ARRAY_SIZE(s_24warm_cmds));
			p_dsi_gamma_cmds_mi3 = s_24warm_cmds;
			gamma_cmds_cnt_mi3 = ARRAY_SIZE(s_24warm_cmds);
			pr_info("panel: %s: set gamma 22\n", __func__);

		}
		break;
	case 0x2:	/* 24 */
		if (mfe_id == 0x10) {
			tegra_dsi_set_dispparam(s_24_cmds_2, ARRAY_SIZE(s_24_cmds_2));
			pr_info("panel: %s: set gamma 24 (Color Fix)\n", __func__);
		} else {
			tegra_dsi_set_dispparam(s_24_cmds, ARRAY_SIZE(s_24_cmds));
			pr_info("panel: %s: set gamma 24\n", __func__);
		}
		p_dsi_gamma_cmds_mi3 = NULL;/*default setting, we don't need to set it in panel turn on */
		gamma_cmds_cnt_mi3 = 0;
		break;
	case 0x3:	/* 25 */
		if (mfe_id == 0x10) {
			tegra_dsi_set_dispparam(s_24cold_cmds_2, ARRAY_SIZE(s_24cold_cmds_2));
			p_dsi_gamma_cmds_mi3 = s_24cold_cmds_2;
			gamma_cmds_cnt_mi3 = ARRAY_SIZE(s_24cold_cmds_2);
			pr_info("panel: %s: set gamma 25 (Color Fix)\n", __func__);
		} else {
			tegra_dsi_set_dispparam(s_24cold_cmds, ARRAY_SIZE(s_24cold_cmds));
			p_dsi_gamma_cmds_mi3 = s_24cold_cmds;
			gamma_cmds_cnt_mi3 = ARRAY_SIZE(s_24cold_cmds);
			pr_info("panel: %s: set gamma 25\n", __func__);
		}
		break;
	case 0xF:
		break;
	default:
		break;
	}

	temp = param & 0x000000F0;
	switch (temp) {		/* CE */
	case 0x10:		/*ce mode1*/
		tegra_dsi_set_dispparam(dsi_s_ce_on_cmds, ARRAY_SIZE(dsi_s_ce_on_cmds));
		p_dsi_ce_cmds_mi3 = NULL;/*default setting is enabled */
		ce_cmds_cnt_mi3 = 0;
		pr_info("panel: %s: set ce on\n", __func__);
		break;
	case 0xF0:
		tegra_dsi_set_dispparam(dsi_s_ce_off_cmds, ARRAY_SIZE(dsi_s_ce_off_cmds));
		p_dsi_ce_cmds_mi3 = dsi_s_ce_off_cmds;
		ce_cmds_cnt_mi3 = ARRAY_SIZE(dsi_s_ce_off_cmds);
		pr_info("panel: %s: set ce off\n", __func__);
		break;
	default:
		break;
	}

	temp = param & 0x00000F00;
	switch (temp) {		/* CABC */
	case 0x100:
		tegra_dsi_send_cmds_hs(dsi_s_cabc_on_cmds, ARRAY_SIZE(dsi_s_cabc_on_cmds));
		pr_info("panel: %s: set cabc on\n", __func__);
		break;
	case 0xF00:
		tegra_dsi_send_cmds_hs(dsi_s_cabc_off_cmds, ARRAY_SIZE(dsi_s_cabc_off_cmds));
		pr_info("panel: %s: set cabc off\n", __func__);
		break;
	default:
		break;
	}

	temp = param & 0x0000F000;
	switch (temp) {		/* Other Func */
	case 0x1000:	/* read manufacture id */
		manufacture_id = tegra_dsi_read_manufacture_id();
		pr_info("panel: %s: manufacture id is %d\n", __func__, manufacture_id);
		break;
	case 0x2000:	/* recovery */
		tegra_dsi_send_cmds_hs(dsi_s_recovery_cmds, ARRAY_SIZE(dsi_s_recovery_cmds));
		break;
	case 0x3000:
		tegra_dsi_set_dispparam(dsi_s_recovery_cmds, ARRAY_SIZE(dsi_s_recovery_cmds));
		break;
	case 0x4000:
		tegra_dsi_send_cmds_hs(dsi_s_recoverypwm_cmds, ARRAY_SIZE(dsi_s_recoverypwm_cmds));
		break;
	case 0x5000:
		tegra_dsi_set_dispparam(dsi_s_recoverypwm_cmds, ARRAY_SIZE(dsi_s_recoverypwm_cmds));
		pr_info("panel: %s: lp 3 commands\n", __func__);
		break;
	case 0x6000:
		tegra_dsi_set_dispparam(dsi_s_recoverypwm1_cmds, ARRAY_SIZE(dsi_s_recoverypwm1_cmds));
		pr_info("panel: %s: lp 51h\n", __func__);
		break;
	case 0x7000:
		tegra_dsi_set_dispparam(dsi_s_recoverypwm2_cmds, ARRAY_SIZE(dsi_s_recoverypwm2_cmds));
		pr_info("panel: %s: lp 53h\n", __func__);
		break;
	case 0x8000:
		tegra_dsi_set_dispparam(dsi_s_recoverypwm3_cmds, ARRAY_SIZE(dsi_s_recoverypwm3_cmds));
		pr_info("panel: %s: lp 55h\n", __func__);
		break;
	case 0xA000:
		pr_info("panel: %s: \n", __func__);
		break;
	case 0xB000:
		pr_info("panel: %s: \n", __func__);
		break;
	case 0xC000:
		pr_info("panel: %s: \n", __func__);
		break;
	case 0xD000:
		pr_info("panel: %s: \n", __func__);
		break;
	case 0xE000:
		pr_info("panel: %s: \n", __func__);
		break;
	case 0xF000:
		break;
	default:
		break;
	}
}

struct tegra_panel dsi_s_1080p_5_mi3 = {
	.init_sd_settings = dsi_s_mi3_1080p_5_sd_settings_init,
	.init_dc_out = dsi_s_mi3_1080p_5_dc_out_init,
	.init_fb_data = dsi_s_mi3_1080p_5_fb_data_init,
	.init_resources = dsi_s_mi3_1080p_5_resources_init,
#ifdef CONFIG_TEGRA_DC_CMU
	.init_cmu_data = dsi_s_mi3_1080p_5_cmu_init,
#endif
	.set_dispparam = dsi_s_mi3_set_dispparam,
	.panel_gamma_select = NULL,
};
EXPORT_SYMBOL(dsi_s_1080p_5_mi3);
